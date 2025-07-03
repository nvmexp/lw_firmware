/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pgpsigk10x.c
 * @brief  PMU PSI related Hal functions for GK10X.
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pmgr.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib_mutex.h"
#include "pmu_objpg.h"
#include "pmu_objpmu.h"
#include "pmu_objpmgr.h"
#include "pmu_objtimer.h"

#include "dbgprintf.h"

#include "config/g_psi_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * @brief Maximum latency to flush GPIO
 */
#define GPIO_CNTL_TRIGGER_UDPATE_DONE_TIME_US               (50)

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_psiGpioTrigger_GMXXX(LwU8 ctrlId, LwU8 railId, LwU32 stepMask, LwBool bEnable)
                  GCC_ATTRIB_SECTION("imem_libPsi", "s_psiGpioTrigger_GMXXX");
static LwBool     s_psiIsLwrrentBelowThreshold_GMXXX(LwU8 ctrlId, LwU8 railId, LwU32 stepMask)
                  GCC_ATTRIB_SECTION("imem_libPsi", "s_psiIsLwrrentBelowThreshold_GMXXX");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Engage PSI
 *
 * Engage PSI only if PSI is not already engaged by some other power feature.
 *
 * NOTE: PSI entry sequence cant be paralleled with Power feature's entry
 *       sequence. This API is not designed for that.
 *
 * @param[in]   ctrlId          PSI Ctrl Id
 * @param[in]   stepMask        Mask of PSI_ENTRY_EXIT_STEP_*
 *
 * @return  FLCN_OK         PSI engaged successfully OR PSI was already
 *                          engaged
 */
FLCN_STATUS
psiEngagePsi_GMXXX
(
    LwU8   ctrlId,
    LwU32  railMask,
    LwU32  stepMask
)
{
    PSI_RAIL *pPsiRail;
    LwU8      railId;

    // For Pre Pascal GPUs we only support PSI for LWVDD Rail.
    PMU_HALT_COND((railMask & ~BIT(RM_PMU_PSI_RAIL_ID_LWVDD)) == 0);

    railId   = RM_PMU_PSI_RAIL_ID_LWVDD;
    pPsiRail = PSI_GET_RAIL(railId);

    // Return early if requested feature ID is invalid
    if (ctrlId == RM_PMU_PSI_CTRL_ID__ILWALID)
    {
        return FLCN_OK;
    }

    //
    // Return early if PSI is owned by other valid feature.
    // i.e.
    // LwrrentOwner != Requested Feature Owner AND
    // LwrrentOwner != invalid feature
    //
    if ((pPsiRail->lwrrentOwnerId != ctrlId)  &&
        (pPsiRail->lwrrentOwnerId != RM_PMU_PSI_CTRL_ID__ILWALID))
    {
        return FLCN_OK;
    }

    //
    // Return early if Sleep Current is not less than crossover current
    // for Current Aware PSI.
    //
    if (PSI_FLAVOUR_IS_ENABLED(LWRRENT_AWARE, railId) &&
        !s_psiIsLwrrentBelowThreshold_GMXXX(ctrlId, railId, stepMask))
    {
        return FLCN_OK;
    }

    // Take the ownership of PSI
    pPsiRail->lwrrentOwnerId = ctrlId;

    // Engage PSI
    if (FLCN_OK != s_psiGpioTrigger_GMXXX(ctrlId, railId, stepMask, LW_TRUE))
    {
        pPsiRail->lwrrentOwnerId = RM_PMU_PSI_CTRL_ID__ILWALID;
    }

    return FLCN_OK;
}

/*!
 * @brief Dis-engage PSI
 *
 * Disengage PSI only if PSI was engaged by requesting power feature. Wait for
 * settleTimeUs after dis-engaging PSI. The regulator needs some time to
 * complete the switch before we can proceed further.
 *
 * @param[in]   ctrlId          PSI Ctrl Id
 * @param[in]   stepMask        Mask of PSI_ENTRY_EXIT_STEP_*
 *
 * @return  FLCN_OK         PSI dis-engaged successfully if it was engaged by
 *                          requesting power feature engaged.
 */
FLCN_STATUS
psiDisengagePsi_GMXXX
(
    LwU8   ctrlId,
    LwU32  railMask,
    LwU32  stepMask
)
{
    LwU8    railId;

    // For Pre Pascal GPUs we only support PSI for LWVDD Rail.
    PMU_HALT_COND((railMask & ~BIT(RM_PMU_PSI_RAIL_ID_LWVDD)) == 0);

    railId = RM_PMU_PSI_RAIL_ID_LWVDD;

    //
    // Return early if requested ctrlId is invalid OR PSI is owned by some
    //  other power feature than ctrlId.
    //
    if ((ctrlId == RM_PMU_PSI_CTRL_ID__ILWALID) ||
        (PSI_GET_RAIL(railId)->lwrrentOwnerId != ctrlId))
    {
        return FLCN_OK;
    }

    if (FLCN_OK != s_psiGpioTrigger_GMXXX(ctrlId, railId, stepMask, LW_FALSE))
    {
        PMU_HALT();
    }

    // Update current PSI owner
    if (PSI_STEP_ENABLED(stepMask, RELEASE_OWNERSHIP))
    {
        PSI_GET_RAIL(railId)->lwrrentOwnerId = RM_PMU_PSI_CTRL_ID__ILWALID;
    }

    return FLCN_OK;
}

/*!
 * @brief Sequence to enable or disable LWVDD Power State Indicator (PSI)
 *
 * Function implements following sequence -
 * 1) Acquire GPIO mutex to avoid race condition between RM and PMU for updating
 *    GPIO pins.
 * 2) Update GPIO_6_OUTPUT_CNTL and wait until GPIO gets flushed.
 * 3) Release GPIO mutex.
 * 4) Wait for minimum pulse width low time on PSI entry.
 *    Wait for regulator settle time on PSI exit.
 *
 * @param[in]   ctrlId        PSI Ctrl Id
 * @param[in]   stepMask      Mask of PSI_ENTRY_EXIT_STEP_*
 * @param[in]   bEnable       Enable LWVDD PSI  if LW_TRUE
 *                            Disable LWVDD PSI if LW_FALSE
 */
static FLCN_STATUS
s_psiGpioTrigger_GMXXX
(
    LwU8   ctrlId,
    LwU8   railId,
    LwU32  stepMask,
    LwBool bEnable
)
{
    PSI_CTRL   *pPsiCtrl = PSI_GET_CTRL(ctrlId);
    PSI_RAIL   *pPsiRail = PSI_GET_RAIL(railId);
    FLCN_STATUS status   = FLCN_OK;
    LwU32       regVal;

    // Acquire the GPIO mutex. Return early if we failed to aquire mutex.
    if (PSI_STEP_ENABLED(stepMask, ACQUIRE_MUTEX))
    {
        status =  mutexAcquire(LW_MUTEX_ID_GPIO,
                               pPsiCtrl->mutexAcquireTimeoutUs*1000,
                               &(Psi.mutexTokenId));
        if (status != FLCN_OK)
        {
            return status;
        }
    }

    //
    // Engage/Disengage PSI
    //
    // GPIO6 is an active low signal. 0 means engage PSI and 1 means dis-engage
    // PSI
    //
    if (PSI_STEP_ENABLED(stepMask, WRITE_GPIO))
    {
        regVal = REG_RD32(FECS, LW_PMGR_GPIO_OUTPUT_CNTL(pPsiRail->gpioPin));

        if (bEnable)
        {
            //
            // For AutpPSI, enable PSI is equivalent to putting the
            // GPIO pin into tristate
            //
            if (PSI_FLAVOUR_IS_ENABLED(AUTO, railId))
            {
                regVal = FLD_SET_DRF(_PMGR, _GPIO_OUTPUT_CNTL,
                             _IO_OUT_EN, _NO, regVal);
            }
            else
            {
                regVal = FLD_SET_DRF(_PMGR, _GPIO_OUTPUT_CNTL,
                             _IO_OUTPUT, _0, regVal);
            }

            // Increment Engage count
            psiCtrlEngageCountIncrease(ctrlId, railId);
        }
        else
        {
            regVal = FLD_SET_DRF(_PMGR, _GPIO_OUTPUT_CNTL, _IO_OUTPUT, _1, regVal);

            //
            // For Auto PSI, disable PSI should pull out the GPIO from
            // tristate, and drive a 1 to the pin.
            //
            if (PSI_FLAVOUR_IS_ENABLED(AUTO, railId))
            {
                regVal = FLD_SET_DRF(_PMGR, _GPIO_OUTPUT_CNTL,
                                     _IO_OUT_EN, _YES, regVal);
            }
        }

        REG_WR32(FECS, LW_PMGR_GPIO_OUTPUT_CNTL(pPsiRail->gpioPin), regVal);

        //
        // Once _GPIO_OUTPUT_CNTL[i] is updated with the right value, the trigger
        // is done by write to a separate register.
        //
        REG_FLD_WR_DRF_DEF(FECS, _PMGR, _GPIO_OUTPUT_CNTL_TRIGGER, _UPDATE, _TRIGGER);
    }

    // Poll for roughly 50us for the trigger to get done.
    if (PSI_STEP_ENABLED(stepMask, WAIT_FOR_GPIO_FLUSH))
    {
        if (!PMU_REG32_POLL_US(
             USE_FECS(LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER),
             DRF_SHIFTMASK(LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE),
             LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_DONE,
             GPIO_CNTL_TRIGGER_UDPATE_DONE_TIME_US,
             PMU_REG_POLL_MODE_BAR0_EQ))
        {
            PMU_HALT();
        }
    }

    // Release the GPIO mutex.
    if (PSI_STEP_ENABLED(stepMask, RELEASE_MUTEX))
    {
        mutexRelease(LW_MUTEX_ID_GPIO, Psi.mutexTokenId);
    }

    //
    // After PSI got "engaged", wait for minimum time for which power regulator
    // should be in PSI. In general, this time is "Zero" or equal to regulator
    // settle time.
    //
    // Wait for settleTimeUs after "dis-engaging" PSI. The regulator needs some
    // time to complete the switch before we can proceed further.
    //

    if (PSI_STEP_ENABLED(stepMask, SETTLE_TIME) &&
        pPsiRail->settleTimeUs > 0)
    {
        OS_PTIMER_SPIN_WAIT_US(pPsiRail->settleTimeUs);
    }

    return status;
}

/*!
 * @brief Check Sleep Current for Current Aware PSI
 *
 * @return  LW_TRUE     Isleep less than Icross
 *          LW_FALSE    Otherwise
 */
static LwBool
s_psiIsLwrrentBelowThreshold_GMXXX
(
    LwU8   ctrlId,
    LwU8   railId,
    LwU32  stepMask
)
{
    PSI_CTRL *pPsiCtrl;

    if (PSI_FLAVOUR_IS_ENABLED(LWRRENT_AWARE, railId) &&
        PSI_STEP_ENABLED(stepMask, CHECK_LWRRENT))
    {
        pPsiCtrl = PSI_GET_CTRL(ctrlId);

        //
        // Sleep current should be less than crossover current to
        // engage current aware PSI for all voltage rails.
        // Check for Logic/sram rail below.
        //
        // i.e. Isleep < iCrossmA
        //
        // Till Maxwell, we only support LWVDD Rail PSI.
        //
        if (pPsiCtrl->pRailStat[railId]->iSleepmA >=
            PSI_GET_RAIL(railId)->iCrossmA)
        {
            return LW_FALSE;
        }
    }

    return LW_TRUE;
}
