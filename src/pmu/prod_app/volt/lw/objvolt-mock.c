/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    objvolt-mock.c
 * @brief   Mock implementations of OBJVOLT public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "volt/objvolt-mock.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
void
voltMockInit(void)
{
    RESET_FAKE(constructVolt_MOCK);
}

/* ------------------------ Mocked Functions -------------------------------- */
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, constructVolt_MOCK);

/*!
 * @brief   MOCK implementation of voltVoltSanityCheck.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref voltVoltSanityCheck_MOCK_CONFIG. See
 *          @ref voltVoltSanityCheck_IMPL() for original interface.
 *
 * @param[in]  clientID     Voltage Policy Client ID
 * @param[in]  pList        List containing target voltage for the rails
 *
 * @return      voltVoltSanityCheck_MOCK_CONFIG.status
 */
FLCN_STATUS
voltVoltSanityCheck_MOCK
(
    LwU8  clientID,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pRailList,
    LwBool bCheckState
)
{
    return voltVoltSanityCheck_MOCK_CONFIG.status;
}

/* ------------------------ Private Functions ------------------------------- */
