#include "dk_device.h"
#include "dk_queue.h"

using namespace dk::detail;

void Device::checkQueueErrors() noexcept
{
	MutexHolder m{m_queueTableMutex};
	for (unsigned i = 0; i < s_numQueues; i ++)
	{
		DkQueue q = m_queueTable[i];
		if (q) q->checkError();
	}
}

bool Queue::checkError()
{
	NvNotification notif;
	if (R_FAILED(nvGpuChannelGetErrorNotification(&m_gpuChannel, &notif)) || !notif.status)
		return false; // No error

	DK_WARNING("Queue (%u) entered error state", m_id);
	DK_WARNING("  timestamp: %lu", notif.timestamp);
	DK_WARNING("  info32: %u", notif.info32);
	DK_WARNING("  info16: %u", notif.info16);
	DK_WARNING("  status: %u", notif.status);

	NvError error;
	if (R_FAILED(nvGpuChannelGetErrorInfo(&m_gpuChannel, &error)))
		DK_WARNING("  (Failed to retrieve error info)");
	else
	{
		DK_WARNING("  --");
		switch (error.type)
		{
			case 0:
				DK_WARNING("  No error information available");
				break;
			case 1:
				DK_WARNING("  GPU page fault (info 0x%08x)", error.info[0]);
				DK_WARNING("  Address: 0x%02x%08x", error.info[1], error.info[2]);
				DK_WARNING("  Access type: %s", error.info[3] == 2 ? "Write" : "Read");
				break;
			case 2:
				DK_WARNING("  GPU method error (irq 0x%08x)", error.info[0]);
				DK_WARNING("  [%04x:%03x] = 0x%08x", error.info[4], (error.info[1]&0xFFFF)/4, error.info[3]);
				DK_WARNING("  Unknown data: 0x%08x; 0x%04x", error.info[2], error.info[1]>>16);
				break;
			case 3:
				DK_WARNING("  GPU rejected command list");
				break;
			case 4:
				DK_WARNING("  GPU timeout");
				break;
			default:
				DK_WARNING("  Unknown (%u)", error.type);
				break;
		}
	}

	// Enter error state - update the semaphore with the most recent value so that any users
	// who are waiting for work in this failed queue to complete are allowed to end the wait
	m_state = Error;
	getDevice()->getSemaphoreCpuAddr(m_id)->sequence = getDevice()->getSemaphoreValue(m_id);
	return true;
}
