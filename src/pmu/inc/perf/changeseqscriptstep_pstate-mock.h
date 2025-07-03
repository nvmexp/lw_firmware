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
 * @file    changeseqscriptstep_pstate-mock.h
 * @brief   Data required for configuring mock changeseqscriptstep_pstate interfaces.
 */

#ifndef CHANGESEQSCRIPTSTEP_PSTATE_MOCK_H
#define CHANGESEQSCRIPTSTEP_PSTATE_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"

/* ------------------------ Function Prototypes ----------------------------- */
void perfDaemonChangeSeqScriptBuildStep_PSTATEMockInit(void);
void perfDaemonChangeSeqScriptBuildStep_PSTATEMockAddEntry(LwU8 entry, FLCN_STATUS status);
LwU8 perfDaemonChangeSeqScriptBuildStep_PSTATEMockNumCalls(void);

void perfDaemonChangeSeqScriptExelwteStep_PRE_PSTATE_PMUMockInit(void);
void perfDaemonChangeSeqScriptExelwteStep_PRE_PSTATE_PMUMockAddEntry(LwU8 entry, FLCN_STATUS status);

void perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMUMockInit(void);
void perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMUMockAddEntry(LwU8 entry, FLCN_STATUS status);

#endif // CHANGESEQSCRIPTSTEP_PSTATE_MOCK_H
