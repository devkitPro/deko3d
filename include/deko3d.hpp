#pragma once
#ifdef DK_NO_OPAQUE_DUMMY
#undef DK_NO_OPAQUE_DUMMY
#endif
#include <type_traits>
#include <initializer_list>
#include <array>
#ifdef DK_HPP_SUPPORT_VECTOR
#include <vector>
#endif
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

		// ArrayProxy borrowed from Vulkan-Hpp with ♥
		template <typename T>
		class ArrayProxy
		{
			using nonconst_T = typename std::remove_const<T>::type;
			uint32_t m_count;
			T* m_ptr;
		public:
			constexpr ArrayProxy(std::nullptr_t) noexcept : m_count{}, m_ptr{} { }
			ArrayProxy(T& ptr) : m_count{1}, m_ptr(&ptr) { }
			ArrayProxy(uint32_t count, T* ptr) noexcept : m_count{count}, m_ptr{ptr} { }

			template <size_t N>
			ArrayProxy(std::array<nonconst_T, N>& data) noexcept :
				m_count{N}, m_ptr{data.data()} { }

			template <size_t N>
			ArrayProxy(std::array<nonconst_T, N> const& data) noexcept :
				m_count{N} , m_ptr{data.data()} { }

			ArrayProxy(std::initializer_list<T> const& data) noexcept :
				m_count{static_cast<uint32_t>(data.end() - data.begin())},
				m_ptr{data.begin()} { }

#ifdef DK_HPP_SUPPORT_VECTOR
			template <class Allocator = std::allocator<nonconst_T>>
			ArrayProxy(std::vector<nonconst_T, Allocator> & data) noexcept :
				m_count{static_cast<uint32_t>(data.size())},
				m_ptr{data.data()} { }

			template <class Allocator = std::allocator<nonconst_T>>
			ArrayProxy(std::vector<nonconst_T, Allocator> const& data) noexcept :
				m_count{static_cast<uint32_t>(data.size())},
				m_ptr{data.data()} { }
#endif

			const T* begin() const noexcept { return m_ptr; }
			const T* end()   const noexcept { return m_ptr + m_count; }
			const T& front() const noexcept { return *m_ptr; }
			const T& back()  const noexcept { return *(m_ptr + m_count - 1); }
			bool     empty() const noexcept { return m_count == 0; }
			uint32_t size()  const noexcept { return m_count; }
			T*       data()  const noexcept { return m_ptr; }
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
		uint32_t getSize();
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
		void barrier(DkBarrier mode, uint32_t invalidateFlags);
		void bindShader(DkShader const& shader);
		void bindShaders(uint32_t stageMask, detail::ArrayProxy<DkShader const* const> shaders);
		void bindUniformBuffer(DkStage stage, uint32_t id, DkGpuAddr bufAddr, uint32_t bufSize);
		void bindUniformBuffers(DkStage stage, uint32_t firstId, detail::ArrayProxy<DkBufExtents const> buffers);
		void bindStorageBuffer(DkStage stage, uint32_t id, DkGpuAddr bufAddr, uint32_t bufSize);
		void bindStorageBuffers(DkStage stage, uint32_t firstId, detail::ArrayProxy<DkBufExtents const> buffers);
		void bindTexture(DkStage stage, uint32_t id, DkResHandle handle);
		void bindTextures(DkStage stage, uint32_t firstId, detail::ArrayProxy<DkResHandle const> handles);
		void bindImage(DkStage stage, uint32_t id, DkResHandle handle);
		void bindImages(DkStage stage, uint32_t firstId, detail::ArrayProxy<DkResHandle const> handles);
		void dispatchCompute(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ);
		void dispatchComputeIndirect(DkGpuAddr indirect);
		void pushConstants(DkGpuAddr uboAddr, uint32_t uboSize, uint32_t offset, uint32_t size, const void* data);
	};

	struct Queue : public detail::Handle<::DkQueue>
	{
		DK_HANDLE_COMMON_MEMBERS(Queue);
		bool isInErrorState();
		void waitFence(DkFence& fence);
		void signalFence(DkFence& fence, bool flush = false);
		void submitCommands(DkCmdList cmds);
		void flush();
		void waitIdle();
		void present(DkWindow window, int imageSlot);
	};

	struct Shader : public detail::Opaque<::DkShader>
	{
		DK_OPAQUE_COMMON_MEMBERS(Shader);
		bool isValid() const;
		DkStage getStage() const;
	};

	struct ImageLayout : public detail::Opaque<::DkImageLayout>
	{
		DK_OPAQUE_COMMON_MEMBERS(ImageLayout);
		uint64_t getSize() const;
		uint32_t getAlignment() const;
	};

	struct Image : public detail::Opaque<::DkImage>
	{
		DK_OPAQUE_COMMON_MEMBERS(Image);
		void initialize(ImageLayout const& layout, DkMemBlock memBlock, uint32_t offset);
		DkGpuAddr getGpuAddr() const;
		ImageLayout const& getLayout() const;
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
		QueueMaker& setPerWarpScratchMemorySize(uint32_t perWarpScratchMemorySize) noexcept { this->perWarpScratchMemorySize = perWarpScratchMemorySize; return *this; }
		QueueMaker& setMaxConcurrentComputeJobs(uint32_t maxConcurrentComputeJobs) noexcept { this->maxConcurrentComputeJobs = maxConcurrentComputeJobs; return *this; }
		Queue create();
	};

	struct ShaderMaker : public ::DkShaderMaker
	{
		ShaderMaker(DkMemBlock codeMem, uint32_t codeOffset) noexcept : DkShaderMaker{} { ::dkShaderMakerDefaults(this, codeMem, codeOffset); }
		ShaderMaker& setControl(const void* control) noexcept { this->control = control; return *this; }
		ShaderMaker& setProgramId(uint32_t programId) noexcept { this->programId = programId; return *this; }
		void initialize(Shader& obj);
	};

	struct ImageLayoutMaker : public ::DkImageLayoutMaker
	{
		ImageLayoutMaker(DkDevice device) noexcept : DkImageLayoutMaker{} { ::dkImageLayoutMakerDefaults(this, device); }
		ImageLayoutMaker& setType(DkImageType type) noexcept { this->type = type; return *this; }
		ImageLayoutMaker& setFlags(uint32_t flags) noexcept { this->flags = flags; return *this; }
		ImageLayoutMaker& setFormat(DkImageFormat format) noexcept { this->format = format; return *this; }
		ImageLayoutMaker& setMsMode(DkMsMode msMode) noexcept { this->msMode = msMode; return *this; }
		ImageLayoutMaker& setDimensions(uint32_t width, uint32_t height = 0, uint32_t depth = 0) noexcept
		{
			this->dimensions[0] = width;
			this->dimensions[1] = height;
			this->dimensions[2] = depth;
			return *this;
		}
		ImageLayoutMaker& setMipLevels(uint32_t mipLevels) noexcept { this->mipLevels = mipLevels; return *this; }
		ImageLayoutMaker& setPitchStride(uint32_t pitchStride) noexcept { this->pitchStride = pitchStride; return *this; }
		ImageLayoutMaker& setTileSize(DkTileSize tileSize) noexcept { this->tileSize = tileSize; return *this; }
		void initialize(ImageLayout& obj);
	};

	struct ImageView : public ::DkImageView
	{
		ImageView(Image const& image) noexcept : DkImageView{} { ::dkImageViewDefaults(this, &image); }
		void setType(DkImageType type = DkImageType_None) noexcept { this->type = type; }
		void setFormat(DkImageFormat format = DkImageFormat_None) noexcept { this->format = format; }
		void setSwizzle(DkSwizzle x = DkSwizzle_Red, DkSwizzle y = DkSwizzle_Green, DkSwizzle z = DkSwizzle_Blue, DkSwizzle w = DkSwizzle_Alpha) noexcept
		{
			this->swizzle[0] = x;
			this->swizzle[1] = y;
			this->swizzle[2] = z;
			this->swizzle[3] = w;
		}
		void setDsSource(DkDsSource dsSource) noexcept { this->dsSource = dsSource; }
		void setLayers(uint16_t layerOffset = 0, uint16_t layerCount = 0) noexcept
		{
			this->layerOffset = layerOffset;
			this->layerCount = layerCount;
		}
		void setMipLevels(uint8_t mipLevelOffset = 0, uint8_t mipLevelCount = 0) noexcept
		{
			this->mipLevelOffset = mipLevelOffset;
			this->mipLevelCount = mipLevelCount;
		}
	};

	struct ImageDescriptor : public detail::Opaque<::DkImageDescriptor>
	{
		DK_OPAQUE_COMMON_MEMBERS(ImageDescriptor);
		void initialize(ImageView const& view, bool usesLoadOrStore = false);
	};

	inline Device DeviceMaker::create()
	{
		return Device{::dkDeviceCreate(this)};
	}

	inline void Device::destroy()
	{
		::dkDeviceDestroy(*this);
		_clear();
	}

	inline MemBlock MemBlockMaker::create()
	{
		return MemBlock{::dkMemBlockCreate(this)};
	}

	inline void MemBlock::destroy()
	{
		::dkMemBlockDestroy(*this);
		_clear();
	}

	inline void* MemBlock::getCpuAddr()
	{
		return ::dkMemBlockGetCpuAddr(*this);
	}

	inline DkGpuAddr MemBlock::getGpuAddr()
	{
		return ::dkMemBlockGetGpuAddr(*this);
	}

	inline uint32_t MemBlock::getSize()
	{
		return ::dkMemBlockGetSize(*this);
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
		_clear();
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

	void CmdBuf::barrier(DkBarrier mode, uint32_t invalidateFlags)
	{
		::dkCmdBufBarrier(*this, mode, invalidateFlags);
	}

	void CmdBuf::bindShader(DkShader const& shader)
	{
		::dkCmdBufBindShader(*this, &shader);
	}

	void CmdBuf::bindShaders(uint32_t stageMask, detail::ArrayProxy<DkShader const* const> shaders)
	{
		::dkCmdBufBindShaders(*this, stageMask, shaders.data(), shaders.size());
	}

	void CmdBuf::bindUniformBuffer(DkStage stage, uint32_t id, DkGpuAddr bufAddr, uint32_t bufSize)
	{
		::dkCmdBufBindUniformBuffer(*this, stage, id, bufAddr, bufSize);
	}

	void CmdBuf::bindUniformBuffers(DkStage stage, uint32_t firstId, detail::ArrayProxy<DkBufExtents const> buffers)
	{
		::dkCmdBufBindUniformBuffers(*this, stage, firstId, buffers.data(), buffers.size());
	}

	void CmdBuf::bindStorageBuffer(DkStage stage, uint32_t id, DkGpuAddr bufAddr, uint32_t bufSize)
	{
		::dkCmdBufBindStorageBuffer(*this, stage, id, bufAddr, bufSize);
	}

	void CmdBuf::bindStorageBuffers(DkStage stage, uint32_t firstId, detail::ArrayProxy<DkBufExtents const> buffers)
	{
		::dkCmdBufBindStorageBuffers(*this, stage, firstId, buffers.data(), buffers.size());
	}

	void CmdBuf::bindTexture(DkStage stage, uint32_t id, DkResHandle handle)
	{
		::dkCmdBufBindTexture(*this, stage, id, handle);
	}

	void CmdBuf::bindTextures(DkStage stage, uint32_t firstId, detail::ArrayProxy<DkResHandle const> handles)
	{
		::dkCmdBufBindTextures(*this, stage, firstId, handles.data(), handles.size());
	}

	void CmdBuf::bindImage(DkStage stage, uint32_t id, DkResHandle handle)
	{
		::dkCmdBufBindImage(*this, stage, id, handle);
	}

	void CmdBuf::bindImages(DkStage stage, uint32_t firstId, detail::ArrayProxy<DkResHandle const> handles)
	{
		::dkCmdBufBindImages(*this, stage, firstId, handles.data(), handles.size());
	}

	void CmdBuf::dispatchCompute(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ)
	{
		::dkCmdBufDispatchCompute(*this, numGroupsX, numGroupsY, numGroupsZ);
	}

	void CmdBuf::dispatchComputeIndirect(DkGpuAddr indirect)
	{
		::dkCmdBufDispatchComputeIndirect(*this, indirect);
	}

	void CmdBuf::pushConstants(DkGpuAddr uboAddr, uint32_t uboSize, uint32_t offset, uint32_t size, const void* data)
	{
		::dkCmdBufPushConstants(*this, uboAddr, uboSize, offset, size, data);
	}

	inline Queue QueueMaker::create()
	{
		return Queue{::dkQueueCreate(this)};
	}

	inline void Queue::destroy()
	{
		::dkQueueDestroy(*this);
		_clear();
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

	inline void Queue::waitIdle()
	{
		::dkQueueWaitIdle(*this);
	}

	inline void Queue::present(DkWindow window, int imageSlot)
	{
		::dkQueuePresent(*this, window, imageSlot);
	}

	inline void ShaderMaker::initialize(Shader& obj)
	{
		::dkShaderInitialize(&obj, this);
	}

	inline bool Shader::isValid() const
	{
		return ::dkShaderIsValid(this);
	}

	inline DkStage Shader::getStage() const
	{
		return ::dkShaderGetStage(this);
	}

	inline void ImageLayoutMaker::initialize(ImageLayout& obj)
	{
		::dkImageLayoutInitialize(&obj, this);
	}

	inline uint64_t ImageLayout::getSize() const
	{
		return ::dkImageLayoutGetSize(this);
	}

	inline uint32_t ImageLayout::getAlignment() const
	{
		return ::dkImageLayoutGetAlignment(this);
	}

	inline void Image::initialize(ImageLayout const& layout, DkMemBlock memBlock, uint32_t offset)
	{
		::dkImageInitialize(this, &layout, memBlock, offset);
	}

	inline DkGpuAddr Image::getGpuAddr() const
	{
		return ::dkImageGetGpuAddr(this);
	}

	inline ImageLayout const& Image::getLayout() const
	{
		return *static_cast<ImageLayout const*>(::dkImageGetLayout(this));
	}

	inline void ImageDescriptor::initialize(ImageView const& view, bool usesLoadOrStore)
	{
		::dkImageDescriptorInitialize(this, &view, usesLoadOrStore);
	}

	using UniqueDevice = detail::UniqueHandle<Device>;
	using UniqueMemBlock = detail::UniqueHandle<MemBlock>;
	using UniqueCmdBuf = detail::UniqueHandle<CmdBuf>;
	using UniqueQueue = detail::UniqueHandle<Queue>;
}
