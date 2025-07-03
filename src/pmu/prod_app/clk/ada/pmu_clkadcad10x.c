/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadcad10x.c
 * @brief  Hal functions for ADC code in AD10X. Part of AVFS.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
#include "dev_pwr_csb.h"
#include "dev_chiplet_pwr.h"
#include "dev_chiplet_pwr_addendum.h"
#endif // PMU_CLK_ADC_RAM_ASSIST
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Get bitmask of all supported ADCs on this chip
 *
 * @param[out]   pMask       Pointer to the bitmask
 */
void
clkAdcAllMaskGet_AD10X
(
    LwU32  *pMask
)
{
    *pMask = BIT32(LW2080_CTRL_CLK_ADC_ID_SYS)      |
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC0)     |
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC1)     |
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC2)     |
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC3)     |
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC4)     |
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC5)     |
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC6)     |
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC7)     |
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC8)     |
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC9)     |
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC10)    |
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC11)    |
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPCS)     |
             BIT32(LW2080_CTRL_CLK_ADC_ID_SYS_ISINK);
}

#ifdef LW_PTRIM_SYS_NAFLL_ISINK_ADC_CTRL
/*!
 * @brief Get the ADC step size of the given ADC device
 *
 * @param[in]   pAdcDev     Pointer to the ADC device
 *
 * @return      LwU32       Step size in uV
 */
LwU32
clkAdcStepSizeUvGet_AD10X
(
    CLK_ADC_DEVICE  *pAdcDev
)
{
    if (BOARDOBJ_GET_TYPE(pAdcDev) ==
            LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10)
    {
        return LW_PTRIM_SYS_NAFLL_ISINK_ADC_CTRL_STEP_SIZE_UV;
    }
    else
    {
        return CLK_NAFLL_LUT_STEP_SIZE_UV();
    }
}

/*!
 * @brief Get the minimum voltage supported by the given ADC device
 *
 * @param[in]   pAdcDev     Pointer to the ADC device
 *
 * @return      LwU32       Minimum voltage in uV
 */
LwU32
clkAdcMilwoltageUvGet_AD10X
(
    CLK_ADC_DEVICE  *pAdcDev
)
{
    if (BOARDOBJ_GET_TYPE(pAdcDev) ==
            LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10)
    {
        return LW_PTRIM_SYS_NAFLL_ISINK_ADC_CTRL_ZERO_UV;
    }
    else
    {
        return CLK_NAFLL_LUT_MIN_VOLTAGE_UV();
    }
}
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
/*!
 * @brief Enable/disable RAM assist feature for the given ADC device
 *
 * @param[in]   pAdcDev  Pointer to the ADC device
 * @param[in]   bEnable  Boolean to indicate if RAM_ASSIST_ADC_CTRL_ENABLE
 *                       needs to be enabled or disabled
 *
 * @return FLCN_OK
 * 
 */
FLCN_STATUS
clkAdcDeviceRamAssistCtrlEnable_AD10X
(
    CLK_ADC_DEVICE  *pAdcDev,
    LwBool           bEnable
)
{
    LwU32  reg32;
    LwU32  data32;
    LwU32  data32Old;

    // Update the RAM_ASSIST_ADC_CTRL_ENABLE bit to the bEnable state
    reg32  = CLK_ADC_LOGICAL_REG_GET(pAdcDev, RAM_ASSIST_ADC_CTRL);
    data32Old = data32 = REG_RD32(FECS, reg32);

    if (bEnable)
    {
        data32 = FLD_SET_DRF(_PCHIPLET, _PWR_GPC_LWVDD, _RAM_ASSIST_ADC_CTRL_ENABLE, _YES,
                             data32);
    }
    else
    {
        data32 = FLD_SET_DRF(_PCHIPLET, _PWR_GPC_LWVDD, _RAM_ASSIST_ADC_CTRL_ENABLE, _NO,
                             data32);
    }

    // Write if only the bit flipped
    if (data32Old != data32)
    {
        REG_WR32(FECS, reg32, data32);
    }

    return FLCN_OK;
}

/*!
 * @brief Program the RAM_ASSIST VCrit threshold values for a given ADC device
 *
 * @param[in]  pAdcDev            Pointer to the ADC device
 * @param[in]  vcritThreshLowuV   RAM_ASSIST Vcrit Low threshold value in microvolts to program
 * @param[in]  vcritThreshHighuV  RAM_ASSIST Vcrit High threshold value in microvolts to program
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkAdcDeviceProgramVcritThresholds_AD10X
(
    CLK_ADC_DEVICE  *pAdcDev,
    LwU32            vcritThreshLowuV,
    LwU32            vcritThreshHighuV
)
{
    LwU32  reg32;
    LwU32  data32;
    LwU32  data32Old;
    LwU32  vcritThreshLowCode  = CLK_ADC_CODE_GET(vcritThreshLowuV);
    LwU32  vcritThreshHighCode = CLK_ADC_CODE_GET(vcritThreshHighuV);

    // Update the RAM_ASSIST_THRESH values
    reg32  = CLK_ADC_LOGICAL_REG_GET(pAdcDev, RAM_ASSIST_THRESH);
    data32Old = data32 = REG_RD32(FECS, reg32);
    data32 = FLD_SET_DRF_NUM(_PCHIPLET, _PWR_GPC_LWVDD_RAM_ASSIST_THRESH,
                               _VCRIT_LOW, vcritThreshLowCode, data32);
    data32 = FLD_SET_DRF_NUM(_PCHIPLET, _PWR_GPC_LWVDD_RAM_ASSIST_THRESH,
                               _VCRIT_HIGH, vcritThreshHighCode, data32);

    // Write if only there was change in value
    if (data32Old != data32)
    {
        REG_WR32(FECS, reg32, data32);
    }

    return FLCN_OK;
}

/*!
 * @brief Check whether RAM Assist linked with given ADC device is engaged or not
 *
 * @param[in]    pAdcDev    Pointer to the ADC device
 * @param[out]   pBEngaged  Boolean to indicate if RAM_ASSIST linked with given ADC 
                            is Engaged or Not
 *
 * @return FLCN_OK
 * 
 */
FLCN_STATUS
clkAdcDeviceRamAssistIsEngaged_AD10X
(
    CLK_ADC_DEVICE  *pAdcDev,
    LwBool          *pBEngaged
)
{
    LwU32  reg32;
    LwU32  data32;

    // Read the RAM ASSIST DBG Register of the given ADC
    reg32     = CLK_ADC_LOGICAL_REG_GET(pAdcDev, RAM_ASSIST_DBG_0);
    data32    = REG_RD32(FECS, reg32);

    *pBEngaged = FLD_TEST_DRF(_PCHIPLET, _PWR_GPC0_LWVDD_RAM_ASSIST_DBG_0,
                                 _ASSIST_STAT, _ENGAGED, data32);

    return FLCN_OK;
}
#endif // PMU_CLK_ADC_RAM_ASSIST

/*!
 * @copydoc clkAdcCalProgram()
 */
FLCN_STATUS
clkAdcCalProgram_AD10X
(
    CLK_ADC_DEVICE *pAdcDev,
    LwBool          bEnable
)
{
    CLK_ADC_DEVICE_V30 *pAdcDevV30 = (CLK_ADC_DEVICE_V30 *)(pAdcDev);

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_2X) ||
        !PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V30))
    {
        PMU_BREAKPOINT();
    }
    else
    {
        if (bEnable)
        {
            LwU32 adcCtrl2Reg = CLK_ADC_REG_GET(pAdcDev, CTRL2);
            LwU32 adcCtrl2Val = REG_RD32(FECS, adcCtrl2Reg );

#ifdef LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL2
            adcCtrl2Val = FLD_SET_DRF_NUM(_PTRIM_SYS,
                                          _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL2,
                                          _ADC_CTRL_GAIN,
                                          pAdcDevV30->infoData.gain,
                                          adcCtrl2Val);
            adcCtrl2Val = FLD_SET_DRF_NUM(_PTRIM_SYS,
                                          _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL2,
                                          _ADC_CTRL_OFFSET,
                                          pAdcDevV30->infoData.offset,
                                          adcCtrl2Val);
            adcCtrl2Val = FLD_SET_DRF_NUM(_PTRIM_SYS,
                                          _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL2,
                                          _ADC_CTRL_COARSE_OFFSET,
                                          pAdcDevV30->infoData.coarseOffset,
                                          adcCtrl2Val);
            adcCtrl2Val = FLD_SET_DRF_NUM(_PTRIM_SYS,
                                          _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL2,
                                          _ADC_CTRL_COARSE_GAIN,
                                          pAdcDevV30->infoData.coarseGain,
                                          adcCtrl2Val);
#else
            adcCtrl2Val = FLD_SET_DRF_NUM(_PTRIM_SYS,
                                          _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL_2,
                                          _ADC_CTRL_GAIN,
                                          pAdcDevV30->infoData.gain,
                                          adcCtrl2Val);
            adcCtrl2Val = FLD_SET_DRF_NUM(_PTRIM_SYS,
                                          _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL_2,
                                          _ADC_CTRL_OFFSET,
                                          pAdcDevV30->infoData.offset,
                                          adcCtrl2Val);
            adcCtrl2Val = FLD_SET_DRF_NUM(_PTRIM_SYS,
                                          _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL_2,
                                          _ADC_CTRL_COARSE_OFFSET,
                                          pAdcDevV30->infoData.coarseOffset,
                                          adcCtrl2Val);
            adcCtrl2Val = FLD_SET_DRF_NUM(_PTRIM_SYS,
                                          _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL_2,
                                          _ADC_CTRL_COARSE_GAIN,
                                          pAdcDevV30->infoData.coarseGain,
                                          adcCtrl2Val);
#endif

            REG_WR32(FECS, adcCtrl2Reg, adcCtrl2Val);
        }
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
