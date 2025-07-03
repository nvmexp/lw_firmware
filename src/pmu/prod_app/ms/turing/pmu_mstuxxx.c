/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_mstuxxx.c
 * @brief  HAL-interface for the TU10X Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pri_ringstation_sys.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objfifo.h"
#include "pmu_objic.h"

#include "config/g_ms_private.h"

/*!
 * @brief Toggle Priv Ring Access
 *
 * Helper function to Toggle Priv Ring Access. This is used when Priv Ring access
 * needs to be enabled / disabled during Ungating / Gating Clocks From Bypass Path.
 *
 * @param[in]   bDisable    LW_FALSE => Restore Priv Ring Access
 *                          LW_TRUE  => Disable Priv Ring Access
 *
 */
void
msDisablePrivAccess_TU10X (LwBool bDisable)
{
    LwU32 val = 0;

    if (!bDisable)
    {
        // Restore FECS access to the priv_ring.
        REG_WR32(FECS, LW_PPRIV_SYS_PRI_GLOBAL_CONFIG_LOWPOWER,
                          Ms.pClkGatingRegs->priGlobalConfig);
        REG_WR32(FECS, LW_PPRIV_SYS_PRIV_DECODE_CONFIG,
                          Ms.pClkGatingRegs->privDecodeConfig);
    }
    else
    {
        // Cache the value of decode config
        Ms.pClkGatingRegs->privDecodeConfig =
            REG_RD32(FECS, LW_PPRIV_SYS_PRIV_DECODE_CONFIG);

        // Cache the value of Global config.
        Ms.pClkGatingRegs->priGlobalConfig =
            REG_RD32(FECS, LW_PPRIV_SYS_PRI_GLOBAL_CONFIG_LOWPOWER);

        // Setup FECS to reject any access to the priv_ring.
        val = FLD_SET_DRF(_PPRIV, _SYS_PRIV_DECODE_CONFIG,
                          _RING, _ERROR_ON_RING_NOT_STARTED,
                          Ms.pClkGatingRegs->privDecodeConfig);
        REG_WR32(FECS, LW_PPRIV_SYS_PRIV_DECODE_CONFIG, val);

        //
        // As per the security review (Bug: 200346666), new register
        // has been added for MSCG to disable the access for Priv Ring
        // This new register now will generate a new error code as:
        // FECS_DEAD_RING_LOW_POWER - 0xBADF31YY once PRIV ring is disabled
        //
        val = FLD_SET_DRF(_PPRIV, _SYS_PRI_GLOBAL_CONFIG_LOWPOWER, _RING_ACCESS,
                          _DISABLED, Ms.pClkGatingRegs->priGlobalConfig);
        REG_WR32(FECS, LW_PPRIV_SYS_PRI_GLOBAL_CONFIG_LOWPOWER, val);
    }
}
