/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PLM_WPR_ENGINE_H
#define PLM_WPR_ENGINE_H

/*!
 * @file        engine.h
 * @brief       Engine-specific configuration settings and checks.
 */

// Standard includes.
#include <stdbool.h>


//
// Manual per-engine settings:
//   - Engine-specific CSB manual.
//   - Engine-specific PRI manual.
//   - Engine prefix to use for register names.
//   - Whether engine supports NACK_AS_ACK for FBIF.
//   - Whether to test CSB accesses for this engine.
//   - Whether to test GDMA accesses for this engine.
//
#if LWRISCV_IS_ENGINE_fsp

    #include <dev_fsp_csb.h>
    #include <dev_fsp_pri.h>
    
    #define ENGINE_PREFIX FSP

    #define ENGINE_HAS_NACK_AS_ACK true
    #define ENGINE_TEST_CSB true
    #define ENGINE_TEST_GDMA true

#elif LWRISCV_IS_ENGINE_gsp

    #include <dev_gsp_csb.h>
    #include <dev_gsp.h>

    #define ENGINE_PREFIX GSP

    #define ENGINE_HAS_NACK_AS_ACK true
    #define ENGINE_TEST_CSB true
    #define ENGINE_TEST_GDMA true

#elif LWRISCV_IS_ENGINE_lwdec

    #include <dev_lwdec_csb.h>
    #include <dev_lwdec_pri.h>

    #define ENGINE_PREFIX LWDEC0

    #define ENGINE_HAS_NACK_AS_ACK false
    #define ENGINE_TEST_CSB false  // No applicable CSB registers to test.
    #define ENGINE_TEST_GDMA false // No GDMA on LWDEC.

#elif LWRISCV_IS_ENGINE_sec

    #include <dev_sec_csb.h>
    #include <dev_sec_pri.h>

    #define ENGINE_PREFIX SEC

    #define ENGINE_HAS_NACK_AS_ACK true
    #define ENGINE_TEST_CSB true
    #define ENGINE_TEST_GDMA true

#else // LWRISCV_IS_ENGINE
    
    #error "Unrecognized engine. Did you forget to update engine.h?"

#endif // LWRISCV_IS_ENGINE

///////////////////////////////////////////////////////////////////////////////

//
// Manual configuration sanity-checks.
//
_Static_assert(LWRISCV_HAS_PRI,
    "PRI and/or WPR tests are enabled but target engine lacks PRI bus!");

_Static_assert(LWRISCV_HAS_CSB_MMIO || !ENGINE_TEST_CSB,
    "CSB tests are enabled but target engine lacks CSB bus!");

_Static_assert(LWRISCV_HAS_FBIF,
    "WPR tests are enabled but target engine lacks FBIF support!");

_Static_assert(LWRISCV_HAS_FBDMA,
    "DMA tests are enabled but target engine lacks DMA support!");

_Static_assert(LWRISCV_HAS_GDMA || !ENGINE_TEST_GDMA,
    "GDMA tests are enabled but target engine lacks GDMA support!");

_Static_assert(LWRISCV_HAS_S_MODE,
    "S-mode tests are enabled but target engine lacks S-mode!");

///////////////////////////////////////////////////////////////////////////////

//
// Miscellaneous sanity-checks.
//
_Static_assert(LWRISCV_HAS_LOCAL_IO,
    "Tests require local-I/O support in order to function!");

#endif // PLM_WPR_ENGINE_H
