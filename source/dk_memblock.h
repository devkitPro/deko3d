#pragma once
#include "dk_private.h"

class tag_DkMemBlock : public DkObjBase
{
	mutable NvMap m_mapObj;
	uint32_t m_flags;
	uint32_t m_codeSegOffset;
	void* m_ownedMem;
	DkGpuAddr m_gpuAddrPitch;
	DkGpuAddr m_gpuAddrSwizzled;
	DkGpuAddr m_gpuAddrCompressed;

public:
	constexpr tag_DkMemBlock(DkDevice dev) noexcept : DkObjBase{dev},
		m_mapObj{}, m_flags{}, m_codeSegOffset{}, m_ownedMem{},
		m_gpuAddrPitch{DK_GPU_ADDR_INVALID},
		m_gpuAddrSwizzled{DK_GPU_ADDR_INVALID},
		m_gpuAddrCompressed{DK_GPU_ADDR_INVALID} { }
	~tag_DkMemBlock() { destroy(); }

	DkResult initialize(uint32_t flags, void* storage, uint32_t size) noexcept;
	void destroy();

	// Flag traits
	constexpr uint32_t getCpuAccess() const noexcept { return (m_flags >> DkMemBlockFlags_CpuAccessShift) & DkMemAccess_Mask; }
	constexpr uint32_t getGpuAccess() const noexcept { return (m_flags >> DkMemBlockFlags_GpuAccessShift) & DkMemAccess_Mask; }
	constexpr bool isCpuNoAccess() const noexcept { return getCpuAccess() == DkMemAccess_None; }
	constexpr bool isCpuUncached() const noexcept { return getCpuAccess() == DkMemAccess_Uncached; }
	constexpr bool isCpuCached() const noexcept { return getCpuAccess() == DkMemAccess_Cached; }
	constexpr bool isGpuNoAccess() const noexcept { return getGpuAccess() == DkMemAccess_None; }
	constexpr bool isGpuUncached() const noexcept { return getGpuAccess() == DkMemAccess_Uncached; }
	constexpr bool isGpuCached() const noexcept { return getGpuAccess() == DkMemAccess_Cached; }
	constexpr bool isCode() const noexcept { return (m_flags & DkMemBlockFlags_Code) != 0; }
	constexpr bool isImage() const noexcept { return (m_flags & DkMemBlockFlags_Image) != 0; }

	uint32_t getHandle() const noexcept { return m_mapObj.handle; }
	uint32_t getSize() const noexcept { return m_mapObj.size; }
	void* getCpuAddr() const noexcept { return isCpuNoAccess() ? nullptr : nvMapGetCpuAddr(&m_mapObj); }
	uint32_t getCodeSegOffset() const noexcept { return isCode() ? m_codeSegOffset : ~0U; }
	DkGpuAddr getGpuAddrPitch() const noexcept { return m_gpuAddrPitch; }
	DkGpuAddr getGpuAddrSwizzled() const noexcept { return m_gpuAddrSwizzled; }
	DkGpuAddr getGpuAddrCompressed() const noexcept { return m_gpuAddrCompressed; }
};
