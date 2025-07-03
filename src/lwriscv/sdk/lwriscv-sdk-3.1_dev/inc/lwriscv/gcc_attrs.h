/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_GCC_ATTRS_H
#define LWRISCV_GCC_ATTRS_H

/*
 * Common attributes
 */
#define GCC_ATTR_ALIGNED(X)                        __attribute__((aligned((X))))
#define GCC_ATTR_SECTION(X)                        __attribute__((section((X))))
#define GCC_ATTR_UNUSED                            __attribute__((unused))
#define GCC_ATTR_USED                              __attribute__((used))
#define GCC_ATTR_VISIBILITY_HIDDEN                 __attribute__((visibility("hidden")))
#define GCC_ATTR_VISIBILITY_INTERNAL               __attribute__((visibility("internal")))
#define GCC_ATTR_VISIBILITY_PROTECTED              __attribute__((visibility("protected")))
#define GCC_ATTR_WEAK                              __attribute__((weak))

/*
 * Function only attributes
 */
#define GCC_ATTR_ALWAYSINLINE                      __attribute__((always_inline))
#define GCC_ATTR_FORMAT_ARG(Idx)                   __attribute__((format_arg((Idx))))
#define GCC_ATTR_FORMAT_PRINTF(Idx, firstToCheck)  __attribute__((format(printf, (Idx), (firstToCheck))))
#define GCC_ATTR_NAKED                             __attribute__((naked))
#define GCC_ATTR_NOINLINE                          __attribute__((noinline))
#define GCC_ATTR_NONNULL(...)                      __attribute__((nonnull(__VA_ARGS__)))
#define GCC_ATTR_NONNULL_ALL                       __attribute__((nonnull))
#define GCC_ATTR_NORETURN                          __attribute__((noreturn))
#define GCC_ATTR_PURE                              __attribute__((pure))
#define GCC_ATTR_WARN_UNUSED                       __attribute__((warn_unused_result))
#define GCC_ATTR_NO_SSP                            __attribute__((__optimize__("no-stack-protector")))

/*
 * Variable only attributes
 */
#define GCC_ATTR_CLEANUP(cleanupFcn)               __attribute__((cleanup((cleanupFcn))))
#define GCC_ATTR_PACKED                            __attribute__((packed))

/*
 * Misc attributes
 */
#define GCC_ATTR_FALLTHROUGH                       __attribute__((fallthrough))

#endif // LWRISCV_GCC_ATTRS_H
