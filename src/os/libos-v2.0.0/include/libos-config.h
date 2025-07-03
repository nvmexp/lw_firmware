/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBOS_CONFIG_H_
#define LIBOS_CONFIG_H_

#include "lwriscv-version.h"

// Feature toggles are only applicable if a target platform is specified
#ifdef LWRISCV_VERSION
    // Separation Kernel
    #define LIBOS_CONFIG_LWRISCV_SEPKERN        (LWRISCV_VERSION >= LWRISCV_2_0)

    // Supervisor Mode
    #define LIBOS_CONFIG_RISCV_S_MODE           (LWRISCV_VERSION >= LWRISCV_2_0)

#endif // LWRISCV_VERSION

// Aperture Patching Hack
#if defined(LIBOS_EXOKERNEL_GA10X) || defined(LIBOS_EXOKERNEL_GH100)
    #define LIBOS_CONFIG_ENABLE_APERTURE_PATCHING_HACK  1
#else
    #define LIBOS_CONFIG_ENABLE_APERTURE_PATCHING_HACK  0
#endif

// Time Tick Resolution
#if defined(LIBOS_EXOKERNEL_TU10X) || defined(LIBOS_EXOKERNEL_GA100) || defined(LIBOS_EXOKERNEL_GA10X)
    // Each mtime tick is 2^5 = 32 nanoseconds
    #define LIBOS_CONFIG_TIME_TICK_RESOLUTION_LOG2  5
#elif defined(LIBOS_EXOKERNEL_GH100)
    // Each mtime tick is 2^0 = 1 nanosecond
    #define LIBOS_CONFIG_TIME_TICK_RESOLUTION_LOG2  0
#endif

// Support for LIBOS running inside a partition
#if defined(LIBOS_EXOKERNEL_GH100)
    #define LIBOS_CONFIG_PARTITION      1
#else
    #define LIBOS_CONFIG_PARTITION      0
#endif

// GDMA Support
#if defined(LIBOS_EXOKERNEL_GH100)
    #define LIBOS_CONFIG_GDMA_SUPPORT   1
#else
    #define LIBOS_CONFIG_GDMA_SUPPORT   0
#endif

// TCM Sizing
#define LIBOS_CONFIG_DTCM_SIZE      (64u * 1024u)
#if defined(LIBOS_EXOKERNEL_TU10X) || defined(LIBOS_EXOKERNEL_GA100)
    #define LIBOS_CONFIG_ITCM_SIZE  (64u * 1024u)
#elif defined(LIBOS_EXOKERNEL_GA10X)
    #define LIBOS_CONFIG_ITCM_SIZE  (88u * 1024u)
#elif defined(LIBOS_EXOKERNEL_GH100)
    #define LIBOS_CONFIG_ITCM_SIZE  (120u * 1024u)
#endif

// TCM Paging Support - disabled when running in a partition
#define LIBOS_CONFIG_TCM_PAGING     (!LIBOS_CONFIG_PARTITION)

// WPR ID - 0 indicates no WPR
#if defined(LIBOS_EXOKERNEL_GH100)
    #define LIBOS_CONFIG_WPR    2
#else
    #define LIBOS_CONFIG_WPR    0
#endif

#define LIBOS_CONFIG_TIME_TO_NS(mtime)  ((mtime) << LIBOS_CONFIG_TIME_TICK_RESOLUTION_LOG2)
#define LIBOS_CONFIG_NS_TO_TIME(ns)     ((ns) >> LIBOS_CONFIG_TIME_TICK_RESOLUTION_LOG2)

// Undefine these so includers are forced to use CONFIG and VERSION defines
#undef LIBOS_EXOKERNEL_TU10X
#undef LIBOS_EXOKERNEL_GA100
#undef LIBOS_EXOKERNEL_GA10X
#undef LIBOS_EXOKERNEL_GH100

#endif
