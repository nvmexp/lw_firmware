#ifndef CASK_LWRTC_WRAPPER_H
#define CASK_LWRTC_WRAPPER_H
#include "cask/cask.h"

/*! Import lwrtc drivers using dlopen. If the lwrtc dll is not found, we use a fallback implementation for each API
! that always returns lwrtcResult::LWRTC_ERROR_INTERNAL_ERROR
*/


// We only use enum types and API signatures in lwrtc.h, and should not need to link liblwrtc.so
#include <lwrtc.h>
#include <type_traits>

// internal lwrtc API
#include "cask/internal/lwrtc_internal.h"

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
using lwrtcResult = ::lwrtcResult;
using lwrtcProgram = ::lwrtcProgram;

#define CASK_IMPORT_LWRTC_API_DECL(API_NAME) CASK_CORE_LIB_EXPORT extern const decltype(&::API_NAME) API_NAME

// Import lwrtc API declarations here.
// Don't forget to also do definition with CASK_IMPORT_LWRTC_API(API_NAME) in the lwrtcWrapper.cpp file
CASK_IMPORT_LWRTC_API_DECL(lwrtcCreateProgram);
CASK_IMPORT_LWRTC_API_DECL(lwrtcGetProgramLogSize);
CASK_IMPORT_LWRTC_API_DECL(lwrtcCompileProgram);
CASK_IMPORT_LWRTC_API_DECL(lwrtcGetLoweredName);
CASK_IMPORT_LWRTC_API_DECL(lwrtcGetErrorString);
CASK_IMPORT_LWRTC_API_DECL(lwrtcDestroyProgram);
CASK_IMPORT_LWRTC_API_DECL(lwrtcGetProgramLog);
CASK_IMPORT_LWRTC_API_DECL(lwrtcGetPTXSize);
CASK_IMPORT_LWRTC_API_DECL(lwrtcGetPTX);
CASK_IMPORT_LWRTC_API_DECL(lwrtcAddNameExpression);
CASK_IMPORT_LWRTC_API_DECL(lwrtcVersion);
CASK_IMPORT_LWRTC_API_DECL(__lwrtcCPEx);

#if defined(CASK_ENABLE_LW_LINKER_API) || LWDA_VERSION >= 11030
CASK_IMPORT_LWRTC_API_DECL(lwrtcGetLWBINSize);
CASK_IMPORT_LWRTC_API_DECL(lwrtcGetLWBIN);
#endif

#undef CASK_CASK_IMPORT_LWRTC_API_DECL
} // namespace cask

#endif // CASK_LWRTC_WRAPPER_H

