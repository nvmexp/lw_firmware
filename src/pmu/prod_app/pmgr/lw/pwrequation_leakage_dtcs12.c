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
 * @file   pwrequation_leakage_dtcs12.c
 * @brief  PMGR Power Equation Leakage DTCS 1.2 Model Management
 *
 * This module is a collection of functions managing and manipulating state
 * related to Power Equation Leakage DTCS 1.2 objects in the Power Equation Table.
 *
 * https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Equation_Table_1.X
 *
 * The DTCS 1.2 Power Leakage Equation type is an equation specified by the
 * Desktop Chip Soltuions Team.  This equation uses the following independent
 * variables:
 *     IDDQ - Quiescent Current (mA).  Unsigned 32-bit integer.
 *     Vset - Voltage (mV).  Unsigned 32-bit integer.
 *     tj   - Temperature (C). Signed FXP 24.8 value.
 *
 * The DTCS 1.2 Power Leakage Equation is same as DTCS1.1. The only difference is
 * DTCS 1.2 reads IDDQ value from VFE table and Tj value from Therm Channel.
 *
 * Each equation entry specifies the following set of coefficients:
 *     k0 - Unsigned integer value.
 *     K1 - Unsigned FXP 20.12 value.
 *     k2 - Unsigned FXP 20.12 value.
 *     k3 - Signed FXP 24.8 value.
 *     iddqVfeIdx - unsigned 8 bit value.
 *     tjThermChIdx - unsigned 8 bit value.
 *
 * With these values the DTCS 1.2 equation class's power can be callwlated as:
 *     Pclass = Vset * IDDQ * (Vset / k0) ^ (k1) * k2 ^ (tj + k3)
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_fxp.h"
#include "lib_lwf32.h"
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmu_objperf.h"
#include "therm/objtherm.h"
#include "perf/3x/vfe.h"
#include "pmgr/pwrequation_leakage_dtcs12.h"

/* ------------------------- Type Definitions ------------------------------- */
static FLCN_STATUS   s_pwrEquationLeakageDtcs12GetIddq(PWR_EQUATION_LEAKAGE_DTCS12 *pDtcs12)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "s_pwrEquationLeakageDtcs12GetIddq");

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * DTCS 1.2 implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS12
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS12_BOARDOBJ_SET    *pDtcs12Desc =
        (RM_PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS12_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_EQUATION_LEAKAGE_DTCS12                             *pDtcs12;
    FLCN_STATUS                                              status;

    // Construct and initialize parent-class object.
    status = pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS11(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDtcs12 = (PWR_EQUATION_LEAKAGE_DTCS12 *)*ppBoardObj;

    // Set the class-specific values:
    pDtcs12->iddqVfeIdx = pDtcs12Desc->iddqVfeIdx;
    pDtcs12->tjThermChIdx = pDtcs12Desc->tjThermChIdx;

    // Get the IDDQ value.
    status = s_pwrEquationLeakageDtcs12GetIddq(pDtcs12);

    return status;
}

/*!
 * DTCS 1.2 implementation.
 *
 * @copydoc PwrEquationLeakageEvaluatemA
 */
FLCN_STATUS
pwrEquationLeakageEvaluatemA_DTCS12
(
    PWR_EQUATION_LEAKAGE   *pLeakage,
    LwU32                   voltageuV,
    PMGR_LPWR_RESIDENCIES  *pPgRes,
    LwU32                  *pLeakageVal
)
{
    LwTemp                          tj;
    THERM_CHANNEL                  *pThermChannel;
    PWR_EQUATION_LEAKAGE_DTCS12    *pDtcs12 =
        (PWR_EQUATION_LEAKAGE_DTCS12 *)pLeakage;
    FLCN_STATUS                     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_THERM_CHANNEL
    };

    // Get Tj
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        pThermChannel = THERM_CHANNEL_GET(pDtcs12->tjThermChIdx);
        if (pThermChannel == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto pwrEquationLeakageEvaluatemA_DTCS12_detach;
        }

        status = thermChannelTempValueGet(pThermChannel, &tj);

pwrEquationLeakageEvaluatemA_DTCS12_detach:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrEquationLeakageEvaluatemA_DTCS12_exit;
    }

    status = pwrEquationLeakageDtcs11EvaluatemA(
            (PWR_EQUATION_LEAKAGE_DTCS11 *)pLeakage, voltageuV,
                pDtcs12->iddqmA, tj, pLeakageVal);

pwrEquationLeakageEvaluatemA_DTCS12_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS
s_pwrEquationLeakageDtcs12GetIddq
(
    PWR_EQUATION_LEAKAGE_DTCS12 *pDtcs12
)
{
    LwF32           iddqmAFloat;
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
#ifdef DMEM_VA_SUPPORTED
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
#endif
        OSTASK_OVL_DESC_DEFINE_VFE(FULL)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Callwlate the IDDQ for later use (requires PERF DMEM read semaphore).
        perfReadSemaphoreTake();
        {
            status = vfeVarEvaluate(pDtcs12->iddqVfeIdx, &iddqmAFloat);
        }
        perfReadSemaphoreGive();

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto _pwrEquationLeakageDtcs12Load_done;
        }

        // Colwert floating point IDDQ value to unsigned 32bit integer.
        pDtcs12->iddqmA = lwF32ColwertToU32(iddqmAFloat);

_pwrEquationLeakageDtcs12Load_done:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}
