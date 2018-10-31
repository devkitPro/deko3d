#include "dk_memblock.h"
#include "dk_device.h"

DkResult tag_DkMemBlock::initialize(uint32_t flags, void* storage, uint32_t size)
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

	if (!storage)
	{
		m_ownedMem = allocMem(size, DK_MEMBLOCK_ALIGNMENT);
		if (!m_ownedMem)
			return DkResult_OutOfMemory;
		storage = m_ownedMem;
	}

	uint32_t bigPageSize = getDevice()->getGpuInfo().bigPageSize;
	if (R_FAILED(nvMapAlloc(&m_mapObj, storage, size, bigPageSize, NvKind_Pitch, isCpuCached())))
		return DkResult_Fail;

	if (!isGpuNoAccess())
	{
		// Create pitch mapping (TODO: use fixed mapping for DkMemBlockFlags_Code)
		if (R_FAILED(nvAddressSpaceMap(getDevice()->getAddrSpace(),
			getHandle(), isGpuCached(), NvKind_Pitch, &m_gpuAddrPitch)))
			return DkResult_Fail;

		// TODO: create swizzled/compressed mappings for DkMemBlockFlags_Image
	}

	return DkResult_Success;
}

tag_DkMemBlock::~tag_DkMemBlock()
{
	if (m_gpuAddrPitch != DK_GPU_ADDR_INVALID)
		nvAddressSpaceUnmap(getDevice()->getAddrSpace(), m_gpuAddrPitch);

	nvMapFree(&m_mapObj); // does nothing if uninitialized
	if (m_ownedMem)
		freeMem(m_ownedMem);
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
	obj = new(maker->device) tag_DkMemBlock(maker->device);
	if (obj)
	{
		DkResult res = obj->initialize(maker->flags, maker->storage, maker->size);
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
	return obj->getCpuAddr();
}

DkGpuAddr dkMemBlockGetGpuAddr(DkMemBlock obj)
{
	return obj->getGpuAddrPitch();
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
