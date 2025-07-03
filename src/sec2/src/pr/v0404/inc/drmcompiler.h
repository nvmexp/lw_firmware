/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMCOMPILER_H__
#define __DRMCOMPILER_H__

#include <drmfeatures.h>
#include <oemcompiler.h>
// LWE (kwilson) add SEC2 header files
#include <pr/pr_common.h>
#include "sec2sw.h"

#if !defined(DRM_DBG)
#if DBG
#define DRM_DBG 1
#endif /* DBG */
#endif /* DRM_DBG */

#if defined( __powerpc__   ) || defined( __ppc__   ) || defined( __PPC__  ) || \
    defined( __powerpc64__ ) || defined( __ppc64__ ) || defined( __PPC64__)
#define DRM_ARCH_POWERPC 1
#endif

/*
** +---------------------+
** | DRM_VOID definition |
** +---------------------+
*/
#if !DRM_VOID_DEFINED
#undef DRM_VOID_DEFINED
#define DRM_VOID_DEFINED 1
#if DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_IOS || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_LINUX || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_ANDROID || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_MAC
#define DRM_VOID    void
#else
typedef void      DRM_VOID;
#endif /* DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_IOS || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_LINUX || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_ANDROID || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_MAC */
#endif /* !DRM_VOID_DEFINED */

/*
** +------------------+
** | Compiler Version |
** +------------------+
*/

/*
** Set DRM_GNUC_MAJOR and DRM_GNUC_MINOR if not already set.
*/
#if defined(__GNUC__) && !defined(DRM_GNUC_MAJOR)
#define DRM_GNUC_MAJOR __GNUC__
#endif /* defined(__GNUC__) && !defined(DRM_GNUC_MAJOR) */
#if defined(__GNUC_MINOR__) && !defined(DRM_GNUC_MINOR)
#define DRM_GNUC_MINOR __GNUC_MINOR__
#endif /* defined(__GNUC_MINOR__) && !defined(DRM_GNUC_MINOR) */

#if defined(DRM_GNUC_MAJOR) && !defined(DRM_GNUC_MINOR)
#error You or your compiler is setting DRM_GNUC_MAJOR (possibly by setting __GNUC__) but not DRM_GNUC_MINOR (possibly by not setting __GNUC_MINOR__).  You must ensure that both are set or neither are set.
#elif !defined(DRM_GNUC_MAJOR) && defined(DRM_GNUC_MINOR)
#error You or your compiler is setting DRM_GNUC_MINOR (possibly by setting __GNUC_MINOR__) but not DRM_GNUC_MAJOR (possibly by not setting __GNUC__).  You must ensure that both are set or neither are set.
#endif /* defined(DRM_GNUC_MAJOR) && !defined(DRM_GNUC_MINOR) */

/*
** Set DRM_MSC_VER if not already set.
**
** Some lwstomers have codebases that set _MSC_VER to values less than 1300
** when using 3rd party compilers in order to leverage Microsoft headers.
** So, only define DRM_MSC_VER to _MSC_VER if it is at least 1300.
*/
#if !defined(DRM_MSC_VER) && defined(_MSC_VER) && ( _MSC_VER >= 1300 )
#define DRM_MSC_VER _MSC_VER
#endif /* !defined(DRM_MSC_VER) && defined(_MSC_VER) && ( _MSC_VER >= 1300 ) */

/*
** +--------------------------+
** | PREFAST WARNING HANDLERS |
** +--------------------------+
*/
#if defined(_PREFAST_)
#define PREFAST_PUSH_DISABLE_EXPLAINED(warning, explanation) \
    __pragma(prefast(push))\
    __pragma(prefast(disable:warning,explanation))
#define PREFAST_POP __pragma(prefast(pop))

#define PREFAST_PUSH_IGNORE_NONCONST_PARAMS( __reason )                               \
    PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, __reason )        \
    PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_BUFFER_PARAM_25033, __reason )
#define PREFAST_POP_IGNORE_NONCONST_PARAMS                      \
    PREFAST_POP  /* __WARNING_NONCONST_BUFFER_PARAM_25033 */    \
    PREFAST_POP  /* __WARNING_NONCONST_PARAM_25004 */

#define PREFAST_PUSH_IGNORE_NONCONST_PARAMS_FOR_STUB_IMPL PREFAST_PUSH_IGNORE_NONCONST_PARAMS( "Changing parameters to const to satisfy warning would make them not match the real interface." )
#define PREFAST_POP_IGNORE_NONCONST_PARAMS_FOR_STUB_IMPL  PREFAST_POP_IGNORE_NONCONST_PARAMS

#define PREFAST_PUSH_IGNORE_ARGUMENT_CHECKS_FOR_TESTING                                                                                                                                     \
    PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_DEREFERENCING_NULL_POINTER_6011, "Test code intentionally passes invalid parameters to test argument checking in dev code." )                 \
    PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_ILWALID_PARAMETER_6387, "Test code intentionally passes invalid parameters to test argument checking in dev code." )                          \
    PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_PRECONDITION_NULLTERMINATION_VIOLATION_26035, "Test code intentionally passes invalid parameters to test argument checking in dev code." )    \
    PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_POTENTIAL_RANGE_PRECONDITION_VIOLATION_26070, "Test code intentionally passes invalid parameters to test argument checking in dev code." )    \
    PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_POINTER_COPY_COULD_BE_NULL_28183, "Test code intentionally passes invalid parameters to test argument checking in dev code." )                \

#define PREFAST_POP_IGNORE_ARGUMENT_CHECKS_FOR_TESTING                                                                                                                          \
    PREFAST_POP  /* __WARNING_POINTER_COPY_COULD_BE_NULL_28183 */                                                                                                               \
    PREFAST_POP  /* __WARNING_POTENTIAL_RANGE_PRECONDITION_VIOLATION_26070 */                                                                                                   \
    PREFAST_POP  /* __WARNING_PRECONDITION_NULLTERMINATION_VIOLATION_26035 */                                                                                                   \
    PREFAST_POP  /* __WARNING_ILWALID_PARAMETER_6387 */                                                                                                                         \
    PREFAST_POP  /* __WARNING_DEREFERENCING_NULL_POINTER_6011 */                                                                                                                \

#else /* defined(_PREFAST_) */
#define PREFAST_PUSH_DISABLE_EXPLAINED(warning, explanation)
#define PREFAST_POP
#define PREFAST_PUSH_IGNORE_NONCONST_PARAMS( __reason )
#define PREFAST_POP_IGNORE_NONCONST_PARAMS
#define PREFAST_PUSH_IGNORE_NONCONST_PARAMS_FOR_STUB_IMPL
#define PREFAST_POP_IGNORE_NONCONST_PARAMS_FOR_STUB_IMPL
#define PREFAST_PUSH_IGNORE_ARGUMENT_CHECKS_FOR_TESTING
#define PREFAST_POP_IGNORE_ARGUMENT_CHECKS_FOR_TESTING
#endif /* defined(_PREFAST_) */

/*
** +-----------------------+
** | PRAGMA WARNING MACROS |
** +-----------------------+
*/
#if !DRM_PRAGMA_WARNING_MACROS_DEFINED

#if defined(PRAGMA_INTRINSIC) || defined(PRAGMA_WARNING_DEFAULT) || defined(PRAGMA_WARNING_DISABLE)  \
 || defined(PRAGMA_WARNING_PUSH) || defined(PRAGMA_WARNING_POP) || defined(PRAGMA_WARNING_PUSH_WARN) \
 || defined(PRAGMA_PACK_PUSH_VALUE) || defined(PRAGMA_PACK_POP) || defined(PRAGMA_DIAG_OFF)          \
 || defined(PRAGMA_DIAG_ON)
#error You did not set DRM_PRAGMA_WARNING_MACROS_DEFINED to 1 but at least one of its macros is already defined - please refer to oemcompiler.h near DRM_PRAGMA_WARNING_MACROS_DEFINED
#endif /* At least one PRAGMA_WARNING macro is already defined */

#undef DRM_PRAGMA_WARNING_MACROS_DEFINED
#define DRM_PRAGMA_WARNING_MACROS_DEFINED 1

#ifdef DRM_GNUC_MAJOR

#define PRAGMA_INTRINSIC(func)
#define PRAGMA_WARNING_DEFAULT(warningnum)
#define PRAGMA_WARNING_DISABLE(warningnum)
#define PRAGMA_WARNING_PUSH
#define PRAGMA_WARNING_POP
#define PRAGMA_WARNING_PUSH_WARN(warningnum)
#define PRAGMA_PACK_PUSH_VALUE(packval)
#define PRAGMA_PACK_POP

#define DRM_STRINGIFY_TEMP(s) #s
// LWE (nkuo) - remark the Pragma due to the error "#pragma GCC diagnostic not allowed inside functions" in older gcc compiler
#define PRAGMA_DIAG_WITH_TEMP(s) // _Pragma( DRM_STRINGIFY_TEMP( GCC diagnostic s ) )

#if ((DRM_GNUC_MAJOR >= 4) && (DRM_GNUC_MINOR >= 6))
#define PRAGMA_DIAG_OFF(x, reason) PRAGMA_DIAG_WITH_TEMP( push )  \
                PRAGMA_DIAG_WITH_TEMP( ignored DRM_STRINGIFY_TEMP(-W ## x) )
#define PRAGMA_DIAG_ON(x) PRAGMA_DIAG_WITH_TEMP( pop )
#elif ((DRM_GNUC_MAJOR >= 4) && (DRM_GNUC_MINOR >= 2))
#define PRAGMA_DIAG_OFF(x, reason) PRAGMA_DIAG_WITH_TEMP( ignored DRM_STRINGIFY_TEMP(-W ## x) )
#define PRAGMA_DIAG_ON(x) PRAGMA_DIAG_WITH_TEMP( warning DRM_STRINGIFY_TEMP(-W ## x) )
#else /* DRM_GNUC_MAJOR < 4 || DRM_GNUC_MINOR < 2 */
#define PRAGMA_DIAG_OFF(x, reason)
#define PRAGMA_DIAG_ON(x)
#endif /* various DRM_GNUC_MAJOR versions */

#else /* DRM_GNUC_MAJOR */

#define PRAGMA_INTRINSIC(func) __pragma(intrinsic(func))
#define PRAGMA_WARNING_DEFAULT(warningnum) __pragma(warning(default:warningnum))
#define PRAGMA_WARNING_DISABLE(warningnum) __pragma(warning(disable:warningnum))
#define PRAGMA_WARNING_PUSH __pragma(warning(push))
#define PRAGMA_WARNING_POP __pragma(warning(pop))
#define PRAGMA_WARNING_PUSH_WARN(warningnum)    \
    __pragma(warning(push))                     \
    __pragma(warning(disable:warningnum))
#define PRAGMA_PACK_PUSH_VALUE(packval)         \
    __pragma(pack(push))                        \
    __pragma(pack(packval))
#define PRAGMA_PACK_POP __pragma(pack(pop))

#define PRAGMA_DIAG_OFF(x, reason)
#define PRAGMA_DIAG_ON(x)

#endif /* DRM_GNUC_MAJOR */

#endif /* DRM_PRAGMA_WARNING_MACROS_DEFINED */

#if !defined(PRAGMA_INTRINSIC) || !defined(PRAGMA_WARNING_DEFAULT) || !defined(PRAGMA_WARNING_DISABLE)  \
 || !defined(PRAGMA_WARNING_PUSH) || !defined(PRAGMA_WARNING_POP) || !defined(PRAGMA_WARNING_PUSH_WARN) \
 || !defined(PRAGMA_PACK_PUSH_VALUE) || !defined(PRAGMA_PACK_POP) || !defined(PRAGMA_DIAG_OFF)          \
 || !defined(PRAGMA_DIAG_ON)
#error You set DRM_PRAGMA_WARNING_MACROS_DEFINED to 1 but at least one of its macros is not defined - please refer to oemcompiler.h near DRM_PRAGMA_WARNING_MACROS_DEFINED
#endif /* At least one PRAGMA_WARNING macro is not defined */

/*
** +----------------------------------------+
** | 64-BIT TARGET ARCHITECTURE DEFINITIONS |
** +----------------------------------------+
*/
#if !DRM_64BIT_TARGET_DEFINED

#if defined(DRM_64BIT_TARGET)
#error You did not set DRM_64BIT_TARGET_DEFINED to 1 but DRM_64BIT_TARGET is already defined - please refer to oemcompiler.h near DRM_64BIT_TARGET_DEFINED
#endif  /* DRM_64BIT_TARGET is already defined */

#undef DRM_64BIT_TARGET_DEFINED
#define DRM_64BIT_TARGET_DEFINED 1

#if defined(_WIN64)
#define DRM_64BIT_TARGET         1
#elif DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_MAC
#define DRM_64BIT_TARGET         1
#elif defined( __powerpc64__ ) || defined( __ppc64__ ) || defined( __PPC64__)
#define DRM_64BIT_TARGET         1
#elif defined( ARM64 )
#define DRM_64BIT_TARGET         1
#else /* Unknown architecture */
/*
** Assume the target is a 32-bit architecture.
** If the target is 64-bit, DRM_64BIT_TARGET should be set to 1.
*/
#define DRM_64BIT_TARGET         0
#endif /* Known architectures check */
#endif /* DRM_64BIT_TARGET_DEFINED */

#if !defined(DRM_64BIT_TARGET)
#error You set DRM_64BIT_TARGET_DEFINED to 1 but did not define DRM_64BIT_TARGET - please refer to oemcompiler.h near DRM_64BIT_TARGET_DEFINED
#endif /* DRM_64BIT_TARGET is not defined */

/*
** +----------------------------------------+
** | GS STRICT AND DBG OPTIMIZATION PRAGMAS |
** +----------------------------------------+
*/
#if !DRM_STRICT_AND_OPTIMIZATION_PRAGMAS_DEFINED

#if defined(PRAGMA_STRICT_GS_PUSH_ON) || defined(PRAGMA_STRICT_GS_POP) || defined(PRAGMA_DBG_OPTIMIZATION_OFF)         \
 || defined(PRAGMA_DBG_OPTIMIZATION_ON) || defined(PRAGMA_ARM_OPTIMIZATION_OFF) || defined(PRAGMA_ARM_OPTIMIZATION_ON) \
 || defined(PRAGMA_GCC_OPTIMIZATION_OFF) || defined(PRAGMA_GCC_OPTIMIZATION_ON)
#error You did not set DRM_STRICT_AND_OPTIMIZATION_PRAGMAS_DEFINED to 1 but at least one of its macros is already defined - please refer to oemcompiler.h near DRM_STRICT_AND_OPTIMIZATION_PRAGMAS_DEFINED
#endif /* At least one STRICT_AND_OPTIMIZATION_PRAGMA macro is already defined */

#undef DRM_STRICT_AND_OPTIMIZATION_PRAGMAS_DEFINED
#define DRM_STRICT_AND_OPTIMIZATION_PRAGMAS_DEFINED 1

#if defined(DRM_MSC_VER) && !defined(DRM_GNUC_MAJOR)
#define PRAGMA_STRICT_GS_PUSH_ON    __pragma(strict_gs_check(push, on))
#define PRAGMA_STRICT_GS_POP        __pragma(strict_gs_check(pop))
#if DRM_EXPORT_APIS_TO_DLL
#define PRAGMA_COV_OPTIMIZATION_OFF  __pragma(optimize("",off))
#endif  /* DRM_EXPORT_APIS_TO_DLL */
#if DRM_DBG
#define PRAGMA_DBG_OPTIMIZATION_OFF __pragma(optimize("",off))
#define PRAGMA_DBG_OPTIMIZATION_ON  __pragma(optimize("",on))
#else   /* DRM_DBG */
#define PRAGMA_DBG_OPTIMIZATION_OFF
#define PRAGMA_DBG_OPTIMIZATION_ON
#endif  /* DRM_DBG */
#if defined( ARM )
#define PRAGMA_ARM_OPTIMIZATION_OFF __pragma(optimize("",off))
#define PRAGMA_ARM_OPTIMIZATION_ON  __pragma(optimize("",on))
#else /* defined( ARM ) */
#define PRAGMA_ARM_OPTIMIZATION_OFF
#define PRAGMA_ARM_OPTIMIZATION_ON
#endif /* defined( ARM ) */
#define PRAGMA_GCC_OPTIMIZATION_OFF
#define PRAGMA_GCC_OPTIMIZATION_ON
#else /* defined(DRM_MSC_VER) && !defined(DRM_GNUC_MAJOR) */
#if defined(DRM_GNUC_MAJOR)
#define PRAGMA_STRICT_GS_PUSH_ON
#define PRAGMA_STRICT_GS_POP

#if ((DRM_GNUC_MAJOR >= 4) && (DRM_GNUC_MINOR >= 4)) && ( defined(__i386__) || defined(__amd64__) || defined(DRM_ARCH_POWERPC) )
#define PRAGMA_GCC_OPTIMIZATION_OFF _Pragma( "GCC push_options" )      \
                                    _Pragma( "GCC optimize(\"-O0\")" )
#define PRAGMA_GCC_OPTIMIZATION_ON  _Pragma( "GCC pop_options" )
#else /* ((DRM_GNUC_MAJOR >= 4) && (DRM_GNUC_MINOR >= 4)) && ( defined(__i386__) || defined(__amd64__) || defined(DRM_ARCH_POWERPC) ) */
#define PRAGMA_GCC_OPTIMIZATION_OFF
#define PRAGMA_GCC_OPTIMIZATION_ON
#endif /* ((DRM_GNUC_MAJOR >= 4) && (DRM_GNUC_MINOR >= 4)) && ( defined(__i386__) || defined(__amd64__) || defined(DRM_ARCH_POWERPC) ) */
#if DRM_DBG
#define PRAGMA_DBG_OPTIMIZATION_OFF PRAGMA_GCC_OPTIMIZATION_OFF
#define PRAGMA_DBG_OPTIMIZATION_ON  PRAGMA_GCC_OPTIMIZATION_ON
#else /* DRM_DBG */
#define PRAGMA_DBG_OPTIMIZATION_OFF
#define PRAGMA_DBG_OPTIMIZATION_ON
#endif /* DRM_DBG */
#define PRAGMA_ARM_OPTIMIZATION_OFF
#define PRAGMA_ARM_OPTIMIZATION_ON
#else /* defined(DRM_GNUC_MAJOR) */
#define PRAGMA_STRICT_GS_PUSH_ON
#define PRAGMA_STRICT_GS_POP
#define PRAGMA_DBG_OPTIMIZATION_OFF
#define PRAGMA_DBG_OPTIMIZATION_ON
#define PRAGMA_ARM_OPTIMIZATION_OFF
#define PRAGMA_ARM_OPTIMIZATION_ON
#define PRAGMA_GCC_OPTIMIZATION_OFF
#define PRAGMA_GCC_OPTIMIZATION_ON
#endif /* defined(DRM_GNUC_MAJOR) */
#endif /* defined(DRM_MSC_VER) && !defined(DRM_GNUC_MAJOR) */
#endif /* !DRM_STRICT_AND_OPTIMIZATION_PRAGMAS_DEFINED */

#if !defined(PRAGMA_STRICT_GS_PUSH_ON) || !defined(PRAGMA_STRICT_GS_POP) || !defined(PRAGMA_DBG_OPTIMIZATION_OFF)         \
 || !defined(PRAGMA_DBG_OPTIMIZATION_ON) || !defined(PRAGMA_ARM_OPTIMIZATION_OFF) || !defined(PRAGMA_ARM_OPTIMIZATION_ON) \
 || !defined(PRAGMA_GCC_OPTIMIZATION_OFF) || !defined(PRAGMA_GCC_OPTIMIZATION_ON)
#error You set DRM_STRICT_AND_OPTIMIZATION_PRAGMAS_DEFINED to 1 but at least one of its macros is not defined - please refer to oemcompiler.h near DRM_STRICT_AND_OPTIMIZATION_PRAGMAS_DEFINED
#endif /* At least one STRICT_AND_OPTIMIZATION_PRAGMA macro is not defined */

/*
** +--------------------------------------+
** | DISABLE OPTIMIZATIONS WHEN REQUESTED |
** +--------------------------------------+
*/
#if DRM_NO_OPT
PRAGMA_DBG_OPTIMIZATION_OFF
#endif /* DRM_NO_OPT */
#undef PRAGMA_DBG_OPTIMIZATION_OFF

#if !defined(PRAGMA_COV_OPTIMIZATION_OFF)
#define PRAGMA_COV_OPTIMIZATION_OFF
#endif  /* PRAGMA_COV_OPTIMIZATION_OFF */

PRAGMA_COV_OPTIMIZATION_OFF
#undef PRAGMA_DBG_OPTIMIZATION_OFF

/*
** +-----------------------------------+
** | COMPILATION QUALIFIER DEFINITIONS |
** +-----------------------------------+
*/

#if !DRM_COMPILATION_QUALIFIERS_DEFINED

#if defined(DRM_CCALL) || defined(DRM_DLLEXPORT) || defined(DRM_DLLIMPORT) || defined(DRM_ALIGN_4) \
 || defined(DRM_ALIGN_8) || defined(DRM_PACKED) || defined(DRM_DISCARDABLE)
#error You did not set DRM_COMPILATION_QUALIFIERS_DEFINED to 1 but at least one of its macros is already defined - please refer to oemcompiler.h near DRM_COMPILATION_QUALIFIERS_DEFINED
#endif /* At least one COMPILATION_QUALIFIERS macro is already defined */

#undef DRM_COMPILATION_QUALIFIERS_DEFINED
#define DRM_COMPILATION_QUALIFIERS_DEFINED 1

/* ====================== Microsoft specific qualifiers ======================= */
/* ============================================================================ */
#if defined(DRM_MSC_VER)

#define DRM_CCALL      __cdecl
#define DRM_DLLEXPORT  __declspec(dllexport)
#define DRM_DLLIMPORT  __declspec(dllimport)
#if DRM_SUPPORT_FORCE_ALIGN
#define DRM_ALIGN_4    __declspec(align(4))
#define DRM_ALIGN_8    __declspec(align(8))
#endif /* DRM_SUPPORT_FORCE_ALIGN */
#define DRM_PACKED

/*
** Microsoft linkers require that extern const data be marked with __declspec(selectany) for it to be discarded
** when not used.  If this tag is not present the data will be linked into final binaries regardless of it being necessary
** or not.
*/
#define DRM_DISCARDABLE __declspec(selectany)

/* ========================= GNU specific qualifiers ========================== */
/* ============================================================================ */
#elif defined(DRM_GNUC_MAJOR)

#define DRM_CCALL      __attribute__((cdecl))
#define DRM_DLLEXPORT  __attribute__((dllexport))
#define DRM_DLLIMPORT  __attribute__((dllimport))
#if DRM_SUPPORT_FORCE_ALIGN
#define DRM_ALIGN_4    __attribute__((aligned(4)))
#define DRM_ALIGN_8    __attribute__((aligned(8)))
#endif /* DRM_SUPPORT_FORCE_ALIGN */
#define DRM_PACKED     __attribute__((__packed__))

/*
**  Set to nothing as there is no comparable setting
*/
#define DRM_DISCARDABLE

/* ======================== Default (empty) qualifiers ======================== */
/* ============================================================================ */
#else /* neither DRM_MSC_VER nor DRM_GNUC_MAJOR */

#define DRM_CCALL
#define DRM_DLLEXPORT
#define DRM_DLLIMPORT
#define DRM_ALIGN_4
#define DRM_ALIGN_8
#define DRM_PACKED
#define DRM_DISCARDABLE

#endif /* DRM_MSC_VER or DRM_GNUC_MAJOR */

#endif /* !DRM_COMPILATION_QUALIFIERS_DEFINED */

#if !DRM_SUPPORT_FORCE_ALIGN
#undef DRM_ALIGN_4
#define DRM_ALIGN_4
#undef DRM_ALIGN_8
#define DRM_ALIGN_8
#endif /* !DRM_SUPPORT_FORCE_ALIGN */

#if !defined(DRM_CCALL) || !defined(DRM_DLLEXPORT) || !defined(DRM_DLLIMPORT) || !defined(DRM_ALIGN_4) \
 || !defined(DRM_ALIGN_8) || !defined(DRM_PACKED) || !defined(DRM_DISCARDABLE)
#error You set DRM_COMPILATION_QUALIFIERS_DEFINED to 1 but at least one of its macros is not defined - please refer to oemcompiler.h near DRM_COMPILATION_QUALIFIERS_DEFINED
#endif /* At least one COMPILATION_QUALIFIERS macro is not defined */

/*
** +-------------------------------+
** | COMPILATION BEHAVIOR SETTINGS |
** +-------------------------------+
*/
#if !DRM_COMPILATION_BEHAVIOR_DEFINED
/*
#if defined(DRM_NO_INLINE_ATTRIBUTE) || defined(DRM_ALWAYS_INLINE_ATTRIBUTE) || defined(DRM_CALL) \
 || defined(DRM_ALWAYS_INLINE) || defined(DRM_EXTERN_INLINE) || defined(DRM_EXPORTED_INLINE)      \
 || defined(DRM_INLINING_SUPPORTED) || defined(DRM_DWORD_ALIGN) || defined(DRM_NO_INLINE)
#error You did not set DRM_COMPILATION_BEHAVIOR_DEFINED to 1 but at least one of its macros is already defined - please refer to oemcompiler.h near DRM_COMPILATION_BEHAVIOR_DEFINED
#endif // At least one COMPILATION_BEHAVIOR macro is already defined 
*/

#undef DRM_COMPILATION_BEHAVIOR_DEFINED
#define DRM_COMPILATION_BEHAVIOR_DEFINED 1

/* ============================ Microsoft Compiler ============================ */
/* ============================================================================ */
#if defined(DRM_MSC_VER)

// LWE (kwilson) no inlining attribute
//#define DRM_NO_INLINE_ATTRIBUTE
//#define DRM_ALWAYS_INLINE_ATTRIBUTE

/* ------------------------------ MS ANSI build ------------------------------- */
/* ---------------------------------------------------------------------------- */
/* when compiling as ANSI, parameter-passing specifications aren't allowed      */
#if defined(__STDC__)

#define DRM_CALL
#define DRM_ALWAYS_INLINE
#define DRM_EXTERN_INLINE
#define DRM_EXPORTED_INLINE
#define DRM_INLINING_SUPPORTED 0
#define DRM_DWORD_ALIGN
#define DRM_NO_INLINE

/* ---------------------------- MS non-ANSI build ----------------------------- */
/* ---------------------------------------------------------------------------- */
#else /* __STDC__ */

/* ----------------- Building the PK DLL ----------------- */
/* Set the macros to export the APIs and global variables  */
#if DRM_EXPORT_APIS_TO_DLL
#define DRM_API         DRM_DLLEXPORT
#define DRM_API_VOID    DRM_DLLEXPORT
#define DRM_EXPORT_VAR  DRM_DLLEXPORT
#endif  /* DRM_EXPORT_APIS_TO_DLL */

/* ------------ Building the Test Exelwtables ------------ */
/* Set the macro to import the global variables            */
#if DRM_TEST_LINK_TO_DRMAPI_DLL
#define DRM_EXPORT_VAR  DRM_DLLIMPORT
#endif /* DRM_TEST_LINK_TO_DRMAPI_DLL */

#define DRM_CALL                DRM_CCALL
#define DRM_ALWAYS_INLINE       __forceinline
#define DRM_EXTERN_INLINE       extern _inline
#define DRM_EXPORTED_INLINE     _inline
// LWE (kwilson) no inlining supported
#define DRM_INLINING_SUPPORTED  0
#define DRM_DWORD_ALIGN         DRM_ALIGN_4
// LWE (kwilson) no inlining supported
#define DRM_NO_INLINE           //__declspec(noinline)

#endif /* __STDC__ */

/* =============================== GNU Compiler =============================== */
/* ============================================================================ */
#elif defined(DRM_GNUC_MAJOR)

// LWE (kwilson) no inlining supported
//#define DRM_NO_INLINE_ATTRIBUTE __attribute__((noinline))
//#define DRM_ALWAYS_INLINE_ATTRIBUTE __attribute__((always_inline))
#define DRM_CALL

// LWE (kwilson) no inlining supported
#if DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_IOS || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_ANDROID
#define DRM_ALWAYS_INLINE       inline __attribute__((always_inline))
#else
#define DRM_ALWAYS_INLINE        inline
#endif /* DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_IOS || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_ANDROID */

#define DRM_EXTERN_INLINE       extern
#define DRM_EXPORTED_INLINE

#define DRM_INLINING_SUPPORTED  0
#define DRM_DWORD_ALIGN         DRM_ALIGN_4
#define DRM_NO_INLINE

/* ============================= Unknown Compiler ============================= */
/* ============================================================================ */
#else /* Neither DRM_MSC_VER nor DRM_GNUC_MAJOR is defined */
#error Unknown compiler - please refer to oemcompiler.h near DRM_COMPILATION_BEHAVIOR_DEFINED
#endif

#endif /* !DRM_COMPILATION_BEHAVIOR_DEFINED */
/*
#if !defined(DRM_NO_INLINE_ATTRIBUTE) || !defined(DRM_ALWAYS_INLINE_ATTRIBUTE) || !defined(DRM_CALL) \
 || !defined(DRM_ALWAYS_INLINE) || !defined(DRM_EXTERN_INLINE) || !defined(DRM_EXPORTED_INLINE)      \
 || !defined(DRM_INLINING_SUPPORTED) || !defined(DRM_DWORD_ALIGN) || !defined(DRM_NO_INLINE)
#error You set DRM_COMPILATION_BEHAVIOR_DEFINED to 1 but at least one of its macros is not defined - please refer to oemcompiler.h near DRM_COMPILATION_BEHAVIOR_DEFINED
#endif // At least one COMPILATION_BEHAVIOR macro is not defined 
*/

/*
** Lwrrently, creating a DLL is only supported on Microsoft compiler non-ANSI builds,
** so ensure that the following DLL specific macros are empty if not set above.
*/
#if !defined(DRM_API)
#define DRM_API_DEFAULT 1
#define DRM_API
#define DRM_API_VOID
#endif  /* DRM_API */

#if !defined(DRM_EXPORT_VAR)
#define DRM_EXPORT_VAR
#endif  /* DRM_EXPORT_VAR */

/*
** PlayReady PK function declaration should match the following pattern.
**   [static] [DRM_*_INLINE] [DRM_API[_VOID]] <return_type> [DRM_CALL] func(...)
**
**   The following additional restrictions apply.
**   +) DRM_API must not appear without DRM_CALL.
**   +) DRM_API must not appear if <return_type> is either void or DRM_VOID.  (DRM_API_VOID is allowed.)
**   +) DRM_API must not appear with static.
**
**   The following additional preferences apply.
**   +) DRM_API should not appear where static can be used instead.  (i.e. Prefer static.)
**   +) DRM_API or static should appear for all PK-DEV functions.  ("PK-DEV functions" are in files under msi\source)
**   +) DRM_CALL should appear for all PK functions.  ("PK functions" are in files under msi)
**
** Any function failing to meet the above characteristics should be considered a bug to be fixed in a future release.
**
** Rationale behind the pattern:
**   +) Although a number of the pattern's items can appear in a variety of sequence-orderings,
**      using a consistent ordering improves overall codebase readibility.
**   +) Using static first calls out that that the function cannot be called outside this file.
**   +) Including <return_type> immediately before the function name keeps all types used by
**      the function co-located and thus easy to find.
**   +) DRM_API and DRM_API_VOID are typically used internally by Microsoft by code-coverage
**      and static analysis tools.  Inlining macros are used both internally and externally
**      for performance.  Therefore, the inlining macros are placed earlier to make them more obvious.
**
** Rationale behind the additional restrictions:
**   +) All of these restrictions are required for Microsoft's internal code-coverage
**      and static analysis tools to function properly / give accurate data.
**
** Rationale behind additional prefernces:
**   +) Using "static" on a function enables better optimization on some compilers.
**   +) Using DRM_API for non-static functions enables Microsoft's internal code-coverage
**      tools to gather better data.
**   +) Using a consistent calling convention enables better optimization on some compilers.
*/

/*
** +-------------------------------------------------+
** | TARGET ENDIANNESS AND UNALIGNED POINTER SUPPORT |
** +-------------------------------------------------+
*/
#if !defined(TARGET_LITTLE_ENDIAN) || !defined(TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS)
#if defined(_M_IX86)       /* Microsoft X86 compiler detected   */
#ifndef TARGET_LITTLE_ENDIAN
#define TARGET_LITTLE_ENDIAN                        1
#endif  /* TARGET_LITTLE_ENDIAN */
#ifndef TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS
#define TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS    1
#endif  /* TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS */
#elif defined( _M_AMD64 )  /* Microsoft AMD64 compiler detected */
#ifndef TARGET_LITTLE_ENDIAN
#define TARGET_LITTLE_ENDIAN                        1
#endif  /* TARGET_LITTLE_ENDIAN */
#ifndef TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS
#define TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS    1
#endif  /* TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS */
#elif defined( _M_IA64 )   /* Microsoft IA64 compiler detected  */
#ifndef TARGET_LITTLE_ENDIAN
#define TARGET_LITTLE_ENDIAN                        1
#endif  /* TARGET_LITTLE_ENDIAN */
#ifndef TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS
#define TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS    1
#endif  /* TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS */
#elif defined( _XBOX )     /* Microsoft XBOX compiler detected  */
#ifndef TARGET_LITTLE_ENDIAN
#define TARGET_LITTLE_ENDIAN                        0
#endif  /* TARGET_LITTLE_ENDIAN */
#ifndef TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS
#define TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS    1
#endif  /* TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS */
#elif defined( _M_PPC )     /* Microsoft XBOX/PPC compiler detected  */
#ifndef TARGET_LITTLE_ENDIAN
#define TARGET_LITTLE_ENDIAN                        0
#endif  /* TARGET_LITTLE_ENDIAN */
#ifndef TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS
#define TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS    1
#endif  /* TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS */
#elif defined( ARM )
#ifndef TARGET_LITTLE_ENDIAN
#define TARGET_LITTLE_ENDIAN                        1
#endif  /* TARGET_LITTLE_ENDIAN */
#ifndef TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS
#define TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS    0
#endif  /* TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS */
#elif defined( ARM64 )
#ifndef TARGET_LITTLE_ENDIAN
#define TARGET_LITTLE_ENDIAN                        1
#endif  /* TARGET_LITTLE_ENDIAN */
#ifndef TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS
#define TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS    0
#endif  /* TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS */
#elif defined( DRM_ARCH_POWERPC )
#ifndef TARGET_LITTLE_ENDIAN
#define TARGET_LITTLE_ENDIAN                        0
#endif  /* TARGET_LITTLE_ENDIAN */
#ifndef TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS
#define TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS    1
#endif  /* TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS */
/*
** Default to little endian for GCC if TARGET_LITTLE_ENDIAN was not set
*/
#elif defined( DRM_GNUC_MAJOR )
#ifndef TARGET_LITTLE_ENDIAN
#define TARGET_LITTLE_ENDIAN                        1
#endif  /* TARGET_LITTLE_ENDIAN */
#ifndef TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS
#define TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS    0
#endif  /* TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS */
#else  /* unknown target */
#error Unknown target - please refer to oemcompiler.h near TARGET_LITTLE_ENDIAN
#endif /* determining target */
#endif  /* !defined(TARGET_LITTLE_ENDIAN) || !defined(TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS) */

/*
** +-----------------------------+
** | BASIC TYPE/SIZE DEFINITIONS |
** +-----------------------------+
*/

#if !DRM_BASIC_TYPE_SIZES_DEFINED
#undef DRM_BASIC_TYPE_SIZES_DEFINED
#define DRM_BASIC_TYPE_SIZES_DEFINED 1

/*
** In General,
** DRM_CHAR* is used for null terminated ANSI strings
** DRM_BYTE* is used for just plain byte buffer, w/o any termination
*/

/*
** Wide character is special type in C++ - wchar_t,
** while for "C" compilation it is just unsigned short.
** So 2 different types for C and C++.
*/

/* ======================== Microsoft compiler sizes ========================== */
/* ============================================================================ */
#if defined(DRM_MSC_VER)
                                                        /* Size */
typedef unsigned __int8         DRM_BYTE;               /* 1 byte  */
typedef __int8                  DRM_CHAR;               /* 1 byte  */
typedef unsigned __int16        DRM_WORD;               /* 2 bytes */
typedef unsigned __int16        DRM_WCHAR;              /* 2 bytes */
typedef __int32                 DRM_BOOL;               /* 4 bytes */
typedef unsigned long           DRM_DWORD;              /* 4 bytes */
typedef unsigned __int32        DRM_WCHAR32;            /* 4 bytes */
typedef long                    DRM_LONG;               /* 4 bytes */
typedef unsigned __int16        DRM_WCHAR_NATIVE;       /* 2 bytes */

/* =========================== GNU compiler sizes ============================= */
/* ============================================================================ */
#elif defined(DRM_GNUC_MAJOR)

// LWE (nkuo) - below headers are not supported in falcon-gcc. So I'm directly copying the required typedef below.
//#include <stdint.h>
//#include <wchar.h>

/* Signed.  */
typedef signed char             int8_t;
typedef short int               int16_t;
typedef int                     int32_t;
typedef long long int           int64_t;

/* Unsigned.  */
typedef unsigned char           uint8_t;
typedef unsigned short int      uint16_t;
typedef unsigned int            uint32_t;
typedef unsigned long long int  uint64_t;
                                                    /* Size */
typedef uint8_t             DRM_BYTE;               /* 1 byte  */
typedef char                DRM_CHAR;               /* 1 byte  */
typedef uint16_t            DRM_WORD;               /* 2 bytes */

/*
** DRM_WCHAR needs to be a 2 bytes in size.  Opt for wchar_t if it is the right size,
** otherwise use uint16_t
*/
#if __WCHAR_MAX__ == 0xFFFF
typedef wchar_t             DRM_WCHAR;              /* 2 bytes */
#else  /* __WCHAR_MAX__ == 0xFFFF */
typedef uint16_t            DRM_WCHAR;              /* 2 bytes */
#endif /* __WCHAR_MAX__ == 0xFFFF */

typedef int32_t             DRM_BOOL;               /* 4 bytes */
typedef uint32_t            DRM_DWORD;              /* 4 bytes */
typedef uint32_t            DRM_WCHAR32;            /* 4 bytes */

#if DRM_64BIT_TARGET
typedef int32_t             DRM_LONG;               /* 4 bytes */
#else  /* DRM_64BIT_TARGET */
typedef long                DRM_LONG;               /* 4 bytes */
#endif /* DRM_64BIT_TARGET */

#ifdef __cplusplus
typedef wchar_t             DRM_WCHAR_NATIVE;       /* 2 bytes  */
#else  /* __cplusplus */
typedef uint16_t            DRM_WCHAR_NATIVE;       /* 2 bytes  */
#endif /* __cplusplus */

#define  vsprintf_s    vsnprintf

/* ============================== Default sizes =============================== */
/* ============================================================================ */
#else /* Neither DRM_MSC_VER nor DRM_GNUC_MAJOR is defined */
                                                        /* Size */
typedef unsigned char           DRM_BYTE;               /* 1 byte  */
typedef char                    DRM_CHAR;               /* 1 byte  */
typedef unsigned short          DRM_WORD;               /* 2 bytes */
typedef unsigned short          DRM_WCHAR;              /* 2 bytes */
typedef int                     DRM_BOOL;               /* 4 bytes */
typedef unsigned long           DRM_DWORD;              /* 4 bytes */
typedef unsigned int            DRM_WCHAR32;            /* 4 bytes */
typedef long                    DRM_LONG;               /* 4 bytes */

#ifdef __cplusplus
typedef wchar_t                 DRM_WCHAR_NATIVE;       /* 2 bytes */
#else  /* __cplusplus */
typedef unsigned short          DRM_WCHAR_NATIVE;       /* 2 bytes */
#endif /* __cplusplus */

#endif /* DRM_MSC_VER, DRM_GNUC_MAJOR, or neither is defined */

#endif /* !DRM_BASIC_TYPE_SIZES_DEFINED */

/*
** +-----------------------------------+
** | MACRO DEFINITIONS                 |
** +-----------------------------------+
*/

#define DRM_OFFSET_OF(struc,member)   (DRM_SIZE_T)&(((struc *)0)->member)

/*
** DRM_SIZEOF_MEMBER uses the DRM_OFFSET_OF trick to reference a member of a struct for use in sizeof because sizeof(struct foo.bar) doesn't "just work"
** without an explicit instance of foo
*/
#define DRM_SIZEOF_MEMBER(struc,member) sizeof((((struc *)0)->member))
#define DRM_SIZEOF_MEMBER_DEREF(struc,member) sizeof(*((((struc *)0)->member)))


/* ====================== Microsoft specific qualifiers ======================= */
/* ============================================================================ */
#if defined(DRM_MSC_VER)

/*
** Compile-time asserts cause PREfast warnings regarding the comparison of two constants.
** So, enable this macro only when the PREfast tool is not analyzing the code.
*/
#if !defined(_PREFAST_)

    /* Definition of the compile time assert. */
    #define DRMCASSERT( x ) switch(0){case 0:case (x):;}

    /* Assertion of the alignment of a member field within a structure. */
    #define DRMALIGNASSERT( struc, member ) DRMCASSERT( ( DRM_OFFSET_OF( struc, member ) ) % sizeof( DRM_WCHAR ) == 0 )

    /* Assertion of adjacency of two member fields within the same structure. */
    #define DRMADJASSERT( struc, member1, member2 ) DRMCASSERT( ENSURE_MEMBERS_ADJACENT( struc, member1, member2 ) )

#else  /* _PREFAST_ */
    /* Definition of the compile time assert. */
    #define DRMCASSERT( x )

    /* Assertion of the alignment of a member field within a structure. */
    #define DRMALIGNASSERT( struc, member ) DRMASSERT( ( DRM_OFFSET_OF( struc, member ) ) % sizeof( DRM_WCHAR ) == 0 )

    /* Assertion of adjacency of two member fields within the same structure. */
    #define DRMADJASSERT( struc, member1, member2 ) DRMASSERT( ENSURE_MEMBERS_ADJACENT( struc, member1, member2 ) )

#endif /* _PREFAST_ */

#else  /* DRM_MSC_VER */

/* Definition of the compile time assert. */
#define DRMCASSERT( x ) switch(0){case 0:case (x):;}

/* Assertion of the alignment of a member field within a structure. */
#define DRMALIGNASSERT( struc, member ) DRMASSERT( ( DRM_OFFSET_OF( struc, member ) ) % sizeof( DRM_WCHAR ) == 0 )

/* Assertion of adjacency of two member fields within the same structure. */
#define DRMADJASSERT( struc, member1, member2 ) DRMASSERT( ENSURE_MEMBERS_ADJACENT( struc, member1, member2 ) )

#endif /* DRM_MSC_VER */

/*
** This option enables specific struct alignment on some Microsoft internal builds.
** Should not be used.
*/
#define DRM_OBFUS_FIXED_ALIGN
#if !defined(DRM_USE_OBFUS_STRUCT_ALIGN)

#define DRM_OBFUS_FILL_BYTES(x)
#define DRM_OBFUS_PTR_TOP
#define DRM_OBFUS_PTR_BTM
#define DRM_OBFUS_FIXED_ALIGN
#define DRM_OBFUS_INIT_PTR_TOP
#define DRM_OBFUS_INIT_PTR_BTM
#define DRM_OBFUS_INIT_FILL
#define DRM_OBFUS_PTR_WRAP_SIZE 0

#endif /* DRM_USE_OBFUS_STRUCT_ALIGN */

// LWE(kwilson)  set this way in PR3.0
#if (1)//DRM_DBG
#define DRM_FRE_INLINE              DRM_NO_INLINE
//#define DRM_FRE_INLINE_ATTRIBUTE    DRM_NO_INLINE_ATTRIBUTE
#else  /* DRM_DBG */
#define DRM_FRE_INLINE              DRM_ALWAYS_INLINE
#define DRM_FRE_INLINE_ATTRIBUTE    DRM_ALWAYS_INLINE_ATTRIBUTE
#endif /* DRM_DBG */

// LWE(kwilson)  set this way in PR3.0
#if DRM_INLINING_SUPPORTED
//#define DRM_INLINING_MATHSAFE_SUPPORTED 1
#else
//#define DRM_INLINING_MATHSAFE_SUPPORTED 0

#endif /* DRM_INLINING_SUPPORTED */

#define DRM_GLOBAL_CONST const

#if !defined(DRM_COMPILE_FOR_NORMAL_WORLD) && !defined(DRM_COMPILE_FOR_SELWRE_WORLD)
#error Both DRM_COMPILE_FOR_NORMAL_WORLD and DRM_COMPILE_FOR_SELWRE_WORLD are defined.
#elif DRM_COMPILE_FOR_NORMAL_WORLD && DRM_COMPILE_FOR_SELWRE_WORLD
#error Neither DRM_COMPILE_FOR_NORMAL_WORLD nor DRM_COMPILE_FOR_SELWRE_WORLD are non-zero.
#endif

#if defined(DRM_GNUC_MAJOR)

/* If using the GNU compiler use __builtin_expect to improve branch predictions */
#define DRM_LIKELY( expr ) __builtin_expect(!!(expr),1)
#define DRM_UNLIKELY( expr ) __builtin_expect(!!(expr),0)

#else  /* DRM_GNUC_MAJOR */

#define DRM_LIKELY( expr )    (expr)
#define DRM_UNLIKELY( expr )  (expr)

#endif  /* DRM_GNUC_MAJOR */

/*
** +-------------------------+
** | 64-BIT TYPE DEFINITIONS |
** +-------------------------+
*/
#if DRM_SUPPORT_NATIVE_64BIT_TYPES && !DRM_NATIVE_64BIT_TYPES_DEFINED
#undef DRM_NATIVE_64BIT_TYPES_DEFINED
#define DRM_NATIVE_64BIT_TYPES_DEFINED 1
#if defined(DRM_MSC_VER)
typedef          __int64 DRM_INT64;
typedef unsigned __int64 DRM_UINT64;
#elif DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_MAC
typedef          int64_t       DRM_INT64;
typedef          uint64_t      DRM_UINT64;
#elif defined(DRM_GNUC_MAJOR)
typedef          int64_t       DRM_INT64;
// LWE (kwilson) Make aligned
typedef          uint64_t __attribute__((aligned(8)))     DRM_UINT64;
#else
#error Unknown compiler - you must typedef DRM_INT64 and DRM_UINT64 appropriately OR set DRM_SUPPORT_NATIVE_64BIT_TYPES=0
#endif /* defined(DRM_MSC_VER) */
#endif /* DRM_SUPPORT_NATIVE_64BIT_TYPES && !defined(DRM_NATIVE_64BIT_TYPES_DEFINED) */

#endif   /* __DRMCOMPILER_H__ */

