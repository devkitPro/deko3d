#pragma once
#include "dk_private.h"

namespace dk::detail
{

class MemBlock : public ObjBase
{
	mutable NvMap m_mapObj;
	uint32_t m_flags;
	uint32_t m_codeSegOffset;
	void* m_ownedMem;
	DkGpuAddr m_gpuAddrPitch;
	DkGpuAddr m_gpuAddrGeneric;
	DkGpuAddr m_gpuAddrCompressed;

public:
	constexpr MemBlock(DkDevice dev) noexcept : ObjBase{dev},
		m_mapObj{}, m_flags{}, m_codeSegOffset{}, m_ownedMem{},
		m_gpuAddrPitch{DK_GPU_ADDR_INVALID},
		m_gpuAddrGeneric{DK_GPU_ADDR_INVALID},
		m_gpuAddrCompressed{DK_GPU_ADDR_INVALID} { }
	~MemBlock() { destroy(); }

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

	uint32_t getHandle() const noexcept { return nvMapGetHandle(&m_mapObj); }
	uint32_t getId() const noexcept { return nvMapGetId(&m_mapObj); }
	uint32_t getSize() const noexcept { return nvMapGetSize(&m_mapObj); }
	void* getCpuAddr() const noexcept { return isCpuNoAccess() ? nullptr : nvMapGetCpuAddr(&m_mapObj); }
	uint32_t getCodeSegOffset() const noexcept { return isCode() ? m_codeSegOffset : ~0U; }
	DkGpuAddr getGpuAddrPitch() const noexcept { return m_gpuAddrPitch; }
	DkGpuAddr getGpuAddrGeneric() const noexcept { return m_gpuAddrGeneric; }
	DkGpuAddr getGpuAddrCompressed() const noexcept { return m_gpuAddrCompressed; }
	DkGpuAddr getGpuAddrForImage(uint32_t offset, uint32_t size, NvKind kind) noexcept;
};

}
