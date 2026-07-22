#include "plt_hook.h"
#include <android/log.h>
#include <link.h>
#include <string.h>
#include <sys/mman.h>
#include <dlfcn.h>

#ifndef RTLD_DL_LINKMAP
#define RTLD_DL_LINKMAP 2
#endif

#define LOG_TAG "PLT_HOOK"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

static uint64_t page_align(uint64_t addr) {
    return addr & ~0xFFFULL;
}
static int set_rw(uint64_t addr) {
    uint64_t page = page_align(addr);
    return mprotect((void*)page, 0x1000, PROT_READ | PROT_WRITE);
}
static int set_rx(uint64_t addr) {
    uint64_t page = page_align(addr);
    return mprotect((void*)page, 0x1000, PROT_READ | PROT_EXEC);
}

int plt_hook(const char* libname, const char* sym, void* new_func, void** orig_func) {
    void* handle = dlopen(libname, RTLD_LAZY);
    if (!handle) {
        LOGD("dlopen failed %s", libname);
        return -1;
    }
    void* origin = dlsym(handle, sym);
    if (!origin) {
        LOGD("dlsym fail %s", sym);
        dlclose(handle);
        return -2;
    }
    *orig_func = origin;

    struct link_map* map = nullptr;
    dladdr1(origin, nullptr, nullptr, (void**)&map, RTLD_DL_LINKMAP);
    if (!map) {
        dlclose(handle);
        LOGD("get link_map failed");
        return -4;
    }

    Elf64_Dyn* dyn = map->l_ld;
    Elf64_Rel* rel = nullptr;
    Elf64_Sym* symtab = nullptr;
    const char* strtab = nullptr;
    size_t relsz = 0;

    for (; dyn->d_tag != DT_NULL; dyn++) {
        if (dyn->d_tag == DT_JMPREL) rel = (Elf64_Rel*)dyn->d_un.d_ptr;
        if (dyn->d_tag == DT_PLTRELSZ) relsz = dyn->d_un.d_val;
        if (dyn->d_tag == DT_SYMTAB) symtab = (Elf64_Sym*)dyn->d_un.d_ptr;
        if (dyn->d_tag == DT_STRTAB) strtab = (const char*)dyn->d_un.d_ptr;
    }

    for (size_t i = 0; i < relsz / sizeof(Elf64_Rel); i++) {
        Elf64_Rel& r = rel[i];
        uint32_t idx = ELF64_R_SYM(r.r_info);
        const char* name = strtab + symtab[idx].st_name;
        if (strcmp(name, sym) == 0) {
            uint64_t target = r.r_offset;
            set_rw(target);
            *(void**)target = new_func;
            set_rx(target);
            LOGD("Hook success: %s", sym);
            dlclose(handle);
            return 0;
        }
    }
    dlclose(handle);
    LOGD("Cannot find plt entry");
    return -3;
}
