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
 * @file    pwrdev_ba20.c
 * @brief   Block Activity v2.0 HW sensor/controller.
 *
 * @implements PWR_DEVICE
 *
 * @note    Only supports/implements single PWR_DEVICE Provider - index 0.
 *          Provider index parameter will be ignored in all interfaces.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_therm.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmgr/pwrdev_ba20.h"
#include "pmu_objpmgr.h"
#include "pmu_objfuse.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
//
// LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_ADC_OUTPUT_WIDTH = 7
// LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_LUT_INDEX_WIDTH = 4
// LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL_STEP_SIZE_UV = 6.25 mV
//
// Thus, the ADC is 7 bits wide, the LUT index is 4 bits wide, so 1 LUT step
// covers 8 ADC steps. Hence, we have 8 ADC steps / LUT step * 6.25mV / ADC step
// = 50 mV / LUT step.
//
#define LUT_VOLTAGE_STEP_SIZE_UV      \
     (BIT32(LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_ADC_OUTPUT_WIDTH - LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_LUT_INDEX_WIDTH) \
     * LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL_STEP_SIZE_UV)

// Unit Colwersion Factor used in BA20 power device tuple construction.
#define PWR_DEVICE_BA20_UNIT_COLWERSION_FACTOR               1000

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
static FLCN_STATUS s_pwrDevBa20SanityCheck(RM_PMU_PMGR_PWR_DEVICE_DESC_BA20_BOARDOBJ_SET *pBa20Set)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "s_pwrDevBa20SanityCheck");

static FLCN_STATUS s_pwrDevBa20LimitThresholdSet(PWR_DEVICE_BA20 *pBa20, LwU8 limitIdx, LwU32 limitValue)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pwrDevBa20LimitThresholdSet");

static FLCN_STATUS s_pwrDevBa20ComputeShiftA(PWR_DEVICE_BA20 *pBa20, PWR_DEVICE_BA20_VOLT_RAIL_DATA *pBa20VoltRailData,
                                                 LwU8 *pShiftA)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pwrDevBa20ComputeShiftA");

static FLCN_STATUS s_pwrDevBa20ComputeFactorA(PWR_DEVICE_BA20 *pBa20, PWR_DEVICE_BA20_VOLT_RAIL_DATA *pBa20VoltRailData,
                                                  LwU32 voltageuV, LwU32 *pFactorA)
    GCC_ATTRIB_SECTION("imem_pmgr", "s_pwrDevBa20ComputeFactorA");

static FLCN_STATUS s_pwrDevBa20InitFactorALut(PWR_DEVICE_BA20 *pBa20)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pwrDevBa20InitFactorALut");

static FLCN_STATUS s_pwrDevBa20UpdateFactorA(PWR_DEVICE_BA20 *pBa20, PWR_DEVICE_BA20_VOLT_RAIL_DATA *pBa20VoltRailData)
    GCC_ATTRIB_SECTION("imem_pmgrPwrDeviceStateSync", "s_pwrDevBa20UpdateFactorA");

static FLCN_STATUS s_pwrDevBa20UpdateFactorC(PWR_DEVICE_BA20 *pBa20, PWR_DEVICE_BA20_VOLT_RAIL_DATA *pBa20VoltRailData)
    GCC_ATTRIB_SECTION("imem_pmgrPwrDeviceStateSync", "s_pwrDevBa20UpdateFactorC");

static FLCN_STATUS s_pwrDevBa20GetReading(PWR_DEVICE_BA20 *pBa20, LwU8 provIdx, LwU32 *pReading)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "s_pwrDevBa20GetReading");

static FLCN_STATUS s_pwrDevBa20CompleteTupleValues(PWR_DEVICE_BA20 *pBa20, LW2080_CTRL_PMGR_PWR_TUPLE *pTuple)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "s_pwrDevBa20CompleteTupleValues");

/* ------------------------- PWR_DEVICE Interfaces -------------------------- */
/*!
 * Construct a BA20 PWR_DEVICE object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrDevGrpIfaceModel10ObjSetImpl_BA20
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    PWR_DEVICE_BA20                                    *pBa20;
    PWR_DEVICE_BA20_VOLT_RAIL_DATA                     *pBa20VoltRailData;
    VOLT_RAIL                                          *pRail;
    RM_PMU_PMGR_PWR_DEVICE_DESC_BA20_BOARDOBJ_SET      *pBa20Set =
        (RM_PMU_PMGR_PWR_DEVICE_DESC_BA20_BOARDOBJ_SET *)pBoardObjDesc;
    FLCN_STATUS                                         status   = FLCN_OK;
    LwU8                                                voltDomain;
    LwBoardObjIdx                                       i;

    // Construct and initialize parent-class object.
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrDevGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        pwrDevGrpIfaceModel10ObjSetImpl_BA20_done);

    pBa20 = (PWR_DEVICE_BA20 *)*ppBoardObj;

    // Sanity check parameters as received from RM.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrDevBa20SanityCheck(pBa20Set),
        pwrDevGrpIfaceModel10ObjSetImpl_BA20_done);

    // Populate member variables here as received from RM.
    pBa20->bLwrrent      = pBa20Set->bLwrrent;
    pBa20->windowIdx     = pBa20Set->windowIdx;
    pBa20->c0Significand = pBa20Set->c0Significand;
    pBa20->c0RShift      = pBa20Set->c0RShift;
    pBa20->c1Significand = pBa20Set->c1Significand;
    pBa20->c1RShift      = pBa20Set->c1RShift;
    pBa20->c2Significand = pBa20Set->c2Significand;
    pBa20->c2RShift      = pBa20Set->c2RShift;
    pBa20->dbaPeriod     = pBa20Set->dbaPeriod;
    pBa20->bIsT1ModeADC  = pBa20Set->bIsT1ModeADC;
    pBa20->bIsT2ModeADC  = pBa20Set->bIsT2ModeADC;
    pBa20->winSize       = pBa20Set->winSize;
    pBa20->stepSize      = pBa20Set->stepSize;
    pBa20->gpuAdcDevIdx  = pBa20Set->gpuAdcDevIdx;
    pBa20->pwrDomainGPC  = pBa20Set->pwrDomainGPC;
    pBa20->pwrDomainXBAR = pBa20Set->pwrDomainXBAR;
    pBa20->pwrDomainFBP  = pBa20Set->pwrDomainFBP;
    pBa20->scaleFactor   = pBa20Set->scaleFactor;
    pBa20->configuration = 0;

   //
   // Initialize the boolean denoting whether the BA20 power sensor is
   // operating in FBVDD mode.
   //
    pwrDevBa20FBVDDModeIsActiveSet(pBa20, pBa20Set->bFBVDDMode);

    // Construct volt rail-specific information.
    PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_BEGIN(pBa20, &pBa20VoltRailData,
        (VOLT_RAIL **)NULL, i, LW_TRUE, status, pwrDevGrpIfaceModel10ObjSetImpl_BA20_done)
    {
        // Copy in the data as sent by RM.
        pBa20VoltRailData->super = pBa20Set->voltRailData[i];

        // Check that the voltRailIdx has a valid value or the allowed invalid value.
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (VOLT_RAIL_INDEX_IS_VALID(pBa20VoltRailData->super.voltRailIdx) ||
                (pBa20VoltRailData->super.voltRailIdx ==
                    LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)),
            FLCN_ERR_ILWALID_STATE,
            pwrDevGrpIfaceModel10ObjSetImpl_BA20_done);

        // Skip handling of the current config if the voltRailIdx is invalid.
        if (pBa20VoltRailData->super.voltRailIdx ==
            LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)
        {
            continue;
        }

        // Obtain the voltage rail object pointer.
        PWR_DEVICE_BA20_VOLT_RAIL_FROM_VOLT_RAIL_DATA_GET(pBa20,
            pBa20VoltRailData, &pRail, status,
            pwrDevGrpIfaceModel10ObjSetImpl_BA20_done);

        // Obtain the voltage domain.
        voltDomain = voltRailDomainGet(pRail);
        if (voltDomain == LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC)
        {
            pBa20->configuration |= DRF_DEF(_CPWR_THERM,
                _PEAKPOWER_CONFIG1, _BA_SRC_LWVDD,  _ENABLED);

            if (pBa20VoltRailData->super.bFactorALutEnable)
            {
                pBa20->configuration |= DRF_DEF(_CPWR_THERM,
                    _PEAKPOWER_CONFIG1, _LWVDD_FACTOR_A_LUT_EN, _ENABLED);
            }

            if (pBa20VoltRailData->super.adcSel ==
                LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_LUT_ADC_SEL_GPC_ADC_MIN)
            {
                pBa20->configuration |= DRF_DEF(_CPWR_THERM,
                    _PEAKPOWER_CONFIG1, _LUT_ADC_SEL_LWVDD, _GPC_ADC_MIN);
            }
            else if (pBa20VoltRailData->super.adcSel ==
                LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_LUT_ADC_SEL_GPC_ADC_AVG)
            {
                pBa20->configuration |= DRF_DEF(_CPWR_THERM,
                    _PEAKPOWER_CONFIG1, _LUT_ADC_SEL_LWVDD, _GPC_ADC_AVG);
            }
        }
        else if (voltDomain == LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD)
        {
            pBa20->configuration |= DRF_DEF(_CPWR_THERM,
                _PEAKPOWER_CONFIG1, _BA_SRC_MSVDD,  _ENABLED);

            if (pBa20VoltRailData->super.bFactorALutEnable)
            {
                pBa20->configuration |= DRF_DEF(_CPWR_THERM,
                    _PEAKPOWER_CONFIG1, _MSVDD_FACTOR_A_LUT_EN, _ENABLED);
            }

            if (pBa20VoltRailData->super.adcSel ==
                LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_LUT_ADC_SEL_GPC_ADC_MIN)
            {
                pBa20->configuration |= DRF_DEF(_CPWR_THERM,
                    _PEAKPOWER_CONFIG1, _LUT_ADC_SEL_MSVDD, _GPC_ADC_MIN);
            }
            else if (pBa20VoltRailData->super.adcSel ==
                LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_LUT_ADC_SEL_GPC_ADC_AVG)
            {
                pBa20->configuration |= DRF_DEF(_CPWR_THERM,
                    _PEAKPOWER_CONFIG1, _LUT_ADC_SEL_MSVDD, _GPC_ADC_AVG);
            }
        }
        else
        {
            // Any other voltage domain is not allowed.
            PMU_ASSERT_OK_OR_GOTO(status,
                FLCN_ERR_ILWALID_STATE,
                pwrDevGrpIfaceModel10ObjSetImpl_BA20_done);
        }
    }
    PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_END;

    if (pBa20->bIsT1ModeADC)
    {
        pBa20->configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                _BA_T1_HW_ADC_MODE, _ADC);
    }
    if (pBa20->bIsT2ModeADC)
    {
        pBa20->configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                _BA_T2_HW_ADC_MODE, _ADC);
    }

    pBa20->configuration |=
        DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_WINDOW_SIZE,
            pBa20->winSize);

    pBa20->configuration |=
        DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_STEP_SIZE,
            pBa20->stepSize);

    if (pBa20->pwrDomainGPC ==
        LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_POWER_DOMAIN_GPC_MSVDD)
    {
        pBa20->configuration |=
             DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                     _POWER_DOMAIN_GPC , _MSVDD);
    }
    else
    {
        pBa20->configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                    _POWER_DOMAIN_GPC , _LWVDD);
    }

    if (pBa20->pwrDomainXBAR ==
        LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_POWER_DOMAIN_XBAR_MSVDD)
    {
        pBa20->configuration |=
             DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                     _POWER_DOMAIN_XBAR , _MSVDD);
    }
    else
    {
        pBa20->configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                    _POWER_DOMAIN_XBAR , _LWVDD);
    }

    if (pBa20->pwrDomainFBP ==
        LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_POWER_DOMAIN_FBP_MSVDD)
    {
        pBa20->configuration |=
             DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                     _POWER_DOMAIN_FBP , _MSVDD);
    }
    else
    {
        pBa20->configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1,
                    _POWER_DOMAIN_FBP , _LWVDD);
    }

    if ((pBa20->c0Significand != 0) ||
        (pBa20->c1Significand != 0) ||
        (pBa20->c2Significand != 0))
    {
        pBa20->configuration |=
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _BA_PEAK_EN, _ENABLED);
    }

    pBa20->configuration |=
        DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _WINDOW_RST, _TRIGGER) |
        DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG1, _WINDOW_EN, _ENABLED);

    if ((pBa20->c0Significand != 0) ||
        (pBa20->c1Significand != 0) ||
        (pBa20->c2Significand != 0))
    {
        // Compute CONFIG11 registers for EDPp.
        pBa20->edpPConfigC0C1 = FLD_SET_DRF_NUM(_CPWR_THERM,
            _PEAKPOWER_CONFIG11, _C0_SIGNIFICAND,
            pBa20->c0Significand, pBa20->edpPConfigC0C1);
        pBa20->edpPConfigC0C1 = FLD_SET_DRF_NUM(_CPWR_THERM,
            _PEAKPOWER_CONFIG11, _C0_RSHIFT,
            pBa20->c0RShift, pBa20->edpPConfigC0C1);
        pBa20->edpPConfigC0C1 = FLD_SET_DRF_NUM(_CPWR_THERM,
            _PEAKPOWER_CONFIG11, _C1_SIGNIFICAND,
            pBa20->c1Significand, pBa20->edpPConfigC0C1);
        pBa20->edpPConfigC0C1 = FLD_SET_DRF_NUM(_CPWR_THERM,
            _PEAKPOWER_CONFIG11, _C1_RSHIFT, pBa20->c1RShift,
            pBa20->edpPConfigC0C1);

        // Compute CONFIG12 registers for EDPp.
        pBa20->edpPConfigC2Dba = FLD_SET_DRF_NUM(_CPWR_THERM,
            _PEAKPOWER_CONFIG12, _C2_SIGNIFICAND,
            pBa20->c2Significand, pBa20->edpPConfigC2Dba);
        pBa20->edpPConfigC2Dba = FLD_SET_DRF_NUM(_CPWR_THERM,
            _PEAKPOWER_CONFIG12, _C2_RSHIFT, pBa20->c2RShift,
            pBa20->edpPConfigC2Dba);
        pBa20->edpPConfigC2Dba = FLD_SET_DRF_NUM(_CPWR_THERM,
            _PEAKPOWER_CONFIG12, _DBA_PERIOD, pBa20->dbaPeriod,
            pBa20->edpPConfigC2Dba);
    }

    // Cache the chip-specific TPC data.
    PMU_ASSERT_OK_OR_GOTO(status,
        fuseNumTpcGet_HAL(&Fuse, &(pBa20->chipConfig.numTpc)),
        pwrDevGrpIfaceModel10ObjSetImpl_BA20_done);

    // Cache the chip-specific FBPA data.
    PMU_ASSERT_OK_OR_GOTO(status,
        fuseNumFbpaGet_HAL(&Fuse, &(pBa20->chipConfig.numFbpa)),
        pwrDevGrpIfaceModel10ObjSetImpl_BA20_done);

    // Cache the chip-specific MXBAR data.
    PMU_ASSERT_OK_OR_GOTO(status,
        fuseNumMxbarGet_HAL(&Fuse, &(pBa20->chipConfig.numMxbar)),
        pwrDevGrpIfaceModel10ObjSetImpl_BA20_done);

    // Sanity check to ensure that the TPC, FBPA and MXBAR counts are non-zero.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pBa20->chipConfig.numTpc != 0) &&
         (pBa20->chipConfig.numFbpa != 0) &&
         (pBa20->chipConfig.numMxbar != 0)),
        FLCN_ERR_ILWALID_STATE,
        pwrDevGrpIfaceModel10ObjSetImpl_BA20_done);

pwrDevGrpIfaceModel10ObjSetImpl_BA20_done:
    return status;
}

/*!
 * BA20 power device implementation.
 *
 * @copydoc PwrDevLoad
 */
FLCN_STATUS
pwrDevLoad_BA20
(
    PWR_DEVICE *pDev
)
{
    PWR_DEVICE_BA20                *pBa20  = (PWR_DEVICE_BA20 *)pDev;
    PWR_DEVICE_BA20_VOLT_RAIL_DATA *pBa20VoltRailData;
    VOLT_RAIL                      *pRail;
    LwBool                          bFBVDDModeIsActive;
    LwU8                            limitIdx;
    FLCN_STATUS                     status = FLCN_OK;
    LwU8                            voltDomain;
    LwBoardObjIdx                   i;

    //
    // Obtain the boolean denoting whether the BA20 power sensor is operating
    // in FBVDD mode.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrDevBa20FBVDDModeIsActiveGet(pBa20, &bFBVDDModeIsActive),
        pwrDevLoad_BA20_done);

    //
    // Cache "shiftA" values since they will not change.
    // Since for Maxwell A and C have moved to the beginning and
    // BA is being scaled by shiftA we need to compensate for that
    // by right-shifting the scaled product of A*BA by the same
    // amount. (For LWVDD and MSVVDD rails)
    //
    PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_BEGIN(pBa20, &pBa20VoltRailData,
        &pRail, i, LW_FALSE, status, pwrDevLoad_BA20_done)
    {
        // Compute the shiftA value.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrDevBa20ComputeShiftA(pBa20, pBa20VoltRailData,
                &(pBa20VoltRailData->shiftA)),
            pwrDevLoad_BA20_done);

        // Obtain the voltage domain.
        voltDomain = voltRailDomainGet(pRail);
        if (voltDomain == LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC)
        {
            REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG6(pBa20->windowIdx),
                FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG6,
                _LWVDD_BA_SUM_SHIFT, pBa20VoltRailData->shiftA,
                REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG6(pBa20->windowIdx))));
        }
        else if (voltDomain == LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD)
        {
            REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa20->windowIdx),
                FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG8,
                _MSVDD_BA_SUM_SHIFT, pBa20VoltRailData->shiftA,
                REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa20->windowIdx))));
        }
        else
        {
            // Any other voltage domain is not allowed.
            PMU_ASSERT_OK_OR_GOTO(status,
                FLCN_ERR_ILWALID_STATE,
                pwrDevLoad_BA20_done);
        }
    }
    PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_END;

    // Initialize FACTOR C to 0
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG7(pBa20->windowIdx), 0);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(pBa20->windowIdx), 0);

    // Reset limit thresholds on load
    for (limitIdx = 0; limitIdx < PWR_DEVICE_BA20_LIMIT_COUNT; limitIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrDevBa20LimitThresholdSet(pBa20, limitIdx, PWR_DEVICE_NO_LIMIT),
            pwrDevLoad_BA20_done);
    }

    if (bFBVDDModeIsActive)
    {
        // Update Factor A values as per FBVDD mode specification.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrDevBa20UpdateFactorA(pBa20, pBa20VoltRailData),
            pwrDevLoad_BA20_done);
    }
    else
    {
        // Setup Factor A LUT.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrDevBa20InitFactorALut(pBa20),
            pwrDevLoad_BA20_done);
    }

    //
    // Program the number of active TPC instances on the chip. This has
    // already been programmed during devinit but re-programming is required
    // since the number of active TPC instances can reduce via regkey.
    //
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_INTERCEPT_0,
        FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_INTERCEPT_0, _TPC_INST_CNT,
            pBa20->chipConfig.numTpc,
        REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_INTERCEPT_0)));

    //
    // Program the number of active FBPA instances on the chip. This has
    // already been programmed during devinit but re-programming is required
    // since the number of active FB partitions can reduce via regkey.
    //
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_INTERCEPT_1,
        FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_INTERCEPT_1, _FBPA_INST_CNT,
            pBa20->chipConfig.numFbpa,
        REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_INTERCEPT_1)));

    //
    // Program the number of active MXBAR instances on the chip. This has
    // already been programmed during devinit but re-programming is required
    // since the number of active MXBAR instances can reduce via regkey.
    //
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_INTERCEPT_2,
        FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_INTERCEPT_2, _MXBAR_INST_CNT,
            pBa20->chipConfig.numMxbar,
        REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_INTERCEPT_2)));

    // Trigger BA Idle Power Computation.
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_INTERCEPT_CTRL,
        FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_INTERCEPT_CTRL, _UPDATE, _TRIGGER,
        REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_INTERCEPT_CTRL)));

    //
    // Configure the current BA instance by writing into the
    // LW_CPWR_THERM_PEAKPOWER_CONFIG1 register.
    //
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(pBa20->windowIdx),
             pBa20->configuration);

    // Enable Global BA logic.
    REG_WR32(CSB, LW_CPWR_THERM_CONFIG1,
        FLD_SET_DRF(_CPWR_THERM, _CONFIG1, _BA_ENABLE, _YES,
        REG_RD32(CSB, LW_CPWR_THERM_CONFIG1)));

pwrDevLoad_BA20_done:
    return status;
}

/*!
 * @brief   Update BA20 power device's internal state (Factor A and Factor C)
 *
 * This interface is designed to be called in following two situations:
 * - on each change of GPU core voltage (both Factor A & Factor C require update)
 * - periodically to compensate for temperature changes (Factor C only)
 *
 *  PMU formulas look like:
 * - current mode:
 *   A(hw) = (constA * core_Voltage) << shiftA
 *   C(hw) = leakage(mA)
 * - power mode:
 *   A(hw) = (constA * core_Voltage^2) << shiftA
 *   C(hw) = leakage(mW)
 */
FLCN_STATUS
pwrDevStateSync_BA20
(
    PWR_DEVICE *pDev
)
{
    PWR_DEVICE_BA20                *pBa20           = (PWR_DEVICE_BA20 *)pDev;
    PWR_DEVICE_BA20_VOLT_RAIL_DATA *pBa20VoltRailData;
    VOLT_RAIL                      *pRail;
    LwBool                          bFBVDDModeIsActive;
    LwBool                          bVoltageChanged = LW_FALSE;
    LwU32                           voltageuV;
    LwBoardObjIdx                   i;
    FLCN_STATUS                     status          = FLCN_OK;

    //
    // Obtain the boolean denoting whether the BA20 power sensor is operating
    // in FBVDD mode.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrDevBa20FBVDDModeIsActiveGet(pBa20, &bFBVDDModeIsActive),
        pwrDevStateSync_BA20_done);

    //
    // If the BA20 power sensor is operating in FBVDD mode, return immediately
    // since Factor A and Factor C values do not change dynamically in FBVDD
    // Mode.
    //
    if (bFBVDDModeIsActive)
    {
        goto pwrDevStateSync_BA20_done;
    }

    PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_BEGIN(pBa20, &pBa20VoltRailData,
        &pRail, i, LW_FALSE, status, pwrDevStateSync_BA20_done)
    {
        // Obtain the voltage for the current voltage rail.
        PMU_ASSERT_OK_OR_GOTO(status,
            voltRailGetVoltage(pRail, &voltageuV),
            pwrDevStateSync_BA20_done);

        //
        // Update if the voltage for the current voltage rail has
        // changed.
        //
        if (pBa20VoltRailData->voltageuV != voltageuV)
        {
            pBa20VoltRailData->voltageuV = voltageuV;
            bVoltageChanged = LW_TRUE;
        }

        //
        // Update Factor A in HW if Factor A LUT is disabled and the
        // voltage for the current voltage rail has changed.
        //
        if (!pBa20VoltRailData->super.bFactorALutEnable && bVoltageChanged)
        {
            // Obtain the Factor A value.
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrDevBa20UpdateFactorA(pBa20, pBa20VoltRailData),
                pwrDevStateSync_BA20_done);
        }

        // Update Factor C in HW.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrDevBa20UpdateFactorC(pBa20, pBa20VoltRailData),
            pwrDevStateSync_BA20_done);
    }
    PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_END;

pwrDevStateSync_BA20_done:
    return status;
}

/*!
 * BA20 power device implementation.
 *
 * @copydoc PwrDevTupleGet
 */
FLCN_STATUS
pwrDevTupleGet_BA20
(
    PWR_DEVICE                 *pDev,
    LwU8                        provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    PWR_DEVICE_BA20 *pBa20  = (PWR_DEVICE_BA20 *)pDev;
    FLCN_STATUS      status = FLCN_OK;

    // Check whether the provider index is supported.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (provIdx < LW2080_CTRL_PMGR_PWR_DEVICE_BA_2X_PROV_NUM),
        FLCN_ERR_ILWALID_STATE,
        pwrDevTupleGet_BA20_done);

    if (pBa20->bLwrrent)
    {
        // Get Current.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrDevBa20GetReading(pBa20, provIdx, &(pTuple->lwrrmA)),
            pwrDevTupleGet_BA20_done);
    }
    else
    {
        // Get Power.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrDevBa20GetReading(pBa20, provIdx, &(pTuple->pwrmW)),
            pwrDevTupleGet_BA20_done);
    }

    // Complete the Tuple values.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrDevBa20CompleteTupleValues(pBa20, pTuple),
        pwrDevTupleGet_BA20_done);

pwrDevTupleGet_BA20_done:
    return status;
}

/*!
 * BA20 power device implementation.
 *
 * @copydoc PwrDevSetLimit
 */
FLCN_STATUS
pwrDevSetLimit_BA20
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU8        limitIdx,
    LwU8        limitUnit,
    LwU32       limitValue
)
{
    FLCN_STATUS      status = FLCN_OK;
    PWR_DEVICE_BA20 *pBa20  = (PWR_DEVICE_BA20 *)pDev;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_GPUADC1X) &&
        (limitIdx >= PWR_DEVICE_BA20_LIMIT_COUNT) &&
        (limitIdx < RM_PMU_PMGR_PWR_DEVICE_GPUADC1X_EXTENDED_THRESHOLD_NUM))
    {
        PWR_DEVICE_GPUADC1X *pGpuAdc1x;

        // Obtain the GPUADC device index.
        pGpuAdc1x = (PWR_DEVICE_GPUADC1X *)PWR_DEVICE_GET(pBa20->gpuAdcDevIdx);

        // Sanity check.
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pGpuAdc1x != NULL),
            FLCN_ERR_ILWALID_STATE,
            pwrDevSetLimit_BA20_done);

        limitIdx -= PWR_DEVICE_BA20_LIMIT_COUNT;

        // The GPUADC1X interface will call the GPUADC type-specific function
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrDevGpuAdc1xBaSetLimit(pGpuAdc1x, pBa20->windowIdx,
                limitIdx, limitUnit, limitValue),
            pwrDevSetLimit_BA20_done);
    }
    else if (limitIdx < PWR_DEVICE_BA20_LIMIT_COUNT)
    {
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (((limitUnit == LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA) &&
             (pBa20->bLwrrent)) ||
           ((limitUnit == LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW) &&
             (!pBa20->bLwrrent))),
            FLCN_ERR_NOT_SUPPORTED,
            pwrDevSetLimit_BA20_done);

        // Set the limits for BA power device.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrDevBa20LimitThresholdSet(pBa20, limitIdx, limitValue),
            pwrDevSetLimit_BA20_done);
    }
    else
    {
        // limitIdx is outside expected range.
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_ILWALID_STATE,
            pwrDevSetLimit_BA20_done);
    }

pwrDevSetLimit_BA20_done:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief  Sanity check BA20 parameters received from RM.
 *
 * @param[in]   pBa20Set  Pointer to @ref RM_PMU_PMGR_PWR_DEVICE_DESC_BA20_BOARDOBJ_SET
 */
static FLCN_STATUS
s_pwrDevBa20SanityCheck
(
    RM_PMU_PMGR_PWR_DEVICE_DESC_BA20_BOARDOBJ_SET *pBa20Set
)
{
    LwU8        i;
    FLCN_STATUS status = FLCN_OK;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pBa20Set != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrDevBa20SanityCheck_done);

    // Check if the BA Window Index is within range.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pBa20Set->windowIdx < LW_THERM_PEAKPOWER_CONFIG1__SIZE_1),
        FLCN_ERR_ILWALID_STATE,
        s_pwrDevBa20SanityCheck_done);

    // Check if the GPUADC Device Index is valid only if GPUADC1X is enabled.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pBa20Set->gpuAdcDevIdx ==
              LW2080_CTRL_PMGR_PWR_DEVICE_INDEX_ILWALID) ||
         (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_GPUADC1X))),
        FLCN_ERR_ILWALID_STATE,
        s_pwrDevBa20SanityCheck_done);

    // Check if the BA Window Size is within range.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pBa20Set->winSize >=
              LW_CPWR_THERM_PEAKPOWER_CONFIG1_BA_WINDOW_SIZE_MIN) &&
         (pBa20Set->winSize <=
              LW_CPWR_THERM_PEAKPOWER_CONFIG1_BA_WINDOW_SIZE_MAX)),
        FLCN_ERR_ILWALID_STATE,
        s_pwrDevBa20SanityCheck_done);

    // Check if the BA Step Size is within range.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pBa20Set->stepSize <=
             LW_CPWR_THERM_PEAKPOWER_CONFIG1_BA_STEP_SIZE_MAX),
        FLCN_ERR_ILWALID_STATE,
        s_pwrDevBa20SanityCheck_done);

    // Check if the DBA Period is withing range.
    if ((pBa20Set->c0Significand != 0) ||
        (pBa20Set->c1Significand != 0) ||
        (pBa20Set->c2Significand != 0))
    {
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pBa20Set->dbaPeriod <=
                LW_CPWR_THERM_PEAKPOWER_CONFIG12_DBA_PERIOD_MAX),
            FLCN_ERR_ILWALID_STATE,
            s_pwrDevBa20SanityCheck_done);
    }


    for (i = 0; i < LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_MAX_CONFIGS; i++)
    {
        //
        // Check if Factor C LUT is disabled. Lwrrently, we don't support LUT
        // for Factor C.
        //
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (!(pBa20Set->voltRailData[i].bLeakageCLutEnable)),
            FLCN_ERR_ILWALID_STATE,
            s_pwrDevBa20SanityCheck_done);

        //
        // Ensure that the FBVDD mode VBIOS flag is set only if the
        // corresponding PMU feature is enabled.
        //
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (!(pBa20Set->bFBVDDMode) ||
              (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA20_FBVDD_MODE))),
            FLCN_ERR_ILWALID_STATE,
            s_pwrDevBa20SanityCheck_done);

        //
        // Ensure that Factor A LUT is disabled if the BA20 power sensor is
        // operating in FBVDD mode.
        //
        if (pBa20Set->bFBVDDMode)
        {
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (!(pBa20Set->voltRailData[i].bFactorALutEnable)),
                FLCN_ERR_ILWALID_STATE,
                s_pwrDevBa20SanityCheck_done);
        }
    }

s_pwrDevBa20SanityCheck_done:
    return status;
}

/*!
 * @brief  Complete Power Tuple values for BA20 power device after one
 *         of the Tuple metrics (current / power) has been computed.
 *
 * @param[in]        pBa20  Pointer to @ref PWR_DEVICE_BA20
 * @param[in, out]   pTuple Pointer to @ref LW2080_CTRL_PMGR_PWR_TUPLE
 */
static FLCN_STATUS
s_pwrDevBa20CompleteTupleValues
(
    PWR_DEVICE_BA20            *pBa20,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    PWR_DEVICE_BA20_VOLT_RAIL_DATA *pBa20VoltRailData;
    LWU64_TYPE                      result;
    LwU64                           tempReading1;
    LwU64                           tempReading2;
    LwU32                           voltagemV;
    LwU32                           voltageuV         = 0;
    LwU8                            numRailsMonitored = 0;
    LwBoardObjIdx                   i;
    FLCN_STATUS                     status  = FLCN_OK;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pBa20 != NULL) && (pTuple != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrDevBa20CompleteTupleValues_done);

    PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_BEGIN(pBa20, &pBa20VoltRailData,
        (VOLT_RAIL **)NULL, i, LW_FALSE, status,
        s_pwrDevBa20CompleteTupleValues_done)
    {
        voltageuV = pBa20VoltRailData->voltageuV;
        numRailsMonitored++;
    }
    PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_END;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (numRailsMonitored > 0),
        FLCN_ERR_ILWALID_STATE,
        s_pwrDevBa20CompleteTupleValues_done);

    if (numRailsMonitored == 1)
    {
        pTuple->voltuV = voltageuV;
        voltagemV      = LW_UNSIGNED_ROUNDED_DIV(pTuple->voltuV,
            PWR_DEVICE_BA20_UNIT_COLWERSION_FACTOR);

        //
        // Perform intermediate 32-bit multiplications in 64-bit Arithmetic
        // to check for overflow.
        //
        if (pBa20->bLwrrent)
        {
            tempReading1 = (LwU64)voltagemV;
            tempReading2 = (LwU64)pTuple->lwrrmA;
            lwU64Mul(&(result.val), &tempReading1, &tempReading2);
            if (LWU64_HI(result))
            {
                pTuple->pwrmW = LW_U32_MAX;
            }
            else
            {
                pTuple->pwrmW = LWU64_LO(result) /
                    PWR_DEVICE_BA20_UNIT_COLWERSION_FACTOR;
            }
        }
        else
        {
            tempReading1 = (LwU64)pTuple->pwrmW;
            tempReading2 = (LwU64)PWR_DEVICE_BA20_UNIT_COLWERSION_FACTOR;
            lwU64Mul(&(result.val), &tempReading1, &tempReading2);
            if (LWU64_HI(result))
            {
                pTuple->lwrrmA = LW_U32_MAX;
            }
            else
            {
                pTuple->lwrrmA = LW_UNSIGNED_ROUNDED_DIV(LWU64_LO(result),
                                                             voltagemV);
            }
        }
    }
    else
    {
        pTuple->voltuV =
            LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_TUPLE_VOLTAGE_ILWALID;

        if (pBa20->bLwrrent)
        {
            pTuple->pwrmW =
                LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_TUPLE_POWER_ILWALID;
        }
        else
        {
            pTuple->lwrrmA =
                LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_TUPLE_LWRRENT_ILWALID;
        }
    }

s_pwrDevBa20CompleteTupleValues_done:
    return status;
}

/*!
 * @brief   Read power/current from BA20 power device
 *
 * @param[in]   pBa20    Pointer to a PWR_DEVICE_BA20 object
 * @param[in]   provIdx  Index of the requested provider
 * @param[in]   pReading Buffer to store retrieved power / current reading
 *
 * @return  FLCN_OK                Reading succesfully retrieved
 * @return  FLCN_ERR_NOT_SUPPORTED Invalid provider index
 */
static FLCN_STATUS
s_pwrDevBa20GetReading
(
    PWR_DEVICE_BA20 *pBa20,
    LwU8             provIdx,
    LwU32           *pReading
)
{
    PWR_DEVICE_BA20_VOLT_RAIL_DATA *pBa20VoltRailData;
    VOLT_RAIL                      *pRail;
    LwU8                            voltDomain;
    LwBoardObjIdx                   i;
    LwU32                           totalmX;
    FLCN_STATUS                     status  = FLCN_OK;

    // Check whether the provider index is supported.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (provIdx < LW2080_CTRL_PMGR_PWR_DEVICE_BA_2X_PROV_NUM),
        FLCN_ERR_NOT_SUPPORTED,
        s_pwrDevBa20GetReading_done);

    // Obtain the total power / current reading.
    totalmX = DRF_VAL(_CPWR_THERM, _PEAKPOWER_CONFIG10, _WIN_SUM_VALUE,
                REG_RD32(CSB,
                    LW_CPWR_THERM_PEAKPOWER_CONFIG10(pBa20->windowIdx)));

    if (provIdx == LW2080_CTRL_PMGR_PWR_DEVICE_BA_2X_PROV_DYNAMIC)
    {
        PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_BEGIN(pBa20, &pBa20VoltRailData, &pRail,
            i, LW_FALSE, status, s_pwrDevBa20GetReading_done)
        {
            // Obtain the voltage domain.
            voltDomain = voltRailDomainGet(pRail);
            if (voltDomain == LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC)
            {
                totalmX -=
                    DRF_VAL(_CPWR_THERM, _PEAKPOWER_CONFIG7,
                        _LWVDD_LEAKAGE_C, REG_RD32(CSB,
                        LW_CPWR_THERM_PEAKPOWER_CONFIG7(
                            pBa20->windowIdx)));
            }
            else if (voltDomain == LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD)
            {
                totalmX -=
                    DRF_VAL(_CPWR_THERM, _PEAKPOWER_CONFIG9,
                        _MSVDD_LEAKAGE_C, REG_RD32(CSB,
                        LW_CPWR_THERM_PEAKPOWER_CONFIG9(
                            pBa20->windowIdx)));
            }
            else
            {
                // Any other voltage domain is not allowed.
                PMU_ASSERT_OK_OR_GOTO(status,
                    FLCN_ERR_ILWALID_STATE,
                    s_pwrDevBa20GetReading_done);
            }
        }
        PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_END;
    }

   *pReading = totalmX;

s_pwrDevBa20GetReading_done:
    return status;
}

/*!
 * @brief Compute shiftA value.
 *
 * @param[in]       pBa20             Pointer to a PWR_DEVICE_BA20 object
 * @param[in]       pBa20VoltRailData     Pointer to a PWR_DEVICE_BA20_VOLT_RAIL_DATA object
 * @param[in, out]  pShiftA           Pointer to the field containing the shiftA value.
 *
 * @returns FLCN_OK if shiftA computation is successful,
 *          error otherwise.
 *
 */
static FLCN_STATUS
s_pwrDevBa20ComputeShiftA
(
    PWR_DEVICE_BA20                *pBa20,
    PWR_DEVICE_BA20_VOLT_RAIL_DATA *pBa20VoltRailData,
    LwU8                           *pShiftA
)
{
    LwS8             shiftA;
    LwUFXP20_12      maxScaleA;
    VOLT_RAIL       *pRail;
    LwBool           bFBVDDModeIsActive;
    FLCN_STATUS      status        = FLCN_OK;

    //
    // Obtain the boolean denoting whether the BA20 power sensor is operating
    // in FBVDD mode.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrDevBa20FBVDDModeIsActiveGet(pBa20, &bFBVDDModeIsActive),
        s_pwrDevBa20ComputeShiftA_done);

    //
    // If the BA20 power sensor is operating in FBVDD mode, set shiftA value
    // accordingly.
    //
    if (bFBVDDModeIsActive)
    {
        *pShiftA = LW_CPWR_THERM_PEAKPOWER_CONFIGX_BA_SUM_SHIFT_FBVDD_MODE;
    }
    else
    {
        // Obtain the voltage rail object pointer.
        PWR_DEVICE_BA20_VOLT_RAIL_FROM_VOLT_RAIL_DATA_GET(pBa20, pBa20VoltRailData,
            &pRail, status,
            s_pwrDevBa20ComputeShiftA_done);

        // Obtain the largest possible scaleA.
        PMU_ASSERT_OK_OR_GOTO(status,
            voltRailGetScaleFactor(pRail,
                PWR_DEVICE_BA20_SCALE_EQUATION_MAX_VOLTAGE_UV, pBa20->bLwrrent,
                &maxScaleA),
            s_pwrDevBa20ComputeShiftA_done);

        // Get the highest set bit of the integer portion.
        HIGHESTBITIDX_32(maxScaleA);

        //
        // Compute shiftA. shiftA is the amount needed to bring the MSB that is
        // set in maxScale A to the MSB location in the FACTOR_A register
        // field. A positive value indicates a left shift, a negative value
        // indicates a right shift.
        //
        shiftA = DRF_BASE(LW_TYPES_FXP_INTEGER(20, 12)) +
            DRF_SIZE(LW_THERM_PEAKPOWER_CONFIGX_FACTOR_A) - 1
            - maxScaleA;

        //
        // Ensure that shiftA is non-negative, since this would mean some of
        // the integer portion gets shifted out, and this loss of precision is
        // unacceptable.
        //
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (shiftA >= 0),
            FLCN_ERR_ILWALID_STATE,
            s_pwrDevBa20ComputeShiftA_done);

        *pShiftA = shiftA;
    }

s_pwrDevBa20ComputeShiftA_done:
    return status;
}

/*!
 * @brief Compute FACTOR_A for a given voltage
 *
 * @param[in]       pBa20             Pointer to a PWR_DEVICE_BA20 object
 * @param[in]       pBa20VoltRailData     Pointer to a PWR_DEVICE_BA20_VOLT_RAIL_DATA object
 * @param[in]       voltageuV         The voltage to compute FACTOR_A at, in microvolts (uV)
 * @param[in, out]  pFactorA          Pointer to the field containing the Factor A value.
 *
 * @returns FLCN_OK if Factor A computation is successful,
 *          error otherwise.
 *
 */
static FLCN_STATUS
s_pwrDevBa20ComputeFactorA
(
    PWR_DEVICE_BA20                *pBa20,
    PWR_DEVICE_BA20_VOLT_RAIL_DATA *pBa20VoltRailData,
    LwU32                           voltageuV,
    LwU32                          *pFactorA
)
{
    LwUFXP20_12  scaleA;
    VOLT_RAIL   *pRail;
    LwBool       bFBVDDModeIsActive;
    FLCN_STATUS  status = FLCN_OK;

    // Obtain the voltage rail object pointer.
    PWR_DEVICE_BA20_VOLT_RAIL_FROM_VOLT_RAIL_DATA_GET(pBa20, pBa20VoltRailData,
        &pRail, status,
        s_pwrDevBa20ComputeFactorA_done);

    //
    // Obtain the boolean denoting whether the BA20 power sensor is operating
    // in FBVDD mode.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrDevBa20FBVDDModeIsActiveGet(pBa20, &bFBVDDModeIsActive),
        s_pwrDevBa20ComputeFactorA_done);

    //
    // If the BA20 power sensor is operating in FBVDD mode, set FactorA value
    // accordingly.
    //
    if (bFBVDDModeIsActive)
    {
        *pFactorA = LW_CPWR_THERM_PEAKPOWER_CONFIGX_FACTOR_A_FBVDD_MODE;
    }
    else
    {
        // Compute what the new scaling factor should be, called scaleA.
        PMU_ASSERT_OK_OR_GOTO(status,
            voltRailGetScaleFactor(pRail, voltageuV, pBa20->bLwrrent, &scaleA),
            s_pwrDevBa20ComputeFactorA_done);

        //
        // We want BA sum to be multiplied by scaleA. However, HW breaks the
        // multiplication by scaleA into a multiply by factorA, and a bit-shift
        // by shiftA. shiftA is computed at boot, verified to be a right shift,
        // and held constant afterwards. we left shift here to counteract the 
        // right shift that HW will apply.
        //
        scaleA <<= pBa20VoltRailData->shiftA;
        *pFactorA = LW_TYPES_UFXP_X_Y_TO_U32(20, 12, scaleA);

        //
        // TODO-Chandrashis: Scale down final Factor A value by the scaling
        // factor coming from the VBIOS.
        //
    }

s_pwrDevBa20ComputeFactorA_done:
    return status;
}

/*!
 * @brief    Setup HW realtime FACTOR_A control
 *
 * @param[in]  pBa20      Pointer to a PWR_DEVICE_BA20 object
 *
 * @returns FLCN_OK if Factor A LUT initialization is successful,
 *          error otherwise.
 */
FLCN_STATUS
static s_pwrDevBa20InitFactorALut
(
    PWR_DEVICE_BA20 *pBa20
)
{
    PWR_DEVICE_BA20_VOLT_RAIL_DATA *pBa20VoltRailData;
    VOLT_RAIL                      *pRail;
    LwU8                            lutIdx;
    LwU32                           regVal = 0;
    LwU32                           factorA;
    LwU32                           voltageuV;
    FLCN_STATUS                     status = FLCN_OK;
    LwU8                            voltDomain;
    LwBoardObjIdx                   i;

    ct_assert(LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_ADC_OUTPUT_WIDTH >
              LW_CPWR_THERM_PEAKPOWER_LWVDD_ADC_IIR_LUT_INDEX_WIDTH);
    ct_assert(LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL_STEP_SIZE_UV > 0);

    // populate FACTOR_A LUT
    regVal = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG14, _LUT_WRITE_EN,
                         _WRITE, regVal);

    PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_BEGIN(pBa20, &pBa20VoltRailData, &pRail,
        i, LW_FALSE, status, s_pwrDevBa20InitFactorALut_done)
    {
        if (pBa20VoltRailData->super.bFactorALutEnable)
        {
            // Obtain the voltage domain.
            voltDomain = voltRailDomainGet(pRail);
            if (voltDomain == LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC)
            {
                regVal  = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG14,
                              _LUT_SEL, _LWVDD_FACTOR_A, regVal);
            }
            else if (voltDomain == LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD)
            {
                regVal = FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_CONFIG14,
                              _LUT_SEL, _MSVDD_FACTOR_A, regVal);
            }
            else
            {
                // Any other voltage domain is not allowed.
                PMU_ASSERT_OK_OR_GOTO(status,
                    FLCN_ERR_ILWALID_STATE,
                    s_pwrDevBa20InitFactorALut_done);
            }

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
                regVal = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG14,
                            _LUT_INDEX, lutIdx, regVal);

                // Obtain the Factor A value.
                PMU_ASSERT_OK_OR_GOTO(status,
                    s_pwrDevBa20ComputeFactorA(pBa20, pBa20VoltRailData, voltageuV,
                        &factorA),
                    s_pwrDevBa20InitFactorALut_done);

                // Check whether the factorA value may overflow.
                PWR_DEVICE_BA20_OVERFLOW_CHECK(factorA,
                    LW_CPWR_THERM_PEAKPOWER_CONFIG14_LUT_DATA, status,
                    s_pwrDevBa20InitFactorALut_done);

                regVal = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG14,
                                         _LUT_DATA,  factorA, regVal);
                REG_WR32(CSB,
                    LW_CPWR_THERM_PEAKPOWER_CONFIG14(pBa20->windowIdx),
                    regVal);
            }
        }
    }
    PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_END;

s_pwrDevBa20InitFactorALut_done:
    return status;
}

/*!
 * @brief Update FACTOR_A in HW
 *
 * @param[in]  pBa20             Pointer to a PWR_DEVICE_BA20 object
 * @param[in]  pBa20VoltRailData Pointer to a PWR_DEVICE_BA20_VOLT_RAIL_DATA object
 *
 * @returns FLCN_OK if Factor A updation is successful,
 *          error otherwise.
 *
 */
static FLCN_STATUS
s_pwrDevBa20UpdateFactorA
(
    PWR_DEVICE_BA20                *pBa20,
    PWR_DEVICE_BA20_VOLT_RAIL_DATA *pBa20VoltRailData
)
{
    LwU32        factorA;
    LwU32        regVal;
    LwU8         voltDomain;
    VOLT_RAIL   *pRail;
    FLCN_STATUS  status = FLCN_OK;

    // Obtain the Factor A value.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrDevBa20ComputeFactorA(pBa20, pBa20VoltRailData,
            pBa20VoltRailData->voltageuV, &factorA),
        s_pwrDevBa20UpdateFactorA_done);

    // Obtain the voltage rail object pointer.
    PWR_DEVICE_BA20_VOLT_RAIL_FROM_VOLT_RAIL_DATA_GET(pBa20, pBa20VoltRailData,
        &pRail, status,
        s_pwrDevBa20UpdateFactorA_done);

    // Obtain the voltage domain.
    voltDomain = voltRailDomainGet(pRail);
    if (voltDomain == LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC)
    {
        // Check whether the Factor A value may overflow.
        PWR_DEVICE_BA20_OVERFLOW_CHECK(factorA,
            LW_CPWR_THERM_PEAKPOWER_CONFIG6_LWVDD_FACTOR_A, status,
            s_pwrDevBa20UpdateFactorA_done);

        regVal  = REG_RD32(CSB,
            LW_CPWR_THERM_PEAKPOWER_CONFIG6(pBa20->windowIdx));
        regVal  = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG6,
                  _LWVDD_FACTOR_A, factorA, regVal);

        // Update Factor A in HW for LWVDD.
        REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG6(pBa20->windowIdx),
            regVal);
    }
    else if (voltDomain == LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD)
    {
        // Check whether the Factor A value may overflow.
        PWR_DEVICE_BA20_OVERFLOW_CHECK(factorA,
            LW_CPWR_THERM_PEAKPOWER_CONFIG8_MSVDD_FACTOR_A, status,
            s_pwrDevBa20UpdateFactorA_done);

        regVal  = REG_RD32(CSB,
                  LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa20->windowIdx));
        regVal  = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG8,
                  _MSVDD_FACTOR_A, factorA, regVal);

        // Update Factor A in HW for MSVDD.
        REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa20->windowIdx),
            regVal);
    }
    else
    {
        // Any other voltage domain is not allowed.
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_ILWALID_STATE,
            s_pwrDevBa20UpdateFactorA_done);
    }

s_pwrDevBa20UpdateFactorA_done:
    return status;
}

/*!
 * @brief Update FACTOR_C in HW
 *
 * @param[in]  pBa20             Pointer to a PWR_DEVICE_BA20 object
 * @param[in]  pBa20VoltRailData     Pointer to a PWR_DEVICE_BA20_VOLT_RAIL_DATA object
 *
 * @returns FLCN_OK if Factor C updation is successful,
 *          error otherwise.
 *
 */
static FLCN_STATUS
s_pwrDevBa20UpdateFactorC
(
    PWR_DEVICE_BA20                *pBa20,
    PWR_DEVICE_BA20_VOLT_RAIL_DATA *pBa20VoltRailData
)
{
    LwU32        factorC;
    LwU8         voltDomain;
    VOLT_RAIL   *pRail;
    FLCN_STATUS  status = FLCN_OK;

    // Obtain the voltage rail object pointer.
    PWR_DEVICE_BA20_VOLT_RAIL_FROM_VOLT_RAIL_DATA_GET(pBa20, pBa20VoltRailData,
        &pRail, status,
        s_pwrDevBa20UpdateFactorC_done);

    // Obtain the leakage power for the current voltage rail.
    PMU_ASSERT_OK_OR_GOTO(status,
        voltRailGetLeakage(pRail, pBa20VoltRailData->voltageuV,
            pBa20->bLwrrent, LW_TYPES_FXP_ZERO, &factorC),
        s_pwrDevBa20UpdateFactorC_done);

    // Obtain the voltage domain.
    voltDomain = voltRailDomainGet(pRail);
    if (voltDomain == LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC)
    {
        // Check whether the Factor C value may overflow.
        PWR_DEVICE_BA20_OVERFLOW_CHECK(factorC,
            LW_CPWR_THERM_PEAKPOWER_CONFIG7_LWVDD_LEAKAGE_C, status,
            s_pwrDevBa20UpdateFactorC_done);

        // Update Factor C in HW for LWVDD.
        REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG7(pBa20->windowIdx),
            DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG7,
                _LWVDD_LEAKAGE_C, factorC));
    }
    else if (voltDomain == LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD)
    {
        // Check whether the Factor C value may overflow.
        PWR_DEVICE_BA20_OVERFLOW_CHECK(factorC,
            LW_CPWR_THERM_PEAKPOWER_CONFIG9_MSVDD_LEAKAGE_C, status,
            s_pwrDevBa20UpdateFactorC_done);

        // Update Factor C in HW for MSVDD.
        REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(pBa20->windowIdx),
            DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG9,
                _MSVDD_LEAKAGE_C, factorC));
    }
    else
    {
        // Any other voltage domain is not allowed.
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_ILWALID_STATE,
            s_pwrDevBa20UpdateFactorC_done);
    }

s_pwrDevBa20UpdateFactorC_done:
    return status;
}

/*!
 * @brief   Update BA20 power device limit's threshold
 *
 * @param[in]   pBa20       Pointer to a PWR_DEVICE_BA20 object
 * @param[in]   limitIdx    Index of the limit threshold
 * @param[in]   limitValue  Limit value to apply
 *
 * @returns FLCN_OK if the threshold updation is successful,
 *          error otherwise.
 */
FLCN_STATUS
s_pwrDevBa20LimitThresholdSet
(
    PWR_DEVICE_BA20 *pBa20,
    LwU8             limitIdx,
    LwU32            limitValue
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU32       address = 0;
    LwU32       reg32;

    if (limitValue == PWR_DEVICE_NO_LIMIT)
    {
        reg32 = DRF_DEF(_CPWR_THERM, _BA20_THRESHOLD, _EN, _DISABLED);
    }
    else
    {
        reg32 = DRF_NUM(_CPWR_THERM, _BA20_THRESHOLD, _VAL, limitValue);
        reg32 =
            FLD_SET_DRF(_CPWR_THERM, _BA20_THRESHOLD, _EN, _ENABLED, reg32);
    }

    switch (limitIdx)
    {
        case LW_CPWR_THERM_BA20_THRESHOLD_1H:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG2(pBa20->windowIdx);
            break;
        case LW_CPWR_THERM_BA20_THRESHOLD_2H:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG3(pBa20->windowIdx);
            break;
        case LW_CPWR_THERM_BA20_THRESHOLD_1L:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG4(pBa20->windowIdx);
            break;
        case LW_CPWR_THERM_BA20_THRESHOLD_2L:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG5(pBa20->windowIdx);
            break;
        default:
            PMU_ASSERT_OK_OR_GOTO(status,
                FLCN_ERR_ILWALID_STATE,
                s_pwrDevBa20LimitThresholdSet_done);
    }

    REG_WR32(CSB, address, reg32);

s_pwrDevBa20LimitThresholdSet_done:
    return status;
}
