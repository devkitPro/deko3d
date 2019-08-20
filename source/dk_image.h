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
	uint32_t m_stride; // {for pitch-linear only}
};

struct DkImage : public DkImageLayout
{
};

DK_OPAQUE_CHECK(DkImageLayout);
DK_OPAQUE_CHECK(DkImage);

