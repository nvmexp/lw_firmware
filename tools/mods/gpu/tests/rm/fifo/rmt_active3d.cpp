/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_active3d.cpp
//! \brief Test ensures we successfully allocate a 3D object on active channel
//!
//! 3D object allocation requires manipulating an instance memory
//! (the saved context area) but on an active channel instance memory is not
//! used instead live registers are used. Now, if the active channel is
//! swicthed out then some of the freshly updated instance memory is
//! overwritten, When the channel gets swicthed in the garbled state is
//! returned. This problem was seen on 1X, 2X and 3X chips and the solution was
//! provided. The purpose of this test is to early capture this
//! problem if exists. This now loops over all the supported 3D classes on
//! the chip it exelwtes on.

#include "gpu/tests/rmtest.h"

#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "gpu/include/notifier.h"
#include "core/include/platform.h"
#include "gpu/include/gralloc.h"

#include "lwos.h"

#include "class/cl902d.h" // FERMI_TWOD_A

#include "class/cl9097.h" // FERMI_A
#include "class/cl9097sw.h" // FERMI_A
#include "class/cl9197.h" // FERMI_B
#include "class/cla097.h" // KEPLER_A
#include "class/cla197.h" // KEPLER_B
#include "class/cla297.h" // KEPLER_C
#include "class/clb097.h" // MAXWELL_A
#include "class/clb197.h" // MAXWELL_B
#include "class/clc097.h" // PASCAL_A
#include "class/clc197.h" // PASCAL_B
#include "class/clc397.h" // VOLTA_A
#include "class/clc597.h" // TURING_A
#include "class/clc697.h" // AMPERE_A
#include "class/clc797.h" // AMPERE_B
#include "class/clc997.h" // ADA_A
#include "class/clcb97.h" // HOPPER_A
#include "class/clcc97.h" // HOPPER_B
#include "class/clcd97.h" // BLACKWELL_A
#include "class/cl906f.h" // GF100_CHANNEL_GPFIFO
#include "class/cla06fsubch.h" // KEPLER_CHANNEL_GPFIFO_A

#include "ctrl/ctrla06f.h"
#include "core/include/memcheck.h"

#define NO_OF_SUPPORTED_3D    16
#define ERR_NOT_SIZE    0x00001000 /* size of error notifier */
#define PB_SIZE         0x00001000 /* size of channel pushbuffer */

class ActiveChannel3DTest: public RmTest
{
public:
    ActiveChannel3DTest();
    virtual ~ActiveChannel3DTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    Notifier m_Notifier;

    LwRm::Handle m_hObject;
    LwRm::Handle m_hObject3D;
    FLOAT64 m_TimeoutMs;
    TwodAlloc m_TwoD;
    UINT32 twodClass;
    UINT32 m_supported3DClasses[NO_OF_SUPPORTED_3D];
    UINT32 m_3dClassCount;
    bool m_bDeferSched;

};

//! \brief ActiveChannel3DTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
ActiveChannel3DTest::ActiveChannel3DTest()
{
    SetName("ActiveChannel3DTest");

    // Set the initial value of supported 3D classes on current arch to zero
    m_3dClassCount = 0;
    m_hObject = 0;
    m_hObject3D = 0;
    m_TimeoutMs = Tasker::NO_TIMEOUT;
    twodClass = 0;
    memset(&m_supported3DClasses, 0, sizeof(m_supported3DClasses));
    m_bDeferSched = false;
}

//! \brief ActiveChannel3DTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
ActiveChannel3DTest::~ActiveChannel3DTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! On the current environment where the test is been exelwted there are
//! chances that the classes required by this test are not supported,
//! IsTestSupported checks to see whether those classes are supported
//! or not and this decides whether current test should
//! be ilwoked or not. If supported then the test will be ilwoked.
//!
//! \return True if the required classes are supported in the current
//!         environment, false otherwise
//-----------------------------------------------------------------------------
string ActiveChannel3DTest::IsTestSupported()
{
    UINT32 iLoopVar;
    LwRmPtr  pLwRm;
    const UINT32 classes3D[] =
    {
        BLACKWELL_A,    // CD97
        HOPPER_B,       // CC97
        HOPPER_A,       // CB97
        ADA_A,          // C997
        AMPERE_B,       // C797
        AMPERE_A,       // C697
        TURING_A,       // C597
        VOLTA_A,        // C397
        PASCAL_A,       // C097
        PASCAL_B,       // C197
        MAXWELL_B,      // B197
        MAXWELL_A,      // B097
        KEPLER_C,       // A297
        KEPLER_B,       // A197
        KEPLER_A,       // A097
        FERMI_B,        //9197
        FERMI_A,        //9097
    };

    if (!IsClassSupported(FERMI_TWOD_A))
    {
        return "FERMI_TWOD_A, class is not supported on current platform";
    }

    if (!IsClassSupported(GF100_CHANNEL_GPFIFO))
    {
        m_bDeferSched = true;
    }

    for (iLoopVar = 0; iLoopVar < sizeof(classes3D)/sizeof(classes3D[0]);
        iLoopVar++)
    {
        if (IsClassSupported(classes3D[iLoopVar]))
        {
            m_supported3DClasses[m_3dClassCount] = classes3D[iLoopVar];
            m_3dClassCount++;
        }
    }

    //
    // If any one or more 3D classes supported then m_3dClassCount will be
    // more than zero, in that case return true
    //
    if (m_3dClassCount != 0)
        return RUN_RMTEST_TRUE;

    return "No 3D class is supported on current platform";
}

//! \brief Setup all necessary state before running the test.
//!
//! Idea is to reserve all the resources required by this test so if
//! required multiple tests can be ran in parallel.
//! To achieve that this setup function allocates the channel, objects of
//! 2D and 3D classes and required ! notifiers (Write only notifier).
//!
//! \return RC -> OK if everything is allocated, test-specific RC if something
//!         failed while selwring resources.
//! \sa IsSupported
//-----------------------------------------------------------------------------
RC ActiveChannel3DTest::Setup()
{
    RC rc;
    LwRmPtr       pLwRm;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();

    // Override any JS settings for the channel type, this test requires
    // a GpFifoChannel channel.
    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));
    // Support for GF100+
    CHECK_RC(m_TwoD.Alloc(m_hCh, GetBoundGpuDevice()));

    m_hObject = m_TwoD.GetHandle();
    twodClass = m_TwoD.GetClass();

    // Assuming one block count of Notify as not yet defined in the Class
    CHECK_RC(m_Notifier.Allocate(m_pCh,
                                 1,
                                 &m_TestConfig));

    return rc;
}

//! \brief Run the test!
//!
//! After the setup is successful the allocated resources can be used.
//! Run function uses the allocated resources. It sets the 2d object and sets
//! the context DMA for notification making the channel active. After this 3D
//! object is allocated. If 3D object allocation is successful we need to
//! verify that it has not garbled 2D state, to verify we simply execute the
//! NOP, notify pair and if we get the notification then test passes
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ActiveChannel3DTest::Run()
{
    RC rc;
    LwRmPtr pLwRm;
    UINT32 iLoopVar;
    LWA06F_CTRL_GPFIFO_SCHEDULE_PARAMS gpFifoSchedulParams = {0};

    for (iLoopVar = 0; iLoopVar < m_3dClassCount; iLoopVar++)
    {
        // Alloc the object and then schedule it beofre SetObject.

        CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObject3D,
                              m_supported3DClasses[iLoopVar], NULL));

        if (m_bDeferSched)
        {
            gpFifoSchedulParams.bEnable = true;

            // Schedule channel
            CHECK_RC(pLwRm->Control(m_pCh->GetHandle(), LWA06F_CTRL_CMD_GPFIFO_SCHEDULE,
                    &gpFifoSchedulParams, sizeof(gpFifoSchedulParams)));
        }

        CHECK_RC(m_pCh->SetObject(LWA06F_SUBCHANNEL_2D, m_hObject));
        CHECK_RC(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_2D));
        CHECK_RC(m_pCh->Flush());

        m_Notifier.Clear(0);
        CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW902D_NOTIFY,
                              LW902D_NOTIFY_TYPE_WRITE_ONLY));

        CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW902D_NO_OPERATION,0));
        CHECK_RC(m_pCh->Flush());

        CHECK_RC(m_Notifier.Wait(0, m_TimeoutMs));

        pLwRm->Free(m_hObject3D);
        m_hObject3D = 0;

        Printf(Tee::PriLow,"In 3D loop %d ", iLoopVar);
        Printf(Tee::PriLow,"\t class: 0x%x\n",
               (UINT32)m_supported3DClasses[iLoopVar]);
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails,
//! so cleaning up the allocated Notifier and the channel (objects associated
//! with the channel are freed when the channel is freed).
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC ActiveChannel3DTest::Cleanup()
{
    RC rc, firstRc;

    m_Notifier.Free();

    m_TwoD.Free();
    m_hObject = 0;

    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));
    m_pCh = NULL;

    return firstRc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ ActiveChannel3DTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(ActiveChannel3DTest, RmTest,
                 "ActiveChannel3D RM test.");
