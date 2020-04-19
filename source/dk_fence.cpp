#include "dk_fence.h"

DkResult DkFence::wait(s32 timeout_us)
{
	Result res = 0;
	switch (m_type)
	{
		default:
		case DkFence::Invalid:
			return DkResult_Fail;
		case DkFence::Internal:
		{
			// Succeed early if the semaphore is signaled
			if (internalPoll())
				return DkResult_Success;

			// Fail early if the timeout is zero
			if (timeout_us == 0)
				return DkResult_Timeout;

			u64 start = armGetSystemTick();
			do
			{
				s32 wait_timeout = 100000; // 10^5 μs = 100 ms
				if (timeout_us >= 0)
				{
					s32 elapsed_us = armTicksToNs(armGetSystemTick() - start) / 1000U;
					s32 remaining_us = timeout_us - elapsed_us;
					if (remaining_us <= 0)
					{
						res = MAKERESULT(Module_LibnxNvidia, LibnxNvidiaError_Timeout);
						break;
					}
					if (wait_timeout > remaining_us)
						wait_timeout = remaining_us;
				}
				res = nvFenceWait(&m_internal.m_fence, wait_timeout);
				if (R_FAILED(res) && res != MAKERESULT(Module_LibnxNvidia, LibnxNvidiaError_Timeout))
					break;
			} while (R_FAILED(res) || !internalPoll());
			break;
		}
		case DkFence::External:
			res = nvMultiFenceWait(&m_external.m_fence, timeout_us);
			break;
	}

	switch (res)
	{
		case 0:
			return DkResult_Success;
		case MAKERESULT(Module_LibnxNvidia, LibnxNvidiaError_Timeout):
			return DkResult_Timeout;
		default:
			return DkResult_Fail;
	}
}

DkResult dkFenceWait(DkFence* obj, int64_t timeout_ns)
{
	if (obj->m_type == DkFence::Internal)
		DK_ENTRYPOINT(obj->m_internal.m_device);

	s32 timeout_us = -1;
	if (timeout_ns >= 0)
		timeout_us = timeout_ns / 1000;
	return obj->wait(timeout_us);
}
