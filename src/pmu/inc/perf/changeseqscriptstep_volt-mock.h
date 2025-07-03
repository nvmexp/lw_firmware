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
 * @file    changeseqscriptstep_volt.h
 * @brief   Data required for configuring mock changeseqscriptstep_volt interfaces.
 */

#ifndef CHANGESEQSCRIPTSTEP_VOLT_MOCK_H
#define CHANGESEQSCRIPTSTEP_VOLT_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"

/* ------------------------ Function Prototypes ----------------------------- */
void perfDaemonChangeSeqScriptBuildStep_VOLTMockInit(void);
void perfDaemonChangeSeqScriptBuildStep_VOLTMockAddEntry(LwU8 entry, FLCN_STATUS status);

void perfDaemonChangeSeqScriptExelwteStep_VOLTMockInit(void);
void perfDaemonChangeSeqScriptExelwteStep_VOLTMockAddEntry(LwU8 entry, FLCN_STATUS status);

#endif // CHANGESEQSCRIPTSTEP_VOLT_MOCK_H
