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
 * @file    clk_clockmon-mock.h
 * @brief   Data required for configuring mock clk_clockmon interfaces.
 */

#ifndef CLK_CLOCKMON_MOCK_H
#define CLK_CLOCKMON_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"

/* ------------------------ Function Prototypes ----------------------------- */
void clkClockMonsThresholdEvaluateMockInit(void);
void clkClockMonsThresholdEvaluateMockAddEntry(LwU8 entry, FLCN_STATUS status);

#endif // CLK_CLOCKMON_MOCK_H
