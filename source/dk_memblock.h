#pragma once
#include "dk_private.h"
#include "nv/map.h"

class tag_DkMemBlock : public DkObjBase
{
	mutable NvMap m_mapObj;
	uint32_t m_flags;
	void* m_ownedMem;

public:
	constexpr tag_DkMemBlock(DkDevice dev) noexcept : DkObjBase{dev}, m_mapObj{}, m_flags{}, m_ownedMem{} { }

	DkResult initialize(uint32_t flags, void* storage, uint32_t size) noexcept;
	~tag_DkMemBlock();

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
	constexpr bool isImage() const noexcept { return (m_flags & DkMemBlockFlags_Code) != 0; }

	uint32_t getHandle() const noexcept { return m_mapObj.handle; }
	uint32_t getSize() const noexcept { return m_mapObj.size; }
	void* getCpuAddr() const noexcept { return isCpuNoAccess() ? nullptr : nvMapGetCpuAddr(&m_mapObj); }
};
