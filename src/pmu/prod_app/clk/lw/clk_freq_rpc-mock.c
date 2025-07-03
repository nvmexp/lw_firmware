/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "clk/clk_freq_rpc-mock.h"

/* ------------------------ Public Functions -------------------------------- */
/*!
 * Mock implementation of pmuRpcProcessUnitClkFreq_IMPL
 */
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, pmuRpcProcessUnitClkFreq_MOCK, DISP2UNIT_RM_RPC *);
