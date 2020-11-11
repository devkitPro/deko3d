#include "dk_memblock.h"
#include "dk_device.h"

using namespace dk::detail;

DkResult MemBlock::initialize(uint32_t flags, void* storage, uint32_t size)
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

	// Validate access bits for image memory
	if (flags & DkMemBlockFlags_Image)
	{
		if (gpuAccess == DkMemAccess_None)
			return DkResult_BadInput;
	}

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

	getDevice()->incrNvMapCount();

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

		// Create extra mappings for DkMemBlockFlags_Image
		if (flags & DkMemBlockFlags_Image)
		{
			if (R_FAILED(nvAddressSpaceMap(getDevice()->getAddrSpace(),
				getHandle(), isGpuCached(), NvKind_Generic_16BX2, &m_gpuAddrGeneric)))
				return DkResult_Fail;

			if (R_FAILED(nvAddressSpaceMap(getDevice()->getAddrSpace(),
				getHandle(), isGpuCached(), NvKind_C32_2CRA, &m_gpuAddrCompressed)))
				return DkResult_Fail;
		}
	}

	return DkResult_Success;
}

void MemBlock::destroy()
{
	if (m_gpuAddrCompressed != DK_GPU_ADDR_INVALID)
	{
		nvAddressSpaceUnmap(getDevice()->getAddrSpace(), m_gpuAddrCompressed);
		m_gpuAddrCompressed = DK_GPU_ADDR_INVALID;
	}

	if (m_gpuAddrGeneric != DK_GPU_ADDR_INVALID)
	{
		nvAddressSpaceUnmap(getDevice()->getAddrSpace(), m_gpuAddrGeneric);
		m_gpuAddrGeneric = DK_GPU_ADDR_INVALID;
	}

	if (m_gpuAddrPitch != DK_GPU_ADDR_INVALID)
	{
		nvAddressSpaceUnmap(getDevice()->getAddrSpace(), m_gpuAddrPitch);
		if (isCode())
			getDevice()->getCodeSeg().freeSpace(m_gpuAddrPitch, getSize());
		m_gpuAddrPitch = DK_GPU_ADDR_INVALID;
	}

	if (m_mapObj.has_init)
	{
		nvMapClose(&m_mapObj);
		getDevice()->decrNvMapCount();
	}

	if (m_ownedMem)
	{
		freeMem(m_ownedMem);
		m_ownedMem = nullptr;
	}
}

DkGpuAddr MemBlock::getGpuAddrForImage(uint32_t offset, uint32_t size, NvKind kind) noexcept
{
	if (kind == NvKind_Pitch)
		return m_gpuAddrPitch + offset;
	if (kind == NvKind_Generic_16BX2)
		return m_gpuAddrGeneric + offset;
	if (R_FAILED(nvAddressSpaceModify(getDevice()->getAddrSpace(),
		m_gpuAddrCompressed, offset, size, kind)))
		DK_ERROR(DkResult_Fail, "failed to remap memory block for image usage");
	return m_gpuAddrCompressed + offset;
}

DkMemBlock dkMemBlockCreate(DkMemBlockMaker const* maker)
{
	DK_ENTRYPOINT(maker->device);
	DK_DEBUG_SIZE_ALIGN(maker->size, DK_MEMBLOCK_ALIGNMENT);
	DK_DEBUG_DATA_ALIGN(maker->storage, DK_MEMBLOCK_ALIGNMENT);

	DkMemBlock obj = new(maker->device) MemBlock(maker->device);
	DkResult res = obj->initialize(maker->flags, maker->storage, maker->size);
	if (res != DkResult_Success)
	{
		delete obj;
		DK_ERROR(res, "initialization failure");
		return nullptr;
	}
	return obj;
}

void dkMemBlockDestroy(DkMemBlock obj)
{
	DK_ENTRYPOINT(obj);
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
	DK_ENTRYPOINT(obj);
	DK_DEBUG_BAD_INPUT(offset >= obj->getSize() || size > obj->getSize() || offset + size > obj->getSize(), "memory range out of bounds");

	if (obj->isCpuCached() && size)
		armDCacheFlush((uint8_t*)obj->getCpuAddr() + offset, size);

	return DkResult_Success;
}
