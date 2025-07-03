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
 * @file    pstate_35-mock.h
 * @brief   Data required for configuring mock PSTATE_35 interfaces.
 */

#ifndef PSTATE_MOCK_H
#define PSTATE_MOCK_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
void perfPstateClkFreqGet35MockInit();
void perfPstateClkFreqGet35MockAddEntry(LwU8 entry, FLCN_STATUS status);
LwU8 perfPstateClkFreqGet35MockNumCalls();

#endif // PSTATE_MOCK_H
