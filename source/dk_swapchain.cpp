#include "dk_swapchain.h"
#include "dk_device.h"
#include "dk_memblock.h"
#include "dk_queue.h"
#include "dk_image.h"

using namespace dk::detail;
using namespace maxwell;

namespace
{
	struct SwapchainFormatTableEntry
	{
		DkImageFormat dkFmt;
		uint32_t androidFmt;
		NvColorFormat nvFmt;
	};

	const SwapchainFormatTableEntry formatTable[] =
	{
		{ DkImageFormat_R8_Unorm,         PIXEL_FORMAT_Y8,        NvColorFormat_Y8,       }, // android format unverified
		{ DkImageFormat_RG8_Unorm,        PIXEL_FORMAT_Y16,       NvColorFormat_U8_V8,    }, // android format unverified
		{ DkImageFormat_RGBA8_Unorm,      PIXEL_FORMAT_RGBA_8888, NvColorFormat_A8B8G8R8, },
		{ DkImageFormat_RGBX8_Unorm_sRGB, PIXEL_FORMAT_RGBX_8888, NvColorFormat_X8B8G8R8, },
		{ DkImageFormat_RGBA8_Unorm_sRGB, PIXEL_FORMAT_RGBA_8888, NvColorFormat_A8B8G8R8, },
		{ DkImageFormat_RGBX8_Unorm,      PIXEL_FORMAT_RGBX_8888, NvColorFormat_X8B8G8R8, },
		{ DkImageFormat_BGR565_Unorm,     PIXEL_FORMAT_RGB_565,   NvColorFormat_R5G6B5,   },
		{ DkImageFormat_BGRA8_Unorm,      PIXEL_FORMAT_BGRA_8888, NvColorFormat_A8R8G8B8, },
		{ DkImageFormat_BGRA8_Unorm_sRGB, PIXEL_FORMAT_BGRA_8888, NvColorFormat_A8R8G8B8, },
	};

	SwapchainFormatTableEntry const* findFormat(DkImageFormat format)
	{
		for (auto& entry : formatTable)
			if (entry.dkFmt == format)
				return &entry;
		return nullptr;
	}
}

DkResult Swapchain::initialize(void* nativeWindow, DkImage const* const images[], uint32_t numImages)
{
	m_nwin = (NWindow*)nativeWindow;
	m_numImages = numImages;

	DkImage const& firstImage = *images[0];
	uint32_t width = firstImage.m_dimensions[0];
	uint32_t height = firstImage.m_dimensions[1];
	DkImageFormat format = firstImage.m_format;
	auto* fmtData = findFormat(format);
#ifdef DEBUG
	if (!fmtData)
		return DkResult_BadState;
#endif

	// Set up NvGraphicBuffer template
	NvGraphicBuffer grbuf = {};
	grbuf.header.num_ints = (sizeof(NvGraphicBuffer) - sizeof(NativeHandle)) / 4;
	grbuf.unk0 = -1;
	grbuf.magic = 0xDAFFCAFF;
	grbuf.pid = 42;
	grbuf.usage = GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;
	grbuf.format = fmtData->androidFmt;
	grbuf.ext_format = fmtData->androidFmt;
	grbuf.num_planes = 1;
	grbuf.planes[0].width = width;
	grbuf.planes[0].height = height;
	grbuf.planes[0].color_format = fmtData->nvFmt;
	grbuf.planes[0].layout = NvLayout_BlockLinear;
	grbuf.planes[0].kind = NvKind_Generic_16BX2;
	grbuf.planes[0].block_height_log2 = firstImage.m_tileH;

	// Calculate a few things
	uint32_t bytesPerPixel = ((u64)fmtData->nvFmt >> 3) & 0x1F;
	uint32_t widthAlignedBytes = (width*bytesPerPixel + 63) &~ 63; // GOBs are 64 bytes wide
	uint32_t widthAligned = widthAlignedBytes / bytesPerPixel;

	for (uint32_t i = 0; i < numImages; i ++)
	{
		DkImage const& img = *images[i];

		// Configure this image
		m_images[i] = &img;
		grbuf.nvmap_id = img.m_memBlock->getId();
		grbuf.stride = widthAligned;
		grbuf.total_size = img.m_layerSize;
		grbuf.planes[0].pitch = widthAlignedBytes;
		grbuf.planes[0].size = img.m_layerSize;
		grbuf.planes[0].offset = img.m_memOffset;
		if (R_FAILED(nwindowConfigureBuffer(m_nwin, i, &grbuf)))
			return DkResult_Fail;
	}

	// If the origin mode is lower_left (i.e. OpenGL style), all rendered images are upside down
	// (as in the BMP format). The compositor, of course, expects images to be stored the normal
	// way (with Y pointing down), so we must instruct it to vertically flip whatever we feed it.
	if (getDevice()->isOriginModeOpenGL())
		nwindowSetTransform(m_nwin, HAL_TRANSFORM_FLIP_V);

	return DkResult_Success;
}

Swapchain::~Swapchain()
{
	if (m_nwin)
		nwindowReleaseBuffers(m_nwin);
}

void Swapchain::acquireImage(int& imageSlot, DkFence& fence)
{
	fence.m_type = DkFence::External;
	if (R_FAILED(nwindowDequeueBuffer(m_nwin, &imageSlot, &fence.m_external.m_fence)))
		DK_ERROR(DkResult_Fail, "failed to dequeue buffer");
}

void Swapchain::presentImage(int imageSlot, DkFence const& fence)
{
	NvMultiFence nvfence = {};
	nvMultiFenceCreate(&nvfence, &fence.m_internal.m_fence);
	if (R_FAILED(nwindowQueueBuffer(m_nwin, imageSlot, &nvfence)))
		DK_ERROR(DkResult_Fail, "failed to enqueue buffer");
}

void Swapchain::setCrop(int32_t left, int32_t top, int32_t right, int32_t bottom)
{
	if (R_FAILED(nwindowSetCrop(m_nwin, left, top, right, bottom)))
		DK_ERROR(DkResult_Fail, "failed to set window crop");
}

void Swapchain::setSwapInterval(uint32_t interval)
{
	if (R_FAILED(nwindowSetSwapInterval(m_nwin, interval)))
		DK_ERROR(DkResult_Fail, "failed to set swap interval");
}

DkSwapchain dkSwapchainCreate(DkSwapchainMaker const* maker)
{
	DK_ENTRYPOINT(maker->device);
	DK_DEBUG_NON_NULL(maker->nativeWindow);
	DK_DEBUG_BAD_INPUT(!nwindowIsValid((NWindow*)maker->nativeWindow), "invalid native window handle");
	DK_DEBUG_NON_ZERO(maker->numImages);
	for (uint32_t i = 0; i < maker->numImages; i ++)
	{
		DK_DEBUG_NON_NULL(maker->pImages[i]);
		DK_DEBUG_BAD_FLAGS(!(maker->pImages[i]->m_flags & DkImageFlags_UsagePresent), "DkImageFlags_UsagePresent must be set");
		DK_DEBUG_BAD_INPUT(maker->pImages[i]->m_format != maker->pImages[0]->m_format, "mismatched swapchain image formats");
		DK_DEBUG_BAD_INPUT(maker->pImages[i]->m_dimensions[0] != maker->pImages[0]->m_dimensions[0], "mismatched swapchain image widths");
		DK_DEBUG_BAD_INPUT(maker->pImages[i]->m_dimensions[1] != maker->pImages[0]->m_dimensions[1], "mismatched swapchain image heights");
	}

	size_t extraSize = sizeof(DkImage const*) * maker->numImages;
	DkSwapchain obj = new(maker->device, extraSize) Swapchain(maker->device);
	DkResult res = obj->initialize(maker->nativeWindow, maker->pImages, maker->numImages);
	if (res != DkResult_Success)
	{
		delete obj;
		DK_ERROR(res, "initialization failure");
	}
	return obj;
}

void dkSwapchainDestroy(DkSwapchain obj)
{
	DK_ENTRYPOINT(obj);
	delete obj;
}

void dkSwapchainAcquireImage(DkSwapchain obj, int* imageSlot, DkFence* fence)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_NON_NULL(imageSlot);
	DK_DEBUG_NON_NULL(fence);
	obj->acquireImage(*imageSlot, *fence);
}

void dkSwapchainSetCrop(DkSwapchain obj, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
	DK_ENTRYPOINT(obj);
	obj->setCrop(left, top, right, bottom);
}

void dkSwapchainSetSwapInterval(DkSwapchain obj, uint32_t interval)
{
	DK_ENTRYPOINT(obj);
	obj->setSwapInterval(interval);
}

int dkQueueAcquireImage(DkQueue obj, DkSwapchain swapchain)
{
	DK_ENTRYPOINT(obj);
	if (obj->isInErrorState())
		DK_ERROR(DkResult_Fail, "attempted to acquire image using a queue in error state");

	int imageSlot;
	DkFence fence;
	swapchain->acquireImage(imageSlot, fence);
	obj->waitFence(fence);
	return imageSlot;
}

void dkQueuePresentImage(DkQueue obj, DkSwapchain swapchain, int imageSlot)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_BAD_INPUT(imageSlot < 0 || (unsigned)imageSlot >= swapchain->getNumImages(), "image slot out of bounds");
	if (obj->isInErrorState())
		DK_ERROR(DkResult_Fail, "attempted to present image using a queue in error state");

	DkImage const* image = swapchain->getImage(imageSlot);
	if (image->m_flags & DkImageFlags_HwCompression)
		obj->decompressSurface(image);

	DkFence fence;
	obj->signalFence(fence, true);
	obj->flush();
	swapchain->presentImage(imageSlot, fence);
}
