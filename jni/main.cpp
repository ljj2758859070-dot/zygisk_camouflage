#include "zygisk.hpp"
#include <android/log.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>

#define LOG_TAG "KirinCamouflage"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// 原始函数指针预留
static int (*orig_openat)(int dirfd, const char *pathname, int flags, mode_t mode);
static ssize_t (*orig_read)(int fd, void *buf, size_t count);
static off64_t (*orig_lseek64)(int fd, off64_t offset, int whence);
static int (*orig___system_property_get)(const char *name, char *value);

// 虚拟文件存储
struct FakeFile {
    char content[8192];
    size_t length;
    size_t offset;
};
static FakeFile fake_files[10];
static int fake_fd_counter = 0x80000000;

// 伪装SOC参数
static const char *FAKE_SOC_MODEL = "SM8750P";
static const char *FAKE_BOARD_PLATFORM = "qcom";
static const char *FAKE_HARDWARE = "qcom";
static const char *FAKE_BRAND = "google";
static const char *FAKE_MANU = "google";
static const char *FAKE_MODEL = "Pixel 9 Pro";

// 完整你提供的cpuinfo文本
static const char *CPUINFO_TEMPLATE =
"Processor       : AArch64 Processor rev 0 (aarch64)\n"
"Features        : fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp\n"
"CPU implementer : 0x48\n"
"CPU architecture: 8\n"
"CPU variant     : 0x1\n"
"CPU part        : 0xd24\n"
"CPU revision    : 0\n\n"
"processor       : 0\n"
"BogoMIPS        : 26.00\n"
"CPU implementer : 0x48\n"
"CPU architecture: 8\n"
"CPU variant     : 0x1\n"
"CPU part        : 0xd24\n"
"CPU revision    : 0\n\n"
"processor       : 1\n"
"BogoMIPS        : 26.00\n"
"CPU implementer : 0x48\n"
"CPU architecture: 8\n"
"CPU variant     : 0x2\n"
"CPU part        : 0xd47\n"
"CPU revision    : 0\n\n"
"processor       : 2\n"
"BogoMIPS        : 26.00\n"
"CPU implementer : 0x48\n"
"CPU architecture: 8\n"
"CPU variant     : 0x2\n"
"CPU part        : 0xd47\n"
"CPU revision    : 0\n\n"
"processor       : 3\n"
"BogoMIPS        : 26.00\n"
"CPU implementer : 0x48\n"
"CPU architecture: 8\n"
"CPU variant     : 0x2\n"
"CPU part        : 0xd47\n"
"CPU revision    : 0\n\n"
"processor       : 4\n"
"BogoMIPS        : 26.00\n"
"CPU implementer : 0x48\n"
"CPU architecture: 8\n"
"CPU variant     : 0x2\n"
"CPU part        : 0xd47\n"
"CPU revision    : 0\n\n"
"processor       : 5\n"
"BogoMIPS        : 26.00\n"
"CPU implementer : 0x48\n"
"CPU architecture: 8\n"
"CPU variant     : 0x3\n"
"CPU part        : 0xd06\n"
"CPU revision    : 0\n\n"
"processor       : 6\n"
"BogoMIPS        : 26.00\n"
"CPU implementer : 0x48\n"
"CPU architecture: 8\n"
"CPU variant     : 0x3\n"
"CPU part        : 0xd06\n"
"CPU revision    : 0\n\n"
"processor       : 7\n"
"BogoMIPS        : 26.00\n"
"CPU implementer : 0x48\n"
"CPU architecture: 8\n"
"CPU variant     : 0x3\n"
"CPU part        : 0xd06\n"
"CPU revision    : 0\n\n"
"processor       : 8\n"
"BogoMIPS        : 26.00\n"
"CPU implementer : 0x48\n"
"CPU architecture: 8\n"
"CPU variant     : 0x3\n"
"CPU part        : 0xd06\n"
"CPU revision    : 0\n\n"
"Hardware        : HiSilicon Kirin 9030 Pro\n";

// 查找空闲虚拟fd
static int find_fake_fd(void) {
    for (int i = 0; i < 10; i++) {
        if (fake_files[i].content[0] == '\0')
            return fake_fd_counter + i;
    }
    return -1;
}

// 通过fd获取虚拟文件对象
static FakeFile *get_fake_file(int fd) {
    if (fd < fake_fd_counter || fd >= fake_fd_counter + 10) return NULL;
    return &fake_files[fd - fake_fd_counter];
}

// Hook openat 拦截/proc/cpuinfo
int my_openat(int dirfd, const char *pathname, int flags, mode_t mode) {
    if (!pathname) return orig_openat(dirfd, pathname, flags, mode);
    if (strcmp(pathname, "/proc/cpuinfo") == 0) {
        int fd = find_fake_fd();
        if (fd == -1) return orig_openat(dirfd, pathname, flags, mode);
        FakeFile *f = get_fake_file(fd);
        strncpy(f->content, CPUINFO_TEMPLATE, sizeof(f->content)-1);
        f->content[sizeof(f->content)-1] = 0;
        f->length = strlen(f->content);
        f->offset = 0;
        return fd;
    }
    return orig_openat(dirfd, pathname, flags, mode);
}

// Hook read 读取虚拟cpuinfo
ssize_t my_read(int fd, void *buf, size_t count) {
    FakeFile *f = get_fake_file(fd);
    if (f) {
        if (f->offset >= f->length) return 0;
        size_t read_len = count < (f->length - f->offset) ? count : (f->length - f->offset);
        memcpy(buf, f->content + f->offset, read_len);
        f->offset += read_len;
        return read_len;
    }
    return orig_read(fd, buf, count);
}

// Hook lseek64 偏移虚拟文件指针
off64_t my_lseek64(int fd, off64_t offset, int whence) {
    FakeFile *f = get_fake_file(fd);
    if (f) {
        off64_t new_off = 0;
        if (whence == SEEK_SET) new_off = offset;
        else if (whence == SEEK_CUR) new_off = f->offset + offset;
        else if (whence == SEEK_END) new_off = f->length + offset;
        if (new_off < 0) new_off = 0;
        if (new_off > (off64_t)f->length) new_off = f->length;
        f->offset = new_off;
        return new_off;
    }
    return orig_lseek64(fd, offset, whence);
}

// Hook 系统属性，伪装soc/硬件型号
int my___system_property_get(const char *name, char *value) {
    if (strcmp(name, "ro.soc.model") == 0) {
        strcpy(value, FAKE_SOC_MODEL);
        return strlen(value);
    }
    if (strcmp(name, "ro.board.platform") == 0) {
        strcpy(value, FAKE_BOARD_PLATFORM);
        return strlen(value);
    }
    if (strcmp(name, "ro.hardware") == 0) {
        strcpy(value, FAKE_HARDWARE);
        return strlen(value);
    }
    if (strcmp(name, "ro.product.brand") == 0) {
        strcpy(value, FAKE_BRAND);
        return strlen(value);
    }
    if (strcmp(name, "ro.product.manufacturer") == 0) {
        strcpy(value, FAKE_MANU);
        return strlen(value);
    }
    if (strcmp(name, "ro.product.model") == 0) {
        strcpy(value, FAKE_MODEL);
        return strlen(value);
    }
    return orig___system_property_get(name, value);
}

namespace {
class KirinCamouflage {
public:
    static bool pre_fork() {
        return true;
    }

    static void post_fork(uint32_t uid, uint32_t gid, const uint32_t* gids, int gids_count,
                          const char* seinfo, int target_sdk, const char* process_name) {
        LOGD("Fork Process: %s uid=%u", process_name, uid);
    }

    static void post_specialize(const char* process_name) {
        // 修改这里为你要伪装的目标APP包名
        const char target_pkg[] = "com.example.app";
        if (strcmp(process_name, target_pkg) == 0) {
            LOGD("Target app loaded, ready hook");
            // 有plt_hook实现后再取消下面注释
            // plt_hook("shturl.", "openat", (void*)my_openat, (void**)&orig_openat);
            // plt_hook("shturl.", "read", (void*)my_read, (void**)&orig_read);
            // plt_hook("shturl.", "lseek64", (void*)my_lseek64, (void**)&orig_lseek64);
            // plt_hook("shturl.", "__system_property_get", (void*)my___system_property_get, (void**)&orig___system_property_get);
        }
    }

    static bool skip_library(const char *libname) {
        return false;
    }
    static void load_libraries(const char *app_data_dir) {
        LOGD("App data dir: %s", app_data_dir);
    }
};
}

// V1标准导出入口，无任何注册宏
extern "C" __attribute__((visibility("default")))
const zygisk::ModuleApi *zygisk_module_entry(zygisk::ApiVersion ver) {
    static constexpr zygisk::ModuleApi api = {
        .version = zygisk::ApiVersion::v1,
        .pre_fork = &KirinCamouflage::pre_fork,
        .post_fork = &KirinCamouflage::post_fork,
        .post_specialize = &KirinCamouflage::post_specialize,
        .skip_library = &KirinCamouflage::skip_library,
        .load_libraries = &KirinCamouflage::load_libraries,
    };
    return ver == zygisk::ApiVersion::v1 ? &api : nullptr;
}
