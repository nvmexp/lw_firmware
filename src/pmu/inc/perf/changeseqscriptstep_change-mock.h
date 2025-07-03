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
 * @file    changeseqscriptstep_change.h
 * @brief   Data required for configuring mock changeseqscriptstep_change interfaces.
 */

#ifndef CHANGESEQSCRIPTSTEP_CHANGE_MOCK_H
#define CHANGESEQSCRIPTSTEP_CHANGE_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"

/* ------------------------ Function Prototypes ----------------------------- */
void perfDaemonChangeSeqScriptBuildStep_CHANGEMockInit(void);
void perfDaemonChangeSeqScriptBuildStep_CHANGEMockAddEntry(LwU8 entry, FLCN_STATUS status);

void perfDaemonChangeSeqScriptExelwteStep_PRE_CHANGE_PMUMockInit(void);
void perfDaemonChangeSeqScriptExelwteStep_PRE_CHANGE_PMUMockAddEntry(LwU8 entry, FLCN_STATUS status);

void perfDaemonChangeSeqScriptExelwteStep_POST_CHANGE_PMUMockInit(void);
void perfDaemonChangeSeqScriptExelwteStep_POST_CHANGE_PMUMockAddEntry(LwU8 entry, FLCN_STATUS);

#endif // CHANGESEQSCRIPTSTEP_CHANGE_MOCK_H
