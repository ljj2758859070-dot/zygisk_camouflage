#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>
#include <map>
#include <string>
#include "zygisk.hpp"
#include "plt_hook.h"
#include "target_config.h"

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
 class KirinCamouflage : public zygisk::ModuleBase {
 public:
     void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
         if (args->package_name && strcmp(args->package_name, TARGET_PACKAGE_NAME) == 0) {
             srand(time(NULL));
             plt_hook("libc.so", "openat", (void*)my_openat, (void**)&orig_openat);
             plt_hook("libc.so", "read", (void*)my_read, (void**)&orig_read);
             plt_hook("libc.so", "lseek64", (void*)my_lseek64, (void**)&orig_lseek64);
             plt_hook("libc.so", "__system_property_get", (void*)my___system_property_get, (void**)&orig___system_property_get);
        }
    }
};
REGISTER_ZYGISK_MODULE(KirinCamouflage)
