/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_runlist.cpp
//! \brief  FIFO's runlist facility test
//!
//! This test is used to verify the HOST's runlist related functions. Including:
//!      fifoRunlistRemoveChannel
//!      fifoRunlistGroupChannels
//!      fifoRunlistDivideTimeslice
//!      fifoRunlistShowList
//!

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "lwRmApi.h"
#include "class/cl506f.h"
#include "class/cl826f.h"
#include "class/cl502d.h"
#include "core/utility/errloggr.h"
#include "core/include/channel.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl0080/ctrl0080fifo.h"
#include "core/include/memcheck.h"

#define NUM_CHANNEL 40 //!< max channels can be allocated

class FifoRunlistTest: public RmTest
{
public:
    FifoRunlistTest();
    virtual ~FifoRunlistTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    //@{
    //! Test specific functions
    RC RunlistGroupChannelsTest();  //!< Sub-test for LW0080_CTRL_CMD_FIFO_RUNLIST_GROUP_CHANNELS
    RC RunlistDivideTimesliceTest(); //!< Sub-test for LW0080_CTRL_CMD_FIFO_RUNLIST_DIVIDE_TIMESLICE
    //@}
    //@{
    //! Test specific variables
    LwRm::Handle                   m_Ch[NUM_CHANNEL];
    Channel                        *m_pCh[NUM_CHANNEL];
    //@}
};

//! \brief Constructor for FifoRunlistTest
//!
//! Just does nothing except setting a name for the test. And to initialize the
//! data members of FifoRunlistTest objects.
//------------------------------------------------------------------------------
FifoRunlistTest::FifoRunlistTest()
{
    LwU32 index = 0;

    SetName("FifoRunlistTest");

    for (index = 0 ; index < NUM_CHANNEL ; index++ )
    {
        m_Ch[index] = 0;
        m_pCh[index] = NULL;
    }
}

//! \brief FifoRunlistTest destructor
//!
//! All resources should be freed in the Cleanup member function, so
//! destructors shouldn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
FifoRunlistTest::~FifoRunlistTest()
{
}

//! \brief IsTestSupported()
//!
//! Checks whether the classes are supported or not
//!
//------------------------------------------------------------------------------
string FifoRunlistTest::IsTestSupported()
{
    if ( IsClassSupported(LW50_CHANNEL_GPFIFO) ||
         IsClassSupported(G82_CHANNEL_GPFIFO) )
    {
        return RUN_RMTEST_TRUE;
    }
    else
    {
        return "None os these LW50_CHANNEL_GPFIFO, G82_CHANNEL_GPFIFO classes is supported on current platform";
    }
}

//! \brief Setup()
//!
//! This test doesn't do the allocation in the Setup().  The channel alloction
//! is placed at the beginning of each sub-test for making this test be able to
//! run twice in a line.
//!
//! \return RC -> OK if the test gets initilized correctly, test-specific RC if
//!               something failed.
//------------------------------------------------------------------------------
RC FifoRunlistTest::Setup()
{
    RC    rc;

    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    // Override any JS settings for the channel type, this test requires
    // a GpFifoChannel channel.
    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);
    m_TestConfig.SetAllowMultipleChannels(true);

    return rc;
}

//! \brief Run() : The body of this test.
//!
//! In this function, it calls several  sub-tests.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC FifoRunlistTest::Run()
{
    RC       rc;

    CHECK_RC(RunlistGroupChannelsTest());
    CHECK_RC(RunlistDivideTimesliceTest());

    return rc;
}

//! \brief Sub-test for LW0080_CTRL_CMD_FIFO_RUNLIST_GROUP_CHANNELS
//!
//! This test is for LW0080_CTRL_CMD_FIFO_RUNLIST_GROUP_CHANNELS.  It allocates
//! several channels then applies LW0080_CTRL_CMD_FIFO_RUNLIST_GROUP_CHANNELS
//! to group the channels.  It prints out the runlist at the begining and the
//! end of the test.
//!
//! \sa Run()
//------------------------------------------------------------------------------
RC FifoRunlistTest::RunlistGroupChannelsTest()
{
    RC           rc;
    RC           errorRc;
    LwRmPtr      pLwRm;
    LwU32        index = 0;
    LwU32        numChannel = 10; // the number of channels to use in this subtest
    LwRm::Handle hDevice = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    LW0080_CTRL_FIFO_RUNLIST_GROUP_CHANNELS_PARAM     gcParams = {0};

    //
    //  allocate some channels
    //
    for (index = 0 ; index < numChannel ; index++ )
    {
        CHECK_RC(m_TestConfig.AllocateChannel(&(m_pCh[index]), &(m_Ch[index])));

        // Check that there are no errors on the channel.
        errorRc = m_pCh[index]->CheckError();
        if (errorRc != OK)
        {
            return errorRc;
        }
    }

    //
    // manual check point here!
    //  * use fifoRunlistShowList the print out the List to make sure the
    //    Runlist Tree data structure is good
    //

    //
    // start to group chanels
    //

    // (0,1)
    gcParams.hChannel1 = m_Ch[0];
    gcParams.hChannel2 = m_Ch[1];
    errorRc = pLwRm->Control(hDevice,
                     LW0080_CTRL_CMD_FIFO_RUNLIST_GROUP_CHANNELS,
                     &gcParams,
                     sizeof(gcParams));

    if ( errorRc == RC::LWRM_NOT_SUPPORTED )
    {
        Printf(Tee::PriHigh,"The RunlistGroupChannelsTest test is not supported. \n");
        return OK;
    }

    // (0,1,2)
    gcParams.hChannel2 = m_Ch[2];
    CHECK_RC(pLwRm->Control(hDevice,
                     LW0080_CTRL_CMD_FIFO_RUNLIST_GROUP_CHANNELS,
                     &gcParams,
                     sizeof(gcParams)));

    // (0,1,2,3)
    gcParams.hChannel2 = m_Ch[3];
    CHECK_RC(pLwRm->Control(hDevice,
                     LW0080_CTRL_CMD_FIFO_RUNLIST_GROUP_CHANNELS,
                     &gcParams,
                     sizeof(gcParams)));

    // (4,5)
    gcParams.hChannel1 = m_Ch[4];
    gcParams.hChannel2 = m_Ch[5];
    CHECK_RC(pLwRm->Control(hDevice,
                     LW0080_CTRL_CMD_FIFO_RUNLIST_GROUP_CHANNELS,
                     &gcParams,
                     sizeof(gcParams)));

    // group two groups
    // (4,5,0,1,2,3)
    gcParams.hChannel2 = m_Ch[4];
    gcParams.hChannel1 = m_Ch[0];
    CHECK_RC(pLwRm->Control(hDevice,
                     LW0080_CTRL_CMD_FIFO_RUNLIST_GROUP_CHANNELS,
                     &gcParams,
                     sizeof(gcParams)));

    //
    // manual check point here!
    //  * use fifoRunlistShowList the print out the List to make sure the
    //    Runlist Tree data structure is good
    //

    //
    // free the resource
    //
    for (index = 0 ; index < numChannel; index++ )
    {
        m_TestConfig.FreeChannel(m_pCh[index]);
        m_pCh[index]=NULL;
    }

    return rc;
}

//! \brief Sub-test for LW0080_CTRL_CMD_FIFO_RUNLIST_DIVIDE_TIMESLICE
//!
//! \sa Run()
//------------------------------------------------------------------------------
RC FifoRunlistTest::RunlistDivideTimesliceTest()
{
    RC           rc;
    RC           errorRc;
    LwRmPtr      pLwRm;
    LwU32        index = 0;
    LwU32        numChannel = 20; // the number of channels to use in this subtest
    LwRm::Handle hDevice = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    LW0080_CTRL_FIFO_RUNLIST_GROUP_CHANNELS_PARAM       gcParams = {0};
    LW0080_CTRL_FIFO_RUNLIST_DIVIDE_TIMESLICE_PARAM     dtParams = {0};

    //
    //  allocate some channels
    //
    for (index = 0 ; index < numChannel ; index++ )
    {
        CHECK_RC(m_TestConfig.AllocateChannel(&(m_pCh[index]), &(m_Ch[index])));

        // Check that there are no errors on the channel.
        errorRc = m_pCh[index]->CheckError();
        if (errorRc != OK)
        {
            return errorRc;
        }
    }

    //
    // Test for fifoRunlistDivideTimeslice
    //

    // (4) / 4
    dtParams.hChannel = m_Ch[4];
    dtParams.tsDivisor = 4;
    errorRc = pLwRm->Control(hDevice,
                     LW0080_CTRL_CMD_FIFO_RUNLIST_DIVIDE_TIMESLICE,
                     &dtParams,
                     sizeof(dtParams));

    if ( errorRc == RC::LWRM_NOT_SUPPORTED )
    {
        Printf(Tee::PriHigh,"The RunlistDivideTimesliceTest test is not supported. \n");
        return OK;
    }

    // (5) / 5
    dtParams.hChannel = m_Ch[5];
    dtParams.tsDivisor = 5;
    CHECK_RC(pLwRm->Control(hDevice,
                     LW0080_CTRL_CMD_FIFO_RUNLIST_DIVIDE_TIMESLICE,
                     &dtParams,
                     sizeof(dtParams)));
    // (6) / 6
    dtParams.hChannel = m_Ch[6];
    dtParams.tsDivisor = 6;
    CHECK_RC(pLwRm->Control(hDevice,
                     LW0080_CTRL_CMD_FIFO_RUNLIST_DIVIDE_TIMESLICE,
                     &dtParams,
                     sizeof(dtParams)));
    // (7) / 7
    dtParams.hChannel = m_Ch[7];
    dtParams.tsDivisor = 7;
    CHECK_RC(pLwRm->Control(hDevice,
                     LW0080_CTRL_CMD_FIFO_RUNLIST_DIVIDE_TIMESLICE,
                     &dtParams,
                     sizeof(dtParams)));
    // (14) / 6
    dtParams.hChannel = m_Ch[14];
    dtParams.tsDivisor = 6;
    CHECK_RC(pLwRm->Control(hDevice,
                     LW0080_CTRL_CMD_FIFO_RUNLIST_DIVIDE_TIMESLICE,
                     &dtParams,
                     sizeof(dtParams)));

    //
    // to divide the timeslice of a group
    //

    // (0,1)
    gcParams.hChannel1 = m_Ch[0];
    gcParams.hChannel2 = m_Ch[1];
    errorRc = pLwRm->Control(hDevice,
                     LW0080_CTRL_CMD_FIFO_RUNLIST_GROUP_CHANNELS,
                     &gcParams,
                     sizeof(gcParams));

    // (0,1) / 6
    dtParams.hChannel = m_Ch[1];
    dtParams.tsDivisor = 6;
    CHECK_RC(pLwRm->Control(hDevice,
                     LW0080_CTRL_CMD_FIFO_RUNLIST_DIVIDE_TIMESLICE,
                     &dtParams,
                     sizeof(dtParams)));

    //
    // manual check point here!
    //  * use fifoRunlistShowList the print out the List to make sure the
    //    Runlist Tree data structure is good
    //  * Break in debugger and check the timeslice in channel's RAMFC
    //
    // FreeChannel() calls into FifoCommitRunlist().  That is another
    // good place to do manual check.
    //

    //
    // free the resource
    //
    for (index = 0 ; index < numChannel; index++ )
    {
        CHECK_RC(m_TestConfig.FreeChannel(m_pCh[index]));
        m_pCh[index]=NULL;
    }

    return rc;

}

//! \brief Free any resources that this test selwred
//!
//! Cleanup all the allocated resources that the test selwred during Setup.  For
//! this test, it doesn't allocate resources in Setup.  So the Cleanup is an empty
//! function.
//!
//! \sa Setup()
//------------------------------------------------------------------------------
RC FifoRunlistTest::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ FifoRunlistTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(FifoRunlistTest, RmTest,
                 "Test HOST Runlist features.");

