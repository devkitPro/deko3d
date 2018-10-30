#pragma once
#include <tuple>
#include "deko3d.h"

namespace dk
{
	namespace detail
	{
		template <typename T>
		class Handle
		{
			T m_handle;
		protected:
			void _clear() noexcept { m_handle = nullptr; }
		public:
			using Type = T;
			constexpr Handle() noexcept : m_handle{} { }
			constexpr Handle(std::nullptr_t) noexcept : m_handle{} { }
			constexpr Handle(T handle) noexcept : m_handle{handle} { }
			constexpr operator T() const noexcept { return m_handle; }
			constexpr operator bool() const noexcept { return m_handle != nullptr; }
			constexpr bool operator !() const noexcept { return m_handle == nullptr; }
		};

		template <typename T>
		struct UniqueHandle final : public T
		{
			UniqueHandle() = delete;
			UniqueHandle(UniqueHandle&) = delete;
			constexpr UniqueHandle(T&& rhs) noexcept : T{rhs} { rhs = nullptr; }
			~UniqueHandle()
			{
				if (*this) T::destroy();
			}
		};
	}

#define DK_OPAQUE_COMMON_MEMBERS(_name) \
	constexpr _name() noexcept : Handle{} { } \
	constexpr _name(std::nullptr_t arg) noexcept : Handle{arg} { } \
	constexpr _name(::Dk##_name arg) noexcept : Handle{arg} { } \
	_name& operator=(std::nullptr_t) noexcept { _clear(); return *this; } \
	void destroy()

	struct Device : public detail::Handle<::DkDevice>
	{
		DK_OPAQUE_COMMON_MEMBERS(Device);
	};

	struct DeviceMaker : public ::DkDeviceMaker
	{
		DeviceMaker() noexcept : DkDeviceMaker{} { ::dkDeviceMakerDefaults(this); }
		DeviceMaker(DeviceMaker&) = default;
		DeviceMaker(DeviceMaker&&) = default;
		DeviceMaker& setUserData(void* userData) noexcept { this->userData = userData; return *this; }
		DeviceMaker& setFlags(uint32_t flags) noexcept { this->flags = flags; return *this; }
		Device create();
	};

	using UniqueDevice = detail::UniqueHandle<Device>;

	inline Device DeviceMaker::create()
	{
		return Device{::dkDeviceCreate(this)};
	}

	inline void Device::destroy()
	{
		::dkDeviceDestroy(*this);
	}

}
