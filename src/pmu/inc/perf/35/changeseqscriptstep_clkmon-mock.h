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
 * @file    changeseqscriptstep_clkmon.h
 * @brief   Data required for configuring mock changeseqscriptstep_clkmon interfaces.
 */

#ifndef CHANGESEQSCRIPTSTEP_CLKMON_MOCK_H
#define CHANGESEQSCRIPTSTEP_CLKMON_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"

/* ------------------------ Function Prototypes ----------------------------- */
void perfDaemonChangeSeq35ScriptBuildStep_CLK_MONMockInit(void);
void perfDaemonChangeSeq35ScriptBuildStep_CLK_MONMockAddEntry(LwU8 entry, FLCN_STATUS status);

void perfDaemonChangeSeq35ScriptExelwteStep_CLK_MONMockInit(void);
void perfDaemonChangeSeq35ScriptExelwteStep_CLK_MONMockAddEntry(LwU8 entry, FLCN_STATUS status);

#endif // CHANGESEQSCRIPTSTEP_VOLT_MOCK_H
