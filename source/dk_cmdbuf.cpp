#include "dk_cmdbuf.h"
#include "dk_memblock.h"
#include "cmdbuf_writer.h"

using namespace maxwell;
using namespace dk::detail;

CmdBuf::~CmdBuf()
{
	if (m_hasFlushFunc)
		return;

	// Make sure all used chunks get transferred to the free list
	clear();

	CtrlMemChunk *cur, *next;
	for (cur = m_ctrlChunkFree; cur; cur = next)
	{
		next = cur->m_next;
		freeMem(cur);
	}
}

void CmdBuf::addMemory(DkMemBlock mem, uint32_t offset, uint32_t size)
{
	signOffGpfifoEntry();
	m_cmdChunkStartIova = mem->getGpuAddrPitch() + offset;
	m_cmdStartIova = m_cmdChunkStartIova;
	m_cmdChunkStart = (CmdWord*)((char*)mem->getCpuAddr() + offset);
	m_cmdStart = m_cmdChunkStart;
	m_cmdPos = m_cmdStart;
	m_cmdEnd = m_cmdStart + size / sizeof(CmdWord) - m_numReservedWords;
}

DkCmdList CmdBuf::finishList()
{
	// Sign off any remaining GPU commands
	signOffGpfifoEntry();

	// Retrieve the beginning of the control command list
	// If nothing was ever recorded then we just return a null list
	DkCmdList list = DkCmdList(m_ctrlStart);
	if (!list)
		return 0;

	// Finish the list with a return command (uses reserved ctrl space)
	auto* retCmd = static_cast<CtrlCmdHeader*>(m_ctrlPos);
	retCmd->type = CtrlCmdHeader::Return;
	m_ctrlPos = retCmd+1;

	// Reset internal variables
	m_ctrlGpfifo = nullptr;
	m_ctrlStart = nullptr;

	// If we've used up all available control memory in this chunk, just clear it out and move on
	if (m_ctrlPos >= m_ctrlEnd)
	{
		m_ctrlPos = nullptr;
		m_ctrlEnd = nullptr;
	}

	return list;
}

void CmdBuf::clear()
{
	// Transfer all used chunks into the free list
	if (m_ctrlChunkCur)
	{
		m_ctrlChunkCur->m_next = m_ctrlChunkFree;
		m_ctrlChunkFree = m_ctrlChunkCur;
		m_ctrlChunkCur = nullptr;
	}

	// Clear control memory management variables
	m_ctrlGpfifo = nullptr;
	m_ctrlStart = nullptr;
	m_ctrlPos = nullptr;
	m_ctrlEnd = nullptr;

	// Reset command memory back to the beginning of the chunk added by the last addMemory call
	if (m_cmdChunkStart)
	{
		m_cmdStartIova = m_cmdChunkStartIova;
		m_cmdStart = m_cmdChunkStart;
		m_cmdPos = m_cmdChunkStart;
	}
}

CmdWord* CmdBuf::requestCmdMem(uint32_t size)
{
	if (!m_cbAddMem)
	{
		DK_ERROR(DkResult_OutOfMemory, "out of command memory and no add-mem callback set");
		return nullptr;
	}
	m_cbAddMem(m_userData, this, (size+m_numReservedWords)*sizeof(CmdWord));
	if ((m_cmdPos + size) >= m_cmdEnd)
	{
		DK_ERROR(DkResult_OutOfMemory, "add-mem callback did not add enough command memory");
		return nullptr;
	}
	return m_cmdPos;
}

bool CmdBuf::appendRawGpfifoEntry(DkGpuAddr iova, uint32_t numCmds, uint32_t flags)
{
	if (m_ctrlGpfifo)
	{
		if (flags == CtrlCmdGpfifoEntry::AutoKick && m_ctrlGpfifo->arg)
		{
			// Try to coalesce this entry onto the last one, if possible
			auto* lastEntry = reinterpret_cast<CtrlCmdGpfifoEntry*>(m_ctrlGpfifo+1) + m_ctrlGpfifo->arg - 1;
			DkGpuAddr lastEntryEnd = lastEntry->iova + lastEntry->numCmds*sizeof(CmdWord);
			if (lastEntryEnd == iova)
			{
				// Success - all we need to do is update the number of commands
				lastEntry->numCmds += numCmds;
				lastEntry->flags |= CtrlCmdGpfifoEntry::AutoKick;
				return true;
			}
		}
		// If above failed, see if we have enough space to append a new entry
		if (getCtrlSpaceFree() >= sizeof(CtrlCmdGpfifoEntry))
		{
			// We can append the entry
			CtrlCmdGpfifoEntry* entry = static_cast<CtrlCmdGpfifoEntry*>(m_ctrlPos);
			m_ctrlGpfifo->arg++;
			entry->iova = iova;
			entry->numCmds = numCmds;
			entry->flags = flags;
			m_ctrlPos = entry+1;
			return true;
		}
	}

	if (m_hasFlushFunc)
	{
		// Flush the gpfifo entries directly using the provided callback
		CtrlCmdGpfifoEntry* entries = reinterpret_cast<CtrlCmdGpfifoEntry*>(m_ctrlGpfifo+1);
		m_flushFunc(m_flushFuncData, entries, m_ctrlGpfifo->arg);

		// Clear the gpfifo entry list and append this one
		m_ctrlGpfifo->arg = 1;
		entries->iova = iova;
		entries->numCmds = numCmds;
		entries->flags = flags;
		m_ctrlPos = entries+1;
		return true;
	}

	// We need to do it the hard way and create a new Gpfifo command
	m_ctrlGpfifo = appendCtrlCmd(sizeof(CtrlCmdHeader) + sizeof(CtrlCmdGpfifoEntry));
	if (m_ctrlGpfifo) // above already errored out if this failed
	{
		m_ctrlGpfifo->type = CtrlCmdHeader::GpfifoList;
		m_ctrlGpfifo->arg = 1;
		CtrlCmdGpfifoEntry* entry = reinterpret_cast<CtrlCmdGpfifoEntry*>(m_ctrlGpfifo+1);
		entry->iova = iova;
		entry->numCmds = numCmds;
		entry->flags = flags;
		return true;
	}

	return false;
}

CtrlCmdHeader* CmdBuf::appendCtrlCmd(size_t size)
{
	CtrlCmdHeader* ret = nullptr;
	if (getCtrlSpaceFree() >= size)
		ret = static_cast<CtrlCmdHeader*>(m_ctrlPos);
	else
	{
		// Calculate minimum required size for the chunk
		size_t reqSize = size + s_reservedCtrlMem;
		if (reqSize < s_ctrlChunkSize)
			reqSize = s_ctrlChunkSize;

		// Try to find a big enough chunk in the list of free chunks
		CtrlMemChunk* chunk = nullptr;
		CtrlMemChunk** prevNext = &m_ctrlChunkFree;
		for (chunk = *prevNext; chunk; prevNext = &chunk->m_next, chunk = *prevNext)
		{
			if (chunk->m_size >= reqSize)
			{
				// Found a suitable chunk - pop it off the list
				*prevNext = chunk->m_next;
				break;
			}
		}

		// If above didn't give us a chunk, we need to create a new one
		if (!chunk)
		{
			chunk = static_cast<CtrlMemChunk*>(allocMem(sizeof(CtrlMemChunk)+reqSize));
			if (chunk) // above already errored out if this failed
				chunk->m_size = reqSize;
		}

		// Make the chunk's memory available and add it to the list of used chunks
		if (chunk)
		{
			ret = reinterpret_cast<CtrlCmdHeader*>(chunk+1);
			if (m_ctrlPos)
			{
				// Add a jump command (uses reserved ctrl space)
				CtrlCmdJumpCall* jumpCmd = static_cast<CtrlCmdJumpCall*>(m_ctrlPos);
				jumpCmd->type = CtrlCmdHeader::Jump;
				jumpCmd->ptr = ret;
			}
			chunk->m_next = m_ctrlChunkCur;
			m_ctrlChunkCur = chunk;
			m_ctrlEnd = (char*)ret + chunk->m_size - s_reservedCtrlMem;
		}
	}

	if (ret)
	{
		// Update position
		if (!m_ctrlStart)
			m_ctrlStart = ret;
		m_ctrlPos = (char*)ret + size;
		m_ctrlGpfifo = nullptr;
	}
	return ret;
}

DkCmdBuf dkCmdBufCreate(DkCmdBufMaker const* maker)
{
	DK_ENTRYPOINT(maker->device);
	DkCmdBuf obj = nullptr;
	obj = new(maker->device) CmdBuf(*maker);
	return obj;
}

void dkCmdBufDestroy(DkCmdBuf obj)
{
	DK_ENTRYPOINT(obj);
	delete obj;
}

void dkCmdBufAddMemory(DkCmdBuf obj, DkMemBlock mem, uint32_t offset, uint32_t size)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_NON_NULL(mem);
	DK_DEBUG_NON_ZERO(size);
	DK_DEBUG_DATA_ALIGN(offset, DK_CMDMEM_ALIGNMENT);
	DK_DEBUG_SIZE_ALIGN(size, DK_CMDMEM_ALIGNMENT);
	DK_DEBUG_BAD_FLAGS(mem->isGpuNoAccess() || !mem->isCpuUncached(), "DkMemBlock must be created with DkMemBlockFlags_CpuUncached and DkMemBlockFlags_GpuCached");
	obj->addMemory(mem, offset, size);
}

DkCmdList dkCmdBufFinishList(DkCmdBuf obj)
{
	DK_ENTRYPOINT(obj);
	return obj->finishList();
}

void dkCmdBufClear(DkCmdBuf obj)
{
	DK_ENTRYPOINT(obj);
	obj->clear();
}

void dkCmdBufCallList(DkCmdBuf obj, DkCmdList list)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};

	auto* cmd = w.addCtrl<CtrlCmdJumpCall>();
	if (cmd)
	{
		cmd->type = CtrlCmdHeader::Call;
		cmd->ptr = reinterpret_cast<CtrlCmdHeader const*>(list);
	}
}

void dkCmdBufWaitFence(DkCmdBuf obj, DkFence* fence)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};

	auto* cmd = w.addCtrl<CtrlCmdFence>();
	if (cmd)
	{
		cmd->type = CtrlCmdHeader::WaitFence;
		cmd->fence = fence;
	}
}

void dkCmdBufSignalFence(DkCmdBuf obj, DkFence* fence, bool flush)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};

	auto* cmd = w.addCtrl<CtrlCmdFence>();
	if (cmd)
	{
		cmd->type = CtrlCmdHeader::SignalFence;
		cmd->arg = flush ? 1 : 0;
		cmd->fence = fence;
	}
}
