#include "../dk_image_descriptor.h"

using namespace dk::detail;
using namespace maxwell;

namespace
{
	constexpr ImageSwizzle doSwizzle(FormatTraits const& t, DkImageSwizzle swizzle)
	{
		switch (swizzle)
		{
			default:
			case DkImageSwizzle_Zero:
				return ImageSwizzle_Zero;
			case DkImageSwizzle_One:
				return (t.flags & FormatTraitFlags_IsRawInt) ? ImageSwizzle_OneInt : ImageSwizzle_OneFloat;
			case DkImageSwizzle_Red:
				return (ImageSwizzle)t.ticFmt.swizzle_x;
			case DkImageSwizzle_Green:
				return (ImageSwizzle)t.ticFmt.swizzle_y;
			case DkImageSwizzle_Blue:
				return (ImageSwizzle)t.ticFmt.swizzle_z;
			case DkImageSwizzle_Alpha:
				return (ImageSwizzle)t.ticFmt.swizzle_w;
		}
	}
};

void dkImageDescriptorInitialize(DkImageDescriptor* obj, DkImageView const* view, bool usesLoadOrStore, bool decayMS)
{
	memset(obj, 0, sizeof(*obj));

	DkImage const* image = view->pImage;
	DkImageType type = view->type ? view->type : image->m_type;
	DkImageFormat format = view->format ? view->format : image->m_format;
	FormatTraits const& traits = formatTraits[format];

	obj->format_word = traits.ticFmt;
	if (!traits.depthBits)
	{
		obj->format_word.swizzle_x = doSwizzle(traits, view->swizzle[0]);
		obj->format_word.swizzle_y = doSwizzle(traits, view->swizzle[1]);
		obj->format_word.swizzle_z = doSwizzle(traits, view->swizzle[2]);
		obj->format_word.swizzle_w = doSwizzle(traits, view->swizzle[3]);
	}
	else
	{
		bool isD24x8     = format == DkImageFormat_Z24X8 || format == DkImageFormat_Z24S8;
		bool wantStencil = view->dsSource == DkDsSource_Stencil;
		ImageSwizzle sw  = (isD24x8 ^ wantStencil) ? ImageSwizzle_G : ImageSwizzle_R;
		obj->format_word.swizzle_x = sw;
		obj->format_word.swizzle_y = sw;
		obj->format_word.swizzle_z = sw;
		obj->format_word.swizzle_w = wantStencil ? sw : ImageSwizzle_OneFloat;
	}

	obj->is_sRGB = (traits.flags & FormatTraitFlags_IsSrgb) != 0;

	if (type == DkImageType_Buffer)
	{
		obj->hdr_version = TicHdrVersion_1DBuffer;
		obj->texture_type = TextureType_1DBuffer;
	}
	else if (image->m_flags & DkImageFlags_PitchLinear)
	{
		obj->hdr_version = TicHdrVersion_Pitch;
		obj->texture_type = TextureType_2DNoMipmap;
		obj->normalized_coords = type != DkImageType_Rectangle;
		obj->border_size = BorderSize_SamplerColor;
		obj->sector_promotion = SectorPromotion_None;
		obj->pitch_5_20 = image->m_stride >> 5;
	}
	else
	{
		static const uint8_t dkTypeToMaxwell[] =
		{
			[DkImageType_1D          -1] = TextureType_1D,
			[DkImageType_2D          -1] = TextureType_2D,
			[DkImageType_3D          -1] = TextureType_3D,
			[DkImageType_1DArray     -1] = TextureType_1DArray,
			[DkImageType_2DArray     -1] = TextureType_2DArray,
			[DkImageType_2DMS        -1] = TextureType_2D,
			[DkImageType_2DMSArray   -1] = TextureType_2DArray,
			[DkImageType_Rectangle   -1] = TextureType_2D,
			[DkImageType_Cubemap     -1] = TextureType_Cubemap,
			[DkImageType_CubemapArray-1] = TextureType_CubeArray,
			[DkImageType_Buffer      -1] = TextureType_1D,
		};
		obj->hdr_version = TicHdrVersion_BlockLinear;
		if (usesLoadOrStore && (type == DkImageType_Cubemap || type == DkImageType_CubemapArray))
			obj->texture_type = TextureType_2DArray;
		else
			obj->texture_type = dkTypeToMaxwell[type-1];
		obj->normalized_coords = type != DkImageType_Rectangle;
		obj->border_size = BorderSize_SamplerColor;
		obj->sector_promotion = SectorPromotion_To2V;
		obj->lod_aniso_quality_2 = 1;
		obj->tile_width_gobs_log2 = 0;
		obj->tile_height_gobs_log2 = image->m_tileH;
		obj->tile_depth_gobs_log2 = image->m_tileD;
		obj->lod_aniso_quality = LodQuality_High;
		obj->lod_iso_quality = LodQuality_High;
	}

	if (!decayMS)
		obj->msaa_mode = getMsaaMode(image->m_numSamplesLog2);

	DkGpuAddr iova = image->m_iova;
	uint32_t width = image->m_dimensions[0];
	uint32_t height = image->m_dimensions[1];
	uint32_t depth = image->m_dimensions[2];

	if (decayMS)
	{
		width *= image->m_samplesX;
		height *= image->m_samplesY;
	}

	if (type == DkImageType_Buffer)
	{
		obj->width_minus_one_16_31 = (width - 1) >> 16;
		obj->width_minus_one       =  width - 1;
	}
	else
	{
		if (type == DkImageType_1D || type == DkImageType_1DArray)
			height = 1;

		if (type == DkImageType_1DArray || type == DkImageType_2DArray || type == DkImageType_2DMSArray || type == DkImageType_CubemapArray)
		{
			iova += view->layerOffset * image->m_layerSize;
			uint32_t layerCount;
			if (view->layerCount)
				layerCount = view->layerCount;
			else
				layerCount = depth - view->layerOffset;
			if (type != DkImageType_CubemapArray || usesLoadOrStore)
				depth = layerCount;
			else
				depth = layerCount / 6;
		}
		else if (type == DkImageType_Cubemap)
			depth = usesLoadOrStore ? 6 : 1;
		else if (type != DkImageType_3D)
			depth = 1;

		obj->width_minus_one = width - 1;
		obj->height_minus_one = height - 1;
		obj->depth_minus_one = depth - 1;
		obj->sparse_tile_width_gobs_log2 = image->m_tileW;
		obj->aniso_coarse_spread_modifier = AnisoSpreadModifier_None;
		obj->aniso_spread_scale = 0;
		obj->aniso_fine_spread_func = AnisoSpreadFunc_Two;
		obj->aniso_coarse_spread_func = AnisoSpreadFunc_One;
		obj->aniso_fine_spread_modifier = AnisoSpreadModifier_None;

		uint32_t maxMipLevel = image->m_mipLevels >= 16 ? 15 : (image->m_mipLevels-1);
		uint32_t maxMipLevelView = maxMipLevel;
		uint32_t minMipLevelView = view->mipLevelOffset;
		if (minMipLevelView > maxMipLevel)
			minMipLevelView = maxMipLevel;
		if (view->mipLevelCount)
		{
			maxMipLevelView = minMipLevelView + view->mipLevelCount - 1;
			if (maxMipLevelView > maxMipLevel)
				maxMipLevelView = maxMipLevel;
		}

		obj->view_mip_min_level = minMipLevelView;
		obj->view_mip_max_level = maxMipLevelView;
		obj->mip_max_levels = maxMipLevel;
	}

	obj->address_low = iova;
	obj->address_high = iova >> 32;

	obj->load_store_hint_maybe = usesLoadOrStore;
	obj->is_sparse = 0; // Not supported yet
	obj->view_layer_base_0_2  = view->layerOffset;
	obj->view_layer_base_3_7  = view->layerOffset >> 3;
	obj->view_layer_base_8_10 = view->layerOffset >> 8;
}
