#pragma once
#include "dk_private.h"

#include "dksh.h"

struct DkShader
{
	uint32_t m_magic;
	DkStage  m_stage;
	DkshProgramHeader m_hdr;
};

DK_OPAQUE_CHECK(DkShader);
