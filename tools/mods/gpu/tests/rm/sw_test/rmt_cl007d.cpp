/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2011,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_cl007d.cpp
//! \brief  NOP and notify pair test.
//!
//! This test consist of NOP and notify pair test using the class
//! LW04_SOFTWARE_TEST. NOP test is for checking the errors on the channel after
//! setting the object of the class LW04_SOFTWARE_TEST and notifier test consist
//! of checking notifier from different memory spaces.

#include "gpu/tests/rmtest.h"

#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "class/cl003e.h" // LW01_MEMORY_SYSTEM
#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "class/cl007d.h" // LW04_SOFTWARE_TEST
#include "class/cl9097.h" // FERMI_A
#include "class/cl906f.h" // GF100_CHANNEL_GPFIFO
#include "class/cla06fsubch.h" // GF100_CHANNEL_GPFIFO
#include "gpu/include/gpusbdev.h"
#include "core/include/memcheck.h"

enum CLASS007D_SUBCH
{
    SW_OBJ_SUBCH = 5,
    GR_OBJ_SUBCH = LWA06F_SUBCHANNEL_3D,
};

namespace Class007dData
{
    struct memTabStruct
    {
        UINT32 allocMemClass;
        UINT32 allocMemFlags;
    }
    memTab[] =
    {
        {
            LW01_MEMORY_LOCAL_USER,
            0
        },
        {
            LW01_MEMORY_SYSTEM,
            DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
            DRF_DEF(OS02, _FLAGS, _COHERENCY, _CACHED)
        },
    };

    const UINT32 RM_PAGE_SIZE  = 0x1000;
    LwRm::Handle hObject;
}

using namespace Class007dData;

class Class007dTest: public RmTest
{
public:
    Class007dTest();
    virtual ~Class007dTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    //@{
    //! Test specific variables
    LwRm::Handle            m_hObject;
    LwRm::Handle            m_hGraphicsObject;
    LwRm::Handle            m_hMem;
    LwNotification          *m_pNotifyBuffer;
    FLOAT64                 m_TimeoutMs;
    //@}
    //@{
    //! Test specific functions
    RC testNop();
    //@}
};

//! \brief Constructor for Class007dTest
//!
//! Just does nothing except setting a name for the test. The actual
//! functionality is done by Setup()
//!
//! \sa Setup
//------------------------------------------------------------------------------
Class007dTest::Class007dTest()
{
    SetName("Class007dTest");
    m_hMem = 0;
    m_pNotifyBuffer = NULL;
    m_hObject = 0;
    m_hGraphicsObject = 0;
    m_TimeoutMs = Tasker::NO_TIMEOUT;
}
//! \brief Class007dTest destructor
//!
//! All resources should be freed in the Cleanup member function, so
//! destructors shouldn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
Class007dTest::~Class007dTest()
{
}
//! \brief IsTestSupported()
//!
//! Checks whether the classes are supported or not
//!
//! \return True if the  class LW04_SOFTWARE_TEST is supported, False
//!         otherwise
//------------------------------------------------------------------------------
string Class007dTest::IsTestSupported()
{
    if (!IsClassSupported(LW04_SOFTWARE_TEST))
        return "LW04_SOFTWARE_TEST class is not supported on current platform";

    return RUN_RMTEST_TRUE;
}
//! \brief Setup()
//!
//! This function basically allocates a channel with a handle, then allocates
//! an object of the class LW04_SOFTWARE_TEST for that channel
//!
//! \return RC -> OK if everything is allocated, test-specific RC if something
//!               failed while selwring resources.
//------------------------------------------------------------------------------
RC Class007dTest::Setup()
{
    RC rc;
    LwRmPtr    pLwRm;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();

    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR0));

    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObject, LW04_SOFTWARE_TEST, NULL));

    if (IsClassSupported(FERMI_A) == true)
    {
        CHECK_RC(pLwRm->Alloc(m_hCh, &m_hGraphicsObject, FERMI_A, NULL));
    }

    return rc;

}

//! \brief This function runs the test
//!
//! This test set the object of the class using LW07D_SET_OBJECT method.
//! To verify we simply execute the NOP, notify pair and if we don't
//! get the notification then test fails.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//!
//! \sa testNop()
//! \sa testNotify()
//------------------------------------------------------------------------------
RC Class007dTest::Run()
{

    RC rc;

    m_pCh->SetObject(SW_OBJ_SUBCH, m_hObject);

    CHECK_RC(testNop());

    return rc;

}

//! \brief Free any resources that this test selwred
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//!
//! \sa Setup()
//------------------------------------------------------------------------------
RC Class007dTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    pLwRm->Free(m_hObject);
    m_hObject = 0;

    if (IsClassSupported(FERMI_A))
    {
        pLwRm->Free(m_hGraphicsObject);
        m_hGraphicsObject = 0;
    }

    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));
    m_pCh = 0;

    return firstRc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief This test comes before the notifier test.
//!
//! This is testing the NOP operation.
//!
//! \sa Run()
//------------------------------------------------------------------------------
RC Class007dTest::testNop()
{
    RC rc;

    m_pCh->Write(SW_OBJ_SUBCH, LW07D_NO_OPERATION, 0x0);
    m_pCh->Write(SW_OBJ_SUBCH, LW906F_FB_FLUSH, 0);
    m_pCh->Write(SW_OBJ_SUBCH, LW07D_NO_OPERATION, 0x2);

    //
    // FERMITODO::HOST Test subchannel switches and test host + sw class method ordering
    // using semaphore release + sysmembar.
    //
    if (IsClassSupported(FERMI_A)== true)
    {
        m_pCh->Write(SW_OBJ_SUBCH, LW07D_NO_OPERATION, 0x1);
        m_pCh->Write(SW_OBJ_SUBCH, LW906F_FB_FLUSH, 0);
        m_pCh->Write(SW_OBJ_SUBCH, LW07D_NO_OPERATION, 0x2);
        m_pCh->Write(SW_OBJ_SUBCH, LW906F_FB_FLUSH, 0);
        m_pCh->Write(SW_OBJ_SUBCH, LW906F_FB_FLUSH, 0);
        m_pCh->Write(SW_OBJ_SUBCH, LW07D_NO_OPERATION, 0x3);
        m_pCh->SetObject(GR_OBJ_SUBCH, m_hGraphicsObject);
        m_pCh->Write(GR_OBJ_SUBCH, LW9097_SET_SPARE_NOOP02, 0);
        m_pCh->Write(GR_OBJ_SUBCH, LW9097_SET_SPARE_NOOP02, 0);
        m_pCh->Write(GR_OBJ_SUBCH, LW9097_SET_SPARE_NOOP02, 0);
        m_pCh->SetObject(SW_OBJ_SUBCH, m_hObject);
    }

    m_pCh->Flush();
    m_pCh->WaitIdle(m_TimeoutMs);

    // Check that there are no errors on the channel.
    CHECK_RC(m_pCh->CheckError());

    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Class007dTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class007dTest, RmTest,
                 "Class007d RM test.");
