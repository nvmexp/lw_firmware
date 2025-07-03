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
 * @file clk_volt_controller_prop.c
 *
 * @brief Module managing all state related to the CLK_VOLT_CONTROLLER_PROP
 * structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "clk/clk_volt_controller.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_VOLT_CONTROLLER_PROP class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkVoltControllerGrpIfaceModel10ObjSet_PROP
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLK_VOLT_CONTROLLER_PROP_BOARDOBJ_SET *pSet = NULL;
    CLK_VOLT_CONTROLLER_PROP *pVoltControllerPROP = NULL;
    FLCN_STATUS               status              = FLCN_OK;

    // Call into CLK_VOLT_CONTROLLER super-constructor
    status = clkVoltControllerGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVoltControllerGrpIfaceModel10ObjSet_PROP_exit;
    }
    pSet = (RM_PMU_CLK_CLK_VOLT_CONTROLLER_PROP_BOARDOBJ_SET *)pBoardObjDesc;
    pVoltControllerPROP = (CLK_VOLT_CONTROLLER_PROP *)*ppBoardObj;

    // Copy the CLK_VOLT_CONTROLLER_PROP-specific data.
    pVoltControllerPROP->voltHysteresisPositive = pSet->voltHysteresisPositive;
    pVoltControllerPROP->voltHysteresisNegative = pSet->voltHysteresisNegative;
    pVoltControllerPROP->propGain               = pSet->propGain;

    // Reset poison counter
    pVoltControllerPROP->poisonCounter          = 0;

clkVoltControllerGrpIfaceModel10ObjSet_PROP_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkVoltControllerEntryIfaceModel10GetStatus_PROP
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_CLK_CLK_VOLT_CONTROLLER_PROP_BOARDOBJ_GET_STATUS *pGetStatus = NULL;
    CLK_VOLT_CONTROLLER_PROP *pVoltControllerPROP = NULL;
    FLCN_STATUS               status              = FLCN_OK;

    // Call super-class implementation.
    status = clkVoltControllerIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVoltControllerEntryIfaceModel10GetStatus_PROP_exit;
    }
    pVoltControllerPROP = (CLK_VOLT_CONTROLLER_PROP *)pBoardObj;
    pGetStatus  = (RM_PMU_CLK_CLK_VOLT_CONTROLLER_PROP_BOARDOBJ_GET_STATUS *)pPayload;

    // Set the query parameters.
    pGetStatus->erroruV       = pVoltControllerPROP->erroruV;
    pGetStatus->senseduV      = pVoltControllerPROP->senseduV;
    pGetStatus->measureduV    = pVoltControllerPROP->measureduV;
    pGetStatus->poisonCounter = pVoltControllerPROP->poisonCounter;

clkVoltControllerEntryIfaceModel10GetStatus_PROP_exit:
    return status;
}

/*!
 * Evaluates the PROP voltage controller.
 *
 * @param[in] pVoltControllerPROP   PROP voltage controller object pointer
 *
 * @return Status returned from functions called within.
 * @return FLCN_OK
 *      Successfully evaluated the voltage controller.
 */
FLCN_STATUS
clkVoltControllerEval_PROP
(
    CLK_VOLT_CONTROLLER *pVoltController
)
{
    LwSFXP20_12               propTerm;
    LwSFXP20_12               errorLwrrentuV;
    FLCN_STATUS               status = FLCN_OK;
    CLK_VOLT_CONTROLLER_PROP *pVoltControllerPROP = 
        (CLK_VOLT_CONTROLLER_PROP *)pVoltController;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkVoltController)
    };

    // Init per sample voltage offset (uV)
    CLK_VOLT_CONTROLLER_VOLT_OFFSET_UV_SET(pVoltController, 0);

    // Update sensed voltage (uV)
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        pVoltControllerPROP->senseduV = pVoltController->pSensedVoltData->voltageuV;
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    // Update measured voltage (uV)
    status = swCounterAvgCompute(COUNTER_AVG_CLIENT_ID_VOLT,
                pVoltController->voltRailIdx,
               &pVoltControllerPROP->voltuVTargetSample,
               &pVoltControllerPROP->measureduV);
    if (status != FLCN_OK)
    {
        goto clkVoltControllerEval_PROP_exit;
    }

    // Bail out early if this sample is poisoned
    if (clkVoltControllerSamplePoisoned_PROP(pVoltControllerPROP))
    {
        status = FLCN_OK;
        pVoltControllerPROP->poisonCounter++;
        goto clkVoltControllerEval_PROP_exit;
    }

    // Reset poison counter
    pVoltControllerPROP->poisonCounter = 0;

    // Cache the current error to be reported out to external tool
    pVoltControllerPROP->erroruV =
        pVoltControllerPROP->measureduV - pVoltControllerPROP->senseduV;

    // Return early if the error is within the +/- hysteresis.
    if ((pVoltControllerPROP->erroruV >= (pVoltControllerPROP->voltHysteresisNegative)) &&
        (pVoltControllerPROP->erroruV <= (pVoltControllerPROP->voltHysteresisPositive)))
    {
        status = FLCN_OK;
        goto clkVoltControllerEval_PROP_exit;
    }

    // Error (uV) of the current cycle colwerted to LwSFXP20_12
    errorLwrrentuV =
        LW_TYPES_S32_TO_SFXP_X_Y(20, 12, pVoltControllerPROP->erroruV);

    // Proportional term (uV) = propGain (uV) * errorLwrrent (uV)
    propTerm = multSFXP20_12(pVoltControllerPROP->propGain, errorLwrrentuV);

    // Offset voltage (uV) for this iteration = Proportional term (uV)
    CLK_VOLT_CONTROLLER_VOLT_OFFSET_UV_SET(pVoltController, 
        LW_TYPES_SFXP_X_Y_TO_S32(20, 12, propTerm));

    status = clkVoltControllerEval_SUPER(pVoltController);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVoltControllerEval_PROP_exit;
    }

clkVoltControllerEval_PROP_exit:
    return status;
}

/*!
 * Reset the PROP voltage controller
 *
 * @param[in] pVoltControllerPROP   PROP voltage controller object pointer
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkVoltControllerReset_PROP
(
    CLK_VOLT_CONTROLLER *pVoltController
)
{
    CLK_VOLT_CONTROLLER_PROP *pVoltControllerPROP = 
        (CLK_VOLT_CONTROLLER_PROP *)pVoltController;

    (void)swCounterAvgCompute(
        COUNTER_AVG_CLIENT_ID_VOLT,
        pVoltController->voltRailIdx,
        &pVoltControllerPROP->voltuVTargetSample,
        &pVoltControllerPROP->measureduV);

    pVoltControllerPROP->erroruV       = 0;
    pVoltControllerPROP->senseduV      = 0;
    pVoltControllerPROP->measureduV    = 0;
    pVoltControllerPROP->poisonCounter = 0;

    return FLCN_OK;
}

/*!
 * Check if the voltage controller sample is poisoned
 *
 * @param[in] pVoltControllerPROP   PROP voltage controller object pointer
 *
 * @return LW_TRUE - The sample is poisoned.
 * @return LW_FALSE - Otherwise.
 */
LwBool
clkVoltControllerSamplePoisoned_PROP
(
    CLK_VOLT_CONTROLLER_PROP *pVoltControllerPROP
)
{
    LwBool bPoison = LW_FALSE;

    bPoison = clkVoltControllerSamplePoisonedGrRg_PROP(pVoltControllerPROP) ||
              clkVoltControllerSamplePoisonedDroopy_PROP(pVoltControllerPROP);

    return bPoison;
}

/*!
 * Check if the voltage controller sample is poisoned because of GR-RG
 *
 * @param[in] pVoltControllerPROP   PROP voltage controller object pointer
 *
 * @return LW_TRUE - The sample is poisoned.
 * @return LW_FALSE - Otherwise.
 */
LwBool
clkVoltControllerSamplePoisonedGrRg_PROP
(
    CLK_VOLT_CONTROLLER_PROP *pVoltControllerPROP
)
{
    LwBool      bPoison = LW_FALSE;
    FLCN_STATUS status  = FLCN_OK;

    status = clkVoltControllerSamplePoisonedGrRg_SUPER(
                (CLK_VOLT_CONTROLLER *)pVoltControllerPROP, &bPoison);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVoltControllerSamplePoisonedGrRg_PROP_exit;
    }

clkVoltControllerSamplePoisonedGrRg_PROP_exit:
    return bPoison;
}

/*!
 * Check if the voltage controller sample is poisoned because of
 * droopy VR engaged.
 *
 * The sample is poisoned if droopy was enagaged for more time than the
 * minimum limit @ref droopyPctMin.
 *
 * @param[in] pVoltControllerPROP   PROP voltage controller object pointer
 *
 * @return LW_TRUE - The sample is poisoned.
 * @return LW_FALSE - Otherwise.
 */
LwBool
clkVoltControllerSamplePoisonedDroopy_PROP
(
    CLK_VOLT_CONTROLLER_PROP *pVoltControllerPROP
)
{
    LwBool      bPoison = LW_FALSE;
    FLCN_STATUS status  = FLCN_OK;

    status = clkVoltControllerSamplePoisonedDroopy_SUPER(
                (CLK_VOLT_CONTROLLER *)pVoltControllerPROP, &bPoison);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVoltControllerSamplePoisonedDroopy_PROP_exit;
    }

clkVoltControllerSamplePoisonedDroopy_PROP_exit:
    return bPoison;
}

/* ------------------------- Private Functions ------------------------------ */
