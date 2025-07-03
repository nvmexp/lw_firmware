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
 * @file    changeseqscriptstep_lpwr.h
 * @brief   Data required for configuring mock changeseqscriptstep_lpwr interfaces.
 */

#ifndef CHANGESEQSCRIPTSTEP_LPWR_MOCK_H
#define CHANGESEQSCRIPTSTEP_LPWR_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"

/* ------------------------ Function Prototypes ----------------------------- */
void perfDaemonChangeSeqScriptBuildStep_LPWRMockInit(void);
void perfDaemonChangeSeqScriptBuildStep_LPWRMockAddEntry(LwU8 entry, FLCN_STATUS status);

void perfDaemonChangeSeqScriptExelwteStep_LPWRMockInit(void);
void perfDaemonChangeSeqScriptExelwteStep_LPWRMockAddEntry(LwU8 entry, FLCN_STATUS status);

#endif // CHANGESEQSCRIPTSTEP_LPWR_MOCK_H
