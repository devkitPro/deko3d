#pragma once
#include "dk_private.h"

#include "dksh.h"

struct DkShader
{
	uint32_t m_magic;
	DkStage  m_stage;
	uint32_t m_id;
	uint32_t m_cbuf1IovaShift8;
	DkshProgramHeader m_hdr;
};

DK_OPAQUE_CHECK(DkShader);
