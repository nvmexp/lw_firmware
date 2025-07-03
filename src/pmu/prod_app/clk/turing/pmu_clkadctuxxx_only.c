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
 * @file   pmu_clkadctuxxx_only.c
 * @brief  Hal functions for ADC code in TU10X only. Part of AVFS
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
static FLCN_STATUS s_clkAdcCalOffsetProgram_V20(CLK_ADC_DEVICE *pAdcDev, LwS32 offset)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "s_clkAdcCalOffsetProgram_V20");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Program the coarse control for calibration version type V2.0 for the given ADC device
 *
 * @param[in]   pAdcDev     Pointer to the ADC device
 * @param[in]   pCalV20     Pointer to the calibration version 2.0 struct
 * @param[in]   bEnable     Flag to indicate enable/disable operation
 * @param[in]   adcCtrlReg  Register address for ADC_CTRL for given ADC
 *
 * @return  FLCN_OK
 */
FLCN_STATUS
clkAdcCalProgramCoarseControl_TU10X
(
    CLK_ADC_DEVICE              *pAdcDev,
    LW2080_CTRL_CLK_ADC_CAL_V20 *pCalV20,
    LwBool                       bEnable,
    LwU32                        adcCtrlReg
)
{
    if (bEnable)
    {
        LwU32 adcCtrl = REG_RD32(FECS, adcCtrlReg);
        adcCtrl = FLD_SET_DRF_NUM(_PTRIM_SYS,
                                  _NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
                                  _SETUP_COARSE_OFFSET_CALIBRATION,
                                  pCalV20->coarseControl,
                                  adcCtrl);

        REG_WR32(FECS, adcCtrlReg, adcCtrl);
    }

    return FLCN_OK;
}

/*!
 * @brief   Program the calibration offset for the given ADC device to
 *          compensate for high temperature variation.
 * @note    Bug Number: 200423771
 *
 * @param[in]   pAdcDev     Pointer to the ADC device
 *
 * @return FLCN_OK                On success or nothing changed
 * @return FLCN_ERR_NOT_SUPPORTED If the calibration type is anything other
 *                                than @ref LW2080_CTRL_CLK_ADC_CAL_TYPE_V20
 * @return other                  Descriptive error code from sub-routines on
 *                                failure
 */
FLCN_STATUS
clkAdcCalOffsetProgram_TU10X
(
    CLK_ADC_DEVICE              *pAdcDev
)
{
    CLK_ADC_DEVICE_V20 *pAdcDevV20  = (CLK_ADC_DEVICE_V20 *)pAdcDev;
    FLCN_STATUS         status      = FLCN_OK;

    // program only if calibration type is V20 else fail
    if ((pAdcDev->super.type == LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V20) &&
        (pAdcDevV20->data.calType == LW2080_CTRL_CLK_ADC_CAL_TYPE_V20))
    {
        // return early if offset value is invalid
        if (pAdcDevV20->statusData.offset == LW_S32_MAX)
        {
            return status;
        }

        status = s_clkAdcCalOffsetProgram_V20(pAdcDev, pAdcDevV20->statusData.offset);
    }
    else
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Program the offset for calibration version type V2.0 for the given ADC device
 *
 * @param[in]   pAdcDev     Pointer to the ADC device
 * @param[in]   offset      Calibration offset
 *
 * @return  FLCN_OK                    On success
 * @return  FLCN_ERR_ILWALID_ARGUMENT  If ADC device id is invalid
 */
static FLCN_STATUS
s_clkAdcCalOffsetProgram_V20
(
    CLK_ADC_DEVICE *pAdcDev,
    LwS32           offset
)
{
    LwU32       adcCalRegAddr;
    LwU32       adcCalRegValue;
    LwS32       adcCode = offset;
    FLCN_STATUS status = FLCN_OK;

    //
    // Format output to [9] sign, [8:4] integer, [3:0] fraction
    // adcCode = ((adcCode & 0x0000003F) << 4);
    //
    adcCode = (LwS32)((adcCode &
                DRF_MASK(LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL_OFFSET_INTEGER)) << 
                        DRF_SIZE(LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL_OFFSET_FRACTION));

    // TODO: Replace below switch case with CLK_ADC_REG_GET_TU10X
    switch (pAdcDev->id)
    {
        case LW2080_CTRL_CLK_ADC_ID_SYS:
            adcCalRegAddr = LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL;
            break;
        case LW2080_CTRL_CLK_ADC_ID_GPC0:
        case LW2080_CTRL_CLK_ADC_ID_GPC1:
        case LW2080_CTRL_CLK_ADC_ID_GPC2:
        case LW2080_CTRL_CLK_ADC_ID_GPC3:
        case LW2080_CTRL_CLK_ADC_ID_GPC4:
        case LW2080_CTRL_CLK_ADC_ID_GPC5:
            adcCalRegAddr = LW_PTRIM_GPC_GPCNAFLL_ADC_CAL((LwU32)(pAdcDev->id - LW2080_CTRL_CLK_ADC_ID_GPC0));
            break;
        case LW2080_CTRL_CLK_ADC_ID_GPCS:
            adcCalRegAddr = LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CAL;
            break;
        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkAdcCalOffsetProgram_V20_exit;
    }

    // Program NAFLL_ADC_CAL
    adcCalRegValue = REG_RD32(FECS, adcCalRegAddr);
    adcCalRegValue = FLD_SET_DRF_NUM(_PTRIM, _SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL, _OFFSET, adcCode, adcCalRegValue);
    REG_WR32(FECS, adcCalRegAddr, adcCalRegValue);

s_clkAdcCalOffsetProgram_V20_exit:
    return status;
}
