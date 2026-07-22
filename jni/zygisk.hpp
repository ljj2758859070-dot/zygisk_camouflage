#pragma once

#include <cstdint>
#include <string_view>

namespace zygisk {

using uint32 = uint32_t;
using uint64 = uint64_t;
using int32 = int32_t;
using int64 = int64_t;

enum class ApiVersion : uint32 {
    v1 = 1,
};

struct ModuleApi {
    ApiVersion version;

    // Return true if you want to be notified when zygote forks
    bool (*pre_fork)(void);

    // Called in the forked child process before exec
    void (*post_fork)(uint32 uid, uint32 gid, const uint32 *gids, int gids_count,
                      const char *seinfo, int target_sdk,
                      std::string_view *process_name);

    // Called after the process is specialized
    void (*post_specialize)(std::string_view process_name);

    // If returns true, the original library load will be skipped
    bool (*skip_library)(std::string_view libname);

    // Load custom libraries before app_process loads its libraries
    void (*load_libraries)(const char *app_data_dir);
};

// Entry point. Export this function in your module.
extern "C" [[gnu::visibility("default")]]
const ModuleApi *zygisk_module_entry(ApiVersion version);

} // namespace zygisk
