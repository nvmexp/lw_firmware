/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msswasrgmxxxonly.c
 * @brief  SW-ASR specific HAL-interface for the MAXWELL
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_trim.h"
#include "dev_pri_ringstation_fbp.h"
/* ------------------------- Application Includes --------------------------- */

#include "pmu_objms.h"
#include "pmu_swasr.h"
#include "pmu_objpmu.h"

#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
static void s_msSwAsrIssueFbPrivFlush_GM10X                      (void);

/* ------------------------ Public Functions ---------------------------------*/
/*!
 * @brief Disable the DLL (Bug 1388233)
 *
 * The DLL is sourced out of refmpll and if ref path is stopped; the DLL can
 * sometimes lose sync at some frequencies.
 */
void
msSwAsrDramDisableDll_GM10X(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32 val;

    pSwAsr->regs.refMpllDllCfg = REG_RD32(FECS, LW_PTRIM_FBPA_BCAST_DLL_CFG);
    if (FLD_TEST_DRF(_PTRIM_FBPA, _BCAST_DLL_CFG, _DLL_ENABLE, _YES, 
                     pSwAsr->regs.refMpllDllCfg))
    {
        val = FLD_SET_DRF(_PTRIM_FBPA, _BCAST_DLL_CFG, _DLL_ENABLE, _NO,
                          pSwAsr->regs.refMpllDllCfg);
        REG_WR32(FECS, LW_PTRIM_FBPA_BCAST_DLL_CFG, val);
        
        // wait for the previous fbp write to flush
        s_msSwAsrIssueFbPrivFlush_GM10X();
    }
}

/*!
 * @brief Enable the DLL back again if required (Bug 1388233)
 *
 * The DLL needs to be enabled when the ref path is ungated.
 */
void
msSwAsrDramEnableDll_GM10X(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    // 
    // Bug 1388233
    // Enable the DLL back again if required.
    // The DLL needs to be enabled when the ref path 
    // is ungated.
    //
    if (FLD_TEST_DRF(_PTRIM_FBPA, _BCAST_DLL_CFG, _DLL_ENABLE, _YES, 
                     pSwAsr->regs.refMpllDllCfg))
    {
        REG_WR32(FECS, LW_PTRIM_FBPA_BCAST_DLL_CFG, 
                 pSwAsr->regs.refMpllDllCfg);
    }
}

/* ------------------------ Private Functions --------------------------------*/
/*!
 * @brief Issue FB Priv Flush (Or Fence)
 *
 * A polling read to the FB priv FENCE that ensure all commands have been
 * flushed to the FBPA/FBIO registers.
 */
static void
s_msSwAsrIssueFbPrivFlush_GM10X(void)
{
    if (!PMU_REG32_POLL_NS(
         USE_FECS(LW_PPRIV_FBP_PRI_FENCE),
         DRF_SHIFTMASK(LW_PPRIV_FBP_PRI_FENCE_V),
         LW_PPRIV_FBP_PRI_FENCE_V_0,
         SWASR_PRIV_FENCE_TIMEOUT_NS,
         PMU_REG_POLL_MODE_BAR0_EQ))
    {
        DBG_PRINTF_PG(("TO: Fence\n", 0, 0, 0, 0));
        PMU_HALT();
    }
}
