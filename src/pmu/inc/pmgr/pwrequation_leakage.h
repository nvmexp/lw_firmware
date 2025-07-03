/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrequation_leakage.h
 * @brief @copydoc pwrequation_leakage.c
 */

#ifndef PWREQUATION_LEAKAGE_H
#define PWREQUATION_LEAKAGE_H

/* ------------------------- System Includes -------------------------------- */
#include "pmgr/pwrequation.h"
#include "pmgr/lib_pmgr.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
typedef struct PWR_EQUATION_LEAKAGE PWR_EQUATION_LEAKAGE;

/*!
 * Structure representing a LEAkAGE class.
 */
struct PWR_EQUATION_LEAKAGE
{
    /*!
     * PWR_EQUATION super class.
     */
    PWR_EQUATION super;

    /*!
     * Floor-sweeping efficiency value by which to scale leakage power equations
     * for the amount of leakage power eliminated by floorswept area.
     *
     * Unsigned FXP 4.12 value.  Unitless.
     */
    LwUFXP4_12   fsEff;
    /*!
     * Power-gating efficiency value by which to scale leakage power equations
     * for the amount of leakage power eliminated while power-gating is engaged.
     *
     * Unsigned FXP 4.12 value.  Unitless.
     *
     * @deprecated Pre GA10x does not use this variable (0 residency is assumed)
     *             GA10x and onwards we have shifted to @ref PMGR_LPWR_RESIDENCIES
     */
    LwUFXP4_12   pgEff;
};

/*!
 * @interface VOLT_RAIL
 *
 * For given voltage, get the leakage power/current.
 *
 * @param[in]   pEquation       PWR_EQUATION object pointer
 * @param[in]   voltageuV       Voltage value to be used to get the leakage power
 * @param[in]   bIsUnitmA       The required unit of leakage power (mW = LW_FALSE)
 * @param[in]   pgRes           Power gating residency - ratio [0,1]
 * @param[out]  pLeakageVal     Leakage power in mW/ current in mA
 *
 * @return FLCN_OK
 *      successfully got the leakage power/current.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define PwrEquationGetLeakage(fname) FLCN_STATUS (fname)(PWR_EQUATION *pEquation, LwU32 voltageuV, LwBool bIsUnitmA, PMGR_LPWR_RESIDENCIES *pPgRes, LwU32 *pLeakageVal)

/*!
 * @interface PWR_EQUATION_LEAKAGE
 *
 * Evaluates leakage current using PWR_EQUATION_LEAKAGE entry.
 *
 * @param[in]      pLeakage        Pointer to PWR_EQUATION_LEAKAGE object
 * @param[in]      voltageuV       Voltage (in uV) for equation callwlation
 * @param[in]      pgRes           Power Gating residency - ratio [0,1]
 * @param[in/out]  pLeakageVal     Leakage current in mA
 *
 * @return FLCN_OK
 *      successfully got the leakage current.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define PwrEquationLeakageEvaluatemA(fname) FLCN_STATUS (fname)(PWR_EQUATION_LEAKAGE *pLeakage, LwU32 voltageuV, PMGR_LPWR_RESIDENCIES *pPgRes, LwU32 *pLeakageVal)

/*!
 * @interface PWR_EQUATION_LEAKAGE
 *
 * Evaluates leakage power using PWR_EQUATION_LEAKAGE entry.
 *
 * @param[in]      pLeakage        Pointer to PWR_EQUATION_LEAKAGE object
 * @param[in]      voltageuV       Voltage (in uV) for equation callwlation
 * @param[in]      pgRes           Power Gating residency - ratio [0,1]
 * @param[in/out]  pLeakageVal     Leakage power in mW
 *
 * @return FLCN_OK
 *      successfully got the leakage power.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define PwrEquationLeakageEvaluatemW(fname) FLCN_STATUS (fname)(PWR_EQUATION_LEAKAGE *pLeakage, LwU32 voltageuV, PMGR_LPWR_RESIDENCIES *pPgRes, LwU32 *pLeakageVal)

/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Function Prototypes  --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * PWR_EQUATION_LEAKAGE interfaces
 */
PwrEquationLeakageEvaluatemA    (pwrEquationLeakageEvaluatemA_CORE)
    GCC_ATTRIB_SECTION("imem_libLeakage", "pwrEquationLeakageEvaluatemA_CORE")
    GCC_ATTRIB_NOINLINE();
PwrEquationGetLeakage           (pwrEquationGetLeakage)
    GCC_ATTRIB_SECTION("imem_libLeakage", "pwrEquationGetLeakage");
PwrEquationLeakageEvaluatemA    (pwrEquationLeakageEvaluatemA)
    GCC_ATTRIB_SECTION("imem_libLeakage", "pwrEquationLeakageEvaluatemA");
PwrEquationLeakageEvaluatemW    (pwrEquationLeakageEvaluatemW)
    GCC_ATTRIB_SECTION("imem_libLeakage", "pwrEquationLeakageEvaluatemW");

/*!
 * PWR_EQUATION interfaces
 */
BoardObjGrpIfaceModel10ObjSet (pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE");

/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes --------------------------- */

#include "pmgr/pwrequation_leakage_dtcs11.h"
#include "pmgr/pwrequation_leakage_dtcs12.h"
#include "pmgr/pwrequation_leakage_dtcs13.h"

#endif // PWREQUATION_LEAKAGE_H
