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
 * @file    vfe_var_single_sensed_fuse_20.c
 * @brief   VFE_VAR_SINGLE_SENSED_FUSE_20 class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "pmu_bar0.h"
#include "pmu_objfuse.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe_var_single_sensed_fuse_20.h"

/* ------------------------ Static Function Prototypes -----------------------*/
static FLCN_STATUS s_vfeVarReadFuse20(LwU8 fuseId, LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE *pVfeVarFuseVal, LwBool bSigned)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "s_vfeVarReadFuse20");

static BoardObjVirtualTableDynamicCast (s_vfeVarDynamicCast_SINGLE_SENSED_FUSE_20)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeVarDynamicCast_SINGLE_SENSED_FUSE_20")
    GCC_ATTRIB_USED();

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Max fuse size in number of bits
 */
#define VFE_VAR_SINGLE_SENSED_FUSE_20_FUSE_SIZE_MAX   32

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Virtual table for VFE_VAR_SINGLE_SENSED_FUSE_20 object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VfeVarSingleSensedFuse20VirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeVarDynamicCast_SINGLE_SENSED_FUSE_20)
};

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Constructor.
 * @memberof VFE_VAR_SINGLE_SENSED_FUSE_20
 * @public
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_20
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS                                       status   = FLCN_OK;
    RM_PMU_VFE_VAR_SINGLE_SENSED_FUSE_20             *pDescVar =
        (RM_PMU_VFE_VAR_SINGLE_SENSED_FUSE_20 *)pBoardObjDesc;
    VFE_VAR_SINGLE_SENSED_FUSE_20                    *pVfeVar =
        (VFE_VAR_SINGLE_SENSED_FUSE_20 *)*ppBoardObj;
    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE fuseVal;
    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE fuseVer;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_BASE(pModel10,
        ppBoardObj, size, pBoardObjDesc),
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_20_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeVar, *ppBoardObj, PERF, VFE_VAR, SINGLE_SENSED_FUSE_20,
                                  &status, vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_20_exit);

    pVfeVar->fuseId    = pDescVar->fuseId;
    pVfeVar->fuseIdVer = pDescVar->fuseIdVer;

    fuseVal.bSigned            = LW_FALSE;
    fuseVal.data.unsignedValue =
        LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_ILWALID;

    //
    // We should read fuse and fuse version always to make sure the HW is fused
    // correctly
    //
    status = s_vfeVarReadFuse20(pVfeVar->fuseId,
                &fuseVal, pVfeVar->super.fuseBaseExtended.bFuseValueSigned);
    if (status == FLCN_OK)
    {
        pVfeVar->super.fuseValueHwInteger = fuseVal;
    }
    else
    {
        pVfeVar->super.fuseValueHwInteger.data.unsignedValue =
            LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_ILWALID;

        PMU_TRUE_BP();
        //
        // If fuseValue is not overridden by regkey then we cannot proceed
        // further without fuse value. Bail out early.
        //
        if (!(pVfeVar->super.fuseBaseExtended.overrideInfo.bFuseRegkeyOverride))
        {
            goto vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_20_exit;
        }
        else 
        {
            status = FLCN_OK;
        }
    }

    status = s_vfeVarReadFuse20(pVfeVar->fuseIdVer,
                &fuseVer, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_TRUE_BP();

        fuseVer.bSigned            = LW_FALSE;
        fuseVer.data.unsignedValue =
            LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_ILWALID;
        //
        // If fuseValue is not overridden by regkey and version check is needed
        // then we cannot proceed further without fuse version. Bail out early.
        //
        if (!(pVfeVar->super.fuseBaseExtended.overrideInfo.bFuseRegkeyOverride) &&
            pVfeVar->super.fuseBaseExtended.verInfo.bVerCheck)
        {
            goto vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_20_exit;
        }
        else
        {
            status = FLCN_OK;
        }
    }

    // Fuse version is always unsigned
    pVfeVar->super.fuseVersion =
        (LwU8)LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_UNSIGNED_GET(&fuseVer);

    //
    // Fuse value overridden by regkey. Lwrrently all fuse values that can be
    // overridden through regkey are unsigned.
    //
    if (pVfeVar->super.fuseBaseExtended.overrideInfo.bFuseRegkeyOverride)
    {
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (!pVfeVar->super.fuseBaseExtended.bFuseValueSigned),
            FLCN_ERR_ILWALID_ARGUMENT,
            vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_20_exit);

        //
        // Lwrrently all fuse values that can be
        // overridden through regkey are unsigned.
        //
        LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_INIT(&fuseVal,
            pVfeVar->super.fuseBaseExtended.overrideInfo.fuseValOverride, LW_FALSE);
    }
    else
    {
        if (pVfeVar->super.fuseBaseExtended.verInfo.bVerCheck)
        {
            if (fuseVer.data.unsignedValue ==
                LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_ILWALID)
            {
                pVfeVar->super.bVersionCheckFailed  = LW_TRUE;
            }

            //
            // If version doesn't match expected set error status so that we break
            // in RM as well
            //
            else if (pVfeVar->super.fuseBaseExtended.verInfo.bVerExpectedIsMask)
            {
                pVfeVar->super.bVersionCheckFailed =
                    ((LWBIT32(pVfeVar->super.fuseVersion) &
                      (LwU32)pVfeVar->super.fuseBaseExtended.verInfo.verExpected) == 0U);
            }
            else
            {
                pVfeVar->super.bVersionCheckFailed =
                    (pVfeVar->super.fuseVersion != pVfeVar->super.fuseBaseExtended.verInfo.verExpected);
            }

            //
            // We can still trust the fuse value even if version doesn't match.
            // So, use the default value only if version doesn't match and fuse value
            // is not correct (zero) OR the VBIOS flag is set to use default value
            // immediately on version check failure (even if the fuse value read is
            // not bad)
            //
            if (pVfeVar->super.bVersionCheckFailed &&
                ((pVfeVar->super.fuseBaseExtended.verInfo.bUseDefaultOlwerCheckFail) ||
                 (LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_UNSIGNED_GET(&fuseVal) == 0U)))
            {
                fuseVal = pVfeVar->super.fuseBaseExtended.fuseValDefault;
            }
        }
        else
        {
            pVfeVar->super.bVersionCheckFailed = LW_FALSE;
        }

        if (pVfeVar->super.fuseBaseExtended.bFuseValueSigned)
        {
            LwS32 signedTemp = LW_TYPES_SFXP_X_Y_TO_S32(20, 12,
                (pVfeVar->super.fuseBaseExtended.hwCorrectionScale *
                 LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_SIGNED_GET(&fuseVal)));

            signedTemp = signedTemp + pVfeVar->super.fuseBaseExtended.hwCorrectionOffset;

            LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_SIGNED_SET(&fuseVal, signedTemp);
        }
        else
        {
            LwU32 unsignedTemp = LW_TYPES_UFXP_X_Y_TO_U32(20, 12,
                (pVfeVar->super.fuseBaseExtended.hwCorrectionScale *
                 LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_UNSIGNED_GET(&fuseVal)));

            unsignedTemp = unsignedTemp + (LwU32)pVfeVar->super.fuseBaseExtended.hwCorrectionOffset;

            LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_UNSIGNED_SET(&fuseVal, unsignedTemp);
        }
    }

    // Set member variables.
    pVfeVar->super.fuseValueInteger = fuseVal;
    if (pVfeVar->super.fuseBaseExtended.bFuseValueSigned)
    {
        pVfeVar->super.fuseValue =
            lwF32ColwertFromS32(LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_SIGNED_GET(&fuseVal));
    }
    else
    {
        pVfeVar->super.fuseValue =
            lwF32ColwertFromU32(LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_UNSIGNED_GET(&fuseVal));
    }

vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_20_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   VFE_VAR_SINGLE_SENSED_FUSE_20 implementation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeVarDynamicCast_SINGLE_SENSED_FUSE_20
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                          *pObject             = NULL;
    VFE_VAR_SINGLE_SENSED_FUSE_20 *pVfeVarSensedFuse20 =
        (VFE_VAR_SINGLE_SENSED_FUSE_20 *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED_FUSE_20))
    {
        PMU_BREAKPOINT();
        goto s_vfeVarDynamicCast_SINGLE_SENSED_FUSE_20_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, BASE):
        {
            VFE_VAR *pVfeVar = &pVfeVarSensedFuse20->super.super.super.super;
            pObject = (void *)pVfeVar;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE):
        {
            VFE_VAR_SINGLE *pVfeVarSingle = &pVfeVarSensedFuse20->super.super.super;
            pObject = (void *)pVfeVarSingle;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED):
        {
            VFE_VAR_SINGLE_SENSED *pVfeVarSingleSensed = &pVfeVarSensedFuse20->super.super;
            pObject = (void *)pVfeVarSingleSensed;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED_FUSE_BASE):
        {
            VFE_VAR_SINGLE_SENSED_FUSE_BASE *pVfeVarSingleSensedFuseBase =
                &pVfeVarSensedFuse20->super;
            pObject = (void *)pVfeVarSingleSensedFuseBase;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED_FUSE_20):
        {
            pObject = (void *)pVfeVarSensedFuse20;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeVarDynamicCast_SINGLE_SENSED_FUSE_20_exit:
    return pObject;
}

/*!
 * @brief Helper function to read a fuse and return the value in pVfeVarFuseVal
 *
 * @memberof VFE_VAR_SINGLE_SENSED_FUSE_20
 * @private
 *
 * @param[in]       fuseId          Id of the fuse to be read.
 * @param[in,out]   pVfeVarFuseVal  Pointer to fuse value union.
 * @param[in]       bSigned         Boolean flag indicating if the fuse value is a signed Integer.
 *
 * @return  FLCN_OK                 Upon successful read.
 * @return  FLCN_ERR_NOT_SUPPORTED  If the fuse register type is not supported
 * @return  FLCN_ERR_ILWALID_STATE  If the fuse size is invalid
 */
static FLCN_STATUS
s_vfeVarReadFuse20
(
    LwU8                                                fuseId,
    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE  *pVfeVarFuseVal,
    LwBool                                              bSigned
)
{
    LwU8        fuseSize = 0;
    LwU32       fuseVal;
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        fuseStateGet_HAL(&Fuse, fuseId, &fuseSize, &fuseVal),
        s_vfeVarReadFuse20_exit);

    // Assert if fuse size > 32
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (fuseSize <= VFE_VAR_SINGLE_SENSED_FUSE_20_FUSE_SIZE_MAX),
        FLCN_ERR_ILWALID_STATE,
        s_vfeVarReadFuse20_exit);

    //
    // Sign extending negative integer. If the fuse Value CAN be negative
    // then we need to sign-extend the fuse value. To do this check if the
    // first/leading bit of the fuse value is negative. If it is then we
    // need to fill the remaining leading bits with 1s to get the negative
    // number in 32bit 2s complement format.
    //
    if (bSigned                                                  &&
        (fuseSize < VFE_VAR_SINGLE_SENSED_FUSE_20_FUSE_SIZE_MAX) &&
        ((fuseVal & LWBIT32(fuseSize - 1U)) != 0U))
    {
        // Fill leading bits with 1
        fuseVal |= (~0U << fuseSize);
    }

    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE_INIT(pVfeVarFuseVal,
        fuseVal, bSigned);

s_vfeVarReadFuse20_exit:
    return status;
}
