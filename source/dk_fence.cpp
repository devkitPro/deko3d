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
			res = nvFenceWait(&m_internal.m_fence, timeout_us);
			break;
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
	s32 timeout_us = -1;
	if (timeout_ns >= 0)
		timeout_us = timeout_ns / 1000;
	return obj->wait(timeout_us);
}
