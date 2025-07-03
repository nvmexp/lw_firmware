/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    volttest.c
 * @brief   PMU_VOLT_RPC_TEST_EXELWTE related code
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_selwrity.h"
#include "volt/objvolt.h"

#include "config/g_volt_hal.h"
#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Execute Volt Tests
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_VOLT_TEST_EXELWTE
 */
FLCN_STATUS
pmuRpcVoltTestExelwte
(
    RM_PMU_RPC_STRUCT_VOLT_TEST_EXELWTE *pParams
)
{
    FLCN_STATUS status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, libVoltTest)
        };
    pParams->outStatus = LW2080_CTRL_VOLT_GENERIC_TEST_NOT_IMPLEMENTED;
    pParams->outData   = 0;

    if (FLD_TEST_DRF(_VSB, _REG0, _RM_TEST_EXELWTE, _NO, PmuVSBCache))
    {
       pParams->outStatus =
           LW2080_CTRL_VOLT_GENERIC_TEST_INSUFFICIENT_PRIVILEDGE;
       goto pmuRpcVoltTestExelwte_exit;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        switch (pParams->index)
        {
            case LW2080_CTRL_VOLT_GENERIC_TEST_ID_VMIN_CAP:
            {
                status = voltTestVminCap_HAL(&Volt, pParams);
                break;
            }
            case LW2080_CTRL_VOLT_GENERIC_TEST_ID_VMIN_CAP_NEGATIVE:
            {
                voltTestVminCapNegativeCheck_HAL(&Volt, pParams);
                break;
            }
            case LW2080_CTRL_VOLT_GENERIC_TEST_ID_DROOPY_ENGAGE:
            {
                voltTestDroopyEngage_HAL(&Volt, pParams);
                break;
            }
            case LW2080_CTRL_VOLT_GENERIC_TEST_ID_EDPP_THERM_EVENTS:
            {
                voltTestEDPpThermEvents_HAL(&Volt, pParams);
                break;
            }
            case LW2080_CTRL_VOLT_GENERIC_TEST_ID_THERM_MON_EDPP:
            {
                voltTestThermMonitorsOverEDPpEvents_HAL(&Volt, pParams);
                break;
            }
            case LW2080_CTRL_VOLT_GENERIC_TEST_ID_FIXED_SLEW_RATE:
            {
                voltTestFixedSlewRate_HAL(&Volt, pParams);
                break;
            }
            case LW2080_CTRL_VOLT_GENERIC_TEST_ID_FORCE_VMIN:
            {
                voltTestForceVmin_HAL(&Volt, pParams);
                break;
            }
            case LW2080_CTRL_VOLT_GENERIC_TEST_ID_VID_PWM_BOUND_FLOOR:
            {
                voltTestVidPwmBoundFloor_HAL(&Volt, pParams);
                break;
            }
            case LW2080_CTRL_VOLT_GENERIC_TEST_ID_VID_PWM_BOUND_CEIL:
            {
                voltTestVidPwmBoundCeil_HAL(&Volt, pParams);
                break;
            }
            case LW2080_CTRL_VOLT_GENERIC_TEST_ID_POSITIVE_CLVC_OFFSET:
            {
                voltTestPositiveClvcOffset_HAL(&Volt, pParams);
                break;
            }
            case LW2080_CTRL_VOLT_GENERIC_TEST_ID_NEGATIVE_CLVC_OFFSET:
            {
                voltTestNegativeClvcOffset_HAL(&Volt, pParams);
                break;
            }
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

pmuRpcVoltTestExelwte_exit:
    return status;
}
