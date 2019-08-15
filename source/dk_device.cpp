#include "dk_device.h"

using namespace dk::detail;

namespace
{
	void defaultErrorFunc(void* userdata, const char* context, DkResult result)
	{
		fatalSimple(MAKERESULT(359, result));
	}

	DkResult defaultAllocFunc(void* userdata, size_t alignment, size_t size, void** out)
	{
		size = (size + alignment - 1) &~ (alignment - 1);
		*out = aligned_alloc(alignment, size);
		return *out ? DkResult_Success : DkResult_OutOfMemory;
	}

	void defaultFreeFunc(void* userdata, void* mem)
	{
		free(mem);
	}

	Result nvLibInit()
	{
		Result res;
		res = nvInitialize();
		if (R_SUCCEEDED(res))
		{
			res = nvGpuInit();
			if (R_SUCCEEDED(res))
			{
				res = nvFenceInit();
				if (R_SUCCEEDED(res))
					res = nvMapInit();
				if (R_FAILED(res))
					nvGpuExit();
			}
			if (R_FAILED(res))
				nvExit();
		}
		return res;
	}

	void nvLibExit()
	{
		nvMapExit();
		nvFenceExit();
		nvGpuExit();
		nvExit();
	}
}

DkResult tag_DkDevice::initialize()
{
	DkResult res;

	if (R_FAILED(nvLibInit()))
		return DkResult_Fail;
	m_didLibInit = true;

	auto gpuChars = nvGpuGetCharacteristics();
	m_gpuInfo.numWarpsPerSm = gpuChars->sm_arch_warp_count;
	m_gpuInfo.numSms = gpuChars->num_gpc * gpuChars->num_tpc_per_gpc * 1; // Each TPC has a single SM
	m_gpuInfo.bigPageSize = 0x10000; // Hardcoded, as per official software (otherwise this would be gpuChars->big_page_size)
	m_gpuInfo.zcullCtxSize = nvGpuGetZcullCtxSize();
	m_gpuInfo.zcullCtxAlign = 0x1000; // Hardcoded, as per official software
	m_gpuInfo.zcullInfo = nvGpuGetZcullInfo();

	// Create a GPU virtual address space (40-bit)
	if (R_FAILED(nvAddressSpaceCreate(&m_addrSpace, m_gpuInfo.bigPageSize)))
		return DkResult_Fail;

	// Create and initialize a code segment (32-bit) within the address space:
	// this is necessary because shader code resides within a configurable
	// window inside the address space, and offsets within that window are 32-bit
	res = m_codeSeg.initialize();
	if (res != DkResult_Success)
		return res;

	// Allocate memory for the semaphore queries
	res = m_semaphoreMem.initialize(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuUncached,
		nullptr, s_numQueues*sizeof(NvLongSemaphore));
	if (res != DkResult_Success)
		return res;
	memset(m_semaphoreMem.getCpuAddr(), 0, m_semaphoreMem.getSize());

	// TODO: Set up built-in shaders, if we ever decide to have them?
	// TODO: Get zbc active slot mask

	return DkResult_Success;
}

tag_DkDevice::~tag_DkDevice()
{
	if (!m_didLibInit) return;

#ifdef DEBUG
	for (uint32_t i = 0; i < s_numQueues; i ++)
		if (m_queueTable[i])
			raiseError(DK_FUNC_ERROR_CONTEXT, DkResult_BadState);
#endif

	m_semaphoreMem.destroy(); // must do this before NvLib is wound down
	m_codeSeg.cleanup();
	nvAddressSpaceClose(&m_addrSpace); // does nothing if uninitialized
	nvLibExit();
}

int32_t tag_DkDevice::reserveQueueId()
{
	MutexHolder m{m_queueTableMutex};
	for (uint32_t i = 0; i < s_usedQueueBitmapSize; i ++)
	{
		uint32_t mask = m_usedQueues[i];
		int32_t bit = __builtin_ffs(~mask)-1;
		if (bit >= 0)
		{
			m_usedQueues[i] |= 1U << bit;
			return 32*i + bit;
		}
	}
	return -1;
}

void tag_DkDevice::returnQueueId(uint32_t id)
{
	MutexHolder m{m_queueTableMutex};
	m_queueTable[id] = nullptr;
	m_usedQueues[id/32] &= ~(1U << (id & 0x3F));
}

DkDevice dkDeviceCreate(DkDeviceMaker const* maker)
{
	DkDeviceMaker m = *maker;
	if (!m.cbError) m.cbError = defaultErrorFunc;
	if (!m.cbAlloc) m.cbAlloc = defaultAllocFunc;
	if (!m.cbFree)  m.cbFree  = defaultFreeFunc;

	DkDevice dev = new(m) tag_DkDevice(m);
	if (dev)
	{
		DkResult res = dev->initialize();
		if (res != DkResult_Success)
		{
			delete dev;
			dev = nullptr;
			m.cbError(m.userData, DK_FUNC_ERROR_CONTEXT, res);
		}
	}
	return dev;
}

void dkDeviceDestroy(DkDevice obj)
{
	delete obj;
}

void* ObjBase::operator new(size_t size, DkDevice device)
{
	return tag_DkDevice::operator new(size, device->getMaker());
}

void ObjBase::operator delete(void* ptr)
{
	static_cast<ObjBase*>(ptr)->freeMem(ptr);
}

void ObjBase::raiseError(DkDevice device, const char* context, DkResult result)
{
	device->raiseError(context, result);
}

void* ObjBase::allocMem(size_t size, size_t alignment) const noexcept
{
	return m_device->allocMem(size, alignment);
}

void ObjBase::freeMem(void* mem) const noexcept
{
	return m_device->freeMem(mem);
}
