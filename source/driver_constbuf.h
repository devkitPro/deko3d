#pragma once
#include "dk_private.h"

namespace dk::detail
{
	struct BufDescriptor
	{
		DkGpuAddr address;
		uint32_t size;
		uint32_t _padding;
	};

	struct PerStageData
	{
		DkResHandle textures[32];
		DkResHandle images[32];
		BufDescriptor storageBufs[16];
	};

	struct GraphicsDriverCbuf
	{
		uint32_t baseVertex;
		uint32_t baseInstance;
		uint32_t drawId;
		uint32_t fbTexHandle;
		PerStageData data[5];
	};

	struct ComputeDriverCbuf
	{
		uint32_t ctaSize[3];
		uint32_t gridSize[3];
		uint32_t _padding[2];
		DkGpuAddr uniformBufs[16];
		PerStageData data;
	};

	constexpr uint32_t GraphicsDriverCbufSize = (sizeof(GraphicsDriverCbuf) + DK_UNIFORM_BUF_ALIGNMENT - 1) &~ (DK_UNIFORM_BUF_ALIGNMENT - 1);
	constexpr uint32_t ComputeDriverCbufSize  = (sizeof(ComputeDriverCbuf)  + DK_UNIFORM_BUF_ALIGNMENT - 1) &~ (DK_UNIFORM_BUF_ALIGNMENT - 1);
}
