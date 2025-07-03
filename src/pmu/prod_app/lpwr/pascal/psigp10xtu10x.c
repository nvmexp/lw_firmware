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
 * @file   psigp10xtu10x.c
 * @brief  PMU PSI related Hal functions for Pascal till Turing GPU.
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

#include "config/g_pg_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * @brief Maximum latency to flush GPIO
 */
#define GPIO_CNTL_TRIGGER_UDPATE_DONE_TIME_US               (50)
 /*!
 * @brief Sequence to enable or disable Power State Indicator (PSI) for supported rails
 *
 * Function implements following sequence -
 * 1) Acquire GPIO mutex to avoid race condition between RM and PMU for updating
 *    GPIO pins.
 * 2) Update GPIO_X_OUTPUT_CNTL and wait until GPIO gets flushed.
 * 3) Release GPIO mutex.
 * 4) Wait for minimum pulse width low time on PSI entry.
 *    Wait for regulator settle time on PSI exit.
 *
 * @param[in]   ctrlId        PSI Ctrl Id
 * @param[in]   stepMask      Mask of PSI_ENTRY_EXIT_STEP_*
 * @param[in]   bEnable       Enable  PSI if LW_TRUE
 *                            Disable PSI if LW_FALSE
 */
FLCN_STATUS
psiGpioTrigger_GP10X
(
    LwU8   ctrlId,
    LwU32  railMask,
    LwU32  stepMask,
    LwBool bEnable
)
{
    PSI_CTRL   *pPsiCtrl = PSI_GET_CTRL(ctrlId);
    PSI_RAIL   *pPsiRail = NULL;
    FLCN_STATUS status   = FLCN_OK;
    LwU32       regVal;
    LwU32       railId;
    LwU32       settleTimeUs = 0;

    //
    // TODO: In future,we are thinking of removing the stepMask from the PSI
    // sequence. So we should have a way to know if the GPIO mutex is already
    // acquired or not.
    // Therefore, we will implement that logic here so that we should be aware
    // of the mutex state. We will do this logic implementation in next CL.
    //
    // Same way we need to implement logic while we need to release the mutex.
    // That will also be taken care in next Cl.
    //

    // Acquire the GPIO mutex. Return early if we failed to aquire mutex.
    if (PSI_STEP_ENABLED(stepMask, ACQUIRE_MUTEX))
    {
        status = mutexAcquire(LW_MUTEX_ID_GPIO,
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
        FOR_EACH_INDEX_IN_MASK(32, railId, railMask)
        {
            pPsiRail = PSI_GET_RAIL(railId);

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
                regVal = FLD_SET_DRF(_PMGR, _GPIO_OUTPUT_CNTL,
                                     _IO_OUTPUT, _1, regVal);

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
        }
        FOR_EACH_INDEX_IN_MASK_END;

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

    if (PSI_STEP_ENABLED(stepMask, SETTLE_TIME))
    {
        FOR_EACH_INDEX_IN_MASK(32, railId, railMask)
        {
            settleTimeUs = LW_MAX(settleTimeUs,
                                  PSI_GET_RAIL(railId)->settleTimeUs);
        }
        FOR_EACH_INDEX_IN_MASK_END;

        if (settleTimeUs > 0)
        {
            OS_PTIMER_SPIN_WAIT_US(settleTimeUs);
        }
    }
    return status;
}
