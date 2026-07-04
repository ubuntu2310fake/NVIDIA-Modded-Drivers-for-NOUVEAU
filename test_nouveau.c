#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <drm/drm.h>
#include <drm/nouveau_drm.h>
#include <nouveau/nouveau.h>
#include <nouveau/nvif/ioctl.h>

int main() {
    int fd = open("/dev/dri/renderD128", O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/dri/renderD128");
        return 1;
    }
    printf("Opening /dev/dri/renderD128...\n\n");

    printf("--- Test 1: NOUVEAU_GETPARAM ---\n");
    struct drm_nouveau_getparam getparam_req = {
        .param = NOUVEAU_GETPARAM_CHIPSET_ID
    };
    if (ioctl(fd, DRM_IOCTL_NOUVEAU_GETPARAM, &getparam_req) == 0) {
        printf("[SUCCESS] Chipset ID: 0x%llx\n", getparam_req.value);
    } else {
        perror("[FAILED] NOUVEAU_GETPARAM");
    }

    printf("\n--- Test 2: NOUVEAU_GEM_NEW ---\n");
    struct drm_nouveau_gem_new gem_req = {
        .info = { .domain = NOUVEAU_GEM_DOMAIN_VRAM, .size = 4096 }
    };
    int ret = ioctl(fd, DRM_IOCTL_NOUVEAU_GEM_NEW, &gem_req);
    if (ret == 0) {
        printf("[SUCCESS] Allocated 4096 bytes on NVIDIA VRAM!\n");
        printf("GEM Handle: %d\n", gem_req.info.handle);
        printf("VRAM Offset: 0x%llx\n", gem_req.info.offset);
    } else {
        perror("[FAILED] GEM_NEW");
    }

    printf("\n--- Test 3: NOUVEAU_GEM_INFO ---\n");
    struct drm_nouveau_gem_info info_req = {
        .handle = gem_req.info.handle
    };
    ret = ioctl(fd, DRM_IOCTL_NOUVEAU_GEM_INFO, &info_req);
    if (ret == 0) {
        printf("[SUCCESS] GEM Info Map Offset: 0x%llx\n", info_req.map_handle);
    } else {
        perror("[FAILED] GEM_INFO");
    }

    printf("\n--- Test 4: NOUVEAU_CHANNEL_ALLOC ---\n");
    printf("IOCTL Value: 0x%lx\n", DRM_IOCTL_NOUVEAU_CHANNEL_ALLOC);
    struct drm_nouveau_channel_alloc alloc_args = {0};
    if (ioctl(fd, DRM_IOCTL_NOUVEAU_CHANNEL_ALLOC, &alloc_args) < 0) {
        perror("[ERROR] NOUVEAU_CHANNEL_ALLOC failed");
    } else {
        printf("[SUCCESS] Allocated Channel! ID: %d\n", alloc_args.channel);
    }

    close(fd);
    return 0;
}
