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

    bool (*pre_fork)(void);

    void (*post_fork)(uint32 uid, uint32 gid, const uint32 *gids, int gids_count,
                      const char *seinfo, int target_sdk,
                      std::string_view *process_name);

    void (*post_specialize)(std::string_view process_name);

    bool (*skip_library)(std::string_view libname);

    void (*load_libraries)(const char *app_data_dir);
};

extern "C" [[gnu::visibility("default")]]
const ModuleApi *zygisk_module_entry(ApiVersion version);

} // namespace zygisk

#define REGISTER_ZYGISK_MODULE(IMPL)                                          \
extern "C" [[gnu::visibility("default")]]                                     \
const ::zygisk::ModuleApi *zygisk_module_entry(::zygisk::ApiVersion ver) {   \
    static constexpr ::zygisk::ModuleApi api = {                              \
        .version = ::zygisk::ApiVersion::v1,                                  \
        .pre_fork = &IMPL::pre_fork,                                          \
        .post_fork = &IMPL::post_fork,                                        \
        .post_specialize = &IMPL::post_specialize,                            \
        .skip_library = &IMPL::skip_library,                                   \
        .load_libraries = &IMPL::load_libraries,                              \
    };                                                                        \
    return ver == ::zygisk::ApiVersion::v1 ? &api : nullptr;                  \
}
