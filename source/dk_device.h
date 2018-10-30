#pragma once
#include "dk_private.h"

class tag_DkDevice
{
	DkDeviceMaker m_maker;
	mutable NvAddressSpace m_addrSpace;
	bool m_didLibInit;
public:

	constexpr tag_DkDevice(DkDeviceMaker const& m) noexcept : m_maker{m}, m_addrSpace{}, m_didLibInit{} { }
	constexpr DkDeviceMaker const& getMaker() const noexcept { return m_maker; }
	constexpr NvAddressSpace *getAddrSpace() const noexcept { return &m_addrSpace; }

	DkResult initialize() noexcept;
	~tag_DkDevice();

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
