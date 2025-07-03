/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadcgp100.c
 * @brief  Hal functions for ADC code in GP100. Part of AVFS
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objclk.h"
#include "pmu_objpmu.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * GP100 implementation differs from GP102+ since GP100 was running out of DMEM
 * and we had to aggressively decrease its memory footprint.
 */
#define CLK_ADC_REG_GET_GP100(_pAdcDev,_type)                                  \
    ((_pAdcDev)->adcRegAddrBase +                                              \
     (LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_##_type -                          \
      LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL))

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Enable/Disable ADC
 *
 * Always to be called by clkEnableAdc routine which caches the latest
 * enable/disable status. This function should just do the register programming
 *
 * Note: Power/Current
 * A. ADC when enabled and powered on consumes 6mA current
 * B. ADC when disabled but powered on consumes 1.8mA current
 * C. ADC when disabled and powered off consumes 0.01mA current
 *
 * Note: Latency
 * When ADC is enabled, we need to wait roughly 10 cycles for it to start
 * sampling the rail. This however is immeasurable and can be ignored.
 *
 * @param[in]   pAdcDev     Pointer to ADC device
 * @param[in]   bEnable     Flag to indicate enable/disable operation
 *
 * @return FLCN_OK                  Successfully enabled/disabled the ADC
 * @return FLCN_ERR_ILWALID_INDEX    Invalid ADC map index
 */
FLCN_STATUS
clkAdcEnable_GP100
(
    CLK_ADC_DEVICE *pAdcDev,
    LwBool          bEnable
)
{
    LwU32 reg32;
    LwU32 data32;

    reg32 = CLK_ADC_REG_GET_GP100(pAdcDev, CTRL);

    data32 = REG_RD32(FECS, reg32);
    data32 = bEnable ? FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
                                   _ENABLE, _YES, data32) :
                       FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
                                   _ENABLE, _NO, data32);
    REG_WR32(FECS, reg32, data32);

    return FLCN_OK;
}

/*!
 * @brief   Power on/off a given ADC
 *
 * Always to be called by clkAdcPowerOn routine which caches the latest
 * power on/off status. This function should just do the register programming
 *
 * Note: Power/Current
 * A. ADC when enabled and powered on consumes about 6mA current
 * B. ADC when disabled but powered on consumes about 1.8mA current
 * C. ADC when disabled and powered off consumes about 0.01mA current
 *
 * Note: Latency
 * When ADC is powered up, we need to wait roughly 1us for the bias/low pass
 * filter in the ADC to settle down
 *
 * @param[in]   pAdcDev     Pointer to ADC device
 * @param[in]   bPowerOn    Flag to indicate the power on/off operation
 *
 * @return FLCN_OK                  Successfully enabled/disabled and powered
 *                                  on/offthe ADC
 * @return FLCN_ERR_ILWALID_INDEX    Invalid ADC map index
 */
FLCN_STATUS
clkAdcPowerOn_GP100
(
    CLK_ADC_DEVICE *pAdcDev,
    LwBool          bPowerOn
)
{
    LwU32 reg32;
    LwU32 data32;

    reg32 = CLK_ADC_REG_GET_GP100(pAdcDev, CTRL);

    data32 = REG_RD32(FECS, reg32);
    data32 = bPowerOn ? FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
                                    _IDDQ, _POWER_ON, data32) :
                        FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
                                    _IDDQ, _POWER_OFF, data32);
    REG_WR32(FECS, reg32, data32);

    if (bPowerOn)
    {
        //
        // When powering up an ADC, the internal bias/low pass filter needs
        // a fixed delay to settle down before the ADC can be enabled and used
        // for any operation.
        //
        OS_PTIMER_SPIN_WAIT_US(
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL_IDDQ_SETTLE_DELAY_US);
    }
    return FLCN_OK;
}

/*!
 * @brief Get bitmask of all supported ADCs on this chip
 *
 * @param[out]   pMask       Pointer to the bitmask
 */
void
clkAdcAllMaskGet_GP100
(
    LwU32  *pMask
)
{
    *pMask = BIT32(LW2080_CTRL_CLK_ADC_ID_SYS) |
             BIT32(LW2080_CTRL_CLK_ADC_ID_LTC) |
             BIT32(LW2080_CTRL_CLK_ADC_ID_XBAR)|
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC0)|
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC1)|
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC2)|
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC3)|
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC4)|
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC5)|
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPCS)|
             BIT32(LW2080_CTRL_CLK_ADC_ID_SRAM);
}

/*
 * @brief   Set or clear ADC code
 *
 * @param[in]   pAdcDev         Pointer to ADC device
 * @param[in]   overrideMode    SW override mode of operation.
 * @param[in]   overrideCode    SW override ADC code.
 *
 * @return FLCN_OK                  Operation completed successfully
 * @return FLCN_ERR_ILWALID_INDEX    Invalid ADC map index
 */
FLCN_STATUS
clkAdcCodeOverrideSet_GP100
(
    CLK_ADC_DEVICE *pAdcDev,
    LwU8            overrideCode,
    LwU8            overrideMode
)
{
    LwU32  data32 = 0;

    // Compute values to write
    switch (overrideMode)
    {
        case LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_SW_REQ:
        {
            //
            // Set ADC_SAMPLE to 0 => Logic voltage override.
            // This is because we do not care for SRAM voltage for EMU.
            //
            data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_OVERRIDE,
                                     _ADC_SAMPLE, 0, data32);
            // Set ADC_DOUT to the ADC code to override with
            data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_OVERRIDE,
                                     _ADC_DOUT, overrideCode, data32);
            // Set SW_OVERRIDE_ADC_YES
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_OVERRIDE,
                                 _SW_OVERRIDE_ADC, _YES, data32);
            break;
        }
        case LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_HW_REQ:
        {
            // Set SW_OVERRIDE_ADC_NO
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_OVERRIDE,
                             _SW_OVERRIDE_ADC, _NO, data32);
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_ILWALID_ARGUMENT;
        }
    }

    // Now the register writes
    REG_WR32(FECS, CLK_ADC_REG_GET_GP100(pAdcDev, OVERRIDE), data32);

    return FLCN_OK;
}

/*!
 * @brief   Read back the ADC aclwmulator and num_samples.
 *          Cache these values in the given ADC device.
 *
 * @param[in]      pAdcDev              Pointer to the ADC device
 * @param[in/out]  pAdcAccSample        Pointer to the Aclwmulator sample struct
 *
 * @return FLCN_OK                  Operation completed successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT Invalid argument
 * @return FLCN_ERR_ILWALID_INDEX    Invalid ADC map index
 * @return FLCN_ERR_ILWALID_STATE    In case the ADC is not enabled
 */
FLCN_STATUS
clkAdcVoltageParamsRead_GP100
(
    CLK_ADC_DEVICE     *pAdcDev,
    CLK_ADC_ACC_SAMPLE *pAdcAccSample
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pAdcDev == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkAdcVoltageParamsRead_GP100_exit;
    }

    //
    // Return early if the ADC is not already powered-on/enabled
    // (a preceding SET_CONTROL)
    //
    if ((!pAdcDev->bPoweredOn) || (pAdcDev->disableClientsMask != 0))
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto clkAdcVoltageParamsRead_GP100_exit;
    }

    pAdcAccSample->adcAclwmulatorVal =
        REG_RD32(FECS, CLK_ADC_REG_GET_GP100(pAdcDev, ACC));
    pAdcAccSample->adcNumSamplesVal =
        REG_RD32(FECS, CLK_ADC_REG_GET_GP100(pAdcDev, NUM_SAMPLES));

clkAdcVoltageParamsRead_GP100_exit:
    return status;
}

/*!
 * @brief   Initialize register maping within ADC device
 *
 * @param[in]   pAdcDev     Pointer to the ADC device
 *
 * @return  FLCN_ERR_ILWALID_INDEX   If reister mapping init has failed
 * @return  FLCN_OK                 Otherwise
 */
FLCN_STATUS
clkAdcRegMapInit_GP100
(
   CLK_ADC_DEVICE *pAdcDev
)
{
    LwU8    adcId = pAdcDev->id;
    LwU32   adcRegAddrBase;

    switch (adcId)
    {
        case LW2080_CTRL_CLK_ADC_ID_SYS:
        case LW2080_CTRL_CLK_ADC_ID_LTC:
        case LW2080_CTRL_CLK_ADC_ID_XBAR:
            adcRegAddrBase = LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL;
            break;
        case LW2080_CTRL_CLK_ADC_ID_GPC0:
            adcRegAddrBase = LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(0);
            break;
        case LW2080_CTRL_CLK_ADC_ID_GPC1:
            adcRegAddrBase = LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(1);
            break;
        case LW2080_CTRL_CLK_ADC_ID_GPC2:
            adcRegAddrBase = LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(2);
            break;
        case LW2080_CTRL_CLK_ADC_ID_GPC3:
            adcRegAddrBase = LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(3);
            break;
        case LW2080_CTRL_CLK_ADC_ID_GPC4:
            adcRegAddrBase = LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(4);
            break;
        case LW2080_CTRL_CLK_ADC_ID_GPC5:
            adcRegAddrBase = LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(5);
            break;
        case LW2080_CTRL_CLK_ADC_ID_GPCS:
            adcRegAddrBase = LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL;
            break;
        case LW2080_CTRL_CLK_ADC_ID_SRAM:
            adcRegAddrBase = LW_PTRIM_SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_CTRL;
            break;
        default:
            return FLCN_ERR_ILWALID_INDEX;
    }

    pAdcDev->adcRegAddrBase = adcRegAddrBase;

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
