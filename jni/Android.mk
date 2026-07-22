#include "zygisk.hpp"
#include <android/log.h>
#include <string_view>

#define LOG_TAG "KirinCamouflage"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace {

class KirinCamouflage {
public:
    static bool pre_fork() {
        LOGD("pre_fork triggered");
        return true;
    }

    static void post_fork(uint32_t uid, uint32_t gid, const uint32_t* gids, int gids_count,
                          const char* seinfo, int target_sdk, std::string_view* process_name) {
        LOGD("post_fork uid=%u process=%s", uid, process_name->data());
    }

    static void post_specialize(std::string_view process_name) {
        LOGD("post_specialize: %s", process_name.data());
    }

    static bool skip_library(std::string_view libname) {
        LOGD("skip_library check: %s", libname.data());
        return false;
    }

    static void load_libraries(const char* app_data_dir) {
        LOGD("load_libraries dir: %s", app_data_dir);
    }
};

} // anonymous namespace

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
