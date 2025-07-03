/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq-mock.h
 * @brief   Data required for configuring mock changeseq interfaces.
 */

#ifndef CHANGESEQ_DAEMON_MOCK_H
#define CHANGESEQ_DAEMON_MOCK_H

#include "perf/changeseq_daemon.h"
#include "fff.h"

DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, perfDaemonChangeSeqRead_STEP_MOCK, CHANGE_SEQ *, CHANGE_SEQ_SCRIPT *, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER **, LwU8, LwBool);
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, perfDaemonChangeSeqFlush_STEP_MOCK, CHANGE_SEQ *, CHANGE_SEQ_SCRIPT *, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER *, LwU8, LwBool);
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, perfDaemonChangeSeqFlush_CHANGE_MOCK, CHANGE_SEQ *, CHANGE_SEQ_SCRIPT *);
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, perfDaemonChangeSeqValidateChange_MOCK, CHANGE_SEQ *, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *);
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, perfDaemonChangeSeqScriptStepInsert_MOCK, CHANGE_SEQ *, CHANGE_SEQ_SCRIPT *, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER *);

#endif // CHANGESEQ_DAEMON_MOCK_H
