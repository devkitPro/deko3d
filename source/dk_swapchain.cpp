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
		{ DkImageFormat_RGB565_Unorm,     PIXEL_FORMAT_RGB_565,   NvColorFormat_R5G6B5,   },
		{ DkImageFormat_RGBX8_Unorm,      PIXEL_FORMAT_RGBX_8888, NvColorFormat_X8B8G8R8, },
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

DkResult tag_DkSwapchain::initialize(void* nativeWindow, DkImage const* const images[], uint32_t numImages)
{
	m_nwin = (NWindow*)nativeWindow;
	m_numImages = numImages;

#ifdef DEBUG
	for (uint32_t i = 0; i < numImages; i ++)
		if (!images[i])
			return DkResult_BadInput;
#endif

	DkImage const& firstImage = *images[0];
	uint32_t width = firstImage.m_dimensions[0];
	uint32_t height = firstImage.m_dimensions[1];
	DkImageFormat format = firstImage.m_format;
	auto* fmtData = findFormat(format);
#ifdef DEBUG
	if (!(firstImage.m_flags & DkImageFlags_UsagePresent))
		return DkResult_BadInput;
	if (!fmtData)
		return DkResult_NotImplemented;
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
#ifdef DEBUG
		if (!(img.m_flags & DkImageFlags_UsagePresent))
			return DkResult_BadInput;
		if (img.m_format != format || img.m_dimensions[0] != width || img.m_dimensions[1] != height)
			return DkResult_BadInput;
#endif

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

tag_DkSwapchain::~tag_DkSwapchain()
{
	if (m_nwin)
		nwindowReleaseBuffers(m_nwin);
}

void tag_DkSwapchain::acquireImage(int& imageSlot, DkFence& fence)
{
	fence.m_type = DkFence::External;
	if (R_FAILED(nwindowDequeueBuffer(m_nwin, &imageSlot, &fence.m_external.m_fence)))
		raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_Fail);
}

void tag_DkSwapchain::presentImage(int imageSlot, DkFence const& fence)
{
	NvMultiFence nvfence = {};
	nvMultiFenceCreate(&nvfence, &fence.m_internal.m_fence);
	if (R_FAILED(nwindowQueueBuffer(m_nwin, imageSlot, &nvfence)))
		raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_Fail);
}

void tag_DkSwapchain::setCrop(int32_t left, int32_t top, int32_t right, int32_t bottom)
{
	if (R_FAILED(nwindowSetCrop(m_nwin, left, top, right, bottom)))
		raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_Fail);
}

void tag_DkSwapchain::setSwapInterval(uint32_t interval)
{
	if (R_FAILED(nwindowSetSwapInterval(m_nwin, interval)))
		raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_Fail);
}

DkSwapchain dkSwapchainCreate(DkSwapchainMaker const* maker)
{
	DkSwapchain obj = nullptr;
#ifdef DEBUG
	if (!maker || !maker->device)
		return nullptr; // can't do much here
	if (!maker->nativeWindow || !nwindowIsValid((NWindow*)maker->nativeWindow))
		ObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	else if (!maker->numImages)
		ObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif
	{
		size_t extraSize = sizeof(DkImage const*) * maker->numImages;
		obj = new(maker->device, extraSize) tag_DkSwapchain(maker->device);
	}
	if (obj)
	{
		DkResult res = obj->initialize(maker->nativeWindow, maker->pImages, maker->numImages);
		if (res != DkResult_Success)
		{
			delete obj;
			obj = nullptr;
			ObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, res);
		}
	}
	return obj;
}

void dkSwapchainDestroy(DkSwapchain obj)
{
	delete obj;
}

void dkSwapchainAcquireImage(DkSwapchain obj, int* imageSlot, DkFence* fence)
{
#ifdef DEBUG
	if (!imageSlot || !fence)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif
	obj->acquireImage(*imageSlot, *fence);
}

void dkSwapchainSetCrop(DkSwapchain obj, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
	obj->setCrop(left, top, right, bottom);
}

void dkSwapchainSetSwapInterval(DkSwapchain obj, uint32_t interval)
{
	obj->setSwapInterval(interval);
}

int dkQueueAcquireImage(DkQueue obj, DkSwapchain swapchain)
{
	int imageSlot;
	DkFence fence;
	swapchain->acquireImage(imageSlot, fence);
	obj->waitFence(fence);
	return imageSlot;
}

void dkQueuePresentImage(DkQueue obj, DkSwapchain swapchain, int imageSlot)
{
#ifdef DEBUG
	if (imageSlot < 0 || (unsigned)imageSlot >= swapchain->getNumImages())
		return obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif
	DkImage const* image = swapchain->getImage(imageSlot);
	if (image->m_flags & DkImageFlags_HwCompression)
		obj->decompressSurface(image);

	DkFence fence;
	obj->signalFence(fence, true);
	obj->flush();
	swapchain->presentImage(imageSlot, fence);
}
