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
 * @file  pwrchrel_regulator_eff_est_v1.c
 * @brief Interface specification for a PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1.
 *
 * The Regulator efficiency Estimation version 1.0 (V1) Power Channel Relationship
 * Class estimates the regulator efficiency using primary and secondary topology
 * channels for either input side or output side and based on an equation model
 * with known coefficients.
 *
 * 1. When the primary channel input unit is specified in milliWatts (mW), the
 * efficiency equation is
 * Efficiency = C0 * v(P)^2 + C1 * v(S)^2 + C2 * w(P)^2/10^6 +C3 * v(P)    +
 *              C4 * v(S) + C5 * w(P) + C6 * v(P) * v(S) +C7 * v(P) * w(P) +
 *              C8 * v(S) * w(P) + C9
 *
 * 2. When the primary channel unit is specified in milliAmps (mA), the
 * efficiency equation is
 * Efficiency = C0 * v(P)^2 + C1 * v(S)^2 + C2 * I(P)^2/10^6 +C3 * v(P)   +
 *              C4 * v(S) + C5 * I(P) + C6* v(P) * v(S) +C7 * v(P) * I(P) +
 *              C8 * v(S) * I(P) + C9
 * ** Note that onlythe term 2 , term 5 and term 7 are different and depends
 *    on the primary channel unit
 * ** The coefficient C2 is prescaled up value ( *10^6) from VBIOS to fit
 *    the value in 20.12 FXP width, so term 2 also needs to scaled back by 10^6.
 *
 *
 * Since the same uniform model can be used to estimate either input side or
 * output side efficiency, the direction bit is used to figure out if we need to
 * multiply or divide with the primary channel to cacluate the final tuple
 * i.e Pout = Pin * Efficiency
 *             -or -
 *     Pin  = Pout / Efficiency
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmu_objperf.h"
#include "pmgr/pwrchrel_regulator_eff_est_v1.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _REGULATOR_EFF_EST_V1 PWR_CHRELATIONSHIP object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrChRelationshipGrpIfaceModel10ObjSet_REGULATOR_EFF_EST_V1
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pDesc
)
{
    RM_PMU_PMGR_PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1_BOARDOBJ_SET *pDescReffE =
        (RM_PMU_PMGR_PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1_BOARDOBJ_SET *)pDesc;
    PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1            *pReffE;
    FLCN_STATUS                                         status;

    status = pwrChRelationshipGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pDesc);
    if (status != FLCN_OK)
    {
        goto pwrChRelationshipGrpIfaceModel10ObjSet_REGULATOR_EFF_EST_V1_exit;
    }
    pReffE = (PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1 *)*ppBoardObj;

    // Copy in the _REGULATOR_EFF_EST_V1 type-specific data.
    pReffE->chIdxSecondary = pDescReffE->chIdxSecondary;
    pReffE->unitPrimary    = pDescReffE->unitPrimary;
    pReffE->direction      = pDescReffE->direction;
    pReffE->coefficient0   = pDescReffE->coefficient0;
    pReffE->coefficient1   = pDescReffE->coefficient1;
    pReffE->coefficient2   = pDescReffE->coefficient2;
    pReffE->coefficient3   = pDescReffE->coefficient3;
    pReffE->coefficient4   = pDescReffE->coefficient4;
    pReffE->coefficient5   = pDescReffE->coefficient5;
    pReffE->coefficient6   = pDescReffE->coefficient6;
    pReffE->coefficient7   = pDescReffE->coefficient7;
    pReffE->coefficient8   = pDescReffE->coefficient8;
    pReffE->coefficient9   = pDescReffE->coefficient9;

pwrChRelationshipGrpIfaceModel10ObjSet_REGULATOR_EFF_EST_V1_exit:
    return status;
}

/*!
 * @copydoc PwrChRelationshipTupleEvaluate
 */
FLCN_STATUS
pwrChRelationshipTupleEvaluate_REGULATOR_EFF_EST_V1
(
    PWR_CHRELATIONSHIP              *pChRel,
    LwU8                             chIdxEval,
    PWR_CHRELATIONSHIP_TUPLE_STATUS *pStatus
)
{
    PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1 *pReffE =
        (PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1 *)pChRel;
    LW2080_CTRL_PMGR_PWR_TUPLE tuplePrimary          =    { 0 };
    LW2080_CTRL_PMGR_PWR_TUPLE tupleSecondary        =    { 0 };
    FLCN_STATUS status;

    // Read the primary channel's tuple from primary channel index
    status = pwrMonitorChannelTupleStatusGet(Pmgr.pPwrMonitor,
        pChRel->chIdx, &tuplePrimary);
    if (status != FLCN_OK)
    {
        goto pwrChRelationshipTupleEvaluate_REGULATOR_EFF_EST_V1_exit;
    }

    // Read the primary channel's tuple from primary channel index
    status = pwrMonitorChannelTupleStatusGet(Pmgr.pPwrMonitor,
        pReffE->chIdxSecondary, &tupleSecondary);
    if (status != FLCN_OK)
    {
        goto pwrChRelationshipTupleEvaluate_REGULATOR_EFF_EST_V1_exit;
    }

    //
    // Call the helper to callwlate the efficiency using V1.0 coefficients
    // and apply it to the secondary tuple
    //
    status = pwrChRelationshipRegulatorEffEstV1EvalSecondary(pReffE, &tuplePrimary,
        &tupleSecondary);
    if (status != FLCN_OK)
    {
        goto pwrChRelationshipTupleEvaluate_REGULATOR_EFF_EST_V1_exit;
    }

    pStatus->lwrrmA = (LwS32)tupleSecondary.lwrrmA;
    pStatus->voltuV = (LwS32)tupleSecondary.voltuV;
    pStatus->pwrmW  = (LwS32)tupleSecondary.pwrmW;

pwrChRelationshipTupleEvaluate_REGULATOR_EFF_EST_V1_exit:
    return status;
}

/*!
 * @brief Callwlates regulator efficienacy based on class params
 * (efficiency coeffcients (10), primary unit, primary and secondary tuples)
 * The efficiency then  gets muliplied or divided to primary tuples,
 * and final power (mW) and current (mA) values are estimated based on
 * class params direction ( input to output or output to input)
 *
 * @param[in]      pReffE  Pointer to PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1 object.
 * @param[in]      pTuplePrimary  Pointer to the primary channel tuple structure.
 * @param[in/out]  pTupleSecondary  Pointer to the secondary channel tuple structure.
 *
 * @return  FLCN_OK  on success
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT if any input pointers are NULL
 *
 */
FLCN_STATUS
pwrChRelationshipRegulatorEffEstV1EvalSecondary
(
    PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1  *pReffE,
    LW2080_CTRL_PMGR_PWR_TUPLE               *pTuplePrimary,
    LW2080_CTRL_PMGR_PWR_TUPLE               *pTupleSecondary
)
{
    FLCN_STATUS status     = LW_OK;
    LwU64       efficiency;
    LwU64       tmp1;
    LwU64       tmp2;
    LwU64       tmp3;
    LwU64       pwrmWOrLwrrmA;
    LwU32       secondarymV;

    if ((pReffE          == NULL) ||
        (pTuplePrimary   == NULL) ||
        (pTupleSecondary == NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto pwrChRelationshipRegulatorEffEstV1EvalSecondary_exit;
    }

    //
    //  First callwlate the common terms applicable to both
    //  unitPrimary is power (W) and current (A) types
    //  Where all the coefficients are prescaled by 10^3 in the VBIOS
    //  Efficiency = C0*v(P)^2 + C1*v(S)^2    + C3*v(P) +
    //               C4*v(S)   + C6*v(P)*v(S) + C9  in unit of Volt
    //                  OR
    //  Efficiency = C9  + (v(P) (C0 * v(P) + C3 + C6 * v(S))
    //                   + (v(S) (C1 * v(S) + C4))  in unit of Volt
    //                  OR
    //  Efficiency = (C9 * 10^12  + (v_uV(P) (C0 * v_uV(P) + C3 * 10^6 + C6 * v_uV(S))
    //                  + (v_uV(S) (C1 * v_uV(S) + C4 * 10^6)))/10^12  in unit of micro Volt
    //

    // Efficiency * 10^12 = (C9 * 10^12)  in FXP52_12
    tmp1 = 1000000000000;
    tmp2 = (LwU64)pReffE->coefficient9;
    lwU64Mul(&efficiency, &tmp2, &tmp1);

    //
    // Efficiency * 10^12 = (C9 * 10^12) + (v_uV(P) (C0 * v_uV(P) + C3 * 10^6 + C6 * v_uV(S)) in FXP52_12
    // 1. C0 * v_uV(P) in FXP52_12
    //
    tmp1 = (LwU64)pReffE->coefficient0;
    tmp2 = (LwU64)pTuplePrimary->voltuV;
    lwU64Mul(&tmp2, &tmp1, &tmp2);

    // 2. C3 * 10^6  in FXP52_12
    tmp1 = (LwU64)pReffE->coefficient3;
    tmp3 = 1000000;
    lwU64Mul(&tmp3, &tmp1, &tmp3);

    // 3. (C0 * v_uV(P) + C3 * 10^6) in FXP52_12
    lw64Add(&tmp1, &tmp2, &tmp3);

    // 4. C6 * v_uV(S) in FXP52_12
    tmp2 = (LwU64)pReffE->coefficient6;
    tmp3 = (LwU64)pTupleSecondary->voltuV;
    lwU64Mul(&tmp3, &tmp2, &tmp3);

    // 5. (C0 * v_uV(P) + C3 * 10^6 + C6 * v_uV(S)) in FXP52_12
    lw64Add(&tmp2, &tmp1, &tmp3);

    // 6. (v_uV(P) (C0 * v_uV(P) + C3 * 10^6 + C6 * v_uV(S)) in FXP52_12
    tmp1 = (LwU64)pTuplePrimary->voltuV;
    lwU64Mul(&tmp3, &tmp1, &tmp2);

    // 7. Efficiency * 10^12 = (C9 * 10^12) + (v_uV(P) (C0 * v_uV(P) + C3 * 10^6 + C6 * v_uV(S)) in FXP52_12
    lw64Add(&efficiency, &tmp3, &efficiency);

    //
    // Efficiency * 10^12 = (C9 * 10^12) + (v_uV(P) (C0 * v_uV(P) + C3 * 10^6 + C6 * v_uV(S))
    //                      + (v_uV(S) (C1 * v_uV(S) + C4 * 10^6)  in FXP52_12
    // 1. C1 * v_uV(S)  in FXP52_12
    //
    tmp1 = (LwU64)pReffE->coefficient1;
    tmp2 = (LwU64)pTupleSecondary->voltuV;
    lwU64Mul(&tmp2, &tmp1, &tmp2);

    // 2. C4 * 10 ^6 in FXP52_12
    tmp1 = (LwU64)pReffE->coefficient4;
    tmp3 = 1000000;
    lwU64Mul(&tmp3, &tmp1, &tmp3);

    // 3. (C1 * v_uV(S) + C4 * 10 ^6) in FXP52_12
    lw64Add(&tmp1, &tmp2, &tmp3);

    // 4. (v_uV(S) (C1 * v_uV(S) + C4 * 10 ^6) in FXP52_12
    tmp2 = (LwU64)pTupleSecondary->voltuV;
    lwU64Mul(&tmp3, &tmp1, &tmp2);

    //
    // 5 Efficiency * 10^12 = (C9 * 10^12) + (v_uV(P) (C0 * v_uV(P) + C3 * 10^6 + C6 * v_uV(S))
    //                      + (v_uV(S) (C1 * v_uV(S) + C4 * 10^6)  in FXP52_12
    //
    lw64Add(&efficiency, &tmp3, &efficiency);

    //
    // Evaluate the terms based on the unit primary (UNIT_MW or UNIT_MA) input argument.
    // Primary Unit = Watts, sum extra coefficients using following equation
    // C2 is additionally prescaled to 10^3 extra to fit into FXP20:12 in the VBIOS
    //
    // 1. Efficiency += (C2*w(P)^2)/10^3 + C5*w(P) + C7*v(P)*w(P) + C8*v(S)*w(P)  if unit primary is MW
    //                 -OR-
    // 2 .Efficiency += (C2*I(P)^2)/10^3 + C5*I(P) + C7*v(P)*I(P) + C8*v(S)*I(P)  if unit primary is MA
    //
    // 1. Efficiency = (w_mW(P)(C2 * w_mW(P) + C5 *10^6    + C7 * v_uV(P)  + C8 *v_uV(S)))/10^9   in Watts if unit primary is MW
    //                -OR-
    // 2. Efficiency = (I_mA(P)(C2 * I_mA(P) + C5 *10^6    + C7 * v_uV(P)  + C8 *v_uV(S)))/10^9   in Amps if unit primary is MA
    //
    // 1. Efficiency = ((w_mW(P) *10^3)(C2 * w_mW(P) + C5 *10^6  + C7 * v_uV(P)  + C8 *v_uV(S)))/10^12   in Watts if unit primary is MW
    //                -OR-
    // 2. Efficiency = ((I_mA(P) * 10^3)(C2 * I_mA(P) + C5 *10^6 + C7 * v_uV(P)  + C8 *v_uV(S)))/10^12   in Amps if unit primary is MA
    //

    // Pick the equation based on primary unit
    if (pReffE->unitPrimary ==
        LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1_PRIMARY_UNIT_MW)
    {
        pwrmWOrLwrrmA = (LwU64)pTuplePrimary->pwrmW;
    }
    else
    {
        pwrmWOrLwrrmA = (LwU64)pTuplePrimary->lwrrmA;
    }
    //
    // Following is an example if we pick equation (1) above but this is true for (2) since both are in milli (10^3)factor
    // Efficiency * 10^12 = ((w_mW(P) *10^3)(C2 * w_mW(P) + C5 *10^6  + C7 * v_uV(P)  + C8 *v_uV(S))) in FXP52_12
    // 1. C2 * w_mW(P) in FXP52_12
    //
    tmp1 = (LwU64)pReffE->coefficient2;
    lwU64Mul(&tmp3, &tmp1, &pwrmWOrLwrrmA);

    // 2. C5 *10^6  in FXP52_12
    tmp1 = (LwU64)pReffE->coefficient5;
    tmp2 = 1000000;
    lwU64Mul(&tmp2, &tmp1, &tmp2);

    // 3. C2 * w_mW(P) + C5 *10^6 in FXP52_12
    lw64Add(&tmp1, &tmp3, &tmp2);

    // 4. C7 * v_uV(P) in FXP52_12
    tmp2 = (LwU64)pReffE->coefficient7;
    tmp3 = (LwU64)pTuplePrimary->voltuV;
    lwU64Mul(&tmp3, &tmp2, &tmp3);

    // 5. (C2 * w_mW(P) + C5 *10^6 + C7 * v_uV(P)) in FXP52_12
    lw64Add(&tmp1, &tmp3, &tmp1);

    // 6. C8 *v_uV(S) in FXP52_12
    tmp2 = (LwU64)pReffE->coefficient8;
    tmp3 = (LwU64)pTupleSecondary->voltuV;
    lwU64Mul(&tmp3, &tmp2, &tmp3);

    // 7. (C2 * w_mW(P) + C5 *10^6  + C7 * v_uV(P)  + C8 *v_uV(S)) in FXP52_12
    lw64Add(&tmp1, &tmp3, &tmp1);

    // 8. (w_mW(P)(C2 * w_mW(P) + C5 *10^6 + C7 * v_uV(P)  + C8 *v_uV(S))) in FXP52_12
    lwU64Mul(&tmp1, &pwrmWOrLwrrmA, &tmp1);

    // 9. ((w_mW(P) * 10^3)(C2 * w_mW(P) + C5 *10^6 + C7 * v_uV(P)  + C8 *v_uV(S))) in FXP52_12
    tmp2 = 1000;
    lwU64Mul(&tmp1, &tmp1, &tmp2);

    // 10. Efficiency * 10^12 = common terms + terms selected from primary unit in FXP52_12
    lw64Add(&efficiency, &efficiency, &tmp1);

    //
    // 11. Efficiency = Efficiency/10^12 in FXP52_12,
    // Note: At this point efficiency should be positive so LwU64Div can be used safely.
    //
    tmp2 = 1000000000000;
    lwU64Div(&efficiency, &efficiency, &tmp2);

    //
    // Note: Efficiency callwlated  above is w.r.t. (W,A) and in FXP52_12 format, Since the coefficients
    // are all multiplied with 10^3 from VBIOS so it is still scaled to 10^3 at this point
    //
    tmp1 = (LwU64)pTuplePrimary->pwrmW;

    //
    // Finally multiply or divide the efficiency to get the output
    // tupleSecondary(mW, mA) = tuplePrimary (mW, mA) * Efficiency (mW, mA)( direction = _IN_OUT)
    // tupleSecondary(mW, mA) = tuplePrimary (mW, mA) / Efficiency (mW, mA)( direction = _OUT_IN)
    //
    if (pReffE->direction ==
        LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1_DIRECTION_IN_OUT)
    {
        //
        // Efficiency is in FXP52_12 and scaled to 10^3 at this point
        // -or- Efficiency = ((Efficiency/10^3)>>12) or Efficiency/(10^3 * 4096)
        // tuplePrimary (mW) * Efficiency  in FXP52_12 * 10^3
        //
        lwU64Mul(&tmp2, &tmp1, &efficiency);

        // tuplePrimary (mW) * Efficiency
        tmp1 = (LwU64)(1000 * LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1));
        lwU64Div(&tmp2, &tmp2, &tmp1);



    }
    else
    {
        //
        // Efficiency is in FXP52_12 and scaled to 10^3 at this point
        // -or- Efficiency = ((Efficiency/10^3)>>12) or Efficiency/(10^3 * 4096)
        // 1/Efficiency = (10^3 * 4096)/Efficiency
        // tuplePrimary multiplied to  (10^3 * 4096) from equation above
        //
        tmp2 = (LwU64)(1000 * LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1));
        lwU64Mul(&tmp1, &tmp1, &tmp2);

        // tuplePrimary (mW) / Efficiency  in FXP52_12 * 10^3
        lwU64Div(&tmp2, &tmp1, &efficiency);
    }

    // Copy out the secondary power (mW) in LwU32 from tmp2
    pTupleSecondary->pwrmW = LwU64_LO32(tmp2);

    // Callwlate the secondary current (mA) based on secondary voltage (uV)
    secondarymV = LW_UNSIGNED_ROUNDED_DIV(pTupleSecondary->voltuV, 1000);
    pTupleSecondary->lwrrmA = (secondarymV != 0) ?
        (LW_UNSIGNED_ROUNDED_DIV(pTupleSecondary->pwrmW * 1000, secondarymV)) : 0;

pwrChRelationshipRegulatorEffEstV1EvalSecondary_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
