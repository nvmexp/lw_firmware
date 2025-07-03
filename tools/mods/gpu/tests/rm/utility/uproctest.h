/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*!
 * @file  uproctest.h
 * @brief Declare UprocTest class
 */

#ifndef _UPROCTEST_H_
#define _UPROCTEST_H_

#include "core/include/rc.h"
#include "lwtypes.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "gpu/include/notifier.h"

#define RM_UPROC_UNIT_TEST      0x0
#define RM_UPROC_UNIT_NULL      0x1

//Set to 1 to force checking for MSCG wakeup to be correct value.
//Requires ucode support, specifically [uproc]WaitMscgEngaged_HAL()
#define UPROCTEST_MSCG_TEST_STRICT 1

enum UPROC_TEST_TYPE
{
    UPROC_TEST_TYPE_BASIC_CMD_MSG_Q = 0,
    UPROC_TEST_TYPE_SWITCH,
    UPROC_TEST_TYPE_MUTEX,
    UPROC_TEST_TYPE_RTTIMER,
    UPROC_TEST_TYPE_MSCG,
    UPROC_TEST_TYPE_COMMON
};

// This enum holds various wakeup methods available for the GPU.
// The PB Activity corresponds to the method based wakeup, while REG_ACCESS
// corresponds to the register access based wake up for the MS uproc.
// We do not support non-UPROC based wakeup methods. For that, use LpwrTest.
enum UPROCRTOS_WAKE_METHOD
{
    UPROCRTOS_UPROC_REG_ACCESS,
    UPROCRTOS_UPROC_PB_ACTIVITY,
    UPROCRTOS_UPROC_FB_ACCESS,
    UPROCRTOS_WAKE_METHOD_MAX
};

class UprocTest;
class RmTest;
class UprocRtos;

typedef RC (UprocTest::*UprocTestFunc)(void);
typedef RC (UprocTest::*UprocTestFunc_args)(LwU32 argcount, ...);

typedef struct UprocTestCase_t
{
    const LwU32        test_id;
    const LwU32        va_argcount;
    UprocTestFunc      fn_noarg;
    UprocTestFunc_args fn_arg;
    const char         *name;
} UprocTestCase;

//!
//! @class UprocTest
//! @brief Uproc Testing Framework
//!
//! This class provides multiple tests for testing functionality
//! of SEC2, GSP uprocs. Will strive to be agnostic of whether
//! the underlying core is a Falcon or RISCV
//!
class UprocTest
{
public:
    UprocTest(UprocRtos *uprocRtos);
    virtual ~UprocTest()
    {
    }
    RC RunTest(LwU32 test, va_list *arg);

    RC UprocBasicCmdAndMsgQTest();
    RC UprocRtosHsSwitchTest();
    RC UprocRtosMutexTestVerifyMutexes();
    RC UprocRtosRtTimerTest(LwU32 argcount, ...);
    RC UprocRtosMscgTest(LwU32 argcount, ...);
    RC UprocRtosCommonTest(LwU32 argcount, ...);


private:
    UprocRtos *m_uprocRtos;
    const UprocTestCase *m_pLwrrentTest;

    LwU32 AcquireMutexId(void);
    RC    ReleaseMutexId(LwU32 mutexId);
    RC    ReleaseMutex(LwU32 mutexIndex);
    RC    TryMutex(LwU32 mutexIndex, LwU32 mutexId, LwBool bExpectAcquire);
    RC    HsWritePrivProtectedReg(LwU8 reg, LwU32 val);
    RC    WakeupFeature(LwU32 wakeMethod, FLOAT64 waitTimeMs);
#if UPROCTEST_MSCG_TEST_STRICT
    RC    GetMscgWakeupReason(LwU32 *wakeupReason);
#endif
    RC    RmCtrlPwrGateCmd(LW2080_CTRL_POWERGATING_PARAMETER *pPGparams);
};

#endif
