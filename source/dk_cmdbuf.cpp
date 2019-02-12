#include "dk_cmdbuf.h"
#include "dk_memblock.h"

using namespace maxwell;

tag_DkCmdBuf::~tag_DkCmdBuf()
{
	CtrlMemChunk *cur, *next;
	for (cur = m_ctrlChunkFirst; cur; cur = next)
	{
		next = cur->m_next;
		freeMem(cur);
	}
}

void tag_DkCmdBuf::addMemory(DkMemBlock mem, uint32_t offset, uint32_t size)
{
#ifdef DEBUG
	if (!mem || !size)
		return raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (offset & (DK_CMDMEM_ALIGNMENT-1))
		return raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedData);
	if (size & (DK_CMDMEM_ALIGNMENT-1))
		return raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedSize);
	if (mem->isGpuNoAccess() || !mem->isCpuUncached())
		return raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadMemFlags);
#endif
	signOffGpfifoEntry();
	m_cmdStartIova = mem->getGpuAddrPitch() + offset;
	m_cmdStart = (CmdWord*)((char*)mem->getCpuAddr() + offset);
	m_cmdPos = m_cmdStart;
	m_cmdEnd = m_cmdStart + size / sizeof(CmdWord);
}

DkCmdList tag_DkCmdBuf::finishList()
{
	// Sign off any remaining GPU commands
	signOffGpfifoEntry();
#ifdef DEBUG
	if (!m_ctrlChunkCur)
	{
		raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadState);
		return 0;
	}
#endif

	// Finish the list with a return command (uses reserved ctrl space)
	auto* retCmd = static_cast<CtrlCmdHeader*>(m_ctrlPos);
	retCmd->type = CtrlCmdHeader::Return;

	// Reset internal variables
	m_ctrlChunkCur = m_ctrlChunkFirst;
	m_ctrlGpfifo = nullptr;
	m_ctrlPos = m_ctrlChunkFirst+1;
	m_ctrlEnd = (char*)m_ctrlPos + m_ctrlChunkFirst->m_size - s_reservedCtrlMem;

	// The final cmdlist starts after the header of the first chunk
	return DkCmdList(m_ctrlChunkFirst+1);
}

CmdWord* tag_DkCmdBuf::requestCmdMem(uint32_t size)
{
	if (!m_cbAddMem)
	{
		raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_OutOfMemory);
		return nullptr;
	}
	m_cbAddMem(m_userData, this, size*sizeof(CmdWord));
	if ((m_cmdPos + size) >= m_cmdEnd)
	{
		raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_OutOfMemory);
		return nullptr;
	}
	return m_cmdPos;
}

bool tag_DkCmdBuf::appendRawGpfifoEntry(DkGpuAddr iova, uint32_t numCmds, uint32_t flags)
{
	bool didIt = false;

	if (m_ctrlGpfifo)
	{
		if (flags == CtrlCmdGpfifoEntry::AutoKick)
		{
			// Try to coalesce this entry onto the last one, if possible
			auto* lastEntry = reinterpret_cast<CtrlCmdGpfifoEntry*>(m_ctrlGpfifo+1) + m_ctrlGpfifo->arg - 1;
			DkGpuAddr lastEntryEnd = lastEntry->iova + lastEntry->numCmds*sizeof(CmdWord);
			if (lastEntry->flags == flags && lastEntryEnd == iova)
			{
				// Success - all we need to do is update the number of commands
				lastEntry->numCmds += numCmds;
				lastEntry->flags = flags;
				didIt = true;
			}
		}
		// If above failed, see if we have enough space to append a new entry
		if (!didIt && getCtrlSpaceFree() >= sizeof(CtrlCmdGpfifoEntry))
		{
			// We can append the entry
			CtrlCmdGpfifoEntry* entry = static_cast<CtrlCmdGpfifoEntry*>(m_ctrlPos);
			m_ctrlGpfifo->arg++;
			entry->iova = iova;
			entry->numCmds = numCmds;
			entry->flags = flags;
			m_ctrlPos = entry+1;
			didIt = true;
		}
	}

	if (!didIt)
	{
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
			didIt = true;
		}
	}

	return didIt;
}

CtrlCmdHeader* tag_DkCmdBuf::appendCtrlCmd(size_t size)
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

		// Try to find a chunk in the list that is big enough
		CtrlMemChunk* nextChunk = nullptr;
		CtrlMemChunk* prevChunk = m_ctrlChunkCur;
		if (prevChunk)
		{
			for (nextChunk = prevChunk->m_next; nextChunk; prevChunk = nextChunk, nextChunk = prevChunk->m_next)
			{
				if (nextChunk->m_size >= reqSize)
				{
					// Found a suitable chunk - pop it off the list and make it the next one
					prevChunk->m_next = nextChunk->m_next;
					nextChunk->m_next = m_ctrlChunkCur->m_next;
					m_ctrlChunkCur->m_next = nextChunk;
				}
			}
		}
		if (!nextChunk)
		{
			// No such chunk exists, so create a new one.
			nextChunk = static_cast<CtrlMemChunk*>(allocMem(sizeof(CtrlMemChunk)+reqSize));
			if (nextChunk) // above already errored out if this failed
			{
				// Initialize and append it to the list of chunks
				if (m_ctrlChunkCur)
					m_ctrlChunkCur->m_next = nextChunk;
				else
					m_ctrlChunkFirst = nextChunk;
				nextChunk->m_next = nullptr;
				nextChunk->m_size = reqSize;
			}
		}
		if (nextChunk)
		{
			ret = reinterpret_cast<CtrlCmdHeader*>(nextChunk+1);
			if (m_ctrlChunkCur)
			{
				// Add a jump command (uses reserved ctrl space)
				CtrlCmdJumpCall* jumpCmd = static_cast<CtrlCmdJumpCall*>(m_ctrlPos);
				jumpCmd->type = CtrlCmdHeader::Jump;
				jumpCmd->ptr = ret;
			}
			m_ctrlChunkCur = nextChunk;
			m_ctrlPos = ret;
			m_ctrlEnd = (char*)m_ctrlPos + nextChunk->m_size - s_reservedCtrlMem;
		}
	}

	if (ret)
	{
		// Update position
		m_ctrlPos = (char*)m_ctrlPos + size;
		m_ctrlGpfifo = nullptr;
	}
	return ret;
}

DkCmdBuf dkCmdBufCreate(DkCmdBufMaker const* maker)
{
	DkCmdBuf obj = nullptr;
	obj = new(maker->device) tag_DkCmdBuf(*maker);
	return obj;
}

void dkCmdBufDestroy(DkCmdBuf obj)
{
	delete obj;
}

void dkCmdBufAddMemory(DkCmdBuf obj, DkMemBlock mem, uint32_t offset, uint32_t size)
{
	obj->addMemory(mem, offset, size);
}

DkCmdList dkCmdBufFinishList(DkCmdBuf obj)
{
	return obj->finishList();
}

void dkCmdBufWaitFence(DkCmdBuf obj, DkFence* fence)
{
	auto* cmd = obj->appendCtrlCmd<CtrlCmdFence>();
	if (cmd)
	{
		cmd->type = CtrlCmdHeader::WaitFence;
		cmd->fence = fence;
	}
}

void dkCmdBufSignalFence(DkCmdBuf obj, DkFence* fence, bool flush)
{
	auto* cmd = obj->appendCtrlCmd<CtrlCmdFence>();
	if (cmd)
	{
		cmd->type = CtrlCmdHeader::SignalFence;
		cmd->arg = flush ? 1 : 0;
		cmd->fence = fence;
	}
}
