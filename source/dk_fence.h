#pragma once
#include "dk_private.h"

namespace dk::detail
{

struct Fence
{
	enum Type
	{
		Invalid,
		Internal,
		External,
	};

	struct _Internal
	{
		Type m_type;
		uint32_t m_semaphoreValue;
		DkGpuAddr m_semaphoreAddr;
		uint32_t volatile* m_semaphoreCpuAddr;
		NvFence m_fence;
	};

	struct _External
	{
		Type m_type;
		NvMultiFence m_fence;
	};

	union
	{
		Type m_type;
		_Internal m_internal;
		_External m_external;
	};

	bool internalPoll() const
	{
		return (int32_t)(*m_internal.m_semaphoreCpuAddr - m_internal.m_semaphoreValue) >= 0;
	}

	DkResult wait(s32 timeout_us = -1);
};

}

DK_OPAQUE_CHECK(Fence);
