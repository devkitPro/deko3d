#pragma once
#include <stdint.h>

namespace maxwell
{

// Texture Sampler Control Block
// See https://github.com/envytools/envytools/blob/master/rnndb/graph/g80_texture.xml#L367
struct TextureSamplerControl
{
	// 0x00
	uint32_t address_u : 3;
	uint32_t address_v : 3;
	uint32_t address_p : 3;
	uint32_t depth_compare : 1;
	uint32_t depth_compare_op : 3;
	uint32_t srgb_conversion : 1;
	uint32_t font_filter_width : 3;
	uint32_t font_filter_height : 3;
	uint32_t max_anisotropy : 3;
	uint32_t : 9;

	// 0x04
	uint32_t mag_filter : 2;
	uint32_t : 2;
	uint32_t min_filter : 2;
	uint32_t mip_filter : 2;
	uint32_t cubemap_anisotropy : 1;
	uint32_t cubemap_interface_filtering : 1;
	uint32_t reduction_filter : 2;
	uint32_t mip_lod_bias : 13;
	uint32_t float_coord_normalization : 1;
	uint32_t trilin_opt : 5;
	uint32_t : 1;

	// 0x08
	uint32_t min_lod_clamp : 12;
	uint32_t max_lod_clamp : 12;
	uint32_t srgb_border_color_r : 8;

	// 0x0C
	uint32_t : 12;
	uint32_t srgb_border_color_g : 8;
	uint32_t srgb_border_color_b : 8;
	uint32_t : 4;

	// 0x10
	uint32_t border_color_r;

	// 0x14
	uint32_t border_color_g;

	// 0x18
	uint32_t border_color_b;

	// 0x1C
	uint32_t border_color_a;
};

static_assert(sizeof(TextureSamplerControl)==0x20, "Invalid definition of TextureSamplerControl");

}
