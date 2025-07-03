/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrdev_gpuadc1x.c
 * @brief GPUADC1X Driver
 *
 * @implements PWR_DEVICE
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_fxp.h"
#include "pmu_objpmgr.h"
#include "pmu_objperf.h"
#include "pmgr/pwrdev_gpuadc1x.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc pwrDevTupleGet_GPUADC1X
 */
FLCN_STATUS
pwrDevTupleGet_GPUADC1X
(
    PWR_DEVICE                        *pDev,
    PWR_DEVICE_GPUADC1X_PROV          *pProv,
    LW2080_CTRL_PMGR_PWR_TUPLE        *pTuple,
    PWR_DEVICE_GPUADC1X_TUPLE_PARAMS  *pParams
)
{
    PWR_DEVICE_GPUADC1X *pGpuAdc1x = (PWR_DEVICE_GPUADC1X *)pDev;
    FLCN_STATUS          status    = FLCN_OK;
    LwU32                reg32;
    LWU64_TYPE           pwrAcc;

    // Take a SNAP of the ADC registers
    pwrDevGpuAdc1xSnapTrigger(pGpuAdc1x);

    // Get Current
    if (pwrDevGpuAdc1xProvChShuntGet(pProv) !=
            PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE)
    {
        reg32 = 0;
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_IIR_VALUE_INDEX, _INDEX,
                    LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_CH(
                        pwrDevGpuAdc1xProvChShuntGet(pProv)), reg32);
        REG_WR32(CSB, LW_CPWR_THERM_ADC_IIR_VALUE_INDEX, reg32);

        reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_IIR_VALUE_DATA);

        if (pParams->numSamples > 1)
        {
            reg32 = LW_UNSIGNED_ROUNDED_DIV(reg32, pParams->numSamples);
        }
        pTuple->lwrrmA = multUFXP32(pParams->lwrrShiftBits, reg32,
            pProv->lwrrColwFactor);
    }
    else
    {
        pTuple->lwrrmA = 0;
    }

    // Get Voltage
    if (pwrDevGpuAdc1xProvChBusGet(pProv) !=
            PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE)
    {
        LwUFXP28_4 voltColwFactor =
            pwrDevGpuAdc1xVoltColwFactorGet(pGpuAdc1x, pProv);
        reg32 = 0;
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_IIR_VALUE_INDEX, _INDEX,
                    LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_CH(
                        pwrDevGpuAdc1xProvChBusGet(pProv)), reg32);
        REG_WR32(CSB, LW_CPWR_THERM_ADC_IIR_VALUE_INDEX, reg32);

        reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_IIR_VALUE_DATA);

        if (pParams->numSamples > 1)
        {
            reg32 = LW_UNSIGNED_ROUNDED_DIV(reg32, pParams->numSamples);
        }
        pTuple->voltuV = multUFXP32(pParams->voltShiftBits, reg32,
            (voltColwFactor * (LwU32)1000));
    }
    else
    {
        pTuple->voltuV = 0;
    }

    // Get Power
    if (pwrDevGpuAdc1xProvChPairGet(pProv) !=
            PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE)
    {
        reg32 = 0;
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_VALUE_INDEX, _INDEX,
                    LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_CHP(
                        pwrDevGpuAdc1xProvChPairGet(pProv)), reg32);
        REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_VALUE_INDEX, reg32);

        reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_VALUE_DATA);

        if (pParams->numSamples > 1)
        {
            reg32 = LW_UNSIGNED_ROUNDED_DIV(reg32,
                (pParams->numSamples * pParams->numSamples));
        }
        pTuple->pwrmW = multUFXP32(pParams->pwrShiftBits, reg32,
            pProv->pwrColwFactor);

        // Get Power Aclwmulation (energy)
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrDevGpuAdc1xPowerAccGet(pGpuAdc1x, pProv, &pwrAcc.val),
            pwrDevTupleGet_GPUADC1X_exit);

        pmgrAclwpdate(&(pProv->energy), &pwrAcc);

        // Colwert to LwU64_ALIGN32 and save in tuple
        LwU64_ALIGN32_PACK(&pTuple->energymJ,
            PMGR_ACC_DST_GET(&(pProv->energy)));
    }
    else
    {
        pTuple->pwrmW    = 0;
        pwrAcc.val       = 0;
        LwU64_ALIGN32_PACK(&pTuple->energymJ, &pwrAcc.val);
    }

pwrDevTupleGet_GPUADC1X_exit:
    return status;
}

/*!
 * @copydoc pwrDevTupleAccGet_GPUADC1X
 */
FLCN_STATUS
pwrDevTupleAccGet_GPUADC1X
(
    PWR_DEVICE                     *pDev,
    PWR_DEVICE_GPUADC1X_PROV       *pProv,
    LW2080_CTRL_PMGR_PWR_TUPLE_ACC *pTupleAcc
)
{
    PWR_DEVICE_GPUADC1X *pGpuAdc1x = (PWR_DEVICE_GPUADC1X *)pDev;
    FLCN_STATUS          status    = FLCN_OK;
    LWU64_TYPE           reg64;
    LwU32                reg32;

    if (pGpuAdc1x->seqId != pTupleAcc->seqId)
    {
        status = FLCN_ERR_ACC_SEQ_MISMATCH;
        goto pwrDevTupleAccGet_GPUADC1X_exit;
    }

    // Take a SNAP of the ADC registers
    pwrDevGpuAdc1xSnapTrigger(pGpuAdc1x);

    // Get Current Aclwmulation
    if (pwrDevGpuAdc1xProvChShuntGet(pProv) !=
            PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE)
    {
        reg32 = DRF_NUM(_CPWR_THERM, _ADC_ACC_VALUE_INDEX, _INDEX,
                    LW_CPWR_THERM_ADC_ACC_VALUE_INDEX_INDEX_LB_CH(
                        pwrDevGpuAdc1xProvChShuntGet(pProv)));
        REG_WR32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_INDEX, reg32);
        reg64.parts.lo = REG_RD32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_DATA);

        reg32 = DRF_NUM(_CPWR_THERM, _ADC_ACC_VALUE_INDEX, _INDEX,
                    LW_CPWR_THERM_ADC_ACC_VALUE_INDEX_INDEX_UB_CH(
                        pwrDevGpuAdc1xProvChShuntGet(pProv)));
        REG_WR32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_INDEX, reg32);
        reg64.parts.hi = REG_RD32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_DATA);

        pmgrAclwpdate(&(pProv->lwrr), &reg64);
        LwU64_ALIGN32_PACK(&pTupleAcc->lwrrAccnC,
            PMGR_ACC_DST_GET(&(pProv->lwrr)));
    }
    else
    {
        reg64.val = 0;
        LwU64_ALIGN32_PACK(&pTupleAcc->lwrrAccnC, &reg64.val);
    }

    // Get Voltage Aclwmulation
    if (pwrDevGpuAdc1xProvChBusGet(pProv) !=
            PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE)
    {
        reg32 = DRF_NUM(_CPWR_THERM, _ADC_ACC_VALUE_INDEX, _INDEX,
                    LW_CPWR_THERM_ADC_ACC_VALUE_INDEX_INDEX_LB_CH(
                        pwrDevGpuAdc1xProvChBusGet(pProv)));
        REG_WR32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_INDEX, reg32);
        reg64.parts.lo = REG_RD32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_DATA);

        reg32 = DRF_NUM(_CPWR_THERM, _ADC_ACC_VALUE_INDEX, _INDEX,
                    LW_CPWR_THERM_ADC_ACC_VALUE_INDEX_INDEX_UB_CH(
                        pwrDevGpuAdc1xProvChBusGet(pProv)));
        REG_WR32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_INDEX, reg32);
        reg64.parts.hi = REG_RD32(CSB, LW_CPWR_THERM_ADC_ACC_VALUE_DATA);

        pmgrAclwpdate(&(pProv->volt), &reg64);
        LwU64_ALIGN32_PACK(&pTupleAcc->voltAccmVus,
            PMGR_ACC_DST_GET(&(pProv->volt)));
    }
    else
    {
        reg64.val = 0;
        LwU64_ALIGN32_PACK(&pTupleAcc->voltAccmVus, &reg64.val);
    }

    // Get Power Aclwmulation
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrDevGpuAdc1xPowerAccGet(pGpuAdc1x, pProv, &reg64.val),
        pwrDevTupleAccGet_GPUADC1X_exit);

    LwU64_ALIGN32_PACK(&pTupleAcc->pwrAccnJ, &reg64.val);

pwrDevTupleAccGet_GPUADC1X_exit:
    // Update the sequence Id
    pTupleAcc->seqId = pGpuAdc1x->seqId;

    return status;
}

/*!
 * @copydoc pwrDevTupleFullRangeGet_GPUADC1X
 */
FLCN_STATUS
pwrDevTupleFullRangeGet_GPUADC1X
(
    PWR_DEVICE                 *pDev,
    PWR_DEVICE_GPUADC1X_PROV   *pProv,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    PWR_DEVICE_GPUADC1X *pGpuAdc1x = (PWR_DEVICE_GPUADC1X *)pDev;
    LwU32                fullRangeVoltagemV;

    // Colwert fullRangeVoltage to mV
    fullRangeVoltagemV = pwrDevGpuAdc1xFullRangeVoltmVGet(pGpuAdc1x, pProv);

    // Multiplying again by 1000 to colwert to uV
    pTuple->voltuV     = fullRangeVoltagemV * (LwU32)1000;

    // Colwert fullRangeLwrrent to mA
    pTuple->lwrrmA     = pwrDevGpuAdc1xFullRangeLwrrmAGet(pGpuAdc1x, pProv);
    pTuple->pwrmW      = pwrDevGpuAdc1xFullRangePwrmWGet(pGpuAdc1x, pProv);

    return FLCN_OK;
}

/*!
 * @copydoc pwrDevGpuAdc1xPowerAccGet
 */
FLCN_STATUS
pwrDevGpuAdc1xPowerAccGet
(
    PWR_DEVICE_GPUADC1X       *pGpuAdc1x,
    PWR_DEVICE_GPUADC1X_PROV  *pProv,
    LwU64                     *pPwrAccnJ
)
{
    LWU64_TYPE reg64;
    LwU32      reg32;

    if (pwrDevGpuAdc1xProvChPairGet(pProv) !=
            PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE)
    {
        reg32 = DRF_NUM(_CPWR_THERM, _ADC_MUL_ACC_VALUE_INDEX, _INDEX,
                    LW_CPWR_THERM_ADC_MUL_ACC_VALUE_INDEX_INDEX_LB_CHP(
                        pwrDevGpuAdc1xProvChPairGet(pProv)));
        REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_ACC_VALUE_INDEX, reg32);
        reg64.parts.lo = REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA);

        reg32 = DRF_NUM(_CPWR_THERM, _ADC_MUL_ACC_VALUE_INDEX, _INDEX,
                    LW_CPWR_THERM_ADC_MUL_ACC_VALUE_INDEX_INDEX_UB_CHP(
                        pwrDevGpuAdc1xProvChPairGet(pProv)));
        REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_ACC_VALUE_INDEX, reg32);
        reg64.parts.hi = REG_RD32(CSB, LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA);

        pmgrAclwpdate(&(pProv->pwr), &reg64);
        *pPwrAccnJ = *(PMGR_ACC_DST_GET(&(pProv->pwr)));
    }
    else
    {
        *pPwrAccnJ = 0;
    }

    return FLCN_OK;
}

/*!
 * @copydoc pwrDevGpuAdc1xReset
 */
void
pwrDevGpuAdc1xReset
(
    PWR_DEVICE_GPUADC1X *pGpuAdc1x
)
{
    pwrDevGpuAdc1xSwResetTrigger(pGpuAdc1x);
    pGpuAdc1x->seqId++;
}

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */
/*!
 * Construct a GPUADC1X PWR_DEVICE object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrDevGrpIfaceModel10ObjSetImpl_GPUADC1X
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    PWR_DEVICE_GPUADC1X                               *pGpuAdc1x;
    RM_PMU_PMGR_PWR_DEVICE_DESC_GPUADC1X_BOARDOBJ_SET *pGpuAdc1xSet =
        (RM_PMU_PMGR_PWR_DEVICE_DESC_GPUADC1X_BOARDOBJ_SET *)pBoardObjDesc;
    FLCN_STATUS                                        status = FLCN_OK;
    LwU32                                              fullRangeVoltagemV;

    // Construct and initialize parent-class object.
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrDevGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        pwrDevGrpIfaceModel10ObjSetImpl_GPUADC1X_done);

    pGpuAdc1x = (PWR_DEVICE_GPUADC1X *)*ppBoardObj;

    // Sanity check the VFE VAR indexes
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (VFE_VAR_IS_VALID(pGpuAdc1xSet->vcmOffsetVfeVarIdx)  &&
         VFE_VAR_IS_VALID(pGpuAdc1xSet->diffOffsetVfeVarIdx) &&
         VFE_VAR_IS_VALID(pGpuAdc1xSet->diffGailwfeVarIdx)),
        FLCN_ERR_ILWALID_STATE,
        pwrDevGrpIfaceModel10ObjSetImpl_GPUADC1X_done);

    pGpuAdc1x->pwmPeriod           = pGpuAdc1xSet->pwmPeriod;
    pGpuAdc1x->iirLength           = pGpuAdc1xSet->iirLength;
    pGpuAdc1x->sampleDelay         = pGpuAdc1xSet->sampleDelay;
    pGpuAdc1x->activeProvCount     = pGpuAdc1xSet->activeProvCount;
    pGpuAdc1x->resetLength         = pGpuAdc1xSet->resetLength;
    pGpuAdc1x->fullRangeVoltage    = pGpuAdc1xSet->fullRangeVoltage;
    pGpuAdc1x->vcmOffsetVfeVarIdx  = pGpuAdc1xSet->vcmOffsetVfeVarIdx;
    pGpuAdc1x->diffOffsetVfeVarIdx = pGpuAdc1xSet->diffOffsetVfeVarIdx;
    pGpuAdc1x->diffGailwfeVarIdx   = pGpuAdc1xSet->diffGailwfeVarIdx;
    pGpuAdc1x->adcMaxValue         = pGpuAdc1xSet->adcMaxValue;
    pGpuAdc1x->activeChannelsNum   = pGpuAdc1xSet->activeChannelsNum;
    pGpuAdc1x->bFirstLoad          = LW_TRUE;

    // Sanity check adcMaxValue to avoid a divide by zero scenario
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pGpuAdc1x->adcMaxValue != 0),
        FLCN_ERR_ILWALID_STATE,
        pwrDevGrpIfaceModel10ObjSetImpl_GPUADC1X_done);

    //
    // Callwlate voltColwFactor
    //
    // Multiplying fullRangeVoltage by 1000 to colwert to mV
    //   8.8 V * 10.0 mV/V => 18.8 mV
    // Discarding 8 fractional bits
    //
    // We are not using pwrDevGpuAdc1xFullRangeVoltmVGet() to get the
    // fullRangeVoltagemV since that function requires specifying a provider and
    // the pGpuAdc1x->voltColwFactor here is common across providers.
    //
    fullRangeVoltagemV = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(24, 8,
        pGpuAdc1x->fullRangeVoltage * 1000U);

    //
    // Dividing the fullRangeVoltagemV by adcMaxValue and scaling it to FXP28.4
    // Considering fullRangeVoltage of 64V (0x4000 in FXP8.8),
    // voltColwFactor is 0x3F7D mV/code which is effectively FXP10.4
    //
    pGpuAdc1x->voltColwFactor = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(28, 4,
        fullRangeVoltagemV, pGpuAdc1x->adcMaxValue);

    //
    // accColwusPerCycle = ADC_PWM_PERIOD * ACTIVE_CHANNELS_NUM * 10^3
    //                      -------------------------------------------
    //                                   utilsClkFreqKhz
    //                   = pwmPeriod * activeChannelsNum * 10^3 / 108000
    //                   = pwmPeriod * activeChannelsNum / 108
    //
    // Numerical Analysis:
    //   pwmPeriod is FXP12.0 and activeChannelsNum (max=16) is FXP5.0
    //   accColwusPerCycle is FXP20.12
    //   12.0 * 5.0 => 17.0
    //   17.0 << 12 => 17.12
    //                 17.12 => 20.12
    //
    pGpuAdc1x->accColwusPerCycle = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12,
        (pGpuAdc1x->pwmPeriod * pGpuAdc1x->activeChannelsNum), 108);

pwrDevGrpIfaceModel10ObjSetImpl_GPUADC1X_done:
    return status;
}

/*!
 * GPUADC1X power device implementation.
 *
 * @copydoc PwrDevLoad
 */
FLCN_STATUS
pwrDevLoad_GPUADC1X
(
    PWR_DEVICE  *pDev
)
{
    PWR_DEVICE_GPUADC1X *pGpuAdc1x = (PWR_DEVICE_GPUADC1X *)pDev;
    FLCN_STATUS          status    = FLCN_OK;
    LwU32                reg32;

    // Setting up the PWR_SENSE_ADC_CTRL register for enabling the ADC
    reg32 = REG_RD32(FECS, LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL);
    reg32 = FLD_SET_DRF(_PTRIM_SYS_NAFLL, _SYS_PWR_SENSE_ADC_CTRL, _IDDQ,
                _POWER_ON, reg32);
    REG_WR32(FECS, LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL, reg32);

    //
    // Setting up ADC_PWM_PERIOD and ADC_PWM_HI
    // For GPUADC1X, ADC_PWM_HI = ADC_PWM_PERIOD/2
    //
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_PWM);

    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_PWM, _PERIOD,
                pGpuAdc1x->pwmPeriod, reg32);
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_PWM, _HI,
                LW_UNSIGNED_ROUNDED_DIV(pGpuAdc1x->pwmPeriod, 2), reg32);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_PWM, reg32);

    // Setting up ADC_SW_RESET_LENGTH
    pwrDevGpuAdc1xSwResetLengthSet(pGpuAdc1x);

    //
    // Enabling ADC and SW_RESET need to be done in critical section to
    // guarantee a consistent duration between the two operations. OVR-M
    // requires this time to be consistent to synchronize with the ADC.
    //
    appTaskCriticalEnter();
    {
        //
        // Setting up ADC_CTRL
        // 1. Enable ADC_SENSING and ADC_ACC
        // 2. Set ACTIVE_CHANNELS_NUM
        // 3. Set IIR_LENGTH and SAMPLE_DELAY
        //
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL);

        reg32 = FLD_SET_DRF(_CPWR_THERM, _ADC_CTRL, _ADC_SENSING, _ON, reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _ADC_CTRL, _ADC_ACC, _ON, reg32);
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL, _ACTIVE_CHANNELS_NUM,
                    pGpuAdc1x->activeChannelsNum, reg32);
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL, _ADC_IIR_LENGTH,
                    pGpuAdc1x->iirLength, reg32);
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL, _SAMPLE_DELAY,
                    pGpuAdc1x->sampleDelay, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL, reg32);

        //
        // ENABLE bit should be set after 1 us of setting IDDQ bit to POWER_ON.
        // So, do not move IDDQ = POWER_ON register write above here.
        //
        reg32 = REG_RD32(FECS, LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL);
        reg32 = FLD_SET_DRF(_PTRIM_SYS_NAFLL, _SYS_PWR_SENSE_ADC_CTRL, _ENABLE,
                    _YES, reg32);
        REG_WR32(FECS, LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL, reg32);

        //
        // We need to trigger SW_RESET after enabling ADC so that OVR-M can
        // synchronize with the ADC clk signals. Done on first load only
        //
        if (pGpuAdc1x->bFirstLoad)
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrMonitor)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            pwrDevGpuAdc1xSwResetTrigger(pGpuAdc1x);
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        }
    }
    appTaskCriticalExit();

    return status;
}

/* ------------------------- PWR_DEVICE_GPUADC1X Interfaces ----------------- */
/*!
 * GPUADC1X power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xBaSetLimit
 */
FLCN_STATUS
pwrDevGpuAdc1xBaSetLimit
(
    PWR_DEVICE_GPUADC1X *pGpuAdc1x,
    LwU8                 baWindowIdx,
    LwU8                 limitIdx,
    LwU8                 limitUnit,
    LwU32                limitValue
)
{
    switch (BOARDOBJ_GET_TYPE(pGpuAdc1x))
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevGpuAdc1xBaSetLimit_GPUADC10(pGpuAdc1x, baWindowIdx,
                       limitIdx, limitUnit, limitValue);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevGpuAdc1xBaSetLimit_GPUADC11(pGpuAdc1x, baWindowIdx,
                       limitIdx, limitUnit, limitValue);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevGpuAdc1xBaSetLimit_GPUADC13(pGpuAdc1x, baWindowIdx,
                       limitIdx, limitUnit, limitValue);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * GPUADC1X power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xFullRangeVoltmVGet
 */
LwU32
pwrDevGpuAdc1xFullRangeVoltmVGet
(
    PWR_DEVICE_GPUADC1X      *pGpuAdc1x,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    switch (BOARDOBJ_GET_TYPE(pGpuAdc1x))
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevGpuAdc1xFullRangeVoltmVGet_GPUADC10(pGpuAdc1x, pProv);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevGpuAdc1xFullRangeVoltmVGet_GPUADC11(pGpuAdc1x, pProv);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevGpuAdc1xFullRangeVoltmVGet_GPUADC13(pGpuAdc1x, pProv);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * GPUADC1X power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xFullRangeLwrrmAGet
 */
LwU32
pwrDevGpuAdc1xFullRangeLwrrmAGet
(
    PWR_DEVICE_GPUADC1X      *pGpuAdc1x,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    switch (BOARDOBJ_GET_TYPE(pGpuAdc1x))
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevGpuAdc1xFullRangeLwrrmAGet_GPUADC10(pGpuAdc1x, pProv);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevGpuAdc1xFullRangeLwrrmAGet_GPUADC11(pGpuAdc1x, pProv);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevGpuAdc1xFullRangeLwrrmAGet_GPUADC13(pGpuAdc1x, pProv);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * GPUADC1X power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xFullRangePwrmWGet
 */
LwU32
pwrDevGpuAdc1xFullRangePwrmWGet
(
    PWR_DEVICE_GPUADC1X      *pGpuAdc1x,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    switch (BOARDOBJ_GET_TYPE(pGpuAdc1x))
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevGpuAdc1xFullRangePwrmWGet_GPUADC10(pGpuAdc1x, pProv);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevGpuAdc1xFullRangePwrmWGet_GPUADC11(pGpuAdc1x, pProv);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevGpuAdc1xFullRangePwrmWGet_GPUADC13(pGpuAdc1x, pProv);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * GPUADC1X power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xVoltColwFactorGet
 */
LwU32
pwrDevGpuAdc1xVoltColwFactorGet
(
    PWR_DEVICE_GPUADC1X      *pGpuAdc1x,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    switch (BOARDOBJ_GET_TYPE(pGpuAdc1x))
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevGpuAdc1xVoltColwFactorGet_GPUADC10(pGpuAdc1x, pProv);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevGpuAdc1xVoltColwFactorGet_GPUADC11(pGpuAdc1x, pProv);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevGpuAdc1xVoltColwFactorGet_GPUADC13(pGpuAdc1x, pProv);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * GPUADC1X power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xSwResetLengthSet
 */
void
pwrDevGpuAdc1xSwResetLengthSet
(
    PWR_DEVICE_GPUADC1X *pGpuAdc1x
)
{
    switch (BOARDOBJ_GET_TYPE(pGpuAdc1x))
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevGpuAdc1xSwResetLengthSet_GPUADC10(pGpuAdc1x);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevGpuAdc1xSwResetLengthSet_GPUADC11(pGpuAdc1x);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return;
    }

    PMU_BREAKPOINT();
}

/*!
 * GPUADC1X power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xSnapTrigger
 */
void
pwrDevGpuAdc1xSnapTrigger
(
    PWR_DEVICE_GPUADC1X *pGpuAdc1x
)
{
    switch (BOARDOBJ_GET_TYPE(pGpuAdc1x))
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevGpuAdc1xSnapTrigger_GPUADC10(pGpuAdc1x);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevGpuAdc1xSnapTrigger_GPUADC11(pGpuAdc1x);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevGpuAdc1xSnapTrigger_GPUADC13(pGpuAdc1x);
    }

    PMU_BREAKPOINT();
}

/*!
 * GPUADC1X power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xSwResetTrigger
 */
void
pwrDevGpuAdc1xSwResetTrigger
(
    PWR_DEVICE_GPUADC1X *pGpuAdc1x
)
{
    switch (BOARDOBJ_GET_TYPE(pGpuAdc1x))
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevGpuAdc1xSwResetTrigger_GPUADC10(pGpuAdc1x);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevGpuAdc1xSwResetTrigger_GPUADC11(pGpuAdc1x);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return;
    }

    PMU_BREAKPOINT();
}

/* ------------------------- Static Functions ------------------------------- */
