#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <fcntl.h>
#include <drm/drm.h>
#include <drm/nouveau_drm.h>

static int card1_fd = -1;

static uint32_t g_pci_device = 0;
static uint32_t g_chipset = 0x177;
static char g_gpu_name[128] = "NVIDIA GPU";
static char g_chip_name[32] = "NV177";

static uint32_t get_chipset_from_pci_id(uint32_t pci_id) {
    if (pci_id >= 0x2200 && pci_id <= 0x223f) return 0x172; // GA102
    if ((pci_id >= 0x2480 && pci_id <= 0x24bf) || (pci_id >= 0x24e0 && pci_id <= 0x24ff)) return 0x174; // GA104
    if (pci_id >= 0x2500 && pci_id <= 0x257f) return 0x176; // GA106
    if (pci_id >= 0x2580 && pci_id <= 0x25ff) return 0x177; // GA107
    if (pci_id >= 0x2680 && pci_id <= 0x26bf) return 0x192; // AD102
    if (pci_id >= 0x2700 && pci_id <= 0x277f) return 0x193; // AD103
    if (pci_id >= 0x2780 && pci_id <= 0x27ff) return 0x194; // AD104
    if (pci_id >= 0x2800 && pci_id <= 0x287f) return 0x196; // AD106
    if (pci_id >= 0x2880 && pci_id <= 0x28ff) return 0x197; // AD107
    if (pci_id >= 0x1e00 && pci_id <= 0x1e3f) return 0x162; // TU102
    if (pci_id >= 0x1e80 && pci_id <= 0x1ebf) return 0x164; // TU104
    if (pci_id >= 0x1f00 && pci_id <= 0x1f7f) return 0x166; // TU106
    if (pci_id >= 0x2180 && pci_id <= 0x21bf) return 0x168; // TU116
    if (pci_id >= 0x1f80 && pci_id <= 0x1fff) return 0x168; // TU117
    return 0x177; // Default GA107
}

static void detect_gpu_info(void) {
    static int detected = 0;
    if (detected) return;
    detected = 1;

    // Read PCI device ID
    FILE *f = fopen("/sys/class/drm/card1/device/device", "r");
    if (!f) f = fopen("/sys/class/drm/card0/device/device", "r");
    if (f) {
        if (fscanf(f, "0x%x", &g_pci_device) != 1) {
            g_pci_device = 0x25ec; // fallback
        }
        fclose(f);
    } else {
        g_pci_device = 0x25ec;
    }

    g_chipset = get_chipset_from_pci_id(g_pci_device);
    snprintf(g_chip_name, sizeof(g_chip_name), "NV%03X", g_chipset);

    // Read GPU model name
    char slot_name[64] = {0};
    f = fopen("/sys/class/drm/card1/device/uevent", "r");
    if (!f) f = fopen("/sys/class/drm/card0/device/uevent", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PCI_SLOT_NAME=", 14) == 0) {
                sscanf(line + 14, "%s", slot_name);
                break;
            }
        }
        fclose(f);
    }

    if (slot_name[0]) {
        char proc_path[256];
        snprintf(proc_path, sizeof(proc_path), "/proc/driver/nvidia/gpus/%s/information", slot_name);
        f = fopen(proc_path, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "Model:", 6) == 0) {
                    char *p = line + 6;
                    while (*p == ' ' || *p == '\t') p++;
                    size_t len = strlen(p);
                    if (len > 0 && p[len-1] == '\n') p[len-1] = '\0';
                    strncpy(g_gpu_name, p, sizeof(g_gpu_name) - 1);
                    break;
                }
            }
            fclose(f);
        }
    }
}

static int get_nvidia_card_fd(void) {
    if (card1_fd != -1) return card1_fd;
    int fd = open("/dev/dri/card1", O_RDWR);
    if (fd >= 0) {
        card1_fd = fd;
        return fd;
    }
    fd = open("/dev/dri/card0", O_RDWR);
    if (fd >= 0) {
        card1_fd = fd;
        return fd;
    }
    return -1;
}

int ioctl(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    void *argp = va_arg(args, void *);
    va_end(args);

    if (request != 0xC0106400) {
        fprintf(stderr, "[DEBUG] ioctl req=0x%lx\n", request);
    }

    int nvidia_fd = get_nvidia_card_fd();

    if (request == DRM_IOCTL_PRIME_HANDLE_TO_FD) {
        return syscall(SYS_ioctl, nvidia_fd, request, argp);
    }
    if ((request & 0xFF) == 0x09) { /* DRM_IOCTL_GEM_CLOSE */
        return syscall(SYS_ioctl, nvidia_fd, request, argp);
    }

    if (request == DRM_IOCTL_VERSION) {
        int ret = syscall(SYS_ioctl, fd, request, argp);
        if (ret == 0) {
            struct drm_version *v = (struct drm_version *)argp;
            if (v->name && strcmp(v->name, "nvidia-drm") == 0) {
                strcpy(v->name, "nouveau");
                v->name_len = strlen("nouveau");
                v->version_major = 1;
                v->version_minor = 3;
                v->version_patchlevel = 1;
            }
        }
        return ret;
    }

    if (request == DRM_IOCTL_NOUVEAU_GETPARAM) {
        detect_gpu_info();
        struct drm_nouveau_getparam *getparam = argp;
        if (getparam->param == 11) { // NOUVEAU_GETPARAM_CHIPSET_ID
            getparam->value = g_chipset;
        } else if (getparam->param == 4) { // NOUVEAU_GETPARAM_PCI_DEVICE
            getparam->value = g_pci_device;
        } else if (getparam->param == 47) { // NOUVEAU_GETPARAM_EXEC_PUSH_MAX
            getparam->value = 512 * 1024;
        } else if (getparam->param == 13) { // NOUVEAU_GETPARAM_GRAPH_UNITS
            getparam->value = (4 << 8) | 1;
        } else {
            getparam->value = 0;
        }
        return 0;
    }

    if ((request & 0xFF) == 0x47 /* NOUVEAU_NVIF */) {
        struct nvif_ioctl_v0 {
            unsigned char version;
            unsigned char type;
            unsigned char pad02[4];
            unsigned char owner;
            unsigned char route;
            unsigned long long token;
            unsigned long long object;
            unsigned char data[];
        } *nvif = argp;

        if (nvif->type == 0x01) { /* NVIF_IOCTL_V0_SCLASS */
            detect_gpu_info();
            struct nvif_ioctl_sclass_v0 {
                unsigned char version;
                unsigned char count;
                unsigned char pad02[6];
            } *sclass = (void *)nvif->data;
            sclass->count = 6;
            
            struct nvif_ioctl_sclass_oclass_v0 {
                int oclass;
                short minver;
                short maxver;
            } *list = (void *)(sclass + 1);
            
            uint32_t arch_prefix = (g_chipset >= 0x190) ? 0xc9 : (g_chipset >= 0x170) ? 0xc7 : (g_chipset >= 0x160) ? 0xc5 : 0xc3;

            list[0].oclass = 0x902d; /* 2D */
            list[1].oclass = (arch_prefix << 8) | 0x97; /* 3D */
            list[2].oclass = (arch_prefix << 8) | 0xc0; /* Compute */
            list[3].oclass = (arch_prefix << 8) | 0xb5; /* Copy */
            list[4].oclass = 0xc339; /* Kepler m2mf / NVK fallback */
            list[5].oclass = 0xa140; /* Maxwell m2mf (Gallium needs this) */
            return 0;
        } else if (nvif->type == 0x02) { /* NVIF_IOCTL_V0_NEW */
            struct nvif_ioctl_new_v0 {
                unsigned char version;
                unsigned char pad01[6];
                unsigned char route;
                unsigned long long token;
                unsigned long long object;
                unsigned int handle;
                int oclass;
            } *args_new = (void *)nvif->data;
            args_new->route = 0;
            return 0;
        } else if (nvif->type == 0x03) { /* NVIF_IOCTL_V0_DEL */
            return 0;
        } else if (nvif->type == 0x04) { /* NVIF_IOCTL_V0_MTHD */
            struct nvif_ioctl_mthd_v0 {
                unsigned char version;
                unsigned char method;
                unsigned char pad02[6];
            } *mthd = (void *)nvif->data;
            if (mthd->method == 0x00) { /* NV_DEVICE_V0_INFO */
                detect_gpu_info();
                struct my_nv_device_info_v0 {
                    unsigned char version;
                    unsigned char platform;
                    unsigned short chipset;
                    unsigned char revision;
                    unsigned char family;
                    unsigned char pad06[2];
                    unsigned long long ram_size;
                    unsigned long long ram_user;
                    char chip[16];
                    char name[64];
                } *info = (void *)(mthd + 1);
                info->version = 0;
                info->platform = 3; /* PCIE */
                info->chipset = g_chipset;
                info->revision = 0xa1;
                info->family = 0x09; /* Maxwell+ */
                info->ram_size = 4ull * 1024 * 1024 * 1024;
                info->ram_user = info->ram_size;
                strcpy(info->chip, g_chip_name);
                strcpy(info->name, g_gpu_name);
                return 0;
            }
        }
    }

    if (request == 0x40106450 /* DRM_IOCTL_NOUVEAU_VM_INIT */) return 0;
    if (request == 0xc0286451 /* DRM_IOCTL_NOUVEAU_VM_BIND */) return 0;
    
    if (request == DRM_IOCTL_NOUVEAU_EXEC) return 0;
    if (request == DRM_IOCTL_NOUVEAU_GEM_PUSHBUF) {
        struct drm_nouveau_gem_pushbuf {
            uint32_t channel;
            uint32_t nr_buffers;
            uint64_t buffers;
            uint32_t nr_relocs;
            uint32_t nr_push;
            uint64_t relocs;
            uint64_t push;
            uint32_t suffix0;
            uint32_t suffix1;
            uint64_t vram_available;
            uint64_t gart_available;
        } *req = argp;
        req->vram_available = 4ull * 1024 * 1024 * 1024;
        req->gart_available = 4ull * 1024 * 1024 * 1024;
        return 0;
    }
    
    if (request == DRM_IOCTL_NOUVEAU_GEM_NEW) {
        struct drm_nouveau_gem_new *gem_new = argp;
        
        struct drm_mode_create_dumb dumb = {0};
        dumb.width = (gem_new->info.size + 4095) / 4096;
        dumb.height = 1;
        dumb.bpp = 32;
        int ret = syscall(SYS_ioctl, nvidia_fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb);
        
        if (ret == 0) {
            gem_new->info.handle = dumb.handle;
            struct drm_mode_map_dumb map_dumb = {0};
            map_dumb.handle = dumb.handle;
            syscall(SYS_ioctl, nvidia_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb);
            gem_new->info.map_handle = map_dumb.offset;
            gem_new->info.offset = map_dumb.offset;
        } else {
            static int next_handle = 1000;
            static uint64_t next_offset = 0x10000000;
            gem_new->info.handle = next_handle++;
            gem_new->info.map_handle = next_offset;
            gem_new->info.offset = next_offset;
            next_offset += (gem_new->info.size + 0xfff) & ~0xfff;
        }
        return 0;
    }

    if (request == DRM_IOCTL_NOUVEAU_GEM_INFO) return 0;

    if ((request & 0xFF) == 0x42 || (request & 0xFF) == 0x43) { /* DRM_NOUVEAU_CHANNEL_ALLOC */
        struct drm_nouveau_channel_alloc {
            uint32_t     fb_ctxdma_handle;
            uint32_t     tt_ctxdma_handle;
            int          channel;
            uint32_t     pushbuf_domains;
            uint32_t     notifier_handle;
            struct {
                uint32_t handle;
                uint32_t grclass;
            } subchan[8];
            uint32_t     nr_subchan;
        } *req = argp;
        req->channel = 1;
        req->nr_subchan = 0;
        return 0;
    }

    if ((request & 0xFF) == 0xC3) { /* DRM_IOCTL_SYNCOBJ_WAIT */
        struct drm_syncobj_wait {
            uint64_t handles;
            int64_t timeout_nsec;
            uint32_t count_handles;
            uint32_t flags;
            uint32_t first_signaled;
            uint32_t pad;
        } *wait = argp;
        wait->first_signaled = 0;
        return 0;
    }
    if ((request & 0xFF) == 0xCA) { /* DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT */
        struct drm_syncobj_timeline_wait {
            uint64_t handles;
            uint64_t points;
            int64_t timeout_nsec;
            uint32_t count_handles;
            uint32_t flags;
            uint32_t first_signaled;
            uint32_t pad;
        } *wait = argp;
        wait->first_signaled = 0;
        return 0;
    }
    
    int ret = syscall(SYS_ioctl, fd, request, argp);
    if (request != 0xC0106400) {
        fprintf(stderr, "[DEBUG] ioctl returned %d (errno=%d)\n", ret, errno);
    }
    return ret;
}

void *mmap64(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    int nvidia_fd = get_nvidia_card_fd();
    if (offset >= 0x10000000) {
        void* ret = (void *)syscall(SYS_mmap, addr, length, prot, MAP_SHARED, nvidia_fd, offset);
        if (ret != (void*)-1) return ret;
        return (void *)syscall(SYS_mmap, addr, length, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
    return (void *)syscall(SYS_mmap, addr, length, prot, flags, fd, offset);
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return mmap64(addr, length, prot, flags, fd, offset);
}
