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
DK_DECL_OPAQUE(DkShader, 8, 96);
DK_DECL_OPAQUE(DkImageLayout, 8, 128); // size todo
DK_DECL_OPAQUE(DkImage, 8, 128); // size todo
DK_DECL_OPAQUE(DkImageDescriptor, 4, 32);
DK_DECL_HANDLE(DkSampler);
DK_DECL_HANDLE(DkSamplerPool);
DK_DECL_HANDLE(DkSwapchain);

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
typedef uint32_t DkResHandle;
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
#define DK_PER_WARP_SCRATCH_MEM_ALIGNMENT 0x200
#define DK_NUM_UNIFORM_BUFS 16
#define DK_NUM_STORAGE_BUFS 16
#define DK_NUM_TEXTURE_BINDINGS 32
#define DK_NUM_IMAGE_BINDINGS 8
#define DK_UNIFORM_BUF_ALIGNMENT 0x100
#define DK_UNIFORM_BUF_MAX_SIZE 0x10000
#define DK_DEFAULT_MAX_COMPUTE_CONCURRENT_JOBS 128
#define DK_SHADER_CODE_ALIGNMENT 0x100
#define DK_IMAGE_DESCRIPTOR_ALIGNMENT 0x20

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
	DkMemBlockFlags_ZeroFillInit   = 1U << 8,
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
	maker->storage = NULL;
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
	maker->userData = NULL;
	maker->cbAddMem = NULL;
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
	uint32_t perWarpScratchMemorySize;
	uint32_t maxConcurrentComputeJobs;
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
	maker->perWarpScratchMemorySize = 4*DK_PER_WARP_SCRATCH_MEM_ALIGNMENT;
	maker->maxConcurrentComputeJobs = DK_DEFAULT_MAX_COMPUTE_CONCURRENT_JOBS;
}

typedef struct DkShaderMaker
{
	DkMemBlock codeMem;
	const void* control;
	uint32_t codeOffset;
	uint32_t programId;
} DkShaderMaker;

DK_CONSTEXPR void dkShaderMakerDefaults(DkShaderMaker* maker, DkMemBlock codeMem, uint32_t codeOffset)
{
	maker->codeMem = codeMem;
	maker->control = NULL;
	maker->codeOffset = codeOffset;
	maker->programId = 0;
}

typedef enum DkStage
{
	DkStage_Vertex   = 0,
	DkStage_TessCtrl = 1,
	DkStage_TessEval = 2,
	DkStage_Geometry = 3,
	DkStage_Fragment = 4,
	DkStage_Compute  = 5,

	DkStage_MaxGraphics = 5,
} DkStage;

enum
{
	DkStageFlag_Vertex   = 1U << DkStage_Vertex,
	DkStageFlag_TessCtrl = 1U << DkStage_TessCtrl,
	DkStageFlag_TessEval = 1U << DkStage_TessEval,
	DkStageFlag_Geometry = 1U << DkStage_Geometry,
	DkStageFlag_Fragment = 1U << DkStage_Fragment,
	DkStageFlag_Compute  = 1U << DkStage_Compute,

	DkStageFlag_GraphicsMask = (1U << DkStage_MaxGraphics) - 1,
};

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

typedef enum DkImageType
{
	DkImageType_None         = 0,
	DkImageType_1D           = 1,
	DkImageType_2D           = 2,
	DkImageType_3D           = 3,
	DkImageType_1DArray      = 4,
	DkImageType_2DArray      = 5,
	DkImageType_2DMS         = 6,
	DkImageType_2DMSArray    = 7,
	DkImageType_Rectangle    = 8,
	DkImageType_Cubemap      = 9,
	DkImageType_CubemapArray = 10,
	DkImageType_Buffer       = 11,
} DkImageType;

enum
{
	DkImageFlags_BlockLinear    = 0U << 0, // Image is stored in Nvidia block linear format (default).
	DkImageFlags_PitchLinear    = 1U << 0, // Image is stored in standard pitch linear format.
	DkImageFlags_CustomTileSize = 1U << 1, // Use a custom tile size for block linear images.
	DkImageFlags_HwCompression  = 1U << 2, // Specifies that hardware compression is allowed to be enabled (forced true for depth images).
	DkImageFlags_D16EnableZbc   = 1U << 3, // For DkImageFormat_Z16 images, specifies that zero-bandwidth clear is preferred as the hardware compression format.

	DkImageFlags_UsageRender    = 1U << 8,  // Specifies that the image will be used as a render target.
	DkImageFlags_UsageLoadStore = 1U << 9,  // Specifies that the image will be used with shader image load/store commands.
	DkImageFlags_UsagePresent   = 1U << 10, // Specifies that the image will be presented to a DkWindow.
	DkImageFlags_Usage2DEngine  = 1U << 11, // Specifies that the image will be used with the 2D Engine (e.g. for transfers between images)
	DkImageFlags_UsageVideo     = 1U << 12, // Specifies that the image will be used with hardware video encoding/decoding engines
};

typedef enum DkImageFormat
{
	DkImageFormat_None,
	DkImageFormat_R8_Unorm,
	DkImageFormat_R8_Snorm,
	DkImageFormat_R8_Uint,
	DkImageFormat_R8_Sint,
	DkImageFormat_R16_Float,
	DkImageFormat_R16_Unorm,
	DkImageFormat_R16_Snorm,
	DkImageFormat_R16_Uint,
	DkImageFormat_R16_Sint,
	DkImageFormat_R32_Float,
	DkImageFormat_R32_Uint,
	DkImageFormat_R32_Sint,
	DkImageFormat_RG8_Unorm,
	DkImageFormat_RG8_Snorm,
	DkImageFormat_RG8_Uint,
	DkImageFormat_RG8_Sint,
	DkImageFormat_RG16_Float,
	DkImageFormat_RG16_Unorm,
	DkImageFormat_RG16_Snorm,
	DkImageFormat_RG16_Uint,
	DkImageFormat_RG16_Sint,
	DkImageFormat_RG32_Float,
	DkImageFormat_RG32_Uint,
	DkImageFormat_RG32_Sint,
	DkImageFormat_RGB32_Float,
	DkImageFormat_RGB32_Uint,
	DkImageFormat_RGB32_Sint,
	DkImageFormat_RGBA8_Unorm,
	DkImageFormat_RGBA8_Snorm,
	DkImageFormat_RGBA8_Uint,
	DkImageFormat_RGBA8_Sint,
	DkImageFormat_RGBA16_Float,
	DkImageFormat_RGBA16_Unorm,
	DkImageFormat_RGBA16_Snorm,
	DkImageFormat_RGBA16_Uint,
	DkImageFormat_RGBA16_Sint,
	DkImageFormat_RGBA32_Float,
	DkImageFormat_RGBA32_Uint,
	DkImageFormat_RGBA32_Sint,
	DkImageFormat_S8,
	DkImageFormat_Z16,
	DkImageFormat_Z24X8,
	DkImageFormat_ZF32,
	DkImageFormat_Z24S8,
	DkImageFormat_ZF32_X24S8,
	DkImageFormat_RGBX8_Unorm_sRGB,
	DkImageFormat_RGBA8_Unorm_sRGB,
	DkImageFormat_RGBA4_Unorm,
	DkImageFormat_RGB5_Unorm,
	DkImageFormat_RGB5A1_Unorm,
	DkImageFormat_RGB565_Unorm,
	DkImageFormat_RGB10A2_Unorm,
	DkImageFormat_RGB10A2_Uint,
	DkImageFormat_RG11B10_Float,
	DkImageFormat_E5BGR9_Float,
	DkImageFormat_RGB_DXT1,
	DkImageFormat_RGBA_DXT1,
	DkImageFormat_RGBA_DXT23,
	DkImageFormat_RGBA_DXT45,
	DkImageFormat_RGB_DXT1_sRGB,
	DkImageFormat_RGBA_DXT1_sRGB,
	DkImageFormat_RGBA_DXT23_sRGB,
	DkImageFormat_RGBA_DXT45_sRGB,
	DkImageFormat_R_DXN1_Unorm,
	DkImageFormat_R_DXN1_Snorm,
	DkImageFormat_RG_DXN2_Unorm,
	DkImageFormat_RG_DXN2_Snorm,
	DkImageFormat_RGBA_BC7U_Unorm,
	DkImageFormat_RGBA_BC7U_Unorm_sRGB,
	DkImageFormat_RGBA_BC6H_SF16_Float,
	DkImageFormat_RGBA_BC6H_UF16_Float,
	DkImageFormat_RGBX8_Unorm,
	DkImageFormat_RGBX8_Snorm,
	DkImageFormat_RGBX8_Uint,
	DkImageFormat_RGBX8_Sint,
	DkImageFormat_RGBX16_Float,
	DkImageFormat_RGBX16_Unorm,
	DkImageFormat_RGBX16_Snorm,
	DkImageFormat_RGBX16_Uint,
	DkImageFormat_RGBX16_Sint,
	DkImageFormat_RGBX32_Float,
	DkImageFormat_RGBX32_Uint,
	DkImageFormat_RGBX32_Sint,
	DkImageFormat_RGBA_ASTC_4x4,
	DkImageFormat_RGBA_ASTC_5x4,
	DkImageFormat_RGBA_ASTC_5x5,
	DkImageFormat_RGBA_ASTC_6x5,
	DkImageFormat_RGBA_ASTC_6x6,
	DkImageFormat_RGBA_ASTC_8x5,
	DkImageFormat_RGBA_ASTC_8x6,
	DkImageFormat_RGBA_ASTC_8x8,
	DkImageFormat_RGBA_ASTC_10x5,
	DkImageFormat_RGBA_ASTC_10x6,
	DkImageFormat_RGBA_ASTC_10x8,
	DkImageFormat_RGBA_ASTC_10x10,
	DkImageFormat_RGBA_ASTC_12x10,
	DkImageFormat_RGBA_ASTC_12x12,
	DkImageFormat_RGBA_ASTC_4x4_sRGB,
	DkImageFormat_RGBA_ASTC_5x4_sRGB,
	DkImageFormat_RGBA_ASTC_5x5_sRGB,
	DkImageFormat_RGBA_ASTC_6x5_sRGB,
	DkImageFormat_RGBA_ASTC_6x6_sRGB,
	DkImageFormat_RGBA_ASTC_8x5_sRGB,
	DkImageFormat_RGBA_ASTC_8x6_sRGB,
	DkImageFormat_RGBA_ASTC_8x8_sRGB,
	DkImageFormat_RGBA_ASTC_10x5_sRGB,
	DkImageFormat_RGBA_ASTC_10x6_sRGB,
	DkImageFormat_RGBA_ASTC_10x8_sRGB,
	DkImageFormat_RGBA_ASTC_10x10_sRGB,
	DkImageFormat_RGBA_ASTC_12x10_sRGB,
	DkImageFormat_RGBA_ASTC_12x12_sRGB,
	DkImageFormat_BGR565_Unorm,
	DkImageFormat_BGR5_Unorm,
	DkImageFormat_BGR5A1_Unorm,
	DkImageFormat_A5BGR5_Unorm,
	DkImageFormat_BGRX8_Unorm,
	DkImageFormat_BGRA8_Unorm,
	DkImageFormat_BGRX8_Unorm_sRGB,
	DkImageFormat_BGRA8_Unorm_sRGB,

	DkImageFormat_Count,
} DkImageFormat;

typedef enum DkSwizzle
{
	DkSwizzle_Zero  = 0,
	DkSwizzle_One   = 1,
	DkSwizzle_Red   = 2,
	DkSwizzle_Green = 3,
	DkSwizzle_Blue  = 4,
	DkSwizzle_Alpha = 5,
} DkSwizzle;

typedef enum DkMsMode
{
	DkMsMode_1x = 0,
	DkMsMode_2x = 1,
	DkMsMode_4x = 2,
	DkMsMode_8x = 3,
} DkMsMode;

typedef enum DkDsSource
{
	DkDsSource_Depth   = 0,
	DkDsSource_Stencil = 1,
} DkDsSource;

typedef enum DkTileSize
{
	DkTileSize_OneGob        = 0,
	DkTileSize_TwoGobs       = 1,
	DkTileSize_FourGobs      = 2,
	DkTileSize_EightGobs     = 3,
	DkTileSize_SixteenGobs   = 4,
	DkTileSize_ThirtyTwoGobs = 5,
} DkTileSize;

typedef struct DkImageLayoutMaker
{
	DkDevice device;
	DkImageType type;
	uint32_t flags;
	DkImageFormat format;
	DkMsMode msMode;
	uint32_t dimensions[3];
	uint32_t mipLevels;
	union
	{
		uint32_t pitchStride;
		DkTileSize tileSize;
	};
} DkImageLayoutMaker;

DK_CONSTEXPR void dkImageLayoutMakerDefaults(DkImageLayoutMaker* maker, DkDevice device)
{
	maker->device = device;
	maker->type = DkImageType_2D;
	maker->flags = 0;
	maker->format = DkImageFormat_None;
	maker->msMode = DkMsMode_1x;
	maker->dimensions[0] = 0;
	maker->dimensions[1] = 0;
	maker->dimensions[2] = 0;
	maker->mipLevels = 1;
	maker->pitchStride = 0;
}

typedef struct DkImageView
{
	DkImage const* pImage;
	DkImageType type;
	DkImageFormat format;
	DkSwizzle swizzle[4];
	DkDsSource dsSource;
	uint16_t layerOffset;
	uint16_t layerCount;
	uint8_t mipLevelOffset;
	uint8_t mipLevelCount;
} DkImageView;

DK_CONSTEXPR void dkImageViewDefaults(DkImageView* obj, DkImage const* pImage)
{
	obj->pImage = pImage;
	obj->type = DkImageType_None; // no override
	obj->format = DkImageFormat_None; // no override
	obj->swizzle[0] = DkSwizzle_Red;
	obj->swizzle[1] = DkSwizzle_Green;
	obj->swizzle[2] = DkSwizzle_Blue;
	obj->swizzle[3] = DkSwizzle_Alpha;
	obj->dsSource = DkDsSource_Depth;
	obj->layerOffset = 0;
	obj->layerCount = 0; // no override
	obj->mipLevelOffset = 0;
	obj->mipLevelCount = 0; // no override
}

typedef struct DkBufExtents
{
	DkGpuAddr addr;
	uint32_t size;
} DkBufExtents;

DK_CONSTEXPR DkResHandle dkMakeImageHandle(uint32_t id)
{
	return id & ((1U << 20) - 1);
}

DK_CONSTEXPR DkResHandle dkMakeSamplerHandle(uint32_t id)
{
	return id << 20;
}

DK_CONSTEXPR DkResHandle dkMakeTextureHandle(uint32_t imageId, uint32_t samplerId)
{
	return dkMakeImageHandle(imageId) | dkMakeSamplerHandle(samplerId);
}

typedef struct DkDispatchIndirectData
{
	uint32_t numGroupsX;
	uint32_t numGroupsY;
	uint32_t numGroupsZ;
} DkDispatchIndirectData;

typedef struct DkSwapchainMaker
{
	DkDevice device;
	void* nativeWindow;
	DkImage const* const* pImages;
	uint32_t numImages;
} DkSwapchainMaker;

DK_CONSTEXPR void dkSwapchainMakerDefaults(DkSwapchainMaker* maker, DkDevice device, void* nativeWindow, DkImage const* const pImages[], uint32_t numImages)
{
	maker->device = device;
	maker->nativeWindow = nativeWindow;
	maker->pImages = pImages;
	maker->numImages = numImages;
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
void dkCmdBufBindShaders(DkCmdBuf obj, uint32_t stageMask, DkShader const* const shaders[], uint32_t numShaders);
void dkCmdBufBindUniformBuffers(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkBufExtents const buffers[], uint32_t numBuffers);
void dkCmdBufBindStorageBuffers(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkBufExtents const buffers[], uint32_t numBuffers);
void dkCmdBufBindTextures(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkResHandle const handles[], uint32_t numHandles);
void dkCmdBufBindImages(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkResHandle const handles[], uint32_t numHandles);
void dkCmdBufBindImageDescriptorSet(DkCmdBuf obj, DkGpuAddr setAddr, uint32_t numDescriptors);
void dkCmdBufDispatchCompute(DkCmdBuf obj, uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ);
void dkCmdBufDispatchComputeIndirect(DkCmdBuf obj, DkGpuAddr indirect);
void dkCmdBufPushConstants(DkCmdBuf obj, DkGpuAddr uboAddr, uint32_t uboSize, uint32_t offset, uint32_t size, const void* data);

DkQueue dkQueueCreate(DkQueueMaker const* maker);
void dkQueueDestroy(DkQueue obj);
bool dkQueueIsInErrorState(DkQueue obj);
void dkQueueWaitFence(DkQueue obj, DkFence* fence);
void dkQueueSignalFence(DkQueue obj, DkFence* fence, bool flush);
void dkQueueSubmitCommands(DkQueue obj, DkCmdList cmds);
void dkQueueFlush(DkQueue obj);
void dkQueueWaitIdle(DkQueue obj);
int dkQueueAcquireImage(DkQueue obj, DkSwapchain swapchain);
void dkQueuePresentImage(DkQueue obj, DkSwapchain swapchain, int imageSlot);

void dkShaderInitialize(DkShader* obj, DkShaderMaker const* maker);
bool dkShaderIsValid(DkShader const* obj);
DkStage dkShaderGetStage(DkShader const* obj);

void dkImageLayoutInitialize(DkImageLayout* obj, DkImageLayoutMaker const* maker);
uint64_t dkImageLayoutGetSize(DkImageLayout const* obj);
uint32_t dkImageLayoutGetAlignment(DkImageLayout const* obj);

void dkImageInitialize(DkImage* obj, DkImageLayout const* layout, DkMemBlock memBlock, uint32_t offset);
DkGpuAddr dkImageGetGpuAddr(DkImage const* obj);

void dkImageDescriptorInitialize(DkImageDescriptor* obj, DkImageView const* view, bool usesLoadOrStore);

DkSwapchain dkSwapchainCreate(DkSwapchainMaker const* maker);
void dkSwapchainDestroy(DkSwapchain obj);
void dkSwapchainAcquireImage(DkSwapchain obj, int* imageSlot, DkFence* fence);
void dkSwapchainSetCrop(DkSwapchain obj, int32_t left, int32_t top, int32_t right, int32_t bottom);
void dkSwapchainSetSwapInterval(DkSwapchain obj, uint32_t interval);

static inline void dkCmdBufBindShader(DkCmdBuf obj, DkShader const* shader)
{
	DkShader const* table[] = { shader };
	dkCmdBufBindShaders(obj, 1U << dkShaderGetStage(shader), table, 1);
}

static inline void dkCmdBufBindUniformBuffer(DkCmdBuf obj, DkStage stage, uint32_t id, DkGpuAddr bufAddr, uint32_t bufSize)
{
	DkBufExtents ext = { bufAddr, bufSize };
	dkCmdBufBindUniformBuffers(obj, stage, id, &ext, 1);
}

static inline void dkCmdBufBindStorageBuffer(DkCmdBuf obj, DkStage stage, uint32_t id, DkGpuAddr bufAddr, uint32_t bufSize)
{
	DkBufExtents ext = { bufAddr, bufSize };
	dkCmdBufBindStorageBuffers(obj, stage, id, &ext, 1);
}

static inline void dkCmdBufBindTexture(DkCmdBuf obj, DkStage stage, uint32_t id, DkResHandle handle)
{
	dkCmdBufBindTextures(obj, stage, id, &handle, 1);
}

static inline void dkCmdBufBindImage(DkCmdBuf obj, DkStage stage, uint32_t id, DkResHandle handle)
{
	dkCmdBufBindImages(obj, stage, id, &handle, 1);
}

static inline DkImageLayout const* dkImageGetLayout(DkImage const* obj)
{
	return (DkImageLayout const*)obj;
}

#ifdef __cplusplus
}
#endif
