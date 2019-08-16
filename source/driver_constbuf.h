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
		DkResHandle textures[DK_NUM_TEXTURE_BINDINGS];
		DkResHandle images[DK_NUM_IMAGE_BINDINGS];
		BufDescriptor storageBufs[DK_NUM_STORAGE_BUFS];
	};

	struct GraphicsDriverCbuf
	{
		uint32_t baseVertex;
		uint32_t baseInstance;
		uint32_t drawId;
		uint32_t fbTexHandle;
		PerStageData data[DkStage_MaxGraphics];
	};

	struct ComputeDriverCbuf
	{
		uint32_t ctaSize[3];
		uint32_t gridSize[3];
		uint32_t _padding[2];
		DkGpuAddr uniformBufs[DK_NUM_UNIFORM_BUFS];
		PerStageData data;
	};

	constexpr uint32_t GraphicsDriverCbufSize = (sizeof(GraphicsDriverCbuf) + DK_UNIFORM_BUF_ALIGNMENT - 1) &~ (DK_UNIFORM_BUF_ALIGNMENT - 1);
	constexpr uint32_t ComputeDriverCbufSize  = (sizeof(ComputeDriverCbuf)  + DK_UNIFORM_BUF_ALIGNMENT - 1) &~ (DK_UNIFORM_BUF_ALIGNMENT - 1);
}
