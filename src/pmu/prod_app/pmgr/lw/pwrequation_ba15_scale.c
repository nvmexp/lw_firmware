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
 * @file    pwrequation_ba15_scale.c
 * @brief   PMGR Power Equation BA v1.5 Scale Model Management
 *
 * @implements PWR_EQUATION
 *
 * @note    For information on BA15HW device see RM::pwrequation_ba15_scale.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_fxp.h"
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"
#include "pmgr/pwrequation_ba1x_scale.h"
#include "dbgprintf.h"
#include "dev_therm.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

static FLCN_STATUS s_pwrEquationBA15ScaleComputeConstF(PWR_EQUATION_BA15_SCALE *pBa15Scale);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * BA v1.5 Scale implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrEquationGrpIfaceModel10ObjSetImpl_BA15_SCALE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_EQUATION_BA15_SCALE_BOARDOBJ_SET   *pBa15ScaleDesc =
        (RM_PMU_PMGR_PWR_EQUATION_BA15_SCALE_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_EQUATION_BA15_SCALE                            *pBa15Scale;
    FLCN_STATUS                                         status;

    // Construct and initialize parent-class object.
    status = pwrEquationGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto pwrEquationGrpIfaceModel10ObjSetImpl_BA15_SCALE_done;
    }
    pBa15Scale = (PWR_EQUATION_BA15_SCALE *)*ppBoardObj;

    // Set the class-specific values:
    pBa15Scale->refVoltageuV      = pBa15ScaleDesc->refVoltageuV;
    pBa15Scale->ba2mW             = pBa15ScaleDesc->ba2mW;
    pBa15Scale->gpcClkMHz         = pBa15ScaleDesc->gpcClkMHz;
    pBa15Scale->utilsClkMHz       = pBa15ScaleDesc->utilsClkMHz;
    pBa15Scale->maxVoltageuV      = pBa15ScaleDesc->maxVoltageuV;
    pBa15Scale->dynamicEquIdx     = pBa15ScaleDesc->dynamicEquIdx;

    // Callwlate and cache constF
    status = s_pwrEquationBA15ScaleComputeConstF(pBa15Scale);
    if (status != FLCN_OK)
    {
        goto pwrEquationGrpIfaceModel10ObjSetImpl_BA15_SCALE_done;
    }

pwrEquationGrpIfaceModel10ObjSetImpl_BA15_SCALE_done:
    return status;
}

/*!
 * BA v1.5 Shift A implementation.
 *
 * @copydoc PwrEquationBA15ScaleComputeShiftA
 */
LwU8
pwrEquationBA15ScaleComputeShiftA
(
    PWR_EQUATION_BA15_SCALE *pBa15Scale,
    LwBool                   bLwrrent
)
{
    LwUFXP20_12 maxScaleA;
    LwS8 shiftA;

    // get the largest possible scaleA
    maxScaleA = pwrEquationBA15ScaleComputeScaleA(pBa15Scale, bLwrrent,
                                                  pBa15Scale->maxVoltageuV);

    // get the highest set bit of the integer portion
    HIGHESTBITIDX_32(maxScaleA);

    //
    // compute shiftA. shiftA is the amount needed to bring the MSB that is set
    // in maxScale A to the MSB location in the FACTOR_A register field. A
    // positive value indicates a left shift, a negative value indicates a
    // right shift.
    //
    shiftA = DRF_BASE(LW_TYPES_FXP_INTEGER(20, 12)) + DRF_SIZE(LW_THERM_PEAKPOWER_CONFIGX_FACTOR_A) - 1 
        - maxScaleA;

    //
    // ensure that shiftA is non-negative, since this would mean some of the
    // integer portion gets shifted out, and this loss of precision is
    // unacceptable
    //
    if (shiftA < 0)
    {
        // Incorrect VBIOS settings for BA_SUM_SHIFT.
        PMU_HALT();
    }

    return shiftA;
}

/*!
 * BA v1.5 Scale implementation.
 *
 * @copydoc PwrEquationBA15ScaleComputeScaleA
 */
LwUFXP20_12
pwrEquationBA15ScaleComputeScaleA
(
    PWR_EQUATION_BA15_SCALE *pBa15Scale,
    LwBool                   bLwrrent,
    LwU32                    voltageuV
)
{
    LwU32           voltagemV;
    LwUFXP20_12     voltageV;
    LwUFXP20_12     voltageVScaled;
    LwUFXP20_12     scaleA = 0;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwrEquation)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpExtended)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
    };
    PWR_EQUATION_DYNAMIC *pPower;

    voltagemV = LW_UNSIGNED_ROUNDED_DIV(voltageuV, 1000);
    voltageV  = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12, voltagemV, 1000);

    pPower = (PWR_EQUATION_DYNAMIC *)PWR_EQUATION_GET(pBa15Scale->dynamicEquIdx);
    if (pPower == NULL)
    {
        PMU_BREAKPOINT();
        goto pwrEquationBA15ScaleComputeScaleA_exit;
    }

    // Scale the voltage with dynamic scaling voltage exponent wrt current(mA);
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        voltageVScaled = pwrEquationDynamicScaleVoltageLwrrent(pPower, voltageV);
        //
        // Check the correctness of scaled voltage value.  When input voltage >
        // 0, then scaled voltage must be > 0.
        //
        if ((voltageV != 0) &&
            (voltageVScaled == 0))
        {
            // Incorrect scaling return 0
            PMU_HALT();
            goto pwrEquationBA15ScaleComputeScaleA_detach;
        }
        scaleA = multUFXP20_12(pBa15Scale->constF, voltageVScaled);

        if (!bLwrrent)
        {
            scaleA = multUFXP20_12(scaleA, voltageV);
        }

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_BA_WAR_200433679))
        scaleA = multUFXP20_12(scaleA, Pmgr.baInfo.scaleACompensationFactor);
#endif

pwrEquationBA15ScaleComputeScaleA_detach:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

pwrEquationBA15ScaleComputeScaleA_exit:
    return scaleA;
}
/* ------------------------- Private Functions ------------------------------ */
/*!
 * BA v1.5 Scale Equation - CONST F implementation.
 * constF = BA2mW *(UTILSCLK/GPCCLK)/Vref^(Vexp+1)
 *
 * @param[in/out]   pBa15Scale  PWR_EQUATION object pointer for BA15 scaling equ.
 *
 * @return FLCN_STATUS
 *                  FLCN_OK always
 */
static FLCN_STATUS
s_pwrEquationBA15ScaleComputeConstF
(
    PWR_EQUATION_BA15_SCALE *pBa15Scale
)
{
    LwUFXP20_12     constF;
    LwUFXP20_12     refVoltageV;
    LwU32           refVoltagemV;
    LwU32           constClkRatio;
    LwU32           constFScaled;
    FLCN_STATUS     status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwrEquation)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpExtended)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
    };
    PWR_EQUATION_DYNAMIC *pPower;

    // Colwert refVoltageuV to refVoltage
    refVoltagemV  = LW_UNSIGNED_ROUNDED_DIV(pBa15Scale->refVoltageuV, 1000);
    refVoltageV   = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12, refVoltagemV, 1000);

    //
    // Ensure that refVoltageV is not 0, otherwise could see scaled
    // values == 0 returned from @ ref
    // pwrEquationDynamicScaleVoltagePower() below.  We don't want to
    // inject sanity checks for non-zero into that interface, because
    // 0 voltage values are valid usecases with rail gating features
    // (e.g. GPC-RG).
    //
    PMU_HALT_OK_OR_GOTO(status,
        ((refVoltageV != 0) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        s_pwrEquationBA15ScaleComputeConstF_exit);

    pPower = (PWR_EQUATION_DYNAMIC *)PWR_EQUATION_GET(pBa15Scale->dynamicEquIdx);
    if (pPower == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto s_pwrEquationBA15ScaleComputeConstF_exit;
    }

    // Scaled refVoltage with (dynLwrrentVoltExp+1) (i.e wrt Power)
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        refVoltageV = pwrEquationDynamicScaleVoltagePower(pPower, refVoltageV);
        if (refVoltageV == 0)
        {
            // Check incorrect scaling
            PMU_HALT();
            status = FLCN_ERR_ILWALID_STATE;
            goto s_pwrEquationBA15ScaleComputeConstF_detach;
        }

        // constClk = UTILSCLK/GPCCLK
        constClkRatio = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12, pBa15Scale->utilsClkMHz,
                                                    pBa15Scale->gpcClkMHz);

        //
        // constF callwlations
        //
        // constF = BA2mW *(UTILSCLK/GPCCLK)/Vref^(Vexp+1)
        //
        // Numerical Analysis:
        //      UTILSCLK/GPCCLK:
        //          32.0        => 32.0
        //        / 32.0        => 32.0
        //      ------------------------
        //                      => 32.0
        //
        //      CONST F:
        //          20.12       => 20.12
        //        * 32.0  << 12 => 20.12
        //      -----------------------
        //                      => 20.12
        //      OVERFLOW CHECK: As UTILSCLK always lower than GPCCLK,
        //              (UTILSCLK/GPCCLK) will always be within 0 to 1.
        //              Thus, it can't overflow
        //
        //          20.12 << 12 => 8.24
        //        / 20.12       => 20.12
        // ----------------------------
        //                      => 20.12
        //
        //      OVERFLOW CHECK: 20.12 to 8.24 will only overflow if
        //              CONSTF is greater than 255, which won't be the case.
        //              Because BA2mW is within 0 to 10 Range and
        //                  ClkRatio  is within 0 to 1 Range. So resultant
        //              multiplication will always within 0 to 10 which is less than 255.
        //              Thus, it can't overflow
        //
        constF       = multUFXP20_12(pBa15Scale->ba2mW, constClkRatio);
        constFScaled = LW_TYPES_U32_TO_UFXP_X_Y(20, 12, constF);
        constF       = LW_UNSIGNED_ROUNDED_DIV(constFScaled, refVoltageV);

        pBa15Scale->constF = constF;

s_pwrEquationBA15ScaleComputeConstF_detach:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

s_pwrEquationBA15ScaleComputeConstF_exit:
    return status;
}
