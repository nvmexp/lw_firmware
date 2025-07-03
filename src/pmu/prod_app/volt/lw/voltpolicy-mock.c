/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    voltpolicy-mock.c
 * @brief   Mock implementations of VOLT_POLICY public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "volt/objvolt.h"
#include "volt/voltpolicy.h"
#include "volt/voltpolicy-mock.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   MOCK implementation of voltPolicyClientColwertToIdx.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref voltPolicyClientColwertToIdx_MOCK_CONFIG. See
 *          @ref voltPolicyClientColwertToIdx_IMPL() for original interface.
 *
 * @param[in]  clientID     Voltage Policy Client ID
 *
 * @return  voltPolicyClientColwertToIdx_MOCK_CONFIG.seqPolicyIndex
 */
LwU8
voltPolicyClientColwertToIdx_MOCK
(
    LwU8 clientID
)
{
    return voltPolicyClientColwertToIdx_MOCK_CONFIG.seqPolicyIndex;
}

/*!
 * @brief   MOCK implementation of voltPolicySanityCheck.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref voltPolicySanityCheck_MOCK_CONFIG. See
 *          @ref voltPolicySanityCheck_IMPL() for original interface.
 *
 * @param[in]  pPolicy  VOLT_POLICY object pointer
 * @param[in]  count    Number of items in the list
 * @param[in]  pList    List containing target voltage for the rails
 *
 * @return      voltPolicySanityCheck_MOCK_CONFIG.status
 */
FLCN_STATUS
voltPolicySanityCheck_MOCK
(
    VOLT_POLICY *pPolicy,
    LwU8         count,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pList
)
{
    return voltPolicySanityCheck_MOCK_CONFIG.status;
}

/*!
 * @brief   MOCK implementation of voltPolicySetVoltage.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref voltPolicySetVoltage_MOCK_CONFIG. See
 *          @ref voltPolicySetVoltage_IMPL() for original interface.
 *
 * @param[in]  clientID     Voltage Policy Client ID
 * @param[in]  pList        List containing target voltage for the rails
 *
 * @return      voltPolicySetVoltage_MOCK_CONFIG.status
 */
FLCN_STATUS
voltPolicySetVoltage_MOCK
(
    LwU8                                clientID,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST    *pRailList
)
{
    return voltPolicySetVoltage_MOCK_CONFIG.status;
}

/*!
 * @brief   MOCK implementation of voltPoliciesDynamilwpdate.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref voltPoliciesDynamilwpdate_MOCK_CONFIG. See
 *          @ref voltPoliciesDynamilwpdate_IMPL() for original interface.
 *
 * @return     voltPoliciesDynamilwpdate_MOCK_CONFIG.status
 */
FLCN_STATUS
voltPoliciesDynamilwpdate_MOCK(void)
{
    return voltPoliciesDynamilwpdate_MOCK_CONFIG.status;
}

DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, voltPolicyLoad_MOCK, VOLT_POLICY *);
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, voltPolicySetVoltageInternal_MOCK, VOLT_POLICY *, LwU8, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *);
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, voltPolicyDynamilwpdate_MOCK, VOLT_POLICY *);

/* ------------------------ Private Functions ------------------------------- */
