# deko3d Primer - A Small Introduction

## Introduction

deko3d is a low level graphics programming API for use in Nintendo Switch homebrew (along with devkitA64 and libnx), taking inspiration from Vulkan and drawing many similarities stemming from a year-and-a-half long (and still ongoing!) reverse engineering (RE) effort centered around the official NVN library present in official software.

This primer is meant to show potential users of this library around the key foundations of its design, as well as several usage tips and hints that have been discovered throughout the RE and development process.

Familiarity with graphics APIs (mostly Vulkan and OpenGL) and 3D graphics programming in general are indicated as basic requirements to make the most out of deko3d and be able to understand this primer.

> **Warning**: This document is hugely under construction and it is by no means complete. It is recommended to read the deko3d.h header and study carefully the provided examples in the meantime.

## Library Structure and Basic Concepts

### Objects

deko3d is written in the C++ language, however its API is accessible from both plain C and with a convenient C++ wrapper modeled after Vulkan-Hpp. The API is object oriented, and each class of object can belong to one of the following categories according to their nature:

- **Handles**: objects of this type are represented by a single pointer sized value that acts as a handle. These objects hold state which involves the ownership of resources of some kind (GPU channels, memory allocations, etc), and there exists a function to destroy the object and free up all resources consumed by it. Memory needed to store this type of object is internally managed by the library, and no further user intervention other than storing/destroying the handle is required.
	- `DkDevice`
	- `DkMemBlock`
	- `DkCmdBuf`
	- `DkQueue`
	- `DkSwapchain`
- **Opaque objects**: these are structs containing no publicly visible fields, but whose memory the user is responsible for managing. Opaque objects typically hold internal pre-calculated book-keeping information about resources, and they do not need to be destroyed since they do not actually own the resources they describe.
	- `DkFence`
	- `DkShader`
	- `DkImageLayout`
	- `DkImage`
	- `DkImageDescriptor`
	- `DkSamplerDescriptor`
- **Transparent objects**: these are structs containing a full set of public fields that the user is able to change at will. Usually there is a function to initialize them with default values. These objects normally describe mutable hardware state or a complex collection of parameters. In addition, transparent objects (with the name ending in `Maker`) are used to configure the creation of handles and the initialization of opaque objects.
	- `DkDeviceMaker`
	- `DkMemBlockMaker`
	- `DkCmdBufMaker`
	- `DkQueueMaker`
	- `DkShaderMaker`
	- `DkImageLayoutMaker`
	- `DkSwapchainMaker`
	- `DkImageView`
	- `DkSampler`
	- `DkBufExtents`
	- `DkViewport`
	- `DkViewportSwizzle`
	- `DkScissor`
	- `DkRasterizerState`
	- `DkMultisampleState`
	- `DkSampleLocation`
	- `DkColorState`
	- `DkColorWriteState`
	- `DkBlendState`
	- `DkDepthStencilState`
	- `DkVtxAttribState`
	- `DkVtxBufferState`
	- `DkDrawIndirectData`
	- `DkDrawIndexedIndirectData`
	- `DkDispatchIndirectData`
	- `DkBlitRect`
	- `DkCopyBuf`

## Basic workflow

In order to create a handle, a code section like the following will be used:

(C)
```c
// Describe the device we're about to make
DkDeviceMaker maker;
dkDeviceMakerDefaults(&maker);
maker.flags = DkDeviceFlags_OriginLowerLeft;

// Create the device
DkDevice device = dkDeviceCreate(&maker);
```

(C++)
```cpp
// Create the device in one go
// In the C++ wrapper, Maker objects follow the factory pattern
dk::Device device = dk::DeviceMaker{}
	.setFlags(DkDeviceFlags_OriginLowerLeft)
	.create();
```

When we are done with the handle, it will need to be destroyed, like such:

(C)
```c
// Destroy the device
dkDeviceDestroy(device);
```

(C++)
```cpp
// Destroy the device
device.destroy();
```

In C++, an additional series of handle holder types exist (marked with `Unique` in their name), which automatically destroy the handle they own. For example, the above example could have been written as such:

```cpp
// Create the device
// dk::UniqueDevice contains a destructor which will automatically call destroy()
dk::UniqueDevice device = dk::DeviceMaker{}
	.setFlags(DkDeviceFlags_OriginLowerLeft)
	.create();
```

Opaque objects, on the other hand, are *initialized* instead of created. Here is an example:

(C)
```c
// Describe the shader we're about to initialize
DkShaderMaker maker;
dkShaderMakerDefaults(&maker, codeMemBlock, codeOffset);

// Initialize the shader
DkShader shader;
dkShaderInitialize(&shader, &maker);
```

(C++)
```cpp
// Initialize the shader in one go
dk::Shader shader;
dk::ShaderMaker{codeMemBlock, codeOffset}.initialize(shader);
```

Some opaque objects don't get initialized using a Maker object. Here's how that works:

(C)
```c
// Initialize a sampler
DkSampler sampler;
dkSamplerDefaults(&sampler);
sampler.minFilter = DkMipFilter_Linear;
sampler.magFilter = DkMipFilter_Linear;

// Initialize a sample descriptor using our sampler
DkSamplerDescriptor descr;
dkSamplerDescriptorInitialize(&descr, &sampler);
```

(C++)
```cpp
// Initialize a sampler
dk::Sampler sampler;
sampler.setFilter(DkMipFilter_Linear, DkMipFilter_Linear);

// Initialize a sample descriptor using our sampler
dk::SamplerDescriptor descr;
descr.initialize(sampler);
```

### Linking to deko3d

There exist two different compiled versions of the library:

- `libdeko3dd.a`: This is the **Debug** version, which acts as a validation layer with parameter and state checking built-in. In order to link to it specify `-ldeko3dd` in the LIBS section of your Makefile.
- `libdeko3d.a`: This is the **Release** version; which omits parameter and state checking and is built with extra optimizations. In order to link to it specify `-ldeko3d` in the LIBS section of your Makefile.

During development or experimentation it is recommended to link to the Debug version; when it is time to ship a precompiled binary of your application the Release version should be used instead.

## Objects

### Devices (`DkDevice`)

```c
struct DkDeviceMaker
{
	void* userData;
	DkErrorFunc cbError;
	DkAllocFunc cbAlloc;
	DkFreeFunc cbFree;
	uint32_t flags;
};
void dkDeviceMakerDefaults(DkDeviceMaker* maker);
DkDevice dkDeviceCreate(DkDeviceMaker const* maker);
void dkDeviceDestroy(DkDevice obj);
```

`DkDevice` is the root object from which most other deko3d objects can be traced back. It represents the GPU device with a private virtual GPU address space, and provides optional mechanisms for customizing the error handling or memory allocation behavior.

Field      | Default   | Description
-----------|-----------|------------------
`userData` | NULL      | User specified data to be passed to callback
`cbError`  | NULL      | Optional callback used when deko3d detects an unrecoverable error
`cbAlloc`  | NULL      | Optional callback used when deko3d needs to allocate memory
`cbFree`   | NULL      | Optional callback used when deko3d needs to free allocated memory
`flags`    | See below | Device creation flags (see below)

`DkDeviceFlags_*`    | Default | Description
---------------------|---------|--------------------------------------------------
`DepthZeroToOne`     | ✓       | Clip space Z is [0, 1] like Vulkan
`DepthMinusOneToOne` |         | Clip space Z is [-1, 1] like OpenGL
`OriginUpperLeft`    | ✓       | Image rows are stored sequentially from top to bottom, with 0.0 corresponding to the top edge of the image and 1.0 (or the image height if non-normalized) corresponding to the bottom
`OriginLowerLeft`    |         | Image rows are stored sequentially from bottom to top, with 0.0 corresponding to the bottom edge of the image and 1.0 (or the image height if non-normalized) corresponding to the top

If an error occurs and no error callback has been supplied, deko3d aborts program execution. If the release version of the library is used, a Fatal system error is thrown; if the debug version is used on the contrary, error text is sent to stdout, then the application forcefully closes back to the HOME menu.

> **Warning**: The error handling mechanism/validation layer in deko3d is very much preliminary and subject to future enhancements and overhauls. Expect future versions of deko3d to introduce breaking changes when using a custom error handling callback.

By default if memory allocation callbacks are not provided, deko3d uses the standard heap (i.e. malloc/free) for dynamic memory allocations.

`gl_FragCoord` in fragment shaders obeys the device origin mode when it comes to the Y axis and has pixel centers at half-integers, *with GLSL layout qualifiers having absolutely no effect*.
Please note that regardless of the Origin setting, the clip space Y axis points *up* like in OpenGL. Clip space X and Y are both in the range [-1, 1] as well.

### Memory Blocks (`DkMemBlock`)

```c
struct DkMemBlockMaker
{
	DkDevice device;
	uint32_t size;
	uint32_t flags;
	void* storage;
};
void dkMemBlockMakerDefaults(DkMemBlockMaker* maker, DkDevice device, uint32_t size);
DkMemBlock dkMemBlockCreate(DkMemBlockMaker const* maker);
void dkMemBlockDestroy(DkMemBlock obj);
void* dkMemBlockGetCpuAddr(DkMemBlock obj);
DkGpuAddr dkMemBlockGetGpuAddr(DkMemBlock obj);
uint32_t dkMemBlockGetSize(DkMemBlock obj);
DkResult dkMemBlockFlushCpuCache(DkMemBlock obj, uint32_t offset, uint32_t size);
```

`DkMemBlock` is an object that represents a block of memory holding resources that will be used with deko3d, such as GPU command lists, shader code, framebuffers, textures, vertex data, etc. The following settings can be configured during creation:

Field     | Default   | Description
----------|-----------|------------
`device`  | N/A       | Parent device
`size`    | N/A       | Size of the memory block (needs to be a multiple of `DK_MEMBLOCK_ALIGNMENT`)
`flags`   | See below | Memory block properties and configuration (see below)
`storage` | NULL      | Optional explicit storage buffer (needs to be aligned to `DK_MEMBLOCK_ALIGNMENT`). If NULL, deko3d will allocate (and destroy) the buffer using the parent device's memory allocation system.

`DkMemBlockFlags_*` | Default | Description
--------------------|---------|------------
`CpuAccessShift`    | N/A     | Bit position for the CPU's access mode, which regulates visibility and cacheability (see `DkMemAccess_*` below)
`CpuUncached`       | ✓       | `DkMemAccess_Uncached << DkMemBlockFlags_CpuAccessShift`
`CpuCached`         |         | `DkMemAccess_Cached << DkMemBlockFlags_CpuAccessShift`
`GpuAccessShift`    | N/A     | Bit position for the GPU's access mode, which regulates visibility and cacheability (see `DkMemAccess_*` below)
`GpuUncached`       |         | `DkMemAccess_Uncached << DkMemBlockFlags_GpuAccessShift`
`GpuCached`         | ✓       | `DkMemAccess_Cached << DkMemBlockFlags_GpuAccessShift`
`Code`              |         | The block will be used to hold shader code
`Image`             |         | The block will be used to hold image data
`ZeroFillInit`      |         | Zero-fills the memory during initialization

`DkMemAccess_*` | Description
----------------|------------
`None`          | The memory block is not mapped to the CPU/GPU's address space at all
`Uncached`      | The memory block is mapped, but the CPU/GPU is not allowed to cache accesses to it
`Cached`        | The memory block is mapped with full cache support

> **Note**: Each memory block that is created consumes limited bookkeeping resources provided by the operating system, regardless of their actual size. It is advised to "buy" big blocks wholesale from the system and subdivide them in software, instead of creating an individual block for every single resource that the application might want to create.

Once created, the following properties about the memory block can be retrieved:
- `dkMemBlockGetSize` returns the size of the memory block.
- `dkMemBlockGetCpuAddr` returns the CPU address of the memory block, or NULL if the CPU has no access to it (i.e. `DkMemAccess_None`).
- `dkMemBlockGetGpuAddr` returns the *generic* GPU address of the memory block, or NULL if the GPU has no access to it (i.e. `DkMemAccess_None`).

Memory blocks with GPU access can end up having several different mappings in the GPU's address space, depending on how the memory is to be used:
- A *generic* mapping will always be created, suitable for non-image purposes.
- If `DkMemBlockFlags_Code` is specified, the *generic* mapping will be placed within a special *code segment* in the GPU's address space, which is absolutely **necessary** for shader code to be accessible by the GPU. Up to 4 GiB of address space are reserved for the *code segment*.
- If `DkMemBlockFlags_Image` is specified, two extra mappings will be created with the necessary internal GPU memory attributes for normal and "compressed" image accesses respectively.

> **Warning**: Due to a hardware bug the last `DK_SHADER_CODE_UNUSABLE_SIZE` bytes of a memory block are unusable for storing shader code.

> **Warning**: Currently deko3d is unable to reuse previously-reserved mappings inside the *code segment* even if they're freed. Users are advised to reuse old code memory blocks instead of freeing them.

> **Note**: `dkMemBlockFlushCpuCache` is supposed to be used to flush the CPU-side cache for memory blocks with `DkMemBlockFlags_CpuCached`. However, this function is currently **not** implemented.

### Command Buffers (`DkCmdBuf`)

```c
typedef void (*DkCmdBufAddMemFunc)(void* userData, DkCmdBuf cmdbuf, size_t minReqSize);
struct DkCmdBufMaker
{
	DkDevice device;
	void* userData;
	DkCmdBufAddMemFunc cbAddMem;
};
void dkCmdBufMakerDefaults(DkCmdBufMaker* maker, DkDevice device);
DkCmdBuf dkCmdBufCreate(DkCmdBufMaker const* maker);
void dkCmdBufDestroy(DkCmdBuf obj);
void dkCmdBufAddMemory(DkCmdBuf obj, DkMemBlock mem, uint32_t offset, uint32_t size);
DkCmdList dkCmdBufFinishList(DkCmdBuf obj);
void dkCmdBufClear(DkCmdBuf obj);
//...
```

Command buffers (`DkCmdBuf`) allow users to record command lists that can be submitted to a queue (`DkQueue`). A command buffer can take in slices of backing memory, onto which the raw command data will be recorded in a format the the GPU can understand. A user can specify the current slice of backing memory to use with the `dkCmdBufAddMemory` function. It is allowed to call this function in the middle of recording, and the command list that was being recorded will continue at the new location.

> **Note**: Memory added with `dkCmdBufAddMemory` must be aligned to `DK_CMDMEM_ALIGNMENT`, and its size must also be a multiple of `DK_CMDMEM_ALIGNMENT`.

Command buffers maintain and keep ownership of internal bookkeeping memory that is used to fully define a command list, including but not limited to the entire history of memory regions used or fences referenced. When `dkCmdBufFinishList` is called, the currently recorded command list is signed off and a handle to it (`DkCmdList`) is returned. This function can be called as many times as necessary in order to build as many command lists as desired out of the user provided backing memory. Command lists can be submitted to a queue as many times as desired as well. Command list handles will remain valid as long as their parent command buffer continues existing as an active object, the memory slices they're referencing haven't been overwritten by other command lists, or until `dkCmdBufClear` is called. When `dkCmdBufClear` is called, all command lists that have been recorded with the command buffer are destroyed, their handles are invalidated (freeing up internal bookkeeping memory), and the current memory slice is rolled back to the beginning.

If the current slice of backing memory runs out of space during recording, either of two things can happen:
- If the user didn't supply a `cbAddMem` callback, a fatal error is raised.
- Otherwise, the `cbAddMem` callback is called, which is expected to in turn call `dkCmdBufAddMemory` in order to resolve the situation. If the callback doesn't add enough memory, a fatal error is raised.

This mechanism is intended to be used for command buffers backed by dynamic memory, so that they can refill themselves with fresh new memory as needed.

`DkCmdBuf` objects are *externally synchronized*; in other words, they are not in charge of synchronization themselves and thus multiple threads cannot use the same command buffer at the same time. The intended workflow in a multithreaded application is to have multiple worker threads recording commands independently (each fitted with its own command buffer), and have the parent thread collect and submit all the `DkCmdList` handles from the worker threads.

### Fences (`DkFence`)

```c
struct DkFence;
DkResult dkFenceWait(DkFence* obj, int64_t timeout_ns);
```

Fences (`DkFence`) are opaque structs that contain GPU synchronization information, used to determine when work submitted to the GPU has finished executing. Each time they're scheduled to be signaled in a queue (`DkQueue`), either directly or indirectly through a command list, their contents are updated. Fences can be waited on by the GPU or CPU (using the `dkFenceWait` function). When a fence is waited on, the waiter (CPU or GPU) will be kept blocked until the point in which it's signaled (hence marking the completion of dependent work).

Usually fences will be used in a signaling command prior to being waited on. If fences are to be potentially waited on before they're signaled (e.g. if they're used to wait on previous work, with no previous work having been submitted yet), they should be initialized to zero in order to ensure that any initial waits will correctly have no effect.

> **Warning**: Fence wait/signal commands recorded to a command list keep a pointer to the fence struct in the command buffer's bookkeeping memory. Please make sure the struct remains at the same valid memory address for the lifetime of the command list handle; otherwise submitting the command list handle to a queue will result in undefined behavior.

### Queues (`DkQueue`)

```c
struct DkQueueMaker
{
	DkDevice device;
	uint32_t flags;
	uint32_t commandMemorySize;
	uint32_t flushThreshold;
	uint32_t perWarpScratchMemorySize;
	uint32_t maxConcurrentComputeJobs;
};
void dkQueueMakerDefaults(DkQueueMaker* maker, DkDevice device);
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
```

### Shaders (`DkShader`)

#### The deko3d shader compiler

### Images (`DkImage`)

### Samplers (`DkSampler`)

### Swapchains (`DkSwapchain`)

## General commands

### Barriers and synchronization

```c
void dkCmdBufWaitFence(DkCmdBuf obj, DkFence* fence);
void dkCmdBufSignalFence(DkCmdBuf obj, DkFence* fence, bool flush);
void dkCmdBufBarrier(DkCmdBuf obj, DkBarrier mode, uint32_t invalidateFlags);
```

### Shader and render target setup

```c
void dkCmdBufBindShaders(DkCmdBuf obj, uint32_t stageMask, DkShader const* const shaders[], uint32_t numShaders);
void dkCmdBufBindUniformBuffers(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkBufExtents const buffers[], uint32_t numBuffers);
void dkCmdBufBindStorageBuffers(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkBufExtents const buffers[], uint32_t numBuffers);
void dkCmdBufBindTextures(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkResHandle const handles[], uint32_t numHandles);
void dkCmdBufBindImages(DkCmdBuf obj, DkStage stage, uint32_t firstId, DkResHandle const handles[], uint32_t numHandles);
void dkCmdBufBindImageDescriptorSet(DkCmdBuf obj, DkGpuAddr setAddr, uint32_t numDescriptors);
void dkCmdBufBindSamplerDescriptorSet(DkCmdBuf obj, DkGpuAddr setAddr, uint32_t numDescriptors);
void dkCmdBufBindRenderTargets(DkCmdBuf obj, DkImageView const* const colorTargets[], uint32_t numColorTargets, DkImageView const* depthTarget);
```

### Clears and discards

```c
void dkCmdBufClearColor(DkCmdBuf obj, uint32_t targetId, uint32_t clearMask, const void* clearData);
void dkCmdBufClearDepthStencil(DkCmdBuf obj, bool clearDepth, float depthValue, uint8_t stencilMask, uint8_t stencilValue);
void dkCmdBufDiscardColor(DkCmdBuf obj, uint32_t targetId);
void dkCmdBufDiscardDepthStencil(DkCmdBuf obj);
void dkCmdBufResolveDepthValues(DkCmdBuf obj);
```

### Inline data updates

```c
void dkCmdBufPushConstants(DkCmdBuf obj, DkGpuAddr uboAddr, uint32_t uboSize, uint32_t offset, uint32_t size, const void* data);
void dkCmdBufPushData(DkCmdBuf obj, DkGpuAddr addr, const void* data, uint32_t size);
```

**Recommended reads**:
- [vkCmdPushConstants](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPushConstants.html) (in Nvidia hardware and in deko3d; *all* uniform buffers have push constant semantics, and their size can be up to 64 KiB)

### Tiled cache management

```c
void dkCmdBufTiledCacheOp(DkCmdBuf obj, DkTiledCacheOp op);
void dkCmdBufSetTileSize(DkCmdBuf obj, uint32_t width, uint32_t height);
```

## The graphics pipeline

General information about special Nvidia 2nd-gen Maxwell features can be found in this page: [Maxwell GM204 OpenGL extensions](https://developer.nvidia.com/content/maxwell-gm204-opengl-extensions)

### Draw calls

```c
struct DkDrawIndirectData;
struct DkDrawIndexedIndirectData;
void dkCmdBufDraw(DkCmdBuf obj, DkPrimitive prim, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
void dkCmdBufDrawIndirect(DkCmdBuf obj, DkPrimitive prim, DkGpuAddr indirect);
void dkCmdBufDrawIndexed(DkCmdBuf obj, DkPrimitive prim, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
void dkCmdBufDrawIndexedIndirect(DkCmdBuf obj, DkPrimitive prim, DkGpuAddr indirect);
```

**Recommended reads**:
- [vkCmdDraw](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDraw.html)
- [vkCmdDrawIndirect](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndirect.html)
- [vkCmdDrawIndexed](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndexed.html)
- [vkCmdDrawIndexedIndirect](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndexedIndirect.html)
- [VkDrawIndirectCommand](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkDrawIndirectCommand.html)
- [VkDrawIndexedIndirectCommand](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkDrawIndexedIndirectCommand.html)

### Vertex specification

- Attributes
- Vertex buffers
- Index buffers (optional)

```c
struct DkVtxAttribState;
struct DkVtxBufferState;
void dkCmdBufBindVtxAttribState(DkCmdBuf obj, DkVtxAttribState const attribs[], uint32_t numAttribs);
void dkCmdBufBindVtxBufferState(DkCmdBuf obj, DkVtxBufferState const buffers[], uint32_t numBuffers);
void dkCmdBufBindVtxBuffers(DkCmdBuf obj, uint32_t firstId, DkBufExtents const buffers[], uint32_t numBuffers);
void dkCmdBufBindIdxBuffer(DkCmdBuf obj, DkIdxFormat format, DkGpuAddr address);
void dkCmdBufSetPrimitiveRestart(DkCmdBuf obj, bool enable, uint32_t index);
```

### Vertex programmable processing stages

- Vertex shader
- Tessellation (optional)
	- Tessellation control shader (optional)
	- Fixed function tessellator
	- Tessellation evaluation shader
- Geometry shader (optional)

```c
void dkCmdBufSetPatchSize(DkCmdBuf obj, uint32_t size);
void dkCmdBufSetTessOuterLevels(DkCmdBuf obj, float level0, float level1, float level2, float level3);
void dkCmdBufSetTessInnerLevels(DkCmdBuf obj, float level0, float level1);
```

**Recommended reads**:
- [Vertex Shader](https://www.khronos.org/opengl/wiki/Vertex_Shader)
- [Tessellation](https://www.khronos.org/opengl/wiki/Tessellation)
	- [Tessellation Control Shader](https://www.khronos.org/opengl/wiki/Tessellation_Control_Shader)
	- [Tessellation Evaluation Shader](https://www.khronos.org/opengl/wiki/Tessellation_Evaluation_Shader)
- [Geometry Shader](https://www.khronos.org/opengl/wiki/Geometry_Shader)
	- [NV_geometry_shader_passthrough](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_geometry_shader_passthrough.txt) (not yet supported in the shader compiler)

### Vertex post-processing

- ~~*(Transform feedback)*~~ - not supported in deko3d, use compute shaders instead (feature under consideration but its inclusion in future versions is not guaranteed)
- Viewport routing and mask selection (gl_ViewportIndex and gl_ViewportMask[], latter not yet supported in the shader compiler)
- Viewport swizzling
- Clipping and depth clamping
- User-defined clipping
- Perspective divide
- Viewport transform

```c
struct DkViewportSwizzle;
struct DkViewport;
void dkCmdBufSetViewportSwizzles(DkCmdBuf obj, uint32_t firstId, DkViewportSwizzle const swizzles[], uint32_t numSwizzles);
void dkCmdBufSetViewports(DkCmdBuf obj, uint32_t firstId, DkViewport const viewports[], uint32_t numViewports);
```

**Recommended reads**:
- [Vertex Post-Processing](https://www.khronos.org/opengl/wiki/Vertex_Post-Processing)
- [ARB_shader_viewport_layer_array](https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shader_viewport_layer_array.txt) (gl_ViewportIndex)
- [NV_viewport_array2](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_viewport_array2.txt) (gl_ViewportMask[], not yet supported in the shader compiler)
- [NV_viewport_swizzle](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_viewport_swizzle.txt)
- [gl_ClipDistance](https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_ClipDistance.xhtml) (in deko3d all 8 clip distances are enabled; however their default value if they're not written to is 0.0, meaning they have no effect)
- [ARB_depth_clamp](https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_depth_clamp.txt)
- [NV_depth_buffer_float](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_depth_buffer_float.txt) (note the section on storing values outside [0,1] which can stem from negative ranges in glDepthRange (i.e. viewport near/far are swapped), which is supported in deko3d)

### Rasterization

- Face culling
- Scissor test (yes, [Nvidia does it here](https://www.gamedev.net/forums/topic/420841-scissor-test-before-or-after-/))

Types of primitive:

- Point
- Line
- Polygon (triangle)

Special features:

- Fill rectangle
- Legacy smoothing
- Depth bias
- Multisampling
- Conservative rasterization (not yet supported)

```c
struct DkRasterizerState;
struct DkMultisampleState;
struct DkSampleLocation;
void dkCmdBufBindRasterizerState(DkCmdBuf obj, DkRasterizerState const* state);
void dkCmdBufSetPointSize(DkCmdBuf obj, float size);
void dkCmdBufSetLineWidth(DkCmdBuf obj, float width);
void dkCmdBufSetDepthBias(DkCmdBuf obj, float constantFactor, float clamp, float slopeFactor);
void dkCmdBufBindMultisampleState(DkCmdBuf obj, DkMultisampleState const* state);
void dkMultisampleStateSetLocations(DkMultisampleState* obj, DkSampleLocation const* locations, uint32_t numLocations);
```

**Recommended reads**:
- [Primitive Assembly](https://www.khronos.org/opengl/wiki/Primitive_Assembly)
	- [Face Culling](https://www.khronos.org/opengl/wiki/Face_Culling)
- [Scissor Test](https://www.khronos.org/opengl/wiki/Scissor_Test)
- [VkPipelineRasterizationStateCreateInfo](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkPipelineRasterizationStateCreateInfo.html)
- [NV_fill_rectangle](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_fill_rectangle.txt)
- [vkCmdSetDepthBias](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthBias.html)
- [Multisampling](https://www.khronos.org/opengl/wiki/Multisampling)
	- [EXT_raster_multisample](https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_raster_multisample.txt) (General target-independent rasterization)
	- [NV_framebuffer_mixed_samples](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_framebuffer_mixed_samples.txt) (Higher MSAA mode for depth buffer, which afterwards results in coverage reduction)
	- [NV_sample_locations](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_sample_locations.txt) (programmable sample locations)
- [NV_conservative_raster](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_conservative_raster.txt) (not yet supported by deko3d)
	- [NV_conservative_raster_dilate](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_conservative_raster_dilate.txt)

### Fragment early tests

- Early depth test
- Post depth coverage

**Recommended reads**:
- [Early Fragment Test](https://www.khronos.org/opengl/wiki/Early_Fragment_Test)
- [EXT_post_depth_coverage](https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_post_depth_coverage.txt)

### Fragment shader

Per-fragment vs per-sample invocation, gl_SampleID/gl_SamplePosition

**Recommended reads**:
- [Fragment Shader](https://www.khronos.org/opengl/wiki/Fragment_Shader)
- [ARB_sample_shading](https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_sample_shading.txt)

### Sample processing

- Multisample operations
	- Sample mask
	- Alpha to coverage
	- Coverage modulation
- Alpha test
- Depth bounds test
- Late depth and stencil test
- (Multisample) Coverage to color

```c
struct DkColorState;
struct DkDepthStencilState;
void dkCmdBufBindColorState(DkCmdBuf obj, DkColorState const* state);
void dkCmdBufBindDepthStencilState(DkCmdBuf obj, DkDepthStencilState const* state);
void dkCmdBufSetSampleMask(DkCmdBuf obj, uint32_t mask);
void dkCmdBufSetCoverageModulationTable(DkCmdBuf obj, float const table[16]);
void dkCmdBufSetAlphaRef(DkCmdBuf obj, float ref);
void dkCmdBufSetDepthBounds(DkCmdBuf obj, bool enable, float near, float far);
void dkCmdBufSetStencil(DkCmdBuf obj, DkFace face, uint8_t mask, uint8_t funcRef, uint8_t funcMask);
```

**Recommended reads**:
- [glSampleMask](https://www.khronos.org/opengl/wiki/GLAPI/glSampleMask)
- [VkPipelineMultisampleStateCreateInfo](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkPipelineMultisampleStateCreateInfo.html)
- [NV_alpha_to_coverage_dither_control](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_alpha_to_coverage_dither_control.txt)
- [NV_framebuffer_mixed_samples](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_framebuffer_mixed_samples.txt) (note the section about coverage modulation)
- [Per-Sample Processing](https://www.khronos.org/opengl/wiki/Per-Sample_Processing)
	- [Alpha test](https://www.khronos.org/opengl/wiki/Transparency_Sorting#Alpha_test) ("legacy" OpenGL feature, but natively supported by GPU)
	- [EXT_depth_bounds_test](https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_depth_bounds_test.txt)
	- [Depth Test](https://www.khronos.org/opengl/wiki/Depth_Test)
	- [Stencil Test](https://www.khronos.org/opengl/wiki/Stencil_Test)
- [NV_fragment_coverage_to_color](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_fragment_coverage_to_color.txt)
- [Anti-aliased Alpha Test: The Esoteric Alpha To Coverage](https://medium.com/@bgolus/anti-aliased-alpha-test-the-esoteric-alpha-to-coverage-8b177335ae4f)

### Sample output

- Blending
- Logical operations
- Write masks

```c
struct DkColorState;
struct DkDepthStencilState;
struct DkBlendState;
struct DkColorWriteState;
void dkCmdBufBindColorState(DkCmdBuf obj, DkColorState const* state);
void dkCmdBufBindDepthStencilState(DkCmdBuf obj, DkDepthStencilState const* state);
void dkCmdBufBindBlendStates(DkCmdBuf obj, uint32_t firstId, DkBlendState const states[], uint32_t numStates);
void dkCmdBufBindColorWriteState(DkCmdBuf obj, DkColorWriteState const* state);
void dkCmdBufSetBlendConst(DkCmdBuf obj, float red, float green, float blue, float alpha);
```

**Recommended reads**:
- [Blending](https://www.khronos.org/opengl/wiki/Blending)
- [Logical Operation](https://www.khronos.org/opengl/wiki/Logical_Operation)
- [Write Mask](https://www.khronos.org/opengl/wiki/Write_Mask)

## The compute pipeline

```c
struct DkDispatchIndirectData;
void dkCmdBufDispatchCompute(DkCmdBuf obj, uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ);
void dkCmdBufDispatchComputeIndirect(DkCmdBuf obj, DkGpuAddr indirect);
```

## The transfer pipeline

```c
void dkCmdBufCopyBuffer(DkCmdBuf obj, DkGpuAddr srcAddr, DkGpuAddr dstAddr, uint32_t size);
void dkCmdBufCopyImage(DkCmdBuf obj, DkImageView const* srcView, DkBlitRect const* srcRect, DkImageView const* dstView, DkBlitRect const* dstRect, uint32_t flags);
void dkCmdBufBlitImage(DkCmdBuf obj, DkImageView const* srcView, DkBlitRect const* srcRect, DkImageView const* dstView, DkBlitRect const* dstRect, uint32_t flags, uint32_t factor);
void dkCmdBufResolveImage(DkCmdBuf obj, DkImageView const* srcView, DkImageView const* dstView);
void dkCmdBufCopyBufferToImage(DkCmdBuf obj, DkCopyBuf const* src, DkImageView const* dstView, DkBlitRect const* dstRect, uint32_t flags);
void dkCmdBufCopyImageToBuffer(DkCmdBuf obj, DkImageView const* srcView, DkBlitRect const* srcRect, DkCopyBuf const* dst, uint32_t flags);
```
