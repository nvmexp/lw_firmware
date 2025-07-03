/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2007-2017,2019 by LWPU Corporation. All rights reserved.
* All information contained herein is proprietary and confidential
* to LWPU Corporation. Any use, reproduction, or disclosure without
* the written permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file rmt_cl906f.cpp
//! \brief Fermi Host Non-Stall Interrupt test
//!
//! This test sends the LW906F_NON_STALL_INTERRUPT method
//! to cause an interrupt to be sent to RM

#include "gpu/tests/rmt_cl906f.h"
#include "core/include/jscript.h"
#include "lwos.h"
#include "core/include/platform.h"
#include "gpu/tests/gputestc.h"
#include "class/cl906f.h"
#include "class/cl906fsw.h"
#include "class/cl9097.h"
#include "class/cla097.h"  // KEPLER_A
#include "class/cla197.h"  // KEPLER_B
#include "class/cla297.h"  // KEPLER_C
#include "class/cla06f.h"  // KEPLER_CHANNEL_GPFIFO_A
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/cl007d.h"
#include "ctrl/ctrl0080.h"
#include "core/include/memcheck.h"

#define REDUCED_CHANNELS_COUNT      (3)
#define MAX_EVENTS_PER_CHID         (3)

// Quick and dirty hack to allow this test to be a background test.
// We need this for the FermiClockxxx tests
// runControl controls all instances of this test.
    Class906fTest::RunControl Class906fTest::runControl= Class906fTest::FirstTimeRunning;

//! \brief Class Constructor
//!
//! Indicate the name of the test.
//!
//! \sa Setup
//-----------------------------------------------------------------
Class906fTest::Class906fTest()
{
    SetName("Class906fTest");
    memset(m_pCh, 0, sizeof(m_pCh));
    memset(m_hCh, 0, sizeof(m_hCh));
    m_MethodPerCHID = 0;
    m_bNeedWfi      = LW_FALSE;
}

//! \brief Class Destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------
Class906fTest::~Class906fTest()
{

}

//! \brief Whether the test is supported in current environment
//!
//! Since the interrupt is very specific to fermi, we check for
//! for GPU version.
//!
//! \return True if the test can be run in current environment
//!         False otherwise
//-------------------------------------------------------------------
string Class906fTest::IsTestSupported()
{
    m_bNeedWfi = LW_FALSE;

    if(IsClassSupported(PASCAL_CHANNEL_GPFIFO_A))
    {
        m_bNeedWfi = LW_TRUE;
    }

    if(IsClassSupported(GF100_CHANNEL_GPFIFO) &&
       (IsClassSupported(FERMI_A) || IsClassSupported(KEPLER_A) ||
        IsClassSupported(KEPLER_B) || IsClassSupported(KEPLER_C)))
         return RUN_RMTEST_TRUE;
    else
         return "Supported on Fermi+ and GF100_CHANNEL_GPFIFO class should be supported";
}
//! \brief Setup all necessary state before running the test.
//!
//! Setup internal state for the test.
//!
//! \return OK if there are no errors, or an error code.
//-------------------------------------------------------------------
RC Class906fTest::Setup()
{
    RC           rc;
    UINT32       index;

    CHECK_RC(InitFromJs());

    m_TestConfig.SetAllowMultipleChannels(true);

    for(index = 0; index < CLASS906FTEST_CHANNELS_COUNT; index++)
    {
        // Allocate a channel
        CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[index], &m_hCh[index]));

        //
        // Allocate a notifier on this channel
        // The blockCount in Notifier::Allocate stumped me. Its the number of blocks
        // of 16bytes size (BLOCKSIZE in notifier.h)
        //
        m_notifier[index].Allocate(m_pCh[index], 1, &m_TestConfig);
        m_notifier[index].AllocEvent(m_hCh[index], LW906F_NOTIFIERS_NONSTALL);
    }

    return rc;
}

//! \brief Run the test
//!
//! This test will send down host NON_STALL_INTERRUPTs in the pushbuffer
//! and match with the number of events received.
//-----------------------------------------------------------------------
RC Class906fTest::Run()
{
    RC         rc;
    int  counter = 0;

    do
    {
        if (counter == 0)
        {
            counter++;
        }
        else
        {
            Cleanup();
            CHECK_RC(Setup());
        }

        CHECK_RC(RunRegularStream());
        Cleanup();

        CHECK_RC(Setup());
        CHECK_RC(RunWithHostSoftwareMethods());
        Cleanup();

        CHECK_RC(Setup());
        CHECK_RC(RunAllStreamsThenWait());
        Cleanup();

        CHECK_RC(Setup());
        CHECK_RC(RunMultiEvents());
        Cleanup();

        CHECK_RC(Setup());
        CHECK_RC(RunWithGraphics());
        Cleanup();

        CHECK_RC(Setup());
        CHECK_RC(RunWithMidStreamTampering());

    } while(runControl == Class906fTest::ContinueRunning);

    return rc;
}

RC Class906fTest::RunWithGraphics()
{
    LwRmPtr pLwRm;
    RC rc = OK;
    const UINT32 GRAPHICS_SUBCHANNEL   = 0;
    const UINT32 GRAPHICS_SUBCHANNEL_2 = 0x1;
    const UINT32 SOFTWARE_SUBCHANNEL   = 0x4;

    // Allocate graphics contexts
    LwRm::Handle hGraphics[CLASS906FTEST_CHANNELS_COUNT];
    LwRm::Handle hSoftware[CLASS906FTEST_CHANNELS_COUNT];

    for (UINT32 index = 0; index < CLASS906FTEST_CHANNELS_COUNT; index++)
    {
        if (IsClassSupported(FERMI_A))
            CHECK_RC(pLwRm->Alloc(m_hCh[index], &hGraphics[index], FERMI_A,            NULL));
        else if (IsClassSupported(KEPLER_A))
            CHECK_RC(pLwRm->Alloc(m_hCh[index], &hGraphics[index], KEPLER_A,            NULL));
        else if (IsClassSupported(KEPLER_B))
            CHECK_RC(pLwRm->Alloc(m_hCh[index], &hGraphics[index], KEPLER_B,            NULL));
        else
            CHECK_RC(pLwRm->Alloc(m_hCh[index], &hGraphics[index], KEPLER_C,            NULL));
        CHECK_RC(pLwRm->Alloc(m_hCh[index], &hSoftware[index], LW04_SOFTWARE_TEST, NULL));
    }

    // Blast down methods
    for (UINT32 index = 0; index < CLASS906FTEST_CHANNELS_COUNT; index++)
    {
        //
        // Thats right send a software class setobject followed by a graphics setobject
        // we want to make sure RM doesnt trip when it sees a cache1 full of engine methods
        //
        // SETOBJECTS
        CHECK_RC(m_pCh[index]->SetObject(SOFTWARE_SUBCHANNEL,   hSoftware[index]));
        CHECK_RC(m_pCh[index]->SetObject(GRAPHICS_SUBCHANNEL,   hGraphics[index]));
        if (IsClassSupported(FERMI_A))
            CHECK_RC(m_pCh[index]->SetObject(GRAPHICS_SUBCHANNEL_2, hGraphics[index]));

        // METHOD STREAMS
        for (UINT32 innerIndex = 0; innerIndex < m_MethodPerCHID; innerIndex++)
        {
            CHECK_RC(m_pCh[index]->Write(GRAPHICS_SUBCHANNEL,    LW9097_SET_SPARE_NOOP03, 0));
            if(m_bNeedWfi)
            {
                CHECK_RC(m_pCh[index]->Write(GRAPHICS_SUBCHANNEL, LWC06F_WFI, DRF_DEF(C06F, _WFI, _SCOPE, _ALL)));
            }
            CHECK_RC(m_pCh[index]->Write(GRAPHICS_SUBCHANNEL, LW9097_SET_SHADER_EXCEPTIONS,
                                         DRF_DEF(9097, _SET_SHADER_EXCEPTIONS, _ENABLE, _TRUE)));
            if (IsClassSupported(FERMI_A))
                CHECK_RC(m_pCh[index]->Write(GRAPHICS_SUBCHANNEL_2,    LW9097_SET_SPARE_NOOP03, 0));
            if(m_bNeedWfi)
            {
                CHECK_RC(m_pCh[index]->Write(GRAPHICS_SUBCHANNEL, LWC06F_WFI, DRF_DEF(C06F, _WFI, _SCOPE, _ALL)));
            }
            CHECK_RC(m_pCh[index]->Write(GRAPHICS_SUBCHANNEL, LW9097_SET_SHADER_EXCEPTIONS,
                                         DRF_DEF(9097, _SET_SHADER_EXCEPTIONS, _ENABLE, _FALSE)));
        }
        CHECK_RC(WriteStream(index, false));
        CHECK_RC(WaitStream(index));
    }

    // Free graphics contexts
    for (UINT32 index = 0; index < CLASS906FTEST_CHANNELS_COUNT; index++)
    {
        pLwRm->Free(hGraphics[index]);
    }

    return rc;
}

RC Class906fTest::RunRegularStream()
{
    RC rc = OK;

    for (UINT32 index = 0; index < CLASS906FTEST_CHANNELS_COUNT; index++)
    {
        CHECK_RC(WriteStream(index, false));
        CHECK_RC(WaitStream(index));
    }

    return rc;
}

RC Class906fTest::RunWithMidStreamTampering()
{
    RC rc = OK;

    for (UINT32 index = 0; index < CLASS906FTEST_CHANNELS_COUNT; index++)
    {
        CHECK_RC(WriteStream(index, true));
        CHECK_RC(WaitStream(index));
    }

    return rc;
}

RC Class906fTest::RunAllStreamsThenWait()
{
    RC rc = OK;
    UINT32 index = 0;

    for (index = 0; index < CLASS906FTEST_CHANNELS_COUNT; index++)
    {
        CHECK_RC(WriteStream(index, false));
    }

    for (index = 0; index < CLASS906FTEST_CHANNELS_COUNT; index++)
    {
        CHECK_RC(WaitStream(index));
    }

    return rc;
}

RC Class906fTest::RunWithHostSoftwareMethods()
{
    RC rc = OK;
    UINT32 index = 0;

    for (index = 0; index < CLASS906FTEST_CHANNELS_COUNT; index++)
    {
        CHECK_RC(WriteStream(index, false));
        CHECK_RC(WriteHostSoftwareMethod(index));
        CHECK_RC(WaitStream(index));
    }

    return rc;
}

RC Class906fTest::RunMultiEvents()
{
    RC rc = OK;
    UINT32 index = 0;
    UINT32 innerIndex = 0;

    for (index = 0; index < CLASS906FTEST_CHANNELS_COUNT; index++)
    {
        for (innerIndex = 0; innerIndex < MAX_EVENTS_PER_CHID; innerIndex++)
        {
            CHECK_RC(WriteStream(index, false));
            CHECK_RC(WaitStream(index));
        }
    }

    for (index = 0; index < CLASS906FTEST_CHANNELS_COUNT; index++)
    {
        CHECK_RC(WaitStream(index));
    }

    return rc;
}

//! \brief Cleans up after the test.
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails.
//!
//! \sa Setup
//---------------------------------------------------------------------------
RC Class906fTest::Cleanup()
{
    RC rc, firstRc;
    for(UINT32 index = 0; index < CLASS906FTEST_CHANNELS_COUNT; index++)
    {
        // Notifiers
        m_notifier[index].Free();

        // Channels
        FIRST_RC(m_TestConfig.FreeChannel(m_pCh[index]));
        m_pCh[index] = NULL;
        m_hCh[index] = 0;
    }

    return firstRc;
}

RC Class906fTest::tamperChannelProperties(UINT32 ChID)
{
    LwRmPtr pLwRm;
    RC      rc = OK;

    // Program engine timeslice of 256 microseconds
    LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_PARAMS params = {0};
    params.hChannel = m_hCh[ChID];
    params.property = LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_ENGINETIMESLICEINMICROSECONDS;
    params.value    = 0x100ULL;
    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_FIFO_SET_CHANNEL_PROPERTIES,
                            &params,
                            sizeof(params)));

    // Program pbdma timeslice of 256 microseconds
    params.hChannel = m_hCh[ChID];
    params.property = LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_PBDMATIMESLICEINMICROSECONDS;
    params.value    = 0x100ULL;
    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_FIFO_SET_CHANNEL_PROPERTIES,
                            &params,
                            sizeof(params)));

    return rc;
}

RC Class906fTest::WriteStream(UINT32 ChID, bool tamperStream)
{
    RC rc = OK;

    // Send in the first batch of methods
    m_notifier[ChID].Clear(0);

    CHECK_RC(m_pCh[ChID]->Write(0, LW906F_SEMAPHOREA,
                                 LwU64_HI32(m_notifier[ChID].GetCtxDmaOffsetGpu(0)))); // What is the SLI correct way?
    CHECK_RC(m_pCh[ChID]->Write(0, LW906F_SEMAPHOREB,
                                 LwU64_LO32(m_notifier[ChID].GetCtxDmaOffsetGpu(0))));
    CHECK_RC(m_pCh[ChID]->Write(0, LW906F_SEMAPHOREC, 0x0));

    if (tamperStream == true)
    {
        CHECK_RC(tamperChannelProperties(ChID));
    }

    CHECK_RC(m_pCh[ChID]->Write(0, LW906F_SEMAPHORED,
                                 (DRF_DEF(906F, _SEMAPHORED, _OPERATION,    _RELEASE)|
                                  DRF_DEF(906F, _SEMAPHORED, _RELEASE_WFI,  _EN     )|
                                  DRF_DEF(906F, _SEMAPHORED, _RELEASE_SIZE, _16BYTE ))));

    CHECK_RC(m_pCh[ChID]->Write(0, LW906F_FB_FLUSH, 0));

    CHECK_RC(m_pCh[ChID]->Write(0, LW906F_NON_STALL_INTERRUPT, 0));

    CHECK_RC(m_pCh[ChID]->Flush());

    return rc;
}

RC Class906fTest::WriteHostSoftwareMethod(UINT32 ChID)
{
    RC rc = OK;

    CHECK_RC(m_pCh[ChID]->Write(0, LW906F_ILLEGAL, 0xffeeffee));
    CHECK_RC(m_pCh[ChID]->Flush());

    return rc;
}

RC Class906fTest::WaitStream(UINT32 ChID)
{
    RC rc = OK;

    CHECK_RC(m_notifier[ChID].WaitAwaken(m_TestConfig.TimeoutMs()));
    MASSERT(m_notifier[ChID].IsSet(0) == true);

    return rc;
}

//---------------------------------------------------------------------------
// JS Linkage
//---------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//!
//---------------------------------------------------------------------------
JS_CLASS_INHERIT(Class906fTest, RmTest,
                 "Class906fTest RMTest");
CLASS_PROP_READWRITE(Class906fTest, MethodPerCHID, UINT32,
                     "Max Engine Methods Per channel ID");

