#include "dk_queue.h"
#include "dk_shader.h"
#include "cmdbuf_writer.h"

using namespace dk::detail;

namespace
{
#ifdef DEBUG

	constexpr bool checkInRange(uint32_t base, uint32_t size, uint32_t max)
	{
		uint32_t end = base + size;
		// Avoid ( ͡° ͜ʖ ͡°) overflows and underflows
		return end >= base && end <= max;
	}

	DkResult checkBuffers(DkBufExtents const buffers[], uint32_t numBuffers, uint32_t bufAlign, uint32_t maxSize)
	{
		for (uint32_t i = 0; i < numBuffers; i ++)
		{
			DkBufExtents const& buf = buffers[i];
			if (buf.addr & (bufAlign - 1))
				return DkResult_MisalignedData;
			if (buf.size > maxSize)
				return DkResult_BadInput;
		}
		return DkResult_Success;
	}

#endif
}

void dkCmdBufBindShaders(DkCmdBuf obj, uint32_t stageMask, DkShader const* const shaders[], uint32_t numShaders)
{
	CmdBufWriter w{obj};

#ifdef DEBUG
	if (!shaders && numShaders)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	if (stageMask & DkStageFlag_Compute)
	{
#ifdef DEBUG
		if (stageMask &~ DkStageFlag_Compute)
			obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
		if (numShaders != 1)
			obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

		DkShader const* shader = shaders[0];
#ifdef DEBUG
		if (shader->m_stage != DkStage_Compute)
			obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

		auto* cmd = w.addCtrl<CtrlCmdComputeShader>();
		if (cmd)
		{
			cmd->type = CtrlCmdHeader::ComputeBindShader;
			cmd->arg = shader->m_hdr.entrypoint;
			cmd->dataOffset = shader->m_hdr.constbuf1_off;
			cmd->dataSize = shader->m_hdr.constbuf1_sz;
			cmd->numRegisters = shader->m_hdr.num_gprs;
			cmd->numBarriers = shader->m_hdr.comp.num_barriers;
			cmd->sharedMemSize = shader->m_hdr.comp.shared_mem_sz;
			cmd->localLoMemSize = shader->m_hdr.comp.local_pos_mem_sz;
			cmd->localHiMemSize = shader->m_hdr.comp.local_neg_mem_sz;
			cmd->crsSize = shader->m_hdr.comp.crs_sz;
			cmd->perWarpScratchSize = shader->m_hdr.per_warp_scratch_sz;
			cmd->blockDims[0] = shader->m_hdr.comp.block_dims[0];
			cmd->blockDims[1] = shader->m_hdr.comp.block_dims[1];
			cmd->blockDims[2] = shader->m_hdr.comp.block_dims[2];
		}
		return;
	}

	stageMask &= DkStageFlag_GraphicsMask;
	// TODO: bind shit
}

void dkCmdBufBindUniformBuffers(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkBufExtents const buffers[], uint32_t numBuffers)
{
	CmdBufWriter w{obj};

#ifdef DEBUG
	if (stage < DkStage_Vertex || stage > DkStage_Compute)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!buffers && numBuffers)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!checkInRange(firstId, numBuffers, DK_NUM_UNIFORM_BUFS))
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);

	DkResult chk = checkBuffers(buffers, numBuffers, DK_UNIFORM_BUF_ALIGNMENT, DK_UNIFORM_BUF_MAX_SIZE);
	if (chk != DkResult_Success)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, chk);
#endif

	if (stage == DkStage_Compute)
	{
		for (uint32_t i = 0; i < numBuffers; i ++)
		{
			auto* cmd = w.addCtrl<CtrlCmdComputeAddress>();
			if (!cmd) return;
			cmd->type = CtrlCmdHeader::ComputeBindBuffer;
			cmd->extra = firstId + i;
			cmd->arg = buffers[i].size;
			cmd->addr = buffers[i].addr;
		}
		return;
	}

	// TODO: bind shit
}

void dkCmdBufBindStorageBuffers(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkBufExtents const buffers[], uint32_t numBuffers)
{
	CmdBufWriter w{obj};

#ifdef DEBUG
	if (stage < DkStage_Vertex || stage > DkStage_Compute)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!buffers && numBuffers)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!checkInRange(firstId, numBuffers, DK_NUM_STORAGE_BUFS))
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	if (stage == DkStage_Compute)
	{
		for (uint32_t i = 0; i < numBuffers; i ++)
		{
			auto* cmd = w.addCtrl<CtrlCmdComputeAddress>();
			if (!cmd) return;
			cmd->type = CtrlCmdHeader::ComputeBindBuffer;
			cmd->extra = DK_NUM_UNIFORM_BUFS + firstId + i;
			cmd->arg = buffers[i].size;
			cmd->addr = buffers[i].addr;
		}
		return;
	}

	// TODO: bind shit
}

void dkCmdBufBindTextures(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkResHandle const handles[], uint32_t numHandles)
{
	CmdBufWriter w{obj};

#ifdef DEBUG
	if (stage < DkStage_Vertex || stage > DkStage_Compute)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!handles && numHandles)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!checkInRange(firstId, numHandles, DK_NUM_TEXTURE_BINDINGS))
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	if (stage == DkStage_Compute)
	{
		for (uint32_t i = 0; i < numHandles; i ++)
		{
			auto* cmd = w.addCtrl<CtrlCmdHeader>();
			if (!cmd) return;
			cmd->type = CtrlCmdHeader::ComputeBindHandle;
			cmd->extra = firstId + i;
			cmd->arg = handles[i];
		}
		return;
	}

	// TODO: bind shit
}

void dkCmdBufBindImages(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkResHandle const handles[], uint32_t numHandles)
{
	CmdBufWriter w{obj};

#ifdef DEBUG
	if (stage < DkStage_Vertex || stage > DkStage_Compute)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!handles && numHandles)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!checkInRange(firstId, numHandles, DK_NUM_IMAGE_BINDINGS))
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	if (stage == DkStage_Compute)
	{
		for (uint32_t i = 0; i < numHandles; i ++)
		{
			auto* cmd = w.addCtrl<CtrlCmdHeader>();
			if (!cmd) return;
			cmd->type = CtrlCmdHeader::ComputeBindHandle;
			cmd->extra = DK_NUM_TEXTURE_BINDINGS + firstId + i;
			cmd->arg = handles[i];
		}
		return;
	}

	// TODO: bind shit
}
