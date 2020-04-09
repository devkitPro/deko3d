#include "queue_compute.h"

#include "cmdbuf_writer.h"

using namespace dk::detail;

inline void ComputeQueue::bindUniformBuffer(uint32_t id, DkGpuAddr addr, uint32_t size)
{
	job.cbuf.uniformBufs[id] = addr;
	if (id < 6)
		bindConstbuf(id+2, addr, size);
}

inline void ComputeQueue::bindStorageBuffer(uint32_t id, DkGpuAddr addr, uint32_t size)
{
	job.cbuf.data.storageBufs[id].address = addr;
	job.cbuf.data.storageBufs[id].size = size;
}

inline void ComputeQueue::bindTexture(uint32_t id, DkResHandle handle)
{
	job.cbuf.data.textures[id] = handle;
}

inline void ComputeQueue::bindImage(uint32_t id, DkResHandle handle)
{
	job.cbuf.data.images[id] = handle;
}

void ComputeQueue::initialize()
{
	initQmd();
	initComputeEngine();
}

CtrlCmdHeader const* ComputeQueue::processCtrlCmd(CtrlCmdHeader const* cmd)
{
	switch (cmd->type)
	{
		default: return nullptr; // shouldn't happen

		case CtrlCmdHeader::ComputeBindShader:
		{
			auto* args = reinterpret_cast<CtrlCmdComputeShader const*>(cmd);
			bindShader(args);
			return args+1;
		}

		case CtrlCmdHeader::ComputeBindBuffer:
		{
			auto* args = reinterpret_cast<CtrlCmdComputeAddress const*>(cmd);
			if (args->extra < DK_NUM_UNIFORM_BUFS)
				bindUniformBuffer(args->extra, args->addr, args->arg);
			else
				bindStorageBuffer(args->extra-DK_NUM_UNIFORM_BUFS, args->addr, args->arg);
			return args+1;
		}

		case CtrlCmdHeader::ComputeBindHandle:
		{
			if (cmd->extra < DK_NUM_TEXTURE_BINDINGS)
				bindTexture(cmd->extra, cmd->arg);
			else
				bindImage(cmd->extra-DK_NUM_TEXTURE_BINDINGS, cmd->arg);
			return cmd+1;
		}

		case CtrlCmdHeader::ComputeDispatch:
		{
			auto* args = reinterpret_cast<CtrlCmdComputeDispatch const*>(cmd);
			dispatch(args->arg, args->numGroupsY, args->numGroupsZ, DK_GPU_ADDR_INVALID);
			return args+1;
		}

		case CtrlCmdHeader::ComputeDispatchIndirect:
		{
			auto* args = reinterpret_cast<CtrlCmdComputeAddress const*>(cmd);
			dispatch(0, 0, 0, args->addr);
			return args+1;
		}
	}
}

void dkCmdBufDispatchCompute(DkCmdBuf obj, uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_NON_ZERO(numGroupsX);
	DK_DEBUG_NON_ZERO(numGroupsY);
	DK_DEBUG_NON_ZERO(numGroupsZ);

	CmdBufWriter w{obj};
	auto* cmd = w.addCtrl<CtrlCmdComputeDispatch>();
	if (cmd)
	{
		cmd->type = CtrlCmdHeader::ComputeDispatch;
		cmd->arg = numGroupsX;
		cmd->numGroupsY = numGroupsY;
		cmd->numGroupsZ = numGroupsZ;
	}
}

void dkCmdBufDispatchComputeIndirect(DkCmdBuf obj, DkGpuAddr indirect)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_BAD_INPUT(indirect == DK_GPU_ADDR_INVALID);
	DK_DEBUG_DATA_ALIGN(indirect, 4);

	CmdBufWriter w{obj};
	auto* cmd = w.addCtrl<CtrlCmdComputeAddress>();
	if (cmd)
	{
		cmd->type = CtrlCmdHeader::ComputeDispatchIndirect;
		cmd->addr = indirect;
	}
}
