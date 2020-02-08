#include "dk_shader.h"
#include "dk_device.h"
#include "dk_memblock.h"

using namespace dk::detail;

namespace
{
	uint32_t programId;

	uint32_t getNewProgramId()
	{
		uint32_t newId;
		uint32_t curId = __atomic_load_n(&programId, __ATOMIC_SEQ_CST);
		do
		{
			newId = curId + 1;
			if (newId == 0)
				newId = 1; // roll over and skip 0
		} while (!__atomic_compare_exchange_n(&programId, &curId, newId, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
		return newId;
	}

	constexpr DkStage DkshProgramTypeToDkStage(uint32_t id)
	{
		switch (id)
		{
			default:
			case DkshProgramType_Vertex:   return DkStage_Vertex;
			case DkshProgramType_Fragment: return DkStage_Fragment;
			case DkshProgramType_Geometry: return DkStage_Geometry;
			case DkshProgramType_TessCtrl: return DkStage_TessCtrl;
			case DkshProgramType_TessEval: return DkStage_TessEval;
			case DkshProgramType_Compute:  return DkStage_Compute;
		}
	}
}

void dkShaderInitialize(DkShader* obj, DkShaderMaker const* maker)
{
#ifdef DEBUG
	if (!obj)
		return;
#endif

	memset(obj, 0, sizeof(*obj));

#ifdef DEBUG
	if (!maker || !maker->codeMem)
		return;
#endif

	// Validate memory block
	DkMemBlock blk = maker->codeMem;
#ifdef DEBUG
	if (!blk->isCode())
	{
		blk->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
		return;
	}
	if (maker->codeOffset & (DK_SHADER_CODE_ALIGNMENT-1))
	{
		blk->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedData);
		return;
	}
#endif

	// Find the DKSH header
	bool hasSeparateControl = maker->control != nullptr;
	DkshHeader* phdr = (DkshHeader*)maker->control;
	uint32_t codeBaseOffset = maker->codeOffset;
	if (!hasSeparateControl)
		phdr = (DkshHeader*)((u8*)blk->getCpuAddr() + codeBaseOffset);

#ifdef DEBUG
	// Validate the DKSH header
	if (phdr->magic != DKSH_MAGIC)
	{
		blk->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
		return;
	}
	if ((phdr->control_sz|phdr->code_sz) & (DK_SHADER_CODE_ALIGNMENT-1))
	{
		blk->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedSize);
		return;
	}
	if (maker->programId >= phdr->num_programs)
	{
		blk->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
		return;
	}
#endif

	// Calculate the offset to the code section within the device's code segment
	if (!hasSeparateControl)
		codeBaseOffset += phdr->control_sz;

#ifdef DEBUG
	// Ensure the code section doesn't extend into the memory block's unusable area
	if (codeBaseOffset + phdr->code_sz > blk->getSize() - DK_SHADER_CODE_UNUSABLE_SIZE)
	{
		blk->raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadInput);
		return;
	}
#endif

	// Find the address to the program header
	auto* progTable = (DkshProgramHeader*)((u8*)phdr + phdr->programs_off);
	auto& progHdr = progTable[maker->programId];

	// Initialize the DkShader struct
	obj->m_magic = DKSH_MAGIC;
	obj->m_stage = DkshProgramTypeToDkStage(progHdr.type);
	obj->m_id    = getNewProgramId();
	obj->m_hdr   = progHdr;
	obj->m_cbuf1IovaShift8 = (blk->getGpuAddrPitch() + codeBaseOffset + obj->m_hdr.constbuf1_off) >> 8;

	// Fix up code/data offsets
	codeBaseOffset += blk->getCodeSegOffset();
	obj->m_hdr.entrypoint    += codeBaseOffset;
	obj->m_hdr.constbuf1_off += codeBaseOffset;

#ifdef DK_SHADER_DEBUG
	printf("ID:    0x%08x\n", obj->m_id);
	printf("Stage: %u\n",     obj->m_stage);
	printf("Entry: 0x%08x\n", obj->m_hdr.entrypoint);
	printf("CB1:   0x%08x\n", obj->m_hdr.constbuf1_off);
	printf("CB1sz: 0x%x\n",   obj->m_hdr.constbuf1_sz);
	printf("GPR:   %u\n",     obj->m_hdr.num_gprs);
#endif
}

bool dkShaderIsValid(DkShader const* obj)
{
	return obj->m_magic == DKSH_MAGIC;
}

DkStage dkShaderGetStage(DkShader const* obj)
{
	return obj->m_stage;
}
