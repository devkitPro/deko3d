#include "dk_image.h"
#include "dk_device.h"
#include "dk_memblock.h"
#include "cmdbuf_writer.h"

#include "engine_3d.h"
#include "engine_2d.h"

using namespace dk::detail;
using namespace maxwell;

namespace
{
	constexpr DkTileSize pickTileSize(uint32_t gobs)
	{
		if (gobs >= 16) return DkTileSize_SixteenGobs;
		if (gobs >= 8)  return DkTileSize_EightGobs;
		if (gobs >= 4)  return DkTileSize_FourGobs;
		if (gobs >= 2)  return DkTileSize_TwoGobs;
		return DkTileSize_OneGob;
	}

	constexpr uint8_t adjustTileSize(uint8_t shift, uint8_t unitFactor, uint32_t dimension)
	{
		if (!shift)
			return 0;

		uint32_t x = uint32_t(unitFactor) << (shift - 1);
		if (x >= dimension)
		{
			while (--shift)
			{
				x >>= 1;
				if (x < dimension)
					break;
			}
		}
		return shift;
	}

	constexpr uint32_t adjustMipSize(uint32_t size, unsigned level)
	{
		size >>= level;
		return size ? size : 1;
	}

	constexpr uint32_t adjustBlockSize(uint32_t size, uint32_t blockSize)
	{
		return (size + blockSize - 1) / blockSize;
	}
}

void DkImageLayout::calcLayerSize()
{
	uint32_t tileWidth  = 64 / m_bytesPerBlock; // non-sparse tile width is always zero
	uint32_t tileHeight = 8 << m_tileH;
	uint32_t tileDepth  = 1 << m_tileD;

	m_layerSize = 0;
	for (unsigned i = 0; i < m_mipLevels; i ++)
	{
		uint32_t levelWidth  = adjustBlockSize(adjustMipSize(m_dimensions[0]*m_samplesX, i), m_blockW);
		uint32_t levelHeight = m_dimsPerLayer>=2 ? adjustBlockSize(adjustMipSize(m_dimensions[1]*m_samplesY, i), m_blockH) : 1;
		uint32_t levelDepth  = m_dimsPerLayer>=3 ? adjustMipSize(m_dimensions[2], i) : 1;
		uint32_t levelWidthBytes = levelWidth << m_bytesPerBlockLog2;

		uint32_t levelTileWShift = adjustTileSize(0,       64, levelWidthBytes); // non-sparse tile width is always zero
		uint32_t levelTileHShift = adjustTileSize(m_tileH, 8,  levelHeight);
		uint32_t levelTileDShift = adjustTileSize(m_tileD, 1,  levelDepth);
		uint32_t levelTileWGobs  = 1U << levelTileWShift;
		uint32_t levelTileHGobs  = 1U << levelTileHShift;
		uint32_t levelTileD      = 1U << levelTileDShift;

		uint32_t levelWidthGobs = (levelWidthBytes + 63) / 64;
		uint32_t levelHeightGobs = (levelHeight + 7) / 8;

		uint32_t levelWidthTiles = (levelWidthGobs + levelTileWGobs - 1) >> levelTileWShift;
		uint32_t levelHeightTiles = (levelHeightGobs + levelTileHGobs - 1) >> levelTileHShift;
		uint32_t levelDepthTiles = (levelDepth + levelTileD - 1) >> levelTileDShift;

		if (m_tileW && tileWidth <= levelWidth && tileHeight <= levelHeight && tileDepth <= levelDepth)
		{
			// For sparse images, we need to align the width using the sparse tile width.
			uint32_t align = 1U << m_tileW;
			levelWidthTiles = (levelWidthTiles + align - 1) &~ (align - 1);
		}

		m_layerSize += uint64_t(levelWidthTiles*levelHeightTiles*levelDepthTiles) << (9 + levelTileWShift + levelTileHShift + levelTileDShift);
	}
}

DkResult ImageInfo::fromImageView(DkImageView const* view, unsigned usage)
{
	DkImage const* image = view->pImage;
	DkImageType type = view->type ? view->type : image->m_type;
	DkImageFormat format = view->format ? view->format : image->m_format;
	FormatTraits const& traits = formatTraits[format];

	bool isRenderTarget = usage == ColorRenderTarget || usage == DepthRenderTarget;
#ifdef DEBUG
	bool formatIsDepth = traits.depthBits || traits.stencilBits;
	if (isRenderTarget && !(image->m_flags & DkImageFlags_UsageRender))
		return DkResult_BadInput;
	if (usage == Transfer2D && !(image->m_flags & DkImageFlags_Usage2DEngine))
		return DkResult_BadInput;
	if (usage == ColorRenderTarget && formatIsDepth)
		return DkResult_BadInput;
	if (usage == DepthRenderTarget && !formatIsDepth)
		return DkResult_BadInput;
	if (usage == DepthRenderTarget && (type == DkImageType_3D || (image->m_flags & DkImageFlags_PitchLinear)))
		return DkResult_BadInput;
#endif

	bool layered;
	switch (type)
	{
#ifndef DEBUG
		default:
#endif
		case DkImageType_2D:
		case DkImageType_3D:
		case DkImageType_2DMS:
		case DkImageType_Rectangle:
			layered = false;
			break;
		case DkImageType_2DArray:
		case DkImageType_2DMSArray:
		case DkImageType_Cubemap:
		case DkImageType_CubemapArray:
			layered = true;
			break;
#ifdef DEBUG
		default:
			return DkResult_BadInput;
#endif
	}

	m_iova   = image->m_iova;
	m_format = isRenderTarget ? traits.renderFmt : traits.engine2dFmt;
	m_width  = adjustBlockSize(image->m_dimensions[0], traits.blockWidth);
	m_height = adjustBlockSize(image->m_dimensions[1], traits.blockHeight);
	m_widthMs = adjustBlockSize(image->m_dimensions[0]*image->m_samplesX, traits.blockWidth);
	m_heightMs = adjustBlockSize(image->m_dimensions[1]*image->m_samplesY, traits.blockHeight);
	m_bytesPerBlock = traits.bytesPerBlock;
	// TODO: Mipmap shit

	using TM   = Engine3D::RenderTarget::TileMode;
	using TM2D = Engine2D::SrcTileMode;
	if (!(image->m_flags & DkImageFlags_PitchLinear))
	{
		uint32_t tileWidth = (64 / traits.bytesPerBlock) << image->m_tileW;

		if (layered)
		{
			m_iova += view->layerOffset * image->m_layerSize;
			if (view->layerCount)
				m_arrayMode = view->layerCount;
			else
				m_arrayMode -= view->layerOffset;
		}

		m_horizontal  = (m_widthMs + tileWidth - 1) &~ (tileWidth - 1);
		m_vertical    = m_heightMs;
		if (isRenderTarget)
			m_tileMode = TM::Width{image->m_tileW} | TM::Height{image->m_tileH} | TM::Depth{image->m_tileD} | TM::Is3D{type==DkImageType_3D};
		else
			m_tileMode = TM2D::Width{image->m_tileW} | TM2D::Height{image->m_tileH} | TM2D::Depth{image->m_tileD};
		m_arrayMode   = (type==DkImageType_3D || layered) ? image->m_dimensions[2] : 1;
		m_layerStride = image->m_layerSize;
		if (isRenderTarget)
			m_layerStride >>= 2;
		m_isLinear = false;
	}
	else
	{
#ifdef DEBUG
		if (layered)
			return DkResult_BadInput;
#endif

		m_horizontal  = image->m_stride;
		m_vertical    = m_height;
		if (isRenderTarget)
			m_tileMode = TM::IsLinear{};
		m_arrayMode   = 0;
		m_layerStride = 0;
		m_isLinear = true;
	}

	return DkResult_Success;
}

void dkImageLayoutInitialize(DkImageLayout* obj, DkImageLayoutMaker const* maker)
{
#ifdef DEBUG
	if (!obj || !maker || !maker->device)
		return; // no way to report error back aaaaaaaaaaa
	if (maker->type <= DkImageType_None || maker->type > DkImageType_Buffer)
		return maker->device->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (maker->format <= DkImageFormat_None || maker->format >= DkImageFormat_Count)
		return maker->device->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if ((maker->type == DkImageType_2DMS || maker->type == DkImageType_2DMSArray) && maker->msMode == DkMsMode_1x)
		return maker->device->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!maker->mipLevels)
		return maker->device->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	memset(obj, 0, sizeof(*obj));
	obj->m_type = maker->type;
	obj->m_flags = maker->flags;
	obj->m_format = maker->format;

	switch (obj->m_type)
	{
		case DkImageType_1D:
		case DkImageType_1DArray:
		case DkImageType_Buffer:
			obj->m_dimsPerLayer = 1;
			break;
		default:
			obj->m_dimsPerLayer = 2;
			break;
		case DkImageType_3D:
			obj->m_dimsPerLayer = 3;
			break;
	}

	switch (obj->m_type)
	{
		default:
			obj->m_hasLayers = 0;
			break;
		case DkImageType_1DArray:
		case DkImageType_2DArray:
		case DkImageType_2DMSArray:
		case DkImageType_CubemapArray:
			obj->m_hasLayers = 1;
			break;
	}

	switch (obj->m_type)
	{
		default:
			obj->m_numSamplesLog2 = DkMsMode_1x;
			break;
		case DkImageType_2DMS:
		case DkImageType_2DMSArray:
			obj->m_numSamplesLog2 = maker->msMode;
			break;
	}

	switch (obj->m_type)
	{
		default:
			obj->m_mipLevels = maker->mipLevels;
			break;
		case DkImageType_2DMS:
		case DkImageType_2DMSArray:
		case DkImageType_Rectangle:
		case DkImageType_Buffer:
			obj->m_mipLevels = 1;
			break;
	}

#ifdef DEBUG
	for (unsigned i = 0; i < (obj->m_dimsPerLayer + obj->m_hasLayers); i ++)
		if (!maker->dimensions[i])
			return maker->device->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	obj->m_dimensions[0] = maker->dimensions[0];
	obj->m_dimensions[1] = maker->dimensions[1];
	obj->m_dimensions[2] = maker->dimensions[2];
	switch (obj->m_type)
	{
		default:
			break;
		case DkImageType_1DArray:
			obj->m_dimensions[2] = obj->m_dimensions[1];
			// fallthrough
		case DkImageType_1D:
			obj->m_dimensions[1] = 1; // Set height to 1, because a proper height is needed for tile size calculation
			break;
		case DkImageType_Cubemap: // A cubemap is actually an array of 6 2D layers (one for each face)
			obj->m_dimensions[2] = 6;
			obj->m_hasLayers = 1;
			break;
		case DkImageType_CubemapArray:
			obj->m_dimensions[2] *= 6; // Likewise, adjust the number of layers
			break;
	}

	auto& traits = formatTraits[obj->m_format];
	obj->m_bytesPerBlock = traits.bytesPerBlock;
	obj->m_bytesPerBlockLog2 = __builtin_clz(obj->m_bytesPerBlock) ^ 0x1f;
	obj->m_blockW = traits.blockWidth;
	obj->m_blockH = traits.blockHeight;

#ifdef DEBUG
	if ((obj->m_flags & DkImageFlags_UsageRender) && !(traits.flags & FormatTraitFlags_CanRender))
		return maker->device->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if ((obj->m_flags & DkImageFlags_UsageLoadStore) && !(traits.flags & FormatTraitFlags_CanLoadStore))
		return maker->device->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if ((obj->m_flags & DkImageFlags_Usage2DEngine) && !(traits.flags & FormatTraitFlags_CanUse2DEngine))
		return maker->device->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (obj->m_flags & DkImageFlags_UsagePresent)
	{
		if (obj->m_flags & DkImageFlags_PitchLinear)
			return maker->device->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
		if (obj->m_dimsPerLayer != 2 || obj->m_hasLayers || obj->m_numSamplesLog2 != DkMsMode_1x)
			return maker->device->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	}
#endif

	switch (obj->m_numSamplesLog2)
	{
		default:
		case DkMsMode_1x:
			obj->m_samplesX = 1;
			obj->m_samplesY = 1;
			break;
		case DkMsMode_2x:
			obj->m_samplesX = 2;
			obj->m_samplesY = 1;
			break;
		case DkMsMode_4x:
			obj->m_samplesX = 2;
			obj->m_samplesY = 2;
			break;
		case DkMsMode_8x:
			obj->m_samplesX = 4;
			obj->m_samplesY = 2;
			break;
		/* Currently not supported, maybe in the future
		case DkMsMode_16x:
			obj->m_samplesX = 4;
			obj->m_samplesY = 4;
			break;
		*/
	}

	if (obj->m_type == DkImageType_Buffer)
	{
		obj->m_layerSize = obj->m_bytesPerBlock * obj->m_dimensions[0];
		obj->m_storageSize = obj->m_layerSize;
		obj->m_alignment = traits.bytesPerBlock;
		if (obj->m_alignment == 12) // i.e. DkImageFormat_RGB32_*
			obj->m_alignment = 4;
		return;
	}

	if (obj->m_flags & DkImageFlags_PitchLinear)
	{
#ifdef DEBUG
		if (obj->m_dimsPerLayer != 2 || obj->m_hasLayers)
			return maker->device->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
		if (obj->m_mipLevels > 1)
			return maker->device->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

		obj->m_stride = maker->pitchStride;
		obj->m_layerSize = maker->pitchStride * obj->m_dimensions[1];
		obj->m_storageSize = obj->m_layerSize;
		if (obj->m_flags & DkImageFlags_UsageRender)
			obj->m_alignment = 128; // see TRM 20.1 "Tiling Formats", also supported by nouveau
		else
			obj->m_alignment = 32; // TRM implies this should be 64, but 32 has been observed instead in official software
		return;
	}

	if (obj->m_flags & DkImageFlags_CustomTileSize)
	{
		if (obj->m_dimsPerLayer == 2)
			obj->m_tileH = maker->tileSize;
		else if (obj->m_dimsPerLayer == 3)
			obj->m_tileD = maker->tileSize;
	}
	else
	{
		if (obj->m_flags & DkImageFlags_UsageVideo)
			obj->m_tileH = DkTileSize_TwoGobs; // Supported by nouveau (nvc0_miptree_init_layout_video) and official software
		else if (obj->m_dimsPerLayer == 2)
		{
			// Calculate actual height, and multiply it by 1.5
			uint32_t actualHeight = obj->m_dimensions[1] * obj->m_samplesY;
			if (obj->m_blockH > 1)
				actualHeight = adjustBlockSize(actualHeight, obj->m_blockH);
			uint32_t heightAndHalf = actualHeight + actualHeight/2;
			uint32_t heightAndHalfInGobs = (heightAndHalf + 7) / 8; // remember, one gob is 8 rows tall
			obj->m_tileH = pickTileSize(heightAndHalfInGobs);
		}
		else if (obj->m_dimsPerLayer == 3)
		{
			// Calculate actual depth, and multiply it by 1.5
			uint32_t actualDepth = obj->m_dimensions[1];
			uint32_t depthAndHalf = actualDepth + actualDepth/2;
			obj->m_tileD = pickTileSize(depthAndHalf);
		}
	}

	// Pick a memory kind for this image
	obj->m_memKind = pickImageMemoryKind(traits, obj->m_numSamplesLog2,
		(obj->m_flags & DkImageFlags_HwCompression) != 0,
		(obj->m_flags & DkImageFlags_D16EnableZbc) != 0);

	obj->calcLayerSize();
	if (obj->m_hasLayers)
		obj->m_storageSize = obj->m_dimensions[2] * obj->m_layerSize;
	else
		obj->m_storageSize = obj->m_layerSize;

	if (obj->m_memKind != NvKind_Pitch && obj->m_memKind != NvKind_Generic_16BX2)
	{
		// Since we are using a special memory kind, we need to align the image and its size
		// to a big page boundary so that we can safely reprotect the memory occupied by it.
		uint32_t bigPageSize = maker->device->getGpuInfo().bigPageSize;
		obj->m_storageSize = (obj->m_storageSize + bigPageSize - 1) &~ (bigPageSize - 1);
		obj->m_alignment = bigPageSize;
	}
	else
		obj->m_alignment = 512;
}

uint64_t dkImageLayoutGetSize(DkImageLayout const* obj)
{
	return obj->m_storageSize;
}

uint32_t dkImageLayoutGetAlignment(DkImageLayout const* obj)
{
	return obj->m_alignment;
}

void dkImageInitialize(DkImage* obj, DkImageLayout const* layout, DkMemBlock memBlock, uint32_t offset)
{
#ifdef DEBUG
	if (!obj || !layout || !memBlock)
		return; // no way to report error back aaaaaaaaaaa
	if (offset & (layout->m_alignment - 1))
		memBlock->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedData);
	if (layout->m_memKind != NvKind_Pitch && !memBlock->isImage())
		memBlock->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadMemFlags);
#endif

	memcpy(obj, layout, sizeof(*layout));
	obj->m_iova = memBlock->getGpuAddrForImage(offset, layout->m_storageSize, (NvKind)layout->m_memKind);
	obj->m_memBlock = memBlock;
	obj->m_memOffset = offset;
}

DkGpuAddr dkImageGetGpuAddr(DkImage const* obj)
{
	return obj->m_iova;
}

void dkCmdBufCopyImage(DkCmdBuf obj, DkImageView const* srcView, DkBlitRect const* srcRect, DkImageView const* dstView, DkBlitRect const* dstRect, uint32_t flags)
{
	obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_NotImplemented);
}

void dkCmdBufBlitImage(DkCmdBuf obj, DkImageView const* srcView, DkBlitRect const* srcRect, DkImageView const* dstView, DkBlitRect const* dstRect, uint32_t flags, uint32_t factor)
{
#ifdef DEBUG
	if (!srcView || !srcView->pImage || !srcRect)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!dstView || !dstView->pImage || !dstRect)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	DkResult res;
	ImageInfo srcInfo, dstInfo;

	res = srcInfo.fromImageView(srcView, ImageInfo::Transfer2D);
#ifdef DEBUG
	if (res != DkResult_Success)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, res);
#endif

	res = dstInfo.fromImageView(dstView, ImageInfo::Transfer2D);
#ifdef DEBUG
	if (res != DkResult_Success)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, res);
#endif

#ifndef DEBUG
	(void)res;
#endif

	bool isSrcTexCompressed = srcView->pImage->m_blockW > 1;
#ifdef DEBUG
	bool isDstTexCompressed = dstView->pImage->m_blockW > 1;
	if (isSrcTexCompressed != isDstTexCompressed)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (isSrcTexCompressed && srcInfo.m_format != dstInfo.m_format)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	BlitParams params;
	params.srcX = srcRect->x;
	params.srcY = srcRect->y;
	params.dstX = dstRect->x;
	params.dstY = dstRect->y;
	params.width = dstRect->width;
	params.height = dstRect->height;
	int32_t srcW = srcRect->width;
	int32_t srcH = srcRect->height;

	if (isSrcTexCompressed)
	{
		uint32_t blockW = srcView->pImage->m_blockW;
		uint32_t blockH = srcView->pImage->m_blockH;
		params.srcX = adjustBlockSize(params.srcX, blockW);
		params.srcY = adjustBlockSize(params.srcY, blockH);
		params.srcX = adjustBlockSize(params.dstX, blockW);
		params.srcY = adjustBlockSize(params.dstY, blockH);
		params.width = adjustBlockSize(params.width, blockW);
		params.height = adjustBlockSize(params.height, blockH);
		srcW = adjustBlockSize(srcW, blockW);
		srcH = adjustBlockSize(srcH, blockH);
	}
	else
	{
		if (srcView->pImage->m_numSamplesLog2 != DkMsMode_1x)
		{
			params.srcX *= srcView->pImage->m_samplesX;
			params.srcY *= srcView->pImage->m_samplesY;
			srcW *= srcView->pImage->m_samplesX;
			srcH *= srcView->pImage->m_samplesY;
		}

		if (dstView->pImage->m_numSamplesLog2 != DkMsMode_1x)
		{
			params.dstX *= dstView->pImage->m_samplesX;
			params.dstY *= dstView->pImage->m_samplesY;
			params.width *= dstView->pImage->m_samplesX;
			params.height *= dstView->pImage->m_samplesY;
		}

		if (flags & DkBlitFlag_FlipX)
		{
			params.srcX += srcW;
			srcW = -srcW;
		}

		if (flags & DkBlitFlag_FlipY)
		{
			params.srcY += srcH;
			srcH = -srcH;
		}
	}

	int32_t dudx = (srcW << DiffFractBits) / (int32_t)params.width;
	int32_t dvdy = (srcH << DiffFractBits) / (int32_t)params.height;

	params.srcX = (params.srcX << SrcFractBits) + (dudx >> (DiffFractBits-SrcFractBits+1));
	params.srcY = (params.srcY << SrcFractBits) + (dvdy >> (DiffFractBits-SrcFractBits+1));

	uint32_t newFlags = Blit2D_SetupEngine | Blit2D_OriginCorner | (flags & DkBlitFlag_Mode_Mask);
	if (flags & DkBlitFlag_FilterLinear)
		newFlags |= Blit2D_UseFilter;

	Blit2DEngine(obj, srcInfo, dstInfo, params, dudx, dvdy, newFlags, factor);
}

void dkCmdBufResolveImage(DkCmdBuf obj, DkImageView const* srcView, DkImageView const* dstView)
{
	obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_NotImplemented);
}

void dkCmdBufCopyBufferToImage(DkCmdBuf obj, DkCopyBuf const* src, DkImageView const* dstView, DkBlitRect const* dstRect, uint32_t flags)
{
#ifdef DEBUG
	if (!src || src->addr == DK_GPU_ADDR_INVALID)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!dstView || !dstView->pImage)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!dstRect)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	ImageInfo dstInfo;
	DkResult res = dstInfo.fromImageView(dstView, ImageInfo::TransferCopy);
#ifdef DEBUG
	if (res != DkResult_Success)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, res);
#endif

#ifndef DEBUG
	(void)res;
#endif

	ImageInfo srcInfo = {};
	srcInfo.m_iova = src->addr;
	srcInfo.m_horizontal = src->rowLength ? src->rowLength : dstRect->width*dstInfo.m_bytesPerBlock;
	srcInfo.m_vertical = dstRect->height;
	srcInfo.m_format = dstInfo.m_format;
	srcInfo.m_arrayMode = dstRect->depth;
	srcInfo.m_layerStride = src->imageHeight ? src->imageHeight : srcInfo.m_vertical*srcInfo.m_horizontal;
	srcInfo.m_width = dstRect->width;
	srcInfo.m_height = dstRect->height;
	srcInfo.m_widthMs = srcInfo.m_width;
	srcInfo.m_heightMs = srcInfo.m_height;
	srcInfo.m_bytesPerBlock = dstInfo.m_bytesPerBlock;
	srcInfo.m_isLinear = true;

	bool canUse2D = true;
	if (srcInfo.m_iova & (DK_IMAGE_LINEAR_STRIDE_ALIGNMENT-1))
		canUse2D = false; // Technically there are ways to work around this by extending width a bit, but w/e
	if (srcInfo.m_horizontal & (DK_IMAGE_LINEAR_STRIDE_ALIGNMENT-1))
		canUse2D = false;
	if (!(formatTraits[dstView->format ? dstView->format : dstView->pImage->m_format].flags & FormatTraitFlags_CanUse2DEngine))
		canUse2D = false;

#ifdef DEBUG
	bool needs2D = false;
	if (flags & (DkBlitFlag_FlipX))
		needs2D = true;

	if (needs2D && !canUse2D)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadState);
#endif

	BlitParams params;
	params.srcX = 0;
	params.srcY = 0;
	params.dstX = dstRect->x;
	params.dstY = dstRect->y;
	params.width = dstRect->width;
	params.height = dstRect->height;

	if (canUse2D)
	{
		int dudx = 1, dvdy = 1;

		if (flags & DkBlitFlag_FlipX)
		{
			params.srcX = params.width;
			dudx = -1;
		}

		if (flags & DkBlitFlag_FlipY)
		{
			params.srcY = params.height;
			dvdy = -1;
		}

		params.srcX = (params.srcX<<SrcFractBits) + (dudx<<(SrcFractBits-1));
		params.srcY = (params.srcY<<SrcFractBits) + (dvdy<<(SrcFractBits-1));
		uint32_t blitFlags = Blit2D_SetupEngine | Blit2D_OriginCorner;

		Blit2DEngine(obj, srcInfo, dstInfo, params, dudx<<DiffFractBits, dvdy<<DiffFractBits, blitFlags, 0);
	}
	else
	{
		if (flags & DkBlitFlag_FlipY)
		{
			srcInfo.m_iova += (params.height-1)*srcInfo.m_horizontal;
			srcInfo.m_horizontal = -srcInfo.m_horizontal;
		}

		BlitCopyEngine(obj, srcInfo, dstInfo, params);
	}
}

void dkCmdBufCopyImageToBuffer(DkCmdBuf obj, DkImageView const* srcView, DkBlitRect const* srcRect, DkCopyBuf const* dst, uint32_t flags)
{
	obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_NotImplemented);
}
