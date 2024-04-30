#include "../dk_queue.h"
#include "../cmdbuf_writer.h"

#include "mme_macros.h"
#include "engine_3d.h"
#include "engine_compute.h"
#include "engine_inline.h"
#include "engine_2d.h"
#include "engine_copy.h"
#include "engine_gpfifo.h"

using namespace maxwell;
using namespace dk::detail;

void Queue::setupEngines()
{
	CmdBufWriter w{&m_cmdBuf};
	w.reserveAdd(
		BindEngine(3D),
		BindEngine(Compute),
		BindEngine(Copy),
		BindEngine(Inline),
		BindEngine(2D),
		CmdsFromArray(MmeMacro_SetupCmds)
	);
}

void Queue::postSubmitFlush()
{
	// Invalidate image cache, image/sampler descriptor cache, shader caches, and L2 cache
	// This is done to ensure the visibility of CPU updates between calls to dkQueueFlush()
	dkCmdBufBarrier(&m_cmdBuf,
		DkBarrier_None,
		DkInvalidateFlags_Image |
		DkInvalidateFlags_Shader |
		DkInvalidateFlags_Descriptors |
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
	DK_ENTRYPOINT(obj);
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

	if (invalidateFlags & DkInvalidateFlags_Shader)
	{
		using ISC = Engine3D::InvalidateShaderCaches;
		w << CmdInline(3D, InvalidateShaderCaches{},
			ISC::Instruction{} | ISC::GlobalData{} | ISC::Constant{}
		);
	}

	if (invalidateFlags & DkInvalidateFlags_Descriptors)
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

void dkCmdBufBindImageDescriptorSet(DkCmdBuf obj, DkGpuAddr setAddr, uint32_t numDescriptors)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_DATA_ALIGN(setAddr, DK_IMAGE_DESCRIPTOR_ALIGNMENT);
	DK_DEBUG_NON_ZERO(numDescriptors);
	CmdBufWriter w{obj};
	w.reserve(8);

	w << Cmd(3D,      SetTexHeaderPool{}, Iova(setAddr), numDescriptors-1);
	w << Cmd(Compute, SetTexHeaderPool{}, Iova(setAddr), numDescriptors-1);
}

void dkCmdBufBindSamplerDescriptorSet(DkCmdBuf obj, DkGpuAddr setAddr, uint32_t numDescriptors)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_DATA_ALIGN(setAddr, DK_SAMPLER_DESCRIPTOR_ALIGNMENT);
	DK_DEBUG_NON_ZERO(numDescriptors);
	CmdBufWriter w{obj};
	w.reserve(8);

	w << Cmd(3D,      SetTexSamplerPool{}, Iova(setAddr), numDescriptors-1);
	w << Cmd(Compute, SetTexSamplerPool{}, Iova(setAddr), numDescriptors-1);
}

void dkCmdBufPushConstants(DkCmdBuf obj, DkGpuAddr uboAddr, uint32_t uboSize, uint32_t offset, uint32_t size, const void* data)
{
	DK_ENTRYPOINT(obj);
	DK_DEBUG_DATA_ALIGN(uboAddr, DK_UNIFORM_BUF_ALIGNMENT);
	DK_DEBUG_SIZE_ALIGN(uboSize, DK_UNIFORM_BUF_ALIGNMENT);
	DK_DEBUG_BAD_INPUT(uboSize > DK_UNIFORM_BUF_MAX_SIZE);
	DK_DEBUG_DATA_ALIGN(offset, 4);
	DK_DEBUG_SIZE_ALIGN(size, 4);
	DK_DEBUG_BAD_INPUT(size > 0x7FFC);
	DK_DEBUG_BAD_INPUT((offset >= uboSize) || (size > uboSize));
	DK_DEBUG_BAD_INPUT((offset + size) > uboSize);
	DK_DEBUG_NON_NULL(data);
	uint32_t sizeWords = size/4;
	CmdBufWriter w{obj};
	w.reserve(7 + sizeWords);

	w << Cmd(3D, ConstbufSelectorSize{},
		(uboSize + 0xFF) &~ 0xFF, Iova(uboAddr), offset
	);

	w << CmdInline(3D, PipeNop{}, 0);
	w << CmdList<1>{ MakeCmdHeader(NonIncreasing, sizeWords, Subchannel3D, Engine3D::LoadConstbufData{}) };
	w.addRawData(data, size);
}
