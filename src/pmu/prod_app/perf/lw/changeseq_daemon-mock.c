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
 * @file    changeseq-mock.c
 * @brief   Mock implementations of changeseq public interfaces.
 */

#include "perf/changeseq_daemon-mock.h"

DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, perfDaemonChangeSeqRead_STEP_MOCK, CHANGE_SEQ *, CHANGE_SEQ_SCRIPT *, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER **, LwU8, LwBool);
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, perfDaemonChangeSeqFlush_STEP_MOCK, CHANGE_SEQ *, CHANGE_SEQ_SCRIPT *, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER *, LwU8, LwBool);
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, perfDaemonChangeSeqFlush_CHANGE_MOCK, CHANGE_SEQ *, CHANGE_SEQ_SCRIPT *);
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, perfDaemonChangeSeqValidateChange_MOCK, CHANGE_SEQ *, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *);
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, perfDaemonChangeSeqScriptStepInsert_MOCK, CHANGE_SEQ *, CHANGE_SEQ_SCRIPT *, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER *);
