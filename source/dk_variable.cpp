#include "dk_variable.h"
#include "dk_memblock.h"
#include "cmdbuf_writer.h"

#include "engine_3d.h"
#include "engine_gpfifo.h"

using namespace maxwell;
using namespace dk::detail;

using E = Engine3D;
using S = EngineGpfifo::Semaphore;
using R = Engine3D::SetReportSemaphore;

namespace
{
	uint32_t getGpfifoWaitAction(DkVarCompareOp op)
	{
		switch (op)
		{
			default:
			case DkVarCompareOp_Equal:
				return S::Operation::Acquire | S::AcquireSwitch{};
			case DkVarCompareOp_Sequential:
				return S::Operation::AcqGeq  | S::AcquireSwitch{};
		}
	}

	uint32_t getGpfifoSignalAction(DkVarOp op)
	{
		uint32_t action = S::ReleaseWfiDisable{} | S::ReleaseSize::_4;
		if (op == DkVarOp_Set)
			action |= S::Operation::Release;
		else
			action |= S::Operation::Reduction | S::Format::Unsigned;

		switch (op)
		{
			default:
			case DkVarOp_Set:
				break;
			case DkVarOp_Add:
			case DkVarOp_Sub:
				action |= S::Reduction::Add;
				break;
			case DkVarOp_And:
				action |= S::Reduction::And;
				break;
			case DkVarOp_Or:
				action |= S::Reduction::Or;
				break;
			case DkVarOp_Xor:
				action |= S::Reduction::Xor;
				break;
		}

		return action;
	}

	uint32_t get3dSignalAction(DkVarOp op, DkPipelinePos pos)
	{
		uint32_t action = R::Operation::Release | R::FenceEnable{} | R::Format::Unsigned32 | R::StructureSize::OneWord;
		if (op != DkVarOp_Set)
			action |= R::ReductionEnable{};

		switch (op)
		{
			default:
			case DkVarOp_Set:
				break;
			case DkVarOp_Add:
			case DkVarOp_Sub:
				action |= R::ReductionOp::Add;
				break;
			case DkVarOp_And:
				action |= R::ReductionOp::And;
				break;
			case DkVarOp_Or:
				action |= R::ReductionOp::Or;
				break;
			case DkVarOp_Xor:
				action |= R::ReductionOp::Xor;
				break;
		}

		switch (pos)
		{
			default:
			case DkPipelinePos_Rasterizer:
				action |= R::Unit::Rast;
				break;
			case DkPipelinePos_Bottom:
				action |= R::Unit::Crop;
				break;
		}

		return action;
	}
}

void dkVariableInitialize(DkVariable* obj, DkMemBlock mem, uint32_t offset)
{
	DK_ENTRYPOINT(mem);
	DK_DEBUG_NON_NULL(obj);
	DK_DEBUG_DATA_ALIGN(offset, 4);
	DK_DEBUG_BAD_INPUT(offset >= mem->getSize(), "offset out of bounds");
	DK_DEBUG_BAD_STATE(!mem->isCpuUncached(), "memblock must be DkMemBlockFlags_CpuUncached");
	DK_DEBUG_BAD_STATE(!mem->isGpuUncached(), "memblock must be DkMemBlockFlags_GpuUncached");

	obj->m_cpuAddr = (uint32_t*)((uint8_t*)mem->getCpuAddr() + offset);
	obj->m_gpuAddr = mem->getGpuAddrPitch() + offset;
}

uint32_t dkVariableRead(DkVariable const* obj)
{
	return __atomic_load_n(obj->m_cpuAddr, __ATOMIC_SEQ_CST);
}

void dkVariableSignal(DkVariable const* obj, DkVarOp op, uint32_t value)
{
	switch (op)
	{
		default:
		case DkVarOp_Set:
			__atomic_store_n(obj->m_cpuAddr, value, __ATOMIC_SEQ_CST);
			break;
		case DkVarOp_Sub:
			value = -value;
			// fallthrough
		case DkVarOp_Add:
			__atomic_add_fetch(obj->m_cpuAddr, value, __ATOMIC_SEQ_CST);
			break;
		case DkVarOp_And:
			__atomic_and_fetch(obj->m_cpuAddr, value, __ATOMIC_SEQ_CST);
			break;
		case DkVarOp_Or:
			__atomic_or_fetch(obj->m_cpuAddr, value, __ATOMIC_SEQ_CST);
			break;
		case DkVarOp_Xor:
			__atomic_xor_fetch(obj->m_cpuAddr, value, __ATOMIC_SEQ_CST);
			break;
	}
	__asm__ __volatile__("dmb\tst" ::: "memory"); // "DMB operation that waits only for stores to complete." i.e. so that it is visible to the GPU
}

void dkCmdBufWaitVariable(DkCmdBuf obj, DkVariable const* var, DkVarCompareOp op, uint32_t value)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_NON_NULL(var);
	DK_DEBUG_NON_NULL(var->m_cpuAddr);

	CmdBufWriter w{obj};
	w.reserve(5);

	w << Cmd(Gpfifo, SemaphoreOffset{}, Iova(var->m_gpuAddr), value, getGpfifoWaitAction(op));
}

void dkCmdBufSignalVariable(DkCmdBuf obj, DkVariable const* var, DkVarOp op, uint32_t value, DkPipelinePos pos)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_NON_NULL(var);
	DK_DEBUG_NON_NULL(var->m_cpuAddr);

	CmdBufWriter w{obj};

	if (op == DkVarOp_Sub)
		value = -value;

	if (pos == DkPipelinePos_Top)
	{
		w.reserve(5);
		w << Cmd(Gpfifo, SemaphoreOffset{}, Iova(var->m_gpuAddr), value, getGpfifoSignalAction(op));
	}
	else
	{
		w.reserve(7);
		w << CmdInline(3D, UnknownFlush{}, 0);
		w << Cmd(3D, SetReportSemaphoreOffset{}, Iova(var->m_gpuAddr), value, get3dSignalAction(op, pos));
		w << CmdInline(3D, TiledCacheFlush{}, Engine3D::TiledCacheFlush::Flush);
	}
}

void dkCmdBufReportCounter(DkCmdBuf obj, DkCounter type, DkGpuAddr addr)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_BAD_INPUT(addr == DK_GPU_ADDR_INVALID);
	DK_DEBUG_DATA_ALIGN(addr, 16);
	CmdBufWriter w{obj};
	w.reserve((type == DkCounter_ZcullStats ? 4 : 1) * 5 + 1);

	switch (type) {
		default:
			static const uint32_t dkTypeToMaxwell[] =
			{
				[DkCounter_TimestampPipelineTop]               =                     R::Counter::Payload,
				[DkCounter_Timestamp]                          = R::Unit::Crop     | R::Counter::Payload,
				[DkCounter_SamplesPassed]                      = R::Unit::Crop     | R::Counter::SamplesPassed,
				[DkCounter_ZcullStats]                         = R::Unit::Crop     | R::Counter::Payload,
				[DkCounter_InputVertices]                      = R::Unit::VFetch   | R::Counter::InputVertices,
				[DkCounter_InputPrimitives]                    = R::Unit::VFetch   | R::Counter::InputPrimitives,
				[DkCounter_VertexShaderInvocations]            = R::Unit::VP       | R::Counter::VertexShaderInvocations,
				[DkCounter_TessControlShaderInvocations]       = R::Unit::TessCtrl | R::Counter::TessControlShaderInvocations,
				[DkCounter_TessEvaluationShaderInvocations]    = R::Unit::TessEval | R::Counter::TessEvaluationShaderInvocations,
				[DkCounter_GeometryShaderInvocations]          = R::Unit::GP       | R::Counter::GeometryShaderInvocations,
				[DkCounter_FragmentShaderInvocations]          = R::Unit::Prop     | R::Counter::FragmentShaderInvocations,
				[DkCounter_TessEvaluationShaderPrimitives]     = R::Unit::TessEval | R::Counter::TessEvaluationShaderPrimitives,
				[DkCounter_GeometryShaderPrimitives]           = R::Unit::GP       | R::Counter::GeometryShaderPrimitives,
				[DkCounter_ClipperInputPrimitives]             = R::Unit::Rast     | R::Counter::ClipperInputPrimitives,
				[DkCounter_ClipperOutputPrimitives]            = R::Unit::Rast     | R::Counter::ClipperOutputPrimitives,
				[DkCounter_PrimitivesGenerated]                = R::Unit::StrmOut  | R::Counter::PrimitivesGenerated,
				[DkCounter_TransformFeedbackPrimitivesWritten] = R::Unit::StrmOut  | R::Counter::TransformFeedbackPrimitivesWritten,
			};

			w << Cmd(3D, SetReportSemaphoreOffset{}, Iova(addr), 0,
				R::Operation::Counter |
				dkTypeToMaxwell[type] |
				R::StructureSize::FourWords
			);
			break;
		case DkCounter_ZcullStats:
			w << Cmd(3D, SetReportSemaphoreOffset{}, Iova(addr), 0,
				R::Operation::Counter |
				R::Unit::ZCull |
				R::Counter::ZcullStats0 |
				R::StructureSize::OneWord
			);
			w << Cmd(3D, SetReportSemaphoreOffset{}, Iova(addr + 4), 0,
				R::Operation::Counter |
				R::Unit::ZCull |
				R::Counter::ZcullStats1 |
				R::StructureSize::OneWord
			);
			w << Cmd(3D, SetReportSemaphoreOffset{}, Iova(addr + 8), 0,
				R::Operation::Counter |
				R::Unit::ZCull |
				R::Counter::ZcullStats2 |
				R::StructureSize::OneWord
			);
			w << Cmd(3D, SetReportSemaphoreOffset{}, Iova(addr + 12), 0,
				R::Operation::Counter |
				R::Unit::ZCull |
				R::Counter::ZcullStats3 |
				R::StructureSize::OneWord
			);
			break;
		case DkCounter_TimestampPipelineTop:
			using S = EngineGpfifo::Semaphore;
			w << Cmd(Gpfifo, SemaphoreOffset{}, Iova(addr), 0,
				S::ReleaseWfiDisable{} |
				S::Operation::Release |
				S::ReleaseSize::_16
			);
			break;
	}

	w << CmdInline(3D, TiledCacheFlush{}, 0);
}

void dkCmdBufReportValue(DkCmdBuf obj, uint32_t value, DkGpuAddr addr)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_BAD_INPUT(addr == DK_GPU_ADDR_INVALID);
	DK_DEBUG_DATA_ALIGN(addr, 16);
	CmdBufWriter w{obj};
	w.reserve(7);

	w << CmdInline(3D, UnknownFlush{}, 0);
	w << Cmd(3D, SetReportSemaphoreOffset{}, Iova(addr), value,
		R::Operation::Counter |
		R::Unit::Crop |
		R::StructureSize::FourWords
	);
	w << CmdInline(3D, TiledCacheFlush{}, 0);
}

void dkCmdBufResetCounter(DkCmdBuf obj, DkCounter type)
{
	DK_ENTRYPOINT(obj);
	CmdBufWriter w{obj};
	w.reserve(2);

	static const uint8_t dkTypeToMaxwell[] =
	{
		[DkCounter_TimestampPipelineTop]               = E::ResetCounter::SamplesPassed,
		[DkCounter_Timestamp]                          = E::ResetCounter::SamplesPassed,
		[DkCounter_SamplesPassed]                      = E::ResetCounter::SamplesPassed,
		[DkCounter_ZcullStats]                         = E::ResetCounter::ZcullStats,
		[DkCounter_InputVertices]                      = E::ResetCounter::InputVertices,
		[DkCounter_InputPrimitives]                    = E::ResetCounter::InputPrimitives,
		[DkCounter_VertexShaderInvocations]            = E::ResetCounter::VertexShaderInvocations,
		[DkCounter_TessControlShaderInvocations]       = E::ResetCounter::TessControlShaderInvocations,
		[DkCounter_TessEvaluationShaderInvocations]    = E::ResetCounter::TessEvaluationShaderInvocations,
		[DkCounter_GeometryShaderInvocations]          = E::ResetCounter::GeometryShaderInvocations,
		[DkCounter_FragmentShaderInvocations]          = E::ResetCounter::FragmentShaderInvocations,
		[DkCounter_TessEvaluationShaderPrimitives]     = E::ResetCounter::TessEvaluationShaderPrimitives,
		[DkCounter_GeometryShaderPrimitives]           = E::ResetCounter::GeometryShaderPrimitives,
		[DkCounter_ClipperInputPrimitives]             = E::ResetCounter::ClipperInputPrimitives,
		[DkCounter_ClipperOutputPrimitives]            = E::ResetCounter::ClipperOutputPrimitives,
		[DkCounter_PrimitivesGenerated]                = E::ResetCounter::PrimitivesGenerated,
		[DkCounter_TransformFeedbackPrimitivesWritten] = E::ResetCounter::TransformFeedbackPrimitivesWritten,
	};

	w << Cmd(3D, ResetCounter{}, dkTypeToMaxwell[type]);
}
