/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadcgp10x_extended.c
 * @brief  Hal functions for ADC code in GP10X for the extended ADC infra. Part
 *         of AVFS
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
clkAdcVoltageParamsRead_GP10X
(
    CLK_ADC_DEVICE     *pAdcDev,
    CLK_ADC_ACC_SAMPLE *pAdcAccSample
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pAdcDev == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkAdcVoltageParamsRead_GP10X_exit;
    }

    //
    // Return early if the ADC is not already powered-on/enabled
    // (a preceding SET_CONTROL)
    //
    if ((!pAdcDev->bPoweredOn) || (pAdcDev->disableClientsMask != 0U))
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto clkAdcVoltageParamsRead_GP10X_exit;
    }

    // Program CAPTURE_ACC to capture/latch aclwmulator values.
    status = clkAdcAccCapture_HAL(&Clk, pAdcDev, LW_FALSE);
    if (status != FLCN_OK)
    {
        goto clkAdcVoltageParamsRead_GP10X_exit;
    }

    pAdcAccSample->adcAclwmulatorVal =
        REG_RD32(FECS, CLK_ADC_REG_GET(pAdcDev, ACC));
    pAdcAccSample->adcNumSamplesVal =
        REG_RD32(FECS, CLK_ADC_REG_GET(pAdcDev, NUM_SAMPLES));

clkAdcVoltageParamsRead_GP10X_exit:
    return status;
}

/*!
 * @brief Program CAPTURE_ACC to capture/latch ADC aclwmulator values before
 *        reading them.
 *
 * @param[in]   pAdcDev     Pointer to the ADC device
 * @param[in]   bLatchAcc   Unused in this implementation
 *
 * @return  FLCN_OK         Operation completed successfully
 */
FLCN_STATUS
clkAdcAccCapture_GP10X
(
    CLK_ADC_DEVICE *pAdcDev,
    LwBool          bLatchAcc
)
{
    LwU32 addrCtrl = CLK_ADC_REG_GET(pAdcDev, CTRL);;
    LwU32 data32;

    data32 = REG_RD32(FECS, addrCtrl);
    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
                             _CAPTURE_ACC, 1, data32);
    REG_WR32(FECS, addrCtrl, data32);

    // Add a delay of 5us to propogate latched values.
    OS_PTIMER_SPIN_WAIT_NS(CLK_ADC_ACC_RDACK_TIMEOUT_US * 1000);

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
