/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrdev_gpuadc10.c
 * @brief GPUADC10 Driver
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
#include "pmgr/pwrdev_gpuadc10.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
static FLCN_STATUS s_pwrDevGpuAdc10ProvChannelMapConstruct(PWR_DEVICE_GPUADC10 *pGpuAdc10)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "s_pwrDevGpuAdc10ProvChannelMapConstruct");
static LwU8 s_pwrDevColwertPolicyLimitUnit(LwU8 limitUnit)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pwrDevColwertPolicyLimitUnit");
static FLCN_STATUS s_pwrDevGpuAdc10UpdateFuseParams(PWR_DEVICE_GPUADC10 *pGpuAdc10)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pwrDevGpuAdc10UpdateFuseParams");

/* ------------------------ Static Inline Functions --------------------------*/
/*!
 * Helper function to get the pointer to @ref PWR_DEVICE_GPUADC1X_PROV given a
 * provider index
 *
 * @param[in]   pGpuAdc10
 *     Pointer to @ref PWR_DEVICE_GPUADC10.
 * @param[in]   provIdx
 *     Provider index for which the structure pointer is to be returned.
 * @return
 *     Pointer to @ref PWR_DEVICE_GPUADC1X_PROV if provIdx is valid
 *     NULL for invalid provIdx
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline PWR_DEVICE_GPUADC1X_PROV *
pwrDevGpuAdc10ProvGet
(
    PWR_DEVICE_GPUADC10 *pGpuAdc10,
    LwU8                 provIdx
)
{
    if (provIdx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PHYSICAL_PROV_NUM)
    {
       return &pGpuAdc10->prov[provIdx];
    }

    return NULL;
}

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */

/*!
 * Construct a GPUADC10 PWR_DEVICE object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrDevGrpIfaceModel10ObjSetImpl_GPUADC10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    PWR_DEVICE_GPUADC10                               *pGpuAdc10;
    RM_PMU_PMGR_PWR_DEVICE_DESC_GPUADC10_BOARDOBJ_SET *pGpuAdc10Set =
        (RM_PMU_PMGR_PWR_DEVICE_DESC_GPUADC10_BOARDOBJ_SET *)pBoardObjDesc;
    FLCN_STATUS                                        status = FLCN_OK;
    LwU32                                              fullRangeLwrrentmA;
    LwU32                                              i;
    OSTASK_OVL_DESC                                    ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgrPwrDeviceStateSync)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfVfe)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Construct and initialize parent-class object.
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrDevGrpIfaceModel10ObjSetImpl_GPUADC1X(pModel10, ppBoardObj, size, pBoardObjDesc),
            pwrDevGrpIfaceModel10ObjSetImpl_GPUADC10_done);

        pGpuAdc10 = (PWR_DEVICE_GPUADC10 *)*ppBoardObj;

        pGpuAdc10->ipc    = pGpuAdc10Set->ipc;
        pGpuAdc10->beacon = pGpuAdc10Set->beacon;

        // numSamples is always 1 for GPUADC10
        pGpuAdc10->tupleParams.numSamples = 1;

        //
        // LW_CPWR_THERM_ADC_IIR_VALUE_DATA is FXP7.25
        // 7th bit of ADC Code will always be 0 after applying the ADC raw
        // correction. So effectively the IIR value is FXP6.25
        // lwrrColwFactor is FXP10.4 (mA/code)
        // 6.25 code * 10.4 mA/code => 16.29 mA
        // 29 fractional bits will be discarded
        //
        pGpuAdc10->tupleParams.lwrrShiftBits = 29;

        //
        // LW_CPWR_THERM_ADC_IIR_VALUE_DATA is FXP7.25
        // 7th bit of ADC Code will always be 0 after applying the ADC raw
        // correction. So effectively the IIR value is FXP6.25
        // voltColwFactor is FXP10.4 (mV/code)
        // voltColwFactor is multiplied by 1000 for the result in uV
        // 10.4  mV/code * 10.0         => 20.4  uV/code
        //  6.25 code    * 20.4 uV/code => 26.29 uV
        // 29 fractional bits will be discarded
        //
        pGpuAdc10->tupleParams.voltShiftBits = 29;

        //
        // LW_CPWR_THERM_ADC_MUL_VALUE_DATA is FXP14.18
        // 14.18 code * 10.4 mW/code => 24.22 mW
        // 22 fractional bits will be discarded
        //
        pGpuAdc10->tupleParams.pwrShiftBits = 22;

        //
        // Construct the channelMap for each SW provider to point to the HW
        // channels that it constitutes.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrDevGpuAdc10ProvChannelMapConstruct(pGpuAdc10),
            pwrDevGrpIfaceModel10ObjSetImpl_GPUADC10_done);

        // Callwlate lwrrColwFactor and pwrColwFactor
        for (i = 0; i < pGpuAdc10->super.activeProvCount; i++)
        {
            PWR_DEVICE_GPUADC1X_PROV *pProv = pwrDevGpuAdc10ProvGet(pGpuAdc10, i);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pProv != NULL),
                FLCN_ERR_ILWALID_STATE,
                pwrDevGrpIfaceModel10ObjSetImpl_GPUADC10_done);

            pProv->fullRangeLwrrent = pGpuAdc10Set->fullRangeLwrrent[i];

            // Colwert fullRangeLwrrent to mA
            fullRangeLwrrentmA = pwrDevGpuAdc10FullRangeLwrrmAGet(pGpuAdc10, pProv);

            //
            // Dividing fullRangeLwrrentmA by adcMaxValue and scaling it to
            // FXP28.4
            // Considering fullRangeLwrrent of 64A (0x4000 in FXP8.8),
            // lwrrColwFactor is 0x3F7D mA/code which is effectively FXP10.4
            //
            pProv->lwrrColwFactor = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(
                28, 4, fullRangeLwrrentmA, pGpuAdc10->super.adcMaxValue);

            //
            // lwrrColwFactor is FXP10.4 (mA/code)
            // accColwusPerCycle is FXP3.12 (us/cycle)
            // scaleFactor:  lwrrColwFactor * accColwusPerCycle
            // 10.4 mA/code * 3.12 us/cycle => 13.16 mA*us/code/cycle
            //
            // LW_CPWR_THERM_ADC_ACC_VALUE_DATA is FXP39.25 code*cycle
            // 39.25 code*cycle *  13.16 mA*us/code/cycle => 52.41 mA*us
            // 52.41 mA*us      >> 41                     => 52.0  mA*us
            // shiftBits = 41 to discard the fractional bits after scaling
            //
            pProv->lwrr.scaleFactor =
                pProv->lwrrColwFactor * pGpuAdc10->super.accColwusPerCycle;
            pProv->lwrr.shiftBits   = 41;

            //
            // voltColwFactor is FXP10.4 (mV/code)
            // accColwusPerCycle is FXP3.12 (us/cycle)
            // scaleFactor:  voltColwFactor * accColwusPerCycle
            // 10.4 mV/code * 3.12 us/cycle => 13.16 mV*us/code/cycle
            //
            // LW_CPWR_THERM_ADC_ACC_VALUE_DATA is FXP39.25 code*cycle
            // 39.25 code*cycle *  13.16 mV*us/code/cycle => 52.41 mV*us
            // 52.41 mV*us      >> 41                     => 52.0  mV*us
            // shiftBits = 41 to discard the fractional bits after scaling
            //
            pProv->volt.scaleFactor =
                pGpuAdc10->super.voltColwFactor * pGpuAdc10->super.accColwusPerCycle;
            pProv->volt.shiftBits   = 41;

            //
            // pwrColwFactor = lwrrColwFactor * voltColwFactor
            // 10.4 mA/code * 10.4 mV/code => 20.8 uW/code
            //
            // Dividing by 1000 to colwert it to mW/code
            // 20.8 uW/code /  10.0 uW/mW => 10.8 mW/code
            // 10.8 mW/code >> 4          => 10.4 mW/code
            // Discarding 4 fractional bits
            //
            pProv->pwrColwFactor = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(
                28, 4, ((pProv->lwrrColwFactor *
                         pGpuAdc10->super.voltColwFactor) / (LwU32)1000));

            //
            // pwrColwFactor is FXP10.4 (mW/code)
            // accColwusPerCycle is FXP3.12 (us/cycle)
            // scaleFactor:  pwrColwFactor * accColwusPerCycle
            // 10.4 mW/code * 3.12 us/cycle => 13.16 mW*us/code/cycle
            //
            // LW_CPWR_THERM_ADC_MUL_ACC_VALUE_DATA is FXP46.18 code*cycle.
            // 46.18 code*cycle *  13.16 mW*us/code/cycle => 59.34 mW*us
            // 59.34 mW*us      >> 34                     => 59.0  mW*us
            // shiftBits = 34 to discard the fractional bits after scaling
            //
            pProv->pwr.scaleFactor =
                pProv->pwrColwFactor * pGpuAdc10->super.accColwusPerCycle;
            pProv->pwr.shiftBits   = 34;

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
            // shiftBits = 24 to discard the fractional bits after scaling
            //
            pProv->energy.scaleFactor =
                LW_TYPES_U32_TO_UFXP_X_Y_SCALED(8, 24, 1, 1000000);
            pProv->energy.shiftBits   = 24;
        }

        // Nothing more to do if beacon feature is not enabled
        if (!pGpuAdc10->beacon.bEnable)
        {
            status = FLCN_OK;
            goto pwrDevGrpIfaceModel10ObjSetImpl_GPUADC10_done;
        }

        // Set the flag to schedule callback
        Pmgr.bDevCallbackGpuAdc10 = LW_TRUE;

        //
        // Callwlate the beacon threshold and max spread in uV
        // Numerical Analysis:
        //   voltColwFactor is FXP28.4 and threshold/maxSpread are FXP32.0
        //   32.0 * 28.4 => 28.4
        //   4 fractional bits will be discarded.
        // Overflow Analysis:
        //   Max voltage possible is 0xFFFF (FXP8.8) V which corresponds to a
        //   voltColwFactor of 0x3E00C49 uV/bit in FXP 28.4 and max value of
        //   threshold/maxSpread is 0x3F and multiplication of these values is
        //   0xF42305F7 so overflow is not possible.
        //
        pGpuAdc10->beaconState.thresholduV = multUFXP32(4,
            (LwU32)pGpuAdc10->beacon.threshold, pGpuAdc10->super.voltColwFactor);
        pGpuAdc10->beaconState.maxSpreaduV = multUFXP32(4,
            (LwU32)pGpuAdc10->beacon.maxSpread, pGpuAdc10->super.voltColwFactor);

        //
        // If beacon feature is enabled, mask cannot be zero.
        // TODO-Tariq: Define a macro to check for zero mask
        //
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pGpuAdc10->beacon.channelMask.super.pData[0] != 0U),
            FLCN_ERR_ILWALID_STATE,
            pwrDevGrpIfaceModel10ObjSetImpl_GPUADC10_done);

        if (pGpuAdc10->beaconState.pChannelStatus == NULL)
        {
            pGpuAdc10->beaconState.pChannelStatus = lwosCallocType(
                OVL_INDEX_DMEM(pmgrPwrDeviceStateSync), 1, PWR_CHANNELS_STATUS);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pGpuAdc10->beaconState.pChannelStatus != NULL),
                FLCN_ERR_NO_FREE_MEM,
                pwrDevGrpIfaceModel10ObjSetImpl_GPUADC10_done);

            // Init and copy in the channelMask.
            boardObjGrpMaskInit_E32(&(pGpuAdc10->beaconState.pChannelStatus->mask));
            PMU_ASSERT_OK_OR_GOTO(status,
                boardObjGrpMaskImport_E32(
                    &(pGpuAdc10->beaconState.pChannelStatus->mask),
                    &(pGpuAdc10->beacon.channelMask)),
                pwrDevGrpIfaceModel10ObjSetImpl_GPUADC10_done);
        }
    }
pwrDevGrpIfaceModel10ObjSetImpl_GPUADC10_done:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevLoad
 */
FLCN_STATUS
pwrDevLoad_GPUADC10
(
    PWR_DEVICE  *pDev
)
{
    PWR_DEVICE_GPUADC10      *pGpuAdc10 = (PWR_DEVICE_GPUADC10 *)pDev;
    FLCN_STATUS               status    = FLCN_OK;
    PWR_DEVICE_GPUADC1X_PROV *pProv;
    LwU32                     reg32;
    LwU32                     i;
    OSTASK_OVL_DESC           ovlDescListFuse[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
        OSTASK_OVL_DESC_DEFINE_VFE(FULL)
    };

    // Registers which use fuse values must be programmed first
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListFuse);
    {
        status = s_pwrDevGpuAdc10UpdateFuseParams(pGpuAdc10);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListFuse);

    PMU_ASSERT_OK_OR_GOTO(status, status, pwrDevLoad_GPUADC10_done);

    // Call super-class implementation
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrDevLoad_GPUADC1X(pDev),
        pwrDevLoad_GPUADC10_done);

    // Setting up the ADC_MUL_CTRL
    for (i = 0; i < pGpuAdc10->super.activeProvCount; i++)
    {
        pProv = pwrDevGpuAdc10ProvGet(pGpuAdc10, i);
        if ((pProv == NULL) ||
            (pwrDevGpuAdc1xProvChPairGet(pProv) ==
                 PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE))
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto pwrDevLoad_GPUADC10_done;
        }
        reg32 = 0;
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_CTRL_INDEX, _INDEX,
            LW_CPWR_THERM_ADC_MUL_VALUE_INDEX_INDEX_CHP(
            pwrDevGpuAdc1xProvChPairGet(pProv)), reg32);
        REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_INDEX, reg32);

        reg32 = LW_CPWR_THERM_ADC_MUL_CTRL_DATA_VALUE_CHP0_CFG_INIT;
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_CTRL_DATA,
            _VALUE_CHP0_CFG_OP1, LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_CH(
            pwrDevGpuAdc1xProvChBusGet(pProv)), reg32);
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_CTRL_DATA,
            _VALUE_CHP0_CFG_OP2, LW_CPWR_THERM_ADC_IIR_VALUE_INDEX_INDEX_CH(
            pwrDevGpuAdc1xProvChShuntGet(pProv)), reg32);
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_MUL_CTRL_DATA,
                    _VALUE_CHP0_CFG_ENABLE, 1, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_ADC_MUL_CTRL_DATA, reg32);
    }

    // Setting up the IPC config
    for (i = 0; i < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_NUM; i++)
    {
        reg32 = 0;
        switch (pGpuAdc10->ipc.instance[i].sourceType)
        {
            case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_ADC:
            {
                pProv = pwrDevGpuAdc10ProvGet(pGpuAdc10,
                            pGpuAdc10->ipc.instance[i].index.provider);
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (pProv != NULL),
                    FLCN_ERR_ILWALID_STATE,
                    pwrDevLoad_GPUADC10_done);

                reg32 = FLD_SET_DRF(_CPWR_THERM, _IPC_CTRL, _EN, _ON, reg32);
                switch (pGpuAdc10->ipc.instance[i].providerUnit)
                {
                    case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PROV_UNIT_MV:
                    {
                        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _OP,
                            LW_CPWR_THERM_IPC_CTRL_OP_CH(
                                pwrDevGpuAdc1xProvChBusGet(pProv)), reg32);
                        break;
                    }
                    case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PROV_UNIT_MA:
                    {
                        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _OP,
                            LW_CPWR_THERM_IPC_CTRL_OP_CH(
                                pwrDevGpuAdc1xProvChShuntGet(pProv)), reg32);
                        break;
                    }
                    case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PROV_UNIT_MW:
                    {
                        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _OP,
                            LW_CPWR_THERM_IPC_CTRL_OP_CHP(
                                pwrDevGpuAdc1xProvChPairGet(pProv)), reg32);
                        break;
                    }
                }
                break;
            }
            case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_BA:
            {
                reg32 = FLD_SET_DRF(_CPWR_THERM, _IPC_CTRL, _EN, _ON, reg32);
                reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _OP,
                    LW_CPWR_THERM_IPC_CTRL_OP_BA_W(
                        pGpuAdc10->ipc.instance[i].index.window), reg32);
                break;
            }
            case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_NONE:
            {
                // IPC instance is not configured/unused, so disable it
                reg32 = FLD_SET_DRF(_CPWR_THERM, _IPC_CTRL, _EN, _OFF, reg32);
                break;
            }
        }
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _IIR_GAIN,
                    pGpuAdc10->ipc.instance[i].iirGain, reg32);
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _IIR_LENGTH,
                    pGpuAdc10->ipc.instance[i].iirLength, reg32);
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _IIR_DOWNSHIFT,
                    pGpuAdc10->ipc.instance[i].iirDownshift, reg32);
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_CTRL, _PROPORTIONAL_DOWNSHIFT,
                    pGpuAdc10->ipc.instance[i].propDownshift, reg32);

        //
        // Set the IPC_REF(i) value to 0xFFFFFFFF before writing to IPC_CTRL so
        // that IPC capping doesn't get enabled with a 0 threshold limit.
        // IPC_REF will be 0 only on first load. On subsequent loads it will
        // already have the values programmed by HW_THRESHOLD policy.
        //
        if (pGpuAdc10->super.bFirstLoad)
        {
            REG_WR32(CSB, LW_CPWR_THERM_IPC_REF(i), 0xFFFFFFFF);
        }
        REG_WR32(CSB, LW_CPWR_THERM_IPC_CTRL(i), reg32);
    }

    reg32 = 0;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _IPC_COMP_MAX_OFFSET, _VALUE,
                pGpuAdc10->ipc.compMaxOffset, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_IPC_COMP_MAX_OFFSET, reg32);

    if (pGpuAdc10->beacon.bEnable)
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgrPwrDeviceStateSync)
            OSTASK_OVL_DESC_DEFINE_PWR_CHANNELS_STATUS_GET
        };
        //
        // Call first evaluation of pwrChannelsStatusGet() so that will have
        // last values cached by first evaluation.
        //
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = pwrChannelsStatusGet(pGpuAdc10->beaconState.pChannelStatus);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        PMU_ASSERT_OK_OR_GOTO(status, status, pwrDevLoad_GPUADC10_done);
    }

    // Update bFirstLoad to LW_FALSE on successfully completing first load
    if (pGpuAdc10->super.bFirstLoad)
    {
        pGpuAdc10->super.bFirstLoad = LW_FALSE;
    }

pwrDevLoad_GPUADC10_done:
    return status;
}

/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevTupleGet
 */
FLCN_STATUS
pwrDevTupleGet_GPUADC10
(
    PWR_DEVICE                 *pDev,
    LwU8                        provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    PWR_DEVICE_GPUADC10      *pGpuAdc10 = (PWR_DEVICE_GPUADC10 *)pDev;
    FLCN_STATUS               status    = FLCN_OK;
    PWR_DEVICE_GPUADC1X_PROV *pProv;

    // Sanity check the provIdx
    if ((provIdx >= pGpuAdc10->super.activeProvCount) &&
        (provIdx < RM_PMU_PMGR_PWR_DEVICE_GPUADC10_PROV_NUM))
    {
        //
        // provIdx points to a logical provider or a non-active physical
        // provider which is valid but we do not support pwrDevTupleGet
        // interface for this provider.
        //
        status = FLCN_OK;
        goto pwrDevTupleGet_GPUADC10_exit;
    }
    else
    {
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (provIdx < RM_PMU_PMGR_PWR_DEVICE_GPUADC10_PROV_NUM),
            FLCN_ERR_ILWALID_ARGUMENT,
            pwrDevTupleGet_GPUADC10_exit);
    }

    pProv = pwrDevGpuAdc10ProvGet(pGpuAdc10, provIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProv != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevTupleGet_GPUADC10_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        pwrDevTupleGet_GPUADC1X(pDev, pProv, pTuple, &(pGpuAdc10->tupleParams)),
        pwrDevTupleGet_GPUADC10_exit);

pwrDevTupleGet_GPUADC10_exit:
    return status;
}

/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevTupleAccGet
 */
FLCN_STATUS
pwrDevTupleAccGet_GPUADC10
(
    PWR_DEVICE                     *pDev,
    LwU8                            provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE_ACC *pTupleAcc
)
{
    PWR_DEVICE_GPUADC10      *pGpuAdc10  = (PWR_DEVICE_GPUADC10 *)pDev;
    FLCN_STATUS               status     = FLCN_OK;
    PWR_DEVICE_GPUADC1X_PROV *pProv;

    // Sanity check the provIdx
    if ((provIdx >= pGpuAdc10->super.activeProvCount) &&
        (provIdx < RM_PMU_PMGR_PWR_DEVICE_GPUADC10_PROV_NUM))
    {
        //
        // provIdx points to a logical provider or a non-active physical
        // provider which is valid but we do not support pwrDevTupleAccGet
        // interface for this provider.
        //
        status = FLCN_OK;
        goto pwrDevTupleAccGet_GPUADC10_exit;
    }
    else
    {
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (provIdx < RM_PMU_PMGR_PWR_DEVICE_GPUADC10_PROV_NUM),
            FLCN_ERR_ILWALID_ARGUMENT,
            pwrDevTupleAccGet_GPUADC10_exit);
    }

    pProv = pwrDevGpuAdc10ProvGet(pGpuAdc10, provIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProv != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevTupleAccGet_GPUADC10_exit);

    // Call super-class implementation
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrDevTupleAccGet_GPUADC1X(pDev, pProv, pTupleAcc),
        pwrDevTupleAccGet_GPUADC10_exit);

pwrDevTupleAccGet_GPUADC10_exit:
    return status;
}

/*!
 * GPUADC10 power device implementation to return full range tuple.
 *
 * @copydoc PwrDevTupleGet
 */
FLCN_STATUS
pwrDevTupleFullRangeGet_GPUADC10
(
    PWR_DEVICE                 *pDev,
    LwU8                        provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    PWR_DEVICE_GPUADC10      *pGpuAdc10 = (PWR_DEVICE_GPUADC10 *)pDev;
    FLCN_STATUS               status    = FLCN_OK;
    PWR_DEVICE_GPUADC1X_PROV *pProv;

    // Sanity check the provIdx
    if ((provIdx >= pGpuAdc10->super.activeProvCount) &&
        (provIdx < RM_PMU_PMGR_PWR_DEVICE_GPUADC10_PROV_NUM))
    {
        //
        // provIdx points to a logical provider or a non-active physical
        // provider which is valid but we do not support pwrDevTupleGet
        // interface for this provider.
        //
        status = FLCN_OK;
        goto pwrDevTupleFullRangeGet_GPUADC10_exit;
    }
    else
    {
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (provIdx < RM_PMU_PMGR_PWR_DEVICE_GPUADC10_PROV_NUM),
            FLCN_ERR_ILWALID_ARGUMENT,
            pwrDevTupleFullRangeGet_GPUADC10_exit);
    }

    pProv = pwrDevGpuAdc10ProvGet(pGpuAdc10, provIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProv != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevTupleFullRangeGet_GPUADC10_exit);

    // Call super-class implementation
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrDevTupleFullRangeGet_GPUADC1X(pDev, pProv, pTuple),
        pwrDevTupleFullRangeGet_GPUADC10_exit);

pwrDevTupleFullRangeGet_GPUADC10_exit:
    return status;
}

/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevSetLimit
 */
FLCN_STATUS
pwrDevSetLimit_GPUADC10
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU8        limitIdx,
    LwU8        limitUnit,
    LwU32       limitValue
)
{
    PWR_DEVICE_GPUADC10      *pGpuAdc10 = (PWR_DEVICE_GPUADC10 *)pDev;
    FLCN_STATUS               status    = FLCN_OK;
    LwU32                     reg32     = 0;
    PWR_DEVICE_GPUADC1X_PROV *pProv;
    LwU8                      devLimitUnit;

    // Sanity check limitIdx
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (limitIdx < pwrDevThresholdNumGet_GPUADC10(pDev)),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevSetLimit_GPUADC10_exit);

    // Sanity check limitUnit matches IPC(limitIdx) unit
    devLimitUnit = s_pwrDevColwertPolicyLimitUnit(limitUnit);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (devLimitUnit == pGpuAdc10->ipc.instance[limitIdx].providerUnit),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevSetLimit_GPUADC10_exit);

    // Sanity check if provIdx corresponds to the IPC(limitIdx) instance
    switch (pGpuAdc10->ipc.instance[limitIdx].sourceType)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_ADC:
        {
            // Additionally, sanity check that the provider is active
            PMU_ASSERT_TRUE_OR_GOTO(status,
                ((pGpuAdc10->ipc.instance[limitIdx].index.provider == provIdx) &&
                 (provIdx < pGpuAdc10->super.activeProvCount)),
                FLCN_ERR_ILWALID_ARGUMENT,
                pwrDevSetLimit_GPUADC10_exit);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_BA:
        {
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pGpuAdc10->ipc.instance[limitIdx].index.window == (provIdx -
                    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_BA_IPC_PROV_IDX_START)),
                FLCN_ERR_ILWALID_ARGUMENT,
                pwrDevSetLimit_GPUADC10_exit);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_NONE:
        default:
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                FLCN_ERR_ILWALID_ARGUMENT,
                pwrDevSetLimit_GPUADC10_exit);
        }
    }

    pProv = pwrDevGpuAdc10ProvGet(pGpuAdc10, provIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProv != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevSetLimit_GPUADC10_exit);

    switch (pGpuAdc10->ipc.instance[limitIdx].sourceType)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_ADC:
        {
            if (pGpuAdc10->ipc.instance[limitIdx].providerUnit ==
                    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PROV_UNIT_MA)
            {
                //
                // IPC_REF = ((1 << 29) / lwrrColwFactor) * limitValue
                // Numerical analysis:
                // 32.0 << 29   =>  3.29
                //  3.29 / 28.4 =>  7.25
                //  7.25 * 32.0 =>  7.25
                //
                reg32 = LW_TYPES_U32_TO_UFXP_X_Y(3, 29, 1);
                reg32 = LW_UNSIGNED_ROUNDED_DIV(reg32, pProv->lwrrColwFactor);
                reg32 = reg32 * limitValue;
            }
            else if (pGpuAdc10->ipc.instance[limitIdx].providerUnit ==
                         LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PROV_UNIT_MW)
            {
                //
                // IPC_REF = ((1 << 22) / pwrColwFactor) * limitValue
                // Numerical analysis:
                // 32.0 << 22   => 10.22
                // 10.22 / 28.4 => 14.18
                // 14.18 * 32.0 => 14.18
                //
                reg32 = LW_TYPES_U32_TO_UFXP_X_Y(10, 22, 1);
                reg32 = LW_UNSIGNED_ROUNDED_DIV(reg32, pProv->pwrColwFactor);
                reg32 = reg32 * limitValue;
            }
            REG_WR32(CSB, LW_CPWR_THERM_IPC_REF(limitIdx), reg32);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_BA:
        {
            REG_WR32(CSB, LW_CPWR_THERM_IPC_REF(limitIdx), limitValue);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_NONE:
        default:
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                FLCN_ERR_ILWALID_ARGUMENT,
                pwrDevSetLimit_GPUADC10_exit);
        }
    }

pwrDevSetLimit_GPUADC10_exit:
    return status;
}

/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevGetLimit
 */
FLCN_STATUS
pwrDevGetLimit_GPUADC10
(
    PWR_DEVICE            *pDev,
    LwU8                   provIdx,
    LwU8                   limitIdx,
    RM_PMU_BOARDOBJ       *pBuf
)
{
    PWR_DEVICE_GPUADC10 *pGpuAdc10 = (PWR_DEVICE_GPUADC10 *)pDev;
    RM_PMU_PMGR_PWR_DEVICE_BOARDOBJ_GET_STATUS *pStatus =
        (RM_PMU_PMGR_PWR_DEVICE_BOARDOBJ_GET_STATUS *)pBuf;
    FLCN_STATUS          status    = FLCN_OK;
    LwU32                reg32;

    // Sanity check limitIdx
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (limitIdx < pwrDevThresholdNumGet_GPUADC10(pDev)),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevGetLimit_GPUADC10_exit);

    // Return early if provIdx doesn't match-up with the limitIdx
    switch (pGpuAdc10->ipc.instance[limitIdx].sourceType)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_ADC:
        {
            // Additionally, sanity check that the provider is active
            if ((pGpuAdc10->ipc.instance[limitIdx].index.provider != provIdx) ||
                (provIdx >= pGpuAdc10->super.activeProvCount))
            {
                pStatus->providers[provIdx].thresholds[limitIdx] = 0;
                goto pwrDevGetLimit_GPUADC10_exit;
            }
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_BA:
        {
            if (pGpuAdc10->ipc.instance[limitIdx].index.window != (provIdx -
                    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_BA_IPC_PROV_IDX_START))
            {
                pStatus->providers[provIdx].thresholds[limitIdx] = 0;
                goto pwrDevGetLimit_GPUADC10_exit;
            }
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_NONE:
        {
            pStatus->providers[provIdx].thresholds[limitIdx] = 0;
            goto pwrDevGetLimit_GPUADC10_exit;
        }
        default:
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                FLCN_ERR_ILWALID_ARGUMENT,
                pwrDevGetLimit_GPUADC10_exit);
        }
    }

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_IPC_REF(limitIdx));
    switch (pGpuAdc10->ipc.instance[limitIdx].sourceType)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_ADC:
        {
            PWR_DEVICE_GPUADC1X_PROV *pProv = pwrDevGpuAdc10ProvGet(pGpuAdc10,
                pGpuAdc10->ipc.instance[limitIdx].index.provider);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pProv != NULL),
                FLCN_ERR_ILWALID_STATE,
                pwrDevGetLimit_GPUADC10_exit);

            if ((pGpuAdc10->ipc.instance[limitIdx].providerUnit ==
                     LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PROV_UNIT_MA))
            {
                //
                // reg32 data (LW_CPWR_THERM_IPC_REF) here is FXP 7.25
                // 7.25 >> 13   =>  7.12
                // 7.12 *  28.4 => 31.16
                // 16 fractional bits will be discarded
                //
                reg32 = LW_RIGHT_SHIFT_ROUNDED(reg32, 13U);
                reg32 = multUFXP32(16, reg32, pProv->lwrrColwFactor);
            }
            else if ((pGpuAdc10->ipc.instance[limitIdx].providerUnit ==
                          LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PROV_UNIT_MW))
            {
                //
                // reg32 data (LW_CPWR_THERM_IPC_REF) here is FXP 14.18
                // 14.18 >>  6   => 14.12
                // 14.12 *  28.4 => 42.16
                // 16 fractional bits will be discarded
                //
                reg32 = LW_RIGHT_SHIFT_ROUNDED(reg32, 6U);
                reg32 = multUFXP32(16, reg32, pProv->pwrColwFactor);
            }
            break;
        }
        case LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC_SRC_TYPE_BA:
        {
            break;
        }
    }

    pStatus->providers[provIdx].thresholds[limitIdx] = reg32;

pwrDevGetLimit_GPUADC10_exit:
    return status;
}

/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevStateSync
 */
FLCN_STATUS
pwrDevStateSync_GPUADC10
(
    PWR_DEVICE *pDev
)
{
    PWR_DEVICE_GPUADC10 *pGpuAdc10      = (PWR_DEVICE_GPUADC10 *)pDev;
    PWR_CHANNELS_STATUS *pChannelStatus = pGpuAdc10->beaconState.pChannelStatus;
    FLCN_STATUS          status         = FLCN_OK;
    LwBool               bReset         = LW_FALSE;
    LwU32                voltMaxuV      = LW_U32_MIN;
    LwU32                voltMinuV      = LW_U32_MAX;
    LwU32                voltuV;
    LwU32                maxSpreaduV    = 0;
    LwBoardObjIdx        idx;
    OSTASK_OVL_DESC      ovlDescList[]  = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgrPwrDeviceStateSync)
        OSTASK_OVL_DESC_DEFINE_PWR_CHANNELS_STATUS_GET
    };

    // Return early if beacon feature is not enabled
    if (!pGpuAdc10->beacon.bEnable)
    {
        return status;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Get tuple of the channels in beacon mask
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrChannelsStatusGet(pChannelStatus),
            pwrDevStateSync_GPUADC10_exit);

        BOARDOBJGRPMASK_FOR_EACH_BEGIN(&pChannelStatus->mask, idx)
        {
            voltuV = pChannelStatus->channels[idx].channel.tuple.voltuV;

            // Voltage should be greater than the beacon threshold
            if (voltuV <= pGpuAdc10->beaconState.thresholduV)
            {
                bReset = LW_TRUE;
                break;
            }

            voltMaxuV = LW_MAX(voltMaxuV, voltuV);
            voltMinuV = LW_MIN(voltMinuV, voltuV);
            maxSpreaduV = voltMaxuV - voltMinuV;

            // The voltage spread across channels should be less than the max spread
            if (maxSpreaduV >= pGpuAdc10->beaconState.maxSpreaduV)
            {
                bReset = LW_TRUE;
                break;
            }
        }
        BOARDOBJGRPMASK_FOR_EACH_END;

        // Trigger a reset if required
        if (bReset)
        {
            pwrDevGpuAdc1xReset(&pGpuAdc10->super);
        }

pwrDevStateSync_GPUADC10_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/* ------------------------- PWR_DEVICE_GPUADC1X Interfaces ----------------- */
/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xBaSetLimit
 */
FLCN_STATUS
pwrDevGpuAdc1xBaSetLimit_GPUADC10
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
    // https://confluence.lwpu.com/pages/viewpage.action?pageId=63104530#GPUADC%E2%80%93Newpowerdeviceonchip-GPUADC1.0
    //
    provIdx = baWindowIdx +
        LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_BA_IPC_PROV_IDX_START;

    return pwrDevSetLimit_GPUADC10(&pGpuAdc1x->super, provIdx, limitIdx,
               limitUnit, limitValue);
}

/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xFullRangeVoltmVGet
 */
LwU32
pwrDevGpuAdc1xFullRangeVoltmVGet_GPUADC10
(
    PWR_DEVICE_GPUADC1X      *pGpuAdc1x,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    PWR_DEVICE_GPUADC10 *pGpuAdc10 = (PWR_DEVICE_GPUADC10 *)pGpuAdc1x;

    return pwrDevGpuAdc10FullRangeVoltmVGet(pGpuAdc10, pProv);
}

/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xFullRangeLwrrmAGet
 */
LwU32
pwrDevGpuAdc1xFullRangeLwrrmAGet_GPUADC10
(
    PWR_DEVICE_GPUADC1X      *pGpuAdc1x,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    PWR_DEVICE_GPUADC10 *pGpuAdc10 = (PWR_DEVICE_GPUADC10 *)pGpuAdc1x;

    return pwrDevGpuAdc10FullRangeLwrrmAGet(pGpuAdc10, pProv);
}

/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xFullRangePwrmWGet
 */
LwU32
pwrDevGpuAdc1xFullRangePwrmWGet_GPUADC10
(
    PWR_DEVICE_GPUADC1X      *pGpuAdc1x,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    PWR_DEVICE_GPUADC10 *pGpuAdc10 = (PWR_DEVICE_GPUADC10 *)pGpuAdc1x;

    return pwrDevGpuAdc10FullRangePwrmWGet(pGpuAdc10, pProv);
}

/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xVoltColwFactorGet
 */
LwUFXP28_4
pwrDevGpuAdc1xVoltColwFactorGet_GPUADC10
(
    PWR_DEVICE_GPUADC1X      *pGpuAdc1x,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    return pGpuAdc1x->voltColwFactor;
}

/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xSwResetLengthSet
 */
void
pwrDevGpuAdc1xSwResetLengthSet_GPUADC10
(
    PWR_DEVICE_GPUADC1X *pGpuAdc1x
)
{
    LwU32 reg32 = 0;

    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_SW_RESET, _LENGTH,
                pGpuAdc1x->resetLength, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_SW_RESET, reg32);
}

/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xSnapTrigger
 */
void
pwrDevGpuAdc1xSnapTrigger_GPUADC10
(
    PWR_DEVICE_GPUADC1X *pGpuAdc1x
)
{
    LwU32 reg32;

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_CTRL);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _ADC_CTRL, _SNAP, _TRIGGER, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_CTRL, reg32);
}

/*!
 * GPUADC10 power device implementation.
 *
 * @copydoc PwrDevGpuAdc1xSwResetTrigger
 */
void
pwrDevGpuAdc1xSwResetTrigger_GPUADC10
(
    PWR_DEVICE_GPUADC1X *pGpuAdc1x
)
{
    LwU32 reg32;

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_SW_RESET);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _ADC_SW_RESET, _RST, _TRIGGER, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_SW_RESET, reg32);
}

/* ------------------------- Static Functions ------------------------------- */
/*!
 * Helper function to populate the channelMap for each provider.
 * The channelMap stores the information regarding which HW channels constitutes
 * the bus, shunt and pair (multiplier) for a SW provider.
 *
 * @param[in/out]  pGpuAdc10 Pointer to PWR_DEVICE_GPUADC10
 * @return
 *     FLCN_OK                Provider to channel map created successfully
 *     FLCN_ERR_ILWALID_STATE Provider configuration is invalid
 */
static FLCN_STATUS
s_pwrDevGpuAdc10ProvChannelMapConstruct
(
    PWR_DEVICE_GPUADC10 *pGpuAdc10
)
{
    FLCN_STATUS               status  = FLCN_OK;
    LwU8                      provIdx = 0;
    PWR_DEVICE_GPUADC1X_PROV *pProv;

    for (provIdx = 0;
         provIdx < pGpuAdc10->super.activeProvCount;
         provIdx++)
    {
        pProv = pwrDevGpuAdc10ProvGet(pGpuAdc10, provIdx);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pProv != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_pwrDevGpuAdc10ProvChannelMapConstruct_exit);

        pProv->channelMap.bus   = provIdx*2;
        pProv->channelMap.shunt = provIdx*2 + 1;
        pProv->channelMap.pair  = provIdx;
    }
    for (provIdx = pGpuAdc10->super.activeProvCount;
         provIdx < LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PHYSICAL_PROV_NUM;
         provIdx++)
    {
        pProv = pwrDevGpuAdc10ProvGet(pGpuAdc10, provIdx);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pProv != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_pwrDevGpuAdc10ProvChannelMapConstruct_exit);

        pProv->channelMap.bus   = PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE;
        pProv->channelMap.shunt = PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE;
        pProv->channelMap.pair  = PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE;
    }

s_pwrDevGpuAdc10ProvChannelMapConstruct_exit:
    return status;
}

/*!
 * Helper function to colwert from policy limit unit to GPUADC10 provider limit
 * unit
 *
 * @param[in] limitUnit    Policy Limit Unit defined by
 *                         @ref LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_<xyz>
 * @return    devLimitUnit defined by
 *            @ref LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PROV_UNIT_<xyz>
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
            return LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PROV_UNIT_MA;
        case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW:
            return LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PROV_UNIT_MW;
    }

    return LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PROV_UNIT_ILWALID;
}

/*!
 * Helper function to read fuse version and fuse values and update the registers
 * with values based on the fuse version
 *
 * @param[in] pGpuAdc10    Pointer to PWR_DEVICE_GPUADC10
 *
 * @return    FLCN_OK
 *     Registers successfully updated
 *
 * @return    Other errors
 *     Unexpected errors returned from VFE
 */
static FLCN_STATUS
s_pwrDevGpuAdc10UpdateFuseParams
(
    PWR_DEVICE_GPUADC10 *pGpuAdc10
)
{
    LwF32       vcmOffsetFloat       = 0;
    LwF32       diffOffsetFloat      = 0;
    LwF32       diffGainFloat        = 0;
    FLCN_STATUS status               = FLCN_OK;
    LwU32       reg32;
    LwU32       temp;

    // Grab the PERF DMEM read semaphore
    perfReadSemaphoreTake();

    // Read the VCM_OFFSET.
    if (VFE_VAR_IS_VALID(pGpuAdc10->super.vcmOffsetVfeVarIdx))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            vfeVarEvaluate(pGpuAdc10->super.vcmOffsetVfeVarIdx, &vcmOffsetFloat),
            s_pwrDevGpuAdc10UpdateFuseParams_done);
    }

    // Read the DIFF_OFFSET.
    if (VFE_VAR_IS_VALID(pGpuAdc10->super.diffOffsetVfeVarIdx))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            vfeVarEvaluate(pGpuAdc10->super.diffOffsetVfeVarIdx, &diffOffsetFloat),
            s_pwrDevGpuAdc10UpdateFuseParams_done);
    }

    // Read the DIFF_GAIN.
    if (VFE_VAR_IS_VALID(pGpuAdc10->super.diffGailwfeVarIdx))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            vfeVarEvaluate(pGpuAdc10->super.diffGailwfeVarIdx, &diffGainFloat),
            s_pwrDevGpuAdc10UpdateFuseParams_done);
    }

    // Release the PERF DMEM read semaphore
    perfReadSemaphoreGive();

    // Read the SYS_PWR_SENSE_ADC_CTRL2 register
    reg32 = REG_RD32(FECS, LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL2);

    // Colwert floating point values to unsigned 32bit integer.
    temp = lwF32ColwertToU32(vcmOffsetFloat);
    reg32 = FLD_SET_DRF_NUM(_PTRIM_SYS_NAFLL, _SYS_PWR_SENSE_ADC_CTRL2,
                _ADC_CTRL_OFFSET, temp, reg32);
    temp = lwF32ColwertToU32(diffGainFloat);
    reg32 = FLD_SET_DRF_NUM(_PTRIM_SYS_NAFLL, _SYS_PWR_SENSE_ADC_CTRL2,
                _ADC_CTRL_GAIN, temp, reg32);

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_GPUADC10_COARSE_OFFSET))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            pmgrPwrDevGpuAdc10CoarseOffsetUpdate_HAL(&Pmgr, &reg32),
            s_pwrDevGpuAdc10UpdateFuseParams_done);
    }
    REG_WR32(FECS, LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL2, reg32);

    // Read the LW_THERM_ADC_RAW_CORRECTION_VALUE register
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_ADC_RAW_CORRECTION);

    // Colwert floating point values to unsigned 32bit integer.
    temp = lwF32ColwertToU32(diffOffsetFloat);
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _ADC_RAW_CORRECTION, _VALUE, temp,
                reg32);
    REG_WR32(CSB, LW_CPWR_THERM_ADC_RAW_CORRECTION, reg32);

s_pwrDevGpuAdc10UpdateFuseParams_done:
    return status;
}
