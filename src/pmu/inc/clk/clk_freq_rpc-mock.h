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
 * @file    clk_freq_rpc-mock.h
 * @brief   mocking interfaces for clk_freq_rpc-mock.c
 */

#ifndef CLK_FREQ_RPC_MOCK_H
#define CLK_FREQ_RPC_MOCK_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "g_pmurpc.h"
#include "fff.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, pmuRpcProcessUnitClkFreq_MOCK, DISP2UNIT_RM_RPC *);

#endif // CLK_FREQ_RPC_MOCK_H
