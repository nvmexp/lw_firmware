/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PLM_WPR_CONFIG_H
#define PLM_WPR_CONFIG_H

/*!
 * @file        config.h
 * @brief       Test configuration settings.
 *
 * @note        Engine-specific configurations are located in engine.h.
 */

// Manual includes.
#include <dev_gc6_island.h>

// SDK includes.
#include <lwmisc.h>

// LWRISCV includes.
#include <lwriscv/status.h>

// LIBLWRISCV includes.
#include <liblwriscv/dma.h>
#include <liblwriscv/fbif.h>

// Local includes.
#include "engine.h"
#include "fetch.h"
#include "util.h"

// Conditional includes.
#if ENGINE_TEST_GDMA
#include <liblwriscv/gdma.h>
#endif // ENGINE_TEST_GDMA

///////////////////////////////////////////////////////////////////////////////

// Index of aperture to use when accessing test targets via DMA.
#define CONFIG_ACCESS_DMA_INDEX 0U

// Expected error code for negative DMA attempts (to differentiate failures).
#define CONFIG_ACCESS_DMA_NACK LWRV_ERR_DMA_NACK

// Aperture settings to use when accessing test targets via DMA.
extern const FBIF_APERTURE_CFG g_configAccessDmaAperture;

///////////////////////////////////////////////////////////////////////////////

#if ENGINE_TEST_GDMA

// Index of (sub)channel to use when accessing test targets via GDMA.
#define CONFIG_ACCESS_GDMA_CHANNEL      0U
#define CONFIG_ACCESS_GDMA_SUBCHANNEL   0U

// Expected error code for negative GDMA attempts (to differentiate failures).
#define CONFIG_ACCESS_GDMA_NACK LWRV_ERR_GENERIC

// Channel settings to use when accessing test targets via GDMA.
extern const GDMA_CHANNEL_PRIV_CFG g_configAccessGdmaChannelPrivate;
extern const GDMA_CHANNEL_CFG      g_configAccessGdmaChannel;

#endif // ENGINE_TEST_GDMA

///////////////////////////////////////////////////////////////////////////////

// Offsets for supported test targets.
#define CONFIG_TARGET_OFFSET_CSB UTIL_JOIN(LW_C, ENGINE_PREFIX, _QUEUE_HEAD(0))
#define CONFIG_TARGET_OFFSET_PRI LW_PGC6_BSI_SELWRE_SCRATCH_1
#define CONFIG_TARGET_OFFSET_WPR 0x00001000U

// Offsets for priv-level masks of supported register-type test targets.
#define CONFIG_TARGET_PLM_CSB UTIL_JOIN(LW_C, ENGINE_PREFIX, _QUEUE_HEAD__PRIV_LEVEL_MASK(0))
#define CONFIG_TARGET_PLM_PRI LW_PGC6_BSI_SELWRE_SCRATCH_1__PRIV_LEVEL_MASK

//
// 32-bit sentinel values for testing access to the test targets. Must all be
// unique and nonzero.
//
#define CONFIG_TARGET_SENTINEL_PRI_LOAD  0x12345678U
#define CONFIG_TARGET_SENTINEL_PRI_GDMA  0x52431AFLW    // Must not equal CONFIG_ACCESS_GDMA_NACK.
#define CONFIG_TARGET_SENTINEL_CSB_LOAD  0xAABBCCDDU
#define CONFIG_TARGET_SENTINEL_WPR_LOAD  0xF00DFACEU
#define CONFIG_TARGET_SENTINEL_WPR_FETCH FETCH_RET_CODE // From fetch.h.
#define CONFIG_TARGET_SENTINEL_WPR_DMA   0x19283765U    // Must not equal CONFIG_ACCESS_DMA_NACK.
#define CONFIG_TARGET_SENTINEL_WPR_GDMA  0x917EF18EU    // Must not equal CONFIG_ACCESS_GDMA_NACK.
#define CONFIG_TARGET_SENTINEL_ALL_CLEAR 0xACACACALW

// Sizes for supported scalable test targets.
#define CONFIG_TARGET_SIZE_WPR 0x00001000U

///////////////////////////////////////////////////////////////////////////////

// The number of MPU entries needed by the test environment.
#define CONFIG_TEST_MPU_COUNT 1U

// The index of the test environment's MPU entry (zero to avoid overriding).
#define CONFIG_TEST_MPU_INDEX 0U

///////////////////////////////////////////////////////////////////////////////

// Verify WPR base address meets fetch requirements.
_Static_assert(
    LW_IS_ALIGNED64(CONFIG_TARGET_OFFSET_WPR, 4U),
    "WPR base does not meet minimum RISC-V instruction-alignment requirements!"
);

// Verify WPR base address meets DMA requirements.
_Static_assert(
    LW_IS_ALIGNED64(CONFIG_TARGET_OFFSET_WPR, DMA_BLOCK_SIZE_MIN),
    "WPR base does not meet minimum DMA alignment requirements!"
);

#if ENGINE_TEST_GDMA
// Verify WPR base address meets GDMA requirements.
_Static_assert(
    LW_IS_ALIGNED64(CONFIG_TARGET_OFFSET_WPR, GDMA_ADDR_ALIGN),
    "WPR base does not meet minimum GDMA alignment requirements!"
);
#endif // ENGINE_TEST_GDMA

// Verify WPR size is sufficient to hold sentinel values.
_Static_assert(
    CONFIG_TARGET_SIZE_WPR >= sizeof(uint32_t),
    "WPR size is too small to fit sentinel values!"
);

///////////////////////////////////////////////////////////////////////////////

//
// Verify liblwriscv configuration.
//
_Static_assert(LWRISCV_CONFIG_CPU_MODE == 3,
    "Tests require M-mode privileges in order to function!"
);
_Static_assert(LWRISCV_CONFIG_DEBUG_PRINT_LEVEL > 0,
    "Print level must be nonzero in order for test results to be reported!"
);
_Static_assert(LWRISCV_FEATURE_DMA,
    "(G)DMA tests are enabled but liblwriscv DMA driver is missing!"
);
_Static_assert(LWRISCV_HAS_ARCH_rv64imf,
    "Unexpected architecture - compressed ISA must be disabled!"
);
_Static_assert(LWRISCV_WAR_FBIF_IS_DEAD,
    "Warning: disbaling WAR_FBIF_IS_DEAD can lead to unexpected hangs!"
);

#endif // PLM_WPR_CONFIG_H
