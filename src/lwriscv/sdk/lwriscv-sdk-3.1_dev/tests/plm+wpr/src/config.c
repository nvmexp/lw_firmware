/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file        config.h
 * @brief       Test configuration settings.
 *
 * @note        Engine-specific configurations are located in engine.h.
 */

// Standard includes.
#include <stdbool.h>

// LIBLWRISCV includes.
#include <liblwriscv/fbif.h>

// Local includes.
#include "config.h"
#include "engine.h"

// Conditional includes.
#if ENGINE_TEST_GDMA
#include <liblwriscv/gdma.h>
#endif // ENGINE_TEST_GDMA


// Aperture settings to use when accessing test targets via DMA.
const FBIF_APERTURE_CFG g_configAccessDmaAperture =
{
    // Configure index.
    .apertureIdx = CONFIG_ACCESS_DMA_INDEX,

    // Only supported target for DMA right now is WPR (subsection of FB).
    .target = FBIF_TRANSCFG_TARGET_LOCAL_FB,

    // Use physical addressing.
    .bTargetVa = false,

    // Default L2-eviction class.
    .l2cWr = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL,
    .l2cRd = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL,

    // No address checks.
    .fbifTranscfWachk0Enable = false,
    .fbifTranscfWachk1Enable = false,
    .fbifTranscfRachk0Enable = false,
    .fbifTranscfRachk1Enable = false,

    // Not applicable.
    .bEngineIdFlagOwn = false,

    // Will be configured separately as needed.
    .regionid = 0,
};

///////////////////////////////////////////////////////////////////////////////

#if ENGINE_TEST_GDMA
// Private channel settings to use when accessing test targets via GDMA.
const GDMA_CHANNEL_PRIV_CFG g_configAccessGdmaChannelPrivate =
{
    // Use physical addressing.
    .bVirtual = false,

    // Not applicable.
    .asid = 0,
};

// Channel settings to use when accessing test targets via GDMA.
const GDMA_CHANNEL_CFG g_configAccessGdmaChannel = 
{
    // Give this channel highest priority.
    .rrWeight = 16,

    // Unlimited in-flight transaction size.
    .maxInFlight = 0,

    // Disable IRQ at the channel level.
    .bIrqEn = false,

    // Use register-mode programming.
    .bMemq = false,
};
#endif // ENGINE_TEST_GDMA
