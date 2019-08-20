#include "dk_image.h"
#include "dk_device.h"

using namespace dk::detail;
using namespace maxwell;

namespace
{
	DkTileSize pickTileSize(uint32_t gobs)
	{
		if (gobs >= 16) return DkTileSize_SixteenGobs;
		if (gobs >= 8)  return DkTileSize_EightGobs;
		if (gobs >= 4)  return DkTileSize_FourGobs;
		if (gobs >= 2)  return DkTileSize_TwoGobs;
		return DkTileSize_OneGob;
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
	if (maker->type != DkImageType_2DMS && maker->type != DkImageType_2DMSArray && maker->msMode == DkMsMode_1x)
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
		if (obj->m_dimsPerLayer == 2)
		{
			// Calculate actual height, and multiply it by 1.5
			uint32_t actualHeight = obj->m_dimensions[1] * obj->m_samplesY;
			if (obj->m_blockH > 1)
				actualHeight = (actualHeight + obj->m_blockH - 1) / obj->m_blockH;
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

	// TODO: Calculate layer size
	// TODO: Calculate storage size
}
