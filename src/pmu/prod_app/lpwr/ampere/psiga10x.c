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
 * @file   pmu_pgpsigp10x.c
 * @brief  PMU PSI related Hal functions for Pascal and Later GPU.
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pmgr.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
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
psiGpioTrigger_GA10X
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

            regVal = REG_RD32(FECS, LW_GPIO_OUTPUT_CNTL(pPsiRail->gpioPin));

            if (bEnable)
            {
                //
                // For AutpPSI, enable PSI is equivalent to putting the
                // GPIO pin into tristate
                //
                if (PSI_FLAVOUR_IS_ENABLED(AUTO, railId))
                {
                    regVal = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL,
                                         _IO_OUT_EN, _NO, regVal);
                }
                else
                {
                    regVal = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL,
                                         _IO_OUTPUT, _0, regVal);
                }

                // Increment Engage count
                psiCtrlEngageCountIncrease(ctrlId, railId);

            }
            else
            {
                regVal = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL,
                                     _IO_OUTPUT, _1, regVal);

                //
                // For Auto PSI, disable PSI should pull out the GPIO from
                // tristate, and drive a 1 to the pin.
                //
                if (PSI_FLAVOUR_IS_ENABLED(AUTO, railId))
                {
                    regVal = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL,
                                         _IO_OUT_EN, _YES, regVal);
                }
            }

            REG_WR32(FECS, LW_GPIO_OUTPUT_CNTL(pPsiRail->gpioPin), regVal);
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

/*!
 * @brief Function to trigger the I2C command to enagage/disengage PSI
 *
 * @param[in]   ctrlId        PSI Ctrl Id
 * @param[in]   railMask      PSI rail mask
 * @param[in]   bEnable       Enable  PSI if LW_TRUE
 *                            Disable PSI if LW_FALSE
 */
FLCN_STATUS
psiI2cCommandTrigger_GA10X
(
    LwU8   ctrlId,
    LwU32  railMask,
    LwBool bPsiEnable
)
{
    PSI_RAIL    *pPsiRail = NULL;
    FLCN_STATUS  status   = FLCN_OK;
    I2C_DEVICE  *pI2cDev;
    LwU32        railId;
    LwU32        railSelectAddr;
    LwU32        railPhaseUpdateAddr;
    LwU16        railSelectCmd;
    LwU16        railPhaseSetCmd;
    LwU32        settleTimeUs = 0;

    FOR_EACH_INDEX_IN_MASK(32, railId, railMask)
    {
        pPsiRail = PSI_GET_RAIL(railId);

        // Get I2C device info
        pI2cDev = I2C_DEVICE_GET(pPsiRail->pI2cInfo->i2cDevIdx);
        if (pI2cDev == NULL)
        {
            PMU_BREAKPOINT();

            status = FLCN_ERROR;
            goto psiI2cCommandTrigger_GA10X_exit;
        }

        // I2C Command Sequence for PSI based on VR MP2891
        if (pPsiRail->pI2cInfo->i2cDevType == RM_PMU_I2C_DEVICE_TYPE_POWER_CONTRL_MP28XX)
        {
            // Get the I2C Command for given Rail
            if (bPsiEnable)
            {
                // I2C command to selct the Rail
                railSelectAddr = pPsiRail->pI2cInfo->psiEngageCommand[PSI_I2C_CMD_RAIL_SELECT].controlAddr;
                railSelectCmd  = pPsiRail->pI2cInfo->psiEngageCommand[PSI_I2C_CMD_RAIL_SELECT].val;

                // I2C command to change the Rail Phase
                railPhaseUpdateAddr =  pPsiRail->pI2cInfo->psiEngageCommand[PSI_I2C_CMD_RAIL_PHASE_UPDATE].controlAddr;
                railPhaseSetCmd     =  pPsiRail->pI2cInfo->psiEngageCommand[PSI_I2C_CMD_RAIL_PHASE_UPDATE].val;
            }
            else
            {
                // I2C command to selct the Rail
                railSelectAddr = pPsiRail->pI2cInfo->psiDisengageCommand[PSI_I2C_CMD_RAIL_SELECT].controlAddr;
                railSelectCmd  = pPsiRail->pI2cInfo->psiDisengageCommand[PSI_I2C_CMD_RAIL_SELECT].val;

                // I2C command to change the Rail Phase
                railPhaseUpdateAddr =  pPsiRail->pI2cInfo->psiDisengageCommand[PSI_I2C_CMD_RAIL_PHASE_UPDATE].controlAddr;
                railPhaseSetCmd     =  pPsiRail->pI2cInfo->psiDisengageCommand[PSI_I2C_CMD_RAIL_PHASE_UPDATE].val;
            }

            //
            // Step 1 - Select the Rail on I2C device - 1 Byte Command
            //
            // Step 2 - Update the Phase of Selected Rail. - 2 Byte Command
            //

            // Step 1
            status = i2cDevWriteReg8(pI2cDev, LpwrI2cQueue, railSelectAddr, railSelectCmd);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto psiI2cCommandTrigger_GA10X_exit;
            }

            // Step 2
            status = i2cDevWriteReg16(pI2cDev, LpwrI2cQueue, railPhaseUpdateAddr, railPhaseSetCmd);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto psiI2cCommandTrigger_GA10X_exit;
            }
        }
        else
        {
            PMU_BREAKPOINT();
        }

        if (bPsiEnable)
        {
            // Increment Engage count as I2C trigger was successfull
            psiCtrlEngageCountIncrease(ctrlId, railId);
        }

        settleTimeUs = LW_MAX(settleTimeUs,
                              PSI_GET_RAIL(railId)->settleTimeUs);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //
    // After PSI got "engaged", wait for minimum time for which power regulator
    // should be in PSI. In general, this time is "Zero" or equal to regulator
    // settle time.
    //
    // Wait for settleTimeUs after "dis-engaging" PSI. The regulator needs some
    // time to complete the switch before we can proceed further.
    //

    if (settleTimeUs > 0)
    {
        OS_PTIMER_SPIN_WAIT_US(settleTimeUs);
    }

psiI2cCommandTrigger_GA10X_exit:

    return status;
}
