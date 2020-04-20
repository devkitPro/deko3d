#include "dk_queue.h"
#include "dk_device.h"
#include "queue_compute.h"

#include "cmdbuf_writer.h"

#include "engine_3d.h"
#include "engine_gpfifo.h"

using namespace maxwell;
using namespace dk::detail;

DkResult Queue::initialize()
{
	DkResult res;

	// Calculate channel priority
	NvChannelPriority prio;
	switch (m_flags & DkQueueFlags_PrioMask)
	{
		default:
		case DkQueueFlags_MediumPrio: prio = NvChannelPriority_Medium; break;
		case DkQueueFlags_HighPrio:   prio = NvChannelPriority_High;   break;
		case DkQueueFlags_LowPrio:    prio = NvChannelPriority_Low;    break;
	}

	// Create GPU channel
	if (R_FAILED(nvGpuChannelCreate(&m_gpuChannel, getDevice()->getAddrSpace(), prio)))
		return DkResult_Fail;

	// Allocate cmdbuf
	res = m_cmdBufMemBlock.initialize(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuUncached, nullptr, m_cmdBufRing.getSize());
	if (res != DkResult_Success)
		return res;

	// Add initial chunk of command memory for init purposes
	addCmdMemory(m_cmdBufRing.getSize()/2);
#ifdef DK_QUEUE_DEBUG
	printf("cmdBufRing: sz=0x%x con=0x%x pro=0x%x fli=0x%x\n", m_cmdBufRing.getSize(), m_cmdBufRing.getConsumer(), m_cmdBufRing.getProducer(), m_cmdBufRing.getInFlight());
#endif

	// Allocate the work buffer
	res = m_workBuf.initialize();
	if (res != DkResult_Success)
		return res;

#ifdef DK_QUEUE_WORKBUF_DEBUG
	printf("Work buf: 0x%010lx\n", m_workBuf.getGpuAddrPitch());
#endif

	// Bind the Zcull context if present
	if (m_workBuf.getZcullCtxSize())
	{
		Result rc = nvGpuChannelZcullBind(&m_gpuChannel, m_workBuf.getZcullCtx());
		if (R_FAILED(rc))
			return DkResult_Fail;
	}

	setupEngines();
	setupTransfer();
	if (hasGraphics())
		setup3DEngine();
	if (hasCompute())
	{
		m_computeQueue = new(this+1) ComputeQueue(this);
		m_computeQueue->initialize();
	}
	postSubmitFlush();
	flush();

#ifdef DK_QUEUE_DEBUG
	printf("cmdBufRing: sz=0x%x con=0x%x pro=0x%x fli=0x%x\n", m_cmdBufRing.getSize(), m_cmdBufRing.getConsumer(), m_cmdBufRing.getProducer(), m_cmdBufRing.getInFlight());
#endif

	m_state = Healthy;
	getDevice()->registerQueue(m_id, this);
	return DkResult_Success;
}

Queue::~Queue()
{
	if (m_state == Healthy)
		waitIdle();

	if (m_computeQueue)
		m_computeQueue->~ComputeQueue();

	nvGpuChannelClose(&m_gpuChannel);
	getDevice()->returnQueueId(m_id);
}

void Queue::addCmdMemory(size_t minReqSize)
{
	uint32_t inFlightSize = getInFlightCmdSize();
	uint32_t idealSize = minReqSize;
	if (inFlightSize + minReqSize < m_cmdBufFlushThreshold)
		idealSize = m_cmdBufFlushThreshold - inFlightSize;

	uint32_t offset;
#ifdef DK_QUEUE_DEBUG
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

bool Queue::waitFenceRing(bool peek)
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

void Queue::flushRing(bool fenceFlush)
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

void Queue::onCmdBufAddMem(size_t minReqSize)
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

void Queue::appendGpfifoEntries(CtrlCmdGpfifoEntry const* entries, uint32_t numEntries)
{
	for (unsigned i = 0; i < numEntries; i ++)
	{
		auto& ent = entries[i];
#ifdef DK_QUEUE_DEBUG
		printf("  [%u]: iova 0x%010lx numCmds %u flags %x\n", i, ent.iova, ent.numCmds, ent.flags);
#endif
		u32 flags = GPFIFO_ENTRY_NOT_MAIN | ((ent.flags & CtrlCmdGpfifoEntry::NoPrefetch) ? GPFIFO_ENTRY_NO_PREFETCH : 0);
		u32 threshold = (ent.flags & CtrlCmdGpfifoEntry::AutoKick) ? 8 : 0;
		if (R_FAILED(nvGpuChannelAppendEntry(&m_gpuChannel, ent.iova, ent.numCmds, flags, threshold)))
		{
			if (!checkError())
				DK_ERROR(DkResult_Fail, "gpfifo entry append failed, but no error was reported");
			return;
		}
	}
}

void Queue::waitFence(DkFence& fence)
{
#ifdef DK_QUEUE_DEBUG
	printf("  waitFence %p\n", &fence);
#endif

	if (isInErrorState())
		return;

	using S = EngineGpfifo::Semaphore;
	using F = EngineGpfifo::FenceAction;
	CmdBufWriterChecked w{&m_cmdBuf};

	switch (fence.m_type)
	{
		case DkFence::Type::Invalid:
			break;
		case DkFence::Type::Internal:
			w << Cmd(Gpfifo, SemaphoreOffset{},
				Iova(fence.m_internal.m_semaphoreAddr),
				fence.m_internal.m_semaphoreValue,
				S::Operation::AcqGeq | S::AcquireSwitch{}
			);
			break;
		case DkFence::Type::External:
			for (u32 i = 0; i < fence.m_external.m_fence.num_fences; i ++)
			{
				NvFence const& f = fence.m_external.m_fence.fences[i];
				if ((s32)f.id < 0) continue;
				w << Cmd(Gpfifo, FenceValue{},
					f.value,
					F::Operation::Acquire | F::Id{f.id}
				);
			}
			break;
		default:
			DK_ERROR(DkResult_BadState, "invalid fence type");
	}
}

void Queue::signalFence(DkFence& fence, bool flush)
{
#ifdef DK_QUEUE_DEBUG
	printf("  signalFence %p %d\n", &fence, flush);
#endif

	fence.m_type = DkFence::Internal;
	fence.m_internal.m_semaphoreAddr = getDevice()->getSemaphoreGpuAddr(m_id);
	fence.m_internal.m_semaphoreCpuAddr = &getDevice()->getSemaphoreCpuAddr(m_id)->sequence;
	fence.m_internal.m_device = getDevice();

	if (!isInErrorState())
	{
		using A = Engine3D::SyncptAction;
		using S = Engine3D::SetReportSemaphore;
		u32 id = nvGpuChannelGetSyncpointId(&m_gpuChannel);
		uint32_t action = A::Id{id} | A::Increment{};
		CmdBufWriter w{&m_cmdBuf};
		w.reserve(12);

		w << CmdInline(3D, UnknownFlush{}, 0);
		if (!flush)
			w << Cmd(3D, SyncptAction{}, action);
		else
		{
			action |= A::FlushCache{};
			w << Cmd(3D, SyncptAction{}, action);
			w << Cmd(3D, SyncptAction{}, action);
			nvGpuChannelIncrFence(&m_gpuChannel);
		}
		nvGpuChannelIncrFence(&m_gpuChannel);
		fence.m_internal.m_semaphoreValue = getDevice()->incrSemaphoreValue(m_id);

		w << CmdInline(3D, UnknownFlush{}, 0);
		w << Cmd(3D, SetReportSemaphoreOffset{},
			Iova(fence.m_internal.m_semaphoreAddr),
			fence.m_internal.m_semaphoreValue,
			S::Operation::Release | S::FenceEnable{} | S::Unit::Crop | S::StructureSize::OneWord
		);

		w << CmdInline(3D, TiledCacheFlush{}, Engine3D::TiledCacheFlush::Flush);
	}
	else
		fence.m_internal.m_semaphoreValue = getDevice()->getSemaphoreValue(m_id);

	nvGpuChannelGetFence(&m_gpuChannel, &fence.m_internal.m_fence);
}

void Queue::submitCommands(DkCmdList list)
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
			case CtrlCmdHeader::ComputeBindShader ... CtrlCmdHeader::ComputeDispatchIndirect:
			{
				next = m_computeQueue->processCtrlCmd(cur);
				break;
			}
		}
	}
}

void Queue::flush()
{
	if (isInErrorState())
	{
		DK_ERROR(DkResult_Fail, "attempted to flush queue in error state");
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
			if (!checkError())
				DK_ERROR(DkResult_Fail, "gpu channel kickoff failed, but no error was reported");
			return;
		}
		// - Update device query data (is this really necessary?)
		m_cmdBufRing.updateProducer(getCmdOffset());
		addCmdMemory(m_cmdBufPerFenceSliceSize);
		postSubmitFlush();
		m_cmdBuf.flushGpfifoEntries();
	}
}

void Queue::waitIdle()
{
	if (isInErrorState())
		return;

	DkFence fence;
	signalFence(fence, true);
	flush();
	fence.wait();
}

DkQueue dkQueueCreate(DkQueueMaker const* maker)
{
	DK_ENTRYPOINT(maker->device);
	DK_DEBUG_BAD_INPUT(maker->commandMemorySize < DK_QUEUE_MIN_CMDMEM_SIZE);
	DK_DEBUG_SIZE_ALIGN(maker->commandMemorySize, DK_MEMBLOCK_ALIGNMENT);
	DK_DEBUG_BAD_INPUT(maker->flushThreshold < DK_MEMBLOCK_ALIGNMENT || maker->flushThreshold > maker->commandMemorySize);
	DK_DEBUG_SIZE_ALIGN(maker->perWarpScratchMemorySize, DK_PER_WARP_SCRATCH_MEM_ALIGNMENT);
	DK_DEBUG_BAD_INPUT(!maker->maxConcurrentComputeJobs && (maker->flags & DkQueueFlags_Compute));

	size_t extraSize = 0;
	if (maker->flags & DkQueueFlags_Compute)
		extraSize += sizeof(ComputeQueue);

	int32_t id = maker->device->reserveQueueId();
	if (id < 0)
		DK_ERROR(DkResult_BadState, "too many queues created");

	DkQueue obj = new(maker->device, extraSize) Queue(*maker, id);
	DkResult res = obj->initialize();
	if (res != DkResult_Success)
	{
		delete obj;
		DK_ERROR(res, "initialization failure");
		return nullptr;
	}
	return obj;
}

void dkQueueDestroy(DkQueue obj)
{
	DK_ENTRYPOINT(obj);
	delete obj;
}

bool dkQueueIsInErrorState(DkQueue obj)
{
	return obj->isInErrorState();
}

void dkQueueWaitFence(DkQueue obj, DkFence* fence)
{
	DK_ENTRYPOINT(obj);
	obj->waitFence(*fence);
}

void dkQueueSignalFence(DkQueue obj, DkFence* fence, bool flush)
{
	DK_ENTRYPOINT(obj);
	obj->signalFence(*fence, flush);
}

void dkQueueSubmitCommands(DkQueue obj, DkCmdList cmds)
{
	DK_ENTRYPOINT(obj);
	if (obj->isInErrorState())
		DK_ERROR(DkResult_Fail, "attempt to submit commands to a queue in error state");

	obj->submitCommands(cmds);
}

void dkQueueFlush(DkQueue obj)
{
	DK_ENTRYPOINT(obj);
	obj->flush();
}

void dkQueueWaitIdle(DkQueue obj)
{
	DK_ENTRYPOINT(obj);
	obj->waitIdle();
}

//-----------------------------------------------------------------------------
// Shims for conditionally linked features
//-----------------------------------------------------------------------------

DK_WEAK void Queue::setup3DEngine()
{
	DK_WARNING("graphics-capable DkQueue created, but Draw never called");
}

DK_WEAK void ComputeQueue::initialize()
{
	DK_WARNING("compute-capable DkQueue created, but Dispatch never called");
}

DK_WEAK CtrlCmdHeader const* ComputeQueue::processCtrlCmd(CtrlCmdHeader const* cmd)
{
	DK_WARNING("compute command called, but Dispatch never called");

	switch (cmd->type)
	{
		default: return nullptr; // shouldn't happen

		case CtrlCmdHeader::ComputeBindShader:
			return reinterpret_cast<CtrlCmdComputeShader const*>(cmd)+1;

		case CtrlCmdHeader::ComputeBindBuffer:
		case CtrlCmdHeader::ComputeDispatchIndirect:
			return reinterpret_cast<CtrlCmdComputeAddress const*>(cmd)+1;

		case CtrlCmdHeader::ComputeBindHandle:
			return cmd+1;

		case CtrlCmdHeader::ComputeDispatch:
			return reinterpret_cast<CtrlCmdComputeDispatch const*>(cmd)+1;
	}
}
