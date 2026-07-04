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

---

## Recreating the Environment (Build Guide)

To ensure your environment matches ours exactly, you need to download and compile the official **Mesa 23.3.6** and **NVIDIA Open GPU Kernel Modules (v610.43.02)** from source.

### 1. Install Build Dependencies

Install the required compiler, build tools, and development headers for your Linux distribution:

#### Debian / Ubuntu
```bash
sudo apt update
sudo apt install -y build-essential meson ninja-build git wget bison flex \
  python3-mako pkg-config zlib1g-dev libdrm-dev libwayland-dev libelf-dev \
  libx11-dev libxcb-dev libxdg-basedir-dev
```

#### Fedora
```bash
sudo dnf install -y gcc gcc-c++ make meson ninja-build git wget bison flex \
  python3-mako pkgconfig zlib-devel libdrm-devel wayland-devel elfutils-libelf-devel \
  libX11-devel libxcb-devel
```

#### Arch Linux
```bash
sudo pacman -Syu --needed base-devel meson ninja git wget bison flex \
  python-mako pkgconf zlib libdrm wayland elfutils libx11 libxcb
```

---

### 2. Build and Load NVIDIA Open Kernel Modules (v610.43.02)
Clone the exact version of the NVIDIA open kernel modules and build it:
```bash
git clone --branch 610.43.02 https://github.com/NVIDIA/open-gpu-kernel-modules.git
cd open-gpu-kernel-modules
make -j$(nproc)
# Load the built nvidia-drm modules
sudo make modules_install
sudo depmod -a
```

---

### 3. Download and Build Mesa 23.3.6
Download and compile the exact Mesa version:
```bash
wget https://archive.mesa3d.org/mesa-23.3.6.tar.xz
tar -xf mesa-23.3.6.tar.xz
cd mesa-23.3.6

# Configure the build (ensure Nouveau Gallium and NVK Vulkan drivers are enabled)
meson setup build \
  -Dgallium-drivers=nouveau \
  -Dvulkan-drivers=nouveau \
  -Dglx=dri \
  -Dplatforms=wayland,x11 \
  -Dbuildtype=release

# Compile the drivers
meson compile -C build
```

---

## Running the Wrapper

### 1. Build the wrapper
Compile the wrapper library:
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
