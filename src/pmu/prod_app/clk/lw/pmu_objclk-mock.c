/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_objclk-mock.c
 * @brief   Mock implementations of pmu_objclk public interfaces.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk-mock.h"

//! Mock version of Clk.
OBJCLK Clk;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
void
clkMockInit(void)
{
    RESET_FAKE(constructClk_MOCK)
    RESET_FAKE(clkPreInit_MOCK)
}

/* ------------------------- Mocked Functions ------------------------------- */
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, constructClk_MOCK);

DEFINE_FAKE_VOID_FUNC(clkPreInit_MOCK);

DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, clkPostInit_MOCK);

/* ------------------------- Static Functions ------------------------------- */
