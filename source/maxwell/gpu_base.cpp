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
