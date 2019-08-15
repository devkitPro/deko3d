#include "dk_memblock.h"
#include "dk_device.h"

using namespace dk::detail;

DkResult tag_DkMemBlock::initialize(uint32_t flags, void* storage, uint32_t size)
{
	// Extract CPU/GPU access bits
	uint32_t cpuAccess = (flags >> DkMemBlockFlags_CpuAccessShift) & DkMemAccess_Mask;
	uint32_t gpuAccess = (flags >> DkMemBlockFlags_GpuAccessShift) & DkMemAccess_Mask;
	flags &= ~(DkMemBlockFlags_CpuAccessMask | DkMemBlockFlags_GpuAccessMask);

	// Extract special flags
	bool zeroFillInit = (flags & DkMemBlockFlags_ZeroFillInit) != 0;
	flags &= ~DkMemBlockFlags_ZeroFillInit;

	// Validate access bits for code memory
	if (flags & DkMemBlockFlags_Code)
	{
		// Ensure code memory can be accessed by the CPU
		if (cpuAccess == DkMemAccess_None)
			cpuAccess = DkMemAccess_Uncached;

		// It is invalid to request code memory that is not GPU accessible for obvious reasons...
		if (gpuAccess == DkMemAccess_None)
			return DkResult_BadInput;
	}

	// Validate access bits for image memory (TODO: implement image memory)
	if (flags & DkMemBlockFlags_Image)
		return DkResult_NotImplemented;

	// Insert back access bits into flags
	flags |= cpuAccess << DkMemBlockFlags_CpuAccessShift;
	flags |= gpuAccess << DkMemBlockFlags_GpuAccessShift;
	m_flags = flags;

	// Allocate backing memory if the user didn't supply storage for this memory block
	if (!storage)
	{
		m_ownedMem = allocMem(size, DK_MEMBLOCK_ALIGNMENT);
		if (!m_ownedMem)
			return DkResult_OutOfMemory;
		storage = m_ownedMem;
	}

	// Zerofill the memory if requested
	if (zeroFillInit)
		memset(storage, 0, size);

	// Create a NvMap object on this memory
	uint32_t bigPageSize = getDevice()->getGpuInfo().bigPageSize;
	if (R_FAILED(nvMapCreate(&m_mapObj, storage, size, bigPageSize, NvKind_Pitch, isCpuCached())))
		return DkResult_Fail;

	// Map the block into the GPU address space, if the GPU is to have access to this memory block
	if (!isGpuNoAccess())
	{
		// Create pitch mapping
		if (!isCode())
		{
			// For non-code memory blocks, we can just let the system place it automatically.
			if (R_FAILED(nvAddressSpaceMap(getDevice()->getAddrSpace(),
				getHandle(), isGpuCached(), NvKind_Pitch, &m_gpuAddrPitch)))
				return DkResult_Fail;
		}
		else
		{
			auto& codeSeg = getDevice()->getCodeSeg();

			// Reserve a suitable chunk of address space in the code segment
			if (!codeSeg.allocSpace(size, m_gpuAddrPitch))
				return DkResult_Fail;

			// Create a fixed mapping on said chunk
			if (R_FAILED(nvAddressSpaceMapFixed(getDevice()->getAddrSpace(),
				getHandle(), isGpuCached(), NvKind_Pitch, m_gpuAddrPitch)))
			{
				codeSeg.freeSpace(m_gpuAddrPitch, size);
				m_gpuAddrPitch = DK_GPU_ADDR_INVALID;
				return DkResult_Fail;
			}

			// Retrieve the code segment offset
			m_codeSegOffset = codeSeg.calcOffset(m_gpuAddrPitch);
		}

		// TODO: create swizzled/compressed mappings for DkMemBlockFlags_Image
	}

	return DkResult_Success;
}

void tag_DkMemBlock::destroy()
{
	if (m_gpuAddrPitch != DK_GPU_ADDR_INVALID)
	{
		nvAddressSpaceUnmap(getDevice()->getAddrSpace(), m_gpuAddrPitch);
		if (isCode())
			getDevice()->getCodeSeg().freeSpace(m_gpuAddrPitch, getSize());
		m_gpuAddrPitch = DK_GPU_ADDR_INVALID;
	}

	nvMapClose(&m_mapObj); // does nothing if uninitialized
	if (m_ownedMem)
	{
		freeMem(m_ownedMem);
		m_ownedMem = nullptr;
	}
}

DkMemBlock dkMemBlockCreate(DkMemBlockMaker const* maker)
{
	DkMemBlock obj = nullptr;
#ifdef DEBUG
	if (maker->size & (DK_MEMBLOCK_ALIGNMENT-1))
		ObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedSize);
	else if (uintptr_t(maker->storage) & (DK_MEMBLOCK_ALIGNMENT-1))
		ObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, DkResult_MisalignedData);
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
			ObjBase::raiseError(maker->device, DK_FUNC_ERROR_CONTEXT, res);
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

uint32_t dkMemBlockGetSize(DkMemBlock obj)
{
	return obj->getSize();
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
