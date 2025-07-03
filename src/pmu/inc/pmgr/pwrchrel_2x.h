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
 * @file  pwrchannel_2x.h
 * @brief @copydoc pwrchannel_2x.c
 */

#ifndef PWRCHREL_2X_H
#define PWRCHREL_2X_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchrel.h"

/* ------------------------- Macros ---------------------------------------- */
/*!
 * Wrapper macro to expand to boardObjIfaceModel10GetStatus(). Remove this when an actual
 * implementation is done for pwrChRelationshipIfaceModel10GetStatus_SUPER()
 */
#define pwrChRelationshipIfaceModel10GetStatus_SUPER(pModel10, pPayload)                 \
    boardObjIfaceModel10GetStatus(pModel10, pPayload)

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * This structure is used to return dynamic state related to a PWR_CHRELATIONSHIP.
 */
typedef struct
{
    /*!
     * The power (mW) of the PWR_CHRELATIONSHIP entry for the current iteration of sampling.
     */
    LwS32   pwrmW;

    /*!
     * The current (mA) of the PWR_CHRELATIONSHIP entry for the current iteration of sampling.
     */
    LwS32   lwrrmA;

    /*!
     * The voltage (uV) of the PWR_CHRELATIONSHIP entry for the current iteration of sampling.
     */
    LwS32   voltuV;
} PWR_CHRELATIONSHIP_TUPLE_STATUS;

/*!
 * @interface PWR_CHRELATIONSHIP
 *
 * Evaluates a PWR_CHRELATIONSHIP entry to retrieve the power, current and
 * voltage tuple associated with the relationship. Most implementations will
 * retrieve the power, current and voltage associated with the channel pointed
 * at by chIdx and then perform some callwlation/operation on that value.
 *
 * @param[in]   pChRel      Pointer to PWR_CHRELATIONSHIP object
 * @param[in]   chIdxEval
 *     Index of PWR_CHANNEL object which has requested evaluation of this
 *     PWR_CHRELATIONSHIP object.
 * @param[out]  pStatus     @ref PWR_CHRELATIONSHIP_TUPLE_STATUS
 *
 * @return FLCN_OK
 *      Tuple successfully obtained.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      This operation is not supported on given PWR_CHRELATIONSHIP.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define PwrChRelationshipTupleEvaluate(fname) FLCN_STATUS (fname)(PWR_CHRELATIONSHIP *pChRel, LwU8 chIdxEval, PWR_CHRELATIONSHIP_TUPLE_STATUS *pStatus)

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Inline Functions ------------------------------ */
/*!
 * Copy Tuple values from a @ref PWR_CHRELATIONSHIP_TUPLE_STATUS object into a
 * @ref LW2080_CTRL_PMGR_PWR_TUPLE object.
 *
 * @param[in]  pTuple   LW2080_CTRL_PMGR_PWR_TUPLE pointer
 * @param[in]  pStatus  PWR_CHRELATIONSHIP_TUPLE_STATUS pointer
 *
 * @return FLCN_OK
 *      Tuple values have been successfully copied.
 * @return Other unexpected errors
 *      An error oclwred while copying tuple values.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrChRelationshipTupleCopy
(
    LW2080_CTRL_PMGR_PWR_TUPLE      *pTuple,
    PWR_CHRELATIONSHIP_TUPLE_STATUS *pStatus
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pTuple != NULL) && (pStatus != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrChRelationshipTupleCopy_exit);

    pTuple->pwrmW  = (LwU32)LW_MAX(0, pStatus->pwrmW);
    pTuple->voltuV = (LwU32)LW_MAX(0, pStatus->voltuV);
    pTuple->lwrrmA = (LwU32)LW_MAX(0, pStatus->lwrrmA);

pwrChRelationshipTupleCopy_exit:
    return status;
}

/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_CHRELATIONSHIP interfaces
 */
PwrChRelationshipTupleEvaluate (pwrChRelationshipTupleEvaluate)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChRelationshipTupleEvaluate");
BoardObjIfaceModel10GetStatus                  (pwrChRelationshipIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrChRelationshipIfaceModel10GetStatus");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PWRCHREL_2X_H
