/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef OBJTHERM_MOCK_H
#define OBJTHERM_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "fff.h"
#include "objtherm.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initilizes therm mocking.
 */
void thermMockInit(void);

/*!
 * @copydoc constructTherm
 *
 * @note    Mock implementation of @ref constructTherm
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, constructTherm_MOCK);

#endif // OBJTHERM_MOCK_H
