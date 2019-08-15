#pragma once
#include "dk_private.h"
#include "dk_memblock.h"
#include "dk_ctrlcmd.h"
#include "maxwell/command.h"

namespace dk::detail
{
	using GpfifoFlushFunc = void(*)(void* data, CtrlCmdGpfifoEntry const* entries, uint32_t numEntries);
}

class tag_DkCmdBuf : public dk::detail::ObjBase
{
	struct CtrlMemChunk
	{
		CtrlMemChunk *m_next;
		size_t m_size;
	};

	static constexpr size_t s_ctrlChunkSize = 1024 - sizeof(CtrlMemChunk);
	static constexpr auto s_reservedCtrlMem = sizeof(dk::detail::CtrlCmdJumpCall);

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
			dk::detail::GpfifoFlushFunc m_flushFunc;
			void* m_flushFuncData;
		};
	};
	dk::detail::CtrlCmdHeader *m_ctrlGpfifo;
	void *m_ctrlPos, *m_ctrlEnd;
	DkGpuAddr m_cmdStartIova;
	maxwell::CmdWord *m_cmdChunkStart, *m_cmdStart, *m_cmdPos, *m_cmdEnd;
public:
	constexpr tag_DkCmdBuf(DkCmdBufMaker const& maker, uint32_t rw = 0) noexcept : ObjBase{maker.device},
		m_userData{maker.userData}, m_cbAddMem{maker.cbAddMem}, m_numReservedWords{rw}, m_hasFlushFunc{false},
		m_ctrlChunkFirst{}, m_ctrlChunkCur{}, m_ctrlGpfifo{}, m_ctrlPos{}, m_ctrlEnd{},
		m_cmdStartIova{}, m_cmdChunkStart{}, m_cmdStart{}, m_cmdPos{}, m_cmdEnd{} { }
	~tag_DkCmdBuf();

	void useGpfifoFlushFunc(dk::detail::GpfifoFlushFunc func, void* data, dk::detail::CtrlCmdHeader* mem, uint32_t maxEntries)
	{
		m_hasFlushFunc = true;
		m_flushFunc = func;
		m_flushFuncData = data;
		m_ctrlGpfifo = mem;
		m_ctrlPos = m_ctrlGpfifo+1;
		m_ctrlEnd = (char*)m_ctrlPos + maxEntries*sizeof(dk::detail::CtrlCmdGpfifoEntry);
	}

	void unlockReservedWords()
	{
		m_cmdEnd += m_numReservedWords;
	}

	void addMemory(DkMemBlock mem, uint32_t offset, uint32_t size);
	DkCmdList finishList();

	constexpr bool isDirty() const noexcept { return m_cmdStart != m_cmdPos; }
	constexpr uint32_t getCmdOffset() const noexcept { return uint32_t((char*)m_cmdPos - (char*)m_cmdChunkStart); }
	constexpr size_t getCtrlSpaceFree() const noexcept { return size_t((char*)m_ctrlEnd-(char*)m_ctrlPos); }
	maxwell::CmdWord* requestCmdMem(uint32_t size);

	bool appendRawGpfifoEntry(DkGpuAddr iova, uint32_t numCmds, uint32_t flags);
	void signOffGpfifoEntry(uint32_t flags = dk::detail::CtrlCmdGpfifoEntry::AutoKick)
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
			auto* entries = reinterpret_cast<dk::detail::CtrlCmdGpfifoEntry*>(m_ctrlGpfifo+1);
			m_flushFunc(m_flushFuncData, entries, numEntries);
			m_ctrlGpfifo->arg = 0;
			m_ctrlPos = entries;
		}
	}

	dk::detail::CtrlCmdHeader* appendCtrlCmd(size_t size);
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
	void appendDirectly(maxwell::CmdList<size> const& cmds)
	{
		cmds.copyTo(m_cmdPos);
		m_cmdPos += size;
	}

	template <uint32_t size>
	void appendDirectly(maxwell::CmdList<size>&& cmds)
	{
		cmds.moveTo(std::move(m_cmdPos));
		m_cmdPos += size;
	}

	template <uint32_t... sizes>
	void appendDirectly(maxwell::CmdList<sizes>&&... cmds)
	{
		auto list = Cmds(std::move(cmds)...);
		appendDirectly(std::move(list));
	}

	template <uint32_t size>
	maxwell::CmdWord* append(maxwell::CmdList<size> const& cmds)
	{
		maxwell::CmdWord* p = reserveCmdMem(size);
		if (p) appendDirectly(cmds);
		return p;
	}

	template <uint32_t size>
	maxwell::CmdWord* append(maxwell::CmdList<size>&& cmds)
	{
		maxwell::CmdWord* p = reserveCmdMem(size);
		if (p) appendDirectly(std::move(cmds));
		return p;
	}

	template <uint32_t... sizes>
	maxwell::CmdWord* append(maxwell::CmdList<sizes>&&... cmds)
	{
		auto list = Cmds(std::move(cmds)...);
		return append(std::move(list));
	}
};
