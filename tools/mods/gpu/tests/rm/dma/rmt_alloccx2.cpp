/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2011,2016,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file rmt_alloccx2.cpp
//! \brief RmAllocContextDma2 functionality test
//!
//! This test basically tests the functionality of allocating a context DMA
//! and binding it to a channel
//! We are testing the cases for the flags of context DMA to be READ_WRITE,
//! READ_ONLY, WRITE_ONLY by communicating with
//! notify buffer via DMA context.

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"

#include "class/cl0070.h"
#include "class/cl5097.h" // LW50_TESLA
#include "class/cl5097sw.h" // LW50_TESLA. Notification fields added later
#include "class/cl9097.h" // FERMI_A
#include "class/cl9097sw.h" // FERMI_A. Notification fields added later
#include "class/cl8697.h"
#include "class/cla097.h"
#include "class/cla06fsubch.h"
#include "gpu/include/notifier.h"
#include "core/include/memcheck.h"

//
// NUM_OF_ACCESS is a count of three types of memory access: READ_WRITE,
//                                               READ_ONLY and WRITE_ONLY.
//
#define NUM_OF_ACCESS       3
#define MAXCOUNT            1

enum
{
    READ_WRITE,
    READ_ONLY,
    WRITE_ONLY
};

class AllocContextDma2Test: public RmTest
{
public:
    AllocContextDma2Test();
    virtual ~AllocContextDma2Test();

    virtual string     IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:

    //! Test specific variables
    FLOAT64          m_TimeoutMs;
    Notifier         m_Notifier;
    LwRm::Handle     m_hObject;

    RC AllocNotifier(UINT32 access);
    RC TestNotifier(UINT32 access);

    GrClassAllocator *pTeslaAlloc;
    GrClassAllocator *pFermiAlloc;
};

//! \brief Constructor for AllocContextDma2Test
//!
//! Just does nothing except setting a name for the test..the actual
//! functionality is done by Setup()
//!
//! \sa Setup
//----------------------------------------------------------------------------
AllocContextDma2Test::AllocContextDma2Test()
{
    SetName("AllocContextDma2Test");

    m_TimeoutMs = Tasker::NO_TIMEOUT;
    m_hObject   = 0;
    pTeslaAlloc = new ThreeDAlloc();
    pFermiAlloc = new ThreeDAlloc();

    pTeslaAlloc->SetOldestSupportedClass(LW50_TESLA);
    pTeslaAlloc->SetNewestSupportedClass(GT21A_TESLA);

    pFermiAlloc->SetOldestSupportedClass(FERMI_A);
}

//! \brief AllocContextDma2Test destructor
//!
//! All resources should be freed in the Cleanup member function, so
//! destructors shouldn't do much.
//!
//! \sa Cleanup
//---------------------------------------------------------------------------
AllocContextDma2Test::~AllocContextDma2Test()
{
    delete pTeslaAlloc;
    delete pFermiAlloc;
}

//! \brief IsTestSupported()
//!
//! Checks whether the hardware classes are supported or not
//------------------------------------------------------------------------
string AllocContextDma2Test::IsTestSupported()
{
    GpuDevice *pGpuDev = GetBoundGpuDevice();

    if( pTeslaAlloc->IsSupported(pGpuDev) ||
           pFermiAlloc->IsSupported(pGpuDev) )
        return RUN_RMTEST_TRUE;

    return "Neither fermi nor tesla device alloc supported on current plateform";
}

//! \brief Setup()
//!
//! This function basically allocates a channel with a handle, then allocates
//! an object of a class for that channel
//!
//! \return RC -> OK if everything is allocated, test-specific RC if something
//!               failed while selwring resources.
//!
//! \sa Run
//---------------------------------------------------------------------------
RC AllocContextDma2Test::Setup()
{
    RC           rc;
    LwRmPtr      pLwRm;
    GpuDevice *pGpuDev = GetBoundGpuDevice();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();

    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_GPFIFO_CHANNEL))
    {
        // Override any JS settings for the channel type, this test requires
        // a GpFifoChannel channel.
        m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);
    }

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));

    if (pTeslaAlloc->IsSupported(pGpuDev))
    {
        CHECK_RC(pTeslaAlloc->Alloc(m_hCh, pGpuDev));
    }
    else if (pFermiAlloc->IsSupported(pGpuDev))
    {
        CHECK_RC(pFermiAlloc->Alloc(m_hCh, pGpuDev));
    }
    else
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

//! \brief This function runs the test
//!
//! This test uses standard notifier for memory allocation, Maps a notifyBuffer to the
//!  memory allocated, basically allocates a context DMA to the memory allocated and then
//! binds the context DMA with the channel. The test sets the flags for the DMA
//! as READ_WRITE, READ_ONLY, WRITE_ONLY, then tests for the status of the
//! notifyBuffer everytime.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the test
//!               failed
//!
//! \sa AllocNotifier
//! \sa FreeNotifier
//! \sa TestNotifier
//------------------------------------------------------------------------------
RC AllocContextDma2Test::Run()
{
    RC rc;

    for(UINT32 i = 0;i < NUM_OF_ACCESS; i++)
    {
        CHECK_RC(AllocNotifier(i));

        // Writing a notifier to the read only context dma will cause
        // a graphics fault.  It would be nice to verify that someday
        // with the error context DMA.
        if(i != READ_ONLY)
            CHECK_RC(TestNotifier(i));

        m_Notifier.Free();
    }

    return rc;
}

//! \brief Cleanup()
//!
//! free the resources allocated
//!
//! \sa Setup
//--------------------------------------------------------------------------
RC AllocContextDma2Test::Cleanup()
{
    RC rc, firstRc;

    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));
    m_pCh = 0;

    return firstRc;
}

RC AllocContextDma2Test::AllocNotifier(UINT32 access)
{
    RC              rc;
    Memory::Protect accessProtect;

    if (access == READ_ONLY)
        accessProtect = Memory::Readable;
    else if (access == WRITE_ONLY)
        accessProtect = Memory::Writeable;
    else
        accessProtect = Memory::ReadWrite;

    m_Notifier.Allocate(m_pCh,
                            1,
                            m_TestConfig.DmaProtocol(),
                            m_TestConfig.NotifierLocation(),
                            m_TestConfig.GetGpuDevice(),
                            accessProtect);

    return rc;
}

//! \brief Original testing happens here and is called by Run()
//!
//! Here user commands are written in the form of "Subchannel,
//! Method, Data" and are flushed into the Chip's FIFO.The commands include a
//! "NOTIFY" method which notifies on "WRITE_ONLY".
//! The success of the test is checked by polling for the notification
//! in the notification memory
//!
//! \return RC->OK if the test is successfull, RC->TIMEOUT if timed out while
//! polling, returns rc as it is if other errors occur
//!
//! \sa Run
//-----------------------------------------------------------------------
RC AllocContextDma2Test::TestNotifier(UINT32 flag)
{
    RC rc;
    GpuDevice *pGpuDev = GetBoundGpuDevice();

    if (pTeslaAlloc->IsSupported(pGpuDev))
    {
        CHECK_RC(m_pCh->SetObject(0, pTeslaAlloc->GetHandle()));
        CHECK_RC(m_Notifier.Instantiate(0,
                                        LW5097_SET_CONTEXT_DMA_NOTIFY));
        m_Notifier.Clear(LW5097_NOTIFIERS_NOTIFY);
        CHECK_RC(m_Notifier.Write(0));
        CHECK_RC(m_pCh->Write(0, LW5097_NO_OPERATION, 0));
        CHECK_RC(m_pCh->Flush());

        CHECK_RC(m_Notifier.Wait(LW5097_NOTIFIERS_NOTIFY, m_TimeoutMs));
    }
    else if (pFermiAlloc->IsSupported(pGpuDev))
    {
        CHECK_RC(m_pCh->SetObject(0, pFermiAlloc->GetHandle()));
        CHECK_RC(m_Notifier.Instantiate(0));
        m_Notifier.Clear(LW9097_NOTIFIERS_NOTIFY);
        CHECK_RC(m_pCh->Write(0,LW9097_NOTIFY,
                              LW9097_NOTIFY_TYPE_WRITE_ONLY));
        CHECK_RC(m_pCh->Write(0, LW9097_NO_OPERATION, 0));
        CHECK_RC(m_pCh->Flush());

        CHECK_RC(m_Notifier.Wait(LW9097_NOTIFIERS_NOTIFY, m_TimeoutMs));
    }
    else
    {
         return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ AllocContextDma2Test
//! object.
//---------------------------------------------------------------------------
JS_CLASS_INHERIT(AllocContextDma2Test, RmTest,
                 "RmAllocContextDma2 functionality test.");
