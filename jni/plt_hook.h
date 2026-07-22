#pragma once
#include <cstdint>
int plt_hook(const char* libname, const char* sym, void* new_func, void** orig_func);
