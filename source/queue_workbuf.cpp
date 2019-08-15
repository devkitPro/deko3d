#include "queue_workbuf.h"

#include "dk_device.h"
#include "driver_constbuf.h"
#include "maxwell/compute_qmd.h"

using namespace dk::detail;

QueueWorkBuf::QueueWorkBuf(DkQueueMaker const& maker) : tag_DkMemBlock{maker.device},
	m_scratchMemOffset{}, m_scratchMemSize{},
	m_graphicsCbufOffset{}, m_graphicsCbufSize{},
	m_vtxRunoutBufOffset{}, m_vtxRunoutBufSize{},
	m_zcullCtxOffset{}, m_zcullCtxSize{},
	m_computeJobsOffset{}, m_computeJobsCount{},
	m_totalSize{}
{
	auto& info = getDevice()->getGpuInfo();
	bool hasGraphics = (maker.flags & DkQueueFlags_Graphics) != 0;
	bool hasCompute  = (maker.flags & DkQueueFlags_Compute)  != 0;

	if (hasGraphics || hasCompute)
	{
		uint32_t totalScratchMemorySize = maker.perWarpScratchMemorySize * info.numWarpsPerSm * info.numSms;
		totalScratchMemorySize = (totalScratchMemorySize + 0x1FFFF) &~ 0x1FFFF; // Align size to 128 KiB...
		m_scratchMemSize = addSection(m_scratchMemOffset, totalScratchMemorySize, 0x1000); // ... although the buffer itself only needs page alignment
	}

	if (hasGraphics)
	{
		m_graphicsCbufSize = addSection(m_graphicsCbufOffset, sizeof(GraphicsDriverCbuf), DK_UNIFORM_BUF_ALIGNMENT);
		m_vtxRunoutBufSize = addSection(m_vtxRunoutBufOffset, 0x100, 0x100); // TODO: verify alignment?
		if (!(maker.flags & DkQueueFlags_DisableZcull))
			m_zcullCtxSize = addSection(m_zcullCtxOffset, info.zcullCtxSize, info.zcullCtxAlign);
	}

	if (hasCompute)
	{
		uint32_t jobSize = sizeof(maxwell::ComputeQmd) + sizeof(ComputeDriverCbuf);
		m_computeJobsCount = maker.maxConcurrentComputeJobs;
		addSection(m_computeJobsOffset, m_computeJobsCount*jobSize, DK_UNIFORM_BUF_ALIGNMENT);
	}

	m_totalSize = (m_totalSize + DK_MEMBLOCK_ALIGNMENT - 1) &~ (DK_MEMBLOCK_ALIGNMENT - 1);

#ifdef DK_QUEUE_WORKBUF_DEBUG
	printf("Scratch mem:    0x%x (0x%x bytes)\n", m_scratchMemOffset, m_scratchMemSize);
	printf("Graphics cbuf:  0x%x (0x%x bytes)\n", m_graphicsCbufOffset, m_graphicsCbufSize);
	printf("Vtx runout buf: 0x%x (0x%x bytes)\n", m_vtxRunoutBufOffset, m_vtxRunoutBufSize);
	printf("Zcull ctx:      0x%x (0x%x bytes)\n", m_zcullCtxOffset, m_zcullCtxSize);
	printf("Compute mem:    0x%x (%u jobs)\n",    m_computeJobsOffset, m_computeJobsCount);
	printf("Total size:     0x%x bytes\n",        m_totalSize);
#endif
}
