/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrdev_gpuadc13.c
 * @brief GPUADC13 Driver
 *
 * @implements PWR_DEVICE
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_fxp.h"
#include "pmu_objpmgr.h"
#include "pmu_objperf.h"
#include "pmgr/pwrdev_gpuadc13.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
static FLCN_STATUS s_pwrDevGpuAdc13ProvConstruct(PWR_DEVICE_GPUADC13 *pGpuAdc13, RM_PMU_PMGR_PWR_DEVICE_DESC_GPUADC13_BOARDOBJ_SET *pGpuAdc13Set)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "s_pwrDevGpuAdc13ProvConstruct");
static FLCN_STATUS s_pwrDevGpuAdc13ProvColwFactorCompute(PWR_DEVICE_GPUADC13 *pGpuAdc13, PWR_DEVICE_GPUADC13_PROV *pProv)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "s_pwrDevGpuAdc13ProvColwFactorCompute");
static LwU8 s_pwrDevColwertPolicyLimitUnit(LwU8 limitUnit)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pwrDevColwertPolicyLimitUnit");
static FLCN_STATUS s_pwrDevGpuAdc13UpdateFuseParams(PWR_DEVICE_GPUADC13 *pGpuAdc13)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pwrDevGpuAdc13UpdateFuseParams");
static FLCN_STATUS s_pwrDevGpuAdc13PreLoad(PWR_DEVICE_GPUADC13 *pGpuAdc13)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pwrDevGpuAdc13PreLoad");

/* ------------------------ Static Inline Functions --------------------------*/
/*!
 * Helper function to get the pointer to @ref PWR_DEVICE_GPUADC13_PROV given a
 * provider index
 *
 * @param[in]   pGpuAdc13
 *     Pointer to @ref PWR_DEVICE_GPUADC13.
 * @param[in]   provIdx
 *     Provider index for which the structure pointer is to be returned.
 * @return
 *     Pointer to @ref PWR_DEVICE_GPUADC13_PROV if provIdx is valid
 *     NULL for invalid provIdx
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline PWR_DEVICE_GPUADC13_PROV *
pwrDevGpuAdc13ProvGet
(
    PWR_DEVICE_GPUADC13 *pGpuAdc13,
    LwU8                 provIdx
)
{
    if (provIdx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PHYSICAL_PROV_NUM)
    {
       return &pGpuAdc13->prov[provIdx];
    }

    return NULL;
}

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */

/*!
 * Construct a GPUADC13 PWR_DEVICE object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrDevGrpIfaceModel10ObjSetImpl_GPUADC13
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    PWR_DEVICE_GPUADC13                               *pGpuAdc13;
    RM_PMU_PMGR_PWR_DEVICE_DESC_GPUADC13_BOARDOBJ_SET *pGpuAdc13Set =
        (RM_PMU_PMGR_PWR_DEVICE_DESC_GPUADC13_BOARDOBJ_SET *)pBoardObjDesc;
    FLCN_STATUS                                        status = FLCN_OK;
    OSTASK_OVL_DESC                                    ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfVfe)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Construct and initialize parent-class object.
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrDevGrpIfaceModel10ObjSetImpl_GPUADC1X(pModel10, ppBoardObj, size, pBoardObjDesc),
            pwrDevGrpIfaceModel10ObjSetImpl_GPUADC13_done);

        // Sanity check the VFE VAR index
        PMU_ASSERT_TRUE_OR_GOTO(status,
            VFE_VAR_IS_VALID(pGpuAdc13Set->vcmCoarseOffsetVfeVarIdx),
            FLCN_ERR_ILWALID_STATE,
            pwrDevGrpIfaceModel10ObjSetImpl_GPUADC13_done);

        pGpuAdc13 = (PWR_DEVICE_GPUADC13 *)*ppBoardObj;

        pGpuAdc13->operatingMode     = pGpuAdc13Set->operatingMode;
        pGpuAdc13->numSamples        = pGpuAdc13Set->numSamples;
        pGpuAdc13->ovrm              = pGpuAdc13Set->ovrm;
        pGpuAdc13->ipc               = pGpuAdc13Set->ipc;
        pGpuAdc13->beacon            = pGpuAdc13Set->beacon;
        pGpuAdc13->offset            = pGpuAdc13Set->offset;
        pGpuAdc13->sum               = pGpuAdc13Set->sum;
        pGpuAdc13->adcPwm            = pGpuAdc13Set->adcPwm;
        pGpuAdc13->vcmCoarseOffsetVfeVarIdx =
            pGpuAdc13Set->vcmCoarseOffsetVfeVarIdx;

        pGpuAdc13->tupleParams.numSamples = pGpuAdc13Set->numSamples;

        //
        // LW_CPWR_THERM_ADC_IIR_VALUE_DATA is FXP11.21 (code*sample)
        // lwrrColwFactor is FXP14.4 (mA/code)
        // numSamples is FXP5.0 (sample), min=1 and max=16
        // ADC output is only 7 bits, the addition of 4 integer bits in IIR
        // value is due to multisampling so dividing by numSamples will give
        // a max of 7 integer bits in the result.
        //   11.21 code*sample /   5.0 sample  =>  7.21 code
        //    7.21 code        *  14.4 mA/code => 21.25 mA
        // 25 fractional bits will be discarded by multUFXP32
        //
        pGpuAdc13->tupleParams.lwrrShiftBits = 25;

        //
        // LW_CPWR_THERM_ADC_IIR_VALUE_DATA is FXP11.21 (code*sample)
        // voltColwFactor is FXP10.4 (mV/code)
        // voltColwFactor * 1000 is FXP20.4 (uV/code)
        // numSamples is FXP5.0 (sample), min=1 and max=16
        // ADC output is only 7 bits, the addition of 4 integer bits in IIR
        // value is due to multisampling so dividing by numSamples will give
        // a max of 7 integer bits in the result.
        //   11.21 code*sample /   5.0 sample  =>  7.21 code
        //    7.21 code        *  20.4 uV/code => 27.25 uV
        // 25 fractional bits will be discarded by multUFXP32
        //
        pGpuAdc13->tupleParams.voltShiftBits = 25;

        //
        // LW_CPWR_THERM_ADC_IIR_VALUE_DATA is FXP22.10 (code*sample^2)
        // pwrColwFactor is FXP14.4 (mW/code)
        // numSamples is FXP5.0 (sample), min=1 and max=16
        // Based on the analysis for volt and lwrr, their multiplication will
        // give 22 bits but on dividing by numSamples^2, we will be getting only
        // 14 bits in the result.
        //   22.10 code*sample^2 /   9.0 sample^2 => 14.10 code
        //   14.10 code          *  14.4 mW/code  => 28.14 mW
        // 14 fractional bits will be discarded
        //
        pGpuAdc13->tupleParams.pwrShiftBits = 14;

        //
        // Construct the channelMap and compute the colwersion factors for each
        // SW provider to point to the HW channels that it constitutes. Also,
        // this function verifies the beacon provider is specified correctly and
        // updates the offset channel provider index for Gen2
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrDevGpuAdc13ProvConstruct(pGpuAdc13, pGpuAdc13Set),
            pwrDevGrpIfaceModel10ObjSetImpl_GPUADC13_done);

        //
        // TODO-Tariq: Remove this field and all code wrapped with this check
        // once PMU SW is tested on emulation.
        //
        pGpuAdc13->bPmuConfigReg = LW_FALSE;

        // Default to LW_FALSE
        pGpuAdc13->bAdcRawCorrection = LW_FALSE;

        //
        // Set boolean for ADC_RAW_CORRECTION based on Offset configuration
        // and whether PMU needs to configure ADC registers.
        //
        if (pGpuAdc13->bPmuConfigReg &&
            (pGpuAdc13->offset.instance[0].provIdx ==
                LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE) &&
            (pGpuAdc13->offset.instance[1].provIdx ==
                LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE))
        {
            pGpuAdc13->bAdcRawCorrection = LW_TRUE;
        }

        // TODO-Tariq: Enable _BEACON_INTERRUPT after GPUADC13 is fully enabled
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BEACON_INTERRUPT))
        //
        // Set the pwrDevIdxBeacon so that beacon interrupts are routed to this
        // device
        //
        if ((pGpuAdc13->beacon.instance[0].provIdx !=
                LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE) ||
            (pGpuAdc13->beacon.instance[1].provIdx !=
                LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE))
        {
            if (Pmgr.pwrDevIdxBeacon == LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
            {
                Pmgr.pwrDevIdxBeacon = BOARDOBJ_GET_GRP_IDX(*ppBoardObj);
            }
            else
            {
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (Pmgr.pwrDevIdxBeacon == BOARDOBJ_GET_GRP_IDX(*ppBoardObj)),
                    FLCN_ERR_ILWALID_STATE,
                    pwrDevGrpIfaceModel10ObjSetImpl_GPUADC13_done);
            }
        }
#endif
    }
pwrDevGrpIfaceModel10ObjSetImpl_GPUADC13_done:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * GPUADC13 power device implementation.
 *
 * @copydoc PwrDevLoad
 */
FLCN_STATUS
pwrDevLoad_GPUADC13
(
    PWR_DEVICE  *pDev
)
{
    PWR_DEVICE_GPUADC13      *pGpuAdc13 = (PWR_DEVICE_GPUADC13 *)pDev;
    FLCN_STATUS               status    = FLCN_OK;
    PWR_DEVICE_GPUADC13_PROV *pProv;
    LwU32                     reg32;
    LwU32                     i;
    OSTASK_OVL_DESC           ovlDescListFuse[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
        OSTASK_OVL_DESC_DEFINE_VFE(FULL)
    };

    // Registers which use fuse values must be programmed first
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListFuse);
    {
        status = s_pwrDevGpuAdc13UpdateFuseParams(pGpuAdc13);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListFuse);

    PMU_ASSERT_OK_OR_GOTO(status, status, pwrDevLoad_GPUADC13_done);

    //
    // PreLoad and super-class Load functions will configure ADC registers only
    // if the boolean bPmuConfigReg is LW_TRUE. If LW_FALSE, the ADC registers
    // would have already been configured by devinit ucode and raised the PLM of
    // these registers to L3.
    //
    if (pGpuAdc13->bPmuConfigReg)
    {
        // Settings to be done before enabling ADC sensing
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrDevGpuAdc13PreLoad(pGpuAdc13),
            pwrDevLoad_GPUADC13_done);

        // Call super-class implementation
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrDevLoad_GPUADC1X(pDev),
            pwrDevLoad_GPUADC13_done);
    }

    // Setting up ADC_MULTISAMPLER. This must be done after ADC enablement.
    reg32 = REG_RD32(FECS, LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_MULTISAMPLER);
    reg32 = FLD_SET_DRF_NUM(_PTRIM_SYS_NAFLL, _SYS_PWR_SENSE_ADC_MULTISAMPLER,
        _NUM_SAMPLES, pGpuAdc13->numSamples - 1, reg32);
    reg32 = FLD_SET_DRF(_PTRIM_SYS_NAFLL, _SYS_PWR_SENSE_ADC_MULTISAMPLER,
        _ENABLE, _ON, reg32);
    REG_WR32(FECS, LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_MULTISAMPLER, reg32);

    if (pGpuAdc13->bPmuConfigReg)
    {
        // Setting up the IPC config
        for (i = 0; i < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_NUM; i++)
        {
            reg32 = 0;
            switch (pGpuAdc13->ipc.instance[i].sourceType)
            {
                case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_ADC:
                {
                    pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13,
                                pGpuAdc13->ipc.instance[i].index.provider);
                    PMU_ASSERT_TRUE_OR_GOTO(status,
                        (pProv != NULL),
                        FLCN_ERR_ILWALID_STATE,
                        pwrDevLoad_GPUADC13_done);

                    reg32 = FLD_SET_DRF(_CPWR_THERM, _IPC_CTRL, _EN, _ON, reg32);
                    switch (pGpuAdc13->ipc.instance[i].providerUnit)
                    {
                        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_MV:
                        {
                            PMU_ASSERT_TRUE_OR_GOTO(status,
                                (pwrDevGpuAdc1xProvChBusGet(&pProv->super) !=
                                     PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE),
                                FLCN_ERR_ILWALID_STATE,
                                pwrDevLoad_GPUADC13_done);

                            reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _OP,
                                LW_CPWR_THERM_IPC_CTRL_OP_CH(
                                    pwrDevGpuAdc1xProvChBusGet(&pProv->super)), reg32);
                            break;
                        }
                        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_MA:
                        {
                            PMU_ASSERT_TRUE_OR_GOTO(status,
                                (pwrDevGpuAdc1xProvChShuntGet(&pProv->super) !=
                                     PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE),
                                FLCN_ERR_ILWALID_STATE,
                                pwrDevLoad_GPUADC13_done);

                            reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _OP,
                                LW_CPWR_THERM_IPC_CTRL_OP_CH(
                                    pwrDevGpuAdc1xProvChShuntGet(&pProv->super)), reg32);
                            break;
                        }
                        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_MW:
                        {
                            PMU_ASSERT_TRUE_OR_GOTO(status,
                                (pwrDevGpuAdc1xProvChPairGet(&pProv->super) !=
                                     PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE),
                                FLCN_ERR_ILWALID_STATE,
                                pwrDevLoad_GPUADC13_done);

                            reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _OP,
                                LW_CPWR_THERM_IPC_CTRL_OP_CHP(
                                    pwrDevGpuAdc1xProvChPairGet(&pProv->super)), reg32);
                            break;
                        }
                    }
                    break;
                }
                case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_BA:
                {
                    reg32 = FLD_SET_DRF(_CPWR_THERM, _IPC_CTRL, _EN, _ON, reg32);
                    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _OP,
                        LW_CPWR_THERM_IPC_CTRL_OP_BA_W(
                            pGpuAdc13->ipc.instance[i].index.window), reg32);
                    break;
                }
                case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_ADC_SUM:
                {
                    reg32 = FLD_SET_DRF(_CPWR_THERM, _IPC_CTRL, _EN, _ON, reg32);
                    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _OP,
                        LW_CPWR_THERM_IPC_CTRL_OP_SUM(
                            pGpuAdc13->ipc.instance[i].index.sum), reg32);
                    break;
                }
                case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_NONE:
                {
                    // IPC instance is not configured/unused, so disable it
                    reg32 = FLD_SET_DRF(_CPWR_THERM, _IPC_CTRL, _EN, _OFF, reg32);
                    break;
                }
            }

            //
            // No need to set IPC_REF to a high value here since the IPC_REF
            // bounds registers will ensure that IPC_REF = 0 will never take
            // effect
            // TODO-Tariq: Add support to configure the IPC_REF bounds.
            //
            REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL(i), reg32);
        }
    }

    // Update bFirstLoad to LW_FALSE on successfully completing first load
    if (pGpuAdc13->super.bFirstLoad)
    {
        pGpuAdc13->super.bFirstLoad = LW_FALSE;
    }

pwrDevLoad_GPUADC13_done:
    return status;
}

/*!
 * GPUADC13 power device implementation.
 *
 * @copydoc PwrDevTupleGet
 */
FLCN_STATUS
pwrDevTupleGet_GPUADC13
(
    PWR_DEVICE                 *pDev,
    LwU8                        provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    PWR_DEVICE_GPUADC13      *pGpuAdc13 = (PWR_DEVICE_GPUADC13 *)pDev;
    FLCN_STATUS               status    = FLCN_OK;
    PWR_DEVICE_GPUADC13_PROV *pProv;

    // Sanity check the provIdx
    if ((provIdx >= pGpuAdc13->super.activeProvCount) &&
        (provIdx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_NUM))
    {
        //
        // provIdx points to a logical provider or a non-active physical
        // provider which is valid but we do not support pwrDevTupleGet
        // interface for this provider.
        //
        status = FLCN_OK;
        goto pwrDevTupleGet_GPUADC13_exit;
    }
    else
    {
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (provIdx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_NUM),
            FLCN_ERR_ILWALID_ARGUMENT,
            pwrDevTupleGet_GPUADC13_exit);
    }

    pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13, provIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProv != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevTupleGet_GPUADC13_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        pwrDevTupleGet_GPUADC1X(pDev, &pProv->super, pTuple, &(pGpuAdc13->tupleParams)),
        pwrDevTupleGet_GPUADC13_exit);

pwrDevTupleGet_GPUADC13_exit:
    return status;
}

/*!
 * GPUADC13 power device implementation.
 *
 * @copydoc PwrDevTupleAccGet
 */
FLCN_STATUS
pwrDevTupleAccGet_GPUADC13
(
    PWR_DEVICE                     *pDev,
    LwU8                            provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE_ACC *pTupleAcc
)
{
    PWR_DEVICE_GPUADC13      *pGpuAdc13 = (PWR_DEVICE_GPUADC13 *)pDev;
    FLCN_STATUS               status    = FLCN_OK;
    PWR_DEVICE_GPUADC13_PROV *pProv;

    // Sanity check the provIdx
    if ((provIdx >= pGpuAdc13->super.activeProvCount) &&
        (provIdx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_NUM))
    {
        //
        // provIdx points to a logical provider or a non-active physical
        // provider which is valid but we do not support pwrDevTupleAccGet
        // interface for this provider.
        //
        status = FLCN_OK;
        goto pwrDevTupleAccGet_GPUADC13_exit;
    }
    else
    {
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (provIdx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_NUM),
            FLCN_ERR_ILWALID_ARGUMENT,
            pwrDevTupleAccGet_GPUADC13_exit);
    }

    pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13, provIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProv != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevTupleAccGet_GPUADC13_exit);

    // Call super-class implementation
    status = pwrDevTupleAccGet_GPUADC1X(pDev, &pProv->super, pTupleAcc);
    if (status == FLCN_ERR_ACC_SEQ_MISMATCH)
    {
        goto pwrDevTupleAccGet_GPUADC13_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status, status, pwrDevTupleAccGet_GPUADC13_exit);

pwrDevTupleAccGet_GPUADC13_exit:
    return status;
}

/*!
 * GPUADC13 power device implementation to return full range tuple.
 *
 * @copydoc PwrDevTupleGet
 */
FLCN_STATUS
pwrDevTupleFullRangeGet_GPUADC13
(
    PWR_DEVICE                 *pDev,
    LwU8                        provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    PWR_DEVICE_GPUADC13      *pGpuAdc13 = (PWR_DEVICE_GPUADC13 *)pDev;
    FLCN_STATUS               status    = FLCN_OK;
    PWR_DEVICE_GPUADC13_PROV *pProv;

    // Sanity check the provIdx
    if ((provIdx >= pGpuAdc13->super.activeProvCount) &&
        (provIdx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_NUM))
    {
        //
        // provIdx points to a logical provider or a non-active physical
        // provider which is valid but we do not support pwrDevTupleGet
        // interface for this provider.
        //
        status = FLCN_OK;
        goto pwrDevTupleFullRangeGet_GPUADC13_exit;
    }
    else
    {
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (provIdx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_NUM),
            FLCN_ERR_ILWALID_ARGUMENT,
            pwrDevTupleFullRangeGet_GPUADC13_exit);
    }

    pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13, provIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProv != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevTupleFullRangeGet_GPUADC13_exit);

    // Call super-class implementation
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrDevTupleFullRangeGet_GPUADC1X(pDev, &pProv->super, pTuple),
        pwrDevTupleFullRangeGet_GPUADC13_exit);

pwrDevTupleFullRangeGet_GPUADC13_exit:
    return status;
}

/*!
 * GPUADC13 power device implementation.
 *
 * @copydoc PwrDevSetLimit
 */
FLCN_STATUS
pwrDevSetLimit_GPUADC13
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU8        limitIdx,
    LwU8        limitUnit,
    LwU32       limitValue
)
{
    PWR_DEVICE_GPUADC13      *pGpuAdc13 = (PWR_DEVICE_GPUADC13 *)pDev;
    FLCN_STATUS               status    = FLCN_OK;
    LwU32                     reg32     = 0;
    PWR_DEVICE_GPUADC13_PROV *pProv;
    LwU8                      devLimitUnit;

    // Sanity check limitIdx
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (limitIdx < pwrDevThresholdNumGet_GPUADC13(pDev)),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevSetLimit_GPUADC13_exit);

    // Sanity check limitUnit matches IPC(limitIdx) unit
    devLimitUnit = s_pwrDevColwertPolicyLimitUnit(limitUnit);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (devLimitUnit == pGpuAdc13->ipc.instance[limitIdx].providerUnit),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevSetLimit_GPUADC13_exit);

    // Sanity check if provIdx corresponds to the IPC(limitIdx) instance
    switch (pGpuAdc13->ipc.instance[limitIdx].sourceType)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_ADC:
        {
            // Additionally, sanity check that the provider is active
            PMU_ASSERT_TRUE_OR_GOTO(status,
                ((pGpuAdc13->ipc.instance[limitIdx].index.provider == provIdx) &&
                 (provIdx < pGpuAdc13->super.activeProvCount)),
                FLCN_ERR_ILWALID_ARGUMENT,
                pwrDevSetLimit_GPUADC13_exit);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_BA:
        {
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pGpuAdc13->ipc.instance[limitIdx].index.window == (provIdx -
                    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_BA_IPC_PROV_IDX_START)),
                FLCN_ERR_ILWALID_ARGUMENT,
                pwrDevSetLimit_GPUADC13_exit);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_ADC_SUM:
        {
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pGpuAdc13->ipc.instance[limitIdx].index.sum == (provIdx -
                     LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PHYSICAL_PROV_NUM)),
                FLCN_ERR_ILWALID_ARGUMENT,
                pwrDevSetLimit_GPUADC13_exit);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_NONE:
        default:
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                FLCN_ERR_ILWALID_ARGUMENT,
                pwrDevSetLimit_GPUADC13_exit);
        }
    }

    switch (pGpuAdc13->ipc.instance[limitIdx].sourceType)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_ADC:
        {
            pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13, provIdx);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pProv != NULL),
                FLCN_ERR_ILWALID_ARGUMENT,
                pwrDevSetLimit_GPUADC13_exit);

            if (pGpuAdc13->ipc.instance[limitIdx].providerUnit ==
                    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_MA)
            {
                //
                // IPC_REF = ((numSamples << 25) / lwrrColwFactor) * limitValue
                // Numerical analysis:
                // 5.0  sample << 25             =>  5.25 sample
                // 5.25 sample / 14.4 mA/code    =>  5.21 sample*code/mA
                // 5.21 sample*code/mA * 32.0 mA => 37.21 sample*code
                //                               => 11.21 sample*code
                //
                // Above multiplication will overflow only if limitValue is
                // larger than fullRangeLwrrent. So, checking for that will
                // avoid overflow situation.
                //
                // For limitValue larger than fullRangeLwrrent, set IPC_REF to
                // max possible value.
                //
                if (limitValue > pwrDevGpuAdc13FullRangeLwrrmAGet(pGpuAdc13, pProv))
                {
                    reg32 = LW_U32_MAX;
                }
                else
                {
                    reg32 = LW_TYPES_U32_TO_UFXP_X_Y(7, 25, pGpuAdc13->numSamples);
                    reg32 = LW_UNSIGNED_ROUNDED_DIV(reg32, pProv->super.lwrrColwFactor);
                    reg32 = reg32 * limitValue;
                }
            }
            else if (pGpuAdc13->ipc.instance[limitIdx].providerUnit ==
                         LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_MW)
            {
                //
                // IPC_REF = ((numSamples^2 << 14) / pwrColwFactor) * limitValue
                // Numerical analysis:
                // 9.0  sample^2 << 14             =>  9.14 sample^2
                // 9.14 sample^2 /  28.4 mW/code   =>  9.10 sample^2*code/mW
                // 9.10 sample^2*code/mW * 32.0 mW => 41.10 sample^2*code
                //                                 => 22.10 sample^2*code
                //
                // Above multiplication will overflow only if limitValue is
                // larger than fullRangeLwrrent * fullRangeVoltage. So, checking
                // for that will avoid overflow situation.
                //
                // For limitValue larger than fullRangePower, set IPC_REF to
                // max possible value.
                //
                if (limitValue >
                        pwrDevGpuAdc13FullRangePwrmWGet(pGpuAdc13, pProv))
                {
                    reg32 = LW_U32_MAX;
                }
                else
                {
                    reg32 = LW_TYPES_U32_TO_UFXP_X_Y(18, 14,
                        (pGpuAdc13->numSamples * pGpuAdc13->numSamples));
                    reg32 = LW_UNSIGNED_ROUNDED_DIV(reg32, pProv->super.pwrColwFactor);
                    reg32 = reg32 * limitValue;
                }
            }
            REG_WR32(CSB, LW_CPWR_THERM_IPC_REF(limitIdx), reg32);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_BA:
        {
            REG_WR32(CSB, LW_CPWR_THERM_IPC_REF(limitIdx), limitValue);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_ADC_SUM:
        {
            LwU8  sumIdx = pGpuAdc13->ipc.instance[limitIdx].index.sum;
            LwU8  i;

            if (pGpuAdc13->ipc.instance[limitIdx].providerUnit ==
                    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_MA)
            {
                LwU32 fullRangeLwrrentmASum = 0;
                for (i = 0;
                     i < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_INPUT_NUM;
                     i++)
                {
                    if (pGpuAdc13->sum.instance[sumIdx].input[i].provIdx !=
                            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE)
                    {
                        pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13,
                            pGpuAdc13->sum.instance[sumIdx].input[i].provIdx);
                        PMU_ASSERT_TRUE_OR_GOTO(status,
                            (pProv != NULL),
                            FLCN_ERR_ILWALID_ARGUMENT,
                            pwrDevSetLimit_GPUADC13_exit);

                        fullRangeLwrrentmASum +=
                            pwrDevGpuAdc13FullRangeLwrrmAGet(pGpuAdc13, pProv);
                    }
                }

                //
                // IPC_REF = ((numSamples << 25) / colwFactor.lwrr) * limitValue
                // Numerical analysis:
                // 5.0  sample << 25             =>  5.25 sample
                // 5.25 sample / 14.4 mA/code    =>  5.21 sample*code/mA
                // 5.21 sample*code/mA * 32.0 mA => 37.21 sample*code
                //                               => 11.21 sample*code
                //
                // Above multiplication will overflow only if limitValue is
                // larger than fullRangeLwrrent. So, checking for that will
                // avoid overflow situation.
                //
                // For limitValue larger than fullRangeLwrrent, set IPC_REF to
                // max possible value.
                //
                if (limitValue > fullRangeLwrrentmASum)
                {
                    reg32 = LW_U32_MAX;
                }
                else
                {
                    reg32 = LW_TYPES_U32_TO_UFXP_X_Y(7, 25, pGpuAdc13->numSamples);
                    reg32 = LW_UNSIGNED_ROUNDED_DIV(reg32,
                        pGpuAdc13->sum.instance[sumIdx].colwFactor.lwrr);
                    reg32 = reg32 * limitValue;
                }
            }
            else if (pGpuAdc13->ipc.instance[limitIdx].providerUnit ==
                         LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_MW)
            {
                LwU32 fullRangePowermWSum = 0;
                for (i = 0;
                     i < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_INPUT_NUM;
                     i++)
                {
                    if (pGpuAdc13->sum.instance[sumIdx].input[i].provIdx !=
                            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE)
                    {
                        pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13,
                            pGpuAdc13->sum.instance[sumIdx].input[i].provIdx);
                        PMU_ASSERT_TRUE_OR_GOTO(status,
                            (pProv != NULL),
                            FLCN_ERR_ILWALID_ARGUMENT,
                            pwrDevSetLimit_GPUADC13_exit);

                        fullRangePowermWSum +=
                            pwrDevGpuAdc13FullRangePwrmWGet(pGpuAdc13, pProv);
                    }
                }

                //
                // IPC_REF = ((numSamples^2 << 14) / colwFactor.pwr) * limitValue
                // Numerical analysis:
                // 9.0  sample^2 << 14             =>  9.14 sample^2
                // 9.14 sample^2 /  28.4 mW/code   =>  9.10 sample^2*code/mW
                // 9.10 sample^2*code/mW * 32.0 mW => 41.10 sample^2*code
                //                                 => 22.10 sample^2*code
                //
                // Above multiplication will overflow only if limitValue is
                // larger than fullRangeLwrrent * fullRangeVoltage. So, checking
                // for that will avoid overflow situation.
                //
                // For limitValue larger than fullRangePower, set IPC_REF to
                // max possible value.
                //
                if (limitValue > fullRangePowermWSum)
                {
                    reg32 = LW_U32_MAX;
                }
                else
                {
                    reg32 = LW_TYPES_U32_TO_UFXP_X_Y(18, 14,
                        (pGpuAdc13->numSamples * pGpuAdc13->numSamples));
                    reg32 = LW_UNSIGNED_ROUNDED_DIV(reg32,
                        pGpuAdc13->sum.instance[sumIdx].colwFactor.pwr);
                    reg32 = reg32 * limitValue;
                }
            }
            REG_WR32(CSB, LW_CPWR_THERM_IPC_REF(limitIdx), reg32);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_NONE:
        default:
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                FLCN_ERR_ILWALID_ARGUMENT,
                pwrDevSetLimit_GPUADC13_exit);
        }
    }

pwrDevSetLimit_GPUADC13_exit:
    return status;
}

/*!
 * GPUADC13 power device implementation.
 *
 * @copydoc PwrDevGetLimit
 */
FLCN_STATUS
pwrDevGetLimit_GPUADC13
(
    PWR_DEVICE            *pDev,
    LwU8                   provIdx,
    LwU8                   limitIdx,
    RM_PMU_BOARDOBJ       *pBuf
)
{
    PWR_DEVICE_GPUADC13 *pGpuAdc13 = (PWR_DEVICE_GPUADC13 *)pDev;
    RM_PMU_PMGR_PWR_DEVICE_BOARDOBJ_GET_STATUS *pStatus =
        (RM_PMU_PMGR_PWR_DEVICE_BOARDOBJ_GET_STATUS *)pBuf;
    FLCN_STATUS          status    = FLCN_OK;
    LwU32                reg32;

    // Sanity check limitIdx
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (limitIdx < pwrDevThresholdNumGet_GPUADC13(pDev)),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevGetLimit_GPUADC13_exit);

    // Return early if provIdx doesn't match-up with the limitIdx
    switch (pGpuAdc13->ipc.instance[limitIdx].sourceType)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_ADC:
        {
            // Additionally, sanity check that the provider is active
            if ((pGpuAdc13->ipc.instance[limitIdx].index.provider != provIdx) ||
                (provIdx >= pGpuAdc13->super.activeProvCount))
            {
                pStatus->providers[provIdx].thresholds[limitIdx] = 0;
                goto pwrDevGetLimit_GPUADC13_exit;
            }
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_BA:
        {
            if (pGpuAdc13->ipc.instance[limitIdx].index.window != (provIdx -
                    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_BA_IPC_PROV_IDX_START))
            {
                pStatus->providers[provIdx].thresholds[limitIdx] = 0;
                goto pwrDevGetLimit_GPUADC13_exit;
            }
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_ADC_SUM:
        {
            if (pGpuAdc13->ipc.instance[limitIdx].index.sum != (provIdx -
                    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PHYSICAL_PROV_NUM))
            {
                pStatus->providers[provIdx].thresholds[limitIdx] = 0;
                goto pwrDevGetLimit_GPUADC13_exit;
            }
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_NONE:
        {
            pStatus->providers[provIdx].thresholds[limitIdx] = 0;
            goto pwrDevGetLimit_GPUADC13_exit;
        }
        default:
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                FLCN_ERR_ILWALID_ARGUMENT,
                pwrDevGetLimit_GPUADC13_exit);
        }
    }

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_IPC_REF(limitIdx));
    switch (pGpuAdc13->ipc.instance[limitIdx].sourceType)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_ADC:
        {
            PWR_DEVICE_GPUADC13_PROV *pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13,
                pGpuAdc13->ipc.instance[limitIdx].index.provider);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pProv != NULL),
                FLCN_ERR_ILWALID_STATE,
                pwrDevGetLimit_GPUADC13_exit);

            if ((pGpuAdc13->ipc.instance[limitIdx].providerUnit ==
                     LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_MA))
            {
                //
                // LW_CPWR_THERM_IPC_REF is FXP11.21 (code*sample)
                // lwrrColwFactor is FXP14.4 (mA/code)
                // numSamples is FXP5.0 (sample), min=1 and max=16
                // ADC output is only 7 bits, the addition of 4 integer bits in
                // IIR value is due to multisampling so dividing by numSamples
                // will give a max of 7 integer bits in the result.
                //   11.21 code*sample /   5.0 sample  =>  7.21 code
                //    7.21 code        *  14.4 mA/code => 21.25 mA
                // 25 fractional bits will be discarded by multUFXP32
                //
                reg32 = LW_UNSIGNED_ROUNDED_DIV(reg32, pGpuAdc13->numSamples);
                reg32 = multUFXP32(25, reg32, pProv->super.lwrrColwFactor);
            }
            else if ((pGpuAdc13->ipc.instance[limitIdx].providerUnit ==
                          LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_MW))
            {
                //
                // LW_CPWR_THERM_IPC_REF is FXP22.10 (code*sample^2)
                // pwrColwFactor is FXP14.4 (mW/code)
                // numSamples is FXP5.0 (sample), min=1 and max=16
                // Based on the analysis for volt and lwrr, their multiplication
                // will give 22 bits but on dividing by numSamples^2, we will be
                // getting only 14 bits in the result.
                //   22.10 code*sample^2 /   9.0 sample^2 => 14.10 code
                //   14.10 code          *  14.4 mW/code  => 28.14 mW
                // 14 fractional bits will be discarded
                //
                reg32 = LW_UNSIGNED_ROUNDED_DIV(reg32,
                        (pGpuAdc13->numSamples * pGpuAdc13->numSamples));
                reg32 = multUFXP32(14, reg32, pProv->super.pwrColwFactor);
            }
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_BA:
        {
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_SRC_TYPE_ADC_SUM:
        {
            if ((pGpuAdc13->ipc.instance[limitIdx].providerUnit ==
                     LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_MA))
            {
                //
                // LW_CPWR_THERM_IPC_REF is FXP11.21 (code*sample)
                // colwFactor.lwrr is FXP14.4 (mA/code)
                // numSamples is FXP5.0 (sample), min=1 and max=16
                // SUM output is only 7 bits, the addition of 4 integer bits in
                // IIR value is due to multisampling so dividing by numSamples
                // will give a max of 7 integer bits in the result.
                //   11.21 code*sample /   5.0 sample  =>  7.21 code
                //    7.21 code        *  14.4 mA/code => 21.25 mA
                // 25 fractional bits will be discarded by multUFXP32
                //
                reg32 = LW_UNSIGNED_ROUNDED_DIV(reg32, pGpuAdc13->numSamples);
                reg32 = multUFXP32(25, reg32,
                    pGpuAdc13->sum.instance[pGpuAdc13->ipc.instance[limitIdx].index.sum].colwFactor.lwrr);
            }
            else if ((pGpuAdc13->ipc.instance[limitIdx].providerUnit ==
                          LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_MW))
            {
                //
                // LW_CPWR_THERM_IPC_REF is FXP22.10 (code*sample^2)
                // colwFactor.pwr is FXP14.4 (mW/code)
                // numSamples is FXP5.0 (sample), min=1 and max=16
                // Based on the analysis for volt and lwrr, their multiplication
                // will give 22 bits but on dividing by numSamples^2, we will be
                // getting only 14 bits in the result.
                //   22.10 code*sample^2 /   9.0 sample^2 => 14.10 code
                //   14.10 code          *  14.4 mW/code  => 28.14 mW
                // 14 fractional bits will be discarded
                // To handle overflow, ensure that the IPC_REF value is not
                // greater than fullRangePowermWSum represented in ADC code.
                //
                PWR_DEVICE_GPUADC13_PROV *pProv;
                LwU32 fullRangePowermWSum = 0;
                LwU32 temp;
                LwU8  sumIdx = pGpuAdc13->ipc.instance[limitIdx].index.sum;
                LwU8  i;
                for (i = 0;
                     i < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_INPUT_NUM;
                     i++)
                {
                    if (pGpuAdc13->sum.instance[sumIdx].input[i].provIdx !=
                            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE)
                    {
                        pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13,
                            pGpuAdc13->sum.instance[sumIdx].input[i].provIdx);
                        PMU_ASSERT_TRUE_OR_GOTO(status,
                            (pProv != NULL),
                            FLCN_ERR_ILWALID_ARGUMENT,
                            pwrDevGetLimit_GPUADC13_exit);

                        fullRangePowermWSum +=
                            pwrDevGpuAdc13FullRangePwrmWGet(pGpuAdc13, pProv);
                    }
                }

                //
                // This computation is copied from setLimit to find IPC_REF
                // corresponding to fullRangePowermWSum
                //
                temp = LW_TYPES_U32_TO_UFXP_X_Y(18, 14,
                    (pGpuAdc13->numSamples * pGpuAdc13->numSamples));
                temp = LW_UNSIGNED_ROUNDED_DIV(temp,
                    pGpuAdc13->sum.instance[sumIdx].colwFactor.pwr);
                temp = temp * fullRangePowermWSum;
                if (reg32 > temp)
                {
                    //
                    // IPC_REF value is greater than max possible value as per
                    // VBIOS configuration, return LW_U32_MAX
                    //
                    reg32 = LW_U32_MAX;
                }
                else
                {
                    reg32 = LW_UNSIGNED_ROUNDED_DIV(reg32,
                            (pGpuAdc13->numSamples * pGpuAdc13->numSamples));
                    reg32 = multUFXP32(14, reg32,
                        pGpuAdc13->sum.instance[pGpuAdc13->ipc.instance[limitIdx].index.sum].colwFactor.pwr);
                }
            }
            break;
        }
    }

    pStatus->providers[provIdx].thresholds[limitIdx] = reg32;

pwrDevGetLimit_GPUADC13_exit:
    return status;
}

/* ------------------------- PWR_DEVICE_GPUADC1X Interfaces ----------------- */
/*!
 * GPUADC13 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xBaSetLimit
 */
FLCN_STATUS
pwrDevGpuAdc1xBaSetLimit_GPUADC13
(
    PWR_DEVICE_GPUADC1X *pGpuAdc1x,
    LwU8                 baWindowIdx,
    LwU8                 limitIdx,
    LwU8                 limitUnit,
    LwU32                limitValue
)
{
    LwU8 provIdx;

    //
    // Translate the baWindowIdx to GPUADC provIdx based on the spec defined at
    // https://confluence.lwpu.com/display/RMPWR/GPUADC+1.3+-+Software+Architecture+and+Design+Document#GPUADC1.3-SoftwareArchitectureandDesignDolwment-LogicalProviders
    //
    provIdx = baWindowIdx +
        LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_BA_IPC_PROV_IDX_START;

    return pwrDevSetLimit_GPUADC13(&pGpuAdc1x->super, provIdx, limitIdx,
               limitUnit, limitValue);
}

/*!
 * GPUADC13 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xFullRangeVoltmVGet
 */
LwU32
pwrDevGpuAdc1xFullRangeVoltmVGet_GPUADC13
(
    PWR_DEVICE_GPUADC1X      *pGpuAdc1x,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    PWR_DEVICE_GPUADC13      *pGpuAdc13 = (PWR_DEVICE_GPUADC13 *)pGpuAdc1x;
    PWR_DEVICE_GPUADC13_PROV *pProv13   = (PWR_DEVICE_GPUADC13_PROV *)pProv;

    return pwrDevGpuAdc13FullRangeVoltmVGet(pGpuAdc13, pProv13);
}

/*!
 * GPUADC13 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xFullRangeLwrrmAGet
 */
LwU32
pwrDevGpuAdc1xFullRangeLwrrmAGet_GPUADC13
(
    PWR_DEVICE_GPUADC1X      *pGpuAdc1x,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    PWR_DEVICE_GPUADC13      *pGpuAdc13 = (PWR_DEVICE_GPUADC13 *)pGpuAdc1x;
    PWR_DEVICE_GPUADC13_PROV *pProv13   = (PWR_DEVICE_GPUADC13_PROV *)pProv;

    return pwrDevGpuAdc13FullRangeLwrrmAGet(pGpuAdc13, pProv13);
}

/*!
 * GPUADC13 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xFullRangePwrmWGet
 */
LwU32
pwrDevGpuAdc1xFullRangePwrmWGet_GPUADC13
(
    PWR_DEVICE_GPUADC1X      *pGpuAdc1x,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    PWR_DEVICE_GPUADC13      *pGpuAdc13 = (PWR_DEVICE_GPUADC13 *)pGpuAdc1x;
    PWR_DEVICE_GPUADC13_PROV *pProv13   = (PWR_DEVICE_GPUADC13_PROV *)pProv;

    return pwrDevGpuAdc13FullRangePwrmWGet(pGpuAdc13, pProv13);
}

/*!
 * GPUADC13 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xVoltColwFactorGet
 */
LwUFXP28_4
pwrDevGpuAdc1xVoltColwFactorGet_GPUADC13
(
    PWR_DEVICE_GPUADC1X      *pGpuAdc1x,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    PWR_DEVICE_GPUADC13_PROV *pProv13 = (PWR_DEVICE_GPUADC13_PROV *)pProv;

    return pProv13->voltColwFactor;
}

/*!
 * GPUADC13 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xSwResetLengthSet
 */
void
pwrDevGpuAdc1xSwResetLengthSet_GPUADC13
(
    PWR_DEVICE_GPUADC1X *pGpuAdc1x
)
{
    LwU32 reg32;

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_RESET);
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_RESET, _LENGTH,
                pGpuAdc1x->resetLength, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_RESET, reg32);
}

/*!
 * GPUADC13 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xSnapTrigger
 */
void
pwrDevGpuAdc1xSnapTrigger_GPUADC13
(
    PWR_DEVICE_GPUADC1X *pGpuAdc1x
)
{
    LwU32 reg32;

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_SNAPSHOT);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _ADC_SNAPSHOT, _SNAP, _TRIGGER, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_SNAPSHOT, reg32);
}

/*!
 * GPUADC13 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xSwResetTrigger
 */
void
pwrDevGpuAdc1xSwResetTrigger_GPUADC13
(
    PWR_DEVICE_GPUADC1X *pGpuAdc1x
)
{
    LwU32 reg32;

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_RESET);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _ADC_RESET, _SW_RST, _TRIGGER, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_RESET, reg32);
}

/* ------------------------- Static Functions ------------------------------- */
/*!
 * Helper function to populate the channelMap and compute colwersion factor for
 * each provider.
 * The channelMap stores the information regarding which HW channels constitutes
 * the bus, shunt and pair (multiplier) for a SW provider.
 * Additionaly, this function also designates which providers will serve as the
 * offset channels and verifies that the beacon channels are correct as per the
 * VBIOS.
 *
 * Examples of channel programming can be found here:
 * TODO-Tariq: Add link to document detailing channel programming examples
 *
 * @param[in/out]  pGpuAdc13 Pointer to PWR_DEVICE_GPUADC13
 * @return
 *     FLCN_OK     Operation completed successfully
 *     FLCN_ERR_NOT_SUPPORTED
 *                 OVRM device gen not supported
 *     FLCN_ERR_ILWALID_STATE
 *                 Beacon provider index does not match ground
 *                 or fullRangeLwrrent for a provider index is set to 0
 *                 or provider configuration is invalid
 */
static FLCN_STATUS
s_pwrDevGpuAdc13ProvConstruct
(
    PWR_DEVICE_GPUADC13 *pGpuAdc13,
    RM_PMU_PMGR_PWR_DEVICE_DESC_GPUADC13_BOARDOBJ_SET *pGpuAdc13Set
)
{
    FLCN_STATUS               status    = FLCN_OK;
    LwU8                      devIdx    = 0;
    LwU8                      provIdx   = 0;
    LwU8                      chIdx     = 0;
    LwU8                      chPairIdx = 0;
    PWR_DEVICE_GPUADC13_PROV *pProv;

    for (devIdx = 0;
         devIdx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_OVRM_NUM;
         devIdx++)
    {
        if (pGpuAdc13->ovrm.device[devIdx].gen ==
            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_OVRM_GEN_2)
        {
            LwU8 i;
            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_OVRM_DATA_GEN2 *pGen2 =
                &(pGpuAdc13->ovrm.device[devIdx].data.gen2);

            for (i = 0; i < pGen2->tupleProvNum; i++)
            {
                pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13, provIdx);

                // Sanity check the pointer and fullRangeLwrrent/Voltage
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    ((pProv != NULL) &&
                     (pGpuAdc13Set->fullRangeLwrrent[provIdx] != 0) &&
                     (pGpuAdc13->super.fullRangeVoltage != 0)),
                    FLCN_ERR_ILWALID_STATE,
                    s_pwrDevGpuAdc13ProvConstruct_exit);

                pProv->super.fullRangeLwrrent = pGpuAdc13Set->fullRangeLwrrent[provIdx];
                pProv->fullRangeVoltage = pGpuAdc13->super.fullRangeVoltage;

                PMU_ASSERT_OK_OR_GOTO(status,
                    s_pwrDevGpuAdc13ProvColwFactorCompute(pGpuAdc13, pProv),
                    s_pwrDevGpuAdc13ProvConstruct_exit);

                //
                // The ordering of the bus and shunt channels is important here.
                // As per the GPUADC1X spec, bus channel always comes first.
                //
                pProv->super.channelMap.bus   = chIdx++;
                pProv->super.channelMap.shunt = chIdx++;
                pProv->super.channelMap.pair  = chPairIdx++;
                provIdx++;
            }

            if (pGen2->imon.bEnabled)
            {
                pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13, provIdx);

                // Sanity check the pointer and fullRangeLwrrent/Voltage
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    ((pProv != NULL) &&
                     (pGpuAdc13Set->fullRangeLwrrent[provIdx] != 0) &&
                     (pGen2->imon.fullRangeVoltage != 0)),
                    FLCN_ERR_ILWALID_STATE,
                    s_pwrDevGpuAdc13ProvConstruct_exit);

                pProv->super.fullRangeLwrrent = pGpuAdc13Set->fullRangeLwrrent[provIdx];
                pProv->fullRangeVoltage = pGen2->imon.fullRangeVoltage;

                PMU_ASSERT_OK_OR_GOTO(status,
                    s_pwrDevGpuAdc13ProvColwFactorCompute(pGpuAdc13, pProv),
                    s_pwrDevGpuAdc13ProvConstruct_exit);

                //
                // The ordering of the bus and shunt channels is important here.
                // As per the GPUADC1X spec, bus channel always comes first.
                //
                pProv->super.channelMap.bus   = chIdx++;
                pProv->super.channelMap.shunt = chIdx++;
                pProv->super.channelMap.pair  = chPairIdx++;
                provIdx++;
            }

            if (pGen2->bGround)
            {
                pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13, provIdx);

                // Sanity check the pointer and fullRangeLwrrent/Voltage
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    ((pProv != NULL) &&
                     (pGpuAdc13Set->fullRangeLwrrent[provIdx] != 0) &&
                     (pGpuAdc13->super.fullRangeVoltage != 0)),
                    FLCN_ERR_ILWALID_STATE,
                    s_pwrDevGpuAdc13ProvConstruct_exit);

                pProv->super.fullRangeLwrrent = pGpuAdc13Set->fullRangeLwrrent[provIdx];
                pProv->fullRangeVoltage = pGpuAdc13->super.fullRangeVoltage;

                PMU_ASSERT_OK_OR_GOTO(status,
                    s_pwrDevGpuAdc13ProvColwFactorCompute(pGpuAdc13, pProv),
                    s_pwrDevGpuAdc13ProvConstruct_exit);

                // Beacon channel is always the bus channel in a provider
                pProv->super.channelMap.bus = chIdx++;
                pProv->super.channelMap.shunt =
                    PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE;
                pProv->super.channelMap.pair  =
                    PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE;

                // Cross check the beacon provider index with ground
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (pGpuAdc13->beacon.instance[devIdx].provIdx == provIdx),
                    FLCN_ERR_ILWALID_STATE,
                    s_pwrDevGpuAdc13ProvConstruct_exit);

                // Set the offset provider index to ground
                pGpuAdc13->offset.instance[devIdx].provIdx = provIdx;
                provIdx++;
            }
            else
            {
                // No offset channel
                pGpuAdc13->offset.instance[devIdx].provIdx =
                    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE;
            }
        }
        else if (pGpuAdc13->ovrm.device[devIdx].gen ==
            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_OVRM_GEN_1)
        {
            //
            // For Gen1, we iterate over all providers up to the activeProvCount
            // which is an overall count and not per device, hence we do not
            // need to iterate over all devices. The 'break' below ensures this
            // behavior.
            //
            for (provIdx = 0;
                 provIdx < pGpuAdc13->super.activeProvCount;
                 provIdx++)
            {
                pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13, provIdx);

                // Sanity check the pointer and fullRangeLwrrent/Voltage
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    ((pProv != NULL) &&
                     (pGpuAdc13Set->fullRangeLwrrent[provIdx] != 0) &&
                     (pGpuAdc13->super.fullRangeVoltage != 0)),
                    FLCN_ERR_ILWALID_STATE,
                    s_pwrDevGpuAdc13ProvConstruct_exit);

                pProv->super.fullRangeLwrrent = pGpuAdc13Set->fullRangeLwrrent[provIdx];
                pProv->fullRangeVoltage = pGpuAdc13->super.fullRangeVoltage;

                PMU_ASSERT_OK_OR_GOTO(status,
                    s_pwrDevGpuAdc13ProvColwFactorCompute(pGpuAdc13, pProv),
                    s_pwrDevGpuAdc13ProvConstruct_exit);

                pProv->super.channelMap.bus   = provIdx*2;
                pProv->super.channelMap.shunt = provIdx*2 + 1;
                pProv->super.channelMap.pair  = provIdx;
            }

            for (provIdx = pGpuAdc13->super.activeProvCount;
                 provIdx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PHYSICAL_PROV_NUM;
                 provIdx++)
            {
                pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13, provIdx);
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (pProv != NULL),
                    FLCN_ERR_ILWALID_STATE,
                    s_pwrDevGpuAdc13ProvConstruct_exit);

                pProv->super.channelMap.bus   =
                    PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE;
                pProv->super.channelMap.shunt =
                    PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE;
                pProv->super.channelMap.pair  =
                    PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE;
            }
            break;
        }
        else if (pGpuAdc13->ovrm.device[devIdx].gen ==
            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_OVRM_GEN_ILWALID)
        {
            //
            // This device is not to be used or does not exist on board
            // Set the offset provIdx to _NONE and cross check that beacon
            // provIdx is configured as _NONE.
            //
            pGpuAdc13->offset.instance[devIdx].provIdx =
                LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE;
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pGpuAdc13->beacon.instance[devIdx].provIdx ==
                     LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE),
                FLCN_ERR_ILWALID_STATE,
                s_pwrDevGpuAdc13ProvConstruct_exit);
        }
        else
        {
            // Unsupported OVRM Gen
            PMU_ASSERT_OK_OR_GOTO(status,
                FLCN_ERR_NOT_SUPPORTED,
                s_pwrDevGpuAdc13ProvConstruct_exit);
        }
    }

s_pwrDevGpuAdc13ProvConstruct_exit:
    return status;
}

/*!
 * Helper function to compute the colwersion factors for voltage, current
 * and power for each provider.
 * Additionaly, this function also pre-computes and saves the scaleFactor and
 * shiftBits that would be required when colwerting the ADC_ACC code to physical
 * units for voltage, current, power and energy.
 *
 * @param[in/out]  pGpuAdc13 Pointer to PWR_DEVICE_GPUADC13
 * @param[in/out]  pProv     Pointer to PWR_DEVICE_GPUADC13_PROV
 * @return
 *     FLCN_OK     Operation completed successfully
 */
static FLCN_STATUS
s_pwrDevGpuAdc13ProvColwFactorCompute
(
    PWR_DEVICE_GPUADC13      *pGpuAdc13,
    PWR_DEVICE_GPUADC13_PROV *pProv
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       temp;
    LwU32       fullRangeLwrrentmA;
    LwU32       fullRangeVoltagemV;

    // Colwert fullRangeLwrrent to mA
    fullRangeLwrrentmA = pwrDevGpuAdc13FullRangeLwrrmAGet(pGpuAdc13, pProv);

    //
    // Callwlate lwrrColwFactor (mA/code)
    // lwrrColwFactor = fullRangeLwrrentmA (mA) / adcMaxValue (code)
    // fullRangeLwrrentmA is FXP32.0 but we don't expect the value to be
    // practically greater than 21 bits (~2000A) so using FXP21.0 for
    // numerical analysis.
    // adcMaxValue of 0x7F is FXP7.0
    //   (21.0 mA << 4) / 7.0 code => 14.4 mA/code
    //
    pProv->super.lwrrColwFactor = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(
        28, 4, fullRangeLwrrentmA, pGpuAdc13->super.adcMaxValue);

    //
    // LW_CPWR_THERM_ADC_ACC_VALUE_DATA is FXP43.21 (code*sample*cycle)
    // lwrrColwFactor is FXP14.4 (mA/code)
    // accColwusPerCycle is FXP4.12 (us/cycle)
    // numSamples is FXP5.0 (sample), min=1 and max=16
    // scaling value: lwrrColwFactor * accColwusPerCycle / numSamples
    //
    // 14.4 mA/code  * 4.12 us/cycle => 18.16 mA*us/code/cycle
    // 2 fractional bits need to be discarded by multUFXP32
    // 18.16 mA*us/code/cycle >> 2   => 18.14 mA*us/code/cycle
    // Max integer bits in result of division will be based on min value
    // of divisor which in this case is 1.
    // 18.14 mA*us/code/cycle / 5.0 sample
    //                               => 18.14 mA*us/code/cycle/sample
    //
    // 43.21 code*sample*cycle *  18.14 mA*us/code/cycle/sample
    //                               => 61.35 mA*us
    // 61.35 mA*us             >> 35 => 61.0  mA*us
    // shiftBits = 35 to discard the fractional bits after scaling
    //
    temp = multUFXP32(2, pProv->super.lwrrColwFactor,
        pGpuAdc13->super.accColwusPerCycle);
    pProv->super.lwrr.scaleFactor = LW_UNSIGNED_ROUNDED_DIV(temp,
        pGpuAdc13->numSamples);
    pProv->super.lwrr.shiftBits   = 35;

    // Colwert fullRangeVoltage to mV
    fullRangeVoltagemV = pwrDevGpuAdc13FullRangeVoltmVGet(pGpuAdc13, pProv);

    //
    // Dividing the fullRangeVoltagemV by adcMaxValue and scaling it to FXP28.4
    // Considering fullRangeVoltage of 64V (0x4000 in FXP8.8),
    // voltColwFactor is 0x3F7D mV/code which is effectively FXP10.4
    //
    pProv->voltColwFactor = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(28, 4,
        fullRangeVoltagemV, pGpuAdc13->super.adcMaxValue);

    //
    // LW_CPWR_THERM_ADC_ACC_VALUE_DATA is FXP43.21 (code*sample*cycle)
    // voltColwFactor is FXP10.4 (mV/code)
    // accColwusPerCycle is FXP4.12 (us/cycle)
    // numSamples is FXP5.0 (sample), min=1 and max=16
    // scaling value:  voltColwFactor * accColwusPerCycle / numSamples
    //
    // 10.4  mV/code * 4.12 us/cycle => 14.16 mV*us/code/cycle
    // Max integer bits in result of division will be based on min value
    // of divisor which in this case is 1.
    // 14.16 mV*us/code/cycle / 5.0 sample
    //                               => 14.16 mV*us/code/cycle/sample
    //
    // 43.21 code*sample*cycle *  14.16 mV*us/code/cycle/sample
    //                               => 57.37 mV*us
    // 57.37 mV*us             >> 37 => 57.0  mV*us
    // shiftBits = 37 to discard the fractional bits after scaling
    //
    temp = pProv->voltColwFactor *
        pGpuAdc13->super.accColwusPerCycle;
    pProv->super.volt.scaleFactor = LW_UNSIGNED_ROUNDED_DIV(temp,
        pGpuAdc13->numSamples);
    pProv->super.volt.shiftBits   = 37;

    //
    // Callwlate pwrColwFactor (mW/code)
    // pwrColwFactor (mW/code)
    //     = lwrrColwFactor (mA/code) * voltColwFactor (mV/code) / 1000
    //   14.4 mA/code *  10.4 mV/code => 24.8 uW/code
    //   24.8 uW/code /  10.0 mW/uW   => 14.8 mW/code
    // Discarding 4 fractional bits
    //   14.8 mW/code >> 4            => 14.4 mW/code
    //
    pProv->super.pwrColwFactor = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(
        28, 4, ((pProv->super.lwrrColwFactor *
                 pProv->voltColwFactor) / (LwU32)1000));

    //
    // LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA is
    //     FXP54.10 (code*sample^2*cycle)
    // pwrColwFactor is FXP14.4 (mW/code)
    // accColwusPerCycle is FXP4.12 (us/cycle)
    // numSamples^2 is FXP9.0 (sample), min=1 and max=256
    // scaling value: pwrColwFactor * accColwusPerCycle / (numSamples)^2
    //
    // 14.4 mW/code  * 4.12 us/cycle  => 18.16 mW*us/code/cycle
    // 2 fractional bits need to be discarded by multUFXP32
    // 18.16 mW*us/code/cycle >> 2    => 18.14 mW*us/code/cycle
    // Max integer bits in result of division will be based on min value
    // of divisor which in this case is 1.
    // 18.14 mW*us/code/cycle  / 9.0 sample^2
    //                                => 18.14 mW*us/code/cycle/sample^2
    //
    // 54.10 code*sample^2*cycle * 18.14 mW*us/code/cycle/sample^2
    //                                => 72.24 mW*us
    // 72.24 mW*us              >> 24 => 72.0  mW*us
    // shiftBits = 24 to discard the fractional bits after scaling
    //
    // 72.0 means there is overflow of 8 bits. Minimum time taken to
    // overflow will be when ADC_MUL_ACC_VALUE overflows 46 bits while
    // aclwmulating the maximum ADC code value which is 22 bits.
    // Analysis to callwlate time taken to overflow.
    // 46 bits => 0x00003FFF FFFFFFFF = a
    // 22 bits => 0x00000000 003FFFFF = b
    // numCycles = a / b = 0x01000004 = 16777220 cycles
    // accColwusPerCycle on TU102 = 5.44 us/cycle
    // numCycles * accColwusPerCycle = 16777220 * 5.44 us
    //                               = 91342642.22     us
    //                               = 91.34 seconds
    // Given that acc value will overflow in 91 seconds for the above
    // value of accColwusPerCycle, we can safely assume that overflow
    // will not occur since at worst low power sampling will be 1s.
    //
    temp = multUFXP32(2, pProv->super.pwrColwFactor,
        pGpuAdc13->super.accColwusPerCycle);
    pProv->super.pwr.scaleFactor = LW_UNSIGNED_ROUNDED_DIV(temp,
        (pGpuAdc13->numSamples * pGpuAdc13->numSamples));
    pProv->super.pwr.shiftBits   = 24;

    //
    // Colwerting pwrAcc (nJ) to energy (mJ)
    // energymJ = pwrAcc (nJ) / 1000000
    //          = pwrAcc (nJ) * (1/1000000)
    //          = pwrAcc (nJ) * scaleFactor
    // scaleFactor = 1/1000000 represented as FXP 8.24
    // pwrAcc is 64.0 and multiplication by scaleFactor is roughly equivalent to
    // right shift by 20 bits
    // 64.0  nJ *  8.24 mJ/nJ => 44.24 mJ
    // 44.24 mJ >> 24         => 44.0  mJ
    // shiftBits = 24 to discard the 24 fractional bits after scaling
    //
    pProv->super.energy.scaleFactor =
        LW_TYPES_U32_TO_UFXP_X_Y_SCALED(8, 24, 1, 1000000);
    pProv->super.energy.shiftBits   = 24;

    return status;
}

/*!
 * Helper function to read fuse version and fuse values and update the registers
 * with values based on the fuse version
 *
 * @param[in] pGpuAdc13    Pointer to PWR_DEVICE_GPUADC13
 *
 * @return    FLCN_OK
 *     Registers successfully updated
 *
 * @return    Other errors
 *     Unexpected errors returned from VFE
 */
static FLCN_STATUS
s_pwrDevGpuAdc13UpdateFuseParams
(
    PWR_DEVICE_GPUADC13 *pGpuAdc13
)
{
    LwF32       vcmOffsetFloat       = 0;
    LwF32       diffOffsetFloat      = 0;
    LwF32       diffGainFloat        = 0;
    LwF32       vcmCoarseOffsetFloat = 0;
    FLCN_STATUS status               = FLCN_OK;
    LwU32       reg32;
    LwU32       temp;

    // Grab the PERF DMEM read semaphore
    perfReadSemaphoreTake();

    // Read the VCM_OFFSET.
    if (VFE_VAR_IS_VALID(pGpuAdc13->super.vcmOffsetVfeVarIdx))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            vfeVarEvaluate(pGpuAdc13->super.vcmOffsetVfeVarIdx, &vcmOffsetFloat),
            s_pwrDevGpuAdc13UpdateFuseParams_done);
    }

    // Read the DIFF_OFFSET.
    if (VFE_VAR_IS_VALID(pGpuAdc13->super.diffOffsetVfeVarIdx))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            vfeVarEvaluate(pGpuAdc13->super.diffOffsetVfeVarIdx, &diffOffsetFloat),
            s_pwrDevGpuAdc13UpdateFuseParams_done);
    }

    // Read the DIFF_GAIN.
    if (VFE_VAR_IS_VALID(pGpuAdc13->super.diffGailwfeVarIdx))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            vfeVarEvaluate(pGpuAdc13->super.diffGailwfeVarIdx, &diffGainFloat),
            s_pwrDevGpuAdc13UpdateFuseParams_done);
    }

    // Read the VCM_COARSE_OFFSET.
    if (VFE_VAR_IS_VALID(pGpuAdc13->vcmCoarseOffsetVfeVarIdx))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            vfeVarEvaluate(pGpuAdc13->vcmCoarseOffsetVfeVarIdx, &vcmCoarseOffsetFloat),
            s_pwrDevGpuAdc13UpdateFuseParams_done);
    }

    // Release the PERF DMEM read semaphore
    perfReadSemaphoreGive();

    // Read the SYS_PWR_SENSE_ADC_CTRL2 register
    reg32 = REG_RD32(FECS, LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL_2);

    // Colwert floating point values to unsigned 32bit integer.
    temp = lwF32ColwertToU32(vcmOffsetFloat);
    reg32 = FLD_SET_DRF_NUM(_PTRIM_SYS_NAFLL, _SYS_PWR_SENSE_ADC_CTRL_2,
                _ADC_CTRL_OFFSET, temp, reg32);
    temp = lwF32ColwertToU32(diffGainFloat);
    reg32 = FLD_SET_DRF_NUM(_PTRIM_SYS_NAFLL, _SYS_PWR_SENSE_ADC_CTRL_2,
                _ADC_CTRL_GAIN, temp, reg32);
    temp = lwF32ColwertToU32(vcmCoarseOffsetFloat);
    reg32 = FLD_SET_DRF_NUM(_PTRIM_SYS_NAFLL, _SYS_PWR_SENSE_ADC_CTRL_2,
                _ADC_CTRL_COARSE_OFFSET, temp, reg32);

    REG_WR32(FECS, LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL_2, reg32);

    //
    // ADC_RAW_CORRECTION register can be configured from PMU ucode only if the
    // boolean bPmuConfigReg is LW_TRUE. If LW_FALSE, the ADC registers would
    // have already been configured by devinit ucode and raised the PLM of
    // these registers to L3.
    //
    if (pGpuAdc13->bPmuConfigReg)
    {
        // Read the LW_THERM_ADC_RAW_CORRECTION_VALUE register
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_RAW_CORRECTION);

        if (pGpuAdc13->bAdcRawCorrection)
        {
            // Colwert floating point values to unsigned 32bit integer.
            temp = lwF32ColwertToU32(diffOffsetFloat);

            //
            // Numerical Analysis:
            //   DIFF_OFFSET is FXP7.2 and numSamples is FXP4.0
            //   7.2 * 4.0 => 11.2
            //   11.2 >> 2 => 11.0
            // Multiplication yields FXP11.2 and the 2 fractional bits are
            // discarded before writing to ADC_RAW_CORRECTION which is 11-bits.
            //
            temp *= pGpuAdc13->numSamples;
            temp >>= 2;
        }
        else
        {
            // ADC_RAW_CORRECTION needs to be explicitly set to 0 in this case
            temp = 0;
        }
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_RAW_CORRECTION, _VALUE, temp,
                    reg32);
        REG_WR32(CSB, LW_CPWR_THERM_ADC_RAW_CORRECTION, reg32);
    }

s_pwrDevGpuAdc13UpdateFuseParams_done:
    return status;
}

/*!
 * Helper function to configure all settings required before enabling ADC
 * sensing via @ref pwrDevLoad_GPUADC1X
 *
 * @param[in] pGpuAdc13    Pointer to PWR_DEVICE_GPUADC13
 *
 * @return    FLCN_OK
 *     Successfully configured parameters
 * @return    FLCN_ERR_ILWALID_STATE
 *     Invalid configuration
 */
static FLCN_STATUS
s_pwrDevGpuAdc13PreLoad
(
    PWR_DEVICE_GPUADC13 *pGpuAdc13
)
{
    FLCN_STATUS               status  = FLCN_OK;
    PWR_DEVICE_GPUADC13_PROV *pProv;
    LwU32                     reg32;
    LwU32                     i;
    LwU32                     idx;

    // Setting up ADC operating mode
    if (pGpuAdc13->operatingMode ==
            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_OPERATING_MODE_ALT)
    {
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _ADC_CTRL, _DEVICE_MODE, _ALT, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL, reg32);

        // Apply NEG2POS_BIAS
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL4);
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL4, _NEG2POS_BIAS,
            pGpuAdc13->numSamples * pGpuAdc13->super.adcMaxValue, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL4, reg32);
    }

    // Setting up ADC_CTRL2 configuration
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL2);

    // BEACON channel
    for (idx = 0;
         idx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_BEACON_NUM;
         idx++)
    {
        if (pGpuAdc13->beacon.instance[idx].provIdx !=
                LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE)
        {
            pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13,
                pGpuAdc13->beacon.instance[idx].provIdx);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                ((pProv != NULL) &&
                 (pwrDevGpuAdc1xProvChBusGet(&pProv->super) !=
                      PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE)),
                FLCN_ERR_ILWALID_STATE,
                s_pwrDevGpuAdc13PreLoad_exit);

            reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL2, _BEACON, idx,
                LW_CPWR_THERM_ADC_CTRL2_BEACON_CH(
                    pwrDevGpuAdc1xProvChBusGet(&pProv->super)), reg32);
        }
        else
        {
            reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL2, _BEACON, idx,
                LW_CPWR_THERM_ADC_CTRL2_BEACON_NONE, reg32);
        }
    }

    // OFFSET channel
    for (idx = 0;
         idx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_OFFSET_NUM;
         idx++)
    {
        if (pGpuAdc13->offset.instance[idx].provIdx !=
                LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE)
        {
            pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13,
                pGpuAdc13->offset.instance[idx].provIdx);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pProv != NULL),
                FLCN_ERR_ILWALID_STATE,
                s_pwrDevGpuAdc13PreLoad_exit);

            if (pGpuAdc13->ovrm.device[idx].gen ==
                    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_OVRM_GEN_2)
            {
                // On Gen2 both OFFSET and BEACON are the bus channel of a provider
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (pwrDevGpuAdc1xProvChBusGet(&pProv->super) !=
                         PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE),
                    FLCN_ERR_ILWALID_STATE,
                    s_pwrDevGpuAdc13PreLoad_exit);

                reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL2, _OFFSET, idx,
                    LW_CPWR_THERM_ADC_CTRL2_OFFSET_CH(
                        pwrDevGpuAdc1xProvChBusGet(&pProv->super)), reg32);
            }
            else
            {
                // On Gen1 OFFSET is the shunt channel of a provider
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (pwrDevGpuAdc1xProvChShuntGet(&pProv->super) !=
                         PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE),
                    FLCN_ERR_ILWALID_STATE,
                    s_pwrDevGpuAdc13PreLoad_exit);

                reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL2, _OFFSET, idx,
                    LW_CPWR_THERM_ADC_CTRL2_OFFSET_CH(
                        pwrDevGpuAdc1xProvChShuntGet(&pProv->super)), reg32);
            }
        }
        else
        {
            reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL2, _OFFSET, idx,
                LW_CPWR_THERM_ADC_CTRL2_OFFSET_NONE, reg32);
        }
    }
    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL2, reg32);

    // Setting up ADC_CTRL3 configuration
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL3);

    // BEACON threshold and comparison function
    for (idx = 0;
         idx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_BEACON_NUM;
         idx++)
    {
        if (pGpuAdc13->beacon.instance[idx].provIdx !=
                LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE)
        {
            //
            // Threshold is specified for one ADC sample as FXP7.0 in VBIOS
            // and numSamples is FXP4.0. _BEACON_THRESH is 11-bits.
            // 7.0 * 4.0 => 11.0
            //
            reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL3, _BEACON_THRESH,
                idx, pGpuAdc13->beacon.instance[idx].threshold * pGpuAdc13->numSamples,
                reg32);
            switch (pGpuAdc13->beacon.instance[idx].compFunc)
            {
                case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_BEACON_COMP_GT:
                {
                    reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL3,
                        _BEACON_COMP, idx, LW_CPWR_THERM_ADC_CTRL3_BEACON_COMP_GT,
                        reg32);
                    break;
                }
                case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_BEACON_COMP_LT:
                {
                    reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_CTRL3,
                        _BEACON_COMP, idx, LW_CPWR_THERM_ADC_CTRL3_BEACON_COMP_LT,
                        reg32);
                    break;
                }
                default:
                {
                    PMU_ASSERT_OK_OR_GOTO(status,
                        FLCN_ERR_ILWALID_STATE,
                        s_pwrDevGpuAdc13PreLoad_exit);
                }
            }
        }
    }
    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL3, reg32);

    //
    // Setting up ADC_OFFSET_MAP
    // Sanity check to ensure that same provider is not configured to use both
    // OFFSET1 and OFFSET2
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pGpuAdc13->offset.instance[0].provMask &
          pGpuAdc13->offset.instance[1].provMask) == 0),
        FLCN_ERR_ILWALID_STATE,
        s_pwrDevGpuAdc13PreLoad_exit);

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_OFFSET_MAP);
    for (idx = 0;
         idx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_OFFSET_NUM;
         idx++)
    {
        FOR_EACH_INDEX_IN_MASK(16, i, pGpuAdc13->offset.instance[idx].provMask)
        {
            pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13, i);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pProv != NULL),
                FLCN_ERR_ILWALID_STATE,
                s_pwrDevGpuAdc13PreLoad_exit);

            if (pwrDevGpuAdc1xProvChBusGet(&pProv->super) !=
                    PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE)
            {
                if (pwrDevGpuAdc1xProvChBusGet(&pProv->super) <
                        LW_CPWR_THERM_ADC_OFFSET_MAP_CH__SIZE_1)
                {
                    reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_OFFSET_MAP, _CH,
                        pwrDevGpuAdc1xProvChBusGet(&pProv->super),
                        LW_CPWR_THERM_ADC_OFFSET_MAP_CH_OFFSET(idx), reg32);
                }
                else if (pwrDevGpuAdc1xProvChBusGet(&pProv->super) <
                             LW_CPWR_THERM_ADC_OFFSET_MAP2_CH__SIZE_1)
                {
                    reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_OFFSET_MAP2, _CH,
                        pwrDevGpuAdc1xProvChBusGet(&pProv->super),
                        LW_CPWR_THERM_ADC_OFFSET_MAP2_CH_OFFSET(idx), reg32);
                }
            }
            if (pwrDevGpuAdc1xProvChShuntGet(&pProv->super) !=
                    PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE)
            {
                if (pwrDevGpuAdc1xProvChShuntGet(&pProv->super) <
                        LW_CPWR_THERM_ADC_OFFSET_MAP_CH__SIZE_1)
                {
                    reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_OFFSET_MAP, _CH,
                        pwrDevGpuAdc1xProvChShuntGet(&pProv->super),
                        LW_CPWR_THERM_ADC_OFFSET_MAP_CH_OFFSET(idx), reg32);
                }
                else if (pwrDevGpuAdc1xProvChShuntGet(&pProv->super) <
                             LW_CPWR_THERM_ADC_OFFSET_MAP2_CH__SIZE_1)
                {
                    reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _ADC_OFFSET_MAP2, _CH,
                        pwrDevGpuAdc1xProvChShuntGet(&pProv->super),
                        LW_CPWR_THERM_ADC_OFFSET_MAP2_CH_OFFSET(idx), reg32);
                }
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    REG_WR32(CSB, LW_CPWR_THERM_ADC_OFFSET_MAP, reg32);

    // Setting up ADC_MUL_CTRL configuration
    for (i = 0; i < pGpuAdc13->super.activeProvCount; i++)
    {
        pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13, i);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pProv != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_pwrDevGpuAdc13PreLoad_exit);

        if (pwrDevGpuAdc1xProvChPairGet(&pProv->super) !=
                PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE)
        {
            reg32 = 0;
            reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_CTRL_INDEX, _INDEX,
                LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_CHP(
                pwrDevGpuAdc1xProvChPairGet(&pProv->super)), reg32);
            REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_INDEX, reg32);

            reg32 = LW_CPWR_THERM_ADC_MUL_CTRL_DATA_VALUE_INIT;
            reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_CTRL_DATA,
                _VALUE_CHP_CFG_OP1, LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_CH(
                pwrDevGpuAdc1xProvChBusGet(&pProv->super)), reg32);
            reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_CTRL_DATA,
                _VALUE_CHP_CFG_OP2, LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_CH(
                pwrDevGpuAdc1xProvChShuntGet(&pProv->super)), reg32);
            reg32 = FLD_SET_DRF(_CPWR_THERM, _ADC_MUL_CTRL_DATA,
                _VALUE_CHP_CFG_ENABLE, _ON, reg32);
            REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_DATA, reg32);
        }
    }

    // Setting up SUM blocks
    if (pGpuAdc13->sum.instance[0].provUnit !=
            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_PROV_UNIT_DISABLED)
    {
        // SUM1_INPUT
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT);

        for (i = 0; i < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_INPUT_NUM; i++)
        {
            if (pGpuAdc13->sum.instance[0].input[i].provIdx ==
                    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE)
            {
                idx = LW_CPWR_THERM_IPC_CTRL_SUM_INPUT_NO_CH;
            }
            else
            {
                pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13,
                    pGpuAdc13->sum.instance[0].input[i].provIdx);
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (pProv != NULL),
                    FLCN_ERR_ILWALID_STATE,
                    s_pwrDevGpuAdc13PreLoad_exit);

                switch (pGpuAdc13->sum.instance[0].provUnit)
                {
                    case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_PROV_UNIT_MV:
                    {
                        PMU_ASSERT_TRUE_OR_GOTO(status,
                            (pwrDevGpuAdc1xProvChBusGet(&pProv->super) !=
                                 PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE),
                            FLCN_ERR_ILWALID_STATE,
                            s_pwrDevGpuAdc13PreLoad_exit);

                        idx = LW_CPWR_THERM_IPC_CTRL_SUM_INPUT_CH(
                                  pwrDevGpuAdc1xProvChBusGet(&pProv->super));
                        break;
                    }
                    case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_PROV_UNIT_MA:
                    {
                        PMU_ASSERT_TRUE_OR_GOTO(status,
                            (pwrDevGpuAdc1xProvChShuntGet(&pProv->super) !=
                                 PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE),
                            FLCN_ERR_ILWALID_STATE,
                            s_pwrDevGpuAdc13PreLoad_exit);

                        idx = LW_CPWR_THERM_IPC_CTRL_SUM_INPUT_CH(
                                  pwrDevGpuAdc1xProvChShuntGet(&pProv->super));
                        break;
                    }
                    case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_PROV_UNIT_MW:
                    {
                        PMU_ASSERT_TRUE_OR_GOTO(status,
                            (pwrDevGpuAdc1xProvChPairGet(&pProv->super) !=
                                 PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE),
                            FLCN_ERR_ILWALID_STATE,
                            s_pwrDevGpuAdc13PreLoad_exit);

                        idx = LW_CPWR_THERM_IPC_CTRL_SUM_INPUT_CHP(
                                  pwrDevGpuAdc1xProvChPairGet(&pProv->super));
                        break;
                    }
                    default:
                    {
                        PMU_ASSERT_OK_OR_GOTO(status,
                            FLCN_ERR_ILWALID_STATE,
                            s_pwrDevGpuAdc13PreLoad_exit);
                    }
                }
            }
            reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL_SUM1_INPUT,
                _INPUT, i, idx, reg32);
        }
        REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM1_INPUT, reg32);

        // SUM1 Scale Factor
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM1_SF);

        for (i = 0; i < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_INPUT_NUM; i++)
        {
            reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL_SUM1_SF,
                _SF, i, pGpuAdc13->sum.instance[0].input[i].scaleFactor, reg32);
        }
        REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM1_SF, reg32);
    }

    if (pGpuAdc13->sum.instance[1].provUnit !=
            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_PROV_UNIT_DISABLED)
    {
        // SUM2_INPUT
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM2_INPUT);

        for (i = 0; i < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_INPUT_NUM; i++)
        {
            if (pGpuAdc13->sum.instance[1].input[i].provIdx ==
                    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_IDX_NONE)
            {
                idx = LW_CPWR_THERM_IPC_CTRL_SUM_INPUT_NO_CH;
            }
            else
            {
                pProv = pwrDevGpuAdc13ProvGet(pGpuAdc13,
                    pGpuAdc13->sum.instance[1].input[i].provIdx);
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (pProv != NULL),
                    FLCN_ERR_ILWALID_STATE,
                    s_pwrDevGpuAdc13PreLoad_exit);

                switch (pGpuAdc13->sum.instance[1].provUnit)
                {
                    case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_PROV_UNIT_MV:
                    {
                        PMU_ASSERT_TRUE_OR_GOTO(status,
                            (pwrDevGpuAdc1xProvChBusGet(&pProv->super) !=
                                 PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE),
                            FLCN_ERR_ILWALID_STATE,
                            s_pwrDevGpuAdc13PreLoad_exit);

                        idx = LW_CPWR_THERM_IPC_CTRL_SUM_INPUT_CH(
                                  pwrDevGpuAdc1xProvChBusGet(&pProv->super));
                        break;
                    }
                    case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_PROV_UNIT_MA:
                    {
                        PMU_ASSERT_TRUE_OR_GOTO(status,
                            (pwrDevGpuAdc1xProvChShuntGet(&pProv->super) !=
                                 PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE),
                            FLCN_ERR_ILWALID_STATE,
                            s_pwrDevGpuAdc13PreLoad_exit);

                        idx = LW_CPWR_THERM_IPC_CTRL_SUM_INPUT_CH(
                                  pwrDevGpuAdc1xProvChShuntGet(&pProv->super));
                        break;
                    }
                    case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_PROV_UNIT_MW:
                    {
                        PMU_ASSERT_TRUE_OR_GOTO(status,
                            (pwrDevGpuAdc1xProvChPairGet(&pProv->super) !=
                                 PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE),
                            FLCN_ERR_ILWALID_STATE,
                            s_pwrDevGpuAdc13PreLoad_exit);

                        idx = LW_CPWR_THERM_IPC_CTRL_SUM_INPUT_CHP(
                                  pwrDevGpuAdc1xProvChPairGet(&pProv->super));
                        break;
                    }
                    default:
                    {
                        PMU_ASSERT_OK_OR_GOTO(status,
                            FLCN_ERR_ILWALID_STATE,
                            s_pwrDevGpuAdc13PreLoad_exit);
                    }
                }
            }
            reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL_SUM2_INPUT,
                _INPUT, i, idx, reg32);
        }
        REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM2_INPUT, reg32);

        // SUM2 Scale Factor
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM2_SF);

        for (i = 0; i < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM_INPUT_NUM; i++)
        {
            reg32 = FLD_IDX_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL_SUM2_SF,
                _SF, i, pGpuAdc13->sum.instance[1].input[i].scaleFactor, reg32);
        }
        REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL_SUM2_SF, reg32);
    }

s_pwrDevGpuAdc13PreLoad_exit:
    return status;
}

/*!
 * Helper function to colwert from policy limit unit to GPUADC13 provider limit
 * unit
 *
 * @param[in] limitUnit    Policy Limit Unit defined by
 *                         @ref LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_<xyz>
 * @return    devLimitUnit defined by
 *            @ref LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_<xyz>
 */
static LwU8
s_pwrDevColwertPolicyLimitUnit
(
    LwU8 limitUnit
)
{
    switch (limitUnit)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA:
            return LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_MA;
        case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW:
            return LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_MW;
    }

    return LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC_PROV_UNIT_ILWALID;
}
