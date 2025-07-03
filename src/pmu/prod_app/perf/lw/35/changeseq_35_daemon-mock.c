/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq_35_daemon-mock.c
 * @brief   Mocked versions of the CHANGE_SEQ_35 daemon interface.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "perf/changeseq.h"

/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @brief MOCK implementation of perfDaemonWaitForCompletion().
 *
 * @param[in]   pChangeSeq
 * @param[in]   pStepSuper
 *
 * @return FLCN_OK
 */
FLCN_STATUS perfDaemonChangeSeqScriptExelwteStep_35_MOCK
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    return FLCN_OK;
}
