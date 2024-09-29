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

	constexpr uint32_t adjustSize(uint32_t size, unsigned level, uint32_t blockSize)
	{
		return adjustBlockSize(adjustMipSize(size, level), blockSize);
	}

	constexpr uint64_t alignLayerSize(uint64_t layerSize, uint32_t height, uint32_t depth, uint32_t blockH, uint32_t log2TileH, uint32_t log2TileD)
	{
		height = adjustBlockSize(height, blockH);
		while (log2TileH != 0 && height <= 8U << (log2TileH - 1))
			--log2TileH;

		while (log2TileD != 0 && depth <= 1U << (log2TileD - 1))
			--log2TileD;

		uint32_t gobsShift = log2TileH + log2TileD + 9;
		uint64_t sizeInGobs = layerSize >> gobsShift;

		if (layerSize != sizeInGobs << gobsShift)
			return (sizeInGobs + 1) << gobsShift;
		return layerSize;
	}
}

uint64_t DkImageLayout::calcLevelOffset(unsigned level) const
{
	uint32_t tileWidth  = 64 / m_bytesPerBlock; // non-sparse tile width is always zero
	uint32_t tileHeight = 8 << m_tileH;
	uint32_t tileDepth  = 1 << m_tileD;

	u64 offset = 0;
	for (unsigned i = 0; i < level; i ++)
	{
		uint32_t levelWidth  = adjustSize(m_dimensions[0]*m_samplesX, i, m_blockW);
		uint32_t levelHeight = m_dimsPerLayer>=2 ? adjustSize(m_dimensions[1]*m_samplesY, i, m_blockH) : 1;
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

		offset += uint64_t(levelWidthTiles*levelHeightTiles*levelDepthTiles) << (9 + levelTileWShift + levelTileHShift + levelTileDShift);
	}

	return offset;
}

void ImageInfo::fromImageView(DkImageView const* view, unsigned usage)
{
	DK_DEBUG_NON_NULL(view);
	DK_DEBUG_NON_NULL(view->pImage);

	DkImage const* image = view->pImage;
	DkImageType type = view->type ? view->type : image->m_type;
	DkImageFormat format = view->format ? view->format : image->m_format;
	FormatTraits const& traits = formatTraits[format];

	// Check type compatibility
	DK_DEBUG_BAD_INPUT(GetBaseImageType(type) != GetBaseImageType(image->m_type),
		"DkImageView format override has mismatching base type");
	if (format != image->m_format)
	{
		// Check format compatibility
		DK_DEBUG_BAD_INPUT(traits.bytesPerBlock != image->m_bytesPerBlock,
			"DkImageView format override has mismatching bpp");
		DK_DEBUG_BAD_INPUT(traits.blockWidth != image->m_blockW,
			"DkImageView format override has mismatching block width");
		DK_DEBUG_BAD_INPUT(traits.blockHeight != image->m_blockH,
			"DkImageView format override has mismatching block height");
	}

	[[maybe_unused]] const bool isRenderTarget = usage == ColorRenderTarget || usage == DepthRenderTarget;
	[[maybe_unused]] const bool formatIsDepth = traits.depthBits || traits.stencilBits;
	DK_DEBUG_BAD_FLAGS(isRenderTarget && !(image->m_flags & DkImageFlags_UsageRender),
		"image used as render target must have DkImageFlags_UsageRender set");
	DK_DEBUG_BAD_FLAGS(usage == Transfer2D && !(image->m_flags & DkImageFlags_Usage2DEngine),
		"image used with the 2D engine must have DkImageFlags_Usage2DEngine set");
	DK_DEBUG_BAD_INPUT(usage == ColorRenderTarget && formatIsDepth,
		"attempted to use depth image as color render target");
	DK_DEBUG_BAD_INPUT(usage == DepthRenderTarget && !formatIsDepth,
		"attempted to use color image as depth render target");
	DK_DEBUG_BAD_INPUT(usage == DepthRenderTarget && (type == DkImageType_3D || (image->m_flags & DkImageFlags_PitchLinear)),
		"depth render targets cannot be DkImageType_3D or DkImageFlags_PitchLinear");
	DK_DEBUG_BAD_INPUT(view->mipLevelOffset >= image->m_mipLevels, "mip level out of bounds");
	DK_DEBUG_BAD_INPUT(type == DkImageType_3D && (view->layerOffset || view->layerCount),
		"cannot use layerOffset/layerCount in DkImageView with DkImageType_3D images");

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
			DK_ERROR(DkResult_BadInput, "invalid overridden type in DkImageView");
#endif
	}

	m_iova   = image->m_iova;
	m_format = isRenderTarget ? traits.renderFmt : traits.engine2dFmt;
	m_width  = adjustSize(image->m_dimensions[0], view->mipLevelOffset, traits.blockWidth);
	m_height = adjustSize(image->m_dimensions[1], view->mipLevelOffset, traits.blockHeight);
	m_widthMs = adjustSize(image->m_dimensions[0]*image->m_samplesX, view->mipLevelOffset, traits.blockWidth);
	m_heightMs = adjustSize(image->m_dimensions[1]*image->m_samplesY, view->mipLevelOffset, traits.blockHeight);
	m_depth = type == DkImageType_3D ? adjustMipSize(image->m_dimensions[2], view->mipLevelOffset) : 1;
	m_bytesPerBlock = traits.bytesPerBlock;
	m_isLayered = layered;

	using TM = Engine3D::RenderTarget::TileMode;
	if (!(image->m_flags & DkImageFlags_PitchLinear))
	{
		unsigned tileWShift = image->m_tileW;
		unsigned tileHShift = image->m_tileH;
		unsigned tileDShift = image->m_tileD;
		if (view->mipLevelOffset)
		{
			m_iova += image->calcLevelOffset(view->mipLevelOffset);
			if (type != DkImageType_3D)
				tileHShift = adjustTileSize(tileHShift, 8, m_heightMs);
			else
				tileDShift = adjustTileSize(tileDShift, 1, m_depth);
		}

		if (view->layerOffset)
		{
			DK_DEBUG_BAD_INPUT(view->layerOffset >= image->m_dimensions[2],
				"DkImageView layerOffset out of bounds");
			m_iova += view->layerOffset * image->m_layerSize;
		}

		if (type == DkImageType_3D)
			m_arrayMode = m_depth;
		else if (layered)
		{
			if (view->layerCount)
			{
				DK_DEBUG_BAD_INPUT(view->layerOffset + view->layerCount > image->m_dimensions[2],
					"DkImageView layerOffset+layerCount out of bounds");
				m_arrayMode = view->layerCount;
			}
			else
				m_arrayMode = image->m_dimensions[2] - view->layerOffset;
		}
		else
			m_arrayMode = 1;

		uint32_t tileWidth = (64 / traits.bytesPerBlock) << tileWShift;
		m_horizontal  = (m_widthMs + tileWidth - 1) &~ (tileWidth - 1);
		m_vertical    = m_heightMs;
		m_tileMode    = TM::Width{tileWShift} | TM::Height{tileHShift} | TM::Depth{tileDShift};
		if (isRenderTarget)
			m_tileMode |= TM::Is3D{type==DkImageType_3D};
		m_layerStride = image->m_layerSize;
		if (isRenderTarget)
			m_layerStride >>= 2;
		m_isLinear = false;
	}
	else
	{
		DK_DEBUG_BAD_INPUT(layered, "pitch linear images cannot be layered");
		m_horizontal  = image->m_stride;
		m_vertical    = m_height;
		if (isRenderTarget)
			m_tileMode = TM::IsLinear{};
		m_arrayMode   = 1;
		m_layerStride = 0;
		m_isLinear = true;
	}
}

void dkImageLayoutInitialize(DkImageLayout* obj, DkImageLayoutMaker const* maker)
{
	DK_ENTRYPOINT(maker->device);
	DK_DEBUG_BAD_INPUT(maker->type <= DkImageType_None || maker->type > DkImageType_Buffer, "invalid type");
	DK_DEBUG_BAD_INPUT(maker->format <= DkImageFormat_None || maker->format >= DkImageFormat_Count, "invalid format");
	DK_DEBUG_BAD_INPUT((maker->type == DkImageType_2DMS || maker->type == DkImageType_2DMSArray) && maker->msMode == DkMsMode_1x,
		"cannot use multisampling mode with non-multisampling image types");
	DK_DEBUG_BAD_INPUT(!maker->mipLevels, "there needs to be at least one mip level");

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

	for (unsigned i = 0; i < (obj->m_dimsPerLayer + obj->m_hasLayers); i ++)
		DK_DEBUG_NON_ZERO(maker->dimensions[i]);

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

	// Ensure non-3D and non-layered types have a sensible value as their "number of layers"
	if (obj->m_type != DkImageType_3D && !obj->m_hasLayers)
		obj->m_dimensions[2] = 1;

	auto& traits = formatTraits[obj->m_format];
	obj->m_bytesPerBlock = traits.bytesPerBlock;
	obj->m_bytesPerBlockLog2 = __builtin_clz(obj->m_bytesPerBlock) ^ 0x1f;
	obj->m_blockW = traits.blockWidth;
	obj->m_blockH = traits.blockHeight;

	DK_DEBUG_BAD_FLAGS((obj->m_flags & DkImageFlags_UsageRender) && !(traits.flags & FormatTraitFlags_CanRender),
		"specified format does not support DkImageFlags_UsageRender");
	DK_DEBUG_BAD_FLAGS((obj->m_flags & DkImageFlags_UsageLoadStore) && !(traits.flags & FormatTraitFlags_CanLoadStore),
		"specified format does not support DkImageFlags_UsageLoadStore");
	DK_DEBUG_BAD_FLAGS((obj->m_flags & DkImageFlags_Usage2DEngine) && !(traits.flags & FormatTraitFlags_CanUse2DEngine),
		"specified format does not support DkImageFlags_Usage2DEngine");
	DK_DEBUG_BAD_INPUT((obj->m_flags & DkImageFlags_Usage2DEngine) && obj->m_dimsPerLayer > 2,
		"DkImageFlags_Usage2DEngine not supported with 3D images"); // 3D images with 2D engine: there are ways to work around it but it's ugly so we won't bother.
	if (obj->m_flags & DkImageFlags_UsagePresent)
	{
		DK_DEBUG_BAD_FLAGS(obj->m_flags & DkImageFlags_PitchLinear,
			"cannot use DkImageFlags_UsagePresent with DkImageFlags_PitchLinear");
		DK_DEBUG_BAD_INPUT(obj->m_dimsPerLayer != 2 || obj->m_hasLayers || obj->m_numSamplesLog2 != DkMsMode_1x,
			"presentable images must be 2D non-layered non-multisampled images");
	}

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
		DK_DEBUG_BAD_INPUT(obj->m_dimsPerLayer != 2 || obj->m_hasLayers,
			"pitch linear images must be 2D non-layered");
		DK_DEBUG_BAD_INPUT(obj->m_mipLevels > 1,
			"pitch linear images cannot have more than one mipmap level");

		if (obj->m_flags & DkImageFlags_UsageRender)
			obj->m_alignment = 128; // see TRM 20.1 "Tiling Formats", also supported by nouveau
		else
			obj->m_alignment = 32; // TRM implies this should be 64, but 32 has been observed instead in official software

		if (maker->pitchStride)
			obj->m_stride = maker->pitchStride;
		else
			obj->m_stride = (obj->m_bytesPerBlock * obj->m_dimensions[0] + obj->m_alignment - 1) &~ (obj->m_alignment - 1);

		obj->m_layerSize = obj->m_stride * obj->m_dimensions[1];
		obj->m_storageSize = obj->m_layerSize;
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
			uint32_t actualDepth = obj->m_dimensions[2];
			uint32_t depthAndHalf = actualDepth + actualDepth/2;
			obj->m_tileD = pickTileSize(depthAndHalf);
		}
	}

	// Pick a memory kind for this image
	obj->m_memKind = pickImageMemoryKind(traits, obj->m_numSamplesLog2,
		(obj->m_flags & DkImageFlags_HwCompression) != 0,
		(obj->m_flags & DkImageFlags_Z16EnableZbc) != 0);

	obj->m_layerSize = obj->calcLevelOffset(obj->m_mipLevels);
	if (obj->m_hasLayers)
		obj->m_layerSize = alignLayerSize(obj->m_layerSize, obj->m_dimensions[1], obj->m_dimensions[2], obj->m_blockH, obj->m_tileH, obj->m_tileD);

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
	DK_ENTRYPOINT(memBlock);
	DK_DEBUG_DATA_ALIGN(offset, layout->m_alignment);
	DK_DEBUG_BAD_FLAGS(layout->m_memKind != NvKind_Pitch && !memBlock->isImage(), "DkMemBlock must be created with DkMemBlockFlags_Image");

	memcpy(obj, layout, sizeof(*layout));
	obj->m_iova = memBlock->getGpuAddrForImage(offset, layout->m_storageSize, (NvKind)layout->m_memKind);
	obj->m_memBlock = memBlock;
	obj->m_memOffset = offset;
}

DkGpuAddr dkImageGetGpuAddr(DkImage const* obj)
{
	return obj->m_iova;
}

void dkCmdBufCopyImage(DkCmdBuf obj, DkImageView const* srcView, DkImageRect const* srcRect, DkImageView const* dstView, DkImageRect const* dstRect, uint32_t flags)
{
	DK_ENTRYPOINT(obj);

	ImageInfo srcInfo, dstInfo;
	srcInfo.fromImageView(srcView, ImageInfo::TransferCopy);
	dstInfo.fromImageView(dstView, ImageInfo::TransferCopy);

	[[maybe_unused]] const bool isSrcTexCompressed = srcView->pImage->m_blockW > 1;
	[[maybe_unused]] const bool isDstTexCompressed = dstView->pImage->m_blockW > 1;
	DK_DEBUG_BAD_INPUT(isSrcTexCompressed != isDstTexCompressed, "mismatched compression attributes");
	DK_DEBUG_BAD_INPUT(isSrcTexCompressed && srcInfo.m_format != dstInfo.m_format, "compression formats must match");

	DK_DEBUG_BAD_INPUT(!srcRect || !srcRect->width || !srcRect->height || !srcRect->depth, "invalid srcRect");
	DK_DEBUG_BAD_INPUT(srcRect->x + srcRect->width > srcView->pImage->m_dimensions[0], "srcRect x/width out of bounds");
	DK_DEBUG_BAD_INPUT(srcRect->y + srcRect->height > srcView->pImage->m_dimensions[1], "srcRect y/height out of bounds");
	DK_DEBUG_BAD_INPUT(srcRect->z + srcRect->depth > srcInfo.m_arrayMode, "srcRect z/depth out of bounds");

	DK_DEBUG_BAD_INPUT(!dstRect || !dstRect->width || !dstRect->height || !dstRect->depth, "invalid dstRect");
	DK_DEBUG_BAD_INPUT(dstRect->x + dstRect->width > dstView->pImage->m_dimensions[0], "dstRect x/width out of bounds");
	DK_DEBUG_BAD_INPUT(dstRect->y + dstRect->height > dstView->pImage->m_dimensions[1], "dstRect y/height out of bounds");
	DK_DEBUG_BAD_INPUT(dstRect->z + dstRect->depth > dstInfo.m_arrayMode, "dstRect z/depth out of bounds");

	DK_DEBUG_BAD_INPUT(srcRect->width != dstRect->width, "srcRect/dstRect width must match");
	DK_DEBUG_BAD_INPUT(srcRect->height != dstRect->height, "srcRect/dstRect height must match");
	DK_DEBUG_BAD_INPUT(srcRect->depth != dstRect->depth, "srcRect/dstRect depth must match");

	BlitParams params;
	params.srcX = srcRect->x;
	params.srcY = srcRect->y;
	params.dstX = dstRect->x;
	params.dstY = dstRect->y;
	params.width = srcRect->width;
	params.height = srcRect->height;

	for (uint32_t z = 0; z < dstRect->depth; z ++)
	{
		uint32_t srcZ = z;
		uint32_t dstZ = dstRect->z + z;
		if (flags & DkBlitFlag_FlipZ)
			srcZ = dstRect->depth - z - 1;
		BlitCopyEngine(obj, srcInfo, dstInfo, params, srcZ, dstZ);
	}
}

void dkCmdBufBlitImage(DkCmdBuf obj, DkImageView const* srcView, DkImageRect const* srcRect, DkImageView const* dstView, DkImageRect const* dstRect, uint32_t flags, uint32_t factor)
{
	DK_ENTRYPOINT(obj);

	ImageInfo srcInfo, dstInfo;
	srcInfo.fromImageView(srcView, ImageInfo::Transfer2D);
	dstInfo.fromImageView(dstView, ImageInfo::Transfer2D);

	[[maybe_unused]] const bool isSrcTexCompressed = srcView->pImage->m_blockW > 1;
	[[maybe_unused]] const bool isDstTexCompressed = dstView->pImage->m_blockW > 1;
	DK_DEBUG_BAD_INPUT(isSrcTexCompressed != isDstTexCompressed, "mismatched compression attributes");
	DK_DEBUG_BAD_INPUT(isSrcTexCompressed && srcInfo.m_format != dstInfo.m_format, "compression formats must match");

	DK_DEBUG_BAD_INPUT(!srcRect || !srcRect->width || !srcRect->height || !srcRect->depth, "invalid srcRect");
	DK_DEBUG_BAD_INPUT(srcRect->x + srcRect->width > srcView->pImage->m_dimensions[0], "srcRect x/width out of bounds");
	DK_DEBUG_BAD_INPUT(srcRect->y + srcRect->height > srcView->pImage->m_dimensions[1], "srcRect y/height out of bounds");
	DK_DEBUG_BAD_INPUT(srcRect->z + srcRect->depth > srcInfo.m_arrayMode, "srcRect z/depth out of bounds");

	DK_DEBUG_BAD_INPUT(!dstRect || !dstRect->width || !dstRect->height || !dstRect->depth, "invalid dstRect");
	DK_DEBUG_BAD_INPUT(dstRect->x + dstRect->width > dstView->pImage->m_dimensions[0], "dstRect x/width out of bounds");
	DK_DEBUG_BAD_INPUT(dstRect->y + dstRect->height > dstView->pImage->m_dimensions[1], "dstRect y/height out of bounds");
	DK_DEBUG_BAD_INPUT(dstRect->z + dstRect->depth > dstInfo.m_arrayMode, "dstRect z/depth out of bounds");

	DK_DEBUG_BAD_INPUT(srcRect->depth != dstRect->depth, "srcRect/dstRect depth must match");

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

	if (srcRect->z)
		srcInfo.m_iova += srcRect->z * srcInfo.m_layerStride;
	if (dstRect->z)
		dstInfo.m_iova += dstRect->z * dstInfo.m_layerStride;

	int64_t srcLayerStride = srcInfo.m_layerStride;
	if (flags & DkBlitFlag_FlipZ)
	{
		srcInfo.m_iova += (srcRect->depth - 1) * srcInfo.m_layerStride;
		srcLayerStride = -srcInfo.m_layerStride;
	}

	for (uint32_t z = 0; z < dstRect->depth; z ++)
	{
		Blit2DEngine(obj, srcInfo, dstInfo, params, dudx, dvdy, newFlags, factor);
		srcInfo.m_iova += srcLayerStride;
		dstInfo.m_iova += dstInfo.m_layerStride;
		newFlags &= ~Blit2D_SetupEngine;
	}
}

void dkCmdBufResolveImage(DkCmdBuf obj, DkImageView const* srcView, DkImageView const* dstView)
{
	DK_ENTRYPOINT(obj);

	ImageInfo srcInfo, dstInfo;
	srcInfo.fromImageView(srcView, ImageInfo::Transfer2D);
	dstInfo.fromImageView(dstView, ImageInfo::Transfer2D);

	DK_DEBUG_BAD_INPUT(!srcView->pImage->m_numSamplesLog2 || dstView->pImage->m_numSamplesLog2,
		"must specify multisampled source and non-multisampled destination");
	DK_DEBUG_BAD_INPUT(srcInfo.m_isLayered || dstInfo.m_isLayered,
		"cannot use layered source/destination");
	DK_DEBUG_BAD_INPUT(srcInfo.m_width != dstInfo.m_width, "mismatched width");
	DK_DEBUG_BAD_INPUT(srcInfo.m_height != dstInfo.m_height, "mismatched height");

	unsigned samplesX = srcView->pImage->m_samplesX;
	unsigned samplesY = srcView->pImage->m_samplesY;

	BlitParams params;
	params.srcX = (samplesX-1) << (SrcFractBits-1);
	params.srcY = (samplesY-1) << (SrcFractBits-1);
	params.dstX = 0;
	params.dstY = 0;
	params.width = srcInfo.m_width;
	params.height = srcInfo.m_height;

	Blit2DEngine(obj, srcInfo, dstInfo, params,
		samplesX<<DiffFractBits, samplesY<<DiffFractBits,
		Blit2D_SetupEngine | Blit2D_UseFilter, 0);
}

void dkCmdBufCopyBufferToImage(DkCmdBuf obj, DkCopyBuf const* src, DkImageView const* dstView, DkImageRect const* dstRect, uint32_t flags)
{
	DK_ENTRYPOINT(obj);

	ImageInfo dstInfo;
	dstInfo.fromImageView(dstView, ImageInfo::TransferCopy);

	DK_DEBUG_BAD_INPUT(!src || src->addr == DK_GPU_ADDR_INVALID, "invalid src");
	DK_DEBUG_BAD_INPUT(!dstRect || !dstRect->width || !dstRect->height || !dstRect->depth, "invalid dstView");
	DK_DEBUG_BAD_INPUT(dstRect->x + dstRect->width > dstView->pImage->m_dimensions[0], "dstRect x/width out of bounds");
	DK_DEBUG_BAD_INPUT(dstRect->y + dstRect->height > dstView->pImage->m_dimensions[1], "dstRect y/height out of bounds");
	DK_DEBUG_BAD_INPUT(dstRect->z + dstRect->depth > dstInfo.m_arrayMode, "dstRect z/depth out of bounds");

	BlitParams params;
	params.srcX = 0;
	params.srcY = 0;
	params.dstX = dstRect->x;
	params.dstY = dstRect->y;
	params.width = dstRect->width;
	params.height = dstRect->height;

	auto& traits = formatTraits[dstView->format ? dstView->format : dstView->pImage->m_format];
	const bool isCompressed = traits.blockWidth > 1 || traits.blockHeight > 1;

	if (isCompressed)
	{
		// Horizontal/vertical flips can't be used with compressed formats for obvious reasons
		DK_DEBUG_BAD_FLAGS(flags & (DkBlitFlag_FlipX|DkBlitFlag_FlipY), "cannot use DkBlitFlag_FlipX/FlipY with compressed formats");

		params.dstX = adjustBlockSize(params.dstX, traits.blockWidth);
		params.dstY = adjustBlockSize(params.dstY, traits.blockHeight);
		params.width = adjustBlockSize(params.width, traits.blockWidth);
		params.height = adjustBlockSize(params.height, traits.blockHeight);
	}

	ImageInfo srcInfo = {};
	srcInfo.m_iova = src->addr;
	srcInfo.m_horizontal = src->rowLength ? src->rowLength : params.width*dstInfo.m_bytesPerBlock;
	srcInfo.m_vertical = dstRect->height;
	srcInfo.m_format = dstInfo.m_format;
	srcInfo.m_arrayMode = dstRect->depth;
	srcInfo.m_layerStride = src->imageHeight ? src->imageHeight : params.height*srcInfo.m_horizontal;
	srcInfo.m_width = dstRect->width;
	srcInfo.m_height = dstRect->height;
	srcInfo.m_widthMs = srcInfo.m_width;
	srcInfo.m_heightMs = srcInfo.m_height;
	srcInfo.m_bytesPerBlock = dstInfo.m_bytesPerBlock;
	srcInfo.m_isLinear = true;
	srcInfo.m_isLayered = true;

	bool needs2D = false;
	if (flags & (DkBlitFlag_FlipX))
		needs2D = true;

	if (needs2D)
	{
		// Check whether we can really use the 2D engine
		DK_DEBUG_BAD_FLAGS(!(dstView->pImage->m_flags & DkImageFlags_Usage2DEngine), "DkImageFlags_Usage2DEngine required in destination image");
		DK_DEBUG_BAD_FLAGS(!(traits.flags & FormatTraitFlags_CanUse2DEngine), "specified format does not support DkImageFlags_Usage2DEngine");
		DK_DEBUG_DATA_ALIGN(srcInfo.m_iova, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT); // Technically there are ways to work around this by extending width a bit, but w/e
		DK_DEBUG_SIZE_ALIGN(srcInfo.m_horizontal, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
		DK_DEBUG_BAD_INPUT(isCompressed, "cannot use 2D engine with compressed formats"); // Technically the 2D engine *can* be used with compressed formats, but there isn't really any point in doing so

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

		if (dstRect->z)
			dstInfo.m_iova += dstRect->z * dstInfo.m_layerStride;

		int64_t srcLayerStride = srcInfo.m_layerStride;
		if (flags & DkBlitFlag_FlipZ)
		{
			srcInfo.m_iova += (dstRect->depth - 1) * srcInfo.m_layerStride;
			srcLayerStride = -srcInfo.m_layerStride;
		}

		for (uint32_t z = 0; z < dstRect->depth; z ++)
		{
			Blit2DEngine(obj, srcInfo, dstInfo, params, dudx<<DiffFractBits, dvdy<<DiffFractBits, blitFlags, 0);
			srcInfo.m_iova += srcLayerStride;
			dstInfo.m_iova += dstInfo.m_layerStride;
			blitFlags &= ~Blit2D_SetupEngine;
		}
	}
	else
	{
		if (flags & DkBlitFlag_FlipY)
		{
			srcInfo.m_iova += (params.height-1)*srcInfo.m_horizontal;
			srcInfo.m_horizontal = -srcInfo.m_horizontal;
		}

		for (uint32_t z = 0; z < dstRect->depth; z ++)
		{
			uint32_t srcZ = z;
			uint32_t dstZ = dstRect->z + z;
			if (flags & DkBlitFlag_FlipZ)
				srcZ = dstRect->depth - z - 1;
			BlitCopyEngine(obj, srcInfo, dstInfo, params, srcZ, dstZ);
		}
	}
}

void dkCmdBufCopyImageToBuffer(DkCmdBuf obj, DkImageView const* srcView, DkImageRect const* srcRect, DkCopyBuf const* dst, uint32_t flags)
{
	DK_ENTRYPOINT(obj);

	ImageInfo srcInfo;
	srcInfo.fromImageView(srcView, ImageInfo::TransferCopy);

	DK_DEBUG_BAD_INPUT(!dst || dst->addr == DK_GPU_ADDR_INVALID, "invalid dst");
	DK_DEBUG_BAD_INPUT(!srcRect || !srcRect->width || !srcRect->height || !srcRect->depth, "invalid dstView");
	DK_DEBUG_BAD_INPUT(srcRect->x + srcRect->width > srcView->pImage->m_dimensions[0], "srcRect x/width out of bounds");
	DK_DEBUG_BAD_INPUT(srcRect->y + srcRect->height > srcView->pImage->m_dimensions[1], "srcRect y/height out of bounds");
	DK_DEBUG_BAD_INPUT(srcRect->z + srcRect->depth > srcInfo.m_arrayMode, "srcRect z/depth out of bounds");

	BlitParams params;
	params.srcX = srcRect->x;
	params.srcY = srcRect->y;
	params.dstX = 0;
	params.dstY = 0;
	params.width = srcRect->width;
	params.height = srcRect->height;

	auto& traits = formatTraits[srcView->format ? srcView->format : srcView->pImage->m_format];
	const bool isCompressed = traits.blockWidth > 1 || traits.blockHeight > 1;

	if (isCompressed)
	{
		// Horizontal/vertical flips can't be used with compressed formats for obvious reasons
		DK_DEBUG_BAD_FLAGS(flags & (DkBlitFlag_FlipX|DkBlitFlag_FlipY), "cannot use DkBlitFlag_FlipX/FlipY with compressed formats");

		params.srcX = adjustBlockSize(params.srcX, traits.blockWidth);
		params.srcY = adjustBlockSize(params.srcY, traits.blockHeight);
		params.width = adjustBlockSize(params.width, traits.blockWidth);
		params.height = adjustBlockSize(params.height, traits.blockHeight);
	}

	ImageInfo dstInfo = {};
	dstInfo.m_iova = dst->addr;
	dstInfo.m_horizontal = dst->rowLength ? dst->rowLength : params.width*srcInfo.m_bytesPerBlock;
	dstInfo.m_vertical = srcRect->height;
	dstInfo.m_format = srcInfo.m_format;
	dstInfo.m_arrayMode = srcRect->depth;
	dstInfo.m_layerStride = dst->imageHeight ? dst->imageHeight : params.height*dstInfo.m_horizontal;
	dstInfo.m_width = srcRect->width;
	dstInfo.m_height = srcRect->height;
	dstInfo.m_widthMs = dstInfo.m_width;
	dstInfo.m_heightMs = dstInfo.m_height;
	dstInfo.m_bytesPerBlock = srcInfo.m_bytesPerBlock;
	dstInfo.m_isLinear = true;
	dstInfo.m_isLayered = true;

	bool needs2D = false;
	if (flags & (DkBlitFlag_FlipX))
		needs2D = true;

	if (needs2D)
	{
		// Check whether we can really use the 2D engine
		DK_DEBUG_BAD_FLAGS(!(srcView->pImage->m_flags & DkImageFlags_Usage2DEngine), "DkImageFlags_Usage2DEngine required in destination image");
		DK_DEBUG_BAD_FLAGS(!(traits.flags & FormatTraitFlags_CanUse2DEngine), "specified format does not support DkImageFlags_Usage2DEngine");
		DK_DEBUG_DATA_ALIGN(dstInfo.m_iova, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT); // Technically there are ways to work around this by extending width a bit, but w/e
		DK_DEBUG_SIZE_ALIGN(dstInfo.m_horizontal, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
		DK_DEBUG_BAD_INPUT(isCompressed, "cannot use 2D engine with compressed formats"); // Technically the 2D engine *can* be used with compressed formats, but there isn't really any point in doing so

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

		if (srcRect->z)
			srcInfo.m_iova += srcRect->z * srcInfo.m_layerStride;

		int64_t srcLayerStride = dstInfo.m_layerStride;
		if (flags & DkBlitFlag_FlipZ)
		{
			srcInfo.m_iova += (srcRect->depth - 1) * srcInfo.m_layerStride;
			srcLayerStride = -srcInfo.m_layerStride;
		}

		for (uint32_t z = 0; z < srcRect->depth; z ++)
		{
			Blit2DEngine(obj, srcInfo, dstInfo, params, dudx<<DiffFractBits, dvdy<<DiffFractBits, blitFlags, 0);
			srcInfo.m_iova += srcLayerStride;
			dstInfo.m_iova += srcInfo.m_layerStride;
			blitFlags &= ~Blit2D_SetupEngine;
		}
	}
	else
	{
		if (flags & DkBlitFlag_FlipY)
		{
			dstInfo.m_iova += (params.height-1)*dstInfo.m_horizontal;
			dstInfo.m_horizontal = -dstInfo.m_horizontal;
		}

		for (uint32_t z = 0; z < srcRect->depth; z ++)
		{
			uint32_t srcZ = srcRect->z + z;
			uint32_t dstZ = z;
			if (flags & DkBlitFlag_FlipZ)
				srcZ = srcRect->depth - z - 1;
			BlitCopyEngine(obj, srcInfo, dstInfo, params, srcZ, dstZ);
		}
	}
}
