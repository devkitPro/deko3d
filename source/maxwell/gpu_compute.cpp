#include "../dk_queue.h"
#include "../queue_compute.h"
#include "../cmdbuf_writer.h"

#include "engine_compute.h"

using namespace maxwell;
using namespace dk::detail;

void ComputeQueue::initQmd()
{
	printf("{STUBBED} ComputeQueue::initQmd\n");
}

void ComputeQueue::initComputeEngine()
{
	printf("{STUBBED} ComputeQueue::initComputeEngine\n");
}

void ComputeQueue::bindConstbuf(uint32_t id, DkGpuAddr addr, uint32_t size)
{
	printf("{STUBBED} ComputeQueue::bindConstbuf\n");
}

void ComputeQueue::bindShader(CtrlCmdComputeShader const* cmd)
{
	printf("{STUBBED} ComputeQueue::bindShader\n");
}

void ComputeQueue::dispatch(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ, DkGpuAddr indirect)
{
	printf("{STUBBED} ComputeQueue::dispatch\n");
}
