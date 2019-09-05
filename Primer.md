# deko3d Primer - A Small Introduction

## Introduction

deko3d is a low level graphics programming API for use in Nintendo Switch homebrew (along with devkitA64 and libnx), taking inspiration from Vulkan and drawing many similarities stemming from a year-long-plus reverse engineering (RE) effort centered around the official NVN library present in official software.

This primer is meant to show potential users of this library around the key foundations of its design, as well as several usage tips and hints that have been discovered throughout the RE and development process.

Familiarity with graphics APIs (mostly Vulkan and OpenGL) and 3D graphics programming in general are indicated as basic requirements to make the most out of deko3d and be able to understand this primer.

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
	- `DkScissor`
	- `DkRasterizerState`
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

### Error checking: Debug vs Release

## Objects

### Devices (`DkDevice`)

### Memory Blocks (`DkMemBlock`)

### Command Buffers (`DkCmdBuf`)

### Fences (`DkFence`)

### Queues (`DkQueue`)

### Shaders (`DkShader`)

#### The deko3d shader compiler

### Images (`DkImage`)

### Samplers (`DkSampler`)

### Swapchains (`DkSwapchain`)

## Functionality

### Setting up a queue

### Synchronization and presentation

### Binding shaders and shader resources

### Running compute jobs

### Setting up a 3D rendering pipeline

### Setting up vertex and index buffers

### Drawing 3D primitives

### Barriers and tiled cache

### Transferring data

### Render targets and tests

### Multisampling

### Fixed function fragment operations (blending)
