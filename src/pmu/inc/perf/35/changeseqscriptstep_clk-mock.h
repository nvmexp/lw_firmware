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
 * @file    changeseqscriptstep_clk.h
 * @brief   Data required for configuring mock changeseqscriptstep_clk interfaces.
 */

#ifndef CHANGESEQSCRIPTSTEP_CLK_MOCK_H
#define CHANGESEQSCRIPTSTEP_CLK_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"

/* ------------------------ Function Prototypes ----------------------------- */
void perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKSMockInit(void);
void perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKSMockAddEntry(LwU8 entry, FLCN_STATUS status);

void perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKSMockInit(void);
void perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKSMockAddEntry(LwU8 entry, FLCN_STATUS status);

void perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKSMockInit(void);
void perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKSMockAddEntry(LwU8 entry, FLCN_STATUS status);

void perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKSMockInit(void);
void perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKSMockAddEntry(LwU8 entry, FLCN_STATUS status);

#endif // CHANGESEQSCRIPTSTEP_CLK_MOCK_H
