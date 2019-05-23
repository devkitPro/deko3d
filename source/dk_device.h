#pragma once
#include "dk_private.h"

struct DkGpuInfo
{
	u32 bigPageSize;
	u32 zcullCtxSize;
};

struct NvLongSemaphore
{
	u32 sequence;
	u32 padding;
	u64 timestamp;
};

class tag_DkDevice
{
	static constexpr unsigned s_numQueues = DK_MEMBLOCK_ALIGNMENT / sizeof(NvLongSemaphore);
	static constexpr unsigned s_usedQueueBitmapSize = (s_numQueues + 31) >> 5;

	DkDeviceMaker m_maker;
	mutable NvAddressSpace m_addrSpace;
	DkGpuInfo m_gpuInfo;
	bool m_didLibInit;

	Mutex m_queueTableMutex;
	DkQueue m_queueTable[s_numQueues];
	uint32_t m_usedQueues[s_usedQueueBitmapSize];

public:

	constexpr tag_DkDevice(DkDeviceMaker const& m) noexcept :
		m_maker{m}, m_addrSpace{}, m_gpuInfo{}, m_didLibInit{},
		m_queueTableMutex{}, m_queueTable{}, m_usedQueues{} { }
	constexpr DkDeviceMaker const& getMaker() const noexcept { return m_maker; }
	constexpr NvAddressSpace *getAddrSpace() const noexcept { return &m_addrSpace; }
	constexpr DkGpuInfo const& getGpuInfo() const noexcept { return m_gpuInfo; }

	DkResult initialize() noexcept;
	~tag_DkDevice();

	int32_t reserveQueueId() noexcept;
	void returnQueueId(uint32_t id) noexcept;
	void registerQueue(uint32_t id, DkQueue queue) noexcept
	{
		m_queueTable[id] = queue;
	}

	void raiseError(const char* context, DkResult result) const
	{
		m_maker.cbError(m_maker.userData, context, result);
	}

	void* allocMem(size_t size, size_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__) const noexcept
	{
		void* ptr = nullptr;
		DkResult res = m_maker.cbAlloc(m_maker.userData, alignment, size, &ptr);
		if (res != DkResult_Success)
			m_maker.cbError(m_maker.userData, DK_FUNC_ERROR_CONTEXT, res);
		return ptr;
	}

	void freeMem(void* mem) const noexcept
	{
		m_maker.cbFree(m_maker.userData, mem);
	}

	void* operator new(size_t size, DkDeviceMaker const& m) noexcept
	{
		void* ptr = nullptr;
		DkResult res = m.cbAlloc(m.userData, __STDCPP_DEFAULT_NEW_ALIGNMENT__, size, &ptr);
		if (res != DkResult_Success)
			m.cbError(m.userData, DK_FUNC_ERROR_CONTEXT, res);
		return ptr;
	};

	void operator delete(void* ptr) noexcept
	{
		static_cast<DkDevice>(ptr)->freeMem(ptr);
	}
};
