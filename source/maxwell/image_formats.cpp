#include <deko3d.h>
#include "image_formats.h"

using namespace maxwell;

#define _C(x)  ColorCompressionKind_##x
#define _D(x)  DepthCompressionKind_##x
#define _CS(x) ColorSurfaceFormat_##x
#define _DS(x) DepthSurfaceFormat_##x
#define _(fmt,r,g,b,a,x,y,z,w) MakeTicFormat( \
	ImageFormat_##fmt, \
	ImageComponent_##r, ImageComponent_##g, ImageComponent_##b, ImageComponent_##a, \
	ImageSwizzle_##x, ImageSwizzle_##y, ImageSwizzle_##z, ImageSwizzle_##w \
)

const FormatTraits maxwell::formatTraits[] =
{
	{}, // dummy entry for DkImageFormat_None
#include "format_traits.inc"
};

static_assert(sizeof(formatTraits)/sizeof(FormatTraits) == DkImageFormat_Count);
