#pragma once
#include "dk_private.h"
#include "dk_memblock.h"
#include "dk_fence.h"
#include "dk_cmdbuf.h"
#include "ringbuf.h"
#include "queue_workbuf.h"

class tag_DkQueue : public dk::detail::ObjBase
{
	static constexpr uint32_t s_numReservedWords = 12;
	static constexpr size_t s_maxQueuedGpfifoEntries = 64;
	static constexpr uint32_t s_numFences = 16;

	uint32_t m_id;
	uint32_t m_flags;
	enum
	{
		Uninitialized = 0,
		Healthy = 1,
		Error = 2,
	} m_state;

	NvGpuChannel m_gpuChannel;
	tag_DkMemBlock m_cmdBufMemBlock;
	tag_DkCmdBuf m_cmdBuf;

	dk::detail::CtrlCmdHeader m_cmdBufCtrlHeader;
	dk::detail::CtrlCmdGpfifoEntry m_gpfifoEntries[s_maxQueuedGpfifoEntries];

	dk::detail::RingBuf<uint32_t> m_cmdBufRing;
	uint32_t m_cmdBufFlushThreshold;
	uint32_t m_cmdBufPerFenceSliceSize;

	dk::detail::RingBuf<uint32_t> m_fenceRing;
	DkFence m_fences[s_numFences];
	uint32_t m_fenceCmdOffsets[s_numFences];
	uint32_t m_fenceLastFlushOffset;

	dk::detail::QueueWorkBuf m_workBuf;

	uint32_t getCmdOffset() const noexcept { return m_cmdBufRing.getProducer() + m_cmdBuf.getCmdOffset(); }
	uint32_t getInFlightCmdSize() const noexcept { return m_cmdBufRing.getInFlight() + m_cmdBuf.getCmdOffset(); }
	uint32_t getSizeSinceLastFenceFlush() const noexcept
	{
		uint32_t offset = getCmdOffset();
		if (offset < m_fenceLastFlushOffset)
			return m_cmdBufRing.getSize() + offset - m_fenceLastFlushOffset;
		else
			return offset - m_fenceLastFlushOffset;
	}

	void addCmdMemory(size_t minReqSize) noexcept;
	bool waitFenceRing(bool peek = false) noexcept;
	void flushRing(bool fenceFlush = false) noexcept;

	void onCmdBufAddMem(size_t minReqSize) noexcept;
	void appendGpfifoEntries(dk::detail::CtrlCmdGpfifoEntry const* entries, uint32_t numEntries) noexcept;

	bool hasPendingCommands() const noexcept
	{
		return m_cmdBuf.isDirty() || m_cmdBufCtrlHeader.arg != 0;
	}

	void flushCmdBuf() noexcept
	{
		if (hasPendingCommands())
		{
			m_cmdBuf.signOffGpfifoEntry(
				dk::detail::CtrlCmdGpfifoEntry::AutoKick |
				dk::detail::CtrlCmdGpfifoEntry::NoPrefetch);
			m_cmdBuf.flushGpfifoEntries();
		}
	}

	static void _addMemFunc(void* userData, DkCmdBuf cmdbuf, size_t minReqSize) noexcept
	{
		static_cast<tag_DkQueue*>(userData)->onCmdBufAddMem(minReqSize);
	}

	static void _gpfifoFlushFunc(void* data, dk::detail::CtrlCmdGpfifoEntry const* entries, uint32_t numEntries) noexcept
	{
		static_cast<tag_DkQueue*>(data)->appendGpfifoEntries(entries, numEntries);
	}

	void setupEngines();
	void postSubmitFlush();

public:
	tag_DkQueue(DkQueueMaker const& maker, uint32_t id) : ObjBase{maker.device},
		m_id{id}, m_flags{maker.flags}, m_state{Uninitialized}, m_gpuChannel{},
		m_cmdBufMemBlock{maker.device}, m_cmdBuf{{maker.device,this,_addMemFunc},s_numReservedWords},
		m_cmdBufCtrlHeader{}, m_gpfifoEntries{},
		m_cmdBufRing{maker.commandMemorySize}, m_cmdBufFlushThreshold{maker.flushThreshold}, m_cmdBufPerFenceSliceSize{maker.commandMemorySize/s_numFences},
		m_fenceRing{s_numFences}, m_fences{}, m_fenceCmdOffsets{}, m_fenceLastFlushOffset{},
		m_workBuf{maker}
	{
		m_cmdBuf.useGpfifoFlushFunc(_gpfifoFlushFunc, this, &m_cmdBufCtrlHeader, s_maxQueuedGpfifoEntries);
	}

	bool hasGraphics() const noexcept { return (m_flags & DkQueueFlags_Graphics) != 0; }
	bool hasCompute() const noexcept { return (m_flags & DkQueueFlags_Compute) != 0; }
	bool hasTransfer() const noexcept { return (m_flags & DkQueueFlags_Transfer) != 0; }
	bool hasZcull() const noexcept { return (m_flags & DkQueueFlags_DisableZcull) == 0; }
	bool isDepthModeOpenGL() const noexcept { return (m_flags & DkQueueFlags_DepthZeroToOne) == 0; }
	bool isOriginModeOpenGL() const noexcept { return (m_flags & DkQueueFlags_OriginUpperLeft) == 0; }
	bool isInErrorState() const noexcept { return m_state == Error; }

	~tag_DkQueue();
	DkResult initialize();
	void waitFence(DkFence& fence);
	void signalFence(DkFence& fence, bool flush);
	void submitCommands(DkCmdList list);
	void flush();
	void waitIdle();
};
