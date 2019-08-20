#pragma once
#include <stdint.h>

namespace maxwell
{

enum ImageFormat
{
	ImageFormat_R32_G32_B32_A32 = 0x01,
	ImageFormat_R32_G32_B32 = 0x02,
	ImageFormat_R16_G16_B16_A16 = 0x03,
	ImageFormat_R32_G32 = 0x04,
	ImageFormat_R32_B24G8 = 0x05,
	ImageFormat_ETC2_RGB = 0x06,
	ImageFormat_X8B8G8R8 = 0x07,
	ImageFormat_A8R8G8B8 = 0x08,
	ImageFormat_A2B10G10R10 = 0x09,
	ImageFormat_ETC2_RGB_PTA = 0x0A,
	ImageFormat_ETC2_RGBA = 0x0B,
	ImageFormat_R16_G16 = 0x0C,
	ImageFormat_G8R24 = 0x0D,
	ImageFormat_G24R8 = 0x0E,
	ImageFormat_R32 = 0x0F,
	ImageFormat_BC6H_SF16 = 0x10,
	ImageFormat_BC6H_UF16 = 0x11,
	ImageFormat_A4B4G4R4 = 0x12,
	ImageFormat_A5B5G5R1 = 0x13,
	ImageFormat_A1B5G5R5 = 0x14,
	ImageFormat_B5G6R5 = 0x15,
	ImageFormat_B6G5R5 = 0x16,
	ImageFormat_BC7U = 0x17,
	ImageFormat_G8R8 = 0x18,
	ImageFormat_EAC = 0x19,
	ImageFormat_EACX2 = 0x1A,
	ImageFormat_R16 = 0x1B,
	ImageFormat_Y8_VIDEO = 0x1C,
	ImageFormat_R8 = 0x1D,
	ImageFormat_G4R4 = 0x1E,
	ImageFormat_R1 = 0x1F,
	ImageFormat_E5B9G9R9_SHAREDEXP = 0x20,
	ImageFormat_BF10GF11RF11 = 0x21,
	ImageFormat_G8B8G8R8 = 0x22,
	ImageFormat_B8G8R8G8 = 0x23,
	ImageFormat_DXT1 = 0x24,
	ImageFormat_DXT23 = 0x25,
	ImageFormat_DXT45 = 0x26,
	ImageFormat_DXN1 = 0x27,
	ImageFormat_DXN2 = 0x28,
	ImageFormat_Z24S8 = 0x29,
	ImageFormat_X8Z24 = 0x2A,
	ImageFormat_S8Z24 = 0x2B,
	ImageFormat_X4V4Z24__COV4R4V = 0x2C,
	ImageFormat_X4V4Z24__COV8R8V = 0x2D,
	ImageFormat_V8Z24__COV4R12V = 0x2E,
	ImageFormat_ZF32 = 0x2F,
	ImageFormat_ZF32_X24S8 = 0x30,
	ImageFormat_X8Z24_X20V4S8__COV4R4V = 0x31,
	ImageFormat_X8Z24_X20V4S8__COV8R8V = 0x32,
	ImageFormat_ZF32_X20V4X8__COV4R4V = 0x33,
	ImageFormat_ZF32_X20V4X8__COV8R8V = 0x34,
	ImageFormat_ZF32_X20V4S8__COV4R4V = 0x35,
	ImageFormat_ZF32_X20V4S8__COV8R8V = 0x36,
	ImageFormat_X8Z24_X16V8S8__COV4R12V = 0x37,
	ImageFormat_ZF32_X16V8X8__COV4R12V = 0x38,
	ImageFormat_ZF32_X16V8S8__COV4R12V = 0x39,
	ImageFormat_Z16 = 0x3A,
	ImageFormat_V8Z24__COV8R24V = 0x3B,
	ImageFormat_X8Z24_X16V8S8__COV8R24V = 0x3C,
	ImageFormat_ZF32_X16V8X8__COV8R24V = 0x3D,
	ImageFormat_ZF32_X16V8S8__COV8R24V = 0x3E,
	ImageFormat_ASTC_2D_4X4 = 0x40,
	ImageFormat_ASTC_2D_5X5 = 0x41,
	ImageFormat_ASTC_2D_6X6 = 0x42,
	ImageFormat_ASTC_2D_8X8 = 0x44,
	ImageFormat_ASTC_2D_10X10 = 0x45,
	ImageFormat_ASTC_2D_12X12 = 0x46,
	ImageFormat_ASTC_2D_5X4 = 0x50,
	ImageFormat_ASTC_2D_6X5 = 0x51,
	ImageFormat_ASTC_2D_8X6 = 0x52,
	ImageFormat_ASTC_2D_10X8 = 0x53,
	ImageFormat_ASTC_2D_12X10 = 0x54,
	ImageFormat_ASTC_2D_8X5 = 0x55,
	ImageFormat_ASTC_2D_10X5 = 0x56,
	ImageFormat_ASTC_2D_10X6 = 0x57,
};

enum ImageComponent
{
	ImageComponent_Snorm = 1,
	ImageComponent_Unorm = 2,
	ImageComponent_Sint = 3,
	ImageComponent_Uint = 4,
	ImageComponent_SnormForceFp16 = 5,
	ImageComponent_UnormForceFp16 = 6,
	ImageComponent_Float = 7,
};

enum ImageSwizzle
{
	ImageSwizzle_Zero = 0,
	ImageSwizzle_R = 2,
	ImageSwizzle_G = 3,
	ImageSwizzle_B = 4,
	ImageSwizzle_A = 5,
	ImageSwizzle_OneInt = 6,
	ImageSwizzle_OneFloat = 7,
};

enum ColorSurfaceFormat
{
	ColorSurfaceFormat_Bitmap = 0x1C,
	ColorSurfaceFormat_Unknown1D = 0x1D,
	ColorSurfaceFormat_RGBA32Float = 0xC0,
	ColorSurfaceFormat_RGBA32Sint = 0xC1,
	ColorSurfaceFormat_RGBA32Uint = 0xC2,
	ColorSurfaceFormat_RGBX32Float = 0xC3,
	ColorSurfaceFormat_RGBX32Sint = 0xC4,
	ColorSurfaceFormat_RGBX32Uint = 0xC5,
	ColorSurfaceFormat_RGBA16Unorm = 0xC6,
	ColorSurfaceFormat_RGBA16Snorm = 0xC7,
	ColorSurfaceFormat_RGBA16Sint = 0xC8,
	ColorSurfaceFormat_RGBA16Uint = 0xC9,
	ColorSurfaceFormat_RGBA16Float = 0xCA,
	ColorSurfaceFormat_RG32Float = 0xCB,
	ColorSurfaceFormat_RG32Sint = 0xCC,
	ColorSurfaceFormat_RG32Uint = 0xCD,
	ColorSurfaceFormat_RGBX16Float = 0xCE,
	ColorSurfaceFormat_BGRA8Unorm = 0xCF,
	ColorSurfaceFormat_BGRA8Srgb = 0xD0,
	ColorSurfaceFormat_RGB10A2Unorm = 0xD1,
	ColorSurfaceFormat_RGB10A2Uint = 0xD2,
	ColorSurfaceFormat_RGBA8Unorm = 0xD5,
	ColorSurfaceFormat_RGBA8Srgb = 0xD6,
	ColorSurfaceFormat_RGBA8Snorm = 0xD7,
	ColorSurfaceFormat_RGBA8Sint = 0xD8,
	ColorSurfaceFormat_RGBA8Uint = 0xD9,
	ColorSurfaceFormat_RG16Unorm = 0xDA,
	ColorSurfaceFormat_RG16Snorm = 0xDB,
	ColorSurfaceFormat_RG16Sint = 0xDC,
	ColorSurfaceFormat_RG16Uint = 0xDD,
	ColorSurfaceFormat_RG16Float = 0xDE,
	ColorSurfaceFormat_BGR10A2Unorm = 0xDF,
	ColorSurfaceFormat_R11G11B10Float = 0xE0,
	ColorSurfaceFormat_R32Sint = 0xE3,
	ColorSurfaceFormat_R32Uint = 0xE4,
	ColorSurfaceFormat_R32Float = 0xE5,
	ColorSurfaceFormat_BGRX8Unorm = 0xE6,
	ColorSurfaceFormat_BGRX8Srgb = 0xE7,
	ColorSurfaceFormat_B5G6R5Unorm = 0xE8,
	ColorSurfaceFormat_BGR5A1Unorm = 0xE9,
	ColorSurfaceFormat_RG8Unorm = 0xEA,
	ColorSurfaceFormat_RG8Snorm = 0xEB,
	ColorSurfaceFormat_RG8Sint = 0xEC,
	ColorSurfaceFormat_RG8Uint = 0xED,
	ColorSurfaceFormat_R16Unorm = 0xEE,
	ColorSurfaceFormat_R16Snorm = 0xEF,
	ColorSurfaceFormat_R16Sint = 0xF0,
	ColorSurfaceFormat_R16Uint = 0xF1,
	ColorSurfaceFormat_R16Float = 0xF2,
	ColorSurfaceFormat_R8Unorm = 0xF3,
	ColorSurfaceFormat_R8Snorm = 0xF4,
	ColorSurfaceFormat_R8Sint = 0xF5,
	ColorSurfaceFormat_R8Uint = 0xF6,
	ColorSurfaceFormat_A8Unorm = 0xF7,
	ColorSurfaceFormat_BGR5X1Unorm = 0xF8,
	ColorSurfaceFormat_RGBX8Unorm = 0xF9,
	ColorSurfaceFormat_RGBX8Srgb = 0xFA,
	ColorSurfaceFormat_BGR5X1UnormUnknownFB = 0xFB,
	ColorSurfaceFormat_BGR5X1UnormUnknownFC = 0xFC,
	ColorSurfaceFormat_BGRX8UnormUnknownFD = 0xFD,
	ColorSurfaceFormat_BGRX8UnormUnknownFE = 0xFE,
	ColorSurfaceFormat_Y32UintUnknownFF = 0xFF,
};

enum DepthSurfaceFormat
{
	DepthSurfaceFormat_Z32Float = 0x0A,
	DepthSurfaceFormat_Z16Unorm = 0x13,
	DepthSurfaceFormat_S8Z24Unorm = 0x14,
	DepthSurfaceFormat_Z24X8Unorm = 0x15,
	DepthSurfaceFormat_Z24S8Unorm = 0x16,
	DepthSurfaceFormat_S8Uint = 0x17,
	DepthSurfaceFormat_Z24C8Unorm = 0x18,
	DepthSurfaceFormat_Z32S8X24Float = 0x19,
	DepthSurfaceFormat_Z24X8S8C8X16Unorm = 0x1D,
	DepthSurfaceFormat_Z32X8C8X16Float = 0x1E,
	DepthSurfaceFormat_Z32S8C8X16Float = 0x1F,
};

enum ColorCompressionKind
{
	ColorCompressionKind_8   = 1,
	ColorCompressionKind_16  = 2,
	ColorCompressionKind_24  = 3,
	ColorCompressionKind_32  = 4,
	ColorCompressionKind_64  = 5,
	ColorCompressionKind_128 = 6,
};

enum DepthCompressionKind
{
	DepthCompressionKind_S8            = 0,
	DepthCompressionKind_S8Z24         = 1,
	DepthCompressionKind_ZF32          = 2,
	DepthCompressionKind_Z24S8         = 3,
	//DepthCompressionKind_S8Z24       = 4,
	DepthCompressionKind_ZF32_X24S8    = 5,
	DepthCompressionKind_X8Z24_X16V8S8 = 6,
	DepthCompressionKind_Z16           = 7,
};

enum
{
	FormatTraitFlags_IsRawInt     = 1U << 0,
	FormatTraitFlags_IsSrgb       = 1U << 1,
	FormatTraitFlags_IsUnorm      = 1U << 2,
	FormatTraitFlags_IsDepthFloat = 1U << 3,

	FormatTraitFlags_CanRender = 1U << 8,
	FormatTraitFlags_CanUnk0   = 1U << 9,
	FormatTraitFlags_CanUnk1   = 1U << 10,
	FormatTraitFlags_CanUnk2   = 1U << 11,
};

constexpr uint32_t _ShiftTicField(uint32_t field, uint32_t pos, uint32_t size)
{
	field &= (uint32_t{1} << size) - 1;
	return field << pos;
}

constexpr uint32_t MakeTicFormat(
	ImageFormat fmt,
	ImageComponent r, ImageComponent g, ImageComponent b, ImageComponent a,
	ImageSwizzle x, ImageSwizzle y, ImageSwizzle z, ImageSwizzle w
)
{
	return
		_ShiftTicField(fmt,0,7) |
		_ShiftTicField(r,7, 3) | _ShiftTicField(g,10,3) | _ShiftTicField(b,13,3) | _ShiftTicField(a,16,3) |
		_ShiftTicField(x,19,3) | _ShiftTicField(y,22,3) | _ShiftTicField(z,25,3) | _ShiftTicField(w,28,3);
}

struct FormatTraits
{
	uint16_t flags;
	uint8_t redBits, greenBits, blueBits, alphaBits, depthBits, stencilBits;
	uint8_t bytesPerBlock, blockWidth, blockHeight;
	uint8_t colorCompKind, depthCompKind;
	uint8_t surfaceFmt, engine2dFmt;
	uint32_t ticFmt;
};

extern const FormatTraits formatTraits[];

}
