#include "dk_queue.h"

void tag_DkQueue::addCmdMemory(size_t minReqSize)
{
	uint32_t inFlightSize = getInFlightCmdSize();
	uint32_t idealSize = minReqSize;
	if (inFlightSize + minReqSize < m_cmdBufFlushThreshold)
		idealSize = m_cmdBufFlushThreshold - inFlightSize;

	uint32_t offset;
	uint32_t availableSize = m_cmdBufRing.reserve(offset, minReqSize);
	bool peek = true;
	while (!availableSize)
	{
		waitFenceRing(peek);
		peek = false;
		availableSize = m_cmdBufRing.reserve(offset, minReqSize);
	}

	m_cmdBuf.addMemory(&m_workBufMemBlock, offset, availableSize < idealSize ? availableSize : idealSize);
}

bool tag_DkQueue::waitFenceRing(bool peek)
{
	uint32_t id;
	int32_t timeout = peek ? 0 : -1;
	bool waited = false;
	while (m_fenceRing.getFirstInFlight(id))
	{
		DkResult res = m_fences[id].wait(timeout);
		if (res == DkResult_Timeout)
			break;
		m_cmdBufRing.updateConsumer(m_fenceCmdOffsets[id]);
		m_fenceRing.consumeOne();
		timeout = 0;
		waited = true;
	}
	return waited;
}

void tag_DkQueue::flushRing()
{
	uint32_t id;
	while (!m_fenceRing.reserve(id, 1))
		waitFenceRing();

	// TODO:
	// - Unlock cmdbuf reserved space
	// - fenceSync() (generates cmds)
	m_fenceCmdOffsets[id] = getCmdOffset();
	m_fenceRing.updateProducer(id+1);
}

void tag_DkQueue::onCmdBufAddMem(size_t minReqSize)
{
	bool shouldAddMem = false;
	if (getInFlightCmdSize() + 2*minReqSize < m_cmdBufFlushThreshold)
		shouldAddMem = true;
	else
	{
		flush();
		shouldAddMem = minReqSize > m_cmdBufPerFenceSliceSize;
	}
	if (shouldAddMem)
	{
		m_cmdBufRing.updateProducer(getCmdOffset());
		addCmdMemory(minReqSize);
	}
}

void tag_DkQueue::appendGpfifoEntries(CtrlCmdGpfifoEntry const* entries, uint32_t numEntries)
{
	for (unsigned i = 0; i < numEntries; i ++)
	{
		auto& ent = entries[i];
#ifdef DEBUG
		printf("  [%u]: iova 0x%010lx numCmds %u flags %x\n", i, ent.iova, ent.numCmds, ent.flags);
#endif
		// TODO: nvGpuChannelAppendEntry
		(void)ent;
	}
}

void tag_DkQueue::submitCommands(DkCmdList list)
{
	CtrlCmdHeader const *cur, *next;
	for (cur = reinterpret_cast<CtrlCmdHeader*>(list); cur; cur = next)
	{
#ifdef DEBUG
		printf("cmd %u (%u) (%p)\n", unsigned(cur->type), unsigned(cur->arg), cur);
#endif
		switch (cur->type)
		{
			default:
			case CtrlCmdHeader::Return:
				next = nullptr;
				break;
			case CtrlCmdHeader::Jump:
			case CtrlCmdHeader::Call:
			{
				auto* cmd = static_cast<CtrlCmdJumpCall const*>(cur);
#ifdef DEBUG
				printf("--> %p\n", cmd->ptr);
#endif
				if (cur->type == CtrlCmdHeader::Jump)
					next = cmd->ptr;
				else
				{
					submitCommands(DkCmdList(cmd->ptr));
					next = cmd+1;
				}
				break;
			}
			case CtrlCmdHeader::SignalFence:
			case CtrlCmdHeader::WaitFence:
			{
				auto* cmd = static_cast<CtrlCmdFence const*>(cur);
#ifdef DEBUG
				printf("--> %p\n", cmd->fence);
#endif
				next = cmd+1;
				break;
			}
			case CtrlCmdHeader::GpfifoList:
			{
				auto* entries = reinterpret_cast<CtrlCmdGpfifoEntry const*>(cur+1);
				flushCmdBuf();
				appendGpfifoEntries(entries, cur->arg);
				next = reinterpret_cast<CtrlCmdHeader const*>(entries+cur->arg);
				break;
			}
		}
	}
}

void tag_DkQueue::flush()
{
	// TODO: check error state
	if (m_cmdBufCtrlHeader.arg || m_cmdBuf.getCmdOffset())
	{
		if (getInFlightCmdSize() >= m_cmdBufPerFenceSliceSize)
			flushRing();
		flushCmdBuf();
		// TODO:
		// - Do the ZBC shit
		// - nvGpuChannelKickoff
#ifdef DEBUG
		printf("Kickoff happens\n");
#endif
		// - Update device query data (is this really necessary?)
		m_cmdBufRing.updateProducer(getCmdOffset());
		addCmdMemory(m_cmdBufPerFenceSliceSize);
		// - post-submit flush cmds
		m_cmdBuf.flushGpfifoEntries();
	}
}
