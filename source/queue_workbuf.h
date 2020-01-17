#pragma once
#include "dk_private.h"
#include "dk_memblock.h"

namespace dk::detail
{
	class QueueWorkBuf : public MemBlock
	{
		// Work memory needed for Graphics/Compute-capable queues
		uint32_t m_scratchMemOffset;
		uint32_t m_scratchMemSize;

		// Work memory needed for Graphics-capable queues
		uint32_t m_graphicsCbufOffset;
		uint32_t m_graphicsCbufSize;
		uint32_t m_vtxRunoutBufOffset;
		uint32_t m_vtxRunoutBufSize;
		uint32_t m_zcullCtxOffset;
		uint32_t m_zcullCtxSize;

		// Work memory needed for Compute-capable queues
		uint32_t m_computeJobsOffset;
		uint32_t m_computeJobsCount;

		// Total size of the work buffer
		uint32_t m_totalSize;

		// Effective per-warp scratch memory size
		uint32_t m_perWarpScratchSize;

		uint32_t addSection(uint32_t& offset, uint32_t size, uint32_t align) noexcept
		{
			offset = (m_totalSize + align - 1) &~ (align - 1);
			size   = (size        + align - 1) &~ (align - 1);
			m_totalSize = offset + size;
			return size;
		}

		DkGpuAddr getGpuAddr(uint32_t offset) const noexcept { return getGpuAddrPitch() + offset; }

	public:
		QueueWorkBuf(DkQueueMaker const& maker) noexcept;

		DkResult initialize() noexcept
		{
			return MemBlock::initialize(
				DkMemBlockFlags_GpuCached | DkMemBlockFlags_ZeroFillInit,
				nullptr,
				m_totalSize);
		}

		DkGpuAddr getScratchMem() const noexcept { return getGpuAddr(m_scratchMemOffset); }
		uint32_t getScratchMemSize() const noexcept { return m_scratchMemSize; }
		uint32_t getPerWarpScratchSize() const noexcept { return m_perWarpScratchSize; }

		DkGpuAddr getGraphicsCbuf() const noexcept { return getGpuAddr(m_graphicsCbufOffset); }
		uint32_t getGraphicsCbufSize() const noexcept { return m_graphicsCbufSize; }

		DkGpuAddr getVtxRunoutBuf() const noexcept { return getGpuAddr(m_vtxRunoutBufOffset); }
		uint32_t getVtxRunoutBufSize() const noexcept { return m_vtxRunoutBufSize; }

		DkGpuAddr getZcullCtx() const noexcept { return getGpuAddr(m_zcullCtxOffset); }
		uint32_t getZcullCtxSize() const noexcept { return m_zcullCtxSize; }

		DkGpuAddr getComputeJobs() const noexcept { return getGpuAddr(m_computeJobsOffset); }
		uint32_t getComputeJobsCount() const noexcept { return m_computeJobsCount; }
	};
}
