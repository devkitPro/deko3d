#include <math.h>
#include "../dk_sampler_descriptor.h"

using namespace dk::detail;
using namespace maxwell;

namespace
{
	constexpr float clamp(float value, float min, float max)
	{
		return value < min ? min : (value > max ? max : value);
	}

	constexpr unsigned roundToInt(float value)
	{
		return (unsigned)(value + 0.5f);
	}

	template <typename T = unsigned>
	constexpr unsigned floatToFixed(float value, unsigned fractBits)
	{
		float scale = float(1U<<fractBits);
		return T(value*scale);
	}

	constexpr uint8_t anisoTable[] = { 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 6, 6, 7 };

	constexpr float lodSnapTable[] =
	{
		0.0, -0.055556, -0.1, -0.13636, -0.16667, -0.19231, -0.21429, -0.23333,
		-0.25, -0.27778, -0.3, -0.31818, -0.33333, -0.34615, -0.35714, -0.36667,
		-0.375, -0.38889, -0.4, -0.40909, -0.41667, -0.42308, -0.42857, -0.43333,
		-0.4375, -0.44444, -0.45, -0.45455, -0.45833, -0.46154, -0.46429, -0.46667,
	};

	// See https://gist.github.com/rygorous/2203834#file-gistfile1-cpp-L41-L62
	// TODO: Optimize with a LUT, similar to what mesa does
	constexpr uint8_t floatToSrgb8(float f)
	{
		float s = 0.0f;

		if (!(f > 0.0f)) // also covers NaNs
			s = 0.0f;
		else if (f <= 0.0031308f)
			s = 12.92f * f;
		else if (f < 1.0f)
			s = 1.055f * powf(f, 1.0f / 2.4f) - 0.055f;
		else
			s = 1.0f;

		return roundToInt(s * 255.0f);
	}
}

void dkSamplerDescriptorInitialize(DkSamplerDescriptor* obj, DkSampler const* sampler)
{
	memset(obj, 0, sizeof(*obj));

	unsigned lodSnap;
	for (lodSnap = 0; lodSnap < 0x20; lodSnap ++)
		if ((-lodSnapTable[lodSnap]) >= sampler->lodSnap)
			break;
	if (lodSnap == 0x20)
		lodSnap = 0x1F;

	float lodClampMin = clamp(sampler->lodClampMin, 0.0f, 15.0f);
	float lodClampMax = clamp(sampler->lodClampMax, 0.0f, 15.0f);
	if (lodClampMax < lodClampMin)
		lodClampMax = lodClampMin;

	obj->address_u = sampler->wrapMode[0];
	obj->address_v = sampler->wrapMode[1];
	obj->address_p = sampler->wrapMode[2];
	obj->depth_compare = sampler->compareEnable;
	obj->depth_compare_op = (unsigned)sampler->compareOp - 1;
	obj->srgb_conversion = 1;
	obj->font_filter_width = 1;
	obj->font_filter_height = 1;
	obj->max_anisotropy = anisoTable[roundToInt(clamp(sampler->maxAnisotropy, 1.0f, 16.0f))-1];

	obj->mag_filter = sampler->magFilter;
	obj->min_filter = sampler->minFilter;
	obj->mip_filter = sampler->mipFilter;
	obj->cubemap_anisotropy = 1;
	obj->cubemap_interface_filtering = 1;
	obj->reduction_filter = sampler->reductionMode;
	obj->mip_lod_bias = floatToFixed<signed>(clamp(lodSnapTable[lodSnap] + sampler->lodBias, -15.0f, +15.0f), 8);
	obj->float_coord_normalization = 0;
	obj->trilin_opt = lodSnap;

	obj->min_lod_clamp = floatToFixed(lodClampMin, 8);
	obj->max_lod_clamp = floatToFixed(lodClampMax, 8);
	obj->srgb_border_color_r = floatToSrgb8(sampler->borderColor[0].value_f);

	obj->srgb_border_color_g = floatToSrgb8(sampler->borderColor[1].value_f);
	obj->srgb_border_color_b = floatToSrgb8(sampler->borderColor[2].value_f);

	obj->border_color_r = sampler->borderColor[0].value_ui;
	obj->border_color_g = sampler->borderColor[1].value_ui;
	obj->border_color_b = sampler->borderColor[2].value_ui;
	obj->border_color_a = sampler->borderColor[3].value_ui;
}
