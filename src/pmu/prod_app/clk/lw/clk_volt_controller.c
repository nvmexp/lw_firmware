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
 * @file clk_volt_controller.c
 *
 * @brief Module managing all state related to the CLK_VOLT_CONTROLLER structure.
 */
//! https://confluence.lwpu.com/pages/viewpage.action?pageId=159882880

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "pmu_objpg.h"
#include "clk/clk_volt_controller.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader    (s_clkVoltControllerIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkVoltControllerIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry     (s_clkVoltControllerIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkVoltControllerIfaceModel10SetEntry");
static BoardObjIfaceModel10GetStatus                 (s_clkVoltControllerIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "s_clkVoltControllerIfaceModel10GetStatus");
static BoardObjGrpIfaceModel10GetStatusHeader    (s_clkVoltControllerIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "s_clkVoltControllerIfaceModel10GetStatusHeader");

/* ------------------------ Macros ----------------------------------------- */

/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_VOLT_CONTROLLER handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkVoltControllerBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    LwBool          bFirstTime    = (CLK_VOLT_CONTROLLERS_GET()->super.super.objSlots == 0U);
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkVoltController)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,    // _grpType
            CLK_VOLT_CONTROLLER,                        // _class
            pBuffer,                                    // _pBuffer
            s_clkVoltControllerIfaceModel10SetHeader,         // _hdrFunc
            s_clkVoltControllerIfaceModel10SetEntry,          // _entryFunc
            turingAndLater.clk.clkVoltControllerGrpSet, // _ssElement
            OVL_INDEX_DMEM(clkVoltController),          // _ovlIdxDmem
            BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    // Trigger ilwalidation on SET_CONTROL cmds to respect new delta ranges.
    if (!bFirstTime)
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkController)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkVoltController)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Override the delta to zero on set control
            status = perfClkControllersIlwalidate(LW_TRUE);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVoltControllerBoardObjGrpIfaceModel10Set_exit;
        }
    }

clkVoltControllerBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @brief Queries all boardobj devices
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkVoltControllerBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkVoltController)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32,                    // _grpType
            CLK_VOLT_CONTROLLER,                                // _class
            pBuffer,                                            // _pBuffer
            s_clkVoltControllerIfaceModel10GetStatusHeader,                 // _hdrFunc
            s_clkVoltControllerIfaceModel10GetStatus,                       // _entryFunc
            turingAndLater.clk.clkVoltControllerGrpGetStatus);  // _ssElement
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * CLK_VOLT_CONTROLLER super-class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkVoltControllerGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    RM_PMU_CLK_CLK_VOLT_CONTROLLER_BOARDOBJ_SET *pSet = NULL;
    CLK_VOLT_CONTROLLER *pVoltController = NULL;
    FLCN_STATUS          status          = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        CLK_VOLT_CONTROLLER_SENSED_VOLTAGE_OVERLAYS
    };
    LwBool bGrRgEnabled = PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG) &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_GR_RG);

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // Call into BOARDOBJ super-constructor, which will do actual memory
        // allocation.
        //
        status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBuf);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVoltControllerGrpIfaceModel10ObjSet_SUPER_exit;
        }
        pSet = (RM_PMU_CLK_CLK_VOLT_CONTROLLER_BOARDOBJ_SET *)pBuf;
        pVoltController = (CLK_VOLT_CONTROLLER *)*ppBoardObj;

        // Copy the CLK_VOLT_CONTROLLER-specific data.
        pVoltController->adcMode            = pSet->adcMode;
        pVoltController->adcMask            = pSet->adcMask;
        pVoltController->voltRailIdx        = pSet->voltRailIdx;
        pVoltController->thermMonIdx        = pSet->thermMonIdx;
        pVoltController->droopyPctMin       = pSet->droopyPctMin;
        pVoltController->voltOffsetMinuV    = pSet->voltOffsetMinuV;
        pVoltController->voltOffsetMaxuV    = pSet->voltOffsetMaxuV;
        pVoltController->disableClientsMask = pSet->disableClientsMask;

        // Allocate memory for the VOLT_RAIL_SENSED_VOLTAGE_DATA samples
        if (pVoltController->pSensedVoltData == NULL)
        {
            pVoltController->pSensedVoltData =
            lwosCallocType(OVL_INDEX_DMEM(clkVoltController), 1,
                VOLT_RAIL_SENSED_VOLTAGE_DATA);
            if (pVoltController->pSensedVoltData == NULL)
            {
                status = FLCN_ERR_NO_FREE_MEM;
                PMU_BREAKPOINT();
                goto clkVoltControllerGrpIfaceModel10ObjSet_SUPER_exit;
            }
        }

        if (pVoltController->pSensedVoltData->pClkAdcAccSample == NULL)
        {
            status = voltRailSensedVoltageDataConstruct(
                pVoltController->pSensedVoltData,
                VOLT_RAIL_GET(pVoltController->voltRailIdx),
                pVoltController->adcMode,
                OVL_INDEX_DMEM(clkVoltController));

            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVoltControllerGrpIfaceModel10ObjSet_SUPER_exit;
            }
        }

        if (voltRailVoltDomainColwertToIdx(LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD) !=
                pVoltController->voltRailIdx)
        {
            //
            // MSVDD Rail is ALWAYS ON, no poisoning required.
            // Cache only if volt conroller is not associated with MSVDD rail.
            //
            if (bGrRgEnabled)
            {
                appTaskCriticalEnter();
                {
                    pVoltController->grRgGatingCount =
                    PG_STAT_ENTRY_COUNT_GET(RM_PMU_LPWR_CTRL_ID_GR_RG);
                }
                appTaskCriticalExit();
            }
        }

clkVoltControllerGrpIfaceModel10ObjSet_SUPER_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkVoltControllerIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_CLK_CLK_VOLT_CONTROLLER_BOARDOBJ_GET_STATUS *pGetStatus = NULL;
    CLK_VOLT_CONTROLLER *pVoltController = NULL;
    FLCN_STATUS          status          = FLCN_OK;

    status = boardObjIfaceModel10GetStatus(pModel10, pBuf);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVoltControllerIfaceModel10GetStatus_SUPER_exit;
    }
    pVoltController = (CLK_VOLT_CONTROLLER *)pBoardObj;
    pGetStatus = (RM_PMU_CLK_CLK_VOLT_CONTROLLER_BOARDOBJ_GET_STATUS *)pBuf;

    // Get the CLK_VOLT_CONTROLLER query parameters if any
    pGetStatus->disableClientsMask = pVoltController->disableClientsMask;
    pGetStatus->voltOffsetuV       = CLK_VOLT_CONTROLLER_VOLT_OFFSET_UV_GET(pVoltController);

clkVoltControllerIfaceModel10GetStatus_SUPER_exit:
    return status;
}

/*!
 * Update cached voltage offset with the value which is actually
 * applied by the voltage code.
 *
 * @param[in] pVoltController   Voltage controller object pointer
 *
 * @return Status returned from functions called within.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If it's a unsupported voltage controller type.
 */
FLCN_STATUS
clkVoltControllersTotalMaxVoltOffsetuVUpdate
(
    CLK_VOLT_CONTROLLER *pVoltController
)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqClient)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        Clk.voltControllers.totalMaxVoltOffsetuV[pVoltController->voltRailIdx] = 
            perfChangeSeqChangeLastVoltOffsetuVGet(pVoltController->voltRailIdx,
                LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLVC);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK;
}

/*!
 * Evaluates the voltage controller based on the type.
 *
 * @param[in] pVoltController   Voltage controller object pointer
 *
 * @return Status returned from functions called within.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If it's a unsupported voltage controller type.
 */
FLCN_STATUS
clkVoltControllerEval
(
    CLK_VOLT_CONTROLLER *pVoltController
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;
    OSTASK_OVL_DESC ovlDescList[] = {
        CLK_VOLT_CONTROLLER_SENSED_VOLTAGE_OVERLAYS
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = voltRailGetVoltageSensed(
                    VOLT_RAIL_GET(pVoltController->voltRailIdx),
                    pVoltController->pSensedVoltData);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVoltControllerEval_exit;
    }

    switch (BOARDOBJ_GET_TYPE(pVoltController))
    {
        case LW2080_CTRL_CLK_CLK_VOLT_CONTROLLER_TYPE_PROP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER_PROP))
            {
                status = clkVoltControllerEval_PROP(pVoltController);
            }
            break;
        }
        default:
        {
            // Nothing to do
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVoltControllerEval_exit;
    }

clkVoltControllerEval_exit:
    return status;
}

/*!
 * Reset the voltage controller
 *
 * @param[in] pVoltController   Voltage controller object pointer
 *
 * @return Status returned from functions called within.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If it's a unsupported voltage controller type.
 */
FLCN_STATUS
clkVoltControllerReset
(
    CLK_VOLT_CONTROLLER *pVoltController
)
{
    FLCN_STATUS     status        = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pVoltController))
    {
        case LW2080_CTRL_CLK_CLK_VOLT_CONTROLLER_TYPE_PROP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER_PROP))
            {
                status = clkVoltControllerReset_PROP(pVoltController);
            }
            break;
        }
        default:
        {
            // Nothing to do
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVoltControllerReset_exit;
    }

clkVoltControllerReset_exit:
    return status;
}

/*!
 * Disable/Enable Voltage Controllers on all rails
 *
 * @param[in] clientId       @ref LW2080_CTRL_CLK_CLK_VOLT_CONTROLLER_CLIENT_ID_<xyz>
 * @param[in] bDisable       @ref Disable if TRUE, Enable if FALSE
 *
 * @return Status returned from functions called within.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If it's a unsupported voltage controller type.
 */
FLCN_STATUS
clkVoltControllersDisable
(
    LwU8      clientId,
    LwBool    bDisable
)
{
    FLCN_STATUS status = LW_OK;
    LwU8        idx;

    for (idx = 0; idx < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS; idx++)
    {
        if (VOLT_RAIL_INDEX_IS_VALID(idx))
        {
            status = clkVoltControllerDisable(clientId, idx, bDisable);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVoltControllersDisable_exit;
            }
        }
    }

clkVoltControllersDisable_exit:
    return status;
}

/*!
 * Disable/Enable Voltage Controller
 *
 * @param[in] clientId       @ref LW2080_CTRL_CLK_CLK_VOLT_CONTROLLER_CLIENT_ID_<xyz>
 * @param[in] voltRailIdx    Index of voltage rail on which controllers to be disabled/enabled
 * @param[in] bDisable       TRUE if disable, FALSE if enable
 *
 * @return Status returned from functions called within.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If it's a unsupported voltage controller type.
 */
FLCN_STATUS
clkVoltControllerDisable
(
    LwU8      clientId,
    LwU8      voltRailIdx,
    LwBool    bDisable
)
{
    CLK_VOLT_CONTROLLER *pVoltController = NULL;
    LwBoardObjIdx        idx;

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_VOLT_CONTROLLER, pVoltController, idx, NULL)
    {
        if (pVoltController->voltRailIdx == voltRailIdx)
    {
    if (bDisable)
    {
        pVoltController->disableClientsMask |= LWBIT32(clientId);
    }
    else
    {
        pVoltController->disableClientsMask &= ~LWBIT32(clientId);
    }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return LW_OK;
}

/*!
 * @ref OsTimerOsCallback
 * OS_TMR callback for voltage controller.
 */
FLCN_STATUS
clkVoltControllerOsCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    CLK_VOLT_CONTROLLER *pVoltController;
    LwBoardObjIdx idx;
    FLCN_STATUS status = FLCN_OK;

    // Init voltage offset
    for (idx = 0; idx < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS; idx++)
    {
        Clk.voltControllers.sampleMaxVoltOffsetuV[idx] = LW_S32_MIN;
    }

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_VOLT_CONTROLLER, pVoltController, idx, NULL)
    {
        if (CLK_VOLT_CONTROLLER_IS_DISABLED(pVoltController))
        {
            // Skip this disabled controller
            continue;
        }

        //
        // Update the total max voltage offset with the actually applied value
        // by the voltage code.
        //
        clkVoltControllersTotalMaxVoltOffsetuVUpdate(pVoltController);

        // Evaluate controllers.
        PMU_ASSERT_OK_OR_GOTO(status,
            clkVoltControllerEval(pVoltController),
            clkVoltControllerOsCallback_exit);

        // Get max voltage offset per sample across all the voltage controllers.
        Clk.voltControllers.sampleMaxVoltOffsetuV[pVoltController->voltRailIdx] =
        LW_MAX(CLK_VOLT_CONTROLLER_VOLT_OFFSET_UV_GET(pVoltController),
            Clk.voltControllers.sampleMaxVoltOffsetuV[pVoltController->voltRailIdx]);
    }
    BOARDOBJGRP_ITERATOR_END;

clkVoltControllerOsCallback_exit:
    return status;
}

/*!
 * Evaluates the voltage controller based on the type.
 *
 * @param[in] pVoltController    Voltage controller object pointer
 *
 * @return Status returned from functions called within.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If it's a unsupported voltage controller type.
 */
FLCN_STATUS
clkVoltControllerEval_SUPER
(
    CLK_VOLT_CONTROLLER *pVoltController
)
{
    LwS32 totalMaxVoltOffsetuV;

    //
    // Adjust the per sample voltage offset with last applied final offset and trim the
    // new final offset with POR ranges.
    //
    totalMaxVoltOffsetuV = Clk.voltControllers.totalMaxVoltOffsetuV[pVoltController->voltRailIdx];
    if ((totalMaxVoltOffsetuV + pVoltController->voltOffsetuV) < pVoltController->voltOffsetMinuV)
    {
        pVoltController->voltOffsetuV = pVoltController->voltOffsetMinuV - totalMaxVoltOffsetuV;
    }
    else if ((pVoltController->voltOffsetuV + totalMaxVoltOffsetuV) > pVoltController->voltOffsetMaxuV)
    {
        pVoltController->voltOffsetuV = pVoltController->voltOffsetMaxuV - totalMaxVoltOffsetuV;
    }

    return FLCN_OK;
}


/*!
 * Check if the voltage controller sample is poisoned because of
 * GR-RG
 *
 * @param[in] pVoltController    Voltage controller object pointer
 *
 * @return LW_TRUE - The sample is poisoned.
 * @return LW_FALSE - Otherwise.
 */
FLCN_STATUS
clkVoltControllerSamplePoisonedGrRg_SUPER
(
    CLK_VOLT_CONTROLLER *pVoltController,
    LwBool              *pbPoison
)
{
    LwU8   voltRailIdxMsVdd = LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID;
    LwU32  grRgGatingCount = 0;
    LwBool bGrRgEnabled    = PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG) &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_GR_RG);

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
    };

    // query msvdd voltage rail index
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        voltRailIdxMsVdd = voltRailVoltDomainColwertToIdx(LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    if (voltRailIdxMsVdd == pVoltController->voltRailIdx)    {
        // MSVDD Rail is ALWAYS ON, no poisoning required.
        *pbPoison = LW_FALSE;
        goto clkVoltControllerSamplePoisonedGrRg_SUPER_exit;
    }

    if (bGrRgEnabled)
    {
        appTaskCriticalEnter();
        {
            grRgGatingCount = PG_STAT_ENTRY_COUNT_GET(RM_PMU_LPWR_CTRL_ID_GR_RG);
            if ((PG_IS_ENGAGED(RM_PMU_LPWR_CTRL_ID_GR_RG)) ||
                (pVoltController->grRgGatingCount != grRgGatingCount))
            {
                *pbPoison = LW_TRUE;
                pVoltController->grRgGatingCount = grRgGatingCount;
            }
        }
        appTaskCriticalExit();
    }

clkVoltControllerSamplePoisonedGrRg_SUPER_exit:
    return LW_OK;
}

/*!
 * Check if the voltage controller sample is poisoned because of
 * droopy VR engaged.
 *
 * The sample is poisoned if droopy was enagaged for more time than the
 * minimum limit @ref droopyPctMin.
 *
 * @param[in] pVoltController    Voltage controller object pointer
 *
 * @return LW_TRUE - The sample is poisoned.
 * @return LW_FALSE - Otherwise.
 */
FLCN_STATUS
clkVoltControllerSamplePoisonedDroopy_SUPER
(
    CLK_VOLT_CONTROLLER *pVoltController,
    LwBool              *pbPoison
)
{
    LwUFXP52_12     num_UFXP52_12;
    LwUFXP52_12     quotient_UFXP52_12;
    LwUFXP4_12      droopyPct;
    FLCN_TIMESTAMP  lwrrDroopyEvalTimeNs;
    FLCN_TIMESTAMP  elapsedDroopyEvalTimeNs;
    FLCN_TIMESTAMP  lwrrDroopyEngageTimeNs;
    FLCN_TIMESTAMP  elapsedDroopyEngageTimeNs;
    THRM_MON       *pThrmMon;
    FLCN_STATUS     status  = FLCN_OK;

    // Sanity check
    if (pbPoison == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVoltControllerSamplePoisonedDroopy_SUPER_exit;
    }

    // Init bPoison flag
    *pbPoison = LW_FALSE;

    // Return early if Thermal Monitor Index is invalid.
    if (LW2080_CTRL_THERMAL_THERM_MONITOR_IDX_ILWALID ==
        pVoltController->thermMonIdx)
    {
        status = FLCN_OK;
        goto clkVoltControllerSamplePoisonedDroopy_SUPER_exit;
    }

    // Get Thermal Monitor object corresponding to index.
    pThrmMon = THRM_MON_GET(pVoltController->thermMonIdx);
    if (pThrmMon == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkVoltControllerSamplePoisonedDroopy_SUPER_exit;
    }

    // Get current timer values.
    status = thrmMonTimer64Get(pThrmMon, &lwrrDroopyEvalTimeNs, &lwrrDroopyEngageTimeNs);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVoltControllerSamplePoisonedDroopy_SUPER_exit;
    }

    // Compute elapsed time values.
    lw64Sub(&elapsedDroopyEvalTimeNs.data, &lwrrDroopyEvalTimeNs.data,
            &pVoltController->prevDroopyEvalTimeNs.data);
    lw64Sub(&elapsedDroopyEngageTimeNs.data, &lwrrDroopyEngageTimeNs.data,
            &pVoltController->prevDroopyEngageTimeNs.data);

    // Validate the evaluation time.
    PMU_ASSERT_OK_OR_GOTO(status,
        (elapsedDroopyEvalTimeNs.data == LW_TYPES_FXP_ZERO) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        clkVoltControllerSamplePoisonedDroopy_SUPER_exit);
    //
    // Droopy engage time should not be higher than the evaluation time with some
    // margin to account for measurement error.
    //
    if (lwU64CmpGT(&elapsedDroopyEngageTimeNs.data, &elapsedDroopyEvalTimeNs.data))
    {
        FLCN_TIMESTAMP tempEvalTimeNs;

        // Add 1000 ns margin for measurement errors.
        tempEvalTimeNs.data = 1000U;

        lw64Add(&tempEvalTimeNs.data, &tempEvalTimeNs.data, &elapsedDroopyEvalTimeNs.data);

        if (lwU64CmpGT(&elapsedDroopyEngageTimeNs.data, &tempEvalTimeNs.data))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto clkVoltControllerSamplePoisonedDroopy_SUPER_exit;
        }

        // Override the engage time to eval time.
        elapsedDroopyEngageTimeNs.data = elapsedDroopyEvalTimeNs.data;
    }

    // Cache current time values for next iteration.
    pVoltController->prevDroopyEvalTimeNs   = lwrrDroopyEvalTimeNs;
    pVoltController->prevDroopyEngageTimeNs = lwrrDroopyEngageTimeNs;

    //
    // Compute percentage time(as ratio in LwUFXP4_12) droopy was engaged.
    // Note that imem overlays required for 64-bit math below are already
    // attached in clkVoltControllerOsCallback function and this function
    // is eventually called from there.
    //
    num_UFXP52_12 = LW_TYPES_U64_TO_UFXP_X_Y(52, 12, elapsedDroopyEngageTimeNs.data);
    lwU64DivRounded(&quotient_UFXP52_12, &num_UFXP52_12, &elapsedDroopyEvalTimeNs.data);
    droopyPct = LwU32_LO16(LwU64_LO32(quotient_UFXP52_12));

    // Poison the sample if droopy engaged more than the min limit.
    if (droopyPct > pVoltController->droopyPctMin)
    {
        *pbPoison = LW_TRUE;
    }

clkVoltControllerSamplePoisonedDroopy_SUPER_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * CLK_VOLT_CONTROLLER implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_clkVoltControllerIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_VOLT_CONTROLLER_BOARDOBJGRP_SET_HEADER *pSet = NULL;
    CLK_VOLT_CONTROLLERS *pVoltControllers = NULL;
    FLCN_STATUS           status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pBuf);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkVoltControllerIfaceModel10SetHeader_exit;
    }
    pSet = (RM_PMU_CLK_CLK_VOLT_CONTROLLER_BOARDOBJGRP_SET_HEADER *)pBuf;
    pVoltControllers = (CLK_VOLT_CONTROLLERS *)pBObjGrp;

    // Copy global CLK_VOLT_CONTROLLERS state.
    pVoltControllers->samplingPeriodms      = pSet->samplingPeriodms;
    pVoltControllers->lowSamplingMultiplier = pSet->lowSamplingMultiplier;
    pVoltControllers->voltOffsetThresholduV = pSet->voltOffsetThresholduV;
    pVoltControllers->disableClientsMask    = pSet->disableClientsMask;

s_clkVoltControllerIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * CLK_VOLT_CONTROLLER constructor entry point - constructs a CLK_VOLT_CONTROLLER
 * of the corresponding type.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clkVoltControllerIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_CLK_CLK_VOLT_CONTROLLER_TYPE_PROP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER_PROP))
            {
                status = clkVoltControllerGrpIfaceModel10ObjSet_PROP(pModel10, ppBoardObj,
                            sizeof(CLK_VOLT_CONTROLLER_PROP), pBuf);
            }
            break;
        }
        default:
        {
            // Nothing to do
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkVoltControllerIfaceModel10SetEntry_exit;
    }

s_clkVoltControllerIfaceModel10SetEntry_exit:
    return status;
}

/*!
 * @Copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
static FLCN_STATUS
s_clkVoltControllerIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10         *pModel10,
    RM_PMU_BOARDOBJGRP  *pBuf,
    BOARDOBJGRPMASK     *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_VOLT_CONTROLLER_BOARDOBJGRP_GET_STATUS_HEADER *pQuery;
    CLK_VOLT_CONTROLLERS *pVoltControllers = NULL;
    LwU8                  i;

    pQuery = (RM_PMU_CLK_CLK_VOLT_CONTROLLER_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    pVoltControllers = (CLK_VOLT_CONTROLLERS *)pBoardObjGrp;

    // Get the CLK_VOLT_CONTROLLER query parameters.
    for (i = 0; i < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS; i++)
    {
        pQuery->sampleMaxVoltOffsetuV[i] = pVoltControllers->sampleMaxVoltOffsetuV[i];
        pQuery->totalMaxVoltOffsetuV[i]  = pVoltControllers->totalMaxVoltOffsetuV[i];
    }
    return FLCN_OK;
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_clkVoltControllerIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_CLK_CLK_VOLT_CONTROLLER_TYPE_PROP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER_PROP))
            {
                status = clkVoltControllerEntryIfaceModel10GetStatus_PROP(pModel10, pBuf);
            }
            break;
        }
        default:
        {
            // Nothing to do
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkVoltControllerIfaceModel10GetStatus_exit;
    }

s_clkVoltControllerIfaceModel10GetStatus_exit:
    return status;
}
