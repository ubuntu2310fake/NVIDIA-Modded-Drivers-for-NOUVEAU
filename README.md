# Nouveau/NVK on Nvidia Proprietary Kernel Wrapper (PoC)

This is a Proof-of-Concept `LD_PRELOAD` wrapper that tricks Mesa's open-source userspace drivers (**Nouveau Gallium** for OpenGL and **NVK** for Vulkan) into initializing and running directly on top of the proprietary NVIDIA GPU kernel driver (`nvidia-drm` / `open-gpu-kernel-modules`).

Normally, these two worlds are completely incompatible: Mesa's open-source drivers require the open-source `nouveau` kernel module, while the proprietary user space driver requires the proprietary NVIDIA kernel driver.

## How the Magic Works

The wrapper intercepts DRM-related system calls (`ioctl`, `mmap`, `mmap64`) using `LD_PRELOAD` to perform the following translations:

1. **GPU Architecture Spoofing**: It dynamically reads your GPU's PCI device ID and model name from `/sys/class/drm/` and `/proc/driver/nvidia/gpus/` to determine the correct architecture (Volta, Turing, Ampere, Ada Lovelace). It then maps it to the matching Nouveau chipset ID and GPU class capabilities, tricking the userspace driver into thinking it is running on a compatible Nouveau hardware setup.
2. **Buffer Allocation Redirection (Proxy Node)**: Nouveau Allocations (`DRM_IOCTL_NOUVEAU_GEM_NEW`) are redirected to the Nvidia primary device node (`/dev/dri/card1` or `/dev/dri/card0`) using standard `DRM_IOCTL_MODE_CREATE_DUMB` calls. This allocates real GPU memory backed by actual Nvidia handles.
3. **Wayland WSI Export Support**: When the Wayland WSI layer attempts to export the image handles into DMA-BUF file descriptors via `DRM_IOCTL_PRIME_HANDLE_TO_FD`, the wrapper intercepts and redirects this to the primary Nvidia node. Since the handle is a real Nvidia dumb buffer, the kernel successfully exports it, enabling successful Wayland swapchain creation!
4. **Execution Interception**: Commands submitted to the GPU (`DRM_IOCTL_NOUVEAU_EXEC`) and VM bindings are bypassed/mocked to report success. 

> [!NOTE]
> Since GPU command submission (`NOUVEAU_EXEC`) is bypassed, no actual drawing operations take place on the hardware. The target buffer remains empty/unmodified, but the logic successfully passes all initialization checks, allocations, wayland surface integrations, and runs the rendering loop continuously without crashing. It functions as a highly advanced **Mock/Null Hybrid Driver** for reverse engineering.

## Setup & Compilation

### 1. Build the wrapper
Compile the wrapper into a shared library:
```bash
./compile.sh
```
This produces `libdrm_hook.so`.

### 2. Run Vulkan (NVK)
To run a Vulkan application (like `vkcube` or `vulkaninfo`) under the Mesa NVK driver:
```bash
./run_nouveau_vulkan.sh vulkaninfo
./run_nouveau_vulkan.sh vkcube
```

### 3. Run OpenGL (Nouveau Gallium)
To run an OpenGL application (like `glxgears`) under the Mesa Nouveau driver:
```bash
./run_nouveau_opengl.sh glxgears
```

## Structure
- [hook.c](file:///home/truonghieu/Documents/hook.c): The core `LD_PRELOAD` hooking logic.
- [compile.sh](file:///home/truonghieu/Documents/compile.sh): Build script.
- [run_nouveau_vulkan.sh](file:///home/truonghieu/Documents/run_nouveau_vulkan.sh): Vulkan/NVK runner.
- [run_nouveau_opengl.sh](file:///home/truonghieu/Documents/run_nouveau_opengl.sh): OpenGL/Nouveau runner.
