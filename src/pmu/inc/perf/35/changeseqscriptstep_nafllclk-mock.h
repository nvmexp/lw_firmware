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
 * @file    changeseqscriptstep_nafllclk-mock.h
 * @brief   Data required for configuring mock changeseqscriptstep_nafllclk interfaces.
 */

#ifndef CHANGESEQSCRIPTSTEP_NAFLLCLK_MOCK_H
#define CHANGESEQSCRIPTSTEP_NAFLLCLK_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"

/* ------------------------ Function Prototypes ----------------------------- */
void perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKSMockInit(void);
void perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKSMockAddEntry(LwU8 entry, FLCN_STATUS status);

#endif // CHANGESEQSCRIPTSTEP_NAFLLCLK_MOCK_H
