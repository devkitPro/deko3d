#include "../dk_queue.h"
#include "../dk_device.h"
#include "../queue_compute.h"
#include "../cmdbuf_writer.h"

#include "engine_compute.h"

using namespace maxwell;
using namespace dk::detail;

using C = EngineCompute;

void ComputeQueue::initQmd()
{
	job.qmd.api_visible_call_limit = 1; // no_limit
	job.qmd.sm_global_caching_enable = 1;
	job.qmd.l1_configuration = 3; // directly_adressable_memory_size_48kb
	job.qmd.qmd_version = 7;
	job.qmd.qmd_major_version = 1;
}

void ComputeQueue::initComputeEngine()
{
	DkDevice dev = m_parent.getDevice();
	CmdBufWriter w{&m_parent.m_cmdBuf};
	w.reserve(20);

	DkGpuAddr scratchMemIova = m_parent.m_workBuf.getScratchMem();
	uint32_t scratchMemPerSm = m_parent.m_workBuf.getScratchMemSize() / dev->getGpuInfo().numSms;
	scratchMemPerSm &= ~0x7FFF;

	w << CmdInline(Compute, SetShaderExceptions{}, 0);
	w << CmdInline(Compute, SetBindlessTexture{}, 0); // Using constbuf0 as the texture constbuf
	w << Cmd(Compute, SetShaderLocalMemoryWindow{}, 0x01000000);
	w << Cmd(Compute, SetShaderSharedMemoryWindow{}, 0x03000000);
	w << Cmd(Compute, SetProgramRegion{}, Iova(dev->getCodeSeg().getBase()));
	w << CmdInline(Compute, SetSpaVersion{},
		C::SetSpaVersion::Major{5} | C::SetSpaVersion::Minor{3} // SM 5.3
	);
	w << Cmd(Compute, SetShaderLocalMemory{}, Iova(scratchMemIova));
	w << Cmd(Compute, SetShaderLocalMemoryNonThrottledA{},
		0, scratchMemPerSm, 0x100, // NonThrottled
		0, scratchMemPerSm, 0x100  // Throttled
	);
}

void ComputeQueue::bindConstbuf(uint32_t id, DkGpuAddr addr, uint32_t size)
{
	if (size)
	{
		job.qmd.constant_buffer_valid |= 1U << id;
		job.qmd.constant_buffer[id].addr_lower = addr;
		job.qmd.constant_buffer[id].addr_upper = addr >> 32;
		job.qmd.constant_buffer[id].size = size;
	}
	else
		job.qmd.constant_buffer_valid &= ~(1U << id);
}

void ComputeQueue::bindShader(CtrlCmdComputeShader const* cmd)
{
	DkDevice dev = m_parent.getDevice();
	auto& info = dev->getGpuInfo();

	// Check if there's enough available scratch memory to run the shader at full speed
	uint32_t totalScratchMem = m_parent.m_workBuf.getScratchMemSize();
	uint32_t availablePerWarpScratchMem = m_parent.m_workBuf.getPerWarpScratchSize();
	if (availablePerWarpScratchMem < cmd->perWarpScratchSize)
	{
		// There isn't - so now check if there's enough memory to run the shader at all
		uint32_t minRequiredScratchMem = (cmd->perWarpScratchSize * info.numWarpsPerSm + 0x7FFF) &~ 0x7FFF;
#ifdef DEBUG
		printf("Warning: Throttling compute shaders (0x%x - 0x%x - 0x%x)\n", cmd->perWarpScratchSize, minRequiredScratchMem, totalScratchMem);
#endif
		if (totalScratchMem < minRequiredScratchMem)
		{
			m_parent.raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_OutOfMemory);
			return;
		}

		// Calculate and configure the throttling parameters
		job.qmd.throttled = 1;
		uint32_t maxSms = totalScratchMem / minRequiredScratchMem;
		if (maxSms != m_curSmThrottling)
		{
			m_curSmThrottling = maxSms;
			CmdBufWriterChecked w{&m_parent.m_cmdBuf};
			w << Cmd(Compute, SetShaderLocalMemoryThrottledA{},
				0, minRequiredScratchMem, maxSms
			);
		}
	}
	else // We can run the shader at full speed.
		job.qmd.throttled = 0;

	job.qmd.program_offset = cmd->arg;
	job.qmd.shared_memory_size = cmd->sharedMemSize;
	job.qmd.cta_thread_dimension0 = cmd->blockDims[0];
	job.qmd.cta_thread_dimension1 = cmd->blockDims[1];
	job.qmd.cta_thread_dimension2 = cmd->blockDims[2];
	job.qmd.shader_local_memory_low_size = cmd->localLoMemSize;
	job.qmd.barrier_count = cmd->numBarriers;
	job.qmd.shader_local_memory_high_size = cmd->localHiMemSize;
	job.qmd.register_count = cmd->numRegisters;
	job.qmd.shader_local_memory_crs_size = cmd->crsSize;
	bindConstbuf(1, dev->getCodeSeg().getBase() + cmd->dataOffset, (cmd->dataSize + 0xFF) &~ 0xFF);

	job.cbuf.ctaSize[0] = cmd->blockDims[0];
	job.cbuf.ctaSize[1] = cmd->blockDims[1];
	job.cbuf.ctaSize[2] = cmd->blockDims[2];
}

void ComputeQueue::dispatch(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ, DkGpuAddr indirect)
{
	CmdBufWriter w{&m_parent.m_cmdBuf};
	w.reserve(12 + s_jobSizeWords);

	uint32_t jobId = m_curJob;
	if (jobId >= m_parent.m_workBuf.getComputeJobsCount())
	{
		// We ran out of slots in the job queue, so now we need to wait for
		// all previous jobs to finish and start over from the beginning
		using ISC = C::InvalidateShaderCaches;
		w << CmdInline(Compute, WaitForIdle{}, 0);
		w << CmdInline(Compute, InvalidateShaderCaches{}, ISC::Constant{}); // we're overwriting old cbufs so invalidate this too
		jobId = 0;
	}
	m_curJob = jobId + 1; // advance the job counter

	// Calculate the address to the job within the queue
	DkGpuAddr jobAddr = m_parent.m_workBuf.getComputeJobs();
	jobAddr += jobId * s_jobSizeAlign;

	// Update grid dimension parameters in the QMD
	job.qmd.cta_raster_width  = numGroupsX;
	job.qmd.cta_raster_height = numGroupsY;
	job.qmd.cta_raster_depth  = numGroupsZ;
	bindConstbuf(0, jobAddr + sizeof(ComputeQmd), ComputeDriverCbufSize);

	// Update grid dimension parameters in the driver constbuf
	job.cbuf.gridSize[0] = numGroupsX;
	job.cbuf.gridSize[1] = numGroupsY;
	job.cbuf.gridSize[2] = numGroupsZ;

	// Time to copy the job to the job queue!
	w << Cmd(Compute, LineLengthIn{},
		s_jobSizeBytes, // LineLengthIn
		1,              // LineCount
		Iova(jobAddr)   // OffsetOut
	);
	w << CmdInline(Compute, LaunchDma{},
		C::LaunchDma::DstMemoryLayout::Pitch | C::LaunchDma::CompletionType::FlushOnly
	);
	w << CmdList<1>{ MakeCmdHeader(NonIncreasing, s_jobSizeWords, SubchannelCompute, C::LoadInlineData{}) };
	w.addRawData(&job, s_jobSizeBytes);

	// Launch the job
	w << Cmd(Compute, SendPcasA{}, uint32_t(jobAddr>>8));
	w << CmdInline(Compute, SendSignalingPcasB{},
		C::SendSignalingPcasB::Invalidate{} | C::SendSignalingPcasB::Schedule{}
	);
}
