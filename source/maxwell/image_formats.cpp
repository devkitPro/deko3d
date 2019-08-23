#include <switch.h>
#include <deko3d.h>
#include "image_formats.h"

using namespace maxwell;

#define _C(x)  ColorCompressionKind_##x
#define _D(x)  DepthCompressionKind_##x
#define _CS(x) ColorSurfaceFormat_##x
#define _DS(x) DepthSurfaceFormat_##x
#define _(fmt,r,g,b,a,x,y,z,w) (TicFormatWord){ \
	ImageFormat_##fmt, \
	ImageComponent_##r, ImageComponent_##g, ImageComponent_##b, ImageComponent_##a, \
	ImageSwizzle_##x, ImageSwizzle_##y, ImageSwizzle_##z, ImageSwizzle_##w, \
	0, \
}

const FormatTraits maxwell::formatTraits[] =
{
	{}, // dummy entry for DkImageFormat_None
#include "format_traits.inc"
};

static_assert(sizeof(formatTraits)/sizeof(FormatTraits) == DkImageFormat_Count);

unsigned maxwell::pickImageMemoryKind(FormatTraits const& traits, uint32_t msMode, bool compressed, bool d16EnableZbc)
{
	if (traits.depthBits || traits.stencilBits)
	{
		// Depth image
		switch (traits.depthCompKind)
		{
			default:
				break;
			case DepthCompressionKind_S8:
				return compressed ? NvKind_S8_2S : NvKind_S8;
			case DepthCompressionKind_S8Z24:
				if (!compressed)
					return NvKind_S8Z24;
				switch (msMode)
				{
					default:
					case DkMsMode_1x:  return NvKind_S8Z24_2CZ;
					case DkMsMode_2x:  return NvKind_S8Z24_MS2_2CZ;
					case DkMsMode_4x:  return NvKind_S8Z24_MS4_2CZ;
					case DkMsMode_8x:  return NvKind_S8Z24_MS8_2CZ;
				}
			case DepthCompressionKind_ZF32:
				if (!compressed)
					return NvKind_ZF32;
				switch (msMode)
				{
					default:
					case DkMsMode_1x:  return NvKind_ZF32_2CZ;
					case DkMsMode_2x:  return NvKind_ZF32_MS2_2CZ;
					case DkMsMode_4x:  return NvKind_ZF32_MS4_2CZ;
					case DkMsMode_8x:  return NvKind_ZF32_MS8_2CZ;
				}
			case DepthCompressionKind_Z24S8:
				if (!compressed)
					return NvKind_Z24S8;
				switch (msMode)
				{
					default:
					case DkMsMode_1x:  return NvKind_Z24S8_2CZ;
					case DkMsMode_2x:  return NvKind_Z24S8_MS2_2CZ;
					case DkMsMode_4x:  return NvKind_Z24S8_MS4_2CZ;
					case DkMsMode_8x:  return NvKind_Z24S8_MS8_2CZ;
				}
			case DepthCompressionKind_ZF32_X24S8:
				if (!compressed)
					return NvKind_ZF32_X24S8;
				switch (msMode)
				{
					default:
					case DkMsMode_1x:  return NvKind_ZF32_X24S8_2CSZV;
					case DkMsMode_2x:  return NvKind_ZF32_X24S8_MS2_2CSZV;
					case DkMsMode_4x:  return NvKind_ZF32_X24S8_MS4_2CSZV;
					case DkMsMode_8x:  return NvKind_ZF32_X24S8_MS8_2CSZV;
				}
			case DepthCompressionKind_Z16:
				if (!compressed)
					return NvKind_Z16;
				switch (msMode)
				{
					default:
					case DkMsMode_1x:  return d16EnableZbc ? NvKind_Z16_2C     : NvKind_Z16_2Z;
					case DkMsMode_2x:  return d16EnableZbc ? NvKind_Z16_MS2_2C : NvKind_Z16_MS2_2Z;
					case DkMsMode_4x:  return d16EnableZbc ? NvKind_Z16_MS4_2C : NvKind_Z16_MS4_2Z;
					case DkMsMode_8x:  return d16EnableZbc ? NvKind_Z16_MS8_2C : NvKind_Z16_MS8_2Z;
				}
		}
	}
	else if (compressed)
	{
		// (Compressed) color image
		switch (traits.colorCompKind)
		{
			default:
				break;
			case ColorCompressionKind_32:
				switch (msMode)
				{
					default:
					case DkMsMode_1x:  return NvKind_C32_2CRA;
					case DkMsMode_2x:  return NvKind_C32_MS2_2CRA;
					case DkMsMode_4x:  return NvKind_C32_MS4_2CBR; // 2BRA also observed as an alternate mode
					case DkMsMode_8x:  return NvKind_C32_MS8_MS16_2CRA;
				}
			case ColorCompressionKind_64:
				switch (msMode)
				{
					default:
					case DkMsMode_1x:  return NvKind_C64_2CRA;
					case DkMsMode_2x:  return NvKind_C64_MS2_2CRA;
					case DkMsMode_4x:  return NvKind_C64_MS4_2CBR; // 2BRA also observed as an alternate mode
					case DkMsMode_8x:  return NvKind_C64_MS8_MS16_2CRA;
				}
			case ColorCompressionKind_128:
				switch (msMode)
				{
					default:
					case DkMsMode_1x:  return NvKind_C128_2CR;
					case DkMsMode_2x:  return NvKind_C128_MS2_2CR;
					case DkMsMode_4x:  return NvKind_C128_MS4_2CR;
					case DkMsMode_8x:  return NvKind_C128_MS8_MS16_2CR;
				}
		}
	}

	// Otherwise just return Generic
	return NvKind_Generic_16BX2;
}
