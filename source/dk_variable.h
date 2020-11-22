#pragma once
#include "dk_private.h"

namespace dk::detail
{

struct Variable
{
	uint32_t* m_cpuAddr;
	DkGpuAddr m_gpuAddr;
};

}

DK_OPAQUE_CHECK(Variable);
