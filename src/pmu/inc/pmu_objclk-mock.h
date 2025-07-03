/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_objclk-mock.h
 * @brief   Data required for configuring mock pmu_objclk interfaces.
 */

#ifndef PMU_OBJCLK_MOCK_H
#define PMU_OBJCLK_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "ut_port_types.h"
#include "fff.h"
#include "flcnifcmn.h"
#include "pmu_objclk.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes mocking for clk
 */
void clkMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc constructClk_MOCK
 *
 * @note    Mock implementation of @ref constructClk_MOCK
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, constructClk_MOCK);
/*!
 * @copydoc clkPreInit
 *
 * @note    Mock implementation of @ref clkPreInit
 */
DECLARE_FAKE_VOID_FUNC(clkPreInit_MOCK);

DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, clkPostInit_MOCK);


#endif // PMU_OBJCLK_MOCK_H
