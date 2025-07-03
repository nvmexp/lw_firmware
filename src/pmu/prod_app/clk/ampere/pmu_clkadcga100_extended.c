/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadcga100_extended.c
 * @brief  Hal functions for ADC code in GA100. Part of AVFS
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objclk.h"
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Read back the ADC aclwmulator and num_samples.
 *          Cache these values in the given ADC device.
 *
 * @param[in]      pAdcDev              Pointer to the ADC device
 * @param[in,out]  pAdcAccSample        Pointer to the Aclwmulator sample struct
 *
 * @return FLCN_OK                   Operation completed successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT In case ADC device is NULL
 * @return FLCN_ERR_ILWALID_STATE    In case the ADC is not powered-on/enabled
 */
FLCN_STATUS
clkAdcVoltageParamsRead_GA100
(
    CLK_ADC_DEVICE     *pAdcDev,
    CLK_ADC_ACC_SAMPLE *pAdcAccSample
)
{
    FLCN_STATUS status      = FLCN_OK;
    LwBool      bCaptureAcc = LW_FALSE;

    if ((pAdcDev == NULL) || (pAdcAccSample == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkAdcVoltageParamsRead_GA100_exit;
    }

    //
    // Return early if the ADC is not already powered-on/enabled
    // (a preceding SET_CONTROL)
    //
    if ((!pAdcDev->bPoweredOn) || (pAdcDev->disableClientsMask != 0U))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkAdcVoltageParamsRead_GA100_exit;
    }

    if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_ACC_BATCH_LATCH_SUPPORTED))
    {
        if (BOARDOBJ_GET_TYPE(pAdcDev) !=
                LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10)
        {
            bCaptureAcc = LW_TRUE;
        }
    }

    // Program CAPTURE_ACC to capture/latch aclwmulator values.
    if (bCaptureAcc)
    {
        status = clkAdcAccCapture_HAL(&Clk, pAdcDev, LW_TRUE);
        if (status != FLCN_OK)
        {
            goto clkAdcVoltageParamsRead_GA100_exit;
        }
    }

    // Read back aclwmulator values
    pAdcAccSample->adcAclwmulatorVal =
        REG_RD32(FECS, CLK_ADC_REG_GET(pAdcDev, ACC));
    pAdcAccSample->adcNumSamplesVal =
        REG_RD32(FECS, CLK_ADC_REG_GET(pAdcDev, NUM_SAMPLES));

    // Program CAPTURE_ACC to unlatch aclwmulator values.
    if (bCaptureAcc)
    {
        status = clkAdcAccCapture_HAL(&Clk, pAdcDev, LW_FALSE);
        if (status != FLCN_OK)
        {
            goto clkAdcVoltageParamsRead_GA100_exit;
        }
    }

clkAdcVoltageParamsRead_GA100_exit:
    return status;
}

/*!
 * @brief Program CAPTURE_ACC to capture/latch ADC aclwmulator values before
 *        reading them.
 *
 * @param[in]   pAdcDev     Pointer to the ADC device
 * @param[in]   bLatchAcc   Boolean to indicate if CAPTURE_ACC is set
 *
 * @return FLCN_OK              Operation completed successfully
 * @return FLCN_ERR_TIMEOUT     Timeout on polling for RDACK
 */
FLCN_STATUS
clkAdcAccCapture_GA100
(
    CLK_ADC_DEVICE *pAdcDev,
    LwBool          bLatchAcc
)
{
    FLCN_STATUS status   = FLCN_OK;
    LwU32       addrCtrl = CLK_ADC_REG_GET(pAdcDev, CTRL);
    LwU32       data32;

    data32 = REG_RD32(FECS, addrCtrl);
    if (bLatchAcc)
    {
        //
        // This stops continually capture ADC_ACC and ADC_NUM_SAMPLES values.
        // This ensures the registers are frozen to the last value encountered
        // while CAPTURE_ACC=1. ADC_ACC.VAL and NUM_SAMPLES are guaranteed to be
        // from the same point in time.
        //
        data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
                             _CAPTURE_ACC, 0, data32);
    }
    else
    {
        data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
                             _CAPTURE_ACC, 1, data32);
    }
    REG_WR32(FECS, addrCtrl, data32);

    // Poll on _RDACK_ADC_ACC for 5 us
    if (!PMU_REG32_POLL_US(
            USE_FECS(addrCtrl),                                                      // Address
            DRF_SHIFTMASK(LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL_RDACK_ADC_ACC), // Mask
            (bLatchAcc ? 
            DRF_NUM(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, _RDACK_ADC_ACC, 0) :
            DRF_NUM(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, _RDACK_ADC_ACC, 1)), // Value
            CLK_ADC_ACC_RDACK_TIMEOUT_US,                                            // Timeout
            PMU_REG_POLL_MODE_BAR0_EQ))                                              // Mode
    {
        //
        // Even if we timeout do continue with ACC reads anyway
        // http://lwbugs/2571189
        // TODO: add feature to catch the timeout and halt on debug builds
        //
        status = FLCN_OK;
        goto clkAdcAccCapture_GA100_exit;
    }

clkAdcAccCapture_GA100_exit:
    return status;
}

/*!
 * @brief   Initializes requested ADC aclwmulators
 *
 * @param[in]     pAdcDev           Pointer to the ADC device
 * @param[in,out] pAdcAccSample     Pointer to the Aclwmulator sample struct
 *
 * @return FLCN_OK              Operation completed successfully
 * @return FLCN_ERR_TIMEOUT     Timeout on polling for RDACK
 */
FLCN_STATUS
clkAdcAccInit_GA100
(
    CLK_ADC_DEVICE     *pAdcDev,
    CLK_ADC_ACC_SAMPLE *pAdcAccSample
)
{
    FLCN_STATUS status   = FLCN_OK;
    LwU32       addrCtrl = CLK_ADC_REG_GET(pAdcDev, CTRL);
    LwU32       data32   = REG_RD32(FECS, addrCtrl);

    //
    // Enable the aclwmulator for sampling voltage & tell the aclwmulator to
    // accumulate calibrated values.
    //
    data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, _ACC_ENABLE, _YES, data32) |
             FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, _ACC_SEL, _CALIBRATED, data32);

    REG_WR32(FECS, addrCtrl, data32);

    // Enable continuous capture of ADC_ACC and ADC_NUM_SAMPLES values
    status = clkAdcAccCapture_HAL(&Clk, pAdcDev, LW_FALSE);
    if (status != LW_OK)
    {
        goto clkAdcAccInit_GA100_exit;
    }

clkAdcAccInit_GA100_exit:
    return status;
}

/*!
 * @brief   Program the calibration offset for the given ADC device to compensate
 * for voltage/temperature based variations in calibration.
 *
 * @param[in]   pAdcDev             Pointer to the ADC device
 *
 * @return FLCN_OK
 *      ADC code correction offset programmed correctly
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If the ADC type is prior to @ref LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30
 * @return other
 *      Descriptive error code from sub-routines on failure
 */
FLCN_STATUS
clkAdcCalOffsetProgram_GA100
(
    CLK_ADC_DEVICE     *pAdcDev
)
{
    CLK_ADC_DEVICE_V30 *pAdcDevV30 = (CLK_ADC_DEVICE_V30 *)pAdcDev;
    FLCN_STATUS         status     = FLCN_OK;
    LwU32               adcCalRegValue;
    LwS32               adcCorrectionOffset; 

    if (BOARDOBJ_GET_TYPE(pAdcDev) < LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto _clkAdcCalOffsetProgram_GA100_exit;
    }

    //
    // Format output to [9] sign, [8:4] integer, [3:0] fraction
    // adcCorrectionOffset = ((callwlatedAdcCorrectionOffset & 0x0000003F) << 4);
    //
    adcCorrectionOffset = (LwS32)((pAdcDevV30->targetAdcCodeCorrectionOffset &
                DRF_MASK(LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL_OFFSET_INTEGER)) <<
                        DRF_SIZE(LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL_OFFSET_FRACTION));

    // Program NAFLL_ADC_CAL with the code correction offset
    adcCalRegValue = REG_RD32(FECS, CLK_ADC_REG_GET(pAdcDev, CAL));
    adcCalRegValue = FLD_SET_DRF_NUM(_PTRIM, _SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL,
                                     _OFFSET, adcCorrectionOffset, adcCalRegValue);
    adcCalRegValue = FLD_SET_DRF(_PTRIM, _SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL,
                                            _DISABLE, _NO, adcCalRegValue);
    REG_WR32(FECS, CLK_ADC_REG_GET(pAdcDev, CAL), adcCalRegValue);

_clkAdcCalOffsetProgram_GA100_exit:
    return status;
}
/* ------------------------- Private Functions ------------------------------ */
