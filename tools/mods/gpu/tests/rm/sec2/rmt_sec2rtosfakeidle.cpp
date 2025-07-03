/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_Sec2RtosFakeIdle.cpp
//! \brief rmtest for SEC2
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/utility/sec2rtossub.h"

#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"

#include "class/cl9070.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/clb6b9.h"
#include "ctrl/ctrlb6b9.h"

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl95a1.h" // LW95A1_TSEC

#include "pascal/gp102/dev_sec_pri.h"
#include "pascal/gp102/dev_pwr_pri.h"

#define IsGP102orBetter(p)   ((p >= Gpu::GP102))

class Sec2RtosFakeIdleTest : public RmTest
{
public:
    Sec2RtosFakeIdleTest();
    virtual ~Sec2RtosFakeIdleTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    Sec2Rtos     *m_pSec2Rtos = nullptr;
    GpuSubdevice *m_Parent    = nullptr;
    RC            Sec2RtosFakeIdleTestSemaphoreTest();

    UINT64       m_gpuAddr    = 0;
    UINT32 *     m_cpuAddr    = nullptr;

    LwRm::Handle m_hObj       = 0;

    LwRm::Handle m_hSemMem    = 0;
    LwRm::Handle m_hVA        = 0;
    FLOAT64      m_TimeoutMs  = Tasker::NO_TIMEOUT;
};

//! \brief Sec2RtosFakeIdleTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
Sec2RtosFakeIdleTest::Sec2RtosFakeIdleTest()
{
    SetName("Sec2RtosFakeIdleTest");
}

//! \brief Sec2RtosFakeIdleTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
Sec2RtosFakeIdleTest::~Sec2RtosFakeIdleTest()
{
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Sec2RtosFakeIdleTest::IsTestSupported()
{
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();
    RC              rc;

    if (!IsClassSupported(MAXWELL_SEC2) || !IsGP102orBetter(chipName))
    {
        return "[SEC2 RTOS RMTest] : Supported only on GP10X (>= GP102) chips with SEC2 RTOS";
    }

    // Get SEC2 RTOS class instance
    m_Parent = GetBoundGpuSubdevice();
    rc = (m_Parent->GetSec2Rtos(&m_pSec2Rtos));
    if (OK != rc)
    {
        return "[SEC2 RTOS FakeIdle RMTest] : SEC2 RTOS not supported";
    }

    if (!m_pSec2Rtos->CheckTestSupported(RM_SEC2_CMD_TYPE(TEST, FAKEIDLE_TEST)))
    {
        return "[SEC2 RTOS FakeIdle RMTest] : Test is not supported";
    }
    return RUN_RMTEST_TRUE;
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC Sec2RtosFakeIdleTest::Setup()
{
    RC           rc;
    const UINT32 memSize = 0x1000;
    LwRmPtr      pLwRm;

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_SEC2));

    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObj, LW95A1_TSEC, NULL));

    CHECK_RC(pLwRm->AllocSystemMemory(&m_hSemMem,
             DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
             DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
             DRF_DEF(OS02, _FLAGS, _COHERENCY, _UNCACHED),
             memSize, GetBoundGpuDevice()));

    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &m_hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hSemMem, 0, memSize,
                                 LW04_MAP_MEMORY_FLAGS_NONE, &m_gpuAddr, GetBoundGpuDevice()));
    CHECK_RC(pLwRm->MapMemory(m_hSemMem, 0, memSize, (void **)&m_cpuAddr, 0, GetBoundGpuSubdevice()));
    return rc;
}

//! \brief Run the test
//!
//! Make sure SEC2 is bootstrapped. Then run test items.
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC Sec2RtosFakeIdleTest::Run()
{
    RC rc;
    AcrVerify lsSec2State;

    CHECK_RC(lsSec2State.BindTestDevice(GetBoundTestDevice()));

    rc = m_pSec2Rtos->WaitSEC2Ready();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:Sec2RtosFakeIdleTest::%s: Ucode OS is not ready. Skipping tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why SEC2 bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }

    if (lsSec2State.IsTestSupported() == RUN_RMTEST_TRUE)
    {
        lsSec2State.Setup();
        // Verify whether SEC2 is booted in the expected mode or not
        rc = lsSec2State.VerifyFalconStatus(LSF_FALCON_ID_SEC2, LW_TRUE);
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "%d: Sec2RtosFakeIdleTest %s: SEC2 failed to boot in expected Mode\n",
                   __LINE__, __FUNCTION__);
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }
    else
    {
        Printf(Tee::PriHigh, "%d: Sec2RtosFakeIdleTest %s: LS Falcon test not supported\n",
               __LINE__, __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    // Run Cmd & Msg Queue Test
    Printf(Tee::PriHigh,
           "%d:Sec2RtosFakeIdleTest::%s: Starts Fake Idle Test...\n",
            __LINE__,__FUNCTION__);

    CHECK_RC(Sec2RtosFakeIdleTestSemaphoreTest());

    return rc;
}

//! \brief Sec2RtosFakeIdleTestSemaphoreTest()
//!
//! Tests fake idle by masking off interrupts and sending semaphore methods.
//! The idle status is taken from the PMU register LW_PPWR_PMU_IDLE_STATUS.
//! This is the same status sent from SEC2 to host.
//!
//! When fake-idle is in use, the engine status is idle if:
//! !(mthd_fifo_non_empty || ctxsw_request_pending || sw_programmed_busy)
//!
//! The uCode portion of the test is used to mask/unmask interrupts,
//! manually toggle between SW-IDLE and SW-BUSY, and to wait until
//! the ctxsw or method is pending to read the status.
//!
//! Stages of the test are as follows:
//! 1. Mask off interrupts in uCode, check that SEC2 is idle to start the test
//! 2. Send a method to SEC2, check that SEC2 reports busy after receiving ctxsw
//! 3. Unmask Interrupts, allowing ctxsw and method to be processed
//!
//! 4. Mask off interrupts in uCode again
//! 5. Send a second method to SEC2, check that SEC2 reports busy after receiving method
//! 6. Unmask Interrupts, allowing method to be processed
//! 7. Check that engine is idle after processing
//!
//! 8. Program ENGINE_BUSY_CYA to SW_BUSY, check that engine reports busy
//! 9. Program ENGINE_BUSY_CYA to SW_IDLE, check that engine reports idle again
//!
//! \return OK if all the tests passed, specific RC if failed
//! \sa Run()
RC Sec2RtosFakeIdleTest::Sec2RtosFakeIdleTestSemaphoreTest()
{
    RC                      rc = OK;
    RM_FLCN_CMD_SEC2        sec2Cmd;
    RM_FLCN_MSG_SEC2        sec2Msg;
    UINT32                  seqNum;

    memset(&sec2Cmd, 0, sizeof(RM_FLCN_CMD_SEC2));
    memset(&sec2Msg, 0, sizeof(RM_FLCN_MSG_SEC2));

    /* Set the cmd members */
    sec2Cmd.hdr.unitId            = RM_SEC2_UNIT_TEST;
    sec2Cmd.hdr.size              =
            RM_SEC2_CMD_SIZE(TEST, FAKEIDLE_TEST);
    sec2Cmd.cmd.sec2Test.cmdType  =
            RM_SEC2_CMD_TYPE(TEST, FAKEIDLE_TEST);

    Printf(Tee::PriHigh, "\n\n********** Sec2RtosFakeIdleTest::%s - start ********************\n\n\n",
           __FUNCTION__);

    //
    // 1. Mask off interrupts in uCode, check that SEC2 is idle to start the test
    //

    // Mask off interrupts in uCode
    Printf(Tee::PriHigh,
       "%d:Sec2RtosRttimerTest::%s: Masking off method/ctxsw interrupts...\n",
        __LINE__, __FUNCTION__);
    sec2Cmd.cmd.sec2Test.fakeidle.op = FAKEIDLE_MASK_INTR;
    rc = m_pSec2Rtos->Command(&sec2Cmd,
                              &sec2Msg,
                              &seqNum);
    CHECK_RC(rc);

    // Ensure SEC2 is idle to start the test
    if (FLD_TEST_DRF(_PPWR, _PMU_IDLE_STATUS, _SEC, _BUSY,
        m_Parent->RegRd32(LW_PPWR_PMU_IDLE_STATUS)))
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: Falcon reports idle at start of test \"FAILED\"\n",
            __LINE__, __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: Falcon reports idle at start of test \"PASSED\"\n",
            __LINE__, __FUNCTION__);
    }

    //
    // 2. Send a method to SEC2, check that SEC2 reports busy after receiving ctxsw
    //

    // Send Semaphore Method
    MEM_WR32(m_cpuAddr, 0x87654321);
    m_pCh->SetObject(0, m_hObj);
    m_pCh->Write(0, LW95A1_SEMAPHORE_A,
        DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (UINT32)(m_gpuAddr >> 32LL)));
    m_pCh->Flush();

    // Have uCode wait to receive pending ctxsw. Return error on uCode timeout
    sec2Cmd.cmd.sec2Test.fakeidle.op = FAKEIDLE_CTXSW_DETECT;
    rc = m_pSec2Rtos->Command(&sec2Cmd,
                              &sec2Msg,
                              &seqNum);
    CHECK_RC(rc);
    if (sec2Msg.msg.sec2Test.fakeidle.status != RM_SEC2_TEST_MSG_STATUS_OK)
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: SEC2 timed out waiting to receive CTXSW from host!\n",
            __LINE__, __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // Check that SEC2 is busy when ctxsw is pending
    if (FLD_TEST_DRF(_PPWR, _PMU_IDLE_STATUS, _SEC, _IDLE,
        m_Parent->RegRd32(LW_PPWR_PMU_IDLE_STATUS)))
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: Falcon reports busy after receiving ctxsw \"FAILED\"\n",
            __LINE__, __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: Falcon reports busy after receiving ctxsw \"PASSED\"\n",
            __LINE__, __FUNCTION__);
    }

    //
    // 3. Unmask Interrupts, allowing ctxsw and method to be processed
    //
    sec2Cmd.cmd.sec2Test.fakeidle.op = FAKEIDLE_UNMASK_INTR;
    rc = m_pSec2Rtos->Command(&sec2Cmd,
                              &sec2Msg,
                              &seqNum);
    CHECK_RC(rc);

    //
    // 4. Mask off interrupts again
    //
    Printf(Tee::PriHigh,
       "%d:Sec2RtosRttimerTest::%s: Masking off method/ctxsw interrupts...\n",
        __LINE__, __FUNCTION__);
    sec2Cmd.cmd.sec2Test.fakeidle.op = FAKEIDLE_MASK_INTR;
    rc = m_pSec2Rtos->Command(&sec2Cmd,
                              &sec2Msg,
                              &seqNum);

    //
    // 5. Send a second method to SEC2, check that SEC2 reports busy after receiving method
    //

    // Send second semaphore method
    CHECK_RC(rc);
    m_pCh->Write(0, LW95A1_SEMAPHORE_B, (UINT32)(m_gpuAddr & 0xffffffffLL));
    m_pCh->Flush();

    // Have uCode wait to receive pending method. Return error on uCode timeout
    sec2Cmd.cmd.sec2Test.fakeidle.op = FAKEIDLE_MTHD_DETECT;
    rc = m_pSec2Rtos->Command(&sec2Cmd,
                              &sec2Msg,
                              &seqNum);
    CHECK_RC(rc);
    if (sec2Msg.msg.sec2Test.fakeidle.status != RM_SEC2_TEST_MSG_STATUS_OK)
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: SEC2 timed out waiting to receive method from host!\n",
            __LINE__, __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // Check that SEC2 is busy when method is pending
    if (FLD_TEST_DRF(_PPWR, _PMU_IDLE_STATUS, _SEC, _IDLE,
        m_Parent->RegRd32(LW_PPWR_PMU_IDLE_STATUS)))
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: Falcon reports busy after receiving method \"FAILED\"\n",
            __LINE__, __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: Falcon reports busy after receiving method \"PASSED\"\n",
            __LINE__, __FUNCTION__);
    }

    //
    // 6. Unmask Interrupts, allowing method to be processed
    //
    sec2Cmd.cmd.sec2Test.fakeidle.op = FAKEIDLE_UNMASK_INTR;
    rc = m_pSec2Rtos->Command(&sec2Cmd,
                              &sec2Msg,
                              &seqNum);
    CHECK_RC(rc);

    //
    // 7. Check that engine is idle after processing
    //
    sec2Cmd.cmd.sec2Test.fakeidle.op = FAKEIDLE_COMPLETE;
    rc = m_pSec2Rtos->Command(&sec2Cmd,
                              &sec2Msg,
                              &seqNum);
    CHECK_RC(rc);
    if (FLD_TEST_DRF(_PPWR, _PMU_IDLE_STATUS, _SEC, _BUSY,
        m_Parent->RegRd32(LW_PPWR_PMU_IDLE_STATUS)))
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: Falcon reports idle at end of test \"FAILED\"\n",
            __LINE__, __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: Falcon reports idle at end of test \"PASSED\"\n",
            __LINE__, __FUNCTION__);
    }

    //
    // 8. Program ENGINE_BUSY_CYA to SW_BUSY, check that engine reports busy
    //
    sec2Cmd.cmd.sec2Test.fakeidle.op = FAKEIDLE_PROGRAM_BUSY;
    rc = m_pSec2Rtos->Command(&sec2Cmd,
                              &sec2Msg,
                              &seqNum);
    CHECK_RC(rc);
    if (FLD_TEST_DRF(_PPWR, _PMU_IDLE_STATUS, _SEC, _IDLE,
        m_Parent->RegRd32(LW_PPWR_PMU_IDLE_STATUS)))
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: Falcon reports busy when SW-BUSY set \"FAILED\"\n",
            __LINE__, __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: Falcon reports busy when SW-BUSY set \"PASSED\"\n",
            __LINE__, __FUNCTION__);
    }

    //
    // 9. Program ENGINE_BUSY_CYA to SW_IDLE, check that engine reports idle again
    //
    sec2Cmd.cmd.sec2Test.fakeidle.op = FAKEIDLE_PROGRAM_IDLE;
    rc = m_pSec2Rtos->Command(&sec2Cmd,
                              &sec2Msg,
                              &seqNum);
    CHECK_RC(rc);
    if (FLD_TEST_DRF(_PPWR, _PMU_IDLE_STATUS, _SEC, _BUSY,
        m_Parent->RegRd32(LW_PPWR_PMU_IDLE_STATUS)))
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: Falcon reports idle when SW-IDLE set \"FAILED\"\n",
            __LINE__, __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRttimerTest::%s: Falcon reports idle when SW-IDLE set \"PASSED\"\n",
            __LINE__, __FUNCTION__);
    }
    Printf(Tee::PriHigh, "********** Sec2RtosFakeIdleTest::%s - end ********************\n",
           __FUNCTION__);
    return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC Sec2RtosFakeIdleTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    pLwRm->UnmapMemory(m_hSemMem, m_cpuAddr, 0, GetBoundGpuSubdevice());
    pLwRm->UnmapMemoryDma(m_hVA, m_hSemMem,
                          LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, GetBoundGpuDevice());

    pLwRm->Free(m_hVA);
    pLwRm->Free(m_hSemMem);
    pLwRm->Free(m_hObj);
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    m_pSec2Rtos = NULL;
    return firstRc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Sec2RtosFakeIdleTest, RmTest, "SEC2 RTOS Fake Idle Test. \n");

