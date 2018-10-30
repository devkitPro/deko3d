#pragma once
#include "dk_private.h"

class tag_DkDevice
{
	DkDeviceMaker m_maker;
public:

	constexpr tag_DkDevice(DkDeviceMaker const& m) noexcept : m_maker{m} { }
	constexpr DkDeviceMaker const& getMaker() const noexcept { return m_maker; }

	void* operator new(size_t size, DkDeviceMaker const& m) noexcept
	{
		void* ptr = nullptr;
		DkResult res = m.cbAlloc(m.userData, __STDCPP_DEFAULT_NEW_ALIGNMENT__, size, &ptr);
		if (res != DkResult_Success)
			m.cbError(m.userData, res);
		return ptr;
	};

	void operator delete(void* ptr) noexcept
	{
		DkDeviceMaker const& m = static_cast<DkDevice>(ptr)->getMaker();
		m.cbFree(m.userData, ptr);
	}
};
