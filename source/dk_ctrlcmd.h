#pragma once
#include "dk_private.h"

namespace dk::detail
{

struct CtrlCmdHeader
{
	enum // Types
	{
		Return,      // no arguments
		Jump,        // see CtrlCmdJumpCall
		Call,        // see CtrlCmdJumpCall
		GpfifoList,  // followed by arg*CtrlCmdGpfifoEntry
		WaitFence,   // see CtrlCmdFence
		SignalFence, // see CtrlCmdFence, arg = flush flag

		ComputeBindShader,        // see CtrlCmdComputeShader, arg = codeOffset
		ComputeBindBuffer,        // see CtrlCmdComputeAddress, extra: {0..15}->uniform {16..31}->storage, arg = size
		ComputeBindHandle,        // extra: {0..31}->tex {32..39}->img, arg = handle
		ComputeDispatch,          // see CtrlCmdComputeDispatch, arg = numGroupsX
		ComputeDispatchIndirect,  // see CtrlCmdComputeAddress
	};

	uint64_t type : 8;
	uint64_t extra : 24;
	uint64_t arg : 32;
};

struct CtrlCmdJumpCall : public CtrlCmdHeader
{
	CtrlCmdHeader const* ptr;
};

struct CtrlCmdGpfifoEntry
{
	enum // Flags
	{
		AutoKick = BIT(0),
		NoPrefetch = BIT(1),
	};
	DkGpuAddr iova;
	uint32_t numCmds;
	uint32_t flags;
};

struct CtrlCmdFence : public CtrlCmdHeader
{
	DkFence* fence;
};

struct CtrlCmdComputeShader : public CtrlCmdHeader
{
	uint32_t dataOffset;
	uint32_t dataSize;
	uint32_t numRegisters;
	uint32_t numBarriers;
	uint32_t sharedMemSize;
	uint32_t localLoMemSize;
	uint32_t localHiMemSize;
	uint32_t crsSize;
	uint32_t perWarpScratchSize;
	uint32_t blockDims[3];
};

struct CtrlCmdComputeAddress : public CtrlCmdHeader
{
	DkGpuAddr addr;
};

struct CtrlCmdComputeDispatch : public CtrlCmdHeader
{
	uint32_t numGroupsY;
	uint32_t numGroupsZ;
};

}
