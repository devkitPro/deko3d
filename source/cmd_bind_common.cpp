#include "dk_queue.h"
#include "dk_shader.h"
#include "cmdbuf_writer.h"

#include "mme_macros.h"
#include "engine_3d.h"

#include "driver_constbuf.h"

using namespace dk::detail;
using namespace maxwell;

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
	DK_ENTRYPOINT(obj);
	DK_DEBUG_NON_NULL_ARRAY(shaders, numShaders);
	CmdBufWriter w{obj};

	if (stageMask & DkStageFlag_Compute)
	{
		DK_DEBUG_BAD_FLAGS(stageMask &~ DkStageFlag_Compute, "must only bind DkStageFlag_Compute by itself");
		DK_DEBUG_BAD_INPUT(numShaders != 1, "must only bind a single compute shader");

		DkShader const* shader = shaders[0];
		DK_DEBUG_BAD_INPUT(shader->m_stage != DkStage_Compute, "specified compute shader not a compute shader");

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
	for (uint32_t i = 0; i < numShaders; i ++)
	{
		DkShader const* shader = shaders[i];
		uint32_t curMask = 1U << shader->m_stage;
		if (!(stageMask & curMask))
			continue;

		uint32_t sizeReq = 7;
		auto& hdr = shader->m_hdr;
		switch (shader->m_stage)
		{
			default:
				break;
			case DkStage_TessEval:
				sizeReq += 1;
				break;
			case DkStage_Geometry:
				sizeReq += 1 + (hdr.geom.has_table_490 ? 9 : 0);
				break;
			case DkStage_Vertex:
				sizeReq += hdr.vert.alt_num_gprs ? 5 : 1;
				break;
			case DkStage_Fragment:
				sizeReq += 9 + (hdr.frag.has_table_3d1 ? 5 : 0);
				break;
		}

		w.reserve(sizeReq);
		if (shader->m_stage == DkStage_Vertex)
		{
			if (hdr.vert.alt_num_gprs)
				w << Macro(BindProgram, 0, shader->m_id, hdr.vert.alt_entrypoint, hdr.vert.alt_num_gprs);
			else
				w << CmdInline(3D, SetProgram::Config{0}, 0); // disable VertexA
		}

		w << Macro(BindProgram, 1+unsigned(shader->m_stage),
			shader->m_id,
			hdr.entrypoint,
			hdr.num_gprs,
			(hdr.constbuf1_sz + 0xFF) &~ 0xFF,
			shader->m_cbuf1IovaShift8
		);

		switch (shader->m_stage)
		{
			default:
				break;
			case DkStage_TessEval:
				w << MakeInlineCmd(Subchannel3D, 0x0c8, hdr.tess_eval.param_c8);
				break;
			case DkStage_Geometry:
				w << MakeInlineCmd(Subchannel3D, 0x47c, hdr.geom.flag_47c);
				if (hdr.geom.has_table_490)
					w << MakeIncreasingCmd(Subchannel3D, 0x490,
						hdr.geom.table_490[0], hdr.geom.table_490[1], hdr.geom.table_490[2], hdr.geom.table_490[3],
						hdr.geom.table_490[4], hdr.geom.table_490[5], hdr.geom.table_490[6], hdr.geom.table_490[7]);
				break;
			case DkStage_Fragment:
				w << MakeInlineCmd(Subchannel3D, 0x3d0, hdr.frag.has_table_3d1);
				if (hdr.frag.has_table_3d1)
					w << MakeIncreasingCmd(Subchannel3D, 0x3d0,
						hdr.frag.table_3d1[0], hdr.frag.table_3d1[1], hdr.frag.table_3d1[2], hdr.frag.table_3d1[3]);
				w << MakeInlineCmd(Subchannel3D, 0x084, hdr.frag.early_fragment_tests);
				w << MakeInlineCmd(Subchannel3D, 0x3c7, hdr.frag.post_depth_coverage);
				w << MakeIncreasingCmd(Subchannel3D, 0x0d8, hdr.frag.param_d8, 0x20);
				w << MakeInlineCmd(Subchannel3D, 0x489, hdr.frag.param_489);
				w << MakeInlineCmd(Subchannel3D, 0x65b, hdr.frag.param_65b);
				w << MakeInlineCmd(Subchannel3D, 0x1d5, hdr.frag.persample_invocation ? 0x30 : 0x01);
				break;
		}

		w.flush(true);
		stageMask &= ~curMask;
	}

	// Unbind stages not found in the remaining bits of the stage mask
	// (except for the Vertex stage, which is always enabled)
	stageMask &= ~DkStageFlag_Vertex;
	if (stageMask)
	{
		w.reserve(__builtin_popcount(stageMask) + !!(stageMask & DkStageFlag_Geometry));
		for (unsigned i = DkStage_TessCtrl; i < DkStage_MaxGraphics; i ++)
			if (stageMask & (1U<<i))
				w << CmdInline(3D, SetProgram::Config{i+1}, Engine3D::SetProgram::Config::StageId{i+1});
		if (stageMask & DkStageFlag_Geometry)
			w << MakeInlineCmd(Subchannel3D, 0x47c, 0);
	}
}

void dkCmdBufBindUniformBuffers(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkBufExtents const buffers[], uint32_t numBuffers)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_BAD_INPUT(stage < DkStage_Vertex || stage > DkStage_Compute, "invalid stage");
	DK_DEBUG_NON_NULL_ARRAY(buffers, numBuffers);
	DK_DEBUG_BAD_INPUT(!checkInRange(firstId, numBuffers, DK_NUM_UNIFORM_BUFS));
	DK_DEBUG_CHECK(checkBuffers(buffers, numBuffers, DK_UNIFORM_BUF_ALIGNMENT, DK_UNIFORM_BUF_MAX_SIZE));
	CmdBufWriter w{obj};

	if (stage == DkStage_Compute)
	{
		for (uint32_t i = 0; i < numBuffers; i ++)
		{
			auto* cmd = w.addCtrl<CtrlCmdComputeAddress>();
			if (!cmd) return;
			cmd->type = CtrlCmdHeader::ComputeBindBuffer;
			cmd->extra = firstId + i;
			cmd->arg = (buffers[i].size + 0xFF) &~ 0xFF;
			cmd->addr = buffers[i].addr;
		}
		return;
	}

	w.reserve(5*numBuffers);
	for (uint32_t i = 0; i < numBuffers; i ++)
	{
		DkBufExtents const& buf = buffers[i];
		if (buf.size)
		{
			w << Cmd(3D, ConstbufSelectorSize{}, (buf.size + 0xFF) &~ 0xFF, Iova(buf.addr));
			w << CmdInline(3D, Bind::Constbuf{stage}, Engine3D::Bind::Constbuf::Valid{} | Engine3D::Bind::Constbuf::Index{2+firstId+i});
		}
		else
			w << CmdInline(3D, Bind::Constbuf{stage}, Engine3D::Bind::Constbuf::Index{2+firstId+i});
	}
}

void dkCmdBufBindStorageBuffers(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkBufExtents const buffers[], uint32_t numBuffers)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_BAD_INPUT(stage < DkStage_Vertex || stage > DkStage_Compute, "invalid stage");
	DK_DEBUG_NON_NULL_ARRAY(buffers, numBuffers);
	DK_DEBUG_BAD_INPUT(!checkInRange(firstId, numBuffers, DK_NUM_STORAGE_BUFS));
	CmdBufWriter w{obj};

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

	static_assert(sizeof(DkBufExtents)        == sizeof(BufDescriptor),           "Bad definition for DkBufExtents");
	static_assert(offsetof(DkBufExtents,addr) == offsetof(BufDescriptor,address), "Bad definition for DkBufExtents");
	static_assert(offsetof(DkBufExtents,size) == offsetof(BufDescriptor,size),    "Bad definition for DkBufExtents");

	w.reserve(2 + numBuffers*4);
	w << MacroInline(SelectDriverConstbuf, offsetof(GraphicsDriverCbuf, data[stage].storageBufs[firstId])/4);
	w << CmdList<1>{ MakeCmdHeader(NonIncreasing, numBuffers*4, Subchannel3D, Engine3D::LoadConstbufData{}) };
	w.addRawData(buffers, numBuffers*sizeof(DkBufExtents));
}

void dkCmdBufBindTextures(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkResHandle const handles[], uint32_t numHandles)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_BAD_INPUT(stage < DkStage_Vertex || stage > DkStage_Compute, "invalid stage");
	DK_DEBUG_NON_NULL_ARRAY(handles, numHandles);
	DK_DEBUG_BAD_INPUT(!checkInRange(firstId, numHandles, DK_NUM_TEXTURE_BINDINGS));
	CmdBufWriter w{obj};

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

	w.reserve(3 + numHandles);
	w << MacroInline(SelectDriverConstbuf, offsetof(GraphicsDriverCbuf, data[stage].textures[firstId])/4);
	w << CmdInline(3D, PipeNop{}, 0);
	w << CmdList<1>{ MakeCmdHeader(NonIncreasing, numHandles, Subchannel3D, Engine3D::LoadConstbufData{}) };
	w.addRawData(handles, numHandles*sizeof(DkResHandle));
}

void dkCmdBufBindImages(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkResHandle const handles[], uint32_t numHandles)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_BAD_INPUT(stage < DkStage_Vertex || stage > DkStage_Compute, "invalid stage");
	DK_DEBUG_NON_NULL_ARRAY(!handles, numHandles);
	DK_DEBUG_BAD_INPUT(!checkInRange(firstId, numHandles, DK_NUM_IMAGE_BINDINGS));
	CmdBufWriter w{obj};

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

	w.reserve(3 + numHandles);
	w << MacroInline(SelectDriverConstbuf, offsetof(GraphicsDriverCbuf, data[stage].images[firstId])/4);
	w << CmdInline(3D, PipeNop{}, 0);
	w << CmdList<1>{ MakeCmdHeader(NonIncreasing, numHandles, Subchannel3D, Engine3D::LoadConstbufData{}) };
	w.addRawData(handles, numHandles*sizeof(DkResHandle));
}
