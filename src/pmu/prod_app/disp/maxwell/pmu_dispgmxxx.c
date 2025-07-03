/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_dispgmxxx.c
 * @brief  HAL-interface for the GMxxx Display Engine.
 *
 * The following Display HAL routines service all GM10X chips, including chips
 * that do not have a Display engine. At this time, we do NOT stub out the
 * functions on displayless chips. It is therefore unsafe to call a specific
 * set of the following HALs on displayless chips. You must always refer to the
 * function dolwmenation of HAL to learn if it is safe to call on displayless
 * chips. In cases where the dolwmenation indicates it is unsafe to call, the
 * dolwmenation will provide additional information on how (and why) to avoid
 * the call. HALs that are unsafe to call on displayless chips will still
 * minimally debug-break if there is no Display engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_fuse.h"
#include "dev_disp.h"
#include "dev_pwr_csb.h"
#include "dev_gc6_island.h"
#include "pmu/pmumailboxid.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objdisp.h"
#include "pmu_objfuse.h"
#include "pmu_disp.h"
#include "pmu_didle.h"
#include "dbgprintf.h"
#include "pmu_objlsf.h"
#include "class/cl947d.h"
#include "lwos_dma.h"
#include "lwuproc.h"

#include "config/g_disp_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Prototypes-------------------------------------- */

static void  s_dispSetDbgMode_GM10X(LwU32 dispChnlNum, LwBool bSetDbgMode);
static void  s_dispPollForCoreMethodStateIdle_GM10X(void);

/* -------------------------  Global variables  -------------------------------*/
/*!
 * One-time Display-engine related HW/SW initialization.
 *
 * @param[in/out]  pBDispPresent   Pointer to the buffer storing the display
 *                                 present state
 *
 * @note This HAL is always safe to call.
 */
void
dispPreInit_GMXXX
(
    LwBool *pBDispPresent
)
{
    //
    // Determine if this chip has a Display engine. Do NOT access any PDISP
    // registers prior to this point. Any PDISP register accesses that follow
    // must be protected by checks that ensure the Display engine is present
    // (either checks with DISP_IS_PRESENT() macro or checks based on features
    // that will be disabled when there is no Display).
    //
    *pBDispPresent = FLD_TEST_DRF(_FUSE, _STATUS_OPT_DISPLAY, _DATA, _ENABLE,
                                  fuseRead(LW_FUSE_STATUS_OPT_DISPLAY));
}

/*!
 * @brief Enable/Disable RM_ONESHOT interrupts
 * Function enable/disables DWCF, Channel exception interrupts for PMU.
 */
LwBool
dispRmOneshotStateUpdate_GM10X(void)
{
    LwU32  stallLock;
    LwBool bLwrrentActive = LW_FALSE;
    LwBool bDeactivated   = LW_FALSE;
    DISP_RM_ONESHOT_CONTEXT* pRmOneshotCtx = DispContext.pRmOneshotCtx;

    //
    // Step1) Check current state of oneshot mode.
    //        Oneshot mode will be active if -
    //        a) RM allows oneshot mode        AND
    //        b) Stall Lock is set to ONESHOT
    //
    if (pRmOneshotCtx->bRmAllow)
    {
        stallLock = REG_RD32(FECS, LW_UDISP_DSI_CHN_ARMED_BASEADR(LW_PDISP_907D_CHN_CORE) +
                                   LW947D_HEAD_SET_STALL_LOCK(pRmOneshotCtx->head.idx));

        if (FLD_TEST_DRF(947D, _HEAD_SET_STALL_LOCK, _ENABLE, _TRUE,     stallLock) &&
            FLD_TEST_DRF(947D, _HEAD_SET_STALL_LOCK, _MODE,   _ONE_SHOT, stallLock))
        {
            bLwrrentActive = LW_TRUE;
        }
    }

    //
    // Step2) Update interrupt state if new state does not match with
    //        old state of oneshot mode
    //
    if (bLwrrentActive != pRmOneshotCtx->bActive)
    {
        // Step2.1) Enable/Disable DWCF interrupt for PMU
        dispProgramDwcfIntr_HAL(&Disp, bLwrrentActive);

        // Step2.2) Update state cache
        pRmOneshotCtx->bActive = bLwrrentActive;

        if (!bLwrrentActive)
        {
            bDeactivated                 = LW_TRUE;
            pRmOneshotCtx->bNisoMsActive = LW_FALSE;
        }
    }

    return bDeactivated;
}

/*!
 * Puts the given channel to debug mode/restores back to normal operation.
 *
 * @param[in] dispChnlNum  Display channel number.
 * @param[in] bSetDbgMode  Enable/disable debug mode.
 */
static void
s_dispSetDbgMode_GM10X
(
    LwU32  dispChnlNum,
    LwBool bSetDbgMode
)
{
    LwU32 debugCtlRegVal;

    // BREAKPOINT_COND
    if (!DISP_IS_PRESENT())
    {
        PMU_BREAKPOINT();
    }

    debugCtlRegVal = REG_RD32(FECS, LW_PDISP_DSI_DEBUG_CTL(dispChnlNum));

    if (bSetDbgMode)
    {
        // If not already enabled, enable it now.
        if (!FLD_TEST_DRF(_PDISP, _DSI_DEBUG_CTL, _MODE, _ENABLE, debugCtlRegVal))
        {
            debugCtlRegVal = FLD_SET_DRF(_PDISP, _DSI_DEBUG_CTL, _MODE, _ENABLE,
                                         debugCtlRegVal);
            REG_WR32(FECS, LW_PDISP_DSI_DEBUG_CTL(dispChnlNum), debugCtlRegVal);
        }
    }
    else
    {
        // If not already disabled, disable it now.
        if (FLD_TEST_DRF(_PDISP, _DSI_DEBUG_CTL, _MODE, _ENABLE, debugCtlRegVal))
        {
            debugCtlRegVal = FLD_SET_DRF(_PDISP, _DSI_DEBUG_CTL, _MODE, _DISABLE,
                                         debugCtlRegVal);
            REG_WR32(FECS, LW_PDISP_DSI_DEBUG_CTL(dispChnlNum), debugCtlRegVal);
        }
    }
}

/*!
 * Poll for core channel method state to go to idle.
 */
static void
s_dispPollForCoreMethodStateIdle_GM10X(void)
{
    // BREAKPOINT_COND
    if (!DISP_IS_PRESENT())
    {
        PMU_BREAKPOINT();
    }

    while (FLD_TEST_DRF(_PDISP, _CHNCTL_CORE, _STATUS_METHOD_EXEC,
                        _RUNNING, REG_RD32(FECS, LW_PDISP_CHNCTL_CORE(0))))
    {
        // NOP
    }
}

/*!
 * @brief   Check if the driver has been hacked to run VRR
 * Every 1Hz:
 * We check first that we are running DirectDrive but not GSync or LWSR
 * 1/ If LW907D_HEAD_SET_STALL_LOCK(head) has __ENABLE, _TRUE and
 *    _MODE, _ONE_SHOT
 *    then we are running some sort of VRR
 * 2/ If LW_PDISP_RG_RASTER_V_EXTEND_BACK_PORCH(head) has _SET_ENABLE_YES and
 *    is set to 2, then we are running GSync with a GSync monitor
 * 3/ If the framelock pin is configured and there is no raster lock we are
 *    running LWSR (and not SLI or Surround)
 * With that we can determine if we run DirectDrive without a GSync monitor
 * If so we set the STALL_LOCK_MODE to _CONTINUOUS
 *
 * @return void
 */
void
dispCheckForVrrHackBug1604175_GM10X()
{
    LwU8            headNum;
    LwU32           regValue;
    static LwBool   bCheckAgain = LW_TRUE;

    // For all heads...
    for (headNum = 0; headNum < Disp.numHeads; headNum++)
    {
        // ... Check that Stall Lock is set to enable and one shot mode
        regValue = REG_RD32(FECS, LW_UDISP_DSI_CHN_ARMED_BASEADR(LW_PDISP_907D_CHN_CORE) +
                            LW947D_HEAD_SET_STALL_LOCK(headNum));
        if (!FLD_TEST_DRF(947D, _HEAD_SET_STALL_LOCK, _ENABLE, _TRUE, regValue) ||
            !FLD_TEST_DRF(947D, _HEAD_SET_STALL_LOCK, _MODE, _ONE_SHOT,regValue))
        {
            //
            // We are not dointg one-shot mode.
            //
            continue;
        }

        // ... Check if we are in VRR SLI mode
        regValue = REG_RD32(FECS, LW_UDISP_DSI_CHN_ARMED_BASEADR(LW_PDISP_907D_CHN_CORE) +
                         LW947D_HEAD_SET_CONTROL(headNum));
        if (FLD_TEST_DRF(947D, _HEAD_SET_CONTROL, _MASTER_LOCK_MODE, _RASTER_LOCK, regValue))
        {
            //
            // We are in bridged VRR SLI
            //
            continue;
        }

        // Do not override regValue here, see below.

        //
        // ... From the register value read above (Stall Lock) check if it has a
        // lock pin configured. An LWSR panel will always have the lock pin being setup.
        //
        if (!FLD_TEST_DRF(947D, _HEAD_SET_STALL_LOCK, _LOCK_PIN, _UNSPECIFIED, regValue))
        {
            // We are doing LWSR
            continue;
        }

        //
        // ... Check if LW_PDISP_RG_RASTER_V_EXTEND_BACK_PORCH(head) has
        // _SET_ENABLE_YES and is set to 2
        //
        regValue = REG_RD32(FECS, LW_PDISP_RG_RASTER_V_EXTEND_BACK_PORCH(headNum));
        if (FLD_TEST_DRF(_PDISP, _RG_RASTER_V_EXTEND_BACK_PORCH, _SET_ENABLE, _YES, regValue) &&
            FLD_TEST_DRF_NUM(_PDISP, _RG_RASTER_V_EXTEND_BACK_PORCH, _EXTEND_HEIGHT, 2, regValue))
        {
            // We are doing Desktop GSync.
            continue;
        }

        // At this point we are doing Notebook DirectDrive.
        break;
    }

    if (headNum == Disp.numHeads)
    {
        // We did not find we are doing Notebook DirectDrive.
        bCheckAgain = LW_TRUE;
        return;
    }

    if (bCheckAgain)
    {
        //
        // We need to make sure we are not checking in the middle of a settings
        // For example at the time we check that we did set stall lock but not
        // yet the frame lock or back porch extension.
        // This would give a false result.
        // So check one more time on second later.
        //
        bCheckAgain = LW_FALSE;
        return;
    }

    //
    // This function is only run with GPUs that are not approved
    // for Notebook Direct Drive: we are hacked
    //

    // Enable core channel DSI debug mode
    s_dispSetDbgMode_GM10X(LW_PDISP_907D_CHN_CORE, LW_TRUE);

    // Wait for core channel to be idle
    s_dispPollForCoreMethodStateIdle_GM10X();

    //Disable Stall Lock
    regValue = 0;
    regValue = FLD_SET_DRF(947D, _HEAD_SET_STALL_LOCK, _ENABLE, _FALSE, regValue);
    regValue = FLD_SET_DRF(947D, _HEAD_SET_STALL_LOCK, _LOCK_PIN, _UNSPECIFIED, regValue);
    regValue = FLD_SET_DRF(947D, _HEAD_SET_STALL_LOCK, _UNSTALL_MODE, _LINE_LOCK, regValue);
    dispEnqMethodForChannel_HAL(&Disp, LW_PDISP_907D_CHN_CORE,
                                LW947D_HEAD_SET_STALL_LOCK(headNum), regValue);

    // Send update on core channel
    dispEnqMethodForChannel_HAL(&Disp,
                                LW_PDISP_907D_CHN_CORE,
                                LW947D_UPDATE,
                                0);

    // Disable core channel DSI debug mode
    s_dispSetDbgMode_GM10X(LW_PDISP_907D_CHN_CORE, LW_FALSE);

    // If other heads are doing VRR, they will be check subsequently
}

/*!
 * @brief Returns if notebook Gsync is supported on this GPU
 * @return LwBool LW_TRUE if the GPU supports Notebook DirectDrive, LW_FALSE otherwise
 */
LwBool
dispIsNotebookGsyncSupported_GM20X(void)
{
     if (Lsf.brssData.bIsValid &&
         FLD_TEST_DRF(_BRSS, _SEC_HULKDATA2_RM_FEATURE, _GSYNC, _ENABLE, Lsf.brssData.hulkData[2]))
     {
         // There is a valid HULK license
         return LW_TRUE;
     }

    //
    // Check if the GPU is in the approved list for Notebook directDrive
    // Obfuscated so it has less chance to appear in clear in binary.
    //
    return ((Pmu.gpuDevId * 2 - 3 == 0x2c2b) ||  // For DevId 0x1617
            (Pmu.gpuDevId * 3 - 4 == 0x4244) ||  // For DevId 0x1618
            (Pmu.gpuDevId * 4 - 5 == 0x585f) ||  // For DevId 0x1619
            (Pmu.gpuDevId * 5 - 6 == 0x6e7c) ||  // For DevId 0x161A
            (Pmu.gpuDevId * 6 - 7 == 0x8663));   // For DevId 0x1667
}
