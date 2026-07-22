#pragma once

#include <cstdint>

namespace zygisk {
using uint32 = uint32_t;
enum class ApiVersion : uint32 { v1 = 1 };

struct ModuleApi {
    ApiVersion version;
    bool (*pre_fork)(void);
    void (*post_fork)(uint32 uid, uint32 gid, const uint32 *gids, int gids_count,
                      const char *seinfo, int target_sdk, const char *process_name);
    void (*post_specialize)(const char *process_name);
    bool (*skip_library)(const char *libname);
    void (*load_libraries)(const char *app_data_dir);
};

extern "C" __attribute__((visibility("default")))
const ModuleApi *zygisk_module_entry(ApiVersion version);
}
