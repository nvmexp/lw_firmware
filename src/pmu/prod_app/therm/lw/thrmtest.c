/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrmtest.c
 * @brief   PMU_THERM_RPC_TEST_EXELWTE related code
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_selwrity.h"
#include "therm/objtherm.h"

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Execute Therm Tests
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE
 */
FLCN_STATUS
pmuRpcThermTestExelwte
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, libThermTest)
    };
    FLCN_STATUS     status        = FLCN_OK;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_NOT_IMPLEMENTED;
    pParams->outData   = 0;

    if (FLD_TEST_DRF(_VSB, _REG0, _RM_TEST_EXELWTE, _NO, PmuVSBCache))
    {
       pParams->outStatus =
           LW2080_CTRL_THERMAL_GENERIC_TEST_INSUFFICIENT_PRIVILEDGE;
       goto pmuRpcThermTestExelwte_exit;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        switch (pParams->index)
        {
            case LW2080_CTRL_THERMAL_GENERIC_TEST_ID_INT_SENSORS:
            {
                status = thermTestIntSensors_HAL(&Therm, pParams);
                break;
            }
            case LW2080_CTRL_THERMAL_GENERIC_TEST_ID_THERMAL_SLOWDOWN:
            {
                status = thermTestThermalSlowdown_HAL(&Therm, pParams);
                break;
            }
            case LW2080_CTRL_THERMAL_GENERIC_TEST_ID_BA_SLOWDOWN:
            {
                status = thermTestBaSlowdown_HAL(&Therm, pParams);
                break;
            }
            case LW2080_CTRL_THERMAL_GENERIC_TEST_ID_HW_ADC_SLOWDOWN:
            {
                status = thermTestHwAdcSlowdown_HAL(&Therm, pParams);
                break;
            }
            case LW2080_CTRL_THERMAL_GENERIC_TEST_ID_DYNAMIC_HOTSPOT:
            {
                status = thermTestDynamicHotspot_HAL(&Therm, pParams);
                break;
            }

            case LW2080_CTRL_THERMAL_GENERIC_TEST_ID_TEMP_OVERRIDE:
            {
                status = thermTestTempOverride_HAL(&Therm, pParams);
                break;
            }

            case LW2080_CTRL_THERMAL_GENERIC_TEST_ID_THERMAL_MONITORS:
            {
                status = thermTestThermalMonitors_HAL(&Therm, pParams);
                break;
            }
            case LW2080_CTRL_THERMAL_GENERIC_TEST_ID_DEDICATED_OVERT:
            {
                status = thermTestDedicatedOvert_HAL(&Therm, pParams);
                break;
            }

            case LW2080_CTRL_THERMAL_GENERIC_TEST_ID_DEDICATED_OVERT_NEGATIVE:
            {
                status = thermTestDedicatedOvertNegativeCheck_HAL(&Therm, pParams);
                break;
            }

            case LW2080_CTRL_THERMAL_GENERIC_TEST_ID_PEAK_SLOWDOWN:
            {
                status = thermTestPeakSlowdown_HAL(&Therm, pParams);
                break;
            }

            case LW2080_CTRL_THERMAL_GENERIC_TEST_ID_GLOBAL_SNAPSHOT:
            {
                status = thermTestGlobalSnapshot_HAL(&Therm, pParams);
                break;
            }

            case LW2080_CTRL_THERMAL_GENERIC_TEST_ID_BCAST_ACCESS:
            {
                status = thermTestGpcBcastAccess_HAL(&Therm, pParams);
                break;
            }
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

pmuRpcThermTestExelwte_exit:
    return status;
}
