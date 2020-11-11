#include <stdarg.h>
#include "dk_device.h"

using namespace dk::detail;

#ifdef DEBUG
extern "C" u32 __nx_applet_exit_mode;
#endif

namespace
{
#ifdef DEBUG
	thread_local struct {
		DkDeviceMaker const* maker;
		const char* funcname;
	} g_debugContext;

	void defaultDebugFunc(void* userdata, const char* context, DkResult result, const char* message)
	{
		if (result != DkResult_Success)
		{
			fprintf(stderr, "{FATAL} deko3d error (%d) in %s - %s\n", result, context, message);

			ErrorApplicationConfig c;
			errorApplicationCreate(&c, context, message);
			errorApplicationSetNumber(&c, result);
			errorApplicationShow(&c);

			__nx_applet_exit_mode = 1;
			exit(1);
		}
		else
			fprintf(stderr, "deko3d warning in %s: %s\n", context, message);
	}
#endif

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

DkResult Device::initialize()
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
	res = m_semaphoreMem.initialize(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuUncached | DkMemBlockFlags_ZeroFillInit,
		nullptr, s_numQueues*sizeof(NvLongSemaphore));
	if (res != DkResult_Success)
		return res;

	// TODO: Set up built-in shaders, if we ever decide to have them?
	// TODO: Get zbc active slot mask

	return DkResult_Success;
}

Device::~Device()
{
	if (!m_didLibInit) return;

#ifdef DEBUG
	for (uint32_t i = 0; i < s_numQueues; i ++)
		if (m_queueTable[i])
			DK_ERROR(DkResult_BadState, "unfreed queues");
#endif

	m_semaphoreMem.destroy(); // must do this before NvLib is wound down
	m_codeSeg.cleanup();

#ifdef DEBUG
	// Ensure there are no outstanding unfreed memory blocks
	if (m_nvMapCount != 0)
		DK_ERROR(DkResult_BadState, "unfreed memory blocks");
#endif

	nvAddressSpaceClose(&m_addrSpace); // does nothing if uninitialized
	nvLibExit();
}

int32_t Device::reserveQueueId()
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

void Device::returnQueueId(uint32_t id)
{
	MutexHolder m{m_queueTableMutex};
	m_queueTable[id] = nullptr;
	m_usedQueues[id/32] &= ~(1U << (id & 0x3F));
}

void* Device::allocMem(size_t size, size_t alignment) const noexcept
{
	void* ptr = nullptr;
	DkResult res = m_maker.cbAlloc(m_maker.userData, alignment, size, &ptr);
	if (res != DkResult_Success)
		DK_DEVICE_ERROR(m_maker, g_debugContext.funcname, res, "allocation callback failed");
	return ptr;
}

void Device::freeMem(void* mem) const noexcept
{
	m_maker.cbFree(m_maker.userData, mem);
}

DkDevice dkDeviceCreate(DkDeviceMaker const* maker)
{
	DkDeviceMaker m = *maker;
#ifdef DEBUG
	if (!m.cbDebug) m.cbDebug = defaultDebugFunc;
#endif
	if (!m.cbAlloc) m.cbAlloc = defaultAllocFunc;
	if (!m.cbFree)  m.cbFree  = defaultFreeFunc;

	DkDevice dev = new(m) Device(m);
	DkResult res = dev->initialize();
	if (res != DkResult_Success)
	{
		delete dev;
		DK_DEVICE_ERROR(m, "dkDeviceCreate", res, "initialization failure");
		return nullptr;
	}
	return dev;
}

void dkDeviceDestroy(DkDevice obj)
{
	DK_ENTRYPOINT(obj);
	delete obj;
}

void* ObjBase::operator new(size_t size, DkDevice device)
{
	return device->allocMem(size);
}

void ObjBase::operator delete(void* ptr)
{
	static_cast<ObjBase*>(ptr)->freeMem(ptr);
}

#ifdef DEBUG

void dk::detail::SetContextForDebug(Device const* dev, const char* funcname)
{
	g_debugContext.maker = &dev->getMaker();
	g_debugContext.funcname = funcname;
}

void dk::detail::RaiseError(DkResult result, const char* message)
{
	InvokeDebugCallback(*g_debugContext.maker, g_debugContext.funcname, result, message);
	// If above returns, just fall back on the default debug callback
	defaultDebugFunc(nullptr, g_debugContext.funcname, result, message);
	__builtin_trap();
}

void dk::detail::WarningMsg(const char* fmt, ...)
{
	char message[1024];

	va_list va;
	va_start(va, fmt);
	vsnprintf(message, sizeof(message), fmt, va);
	va_end(va);

	InvokeDebugCallback(*g_debugContext.maker, g_debugContext.funcname, DkResult_Success, message);
}

#else

void dk::detail::RaiseError(DkResult result)
{
	diagAbortWithResult(MAKERESULT(359, result));
}

#endif

void* ObjBase::allocMem(size_t size, size_t alignment) const noexcept
{
	return m_device->allocMem(size, alignment);
}

void ObjBase::freeMem(void* mem) const noexcept
{
	return m_device->freeMem(mem);
}
