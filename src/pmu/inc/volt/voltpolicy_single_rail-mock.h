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
 * @file    voltpolicy_single_rail-mock.h
 * @brief   Data required for configuring mock VOLT_POLICY_SINGLE_RAIL interfaces.
 */

#ifndef VOLTPOLICY_SINGLE_RAIL_MOCK_H
#define VOLTPOLICY_SINGLE_RAIL_MOCK_H

#include "fff.h"

/* ------------------------ Mock function Definitions ----------------------- */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, voltPolicyLoad_SINGLE_RAIL_MOCK, VOLT_POLICY *);
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, voltPolicySetVoltage_SINGLE_RAIL_MOCK, VOLT_POLICY *, LwU8, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *);

#endif // VOLTPOLICY_MOCK_H
