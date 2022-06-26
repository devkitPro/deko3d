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
	- `DkVariable`
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
	- `DkImageRect`
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
typedef void (*DkDebugFunc)(void* userData, const char* context, DkResult result, const char* message);
typedef DkResult (*DkAllocFunc)(void* userData, size_t alignment, size_t size, void** out);
typedef void (*DkFreeFunc)(void* userData, void* mem);

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
`userData` | NULL      | User specified data to be passed to the callbacks
`cbDebug`  | NULL      | Optional callback used by the debug version of the library.
`cbAlloc`  | NULL      | Optional callback used when deko3d needs to allocate memory
`cbFree`   | NULL      | Optional callback used when deko3d needs to free allocated memory
`flags`    | See below | Device creation flags (see below)

`DkDeviceFlags_*`    | Default | Description
---------------------|---------|--------------------------------------------------
`DepthZeroToOne`     | ✓       | Clip space Z is [0, 1] like Vulkan
`DepthMinusOneToOne` |         | Clip space Z is [-1, 1] like OpenGL
`OriginUpperLeft`    | ✓       | Image rows are stored sequentially from top to bottom, with 0.0 corresponding to the top edge of the image and 1.0 (or the image height if non-normalized) corresponding to the bottom
`OriginLowerLeft`    |         | Image rows are stored sequentially from bottom to top, with 0.0 corresponding to the bottom edge of the image and 1.0 (or the image height if non-normalized) corresponding to the top

The debugging version of the library can use an optional callback (`cbDebug`). There are two situations in which deko3d makes usage of the debug callback:

- Warnings: a suboptimal situation is detected, and deko3d calls the callback with `DkResult_Success` as the result parameter. This is a non-fatal invocation which keeps the application running afterwards.
- Errors: a certain parameter/state validation check fails, or an unrecoverable system/GPU error occurs. In this situation the result parameter is different from `DkResult_Success`, and the debug callback is expected to not return.

In both cases, the `context` parameter is a string indicating the name of the most recently called deko3d entrypoint, while `message` is a string containing additional information to be used for debugging purposes.

If no debugging callback has been supplied (or if the callback returns during error handling), deko3d outputs information to stderr. In the case of errors, an error message is displayed on the screen using the error applet, and the application forcefully closes back to the HOME menu.

In the release version of deko3d, warnings and parameter/state validation don't exist. However, if unrecoverable errors happen, a Fatal system error is forcefully thrown. The release version of deko3d does not make use of `cbDebug` *at all*.

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
- `dkMemBlockGetGpuAddr` returns the *generic* GPU address of the memory block, or `DK_GPU_ADDR_INVALID` if the GPU has no access to it (i.e. `DkMemAccess_None`).

Memory blocks with GPU access can end up having several different mappings in the GPU's address space, depending on how the memory is to be used:
- A *generic* mapping will always be created, suitable for non-image purposes.
- If `DkMemBlockFlags_Code` is specified, the *generic* mapping will be placed within a special *code segment* in the GPU's address space, which is absolutely **necessary** for shader code to be accessible by the GPU. Up to 4 GiB of address space are reserved for the *code segment*.
- If `DkMemBlockFlags_Image` is specified, two extra mappings will be created with the necessary internal GPU memory attributes for normal and "compressed" image accesses respectively.

> **Warning**: Due to a hardware bug the last `DK_SHADER_CODE_UNUSABLE_SIZE` bytes of a memory block are unusable for storing shader code.

> **Warning**: Currently deko3d is unable to reuse previously-reserved mappings inside the *code segment* even if they're freed. Users are advised to reuse old code memory blocks instead of freeing them.

> **Note**: Memory blocks with CPU cacheability (`DkMemBlockFlags_CpuCached`) can be used. `dkMemBlockFlushCpuCache` can be used to flush the CPU-side cache (i.e. clean+invalidate), and after that point all writes done on the CPU become visible by GPU. However if the memory block also has GPU cacheability (`DkMemBlockFlags_GpuCached`) care must be taken so that the GPU side caches are invalidated before accessing the memory. There is also no support for invalidating the CPU-side cache as it is a dangerous (and privileged!) operation; so users should **avoid** using CpuCached memory for GPU->CPU communication.

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
void dkCmdBufCallList(DkCmdBuf obj, DkCmdList list);
//...
```

Command buffers (`DkCmdBuf`) allow users to record command lists that can be submitted to a queue (`DkQueue`). A command buffer can take in slices of backing memory, onto which the raw command data will be recorded in a format the the GPU can understand. A user can specify the current slice of backing memory to use with the `dkCmdBufAddMemory` function. It is allowed to call this function in the middle of recording, and the command list that was being recorded will continue at the new location.

> **Note**: Memory added with `dkCmdBufAddMemory` must be aligned to `DK_CMDMEM_ALIGNMENT`, and its size must also be a multiple of `DK_CMDMEM_ALIGNMENT`. In addition, the memory block must have GPU visibility and the CPU-side cache disabled. It is strongly recommended to use `DkMemBlockFlags_GpuCached | DkMemBlockFlags_CpuUncached` as the flags.

Command buffers maintain and keep ownership of internal bookkeeping memory that is used to fully define a command list, including but not limited to the entire history of memory regions used or fences referenced. When `dkCmdBufFinishList` is called, the currently recorded command list is signed off and a handle to it (`DkCmdList`) is returned. This function can be called as many times as necessary in order to build as many command lists as desired out of the user provided backing memory. Command lists can be submitted to a queue as many times as desired as well. Command list handles will remain valid as long as their parent command buffer continues existing as an active object, the memory slices they're referencing haven't been overwritten by other command lists, or until `dkCmdBufClear` is called. When `dkCmdBufClear` is called, all command lists that have been recorded with the command buffer are destroyed, their handles are invalidated (freeing up internal bookkeeping memory), and the current memory slice is rolled back to the beginning.

If the current slice of backing memory runs out of space during recording, either of two things can happen:
- If the user didn't supply a `cbAddMem` callback, a fatal error is raised.
- Otherwise, the `cbAddMem` callback is called, which is expected to in turn call `dkCmdBufAddMemory` in order to resolve the situation. If the callback doesn't add enough memory, a fatal error is raised.

This mechanism is intended to be used for command buffers backed by dynamic memory, so that they can refill themselves with fresh new memory as needed.

Command lists can be reused by other command lists as well. When `dkCmdBufCallList` is called, a reference to the specified `DkCmdList` is inserted into the currently recording command list. This is useful for recording a certain set of commands only once, and afterwards calling this sublist as many times as desired from a parent command list. This also means that sublists need to stay valid for the total lifetime of their parent(s).

`DkCmdBuf` objects are *externally synchronized*; in other words, they are not in charge of synchronization themselves and thus multiple threads cannot use the same command buffer at the same time. The intended workflow in a multithreaded application is to have multiple worker threads recording commands independently (each fitted with its own command buffer), and have the parent thread collect and submit all the `DkCmdList` handles from the worker threads.

### Fences (`DkFence`)

```c
struct DkFence;
DkResult dkFenceWait(DkFence* obj, int64_t timeout_ns);
```

Fences (`DkFence`) are opaque structs that contain GPU synchronization information, used to determine when work submitted to the GPU has finished executing. Each time they're scheduled to be signaled in a queue (`DkQueue`), either directly or indirectly through a command list, their contents are updated. Fences can be waited on by the GPU or CPU (using the `dkFenceWait` function). When a fence is waited on, the waiter (CPU or GPU) will be kept blocked until the point in which it's signaled (hence marking the completion of dependent work).

Usually fences will be used in a signaling command prior to being waited on. If fences are to be potentially waited on before they're signaled (e.g. if they're used to wait on previous work, with no previous work having been submitted yet), they should be initialized to zero in order to ensure that any initial waits will correctly have no effect.

> **Warning**: Fence wait/signal commands recorded to a command list keep a pointer to the fence struct in the command buffer's bookkeeping memory. Please make sure the struct remains at the same valid memory address for the lifetime of the command list handle; otherwise submitting the command list handle to a queue will result in undefined behavior.

### Variables (`DkVariable`)

```c
struct DkVariable;
void dkVariableInitialize(DkVariable* obj, DkMemBlock mem, uint32_t offset);
uint32_t dkVariableRead(DkVariable const* obj);
void dkVariableSignal(DkVariable const* obj, DkVarOp op, uint32_t value);
```

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

Queues (`DkQueue`) are used to asynchronously execute work on the GPU. A queue is basically a list of in-flight work items that the GPU will execute. Work items (in the form of commands or fence/image operations) can be submitted to the queue; and the GPU will pick them up and start executing them *at some point*. The order in which submitted work items are executed is internally decided by the GPU, and can only be controlled by fencing commands and `dkCmdBufBarrier`. Commands within command lists essentially update internal state of the GPU, and launch different tasks such as drawing primitives or dispatching compute jobs. Once again, the order in which commands are executed, as well as the dependencies between different jobs, needs to be manually scheduled using aforementioned fences and barriers; otherwise the GPU might start tackling work items out of order, or before required data from a previous step is completed. Each queue executes work items independently of any other.

Each queue has its own internal state and bookkeeping memory used for diverse purposes. This includes things such as bound render targets, shaders, uniform/vertex/index buffers, textures/samplers, descriptor sets, all the various different state structs, etc; and can be updated by using the appropriate commands. The state of each queue is completely independent from any other.

Field                      | Default                                  | Description
---------------------------|------------------------------------------|------------
`device`                   | N/A                                      | Parent device
`flags`                    | See below                                | Queue creation flags (see below)
`commandMemorySize`        | `DK_QUEUE_MIN_CMDMEM_SIZE`               | Internal command memory size in bytes (must be at least `DK_QUEUE_MIN_CMDMEM_SIZE`)
`flushThreshold`           | `DK_QUEUE_MIN_CMDMEM_SIZE/8`             | Threshold for flushing internal command memory (must be at least `DK_MEMBLOCK_ALIGNMENT` and not more than `commandMemorySize`)
`perWarpScratchMemorySize` | `4*DK_PER_WARP_SCRATCH_MEM_ALIGNMENT`    | Scratch memory allocated to each warp in bytes, must be a multiple of `DK_PER_WARP_SCRATCH_MEM_ALIGNMENT` (can be 0 if scratch memory is not needed)
`maxConcurrentComputeJobs` | `DK_DEFAULT_MAX_COMPUTE_CONCURRENT_JOBS` | For compute-capable queues: maximum number of concurrent compute dispatch jobs (must be at least 1), ignored otherwise

`DkQueueFlags_*` | Default | Description
-----------------|---------|------------
`Graphics`       | ✓       | The queue can execute graphics commands
`Compute`        | ✓       | The queue can execute compute commands
`MediumPrio`     | ✓       | The queue has medium priority
`HighPrio`       |         | The queue has high priority
`LowPrio`        |         | The queue has low priority
`EnableZcull`    | ✓       | Zcull is enabled
`DisableZcull`   |         | Zcull is disabled

During creation, the intended usage of the queue can be specified. Essentially this entails enabling or disabling support for submitting command lists containing graphics or compute commands. If a certain usage bit is not specified, the queue does not reserve resources required for processing said types of commands. Submitting command lists containing commands of a certain type on a queue that has not been created with the corresponding usage bit results in undefined behavior. For example, it is illegal to run `dkCmdBufDispatchCompute` on a queue that does not have the `DkQueueFlags_Compute` flag set. Note that all queues are capable of running transfer commands.

Queues internally generate commands for various purposes. The size of the memory fed to the internal command buffer used for this purpose can be controlled with the `commandMemorySize` field. The memory is managed as a ring buffer divided in a certain number of parts, guarded by a fence at every boundary acting as a checkpoint. deko3d will automatically wait on the required fences before overwriting previously written internal command data, however this entails blocking the CPU for periods of time while the GPU is processing work items. In order to mitigate this, deko3d automatically flushes the queue after a certain threshold (`flushThreshold`) of internal command memory has been consumed; so that the GPU can start tackling its pending work items, and by the time the fence must be waited on it's (hopefully) already signaled with the wait finishing immediately without blocking. A smaller threshold results in more frequent automatic flushes, while a larger threshold results in less flushes but more frequent situations in which the CPU is blocked waiting for the GPU; so users should exercise caution.

Queues manage their list of work items in a lazy fashion. In other words, the work items are enqueued in a list that is submitted to the GPU as a single batch in one go, and said submission does not actually happen until one of the following conditions are met:
- `dkQueueFlush` is called, which signs off the batch of work items and submits it to the GPU. Note that `dkQueuePresentImage` internally calls `dkQueueFlush`.
- Space runs out in the list, which necessitates a flush.
- The flushing threshold for internal command memory (`flushThreshold`) is reached, which results in an automatic flush.

After the queue is flushed, deko3d inserts a barrier that invalidates the image, shader, descriptor and L2 caches - in fact this is the very first work item that will be executed the *next* time the queue is flushed. This makes it possible to update graphical resources on the CPU such as vertex buffers or image/sampler descriptor sets between batches of work items submitted to the queue.

If for some reason the GPU encounters an error while processing work items, the queue enters error state. This can be detected using `dkQueueIsInErrorState`. Once a queue enters error state it is completely toast, and the only legal operation on it is `dkQueueDestroy`. In addition, the debugging version of deko3d is able to print information about the GPU error using the warning mechanism provided by the debug callback.

> **Warning**: Even though deko3d can recover from GPU errors, the operating system seems to be programmed to kill processes that have crashed the GPU a few seconds afterwards. Currently it is not known if this behavior of the OS can be disabled. As a result, it is advised to avoid crashing the GPU if possible; otherwise a few seconds of grace are available in order to e.g. commit unsaved changes to persistent storage.

The GPU can be instructed to wait on a fence using the `dkQueueWaitFence` function. Likewise, a fence can be signaled using `dkQueueSignalFence`. If the `flush` parameter in this function is set to `true` the GPU flushes any dirty cache lines to memory; allowing other observers (such as the CPU) to see the result of writes performed by the GPU up to the point when the fence is signaled. This is important in case e.g. the CPU needs to read the result of GPU work that is performed on a memory block that is set to `DkMemBlockFlags_GpuCached`, such as compute shader writes.

Command lists are submitted to the queue using `dkQueueSubmitCommands`. Command lists can also contain fencing operations, which are described in the previous paragraph and behave in the same way. The command list handle is only used during the call to this function, and it is legal to destroy it afterwards with `dkCmdBufClear`. The only requirement is that command memory submitted to a queue must remain valid (i.e. not freed or overwritten by something else) until it is fully guaranteed that the GPU has finished executing the commands inside.

Usually applications will want to use fences to synchronize themselves with the GPU; however sometimes it is desirable to completely wait for *all* submitted work items to be done executing (such as when potentially in-flight resources are to be destroyed). This can be done using the `dkQueueWaitIdle` function; which is shorthand for signaling a (temporary) fence, flushing the queue and waiting on the fence. This function should only be used sparingly due to its overhead.

The functions `dkQueueAcquireImage` and `dkQueuePresentImage` are used to tie a queue to a swapchain used for presentation. For more information look at the section dealing with swapchains.

### Shaders (`DkShader`)

```c
struct DkShaderMaker;
void dkShaderInitialize(DkShader* obj, DkShaderMaker const* maker);
bool dkShaderIsValid(DkShader const* obj);
DkStage dkShaderGetStage(DkShader const* obj);
```

Shaders are programs that run on the GPU and process different stages of the programmable part of the pipeline. In deko3d they are represented by an opaque object called `DkShader`. These objects contain metadata that allow deko3d to set up shader execution on the GPU. deko3d accepts shaders in the DKSH format, which is a container that bundles together the raw shader machine code accepted directly by the GPU together with the aforementioned metadata.

> **Note**: deko3d **only** accepts native GPU code, i.e. SASS (Streaming Assembler) for Shader Model 5.3; which is the instruction set architecture (ISA) implemented by second-generation Maxwell GPUs. There is absolutely **no way** to use non-native shader languages which require runtime compilation such as GLSL or SPIR-V with deko3d. Users who need to JIT shaders at runtime **must** target the Maxwell 2nd gen ISA (SM53) in some way, shape or form instead of expecting deko3d to accept code written in a foreign language.

Field        | Default | Description
-------------|---------|-------------
`codeMem`    | N/A     | Memory block where shader code resides
`control`    | NULL    | Optional pointer to the DKSH control section (see the "DKSH format" section further below)
`codeOffset` | N/A     | Offset into the memory block where shader code resides; must be a multiple of `DK_SHADER_CODE_ALIGNMENT`
`programId`  | 0       | Index of the shader program to initialize

DKSH shaders can be loaded directly into a code memory block, with `codeMem`/`codeOffset` pointing to the exact place in the code memory block where the shader is loaded. Optionally, the *DKSH control section* can be loaded outside the code memory block - this is explained in the "DKSH format" section further below.

> **Note**: The DKSH format allows for multiple programs (i.e. entrypoints) within a single DKSH shader file, however currently the deko3d shader compiler does not support this feature. Users are advised to leave `programId` at its default value of 0.

`dkShaderIsValid` can be used to check whether a `DkShader` opaque object currently holds a valid shader. `dkShaderGetStage` in addition can be used to retrieve the stage at which the shader executes (such as the vertex or fragment stage).

`DkShader` opaque objects do not claim ownership of any data. They will remain valid as long as the underlying code memory they're referring to is not freed or overwritten by something else.

#### The deko3d shader compiler

In the usual deko3d workflow, shaders are developed in a high-level language and compiled to DKSH on the developer's machine by the build system. A PC tool called `uam` has been developed, and it is able to compile GLSL shaders to the native Maxwell ISA and produce DKSH files usable with deko3d.

```
Usage: uam [options] file
Options:
  -o, --out=<file>   Specifies the output deko3d shader module file (.dksh)
  -r, --raw=<file>   Specifies the file to which output raw Maxwell bytecode
  -t, --tgsi=<file>  Specifies the file to which output intermediary TGSI code
  -s, --stage=<name> Specifies the pipeline stage of the shader
                     (vert, tess_ctrl, tess_eval, geom, frag, comp)
  -v, --version      Displays version information
```

The dialect of GLSL accepted by UAM is essentially the same one that is parsed by mesa/nouveau, with several differences:

- The `DEKO3D` preprocessor symbol is defined, with a value of 100.
- UBO, SSBO, sampler and image bindings are **required to be explicit** (i.e. `layout (binding = N)`), and they have a one-to-one correspondence with deko3d bindings. Failure to specify explicit bindings will result in an error.
- There is support for 16 UBOs, 16 SSBOs, 32 "samplers" (combined image+sampler handle), and 8 images for each and every shader stage; with binding IDs ranging from zero to the corresponding limit minus one. However note that due to hardware limitations, only compute stage UBO bindings 0-5 are natively supported, while 6-15 are emulated as "SSBOs".
- Default uniforms outside UBO blocks are detected, however they are reported as an error due to lack of support in both DKSH and deko3d for retrieving the location of and setting these uniforms.
- `gl_FragCoord` always uses the Y axis convention specified in the flags during the creation of a deko3d device. `layout (origin_upper_left)` has no effect whatsoever and produces a warning, while `layout (pixel_center_integer)` is not supported at all and produces an error.
- Integer divisions and modulo operations with non-constant divisors decay to floating point division, and generate a warning. Well written shaders should avoid these operations for performance and accuracy reasons.
- 64-bit floating point divisions and square roots can only be approximated with native hardware instructions. This results in loss of accuracy, and as such these operations should be avoided, and they generate a warning as well.
- Transform feedback is not supported.
- GLSL shader subroutines (`ARB_shader_subroutine`) are not supported.
- There is no concept of shader linking. Separable programs (`ARB_separate_shader_objects`) are always in effect.

For more information, please read the UAM documentation.

#### DKSH format - loading control/code sections separately

```c
#define DKSH_MAGIC UINT32_C(0x48534B44) // DKSH

struct DkshHeader
{
	uint32_t magic; // DKSH_MAGIC
	uint32_t header_sz; // sizeof(DkshHeader)
	uint32_t control_sz;
	uint32_t code_sz;
	uint32_t programs_off;
	uint32_t num_programs;
};
```

DKSH files consist of two parts consecutively stored: a *control* section and a *code* section. The size of each section is always a multiple of 256 bytes. The control section contains metadata required by deko3d in order to set up the shader, while the code section contains raw shader code and other information required by hardware itself. deko3d allows the user to load the control section in a separate memory location, and the code section in the actual code memory block. This reduces GPU memory waste, since the GPU has no need whatsoever to read the DKSH control section.

In order to do this users need to parse a tiny bit of the DKSH format. The following algorithm shows how to do it:

- Open the DKSH file.
- Read the `DkshHeader` struct from the beginning of the file.
- Allocate a temporary buffer of size `control_sz`.
- Read the entire control section into this buffer (also from the beginning of the file).
- Allocate GPU code memory with size `code_sz` and alignment `DK_SHADER_CODE_ALIGNMENT`.
- Read the entire data section into the GPU code memory (continuing from where the control section left off).
- Close the DKSH file.
- Fill in `DkShaderMaker` so that `control` points to the temporary buffer containing the control section, while `codeMem`/`codeOffset` point to the GPU code memory containing the code section.
- Call `dkShaderInitialize` in order to parse and initialize the shader.
- Free the temporary buffer since deko3d doesn't need it anymore.

### Images (`DkImage`)

### Samplers (`DkSampler`)

### Swapchains (`DkSwapchain`)

## General commands

### Barriers and synchronization

```c
void dkCmdBufWaitFence(DkCmdBuf obj, DkFence* fence);
void dkCmdBufSignalFence(DkCmdBuf obj, DkFence* fence, bool flush);
void dkCmdBufWaitVariable(DkCmdBuf obj, DkVariable const* var, DkVarCompareOp op, uint32_t value);
void dkCmdBufSignalVariable(DkCmdBuf obj, DkVariable const* var, DkVarOp op, uint32_t value, DkPipelinePos pos);
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

### Counters

```c
void dkCmdBufReportCounter(DkCmdBuf obj, DkCounter type, DkGpuAddr addr);
void dkCmdBufReportValue(DkCmdBuf obj, uint32_t value, DkGpuAddr addr);
void dkCmdBufResetCounter(DkCmdBuf obj, DkCounter type);
```

For all counter types except `DkCounter_ZcullStats`, a report consists of two consecutive 64-bit values, the first one being the specified counter, and the second the timestamp at which the report was done.
`dkCmdBufReportValue` will write the specified value as well as the timestamp.
In the case of `DkCounter_ZcullStats`, it reports four 32-bit values relating to ZCull operations. `dkCmdBufResetCounter` resets the specified counter to 0. Some counters cannot be reset.

Note that all three functions entail a tiled cache flush.

### Command capture and replay

```c
void dkCmdBufBeginCaptureCmds(DkCmdBuf obj, uint32_t* storage, uint32_t max_words);
uint32_t dkCmdBufEndCaptureCmds(DkCmdBuf obj);
void dkCmdBufReplayCmds(DkCmdBuf obj, const uint32_t* words, uint32_t num_words);
```

Rendering complex scenes usually involves managing independent entities with changing GPU state. Said structs in deko3d are laid out in a format that is concise and easy to modify; however many need to be converted into raw GPU command data every time they're used in dkCmdBufBind\* functions. Depending on the situation, this may entail unnecessary CPU usage/bottlenecking.

deko3d provides a way to capture raw GPU commands into a user-provided buffer, and replay them with zero computational overhead. When `dkCmdBufBeginCaptureCmds` is called, the command buffer enters *capture mode*. In this mode, commands generated by the different functions are directly written into the user provided storage buffer. Afterwards `dkCmdBufEndCaptureCmds` can be called, and it returns the total number of raw GPU command words that have been captured. These command words can later be replayed in any command buffer using the `dkCmdBufReplayCmds` function. The effect of the replay will be equivalent to calling the original set of dkCmdBuf\* functions used during capture mode.

Please note that capture mode has limitations. In particular, running out of storage for the captured commands results in an unavoidable error (`cbAddMem` is ignored), and the following operations are disallowed:

- Command buffer management operations: `dkCmdBufAddMemory`, `dkCmdBufFinishList`, `dkCmdBufClear`
- Commands which need to store internal bookkeeping information:
  - Any command that uses or configures the compute pipeline
  - Fence commands: `dkCmdBufWaitFence`, `dkCmdBufSignalFence`
  - Indirect draw/dispatch commands: `dkCmdBufDrawIndirect`, `dkCmdBufDrawIndexedIndirect`, `dkCmdBufDispatchComputeIndirect`
  - `dkCmdBufBarrier` with `DkBarrier_Full` mode
  - `dkCmdBufCallList`

> **Warning**: `dkCmdBufReplayCmds` is essentially a backdoor that allows the user to inject arbitrary GPU commands into a command buffer. Please exercise caution and responsibility. Additionally, captured commands might not be compatible if they're replayed using a mismatched version of deko3d.

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
- Viewport routing and mask selection (gl_ViewportIndex and gl_ViewportMask[])
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
- Polygon and line stipples
- Depth bias
- Multisampling
- Conservative rasterization

```c
struct DkRasterizerState;
struct DkMultisampleState;
struct DkSampleLocation;
void dkCmdBufBindRasterizerState(DkCmdBuf obj, DkRasterizerState const* state);
void dkCmdBufSetPointSize(DkCmdBuf obj, float size);
void dkCmdBufSetLineWidth(DkCmdBuf obj, float width);
void dkCmdBufSetLineStipple(DkCmdBuf obj, bool enable, uint32_t factor, uint16_t pattern);
void dkCmdBufSetPolygonStipple(DkCmdBuf obj, uint32_t const pattern[32]);
void dkCmdBufSetConservativeRasterEnable(DkCmdBuf obj, bool enable);
void dkCmdBufSetConservativeRasterDilate(DkCmdBuf obj, float dilate);
void dkCmdBufSetSubpixelPrecisionBias(DkCmdBuf obj, uint32_t xbits, uint32_t ybits);
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
- [glLineStipple](https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glLineStipple.xml)
- [glPolygonStipple](https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glPolygonStipple.xml)
- [Multisampling](https://www.khronos.org/opengl/wiki/Multisampling)
	- [EXT_raster_multisample](https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_raster_multisample.txt) (General target-independent rasterization)
	- [NV_framebuffer_mixed_samples](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_framebuffer_mixed_samples.txt) (Higher MSAA mode for depth buffer, which afterwards results in coverage reduction)
	- [NV_sample_locations](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_sample_locations.txt) (programmable sample locations)
- [NV_conservative_raster](https://www.khronos.org/registry/OpenGL/extensions/NV/NV_conservative_raster.txt)
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
void dkCmdBufCopyImage(DkCmdBuf obj, DkImageView const* srcView, DkImageRect const* srcRect, DkImageView const* dstView, DkImageRect const* dstRect, uint32_t flags);
void dkCmdBufBlitImage(DkCmdBuf obj, DkImageView const* srcView, DkImageRect const* srcRect, DkImageView const* dstView, DkImageRect const* dstRect, uint32_t flags, uint32_t factor);
void dkCmdBufResolveImage(DkCmdBuf obj, DkImageView const* srcView, DkImageView const* dstView);
void dkCmdBufCopyBufferToImage(DkCmdBuf obj, DkCopyBuf const* src, DkImageView const* dstView, DkImageRect const* dstRect, uint32_t flags);
void dkCmdBufCopyImageToBuffer(DkCmdBuf obj, DkImageView const* srcView, DkImageRect const* srcRect, DkCopyBuf const* dst, uint32_t flags);
```
