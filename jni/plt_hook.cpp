#include "plt_hook.h"
#include <cstdint>

#define ELF_R_AARCH64_JUMP_SLOT 1026

static uintptr_t get_module_base(const char *lib) {
    void *handle = dlopen(lib, RTLD_LAZY);
    if (!handle) return 0;
    uintptr_t base = (uintptr_t)dlsym(handle, "__libc_start_main") - 0x100000;
    dlclose(handle);
    return base;
}

bool plt_hook(const char *libname, const char *symbol, void *new_func, void **orig_func) {
    uintptr_t base = get_module_base(libname);
    if (!base) return false;

    struct dl_phdr_info info{};
    dl_iterate_phdr([](struct dl_phdr_info *info, size_t, void *data) -> int {
        auto ctx = (uintptr_t*)data;
        if (std::string(info->dlpi_name).find("libc.so") != std::string::npos) {
            *ctx = (uintptr_t)info->dlpi_addr;
            return 1;
        }
        return 0;
    }, &base);

    void *sym_addr = dlsym(RTLD_NEXT, symbol);
    *orig_func = sym_addr;

    Elf64_Dyn *dyn = nullptr;
    const Elf64_Ehdr *ehdr = (Elf64_Ehdr *)base;
    const Elf64_Phdr *phdr = (Elf64_Phdr *)(base + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_DYNAMIC) {
            dyn = (Elf64_Dyn*)(base + phdr[i].p_vaddr);
            break;
        }
    }
    if (!dyn) return false;

    Elf64_Rel *reloc = nullptr;
    const char *strtab = nullptr;
    Elf64_Sym *symtab = nullptr;

    for (; dyn->d_tag != DT_NULL; dyn++) {
        if (dyn->d_tag == DT_JMPREL) reloc = (Elf64_Rel*)(base + dyn->d_un.d_ptr);
        if (dyn->d_tag == DT_STRTAB) strtab = (const char*)(base + dyn->d_un.d_ptr);
        if (dyn->d_tag == DT_SYMTAB) symtab = (Elf64_Sym*)(base + dyn->d_un.d_ptr);
    }
    if (!reloc || !strtab || !symtab) return false;

    for (; reloc->r_offset; reloc++) {
        if (ELF64_R_TYPE(reloc->r_info) != ELF_R_AARCH64_JUMP_SLOT) continue;
        uint32_t idx = ELF64_R_SYM(reloc->r_info);
        const char *name = strtab + symtab[idx].st_name;
        if (std::string(name) == symbol) {
            uintptr_t *slot = (uintptr_t*)(base + reloc->r_offset);
            *slot = (uintptr_t)new_func;
            return true;
        }
    }
    return false;
}
