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
 * @file  voltrail.c
 * @brief Volt Rail Model
 *
 * This module is a collection of functions managing and manipulating state
 * related to volt rails.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"
#include "volt/voltrail.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "main.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "volt/voltrail_pmumon.h"

#include "config/g_volt_hal.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjIfaceModel10GetStatus                (s_voltRailIfaceModel10GetStatus);
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET))
static FLCN_STATUS s_maxPositiveOffsetGet(VOLT_RAIL  *pRail, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                        *pRailListItem, VOLT_RAIL_OFFSET_RANGE_ITEM *pRailOffsetItem, LwBool bSkipVoltRangeTrim);
static FLCN_STATUS s_maxNegativeOffsetGet(VOLT_RAIL  *pRail, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                        *pRailListItem, VOLT_RAIL_OFFSET_RANGE_ITEM *pRailOffsetItem, LwBool bSkipVoltRangeTrim);
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET)
static FLCN_STATUS s_voltRailSanityCheckState_VF_SWITCH(VOLT_RAIL *pRail, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pListItem);
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
static FLCN_STATUS s_voltRailSanityCheckState(VOLT_RAIL *pRail, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pListItem);
static FLCN_STATUS s_voltRailSanityCheckState_GATE(VOLT_RAIL *pRail, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pListItem);
static FLCN_STATUS s_voltRailSanityCheckState_UNGATE(VOLT_RAIL *pRail, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pListItem);
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_CONTROLLER))
static FLCN_STATUS s_voltRailsClkControllersIlwalidate(void);
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_CONTROLLER))
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_VOLT_MARGIN_OFFSET))
static FLCN_STATUS s_voltRailsQueueVoltOffset(void);
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_CONTROLLER))

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief VOLT_RAIL handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
voltRailBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status     = FLCN_OK;
    LwBool      bFirstTime = ((VOLT_RAILS_GET())->super.super.objSlots == 0U);

    status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,    // _grpType
        VOLT_RAIL,                                  // _class
        pBuffer,                                    // _pBuffer
        voltVoltRailIfaceModel10SetHeader,                // _hdrFunc
        voltVoltRailIfaceModel10SetEntry,                 // _entryFunc
        pascalAndLater.volt.voltRailGrpSet,         // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltRailBoardObjGrpIfaceModel10Set_exit;
    }

    if (!bFirstTime)
    {
        // No VFE re-evaluation required on SET control
        status = voltRailsDynamilwpdate(LW_FALSE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailBoardObjGrpIfaceModel10Set_exit;
        }
    }

voltRailBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader()
 */
FLCN_STATUS
voltVoltRailIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    RM_PMU_VOLT_VOLT_RAIL_BOARDOBJGRP_SET_HEADER  *pSetHdr;
    FLCN_STATUS                                    status;

    // Call to Board Object Group constructor must always be first!
    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        goto voltVoltRailIfaceModel10SetHeader_exit;
    }

    Volt.railMetadata.bFirstTime = BOARDOBJGRP_IS_EMPTY(VOLT_RAIL);
    pSetHdr = (RM_PMU_VOLT_VOLT_RAIL_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    VOLT_RAILS_GET()->voltDomainHAL = pSetHdr->voltDomainHAL;

voltVoltRailIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry()
 */
FLCN_STATUS
voltVoltRailIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    RM_PMU_VOLT_VOLT_RAIL_BOARDOBJ_SET *pSet         =
        (RM_PMU_VOLT_VOLT_RAIL_BOARDOBJ_SET *)pBuf;
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    LwBool                              bConstructed =
        (*ppBoardObj != NULL);
#endif
    VOLT_RAIL                          *pRail;
    FLCN_STATUS                         status;
    LwU8                                i;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, sizeof(VOLT_RAIL), pBuf);
    if (status != FLCN_OK)
    {
        goto voltRailConstruct_exit;
    }
    pRail = (VOLT_RAIL *)*ppBoardObj;

    // Set member variables.
    VFE_EQU_IDX_COPY_RM_TO_PMU(pRail->relLimitVfeEquIdx,
                               pSet->relLimitVfeEquIdx);
    VFE_EQU_IDX_COPY_RM_TO_PMU(pRail->altRelLimitVfeEquIdx,
                               pSet->altRelLimitVfeEquIdx);
    VFE_EQU_IDX_COPY_RM_TO_PMU(pRail->ovLimitVfeEquIdx,
                               pSet->ovLimitVfeEquIdx);
    VFE_EQU_IDX_COPY_RM_TO_PMU(pRail->vminLimitVfeEquIdx,
                               pSet->vminLimitVfeEquIdx);
    VFE_EQU_IDX_COPY_RM_TO_PMU(pRail->voltMarginLimitVfeEquIdx,
                               pSet->voltMarginLimitVfeEquIdx);
    pRail->leakagePwrEquIdx             = pSet->leakagePwrEquIdx;
    pRail->voltDevIdxDefault            = pSet->voltDevIdxDefault;
    pRail->dynamicPwrEquIdx             = pSet->dynamicPwrEquIdx;
    pRail->voltDevIdxIPCVmin            = pSet->voltDevIdxIPCVmin;
    pRail->vbiosBootVoltageuV           = pSet->vbiosBootVoltageuV;

#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_RAM_ASSIST)
    if (Volt.railMetadata.bFirstTime)
    {
        pRail->ramAssist.bEnabled = LW_FALSE;
    }
    pRail->ramAssist.type               = pSet->ramAssist.type;
    VFE_EQU_IDX_COPY_RM_TO_PMU(pRail->ramAssist.vCritLowVfeEquIdx,
                               pSet->ramAssist.vCritLowVfeEquIdx);
    VFE_EQU_IDX_COPY_RM_TO_PMU(pRail->ramAssist.vCritHighVfeEquIdx,
                               pSet->ramAssist.vCritHighVfeEquIdx);
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_SCALING_PWR_EQUATION))
    pRail->baScalingPwrEqnIdx       = pSet->baScalingPwrEqnIdx;
#endif

    for (i = 0; i < LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_MAX_ENTRIES; i++)
    {
        pRail->voltDeltauV[i] = pSet->voltDeltauV[i];
    }

#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLTAGE_SENSED)
    boardObjGrpMaskInit_E32(&(pRail->adcDevMask));
    status = boardObjGrpMaskImport_E32(&(pRail->adcDevMask),
                                       &(pSet->adcDevMask));
    if (status != FLCN_OK)
    {
        goto voltRailConstruct_exit;
    }
#endif

    boardObjGrpMaskInit_E32(&(pRail->voltDevMask));
    status = boardObjGrpMaskImport_E32(&(pRail->voltDevMask),
                                       &(pSet->voltDevMask));
    if (status != FLCN_OK)
    {
        goto voltRailConstruct_exit;
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_CLK_DOMAINS_PROG_MASK))
    boardObjGrpMaskInit_E32(&(pRail->clkDomainsProgMask));
    status = boardObjGrpMaskImport_E32(&(pRail->clkDomainsProgMask),
                                       &(pSet->clkDomainsProgMask));
    if (status != FLCN_OK)
    {
        goto voltRailConstruct_exit;
    }
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    if (!bConstructed)
    {
        // Set the rail action parameter to VF_SWITCH.
        pRail->railAction = LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH;
    }
#endif

voltRailConstruct_exit:
    return status;
}

/*!
 * @brief Load all VOLT_RAILs.
 */
FLCN_STATUS
voltRailsLoad(void)
{
    FLCN_STATUS   status = FLCN_OK;
    VOLT_RAIL    *pRail;
    LwBoardObjIdx i;

    if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    {
        VOLT_DOMAINS_LIST voltDomainSanityList;
        LwU8              i;

        // Get the list of voltage domains to sanitize.
        status = voltRailGetVoltDomainSanityList(Volt.railMetadata.voltDomainHAL, &voltDomainSanityList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailsLoad_exit;
        }

        //
        // Sanity check each default voltage device corresponding to the voltage
        // that may be affected due to rail gating.
        //
        for (i = 0; i < voltDomainSanityList.numDomains; i++)
        {
            VOLT_RAIL   *pRail = VOLT_RAIL_GET(voltRailVoltDomainColwertToIdx(
                                    voltDomainSanityList.voltDomains[i]));
            VOLT_DEVICE *pDevice;

            if (pRail == NULL)
            {
                status = FLCN_ERR_ILWALID_INDEX;
                PMU_BREAKPOINT();
                goto voltRailsLoad_exit;
            }

            pDevice = VOLT_DEVICE_GET(pRail->voltDevIdxDefault);
            if (pDevice == NULL)
            {
                status = FLCN_ERR_ILWALID_INDEX;
                PMU_BREAKPOINT();
                goto voltRailsLoad_exit;
            }

            status = voltDeviceLoadSanityCheck(pDevice);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailsLoad_exit;
            }
        }
    }

    BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, i, NULL)
    {
        VOLT_DEVICE *pDevice = VOLT_DEVICE_GET(pRail->voltDevIdxDefault);

        if (pDevice == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto voltRailsLoad_exit;
        }

        // Obtain voltage of the default VOLT_DEVICE.
        status = voltDeviceGetVoltage(pDevice, &pRail->lwrrVoltDefaultuV);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailsLoad_exit;
        }

        // Round the voltage to the next supported value.
        status = voltDeviceRoundVoltage(pDevice,
                    (LwS32*)&pRail->lwrrVoltDefaultuV, LW_TRUE, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailsLoad_exit;
        }

        if (pRail->vbiosBootVoltageuV == RM_PMU_VOLT_VALUE_0V_IN_UV)
        {
            pRail->vbiosBootVoltageuV = pRail->lwrrVoltDefaultuV;
        }

        if ((PMUCFG_FEATURE_ENABLED(PMU_VOLT_IPC_VMIN)) &&
            (pRail->voltDevIdxIPCVmin != LW2080_CTRL_VOLT_VOLT_DEVICE_INDEX_ILWALID))
        {
            pDevice = VOLT_DEVICE_GET(pRail->voltDevIdxIPCVmin);
            if (pDevice == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto voltRailsLoad_exit;
            }

            status = voltDeviceConfigure(pDevice, LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailsLoad_exit;
            }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAILS_PMUMON))
    {
        status = voltVoltRailsPmumonLoad();
        if (status != FLCN_OK)
        {
            goto voltRailsLoad_exit;
        }
    }

voltRailsLoad_exit:
    return status;
}

/*!
 * @brief Queries all VOLT_RAILS.
 */
FLCN_STATUS
voltRailBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32,     // _grpType
        VOLT_RAIL,                                  // _class
        pBuffer,                                    // _pBuffer
        NULL,                                       // _hdrFunc
        s_voltRailIfaceModel10GetStatus,                            // _entryFunc
        pascalAndLater.volt.voltRailGrpGetStatus);  // _ssElement
}

/*!
 * @brief Update VOLT_RAIL dynamic parameters that depend on VFE.
 */
FLCN_STATUS
voltRailsDynamilwpdate_IMPL
(
    LwBool bVfeEvaluate
)
{
    VOLT_RAIL    *pRail;
    LwBoardObjIdx i;
    FLCN_STATUS   status = FLCN_OK;

    BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, i, NULL)
    {
        status = voltRailDynamilwpdate(pRail, bVfeEvaluate);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailsDynamilwpdate_IMPL_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_VOLT_MARGIN_OFFSET))
    status = s_voltRailsQueueVoltOffset();
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltRailsDynamilwpdate_IMPL_exit;
    }
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_CONTROLLER))
    status = s_voltRailsClkControllersIlwalidate();
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltRailsDynamilwpdate_IMPL_exit;
    }
#endif

voltRailsDynamilwpdate_IMPL_exit:
    return status;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_30))
/*!
 * @brief Obtains current voltage of the requested VOLT_RAIL.
 *
 * @param[in]  railIdx
 *      Voltage Rail Table Index.
 * @param[out] pVoltageuV
 *      Pointer that will hold the current voltage in uV.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_ERR_ILWALID_STATE
 *      PMU VOLT infrastructure isn't loaded.
 * @return  FLCN_OK
 *      Requested current voltage was obtained successfully.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
voltRailGetVoltage_RPC
(
    LwU8    railIdx,
    LwU32  *pVoltageuV
)
{
    VOLT_RAIL  *pRail;
    FLCN_STATUS status;

    // Sanity checks.
    if (pVoltageuV == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto voltRailGetVoltage_RPC_exit;
    }

    if (!PMU_VOLT_IS_LOADED())
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto voltRailGetVoltage_RPC_exit;
    }

    // Obtain VOLT_RAIL pointer from the Voltage Rail Table Index.
    pRail = VOLT_RAIL_GET(railIdx);
    if (pRail == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto voltRailGetVoltage_RPC_exit;
    }

    // Call VOLT_RAILs GET_VOLTAGE interface.
    status = voltRailGetVoltage(pRail, pVoltageuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltRailGetVoltage_RPC_exit;
    }

voltRailGetVoltage_RPC_exit:
    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_30)

/*!
 * @brief Sets the noise unaware Vmin voltage value received from RM.
 *
 * @param[in]  pRailList
 *      Pointer that holds the list containing noise unaware Vmin voltage
 *      for the VOLT_RAILs.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_ERR_ILWALID_STATE
 *      PMU VOLT infrastructure isn't loaded.
 * @return  FLCN_OK
 *      Requested noise unaware voltage was set successfully.
 */
FLCN_STATUS
voltRailSetNoiseUnawareVmin_IMPL
(
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST    *pRailList
)
{
    FLCN_STATUS  status = FLCN_OK;
    LwU8         i;
    VOLT_RAIL   *pRail;
    VOLT_DEVICE *pDevice;
    LwU32        tmpVoltageuV;

    // Sanity checks.
    if (pRailList == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto voltRailSetNoiseUnawareVmin_IMPL_exit;
    }
    if (!PMU_VOLT_IS_LOADED())
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto voltRailSetNoiseUnawareVmin_IMPL_exit;
    }

    for (i = 0; i < pRailList->numRails; i++)
    {
        pRail = VOLT_RAIL_GET(pRailList->rails[i].railIdx);
        if (pRail == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto voltRailSetNoiseUnawareVmin_IMPL_exit;
        }

        // Rounding may change IPC Vmin voltage so use temporary variable.
        tmpVoltageuV = pRailList->rails[i].voltageMinNoiseUnawareuV;

        if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_IPC_VMIN))
        {
            // Set IPC Vmin if voltDevIdxIPCVmin is valid.
            if (pRail->voltDevIdxIPCVmin != LW2080_CTRL_VOLT_VOLT_DEVICE_INDEX_ILWALID)
            {
                pDevice = VOLT_DEVICE_GET(pRail->voltDevIdxIPCVmin);
                if (pDevice == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto voltRailSetNoiseUnawareVmin_IMPL_exit;
                }

                status = voltDeviceRoundVoltage(pDevice, (LwS32*)&tmpVoltageuV,
                            LW_TRUE, LW_TRUE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto voltRailSetNoiseUnawareVmin_IMPL_exit;
                }

                // Actually set the voltage on the particular VOLT_DEVICE.
                status = voltDeviceSetVoltage(pDevice, tmpVoltageuV, LW_FALSE,
                            LW_FALSE, LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto voltRailSetNoiseUnawareVmin_IMPL_exit;
                }
            }
        }
        pRail->voltMinNoiseUnawareuV = tmpVoltageuV;
    }

voltRailSetNoiseUnawareVmin_IMPL_exit:
    return status;
}

/*!
 * @copydoc VoltRailGetVoltage()
 */
FLCN_STATUS
voltRailGetVoltage_IMPL
(
    VOLT_RAIL  *pRail,
    LwU32      *pVoltageuV
)
{
    FLCN_STATUS status = FLCN_OK;

    // Basic sanity checks.
    if ((pRail == NULL) ||
        (pVoltageuV == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto voltRailGetVoltage_IMPL_exit;
    }

    // Obtain voltage of the default VOLT_DEVICE via internal wrapper function.
    voltReadSemaphoreTake();
    *pVoltageuV = pRail->lwrrVoltDefaultuV;
    voltReadSemaphoreGive();

voltRailGetVoltage_IMPL_exit:
    return status;
}

/*!
 * @copydoc VoltRailSetVoltage()
 */
FLCN_STATUS
voltRailSetVoltage_IMPL
(
    VOLT_RAIL  *pRail,
    LwU32       voltageuV,
    LwBool      bTrigger,
    LwBool      bWait,
    LwU8        railAction
)
{
    VOLT_DEVICE    *pDevice;
    LwU32           tmpVoltageuV;
    LwBoardObjIdx   voltDevIdx;
    FLCN_STATUS     status = FLCN_ERR_ILWALID_STATE;

    // Set target voltage on all VOLT_DEVICEs for the VOLT_RAIL.
    BOARDOBJGRP_ITERATOR_BEGIN(VOLT_DEVICE, pDevice, voltDevIdx,
                                &pRail->voltDevMask.super)
    {
        // Rounding may change target voltage so use temporary variable.
        tmpVoltageuV = voltageuV;

        // Round the voltage to the next supported value.
        if (railAction != LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_GATE)
        {
            status = voltDeviceRoundVoltage(pDevice, (LwS32*)&tmpVoltageuV,
                        LW_TRUE, LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailSetVoltage_IMPL_exit;
            }
        }
        // Actually set the voltage on the particular VOLT_DEVICE.
        status = voltDeviceSetVoltage(pDevice, tmpVoltageuV, bTrigger, bWait, railAction);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailSetVoltage_IMPL_exit;
        }

        // Cache voltage of the default VOLT_DEVICE.
        if (BOARDOBJ_GRP_IDX_TO_8BIT(voltDevIdx) == pRail->voltDevIdxDefault)
        {
            pRail->lwrrVoltDefaultuV = tmpVoltageuV;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    pRail->railAction = railAction;
#endif

voltRailSetVoltage_IMPL_exit:
    return status;
}

/*!
 * @copydoc VoltRailRoundVoltage()
 */
FLCN_STATUS
voltRailRoundVoltage_IMPL
(
    VOLT_RAIL  *pRail,
    LwS32      *pVoltageuV,
    LwBool      bRoundUp,
    LwBool      bBound
)
{
    VOLT_DEVICE *pDevice;
    FLCN_STATUS  status;

    pDevice = VOLT_DEVICE_GET(pRail->voltDevIdxDefault);
    if (pDevice == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto voltRailRoundVoltage_exit;
    }

    status = voltDeviceRoundVoltage(pDevice, pVoltageuV, bRoundUp, bBound);

voltRailRoundVoltage_exit:
    return status;
}
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_RAM_ASSIST))
/*!
 * @brief Programs critical threshold voltages for enabling/disabling 
 * ram assist cirlwitry
 * @param[in]  pRail
 *      Pointer that holds the VOLT RAIL OBJECT.
 *
 * @return  FLCN_OK
 *      RAM Assist Critical threshold voltages programmed successfully.
 */
FLCN_STATUS
s_voltRailProgramVcritThresholds
(
    VOLT_RAIL  *pRail
)
{
    FLCN_STATUS status = FLCN_OK;

    status = clkAdcsProgramVcritThresholds(&(pRail->adcDevMask),
                    pRail->ramAssist.vCritLowuV, pRail->ramAssist.vCritHighuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief Enables RAM Assist control
 *
 * @param[in]  pRail
 *      Pointer that holds the VOLT RAIL OBJECT.
 *
 * @return  FLCN_OK
 *      RAM Assist Ctrl enabled successfully.
 */
FLCN_STATUS
s_voltRailRamAssistCtrlEnable
(
    VOLT_RAIL  *pRail
)
{
    FLCN_STATUS status   = FLCN_OK;

    if (!(pRail->ramAssist.bEnabled) &&
        (pRail->ramAssist.type != LW2080_CTRL_VOLT_VOLT_RAIL_RAM_ASSIST_TYPE_DISABLED) &&
        (pRail->ramAssist.type != LW2080_CTRL_VOLT_VOLT_RAIL_RAM_ASSIST_TYPE_STATIC))
    {
        status = clkAdcsRamAssistCtrlEnable(&pRail->adcDevMask, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRamAssistCtrlEnable_exit;
        }
        pRail->ramAssist.bEnabled = LW_TRUE;
    }

voltRamAssistCtrlEnable_exit:
    return status;
}

/*!
 * @brief Sanity Check RAM assist critical Voltages.
 *
 * @param[in]  critHighuV
 *      Critical High threshold Voltage for disabling Ram Assist.
 * @param[in]  critLowuV
 *      Critical Low threshold Voltage for enabling Ram Assist.
 *
 * @return  FLCN_OK
 *      Ram Assist Critical voltages were sanitized successfully.
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 */
FLCN_STATUS
s_voltRailRamAssistSanityCheck
(
    LwU32       critHighuV,
    LwU32       critLowuV
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((critHighuV == RM_PMU_VOLT_VALUE_0V_IN_UV)   ||
        (critLowuV == RM_PMU_VOLT_VALUE_0V_IN_UV))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto voltRailRamAssistSanityCheck_exit;
    }

    if (critHighuV <= critLowuV)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto voltRailRamAssistSanityCheck_exit;
    }

voltRailRamAssistSanityCheck_exit:
    return status;
}

/*!
 * @brief Computes critical threshold voltages for RAM ASSIST.
 *
 * @param[in]  pRail
 *      Pointer that holds the VOLT RAIL OBJECT.
 * @param[in]  bVfeEvaluate
 *      Boolean indicating whether VFE EQN has to be evaluated or use prev result.
 *
 * @return  FLCN_OK
 *      RAM Assist critical voltages updated and programmed in HW.
 */
FLCN_STATUS
s_voltRailRamAssistDynamilwpdate
(
    VOLT_RAIL  *pRail,
    LwBool      bVfeEvaluate
)
{
    LwU32       critHighuV          = RM_PMU_VOLT_VALUE_0V_IN_UV;
    LwU32       critLowuV           = RM_PMU_VOLT_VALUE_0V_IN_UV;
    FLCN_STATUS status              = FLCN_OK;
    LwS32       tmpVoltageuV        = RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED;
    RM_PMU_PERF_VFE_EQU_RESULT
                result;

    if (pRail->ramAssist.type == LW2080_CTRL_VOLT_VOLT_RAIL_RAM_ASSIST_TYPE_DYNAMIC_WITHOUT_LUT)
    {
        // Evaluate Vcrit thresholds for engaging and disengaging ram assist cirlwitry 
        if (pRail->ramAssist.vCritHighVfeEquIdx !=
                PMU_PERF_VFE_EQU_INDEX_ILWALID)
        {
            if (bVfeEvaluate)
            {
                status = vfeEquEvaluate(pRail->ramAssist.vCritHighVfeEquIdx,
                                        NULL, 0,
                                        LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_UV,
                                        &result);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto voltRailRamAssistDynamilwpdate_exit;
                }
                pRail->ramAssist.vCritHighuV = result.voltuV;
            }
            if (pRail->ramAssist.vCritHighuV != RM_PMU_VOLT_VALUE_0V_IN_UV)
            {
                tmpVoltageuV = LW_MAX(0, ((LwS32)pRail->ramAssist.vCritHighuV +
                    pRail->voltDeltauV[LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_VCRIT_HIGH_IDX]));

                // Avoided cast from composite expression to fix MISRA Rule 10.8.
                critHighuV = (LwU32)tmpVoltageuV;

                // Round up Vcrit High Voltage for disengaging RAM ASSIST.
                status = voltRailRoundVoltage(pRail, (LwS32*)&critHighuV, LW_TRUE, LW_TRUE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto voltRailRamAssistDynamilwpdate_exit;
                }
            }
        }

        if (pRail->ramAssist.vCritLowVfeEquIdx !=
                PMU_PERF_VFE_EQU_INDEX_ILWALID)
        {
            if (bVfeEvaluate)
            {
                status = vfeEquEvaluate(pRail->ramAssist.vCritLowVfeEquIdx, NULL, 0,
                                    LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_UV,
                                    &result);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto voltRailRamAssistDynamilwpdate_exit;
                }
                pRail->ramAssist.vCritLowuV = result.voltuV;
            }
            if (pRail->ramAssist.vCritLowuV != RM_PMU_VOLT_VALUE_0V_IN_UV)
            {
                tmpVoltageuV = LW_MAX(0, ((LwS32)pRail->ramAssist.vCritLowuV +
                    pRail->voltDeltauV[LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_VCRIT_LOW_IDX]));

                // Avoided cast from composite expression to fix MISRA Rule 10.8.
                critLowuV = (LwU32)tmpVoltageuV;

                // Round up Vcrit Low Voltage for engaging RAM ASSIST.
                status = voltRailRoundVoltage(pRail, (LwS32*)&critLowuV, LW_TRUE, LW_TRUE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto voltRailRamAssistDynamilwpdate_exit;
                }
            }
        }

        status = s_voltRailRamAssistSanityCheck(critHighuV, critLowuV);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailRamAssistDynamilwpdate_exit;
        }

        pRail->ramAssist.vCritHighuV       = critHighuV;
        pRail->ramAssist.vCritLowuV        = critLowuV;

        status = s_voltRailProgramVcritThresholds(pRail);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailRamAssistDynamilwpdate_exit;
        }
    }

    status = s_voltRailRamAssistCtrlEnable(pRail);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltRailRamAssistDynamilwpdate_exit;
    }

voltRailRamAssistDynamilwpdate_exit:
    return status;
}

/*!
 * @brief Checks whether RAM ASSIST is Engaged for the ADCs in the given rail.
 *
 * @param[in]  pRail
 *      Pointer that holds the VOLT RAIL OBJECT.
 *
 * @param[in]  pGetStatus
 *      Pointer that holds the VOLT RAIL GET STATUS RPC STRUCT.
 *
 * @return  FLCN_OK
 *      RAM Assist Engaged Mask Successfully Updated.
 */
FLCN_STATUS
s_voltRailRamAssistIsEngaged
(
    VOLT_RAIL                                  *pRail,
    RM_PMU_VOLT_VOLT_RAIL_BOARDOBJ_GET_STATUS  *pGetStatus
)
{
    FLCN_STATUS status              = FLCN_OK;
    status = clkAdcsRamAssistIsEngaged(&(pRail->adcDevMask),
                                      &(pGetStatus->ramAssist.engagedMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}
#endif

/*!
 * @copydoc VoltRailDynamilwpdate()
 */
FLCN_STATUS
voltRailDynamilwpdate_IMPL
(
    VOLT_RAIL  *pRail,
    LwBool      bVfeEvaluate
)
{
    LwU32       relLimituV          = RM_PMU_VOLT_VALUE_0V_IN_UV;
    LwU32       altRelLimituV       = RM_PMU_VOLT_VALUE_0V_IN_UV;
    LwU32       ovLimituV           = RM_PMU_VOLT_VALUE_0V_IN_UV;
    LwU32       vminLimituV         = RM_PMU_VOLT_VALUE_0V_IN_UV;
    LwS32       voltMarginLimituV   = RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED;
    LwU32       maxLimituV          = LW_U32_MAX;
    LwS32       tmpVoltageuV        = RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED;
    FLCN_STATUS status              = FLCN_OK;
    LwS32       limituV;
    RM_PMU_PERF_VFE_EQU_RESULT
                result;

    // Evaluate default maximum reliability VFE equation index if its valid.
    if (pRail->relLimitVfeEquIdx != PMU_PERF_VFE_EQU_INDEX_ILWALID)
    {
        if (bVfeEvaluate)
        {
            status = vfeEquEvaluate(pRail->relLimitVfeEquIdx, NULL, 0,
                                LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_UV,
                                &result);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailDynamilwpdate_IMPL_exit;
            }
            pRail->defRelLimituV = result.voltuV;
        }
        if (pRail->defRelLimituV != RM_PMU_VOLT_VALUE_0V_IN_UV)
        {
            tmpVoltageuV = LW_MAX(0, ((LwS32)pRail->defRelLimituV +
                pRail->voltDeltauV[LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_REL_LIM_IDX]));
            // Avoided cast from composite expression to fix MISRA Rule 10.8.
            relLimituV = (LwU32)tmpVoltageuV;

            // Round down the default maximum reliability limit.
            status = voltRailRoundVoltage(pRail, (LwS32*)&relLimituV, LW_FALSE, LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailDynamilwpdate_IMPL_exit;
            }

            // Evaluate maximum voltage limit.
            maxLimituV = LW_MIN(relLimituV, maxLimituV);
        }
    }

    // Evaluate alternate maximum reliability VFE equation index if its valid.
    if (pRail->altRelLimitVfeEquIdx != PMU_PERF_VFE_EQU_INDEX_ILWALID)
    {
        if (bVfeEvaluate)
        {
            status = vfeEquEvaluate(pRail->altRelLimitVfeEquIdx, NULL, 0,
                                LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_UV,
                                &result);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailDynamilwpdate_IMPL_exit;
            }
            pRail->defAltRelLimituV = result.voltuV;
        }
        if (pRail->defAltRelLimituV != RM_PMU_VOLT_VALUE_0V_IN_UV)
        {
            tmpVoltageuV = LW_MAX(0, ((LwS32)pRail->defAltRelLimituV +
              pRail->voltDeltauV[LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_ALT_REL_LIM_IDX]));
            // Avoided cast from composite expression to fix MISRA Rule 10.8.
            altRelLimituV = (LwU32)tmpVoltageuV;

            // Round down the alternate maximum reliability limit.
            status = voltRailRoundVoltage(pRail, (LwS32*)&altRelLimituV, LW_FALSE, LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailDynamilwpdate_IMPL_exit;
            }

            // Evaluate maximum voltage limit.
            maxLimituV = LW_MIN(altRelLimituV, maxLimituV);
        }
    }

    // Evaluate over-voltage VFE equation index if its valid.
    if (pRail->ovLimitVfeEquIdx != PMU_PERF_VFE_EQU_INDEX_ILWALID)
    {
        if (bVfeEvaluate)
        {
            status = vfeEquEvaluate(pRail->ovLimitVfeEquIdx, NULL, 0,
                                LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_UV,
                                &result);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailDynamilwpdate_IMPL_exit;
            }
            pRail->defOvLimituV = result.voltuV;
        }
        if (pRail->defOvLimituV != RM_PMU_VOLT_VALUE_0V_IN_UV)
        {
            tmpVoltageuV = LW_MAX(0, ((LwS32)pRail->defOvLimituV +
                pRail->voltDeltauV[LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_OV_LIM_IDX]));
            // Avoided cast from composite expression to fix MISRA Rule 10.8.
            ovLimituV = (LwU32)tmpVoltageuV;

            // Round down the over-voltage limit.
            status = voltRailRoundVoltage(pRail, (LwS32*)&ovLimituV, LW_FALSE, LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailDynamilwpdate_IMPL_exit;
            }

            // Evaluate maximum voltage limit.
            maxLimituV = LW_MIN(ovLimituV, maxLimituV);
        }
    }

    // Evaluate Vmin VFE equation index if its valid.
    if (pRail->vminLimitVfeEquIdx != PMU_PERF_VFE_EQU_INDEX_ILWALID)
    {
        if (bVfeEvaluate)
        {
            status = vfeEquEvaluate(pRail->vminLimitVfeEquIdx, NULL, 0,
                                LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_UV,
                                &result);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailDynamilwpdate_IMPL_exit;
            }
            pRail->defVminLimituV = result.voltuV;
        }
        if (pRail->defVminLimituV != RM_PMU_VOLT_VALUE_0V_IN_UV)
        {
            tmpVoltageuV = LW_MAX(0, ((LwS32)pRail->defVminLimituV +
                pRail->voltDeltauV[LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_VMIN_LIM_IDX]));
            // Avoided cast from composite expression to fix MISRA Rule 10.8.
            vminLimituV = (LwU32)tmpVoltageuV;

            // Round up the Vmin limit.
            status = voltRailRoundVoltage(pRail, (LwS32*)&vminLimituV, LW_TRUE, LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailDynamilwpdate_IMPL_exit;
            }
        }
    }

    // Evaluate worst case voltage margin VFE equation index if its valid.
    if (pRail->voltMarginLimitVfeEquIdx != PMU_PERF_VFE_EQU_INDEX_ILWALID)
    {
        if (bVfeEvaluate)
        {
            status = vfeEquEvaluate(pRail->voltMarginLimitVfeEquIdx, NULL, 0,
                                LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_DELTA_UV,
                                &result);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailDynamilwpdate_IMPL_exit;
            }
            limituV = result.voltDeltauV;
        }
        else
        {
            limituV = pRail->voltMarginLimituV;
        }
        voltMarginLimituV = limituV;

        //
        // Round up the worst case voltage margin limit.
        // Do NOT bound the voltage as the voltage margin will
        // be around ~12mV whereas the min bound is around ~300mV
        //
        status = voltRailRoundVoltage(pRail, &voltMarginLimituV, LW_TRUE, LW_FALSE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailDynamilwpdate_IMPL_exit;
        }
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_RAM_ASSIST))
    status = s_voltRailRamAssistDynamilwpdate(pRail, bVfeEvaluate);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltRailDynamilwpdate_IMPL_exit;
    }
#endif

    // Cache all the voltage limits.
    pRail->relLimituV        = relLimituV;
    pRail->altRelLimituV     = altRelLimituV;
    pRail->ovLimituV         = ovLimituV;
    pRail->maxLimituV        = maxLimituV;
    pRail->vminLimituV       = vminLimituV;
    pRail->voltMarginLimituV = voltMarginLimituV;

voltRailDynamilwpdate_IMPL_exit:
    return status;
}

/*!
 * @brief VOLT-internal function to obtain current voltage.
 * This function is called only from VOLT code and is not expected to be
 * called by VOLT clients and is not expected to be called by VOLT clients.
 * Please also make sure that this internal function is strictly called only
 * after acquiring either read or write VOLT semaphore or during init/load path
 * where synchronization isn't required.
 *
 * @param[in]  pRail
 *      Pointer to the VOLT_RAIL.
 *
 * @param[in]  pVoltageuV
 *      Pointer to the variable to store current rail voltage.
 *
 * @return  FLCN_OK     Always succeeds.
 */
FLCN_STATUS
voltRailGetVoltageInternal_IMPL
(
    VOLT_RAIL  *pRail,
    LwU32      *pVoltageuV
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((pVoltageuV == NULL) || (pRail == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto voltRailGetVoltageInternal_IMPL_exit;
    }

    // Obtain voltage of the default VOLT_DEVICE.
    *pVoltageuV = pRail->lwrrVoltDefaultuV;

voltRailGetVoltageInternal_IMPL_exit:
    return status;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET))
/*!
 * @brief Compute the maximum positive and maximum negative voltage offset that can be
 * applied to a set of voltage rails.
 *
 * @param[in]       pRailList
 *      Pointer that holds the list containing target voltage data for the VOLT_RAILs.
 * @param[in/out]   pRailOffsetList
 *      Pointer that holds the list storing the voltage data for the VOLT_RAILs.
 * @param[in]       bSkipVoltRangeTrim
 *      Boolean tracking whether client explcitly requested to skip the
 *      voltage range trimming.
 *
 * @return  FLCN_OK
 *      Max positive and max negative offset was computed successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
voltRailOffsetRangeGet
(
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST  *pRailList,
    VOLT_RAIL_OFFSET_RANGE_LIST      *pRailOffsetList,
    LwBool                            bSkipVoltRangeTrim
)
{
    FLCN_STATUS    status = FLCN_OK;
    LwU8           i;

    pRailOffsetList->numRails = pRailList->numRails;

    // Compute the offsets for all rails.
    for (i = 0; i < pRailList->numRails; i++)
    {
        LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pRailItem =
            &(pRailList->rails[i]);
        VOLT_RAIL_OFFSET_RANGE_ITEM *pRailOffsetItem    =
            &(pRailOffsetList->railOffsets[i]);
        VOLT_RAIL *pRail                                =
            VOLT_RAIL_GET(pRailList->rails[i].railIdx);

        if (pRail == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto voltRailOffsetRangeGet_exit;
        }

        pRailOffsetItem->railIdx = pRailItem->railIdx;

        // Compute maximum negative offset.
        status = s_maxNegativeOffsetGet(pRail, pRailItem, pRailOffsetItem,
                    bSkipVoltRangeTrim);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailOffsetRangeGet_exit;
        }

        // Compute maximum positive offset.
        status = s_maxPositiveOffsetGet(pRail, pRailItem, pRailOffsetItem,
                    bSkipVoltRangeTrim);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailOffsetRangeGet_exit;
        }
    }

voltRailOffsetRangeGet_exit:
    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET)

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 */
static FLCN_STATUS
s_voltRailIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_VOLT_VOLT_RAIL_BOARDOBJ_GET_STATUS  *pGetStatus;
    VOLT_RAIL                                  *pRail;
    FLCN_STATUS                                 status;

    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto s_voltRailIfaceModel10GetStatus_exit;
    }
    pRail       = (VOLT_RAIL *)pBoardObj;
    pGetStatus  = (RM_PMU_VOLT_VOLT_RAIL_BOARDOBJ_GET_STATUS *)pPayload;

    // Return the current voltage of the default VOLT_DEVICE.
    pGetStatus->lwrrVoltDefaultuV = pRail->lwrrVoltDefaultuV;

    // Return cached voltage limits.
    pGetStatus->relLimituV            = pRail->relLimituV;
    pGetStatus->altRelLimituV         = pRail->altRelLimituV;
    pGetStatus->ovLimituV             = pRail->ovLimituV;
    pGetStatus->maxLimituV            = pRail->maxLimituV;
    pGetStatus->vminLimituV           = pRail->vminLimituV;
    pGetStatus->voltMarginLimituV     = pRail->voltMarginLimituV;
    pGetStatus->voltMinNoiseUnawareuV = pRail->voltMinNoiseUnawareuV;
    pGetStatus->lwrrVoltSenseduV      = RM_PMU_VOLT_VALUE_0V_IN_UV;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_RAM_ASSIST))
    pGetStatus->ramAssist.vCritHighuV = pRail->ramAssist.vCritHighuV;
    pGetStatus->ramAssist.vCritLowuV  = pRail->ramAssist.vCritLowuV;
    pGetStatus->ramAssist.bEnabled    = pRail->ramAssist.bEnabled;

    if ((pRail->ramAssist.type == LW2080_CTRL_VOLT_VOLT_RAIL_RAM_ASSIST_TYPE_DYNAMIC_WITHOUT_LUT)
            ||(pRail->ramAssist.type == LW2080_CTRL_VOLT_VOLT_RAIL_RAM_ASSIST_TYPE_DYNAMIC_WITH_LUT))
    {
        status = s_voltRailRamAssistIsEngaged(pRail, pGetStatus);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_voltRailIfaceModel10GetStatus_exit;
        }
    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_RAM_ASSIST)

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    pGetStatus->railAction            = pRail->railAction;
#else
    pGetStatus->railAction            = LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION)

s_voltRailIfaceModel10GetStatus_exit:
    return status;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_SCALING_PWR_EQUATION))
/*!
 * @copydoc VoltRailGetScaleFactor()
 *
 * Obtain the scale factor through the Scaling Power Equation.
 * The scaling power equation index will propagate from the VBIOS.
 */
FLCN_STATUS
voltRailGetScaleFactor
(
    VOLT_RAIL              *pRail,
    LwU32                   voltageuV,
    LwBool                  bIsUnitmA,
    LwUFXP20_12            *pScaleFactor
)
{
    PWR_EQUATION   *pPwrEquation;
    FLCN_STATUS     status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgr)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgr)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        pPwrEquation = PWR_EQUATION_GET(pRail->baScalingPwrEqnIdx);
        if (pPwrEquation == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto voltRailGetScaleFactor_exit;
        }

        *pScaleFactor = pwrEquationBA15ScaleComputeScaleA((PWR_EQUATION_BA15_SCALE *)pPwrEquation,
                        bIsUnitmA, voltageuV);

voltRailGetScaleFactor_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_SCALING_PWR_EQUATION)

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_EQUATION_LEAKAGE))
/*!
 * @copydoc VoltRailGetLeakage()
 */
FLCN_STATUS
voltRailGetLeakage
(
    VOLT_RAIL              *pRail,
    LwU32                   voltageuV,
    LwBool                  bIsUnitmA,
    PMGR_LPWR_RESIDENCIES  *pPgRes,
    LwU32                   *pLeakageVal
)
{
    PWR_EQUATION   *pPwrEquation;
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgr)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLeakage)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        pPwrEquation = PWR_EQUATION_GET(pRail->leakagePwrEquIdx);
        if (pPwrEquation == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto voltRailGetLeakage_exit;
        }

        status = pwrEquationGetLeakage(pPwrEquation, voltageuV, bIsUnitmA,
                    pPgRes, pLeakageVal);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailGetLeakage_exit;
        }

voltRailGetLeakage_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_EQUATION_LEAKAGE)

/*!
 * @copydoc VoltRailGetVoltageMax()
 *
 * @note Clients outside of OBJVOLT are expected to acquire VOLT read semaphore
 *  before calling this API. Do NOT acquire VOLT read semaphore within this API
 *  otherwise there will be deadlock. All existing callers within OBJVOLT
 *  already acquire VOLT write semaphore for synchronization.
 */
FLCN_STATUS
voltRailGetVoltageMax_IMPL
(
    VOLT_RAIL  *pRail,
    LwU32      *pVoltageuV
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((pVoltageuV == NULL) || (pRail == NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto voltRailGetVoltageMax_IMPL_exit;
    }

    *pVoltageuV = pRail->maxLimituV;

voltRailGetVoltageMax_IMPL_exit:
    return status;
}

/*!
 * @copydoc VoltRailGetVoltageMin()
 */
FLCN_STATUS
voltRailGetVoltageMin
(
    VOLT_RAIL  *pRail,
    LwU32      *pVoltageuV
)
{
    if (pRail->vminLimitVfeEquIdx != PMU_PERF_VFE_EQU_INDEX_ILWALID)
    {
        voltReadSemaphoreTake();
       *pVoltageuV = pRail->vminLimituV;
        voltReadSemaphoreGive();
        return FLCN_OK;
    }

    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc VoltRailGetNoiseUnawareVmin()
 */
FLCN_STATUS
voltRailGetNoiseUnawareVmin
(
    VOLT_RAIL  *pRail,
    LwU32      *pVoltageuV
)
{
    voltReadSemaphoreTake();
    *pVoltageuV = pRail->voltMinNoiseUnawareuV;
    voltReadSemaphoreGive();
    return FLCN_OK;
}

#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLTAGE_SENSED)
/*!
 * @copydoc VoltRailGetVoltageSensed()
 */
FLCN_STATUS
voltRailGetVoltageSensed
(
    VOLT_RAIL                      *pRail,
    VOLT_RAIL_SENSED_VOLTAGE_DATA  *pData
)
{
    OSTASK_OVL_DESC ovlDescList[]   = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkVolt)
    };
    LwU32           milwoltageuV    = LW_U32_MAX;
    LwU32           maxVoltageuV    = LW_U32_MIN;
    LwU32           avgVoltageuV    = RM_PMU_VOLT_VALUE_0V_IN_UV;
    CLK_ADC_DEVICE *pAdcDev;
    LwBoardObjIdx   adcDevIdx;
    LwU8            maxNumAdcDevs;
    FLCN_STATUS     status;

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Sanity check that ADC device mask is non-zero.
        if (boardObjGrpMaskIsZero(&(pRail->adcDevMask)))
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            PMU_BREAKPOINT();
            goto voltRailGetVoltageSensed_exit;
        }

        // Sanity check input pointer.
        if (pData == NULL)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto voltRailGetVoltageSensed_exit;
        }

        // Sanity check that input sample buffer is of sufficient size.
        maxNumAdcDevs = boardObjGrpMaskBitIdxHighest(&pRail->adcDevMask) + 1U;
        if (pData->numSamples < maxNumAdcDevs)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto voltRailGetVoltageSensed_exit;
        }

        // Sanity check sensed voltage mode.
        if (pData->mode >= LW2080_CTRL_VOLT_VOLT_RAIL_SENSED_VOLTAGE_MODE_NUM)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto voltRailGetVoltageSensed_exit;
        }

        // Get the sensed voltage ADC samples for all the ADCs specified by the mask.
        status = CLK_ADCS_VOLTAGE_READ_EXTERNAL(pRail->adcDevMask,
                                                pData->pClkAdcAccSample);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailGetVoltageSensed_exit;
        }

        // Compute actual sensed voltage based on the sensed voltage mode.
        BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, adcDevIdx, &pRail->adcDevMask.super)
        {
            switch (pData->mode)
            {
                case LW2080_CTRL_VOLT_VOLT_RAIL_SENSED_VOLTAGE_MODE_MIN:
                {
                    milwoltageuV =
                        LW_MIN(milwoltageuV,
                            pData->pClkAdcAccSample[adcDevIdx].correctedVoltageuV);
                    break;
                }
                case LW2080_CTRL_VOLT_VOLT_RAIL_SENSED_VOLTAGE_MODE_MAX:
                {
                    maxVoltageuV =
                        LW_MAX(maxVoltageuV,
                            pData->pClkAdcAccSample[adcDevIdx].correctedVoltageuV);
                    break;
                }
                case LW2080_CTRL_VOLT_VOLT_RAIL_SENSED_VOLTAGE_MODE_AVG:
                {
                    avgVoltageuV +=
                        pData->pClkAdcAccSample[adcDevIdx].correctedVoltageuV;
                    break;
                }
                default:
                {
                    status = FLCN_ERR_NOT_SUPPORTED;
                    PMU_BREAKPOINT();
                    goto voltRailGetVoltageSensed_exit;
                    break;
                }
            }
        }
        BOARDOBJGRP_ITERATOR_END;

        // Copy out the sensed voltage.
        switch (pData->mode)
        {
            case LW2080_CTRL_VOLT_VOLT_RAIL_SENSED_VOLTAGE_MODE_MIN:
            {
                pData->voltageuV = milwoltageuV;
                break;
            }
            case LW2080_CTRL_VOLT_VOLT_RAIL_SENSED_VOLTAGE_MODE_MAX:
            {
                pData->voltageuV = maxVoltageuV;
                break;
            }
            case LW2080_CTRL_VOLT_VOLT_RAIL_SENSED_VOLTAGE_MODE_AVG:
            {
                pData->voltageuV =
                    LW_UNSIGNED_ROUNDED_DIV(avgVoltageuV,
                        boardObjGrpMaskBitSetCount(&pRail->adcDevMask));
                break;
            }
            default:
            {
                status = FLCN_ERR_NOT_SUPPORTED;
                PMU_BREAKPOINT();
                goto voltRailGetVoltageSensed_exit;
                break;
            }
        }

        // Round up the sensed voltage.
        status = voltRailRoundVoltage(pRail, (LwS32*)&pData->voltageuV,
                    LW_TRUE, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailGetVoltageSensed_exit;
        }

voltRailGetVoltageSensed_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}
#endif

/*!
 * @copydoc VoltRailVoltDomainColwertToIdx()
 */
LwU8
voltRailVoltDomainColwertToIdx
(
    LwU8    voltDomain
)
{
    switch (Volt.railMetadata.voltDomainHAL)
    {
        case LW2080_CTRL_VOLT_VOLT_DOMAIN_HAL_GP10X_SINGLE_RAIL:
        {
            switch (voltDomain)
            {
                case LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC:
                    return 0;
            }
            break;
        }
        case LW2080_CTRL_VOLT_VOLT_DOMAIN_HAL_GP10X_SPLIT_RAIL:
        {
            switch (voltDomain)
            {
                case LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC:
                    return 0;
                case LW2080_CTRL_VOLT_VOLT_DOMAIN_SRAM:
                    return 1;
            }
            break;
        }
        case LW2080_CTRL_VOLT_VOLT_DOMAIN_HAL_GA10X_MULTI_RAIL:
        {
            switch (voltDomain)
            {
                case LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC:
                    return 0;
                case LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD:
                    return 1;
            }
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

    return LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID;
}

/*!
 * @brief Sanity checks target voltage rails data including boundary checks, rail action based checks etc.
 *
 * @param[in]  pRailList
 *      Pointer that holds the list containing target voltage data for the VOLT_RAILs.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_OK
 *      Requested target voltages were sanitized successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
voltRailSanityCheck
(
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pRailList,
    LwBool                           bCheckState
)
{
    FLCN_STATUS   status         = FLCN_OK;
    VOLT_RAIL    *pVoltRail      = NULL;
    VOLT_DEVICE  *pDefaultDevice = NULL;
    LwU8          i;

    // Sanity checks.
    if ((pRailList == NULL)         ||
        (pRailList->numRails == 0U) ||
        (pRailList->numRails > LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto voltRailSanityCheck_exit;
    }

    for (i = 0; i < pRailList->numRails; i++)
    {
        if ((pRailList->rails[i].railIdx ==
                LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)   ||
            (!VOLT_RAIL_INDEX_IS_VALID(pRailList->rails[i].railIdx)))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto voltRailSanityCheck_exit;
        }

        pVoltRail = VOLT_RAIL_GET(pRailList->rails[i].railIdx);

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
        if (bCheckState)
        {
            status = s_voltRailSanityCheckState(pVoltRail, &(pRailList->rails[i]));
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailSanityCheck_exit;
            }
        }
#else
        status = s_voltRailSanityCheckState_VF_SWITCH(pVoltRail, &(pRailList->rails[i]));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltRailSanityCheck_exit;
        }
#endif

        pDefaultDevice = voltRailDefaultVoltDevGet(pVoltRail);
        if (pDefaultDevice == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto voltRailSanityCheck_exit;
        }

        if (bCheckState)
        {
            status = voltDeviceSanityCheck(pDefaultDevice, pRailList->rails[i].railAction);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltRailSanityCheck_exit;
            }
        }
    }

voltRailSanityCheck_exit:
    return status;
}

/*!
 * @copydoc VoltRailGetVbiosBootVoltage()
 */
FLCN_STATUS
voltRailGetVbiosBootVoltage
(
    VOLT_RAIL  *pRail,
    LwU32      *pVoltageuV
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((pVoltageuV == NULL) || (pRail == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto voltRailGetVbiosBootVoltage_exit;
    }

    voltReadSemaphoreTake();
    *pVoltageuV = pRail->vbiosBootVoltageuV;
    voltReadSemaphoreGive();

voltRailGetVbiosBootVoltage_exit:
    return status;
}

/*!
 * @copydoc VoltRailGetVoltDomainSanityList()
 */
FLCN_STATUS
voltRailGetVoltDomainSanityList
(
    LwU8    voltDomainHAL,
    VOLT_DOMAINS_LIST
           *pVoltDomainsList
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        count  = 0;

    if (pVoltDomainsList == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto voltRailGetVoltDomainSanityList_exit;
    }

    memset(pVoltDomainsList, 0, sizeof(VOLT_DOMAINS_LIST));

    switch (voltDomainHAL)
    {
        case LW2080_CTRL_VOLT_VOLT_DOMAIN_HAL_GA10X_MULTI_RAIL:
        {
            // LOGIC rail can be gated when MULTI_RAIL voltage domain HAL is enabled.
            pVoltDomainsList->voltDomains[count++] = LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC;
            pVoltDomainsList->numDomains = count;
            break;
        }
        default:
        {
            // Nothing to be done.
            break;
        }
    }

voltRailGetVoltDomainSanityList_exit:
    return status;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET))
/*!
 * @brief Compute the maximum negative voltage offset that can be applied to the
 *        input VOLT_RAIL.
 *
 * @param[in]  pRail
 *      Pointer to the VOLT_RAIL.
 * @param[in]  pRailListItem
 *      Pointer that holds the item containing input target voltage data.
 * @param[out]  pRailOffsetItem
 *      Pointer that holds the item containing voltage offset information.
 * @param[in]   bSkipVoltRangeTrim
 *      Boolean tracking whether client explcitly requested to skip the
 *      voltage range trimming.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_OK
 *      Max positive and max negative offset was computed successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
static FLCN_STATUS
s_maxNegativeOffsetGet
(
    VOLT_RAIL                              *pRail,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM   *pRailListItem,
    VOLT_RAIL_OFFSET_RANGE_ITEM            *pRailOffsetItem,
    LwBool                                  bSkipVoltRangeTrim
)
{
    LwS32           voltOffsetNegativeMaxuV;
    LwU32           vminLimituV;
    LwU32           milwoltageValueuV;
    FLCN_STATUS     status;

    // Init
    pRailOffsetItem->voltOffsetNegativeMaxuV = 0;

    status = voltRailGetVoltageMin(pRail, &vminLimituV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_maxNegativeOffsetGet_exit;
    }

    milwoltageValueuV = LW_MAX(pRailListItem->voltageMinNoiseUnawareuV, vminLimituV);
    if (pRailListItem->voltageuV < milwoltageValueuV)
    {
        if (bSkipVoltRangeTrim)
        {
            status = FLCN_OK;
        }
        else
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
        }

        goto s_maxNegativeOffsetGet_exit;
    }

    voltOffsetNegativeMaxuV = (LwS32)(milwoltageValueuV - pRailListItem->voltageuV);

    // Round up maximum negative voltage offset.
    status = voltRailRoundVoltage(pRail,
                &voltOffsetNegativeMaxuV, LW_TRUE, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_maxNegativeOffsetGet_exit;
    }

    // Store it in the output structure.
    pRailOffsetItem->voltOffsetNegativeMaxuV = voltOffsetNegativeMaxuV;

s_maxNegativeOffsetGet_exit:
    return status;
}

/*!
 * @brief Compute the maximum positive voltage offset that can be applied to the
 * input VOLT_RAIL.
 *
 * @param[in]   pRail
 *      Pointer to the VOLT_RAIL.
 * @param[in]   pRailListItem
 *      Pointer that holds the item containing input target voltage data.
 * @param[out]  pRailOffsetItem
 *      Pointer that holds the item containing voltage offset information.
 * @param[in]   bSkipVoltRangeTrim
 *      Boolean tracking whether client explcitly requested to skip the
 *      voltage range trimming.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_OK
 *      Max positive and max negative offset was computed successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
static FLCN_STATUS
s_maxPositiveOffsetGet
(
    VOLT_RAIL                              *pRail,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM   *pRailListItem,
    VOLT_RAIL_OFFSET_RANGE_ITEM            *pRailOffsetItem,
    LwBool                                  bSkipVoltRangeTrim
)
{
    LwU32           voltOffsetPositiveMaxuV;
    LwU32           maxLimituV;
    FLCN_STATUS     status;

    // Init
    pRailOffsetItem->voltOffsetPositiveMaxuV = 0;

    status = voltRailGetVoltageMax(pRail, &maxLimituV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_maxPositiveOffsetGet_exit;
    }

    if (pRailListItem->voltageuV > maxLimituV)
    {
        if (bSkipVoltRangeTrim)
        {
            status = FLCN_OK;
        }
        else
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
        }

        goto s_maxPositiveOffsetGet_exit;
    }

    voltOffsetPositiveMaxuV = maxLimituV - pRailListItem->voltageuV;

    // Round down the voltage value.
    status = voltRailRoundVoltage(pRail,
                (LwS32 *)&voltOffsetPositiveMaxuV, LW_FALSE, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_maxPositiveOffsetGet_exit;
    }

    // Store it in the output structure.
    pRailOffsetItem->voltOffsetPositiveMaxuV = voltOffsetPositiveMaxuV;

s_maxPositiveOffsetGet_exit:
    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET)

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
/*!
 * @brief Determines if the input rail action is valid considering the current
 *        state of the rail. For full state transition table, refer MEMSYS-VOLT
 *        design at https://confluence.lwpu.com/pages/viewpage.action?pageId=187676304#MemSysIsland:VOLT&THERM:DesignDolwment-Validstatesforavoltagerail
 *
 * @param[in]  pVoltRail    Pointer to VOLT_RAIL
 * @param[in]  pListItem    Pointer to the list item containing target voltage data.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_OK
 *      Sanity check was OK.
 * @return  Other errors propagated from other helper functions.
 */
static FLCN_STATUS
s_voltRailSanityCheckState
(
    VOLT_RAIL *pVoltRail,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
              *pListItem
)
{
    FLCN_STATUS status;

    switch (pListItem->railAction)
    {
        case LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH:
        {
            status = s_voltRailSanityCheckState_VF_SWITCH(pVoltRail, pListItem);
            break;
        }
        case LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_GATE:
        {
            status = s_voltRailSanityCheckState_GATE(pVoltRail, pListItem);
            break;
        }
        case LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_UNGATE:
        {
            status = s_voltRailSanityCheckState_UNGATE(pVoltRail, pListItem);
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto s_voltRailSanityCheckState_exit;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltRailSanityCheckState_exit;
    }

s_voltRailSanityCheckState_exit:
    return status;
}

/*!
 * @brief  Sanity checks VOLT_RAIL data for proposed GATE state.
 *
 * @param[in]  pVoltRail    Pointer ot VOLT_RAIL.
 * @param[in]  pListItem    Pointer to the list item containing target voltage data.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_OK
 *      Valid input parameters.
 */
static FLCN_STATUS
s_voltRailSanityCheckState_GATE
(
    VOLT_RAIL *pVoltRail,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
              *pListItem
)
{
    return (pVoltRail->railAction == LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_GATE) ?
            FLCN_ERR_ILWALID_ARGUMENT :
            FLCN_OK;
}

/*!
 * @brief Sanity checks VOLT_RAIL data for proposed UNGATE state.
 *
 * @param[in]  pVoltRail    Pointer ot VOLT_RAIL.
 * @param[in]  pListItem    Pointer to the list item containing target voltage data.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_OK
 *      Valid input parameters.
 */
static FLCN_STATUS
s_voltRailSanityCheckState_UNGATE
(
    VOLT_RAIL *pVoltRail,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
              *pListItem
)
{
    return (pVoltRail->railAction == LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_GATE)   ?
            FLCN_OK :
            FLCN_ERR_ILWALID_ARGUMENT;
}
#endif

/*!
 * @brief Sanity checks VOLT_RAIL data for input VF_SWITCH state.
 *
 * @param[in]  pVoltRail    Pointer ot VOLT_RAIL.
 * @param[in]  pListItem    Pointer to the list item containing target voltage data.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_OK
 *      Requested target voltages were sanitized successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
static FLCN_STATUS
s_voltRailSanityCheckState_VF_SWITCH
(
    VOLT_RAIL *pVoltRail,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
              *pListItem
)
{
    FLCN_STATUS status = FLCN_OK;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    if (pVoltRail->railAction == LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_GATE)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_voltRailSanityCheckState_VF_SWITCH_exit;
    }
#endif

    // Target voltage and noise-unaware Vmin should not be zero.
    if ((pListItem->voltageuV ==
            RM_PMU_VOLT_VALUE_0V_IN_UV) ||
        (pListItem->voltageMinNoiseUnawareuV ==
            RM_PMU_VOLT_VALUE_0V_IN_UV))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_voltRailSanityCheckState_VF_SWITCH_exit;
    }

    //
    // Target voltage should never be less than noise-unaware Vmin.
    // Please note that voltageuV and voltageMinNoiseUnawareuV are unrounded here.
    //
    if (pListItem->voltageuV <
            pListItem->voltageMinNoiseUnawareuV)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_voltRailSanityCheckState_VF_SWITCH_exit;
    }

s_voltRailSanityCheckState_VF_SWITCH_exit:
    return status;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_CONTROLLER))
/*!
 * @brief Queues CLK Controllers ilwalidation event to PERF task
 *
 * @return  FLCN_OK
 *      Event was queued successfully
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
s_voltRailsClkControllersIlwalidate(void)
{
    // Queue the change request to PERF task
    FLCN_STATUS                              status;
    DISPATCH_PERF_CLK_CONTROLLERS_ILWALIDATE disp2perfClkControllerIlwalidate;

    memset(&disp2perfClkControllerIlwalidate, 0,
        sizeof(DISPATCH_PERF_CLK_CONTROLLERS_ILWALIDATE));
    disp2perfClkControllerIlwalidate.hdr.eventType =
        PERF_EVENT_ID_PERF_CLK_CONTROLLERS_ILWALIDATE;

    // Queue the event
    status = lwrtosQueueIdSend(LWOS_QUEUE_ID(PMU, PERF),
          &disp2perfClkControllerIlwalidate,
          sizeof(disp2perfClkControllerIlwalidate), 0);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltRailsClkControllersIlwalidate_exit;
    }

s_voltRailsClkControllersIlwalidate_exit:
    return status;
}
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_CONTROLLER))

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_VOLT_MARGIN_OFFSET))
/*!
 * @brief Queue volt rails voltage margin voltage offset
 *
 * @return  FLCN_OK
 *      Event was queued successfully
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
s_voltRailsQueueVoltOffset(void)
{
    LwBool          bChangeRequired         = LW_FALSE;
    FLCN_STATUS     status                  = FLCN_OK;
    OSTASK_OVL_DESC perfChangeOvlDescList[] = {
        CHANGE_SEQ_OVERLAYS_BASE
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(perfChangeOvlDescList);
    {
        LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET voltOffset;
        LwBoardObjIdx   railIdx;
        VOLT_RAIL      *pRail = NULL;

        memset(&voltOffset, 0,
            sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET));

        // Init the offset override request.
        voltOffset.bOverrideVoltOffset = LW_TRUE;

        //
        // Do not force trigger perf change as it will be done via
        // arbiter through ilwalidation chain.
        //
        voltOffset.bForceChange = LW_FALSE;

        BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, railIdx, NULL)
        {
            if (pRail->voltMarginLimitVfeEquIdx != PMU_PERF_VFE_EQU_INDEX_ILWALID)
            {
                // Update rail mask
                LW2080_CTRL_BOARDOBJGRP_MASK_BIT_SET(&(voltOffset.voltRailsMask.super), railIdx);

                // Update the controller mask.
                voltOffset.rails[railIdx].offsetMask = LWBIT32(LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_VOLT_MARGIN);

                // Update volt offset
                voltOffset.rails[railIdx].voltOffsetuV[LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_VOLT_MARGIN] =
                    pRail->voltMarginLimituV;

                // Request change
                bChangeRequired = LW_TRUE;
            }
        }
        BOARDOBJGRP_ITERATOR_END;

        if (bChangeRequired)
        {
            // Apply new delta to perf change sequencer.
            status = perfChangeSeqQueueVoltOffset(&voltOffset);
        }

    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(perfChangeOvlDescList);

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltRailsQueueVoltOffset_exit;
    }

s_voltRailsQueueVoltOffset_exit:
    return status;
}
#endif // (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_VOLT_MARGIN_OFFSET))
