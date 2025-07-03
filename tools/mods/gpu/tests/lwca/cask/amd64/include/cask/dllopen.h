#ifndef INCLUDE_GUARD_CASK_DLL_OPEN_H
#define INCLUDE_GUARD_CASK_DLL_OPEN_H
#if defined(_WIN32)
// for windows lwrtc name, please refer to
// https://docs.lwpu.com/lwca/lwrtc/index.html#lwrtc-library-versioning
#define LWDA_LIB_NAME "lwca"
#define LWRTC_LIB_NAME "rtc64_112_0"
#else
#define LWDA_LIB_NAME "lwca"
#define LWRTC_LIB_NAME "lwrtc"
#endif

#if defined(_WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif // defined(WIN32_LEAN_AND_MEAN)
#include <windows.h>
typedef HMODULE DynamicLibrary;
#define dllOpen_(name) \
            LoadLibraryA(name)
#define dllOpen(name) \
            LoadLibraryA("lw" name ".dll")
#define dllClose(handle) \
            FreeLibrary(handle)
#define dllGetSym(handle, name) \
            GetProcAddress(handle, name)
#else
#include <dlfcn.h>
typedef void *DynamicLibrary;
#define dllOpen_(name) \
            dlopen(name, RTLD_LAZY)
#define dllOpen(name) \
            dlopen("lib" name ".so", RTLD_LAZY)
#define dllClose(handle) \
            dlclose(handle)
#define dllGetSym(handle, name) \
            dlsym(handle, name)
#endif

#endif // INCLUDE_GUARD_CASK_DLL_OPEN_H
