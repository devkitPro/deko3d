// Temporary header file which will be replaced once iface generation works

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>

#if __cplusplus >= 201402L
#define DK_CONSTEXPR constexpr
#else
#define DK_CONSTEXPR static inline
#endif

#define DK_DECL_HANDLE(_typename) \
	typedef struct tag_##_typename *_typename

#ifndef DK_NO_OPAQUE_DUMMY
#define DK_DECL_OPAQUE(_typename, _align, _size) \
	typedef struct _typename \
	{ \
		alignas(_align) uint8_t _storage[_size]; \
	} _typename
#else
#define DK_DECL_OPAQUE(_typename, _align, _size) \
	struct _typename; /* forward declaration */ \
	constexpr unsigned _align_##_typename = _align; \
	constexpr unsigned _size_##_typename = _size
#endif

DK_DECL_HANDLE(DkDevice);
DK_DECL_HANDLE(DkMemBlock);
DK_DECL_OPAQUE(DkFence, 8, 40);
DK_DECL_HANDLE(DkCmdBuf);
DK_DECL_HANDLE(DkQueue);
DK_DECL_HANDLE(DkShader);
DK_DECL_HANDLE(DkImage);
DK_DECL_HANDLE(DkImageView);
DK_DECL_HANDLE(DkImagePool);
DK_DECL_HANDLE(DkSampler);
DK_DECL_HANDLE(DkSamplerPool);
DK_DECL_HANDLE(DkWindow);

typedef enum
{
	DkResult_Success,
	DkResult_Fail,
	DkResult_Timeout,
	DkResult_OutOfMemory,
	DkResult_NotImplemented,
	DkResult_MisalignedSize,
	DkResult_MisalignedData,
	DkResult_BadInput,
	DkResult_BadMemFlags,
	DkResult_BadState,
} DkResult;

#define DK_GPU_ADDR_INVALID (~0ULL)

typedef uint64_t DkGpuAddr;
typedef uintptr_t DkCmdList;
typedef void (*DkErrorFunc)(void* userData, const char* context, DkResult result);
typedef DkResult (*DkAllocFunc)(void* userData, size_t alignment, size_t size, void** out);
typedef void (*DkFreeFunc)(void* userData, void* mem);
typedef void (*DkCmdBufAddMemFunc)(void* userData, DkCmdBuf cmdbuf, size_t minReqSize);

typedef struct DkDeviceMaker
{
	void* userData;
	DkErrorFunc cbError;
	DkAllocFunc cbAlloc;
	DkFreeFunc cbFree;
	uint32_t flags;
} DkDeviceMaker;

DK_CONSTEXPR void dkDeviceMakerDefaults(DkDeviceMaker* maker)
{
	maker->userData = NULL;
	maker->cbError = NULL;
	maker->cbAlloc = NULL;
	maker->cbFree = NULL;
	maker->flags = 0;
}

#define DK_MEMBLOCK_ALIGNMENT 0x1000
#define DK_CMDMEM_ALIGNMENT 4
#define DK_QUEUE_MIN_CMDMEM_SIZE 0x10000

enum
{
	DkMemAccess_None = 0U,
	DkMemAccess_Uncached = 1U,
	DkMemAccess_Cached = 2U,
	DkMemAccess_Mask = 3U,
};

enum
{
	DkMemBlockFlags_CpuAccessShift = 0U,
	DkMemBlockFlags_GpuAccessShift = 2U,
	DkMemBlockFlags_CpuUncached    = DkMemAccess_Uncached << DkMemBlockFlags_CpuAccessShift,
	DkMemBlockFlags_CpuCached      = DkMemAccess_Cached   << DkMemBlockFlags_CpuAccessShift,
	DkMemBlockFlags_CpuAccessMask  = DkMemAccess_Mask     << DkMemBlockFlags_CpuAccessShift,
	DkMemBlockFlags_GpuUncached    = DkMemAccess_Uncached << DkMemBlockFlags_GpuAccessShift,
	DkMemBlockFlags_GpuCached      = DkMemAccess_Cached   << DkMemBlockFlags_GpuAccessShift,
	DkMemBlockFlags_GpuAccessMask  = DkMemAccess_Mask     << DkMemBlockFlags_GpuAccessShift,
	DkMemBlockFlags_Code           = 1U << 4,
	DkMemBlockFlags_Image          = 1U << 5,
};

typedef struct DkMemBlockMaker
{
	DkDevice device;
	uint32_t size;
	uint32_t flags;
	void* storage;
} DkMemBlockMaker;

DK_CONSTEXPR void dkMemBlockMakerDefaults(DkMemBlockMaker* maker, DkDevice device, uint32_t size)
{
	maker->device = device;
	maker->size = size;
	maker->flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
	maker->storage = nullptr;
}

typedef struct DkCmdBufMaker
{
	DkDevice device;
	void* userData;
	DkCmdBufAddMemFunc cbAddMem;
} DkCmdBufMaker;

DK_CONSTEXPR void dkCmdBufMakerDefaults(DkCmdBufMaker* maker, DkDevice device)
{
	maker->device = device;
	maker->userData = nullptr;
	maker->cbAddMem = nullptr;
}

enum
{
	DkQueueFlags_Graphics              = 1U << 0,
	DkQueueFlags_Compute               = 1U << 1,
	DkQueueFlags_Transfer              = 1U << 2,
	DkQueueFlags_EnableZcull           = 0U << 4,
	DkQueueFlags_DisableZcull          = 1U << 4,
	DkQueueFlags_DepthNegativeOneToOne = 0U << 8,
	DkQueueFlags_DepthZeroToOne        = 1U << 8,
	DkQueueFlags_OriginLowerLeft       = 0U << 9,
	DkQueueFlags_OriginUpperLeft       = 1U << 9,
};

typedef struct DkQueueMaker
{
	DkDevice device;
	uint32_t flags;
	uint32_t commandMemorySize;
	uint32_t flushThreshold;
} DkQueueMaker;

DK_CONSTEXPR void dkQueueMakerDefaults(DkQueueMaker* maker, DkDevice device)
{
	maker->device = device;
	maker->flags =
		DkQueueFlags_Graphics | DkQueueFlags_Compute | DkQueueFlags_Transfer |
		DkQueueFlags_EnableZcull |
		DkQueueFlags_DepthNegativeOneToOne | DkQueueFlags_OriginLowerLeft;
	maker->commandMemorySize = DK_QUEUE_MIN_CMDMEM_SIZE;
	maker->flushThreshold = DK_QUEUE_MIN_CMDMEM_SIZE/8;
}

typedef enum DkBarrier
{
	DkBarrier_None       = 0, // No ordering is performed
	DkBarrier_Tiles      = 1, // Orders the processing of tiles (similar to Vulkan subpasses)
	DkBarrier_Fragments  = 2, // Orders the processing of fragments (similar to Vulkan renderpasses)
	DkBarrier_Primitives = 3, // Completes the processing of all previous primitives and compute jobs
	DkBarrier_Full       = 4, // Completes the processing of all previous commands while disabling command list prefetch
} DkBarrier;

enum
{
	DkInvalidateFlags_Image   = 1U << 0, // Invalidates the image (texture) cache
	DkInvalidateFlags_Code    = 1U << 1, // Invalidates the shader code/data/uniform cache
	DkInvalidateFlags_Pool    = 1U << 2, // Invalidates the image/sampler pool (descriptor) cache
	DkInvalidateFlags_Zcull   = 1U << 3, // Invalidates Zcull state
	DkInvalidateFlags_L2Cache = 1U << 4, // Invalidates the L2 cache
};

#ifdef __cplusplus
extern "C" {
#endif

DkDevice dkDeviceCreate(DkDeviceMaker const* maker);
void dkDeviceDestroy(DkDevice obj);

DkMemBlock dkMemBlockCreate(DkMemBlockMaker const* maker);
void dkMemBlockDestroy(DkMemBlock obj);
void* dkMemBlockGetCpuAddr(DkMemBlock obj);
DkGpuAddr dkMemBlockGetGpuAddr(DkMemBlock obj);
uint32_t dkMemBlockGetSize(DkMemBlock obj);
DkResult dkMemBlockFlushCpuCache(DkMemBlock obj, uint32_t offset, uint32_t size);
DkResult dkMemBlockInvalidateCpuCache(DkMemBlock obj, uint32_t offset, uint32_t size);

DkResult dkFenceWait(DkFence* obj, int64_t timeout_ns);

DkCmdBuf dkCmdBufCreate(DkCmdBufMaker const* maker);
void dkCmdBufDestroy(DkCmdBuf obj);
void dkCmdBufAddMemory(DkCmdBuf obj, DkMemBlock mem, uint32_t offset, uint32_t size);
DkCmdList dkCmdBufFinishList(DkCmdBuf obj);
void dkCmdBufWaitFence(DkCmdBuf obj, DkFence* fence);
void dkCmdBufSignalFence(DkCmdBuf obj, DkFence* fence, bool flush);
void dkCmdBufBarrier(DkCmdBuf obj, DkBarrier mode, uint32_t invalidateFlags);

DkQueue dkQueueCreate(DkQueueMaker const* maker);
void dkQueueDestroy(DkQueue obj);
bool dkQueueIsInErrorState(DkQueue obj);
void dkQueueWaitFence(DkQueue obj, DkFence* fence);
void dkQueueSignalFence(DkQueue obj, DkFence* fence, bool flush);
void dkQueueSubmitCommands(DkQueue obj, DkCmdList cmds);
void dkQueueFlush(DkQueue obj);
void dkQueueWaitIdle(DkQueue obj);
void dkQueuePresent(DkQueue obj, DkWindow window, int imageSlot);

#ifdef __cplusplus
}
#endif
