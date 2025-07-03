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
 * @file    voltpolicy-mock.h
 * @brief   Data required for configuring mock VOLT_POLICY interfaces.
 */

#ifndef VOLTPOLICY_MOCK_H
#define VOLTPOLICY_MOCK_H

#include "fff.h"

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Configuration variables for @ref voltPolicyClientColwertToIdx_MOCK.
 */
typedef struct VOLT_POLICY_CLIENT_COLWERT_TO_IDX_MOCK_CONFIG
{
    /*!
     * Mock returned seqPolicyIndex
     */
    LwU8 seqPolicyIndex;
} VOLT_POLICY_CLIENT_COLWERT_TO_IDX_MOCK_CONFIG;

/*!
 * Configuration variables for @ref voltPolicySanityCheck_MOCK.
 */
typedef struct VOLT_POLICY_SANITY_CHECK_MOCK_CONFIG
{
    /*!
     * Mock return status
     */
    FLCN_STATUS status;
} VOLT_POLICY_SANITY_CHECK_MOCK_CONFIG;

/*!
 * Configuration variables for @ref voltPolicySetVoltage_MOCK.
 */
typedef struct VOLT_POLICY_SET_VOLTAGE_MOCK_CONFIG
{
    /*!
     * Mock return status
     */
    FLCN_STATUS status;
} VOLT_POLICY_SET_VOLTAGE_MOCK_CONFIG;

/*!
 * Configuration variables for @ref voltPoliciesDynamilwpdate_MOCK.
 */
typedef struct VOLT_POLICIES_DYNAMIC_UPDATE_MOCK_CONFIG
{
    /*!
     * Mock return status
     */
    FLCN_STATUS status;
} VOLT_POLICIES_DYNAMIC_UPDATE_MOCK_CONFIG;

/* ------------------------ Structures ---------------------------- */
VOLT_POLICY_CLIENT_COLWERT_TO_IDX_MOCK_CONFIG voltPolicyClientColwertToIdx_MOCK_CONFIG;
VOLT_POLICY_SANITY_CHECK_MOCK_CONFIG          voltPolicySanityCheck_MOCK_CONFIG;
VOLT_POLICY_SET_VOLTAGE_MOCK_CONFIG           voltPolicySetVoltage_MOCK_CONFIG;
VOLT_POLICIES_DYNAMIC_UPDATE_MOCK_CONFIG      voltPoliciesDynamilwpdate_MOCK_CONFIG;

DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, voltPolicyLoad_MOCK, VOLT_POLICY *);
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, voltPolicySetVoltageInternal_MOCK, VOLT_POLICY *, LwU8, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *);
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, voltPolicyDynamilwpdate_MOCK, VOLT_POLICY *);

#endif // VOLTPOLICY_MOCK_H
