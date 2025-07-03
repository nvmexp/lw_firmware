#ifndef INCLUDE_GUARD_CASK_PRINT_PARAMS_H
#define INCLUDE_GUARD_CASK_PRINT_PARAMS_H
#include "cask.h"

#include <stdint.h>             // for int64_t etc
#include <cstdio>               // for printf
#include <lwComplex.h>          // for lwComplex

namespace cask {

inline void printInt32(const char *name, const char *base, const char *field, int cstBase) {
    int64_t diff = field - base;
    int32_t addr = cstBase + (int32_t) diff;
    int32_t data = *(int32_t*) field;
    printf("[addr=0x%05x, dtype=i32] %s: %16d (0x%08x)\n", addr, name, data, data);
}

inline void printFloat(const char *name, const void *base, const void *field, int cstBase) {
    int64_t diff = ptrToInt64(field) - ptrToInt64(base);
    int32_t addr = cstBase + (int32_t) diff;
    float   data = *reinterpret_cast<const float*>(field);
    printf("[addr=0x%05x, dtype=f32] %s: %16.8f (0x%08x)\n",
           addr, name, data, *reinterpret_cast<const int32_t*>(field));
}

inline void printDouble(const char *name, const void *base, const void *field, int cstBase) {
    int64_t diff = ptrToInt64(field) - ptrToInt64(base);
    int32_t addr = cstBase + (int32_t) diff;
    float   data = *reinterpret_cast<const double*>(field);
    printf("[addr=0x%05x, dtype=f64] %s: %16.8f (0x%016lx)\n",
           addr, name, data, *reinterpret_cast<const int64_t*>(field));
}

inline void printInt64(const char *name, const char *base, const char *field, int cstBase) {
    int64_t diff = field - base;
    int32_t addr = cstBase + (int32_t) diff;
    int64_t data = *(int64_t*) field;
    printf("[addr=0x%05x, dtype=i64] %s: %16ld (0x%016lx)\n", addr, name, data, data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace cask

#endif // INCLUDE_GUARD_CASK_PRINT_PARAMS_H
