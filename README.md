# 凸 deko3d ( ͡° ͜ʖ ͡°)

![deko3d logo](res/logo-300w.png)

deko3d is a brand new homebrew low level 3D graphics API targetting the Nvidia Tegra X1 processor found inside the Nintendo Switch. It provides the following features:

- Low level and explicit API reminiscent of Vulkan®: deko3d is modeled after popular low level concepts such as devices, queues, command buffers, and much more. deko3d thus provides a window into the inner workings of the GPU.
- Low overhead and explicit resource management: deko3d lets developers take control of resource management, and make decisions that make most sense in each case. In addition, care was taken to avoid unnecessary dynamic memory allocations and other undesired behavior that can result in degraded application performance.
- Control over command generation: deko3d grants developers the ability to manage and record command lists that can be submitted to the GPU, as well as the ability to maintain multiple parallel queues of command processing, each with its own internal (GPU-managed) state.
- Ease of use and convenience: despite being a low level API, care was taken to avoid introducing arbitrary restrictions in the API (such as giant immutable state objects or overengineered abstractions) that do not correspond to how the hardware works. deko3d aims to expose every useful thing that the GPU can do, in the most straightforward way possible. Between pedanticness and programmer happiness, the latter was selected.
- Compile-time resource management: deko3d provides and promotes a workflow involving the compilation and conversion of assets such as shaders or graphics at compile time, therefore avoiding runtime resource waste.
- Debug version of the library, providing parameter and state checking, as well as warning/error messages. The release version of the library omits validation checks and is compiled with extra optimization flags for maximum speed.
- Ability to use a plain C version of the API, as well as a rich C++ wrapper inspired by Vulkan-Hpp.

In conclusion, deko3d is the homebrew answer to Nintendo and Nvidia's proprietary NVN graphics API. deko3d is the culmination of a year-and-a-half long (and still ongoing!) reverse engineering effort centered around NVN, which has in turn heavily influenced its design and implementation. The source code of deko3d is intended to also serve a double purpose as a public example of how to use an Nvidia GPU, as well as aiming to be an accurate-as-possible public repository of register definitions and other hardware information.
