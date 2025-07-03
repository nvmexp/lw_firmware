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
 * @file    voltpolicy_single_rail-mock.c
 * @brief   Mock implementations of VOLT_POLICY_SINGLE_RAIL public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "volt/objvolt.h"
#include "volt/voltpolicy.h"
#include "volt/voltpolicy_single_rail-mock.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, voltPolicyLoad_SINGLE_RAIL_MOCK, VOLT_POLICY *);
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, voltPolicySetVoltage_SINGLE_RAIL_MOCK, VOLT_POLICY *, LwU8, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *);

/* ------------------------ Private Functions ------------------------------- */
