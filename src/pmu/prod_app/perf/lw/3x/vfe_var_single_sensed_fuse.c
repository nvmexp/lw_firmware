/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_var_single_sensed_fuse.c
 * @brief   VFE_VAR_SINGLE_SENSED_FUSE class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "pmu_bar0.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe_var_single_sensed_fuse.h"

/* ------------------------ Static Function Prototypes -----------------------*/
static FLCN_STATUS s_vfeVarReadFuse(LwU8 vfieldId, LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_INFO *pFuseInfo, LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE *pVfeVarFuseVal, LwBool bSigned)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "s_vfeVarReadFuse");
static BoardObjVirtualTableDynamicCast (s_vfeVarDynamicCast_SINGLE_SENSED_FUSE)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeVarDynamicCast_SINGLE_SENSED_FUSE")
    GCC_ATTRIB_USED();

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Virtual table for VFE_VAR_SINGLE_SENSED_FUSE object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VfeVarSingleSensedFuseVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeVarDynamicCast_SINGLE_SENSED_FUSE)
};

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Constructor.
 * @memberof VFE_VAR_SINGLE_SENSED_FUSE
 * @public
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_VFE_VAR_SINGLE_SENSED_FUSE *pDescVar =
        (RM_PMU_VFE_VAR_SINGLE_SENSED_FUSE *)pBoardObjDesc;
    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE fuseVal;
    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE fuseVer;
    VFE_VAR_SINGLE_SENSED_FUSE *pVfeVar;
    FLCN_STATUS                 status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_BASE(pModel10,
        ppBoardObj, size, pBoardObjDesc),
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeVar, *ppBoardObj, PERF, VFE_VAR, SINGLE_SENSED_FUSE,
                                  &status, vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_exit);

   //
    // We should read fuse and fuse version always to make sure the HW is fused
    // correctly
    //
    status = s_vfeVarReadFuse(pDescVar->vFieldId,
                &(pDescVar->fuse), &fuseVal, pDescVar->super.bFuseValueSigned);
    if (status == FLCN_OK)
    {
        pVfeVar->super.fuseValueHwInteger = fuseVal;
    }
    else
    {
        PMU_TRUE_BP();
        //
        // If fuseValue is not overridden by regkey then we cannot proceed
        // further without fuse value. Bail out early.
        //
        if (!(pDescVar->super.overrideInfo.bFuseRegkeyOverride))
        {
            goto vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_exit;
        }
    }

    status = s_vfeVarReadFuse(pDescVar->vFieldIdVer,
                &(pDescVar->fuseVer), &fuseVer, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_TRUE_BP();
        //
        // If fuseValue is not overridden by regkey and version check is needed
        // then we cannot proceed further without fuse version. Bail out early.
        //
        if (!(pDescVar->super.overrideInfo.bFuseRegkeyOverride) &&
            pDescVar->super.verInfo.bVerCheck)
        {
            goto vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_exit;
        }
    }

    // Fuse version is always unsigned
    pVfeVar->super.fuseVersion =
        (LwU8)LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_UNSIGNED_GET(&fuseVer);

    //
    // Fuse value overridden by regkey. Lwrrently all fuse values that can be
    // overridden through regkey are unsigned.
    //
    if (pDescVar->super.overrideInfo.bFuseRegkeyOverride)
    {
        if (pDescVar->super.bFuseValueSigned)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_TRUE_BP();
            goto vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_exit;
        }
        //
        // Lwrrently all fuse values that can be
        // overridden through regkey are unsigned.
        //
        LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_INIT(&fuseVal,
            pDescVar->super.overrideInfo.fuseValOverride, LW_FALSE);
    }
    else
    {
        if (pDescVar->super.verInfo.bVerCheck)
        {
            //
            // If version doesn't match expected set error status so that we break
            // in RM as well
            //
            if (pDescVar->super.verInfo.bVerExpectedIsMask)
            {
                pVfeVar->super.bVersionCheckFailed =
                    ((LWBIT32(pVfeVar->super.fuseVersion) &
                      (LwU32)pDescVar->super.verInfo.verExpected) == 0U);
            }
            else
            {
                pVfeVar->super.bVersionCheckFailed =
                    (pVfeVar->super.fuseVersion != pDescVar->super.verInfo.verExpected);
            }

            //
            // We can still trust the fuse value even if version doesn't match.
            // So, use the default value only if version doesn't match and fuse value
            // is not correct (zero) OR the VBIOS flag is set to use default value
            // immediately on version check failure (even if the fuse value read is
            // not bad)
            //
            if (pVfeVar->super.bVersionCheckFailed &&
                ((pDescVar->super.verInfo.bUseDefaultOlwerCheckFail) ||
                 (LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_UNSIGNED_GET(&fuseVal) == 0U)))
            {
                fuseVal = pDescVar->super.fuseValDefault;
            }
        }
        else
        {
            pVfeVar->super.bVersionCheckFailed = LW_FALSE;
        }

        if (pDescVar->super.bFuseValueSigned)
        {
            LwS32 signedTemp = LW_TYPES_SFXP_X_Y_TO_S32(20, 12,
                (pDescVar->super.hwCorrectionScale *
                 LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_SIGNED_GET(&fuseVal)));

            signedTemp = signedTemp + pDescVar->super.hwCorrectionOffset;

            LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_SIGNED_SET(&fuseVal, signedTemp);
        }
        else
        {
            LwU32 unsignedTemp = LW_TYPES_UFXP_X_Y_TO_U32(20, 12,
                (pDescVar->super.hwCorrectionScale *
                 LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_UNSIGNED_GET(&fuseVal)));

            unsignedTemp = unsignedTemp + (LwU32)pDescVar->super.hwCorrectionOffset;

            LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_UNSIGNED_SET(&fuseVal, unsignedTemp);
        }
    }

    // Set member variables.
    pVfeVar->super.fuseValueInteger = fuseVal;
    if (pDescVar->super.bFuseValueSigned)
    {
        pVfeVar->super.fuseValue =
            lwF32ColwertFromS32(LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_SIGNED_GET(&fuseVal));
    }
    else
    {
        pVfeVar->super.fuseValue =
            lwF32ColwertFromU32(LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_UNSIGNED_GET(&fuseVal));
    }

vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_exit:
    return status;
}

/*!
 * @brief Helper function to read a fuse and return the value in pVfeVarFuseVal
 *
 * @memberof VFE_VAR_SINGLE_SENSED_FUSE
 * @private
 *
 * @param[in]       pFuseInfo       Pointer to struct with information about fuse to be read.
 * @param[in,out]   pVfeVarFuseVal  Pointer to fuse value union.
 * @param[in]       bSigned         Boolean flag indicating if the fuse value is a signed Integer.
 *
 * @return  FLCN_OK                 Upon successful read.
 * @return  FLCN_ERR_NOT_SUPPORTED  If the fuse register type is not supported
 * @return  FLCN_ERR_ILWALID_STATE  If the fuse size is invalid
 */
static FLCN_STATUS
s_vfeVarReadFuse
(
    LwU8                                                vFieldId,
    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_INFO   *pFuseInfo,
    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE  *pVfeVarFuseVal,
    LwBool                                              bSigned
)
{
    LW2080_CTRL_BIOS_VFIELD_REGISTER_SEGMENT   *pFuseSegment = NULL;
    LwU32       indexRegAddr;
    LwU32       regAddr;
    LwU32       regVal;
    LwU32       fuseVal      = 0U;
    LwU8        segmentIdx;
    LwU8        fuseSizeBits = 0U;
    FLCN_STATUS status       = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFIELD_VALIDATION))
    {
        // Ensure that segmentCount is not out-of-bounds
        if (pFuseInfo->segmentCount > LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_SEGMENTS_MAX)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto s_vfeVarReadFuse_exit;
        }
    }

    for (segmentIdx = 0; segmentIdx < pFuseInfo->segmentCount; segmentIdx++)
    {
        pFuseSegment = &(pFuseInfo->segments[segmentIdx]);
        // Clear previous bit since we do an OR later
        regVal = 0;

        switch (pFuseSegment->type)
        {
            case LW2080_CTRL_BIOS_VFIELD_REGISTER_SEGMENT_TYPE_REG:
            {
                // Address of register to read
                regAddr = pFuseSegment->data.reg.addr;

                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFIELD_VALIDATION))
                {
                    // Validate address of register to read
                    status = perfVfieldRegAddrValidate_HAL(&Perf, vFieldId, segmentIdx, regAddr);
                    if (status != FLCN_OK)
                    {
                        goto s_vfeVarReadFuse_exit;
                    }
                }

                // Read register and extract only the bits part of the fuse
                regVal = REF_VAL( pFuseSegment->data.reg.super.highBit:pFuseSegment->data.reg.super.lowBit,
                    (REG_RD32(BAR0, regAddr)));
                break;
            }
            case LW2080_CTRL_BIOS_VFIELD_REGISTER_SEGMENT_TYPE_INDEX_REG:
            {
                // Address of index register to write
                indexRegAddr = pFuseSegment->data.indexReg.regIndex;
                // Address of register to read
                regAddr = pFuseSegment->data.indexReg.addr;

                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFIELD_VALIDATION))
                {
                    // Validate address of index register to write
                    status = perfVfieldRegAddrValidate_HAL(&Perf, vFieldId, segmentIdx, indexRegAddr);
                    if (status != FLCN_OK)
                    {
                        goto s_vfeVarReadFuse_exit;
                    }

                    // Validate address of register to read
                    status = perfVfieldRegAddrValidate_HAL(&Perf, vFieldId, segmentIdx, regAddr);
                    if (status != FLCN_OK)
                    {
                        goto s_vfeVarReadFuse_exit;
                    }
                }

                // Write index register
                REG_WR32(BAR0, indexRegAddr, pFuseSegment->data.indexReg.index);

                // Read register and extract only the bits part of the fuse
                regVal = REF_VAL( pFuseSegment->data.indexReg.super.highBit:pFuseSegment->data.indexReg.super.lowBit,
                    (REG_RD32(BAR0, regAddr)));
                break;
            }
            default:
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NOT_SUPPORTED;
                break;
            }
        }

        if (status != FLCN_OK)
        {
            goto s_vfeVarReadFuse_exit;
        }

        // Make space for the new bits read
        fuseVal <<= (pFuseSegment->data.reg.super.highBit - pFuseSegment->data.reg.super.lowBit + 1U);
        // merge the new bits read
        fuseVal |= regVal;
        // Track fuse size in bits for sign extension of negative fuse value
        fuseSizeBits += (pFuseSegment->data.reg.super.highBit - pFuseSegment->data.reg.super.lowBit + 1U);
    }

    if (fuseSizeBits == 0U)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto s_vfeVarReadFuse_exit;
    }

    //
    // Sign extending negative integer. If the fuse Value CAN be negative
    // then we need to sign-extend the fuse value. To do this check if the
    // first/leading bit of the fuse value is negative. If it is then we
    // need to fill the remaining leading bits with 1s to get the negative
    // number in 32bit 2s complement format.
    //
    if (bSigned &&
        ((fuseVal & LWBIT32(fuseSizeBits - 1U)) != 0U))
    {
        // Fill leading bits with 1
        fuseVal |= (~0U << fuseSizeBits);
    }

    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_INIT(pVfeVarFuseVal,
        fuseVal, bSigned);

s_vfeVarReadFuse_exit:
    return status;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFIELD_VALIDATION))
/*!
 * @brief Validate if a register address is valid for a given VFIELD ID
 *        against a given lookup table.
 *
 * Format of the lookup table:
 *
 * A flat array that maps VFIELD ID to a
 * LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_SEGMENTS_MAX number of valid
 * (hardware) register addresses.
 *
 * For example, if LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_SEGMENTS_MAX is 2
 * the following table has the following meaning
 *
 * {0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0x00000000}
 *      ^           ^           ^           ^- zero means invalid
 *      |           |           |
 *   valid reg addrs for      valid reg addr for
 *      VFIELD ID 0              VFIELD ID 1
 *   (segments 0 and 1)          (segment 0)
 *
 * With pTable pointing at the above example table, only the following calls
 * will succeed and return FLCN_OK:
 *   perfVfieldRegAddrValidateAgainstTable(0, 0, 0xaaaaaaaa, pTable, 4);
 *   perfVfieldRegAddrValidateAgainstTable(0, 1, 0xbbbbbbbb, pTable, 4);
 *   perfVfieldRegAddrValidateAgainstTable(1, 0, 0xcccccccc, pTable, 4);
 *
 * @param[in] vfieldId     VFIELD ID to validate against
 * @param[in] segmentIdx   VFIELD fuse segment index
 * @param[in] regAddr      Register address to validate
 * @param[in] pTable       Pointer to the lookup table to use
 * @param[in] tableLength  Number of array elements in pTable
 *
 * @return FLCN_OK                    If the register address is valid
 * @return FLCN_ERR_ILWALID_ARGUMENT  If the register address is invalid
 */
FLCN_STATUS
vfeVarRegAddrValidateAgainstTable
(
    LwU8         vfieldId,
    LwU8         segmentIdx,
    LwU32        regAddr,
    const LwU32 *pTable,
    LwU32        tableLength
)
{
    FLCN_STATUS status         = FLCN_ERR_ILWALID_ARGUMENT;
    LwU32       tableIdx;
    LwU32       expectedAddr;

    tableIdx = (LwU32) vfieldId;
    tableIdx *= LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_SEGMENTS_MAX;
    tableIdx += segmentIdx;

    if (tableIdx >= tableLength)
    {
        goto vfeVarRegAddrValidateAgainstTable_exit;
    }

    expectedAddr = pTable[tableIdx];
    if (expectedAddr == 0U)
    {
        goto vfeVarRegAddrValidateAgainstTable_exit;
    }

    if (regAddr != expectedAddr)
    {
        goto vfeVarRegAddrValidateAgainstTable_exit;
    }

    // validation succeeded
    status = FLCN_OK;

vfeVarRegAddrValidateAgainstTable_exit:
    return status;
}
#endif  // PMUCFG_FEATURE_ENABLED(PMU_PERF_VFIELD_VALIDATION)

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   VFE_VAR_SINGLE_SENSED_FUSE implementation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeVarDynamicCast_SINGLE_SENSED_FUSE
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                       *pObject           = NULL;
    VFE_VAR_SINGLE_SENSED_FUSE *pVfeVarSensedFuse = (VFE_VAR_SINGLE_SENSED_FUSE *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED_FUSE))
    {
        PMU_BREAKPOINT();
        goto s_vfeVarDynamicCast_SINGLE_SENSED_FUSE_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, BASE):
        {
            VFE_VAR *pVfeVar = &pVfeVarSensedFuse->super.super.super.super;
            pObject = (void *)pVfeVar;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE):
        {
            VFE_VAR_SINGLE *pVfeVarSingle = &pVfeVarSensedFuse->super.super.super;
            pObject = (void *)pVfeVarSingle;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED):
        {
            VFE_VAR_SINGLE_SENSED *pVfeVarSingleSensed = &pVfeVarSensedFuse->super.super;
            pObject = (void *)pVfeVarSingleSensed;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED_FUSE_BASE):
        {
            VFE_VAR_SINGLE_SENSED_FUSE_BASE *pVfeVarSingleSensedFuseBase =
                &pVfeVarSensedFuse->super;
            pObject = (void *)pVfeVarSingleSensedFuseBase;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED_FUSE):
        {
            pObject = (void *)pVfeVarSensedFuse;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeVarDynamicCast_SINGLE_SENSED_FUSE_exit:
    return pObject;
}
