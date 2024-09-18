#pragma once
#include "dk_private.h"
#include "dk_memblock.h"
#include "codesegmgr.h"

#ifdef DEBUG
#define DK_DEVICE_ERROR(_m, _ctx, _res, _msg) \
	::dk::detail::InvokeDebugCallback((_m), (_ctx), (_res), (_msg))
#else
#define DK_DEVICE_ERROR(_m, _ctx, _res, _msg) \
	::dk::detail::RaiseError(_res)
#endif

namespace dk::detail
{

#ifdef DEBUG

NX_INLINE void InvokeDebugCallback(DkDeviceMaker const& m, const char* context, DkResult res, const char* message)
{
	m.cbDebug(m.userData, context, res, message);
}

#endif

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

class Device
{
	static constexpr unsigned s_numQueues = DK_MEMBLOCK_ALIGNMENT / sizeof(NvLongSemaphore);
	static constexpr unsigned s_usedQueueBitmapSize = (s_numQueues + 31) >> 5;

	DkDeviceMaker m_maker;
	mutable NvAddressSpace m_addrSpace;
	GpuInfo m_gpuInfo;
	bool m_didLibInit;

#ifdef DEBUG
	uint32_t m_nvMapCount;
#endif

	Mutex m_queueTableMutex;
	DkQueue m_queueTable[s_numQueues];
	uint32_t m_usedQueues[s_usedQueueBitmapSize];

	MemBlock m_semaphoreMem;
	uint32_t m_semaphores[s_numQueues];

	CodeSegMgr m_codeSeg;

public:

	constexpr Device(DkDeviceMaker const& m) noexcept :
		m_maker{m}, m_addrSpace{}, m_gpuInfo{}, m_didLibInit{},
#ifdef DEBUG
		m_nvMapCount{},
#endif
		m_queueTableMutex{}, m_queueTable{}, m_usedQueues{},
		m_semaphoreMem{this}, m_semaphores{},
		m_codeSeg{this} { }
	constexpr DkDeviceMaker const& getMaker() const noexcept { return m_maker; }
	constexpr NvAddressSpace *getAddrSpace() const noexcept { return &m_addrSpace; }
	constexpr CodeSegMgr &getCodeSeg() noexcept { return m_codeSeg; }
	constexpr GpuInfo const& getGpuInfo() const noexcept { return m_gpuInfo; }

	bool isDepthModeOpenGL() const noexcept { return (m_maker.flags & DkDeviceFlags_DepthMinusOneToOne) != 0; }
	bool isOriginLowerLeft() const noexcept { return (m_maker.flags & DkDeviceFlags_OriginLowerLeft) != 0; }
	bool isYAxisPointsDown() const noexcept { return (m_maker.flags & DkDeviceFlags_YAxisPointsDown) != 0; }

	// Rasterizer uses framebuffer coordinates (with +Y = down) to calculate winding direction.
	// In LowerLeft mode images are stored upside down, so triangle winding must be flipped.
	bool windingFlip() const noexcept { return isOriginLowerLeft(); }

	// Rasterizer expects clip space +Y to point down. In UpperLeft mode with Y pointing up
	// (or LowerLeft mode with Y pointing down) we need to flip the incoming Y coordinate.
	bool viewportFlipY() const noexcept { return !isYAxisPointsDown() ^ !isOriginLowerLeft(); }

	DkResult initialize() noexcept;
	~Device();

	int32_t reserveQueueId() noexcept;
	void returnQueueId(uint32_t id) noexcept;
	void registerQueue(uint32_t id, DkQueue queue) noexcept
	{
		m_queueTable[id] = queue;
	}

	NvLongSemaphore volatile* getSemaphoreCpuAddr(uint32_t id) noexcept
	{
		return (NvLongSemaphore volatile*)m_semaphoreMem.getCpuAddr() + id;
	}

	DkGpuAddr getSemaphoreGpuAddr(uint32_t id) noexcept
	{
		return m_semaphoreMem.getGpuAddrPitch() + id*sizeof(NvLongSemaphore);
	}

	uint32_t getSemaphoreValue(uint32_t id) const noexcept
	{
		return m_semaphores[id];
	}

	uint32_t incrSemaphoreValue(uint32_t id) noexcept
	{
		return ++m_semaphores[id];
	}

	void checkQueueErrors() noexcept;
	void calcZcullStorageInfo(ZcullStorageInfo& out, uint32_t width, uint32_t height, uint32_t depth, DkImageFormat format, DkMsMode msMode);

	void incrNvMapCount() noexcept
	{
#ifdef DEBUG
		__atomic_add_fetch(&m_nvMapCount, 1, __ATOMIC_SEQ_CST);
#endif
	}

	void decrNvMapCount() noexcept
	{
#ifdef DEBUG
		__atomic_sub_fetch(&m_nvMapCount, 1, __ATOMIC_SEQ_CST);
#endif
	}

	void* allocMem(size_t size, size_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__) const noexcept;
	void freeMem(void* mem) const noexcept;

	void* operator new(size_t size, DkDeviceMaker const& m) noexcept
	{
		void* ptr = nullptr;
		DkResult res = m.cbAlloc(m.userData, __STDCPP_DEFAULT_NEW_ALIGNMENT__, size, &ptr);
		if (res != DkResult_Success)
			DK_DEVICE_ERROR(m, "dkDeviceCreate", res, "out of memory for device");
		return ptr;
	};

	void operator delete(void* ptr) noexcept
	{
		static_cast<DkDevice>(ptr)->freeMem(ptr);
	}
};

}
