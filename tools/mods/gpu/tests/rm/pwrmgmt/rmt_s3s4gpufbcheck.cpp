/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2010,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_s3s4gpufbcheck.cpp
//! \brief Stress test System Suspend/Resume functionality
//!
//! This test is targeted to stress the suspend/resume operations.
//! The test takes power state sequence and the number of iterations as
//! input parameters.
//! Default parameter values are:
//! a) Suspend order, S3->Resume->S3->Resume->S4->Resume->S3->Resume.
//! b) Iteration count: 1
//! After each resume, test creates FB activity to verify DRAM is working.
//! and the test also does the FIFO activity by pushing notify/Nop operation to
//! verify that the GPU is alive and can be communicated.

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "gpu/perf/powermgmtutil.h"
#include "lwRmApi.h"

#include "class/cl5097.h" // LW50_TESLA for >= G8x
#include "class/cl5097sw.h" // LW50_TESLA corresponding SW class
#include "core/include/channel.h"
#include "gpu/include/notifier.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/memcheck.h"

#define DEFAULT_ITERATIONS                  1
#define DEFAULT_SLEEP_BETWEEN_SUSPEND_OPS   5000000 // 5 Sec
#define DEFAULT_NUMBER_OF_SUSPENDS          3       // S3 in sequence: 3 times

// The waitable time for sleep
#define DEFAULT_SUSPEND_TIME                15      // 15 Sec
#define DEFAULT_HIBERNATE_TIME              30      // 30 Sec

// As no define for this LW50_TESLA in its header so in the test
#define TESLA_NOTIFIERS_MAXCOUNT            1

class S3S4GpuFbCheckTest : public RmTest
{
public:
    S3S4GpuFbCheckTest();
    virtual ~S3S4GpuFbCheckTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    PowerMgmtUtil m_powerMgmtUtil;
    Notifier m_Notifier;
    LwRm::Handle m_hObject;
    LwRm::Handle m_vidMemHandle;
    UINT32 m_memSize;
    UINT64 m_ulTimeTaken;
    UINT32 m_uiIterations;
    UINT32 m_uiInterval;
    UINT32 m_uiNoOfSuspends;
    INT32 m_iS3sleepTimeInSec;
    INT32 m_iS4sleepTimeInSec;
    UINT32 m_uiTestMode;
    void *m_pMapVidAddr;
    FLOAT64 m_TimeoutMs;

    enum
    {
        DEFAULT_MODE,
        CONFIG_MODE
    };

    //
    //! Sub-Test function
    //
    RC DefaultRun();

    //
    //! Verification function
    //
    RC VerifyGpuAlive();
};

//! \brief S3S4GpuFbCheckTest constructor
//!
//! Placeholder : doesn't do much, functionality in Setup(), just inits some
//! class variables
//!
//! \sa Setup
//-----------------------------------------------------------------------------
S3S4GpuFbCheckTest::S3S4GpuFbCheckTest()
{
    SetName("S3S4GpuFbCheckTest");
    m_hObject = 0;
    m_vidMemHandle = 0;
    m_memSize = 0;
    m_ulTimeTaken = 0;
    m_pMapVidAddr = NULL;
    m_TimeoutMs = Tasker::NO_TIMEOUT;

    // Default initial values
    m_uiIterations = DEFAULT_ITERATIONS;
    m_uiInterval = DEFAULT_SLEEP_BETWEEN_SUSPEND_OPS;
    m_iS3sleepTimeInSec = DEFAULT_SUSPEND_TIME;
    m_iS4sleepTimeInSec = DEFAULT_HIBERNATE_TIME;
    m_uiNoOfSuspends = DEFAULT_NUMBER_OF_SUSPENDS;
    m_uiTestMode = DEFAULT_MODE;
}

//! \brief S3S4GpuFbCheckTest destructor
//!
//! Placeholder: functionality in Cleanup ()
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
S3S4GpuFbCheckTest::~S3S4GpuFbCheckTest()
{
}

//! \brief IsTestSupported(), Looks for whether test can execute in current elw.
//!
//! This test lwrrently run only on h/w, sim supported will be added later.
//! As this test uses the Graphics class solid triangle so need to see whether
//! that's supported on the elw on which its running. This class is supported
//! upto G80 as per the classhal.h in Resman. Lwrrently the test is supported
//! only under WINS so the conditional complie.
//! The test fails on LW3x and we are yet to decide on whether we are should
//! support RM test for LW3x or not [Refer: bug 272543], disabling until then
//! for LW3x and below.
//!
//! \return True if the test can run in the current environment,
//!         false otherwise
//-----------------------------------------------------------------------------
string S3S4GpuFbCheckTest::IsTestSupported()
{
#ifdef LW_WINDOWS

    if (!IsClassSupported(LW50_TESLA))
    {
        // return false for LW3x and below
        Printf(Tee::PriHigh,
               "%d:S3S4GpuFbCheckTest Unsupported on LW3x and below \n",
               __LINE__);
        return "Not Supported on LW3x and below chips";
    }
    else
    {
        if (Platform::GetSimulationMode() != Platform::Hardware)
        {
            Printf(Tee::PriHigh,
                   "%d:S3S4GpuFbCheckTest is supported only on HW \n",
                   __LINE__);
            Printf(Tee::PriHigh,"%d:Returns from non HW platform\n",
                   __LINE__);
            return "Supported only on HW";
        }

        return RUN_RMTEST_TRUE;
    }
#else
    return "Lwrrently supported only on windows";
#endif
}

//! \brief Setup(): Generally used for any test level allocation
//!
//! As after S3, S4 resume there is need to do some FB and channel activity
//! the setup function allocates the required channel+object, Video and System
//! memory
//!
//! \return OK if the passed, specific RC if failed.
//! \sa Run()
//-----------------------------------------------------------------------------
RC S3S4GpuFbCheckTest::Setup()
{
    RC rc;
    LwRmPtr pLwRm;
    UINT64 heapOffset;

    // Lwrrently using the constant RM page size, will change if required
    const UINT32 rmPageSize = 0x1000;
    m_memSize = rmPageSize;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    m_TimeoutMs = m_TestConfig.TimeoutMs();

    // Set Channel type as GPFIFO if supported
    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_GPFIFO_CHANNEL))
         m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh));

    //
    // As following used across runs so placed in setup
    //

    //
    // Set object on sub-channel of allocated channel and placed the DMA
    // notifier, will be used after resuming from S3,S4
    //

    // Support for G8x+
    if (IsClassSupported(LW50_TESLA))
    {
        // Now allocate the object and set it in the channel
        CHECK_RC(pLwRm->Alloc(m_hCh,
                              &m_hObject,
                              LW50_TESLA,
                              NULL));
        CHECK_RC(m_Notifier.Allocate(m_pCh,
                                     TESLA_NOTIFIERS_MAXCOUNT,
                                     &m_TestConfig));
        //
        // Set object on sub-channel of allocated channel and place the DMA
        // notifier, will be used after resuming from S3,S4
        //
        CHECK_RC(m_pCh->Write(0, LW5097_SET_OBJECT, m_hObject));
        CHECK_RC(m_Notifier.Instantiate(0,
                                        LW5097_SET_CONTEXT_DMA_NOTIFY));
    }
    else
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(pLwRm->VidHeapAllocSize(LWOS32_TYPE_IMAGE,
                                     LWOS32_ATTR_NONE,
                                     LWOS32_ATTR2_NONE,
                                     m_memSize,
                                     &m_vidMemHandle,
                                     &heapOffset,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     GetBoundGpuDevice()));

    CHECK_RC(pLwRm->MapMemory(m_vidMemHandle,
                              0,
                              m_memSize,
                              &m_pMapVidAddr,
                              0,
                              GetBoundGpuSubdevice()));

    // For emulation and RTL, reduce the no. of S3 cycles so it doesn't run for hours
    if (Platform::RTL == Platform::GetSimulationMode() ||
        GetBoundGpuSubdevice()->IsEmulation())
    {
        m_uiNoOfSuspends = 1;
    }

    return rc;
}

//! \brief Run ():This is the test entry point.
//!
//! This function switch cases on the modes of exelwtion.There can be two modes
//! First one is default mode which calls the suspend/resume operations in its
//! test imposed default mode. The other mode is config mode where user can
//! configure the sequence of suspend/resume operations, suspend and hibernate
//! wait timers, number of iterations over the user specified suspend/resume
//! sequence, giving a path to stress test such system power state changes.
//!
//! \return OK if the passed, specific RC if failed
//! \sa Setup ()
//-----------------------------------------------------------------------------
RC S3S4GpuFbCheckTest::Run()
{
    RC rc;

    switch(m_uiTestMode)
    {
        case DEFAULT_MODE:
            CHECK_RC(DefaultRun());
            break;

        case CONFIG_MODE:
            //
            // Not implemented in current phase, will be added in immediate
            // next phase.
            //
            break;

        default:
            // do nothing just break
            break;
    }

    return rc;
}

//! \brief Cleanup()
//!
//! Cleans up all the allocated buffers and channel.
//!
//! \sa Setup()
//! \sa Run()
//-----------------------------------------------------------------------------
RC S3S4GpuFbCheckTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    pLwRm->UnmapMemory(m_vidMemHandle, m_pMapVidAddr, 0, GetBoundGpuSubdevice());
    m_Notifier.Free();
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));
    pLwRm->Free(m_vidMemHandle);
    return firstRc;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief DefaultRun(): Function called by default under DVS.
//!
//! This function targets towards the defaulted sequence of suspend/resume
//! operations. The function is called by default if user provides no input.
//! Specifically this defaulted mode call is true in case of DVS nightly/per-cl
//! runs.
//! Default power state transitions sequence is:
//! S3->Resume, S3->Resume, S3->Resume, S4->Resume.
//! After each resume GPU liveliness is verified using sub function
//! VerifyGpuAlive.
//!
//! \return OK if the passed, specific RC if failed
//! \sa Run ()
//-----------------------------------------------------------------------------
RC S3S4GpuFbCheckTest::DefaultRun()
{
    RC rc;
    UINT32 uiOuterLoopVar;
    UINT32 uiInnerLoopVar;
    UINT64 uiStart;

    m_powerMgmtUtil.BindGpuSubdevice(GetBoundGpuSubdevice());

    //
    // Iterate over number of suspends, Defaulted to 1
    // Default Suspend, Hibernate sequence as S3,S3,S3,S4
    //
    for(uiOuterLoopVar = 0;uiOuterLoopVar < m_uiIterations; uiOuterLoopVar++)
    {
        // Do S3 defaulted to 3 times
        for (uiInnerLoopVar = 0;
             uiInnerLoopVar < m_uiNoOfSuspends;
             uiInnerLoopVar++)
        {
            uiStart = Xp::GetWallTimeMS();
            CHECK_RC(
                m_powerMgmtUtil.PowerStateTransition(m_powerMgmtUtil.POWER_S3,
                                                     m_iS3sleepTimeInSec)
                    );
            m_ulTimeTaken = ((Xp::GetWallTimeMS()) - uiStart);
            Printf(Tee::PriLow,"TimeDif before suspend and after resume:%ul \n",
                   (UINT32)m_ulTimeTaken);

            // Verify : Is GPU is alive
            CHECK_RC(VerifyGpuAlive());

            //
            // Call the next PWR transition after 5 sec (default value)
            //
            Utility::Delay(m_uiInterval);
        }

        // S4
        uiStart = Xp::GetWallTimeMS();
        CHECK_RC(m_powerMgmtUtil.PowerStateTransition(m_powerMgmtUtil.POWER_S4,
                                                      m_iS4sleepTimeInSec));
        m_ulTimeTaken = ((Xp::GetWallTimeMS()) - uiStart);
        Printf(Tee::PriLow,"TimeDif before Hibernate and after resume:%ul \n",
               (UINT32)m_ulTimeTaken);

        // Verify : Is GPU is alive
        CHECK_RC(VerifyGpuAlive());

        //
        // Call the next PWR transition after 5 sec (default value)
        //
        Utility::Delay(m_uiInterval);
    }
    return rc;
}

//! \brief VerifyGpuAlive (): Verification Function.
//!
//! This function checks GPU liveliness by doing following two step activity,
//! 1. FB activity by writing and reading back from the VRAM.
//! 2. Checking GPU alive by communicating over FIFO using simple NOP/Notify.
//!
//! \return OK if the passed, specific RC if failed
//! \sa DefaultRun ()
//-----------------------------------------------------------------------------
RC S3S4GpuFbCheckTest::VerifyGpuAlive()
{
    RC rc;
    RC errorRc;
    UINT32 *pVidMapMemIterator;
    UINT32 uiLocalVar;
    UINT32 uiNoOfInts;

    if (m_pMapVidAddr == NULL)
    {
        Printf(Tee::PriHigh,
              "VerifyGpuAlive:Map Memory address must be valid here\n");
        return RC::LWRM_ERROR;
    }

    uiNoOfInts = m_memSize/sizeof(UINT32);

    //
    // Do the simple Channel activity after S3/S4 resume
    // should get notification back, simple WRITE_ONLY, NOP
    //

    // Support for G8x+
    if (IsClassSupported(LW50_TESLA))
    {
        m_Notifier.Clear(LW5097_NOTIFIERS_NOTIFY);
        CHECK_RC(m_Notifier.Write(0));
        CHECK_RC(m_pCh->Write(0, LW5097_NO_OPERATION, 0));
        CHECK_RC(m_pCh->Flush());
        CHECK_RC(m_Notifier.Wait(LW5097_NOTIFIERS_NOTIFY, m_TimeoutMs));
    }
    else
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    errorRc = m_pCh->CheckError();
    if (errorRc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:VerifyGpuAlive:Incorrect Notifier after suspend/resume\n",
               __LINE__);
        return errorRc;
    }

    //
    // Frame buffer activity, writing into memory and reading back
    // the same verifying that the values written correctly
    //

    //
    // Write unique values to the Video memory's mapped address,
    //
    pVidMapMemIterator = (UINT32 *)m_pMapVidAddr;
    for (uiLocalVar = 0; uiLocalVar < uiNoOfInts; uiLocalVar++)
    {
        MEM_WR32(pVidMapMemIterator, uiLocalVar);
        pVidMapMemIterator++;
    }

    //
    // Read values from the Video memory's mapped address, verify
    //
    pVidMapMemIterator = (UINT32 *)m_pMapVidAddr;
    for (uiLocalVar = 0; uiLocalVar < uiNoOfInts; uiLocalVar++)
    {
        if (MEM_RD32(pVidMapMemIterator) != uiLocalVar)
        {
            Printf(Tee::PriHigh,
                  "%d:VerifyGpuAlive:VidMem incorrect after suspend/resume\n",
                  __LINE__);
            return RC::LWRM_ERROR;
        }
        pVidMapMemIterator++;
    }
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++
//! S3S4GpuFbCheckTest object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(S3S4GpuFbCheckTest, RmTest,
"Test used for stressing Power state Suspend/Resume");

