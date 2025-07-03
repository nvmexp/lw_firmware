/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_hostscheduling.cpp
//! \brief Host scheduling Test
//!

#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "random.h"
#include "gpu/utility/surf2d.h"
#include "core/utility/errloggr.h"

#include "class/cl906f.h"               // GF100_CHANNEL_GPFIFO
#include "class/cla06f.h"               // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla06fsubch.h"          // KEPLER_CHANNEL_GPFIFO_A subchannel Mappings
#include "class/cla16f.h"               // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h"             // KEPLER_CHANNEL_GPFIFO_C
#include "class/clb06f.h"               // MAXWELL_CHANNEL_GPFIFO_A
#include "class/cl208f.h"               // LW20_SUBDEVICE_DIAG
#include "class/cl902d.h"               // FERMI_TWOD_A
#include "ctrl/ctrl5080.h"
#include "ctrl/ctrl906f.h"
#include "ctrl/ctrla06f.h"
#include "ctrl/ctrl2080.h"              // LW20_SUBDEVICE
#include "ctrl/ctrl2080/ctrl2080fifo.h"
#include "ctrl/ctrl208f.h"              // LW20_SUBDEVICE_DIAG
#include "ctrl/ctrl208f/ctrl208ffifo.h"
#include "class/cl007d.h"               // LW04_SOFTWARE_TEST
#include "gpu/tests/rmtest.h"
#include "core/include/tasker.h"
#include "gpu/include/gralloc.h"

#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif

// Channel setups
#define MAX_CHANNELS   5

#define CHN_GR1        0
#define CHN_GR2        1
#define CHN_NULL       4

// Test Masks
#define ENGINE_TEST_MASK   (BIT(0) | BIT(1))
#define API_TEST_MASK      (BIT(0) | BIT(4))
#define GR_MASK            (BIT(0) | BIT(1))

// subchannels
// Kepler has moved to a system of dedicated subchannel mappings
// Subchannels were chosen based on cla06fsubch.h in the class SDK
// or following KMDs mappings, or chosen from the remaining options.
#define SUBCH_CTRL      0                      // GPFIFO
#define SUBCH_2D        LWA06F_SUBCHANNEL_2D   // 2D channel
#define SUBCH_NOTIFY    7

// RC Methods
#define ILWALID_METHOD  0xF8
#define ILWALID_DATA    0xFFFFFFFF

// 2D info
#define TWOD_PITCH      (1024*sizeof(LwU32))
#define TWOD_SIZE       (1024*4)
#define TWOD_LINES      (TWOD_SIZE/TWOD_PITCH)

// Describes the type of channel
typedef enum _channelType
{
    NONE,
    GRAPHICS
} CHANNEL_TYPE;

class ScheduleTest: public RmTest
{
    friend class ScheduleChannel;
public:
    ScheduleTest();
    virtual         ~ScheduleTest();

    virtual string  IsTestSupported();

    virtual RC      Setup();
    virtual RC      Run();
    virtual RC      Cleanup();

private:
    // Tests

    RC              disableEnableTest();
    RC              stopChannelTest();
    RC              restartChannelTest();
    RC              randomTest();
    RC              rcFaultTest();

    // Utility
    RC              stopChannel(UINT32 channelMask);
    RC              restartChannel(UINT32 channelMask);

    /*
     * stateMask refers to the enable state of each channel.
     * A bit set to 1 means that the corresponding channel is enabled.
     */
    RC              getChannelEnableState(UINT32* stateMask );

    // Channel stuff
    class ScheduleChannel
    {
    public:
            ScheduleChannel(ScheduleTest* test, LwRm::Handle client);
            ~ScheduleChannel();

            RC                  setup(CHANNEL_TYPE chType);
            RC                  cleanup();
            RC                  provideWork();
            RC                  getEnableState(bool* enabled, LwRm::Handle hSubDevDiag);
            RC                  waitForIdle(UINT32 timeout);
            RC                  fault();
            RC                  writeGrMethods();
            LwRm::Handle        getHandle();

            GrClassAllocator*   pAlloc;
            bool                bEnabled;
            Channel*            pCh;
            Surface2D           chSurf;
            LwRm::Handle        hClient;
            CHANNEL_TYPE        channelType;
            ScheduleTest*       schedTest;
    };

    typedef vector<ScheduleChannel*>  v_schedChan;

    v_schedChan     channelList;

};

ScheduleTest::ScheduleChannel::ScheduleChannel(ScheduleTest* test, LwRm::Handle client )
{
    hClient     = client;
    channelType = NONE;
    pCh         = NULL;
    pAlloc      = NULL;
    schedTest   = test;

}

ScheduleTest::ScheduleChannel::~ScheduleChannel()
{
}
//!
//! \brief Sets up a schedule channel.
//!
//! Allocates memory and objects for a channel to use.
//!
//!
//! \return OK if the methods are written correctly.
//-----------------------------------------------------------------------------
RC ScheduleTest::ScheduleChannel::setup(CHANNEL_TYPE chType)
{
    RC          rc;
    LwRmPtr     pLwRm;

    if(!pCh)
    {
        Printf(Tee::PriHigh, "ScheduleChannel: %s failed. channel is unallocated.\n ", __FUNCTION__ );
        return RC::SOFTWARE_ERROR;
    }

    if(channelType != NONE)
    {
        Printf(Tee::PriHigh, "ScheduleChannel: %s failed.  Channel setup is already complete.\n ", __FUNCTION__ );
        return RC::SOFTWARE_ERROR;
    }
    // Setup the channels.
    channelType = chType;

    // Setup any channel types by use and any restrictions.
    // If we change the video or two-d classes for future gpus, we can fail the 2d section
    // and update as needed.
    if(channelType == GRAPHICS)
    {
        pAlloc = new TwodAlloc();
        CHECK_RC(pAlloc->Alloc(pCh->GetHandle(), schedTest->GetBoundGpuDevice()));
        pAlloc->SetOldestSupportedClass(FERMI_TWOD_A);
        pAlloc->SetNewestSupportedClass(FERMI_TWOD_A);
    }

    if(channelType != NONE)
    {
        chSurf.SetWidth(100);
        chSurf.SetHeight(100);
        chSurf.SetLayout(Surface2D::Pitch);
        chSurf.SetColorFormat(ColorUtils::ColorDepthToFormat(16));
        if(schedTest->GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_PAGING) )
            chSurf.SetAddressModel(Memory::Paging);
        chSurf.SetLocation(Memory::Fb);
        chSurf.SetProtect(Memory::ReadWrite);
        CHECK_RC(chSurf.Alloc(schedTest->GetBoundGpuDevice()));
        CHECK_RC(chSurf.Map(schedTest->GetBoundGpuSubdevice()->GetSubdeviceInst()));
        CHECK_RC(chSurf.BindGpuChannel(pCh->GetHandle()));
    }

    return OK;
}

RC ScheduleTest::ScheduleChannel::cleanup()
{
    LwRmPtr pLwRm;

    if(pAlloc)
    {
        pAlloc->Free();
        delete pAlloc;
        pAlloc = NULL;
    }

    chSurf.Free();
    return OK;

}
LwRm::Handle ScheduleTest::ScheduleChannel::getHandle()
{
    if(!pCh)
        return 0;

    return pCh->GetHandle();
}

//!
//! \brief Write a set of methods to the channel based on their type.
//!
//! Graphics channels will draw a line, video will write to memory.
//!
//! \return OK if the methods are written correctly.
//-----------------------------------------------------------------------------
RC ScheduleTest::ScheduleChannel::provideWork()
{
    if(channelType == GRAPHICS)
    {
        return writeGrMethods();
    }
    return RC::UNSUPPORTED_FUNCTION;
}

//!
//! \brief Write a set of graphics methods to the provided channel
//!
//! Write a set of 2D methods to the channel to draw a line.
//!
//! \return OK if the methods are written correctly.
//-----------------------------------------------------------------------------
RC ScheduleTest::ScheduleChannel::writeGrMethods()
{
    RC      rc        = OK;

    if(!pAlloc->IsSupported(schedTest->GetBoundGpuDevice()))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(pCh->SetObject(SUBCH_2D, pAlloc->GetHandle()));

    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_SET_DST_MEMORY_LAYOUT, LW902D_SET_DST_MEMORY_LAYOUT_V_PITCH));
    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_SET_DST_PITCH, TWOD_PITCH));
    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_SET_DST_WIDTH, TWOD_PITCH/sizeof(LwU32)));

    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_SET_SRC_MEMORY_LAYOUT, LW902D_SET_SRC_MEMORY_LAYOUT_V_PITCH));
    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_SET_SRC_PITCH, TWOD_PITCH));
    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_SET_SRC_WIDTH, TWOD_PITCH/sizeof(LwU32)));

    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_SET_CLIP_ENABLE, DRF_DEF(902D, _SET_CLIP_ENABLE, _V, _FALSE)));

    // use client's 2D SubChannel to draw one line in the given "color"
    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_SET_DST_OFFSET_UPPER, (LwU64_HI32(chSurf.GetCtxDmaOffsetGpu()) && 0xFF)));

    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_SET_DST_OFFSET_LOWER, (LwU64_LO32(chSurf.GetCtxDmaOffsetGpu()))));

    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_RENDER_SOLID_PRIM_MODE, LW902D_RENDER_SOLID_PRIM_MODE_V_LINES));

    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT,
        LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8R8G8B8));

    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_SET_RENDER_SOLID_PRIM_COLOR, 0xFF));

    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_RENDER_SOLID_PRIM_POINT_SET_X(0), 0));
    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_RENDER_SOLID_PRIM_POINT_Y(0), 0));
    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_RENDER_SOLID_PRIM_POINT_SET_X(1), (TWOD_PITCH/sizeof(UINT32)))); // hopefully endpoint dropped
    CHECK_RC(pCh->Write(SUBCH_2D, LW902D_RENDER_SOLID_PRIM_POINT_Y(1), 0));

    CHECK_RC(pCh->Flush());
    return rc;
}

//! \brief Get the current enable state of all channels.
//!
//!
//! \return OK if no errors oclwred.
RC ScheduleTest::ScheduleChannel::getEnableState(bool* enabled, LwRm::Handle hSubDevDiag)
{
    RC rc = OK;
    LW208F_CTRL_FIFO_GET_CHANNEL_STATE_PARAMS channelParams = {0};
    LwRmPtr pLwRm;

    // if the channel type is none, we won't have allocated a channel to have state.
    if(channelType == NONE)
    {
        *enabled = false;
        return OK;
    }

    channelParams.hChannel = getHandle();
    channelParams.hClient = hClient;
    rc = pLwRm->Control(hSubDevDiag, LW208F_CTRL_CMD_FIFO_GET_CHANNEL_STATE,
                        &channelParams, sizeof(channelParams));

    if(rc == OK )
        *enabled = channelParams.bEnabled != LW_FALSE;
    else
        Printf(Tee::PriHigh, "ScheduleTest: %s failed\n ", __FUNCTION__);

    return rc;
}
RC ScheduleTest::ScheduleChannel::waitForIdle(UINT32 timeout)
{
    RC rc = OK;
    rc = pCh->WaitIdle(timeout);
    CHECK_RC(pCh->CheckError());

    return rc;
}

RC ScheduleTest::ScheduleChannel::fault()
{
    RC rc = OK;
    rc = (pCh->Write(0, ILWALID_METHOD,ILWALID_DATA));

        Printf(Tee::PriHigh,
               "ScheduleTest: RC FAULT received: %s\n", rc.Message());
        //rc = RC::SOFTWARE_ERROR;
        // make sure we restore RC behavior.
       // return rc;

    rc = (pCh->Flush());

        Printf(Tee::PriHigh,
               "ScheduleTest: RC received: %s\n", rc.Message());
        //rc = RC::SOFTWARE_ERROR;
        // make sure we restore RC behavior.
        //return rc;

    return rc;
}
//! \brief ScheduleTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
ScheduleTest::ScheduleTest()
{

    Printf(Tee::PriHigh, "ScheduleTest: Entering %s\n", __FUNCTION__);
    SetName("ScheduleTest");
}

//! \brief ScheduleTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
ScheduleTest::~ScheduleTest()
{
    Printf(Tee::PriHigh, "ScheduleTest: Entering %s\n", __FUNCTION__);
}

//! \brief Whether or not the test is supported in the current environment
//!
//! Lwrrently only Kepler and Fermi support scheduling. We also require
//! support for the a 2D graphics class.
//!
//! Note: KEPLER_CHANNEL_GPFIFO_C should be supported at some point.
//!
//! \return True if we lwrrently support Kepler or Fermi GPFIFO
//-----------------------------------------------------------------------------
string ScheduleTest::IsTestSupported()
{
    LwRmPtr pLwRm;
    TwodAlloc       m_TwoD;
    m_TwoD.SetOldestSupportedClass(FERMI_TWOD_A);

    if( (pLwRm->IsClassSupported(KEPLER_CHANNEL_GPFIFO_A, GetBoundGpuDevice())  ||
         pLwRm->IsClassSupported(KEPLER_CHANNEL_GPFIFO_B, GetBoundGpuDevice())  ||
         pLwRm->IsClassSupported(KEPLER_CHANNEL_GPFIFO_C, GetBoundGpuDevice())  ||
         pLwRm->IsClassSupported(MAXWELL_CHANNEL_GPFIFO_A, GetBoundGpuDevice())  ||
         pLwRm->IsClassSupported(GF100_CHANNEL_GPFIFO, GetBoundGpuDevice()) )   &&
         m_TwoD.IsSupported(GetBoundGpuDevice()))
        return RUN_RMTEST_TRUE;

    return "Current platform fails to support Fermi or Kepler channel classes or a two-d class";

}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsSupported
//-----------------------------------------------------------------------------
RC ScheduleTest::Setup()
{
    RC           rc;
    UINT32       i;
    LwRm::Handle m_hClient, han;
    ScheduleChannel* chan;
    UINT32       clientCount = LwRmPtr::GetValidClientCount();

    Printf(Tee::PriAlways, "ScheduleTest: Entering %s\n", __FUNCTION__);
    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_TestConfig.SetAllowMultipleChannels(true);

    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);

    for(i = 0; i < MAX_CHANNELS; i++)
    {
        m_hClient = LwRmPtr(i%clientCount).Get()->GetClientHandle();
        chan = new ScheduleChannel(this, m_hClient);
        // Keep one null channel
        if(i != MAX_CHANNELS-1)
        {
            rc = GetTestConfiguration()->AllocateChannel(&(chan->pCh), &han);
            if (rc != RC::OK)
            {
                delete chan;
                return rc;
            }
        }
        channelList.push_back(chan);
    }

    // Setup the channels we plan on using.
    // Use 0 & 1 for graphics, 2 for video
    channelList[CHN_GR1]->setup(GRAPHICS);
    channelList[CHN_GR2]->setup(GRAPHICS);

    return rc;
}

//! \brief Run() : The body of this test.
//!
//! In this function, it calls several  sub-tests.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC ScheduleTest::Run()
{
    RC rc = OK;

    CHECK_RC(stopChannelTest());
    CHECK_RC(restartChannelTest());
    CHECK_RC(disableEnableTest());
    CHECK_RC(rcFaultTest());
    CHECK_RC(randomTest());

    return rc;
}

//! \brief Stop a set of channels.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC ScheduleTest::stopChannel(UINT32 channelMask)
{
    LW2080_CTRL_FIFO_DISABLE_CHANNELS_PARAMS       disableChannelParams = {0};
    LwRmPtr pLwRm;
    UINT32  i = 0;
    UINT32  j = 0;
    UINT32  status;

    disableChannelParams.bDisable = LW2080_CTRL_FIFO_DISABLE_CHANNEL_TRUE;
    disableChannelParams.numChannels           = 0;

    // Build a list of channels to stop
    for(i = 0; i < channelList.size(); i++ )
    {
        if(channelMask & BIT(i))
        {
            disableChannelParams.hClientList[j]        = channelList[i]->hClient;
            disableChannelParams.hChannelList[j]       = channelList[i]->getHandle();
            disableChannelParams.numChannels++;
            j++;
        }
    }

    // Stop the channel
    status = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()), LW2080_CTRL_CMD_FIFO_DISABLE_CHANNELS,
                            &disableChannelParams, sizeof(disableChannelParams));

    if(status != LW_OK)
    {
        Printf(Tee::PriHigh, "ScheduleTest %s: Channel stop failed with channelMask %x\n ", __FUNCTION__, channelMask);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//! \brief Restart a set of provided channels.
//!
//! \sa
//-----------------------------------------------------------------------------
RC ScheduleTest::restartChannel(UINT32 channelMask )
{
    LW2080_CTRL_FIFO_DISABLE_CHANNELS_PARAMS    disableChannelParams = {0};
    LwRmPtr pLwRm;
    UINT32  i, j = 0;
    UINT32  status;

    disableChannelParams.bDisable       = LW2080_CTRL_FIFO_DISABLE_CHANNEL_FALSE;
    disableChannelParams.numChannels    = 0;

    // Build a list of channels to stop
    for(i = 0; i < channelList.size(); i++ )
    {
        if(channelMask & BIT(i))
        {
            disableChannelParams.hClientList[j]        = channelList[i]->hClient;
            disableChannelParams.hChannelList[j]       = channelList[i]->getHandle();
            disableChannelParams.numChannels++;
            j++;
        }
    }

    // restart the channel
    status = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()), LW2080_CTRL_CMD_FIFO_DISABLE_CHANNELS,
                            &disableChannelParams, sizeof(disableChannelParams));

    if(status != LW_OK)
    {
        Printf(Tee::PriHigh, "ScheduleTest %s: Channel restart failed:  channelMask %x\n ", __FUNCTION__, channelMask);
        return RC::SOFTWARE_ERROR;
    }

    return OK;

}

//! \brief Get the current enable state of all channels.
//!
//!
//! \return OK if no errors oclwred.
RC ScheduleTest::getChannelEnableState(UINT32* stateMask)
{
    RC      rc = OK;
    bool    enabled;
    UINT32  i;
    LwRm::Handle hSubDevDiag;

    hSubDevDiag = GetBoundGpuSubdevice()->GetSubdeviceDiag();

    *stateMask = 0;
    for(i = 0; i < channelList.size(); i++)
    {
       rc = channelList[i]->getEnableState(&enabled, hSubDevDiag);
       if(rc == OK && enabled )
            *stateMask |= BIT(i);
    }

    if(rc != OK)
    {
        Printf(Tee::PriHigh, "ScheduleTest: %s failed:  %x\n ", __FUNCTION__, *stateMask);
    }

    return rc;
}

//! \brief disable a channel and then enable it.
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------

RC ScheduleTest::disableEnableTest()
{
    RC              rc = OK;
    UINT32          state;
    UINT32          i;

    Printf(Tee::PriAlways, "ScheduleTest running %s\n. ", __FUNCTION__);

    // Make sure all channels are enabled before the test
    CHECK_RC(getChannelEnableState( &state));
    if((state & ENGINE_TEST_MASK) != ENGINE_TEST_MASK)
        CHECK_RC(restartChannel(state ^ ENGINE_TEST_MASK)); // Restart any disabled channels and remove the set of channels lwrrently enabled.

    // Prepare the set of channels we will use, & provide some work to preempt on.
    for(i = 0; i < channelList.size(); i++)
        channelList[i]->provideWork();

    // Stop all the channels we are using
    CHECK_RC(stopChannel(ENGINE_TEST_MASK));
    CHECK_RC(getChannelEnableState( &state ));
    if((state & ENGINE_TEST_MASK) !=0)
    {
        Printf(Tee::PriHigh,
               "ScheduleTest: Channels failed to disable.  Mask %x\n", state);
        return RC::SOFTWARE_ERROR;
    }
    // restart the non preempted channels.

    CHECK_RC(restartChannel(BIT(CHN_GR2)));
    CHECK_RC(getChannelEnableState( &state ));

    UINT32 timeoutMs = static_cast<UINT32>(GetTestConfiguration()->TimeoutMs());

    channelList[CHN_GR2]->waitForIdle(timeoutMs);

    CHECK_RC(restartChannel(BIT(CHN_GR1)));
    CHECK_RC(getChannelEnableState( &state ));

    if( (state & ENGINE_TEST_MASK) != ENGINE_TEST_MASK )
    {
        Printf(Tee::PriHigh,
               "ScheduleTest: failed to restart channels, state:0x%x!\n", state);
        return RC::SOFTWARE_ERROR;
    }

    channelList[CHN_GR1]->waitForIdle(timeoutMs);

    return rc;
}

//! \brief test LW2080 control call to stop the channels
//!
//! Tests the control call for state changes and error conditions.
//!
//! \return OK if the test passes.
RC ScheduleTest::stopChannelTest()
{
    RC              rc = OK;
    LwRmPtr         pLwRm;
    UINT32          baseState, state;

    Printf(Tee::PriAlways, "ScheduleTest running %s\n. ", __FUNCTION__);

    LW2080_CTRL_FIFO_DISABLE_CHANNELS_PARAMS       stopChannelParams = {0};
    stopChannelParams.bDisable = LW2080_CTRL_FIFO_DISABLE_CHANNEL_TRUE;

    // Make sure channel 0 is running before we run the test.
    CHECK_RC(restartChannel(BIT(0)));
    CHECK_RC(getChannelEnableState( &state));

    if(!(state & BIT(0)))
    {
         Printf(Tee::PriHigh, "ScheduleTest: %s: Failed to start channel 0.\n", __FUNCTION__);
         return RC::SOFTWARE_ERROR;
    }
    // Stopping 0 channels

    rc = stopChannel(0);
    if(rc == OK)
    {
        Printf(Tee::PriHigh, "StopChannel 0 channel test failed.\n");
        return RC::SOFTWARE_ERROR;
    }

    // numChannels should fail if it is greater than the max
    stopChannelParams.numChannels = LW2080_CTRL_FIFO_DISABLE_CHANNELS_MAX_ENTRIES+1;
    rc =  pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()), LW2080_CTRL_CMD_FIFO_DISABLE_CHANNELS,
                            &stopChannelParams, sizeof(stopChannelParams));
    if(rc == OK)
    {
        Printf(Tee::PriHigh, "StopChannel Max channel count test failed.\n");
        return RC::SOFTWARE_ERROR;
    }

    // Should fail on a mangled handle.
    CHECK_RC(getChannelEnableState( &baseState));
    stopChannelParams.hClientList[0]        = channelList[0]->hClient;
    stopChannelParams.hChannelList[0]       = 0xdead;
    stopChannelParams.numChannels = 1;
    rc =  pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()), LW2080_CTRL_CMD_FIFO_DISABLE_CHANNELS,
                            &stopChannelParams, sizeof(stopChannelParams));

    if(rc == OK)
    {
        Printf(Tee::PriHigh, "StopChannel Mangled handle test failed.\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(getChannelEnableState( &state));
    if(baseState != state)
    {
        Printf(Tee::PriHigh, "StopChannel Mangled handle state test failed.\n");
        return RC::SOFTWARE_ERROR;
    }

    // A proper stop should work without affecting other channels.
    CHECK_RC(getChannelEnableState( &baseState));
    CHECK_RC(stopChannel(BIT(0)));

    rc = getChannelEnableState( &state);
    if( (state ^ baseState) != BIT(0))
    {
        Printf(Tee::PriHigh,
            "StopChannel: Incorrect channel stop: initial state: %x current state %x\n",
             baseState, state );
        return RC::SOFTWARE_ERROR;
    }

    // We should have no issues with stopping a stopped channel, but the state shouldn't change.
    baseState = state;
    CHECK_RC(stopChannel(BIT(0)));
    CHECK_RC(getChannelEnableState( &state));

    if(baseState != state )
    {
        Printf(Tee::PriHigh, "StopChannel double stop test failed \n");
        return RC::SOFTWARE_ERROR;
    }

    if(stopChannel(BIT(CHN_NULL)) == OK)
    {
        Printf(Tee::PriHigh, "StopChannel failed handling NULL channel \n");
    }

    // return the channels used to a known state.
    CHECK_RC(restartChannel(BIT(0)));
    Printf(Tee::PriAlways, "ScheduleTest end%s\n. ", __FUNCTION__);

    return rc;
}

//! \brief test LW2080 control call to restart the channels
//!
//! Tests the control call for state changes and error conditions.
//!
//! \return OK if the test passes.
RC ScheduleTest::restartChannelTest()
{
    RC              rc = OK;
    LwRmPtr         pLwRm;
    UINT32          baseState, state = 0;

    Printf(Tee::PriAlways, "ScheduleTest running %s\n. ", __FUNCTION__);

    LW2080_CTRL_FIFO_DISABLE_CHANNELS_PARAMS       restartChannelParams = {0};
    restartChannelParams.bDisable = LW2080_CTRL_FIFO_DISABLE_CHANNEL_FALSE;

    // disable channel 0 if it isn't already disabled
    CHECK_RC(stopChannel(BIT(0)));
    CHECK_RC(getChannelEnableState( &state));
    if(state & BIT(0))
    {
        Printf(Tee::PriHigh, "Channel 0 failed to stop: State %x.\n", state);
        return RC::SOFTWARE_ERROR;
    }

    // test for no args.
    rc = restartChannel(0);
    if(rc == OK)
    {
        Printf(Tee::PriHigh, "restartChannel 0 channel test failed.\n");
        return RC::SOFTWARE_ERROR;
    }

    // numChannels should fail if it is greater than the max
    restartChannelParams.numChannels       = LW2080_CTRL_FIFO_DISABLE_CHANNELS_MAX_ENTRIES+1;
    rc =  pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()), LW2080_CTRL_CMD_FIFO_DISABLE_CHANNELS,
                            &restartChannelParams, sizeof(restartChannelParams));
    if(rc == OK)
    {
        Printf(Tee::PriHigh, "RestartChannel Max channel count test failed.\n");
        return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(getChannelEnableState( &baseState));
    // Should fail on a mangled handle.
    restartChannelParams.hClientList[0]        = channelList[0]->hClient;
    restartChannelParams.hChannelList[0]       = 0xdead;
    restartChannelParams.numChannels = 1;
    rc =  pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()), LW2080_CTRL_CMD_FIFO_DISABLE_CHANNELS,
                            &restartChannelParams, sizeof(restartChannelParams));
    if(rc == OK)
    {
        Printf(Tee::PriHigh, "restartChannel Mangled handle test failed.\n" );
        return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(getChannelEnableState( &state));
    if(state !=baseState)
    {
        Printf(Tee::PriHigh, "restartChannel Mangled handle test failed.\n" );
        return RC::SOFTWARE_ERROR;
    }

    // A proper restart should work.
    CHECK_RC(getChannelEnableState( &baseState));
    CHECK_RC(restartChannel(BIT(0)));
    CHECK_RC(getChannelEnableState( &state));
    if((baseState ^ state) != BIT(0))
    {
        Printf(Tee::PriHigh, "restartChannel test failed to restart channel 0.\n" );
        return RC::SOFTWARE_ERROR;
    }
    // starting it again should fail

    baseState = state;
    CHECK_RC(restartChannel(BIT(0)));
    CHECK_RC(getChannelEnableState( &state));
    if(baseState != state)
    {
        Printf(Tee::PriHigh, "restartChannel double restart test failed \n");
        return RC::SOFTWARE_ERROR;
    }

    if(stopChannel(BIT(CHN_NULL)) == OK)
    {
        Printf(Tee::PriHigh, "RestartChannel failed handling NULL channel \n");
    }

    return rc;
}

//! \brief Randomizing stress test
//!
//! Handle a bunch of tests points randomly
//!
//! \return OK if test passes
RC ScheduleTest::randomTest()
{
    UINT32 test;
    UINT32 testCase, op, mask;
    RC rc = OK;

    Printf(Tee::PriAlways, "ScheduleTest running %s\n. ", __FUNCTION__);

    for(test = 0; test < 50; test++)
    {
        // Get a random number.  We'll use this to pick operations
        // and which channels to perform them on.
        testCase = DefaultRandom::GetRandom(0, 64-1);
        // bits 0-5 will be out channel list.
        // bits 8 will form the operation.
        op = (testCase >>8) & 0x1;
        mask = testCase & 0x1f;

        switch(op)
        {
            case 0:
            {
                rc = stopChannel(mask);
                break;
            }
            case 1:
            {
                rc = restartChannel(mask);
                break;
            }
         }
         // If we failed, did we attempt to do something to an unallocated channel, or 0 channels?
         if(rc != OK  && mask && !(mask & BIT(4)))
         {
                Printf(Tee::PriHigh, "ScheduleTest %s failed with error %s.  Last mask: %x \n",
                                     __FUNCTION__, rc.Message(), mask);
                return RC::SOFTWARE_ERROR;
         }
    }

    CHECK_RC(restartChannel(0xf));
    return OK;
}

//! \brief generate faults between stop and restart operations
//!
//! Run a set of stop and restarts and fault at various places along the way.
//!
//! \return OK if test passes
RC ScheduleTest::rcFaultTest()
{

    RC              rc = OK;
    UINT32 timeoutMs = static_cast<UINT32>(GetTestConfiguration()->TimeoutMs());

    Printf(Tee::PriAlways, "ScheduleTest running %s\n. ", __FUNCTION__);

    CHECK_RC(StartRCTest());

    // Stop, restart, fault.
    CHECK_RC_CLEANUP(stopChannel(BIT(0)));
    CHECK_RC_CLEANUP(restartChannel(BIT(0)));
    CHECK_RC_CLEANUP(channelList[0]->fault());
    rc = channelList[0]->waitForIdle(timeoutMs);
    if(rc != RC::RM_RCH_PBDMA_ERROR)
    {
        Printf(Tee::PriHigh,
               "ScheduleTest: IdleRC received: %s\n", rc.Message());
        rc = RC::SOFTWARE_ERROR;
        // make sure we restore RC behavior.
        goto Cleanup;
    }

    // Stop, fault, restart.
    CHECK_RC_CLEANUP(stopChannel(BIT(0)));
    rc = (channelList[0]->fault());
    if(rc != RC::RM_RCH_PBDMA_ERROR)
    {
        Printf(Tee::PriHigh,
               "ScheduleTest: Incorrect RC FAULT received: %s\n", rc.Message());
        rc = RC::SOFTWARE_ERROR;
        // make sure we restore RC behavior.
        goto Cleanup;
    }
    CHECK_RC_CLEANUP(restartChannel(BIT(0)));
    CHECK_RC_CLEANUP(channelList[0]->waitForIdle(timeoutMs));

    // Fault, stop, restart.
    CHECK_RC_CLEANUP(channelList[0]->fault());
    CHECK_RC_CLEANUP(stopChannel(BIT(0)));
    CHECK_RC_CLEANUP(restartChannel(BIT(0)));
    rc = channelList[0]->waitForIdle(timeoutMs);
    if(rc != RC::RM_RCH_PBDMA_ERROR)
    {
        Printf(Tee::PriHigh,
               "ScheduleTest: Incorrect RC FAULT received: %s\n", rc.Message());
        rc = RC::SOFTWARE_ERROR;
        // make sure we restore RC behavior.
        goto Cleanup;
    }

Cleanup:
    // restore RC status
    EndRCTest();

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC ScheduleTest::Cleanup()
{
    ScheduleChannel* chan;

    while(channelList.size() != 0)
    {
        channelList.back()->cleanup();
        chan = channelList.back();
        GetTestConfiguration()->FreeChannel(chan->pCh);
        channelList.pop_back();
        delete chan;
    }

    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ ScheduleTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(ScheduleTest, RmTest,
                 "ScheduleTest RM test.");
