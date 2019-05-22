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

		template <typename T>
		struct Opaque : public T
		{
			constexpr Opaque() noexcept : T{} { }
		};
	}

#define DK_HANDLE_COMMON_MEMBERS(_name) \
	constexpr _name() noexcept : Handle{} { } \
	constexpr _name(std::nullptr_t arg) noexcept : Handle{arg} { } \
	constexpr _name(::Dk##_name arg) noexcept : Handle{arg} { } \
	_name& operator=(std::nullptr_t) noexcept { _clear(); return *this; } \
	void destroy()

#define DK_OPAQUE_COMMON_MEMBERS(_name) \
	constexpr _name() noexcept : Opaque{} { }

	struct Device : public detail::Handle<::DkDevice>
	{
		DK_HANDLE_COMMON_MEMBERS(Device);
	};

	struct MemBlock : public detail::Handle<::DkMemBlock>
	{
		DK_HANDLE_COMMON_MEMBERS(MemBlock);
		void* getCpuAddr();
		DkGpuAddr getGpuAddr();
		DkResult flushCpuCache(uint32_t offset, uint32_t size);
		DkResult invalidateCpuCache(uint32_t offset, uint32_t size);
	};

	struct Fence : public detail::Opaque<::DkFence>
	{
		DK_OPAQUE_COMMON_MEMBERS(Fence);
		DkResult wait(int64_t timeout_ns = -1);
	};

	struct CmdBuf : public detail::Handle<::DkCmdBuf>
	{
		DK_HANDLE_COMMON_MEMBERS(CmdBuf);
		void addMemory(DkMemBlock mem, uint32_t offset, uint32_t size);
		DkCmdList finishList();
		void waitFence(DkFence& fence);
		void signalFence(DkFence& fence, bool flush = false);
	};

	struct Queue : public detail::Handle<::DkQueue>
	{
		DK_HANDLE_COMMON_MEMBERS(Queue);
		bool isInErrorState();
		void waitFence(DkFence& fence);
		void signalFence(DkFence& fence, bool flush = false);
		void submitCommands(DkCmdList cmds);
		void flush();
		void present(DkWindow window, int imageSlot);
	};

	struct DeviceMaker : public ::DkDeviceMaker
	{
		DeviceMaker() noexcept : DkDeviceMaker{} { ::dkDeviceMakerDefaults(this); }
		DeviceMaker(DeviceMaker&) = default;
		DeviceMaker(DeviceMaker&&) = default;
		DeviceMaker& setUserData(void* userData) noexcept { this->userData = userData; return *this; }
		DeviceMaker& setCbError(DkErrorFunc cbError) noexcept { this->cbError = cbError; return *this; }
		DeviceMaker& setCbAlloc(DkAllocFunc cbAlloc) noexcept { this->cbAlloc = cbAlloc; return *this; }
		DeviceMaker& setCbFree(DkFreeFunc cbFree) noexcept { this->cbFree = cbFree; return *this; }
		DeviceMaker& setFlags(uint32_t flags) noexcept { this->flags = flags; return *this; }
		Device create();
	};

	struct MemBlockMaker : public ::DkMemBlockMaker
	{
		MemBlockMaker(DkDevice device, uint32_t size) noexcept : DkMemBlockMaker{} { ::dkMemBlockMakerDefaults(this, device, size); }
		MemBlockMaker(MemBlockMaker&) = default;
		MemBlockMaker(MemBlockMaker&&) = default;
		MemBlockMaker& setFlags(uint32_t flags) noexcept { this->flags = flags; return *this; }
		MemBlockMaker& setStorage(void* storage) noexcept { this->storage = storage; return *this; }
		MemBlock create();
	};

	struct CmdBufMaker : public ::DkCmdBufMaker
	{
		CmdBufMaker(DkDevice device) noexcept : DkCmdBufMaker{} { ::dkCmdBufMakerDefaults(this, device); }
		CmdBufMaker& setUserData(void* userData) noexcept { this->userData = userData; return *this; }
		CmdBufMaker& setCbAddMem(DkCmdBufAddMemFunc cbAddMem) noexcept { this->cbAddMem = cbAddMem; return *this; }
		CmdBuf create();
	};

	struct QueueMaker : public ::DkQueueMaker
	{
		QueueMaker(DkDevice device) noexcept : DkQueueMaker{} { ::dkQueueMakerDefaults(this, device); }
		QueueMaker& setFlags(uint32_t flags) noexcept { this->flags = flags; return *this; }
		QueueMaker& setCommandMemorySize(uint32_t commandMemorySize) noexcept { this->commandMemorySize = commandMemorySize; return *this; }
		QueueMaker& setFlushThreshold(uint32_t flushThreshold) noexcept { this->flushThreshold = flushThreshold; return *this; }
		Queue create();
	};

	inline Device DeviceMaker::create()
	{
		return Device{::dkDeviceCreate(this)};
	}

	inline void Device::destroy()
	{
		::dkDeviceDestroy(*this);
	}

	inline MemBlock MemBlockMaker::create()
	{
		return MemBlock{::dkMemBlockCreate(this)};
	}

	inline void MemBlock::destroy()
	{
		::dkMemBlockDestroy(*this);
	}

	inline void* MemBlock::getCpuAddr()
	{
		return ::dkMemBlockGetCpuAddr(*this);
	}

	inline DkGpuAddr MemBlock::getGpuAddr()
	{
		return ::dkMemBlockGetGpuAddr(*this);
	}

	inline DkResult MemBlock::flushCpuCache(uint32_t offset, uint32_t size)
	{
		return ::dkMemBlockFlushCpuCache(*this, offset, size);
	}

	inline DkResult MemBlock::invalidateCpuCache(uint32_t offset, uint32_t size)
	{
		return ::dkMemBlockInvalidateCpuCache(*this, offset, size);
	}

	inline DkResult Fence::wait(int64_t timeout_ns)
	{
		return ::dkFenceWait(this, timeout_ns);
	}

	inline CmdBuf CmdBufMaker::create()
	{
		return CmdBuf{::dkCmdBufCreate(this)};
	}

	inline void CmdBuf::destroy()
	{
		::dkCmdBufDestroy(*this);
	}

	inline void CmdBuf::addMemory(DkMemBlock mem, uint32_t offset, uint32_t size)
	{
		::dkCmdBufAddMemory(*this, mem, offset, size);
	}

	inline DkCmdList CmdBuf::finishList()
	{
		return ::dkCmdBufFinishList(*this);
	}

	inline void CmdBuf::waitFence(DkFence& fence)
	{
		return ::dkCmdBufWaitFence(*this, &fence);
	}

	inline void CmdBuf::signalFence(DkFence& fence, bool flush)
	{
		return ::dkCmdBufSignalFence(*this, &fence, flush);
	}

	inline Queue QueueMaker::create()
	{
		return Queue{::dkQueueCreate(this)};
	}

	inline void Queue::destroy()
	{
		::dkQueueDestroy(*this);
	}

	inline bool Queue::isInErrorState()
	{
		return ::dkQueueIsInErrorState(*this);
	}

	inline void Queue::waitFence(DkFence& fence)
	{
		::dkQueueWaitFence(*this, &fence);
	}

	inline void Queue::signalFence(DkFence& fence, bool flush)
	{
		::dkQueueSignalFence(*this, &fence, flush);
	}

	inline void Queue::submitCommands(DkCmdList cmds)
	{
		::dkQueueSubmitCommands(*this, cmds);
	}

	inline void Queue::flush()
	{
		::dkQueueFlush(*this);
	}

	inline void Queue::present(DkWindow window, int imageSlot)
	{
		::dkQueuePresent(*this, window, imageSlot);
	}

	using UniqueDevice = detail::UniqueHandle<Device>;
	using UniqueMemBlock = detail::UniqueHandle<MemBlock>;
	using UniqueCmdBuf = detail::UniqueHandle<CmdBuf>;
	using UniqueQueue = detail::UniqueHandle<Queue>;
}
