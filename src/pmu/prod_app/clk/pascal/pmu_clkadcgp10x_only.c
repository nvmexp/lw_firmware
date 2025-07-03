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
 * @file   pmu_clkadcgp10x_only.c
 * @brief  Hal functions for ADC code in GP10X only. Part of AVFS
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
#define ADC_STEP_SIZE_UV            10000
#define ADC_MIN_VOLT_UV             450000

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
clkAdcEnable_GP10X
(
    CLK_ADC_DEVICE *pAdcDev,
    LwBool          bEnable
)
{
    LwU32 reg32;
    LwU32 data32;

    reg32 = CLK_ADC_REG_GET(pAdcDev, CTRL);

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
 * @param[in]   pAdcDeve    Pointer to ADC device
 * @param[in]   bPowerOn    Flag to indicate the power on/off operation
 *
 * @return FLCN_OK                  Successfully enabled/disabled and powered
 *                                  on/offthe ADC
 * @return FLCN_ERR_ILWALID_INDEX    Invalid ADC map index
 */
FLCN_STATUS
clkAdcPowerOn_GP10X
(
    CLK_ADC_DEVICE *pAdcDev,
    LwBool          bPowerOn
)
{
    LwU32 reg32;
    LwU32 data32;

    reg32 = CLK_ADC_REG_GET(pAdcDev, CTRL);

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
        // 1us to settle down before the ADC can be enabled and used for any
        // operation
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
clkAdcAllMaskGet_GP10X
(
    LwU32  *pMask
)
{
    *pMask = BIT32(LW2080_CTRL_CLK_ADC_ID_SYS) |
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
 * @copydoc clkAdcCalProgram
 */
FLCN_STATUS
clkAdcCalProgram_GP10X
(
    CLK_ADC_DEVICE *pAdcDev,
    LwBool          bEnable
)
{
    LwU32 addrCal  = CLK_ADC_REG_GET(pAdcDev, CAL);
    LwU32 addrCtrl = CLK_ADC_REG_GET(pAdcDev, CTRL);
    LwU32 adcCal   = 0;
    LwU32 adcCtrl  = 0;
    LwU16 adcGain;
    LwS16 adcOffset;
    LW2080_CTRL_CLK_ADC_CAL_V10 *pCalV10 = &((CLK_ADC_DEVICE_V10 *)(pAdcDev))->data.adcCal;

    // Read the current register value
    adcCal  = REG_RD32(FECS, addrCal);
    adcCtrl = REG_RD32(FECS, addrCtrl);

    if (bEnable)
    {
        // Callwlate and set the ADC gain value
        adcGain = (LwU16)((pCalV10->slope <<
                   DRF_SIZE(LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL_GAIN_FRACTION)) /
                   ADC_STEP_SIZE_UV);

        adcCal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CAL,
                                 _GAIN, adcGain, adcCal);

        // Now the ADC offset
        adcOffset = (LwS16)((((LwS32)pCalV10->intercept - ADC_MIN_VOLT_UV) <<
                     DRF_SIZE(LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL_OFFSET_FRACTION)) /
                     ADC_STEP_SIZE_UV);

        adcCal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CAL,
                                 _OFFSET, adcOffset, adcCal);

        // Finally set disable bit to NO i.e. Enable it ;)
        adcCal = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CAL,
                             _DISABLE, _NO, adcCal);

        // Now aclwmulator should accumulate calibrated values.
        adcCtrl = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
                              _ACC_SEL, _CALIBRATED, adcCtrl);
    }
    else
    {
        // Just set the disable bit to YES
        adcCal = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CAL,
                                 _DISABLE, _YES, adcCal);

        // Aclwmulator should accumulate uncalibrated values.
        adcCtrl = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
                              _ACC_SEL, _UNCALIBRATED, adcCtrl);
    }

    // Now the register writes
    REG_WR32(FECS, addrCal,  adcCal);
    REG_WR32(FECS, addrCtrl, adcCtrl);

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
