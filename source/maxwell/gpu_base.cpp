#include "../dk_queue.h"
#include "../cmdbuf_writer.h"

#include "mme_macros.h"
#include "engine_3d.h"
#include "engine_compute.h"
#include "engine_inline.h"
#include "engine_2d.h"
#include "engine_dma.h"
#include "engine_gpfifo.h"

using namespace maxwell;
using namespace dk::detail;

void tag_DkQueue::setupEngines()
{
	CmdBufWriter w{&m_cmdBuf};
	w.reserveAdd(
		BindEngine(3D),
		BindEngine(Compute),
		BindEngine(DMA),
		BindEngine(Inline),
		BindEngine(2D),
		CmdsFromArray(MmeMacro_SetupCmds)
	);
}

void tag_DkQueue::postSubmitFlush()
{
	// Invalidate texture cache, texture/sampler descriptor cache, shader code cache, and L2 cache
	// This is done to ensure the visibility of CPU updates between calls to dkQueueFlush()
	dkCmdBufBarrier(&m_cmdBuf,
		DkBarrier_None,
		DkInvalidateFlags_Image |
		DkInvalidateFlags_Code  |
		DkInvalidateFlags_Pool  |
		DkInvalidateFlags_L2Cache);

	CmdBufWriter w{&m_cmdBuf};

	// TODO: Reserve space for ZBC commands
	w.split(CtrlCmdGpfifoEntry::NoPrefetch);

	// Append a dummy single-word cmdlist, used as a cmdlist processing barrier
	w.reserveAdd(CmdList<1>{0});
	w.split(CtrlCmdGpfifoEntry::AutoKick | CtrlCmdGpfifoEntry::NoPrefetch);
}

void dkCmdBufBarrier(DkCmdBuf obj, DkBarrier mode, uint32_t invalidateFlags)
{
	CmdBufWriter w{obj};
	w.reserve(12);

	bool needsWfi = false;
	switch (mode)
	{
		default:
		case DkBarrier_None:
			break;
		case DkBarrier_Tiles:
			needsWfi = true;
			w << CmdInline(3D, TiledCacheBarrier{}, 0);
			break;
		case DkBarrier_Fragments:
			needsWfi = true;
			w << CmdInline(3D, FragmentBarrier{}, 0);
			break;
		case DkBarrier_Primitives:
			w << CmdInline(3D, WaitForIdle{}, 0);
			break;
		case DkBarrier_Full:
			w << CmdInline(Gpfifo, SetReference{}, 0);
			w.split(0);
			w << CmdInline(3D, NoOperation{}, 0);
			break;
	}

	if (invalidateFlags & DkInvalidateFlags_Image)
	{
		if (needsWfi)
			w << CmdInline(3D, InvalidateTextureDataCache{}, 0);
		else
			w << CmdInline(3D, InvalidateTextureDataCacheNoWfi{}, 0);
	}

	if (invalidateFlags & DkInvalidateFlags_Code)
	{
		using ISC = Engine3D::InvalidateShaderCaches;
		w << CmdInline(3D, InvalidateShaderCaches{},
			ISC::Instruction{} | ISC::GlobalData{} | ISC::Constant{}
		);
	}

	if (invalidateFlags & DkInvalidateFlags_Pool)
	{
		w << CmdInline(3D, InvalidateTextureHeaderCacheNoWfi{}, 0);
		w << CmdInline(3D, InvalidateSamplerCacheNoWfi{}, 0);
	}

	if (invalidateFlags & DkInvalidateFlags_Zcull)
		w << CmdInline(3D, InvalidateZcullNoWfi{}, 0);

	if (invalidateFlags & DkInvalidateFlags_L2Cache)
	{
		using Op = EngineGpfifo::MemOpB::Operation;
		w << Cmd(Gpfifo, MemOpB{}, Op::L2FlushDirty);
		w << Cmd(Gpfifo, MemOpB{}, Op::L2SysmemInvalidate);
	}

	// For DkBarrier_Full, we sign off a gpfifo entry with the NoPrefetch bit set.
	// This means we tell the command processor to avoid prefetching any further command data.
	// Note that indirect draw/dispatch param buffers are stashed away in the command list as gpfifo entries,
	// so this will effectively ensure that *absolutely everything* will finish before those params get fetched.
	if (mode == DkBarrier_Full)
		w.split(CtrlCmdGpfifoEntry::AutoKick | CtrlCmdGpfifoEntry::NoPrefetch);
}

void dkCmdBufPushConstants(DkCmdBuf obj, DkGpuAddr uboAddr, uint32_t uboSize, uint32_t offset, uint32_t size, const void* data)
{
#ifdef DEBUG
	if (uboAddr & (DK_UNIFORM_BUF_ALIGNMENT-1))
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedData);
	if (uboSize > DK_UNIFORM_BUF_MAX_SIZE)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (offset & 3)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedData);
	if (size & 3)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedSize);
	if ((offset >= uboSize) || (size > uboSize))
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if ((offset + size) > uboSize)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
	if (!data)
		obj->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
#endif

	uint32_t sizeWords = size/4;
	CmdBufWriter w{obj};
	w.reserve(6 + sizeWords);

	w << Cmd(3D, ConstbufSelectorSize{},
		(uboSize + 0xFF) &~ 0xFF, Iova(uboAddr), offset
	);

	w << CmdList<1>{ MakeCmdHeader(NonIncreasing, sizeWords, Subchannel3D, Engine3D::LoadConstbufData{}) };
	w.addRawData(data, size);
}
