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
	void flushRing() noexcept;

	void onCmdBufAddMem(size_t minReqSize) noexcept;
	void appendGpfifoEntries(CtrlCmdGpfifoEntry const* entries, uint32_t numEntries) noexcept;

	void flushCmdBuf() noexcept
	{
		m_cmdBuf.signOffGpfifoEntry(CtrlCmdGpfifoEntry::AutoKick | CtrlCmdGpfifoEntry::NoPrefetch);
		m_cmdBuf.flushGpfifoEntries();
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
	constexpr tag_DkQueue(DkDevice dev) : DkObjBase{dev},
		m_workBufMemBlock{dev}, m_cmdBuf{{dev,this,_addMemFunc},s_numReservedWords},
		m_cmdBufCtrlHeader{}, m_gpfifoEntries{},
		m_cmdBufRing{}, m_cmdBufFlushThreshold{}, m_cmdBufPerFenceSliceSize{},
		m_fenceRing{s_numFences}, m_fences{}, m_fenceCmdOffsets{}
	{
		m_cmdBuf.useGpfifoFlushFunc(_gpfifoFlushFunc, this, &m_cmdBufCtrlHeader, s_maxQueuedGpfifoEntries);
	}

	void submitCommands(DkCmdList list);
	void flush();
};
