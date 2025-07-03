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
 * @file  voltdev_vid.c
 * @brief Volt Device Model Specific to VID
 *
 * This module is a collection of functions managing and manipulating state
 * related to the Wide VID voltage devices which controls/regulates
 * the voltage on an output power rail (e.g. LWVDD).
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "volt/voltdev.h"
#include "lib/lib_gpio.h"

/* ------------------------ Static Function Prototypes ---------------------- */

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a VOLT_DEVICE_VID object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
voltDeviceGrpIfaceModel10ObjSetImpl_VID
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_VOLT_VOLT_DEVICE_VID_BOARDOBJ_SET   *pSet    =
        (RM_PMU_VOLT_VOLT_DEVICE_VID_BOARDOBJ_SET *)pBoardObjDesc;
    VOLT_DEVICE_VID                            *pDev;
    FLCN_STATUS                                 status;
    LwU8                                        i;

    status = voltDeviceGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDev = (VOLT_DEVICE_VID *)*ppBoardObj;

    // Set member variables.
    pDev->voltageBaseuV         = pSet->voltageBaseuV;
    pDev->voltageOffsetScaleuV  = pSet->voltageOffsetScaleuV;
    pDev->vselMask              = pSet->vselMask;
    for (i = 0; i < LW2080_CTRL_VOLT_VOLT_DEV_VID_VSEL_MAX_ENTRIES; i++)
    {
        pDev->gpioPin[i] = pSet->gpioPin[i];
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * Class implementation.
 *
 * @copydoc VoltDeviceGetVoltage
 */
FLCN_STATUS
voltDeviceGetVoltage_VID
(
    VOLT_DEVICE    *pDevice,
    LwU32          *pVoltageuV
)
{
    VOLT_DEVICE_VID    *pDev        = (VOLT_DEVICE_VID *)pDevice;
    FLCN_STATUS         status      = FLCN_ERR_ILWALID_STATE;
    LwU8                lwrrentVsel = 0;
    LwBool              bState;
    LwU8                mask;
    LwU8                i;

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libGpio));

    // Build current VSEL mask by checking status of all VSEL GPIOs.
    for (i = 0; i < LW2080_CTRL_VOLT_VOLT_DEV_VID_VSEL_MAX_ENTRIES; i++)
    {
        mask = BIT(i);

        if ((pDev->vselMask & mask) != 0)
        {
            status = gpioGetState(pDev->gpioPin[i], &bState);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltDeviceGetVoltage_VID_exit;
            }

            // Set the VSEL bit only if current state of the GPIO is ON.
            if (bState)
            {
                lwrrentVsel |= mask;
            }
        }
    }

    // TargetVoltage = BaseVoltage + VID * OffsetScale.
    *pVoltageuV = (LwU32)(
                          (pDev->voltageBaseuV) +
                          (((LwS32)(LwU32)lwrrentVsel) * pDev->voltageOffsetScaleuV)
                         );

voltDeviceGetVoltage_VID_exit:
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libGpio));

    return status;
}

/*!
 * Class implementation.
 *
 * @copydoc VoltDeviceSetVoltage
 */
FLCN_STATUS
voltDeviceSetVoltage_VID
(
    VOLT_DEVICE    *pDevice,
    LwU32           voltageuV,
    LwBool          bTrigger,
    LwBool          bWait,
    LwU8            railAction
)
{
    VOLT_DEVICE_VID    *pDev    = (VOLT_DEVICE_VID *)pDevice;
    FLCN_STATUS         status  = FLCN_OK;
    LwU8                j       = 0;
    GPIO_LIST_ITEM      vsel[LW2080_CTRL_VOLT_VOLT_DEV_VID_VSEL_MAX_ENTRIES];
    LwU8                targetVsel;
    LwU8                mask;
    LwU8                i;

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libGpio));

    // Callwlate target VSEL for the requested voltage.
    targetVsel = (LwU8)(((LwS32)voltageuV - pDev->voltageBaseuV) /
                        (pDev->voltageOffsetScaleuV));

    // Build target GPIO state i.e. ON/OFF for all the VSEL GPIOs.
    for (i = 0; i < LW2080_CTRL_VOLT_VOLT_DEV_VID_VSEL_MAX_ENTRIES; i++)
    {
        mask = BIT(i);

        if ((pDev->vselMask & mask) != 0)
        {
            vsel[j].bState  = ((targetVsel & mask) != 0);
            vsel[j].gpioPin = pDev->gpioPin[i];
            j++;
        }
    }

    // Actually set the VSEL GPIOs.
    status = gpioSetStateList(j, vsel, bTrigger);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltDeviceSetVoltage_VID_exit;
    }

    // Call super-class implementation.
    status = voltDeviceSetVoltage_SUPER(pDevice, voltageuV, bTrigger, bWait, railAction);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltDeviceSetVoltage_VID_exit;
    }

voltDeviceSetVoltage_VID_exit:
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libGpio));

    return status;
}
