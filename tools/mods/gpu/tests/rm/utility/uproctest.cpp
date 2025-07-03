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

#include "uproctest.h"
#include "mods_reg_hal.h"
#include "gpu/reghal/reghal.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rmtest.h"
#include "class/cl902d.h"                            // for FERMI_TWOD_A
#include "pascal/gp102/dev_graphics_nobundle.h"      // for LW_PGRAPH_PRI_SCC_DEBUG reg-access.
#include "class/cla06fsubch.h"                       // for LWA06F_SUBCHANNEL_2D
#include "uprociftest.h"
#include "uprocifcmn.h"
#include "uprocrtos.h"

#define GET_ARRAY_SIZE(x)    (sizeof(x)/sizeof(x[0]))

//! Test Mutex IDs, Indexes, and Ranges
#define UPROC_MUTEX_ID_RM_RANGE_START (8)
#define UPROC_MUTEX_ID_RM_RANGE_END   (254)
#define UPROC_MUTEX_ID_TEST_A         (63)
#define UPROC_MUTEX_ID_TEST_B         (14)
#define UPROC_MUTEX_ID_TEST_C         (222)
#define UPROC_MUTEX_ID_TEST_D         (132)
#define UPROC_MUTEX_TEST_INDEX        (4)

//! Registers needed for mutex tests
struct UprocMutexAddrType
{
    ModsGpuRegField     mutexIdValue;
    ModsGpuRegAddress   mutex;
    ModsGpuRegAddress   mutexIdRelease;
    ModsGpuRegValue     mutexSize;
    ModsGpuRegValue     mutexIdValueNotAvail;
    ModsGpuRegValue     mutexValueInitialLock;
};

//! Maintain Register IDs for each uproc that framework supports
static const UprocMutexAddrType UprocMutex[UprocRtos::UPROC_ID_END] =
{
    {(ModsGpuRegField) NULL, (ModsGpuRegAddress) NULL, (ModsGpuRegAddress) NULL,
     (ModsGpuRegValue) NULL, (ModsGpuRegValue)   NULL, (ModsGpuRegValue)   NULL},
    {   //Sec2
        MODS_PSEC_MUTEX_ID_VALUE,
        MODS_PSEC_MUTEX,
        MODS_PSEC_MUTEX_ID_RELEASE,
        MODS_PSEC_MUTEX__SIZE_1,
        MODS_PSEC_MUTEX_ID_VALUE_NOT_AVAIL,
        MODS_PSEC_MUTEX_VALUE_INITIAL_LOCK
    },
    {   //Gsp
        MODS_PGSP_MUTEX_ID_VALUE,
        MODS_PGSP_MUTEX,
        MODS_PGSP_MUTEX_ID_RELEASE,
        MODS_PGSP_MUTEX__SIZE_1,
        MODS_PGSP_MUTEX_ID_VALUE_NOT_AVAIL,
        MODS_PGSP_MUTEX_VALUE_INITIAL_LOCK
    },
};

//! All the tests that framework supports. Add new tests here
#define LW_UPROC_TEST_NOARG(x) 0, (x), NULL
#define LW_UPROC_TEST_ARG(n, x) (n), NULL, (x)

const UprocTestCase p_UprocTestArr[] =
{
    // test ID                         test function                                                     description
    { UPROC_TEST_TYPE_BASIC_CMD_MSG_Q, LW_UPROC_TEST_NOARG(&UprocTest::UprocBasicCmdAndMsgQTest),        "UPROC Rtos BasicCmdMsgQ Test"},
    { UPROC_TEST_TYPE_SWITCH,          LW_UPROC_TEST_NOARG(&UprocTest::UprocRtosHsSwitchTest),           "UPROC Rtos HS Switch Test"},
    { UPROC_TEST_TYPE_MUTEX,           LW_UPROC_TEST_NOARG(&UprocTest::UprocRtosMutexTestVerifyMutexes), "UPROC Rtos Mutex Test"},
    { UPROC_TEST_TYPE_RTTIMER,         LW_UPROC_TEST_ARG(3,&UprocTest::UprocRtosRtTimerTest),            "UPROC Rtos RtTimer Test"},
    { UPROC_TEST_TYPE_MSCG,            LW_UPROC_TEST_ARG(4,&UprocTest::UprocRtosMscgTest),               "UPROC Rtos MSCG Test"},
    { UPROC_TEST_TYPE_COMMON,          LW_UPROC_TEST_ARG(2,&UprocTest::UprocRtosCommonTest),             "UPROC Rtos Common Test"},
};

//!
//! \brief Constructor
//!
UprocTest::UprocTest(UprocRtos *uprocRtos): m_uprocRtos(uprocRtos)
{
}

//!
//! @brief      Run uproc test(s) on this instance
//!
//! @param[in]  test  The test to be run
//!
//! @return     OK if no test errors, otherwise corresponding RC
//!
RC UprocTest::RunTest(LwU32 test, va_list *arg)
{
    RC rc;
    for (LwU32 testIndex = 0; testIndex < GET_ARRAY_SIZE(p_UprocTestArr); testIndex++)
    {
        if (test == p_UprocTestArr[testIndex].test_id)
        {
            m_pLwrrentTest = &p_UprocTestArr[testIndex];
            Printf(Tee::PriHigh, "Running %s\n", m_pLwrrentTest->name);
            rc = (m_pLwrrentTest->va_argcount > 0) ?
                 (this->*(m_pLwrrentTest->fn_arg))(m_pLwrrentTest->va_argcount, arg) :
                 (this->*(m_pLwrrentTest->fn_noarg))();
            break;
        }
    }
    return rc;
}

//! \brief UprocBasicCmdAndMsgQTest()
//!
//! \param (none)
//!
//! \return OK if test passed, specific RC if failed
RC UprocTest::UprocBasicCmdAndMsgQTest()
{
    RC                      rc           = OK;
    RM_UPROC_TEST_CMD       uprocTestCmd = {0};
    RM_UPROC_TEST_MSG       uprocTestMsg = {0};
    RM_FLCN_QUEUE_HDR       falconHdr    = {0};
    UINT32                  seqNum       = 0;

    //
    // Send an empty command to UPROC.  The NULL command does nothing but
    // make UPROC returns corresponding message.
    //

    falconHdr.unitId = RM_UPROC_UNIT_NULL;
    falconHdr.size   = RM_FLCN_QUEUE_HDR_SIZE;

    for (int i = 0; i < 10; i++)
    {
        Printf(Tee::PriHigh,
               "%d:UprocRtosBasicTest::%s: Submit a command to UPROC...\n",
                __LINE__, __FUNCTION__);

        // submit the command
        rc = m_uprocRtos->Command(&uprocTestCmd, &uprocTestMsg, &falconHdr, &seqNum);

        CHECK_RC(rc);

        Printf(Tee::PriHigh,
               "%d:UprocRtosBasicTest::%s: CMD submission success, got seqNum = %u\n",
               __LINE__, __FUNCTION__, seqNum);
    }
    return rc;
}

//! \brief HsWritePrivProtectedReg()
//!
//! \param reg        : Type of priv protected register to write (supported: 0/1)
//! \param val        : Value to write to register
//!
//! \return OK if all the tests passed, specific RC if failed
RC UprocTest::HsWritePrivProtectedReg(LwU8 reg, LwU32 val)
{
    RC                      rc              = OK;
    RM_UPROC_TEST_CMD       uprocTestCmd    = {0};
    RM_UPROC_TEST_MSG       uprocTestMsg    = {0};
    RM_FLCN_QUEUE_HDR       falconHdr       = {0};
    UINT32                  seqNum          = 0;
    UINT32                  ret             = 0;

    //
    // Send a WR_PRIV_PROTECTED_REG command to SEC2.
    //
    falconHdr.size              =
        RM_UPROC_CMD_SIZE(TEST, WR_PRIV_PROTECTED_REG);

    uprocTestCmd.cmdType        =
        RM_UPROC_CMD_TYPE(TEST, WR_PRIV_PROTECTED_REG);

    uprocTestCmd.wrPrivProtectedReg.val = val;

    //The design of this test relies on hardcoded registers in ucode
    //to be read in HS mode. While not very extensible, at least it's secure.
    if (reg > RM_UPROC_PRIV_PROTECTED_REG_SELWRE_SCRATCH || reg < RM_UPROC_PRIV_PROTECTED_REG_I2CS_SCRATCH)
    {
        Printf(Tee::PriHigh, "%d:UprocRtosHsSwitchTest::%s: Invalid register specified\n",
               __LINE__, __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    uprocTestCmd.wrPrivProtectedReg.regType = reg;

    Printf(Tee::PriHigh,
           "%d:UprocRtosHsSwitchTest::%s: Submitting a command to UPROC to switch "
           "to heavy secure mode and write priv protected register...\n",
            __LINE__, __FUNCTION__);

    // submit the command
    rc = m_uprocRtos->Command(&uprocTestCmd,
                              &uprocTestMsg,
                              &falconHdr,
                              &seqNum);
    CHECK_RC(rc);

    ret = uprocTestMsg.wrPrivProtectedReg.val;
    if (ret != val)
    {
        Printf(Tee::PriHigh,
               "%d:UprocRtosHsSwitchTest::%s: Value of priv protected "
               "register doesn't match value expected. Returned val = 0x%x, "
               "expected val = 0x%x, reg %d\n",
               __LINE__, __FUNCTION__, ret, val, reg);
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriHigh,
           "%d:UprocRtosHsSwitchTest::%s: Returned value from priv protected "
           "register matches expected value.\n",
            __LINE__, __FUNCTION__);
    return rc;
}

//! \brief UprocRtosHsSwitchTest()
//!
//! \param (none)
//!
//! \return OK if test passed, specific RC if failed
RC UprocTest::UprocRtosHsSwitchTest()
{
    RC rc = OK;
    // Run HS Switch Test
    Printf(Tee::PriHigh,
           "%d: UprocRtosHsSwitchTest::%s: Starts HS Switch Test...\n",
            __LINE__,__FUNCTION__);

    CHECK_RC(HsWritePrivProtectedReg(RM_UPROC_PRIV_PROTECTED_REG_I2CS_SCRATCH, 0xbeefbeef));
    CHECK_RC(HsWritePrivProtectedReg(RM_UPROC_PRIV_PROTECTED_REG_SELWRE_SCRATCH, 0xbeefbeef));

    return rc;
}


//! \brief UprocRtosCommonTest()
//!
//! This function sends an test cmds, to RTOS based 
//! on the type of sub test chosen waits for the msg back
//! and checks that the test passed.
//! It Takes the Sub test number as its input argument.
//!
//! \return OK if the Common test passed, SOFTWARE_ERROR if failed
//! \sa Run()
//-----------------------------------------------------------------------------
RC UprocTest::UprocRtosCommonTest(LwU32 argcount, ...)
{
    RC                      rc = OK;
    RM_UPROC_TEST_CMD       uprocTestCmd = { 0 };
    RM_UPROC_TEST_MSG       uprocTestMsg = { 0 };
    RM_FLCN_QUEUE_HDR       falconHdr = { 0 };
    LwU32                   seqNum = 0;
    LwU32                   subTest = 0;

    //Arguments
    va_list args;
    va_list *pParentArgs;

    //This only tries to ensure p_UprocTestArr functions are changed with the
    //test code in tandem.
    MASSERT(argcount == 2);
    va_start(args, argcount);
    pParentArgs = va_arg(args, va_list*);
    subTest = va_arg(*pParentArgs, LwU32);
    va_end(args);

    Printf(Tee::PriHigh,
        "%d:UprocRtosCommonTest::%s: Passed in ARG =  %d\n",
        __LINE__, __FUNCTION__, subTest);

    memset(&uprocTestCmd, 0, sizeof(uprocTestCmd));
    memset(&uprocTestMsg, 0, sizeof(uprocTestMsg));
    memset(&falconHdr, 0, sizeof(falconHdr));

    /* Set the cmd members */
    falconHdr.unitId = RM_UPROC_UNIT_TEST;
    falconHdr.size = RM_UPROC_CMD_SIZE(TEST, COMMON_TEST);

    uprocTestCmd.cmdType = RM_UPROC_CMD_TYPE(TEST, COMMON_TEST);
    uprocTestCmd.commonTest.subCmdType = subTest;

    MASSERT(subTest < RM_UPROC_COMMON_TEST_MAX);

    // submit the command
    rc = m_uprocRtos->Command(&uprocTestCmd,
        &uprocTestMsg,
        &falconHdr,
        &seqNum);
    CHECK_RC(rc);

    Printf(Tee::PriHigh,
        "%d:UprocRtosCommonTest::%s: CMD submission success, got seqNum = %u\n",
        __LINE__, __FUNCTION__, seqNum);

    // Check for the correct unitID
    if (uprocTestMsg.msgType != uprocTestCmd.cmdType)
    {
        Printf(Tee::PriHigh,
            "%d:UprocRtosCommonTest::%s: Wrong Test ID! Expected 0x%X. Got 0x%X\n",
            __LINE__, __FUNCTION__,
            RM_UPROC_TEST_CMD_ID_COMMON_TEST, uprocTestMsg.msgType);
        rc = RC::SOFTWARE_ERROR;
    }

    // Check if it passed
    if (uprocTestMsg.commonTest.status != RM_UPROC_TEST_MSG_STATUS_OK)
    {
        rc = RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriHigh,
        "%d:UprocRtosCommonTest::%s:SubTest %d %s\n",
        __LINE__, __FUNCTION__,
        subTest, (rc == OK) ? "PASSED" : "FAILED");
    CHECK_RC(rc);

    return rc;
}


//! \brief UprocRtosRtTimerTest()
//!
//! This function sends an RT TIMER test cmd, waits for the msg back
//! and checks that the test passed.
//!
//! \return OK if the RT Timer test passed, SOFTWARE_ERROR if failed
//! \sa Run()
//-----------------------------------------------------------------------------
RC UprocTest::UprocRtosRtTimerTest(LwU32 argcount, ...)
{
    RC                      rc = OK;
    RM_UPROC_TEST_CMD       uprocTestCmd = {0};
    RM_UPROC_TEST_MSG       uprocTestMsg = {0};
    RM_FLCN_QUEUE_HDR       falconHdr    = {0};
    LwU32                   seqNum       = 0;

    //Arguments
    va_list args;
    va_list *pParentArgs;
    LwU32 count;
    LwU32 passes;
    LwU32 isRunningOnFmodel;

    //This only tries to ensure p_UprocTestArr functions are changed with the
    //test code in tandem.
    MASSERT(argcount == 3);
    va_start(args, argcount);
    pParentArgs       = va_arg(args, va_list*);
    count             = va_arg(*pParentArgs, LwU32);
    passes            = va_arg(*pParentArgs, LwU32);
    isRunningOnFmodel = va_arg(*pParentArgs, LwU32);
    va_end(args);

    memset(&uprocTestCmd, 0, sizeof(uprocTestCmd));
    memset(&uprocTestMsg, 0, sizeof(uprocTestMsg));
    memset(&falconHdr, 0, sizeof(falconHdr));

    /* Set the cmd members */
    falconHdr.unitId  = RM_UPROC_UNIT_TEST;
    falconHdr.size    = RM_UPROC_CMD_SIZE(TEST, RTTIMER_TEST);

    uprocTestCmd.cmdType = RM_UPROC_CMD_TYPE(TEST, RTTIMER_TEST);
    uprocTestCmd.rttimer.count  = count;

    /* Don't check for timing accuracy on fmodel */
    uprocTestCmd.rttimer.bCheckTime = !isRunningOnFmodel;

    Printf(Tee::PriHigh,
           "%d:UprocRtosRtTimerTest::%s: Set count to %d. Src is PTimer_1US. Timing verification is %s\n",
            __LINE__, __FUNCTION__,
            uprocTestCmd.rttimer.count,
            uprocTestCmd.rttimer.bCheckTime ? "on" : "off");

    for (LwU32 i = 1; i <= passes; i++)
    {
        Printf(Tee::PriHigh,
           "%d:UprocRtosRtTimerTest::%s: Starting iteration %d\n",
            __LINE__, __FUNCTION__, i);

        // submit the command
        rc = m_uprocRtos->Command(&uprocTestCmd,
                                  &uprocTestMsg,
                                  &falconHdr,
                                  &seqNum);
        CHECK_RC(rc);

        Printf(Tee::PriHigh,
               "%d:UprocRtosRtTimerTest::%s: CMD submission success, got seqNum = %u\n",
                __LINE__, __FUNCTION__, seqNum);

        // Check for the correct unitID
        if (uprocTestMsg.msgType != uprocTestCmd.cmdType)
        {
            Printf(Tee::PriHigh,
           "%d:UprocRtosRtTimerTest::%s: Wrong Test ID! Expected 0x%X. Got 0x%X\n",
                __LINE__, __FUNCTION__,
                RM_UPROC_TEST_CMD_ID_RTTIMER_TEST, uprocTestMsg.msgType);
            rc = RC::SOFTWARE_ERROR;
        }

        // Check if it passed
        if (uprocTestMsg.rttimer.status != RM_UPROC_TEST_MSG_STATUS_OK)
        {
            rc = RC::SOFTWARE_ERROR;
        }

        Printf(Tee::PriHigh,
           "%d:UprocRtosRtTimerTest::%s: Iteration %d %s\n",
            __LINE__, __FUNCTION__,
            i, (rc == OK) ? "PASSED" : "FAILED");
        Printf(Tee::PriHigh,
            "%d:UprocRtosRtTimerTest::%s: OneShot mode took %d ns, Continuous mode took %d ns\n",
            __LINE__, __FUNCTION__,
            uprocTestMsg.rttimer.oneShotNs,
            uprocTestMsg.rttimer.continuousNs);
        CHECK_RC(rc);
    }
    return rc;
}

//! \brief RmCtrlPwrGateCmd()
//!
//! This function sets up statisics collection for power gating.
//!
//! \return OK if the RM control call passes, rc otherwise
//!
RC UprocTest::RmCtrlPwrGateCmd(LW2080_CTRL_POWERGATING_PARAMETER *pPGparams)
{
    RC      rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_MC_QUERY_POWERGATING_PARAMETER_PARAMS  QueryParams = {0};
    QueryParams.listSize      = 1;
    QueryParams.parameterList = (LwP64)pPGparams;

    // Send the RM control command.
    rc = pLwRm->ControlBySubdevice(m_uprocRtos->m_pRt->GetBoundGpuSubdevice(),
                                   LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER,
                                   &QueryParams,
                                   sizeof(QueryParams)
                                  );
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "\n %s: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER Query failed with rc=%d\n", __FUNCTION__,(UINT32)rc);
    }
    return rc;
}

#if UPROCTEST_MSCG_TEST_STRICT
//! \brief GetMscgWakeupReason()
//!
//! Retrieves MSCG wakeup reason mask from RM, and stores it into wakeupReason
//!
//! \param (out) wakeupReason
//!
//! \return OK if RM control to retrieve wakeup reason succeeded, otherwise rc.
RC UprocTest::GetMscgWakeupReason(LwU32 *wakeupReason)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_LPWR_FEATURE_PARAMETER_GET_PARAMS PgQueryMsParam = {0};

    PgQueryMsParam.listSize              = 1;
    PgQueryMsParam.list[0].feature.id    = LW2080_CTRL_LPWR_FEATURE_ID_PG;
    PgQueryMsParam.list[0].feature.subId = LW2080_CTRL_LPWR_SUB_FEATURE_ID_MS;
    PgQueryMsParam.list[0].param.id      = LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_WAKEUP_REASON_MASK;

    rc = pLwRm->ControlBySubdevice(m_uprocRtos->m_pRt->GetBoundGpuSubdevice(),
                                   LW2080_CTRL_CMD_LPWR_FEATURE_PARAMETER_GET,
                                   &PgQueryMsParam,
                                   sizeof(PgQueryMsParam)
                                  );
    if (OK == rc)
    {
        *wakeupReason = PgQueryMsParam.list[0].param.val;
    }
    return rc;
}

#endif
//! \brief WakeupFeature()
//!
//! Calls into uproc-specific implementation of MSCG wakeup reason.
//!
//! \param (in) wakeMethod which method the uproc should attempt to exit MSCG with
//! \param (in) waitTimeMs amount of time to wait for MSCG wakeup.
//!
//! \return OK if wakeup call succeeded, rc if it didn't, and SOFTWARE_ERROR if
//!         an invalid wakeup method is specified.
RC UprocTest::WakeupFeature(LwU32 wakeMethod, FLOAT64 waitTimeMs)
{
    RC rc;
    switch (wakeMethod)
    {
        case UPROCRTOS_UPROC_REG_ACCESS:
        case UPROCRTOS_UPROC_PB_ACTIVITY:
        case UPROCRTOS_UPROC_FB_ACCESS:
           rc = m_uprocRtos->SendUprocTraffic(wakeMethod, waitTimeMs);
           break;
        default:
           Printf(Tee::PriHigh, "Unknown/un-implemented wakeup type...\n");
           rc = RC::SOFTWARE_ERROR;
           break;
    }
    return rc;
}

//! @brief: The MSCG test implementation.
//!
//! The LpwrTest strategy is:
//!       a. collect stats from GPU - S1.
//!       b. wait for sometime.
//!       c. wakeup the GR engine.
//!       d. collect stats from GPU - S2.
//!       e. if (S2-S1) > 0, test is pass, else fail.
//!
//! 1. check for the feature to test.
//! 2. test for both kind of wakeup methods - Register access and Method activity.
//! 3. loop the test for N number of times, as given in test args.
//!
//! The STRICT MSCG strategy is:
//!       a. Determine correct wakeup reason.
//!       b. Wait for GPU to enter MSCG, and set up RM to record wakeup reason if it does.
//!       c. Send command to UPROC to wake up from MSCG.
//!       d. Uproc verifies it is in MSCG, then exelwtes wakeup method.
//!       e. Uproc wakes GPU from MSCG, and RM retrieves current wakeup reason.
//!       f. OR the aclwmulated wakeup reason with current wakeup reason.
//!       g. Loop from b) a few times (~5 iterations is usually enough).
//!       h. Check if S2 > S1. If not, fail the test. (LpwrTest criteria)
//!       i. Check if aclwmulated wakeup reason matches the correct wakeup reason.
//!       j. If not, fail the test. (Strict test criteria)
//!       k. Pass the test.
//! A minimum of two loops are required to satisfy LpwrTest criteria.
//! 5 is recommended to pass the Strict criteria.
//!---------------------------------------------------------------------------------------
RC UprocTest::UprocRtosMscgTest(LwU32 argcount, ...)
{
    RC rc;
    LwU32       origGatingCount = 0;
    LwU32       newGatingCount;
    LwU32       diffGatingCount;
    bool        bIsTestPassed   = true;
    LwU64       initialTimeMs   = 0;
    LwU64       finalTimeMs     = 0;
    LwU32       engineState     = 0;
    LW2080_CTRL_POWERGATING_PARAMETER PgParams = {0};
    LwRmPtr     pLwRm;
#if UPROCTEST_MSCG_TEST_STRICT
    LW2080_CTRL_LPWR_FEATURE_PARAMETER_GET_PARAMS PgSetMsParam = {0};
    LwU32       wakeupReason;
    LwU32       aclwmulatedWakeupReason = 0;
    LwU32       correctWakeupReason;
#endif
    //Arguments
    va_list     args;
    va_list    *pParentArgs;
    LwU32       wakeupMethod;
    LwU32       numLoops;
    FLOAT64     timeoutMs;
    FLOAT64     waitTimeMs;

    //This only tries to ensure p_UprocTestArr functions are changed with the
    //test code in tandem.
    MASSERT(argcount == 4);
    va_start(args, argcount);
    pParentArgs    = va_arg(args, va_list*);
    wakeupMethod   = va_arg(*pParentArgs, LwU32);
    numLoops       = va_arg(*pParentArgs, LwU32);
    timeoutMs      = va_arg(*pParentArgs, FLOAT64);
    waitTimeMs     = va_arg(*pParentArgs, FLOAT64);
    va_end(args);

    if (wakeupMethod == UPROCRTOS_UPROC_REG_ACCESS)
    {
        Printf(Tee::PriHigh, "\n******The UPROC - Register Access method is being used for waking the MS Engine.******\n");
    }
    else if (wakeupMethod == UPROCRTOS_UPROC_PB_ACTIVITY)
    {
        Printf(Tee::PriHigh, "\n******The UPROC - Pushbuffer Activity method is being used for waking the MS Engine.******\n");
    }
    else if (wakeupMethod == UPROCRTOS_UPROC_FB_ACCESS)
    {
        Printf(Tee::PriHigh, "\n******The UPROC - Framebuffer Access method is being used for waking the MS Engine.******\n");
    }
    else
    {
        Printf(Tee::PriHigh, "\n? Invalid wakeup method ?\n");
        return RC::SOFTWARE_ERROR;
    }

#if UPROCTEST_MSCG_TEST_STRICT
    correctWakeupReason = m_uprocRtos->GetMscgWakeupReasonMask(wakeupMethod);
    Printf(Tee::PriHigh, "STRICT MSCG TEST ENABLED. MSCG Wakeup reason must include %08x\n", correctWakeupReason);

    PgSetMsParam.listSize = 1;
    PgSetMsParam.list[0].feature.id = LW2080_CTRL_LPWR_FEATURE_ID_PG;
    PgSetMsParam.list[0].feature.subId = LW2080_CTRL_LPWR_SUB_FEATURE_ID_MS;
    PgSetMsParam.list[0].param.id = LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_WAKEUP_TYPE;
#endif

    for (LwU32 i = 0; i < numLoops; i++)
    {
      newGatingCount  = 0;
      Printf(Tee::PriHigh,"\n\n*****************************Loop Number : %d**********************************\n",i+1);

      // Waiting for Sleep time : LpwrWaitTimeMs for entering into an MSCG cycle.
      Printf(Tee::PriHigh, "\nGoing to sleep.\n");
      initialTimeMs  =  Utility::GetSystemTimeInSeconds()*1000;
      do
      {
          finalTimeMs  = Utility::GetSystemTimeInSeconds()*1000;
          Printf(Tee::PriHigh,"Parsing engine state.\n");
          PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE;
          PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
          CHECK_RC(RmCtrlPwrGateCmd(&PgParams));
          engineState = PgParams.parameterValue;
          if (finalTimeMs-initialTimeMs > waitTimeMs)
          {
              Printf(Tee::PriHigh, "Timed-out trying to power down the MS engine\n");
              break;
          }
          if (engineState == LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_ON)
              Printf(Tee::PriHigh, "The MS engine is still not down.");
          Tasker::Sleep(1000);
      } while (engineState != LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF);

      if (engineState == LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF)
          Printf(Tee::PriHigh, "Engine state is powered off.\n");

      // Sending Wake call to the engine.
      if (engineState == LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF)
      {
          Printf(Tee::PriHigh,"Waking up the engine.\n");

#if UPROCTEST_MSCG_TEST_STRICT
          //set cumulative wakeup
          PgSetMsParam.list[0].param.val = 1;
          CHECK_RC(pLwRm->ControlBySubdevice(m_uprocRtos->m_pRt->GetBoundGpuSubdevice(),
                                              LW2080_CTRL_CMD_LPWR_FEATURE_PARAMETER_SET,
                                              &PgSetMsParam,
                                              sizeof(PgSetMsParam)
                                             ));
#endif

          CHECK_RC(WakeupFeature(wakeupMethod, timeoutMs));

#if UPROCTEST_MSCG_TEST_STRICT
          CHECK_RC(GetMscgWakeupReason(&wakeupReason));
          aclwmulatedWakeupReason |= wakeupReason;
          Printf(Tee::PriHigh, "wakeupReason (lwr/acc): 0x%08x/0x%08x", wakeupReason, aclwmulatedWakeupReason);
          //clear cumulative wakeup
          PgSetMsParam.list[0].param.val = 0;
          CHECK_RC(pLwRm->ControlBySubdevice(m_uprocRtos->m_pRt->GetBoundGpuSubdevice(),
                                              LW2080_CTRL_CMD_LPWR_FEATURE_PARAMETER_SET,
                                              &PgSetMsParam,
                                              sizeof(PgSetMsParam)
                                             ));
#endif
      }
      else
      {
          Printf(Tee::PriHigh, "Unable to enter MSCG. Test probably failed!\n");
      }

      /*
       *    Stats collected after coming out of sleep.
       *
       */

      Printf(Tee::PriHigh,"\n\nStat collected after coming out of sleep : \n");

      // Parse the idle threshold for the engine in micro-second.
      PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD_US;
      PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
      RmCtrlPwrGateCmd(&PgParams);
      Printf(Tee::PriHigh, "The Idle threshold for the MS engine is : %u \n", PgParams.parameterValue);

      // Parse Post-power-up threshold for the engine in micro-second.
      PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD_US;
      PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
      RmCtrlPwrGateCmd(&PgParams);
      Printf(Tee::PriHigh, "The PPU threshold for the MS engine is : %u \n", PgParams.parameterValue);

      // Parse initialization status of the MSCG.
      PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_INITIALIZED;
      PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
      RmCtrlPwrGateCmd(&PgParams);
      Printf(Tee::PriHigh, "The initialization status of MSCG is : %u \n", PgParams.parameterValue);

      // Parse gating count.
      PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT;
      PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
      RmCtrlPwrGateCmd(&PgParams);
      Printf(Tee::PriHigh, "The Gating Count for MSCG is : %u \n", PgParams.parameterValue);

      newGatingCount = UNSIGNED_CAST(LwU32, PgParams.parameterValue);

      // Parse deny count.
      PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_DENYCOUNT;
      PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
      RmCtrlPwrGateCmd(&PgParams);
      Printf(Tee::PriHigh, "The deny Count for MSCG is : %u \n", PgParams.parameterValue);

      // Parse InGating count.
      PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_INGATINGCOUNT;
      PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
      RmCtrlPwrGateCmd(&PgParams);
      Printf(Tee::PriHigh, "The Ingating Count for MSCG is : %u \n", PgParams.parameterValue);

      // Parse UnGating count.
      PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_UNGATINGCOUNT;
      PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
      RmCtrlPwrGateCmd(&PgParams);
      Printf(Tee::PriHigh, "The Ungating Count for MSCG is : %u \n", PgParams.parameterValue);

      // Parse InGating Time in US.
      PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_INGATINGTIME_US;
      PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
      RmCtrlPwrGateCmd(&PgParams);
      Printf(Tee::PriHigh, "The ingating time for MSCG is : %u \n", PgParams.parameterValue);

      // Parse UnGating Time in US.
      PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_UNGATINGTIME_US;
      PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
      RmCtrlPwrGateCmd(&PgParams);
      Printf(Tee::PriHigh, "The ungating time for MSCG is : %u \n", PgParams.parameterValue);

      // Parse Avg Entry Time in US.
      PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_AVG_ENTRYTIME_US;
      PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
      RmCtrlPwrGateCmd(&PgParams);
      Printf(Tee::PriHigh, "The average entry time for MSCG is : %u \n", PgParams.parameterValue);

      // Parse Avg Exit Time in US.
      PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_AVG_EXITTIME_US;
      PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
      RmCtrlPwrGateCmd(&PgParams);
      Printf(Tee::PriHigh, "The average exit time for MSCG is : %u \n", PgParams.parameterValue);

      diffGatingCount = newGatingCount - origGatingCount;
      origGatingCount = newGatingCount;
      Printf(Tee::PriHigh,"\n\nThe difference in gating Counts is : %d\n", diffGatingCount);
      if (diffGatingCount > 0)
      {
          bIsTestPassed = true;
          Printf(Tee::PriHigh,"\n\nThe test has passed for %d loop.\n",i+1);
      }
      else
      {
          bIsTestPassed = false;
          break;
      }
    }

#if UPROCTEST_MSCG_TEST_STRICT
    Printf(Tee::PriHigh, "Aclwmulated wakeupReason: 0x%08x\n", aclwmulatedWakeupReason);
    if ((bIsTestPassed == true) && ((aclwmulatedWakeupReason & correctWakeupReason) == 0))
    {
        //fail if we would have passed, but didn't find proper wakeup reason
        Printf(Tee::PriHigh, "Test failed because wakeup reason does not include expected value %08x!", correctWakeupReason);
        bIsTestPassed = false;
    }
#endif

    if (bIsTestPassed == true)
    {
        Printf(Tee::PriHigh, "\nTest Passed.\n");
        rc = OK;
    }
    else
    {
        Printf(Tee::PriHigh, "\nTest Failed.\n");
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}


//! \brief Acquire mutex ID
//!
//! \return acquired mutex ID
LwU32 UprocTest::AcquireMutexId()
{
    const RegHal &regs  = m_uprocRtos->m_pRt->GetBoundGpuSubdevice()->Regs();
    return regs.Read32(UprocMutex[m_uprocRtos->m_UprocId].mutexIdValue);
}

//! \brief Release mutex ID
//!
//! \param[in]  mutexId The index of the mutex ID to release
//!
//! \return     RC::SOFTWARE ERROR if mutex ID is out of range, else OK
RC UprocTest::ReleaseMutexId(LwU32 mutexId)
{
    RegHal &regs  = m_uprocRtos->m_pRt->GetBoundGpuSubdevice()->Regs();
    if ((mutexId > UPROC_MUTEX_ID_RM_RANGE_END) || (mutexId < UPROC_MUTEX_ID_RM_RANGE_START))
    {
        Printf(Tee::PriHigh, "UprocRtosMutexTest::%s Invalid arg: mutexId=0x%0x\n", __FUNCTION__, mutexId);
        return RC::SOFTWARE_ERROR;
    }
    regs.Write32(UprocMutex[m_uprocRtos->m_UprocId].mutexIdRelease, mutexId);
    return OK;
}

//! \brief Test acquiring the mutex
//!
//! \param[in]  mutexIndex      The index of the mutex to acquire
//! \param[in]  mutexId         The id with which we acquire the mutex
//! \param[in]  bExpectAcquire  Whether we expect to acquire the mutex
//!
//! \return     OK bExpectAcquire matches whether we acquired the mutex
//!             RC::SOFTWARE_ERROR if it doesn't match expectation or args are invalid
RC UprocTest::TryMutex(LwU32 mutexIndex, LwU32 mutexId, LwBool bExpectAcquire)
{
    LwU32  readId;
    RegHal &regs  = m_uprocRtos->m_pRt->GetBoundGpuSubdevice()->Regs();

    if ((mutexId > UPROC_MUTEX_ID_RM_RANGE_END) || (mutexId < UPROC_MUTEX_ID_RM_RANGE_START) ||
        (mutexIndex >= regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexSize)))
    {
        Printf(Tee::PriHigh, "UprocRtosMutexTest::%s Invalid arg: mutexId=0x%0x or mutexIndex=0x%0x\n", __FUNCTION__, mutexId, mutexIndex);
        return RC::SOFTWARE_ERROR;
    }

    regs.Write32(UprocMutex[m_uprocRtos->m_UprocId].mutex, mutexIndex, mutexId);

    // Readback and verify it

    // If Acquired
    if ((readId = regs.Read32(UprocMutex[m_uprocRtos->m_UprocId].mutex, mutexIndex)) == mutexId)
    {
        if (!bExpectAcquire)
        {
            Printf(Tee::PriHigh, "UprocRtosMutexTest::%s  Should not have acquired with mutexId %d for mutex index %d\n",
                   __FUNCTION__, mutexId, mutexIndex);
            return RC::SOFTWARE_ERROR;
        }
    }
    // If Not Acquired
    else
    {
        if (bExpectAcquire)
        {
            Printf(Tee::PriHigh, "UprocRtosMutexTest::%s  Read %d, but expected %d for mutex index %d\n",
                   __FUNCTION__, readId, mutexId, mutexIndex);
            return RC::SOFTWARE_ERROR;
        }
    }
    return OK;
}

//! \brief Release mutex
//!
//! \param[in]  mutexIndex  The index of the mutex to release
//!
//! \return     RC::SOFTWARE ERROR if mutex was not released or index is out of range, else OK
//! \sa Sec2RtosMutexTestVerifyMutexes
RC UprocTest::ReleaseMutex(LwU32 mutexIndex)
{
    LwU32  readId;
    RegHal &regs  = m_uprocRtos->m_pRt->GetBoundGpuSubdevice()->Regs();
    if (mutexIndex >= regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexSize))
    {
        Printf(Tee::PriHigh, "UprocRtosMutexTest::%s Invalid arg: mutexIndex=0x%0x\n", __FUNCTION__, mutexIndex);
        return RC::SOFTWARE_ERROR;
    }
    regs.Write32(UprocMutex[m_uprocRtos->m_UprocId].mutex, mutexIndex, regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexValueInitialLock));
    // Readback and verify it was released
    if ((readId = regs.Read32(UprocMutex[m_uprocRtos->m_UprocId].mutex, mutexIndex)) != regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexValueInitialLock))
    {
        Printf(Tee::PriHigh, "UprocRtosMutexTest::%s Mutex %d not released, read %d\n", __FUNCTION__, mutexIndex, readId);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//! \brief UprocRtosMutexTestVerifyMutexes()
//!
//! Test cases derived from DpuVerifFeatureTest
//!
//! Verification Requirement from section 4.2.2.3 of GP10x_SEC_IAS.docx:
//! https://p4viewer.lwpu.com/get///hw/gpu_ip/doc/sec/arch/GP10x_SEC_IAS.docx
//!
//! (1 and 9 have been tested but not checked in, 3 and 4 are not tested directly)
//!
//! 1.  Checks that MUTEX ID obtain/release, MUTEX lock obtain/release can be done by both PRIV and ucode;
//! 2.  Checks that all the 8 to 254 ID values can be obtained, and 255 will return when valid ID is exhausted;
//! 3.  Checks that write 0, 255, or 1~7 values to MUTEX_ID_RELEASE won't have side-effect;
//! 4.  Checks that write 8~254 ID value to MUTEX_ID_RELEASE when not obtained won't have side effect;
//! 5.  Checks that obtained IDs can be returned out-of-order (e.g. obtain 8->9->...->254, return 254->253->...->8);
//! 6.  Checks that all the 0~15 MUTEX locks can be obtained and released
//! 7.  Checks that MUTEX lock can NOT be released by value other than 0
//! 8.  Checks that the obtaining and releasing of different MUTEX locks can be out-of-order
//! 9.  Checks that MUTEX_ID and locks registers won't be affected by priv lockdown
//!
//! \return OK if all the tests passed, specific RC if failed
//! \sa Run()
RC UprocTest::UprocRtosMutexTestVerifyMutexes()
{
    RC     rc       = OK;
    LwU32  mutexId  = 0;
    LwU32  index    = 0;
    LwU32  temp     = 0;
    RegHal &regs    = m_uprocRtos->m_pRt->GetBoundGpuSubdevice()->Regs();
    LwBool mutexIdAcquired[UPROC_MUTEX_ID_RM_RANGE_END + 1] = {0};

    Printf(Tee::PriHigh, "********** UprocRtosMutexTest::%s - Mutex verification start ********************\n",
           __FUNCTION__);

    //
    // 2. Checks that all the 8 to 254 ID values can be obtained
    //
    //    This must use an array since the values are not guaranteed
    //    to be in order if they were used previously. However, the test does assume
    //    that all previously used IDs have been released.
    //
    for (LwU32 i = UPROC_MUTEX_ID_RM_RANGE_START; i <= UPROC_MUTEX_ID_RM_RANGE_END; i++)
    {
        mutexId = AcquireMutexId();
        if (mutexId == regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexIdValueNotAvail))
        {
            Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - mutexId not available! Acquired %d/%d successfully.\n"
                   "mutexIds must not be acquired going into test \"FAILED\"\n",
                   __FUNCTION__,
                   i - UPROC_MUTEX_ID_RM_RANGE_START,
                   UPROC_MUTEX_ID_RM_RANGE_END + 1 - UPROC_MUTEX_ID_RM_RANGE_START);
            return RC::SOFTWARE_ERROR;
        }
        if (mutexId < UPROC_MUTEX_ID_RM_RANGE_START || mutexId > UPROC_MUTEX_ID_RM_RANGE_END)
        {
            Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Acquired mutex ID %d is out of bounds [%d, %d] \"FAILED\"\n",
                   __FUNCTION__, mutexId, UPROC_MUTEX_ID_RM_RANGE_START, UPROC_MUTEX_ID_RM_RANGE_END);
            return RC::SOFTWARE_ERROR;
        }
        mutexIdAcquired[mutexId] = LW_TRUE;
    }

    for (LwU32 i = UPROC_MUTEX_ID_RM_RANGE_START; i <= UPROC_MUTEX_ID_RM_RANGE_END; i++)
    {
        if (mutexIdAcquired[i] == LW_FALSE)
        {
            Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Mutex ID %d was never acquired \"FAILED\"\n",
                   __FUNCTION__, i);
            return RC::SOFTWARE_ERROR;
        }
    }
    Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Acquire all IDs \"PASSED\"\n",
           __FUNCTION__);


    // Check that 255 is returned when IDs are exhausted
    if ((mutexId = AcquireMutexId()) != regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexIdValueNotAvail))
    {
        Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - %d returned on exhaustion \"FAILED\"\n",
               __FUNCTION__, regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexIdValueNotAvail));
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - %d returned on exhaustion \"PASSED\"\n",
               __FUNCTION__, regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexIdValueNotAvail));
    }

    //
    // 5. Checks that obtained IDs can be returned out-of-order (e.g. obtain 8->9->...->254, return 254->253->...->8);
    //
    // We obtained all the mutexIds above..
    // Try releasing 4 different mutexId and acquiring them back...
    // Due to the internal representation, they should be reacquired in the order of release
    //
    CHECK_RC(ReleaseMutexId(UPROC_MUTEX_ID_TEST_A));
    CHECK_RC(ReleaseMutexId(UPROC_MUTEX_ID_TEST_B));
    CHECK_RC(ReleaseMutexId(UPROC_MUTEX_ID_TEST_C));
    CHECK_RC(ReleaseMutexId(UPROC_MUTEX_ID_TEST_D));

    if ((mutexId = AcquireMutexId()) != UPROC_MUTEX_ID_TEST_A)
    {
        Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Release/Acquire mutex ID 'A' read 0x%0x expected 0x%0x \"FAILED\"\n",
               __FUNCTION__, mutexId, UPROC_MUTEX_ID_TEST_A);
        return RC::SOFTWARE_ERROR;
    }
    if ((mutexId = AcquireMutexId()) != UPROC_MUTEX_ID_TEST_B)
    {
        Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Release/Acquire mutex ID 'B' read 0x%0x expected 0x%0x \"FAILED\"\n",
               __FUNCTION__, mutexId, UPROC_MUTEX_ID_TEST_B);
        return RC::SOFTWARE_ERROR;
    }
    if ((mutexId = AcquireMutexId()) != UPROC_MUTEX_ID_TEST_C)
    {
        Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Release/Acquire mutex ID 'C' read 0x%0x expected 0x%0x \"FAILED\"\n",
               __FUNCTION__, mutexId, UPROC_MUTEX_ID_TEST_C);
        return RC::SOFTWARE_ERROR;
    }
    if ((mutexId = AcquireMutexId()) != UPROC_MUTEX_ID_TEST_D)
    {
        Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Release/Acquire mutex ID 'D' read 0x%0x expected 0x%0x \"FAILED\"\n",
               __FUNCTION__, mutexId, UPROC_MUTEX_ID_TEST_D);
        return RC::SOFTWARE_ERROR;
    }
    if ((mutexId = AcquireMutexId()) != regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexIdValueNotAvail))
    {
        Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Release/Acquire mutex ID exhaustion read 0x%0x expected 0x%0x \"FAILED\"\n",
               __FUNCTION__, mutexId, regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexIdValueNotAvail));
        return RC::SOFTWARE_ERROR;
    }
    Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Release/Acquire mutex out of order \"PASSED\"\n",
           __FUNCTION__);

    // Release all the mutexIds now, from 254-8
    while (mutexId > UPROC_MUTEX_ID_RM_RANGE_START)
    {
        CHECK_RC(ReleaseMutexId(--mutexId));
    }
    Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Releasing all mutex IDs \"PASSED\"\n",
           __FUNCTION__);

    // 6.  Checks that all the 0~15 MUTEX locks can be obtained and released

    // Lets acquire all mutexes
    mutexId = AcquireMutexId();

    // Readback and verify it
    for (index = 0; index < regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexSize); index++)
    {
        CHECK_RC(TryMutex(index, mutexId, true));
    }
    Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Acquiring all mutexes with first mutex ID 0x%0x \"PASSED\"\n",
           __FUNCTION__, mutexId);

    temp    = mutexId;
    mutexId = AcquireMutexId();
    // Current mutexId and previous mutexId have to be different
    if (mutexId == temp)
    {
        Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Current mutexId 0x%0x and Previous mutexId 0x%0x are the same \"FAILED\"\n",
               __FUNCTION__, mutexId, temp);
        return RC::SOFTWARE_ERROR;
    }

    // Acquire mutexes again with different ID and it should fail
    CHECK_RC(ReleaseMutexId(temp));
    for (index = 0; index < regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexSize); index++)
    {
        CHECK_RC(TryMutex(index, mutexId, false));
    }
    Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Testing mutex acquire fail with second mutex ID 0x%0x \"PASSED\"\n",
           __FUNCTION__, mutexId);

    // 7. Checks that MUTEX lock can NOT be released by value other than 0
    for (LwU32 val = 1; val <= regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexIdValueNotAvail); val++)
    {
        regs.Write32(UprocMutex[m_uprocRtos->m_UprocId].mutex, UPROC_MUTEX_TEST_INDEX, val);
        CHECK_RC(TryMutex(UPROC_MUTEX_TEST_INDEX, mutexId, false));
    }
    Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Only 0 releases Mutexes \"PASSED\"\n",
           __FUNCTION__);

    // Release all mutexes
    for (index = 0; index < regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexSize); index++)
    {
        CHECK_RC(ReleaseMutex(index));
    }
    Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Releasing all mutexes \"PASSED\"\n",
           __FUNCTION__);

    // Acquire again and it should pass now
    for (index = 0; index < regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexSize); index++)
    {
        CHECK_RC(TryMutex(index, mutexId, true));
    }
    Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Acquiring all mutexes with second mutex ID 0x%0x \"PASSED\"\n",
           __FUNCTION__, mutexId);

    //
    // 8. Checks that the obtaining and releasing of different MUTEX locks can be out-of-order
    //
    //    Uses a different mutexID to reacquire
    //
    CHECK_RC(ReleaseMutex(4));
    CHECK_RC(ReleaseMutex(9));
    CHECK_RC(ReleaseMutex(2));
    CHECK_RC(ReleaseMutex(11));
    mutexId = AcquireMutexId();
    CHECK_RC(TryMutex(2, mutexId, true));
    CHECK_RC(TryMutex(9, mutexId, true));
    CHECK_RC(TryMutex(4, mutexId, true));
    CHECK_RC(TryMutex(11, mutexId, true));

    // Release all mutexes to reset state
    for (index = 0; index < regs.LookupValue(UprocMutex[m_uprocRtos->m_UprocId].mutexSize); index++)
    {
        CHECK_RC(ReleaseMutex(index));
    }
    Printf(Tee::PriHigh, "UprocRtosMutexTest::%s - Releasing all mutexes (second time) \"PASSED\"\n",
           __FUNCTION__);
    // Release all IDs to reset state
    for (mutexId = UPROC_MUTEX_ID_RM_RANGE_START; mutexId < UPROC_MUTEX_ID_RM_RANGE_END; mutexId++)
    {
        CHECK_RC(ReleaseMutexId(mutexId));
    }

    // Done!
    Printf(Tee::PriHigh, "********** UprocRtosMutexTest::%s - Mutex verification end ********************\n",
           __FUNCTION__);

    return rc;
}
