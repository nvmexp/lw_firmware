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
 * @file clk_freq_controller.c
 *
 * @brief Module managing all state related to the CLK_FREQ_CONTROLLER structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "clk/clk_freq_controller.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader   (s_clkFreqControllerIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkFreqControllerIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry    (s_clkFreqControllerIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkFreqControllerIfaceModel10SetEntry");
static LwU8                         s_clkFreqControllerIdToPartIdx(LwU8 controllerId)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkFreqControllerIdToPartIdx");
BoardObjIfaceModel10GetStatus                       (s_clkFreqControllerIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "s_clkFreqControllerIfaceModel10GetStatus");
BoardObjGrpIfaceModel10GetStatusHeader          (s_clkFreqControllerIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "s_clkFreqControllerIfaceModel10GetStatusHeader");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * Initialize Clock Frequency Controllers.
 *
 * @return FLCN_OK
 *       Init Suceess.
 * @return FLCN_ERR_ILLEGAL_OPERATION
 *      If memory is already allocated for clk controllers.
 */
FLCN_STATUS
clkFreqControllersInit()
{
    FLCN_STATUS     status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V20))
    {
        status = clkFreqControllersInit_20();
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkFreqControllersInit_exit;
        }
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
    {
        status = clkFreqControllersInit_10();
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkFreqControllersInit_exit;
        }
    }
    else
    {
        // Do nothing
    }

clkFreqControllersInit_exit:
    return status;
}

/*!
 * Load frequency controllers.
 * Schedules the callback to evaluate the controllers.
 *
 * @param[in] bLoad   LW_TRUE if load ALL the controllers,
 *                    LW_FALSE otherwise
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkFreqControllersLoad
(
   RM_PMU_CLK_LOAD *pClkLoad
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V20))
    {
        status = clkFreqControllersLoad_20(pClkLoad);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
    {
        status = clkFreqControllersLoad_10(pClkLoad);
    }
    else
    {
        // Do nothing
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkFreqControllersLoad_exit;
    }

clkFreqControllersLoad_exit:
    return status;
}

/*!
 * CLK_FREQ_CONTROLLER handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkFreqControllerBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS          status;
    CLK_FREQ_CONTROLLER *pDev = NULL;
    LwBoardObjIdx        devIdx;
    OSTASK_OVL_DESC      ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkControllers)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,    // _grpType
            CLK_FREQ_CONTROLLER,                        // _class
            pBuffer,                                    // _pBuffer
            s_clkFreqControllerIfaceModel10SetHeader,         // _hdrFunc
            s_clkFreqControllerIfaceModel10SetEntry,          // _entryFunc
            pascalAndLater.clk.clkFreqControllerGrpSet, // _ssElement
            OVL_INDEX_DMEM(clkControllers),             // _ovlIdxDmem
            BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables

        BOARDOBJGRP_ITERATOR_BEGIN(CLK_FREQ_CONTROLLER, pDev, devIdx, NULL)
        {
            pDev->pNafllDev = clkNafllFreqCtrlByIdxGet(BOARDOBJ_GRP_IDX_TO_8BIT(devIdx));
        }
        BOARDOBJGRP_ITERATOR_END;
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Queries all boardobj devices
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkFreqControllerBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS          status;
    OSTASK_OVL_DESC      ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkControllers)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32,           // _grpType
            CLK_FREQ_CONTROLLER,                                // _class
            pBuffer,                                            // _pBuffer
            s_clkFreqControllerIfaceModel10GetStatusHeader,                 // _hdrFunc
            s_clkFreqControllerIfaceModel10GetStatus,                       // _entryFunc
            pascalAndLater.clk.clkFreqControllerGrpGetStatus);  // _ssElement
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkFreqControllerIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_CLK_CLK_FREQ_CONTROLLER_BOARDOBJ_GET_STATUS  *pGetStatus;
    CLK_FREQ_CONTROLLER                                 *pFreqController;
    FLCN_STATUS                                          status;

    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto clkFreqControllerIfaceModel10GetStatus_SUPER_exit;
    }
    pFreqController = (CLK_FREQ_CONTROLLER *)pBoardObj;
    pGetStatus = (RM_PMU_CLK_CLK_FREQ_CONTROLLER_BOARDOBJ_GET_STATUS *)pPayload;

    // Get the CLK_FREQ_CONTROLLER query parameters if any
    pGetStatus->disableClientsMask = pFreqController->disableClientsMask;

clkFreqControllerIfaceModel10GetStatus_SUPER_exit:
    return status;
}

/*!
 * CLK_FREQ_CONTROLLER super-class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkFreqControllerGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLK_FREQ_CONTROLLER_10_BOARDOBJ_SET *pSet = NULL;
    CLK_FREQ_CONTROLLER *pFreqController = NULL;
    FLCN_STATUS          status;

    //
    // Call into BOARDOBJ super-constructor, which will do actual memory
    // allocation.
    //
    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkFreqControllerGrpIfaceModel10ObjSet_SUPER_exit;
    }
    pSet = (RM_PMU_CLK_CLK_FREQ_CONTROLLER_10_BOARDOBJ_SET *)pBoardObjDesc;
    pFreqController = (CLK_FREQ_CONTROLLER *)*ppBoardObj;

    // Get clock domain partition index.
    pFreqController->partIdx =
        s_clkFreqControllerIdToPartIdx(pSet->controllerId);

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        // Check boundary conditions on these values
        if ((pSet->freqCapNoiseUnawareVminBelow > pSet->freqCapNoiseUnawareVminAbove) ||
            (pSet->freqHystPosMHz < 0) ||
            (pSet->freqHystNegMHz > 0))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkFreqControllerGrpIfaceModel10ObjSet_SUPER_exit;
        }
    }

    // Copy the CLK_FREQ_CONTROLLER-specific data.
    pFreqController->controllerId   = pSet->controllerId;
    pFreqController->clkDomain      = pSet->clkDomain;
    pFreqController->partsFreqMode  = pSet->partsFreqMode;
    pFreqController->bDisable       = pSet->bDisable;
    pFreqController->freqCapNoiseUnawareVminAbove =
        pSet->freqCapNoiseUnawareVminAbove;
    pFreqController->freqCapNoiseUnawareVminBelow =
        pSet->freqCapNoiseUnawareVminBelow;
    pFreqController->freqHystPosMHz = pSet->freqHystPosMHz;
    pFreqController->freqHystNegMHz = pSet->freqHystNegMHz;
    pFreqController->pNafllDev      = NULL;

clkFreqControllerGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * Evaluates the frequency controller based on the type.
 *
 * @param[in] pFreqController   Frequency controller object pointer
 *
 * @return Status returned from functions called within.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If it's a unsupported frequency controller type.
 */
FLCN_STATUS
clkFreqControllerEval
(
    CLK_FREQ_CONTROLLER *pFreqController
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pFreqController))
    {
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_PI: // To be deprecated, falls through
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_10_PI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
            {
                status = clkFreqControllerEval_PI_10(
                    (CLK_FREQ_CONTROLLER_10_PI *)pFreqController);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_20_PI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V20))
            {
                status = clkFreqControllerEval_PI_20(
                    (CLK_FREQ_CONTROLLER_20_PI *)pFreqController);
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
        goto clkFreqControllerEval_exit;
    }

clkFreqControllerEval_exit:
    return status;
}

/*!
 * Reset the frequency controller
 *
 * @param[in] pFreqController   Frequency controller object pointer
 *
 * @return Status returned from functions called within.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If it's a unsupported frequency controller type.
 */
FLCN_STATUS
clkFreqControllerReset
(
    CLK_FREQ_CONTROLLER *pFreqController
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pFreqController))
    {
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_PI: // To be deprecated, falls through
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_10_PI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
            {
                status = clkFreqControllerReset_PI_10(
                    (CLK_FREQ_CONTROLLER_10_PI *)pFreqController);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_20_PI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V20))
            {
                status = clkFreqControllerReset_PI_20(
                    (CLK_FREQ_CONTROLLER_20_PI *)pFreqController);
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
        goto clkFreqControllerReset_exit;
    }

clkFreqControllerReset_exit:
    return status;
}

/*!
 * Disable/Enable the frequency controller for a given clock domain
 *
 * @param[in]   ctrlId
 *      Index of the frequency controller
 * @param[in]   clientId
 *      Client ID @ref LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_CLIENT_ID_<xyz>.
 * @param[in]   bDisable
 *      Whether to disable the controller. True-Disable, False-Enable.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *      Unsupported clock domain passed.
 * @return Other
 *      Status returned from the functions called within.
 * @return FLCN_OK
 *      Successfully disabled/enabled the controller.
 */
FLCN_STATUS
clkFreqControllerDisable
(
    LwU32  ctrlId,
    LwU8   clientId,
    LwBool bDisable
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V20))
    {
        status = clkFreqControllerDisable_20(ctrlId, clientId, bDisable);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
    {
        status = clkFreqControllerDisable_10(ctrlId, clientId, bDisable);
    }
    else
    {
        // Do nothing
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkFreqControllerDisable_exit;
    }

clkFreqControllerDisable_exit:
    return status;
}

/*!
 * Get the voltage delta for a frequency controller based on the type.
 *
 * @param[in] pFreqController   Frequency controller object pointer
 *
 * @return Voltage delta in uV.
 * @return 0
 *      If it's a unsupported frequency controller type.
 */
LwS32
clkFreqControllerVoltDeltauVGet
(
    CLK_FREQ_CONTROLLER *pFreqController
)
{
    LwS32 voltDeltauV = RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED; 

    switch (BOARDOBJ_GET_TYPE(pFreqController))
    {
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_PI: // To be deprecated, falls through
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_10_PI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
            {
                voltDeltauV = clkFreqControllerVoltDeltauVGet_PI_10(
                            (CLK_FREQ_CONTROLLER_10_PI *)pFreqController);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_20_PI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V20))
            {
                voltDeltauV = clkFreqControllerVoltDeltauVGet_PI_20(
                            (CLK_FREQ_CONTROLLER_20_PI *)pFreqController);
            }
            break;
        }
        default:
        {
            // Nothing to do
            break;
        }
    }

    return voltDeltauV;
}

/*!
 * Set the voltage offset for a frequency controller based on the type.
 *
 * @param[in] pFreqController   Frequency controller object pointer
 * @param[in] voltOffsetuV      Voltage offset in uV.
 * @return 0
 *      If it's a unsupported frequency controller type.
 */
FLCN_STATUS
clkFreqControllerVoltDeltauVSet
(
    CLK_FREQ_CONTROLLER *pFreqController,
    LwS32                voltOffsetuV
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pFreqController))
    {
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_PI: // To be deprecated, falls through
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_10_PI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
            {
                status = clkFreqControllerVoltDeltauVSet_PI_10(
                    (CLK_FREQ_CONTROLLER_10_PI *)pFreqController, voltOffsetuV);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_20_PI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V20))
            {
                status = clkFreqControllerVoltDeltauVSet_PI_20(
                    (CLK_FREQ_CONTROLLER_20_PI *)pFreqController, voltOffsetuV);
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
        goto clkFreqControllerVoltDeltauVSet_exit;
    }

clkFreqControllerVoltDeltauVSet_exit:
    return 0;
}

/*!
 * Get the final voltage delta which is the max value from all the
 * frequency controllers.
 *
 * @return Final voltage delta in uV.
 * @return 0
 *      If all the controllers are disabled or there is no noise in
 *      the system.
 */
LwS32
clkFreqControllersFinalVoltDeltauVGet(void)
{
    return (CLK_CLK_FREQ_CONTROLLERS_GET())->finalVoltDeltauV;
}

/*!
 * @ref OsTimerOsCallback
 * OS_TMR callback for frequency controller.
 */
FLCN_STATUS
clkFreqControllerOsCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkControllers)
    };

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V20))
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = clkFreqControllerOsCallback_20(pCallback);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = clkFreqControllerOsCallback_10(pCallback);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }
    else
    {
        // do nothing
        lwosNOP();
    }

    return status;
}

/*!
 * @ref OsTimerCallback
 * OS_TIMER callback for frequency controller.
 */
void
clkFreqControllerCallback
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    clkFreqControllerOsCallback(NULL);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * CLK_FREQ_CONTROLLER implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_clkFreqControllerIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_FREQ_CONTROLLER_10_BOARDOBJGRP_SET_HEADER *pSet = NULL;
    CLK_FREQ_CONTROLLERS *pFreqControllers = NULL;
    FLCN_STATUS           status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkFreqControllerIfaceModel10SetHeader_exit;
    }
    pSet = (RM_PMU_CLK_CLK_FREQ_CONTROLLER_10_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    pFreqControllers = (CLK_FREQ_CONTROLLERS *)pBObjGrp;

    // Copy global CLK_FREQ_CONTROLLERS state.
    pFreqControllers->samplingPeriodms = pSet->samplingPeriodms;
    pFreqControllers->voltPolicyIdx    = pSet->voltPolicyIdx;
    pFreqControllers->bContinuousMode  = pSet->bContinuousMode;


    // Sanity check the feature enablement to ensure RM - PMU are in sync.
    PMU_HALT_COND(((clkFreqControllersVersionGet() == LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_VERSION_20) &&
                    (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V20))) ||
               ((clkFreqControllersVersionGet() == LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_VERSION_10) &&
                    (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))));

s_clkFreqControllerIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * CLK_FREQ_CONTROLLER constructor entry point - constructs a CLK_FREQ_CONTROLLER
 * of the corresponding type.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clkFreqControllerIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_PI: // To be deprecated, falls through
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_10_PI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
            {
                status = clkFreqControllerGrpIfaceModel10ObjSet_PI_10(pModel10, ppBoardObj,
                    sizeof(CLK_FREQ_CONTROLLER_10_PI), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_20_PI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V20))
            {
                status = clkFreqControllerGrpIfaceModel10ObjSet_PI_20(pModel10, ppBoardObj,
                    sizeof(CLK_FREQ_CONTROLLER_20_PI), pBuf);
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
        goto s_clkFreqControllerIfaceModel10SetEntry_exit;
    }

s_clkFreqControllerIfaceModel10SetEntry_exit:
    return status;
}

/*!
 * @Copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
FLCN_STATUS
s_clkFreqControllerIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10         *pModel10,
    RM_PMU_BOARDOBJGRP  *pBuf,
    BOARDOBJGRPMASK     *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_FREQ_CONTROLLER_BOARDOBJGRP_GET_STATUS_HEADER *pHdr;
    CLK_FREQ_CONTROLLERS                                *pFreqControllers;

    pFreqControllers = (CLK_FREQ_CONTROLLERS *)pBoardObjGrp;
    pHdr = (RM_PMU_CLK_CLK_FREQ_CONTROLLER_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;

    // Get the CLK_FREQ_CONTROLLER query parameters.
    pHdr->finalVoltDeltauV = pFreqControllers->finalVoltDeltauV;

    return FLCN_OK;
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
s_clkFreqControllerIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10           *pModel10,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_PI: // To be deprecated, falls through
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_10_PI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
            {
                status = clkFreqControllerEntryIfaceModel10GetStatus_PI_10(pModel10, pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_20_PI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V20))
            {
                status = clkFreqControllerEntryIfaceModel10GetStatus_PI_20(pModel10, pBuf);
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
        goto s_clkFreqControllerIfaceModel10GetStatus_exit;
    }

s_clkFreqControllerIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * Get partition index based on the controller ID.
 *
 * @param[in] controllerId
 *      Frequency controller ID @ref LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_<xyz>
 *
 * @return Partition index @ref LW2080_CTRL_CLK_DOMAIN_PART_IDX_<xyz>.
 *
 */
static LwU8
s_clkFreqControllerIdToPartIdx
(
    LwU8 controllerId
)
{
    LwU8 partIdx;

    switch (controllerId)
    {
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_SYS:
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_LTC:
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_XBAR:
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_LWD:
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_HOST:
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_GPCS:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_GPC0:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_0;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_GPC1:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_1;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_GPC2:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_2;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_GPC3:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_3;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_GPC4:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_4;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_GPC5:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_5;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_GPC6:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_6;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_GPC7:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_7;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_GPC8:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_8;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_GPC9:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_9;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_GPC10:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_10;
            break;
        }
        case LW2080_CTRL_CLK_CLK_FREQ_CONTROLLER_ID_GPC11:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_11;
            break;
        }
        default:
        {
            partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED;
            PMU_BREAKPOINT();
            break;
        }
    }

    return partIdx;
}

