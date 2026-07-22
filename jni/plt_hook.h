#ifndef PLT_HOOK_H
#define PLT_HOOK_H

#include <dlfcn.h>
#include <link.h>
#include <string>

bool plt_hook(const char *libname, const char *sym, void *new_func, void **orig_func);

#endif
