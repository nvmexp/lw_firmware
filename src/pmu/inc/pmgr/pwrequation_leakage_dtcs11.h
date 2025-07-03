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
 * @file  pwrequation_leakage_dtcs11.h
 * @brief @copydoc pwrequation_leakage_dtcs11.c
 */

#ifndef PWREQUATION_LEAKAGE_DTCS11_H
#define PWREQUATION_LEAKAGE_DTCS11_H

/* ------------------------- System Includes -------------------------------- */
#include "pmgr/pwrequation_leakage.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
typedef struct PWR_EQUATION_LEAKAGE_DTCS11 PWR_EQUATION_LEAKAGE_DTCS11;

/*!
 * Structure representing a DTCS 1.1 leakage equation entry.
 */
struct PWR_EQUATION_LEAKAGE_DTCS11
{
    /*!
     * PWR_EQUATION_LEAKAGE super class.
     */
    PWR_EQUATION_LEAKAGE super;

    /*!
     * Unsigned integer k0 coefficient.  Units of mV.
     */
    LwU32                k0;
    /*!
     * Unsigned FXP 20.12 value.  Unitless.
     */
    LwUFXP20_12          k1;
    /*!
     * Unsigned FXP 20.12 value.  Unitless.
     */
    LwUFXP20_12          k2;
    /*!
     * Signed FXP 24.8 value.  Units of C.
     */
    LwTemp               k3;
};

/* ------------------------- Macros ------------------------------------------*/
/* ------------------------- Function Prototypes  --------------------------- */
/*!
 * @interface PWR_EQUATION_LEAKAGE_DTCS11
 *
 * The actual power leakage evaluation function. Taking additional IDDQ and Tj
 * value as parametes.
 *
 * @param[in]     pDtcs11         Pointer to PWR_EQUATION_LEAKAGE_DTCS11 object
 * @param[in]     voltageuV       Voltage (in uV) for equation callwlation
 * @param[in]     iddqmA          IDDQ fuse value
 * @param[in]     tj              Tj value
 * @param[in/out] pLeakageVal     Leakage current in mA
 *
 * @return FLCN_OK
 *      successfully got the leakage current.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define PwrEquationLeakageDtcs11EvaluatemA(fname) FLCN_STATUS (fname) (PWR_EQUATION_LEAKAGE_DTCS11 *pDtcs11, LwU32 voltageuV, LwU32 iddqmA, LwTemp tj, LwU32 *pLeakageVal)

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * PWR_EQUATION interfaces
 */
BoardObjGrpIfaceModel10ObjSet (pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS11)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS11");

/*!
 * PWR_EQUATION_LEAKAGE interfaces
 */
PwrEquationLeakageEvaluatemA (pwrEquationLeakageEvaluatemA_DTCS11)
    GCC_ATTRIB_SECTION("imem_libLeakage", "pwrEquationLeakageEvaluatemA_DTCS11")
    GCC_ATTRIB_NOINLINE();

/*!
 * PWR_EQUATION_LEAKAGE_DTCS11 interfaces
 */
PwrEquationLeakageDtcs11EvaluatemA  (pwrEquationLeakageDtcs11EvaluatemA)
    GCC_ATTRIB_SECTION("imem_libLeakage", "pwrEquationLeakageDtcs11EvaluatemA");

/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes --------------------------- */

#endif // PWREQUATION_LEAKAGE_DTCS11_H
