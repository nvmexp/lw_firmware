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
 * @file    lib_semaphore-mock.c
 * @brief   Definitions for mocked versions of functions in lib_semaphore
 */

/* ------------------------- System Includes -------------------------------- */
#include "lib_semaphore-mock.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Mocked Functions ------------------------------- */
DEFINE_FAKE_VALUE_FUNC(PSEMAPHORE_RW, osSemaphoreCreateRW_MOCK, LwU8);
