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

## Installation

Users don't need to do anything special on their own to install deko3d, as this is a core homebrew library in the `switch-dev` group supplied through [devkitPro pacman](https://devkitpro.org/wiki/devkitPro_pacman). However, users who haven't received deko3d because they set up their toolchain installation prior to its release may want to run these commands:

	(sudo) (dkp-)pacman -Syu
	(sudo) (dkp-)pacman -S --noconfirm --needed switch-dev

Nonetheless for documentation's sake it is pointed out that building deko3d from source requires building and installing [dekotools](https://github.com/fincs/dekotools). No support nor precompiled binaries are provided for these tools though, since users are expected and encouraged to use the prebuilt binaries on devkitPro's pacman repository. Developers wishing to contribute to deko3d are kindly invited to talk to us at devkitPro first, through the usual hacking channels :)

## Preemptively Answered Questions (PAQ)

### Can I use the shader compiler inside my program?

The short answer is no: deko3d's shader compiler (UAM) is a PC tool that runs at compile time as opposed to at runtime, and this is a *main design goal* of deko3d.

The long answer is: in order to provide a lightweight API that doesn't make compromises, certain hindersome aspects of typical cross-platform graphics APIs such as forceful runtime compilation of shaders were left out. This doesn't mean that it is *impossible* to dynamically generate shaders; it only means that deko3d only provides the means to use native code. Users could target the native shader instruction set in their JIT engines; or even write a new shader compiler from scratch that can be made to run on the Switch. (If anyone is willing to make that happen, please talk to us as we would be *very* interested in it!)

### Is this a clone of NVN?

deko3d is not a clone of NVN. However, deko3d was developed as a result of NVN reverse engineering work, and as such it borrows many implementation ideas. Essentially, deko3d was designed to fulfill the same place in homebrew that NVN does in official software.

### Why make yet another API instead of reimplementing NVN?

For the same reason that libnx is not a carbon copy of the official SDK. deko3d is intended to be an independent homebrew thing, and it is designed specifically to cater to homebrew needs, workflows and standards.

### Why isn't this a Vulkan driver?

Nvidia GPUs are poorly understood pieces of hardware due to the vendor's refusal to publish hardware documentation, specifically 3D class methods and the shader ISA. For this reason, reverse engineering NVN and implementing a homebrew counterpart (deko3d) is a precondition for doing anything that requires a better understanding of the hardware. This process has shed a considerable amount of light on this topic, and there's even the chance of a homebrew Vulkan driver (running on top of deko3d) being developed in the future. In addition, the Vulkan API is designed in a way that adds artificial restrictions and imposes workflows that do not correspond to how Nvidia hardware works; and many Nvidia hardware features aren't exposed in an optimal way (if at all). deko3d on the other hand tries to be as honest as possible with the reality of Nvidia hardware.

### Why and when should I use deko3d instead of switch-mesa's OpenGL driver?

deko3d is a lightweight low level API; several orders of magnitude more lightweight than switch-mesa. A typical application using switch-mesa will usually be several megabytes in size (around 8 MB or larger); while a typical application using deko3d will clock at around 500 KB or even smaller. Also, deko3d has native support for many Kepler/Maxwell performance-enhancing hardware features that nouveau doesn't have such as Zcull, the tiled cache, compressed render targets (decompressed prior to presentation), several optimizations in the shader compiler, and more.

On the other hand, switch-mesa provides a standard OpenGL API, while deko3d is a Switch homebrew specific API. Users would be advised to stick to switch-mesa (or libraries using it such as SDL) if a standard API is desired for cross-platform support. Alternatively users may want to implement a separate deko3d backend for their applications, however this also increases the amount of maintenance work.

### Is deko3d faster than OpenGL?

As mentioned previously, deko3d has native support for several Kepler/Maxwell performance-enhancing features that nouveau hasn't been updated to use; mostly due to hardware obscurity (which has been mostly solved by reverse engineering NVN). In addition, there is substantially lower CPU overhead compared to OpenGL, since deko3d is a low-level graphics API. The same reasons that would prompt users to switch to Vulkan for the purposes of reducing CPU/GPU overhead apply to deko3d as well.

### Why does this project exist instead of contributing to nouveau, such as by improving the existing code or writing a Vulkan driver?

The way mesa and nouveau are designed and developed makes it difficult to add support for certain missing hardware features. Many things have changed since the early versions of Nvidia architectures nouveau originally started with; and adding support for newer stuff has quickly become an uphill battle, especially due to the reclocking issue which renders nouveau virtually unusable (and untestable) on anything newer than Kepler, as well as the general lack of Nvidia hardware documentation. As an unfortunate result, Vulkan for nouveau is essentially out of the question without major efforts. In addition, nouveau's Linux kernel interface wasn't designed with low level APIs in mind, further hindering a potential Vulkan implementation.

The NVN reverse engineering route has resulted in much cleaner, straightforward, and fruitful results - the homebrew community has received a more suitable and lightweight low level API, and without needing to depend on nouveau's uncertain future. In fact, at the time of writing, it is unfortunately not clear if nouveau will ever receive Vulkan support.

With that said, deko3d is released under a permissive zlib license. All developers are free to study and use it as reference material, including nouveau developers; in fact they are very much welcome to do so. Specifically, there are certain improvements in the shader compiler (which *is* based off mesa/nouveau sources) which we would be more than happy to upstream; as well as assisting with anything else we might be asked about.
