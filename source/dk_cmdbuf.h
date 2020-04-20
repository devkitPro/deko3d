#pragma once
#include "dk_private.h"
#include "dk_memblock.h"
#include "dk_ctrlcmd.h"
#include "maxwell/command.h"

namespace dk::detail
{

using GpfifoFlushFunc = void(*)(void* data, CtrlCmdGpfifoEntry const* entries, uint32_t numEntries);
template <bool> class CmdBufWriter;

class CmdBuf : public ObjBase
{
	template <bool> friend class CmdBufWriter;

	struct CtrlMemChunk
	{
		CtrlMemChunk *m_next;
		size_t m_size;
	};

	static constexpr size_t s_ctrlChunkSize = 1024 - sizeof(CtrlMemChunk);
	static constexpr auto s_reservedCtrlMem = sizeof(CtrlCmdJumpCall);

	void* m_userData;
	DkCmdBufAddMemFunc m_cbAddMem;

	uint32_t m_numReservedWords;
	bool m_hasFlushFunc;
	bool m_isCapturing;

	union
	{
		struct
		{
			CtrlMemChunk *m_ctrlChunkCur, *m_ctrlChunkFree;
		};
		struct
		{
			GpfifoFlushFunc m_flushFunc;
			void* m_flushFuncData;
		};
	};
	CtrlCmdHeader *m_ctrlGpfifo;
	void *m_ctrlStart, *m_ctrlPos, *m_ctrlEnd;
	DkGpuAddr m_cmdChunkStartIova, m_cmdStartIova;
	maxwell::CmdWord *m_cmdChunkStart, *m_cmdStart, *m_cmdPos, *m_cmdEnd;
public:
	constexpr CmdBuf(DkCmdBufMaker const& maker, uint32_t rw = 0) noexcept : ObjBase{maker.device},
		m_userData{maker.userData}, m_cbAddMem{maker.cbAddMem}, m_numReservedWords{rw}, m_hasFlushFunc{false}, m_isCapturing{false},
		m_ctrlChunkCur{}, m_ctrlChunkFree{}, m_ctrlGpfifo{}, m_ctrlStart{}, m_ctrlPos{}, m_ctrlEnd{},
		m_cmdChunkStartIova{}, m_cmdStartIova{}, m_cmdChunkStart{}, m_cmdStart{}, m_cmdPos{}, m_cmdEnd{} { }
	~CmdBuf();

	void useGpfifoFlushFunc(GpfifoFlushFunc func, void* data, CtrlCmdHeader* mem, uint32_t maxEntries)
	{
		m_hasFlushFunc = true;
		m_flushFunc = func;
		m_flushFuncData = data;
		m_ctrlGpfifo = mem;
		m_ctrlStart = nullptr;
		m_ctrlPos = m_ctrlGpfifo+1;
		m_ctrlEnd = (char*)m_ctrlPos + maxEntries*sizeof(CtrlCmdGpfifoEntry);
	}

	void unlockReservedWords()
	{
		m_cmdEnd += m_numReservedWords;
	}

	void addMemory(DkMemBlock mem, uint32_t offset, uint32_t size);
	DkCmdList finishList();
	void clear();

	void beginCapture(uint32_t* storage, uint32_t max_words);
	uint32_t endCapture();

	constexpr bool isDirty() const noexcept { return m_cmdStart != m_cmdPos; }
	constexpr bool isCapturing() const noexcept { return m_isCapturing; }
	constexpr uint32_t getCmdOffset() const noexcept { return uint32_t((char*)(void*)m_cmdPos - (char*)(void*)m_cmdChunkStart); }
	constexpr size_t getCtrlSpaceFree() const noexcept { return size_t((char*)(void*)m_ctrlEnd-(char*)(void*)m_ctrlPos); }
	maxwell::CmdWord* requestCmdMem(uint32_t size);
	CtrlCmdHeader* appendCtrlCmd(size_t size);

	bool appendRawGpfifoEntry(DkGpuAddr iova, uint32_t numCmds, uint32_t flags);
	void signOffGpfifoEntry(uint32_t flags = CtrlCmdGpfifoEntry::AutoKick)
	{
		uint32_t numCmds = m_cmdPos - m_cmdStart;
		if (numCmds && appendRawGpfifoEntry(m_cmdStartIova, numCmds, flags))
		{
			m_cmdStart = m_cmdPos;
			m_cmdStartIova += numCmds*sizeof(maxwell::CmdWord);
		}
	}

	void flushGpfifoEntries()
	{
		uint32_t numEntries = m_ctrlGpfifo->arg;
		if (numEntries)
		{
			auto* entries = reinterpret_cast<CtrlCmdGpfifoEntry*>(m_ctrlGpfifo+1);
			m_flushFunc(m_flushFuncData, entries, numEntries);
			m_ctrlGpfifo->arg = 0;
			m_ctrlPos = entries;
		}
	}
};

}
