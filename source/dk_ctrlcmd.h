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
	};

	uint64_t type : 8;
	uint64_t unk : 24;
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

}
