/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchrel_regulator_loss_est.c
 * @brief Interface specification for a PWR_CHRELATIONSHIP_REGULATOR_LOSS_EST.
 *
 * The Regulator Loss Estimation Power Channel Relationship Class computes
 * regulator output current and power via regulator loss equation that
 * references another channel's power which is typically an input to the
 * regulator itself.
 *
 * This class is intended to be used to estimate the output current and power
 * of a regulator (e.g. LWVDD) that neither have ADC's nor BA to compute or
 * measure current and power on the output side of regulator.
 *
 * Given the following values:
 *      Coefficient 0 to 5 (C0 to C5) - The coefficients that are used for
 *          estimating regulator loss.
 *      Regulator Type - Type of regulator (e.g. LWVDD).
 *      Power_Channel = P_in - The current power corresponding to the
 *          specified Power Channel.
 *      Voltage_Channel = V_in - The current voltage corresponding to the
 *          specified Power Channel.
 *      Voltage_Rel = V_out - The output voltage of the regulator.
 *
 * And where:
 *      Power_Rel   = P_out - The output power of the regulator.
 *      Lwrrent_Rel = I_out - The output current of the regulator.
 *      Power_Loss  = P_loss - Regulator losses.
 *
 * Current and Power for the Regulator Loss Estimation Power Channel Relationship
 * can be callwlated as follows:
 *      P_in   = P_loss + P_out
 *      P_out  = I_out * V_out
 *      P_loss = C0 + C1 * I_out + C2 * V_out + C3 * V_in +
 *               C4 * V_out * I_out + C5 * V_in * I_out
 *      I_out  = (P_in - C0 - C2 * V_out - C3 * V_in) /
 *               (C1 + C4 * V_out + C5 * V_in + V_out)
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmu_objperf.h"
#include "pmgr/pwrchrel_regulator_loss_est.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _REGULATOR_LOSS_EST PWR_CHRELATIONSHIP object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrChRelationshipGrpIfaceModel10ObjSet_REGULATOR_LOSS_EST
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pDesc
)
{
    RM_PMU_PMGR_PWR_CHRELATIONSHIP_REGULATOR_LOSS_EST_BOARDOBJ_SET *pDescRLE =
        (RM_PMU_PMGR_PWR_CHRELATIONSHIP_REGULATOR_LOSS_EST_BOARDOBJ_SET *)pDesc;
    PWR_CHRELATIONSHIP_REGULATOR_LOSS_EST              *pRLE;
    FLCN_STATUS                                         status;

    status = pwrChRelationshipGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pRLE = (PWR_CHRELATIONSHIP_REGULATOR_LOSS_EST *)*ppBoardObj;

    // Copy in the _REGULATOR_LOSS_EST type-specific data.
    pRLE->coefficient0  = pDescRLE->coefficient0;
    pRLE->coefficient1  = pDescRLE->coefficient1;
    pRLE->coefficient2  = pDescRLE->coefficient2;
    pRLE->coefficient3  = pDescRLE->coefficient3;
    pRLE->coefficient4  = pDescRLE->coefficient4;
    pRLE->coefficient5  = pDescRLE->coefficient5;

    return status;
}

/*!
 * @copydoc PwrChRelationshipTupleEvaluate
 */
FLCN_STATUS
pwrChRelationshipTupleEvaluate_REGULATOR_LOSS_EST
(
    PWR_CHRELATIONSHIP              *pChRel,
    LwU8                             chIdxEval,
    PWR_CHRELATIONSHIP_TUPLE_STATUS *pStatus
)
{
    PWR_CHRELATIONSHIP_REGULATOR_LOSS_EST      *pRLE    =
        (PWR_CHRELATIONSHIP_REGULATOR_LOSS_EST *)pChRel;
    LW2080_CTRL_PMGR_PWR_TUPLE                  tuple   = { 0 };
    LwU32       voltOutputuV;
    LwS32       voltOutputmV;
    LwS32       voltInputmV;
    LwSFXP20_12 numeratormW;
    LwSFXP20_12 denominatorV;
    FLCN_STATUS status;

    // Query the PWR_CHANNEL tuple for this relationship.
    status = pwrMonitorChannelTupleStatusGet(Pmgr.pPwrMonitor, pChRel->chIdx,
                                             &tuple);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Obtain current output voltage.
    voltOutputuV = perfVoltageuVGet();
    if (voltOutputuV == 0)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    //
    // Colwert input and output voltages from uV to mV for math computations.
    // F32.0 uV / 1000 = F32.0 mV.
    //
    voltOutputmV = (LwS32)(voltOutputuV / 1000);
    voltInputmV  = (LwS32)(tuple.voltuV / 1000);

    //
    // Numerator = P_in - C0 - C2 * V_out - C3 * V_in.
    // F32.0 << 12 - F20.12 - F20.12 * F32.0 = F20.12.
    // F20.12 A * F32.0 mV = F20.12 mW.
    //
    // Note: 32BIT_OVERFLOW - Possible FXP20.12 overflow, not going to fix
    // because this code segment is tied to the
    // @ref PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_REGULATOR_LOSS_EST
    // feature and is not used on AD10X and later chips.
    //
    numeratormW =  LW_TYPES_S32_TO_SFXP_X_Y(20, 12, (LwS32)tuple.pwrmW) -
                   pRLE->coefficient0 -
                  (pRLE->coefficient2 * voltOutputmV) -
                  (pRLE->coefficient3 * voltInputmV);

    //
    // Denominator = C1 + C4 * V_out + C5 * V_in + V_out =>
    // Denominator = C1 + (1 + C4) * V_out + C5 * V_in.
    // F20.12 + F20.12 * F32.0 = F20.12.
    // F20.12 mV / 1000 = F20.12 V.
    //
    // Note: 32BIT_OVERFLOW - Possible FXP20.12 overflow, not going to fix
    // because this code segment is tied to the
    // @ref PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_REGULATOR_LOSS_EST
    // feature and is not used on AD10X and later chips.
    //
    denominatorV = (pRLE->coefficient1 +
                   ((LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 1) + pRLE->coefficient4) * voltOutputmV) +
                   (pRLE->coefficient5 * voltInputmV)) / 1000;

    // F20.12 / F20.12 = F32.0.
    pStatus->lwrrmA = (denominatorV != LW_TYPES_FXP_ZERO) ?
                        (numeratormW / denominatorV) : LW_TYPES_FXP_ZERO;
    pStatus->voltuV = (LwS32)voltOutputuV;
    pStatus->pwrmW  = (pStatus->lwrrmA * voltOutputmV) / 1000;

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
