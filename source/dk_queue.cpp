#include "dk_queue.h"
#include "dk_device.h"

#include "maxwell/helpers.h"
#include "engine_3d.h"

using namespace maxwell;

DkResult tag_DkQueue::initialize()
{
	DkResult res;

	// Create GPU channel
	if (R_FAILED(nvGpuChannelCreate(&m_gpuChannel, getDevice()->getAddrSpace())))
		return DkResult_Fail;

	// Allocate cmdbuf
	res = m_cmdBufMemBlock.initialize(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuUncached, nullptr, m_cmdBufRing.getSize());
	if (res != DkResult_Success)
		return res;

	// Add initial chunk of command memory for init purposes
	addCmdMemory(m_cmdBufRing.getSize()/2);
#ifdef DEBUG
	printf("cmdBufRing: sz=0x%x con=0x%x pro=0x%x fli=0x%x\n", m_cmdBufRing.getSize(), m_cmdBufRing.getConsumer(), m_cmdBufRing.getProducer(), m_cmdBufRing.getInFlight());
#endif

	// TODO:
	// - Calculate work buffer size (including zcullctx)
	// - Allocate work buffer
	// - nvGpuChannelZcullBind
	setupEngines();
	// - Perform gpu init
	// - Post-submit flush commands
	flush();

#ifdef DEBUG
	printf("cmdBufRing: sz=0x%x con=0x%x pro=0x%x fli=0x%x\n", m_cmdBufRing.getSize(), m_cmdBufRing.getConsumer(), m_cmdBufRing.getProducer(), m_cmdBufRing.getInFlight());
#endif

	m_state = Healthy;
	getDevice()->registerQueue(m_id, this);
	return DkResult_Success;
}

tag_DkQueue::~tag_DkQueue()
{
	if (m_state == Healthy)
	{
		DkFence fence;
		signalFence(fence, true);
		flush();
		fence.wait();
	}
	nvGpuChannelClose(&m_gpuChannel);
	getDevice()->returnQueueId(m_id);
}

void tag_DkQueue::addCmdMemory(size_t minReqSize)
{
	uint32_t inFlightSize = getInFlightCmdSize();
	uint32_t idealSize = minReqSize;
	if (inFlightSize + minReqSize < m_cmdBufFlushThreshold)
		idealSize = m_cmdBufFlushThreshold - inFlightSize;

	uint32_t offset;
#ifdef DEBUG
	printf("cmdBufRing: sz=0x%x con=0x%x pro=0x%x fli=0x%x\n", m_cmdBufRing.getSize(), m_cmdBufRing.getConsumer(), m_cmdBufRing.getProducer(), m_cmdBufRing.getInFlight());
	printf("reserving 0x%x\n", (unsigned)minReqSize);
#endif
	uint32_t availableSize = m_cmdBufRing.reserve(offset, minReqSize);
	bool peek = true;
	while (!availableSize)
	{
		waitFenceRing(peek);
		peek = false;
		availableSize = m_cmdBufRing.reserve(offset, minReqSize);
	}

	m_cmdBuf.addMemory(&m_cmdBufMemBlock, offset, availableSize < idealSize ? availableSize : idealSize);
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

void tag_DkQueue::flushRing(bool fenceFlush)
{
	uint32_t id;
	bool peek = true;
	do
	{
		waitFenceRing(peek);
		peek = false;
	}
	while (!m_fenceRing.reserve(id, 1));

	m_cmdBuf.unlockReservedWords();
	signalFence(m_fences[id], fenceFlush);
	m_fenceLastFlushOffset = m_fenceCmdOffsets[id] = getCmdOffset();
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
		u32 flags = GPFIFO_ENTRY_NOT_MAIN | ((ent.flags & CtrlCmdGpfifoEntry::NoPrefetch) ? GPFIFO_ENTRY_NO_PREFETCH : 0);
		u32 threshold = (ent.flags & CtrlCmdGpfifoEntry::AutoKick) ? 8 : 0;
		if (R_FAILED(nvGpuChannelAppendEntry(&m_gpuChannel, ent.iova, ent.numCmds, flags, threshold)))
		{
			m_state = Error;
			raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_Fail);
			return;
		}
	}
}

void tag_DkQueue::waitFence(DkFence& fence)
{
	// TODO
}

void tag_DkQueue::signalFence(DkFence& fence, bool flush)
{
#ifdef DEBUG
	printf("  signalFence %p %d\n", &fence, flush);
#endif
	if (!isInErrorState())
	{
		using A = Engine3D::SyncptAction;
		u32 id = nvGpuChannelGetSyncpointId(&m_gpuChannel);
		uint32_t action = A::Id{id} | A::Increment{};
		if (!flush)
		{
			m_cmdBuf.append(
				CmdInline(3D, UnknownFlush{}, 0),
				Cmd(3D, SyncptAction{}, action)
			);
		} else
		{
			action |= A::FlushCache{};
			m_cmdBuf.append(
				CmdInline(3D, UnknownFlush{}, 0),
				Cmd(3D, SyncptAction{}, action),
				Cmd(3D, SyncptAction{}, action)
			);
			nvGpuChannelIncrFence(&m_gpuChannel);
		}
		nvGpuChannelIncrFence(&m_gpuChannel);
		// TODO: Perform query shit
	}

	fence.m_type = DkFence::Internal;
	nvGpuChannelGetFence(&m_gpuChannel, &fence.m_internal.m_fence);
#ifdef DEBUG
	//printf("  --> %u,%u\n", fence.m_internal.m_fence.id, fence.m_internal.m_fence.value);
#endif
	// TODO: Fill in query shit
}

void tag_DkQueue::submitCommands(DkCmdList list)
{
	CtrlCmdHeader const *cur, *next;
	for (cur = reinterpret_cast<CtrlCmdHeader*>(list); cur; cur = next)
	{
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
				if (cur->type == CtrlCmdHeader::Jump)
					next = cmd->ptr;
				else
				{
					submitCommands(DkCmdList(cmd->ptr));
					next = cmd+1;
				}
				break;
			}
			case CtrlCmdHeader::WaitFence:
			{
				auto* cmd = static_cast<CtrlCmdFence const*>(cur);
				waitFence(*cmd->fence);
				next = cmd+1;
				break;
			}
			case CtrlCmdHeader::SignalFence:
			{
				auto* cmd = static_cast<CtrlCmdFence const*>(cur);
				signalFence(*cmd->fence, cur->arg != 0);
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
	if (isInErrorState())
	{
		raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_Fail);
		return;
	}

	if (m_gpuChannel.num_entries || hasPendingCommands())
	{
		if (getSizeSinceLastFenceFlush() >= m_cmdBufPerFenceSliceSize)
			flushRing();
		flushCmdBuf();
		// TODO:
		// - Do the ZBC shit
		if (R_FAILED(nvGpuChannelKickoff(&m_gpuChannel)))
		{
			m_state = Error;
			raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_Fail);
			return;
		}
		// - Update device query data (is this really necessary?)
		m_cmdBufRing.updateProducer(getCmdOffset());
		addCmdMemory(m_cmdBufPerFenceSliceSize);
		// - post-submit flush cmds
		m_cmdBuf.flushGpfifoEntries();
	}
}

DkQueue dkQueueCreate(DkQueueMaker const* maker)
{
	DkQueue obj = nullptr;
#ifdef DEBUG
	if (!(maker->flags & (DkQueueFlags_Graphics|DkQueueFlags_Compute|DkQueueFlags_Transfer)))
		DkObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	else if (maker->commandMemorySize < DK_QUEUE_MIN_CMDMEM_SIZE)
		DkObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	else if (maker->commandMemorySize & (DK_MEMBLOCK_ALIGNMENT-1))
		DkObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedSize);
	else if (maker->flushThreshold < DK_MEMBLOCK_ALIGNMENT || maker->flushThreshold > maker->commandMemorySize)
		DkObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	else
#endif
	{
		int32_t id = maker->device->reserveQueueId();
		if (id >= 0)
			obj = new(maker->device) tag_DkQueue(*maker, id);
	}
	if (obj)
	{
		DkResult res = obj->initialize();
		if (res != DkResult_Success)
		{
			delete obj;
			obj = nullptr;
			DkObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, res);
		}
	}
	return obj;
}

void dkQueueDestroy(DkQueue obj)
{
	delete obj;
}

bool dkQueueIsInErrorState(DkQueue obj)
{
	return obj->isInErrorState();
}

void dkQueueWaitFence(DkQueue obj, DkFence* fence)
{
	obj->waitFence(*fence);
}

void dkQueueSignalFence(DkQueue obj, DkFence* fence, bool flush)
{
	obj->signalFence(*fence, flush);
}

void dkQueueSubmitCommands(DkQueue obj, DkCmdList cmds)
{
	obj->submitCommands(cmds);
}

void dkQueueFlush(DkQueue obj)
{
	obj->flush();
}

void dkQueuePresent(DkQueue obj, DkWindow window, int imageSlot)
{
	// TODO
}
