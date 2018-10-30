#pragma once
#include "dk_private.h"

class tag_DkMemBlock : public DkObjBase
{
	uint32_t m_flags;
	uint32_t m_size;
public:
	constexpr tag_DkMemBlock(DkDevice dev, uint32_t size) noexcept : DkObjBase{dev}, m_flags{}, m_size{size} { }
	DkResult initialize(uint32_t flags, void* storage) noexcept;

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
};
