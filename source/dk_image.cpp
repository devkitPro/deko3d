#include "dk_image.h"
#include "dk_device.h"
#include "dk_memblock.h"

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
	uint32_t tileWidth  = (64 / m_bytesPerBlock) << m_tileW;
	uint32_t tileHeight = 8 << m_tileH;
	uint32_t tileDepth  = 1 << m_tileD;

	m_layerSize = 0;
	for (unsigned i = 0; i < m_mipLevels; i ++)
	{
		uint32_t levelWidth  = adjustBlockSize(adjustMipSize(m_dimensions[0]*m_samplesX, i), m_blockW);
		uint32_t levelHeight = m_dimsPerLayer>=2 ? adjustBlockSize(adjustMipSize(m_dimensions[1]*m_samplesY, i), m_blockH) : 1;
		uint32_t levelDepth  = m_dimsPerLayer>=3 ? adjustMipSize(m_dimensions[2], i) : 1;
		uint32_t levelWidthBytes = levelWidth << m_bytesPerBlockLog2;

		uint32_t levelTileWShift = adjustTileSize(m_tileW, 64, levelWidthBytes);
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
			// I don't understand this logic - we're mismatching width in tiles with width in gobs?
			// Thankfully this codepath isn't currently taken anyway, since m_tileW is always zero.
			uint32_t align = 1U << m_tileW;
			levelWidthTiles = (levelWidthTiles + align - 1) &~ (align - 1);
		}

		m_layerSize += uint64_t(levelWidthTiles*levelHeightTiles*levelDepthTiles) << (9 + levelTileWShift + levelTileHShift + levelTileDShift);
	}
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
#endif
	if (traits.depthBits || traits.stencilBits)
		obj->m_flags |= DkImageFlags_HwCompression; // Hardware compression is mandatory for depth/stencil images

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

	if (!(obj->m_flags & DkImageFlags_HwCompression))
		obj->m_memKind = NvKind_Generic_16BX2;
	else
	{
		// TODO: Pick a compressed memory kind
		obj->m_memKind = NvKind_Generic_16BX2;
	}

	obj->calcLayerSize();
	if (obj->m_hasLayers)
		obj->m_storageSize = obj->m_dimensions[2] * obj->m_layerSize;
	else
		obj->m_storageSize = obj->m_layerSize;

	if (obj->m_memKind != NvKind_Pitch && obj->m_memKind != NvKind_Generic_16BX2)
	{
		// Since we are using a compressed memory kind, we need to align the image and its size
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
