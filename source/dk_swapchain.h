#pragma once
#include "dk_private.h"

namespace dk::detail
{

class Swapchain : public ObjBase
{
	NWindow* m_nwin;
	DkImage const** m_images;
	uint32_t m_numImages;
public:
	constexpr Swapchain(DkDevice dev) noexcept : ObjBase{dev},
		m_nwin{}, m_images{(DkImage const**)(void*)(this+1)}, m_numImages{}
	{ }
	~Swapchain();

	uint32_t getNumImages() const noexcept { return m_numImages; }
	DkImage const* getImage(unsigned i) const noexcept { return m_images[i]; }

	DkResult initialize(void* nativeWindow, DkImage const* const images[], uint32_t numImages);
	void acquireImage(int& imageSlot, DkFence& fence);
	void presentImage(int imageSlot, DkFence const& fence);
	void setCrop(int32_t left, int32_t top, int32_t right, int32_t bottom);
	void setSwapInterval(uint32_t interval);
};

}
