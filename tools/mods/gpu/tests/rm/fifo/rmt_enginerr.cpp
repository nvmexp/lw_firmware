/*
* Lwidia_COPYRIGHT_BEGIN
*
* Copyright 2007-2012,2015-2019 by LWPU Corporation. All rights reserved.
* All information contained herein is proprietary and confidential
* to LWPU Corporation. Any use, reproduction, or disclosure without
* the written permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file rmt_enginerr.cpp
//! \brief Test Engine RR Scheduling
//!
//! The focus is to test the APIs not so much the underlying hardware

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "lwos.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/channel.h"
#include "core/include/tasker.h"
#include "gpu/tests/gputestc.h"
#include "class/cl90b3.h"
#include "class/cla097.h"
#include "class/cla197.h"
#include "class/cla297.h"
#include "class/cl95b1.h"
#include "class/cl95b2.h"
#include "class/cla0c0.h"
#include "class/cla1c0.h"
#include "class/cla06f.h"       // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla06fsubch.h"  // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla16f.h"       // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h"       // KEPLER_CHANNEL_GPFIFO_C
#include "class/clb06f.h"       // MAXWELL_CHANNEL_GPFIFO_A
#include "class/clc06f.h"       // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc36f.h"       // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc46f.h"       // TURING_CHANNEL_GPFIFO_A
#include "class/clc56f.h"       // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc86f.h"       // HOPPER_CHANNEL_GPFIFO_A
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrla06f.h"
#include "class/cl90b7.h"
#include "core/include/memcheck.h"

#define MAX_CHANNELS_PER_ENGINE     (3)
#define MAX_BUFFERS_PERCHANNEL      (2)

typedef struct
{
    LwU32 engClass;
    LwU32 engSubch;
} EngineEntry;

class EngineRRTest : public RmTest
{
public:
    EngineRRTest();
    virtual ~EngineRRTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    vector <LwRm::Handle>   m_hCh;
    vector <LwRm::Handle>   m_hClassObject;
    vector <Channel*>       m_pCh;
    vector <EngineEntry>    m_EngineClass;
    LwU32                   m_MaxEngines  = 0;
    LwU32                   m_MaxChannels = 0;
    LwRmPtr                 m_pLwRm;
};

static EngineEntry ALLENGINECLASSES[] =
{
    {KEPLER_A, LWA06F_SUBCHANNEL_3D},
    {KEPLER_B, LWA06F_SUBCHANNEL_3D},
    {KEPLER_C, LWA06F_SUBCHANNEL_3D},
    {KEPLER_COMPUTE_A, LWA06F_SUBCHANNEL_COMPUTE},
    {KEPLER_COMPUTE_B, LWA06F_SUBCHANNEL_COMPUTE},
    {LW95B1_VIDEO_MSVLD, 0},
    {LW95B2_VIDEO_MSPDEC,0},
    {LW90B7_VIDEO_ENCODER, 0},
    {GF100_MSPPP, 0},
};

//! \brief Class Constructor
//!
//! Indicate the name of the test.
//!
//! \sa Setup
//-----------------------------------------------------------------
EngineRRTest::EngineRRTest()
{
    SetName("EngineRRTest");
}

//! \brief Class Destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------
EngineRRTest::~EngineRRTest()
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
string EngineRRTest::IsTestSupported()
{
    if(IsClassSupported(KEPLER_CHANNEL_GPFIFO_A) || IsClassSupported(KEPLER_CHANNEL_GPFIFO_B)  ||
       IsClassSupported(KEPLER_CHANNEL_GPFIFO_C) || IsClassSupported(MAXWELL_CHANNEL_GPFIFO_A) ||
       IsClassSupported(PASCAL_CHANNEL_GPFIFO_A) || IsClassSupported(VOLTA_CHANNEL_GPFIFO_A)   ||
       IsClassSupported(TURING_CHANNEL_GPFIFO_A) || IsClassSupported(AMPERE_CHANNEL_GPFIFO_A)  ||
       IsClassSupported(HOPPER_CHANNEL_GPFIFO_A))
        return RUN_RMTEST_TRUE;
    return "No supported host classes are available on current platform";
}

//! \brief Setup all necessary state before running the test.
//!
//! Setup internal state for the test.
//!
//! \return OK if there are no errors, or an error code.
//-------------------------------------------------------------------
RC EngineRRTest::Setup()
{
    RC rc;
    LWA06F_CTRL_GPFIFO_SCHEDULE_PARAMS gpFifoSchedulParams = {0};
    CHECK_RC(InitFromJs());

    m_TestConfig.SetAllowMultipleChannels(true);

    // Build up the classes to test on this chip.
    for (UINT32 counter = 0; counter < sizeof(ALLENGINECLASSES)/sizeof(EngineEntry); counter++)
    {
        if (IsClassSupported(ALLENGINECLASSES[counter].engClass))
        {
            m_EngineClass.push_back(ALLENGINECLASSES[counter]);
        }
    }

    // Now callwalte our maxEngines and maxChannels
    m_MaxEngines  = static_cast<LwU32>(m_EngineClass.size());
    m_MaxChannels = m_MaxEngines * MAX_CHANNELS_PER_ENGINE;

    // Now resize the vectors depending on these sizes.
    m_hCh.resize(m_MaxChannels);
    m_pCh.resize(m_MaxChannels);
    m_hClassObject.resize(m_MaxChannels);

    for (UINT32 counter = 0; counter < m_MaxEngines; counter++)
    {
        // Allocate channels per engine
        for (UINT32 counterTwo = 0; counterTwo < MAX_CHANNELS_PER_ENGINE; counterTwo++)
        {
            UINT32 channelID = (counter * MAX_CHANNELS_PER_ENGINE) + counterTwo;

            CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[channelID], &m_hCh[channelID], counter));
            CHECK_RC( m_pLwRm->Alloc( m_hCh[channelID], &m_hClassObject[channelID],
                                    m_EngineClass[counter].engClass, NULL ) );

            // Schedule the channel after allocation
            gpFifoSchedulParams.bEnable = true;

            CHECK_RC(m_pLwRm->Control(m_pCh[channelID]->GetHandle(), LWA06F_CTRL_CMD_GPFIFO_SCHEDULE,
                        &gpFifoSchedulParams, sizeof(gpFifoSchedulParams)));
        }
    }

    return rc;
}

//! \brief Run the test
//!
//! Exercise and test the call to translate hChannels to ChIDs.
//-----------------------------------------------------------------------
RC EngineRRTest::Run()
{
    RC rc;

    for (UINT32 engineCounter = 0; engineCounter < m_MaxEngines; engineCounter++)
    {
        for (UINT32 counter = 0; counter < MAX_CHANNELS_PER_ENGINE; counter++)
        {
            UINT32 channelID = (engineCounter * MAX_CHANNELS_PER_ENGINE) + counter;

            for(UINT32 innerCounter = 0; innerCounter < MAX_BUFFERS_PERCHANNEL; innerCounter++)
            {
                LwU32 subch = m_EngineClass[engineCounter].engSubch;
                CHECK_RC(m_pCh[channelID]->SetObject(subch, m_hClassObject[channelID]));
                CHECK_RC(m_pCh[channelID]->Write(subch, LWA06F_FB_FLUSH, 0));
                CHECK_RC(m_pCh[channelID]->Flush());
                CHECK_RC(m_pCh[channelID]->WaitIdle(GetTestConfiguration()->TimeoutMs()));
            }
        }
    }

    return OK;
}

//! \brief Cleans up after the test.
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails.
//!
//! \sa Setup
//---------------------------------------------------------------------------
RC EngineRRTest::Cleanup()
{
    RC rc, firstRc;

    for (UINT32 counter = 0; counter < m_MaxChannels; counter++)
    {
        m_pLwRm->Free(m_hClassObject[counter]);
        FIRST_RC(m_TestConfig.FreeChannel(m_pCh[counter]));
    }

    return firstRc;
}

//---------------------------------------------------------------------------
// JS Linkage
//---------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//!
//---------------------------------------------------------------------------
JS_CLASS_INHERIT(EngineRRTest, RmTest,
                 "EngineRRTest RMTest");

