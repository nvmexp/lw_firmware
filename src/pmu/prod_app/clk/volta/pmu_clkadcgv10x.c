/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadcgv10x.c
 * @brief  Common Hal functions for ADC code in GV10X applicable across all
 *         all platforms. Part of AVFS
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
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_clkAdcCalProgram_V10(CLK_ADC_DEVICE *pAdcDevice,
                       LW2080_CTRL_CLK_ADC_CAL_V10 *pCalV10, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkAdcCalProgram_V10");
static FLCN_STATUS s_clkAdcCalProgram_V20(CLK_ADC_DEVICE *pAdcDevice,
                       LW2080_CTRL_CLK_ADC_CAL_V20 *pCalV20, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkAdcCalProgram_V20");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Enable/Disable ADC
 *
 * @details Always to be called by clkEnableAdc routine which caches the latest
 * enable/disable status. This function should just do the register programming
 *
 * @param[in]   pAdcDev     Pointer to ADC device
 * @param[in]   bEnable     Flag to indicate enable/disable operation
 *
 * @return FLCN_OK          Successfully enabled/disabled the ADC
 */
FLCN_STATUS
clkAdcEnable_GV10X
(
    CLK_ADC_DEVICE *pAdcDev,
    LwBool          bEnable
)
{
    LwU32 reg32;
    LwU32 data32;

    reg32 = CLK_ADC_REG_GET(pAdcDev, CTRL);

#ifdef LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_BACKUP_CTRL
    if (pAdcDev->super.type == LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V10)
    {
        reg32 = CLK_ADC_REG_GET(pAdcDev, BACKUP_CTRL);
    }
#endif

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
 * @details Always to be called by clkAdcPowerOn routine which caches the latest
 * power on/off status. This function should just do the register programming
 *
 * @param[in]   pAdcDev     Pointer to ADC device
 * @param[in]   bPowerOn    Flag to indicate the power on/off operation
 *
 * @return FLCN_OK                  Successfully powered on/off the ADC
 */
FLCN_STATUS
clkAdcPowerOn_GV10X
(
    CLK_ADC_DEVICE *pAdcDev,
    LwBool          bPowerOn
)
{
    LwU32 ctrlReg;
    LwU32 data32;

    ctrlReg = CLK_ADC_REG_GET(pAdcDev, CTRL);

#ifdef LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_BACKUP_CTRL
    if (pAdcDev->super.type == LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V10)
    {
        // Select the backup ADC and change the CTRL register to backup as well
        data32 = REG_RD32(FECS, ctrlReg);
        data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
                                    _SEL_BACKUP, _YES, data32);
        REG_WR32(FECS, ctrlReg, data32);
        ctrlReg = CLK_ADC_REG_GET(pAdcDev, BACKUP_CTRL);
    }
#endif

    data32 = REG_RD32(FECS, ctrlReg);
    data32 = bPowerOn ? FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
                                    _IDDQ, _POWER_ON, data32) :
                        FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
                                    _IDDQ, _POWER_OFF, data32);
    REG_WR32(FECS, ctrlReg, data32);

    if (bPowerOn)
    {
        //
        // When powering up an ADC, the internal bias/low pass filter needs
        // some time to settle down before the ADC can be enabled and used for
        // any operation
        //
#ifdef LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_BACKUP_CTRL
        OS_PTIMER_SPIN_WAIT_US((pAdcDev->super.type == LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V10) ?
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_BACKUP_CTRL_IDDQ_SETTLE_DELAY_US   :
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL_IDDQ_SETTLE_DELAY_US);
#elif defined UTF_REG_MOCKING
        //
        // TODO: KW - clean-up once the mockable support is available for
        // osPTimerSpinWaitNs calls. Jira-id: PMUS-1059
        // Do nothing when UTF register mocking is defined to avoid hangs
        //
#else
        OS_PTIMER_SPIN_WAIT_US(LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL_IDDQ_SETTLE_DELAY_US);
#endif
    }
    return FLCN_OK;
}

/*!
 * @brief Get bitmask of all supported ADCs on this chip
 *
 * @param[out]   pMask       Pointer to the bitmask
 */
void
clkAdcAllMaskGet_GV10X
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
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPCS);
}

/*!
 * @copydoc clkAdcCalProgram()
 */
FLCN_STATUS
clkAdcCalProgram_GV10X
(
    CLK_ADC_DEVICE *pAdcDev,
    LwBool          bEnable
)
{
    FLCN_STATUS status = FLCN_OK;
    CLK_ADC_DEVICE_V10 *pAdcDevV10 = (CLK_ADC_DEVICE_V10 *)(pAdcDev);
    CLK_ADC_DEVICE_V20 *pAdcDevV20 = (CLK_ADC_DEVICE_V20 *)(pAdcDev);

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V10) && 
        (pAdcDev->super.type == LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V10))
    {
        status = s_clkAdcCalProgram_V10(pAdcDev,
                     &(pAdcDevV10->data.adcCal),
                     bEnable);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V20_EXTENDED) &&
             (pAdcDev->super.type == LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V20) &&
             (pAdcDevV20->data.calType == LW2080_CTRL_CLK_ADC_CAL_TYPE_V10))
    {
        status = s_clkAdcCalProgram_V10(pAdcDev,
                     &(pAdcDevV20->data.adcCal.calV10),
                     bEnable);
    }
    else if ((pAdcDev->super.type == LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V20) &&
             (pAdcDevV20->data.calType == LW2080_CTRL_CLK_ADC_CAL_TYPE_V20))
    {
        status = s_clkAdcCalProgram_V20(pAdcDev,
                     &(pAdcDevV20->data.adcCal.calV20),
                     bEnable);
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERROR;
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Program the calibration version type V1.0 for the given ADC device
 *
 * @param[in]   pAdcDevice  Pointer to the ADC device
 * @param[in]   pCalV10     Pointer to the calibration version 1.0 struct
 * @param[in]   bEnable     Flag to indicate enable/disable operation
 *
 * @return  FLCN_OK - register programming successful
 */
static FLCN_STATUS
s_clkAdcCalProgram_V10
(
    CLK_ADC_DEVICE              *pAdcDevice,
    LW2080_CTRL_CLK_ADC_CAL_V10 *pCalV10,
    LwBool                       bEnable
)
{
    LwU32 addrCal  = CLK_ADC_REG_GET(pAdcDevice, CAL);
    LwU32 addrCtrl = CLK_ADC_REG_GET(pAdcDevice, CTRL);
    LwU32 adcCal   = 0;
    LwU32 adcCtrl  = 0;
    LwU16 adcGain;
    LwS16 adcOffset;

    // Read the current register value
    adcCal  = REG_RD32(FECS, addrCal);
    adcCtrl = REG_RD32(FECS, addrCtrl);

    if (bEnable)
    {
        // Callwlate and set the ADC gain value
        adcGain = (LwU16)((pCalV10->slope <<
                   DRF_SIZE(LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL_GAIN_FRACTION)) /
                   CLK_NAFLL_LUT_STEP_SIZE_UV());

        adcCal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_CAL,
                                 _GAIN, adcGain, adcCal);

        //
        // Now the ADC offset
        // Too much force casting but given the POR voltage/offset ranges 
        // should be safe:
        //  1. intercept (POR range < 1250000 uV) LwU32 => LwS32
        //  2. step size (current POR 6250 uV) LwU32 => LwS32 
        //  3. intercept/stepsize LwS32 => LwS16
        //
        adcOffset = (LwS16)(((LwS32)(pCalV10->intercept - CLK_NAFLL_LUT_MIN_VOLTAGE_UV()) <<
                                    DRF_SIZE(LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL_OFFSET_FRACTION)) /
                             (LwS32)CLK_NAFLL_LUT_STEP_SIZE_UV());

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

/*!
 * @brief   Program the calibration version type V2.0 for the given ADC device
 *
 * @param[in]   pAdcDevice  Pointer to the ADC device
 * @param[in]   pCalV20     Pointer to the calibration version 2.0 struct
 * @param[in]   bEnable     Flag to indicate enable/disable operation
 *
 * @return FLCN_OK  On success
 * @return other    Descriptive error code from sub-routines on failure
 */
static FLCN_STATUS
s_clkAdcCalProgram_V20
(
    CLK_ADC_DEVICE              *pAdcDevice,
    LW2080_CTRL_CLK_ADC_CAL_V20 *pCalV20,
    LwBool                       bEnable
)
{
    FLCN_STATUS status = FLCN_OK;

    if (bEnable)
    {
        LwU32 adcCtrl2 = REG_RD32(FECS, CLK_ADC_REG_GET(pAdcDevice, CTRL2));
#ifdef LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL2
        adcCtrl2 = FLD_SET_DRF_NUM(_PTRIM_SYS,
                                   _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL2,
                                   _ADC_CTRL_GAIN, 
                                   pCalV20->gain,
                                   adcCtrl2);
        adcCtrl2 = FLD_SET_DRF_NUM(_PTRIM_SYS,
                                   _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL2,
                                   _ADC_CTRL_OFFSET, 
                                   pCalV20->offset,
                                   adcCtrl2);
#else
        adcCtrl2 = FLD_SET_DRF_NUM(_PTRIM_SYS,
                                   _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL_2,
                                   _ADC_CTRL_GAIN, 
                                   pCalV20->gain,
                                   adcCtrl2);
        adcCtrl2 = FLD_SET_DRF_NUM(_PTRIM_SYS,
                                   _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL_2,
                                   _ADC_CTRL_OFFSET, 
                                   pCalV20->offset,
                                   adcCtrl2);
#endif

        REG_WR32(FECS, CLK_ADC_REG_GET(pAdcDevice, CTRL2), adcCtrl2);

        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_TEMPERATURE_VARIATION_WAR_BUG_200423771))
        {
            LwU32 adcCtrlReg = CLK_ADC_REG_GET(pAdcDevice, CTRL);
            status = clkAdcCalProgramCoarseControl_HAL(&Clk, pAdcDevice, pCalV20,
                         bEnable, adcCtrlReg);
        }
    }

    return status;
}
