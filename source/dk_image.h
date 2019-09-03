#pragma once
#include "dk_private.h"

#include "maxwell/image_formats.h"

struct DkImageLayout
{
	DkImageType m_type;
	uint32_t m_flags;
	DkImageFormat m_format;

	uint32_t m_dimensions[3];
	uint8_t m_dimsPerLayer;
	uint8_t m_hasLayers;
	uint8_t m_numSamplesLog2;
	uint8_t m_samplesX, m_samplesY;
	uint8_t m_mipLevels;
	uint8_t m_bytesPerBlock, m_bytesPerBlockLog2;
	uint8_t m_blockW, m_blockH;
	uint8_t m_tileW, m_tileH, m_tileD; // {for block-linear only} in DkTileSize, i.e. log2(number of gobs)
	uint8_t m_memKind;

	uint64_t m_storageSize;
	uint64_t m_layerSize;
	uint32_t m_alignment;
	uint32_t m_stride; // {for pitch-linear only}

	void calcLayerSize();
};

struct DkImage : public DkImageLayout
{
	DkGpuAddr m_iova;
	DkMemBlock m_memBlock;
	uint32_t m_memOffset;
};

DK_OPAQUE_CHECK(DkImageLayout);
DK_OPAQUE_CHECK(DkImage);

namespace dk::detail
{
	struct ImageInfo
	{
		DkGpuAddr m_iova;
		uint32_t m_horizontal;
		uint32_t m_vertical;
		uint32_t m_format;
		uint32_t m_tileMode;
		uint32_t m_arrayMode;
		uint32_t m_layerStride;

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_widthMs;
		uint32_t m_heightMs;
		uint8_t m_bytesPerBlock;
		bool m_isLinear;

		enum
		{
			ColorRenderTarget = 0,
			DepthRenderTarget = 1,
			Transfer2D        = 2,
			TransferCopy      = 3,
		};

		DkResult fromImageView(DkImageView const* view, unsigned usage);
	};

	enum
	{
		Blit2D_SetupEngine  = 1U << 0,
		Blit2D_OriginCorner = 1U << 1,
		Blit2D_UseFilter    = 1U << 2,
	};

	struct BlitParams
	{
		uint32_t srcX;
		uint32_t srcY;
		uint32_t dstX;
		uint32_t dstY;
		uint32_t width;
		uint32_t height;
	};

	// Fractional bits for dudx/dvdy and srcX/Y parameters for Blit2DEngine
	constexpr unsigned DiffFractBits = 15;
	constexpr unsigned SrcFractBits = 4;

	void BlitCopyEngine(DkCmdBuf obj, ImageInfo const& src, ImageInfo const& dst, BlitParams const& params);
	void Blit2DEngine(DkCmdBuf obj, ImageInfo const& src, ImageInfo const& dst, BlitParams const& params, int32_t dudx, int32_t dvdy, uint32_t flags, uint32_t factor);
}
