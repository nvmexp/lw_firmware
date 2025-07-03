/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Sec2RtosMultipleChannelTest test.
// Tests Sec2 using basic semaphore methods on multiple channels, and using
// some commands on the RM command interface. Can also be used to test RC error
// recovery
//

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl95a1.h" // LW95A1_TSEC
#include "class/clb6b9.h"
#include "gpu/utility/sec2rtossub.h"
#include "gpu/include/gpusbdev.h"
#include "core/utility/errloggr.h"
#include "core/include/utility.h"

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "core/include/memcheck.h"

#define NUM_CHANNELS 2

class Sec2RtosMultipleChannelTest: public RmTest
{
public:
    Sec2RtosMultipleChannelTest();
    virtual ~Sec2RtosMultipleChannelTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    UINT64        m_gpuAddr[NUM_CHANNELS];
    UINT32*       m_cpuAddr[NUM_CHANNELS];

    LwRm::Handle  m_hObj[NUM_CHANNELS];

    LwRm::Handle  m_hSemMem[NUM_CHANNELS];
    LwRm::Handle  m_hVA[NUM_CHANNELS];
    FLOAT64       m_TimeoutMs;

    LwRm::Handle  m_hChan[NUM_CHANNELS];
    Channel *     m_pChan[NUM_CHANNELS];
    RC            Sec2RtosBasicTestCmdAndMsgQTest();
    Sec2Rtos     *m_pSec2Rtos;
};

//------------------------------------------------------------------------------
Sec2RtosMultipleChannelTest::Sec2RtosMultipleChannelTest() :
    m_TimeoutMs(Tasker::NO_TIMEOUT),
    m_pSec2Rtos(nullptr)
{
    memset(m_gpuAddr, 0, sizeof(m_gpuAddr));
    memset(m_cpuAddr, 0, sizeof(m_cpuAddr));
    memset(m_hObj,    0, sizeof(m_hObj));
    memset(m_hSemMem, 0, sizeof(m_hSemMem));
    memset(m_hVA,     0, sizeof(m_hVA));
    memset(m_hChan,   0, sizeof(m_hChan));
    memset(m_pChan,   0, sizeof(m_pChan));

    SetName("Sec2RtosMultipleChannelTest");
}

//------------------------------------------------------------------------------
Sec2RtosMultipleChannelTest::~Sec2RtosMultipleChannelTest()
{
}

//------------------------------------------------------------------------
string Sec2RtosMultipleChannelTest::IsTestSupported()
{
    if( IsClassSupported(LW95A1_TSEC) )
        return RUN_RMTEST_TRUE;
    return "LW95A1 class is not supported on current platform";
}

//------------------------------------------------------------------------------
RC Sec2RtosMultipleChannelTest::Setup()
{
    const UINT32 memSize = 0x1000;
    LwRmPtr      pLwRm;
    RC           rc;

    rc = (GetBoundGpuSubdevice()->GetSec2Rtos(&m_pSec2Rtos));
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "SEC2 RTOS not supported\n");
        CHECK_RC(rc);
    }

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();
    m_TestConfig.SetAllowMultipleChannels(true);

    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        CHECK_RC(m_TestConfig.AllocateChannel(&m_pChan[i], &m_hChan[i], LW2080_ENGINE_TYPE_SEC2));
        CHECK_RC(pLwRm->Alloc(m_hChan[i], &m_hObj[i], LW95A1_TSEC, NULL));
        CHECK_RC(pLwRm->AllocSystemMemory(&m_hSemMem[i],
                 DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
                 DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
                 DRF_DEF(OS02, _FLAGS, _COHERENCY, _UNCACHED),
                 memSize, GetBoundGpuDevice()));
        LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
        CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                              &m_hVA[i], LW01_MEMORY_VIRTUAL, &vmparams));
        CHECK_RC(pLwRm->MapMemoryDma(m_hVA[i], m_hSemMem[i], 0, memSize,
                                     LW04_MAP_MEMORY_FLAGS_NONE, &m_gpuAddr[i], GetBoundGpuDevice()));
        CHECK_RC(pLwRm->MapMemory(m_hSemMem[i], 0, memSize, (void **)&m_cpuAddr[i], 0, GetBoundGpuSubdevice()));
    }

    return rc;
}

//------------------------------------------------------------------------
RC Sec2RtosMultipleChannelTest::Run()
{
    RC            rc;
    PollArguments args;
    RC            errorRc, expectedRc;

    MEM_WR32(m_cpuAddr[0], 0x87654321);
    MEM_WR32(m_cpuAddr[1], 0xdeaddead);

    // We expect to get an MMU fault, so don't log it as an error
    CHECK_RC(StartRCTest());

    // Send some methods down channel 0
    m_pChan[0]->SetObject(0, m_hObj[0]);
    m_pChan[0]->Write(0, LW95A1_SEMAPHORE_A,
                DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (UINT32)(m_gpuAddr[0] >> 32LL)));
    m_pChan[0]->Write(0, LW95A1_SEMAPHORE_B, (UINT32)(m_gpuAddr[0] & 0xffffffffLL));
    m_pChan[0]->Write(0, LW95A1_SEMAPHORE_C,
                DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, 0x12345678));
    m_pChan[0]->Write(0, LW95A1_SEMAPHORE_D,
                DRF_DEF(95A1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                DRF_DEF(95A1, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));

    // Send some methods down channel 1
    m_pChan[1]->SetObject(0, m_hObj[1]);

    // Below lines of code send a bad FB address to cause RC error
    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_A,
                DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (UINT32)(0x00ffffff)));
    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_B, (UINT32)(0x00ffff00));

    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_C,
                DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, 0xbeefbeef));
    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_D,
                DRF_DEF(95A1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                DRF_DEF(95A1, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));

    //
    // in case of RC, just some more methods so that we have methods to pop
    // when we fault
    //
    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_A,
                DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (UINT32)(0x00ffffff)));
    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_B, (UINT32)(0x00ffff00));
    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_C,
                DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, 0xbeefbeef));

    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_A,
                DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (UINT32)(0x00ffffff)));
    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_B, (UINT32)(0x00ffff00));
    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_C,
                DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, 0xbeefbeef));

    //
    // Flush both channels. Flush the channel that will fault first to increase
    // the chances of getting a ctx switch request when faulted to test the new
    // HW.
    //
    m_pChan[1]->Flush();
    m_pChan[0]->Flush();

    //
    // Sleep for some time to allow RC recovery to finish before polling to
    // check for semaphore release
    //
    Tasker::Sleep(2000);

    // Now poll on semaphore release on the "good" channel
    args.value  = 0x12345678;
    args.pAddr  = m_cpuAddr[0];
    CHECK_RC(PollVal(&args, m_TimeoutMs));
    Printf(Tee::PriHigh,
           "Succeeded after polling on the first sempahore release!\n");

    // Clear the error on the faulted channel
    errorRc = m_pChan[1]->CheckError();
    expectedRc = RC::RM_RCH_FIFO_ERROR_MMU_ERR_FLT;

    if (errorRc != expectedRc)
    {
        Printf(Tee::PriHigh, "Unexpected error on faulting channel\n");
        return RC::UNEXPECTED_RESULT;
    }
    m_pChan[1]->ClearError();

    // Send some commands down to make sure the command interface is unaffected
    CHECK_RC(Sec2RtosBasicTestCmdAndMsgQTest());

    // Send some more methods down to channel 0
    m_pChan[0]->SetObject(0, m_hObj[0]);
    m_pChan[0]->Write(0, LW95A1_SEMAPHORE_A,
                DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (UINT32)(m_gpuAddr[0] >> 32LL)));
    m_pChan[0]->Write(0, LW95A1_SEMAPHORE_B, (UINT32)(m_gpuAddr[0] & 0xffffffffLL));
    m_pChan[0]->Write(0, LW95A1_SEMAPHORE_C,
                DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, 0xfeedfeed));
    m_pChan[0]->Write(0, LW95A1_SEMAPHORE_D,
                DRF_DEF(95A1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                DRF_DEF(95A1, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));

    // Send some more methods down channel 1
    m_pChan[1]->SetObject(0, m_hObj[1]);
    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_A,
                DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (UINT32)(m_gpuAddr[1] >> 32LL)));
    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_B, (UINT32)(m_gpuAddr[1] & 0xffffffffLL));
    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_C,
                DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, 0xbeefbeef));
    m_pChan[1]->Write(0, LW95A1_SEMAPHORE_D,
                DRF_DEF(95A1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                DRF_DEF(95A1, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));

    // Flush both channels
    m_pChan[0]->Flush();
    m_pChan[1]->Flush();

    // Poll on semaphore releases
    args.value  = 0xfeedfeed;
    args.pAddr  = m_cpuAddr[0];
    CHECK_RC(PollVal(&args, m_TimeoutMs));
    Printf(Tee::PriHigh,
           "Succeeded after polling on the second sempahore release!\n");

    args.value  = 0xbeefbeef;
    args.pAddr  = m_cpuAddr[1];
    CHECK_RC(PollVal(&args, m_TimeoutMs));
    Printf(Tee::PriHigh,
           "Succeeded after polling on the third sempahore release!\n");

    CHECK_RC(EndRCTest());

    return rc;
}

//! \brief Sec2RtosBasicTestCmdAndMsgQTest()
//!
//! This function runs the following tests as per the specified order
//!   a. Sends an empty command to SEC2.
//!   b. Waits for the SEC2 message in response to the command
//!   c. Prints the received message.
//!
//! \return OK if all the tests passed, specific RC if failed
//! \sa Run()
//-----------------------------------------------------------------------------
RC Sec2RtosMultipleChannelTest::Sec2RtosBasicTestCmdAndMsgQTest()
{
    RC                      rc = OK;
    RM_FLCN_CMD_SEC2        sec2Cmd;
    RM_FLCN_MSG_SEC2        sec2Msg;
    UINT32                  seqNum;

    memset(&sec2Cmd, 0, sizeof(RM_FLCN_CMD_SEC2));
    memset(&sec2Msg, 0, sizeof(RM_FLCN_MSG_SEC2));

    //
    // Send an empty command to SEC2.  The NULL command does nothing but
    // make SEC2 returns corresponding message.
    //
    sec2Cmd.hdr.unitId  = RM_SEC2_UNIT_NULL;
    sec2Cmd.hdr.size    = RM_FLCN_QUEUE_HDR_SIZE;

    for (int i = 0; i < 10; i++)
    {
        Printf(Tee::PriHigh,
               "%d:Sec2RtosBasicTest::%s: Submit a command to SEC2...\n",
                __LINE__, __FUNCTION__);

        // submit the command
        rc = m_pSec2Rtos->Command(&sec2Cmd,
                                  &sec2Msg,
                                  &seqNum);
        CHECK_RC(rc);

        Printf(Tee::PriHigh,
               "%d:Sec2RtosBasicTest::%s: CMD submission success, got seqNum = %u\n",
                __LINE__, __FUNCTION__, seqNum);

        //
        // In function SEC2::Command(), it waits message back if argument pMsg is
        // not NULL
        //

        // response message received, print out the details
        Printf(Tee::PriHigh,
               "%d:Sec2RtosBasicTest::%s: Received command response from SEC2\n",
                __LINE__, __FUNCTION__);
        Printf(Tee::PriHigh,
               "%d:Sec2RtosBasicTest::%s: Printing Header :-\n",
                __LINE__, __FUNCTION__);
        Printf(Tee::PriHigh,
               "unitID = 0x%x ,size = 0x%x ,ctrlFlags  = 0x%x ,seqNumID = 0x%x\n",
               sec2Msg.hdr.unitId, sec2Msg.hdr.size,
               sec2Msg.hdr.ctrlFlags, sec2Msg.hdr.seqNumId);
    }

    return rc;
}

//------------------------------------------------------------------------------
RC Sec2RtosMultipleChannelTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    // 1.
    pLwRm->UnmapMemory(m_hSemMem[0], m_cpuAddr[0], 0, GetBoundGpuSubdevice());
    pLwRm->UnmapMemoryDma(m_hVA[0], m_hSemMem[0],
                          LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr[0], GetBoundGpuDevice());

    pLwRm->Free(m_hVA[0]);
    pLwRm->Free(m_hSemMem[0]);
    pLwRm->Free(m_hObj[0]);
    FIRST_RC(m_TestConfig.FreeChannel(m_pChan[0]));

    // 2.
    pLwRm->UnmapMemory(m_hSemMem[1], m_cpuAddr[1], 0, GetBoundGpuSubdevice());
    pLwRm->UnmapMemoryDma(m_hVA[1], m_hSemMem[1],
                          LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr[1], GetBoundGpuDevice());

    pLwRm->Free(m_hVA[1]);
    pLwRm->Free(m_hSemMem[1]);
    pLwRm->Free(m_hObj[1]);
    FIRST_RC(m_TestConfig.FreeChannel(m_pChan[1]));

    m_pSec2Rtos = NULL;

    return firstRc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Sec2RtosMultipleChannelTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Sec2RtosMultipleChannelTest, RmTest,
                 "Sec2RtosMultipleChannel RM test.");
