#pragma once
#include "dk_private.h"
#include "dk_memblock.h"
#include "dk_ctrlcmd.h"
#include "maxwell/command.h"

typedef void (*DkGpfifoFlushFunc)(void* data, CtrlCmdGpfifoEntry const* entries, uint32_t numEntries);

class tag_DkCmdBuf : public DkObjBase
{
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

	union
	{
		struct
		{
			CtrlMemChunk *m_ctrlChunkFirst, *m_ctrlChunkCur;
		};
		struct
		{
			DkGpfifoFlushFunc m_flushFunc;
			void* m_flushFuncData;
		};
	};
	CtrlCmdHeader *m_ctrlGpfifo;
	void *m_ctrlPos, *m_ctrlEnd;
	DkGpuAddr m_cmdStartIova;
	maxwell::CmdWord *m_cmdChunkStart, *m_cmdStart, *m_cmdPos, *m_cmdEnd;
public:
	constexpr tag_DkCmdBuf(DkCmdBufMaker const& maker, uint32_t rw = 0) noexcept : DkObjBase{maker.device},
		m_userData{maker.userData}, m_cbAddMem{maker.cbAddMem}, m_numReservedWords{rw}, m_hasFlushFunc{false},
		m_ctrlChunkFirst{}, m_ctrlChunkCur{}, m_ctrlGpfifo{}, m_ctrlPos{}, m_ctrlEnd{},
		m_cmdStartIova{}, m_cmdChunkStart{}, m_cmdStart{}, m_cmdPos{}, m_cmdEnd{} { }
	~tag_DkCmdBuf();

	void useGpfifoFlushFunc(DkGpfifoFlushFunc func, void* data, CtrlCmdHeader* mem, uint32_t maxEntries)
	{
		m_hasFlushFunc = true;
		m_flushFunc = func;
		m_flushFuncData = data;
		m_ctrlGpfifo = mem;
		m_ctrlPos = m_ctrlGpfifo+1;
		m_ctrlEnd = (char*)m_ctrlPos + maxEntries*sizeof(CtrlCmdGpfifoEntry);
	}

	void unlockReservedWords()
	{
		m_cmdEnd += m_numReservedWords;
	}

	void addMemory(DkMemBlock mem, uint32_t offset, uint32_t size);
	DkCmdList finishList();

	constexpr uint32_t getCmdOffset() const noexcept { return uint32_t((char*)m_cmdPos - (char*)m_cmdChunkStart); }
	constexpr size_t getCtrlSpaceFree() const noexcept { return size_t((char*)m_ctrlEnd-(char*)m_ctrlPos); }
	maxwell::CmdWord* requestCmdMem(uint32_t size);

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
			CtrlCmdGpfifoEntry* entries = reinterpret_cast<CtrlCmdGpfifoEntry*>(m_ctrlGpfifo+1);
			m_flushFunc(m_flushFuncData, entries, numEntries);
			m_ctrlGpfifo->arg = 0;
			m_ctrlPos = entries;
		}
	}

	CtrlCmdHeader* appendCtrlCmd(size_t size);
	template <typename T>
	T* appendCtrlCmd()
	{
		signOffGpfifoEntry();
		return static_cast<T*>(appendCtrlCmd(sizeof(T)));
	}

	maxwell::CmdWord* reserveCmdMem(uint32_t size)
	{
		maxwell::CmdWord *pos = m_cmdPos;
		if ((pos + size) >= m_cmdEnd)
			pos = requestCmdMem(size);
		return pos;
	}

	template <uint32_t size>
	maxwell::CmdWord* append(maxwell::CmdList<size> const& cmds)
	{
		maxwell::CmdWord* p = reserveCmdMem(size);
		if (p)
		{
			cmds.copyTo(p);
			m_cmdPos = p + size;
		}
		return p;
	}

	template <uint32_t size>
	maxwell::CmdWord* append(maxwell::CmdList<size>&& cmds)
	{
		maxwell::CmdWord* p = reserveCmdMem(size);
		if (p)
		{
			cmds.moveTo(std::move(p));
			m_cmdPos = p + size;
		}
		return p;
	}

	template <uint32_t... sizes>
	maxwell::CmdWord* append(maxwell::CmdList<sizes>&&... cmds)
	{
		auto list = (std::move(cmds) + ...);
		return append(std::move(list));
	}
};
