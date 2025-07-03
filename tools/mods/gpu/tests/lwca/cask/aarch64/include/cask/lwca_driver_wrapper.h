#ifndef CASK_LWDA_DRIVER_WRAPPER_H
#define CASK_LWDA_DRIVER_WRAPPER_H
#include "cask/cask.h"
/*! Import LWCA drivers using dlopen. If the lwca dll is not found, we use a fallback implementation for each API
! that always returns LWresult::LWDA_ERROR_NOT_INITIALIZED
*/


// We only use enum types and API signatures in lwca.h, and should not need to link liblwda.so
#include <lwca.h>
#include <type_traits>
#if (defined WIN32) || (defined _WIN32) || (defined _WINDOWS)
    #ifdef CASK_CORE_LIB_STATIC_DEFINE
        #define CASK_CORE_LIB_EXPORT
    #else
        #ifndef CASK_CORE_LIB_EXPORT
            #ifdef cask_core_lib_EXPORT
                #define CASK_CORE_LIB_EXPORT __declspec(dllexport)
            #else
                #define CASK_CORE_LIB_EXPORT __declspec(dllimport)
            #endif
        #endif
    #endif
#else
    #define CASK_CORE_LIB_EXPORT
#endif


namespace cask
{
using LWresult = ::LWresult;
using LWlinkState = ::LWlinkState;
using LWfunction = ::LWfunction;
using LWmodule = ::LWmodule;
#define CASK_IMPORT_DRI_API_DECL(API_NAME) CASK_CORE_LIB_EXPORT extern const decltype(&::API_NAME) API_NAME



// Import driver API declarations here.
// Don't forget to also do definition with CASK_IMPORT_DRI_API(API_NAME) in lwdaDriverWrapper.cpp file
CASK_IMPORT_DRI_API_DECL(lwDriverGetVersion);
CASK_IMPORT_DRI_API_DECL(lwInit);
CASK_IMPORT_DRI_API_DECL(lwLinkCreate_v2);
CASK_IMPORT_DRI_API_DECL(lwLinkAddData_v2);
CASK_IMPORT_DRI_API_DECL(lwLinkComplete);
CASK_IMPORT_DRI_API_DECL(lwModuleLoadData);
CASK_IMPORT_DRI_API_DECL(lwModuleUnload);
CASK_IMPORT_DRI_API_DECL(lwGetErrorString);
CASK_IMPORT_DRI_API_DECL(lwLinkDestroy);
CASK_IMPORT_DRI_API_DECL(lwModuleGetFunction);
CASK_IMPORT_DRI_API_DECL(lwFuncSetAttribute);
CASK_IMPORT_DRI_API_DECL(lwFuncGetAttribute);
CASK_IMPORT_DRI_API_DECL(lwLaunchKernel);
CASK_IMPORT_DRI_API_DECL(lwGetErrorName);
CASK_IMPORT_DRI_API_DECL(lwModuleLoadFatBinary);
CASK_IMPORT_DRI_API_DECL(lwModuleLoadDataEx);
CASK_IMPORT_DRI_API_DECL(lwLinkAddFile_v2);
CASK_IMPORT_DRI_API_DECL(lwCtxPushLwrrent);
CASK_IMPORT_DRI_API_DECL(lwCtxPopLwrrent);
CASK_IMPORT_DRI_API_DECL(lwCtxGetDevice);
CASK_IMPORT_DRI_API_DECL(lwDevicePrimaryCtxRetain);
CASK_IMPORT_DRI_API_DECL(lwDevicePrimaryCtxRelease);
CASK_IMPORT_DRI_API_DECL(lwDevicePrimaryCtxReset);
CASK_IMPORT_DRI_API_DECL(lwDeviceGet);
CASK_IMPORT_DRI_API_DECL(lwDeviceGetAttribute);
CASK_IMPORT_DRI_API_DECL(lwStreamSynchronize);
CASK_IMPORT_DRI_API_DECL(lwOclwpancyMaxActiveBlocksPerMultiprocessor);

#undef CASK_IMPORT_DRI_API_DECL

LWresult caskLwFuncSetAttribute ( LWfunction hfunc, LWfunction_attribute attrib, int  value );
LWresult caskLwLaunchKernel ( LWfunction f, unsigned int  gridDimX, unsigned int  gridDimY, unsigned int  gridDimZ, unsigned int  blockDimX, unsigned int  blockDimY, unsigned int  blockDimZ, unsigned int  sharedMemBytes, LWstream hStream, void** kernelParams, void** extra );
} // namespace cask

#endif // CASK_LWDA_DRIVER_WRAPPER_H
