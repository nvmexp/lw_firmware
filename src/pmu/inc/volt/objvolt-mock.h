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
 * @file    objvolt-mock.h
 * @brief   Data required for configuring mock OBJVOLT interfaces.
 */

#ifndef OBJVOLT_MOCK_H
#define OBJVOLT_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "ut_port_types.h"
#include "objvolt.h"
#include "fff.h"

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Configuration variables for @ref voltVoltSanityCheck_MOCK.
 */
typedef struct VOLT_VOLT_SANITY_CHECK_MOCK_CONFIG
{
    /*!
     * Mock returned status.
     */
    FLCN_STATUS status;
} VOLT_VOLT_SANITY_CHECK_MOCK_CONFIG;

/* ------------------------ Structures ---------------------------- */
VOLT_VOLT_SANITY_CHECK_MOCK_CONFIG voltVoltSanityCheck_MOCK_CONFIG;

/* ------------------------- Function Prototypes ---------------------------- */
void voltMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc constructVolt
 *
 * @note    Mock implementation of @ref constructVolt
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, constructVolt_MOCK);

#endif // OBJVOLT_MOCK_H
