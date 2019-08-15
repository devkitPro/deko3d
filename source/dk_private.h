#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <deko3d.h>

#ifdef DEBUG
#define DK_ERROR_CONTEXT(_ctx) (_ctx)
#else
#define DK_ERROR_CONTEXT(_ctx) nullptr
#endif

#define DK_FUNC_ERROR_CONTEXT DK_ERROR_CONTEXT(__func__)

#define DK_OPAQUE_CHECK(_typename) \
	static_assert(_size_##_typename >= sizeof(_typename), "Invalid size"); \
	static_assert(_align_##_typename >= alignof(_typename), "Invalid alignment")

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

	static void raiseError(DkDevice device, const char* context, DkResult result);
	void raiseError(const char* context, DkResult result) const
	{
		raiseError(m_device, context, result);
	}

	void* allocMem(size_t size, size_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__) const noexcept;
	void freeMem(void* ptr) const noexcept;
};

}
