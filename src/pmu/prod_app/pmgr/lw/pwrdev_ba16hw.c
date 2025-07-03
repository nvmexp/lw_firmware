/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrdev_ba16hw.c
 * @brief   Block Activity v1.6 HW sensor/controller.
 *
 * @implements PWR_DEVICE
 *
 * @note    Only supports/implements single PWR_DEVICE Provider - index 0.
 *          Provider index parameter will be ignored in all interfaces.
 *
 * @note    Is a subset of BA1XHW device, and used for energy alwmmulation alone.
 *
 * @note    For information on BA1XHW device see RM::pwrdev_ba1xhw.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_graphics_nobundle.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmgr/pwrdev_ba1xhw.h"
#include "pmu_objpmgr.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define LUT_VOLTAGE_STEP_SIZE_UV      \
     (BIT32(LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_ADC_OUTPUT_WIDTH - LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_LUT_INDEX_WIDTH) \
     * LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL_STEP_SIZE_UV)

#define MCLK_FREQUENCY_COUNT          4

// To change after receiving (V, F) tuples from SSG Team.
#define BA16HW_FBVDD_DEFAULT_VOLTAGE  1350000

//
// The value that the LW_PGRAPH_PRI_GPCS_TPCS_SM_QUAD_BA_CONTROL_INDEX field
// should have so that (gpcsTpcsBaData)[12] in Devinit may be modified by
// writing to the _DATA field in LW_PGRAPH_PRI_GPCS_TPCS_SM_QUAD_BA_DATA
// register.
//
#define BUG_200734888_BA_CONTROL_INDEX1            0xc

//
// The new value that the LW_PGRAPH_PRI_GPCS_TPCS_SM_QUAD_BA_DATA_DATA field
// should have in order to patch (gpcsTpcsBaData)[12] in Devinit.
//
#define BUG_200734888_BA_DATA_NEW_VALUE1           0x014701F3

//
// The value that the LW_PGRAPH_PRI_GPCS_TPCS_SM_QUAD_BA_CONTROL_INDEX field
// should have so that (gpcsTpcsBaData)[15] in Devinit may be modified by
// writing to the _DATA field in LW_PGRAPH_PRI_GPCS_TPCS_SM_QUAD_BA_DATA
// register.
//
#define BUG_200734888_BA_CONTROL_INDEX2            0xf

//
// The new value that the LW_PGRAPH_PRI_GPCS_TPCS_SM_QUAD_BA_DATA_DATA field
// should have in order to patch (gpcsTpcsBaData)[15] in Devinit.
//
#define BUG_200734888_BA_DATA_NEW_VALUE2           0x01100340

/* ------------------------- Type Definitions ------------------------------- */
/*
 * This structure is used to store FBVDD rail voltage corresponding to each
 * MCLK Frequency.
 */
typedef struct
{
    LwU32   mClkFreqkHz;
    LwU32   fbvddVoltuV;
} PMGR_MCLK_FREQ_TO_FBVDD_VOLTAGE;

// Values to change based on details provided by SSG Team.
PMGR_MCLK_FREQ_TO_FBVDD_VOLTAGE  mClkFreqkHzToFBVDDVoltuV[MCLK_FREQUENCY_COUNT]
    GCC_ATTRIB_SECTION("dmem_pmgr", "mClkFreqkHzToFBVDDVoltuV") = {{405000, 1250000}, {810000, 1250000}, {6000000, 1250000}, {7000000, 1350000}};
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
static void s_pwrDevBa16HwLimitThresholdSet(PWR_DEVICE_BA16HW *pBa16Hw, LwU8 limitIdx, LwU32 limitValue)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pwrDevBa16HwLimitThresholdSet");

static LwU32 s_pwrDevBa16ComputeFactorA(PWR_EQUATION *pScalingEqu, LwU8 shiftA, LwBool bLwrrent, LwU32 voltageuV)
    GCC_ATTRIB_SECTION("imem_pmgr", "s_pwrDevBa16ComputeFactorA");

static void s_pwrDevBa16InitFactorALut(PWR_DEVICE_BA16HW *pBa16Hw, LwBool bFactorALutEnableLW, LwBool bFactorALutEnableMS)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pwrDevBa16InitFactorALut");

static FLCN_STATUS s_pwrDevBa16GetReading(PWR_DEVICE_BA16HW *pBa16Hw, LwU8 provIdx, LwU32 *pReading)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "s_pwrDevBa16GetReading");
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */
/*!
 * Construct a BA16HW PWR_DEVICE object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrDevGrpIfaceModel10ObjSetImpl_BA16HW
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    PWR_DEVICE_BA16HW                               *pBa16Hw;
    RM_PMU_PMGR_PWR_DEVICE_DESC_BA16HW_BOARDOBJ_SET *pBa16HwSet =
        (RM_PMU_PMGR_PWR_DEVICE_DESC_BA16HW_BOARDOBJ_SET *)pBoardObjDesc;
    FLCN_STATUS                                      status = FLCN_OK;

    //
    // Lwrrently we dont support MS and LW power domains on the same
    // instance of BA16 @ref pwrDevTupleGet_BA16HW
    //
    if (pBa16HwSet->bMonitorLW && pBa16HwSet->bMonitorMS)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto pwrDevGrpIfaceModel10ObjSetImpl_BA16HW_done;
    }

    if (pBa16HwSet->bLeakageCLutEnableMS || pBa16HwSet->bLeakageCLutEnableLW)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto pwrDevGrpIfaceModel10ObjSetImpl_BA16HW_done;
    }
    // Construct and initialize parent-class object.
    status = pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW(pModel10, ppBoardObj,
        size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto pwrDevGrpIfaceModel10ObjSetImpl_BA16HW_done;
    }

    pBa16Hw = (PWR_DEVICE_BA16HW *)*ppBoardObj;

    // Populate member variables here
    pBa16Hw->bMonitorLW           = pBa16HwSet->bMonitorLW;
    pBa16Hw->bMonitorMS           = pBa16HwSet->bMonitorMS;
    pBa16Hw->gpuAdcDevIdx         = pBa16HwSet->gpuAdcDevIdx;
    pBa16Hw->bFactorALutEnableLW  = pBa16HwSet->bFactorALutEnableLW;
    pBa16Hw->bLeakageCLutEnableLW = pBa16HwSet->bLeakageCLutEnableLW;
    pBa16Hw->bFactorALutEnableMS  = pBa16HwSet->bFactorALutEnableMS;
    pBa16Hw->bLeakageCLutEnableMS = pBa16HwSet->bLeakageCLutEnableMS;
    pBa16Hw->bFBVDDMode           = pBa16HwSet->bFBVDDMode;
    pBa16Hw->super.configuration  = 0;

    if (pBa16HwSet->scalingEquIdxMS ==
        LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID)
    {
        pBa16Hw->pScalingEquMS = NULL;
    }
    else
    {
        pBa16Hw->pScalingEquMS =
            PWR_EQUATION_GET(pBa16HwSet->scalingEquIdxMS);
        if (pBa16Hw->pScalingEquMS == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            goto pwrDevGrpIfaceModel10ObjSetImpl_BA16HW_done;
        }
    }

    if (pBa16HwSet->leakageEquIdxMS ==
        LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID)
    {
        pBa16Hw->pLeakageEquMS = NULL;
    }
    else
    {
        pBa16Hw->pLeakageEquMS =
            PWR_EQUATION_GET(pBa16HwSet->leakageEquIdxMS);
        if (pBa16Hw->pLeakageEquMS == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            goto pwrDevGrpIfaceModel10ObjSetImpl_BA16HW_done;
        }
    }

    if (pBa16Hw->bMonitorLW)
    {
        pBa16Hw->super.configuration |= DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
            _BA_SRC_LWVDD,  _ENABLED);
    }
    if (pBa16Hw->bMonitorMS)
    {
        pBa16Hw->super.configuration |= DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
            _BA_SRC_MSVDD, _ENABLED);
    }
    if (pBa16HwSet->bIsT1ModeADC)
    {
        pBa16Hw->super.configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                    _BA_T1_HW_ADC_MODE, _ADC);
    }
    if (pBa16HwSet->bIsT2ModeADC)
    {
        pBa16Hw->super.configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                    _BA_T2_HW_ADC_MODE, _ADC);
    }
    if ((pBa16HwSet->winSize <=
         LW_CPWR_THERM_PEAKPOWER_CONFIG1_BA_WINDOW_SIZE_MAX) ||
        (pBa16HwSet->winSize >=
         LW_CPWR_THERM_PEAKPOWER_CONFIG1_BA_WINDOW_SIZE_MIN))
    {
        pBa16Hw->super.configuration |=
            DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_WINDOW_SIZE,
            pBa16HwSet->winSize);
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto pwrDevGrpIfaceModel10ObjSetImpl_BA16HW_done;
    }
    if (pBa16HwSet->stepSize <=
        LW_CPWR_THERM_PEAKPOWER_CONFIG1_BA_STEP_SIZE_MAX)
    {
        pBa16Hw->super.configuration |=
            DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_STEP_SIZE,
            pBa16HwSet->stepSize);
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto pwrDevGrpIfaceModel10ObjSetImpl_BA16HW_done;
    }
    if (pBa16HwSet->bFactorALutEnableLW)
    {
        pBa16Hw->super.configuration |=
        DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                _LWVDD_FACTOR_A_LUT_EN, _ENABLED);
    }
    if (pBa16HwSet->bFactorALutEnableMS)
    {
        pBa16Hw->super.configuration |=
        DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                _MSVDD_FACTOR_A_LUT_EN, _ENABLED);
    }
    if (pBa16HwSet->bLeakageCLutEnableLW)
    {
        pBa16Hw->super.configuration |=
        DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                _LWVDD_LEAKAGE_C_LUT_EN, _ENABLED);
    }
    if (pBa16HwSet->bLeakageCLutEnableMS)
    {
        pBa16Hw->super.configuration |=
        DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                _MSVDD_LEAKAGE_C_LUT_EN, _ENABLED);
    }
    if (pBa16HwSet->adcSelLW ==
        LW2080_CTRL_PMGR_PWR_DEVICE_BA_16HW_LUT_ADC_SEL_LWVDD_GPC_ADC_MIN)
    {
        pBa16Hw->super.configuration |=
        DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                _LUT_ADC_SEL_LWVDD, _GPC_ADC_MIN);
    }
    if (pBa16HwSet->adcSelLW ==
        LW2080_CTRL_PMGR_PWR_DEVICE_BA_16HW_LUT_ADC_SEL_LWVDD_GPC_ADC_AVG)
    {
        pBa16Hw->super.configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                    _LUT_ADC_SEL_LWVDD, _GPC_ADC_AVG);
    }
    if (pBa16HwSet->adcSelMS ==
        LW2080_CTRL_PMGR_PWR_DEVICE_BA_16HW_LUT_ADC_SEL_MSVDD_GPC_ADC_MIN)
    {
        pBa16Hw->super.configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                    _LUT_ADC_SEL_MSVDD, _GPC_ADC_MIN);
    }
    if (pBa16HwSet->adcSelMS ==
        LW2080_CTRL_PMGR_PWR_DEVICE_BA_16HW_LUT_ADC_SEL_MSVDD_GPC_ADC_AVG)
    {
        pBa16Hw->super.configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                    _LUT_ADC_SEL_MSVDD, _GPC_ADC_AVG);
    }
    if (pBa16HwSet->pwrDomainGPC ==
        LW2080_CTRL_PMGR_PWR_DEVICE_BA_16HW_POWER_DOMAIN_GPC_MSVDD)
    {
        pBa16Hw->super.configuration |=
             DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                     _POWER_DOMAIN_GPC , _MSVDD);
    }
    else
    {
        pBa16Hw->super.configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                    _POWER_DOMAIN_GPC , _LWVDD);
    }
    if (pBa16HwSet->pwrDomainXBAR ==
        LW2080_CTRL_PMGR_PWR_DEVICE_BA_16HW_POWER_DOMAIN_XBAR_MSVDD)
    {
        pBa16Hw->super.configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                    _POWER_DOMAIN_XBAR , _MSVDD);
    }
    else
    {
        pBa16Hw->super.configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                    _POWER_DOMAIN_XBAR , _LWVDD);
    }
    if (pBa16HwSet->pwrDomainFBP ==
        LW2080_CTRL_PMGR_PWR_DEVICE_BA_16HW_POWER_DOMAIN_FBP_MSVDD)
    {
        pBa16Hw->super.configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                    _POWER_DOMAIN_FBP , _MSVDD);
    }
    else
    {
        pBa16Hw->super.configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                    _POWER_DOMAIN_FBP , _LWVDD);
    }

    if ((pBa16HwSet->super.c0Significand != 0) ||
        (pBa16HwSet->super.c1Significand != 0) ||
        (pBa16HwSet->super.c2Significand != 0))
    {
        // Check DBA Period is withing range.
        if (pBa16HwSet->super.dbaPeriod <=
            LW_CPWR_THERM_PEAKPOWER_CONFIG12_DBA_PERIOD_MAX)
        {
            pBa16Hw->super.configuration |=
                DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_PEAK_EN, _ENABLED);
        }
    }

    pBa16Hw->super.configuration |=
        DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _WINDOW_RST, _TRIGGER) |
        DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _WINDOW_EN, _ENABLED);

    if ((pBa16HwSet->super.c0Significand != 0) ||
        (pBa16HwSet->super.c1Significand != 0) ||
        (pBa16HwSet->super.c2Significand != 0))
    {
        // Check DBA Period is withing range.
        if (pBa16HwSet->super.dbaPeriod <=
            LW_CPWR_THERM_PEAKPOWER_CONFIG12_DBA_PERIOD_MAX)
        {
            // Compute CONFIG11 & CONFIG12 registers for EDPp.
            pBa16Hw->super.edpPConfigC0C1 = FLD_SET_DRF_NUM(_CPWR_THERM,
                _PEAKPOWER_CONFIG11, _C0_SIGNIFICAND,
                pBa16HwSet->super.c0Significand, pBa16Hw->super.edpPConfigC0C1);
            pBa16Hw->super.edpPConfigC0C1 = FLD_SET_DRF_NUM(_CPWR_THERM,
                _PEAKPOWER_CONFIG11, _C0_RSHIFT,
                pBa16HwSet->super.c0RShift, pBa16Hw->super.edpPConfigC0C1);
            pBa16Hw->super.edpPConfigC0C1 = FLD_SET_DRF_NUM(_CPWR_THERM,
                _PEAKPOWER_CONFIG11, _C1_SIGNIFICAND,
                pBa16HwSet->super.c1Significand, pBa16Hw->super.edpPConfigC0C1);
            pBa16Hw->super.edpPConfigC0C1 = FLD_SET_DRF_NUM(_CPWR_THERM,
                _PEAKPOWER_CONFIG11, _C1_RSHIFT, pBa16HwSet->super.c1RShift,
                pBa16Hw->super.edpPConfigC0C1);

            pBa16Hw->super.edpPConfigC2Dba = FLD_SET_DRF_NUM(_CPWR_THERM,
                _PEAKPOWER_CONFIG12, _C2_SIGNIFICAND,
                pBa16HwSet->super.c2Significand, pBa16Hw->super.edpPConfigC2Dba);
            pBa16Hw->super.edpPConfigC2Dba = FLD_SET_DRF_NUM(_CPWR_THERM,
                _PEAKPOWER_CONFIG12, _C2_RSHIFT, pBa16HwSet->super.c2RShift,
                pBa16Hw->super.edpPConfigC2Dba);
            pBa16Hw->super.edpPConfigC2Dba = FLD_SET_DRF_NUM(_CPWR_THERM,
                _PEAKPOWER_CONFIG12, _DBA_PERIOD, pBa16HwSet->super.dbaPeriod,
                pBa16Hw->super.edpPConfigC2Dba);
        }
        else
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto pwrDevGrpIfaceModel10ObjSetImpl_BA16HW_done;
        }
    }

pwrDevGrpIfaceModel10ObjSetImpl_BA16HW_done:
    return status;
}

/*!
 * BA16HW power device implementation.
 *
 * @copydoc PwrDevLoad
 */
FLCN_STATUS
pwrDevLoad_BA16HW
(
    PWR_DEVICE *pDev
)
{
    PWR_DEVICE_BA16HW  *pBa16Hw = (PWR_DEVICE_BA16HW *)pDev;
    LwU8                limitIdx;

    if (pBa16Hw->bFBVDDMode)
    {
        // Program Factor A values in FBVDD Mode.
        LwU32 regBa16PeakPowerConfig6 =
            REG_RD32(CSB,
                LW_CPWR_THERM_PEAKPOWER_CONFIG6(pBa16Hw->super.windowIdx));
        regBa16PeakPowerConfig6 =
            FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG6, _LWVDD_FACTOR_A,
                _FBVDD_MODE, regBa16PeakPowerConfig6);
        REG_WR32(CSB,
            LW_CPWR_THERM_PEAKPOWER_CONFIG6(pBa16Hw->super.windowIdx),
                regBa16PeakPowerConfig6);

        LwU32 regBa16PeakPowerConfig8 =
            REG_RD32(CSB,
                LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa16Hw->super.windowIdx));
        regBa16PeakPowerConfig8 =
            FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG8, _MSVDD_FACTOR_A,
                _FBVDD_MODE, regBa16PeakPowerConfig8);
        REG_WR32(CSB,
            LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa16Hw->super.windowIdx),
                regBa16PeakPowerConfig8);

        // Set shiftA factors for LWVDD and MSVDD to 0 in FBVDD Mode.
        pBa16Hw->super.shiftA = 0;
        pBa16Hw->shiftAMS     = 0;
    }
    else
    {
        // Cache "shiftA" since it will not change.
        pBa16Hw->super.shiftA =
                pwrEquationBA15ScaleComputeShiftA(
                    (PWR_EQUATION_BA15_SCALE *)pBa16Hw->super.pScalingEqu,
                    pBa16Hw->super.bLwrrent);
        pBa16Hw->shiftAMS =
                pwrEquationBA15ScaleComputeShiftA(
                    (PWR_EQUATION_BA15_SCALE *)pBa16Hw->pScalingEquMS,
                    pBa16Hw->super.bLwrrent);
    }

    // Initialize FACTOR C to 0
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG7(pBa16Hw->super.windowIdx), 0);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(pBa16Hw->super.windowIdx), 0);

    // Reset limit thresholds on load
    for (limitIdx = 0; limitIdx < PWR_DEVICE_BA1XHW_LIMIT_COUNT; limitIdx++)
    {
        s_pwrDevBa16HwLimitThresholdSet(pBa16Hw, limitIdx, PWR_DEVICE_NO_LIMIT);
    }

    s_pwrDevBa16InitFactorALut(pBa16Hw, pBa16Hw->bFactorALutEnableLW,
        pBa16Hw->bFactorALutEnableMS);

    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(pBa16Hw->super.windowIdx),
             pBa16Hw->super.configuration);
    //
    // Since for Maxwell A and C have moved to the beginning and
    // BA is being scaled by shiftA we need to compensate for that
    // by right-shifting the scaled product of A*BA by the same
    // amount. (For LWVDD and MSVVDD rails)
    //
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG6(pBa16Hw->super.windowIdx),
        FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG6, _LWVDD_BA_SUM_SHIFT,
        pBa16Hw->super.shiftA,
        REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG6(pBa16Hw->super.windowIdx))));

    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa16Hw->super.windowIdx),
        FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG8, _MSVDD_BA_SUM_SHIFT,
        pBa16Hw->shiftAMS,
        REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa16Hw->super.windowIdx))));

    //
    // Enable BA logic
    // TODO - Do this at PWR_DEVICES scope
    //
    REG_WR32(CSB, LW_CPWR_THERM_CONFIG1,
        FLD_SET_DRF(_CPWR_THERM, _CONFIG1, _BA_ENABLE, _YES,
        REG_RD32(CSB, LW_CPWR_THERM_CONFIG1)));

    // Setting up CONFIG11/12 for EDPp Support
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG11(pBa16Hw->super.windowIdx),
                         pBa16Hw->super.edpPConfigC0C1);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG12(pBa16Hw->super.windowIdx),
                         pBa16Hw->super.edpPConfigC2Dba);

    return FLCN_OK;
}

/*!
 * @brief   Update BA power device's internal state (A&C)
 *
 * This interface is designed to be called in following two situations:
 * - on each change of GPU core voltage (both A & C parameters require update)
 * - periodically to compensate for temperature changes (C parameter only)
 *
 *  PMU formulas look like:
 * - current mode:
 *   A(hw) = (constA * core_Voltage) << shiftA
 *   C(hw) = leakage(mA)
 * - power mode:
 *   A(hw) = (constA * core_Voltage^2) << shiftA
 *   C(hw) = leakage(mW)
 *
 * @param[in]   pDev            Pointer to a PWR_DEVICE object
 */
FLCN_STATUS
pwrDevStateSync_BA16HW
(
    PWR_DEVICE *pDev
)
{
    PWR_DEVICE_BA16HW  *pBa16Hw = (PWR_DEVICE_BA16HW *)pDev;
    LwBool              bVoltageChangedLW = LW_FALSE;
    LwBool              bVoltageChangedMS = LW_FALSE;
    LwU32               leakagemX;
    LwU32               voltageuV;
    LwU8                voltRailIdx;
    VOLT_RAIL          *pRail;
    LwU32               factorA;
    LwU32               regVal;
    FLCN_STATUS         status        = FLCN_OK;
    OSTASK_OVL_DESC     ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLeakage)
    };

    //
    // Return immediately since Factor A and Factor C values do not change
    // dynamically in FBVDD Mode.
    //
    if (pBa16Hw->bFBVDDMode)
    {
        return status;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (pBa16Hw->bMonitorLW)
        {
            voltRailIdx = voltRailVoltDomainColwertToIdx(
                              LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC);
            if (voltRailIdx == LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto pwrDevBaStateSync_BA16_done;
            }

            pRail  = VOLT_RAIL_GET(voltRailIdx);
            if (pRail == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_STATE;
                goto pwrDevBaStateSync_BA16_done;
            }

            //
            // Get Voltage and compare it with stored voltage to get the idea that
            // voltage has been changed or not.
            //
            status = voltRailGetVoltage(pRail, &voltageuV);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrDevBaStateSync_BA16_done;
            }

            if (pBa16Hw->super.voltageuV != voltageuV)
            {
                pBa16Hw->super.voltageuV = voltageuV;
                bVoltageChangedLW = LW_TRUE;
            }

            // Update factor C
            if (pBa16Hw->super.pLeakageEqu != NULL)
            {
                //
                // Offset parameter "C" is always updated
                // Callwlate leakage power for given voltage.
                //
                status = pwrEquationGetLeakage(pBa16Hw->super.pLeakageEqu,
                             pBa16Hw->super.voltageuV, pBa16Hw->super.bLwrrent,
                             LW_TYPES_FXP_ZERO, &leakagemX);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pwrDevBaStateSync_BA16_done;
                }

                // Update offset "C" in HW
                REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG7(pBa16Hw->super.windowIdx),
                     DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG7,
                     _LWVDD_LEAKAGE_C, leakagemX));
            }
            //
            // Scaling parameter "A" is updated only on voltage change
            //
            if (bVoltageChangedLW &&
                !pBa16Hw->bFactorALutEnableLW)
            {
                factorA = s_pwrDevBa16ComputeFactorA(pBa16Hw->super.pScalingEqu,
                          pBa16Hw->super.shiftA, pBa16Hw->super.bLwrrent,
                          pBa16Hw->super.voltageuV);
                regVal  = REG_RD32(CSB,
                    LW_CPWR_THERM_PEAKPOWER_CONFIG6(pBa16Hw->super.windowIdx));
                regVal  = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG6,
                          _LWVDD_FACTOR_A, factorA, regVal);

                //Update factor "A" in HW
                REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG6(
                    pBa16Hw->super.windowIdx), regVal);
            }
        }
        if (pBa16Hw->bMonitorMS)
        {
            voltRailIdx = voltRailVoltDomainColwertToIdx(
                              LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD);
            if (voltRailIdx == LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto pwrDevBaStateSync_BA16_done;
            }

            pRail  = VOLT_RAIL_GET(voltRailIdx);
            if (pRail == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_STATE;
                goto pwrDevBaStateSync_BA16_done;
            }

            //
            // Get Voltage and compare it with stored voltage to get the idea that
            // voltage has been changed or not.
            //
            status = voltRailGetVoltage(pRail, &voltageuV);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrDevBaStateSync_BA16_done;
            }

            if (pBa16Hw->voltageMSuV != voltageuV)
            {
                pBa16Hw->voltageMSuV = voltageuV;
                bVoltageChangedMS = LW_TRUE;
            }

            // Update factor C
            if (pBa16Hw->pLeakageEquMS != NULL)
            {
                //
                // Offset parameter "C" is always updated
                // Callwlate leakage power for given voltage.
                //
                status = pwrEquationGetLeakage(pBa16Hw->pLeakageEquMS,
                             pBa16Hw->voltageMSuV, pBa16Hw->super.bLwrrent,
                             LW_TYPES_FXP_ZERO, &leakagemX);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pwrDevBaStateSync_BA16_done;
                }

                // Update offset "C" in HW
                REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(pBa16Hw->super.windowIdx),
                    DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG9,
                    _MSVDD_LEAKAGE_C, leakagemX));
            }
            if (bVoltageChangedMS &&
                !pBa16Hw->bFactorALutEnableMS)
            {
                factorA = s_pwrDevBa16ComputeFactorA(pBa16Hw->pScalingEquMS,
                          pBa16Hw->shiftAMS, pBa16Hw->super.bLwrrent,
                          pBa16Hw->voltageMSuV);
                regVal  = REG_RD32(CSB,
                          LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa16Hw->super.windowIdx));
                regVal  = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG8,
                          _MSVDD_FACTOR_A, factorA, regVal);

                // Update factor "A" in HW for MSVDD
                REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa16Hw->super.windowIdx), regVal);
            }
        }

pwrDevBaStateSync_BA16_done:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * BA16HW power device implementation.
 *
 * @copydoc PwrDevTupleGet
 */
FLCN_STATUS
pwrDevTupleGet_BA16HW
(
    PWR_DEVICE                 *pDev,
    LwU8                        provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    FLCN_STATUS                status          = FLCN_OK;
    CLK_DOMAIN                *pDomain         = NULL;
    CLK_DOMAIN_PROG           *pDomainProg     = NULL;
    LW2080_CTRL_CLK_VF_INPUT   input;
    LW2080_CTRL_CLK_VF_OUTPUT  output;
    LwBool                     bFbvddDataValid = LW_FALSE;
    LwU32                      i;
    LwU32                      voltagemV;
    LwU32                      clkDomainIdx;
    LwU32                      latestMCLKFreqkHz;
    PWR_DEVICE_BA16HW         *pBa16Hw         = (PWR_DEVICE_BA16HW *)pDev;

    if (pBa16Hw->bMonitorLW && pBa16Hw->bMonitorMS)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }

    if (provIdx >= RM_PMU_PMGR_PWR_DEVICE_BA1X_PROV_NUM)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    //
    // If BA is operating in FBVDD Mode, fetch latest MCLK Frequency and
    // lookup FBVDD rail voltage corresponding to it.
    // Otherwise, both LW and MS cannot be enabled at the same time.
    // Default fallback to LWVDD voltage
    //
    if (pBa16Hw->bFBVDDMode)
    {
        // Obtain clock domain index for MCLK.
        PMU_ASSERT_OK_OR_GOTO(status,
            clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_MCLK, &clkDomainIdx),
            pwrDevTupleGet_BA16HW_exit);

        // Fetch latest MCLK Frequency.
        latestMCLKFreqkHz = perfChangeSeqChangeLastClkFreqkHzGet(clkDomainIdx);

        // Obtain clock domain for MCLK.
        pDomain = CLK_DOMAIN_GET(clkDomainIdx);
        if (pDomain == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto pwrDevTupleGet_BA16HW_exit;
        }

        pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
        if (pDomainProg == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto pwrDevTupleGet_BA16HW_exit;
        }

        //
        // Check if FBVDD data is checked in to VBIOS.
        // If yes, ilwoke the CLK API to retrieve the FBVDD voltage,
        // otherwise fallback to hardcoded table here in PMU.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            clkDomainProgFbvddDataValid(pDomainProg, &bFbvddDataValid),
            pwrDevTupleGet_BA16HW_exit);

        if (bFbvddDataValid)
        {
            // Initialize the input VF tuple.
            LW2080_CTRL_CLK_VF_INPUT_INIT(&input);

            // Set parameters for the input VF tuple having the MCLK Frequency as its value.
            input.flags = (LwU8)DRF_DEF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES);
            input.value = latestMCLKFreqkHz;

            // Initialize the output VF tuple.
            LW2080_CTRL_CLK_VF_OUTPUT_INIT(&output);

            // Obtain the FBVDD voltage in uV corresponding to the current MCLK Frequency.
            PMU_ASSERT_OK_OR_GOTO(status,
                clkDomainProgFbvddFreqToVolt(pDomainProg, &input, &output),
                pwrDevTupleGet_BA16HW_exit);

            // Sanity check the inputBestMatch value in the output VF tuple.
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (output.inputBestMatch != LW2080_CTRL_CLK_VF_VALUE_ILWALID),
                FLCN_ERR_ILWALID_STATE,
                pwrDevTupleGet_BA16HW_exit);

            pTuple->voltuV = output.value;
        }
        else
        {
            //
            // Set pTuple->voltuV to the Highest FBVDD Voltage. Also acts as a
            // Fallback option if requested frequency is greater than hardcoded
            // values.
            //
            pTuple->voltuV = BA16HW_FBVDD_DEFAULT_VOLTAGE;

            // Do Linear Search over the Tuple Array.
            for (i = 0; i < MCLK_FREQUENCY_COUNT; i++)
            {
                if (mClkFreqkHzToFBVDDVoltuV[i].mClkFreqkHz >= latestMCLKFreqkHz)
                {
                    pTuple->voltuV = mClkFreqkHzToFBVDDVoltuV[i].fbvddVoltuV;
                    break;
                }
            }
        }
    }
    else if (pBa16Hw->bMonitorMS)
    {
        pTuple->voltuV = pBa16Hw->voltageMSuV;
    }
    else
    {
        pTuple->voltuV = pBa16Hw->super.voltageuV;
    }

    voltagemV = LW_UNSIGNED_ROUNDED_DIV(pTuple->voltuV, 1000);

    if (pBa16Hw->super.bLwrrent)
    {
        // Get Current
        status = s_pwrDevBa16GetReading(pBa16Hw, provIdx, &(pTuple->lwrrmA));
        if (status != FLCN_OK)
        {
            return status;
        }

        // Callwlate Power
        pTuple->pwrmW = voltagemV * (pTuple->lwrrmA) / 1000;

        // Memory Vendor specific adjustment.
        if (pBa16Hw->bFBVDDMode && bFbvddDataValid)
        {
            // Adjust Power value based on Memory Vendor specific slope and intercept.
            PMU_ASSERT_OK_OR_GOTO(status,
                clkDomainProgFbvddPwrAdjust(
                    pDomainProg,
                    &(pTuple->pwrmW),
                    LW_TRUE),
                pwrDevTupleGet_BA16HW_exit);

            // Recompute Current value since BA is operating in Current mode.
            pTuple->lwrrmA = LW_UNSIGNED_ROUNDED_DIV(((pTuple->pwrmW) * 1000), voltagemV);
        }
    }
    else
    {
        // Get Power
        status = s_pwrDevBa16GetReading(pBa16Hw, provIdx, &(pTuple->pwrmW));
        if (status != FLCN_OK)
        {
            return status;
        }

        // Memory Vendor specific adjustment.
        if (pBa16Hw->bFBVDDMode && bFbvddDataValid)
        {
            // Adjust Power value based on Memory Vendor specific slope and intercept.
            PMU_ASSERT_OK_OR_GOTO(status,
                clkDomainProgFbvddPwrAdjust(
                    pDomainProg,
                    &(pTuple->pwrmW),
                    LW_TRUE),
                pwrDevTupleGet_BA16HW_exit);
        }

        // Callwlate current
        pTuple->lwrrmA = LW_UNSIGNED_ROUNDED_DIV(((pTuple->pwrmW) * 1000), voltagemV);
    }

pwrDevTupleGet_BA16HW_exit:
    return status;
}

/*!
 * BA16HW power device implementation.
 *
 * @copydoc PwrDevSetLimit
 */
FLCN_STATUS
pwrDevSetLimit_BA16HW
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU8        limitIdx,
    LwU8        limitUnit,
    LwU32       limitValue
)
{
    FLCN_STATUS status         = FLCN_ERR_NOT_SUPPORTED;;
    PWR_DEVICE_BA16HW *pBa16Hw = (PWR_DEVICE_BA16HW *)pDev;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_GPUADC1X) &&
        (limitIdx >= PWR_DEVICE_BA1XHW_LIMIT_COUNT) &&
        (limitIdx < RM_PMU_PMGR_PWR_DEVICE_GPUADC1X_EXTENDED_THRESHOLD_NUM))
    {
        PWR_DEVICE_GPUADC1X *pGpuAdc1x;

        // Sanity check GPUADC device index
        pGpuAdc1x = (PWR_DEVICE_GPUADC1X *)PWR_DEVICE_GET(pBa16Hw->gpuAdcDevIdx);
        if (pGpuAdc1x == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto pwrDevSetLimit_BA16HW_exit;
        }

        limitIdx -= PWR_DEVICE_BA1XHW_LIMIT_COUNT;

        // The GPUADC1X interface will call the GPUADC type-specific function
        status = pwrDevGpuAdc1xBaSetLimit(pGpuAdc1x, pBa16Hw->super.windowIdx,
                     limitIdx, limitUnit, limitValue);
    }
    else if (limitIdx < PWR_DEVICE_BA1XHW_LIMIT_COUNT)
    {
        if (((limitUnit == LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA) &&
             (pBa16Hw->super.bLwrrent)) ||
           ((limitUnit == LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW) &&
             (!pBa16Hw->super.bLwrrent)))
        {
             s_pwrDevBa16HwLimitThresholdSet(pBa16Hw, limitIdx, limitValue);
             status = FLCN_OK;
        }
    }
    else
    {
        // limitIdx is outside expected range
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
    }

pwrDevSetLimit_BA16HW_exit:
    return status;
}

/*!
 * Function to patch BA weights based on Bug 200734888
 */
FLCN_STATUS
pwrDevPatchBaWeightsBug200734888_BA16HW(void)
{
    // Patch (gpcsTpcsBaData)[12] in Devinit.
    REG_WR32(BAR0, LW_PGRAPH_PRI_GPCS_TPCS_SM_QUAD_BA_CONTROL,
        BUG_200734888_BA_CONTROL_INDEX1);
    REG_WR32(BAR0, LW_PGRAPH_PRI_GPCS_TPCS_SM_QUAD_BA_DATA,
        BUG_200734888_BA_DATA_NEW_VALUE1);

    // Patch (gpcsTpcsBaData)[15] in Devinit.
    REG_WR32(BAR0, LW_PGRAPH_PRI_GPCS_TPCS_SM_QUAD_BA_CONTROL,
        BUG_200734888_BA_CONTROL_INDEX2);
    REG_WR32(BAR0, LW_PGRAPH_PRI_GPCS_TPCS_SM_QUAD_BA_DATA,
        BUG_200734888_BA_DATA_NEW_VALUE2);

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Read power/current from BA power device
 *
 * @param[in]   pDev        Pointer to a PWR_DEVICE_BA16HW object
 * @param[in]   provIdx     Index of the requested provider
 * @param[in]   pReading    Buffer to store retrieved power/current reading
 *
 * @return  FLCN_OK                 Reading succesfully retrieved
 * @return  FLCN_ERR_NOT_SUPPORTED   Invalid provider index
 */
static FLCN_STATUS
s_pwrDevBa16GetReading
(
    PWR_DEVICE_BA16HW *pBa16Hw,
    LwU8               provIdx,
    LwU32             *pReading
)
{
    FLCN_STATUS         status  = FLCN_ERR_NOT_SUPPORTED;
    LwU32               totalmX;

    totalmX =
        DRF_VAL(_CPWR_THERM, _PEAKPOWER_CONFIG10, _WIN_SUM_VALUE,
            REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG10(pBa16Hw->super.windowIdx)));

    // TODO: Hack exposing current estimations through power interface
    if ((provIdx == RM_PMU_PMGR_PWR_DEVICE_BA1X_PROV_TOTAL_POWER) ||
        (provIdx == RM_PMU_PMGR_PWR_DEVICE_BA1X_PROV_TOTAL_LWRRENT))
    {
        *pReading = totalmX;
        status = FLCN_OK;
    }
    // TODO: Hack exposing current estimations through power interface
    else if ((provIdx == RM_PMU_PMGR_PWR_DEVICE_BA1X_PROV_DYNAMIC_POWER) ||
             (provIdx == RM_PMU_PMGR_PWR_DEVICE_BA1X_PROV_DYNAMIC_LWRRENT))
    {
        if (pBa16Hw->bMonitorMS)
        {
            totalmX -=
                DRF_VAL(_CPWR_THERM, _PEAKPOWER_CONFIG9, _MSVDD_LEAKAGE_C,
                REG_RD32(CSB,
                    LW_CPWR_THERM_PEAKPOWER_CONFIG9(pBa16Hw->super.windowIdx)));
        }
        // Default to LWVDD
        else
        {
            totalmX -=
                DRF_VAL(_CPWR_THERM, _PEAKPOWER_CONFIG7, _LWVDD_LEAKAGE_C,
                REG_RD32(CSB,
                    LW_CPWR_THERM_PEAKPOWER_CONFIG7(pBa16Hw->super.windowIdx)));
        }
        *pReading = totalmX;
        status = FLCN_OK;
    }

    return status;
}

/*!
 * @brief Compute FACTOR_A for a give voltage
 *
 * @param[in]  pScalingEqu   pointer to scaling equation PWR_EQUATION
 * @param[in]  shiftA        LwU32 parameter used in computations of
 *                           scaling A, offset C and thresholds
 * @param[in]  bLwrrent      boolean signifying current/power
 * @param[in]  voltageuV     The voltage to compute FACTOR_A at, in microvolts (uV)
 *
 * @returns FACTOR_A value
 *
 * @note TODO - Refactor this to unify with @ref pmgrComputeFactorA()
 */
static LwU32
s_pwrDevBa16ComputeFactorA
(
    PWR_EQUATION   *pScalingEqu,
    LwU8            shiftA,
    LwBool          bLwrrent,
    LwU32           voltageuV
)
{
    LwUFXP20_12 scaleA;

    // compute what the new scaling factor should be, called scaleA
    scaleA = pwrEquationBA15ScaleComputeScaleA(
                (PWR_EQUATION_BA15_SCALE *)pScalingEqu,
                bLwrrent,
                voltageuV);

    //
    // we want BA sum to be multiplied by scaleA. However, HW breaks the multiplication
    // by scaleA into a multiply by factorA, and a bit-shift by shiftA. shiftA is computed
    // at boot, verified to be a right shift, and held constant afterwards. we left shift here
    // to counteract the right shift that HW will apply
    //
    scaleA <<= shiftA;
    return LW_TYPES_UFXP_X_Y_TO_U32(20, 12, scaleA);
}

/*!
 * @brief    Setup HW realtime FACTOR_A control
 *
 * @param[in]   pDev      Pointer to a PWR_DEVICE object
 */
void
static s_pwrDevBa16InitFactorALut
(
    PWR_DEVICE_BA16HW *pBa16Hw,
    LwBool             bFactorALutEnableLW,
    LwBool             bFactorALutEnableMS
)
{
    LwU8               lutIdx;
    LwU32              regVal = 0;
    LwU32              factorA;
    LwU32              voltageuV;

    ct_assert(LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_ADC_OUTPUT_WIDTH >
              LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_LUT_INDEX_WIDTH);
    ct_assert(LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL_STEP_SIZE_UV > 0);

    if (!bFactorALutEnableLW && !bFactorALutEnableMS)
    {
        // Nothing to do
        return;
    }

    // populate FACTOR_A LUT
    regVal = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG14, _LUT_WRITE_EN,
                         _WRITE, regVal);

    for (lutIdx = 0; lutIdx < LW_CPWR_THERM_PEAKPOWER_CONFIG14_LUT_INDEX__SIZE_1; lutIdx++)
    {
        //
        // the factor A LUT maps voltage readouts from the HW ADC to factor A values.
        // each LUT entry holds the factor A value valid for equal-sized range of voltages.
        // HW expects each factor A LUT entry to be computed against the lower bound of
        // the voltage range that the LUT entry handles. therefore, the voltage that
        // an entry should compute factor A against =
        //         <lowest handled voltage> + (<voltage range size of each entry> * <entry indx>)
        //
        voltageuV = LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL_ZERO_UV +
                    (lutIdx * LUT_VOLTAGE_STEP_SIZE_UV);
        regVal = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG14, _LUT_INDEX, lutIdx, regVal);
        if (bFactorALutEnableLW)
        {
            factorA = s_pwrDevBa16ComputeFactorA(pBa16Hw->super.pScalingEqu,
                pBa16Hw->super.shiftA, pBa16Hw->super.bLwrrent, voltageuV);
            regVal = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG14, _LUT_SEL,
                         _LWVDD_FACTOR_A, regVal);
            regVal = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG14,
                                     _LUT_DATA,  factorA, regVal);
            REG_WR32(CSB,
                LW_CPWR_THERM_PEAKPOWER_CONFIG14(pBa16Hw->super.windowIdx),
                regVal);
        }
        if (bFactorALutEnableMS)
        {
            factorA = s_pwrDevBa16ComputeFactorA(pBa16Hw->pScalingEquMS,
                pBa16Hw->shiftAMS, pBa16Hw->super.bLwrrent, voltageuV);
            regVal = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG14, _LUT_SEL,
                         _MSVDD_FACTOR_A, regVal);
            regVal = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG14,
                                     _LUT_DATA,  factorA, regVal);
            REG_WR32(CSB,
                LW_CPWR_THERM_PEAKPOWER_CONFIG14(pBa16Hw->super.windowIdx),
                regVal);
        }
    }
}

/*!
 * @brief   Update BA power device limit's threshold
 *
 * @param[in]   pDev        Pointer to a PWR_DEVICE object
 * @param[in]   limitIdx    Index of the limit threshold
 * @param[in]   limitValue  Limit value to apply
 */
void
s_pwrDevBa16HwLimitThresholdSet
(
    PWR_DEVICE_BA16HW *pBa16Hw,
    LwU8               limitIdx,
    LwU32              limitValue
)
{
    LwU32  address = 0;
    LwU32  reg32;

    if (limitValue == PWR_DEVICE_NO_LIMIT)
    {
        reg32 = DRF_DEF(_CPWR_THERM, _BA1XHW_THRESHOLD, _EN, _DISABLED);
    }
    else
    {
        reg32 = DRF_NUM(_CPWR_THERM, _BA1XHW_THRESHOLD, _VAL, limitValue);
        reg32 =
            FLD_SET_DRF(_CPWR_THERM, _BA1XHW_THRESHOLD, _EN, _ENABLED, reg32);
    }

    switch (limitIdx)
    {
        case LW_CPWR_THERM_BA1XHW_THRESHOLD_1H:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG2(pBa16Hw->super.windowIdx);
            break;
        case LW_CPWR_THERM_BA1XHW_THRESHOLD_2H:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG3(pBa16Hw->super.windowIdx);
            break;
        case LW_CPWR_THERM_BA1XHW_THRESHOLD_1L:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG4(pBa16Hw->super.windowIdx);
            break;
        case LW_CPWR_THERM_BA1XHW_THRESHOLD_2L:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG5(pBa16Hw->super.windowIdx);
            break;
        default:
            PMU_HALT();
            break;
    }

    REG_WR32(CSB, address, reg32);
}
