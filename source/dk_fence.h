#pragma once
#include "dk_private.h"

struct DkFence
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

	DkResult wait(s32 timeout_us = -1);
};

DK_OPAQUE_CHECK(DkFence);
