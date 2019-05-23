#include "dk_device.h"

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
	m_gpuInfo.bigPageSize = gpuChars->big_page_size;
	m_gpuInfo.zcullCtxSize = nvGpuGetZcullCtxSize();

	if (R_FAILED(nvAddressSpaceCreate(&m_addrSpace, m_gpuInfo.bigPageSize)))
		return DkResult_Fail;

	// TODO: Create address space region for code segment

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

	nvAddressSpaceClose(&m_addrSpace); // does nothing if uninitialized
	nvLibExit();
}

int32_t tag_DkDevice::reserveQueueId()
{
	int32_t id = -1;
	mutexLock(&m_queueTableMutex);
	for (uint32_t i = 0; i < s_usedQueueBitmapSize; i ++)
	{
		uint32_t mask = m_usedQueues[i];
		int32_t bit = __builtin_ffs(~mask)-1;
		if (bit >= 0)
		{
			m_usedQueues[i] |= 1U << bit;
			id = 32*i + bit;
			break;
		}
	}
	mutexUnlock(&m_queueTableMutex);
	return id;
}

void tag_DkDevice::returnQueueId(uint32_t id)
{
	mutexLock(&m_queueTableMutex);
	m_queueTable[id] = nullptr;
	m_usedQueues[id/32] &= ~(1U << (id & 0x3F));
	mutexUnlock(&m_queueTableMutex);
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

void* DkObjBase::operator new(size_t size, DkDevice device)
{
	return tag_DkDevice::operator new(size, device->getMaker());
}

void DkObjBase::operator delete(void* ptr)
{
	static_cast<DkObjBase*>(ptr)->freeMem(ptr);
}

void DkObjBase::raiseError(DkDevice device, const char* context, DkResult result)
{
	device->raiseError(context, result);
}

void* DkObjBase::allocMem(size_t size, size_t alignment) const noexcept
{
	return m_device->allocMem(size, alignment);
}

void DkObjBase::freeMem(void* mem) const noexcept
{
	return m_device->freeMem(mem);
}
