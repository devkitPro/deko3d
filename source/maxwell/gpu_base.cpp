#include "../dk_queue.h"

#include "helpers.h"
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
	m_cmdBuf.append(
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

	// TODO: Reserve space for ZBC commands
	m_cmdBuf.signOffGpfifoEntry(CtrlCmdGpfifoEntry::NoPrefetch);

	// Append a dummy single-word cmdlist, used as a cmdlist processing barrier
	m_cmdBuf.append(CmdList<1>{0});
	m_cmdBuf.signOffGpfifoEntry(CtrlCmdGpfifoEntry::AutoKick | CtrlCmdGpfifoEntry::NoPrefetch);
}

void dkCmdBufBarrier(DkCmdBuf obj, DkBarrier mode, uint32_t invalidateFlags)
{
	if (!obj->reserveCmdMem(12))
		return;

	bool needsWfi = false;
	switch (mode)
	{
		default:
		case DkBarrier_None:
			break;
		case DkBarrier_Tiles:
			needsWfi = true;
			obj->appendDirectly(CmdInline(3D, TiledCacheBarrier{}, 0));
			break;
		case DkBarrier_Fragments:
			needsWfi = true;
			obj->appendDirectly(CmdInline(3D, FragmentBarrier{}, 0));
			break;
		case DkBarrier_Primitives:
			obj->appendDirectly(CmdInline(3D, WaitForIdle{}, 0));
			break;
		case DkBarrier_Full:
			obj->appendDirectly(CmdInline(Gpfifo, SetReference{}, 0));
			obj->signOffGpfifoEntry(0);
			obj->appendDirectly(CmdInline(3D, NoOperation{}, 0));
			break;
	}

	if (invalidateFlags & DkInvalidateFlags_Image)
	{
		if (needsWfi)
			obj->appendDirectly(CmdInline(3D, InvalidateTextureDataCache{}, 0));
		else
			obj->appendDirectly(CmdInline(3D, InvalidateTextureDataCacheNoWfi{}, 0));
	}

	if (invalidateFlags & DkInvalidateFlags_Code)
	{
		using ISC = Engine3D::InvalidateShaderCaches;
		obj->appendDirectly(CmdInline(3D, InvalidateShaderCaches{},
			ISC::Instruction{} | ISC::GlobalData{} | ISC::Constant{}
		));
	}

	if (invalidateFlags & DkInvalidateFlags_Pool)
	{
		obj->appendDirectly(CmdInline(3D, InvalidateTextureHeaderCacheNoWfi{}, 0));
		obj->appendDirectly(CmdInline(3D, InvalidateSamplerCacheNoWfi{}, 0));
	}

	if (invalidateFlags & DkInvalidateFlags_Zcull)
		obj->appendDirectly(CmdInline(3D, InvalidateZcullNoWfi{}, 0));

	if (invalidateFlags & DkInvalidateFlags_L2Cache)
	{
		using Op = EngineGpfifo::MemOpB::Operation;
		obj->appendDirectly(Cmd(Gpfifo, MemOpB{}, Op::L2FlushDirty));
		obj->appendDirectly(Cmd(Gpfifo, MemOpB{}, Op::L2SysmemInvalidate));
	}

	// For DkBarrier_Full, we sign off a gpfifo entry with the NoPrefetch bit set.
	// This means we tell the command processor to avoid prefetching any further command data.
	// Note that indirect draw/dispatch param buffers are stashed away in the command list as gpfifo entries,
	// so this will effectively ensure that *absolutely everything* will finish before those params get fetched.
	if (mode == DkBarrier_Full)
		obj->signOffGpfifoEntry(CtrlCmdGpfifoEntry::AutoKick | CtrlCmdGpfifoEntry::NoPrefetch);
}
