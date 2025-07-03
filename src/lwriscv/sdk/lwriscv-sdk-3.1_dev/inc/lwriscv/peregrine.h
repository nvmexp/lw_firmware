/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_PEREGRINE_H
#define LWRISCV_PEREGRINE_H

/*!
 * @file    engine.h
 * @brief   This file hides header differences between different Peregrines.
 */

// Register headers
#if (__riscv_xlen == 32)
    #include LWRISCV32_MANUAL_LOCAL_IO
#else
    #include LWRISCV64_MANUAL_LOCAL_IO
#endif // (__riscv_xlen == 32)

#if LWRISCV_IS_ENGINE_gsp
    #if (LWRISCV_HAS_PRI || LWRISCV_HAS_CSB_OVER_PRI)
        #include <dev_falcon_v4.h>
        #include <dev_riscv_pri.h>
        #define FALCON_BASE   LW_FALCON_GSP_BASE
    #endif
    #if LWRISCV_HAS_PRI
        #define RISCV_BASE    LW_FALCON2_GSP_BASE
    #endif
#elif LWRISCV_IS_ENGINE_pmu
    #if (LWRISCV_HAS_PRI || LWRISCV_HAS_CSB_OVER_PRI)
        #include <dev_falcon_v4.h>
        #include <dev_riscv_pri.h>
        #define FALCON_BASE   LW_FALCON_PWR_BASE
    #endif
    #if LWRISCV_HAS_PRI
        #define RISCV_BASE    LW_FALCON2_PWR_BASE
    #endif
#elif LWRISCV_IS_ENGINE_minion
    #if (LWRISCV_HAS_PRI || LWRISCV_HAS_CSB_OVER_PRI)
        #include <dev_falcon_v4.h>
        #include <dev_riscv_pri.h>
        #define FALCON_BASE   LW_FALCON_MINION_BASE
    #endif
    #if LWRISCV_HAS_PRI
        #define LW_FALCON2_MINION_BASE 0xA06400
        #define RISCV_BASE    LW_FALCON2_MINION_BASE
    #endif
#elif LWRISCV_IS_ENGINE_sec
    #if (LWRISCV_HAS_PRI || LWRISCV_HAS_CSB_OVER_PRI)
        #include <dev_falcon_v4.h>
        #include <dev_riscv_pri.h>
        #define FALCON_BASE   0x840000
    #endif
    #if LWRISCV_HAS_PRI
        #define RISCV_BASE    LW_FALCON2_SEC_BASE
    #endif
#elif LWRISCV_IS_ENGINE_fsp
    #if (LWRISCV_HAS_PRI || LWRISCV_HAS_CSB_OVER_PRI)
        #include <dev_falcon_v4.h>
        #include <dev_riscv_pri.h>
        #define FALCON_BASE   0x8F0000
    #endif
    #if LWRISCV_HAS_PRI
        #define RISCV_BASE    LW_FALCON2_FSP_BASE
    #endif
#elif LWRISCV_IS_ENGINE_lwdec
    #if (LWRISCV_HAS_PRI || LWRISCV_HAS_CSB_OVER_PRI)
        #include <dev_falcon_v4.h>
        #include <dev_riscv_pri.h>
        // Use LWDEC0 base
        #define FALCON_BASE   LW_FALCON_LWDEC0_BASE
    #endif
    #if LWRISCV_HAS_PRI
        // Use LWDEC0 base
        #define RISCV_BASE    LW_FALCON2_LWDEC0_BASE
    #endif
#elif LWRISCV_IS_ENGINE_soe
    #if (LWRISCV_HAS_PRI || LWRISCV_HAS_CSB_OVER_PRI)
        #include <dev_falcon_v4.h>
        #include <dev_riscv_pri.h>
        #define FALCON_BASE   0x840000
    #endif
    #if LWRISCV_HAS_PRI
        #define RISCV_BASE    LW_FALCON2_SOE_BASE
    #endif
#elif LWRISCV_IS_ENGINE_pxuc
    #if (LWRISCV_HAS_PRI || LWRISCV_HAS_CSB_OVER_PRI)
        #include <dev_falcon_v4.h>
        #include <dev_riscv_pri.h>
        #define FALCON_BASE   0x828000
    #endif
    #if LWRISCV_HAS_PRI
        #define RISCV_BASE    (FALCON_BASE + LW_PPXUC_FALCON2_BASE)
    #endif
#endif

#endif // LWRISCV_PEREGRINE_H
