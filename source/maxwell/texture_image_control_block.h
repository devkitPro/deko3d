#pragma once
#include "image_formats.h"

namespace maxwell
{

enum TicHdrVersion
{
	TicHdrVersion_1DBuffer            = 0,
	TicHdrVersion_PitchColorKey       = 1,
	TicHdrVersion_Pitch               = 2,
	TicHdrVersion_BlockLinear         = 3,
	TicHdrVersion_BlockLinearColorKey = 4,
};

enum TextureType
{
	TextureType_1D         = 0,
	TextureType_2D         = 1,
	TextureType_3D         = 2,
	TextureType_Cubemap    = 3,
	TextureType_1DArray    = 4,
	TextureType_2DArray    = 5,
	TextureType_1DBuffer   = 6,
	TextureType_2DNoMipmap = 7,
	TextureType_CubeArray  = 8,
};

enum MsaaMode
{
	MsaaMode_1x1      = 0,
	MsaaMode_2x1      = 1,
	MsaaMode_2x2      = 2,
	MsaaMode_4x2      = 3,
	MsaaMode_4x2_D3D  = 4,
	MsaaMode_2x1_D3D  = 5,
	MsaaMode_4x4      = 6,
	MsaaMode_2x2_VC4  = 8,
	MsaaMode_2x2_VC12 = 9,
	MsaaMode_4x2_VC8  = 10,
	MsaaMode_4x2_VC24 = 11,
};

enum LodQuality
{
	LodQuality_Low  = 0,
	LodQuality_High = 1,
};

enum SectorPromotion
{
	SectorPromotion_None = 0,
	SectorPromotion_To2V = 1,
	SectorPromotion_To2H = 2,
	SectorPromotion_To4  = 3,
};

enum BorderSize
{
	BorderSize_One          = 0,
	BorderSize_Two          = 1,
	BorderSize_Four         = 2,
	BorderSize_Eight        = 3,
	BorderSize_SamplerColor = 7,
};

enum AnisoSpreadModifier
{
	AnisoSpreadModifier_None = 0,
	AnisoSpreadModifier_One  = 1,
	AnisoSpreadModifier_Two  = 2,
	AnisoSpreadModifier_Sqrt = 3,
};

enum AnisoSpreadFunc
{
	AnisoSpreadFunc_Half = 0,
	AnisoSpreadFunc_One  = 1,
	AnisoSpreadFunc_Two  = 2,
	AnisoSpreadFunc_Max  = 3,
};

enum MaxAnisotropy
{
	MaxAnisotropy_1to1  = 0,
	MaxAnisotropy_2to1  = 1,
	MaxAnisotropy_4to1  = 2,
	MaxAnisotropy_6to1  = 3,
	MaxAnisotropy_8to1  = 4,
	MaxAnisotropy_10to1 = 5,
	MaxAnisotropy_12to1 = 6,
	MaxAnisotropy_16to1 = 7,
};

// Texture Image Control Block, for Maxwell 2nd gen
// See https://github.com/envytools/envytools/blob/master/rnndb/graph/gm200_texture.xml
struct TextureImageControl
{
	// 0x00
	TicFormatWord format_word;

	// 0x04
	uint32_t address_low;

	// 0x08
	uint32_t address_high          : 16;
	uint32_t view_layer_base_3_7   : 5;
	uint32_t hdr_version           : 3;
	uint32_t load_store_hint_maybe : 1;
	uint32_t view_coherency_hash   : 4;
	uint32_t view_layer_base_8_10  : 3;

	// 0x0C
	union
	{
		uint16_t width_minus_one_16_31;
		uint16_t pitch_5_20;
		struct
		{
			uint16_t tile_width_gobs_log2        : 3;
			uint16_t tile_height_gobs_log2       : 3;
			uint16_t tile_depth_gobs_log2        : 3;
			uint16_t                             : 1;
			uint16_t sparse_tile_width_gobs_log2 : 3;
			uint16_t gob_3d                      : 1;
			uint16_t                             : 2;
		};
	};
	uint16_t lod_aniso_quality_2          : 1;
	uint16_t lod_aniso_quality            : 1; // LodQuality
	uint16_t lod_iso_quality              : 1; // LodQuality
	uint16_t aniso_coarse_spread_modifier : 2; // AnisoSpreadModifier
	uint16_t aniso_spread_scale           : 5;
	uint16_t use_header_opt_control       : 1;
	uint16_t depth_texture                : 1;
	uint16_t mip_max_levels               : 4;

	// 0x10
	uint32_t width_minus_one       : 16;
	uint32_t view_layer_base_0_2   : 3;
	uint32_t aniso_spread_max_log2 : 3;
	uint32_t is_sRGB               : 1;
	uint32_t texture_type          : 4;  // TextureType
	uint32_t sector_promotion      : 2;  // SectorPromotion
	uint32_t border_size           : 3;  // BorderSize

	// 0x14
	uint32_t height_minus_one  : 16;
	uint32_t depth_minus_one   : 14;
	uint32_t is_sparse         : 1;
	uint32_t normalized_coords : 1;

	// 0x18
	uint32_t color_key_op               : 1;
	uint32_t trilin_opt                 : 5;
	uint32_t mip_lod_bias               : 13;
	uint32_t aniso_bias                 : 4;
	uint32_t aniso_fine_spread_func     : 2;  // AnisoSpreadFunc
	uint32_t aniso_coarse_spread_func   : 2;  // AnisoSpreadFunc
	uint32_t max_anisotropy             : 3;  // MaxAnisotropy
	uint32_t aniso_fine_spread_modifier : 2;  // AnisoSpreadModifier

	// 0x1C
	union
	{
		uint32_t color_key_value;
		struct
		{
			uint32_t view_mip_min_level : 4;
			uint32_t view_mip_max_level : 4;
			uint32_t msaa_mode          : 4;  // MsaaMode
			uint32_t min_lod_clamp      : 12;
			uint32_t                    : 8;
		};
	};
};

static_assert(sizeof(TextureImageControl)==0x20, "Invalid definition of TextureImageControl");

constexpr MsaaMode getMsaaMode(unsigned numSamplesLog2)
{
	switch (numSamplesLog2)
	{
		default:
		case 0: // DkMsMode_1x
			return MsaaMode_1x1;
		case 1: // DkMsMode_2x
			return MsaaMode_2x1_D3D;
		case 2: // DkMsMode_4x
			return MsaaMode_2x2;
		case 3: // DkMsMode_8x
			return MsaaMode_4x2_D3D;
		/* Currently not supported, maybe in the future
		case 4: // DkMsMode_16x
			return MsaaMode_4x4;
		*/
	}
}

}
