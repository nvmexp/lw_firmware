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
 * @file  voltdev.c
 * @brief Volt Device Model
 *
 * This module is a collection of functions managing and manipulating state
 * related to volt devices
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "volt/objvolt.h"
#include "volt/voltdev.h"
#include "main.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * @brief   Array of all vtables pertaining to different VOLT_DEVICE object types.
 */
BOARDOBJ_VIRTUAL_TABLE *VoltDeviceVirtualTables[LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_DEVICE, MAX)] =
{
    [LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_DEVICE, BASE)          ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_DEVICE, VID)           ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_DEVICE, VID_REPROG)    ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_DEVICE, PWM)           ] = VOLT_VOLT_DEVICE_PWM_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_DEVICE, PWM_SCI)       ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_DEVICE, SOC)           ] = NULL,
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief @ref VOLT_DEVICE handler for the @ref RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
voltDeviceBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,  // _grpType
        VOLT_DEVICE,                            // _class
        pBuffer,                                // _pBuffer
        boardObjGrpIfaceModel10SetHeader,             // _hdrFunc
        voltVoltDeviceIfaceModel10SetEntry,           // _entryFunc
        pascalAndLater.volt.voltDeviceGrpSet,   // _ssElement
        OVL_INDEX_DMEM(perf),                   // _ovlIdxDmem
        VoltDeviceVirtualTables);               // _ppObjectVtables
}

/*!
 * @brief Super-class implementation
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
voltDeviceGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_VOLT_VOLT_DEVICE_BOARDOBJ_SET   *pSet =
        (RM_PMU_VOLT_VOLT_DEVICE_BOARDOBJ_SET *)pBoardObjDesc;
    VOLT_DEVICE                            *pDevice;
    FLCN_STATUS                             status;

    // Sanity check.
    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if ((pSet->voltageMinuV == RM_PMU_VOLT_VALUE_0V_IN_UV) || // Sanity check min and max voltage.
            (pSet->voltageMaxuV == RM_PMU_VOLT_VALUE_0V_IN_UV) || // Sanity check min and max voltage.
            (pSet->voltageMinuV > pSet->voltageMaxuV) ||          // Sanity check min and max voltage.
            (pSet->switchDelayus == 0U))                          // Sanity check voltage switch delay.
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto voltDeviceGrpIfaceModel10ObjSetImpl_SUPER_exit;
        }
    }

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto voltDeviceGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pDevice, *ppBoardObj, VOLT, VOLT_DEVICE, BASE,
                                  &status, voltDeviceGrpIfaceModel10ObjSetImpl_SUPER_exit);

    // Set member variables.
    pDevice->switchDelayus          = pSet->switchDelayus;
    pDevice->voltageMinuV           = pSet->voltageMinuV;
    pDevice->voltageMaxuV           = pSet->voltageMaxuV;
    pDevice->voltStepuV             = pSet->voltStepuV;
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    pDevice->powerUpSettleTimeus    = pSet->powerUpSettleTimeus;
    pDevice->powerDownSettleTimeus  = pSet->powerDownSettleTimeus;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION)

voltDeviceGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry()
 */
FLCN_STATUS
voltVoltDeviceIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_VID:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_DEVICE_VID))
            {
                status = voltDeviceGrpIfaceModel10ObjSetImpl_VID(pModel10, ppBoardObj,
                        sizeof(VOLT_DEVICE_VID), pBuf);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_DEVICE_PWM))
            {
                status = voltDeviceGrpIfaceModel10ObjSetImpl_PWM(pModel10, ppBoardObj,
                            sizeof(VOLT_DEVICE_PWM), pBuf);
            }
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief Load all VOLT_DEVICEs.
 */
FLCN_STATUS
voltDevicesLoad(void)
{
    return FLCN_OK;
}

/*!
 * @brief SUPER-class implementation.
 *
 * @copydoc VoltDeviceSetVoltage()
 */
FLCN_STATUS
voltDeviceSetVoltage_SUPER
(
    VOLT_DEVICE    *pDevice,
    LwU32           voltageuV,
    LwBool          bTrigger,
    LwBool          bWait,
    LwU8            railAction
)
{
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    if (railAction == LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH)
    {
#endif
        // Wait for the voltage to settle.
        if (bWait)
        {
            voltDeviceSwitchDelayApply(pDevice);
        }
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    }
    else if (bWait && (railAction == LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_GATE))
    {
        OS_TIMER_WAIT_US(pDevice->powerDownSettleTimeus);
    }
    else if (bWait && (railAction == LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_UNGATE))
    {
        OS_TIMER_WAIT_US(pDevice->powerUpSettleTimeus);
    }
    else
    {
        // To pacify Coverity.
    }
#endif
    return FLCN_OK;
}

/*!
 * @brief Apply VOLT_DEVICE switch delay.
 */
void
voltDeviceSwitchDelayApply
(
    VOLT_DEVICE    *pDevice
)
{
    OS_TIMER_WAIT_US(pDevice->switchDelayus);
}

/*!
 * @copydoc VoltDeviceGetVoltage()
 *
 * @note All VOLT_DEVICE implementations must override this interface
 */
FLCN_STATUS
voltDeviceGetVoltage_IMPL
(
    VOLT_DEVICE    *pDevice,
    LwU32          *pVoltageuV
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch ((BOARDOBJ_GET_TYPE(pDevice)))
    {
        case LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_VID:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_DEVICE_VID))
            {
                status = voltDeviceGetVoltage_VID(pDevice, pVoltageuV);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_DEVICE_PWM))
            {
                status = voltDeviceGetVoltage_PWM(pDevice, pVoltageuV);
            }
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc VoltDeviceSetVoltage()
 */
FLCN_STATUS
voltDeviceSetVoltage_IMPL
(
    VOLT_DEVICE    *pDevice,
    LwU32           voltageuV,
    LwBool          bTrigger,
    LwBool          bWait,
    LwU8            railAction
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch ((BOARDOBJ_GET_TYPE(pDevice)))
    {
        case LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_VID:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_DEVICE_VID))
            {
                status = voltDeviceSetVoltage_VID(pDevice, voltageuV, bTrigger, bWait, railAction);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_DEVICE_PWM))
            {
                status = voltDeviceSetVoltage_PWM(pDevice, voltageuV, bTrigger, bWait, railAction);
            }
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc VoltDeviceRoundVoltage()
 */
FLCN_STATUS
voltDeviceRoundVoltage_IMPL
(
    VOLT_DEVICE    *pDevice,
    LwS32          *pVoltageuV,
    LwBool          bRoundUp,
    LwBool          bBound
)
{
    LwS32   voltStepuV  = (LwS32)pDevice->voltStepuV;
    LwS32   voltageRemuV;

    // If single voltage value is supported, always set to min (== max).
    if (voltStepuV == RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED)
    {
        // For now assuming that deltas are always unbounded.
        *pVoltageuV =
            bBound ? (LwS32)pDevice->voltageMinuV : RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED;
    }
    else
    {
        voltageRemuV = *pVoltageuV % voltStepuV;
        if (voltageRemuV != RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED)
        {
            // Find positive arithmetic reminder for negative voltage.
            if (voltageRemuV < RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED)
            {
                voltageRemuV += voltStepuV;
            }

            // Subtract reminder from the voltage value.
            *pVoltageuV -= voltageRemuV;

            // When rounding up find next higher voltage value.
            if (bRoundUp)
            {
                *pVoltageuV += voltStepuV;
            }
        }
    }

    // Bound the rounded value within supported range.
    if (bBound)
    {
        *pVoltageuV = LW_MAX(*pVoltageuV, (LwS32)pDevice->voltageMinuV);
        *pVoltageuV = LW_MIN(*pVoltageuV, (LwS32)pDevice->voltageMaxuV);
    }

    return FLCN_OK;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_IPC_VMIN))
/*!
 * @copydoc VoltDeviceConfigure()
 */
FLCN_STATUS
voltDeviceConfigure
(
    VOLT_DEVICE *pDevice,
    LwBool       bEnable
)
{
    switch (BOARDOBJ_GET_TYPE(pDevice))
    {
        case LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_VOLT_VOLT_DEVICE_PWM);
            return voltDeviceConfigure_PWM(pDevice, bEnable);
            break;
        default:
            PMU_BREAKPOINT();
            return FLCN_ERR_NOT_SUPPORTED;
            break;
    }
}
#endif

/*!
 * @copydoc VoltDeviceSanityCheck()
 */
FLCN_STATUS
voltDeviceSanityCheck
(
    VOLT_DEVICE    *pDevice,
    LwU8            railAction
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pDevice))
    {
        case LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_VOLT_VOLT_DEVICE_PWM);
            status = voltDeviceSanityCheck_PWM(pDevice, railAction);
            break;
        default:
            status = FLCN_OK;
            break;
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltDeviceSanityCheck_exit;
    }

voltDeviceSanityCheck_exit:
    return status;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
/*!
 * @copydoc VoltDeviceLoadSanityCheck()
 */
FLCN_STATUS
voltDeviceLoadSanityCheck
(
    VOLT_DEVICE    *pDevice
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pDevice))
    {
        case LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_VOLT_VOLT_DEVICE_PWM);
            status = voltDeviceLoadSanityCheck_PWM(pDevice);
            break;
        default:
            status = FLCN_OK;
            break;
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltDeviceLoadSanityCheck_exit;
    }

voltDeviceLoadSanityCheck_exit:
    return status;
}
#endif

/* ------------------------- Private Functions ------------------------------ */
