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
 * @file   pmu_clkadcga100.c
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
#include "lib_lwf32.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Set or clear ADC code
 *
 * @param[in]   pAdcDev         Pointer to ADC device
 * @param[in]   overrideMode    SW override mode of operation.
 * @param[in]   overrideCode    SW override ADC code.
 *
 * @return FLCN_OK                  Operation completed successfully
 * @return FLCN_ERR_ILWALID_INDEX   Invalid ADC map index
 */
FLCN_STATUS
clkAdcCodeOverrideSet_GA100
(
    CLK_ADC_DEVICE *pAdcDev,
    LwU8            overrideCode,
    LwU8            overrideMode
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        devIdx;

    // Program all NAFLL driven by this ADC
    FOR_EACH_INDEX_IN_MASK(32, devIdx, pAdcDev->nafllsSharedMask)
    {
        CLK_NAFLL_DEVICE *pNafllDev = clkNafllDeviceGet(devIdx);
        if (pNafllDev == NULL)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_ILWALID_STATE;
        }

        status = clkNafllAdcMuxOverrideSet_HAL(&Clk, pNafllDev, overrideCode, overrideMode);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkAdcCodeOverrideSet_GA100_exit;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

clkAdcCodeOverrideSet_GA100_exit:
    return status;
}

/*!
 * @brief   Gets the instantaneous ADC code value from the ADC_MONITOR register
 *
 * @param[in]   pAdcDev     Pointer to ADC device
 * @param[in]   pInstCode   Pointer to the instantaneous code value
 *
 * @return FLCN_OK
 * @note The new CDC scheme allows to synchronize ADC_DOUT without requiring SW
 *       request a capture.
 */
FLCN_STATUS
clkAdcInstCodeGet_GA100
(
    CLK_ADC_DEVICE *pAdcDev,
    LwU8            *pInstCode
)
{
    LwU32 reg32;
    LwU32 data32;

    reg32 = CLK_ADC_REG_GET(pAdcDev, MONITOR);

    // Read back the ADC_DOUT for the CODE value
    data32 = REG_RD32(FECS, reg32);
    *pInstCode = DRF_VAL(_PTRIM_SYS, _NAFLL_SYSNAFLL_LWVDD_ADC_MONITOR,
                         _ADC_DOUT, data32);

    return FLCN_OK;
}

/*!
 * @copydoc clkAdcCalProgram()
 */
FLCN_STATUS
clkAdcCalProgram_GA100
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
#endif

            REG_WR32(FECS, adcCtrl2Reg, adcCtrl2Val);
        }
    }

    return FLCN_OK;
}

/*!
 * @brief Get bitmask of all supported ADCs on this chip
 *
 * @param[out]   pMask       Pointer to the bitmask
 */
void
clkAdcAllMaskGet_GA100
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
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC6)|
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPC7)|
             BIT32(LW2080_CTRL_CLK_ADC_ID_GPCS);
}

/* ------------------------- Private Functions ------------------------------ */
