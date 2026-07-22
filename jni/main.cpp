#include "zygisk.hpp"
#include "plt_hook.h"
#include <android/log.h>
#include <string.h>

#define LOG_TAG "PROP_SPOOF"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

static int (*orig___system_property_get)(const char *name, char *value);

// 伪装硬件参数 HUAWEI SGT-AL00 麒麟9030 Pro
#define FAKE_BRAND         "HUAWEI"
#define FAKE_MANUFACTURER  "HUAWEI"
#define FAKE_MODEL         "SGT-AL00"
#define FAKE_DEVICE        "SGT-AL00"
#define FAKE_SOC_MODEL     "HiSilicon Kirin 9030 Pro"
#define FAKE_HW            "kirin"
#define FAKE_BOARD_PLAT    "kirin9030"
#define FAKE_FINGERPRINT   "HUAWEI/SGT-AL00/SGT-AL00:15/HUAWEI.250612/260:user/release-keys"

int my___system_property_get(const char *name, char *value) {
    if (strcmp(name, "ro.product.brand") == 0) {
        strcpy(value, FAKE_BRAND);
        return strlen(value);
    }
    if (strcmp(name, "ro.product.manufacturer") == 0) {
        strcpy(value, FAKE_MANUFACTURER);
        return strlen(value);
    }
    if (strcmp(name, "ro.product.model") == 0) {
        strcpy(value, FAKE_MODEL);
        return strlen(value);
    }
    if (strcmp(name, "ro.product.device") == 0) {
        strcpy(value, FAKE_DEVICE);
        return strlen(value);
    }
    if (strcmp(name, "ro.soc.model") == 0) {
        strcpy(value, FAKE_SOC_MODEL);
        return strlen(value);
    }
    if (strcmp(name, "ro.hardware") == 0) {
        strcpy(value, FAKE_HW);
        return strlen(value);
    }
    if (strcmp(name, "ro.board.platform") == 0) {
        strcpy(value, FAKE_BOARD_PLAT);
        return strlen(value);
    }
    if (strcmp(name, "ro.build.fingerprint") == 0) {
        strcpy(value, FAKE_FINGERPRINT);
        return strlen(value);
    }
    return orig___system_property_get(name, value);
}

namespace {
class PropSpoofModule {
public:
    static bool pre_fork() {
        return true;
    }

    static void post_fork(uint32_t uid, uint32_t gid, const uint32_t* gids, int gids_count,
                          const char* seinfo, int target_sdk, const char* process_name) {
    }

    static void post_specialize(const char* process_name) {
        // 三角洲行动包名
        const char target_pkg[] = "com.tencent.tmgp.dfm";
        if (strcmp(process_name, target_pkg) == 0) {
            LOGD("Target game loaded, start prop hook");
            plt_hook("shturl.", "__system_property_get", (void*)my___system_property_get, (void**)&orig___system_property_get);
        }
    }

    static bool skip_library(const char *libname) {
        return false;
    }
    static void load_libraries(const char *app_data_dir) {
    }
};
}

extern "C" __attribute__((visibility("default")))
const zygisk::ModuleApi *zygisk_module_entry(zygisk::ApiVersion ver) {
    static constexpr zygisk::ModuleApi api = {
        .version = zygisk::ApiVersion::v1,
        .pre_fork = &PropSpoofModule::pre_fork,
        .post_fork = &PropSpoofModule::post_fork,
        .post_specialize = &PropSpoofModule::post_specialize,
        .skip_library = &PropSpoofModule::skip_library,
        .load_libraries = &PropSpoofModule::load_libraries,
    };
    return ver == zygisk::ApiVersion::v1 ? &api : nullptr;
}
