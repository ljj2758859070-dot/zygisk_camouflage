#include "zygisk.hpp"
#include "plt_hook.h"
#include "target_config.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>
#include <map>
#include <string>
#include <android/log.h>

#define LOG_TAG "KirinCamouflage"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// ===================== 全局Hook原始函数指针（完全保留你的代码） =====================
static int (*orig_openat)(int dirfd, const char *pathname, int flags, mode_t mode);
static ssize_t (*orig_read)(int fd, void *buf, size_t count);
static off64_t (*orig_lseek64)(int fd, off64_t offset, int whence);
static int (*orig___system_property_get)(const char *name, char *value);

struct FakeFile {
    std::string content;
    size_t offset = 0;
};
static std::map<int, FakeFile> fake_files;
static int fake_fd_counter = 0x80000000;

// CPUINFO伪装模板
static const char *CPUINFO_TEMPLATE =
"Processor\t: AArch64 Processor rev 0 (aarch64)\n"
"Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp\n"
"CPU implementer\t: 0x48\n"
"CPU architecture: 8\n"
"CPU variant\t: 0x1\n"
"CPU part\t: 0xd24\n"
"CPU revision\t: 0\n\n"
"processor\t: 0\n"
"BogoMIPS\t: 26.00\n"
"CPU implementer\t: 0x48\n"
"CPU architecture: 8\n"
"CPU variant\t: 0x1\n"
"CPU part\t: 0xd24\n"
"CPU revision\t: 0\n\n"
"processor\t: 1\n"
"BogoMIPS\t: 26.00\n"
"CPU implementer\t: 0x48\n"
"CPU architecture: 8\n"
"CPU variant\t: 0x2\n"
"CPU part\t: 0xd47\n"
"CPU revision\t: 0\n\n"
"processor\t: 2\n"
"BogoMIPS\t: 26.00\n"
"CPU implementer\t: 0x48\n"
"CPU architecture: 8\n"
"CPU variant\t: 0x2\n"
"CPU part\t: 0xd47\n"
"CPU revision\t: 0\n\n"
"processor\t: 3\n"
"BogoMIPS\t: 26.00\n"
"CPU implementer\t: 0x48\n"
"CPU architecture: 8\n"
"CPU variant\t: 0x2\n"
"CPU part\t: 0xd47\n"
"CPU revision\t: 0\n\n"
"processor\t: 4\n"
"BogoMIPS\t: 26.00\n"
"CPU implementer\t: 0x48\n"
"CPU architecture: 8\n"
"CPU variant\t: 0x2\n"
"CPU part\t: 0xd47\n"
"CPU revision\t: 0\n\n"
"processor\t: 5\n"
"BogoMIPS\t: 26.00\n"
"CPU implementer\t: 0x48\n"
"CPU architecture: 8\n"
"CPU variant\t: 0x3\n"
"CPU part\t: 0xd06\n"
"CPU revision\t: 0\n\n"
"processor\t: 6\n"
"BogoMIPS\t: 26.00\n"
"CPU implementer\t: 0x48\n"
"CPU architecture: 8\n"
"CPU variant\t: 0x3\n"
"CPU part\t: 0xd06\n"
"CPU revision\t: 0\n\n"
"processor\t: 7\n"
"BogoMIPS\t: 26.00\n"
"CPU implementer\t: 0x48\n"
"CPU architecture: 8\n"
"CPU variant\t: 0x3\n"
"CPU part\t: 0xd06\n"
"CPU revision\t: 0\n\n"
"processor\t: 8\n"
"BogoMIPS\t: 26.00\n"
"CPU implementer\t: 0x48\n"
"CPU architecture: 8\n"
"CPU variant\t: 0x3\n"
"CPU part\t: 0xd06\n"
"CPU revision\t: 0\n\n"
"Hardware\t: " CPU_HARDWARE_STR "\n";

static int read_real_cpu_freq(int cpu_num) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu_num);
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    int freq = 0;
    fscanf(fp, "%d", &freq);
    fclose(fp);
    return freq;
}

static int map_freq(int real_freq, int real_max, int target_max, int target_min) {
    if (real_freq <= 0) return target_min;
    float ratio = (float)real_freq / real_max;
    int mapped = target_min + (int)(ratio * (target_max - target_min));
    mapped += (rand() % 61 - 30);
    if (mapped > target_max) mapped = target_max;
    if (mapped < target_min) mapped = target_min;
    return mapped;
}

static int get_cpu_cluster(int cpu_num) {
    if (cpu_num == 0) return 0;
    if (cpu_num >= 1 && cpu_num <= 4) return 1;
    return 2;
}

// openat 拦截伪造文件
int my_openat(int dirfd, const char *pathname, int flags, mode_t mode) {
    if (!pathname) return orig_openat(dirfd, pathname, flags, mode);
    std::string path(pathname);
    std::string fake_content;
    if (path == "/proc/cpuinfo") {
        fake_content = CPUINFO_TEMPLATE;
    } else if (path == "/proc/device-tree/soc/compatible") {
        fake_content = SOC_COMPAT_STR;
    } else if (path.find("/sys/devices/platform/mali/gpuinfo") != std::string::npos) {
        fake_content = GPUINFO_TEXT;
    } else if (path.find("cpufreq/cpuinfo_max_freq") != std::string::npos) {
        int cpu_num = 0;
        sscanf(pathname, "/sys/devices/system/cpu/cpu%d/", &cpu_num);
        int cluster = get_cpu_cluster(cpu_num);
        int max_freq = (cluster == 0) ? CPU_PRIME_MAX : (cluster == 1) ? CPU_BIG_MAX : CPU_LIT_MAX;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", max_freq);
        fake_content = buf;
    } else if (path.find("cpufreq/cpuinfo_min_freq") != std::string::npos) {
        int cpu_num = 0;
        sscanf(pathname, "/sys/devices/system/cpu/cpu%d/", &cpu_num);
        int cluster = get_cpu_cluster(cpu_num);
        int min_freq = (cluster == 0) ? CPU_PRIME_MIN : (cluster == 1) ? CPU_BIG_MIN : CPU_LIT_MIN;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", min_freq);
        fake_content = buf;
    } else if (path.find("cpufreq/scaling_cur_freq") != std::string::npos) {
        int cpu_num = 0;
        sscanf(pathname, "/sys/devices/system/cpu/cpu%d/", &cpu_num);
        int real_freq = read_real_cpu_freq(cpu_num);
        int cluster = get_cpu_cluster(cpu_num);
        int mapped = (cluster == 0) ? map_freq(real_freq, REAL_CPU_MAX, CPU_PRIME_MAX, CPU_PRIME_MIN) :
                     (cluster == 1) ? map_freq(real_freq, REAL_CPU_MAX, CPU_BIG_MAX, CPU_BIG_MIN) :
                                      map_freq(real_freq, REAL_CPU_MAX, CPU_LIT_MAX, CPU_LIT_MIN);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", mapped);
        fake_content = buf;
    } else {
        return orig_openat(dirfd, pathname, flags, mode);
    }
    int fd = fake_fd_counter++;
    fake_files[fd] = {fake_content, 0};
    return fd;
}

ssize_t my_read(int fd, void *buf, size_t count) {
    auto it = fake_files.find(fd);
    if (it != fake_files.end()) {
        FakeFile &f = it->second;
        if (f.offset >= f.content.size()) return 0;
        size_t avail = f.content.size() - f.offset;
        size_t read_len = (count < avail) ? count : avail;
        memcpy(buf, f.content.c_str() + f.offset, read_len);
        f.offset += read_len;
        return read_len;
    }
    return orig_read(fd, buf, count);
}

off64_t my_lseek64(int fd, off64_t offset, int whence) {
    auto it = fake_files.find(fd);
    if (it != fake_files.end()) {
        FakeFile &f = it->second;
        off64_t new_off = 0;
        if (whence == SEEK_SET) new_off = offset;
        else if (whence == SEEK_CUR) new_off = f.offset + offset;
        else if (whence == SEEK_END) new_off = f.content.size() + offset;
        if (new_off < 0) new_off = 0;
        if (new_off > (off64_t)f.content.size()) new_off = f.content.size();
        f.offset = new_off;
        return new_off;
    }
    return orig_lseek64(fd, offset, whence);
}

// 系统属性伪装
int my___system_property_get(const char *name, char *value) {
    if (strcmp(name, "ro.soc.model") == 0) {
        strcpy(value, SOC_MODEL);
        return strlen(value);
    }
    if (strcmp(name, "ro.board.platform") == 0) {
        strcpy(value, BOARD_PLATFORM);
        return strlen(value);
    }
    if (strcmp(name, "ro.hardware") == 0) {
        strcpy(value, HW_NAME);
        return strlen(value);
    }
    if (strcmp(name, "ro.product.brand") == 0) {
        strcpy(value, PROD_BRAND);
        return strlen(value);
    }
    if (strcmp(name, "ro.product.manufacturer") == 0) {
        strcpy(value, PROD_MANUFACTURER);
        return strlen(value);
    }
    if (strcmp(name, "ro.product.model") == 0) {
        strcpy(value, PROD_MODEL);
        return strlen(value);
    }
    return orig___system_property_get(name, value);
}

// ===================== V1标准回调类（替换原来的ModuleBase继承） =====================
namespace {
class KirinCamouflage {
public:
    static bool pre_fork() {
        return true;
    }

    static void post_fork(uint32_t uid, uint32_t gid, const uint32_t* gids, int gids_count,
                          const char* seinfo, int target_sdk, std::string_view* process_name) {
        LOGD("fork进程: %s uid=%u", process_name->data(), uid);
    }

    // 对应原来postAppSpecialize，在这里判断包名执行Hook
    static void post_specialize(std::string_view process_name) {
        // 匹配目标包名才开启Hook
        if (process_name == TARGET_PACKAGE_NAME) {
            LOGD("命中目标应用，开始劫持libc函数");
            srand(time(NULL));
            plt_hook("libc.so", "openat", (void*)my_openat, (void**)&orig_openat);
            plt_hook("libc.so", "read", (void*)my_read, (void**)&orig_read);
            plt_hook("libc.so", "lseek64", (void*)my_lseek64, (void**)&orig_lseek64);
            plt_hook("libc.so", "__system_property_get", (void*)my___system_property_get, (void**)&orig___system_property_get);
        }
    }

    static bool skip_library(std::string_view libname) {
        return false;
    }

    static void load_libraries(const char* app_data_dir) {}
};
}

// V1标准导出入口，无宏
extern "C" [[gnu::visibility("default")]]
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
