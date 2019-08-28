#pragma once
#include "dk_private.h"
#include "dk_memblock.h"
#include "codesegmgr.h"

namespace dk::detail
{

struct GpuInfo
{
	u32 numWarpsPerSm;
	u32 numSms;
	u32 bigPageSize;
	u32 zcullCtxSize;
	u32 zcullCtxAlign;
	const nvioctl_zcull_info* zcullInfo;
};

struct NvLongSemaphore
{
	u32 sequence;
	u32 padding;
	u64 timestamp;
};

struct ZcullStorageInfo
{
	u32 width;
	u32 height;
	u32 depth;
	u32 imageSize;
	u32 layerSize;
	u32 zetaType;
	u32 totalSize;
};

}

class tag_DkDevice
{
	static constexpr unsigned s_numQueues = DK_MEMBLOCK_ALIGNMENT / sizeof(dk::detail::NvLongSemaphore);
	static constexpr unsigned s_usedQueueBitmapSize = (s_numQueues + 31) >> 5;

	DkDeviceMaker m_maker;
	mutable NvAddressSpace m_addrSpace;
	dk::detail::GpuInfo m_gpuInfo;
	bool m_didLibInit;

	Mutex m_queueTableMutex;
	DkQueue m_queueTable[s_numQueues];
	uint32_t m_usedQueues[s_usedQueueBitmapSize];

	tag_DkMemBlock m_semaphoreMem;
	uint32_t m_semaphores[s_numQueues];

	dk::detail::CodeSegMgr m_codeSeg;

public:

	constexpr tag_DkDevice(DkDeviceMaker const& m) noexcept :
		m_maker{m}, m_addrSpace{}, m_gpuInfo{}, m_didLibInit{},
		m_queueTableMutex{}, m_queueTable{}, m_usedQueues{},
		m_semaphoreMem{this}, m_semaphores{},
		m_codeSeg{this} { }
	constexpr DkDeviceMaker const& getMaker() const noexcept { return m_maker; }
	constexpr NvAddressSpace *getAddrSpace() const noexcept { return &m_addrSpace; }
	constexpr dk::detail::CodeSegMgr &getCodeSeg() noexcept { return m_codeSeg; }
	constexpr dk::detail::GpuInfo const& getGpuInfo() const noexcept { return m_gpuInfo; }

	bool isDepthModeOpenGL() const noexcept { return (m_maker.flags & DkDeviceFlags_DepthMinusOneToOne) != 0; }
	bool isOriginModeOpenGL() const noexcept { return (m_maker.flags & DkDeviceFlags_OriginLowerLeft) != 0; }

	DkResult initialize() noexcept;
	~tag_DkDevice();

	int32_t reserveQueueId() noexcept;
	void returnQueueId(uint32_t id) noexcept;
	void registerQueue(uint32_t id, DkQueue queue) noexcept
	{
		m_queueTable[id] = queue;
	}

	dk::detail::NvLongSemaphore volatile* getSemaphoreCpuAddr(uint32_t id) noexcept
	{
		return (dk::detail::NvLongSemaphore volatile*)m_semaphoreMem.getCpuAddr() + id;
	}

	DkGpuAddr getSemaphoreGpuAddr(uint32_t id) noexcept
	{
		return m_semaphoreMem.getGpuAddrPitch() + id*sizeof(dk::detail::NvLongSemaphore);
	}

	uint32_t getSemaphoreValue(uint32_t id) const noexcept
	{
		return m_semaphores[id];
	}

	uint32_t incrSemaphoreValue(uint32_t id) noexcept
	{
		return ++m_semaphores[id];
	}

	void calcZcullStorageInfo(dk::detail::ZcullStorageInfo& out, uint32_t width, uint32_t height, uint32_t depth, DkImageFormat format, DkMsMode msMode);

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
