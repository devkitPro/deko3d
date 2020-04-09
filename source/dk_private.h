#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <deko3d.h>

#define DK_WEAK __attribute__((weak))

#define DK_OPAQUE_CHECK(_typename) \
	static_assert(::dk::detail::_size_##_typename >= sizeof(::dk::detail::_typename), "Invalid size"); \
	static_assert(::dk::detail::_align_##_typename >= alignof(::dk::detail::_typename), "Invalid alignment")

#ifdef DEBUG

#define DK_ENTRYPOINT(_obj) ::dk::detail::SetContextForDebug((_obj), __FUNCTION__)
#define DK_ERROR(_res,_msg) ::dk::detail::RaiseError((_res),(_msg))
#define DK_WARNING(...)    ::dk::detail::WarningMsg(__VA_ARGS__)

#define DK_STRINGIFY(_x) DK_STRINGIFY_(_x)
#define DK_STRINGIFY_(_x) #_x
#define DK_FILELINE __FILE__ ":" DK_STRINGIFY(__LINE__) ": "

#define DK_MACRO_DEFAULT_IMPL(_x,_a,...) _a
#define DK_MACRO_DEFAULT(_def,...) DK_MACRO_DEFAULT_IMPL(,##__VA_ARGS__,_def)

#define DK_ENTRYPOINT_NOT_IMPLEMENTED(_obj) ({ \
	DK_ENTRYPOINT(_obj); \
	DK_ERROR(DkResult_NotImplemented, "function not implemented"); \
})

#define DK_DEBUG_NON_NULL(_ptr) ({ \
	if (!(_ptr)) DK_ERROR(DkResult_BadInput, DK_FILELINE #_ptr " must not be null"); \
})

#define DK_DEBUG_NON_ZERO(_cnt) ({ \
	if (!(_cnt)) DK_ERROR(DkResult_BadInput, DK_FILELINE #_cnt " must not be zero"); \
})

#define DK_DEBUG_NON_NULL_ARRAY(_ptr,_cnt) ({ \
	if ((_cnt) && !(_ptr)) DK_ERROR(DkResult_BadInput, DK_FILELINE #_ptr " must not be null"); \
})

#define DK_DEBUG_BAD_INPUT(_expr,...) ({ \
	if (_expr) DK_ERROR(DkResult_BadInput, DK_FILELINE DK_MACRO_DEFAULT("bad input: " #_expr,##__VA_ARGS__)); \
})

#define DK_DEBUG_BAD_FLAGS(_expr,...) ({ \
	if (_expr) DK_ERROR(DkResult_BadFlags, DK_FILELINE DK_MACRO_DEFAULT("bad flags: " #_expr,##__VA_ARGS__)); \
})

#define DK_DEBUG_DATA_ALIGN(_data,_align) ({ \
	if (::dk::detail::IsMisaligned((_data),(_align))) \
		DK_ERROR(DkResult_MisalignedData, DK_FILELINE "misaligned data: " #_data " (" #_align ")"); \
})

#define DK_DEBUG_SIZE_ALIGN(_data,_align) ({ \
	if (::dk::detail::IsMisaligned((_data),(_align))) \
		DK_ERROR(DkResult_MisalignedSize, DK_FILELINE "misaligned size: " #_data " (" #_align ")"); \
})

#define DK_DEBUG_CHECK(_expr) ({ \
	DkResult res = (_expr); \
	if (res != DkResult_Success) \
		DK_ERROR(res, DK_FILELINE #_expr); \
})

#else

#define DK_ENTRYPOINT(...) ((void)0)
#define DK_ERROR(_res,_msg) ::dk::detail::RaiseError(_res)
#define DK_WARNING(...)    ((void)0)

#define DK_ENTRYPOINT_NOT_IMPLEMENTED(...) \
	::dk::detail::RaiseError(DkResult_NotImplemented)

#define DK_DEBUG_NON_NULL(...) ((void)0)
#define DK_DEBUG_NON_ZERO(...) ((void)0)
#define DK_DEBUG_NON_NULL_ARRAY(...) ((void)0)
#define DK_DEBUG_BAD_INPUT(...) ((void)0)
#define DK_DEBUG_BAD_FLAGS(...) ((void)0)
#define DK_DEBUG_DATA_ALIGN(...) ((void)0)
#define DK_DEBUG_SIZE_ALIGN(...) ((void)0)
#define DK_DEBUG_CHECK(...) ((void)0)

#endif

namespace dk::detail
{

class MutexHolder
{
	Mutex& m_mutex;
public:
	MutexHolder(Mutex& mutex) noexcept : m_mutex{mutex}
	{
		mutexLock(&m_mutex);
	}
	~MutexHolder()
	{
		mutexUnlock(&m_mutex);
	}
};

class ObjBase
{
	DkDevice m_device;

	void* operator new(size_t size); // private, so that it can't be used
public:
	constexpr ObjBase(DkDevice device) noexcept : m_device{device} { }
	constexpr DkDevice getDevice() const noexcept { return m_device; }

	void* operator new(size_t size, DkDevice device) noexcept;
	void operator delete(void* ptr) noexcept;

	void* operator new(size_t size, DkDevice device, size_t extraSize) noexcept
	{
		size = (size + __STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1) &~ (__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1);
		return operator new(size+extraSize, device);
	}

	void* allocMem(size_t size, size_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__) const noexcept;
	void freeMem(void* ptr) const noexcept;
};

#ifdef DEBUG

void SetContextForDebug(Device const* dev, const char* funcname);

NX_INLINE void SetContextForDebug(ObjBase const* obj, const char* funcname)
{
	SetContextForDebug(obj->getDevice(), funcname);
}

[[noreturn]] void RaiseError(DkResult result, const char* message);
void WarningMsg(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

NX_CONSTEXPR bool IsMisaligned(const void* ptr, uintptr_t align)
{
	return reinterpret_cast<uintptr_t>(ptr) & (align - 1);
}

NX_CONSTEXPR bool IsMisaligned(uint64_t data, uint64_t align)
{
	return data & (align - 1);
}

NX_CONSTEXPR bool IsMisaligned(uint32_t data, uint32_t align)
{
	return data & (align - 1);
}

#else

[[noreturn]] void RaiseError(DkResult result);

#endif

}
