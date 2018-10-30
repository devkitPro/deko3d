#include "dk_memblock.h"

DkResult tag_DkMemBlock::initialize(uint32_t flags, void* storage)
{
	uint32_t cpuAccess = (flags >> DkMemBlockFlags_CpuAccessShift) & DkMemAccess_Mask;
	uint32_t gpuAccess = (flags >> DkMemBlockFlags_GpuAccessShift) & DkMemAccess_Mask;
	flags &= ~(DkMemBlockFlags_CpuAccessMask | DkMemBlockFlags_GpuAccessMask);

	if (flags & DkMemBlockFlags_Code)
		return DkResult_NotImplemented;
	if (flags & DkMemBlockFlags_Image)
		return DkResult_NotImplemented;

	flags |= cpuAccess << DkMemBlockFlags_CpuAccessShift;
	flags |= gpuAccess << DkMemBlockFlags_GpuAccessShift;
	m_flags = flags;

	return DkResult_Success;
}

DkMemBlock dkMemBlockCreate(DkMemBlockMaker const* maker)
{
	DkMemBlock obj = nullptr;
#ifdef DEBUG
	if (maker->size & (DK_MEMBLOCK_ALIGNMENT-1))
		DkObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedSize);
	else if (uintptr_t(maker->storage) & (DK_MEMBLOCK_ALIGNMENT-1))
		DkObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedData);
	else
#endif
	obj = new(maker->device) tag_DkMemBlock(maker->device, maker->size);
	if (obj)
	{
		DkResult res = obj->initialize(maker->flags, maker->storage);
		if (res != DkResult_Success)
		{
			delete obj;
			obj = nullptr;
			DkObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, res);
		}
	}
	return obj;
}

void dkMemBlockDestroy(DkMemBlock obj)
{
	delete obj;
}

void* dkMemBlockGetCpuAddr(DkMemBlock obj)
{
	return nullptr;
}

DkGpuAddr dkMemBlockGetGpuAddr(DkMemBlock obj)
{
	return 0;
}

DkResult dkMemBlockFlushCpuCache(DkMemBlock obj, uint32_t offset, uint32_t size)
{
	if (!obj->isCpuCached())
		return DkResult_Success;
	return DkResult_NotImplemented;
}

DkResult dkMemBlockInvalidateCpuCache(DkMemBlock obj, uint32_t offset, uint32_t size)
{
	if (!obj->isCpuCached())
		return DkResult_Success;
	return DkResult_NotImplemented;
}
