#pragma once
#include "dk_private.h"
#include "dk_memblock.h"
#include "dk_fence.h"
#include "dk_cmdbuf.h"
#include "ringbuf.h"

class tag_DkQueue : public DkObjBase
{
	static constexpr uint32_t s_numReservedWords = 12;
	static constexpr size_t s_maxQueuedGpfifoEntries = 64;
	static constexpr uint32_t s_numFences = 16;

	enum
	{
		Uninitialized = 0,
		Healthy = 1,
		Error = 2,
	} m_state;

	NvGpuChannel m_gpuChannel;
	tag_DkMemBlock m_cmdBufMemBlock;
	tag_DkMemBlock m_workBufMemBlock;
	tag_DkCmdBuf m_cmdBuf;

	CtrlCmdHeader m_cmdBufCtrlHeader;
	CtrlCmdGpfifoEntry m_gpfifoEntries[s_maxQueuedGpfifoEntries];

	RingBuf<uint32_t> m_cmdBufRing;
	uint32_t m_cmdBufFlushThreshold;
	uint32_t m_cmdBufPerFenceSliceSize;

	RingBuf<uint32_t> m_fenceRing;
	DkFence m_fences[s_numFences];
	uint32_t m_fenceCmdOffsets[s_numFences];

	uint32_t getCmdOffset() const noexcept { return m_cmdBufRing.getProducer() + m_cmdBuf.getCmdOffset(); }
	uint32_t getInFlightCmdSize() const noexcept { return m_cmdBufRing.getInFlight() + m_cmdBuf.getCmdOffset(); }

	void addCmdMemory(size_t minReqSize) noexcept;
	bool waitFenceRing(bool peek = false) noexcept;
	void flushRing(bool fenceFlush = false) noexcept;

	void onCmdBufAddMem(size_t minReqSize) noexcept;
	void appendGpfifoEntries(CtrlCmdGpfifoEntry const* entries, uint32_t numEntries) noexcept;

	bool hasPendingCommands() const noexcept
	{
		return m_cmdBuf.isDirty() || m_cmdBufCtrlHeader.arg != 0;
	}

	void flushCmdBuf() noexcept
	{
		if (hasPendingCommands())
		{
			m_cmdBuf.signOffGpfifoEntry(CtrlCmdGpfifoEntry::AutoKick | CtrlCmdGpfifoEntry::NoPrefetch);
			m_cmdBuf.flushGpfifoEntries();
		}
	}

	static void _addMemFunc(void* userData, DkCmdBuf cmdbuf, size_t minReqSize) noexcept
	{
		static_cast<tag_DkQueue*>(userData)->onCmdBufAddMem(minReqSize);
	}

	static void _gpfifoFlushFunc(void* data, CtrlCmdGpfifoEntry const* entries, uint32_t numEntries) noexcept
	{
		static_cast<tag_DkQueue*>(data)->appendGpfifoEntries(entries, numEntries);
	}

public:
	constexpr tag_DkQueue(DkQueueMaker const& maker) : DkObjBase{maker.device},
		m_state{Uninitialized}, m_gpuChannel{},
		m_cmdBufMemBlock{maker.device}, m_workBufMemBlock{maker.device}, m_cmdBuf{{maker.device,this,_addMemFunc},s_numReservedWords},
		m_cmdBufCtrlHeader{}, m_gpfifoEntries{},
		m_cmdBufRing{maker.commandMemorySize}, m_cmdBufFlushThreshold{maker.flushThreshold}, m_cmdBufPerFenceSliceSize{maker.commandMemorySize/s_numFences},
		m_fenceRing{s_numFences}, m_fences{}, m_fenceCmdOffsets{}
	{
		m_cmdBuf.useGpfifoFlushFunc(_gpfifoFlushFunc, this, &m_cmdBufCtrlHeader, s_maxQueuedGpfifoEntries);
	}

	bool isInErrorState() const noexcept { return m_state == Error; }

	~tag_DkQueue();
	DkResult initialize();
	void waitFence(DkFence& fence);
	void signalFence(DkFence& fence, bool flush);
	void submitCommands(DkCmdList list);
	void flush();
};
