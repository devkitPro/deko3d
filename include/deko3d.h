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
DK_DECL_HANDLE(DkQueue);
DK_DECL_HANDLE(DkShader);
DK_DECL_HANDLE(DkImage);
DK_DECL_HANDLE(DkImageView);
DK_DECL_HANDLE(DkImagePool);
DK_DECL_HANDLE(DkSampler);
DK_DECL_HANDLE(DkSamplerPool);
DK_DECL_HANDLE(DkCmdBuf);
DK_DECL_HANDLE(DkWindow);

typedef enum
{
	DkResult_Success,
	DkResult_Fail,
	DkResult_NotImplemented,
	DkResult_OutOfMemory,
	DkResult_MisalignedSize,
	DkResult_MisalignedData,
	DkResult_Timeout,
} DkResult;

#define DK_GPU_ADDR_INVALID (~0ULL)

typedef uint64_t DkGpuAddr;
typedef void (*DkErrorFunc)(void* userData, const char* context, DkResult result);
typedef DkResult (*DkAllocFunc)(void* userData, size_t alignment, size_t size, void** out);
typedef void (*DkFreeFunc)(void* userData, void* mem);

enum
{
	DkDeviceFlags_
};

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

#ifdef __cplusplus
extern "C" {
#endif

DkDevice dkDeviceCreate(DkDeviceMaker const* maker);
void dkDeviceDestroy(DkDevice obj);

DkMemBlock dkMemBlockCreate(DkMemBlockMaker const* maker);
void dkMemBlockDestroy(DkMemBlock obj);
void* dkMemBlockGetCpuAddr(DkMemBlock obj);
DkGpuAddr dkMemBlockGetGpuAddr(DkMemBlock obj);
DkResult dkMemBlockFlushCpuCache(DkMemBlock obj, uint32_t offset, uint32_t size);
DkResult dkMemBlockInvalidateCpuCache(DkMemBlock obj, uint32_t offset, uint32_t size);

DkResult dkFenceWait(DkFence* obj, int64_t timeout_ns);

#ifdef __cplusplus
}
#endif
