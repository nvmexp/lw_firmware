/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
*/

//! \file rmt_gpufifostress.cpp
//! \brief RmGpuFifoStressTest functionality test
//!
//! This test basically tests the functionality of Gpu-Fifo by stressing it.
//! By stressing means allocate channels, memory, context dma, do some
//! rendering, then flush the commands. Again continue rendering and flush.
//! Also, in this test, we considered different threads running simultaneously
//! acting as clients and each performing continuous rendering until timecount
//! elapses or any error oclwrs on any channel. By doing this, we are able to
//! test the capacity of GpuFifo and see whether the functionality reaches any
//! saturation beyond which GpuFifo cannot be stressed(elastic limit).

//!
//! In case of FERMI we will consider only one thread and one channel
//! (for time being) because of the bug 348583 in fmodel.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "gpu/include/notifier.h"
#include "lwos.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "gpu/utility/surf2d.h"
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl003e.h" // LW01_MEMORY_SYSTEM
#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "class/cl502d.h" // LW50_TWOD
#include "class/cl5097.h" // LW50_TESLA
#include "class/cl8397.h" // GT200_TESLA
#include "class/cl9097.h" // FERMI_A
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
#include "class/cl5097sw.h"
#include "class/cl9097sw.h"
#include "class/clb097sw.h" // MAXWELL_A sw
#include "class/clb197sw.h" // MAXWELL_B sw
#include "class/clc097sw.h" // PASCAL_A sw
#include "class/clc197sw.h" // PASCAL_B sw
#include "class/clc397sw.h" // VOLTA_A sw
#include "class/clc597sw.h" // TURING_A sw
#include "class/clc697sw.h" // AMPERE_A sw
#include "class/clc797sw.h" // AMPERE_B sw
#include "class/clc997sw.h" // ADA_A sw
#include "class/clcb97sw.h" // HOPPER_A sw
#include "class/clcc97sw.h" // HOPPER_B sw
#include "class/clcd97sw.h" // BLACKWELL_A sw
#include "class/cla06fsubch.h"
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"

//
// There seems to be a restriction for the number of threads to run
// in MODS. Hence, there is an upper bound for the maximum number
// of threads to be run simulataneously. The number came out to be around
// 58 for SIMULATOR and 27 for HW. here, we have considered the
// number of threads as 20 just be on safer side for the first few phases,
// test will be revisited.
//
// We can solve the problem above by having more channels per thread.
// Each thread will allocate CHANNELS_PER_THREAD number of channels and
// loop through them.
// Total number of channels = ThreadNumber * CHANNELS_PER_THREAD

//
//here Channel Count is equal to thread count
//
#define THREAD_COUNT    25

#define THREAD_COUNT_CMODEL 12

#define TEST_RUNTIME    30

// As noty defined for LW50_TESLA in its header so in the test
#define TESLA_NOTIFIERS_MAXCOUNT    1

// As not defined fo FERMI_A
#define FERMI_A_NOTIFIERS_MAXCOUNT    1
#define KEPLER_A_NOTIFIERS_MAXCOUNT   1
#define KEPLER_B_NOTIFIERS_MAXCOUNT   1
#define KEPLER_C_NOTIFIERS_MAXCOUNT   1
#define MAXWELL_A_NOTIFIERS_MAXCOUNT  1
#define MAXWELL_B_NOTIFIERS_MAXCOUNT  1
#define PASCAL_A_NOTIFIERS_MAXCOUNT   1
#define PASCAL_B_NOTIFIERS_MAXCOUNT   1

// TESLA defines
#define TESLA2_NOTIFIERS_NOTIFY      0

//Sub channel Count
#define MAX_SUBCHANNEL_COUNT        8

#define MAX_CHANNEL_COUNT           25

#define GPUFIFO_LASTSUBCH           7

const UINT32 RM_PAGE_SIZE = 0x1000;

UINT64 memLimit = 2*RM_PAGE_SIZE - 1;

UINT32 g_nMilliseconds = 1000;
UINT32 g_nThreadNumber = 1;

bool g_bIsHW = false;

bool g_bStopOnError = false;
bool g_bTimeExpired = false;

const LwU32 TestMemSize  = 4 * 1024;              // 4KB render page
const LwU32 TestMemPitch = 1024*sizeof(LwU32);   // 4KB per line
const LwU32 TestMemLines = TestMemSize/TestMemPitch; // 1 line

UINT32  g_nThreadCompletionCount = 0;
void *g_pMutexHandle = NULL;
void *g_pChMutexHandle = NULL;
UINT32 g_nPrintCount = 0;
UINT32 g_nChCount =0;

//defines specific to tesla test
//subchannel specific
#define TESLA_SUBCH_HW2D 0
#define TESLA_SUBCH_HW3D 1
#define TESLA_SUBCH_SW3D 3
#define TESLA_SUBCH_NOP1 4
#define TESLA_SUBCH_NOP2 5
#define TESLA_SUBCH_NOP3 6
#define TESLA_SUBCH_WAITFORNOTIFY   7

struct THREAD_INFO
{
    UINT32 nThreadID;
    GpuSubdevice *pSubdev;
    GpuDevice *pDev;
};

static void AllocTestThread(void *);

class Stress
{
public:
    Stress(THREAD_INFO *psThInfo);
    ~Stress();

private:
    //! Test specific variables
    THREAD_INFO *m_psThInfo;

    GpuTestConfiguration    m_TestConfig;
    Notifier        m_Notifier;
    LwRm::Handle    m_hCh;
    Channel         *m_pCh;
    RC              m_StressRc;
    UINT32          m_nObjRefCount;
    UINT32          m_ClientNum;
    LwRm::Handle    m_hObject[MAX_SUBCHANNEL_COUNT];
    GpuSubdevice    *m_pSubdev;
    GpuDevice       *m_pDev;

    bool            m_bFree2d;

    UINT64           m_gpuAddr;
    LwRm::Handle     m_hVidMem;
    LwRm::Handle     m_hVA;

    LwRm::Handle     m_hNotifierMem;
    LwRm::Handle     m_hNotifyEvent;
    LwRm::Handle     m_hNotifyCtxDma;

    ModsEvent *m_pNotifyEvent;
    LwNotification *m_pNotifyBuffer;
    void *m_pNotifierMem;
    void *m_pEventAddr;

    bool bSetupComplete;

    // Surfaces
    Surface2D m_FbMem; ///< Original image source is put here
    Surface2D m_SysMem; ///< Source image is transferred into here.

    /// Graphics mode to run the test.
    struct
    {
        UINT32 Width;
        UINT32 Height;
        UINT32 Depth;
        UINT32 RefreshRate;
    } m_Mode;

    //! Test specific functions
    RC AllocAndTestChannel();

    //
    // Test using FERMI_A
    //
    RC Class9097Test();
    RC ClassA097Test();
    RC ClassA197Test();
    RC ClassA297Test();
    RC ClassB097Test();
    RC ClassB197Test();
    RC ClassC097Test();
    RC ClassC197Test();
    RC ClassC397Test();
    RC ClassC497Test();
    RC ClassC597Test();
    RC ClassC697Test();
    RC ClassC797Test();
    RC ClassC997Test();
    RC ClassCB97Test();
    RC ClassCC97Test();
    RC ClassCD97Test();

    //!Test Specific methodes
    RC RunTeslaStress();
    RC AllocTestMemory2D();

    RC Writeto2DSubchP1(UINT32 nSubCh);
    RC Writeto3DSubchP1(UINT32 nSubCh);
    RC Writeto3DSWmethodsP1(UINT32 nSubCh);
    RC Writeto2DSubchP2(UINT32 nSubCh);
    RC Writeto3DSubchP2(UINT32 nSubCh);
    RC Writeto3DSWmethodsP2(UINT32 nSubCh);

    RC WriteNOPmethods(UINT32 nSubCh);
    RC WritetoSubchAndWaitOnEvt(UINT32 nSubCh);

    RC WriteGT200SWmethods(UINT32 nSubCh);
    RC Write8397NOP(UINT32 nSubCh);

    void FreeTestMemory2D();
    void FreeTestResources();
};

class GpuFifoStressTest: public RmTest
{
public:
    GpuFifoStressTest();
    virtual ~GpuFifoStressTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(StressTime, UINT32); //Configurable Stress time

private:
    //
    //this variable for timer count
    //used for nightly builds
    UINT32  m_StressTime;
    THREAD_INFO *m_psThInfo[THREAD_COUNT];
};

//! \brief Constructor for GpuFifoStressTest
//!
//! Just does nothing except setting a name for the test..the actual
//! functionality is done by Setup()
//!
//! \sa Setup
//----------------------------------------------------------------------------
GpuFifoStressTest::GpuFifoStressTest() : m_StressTime(0)
{
    SetName("GpuFifoStressTest");
}

//! \brief GpuFifoStressTest destructor
//!
//! All resources should be freed in the test itself
//! destructors shouldn't do much.
//!
//! \sa Cleanup
//----------------------------------------------------------------------------
GpuFifoStressTest::~GpuFifoStressTest()
{
}

//! \brief IsTestSupported()
//!
//! Checks whether the hardware classes are supported or not
//----------------------------------------------------------------------------
string GpuFifoStressTest::IsTestSupported()
{
     ThreeDAlloc threedAlloc_First;
     ThreeDAlloc threedAlloc_Second;

     threedAlloc_First.SetOldestSupportedClass(LW50_TESLA);
     threedAlloc_First.SetNewestSupportedClass(LW50_TESLA);
     threedAlloc_Second.SetOldestSupportedClass(FERMI_A);

     if( !threedAlloc_First.IsSupported(GetBoundGpuDevice()) && !threedAlloc_Second.IsSupported(GetBoundGpuDevice()))
        return "None of these FERMI+, LW50_TESLA, classes is supported on current platform !";

    return RUN_RMTEST_TRUE;
}

//! \brief Setup()
//!
//! This function basically does nothing. Allocation will be done in the
//! constructor of class "Stress"
//!
//! \sa Stress::Stress()
//! \sa Run
//--------------------------------------------------------------------------
RC GpuFifoStressTest::Setup()
{
    RC rc;
    LwRmPtr pLwRm;
    ThreeDAlloc threedAlloc;

    threedAlloc.SetOldestSupportedClass(FERMI_A);
    g_bIsHW = Platform::GetSimulationMode() == Platform::Hardware;

    // i am putting fermi_a thread count as only 1 i have to enhance it to 25
    if(!threedAlloc.IsSupported(GetBoundGpuDevice()))
    {
        if(!pLwRm->IsClassSupported(LW50_TESLA, GetBoundGpuDevice()))
        {
            //for G7X Series Test is failing to allocate 25 channels at a time
            // so i made it as 20
            g_nThreadNumber = (g_bIsHW) ? THREAD_COUNT - 5 : THREAD_COUNT_CMODEL;
        }
        else
        {
            g_nThreadNumber = (g_bIsHW) ? THREAD_COUNT  : THREAD_COUNT_CMODEL;
        }
    }

    return rc;
}

//! \brief This function runs the test
//!
//! This function basically creates threads which act as clients for
//! further testing. Once the threads are created, they will start running
//! by switching context among themselves. Here, we also defined a
//! timecount which counts down and once it becomes zero, the thread should
//! stop exelwting. Also, we constrained that even when any client encounters
//! an error, exit the threads.
//!
//! \return RC -> OK if the test ran successfully, test-specific RC if test
//!               failed
//!
//! \sa AllocTestThread
//! \sa Stress::Stress()
//----------------------------------------------------------------------------
RC GpuFifoStressTest::Run()
{
    UINT32 i;
    RC rc;
    LwU32 nLwrTime;
    UINT32 nStartTime = Utility::GetSystemTimeInSeconds();
    UINT32 nPrevTimeCount = 0;
    UINT32 nPresTimeCount = 0;

    if(m_StressTime == 0)
    {
        //load the Stress Defaults
        m_StressTime = TEST_RUNTIME;
    }
    //
    //Allocate Mutex for sync among the threads
    //
    g_pMutexHandle = Tasker::AllocMutex("GpuFifoStressTest::g_pMutexHandle",
                                        Tasker::mtxUnchecked);
    g_pChMutexHandle = Tasker::AllocMutex("GpuFifoStressTest::g_pChMutexHandle",
                                          Tasker::mtxUnchecked);

    for (i = 0; i < g_nThreadNumber; i++)
    {
        //
        // creating threads which indeed act as clients
        //
        m_psThInfo[i] = new THREAD_INFO;
        m_psThInfo[i]->nThreadID = i;
        m_psThInfo[i]->pDev = GetBoundGpuDevice();
        m_psThInfo[i]->pSubdev = GetBoundGpuSubdevice();
        Tasker::CreateThread(AllocTestThread,(void *)m_psThInfo[i],0,NULL);
    }

    //
    //Delay some time to threads to start
    //
    Tasker::Sleep(100);

    for (nLwrTime = Utility::GetSystemTimeInSeconds();
        ((nLwrTime - nStartTime ) < m_StressTime) && (!g_bStopOnError);
        nLwrTime = Utility::GetSystemTimeInSeconds())

    {
        if(g_nPrintCount == g_nThreadNumber)
        {
            nPresTimeCount = (m_StressTime)-(nLwrTime - nStartTime);

            if(nPrevTimeCount != nPresTimeCount)
            {
//#if _DEBUG
//                Printf(Tee::PriLow,"\n TestRemaingTime :%d Sec\n",nPresTimeCount);
//#endif
                nPrevTimeCount  = nPresTimeCount;
            }
//            else
//            {
//#if _DEBUG
//                Printf(Tee::PriLow,"\n");
//#endif
//            }

            Tasker::AcquireMutex(g_pMutexHandle);
            g_nPrintCount = 0;
            Tasker::ReleaseMutex(g_pMutexHandle);
        }
        Tasker::Yield();
    }

    Tasker::AcquireMutex(g_pMutexHandle);
    g_bTimeExpired = true;
    Tasker::ReleaseMutex(g_pMutexHandle);

    //If error oclwred in any of the threads(clients)
    //return RC-> NOT OK
    //else RC -> OK

    if(g_bStopOnError)
    {
        rc = 1;
    }

    while(g_nThreadNumber > g_nThreadCompletionCount)
    {
        //
        //Wait for all the threads to complete their exelwtion
        //
        Tasker::Sleep(g_nMilliseconds);
    }

    //
    //Freeing the Mutex
    //
    Tasker::FreeMutex(g_pMutexHandle);
    Tasker::FreeMutex(g_pChMutexHandle);

    for (i = 0; i < g_nThreadNumber; i++)
    {
        delete m_psThInfo[i];
    }
    return rc;
}

//! \brief Cleanup()
//!
//! does nothing, freeing takes place in Stress::AllocTestThread()
//!
//! \sa Run
//! \sa AllocTestThread
//! \sa Stress::Stress
//----------------------------------------------------------------------------
RC GpuFifoStressTest::Cleanup()
{
    return OK;
}

//! \brief AllocTestThread
//!
//! just creating an instance of class "Stress" and exiting
//! the thread finally. This acts as an entry point to the test thread
//!
//! \sa Stress::Stress
//----------------------------------------------------------------------------
void AllocTestThread(void *p)
{
    THREAD_INFO *psThInfo =(THREAD_INFO *) p;

    while(true)
    {
        if(g_bStopOnError || g_bTimeExpired)
        {
            break;
        }
        //
        //Creating an instance of the class "Stress"
        //
        Stress *pTest = new Stress(psThInfo);

        // Simply printing to use TEST var and avoid warning
        //lot of prints so i am removing
        Printf(Tee::PriNormal,"TESTADDRESS: %p\n",pTest);

        if(pTest != NULL)
        {
            delete pTest;
            pTest = NULL;
        }

        Tasker::AcquireMutex(g_pMutexHandle);
        g_nPrintCount++;
        Tasker::ReleaseMutex(g_pMutexHandle);

        while((g_nChCount >= MAX_CHANNEL_COUNT) && (!g_bStopOnError) && (!g_bTimeExpired))
        {
            Tasker::Yield();
        }
    }

    Tasker::AcquireMutex(g_pMutexHandle);
    g_nThreadCompletionCount++;
    Tasker::ReleaseMutex(g_pMutexHandle);

    Tasker::ExitThread();
}

//! \brief Stress::Stress()
//!
//! Allocate memory, associate context dma to that,
//! then perform the testing which actually stresses the GpuFifo
//! by calling AllocAndTestChannel()
//!
//! \sa Stress::AllocAndTestChannel
//----------------------------------------------------------------------------
Stress::Stress(THREAD_INFO *psThInfo)
{
    RC rc;
    LwRmPtr pLwRm;
    m_pSubdev = psThInfo->pSubdev;
    m_pDev = psThInfo->pDev;
    m_ClientNum = psThInfo->nThreadID;
    m_nObjRefCount = 0;
    m_pNotifyEvent = NULL;
    m_pNotifierMem = NULL;
    m_hVidMem = 0;
    m_hVA = 0;

    m_hNotifierMem = 0;
    m_hNotifyEvent = 0;
    m_hNotifyCtxDma = 0;

    m_psThInfo = psThInfo;

    m_pCh = NULL;

    // Ask our base class(es) to setup internal state based on JS settings
    rc = m_TestConfig.InitFromJs();
    if(rc != OK)
    {
        Printf(Tee::PriHigh,
            "Error oclwred on Channel number %d:  ",m_ClientNum);
        Printf(Tee::PriHigh,"During InitFromJs()\n");
        goto Cleanup;
    }

    m_TestConfig.BindGpuDevice(m_pDev);

    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);

    // Call Test function where to stress on channels
    //
    rc = AllocAndTestChannel();

    if(rc != OK)
    {
        //
        // Indicate that some error oclwred on the channel
        //
        Tasker::AcquireMutex(g_pMutexHandle);
        g_bStopOnError = true;
        Tasker::ReleaseMutex(g_pMutexHandle);
        Printf(Tee::PriHigh,
            "Error oclwred on Channel number %d \n",m_ClientNum);
        goto Cleanup;
    }

Cleanup:
    //
    // stop all the threads, either error or done with it
    //
    m_StressRc = rc;
    if(m_StressRc != OK)
    {
        Printf(Tee::PriHigh,
            "RC Error colwred on Channel number %d is %d \n",
            m_ClientNum, (UINT32)m_StressRc);
    }

}

//! \brief ~Stress()
//!Destructor of Stress
//!frees the Object allocated and decrements the reference count
//!free the channel allocated
//! \sa Stress::~Stress()
//! \sa Run
//--------------------------------------------------------------------------
Stress::~Stress()
{
    LwRmPtr pLwRm;

    //
    //Frees the Objects allocated and decrements the referrence count
    //
    while(m_nObjRefCount)
    {
        //if any object still not freed then free here
        pLwRm->Free(m_hObject[m_nObjRefCount]);
        m_nObjRefCount--;
    }
    if(m_pCh)
    {
        //
        //Free the Channel
        //
        m_TestConfig.FreeChannel(m_pCh);

        Tasker::AcquireMutex(g_pChMutexHandle);
        g_nChCount--;
        Tasker::ReleaseMutex(g_pChMutexHandle);

    }
}

//! \brief Stress::AllocAndTestChannel()
//!
//! Keep running the test by allocating a channel, then do rendering
//! and flush the commands and again push the commands and flush
//! until any error oclwred in any of the threads or the timecount
//! reaches 0
//!
//! \return Rc->OK when the test runs successfully, else
//!             else test-specific RC error
//! \sa Stress::Stress
//! \sa AllocTestThread
//! \sa Run
//----------------------------------------------------------------------------
RC Stress::AllocAndTestChannel()
{
    LwRmPtr pLwRm;
    RC rc;
    UINT32 errorRc;

    if((rc = m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0))) != OK)
    {
        errorRc = rc;
        Printf(Tee::PriHigh,
            "Channel Allocation failed errorRc : %d\n",errorRc);
        return rc;
    }

    m_TestConfig.SetAllowMultipleChannels(true);

    Tasker::AcquireMutex(g_pChMutexHandle);
    g_nChCount++;
    Tasker::ReleaseMutex(g_pChMutexHandle);

    if (pLwRm->IsClassSupported(LW50_TESLA, m_pDev))
    {
        //
        // Test using LW50_TESLA
        //
        CHECK_RC(RunTeslaStress());
    }
    else if (pLwRm->IsClassSupported(FERMI_A, m_pDev))
    {
        //
        // Test using FERMI_A
        //
        //TBD Still need to updated with s/w methodes of fermi
        CHECK_RC(Class9097Test());
    }
    else if (pLwRm->IsClassSupported(KEPLER_A, m_pDev))
    {
        //TBD Still need to updated with s/w methodes of fermi
        //and kepler i2m ops
        CHECK_RC(ClassA097Test());
    }
    else if (pLwRm->IsClassSupported(KEPLER_B, m_pDev))
    {
        //TBD Still need to updated with s/w methodes of fermi
        //and kepler i2m ops
        //Bug No 814733: Remove KEPLER_A support from GK110
        CHECK_RC(ClassA197Test());
    }
    else if (pLwRm->IsClassSupported(KEPLER_C, m_pDev))
    {
        //TBD Still need to updated with s/w methodes of fermi
        //and kepler i2m ops
        //Bug No 815091: Remove KEPLER_A support from GK20A
        CHECK_RC(ClassA297Test());
    }
    else if (pLwRm->IsClassSupported(MAXWELL_A, m_pDev))
    {
        //TBD Still need to updated with s/w methodes of fermi
        //and kepler i2m ops
        CHECK_RC(ClassB097Test());
    }
    else if (pLwRm->IsClassSupported(MAXWELL_B, m_pDev))
    {
        //TBD Still need to updated with s/w methodes of fermi
        //and kepler i2m ops
        CHECK_RC(ClassB197Test());
    }
    else if (pLwRm->IsClassSupported(PASCAL_A, m_pDev))
    {
        //TBD Still need to updated with s/w methodes of fermi
        //and kepler i2m ops
        CHECK_RC(ClassC097Test());
    }
    else if (pLwRm->IsClassSupported(PASCAL_B, m_pDev))
    {
        //TBD Still need to updated with s/w methodes of fermi
        //and kepler i2m ops
        CHECK_RC(ClassC197Test());
    }
    else if (pLwRm->IsClassSupported(TURING_A, m_pDev))
    {
        CHECK_RC(ClassC597Test());
    }
    else if (pLwRm->IsClassSupported(AMPERE_A, m_pDev))
    {
        CHECK_RC(ClassC697Test());
    }
    else if (pLwRm->IsClassSupported(AMPERE_B, m_pDev))
    {
        CHECK_RC(ClassC797Test());
    }
    else if (pLwRm->IsClassSupported(ADA_A, m_pDev))
    {
        CHECK_RC(ClassC997Test());
    }
    else if (pLwRm->IsClassSupported(HOPPER_A, m_pDev))
    {
        CHECK_RC(ClassCB97Test());
    }
    else if (pLwRm->IsClassSupported(HOPPER_B, m_pDev))
    {
        CHECK_RC(ClassCC97Test());
    }
    else if (pLwRm->IsClassSupported(BLACKWELL_A, m_pDev))
    {
        CHECK_RC(ClassCD97Test());
    }
    else if (pLwRm->IsClassSupported(VOLTA_A, m_pDev))
    {
        //TBD Still need to updated with s/w methodes of fermi
        //and kepler i2m ops
        CHECK_RC(ClassC397Test());
    }
    return rc;
}

//! \brief RC Stress::RunTeslaStress()
//! run the Tesla Stress using operation like
//! 2D methodes,3D S/W methodes,3D h/w methodes
//! \return Rc->OK when the test runs successfully, else
//!             else test-specific RC error
//----------------------------------------------------------------------------
RC Stress::RunTeslaStress()
{

    //This methode Stress the Channel and SubChannel
    RC rc;
    LwRmPtr pLwRm;

    m_bFree2d = false;

    //for sub channel interweaving
    //6   -->DISAPLAY_SW methodes
    //0   --> 2D some methods p1
    //1   --> 3D some HW Mthds p1
    //0  --> 2D some more methods p2
    //3   --> 3D some SW Mthds p1
    //1   --> 3D some HW Mthds p2
    //4   --> GR Nop/Notify
    //5   --> GR Nop/Notify
    //3   --> 3D some SW methds P2
    //6   --> GR Nop/Notify
    //7   --> flush & wait on event

    //0   --> 2D some methods P1
    if(pLwRm->IsClassSupported(LW50_TWOD, m_pDev))
    {
        CHECK_RC_CLEANUP(Writeto2DSubchP1(TESLA_SUBCH_HW2D));
    }

    //1   --> 3D some HW methdsP1
    CHECK_RC_CLEANUP(Writeto3DSubchP1(TESLA_SUBCH_HW3D));

    //0  --> 2D some more methods P2
    if(pLwRm->IsClassSupported(LW50_TWOD, m_pDev))
    {
        CHECK_RC_CLEANUP(Writeto2DSubchP2(TESLA_SUBCH_HW2D));
    }

    //3   --> 3D some SW methds P1
    CHECK_RC_CLEANUP(Writeto3DSWmethodsP1(TESLA_SUBCH_SW3D));

    //1   --> 3D some HW methds P2
    CHECK_RC_CLEANUP(Writeto3DSubchP2(TESLA_SUBCH_HW3D));

    //4   --> GR Nop/Notify
    CHECK_RC_CLEANUP(m_pCh->Write(TESLA_SUBCH_NOP1, LW5097_SET_OBJECT, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(TESLA_SUBCH_NOP1,LW5097_NO_OPERATION,0));

    //5   --> GR Nop/Notify
    CHECK_RC_CLEANUP(m_pCh->Write(TESLA_SUBCH_NOP2, LW5097_SET_OBJECT, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(TESLA_SUBCH_NOP2,LW5097_NO_OPERATION,0));

    //3   --> 3D some SW methds P2
    CHECK_RC_CLEANUP(Writeto3DSWmethodsP2(TESLA_SUBCH_SW3D));

    //6   --> GR Nop/Notify
    CHECK_RC_CLEANUP(m_pCh->Write(TESLA_SUBCH_NOP3, LW5097_SET_OBJECT, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(TESLA_SUBCH_NOP3,LW5097_NO_OPERATION,0));

    //7   --> Wait on event AND FLUSH
    CHECK_RC_CLEANUP(WritetoSubchAndWaitOnEvt(TESLA_SUBCH_WAITFORNOTIFY));

    if(pLwRm->IsClassSupported(GT200_TESLA, m_pDev))
    {
        //GT200_TESLA
        WriteGT200SWmethods(0);
    }

Cleanup:
    FreeTestResources();

    if(rc != OK)
    {
        Printf(Tee::PriHigh,
            "Error in RunTeslaStress : %d \n",(UINT32)rc);
    }

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();

    return rc;
}

//! \brief void Stress::FreeTestResources()
//! Frees the Resourses allocated by Tesla Srress
//----------------------------------------------------------------------------
void Stress::FreeTestResources()
{
    LwRmPtr pLwRm;

    while(m_nObjRefCount)
    {
        //Free the objects allocated
        //both 2d 3d
        pLwRm->Free(m_hObject[m_nObjRefCount]);
        m_nObjRefCount--;
    }
}

RC Stress::Write8397NOP(UINT32 nSubCh)
{
    RC rc;
    CHECK_RC(m_pCh->Write(nSubCh, LW8397_NO_OPERATION, 0));
    CHECK_RC(m_pCh->Write(nSubCh, LW8397_NO_OPERATION, 0));
    CHECK_RC(m_pCh->Write(nSubCh, LW8397_NO_OPERATION, 0));
    CHECK_RC(m_pCh->Write(nSubCh, LW8397_NO_OPERATION, 0));

    return rc;
}

//! \brief RC Stress ::WriteGT200SWmethodes(UINT32 nSubCh)
//! writes GT200 SW methodes  to channel
//----------------------------------------------------------------------------
RC Stress::WriteGT200SWmethods(UINT32 nSubCh)
{
    Notifier obNotifier;

    LwRmPtr pLwRm;
    LwRm::Handle hTeslaObject;
    RC rc;

    //Allocate Object
    LW_GR_ALLOCATION_PARAMETERS    dispAllocParams = {0};
    dispAllocParams.version = 2;
    dispAllocParams.flags = 0;
    dispAllocParams.size = sizeof(dispAllocParams);
    dispAllocParams.caps = 0;

    CHECK_RC_CLEANUP(pLwRm->Alloc(m_hCh, &hTeslaObject, GT200_TESLA, &dispAllocParams));

    CHECK_RC_CLEANUP(m_pCh->SetObject(nSubCh, hTeslaObject));

    // Disable all shader exceptions
    CHECK_RC_CLEANUP(m_pCh->Write(nSubCh, LW8397_SET_SHADER_EXCEPTIONS,
        DRF_DEF(8397, _SET_SHADER_EXCEPTIONS, _ENABLE, _FALSE)));

    CHECK_RC_CLEANUP(Write8397NOP(nSubCh));

    // Enable all shader exceptions
    CHECK_RC_CLEANUP(m_pCh->Write(nSubCh, LW8397_SET_SHADER_EXCEPTIONS,
        DRF_DEF(8397, _SET_SHADER_EXCEPTIONS, _ENABLE, _TRUE)));

    CHECK_RC_CLEANUP(Write8397NOP(nSubCh));

    // Disable fast polymode
    CHECK_RC_CLEANUP(m_pCh->Write(nSubCh, LW8397_SET_FAST_POLYMODE,
        DRF_DEF(8397, _SET_FAST_POLYMODE, _ENABLE, _FALSE)));

    CHECK_RC_CLEANUP(Write8397NOP(nSubCh));

    // Enable fast polymode
    CHECK_RC_CLEANUP(m_pCh->Write(nSubCh, LW8397_SET_FAST_POLYMODE,
        DRF_DEF(8397, _SET_FAST_POLYMODE, _ENABLE, _TRUE)));

    CHECK_RC_CLEANUP(Write8397NOP(nSubCh));

    //Allocate Notifier and wait on notifier

    CHECK_RC_CLEANUP(obNotifier.Allocate(m_pCh,
        1,
        &m_TestConfig));

    CHECK_RC_CLEANUP(obNotifier.Instantiate(nSubCh,
        LW8397_SET_CONTEXT_DMA_NOTIFY));

    obNotifier.Clear(TESLA2_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_pCh->Write(nSubCh,
        LW8397_NOTIFY,
        LW8397_NOTIFY_TYPE_WRITE_ONLY));

    CHECK_RC_CLEANUP(m_pCh->Write(nSubCh, LW8397_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    CHECK_RC_CLEANUP(obNotifier.Wait(TESLA2_NOTIFIERS_NOTIFY, (3 * m_TestConfig.TimeoutMs())));

Cleanup:

    if(hTeslaObject != 0)
    {
        pLwRm->Free(hTeslaObject);
    }

    obNotifier.Free();

    if(rc != OK)
    {
        Printf(Tee::PriHigh,"RC Error oclwred WriteGT200SWmethods %d\n",(UINT32)rc);
    }
    return rc;
}

//! \brief RC Stress ::WritetoSubchAndWaitOnEvt(UINT32 nSubCh)
//! writes notifier to last subchannel and waits for the event to trigger
//----------------------------------------------------------------------------
RC Stress ::WritetoSubchAndWaitOnEvt(UINT32 nSubCh)
{
    LwRmPtr pLwRm;
    RC rc;
    const UINT32 memSize = 0x1000;
    LwU64 nMemLimit = memSize -1;
    void *pDummy;

    CHECK_RC(pLwRm->AllocMemory(&m_hNotifierMem, LW01_MEMORY_SYSTEM,
        DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
        DRF_DEF(OS02, _FLAGS, _COHERENCY, _UNCACHED) |
        DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
        DRF_DEF(OS02, _FLAGS, _MAPPING, _NO_MAP),
        &pDummy, &nMemLimit, m_pDev));

    CHECK_RC(pLwRm->MapMemory(m_hNotifierMem,0,memSize,&m_pNotifierMem,0,m_pDev->GetSubdevice(0)));

    m_pNotifyBuffer = (LwNotification *)((char *)m_pNotifierMem + 0);

    CHECK_RC(pLwRm->AllocContextDma(&m_hNotifyCtxDma,
        DRF_DEF(OS03, _FLAGS, _HASH_TABLE, _DISABLE) |DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE),
        m_hNotifierMem,
        0,
        nMemLimit));

    m_pNotifyEvent = Tasker::AllocEvent();

    m_pEventAddr = Tasker::GetOsEvent(
            m_pNotifyEvent,
            pLwRm->GetClientHandle(),
            pLwRm->GetDeviceHandle(m_pDev));

    CHECK_RC_CLEANUP(pLwRm->AllocEvent(m_hObject[m_nObjRefCount],&m_hNotifyEvent,LW01_EVENT_OS_EVENT,0,m_pEventAddr));

    MEM_WR16(&m_pNotifyBuffer->status, 0x8000);

    CHECK_RC_CLEANUP(m_pCh->Write(GPUFIFO_LASTSUBCH, LW5097_SET_OBJECT, m_hObject[m_nObjRefCount]));

    //LW5097_SET_CONTEXT_DMA_NOTIFY
    CHECK_RC_CLEANUP(m_pCh->Write(GPUFIFO_LASTSUBCH, LW5097_SET_CONTEXT_DMA_NOTIFY,m_hNotifyCtxDma));

    CHECK_RC_CLEANUP(m_pCh->Write(GPUFIFO_LASTSUBCH, LW5097_NOTIFY,
        LW5097_NOTIFY_TYPE_WRITE_THEN_AWAKEN));

    // Writing some method of the object
    CHECK_RC_CLEANUP(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_VERTEX, 0));

    // Writing some method of the object
    CHECK_RC_CLEANUP(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_SHADER_PROGRAM, 0));

    CHECK_RC_CLEANUP(m_pCh->Write(GPUFIFO_LASTSUBCH , LW5097_NO_OPERATION, 0));

    //flush all the methodes on channel
    CHECK_RC_CLEANUP(m_pCh->Flush());

    //wait on event
    CHECK_RC_CLEANUP(Tasker::WaitOnEvent(m_pNotifyEvent,
                         Tasker::FixTimeout(m_TestConfig.TimeoutMs())));

Cleanup:

    if(m_hNotifierMem != 0 && m_hNotifyCtxDma != 0 && m_pNotifyEvent != NULL)
    {
        pLwRm->UnmapMemory(m_hNotifierMem,(void *)m_pNotifierMem, 0, m_pDev->GetSubdevice(0));
        pLwRm->Free(m_hNotifyCtxDma);
        pLwRm->Free(m_hNotifierMem);
        Tasker::FreeEvent(m_pNotifyEvent);
        pLwRm->Free(m_hObject[m_nObjRefCount--]);
    }

    if (m_bFree2d)
    {
        //
        //free The Resources allocated for calling 2D & 3D methodes
        //
        FreeTestMemory2D();
    }

    if(rc != OK)
    {
        Printf(Tee::PriHigh,"RC Error oclwred WritetoSubchAndWaitOnEvt %d\n",(UINT32)rc);
    }

    return rc;
}

//! \brief RC Stress ::AllocTestMemory2D()
//!  Aloocates Test Memeory
//----------------------------------------------------------------------------
RC Stress::AllocTestMemory2D()
{
    RC rc;
    LwRmPtr pLwRm;

    UINT32 flags = 0;

    // Allocate mapping virtual object
    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(m_pDev),
                          &m_hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    // Allocate  video memory
    CHECK_RC(pLwRm->VidHeapAllocSize(LWOS32_TYPE_IMAGE,
        LWOS32_ATTR_NONE,
        LWOS32_ATTR2_NONE,
        TestMemSize,
        &m_hVidMem,
        0,
        nullptr,
        nullptr,
        nullptr,
        m_pDev));

    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hVidMem,
        0, TestMemSize,
        flags,
        &m_gpuAddr, m_pDev));

    m_bFree2d = true;
    return rc;
}

//! \brief RC Stress ::FreeTestMemory2D()
//!  Frees Test Memeory2D
//----------------------------------------------------------------------------
void Stress::FreeTestMemory2D()
{
    LwRmPtr pLwRm;

    if(m_hVidMem != 0 && m_gpuAddr != 0 && m_hVA != 0)
    {
        pLwRm->UnmapMemoryDma(m_hVA, m_hVidMem,
            LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, m_pDev);
        pLwRm->Free(m_hVidMem);
        pLwRm->Free(m_hVA);
    }
}

//! \brief RC Stress ::Writeto2DSubchP1(UINT32 nSubCh)
//!  tests with 2d hwmethodes
//----------------------------------------------------------------------------
RC Stress::Writeto2DSubchP1(UINT32 nSubCh)
{
    LwRmPtr pLwRm;
    RC rc;

    //Alocate the memory space to render a Line
    CHECK_RC(AllocTestMemory2D());

    // Hopefully we can set all these ctxdma simply to be the client,channel va space.
    if((rc = pLwRm->Alloc(m_hCh,&m_hObject[++m_nObjRefCount],LW50_TWOD,0)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }
    CHECK_RC(m_pCh->SetObject(nSubCh,m_hObject[m_nObjRefCount]));

    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_CONTEXT_DMA_NOTIFY, m_hVA));
    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_DST_CONTEXT_DMA, m_hVA));
    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_SRC_CONTEXT_DMA, m_hVA));

    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_DST_MEMORY_LAYOUT, LW502D_SET_DST_MEMORY_LAYOUT_V_PITCH));
    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_DST_PITCH,         TestMemPitch/*bytes*/));
    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_DST_WIDTH,         TestMemPitch/sizeof(UINT32)/*pixels*/));

    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_SRC_MEMORY_LAYOUT, LW502D_SET_SRC_MEMORY_LAYOUT_V_PITCH));
    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_SRC_PITCH,         TestMemPitch/*bytes*/));
    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_SRC_WIDTH,         TestMemPitch/sizeof(UINT32)/*pixels*/));

    return rc;
}
//! \brief RC Stress ::Writeto2DSubchP2(UINT32 nSubCh)
//!  tests with 2d hwmethodes
//----------------------------------------------------------------------------
RC Stress::Writeto2DSubchP2(UINT32 nSubCh)
{

    RC rc;
    UINT32 color = 0x55555555;

    // we enable clipping by default to the limit of the dest bound rectangle..
    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_CLIP_ENABLE,  DRF_DEF(502D, _SET_CLIP_ENABLE, _V, _FALSE)));

    // use client's 2D SubChannel to draw one line in the given "color"
    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_DST_OFFSET_UPPER, (LwU32)(m_gpuAddr>>32)));

    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_DST_OFFSET_LOWER, (LwU32)(m_gpuAddr & (~(LwU32)0))));

    CHECK_RC(m_pCh->Write(nSubCh, LW502D_RENDER_SOLID_PRIM_MODE, LW502D_RENDER_SOLID_PRIM_MODE_V_LINES));

    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT,
        LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8R8G8B8));

    CHECK_RC(m_pCh->Write(nSubCh, LW502D_SET_RENDER_SOLID_PRIM_COLOR, color));

    CHECK_RC(m_pCh->Write(nSubCh, LW502D_RENDER_SOLID_PRIM_POINT_SET_X(0), 0));
    CHECK_RC(m_pCh->Write(nSubCh, LW502D_RENDER_SOLID_PRIM_POINT_Y(0), 0));
    CHECK_RC(m_pCh->Write(nSubCh, LW502D_RENDER_SOLID_PRIM_POINT_SET_X(1), (TestMemPitch/sizeof(UINT32)))); //hopefully endpoint dropped
    CHECK_RC(m_pCh->Write(nSubCh, LW502D_RENDER_SOLID_PRIM_POINT_Y(1), 0));

    return rc;
}

//! \brief RC Stress ::Writeto3DSubchP1(UINT32 nSubCh)
//!  tests with 3d hwmethodes
//----------------------------------------------------------------------------
RC Stress::Writeto3DSubchP1(UINT32 nSubCh)
{
    RC rc;

    LwRmPtr pLwRm;

    if((rc = pLwRm->Alloc(m_hCh,&m_hObject[++m_nObjRefCount],LW50_TESLA,0)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }
    CHECK_RC(m_pCh->SetObject(nSubCh,m_hObject[m_nObjRefCount]));

    // Hopefully we can set all these ctxdma simply to be the client,channel va space.
    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_CONTEXT_DMA_NOTIFY, m_hVA));
    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_ZETA, m_hVA));
    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_SEMAPHORE, m_hVA));
    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_VERTEX, m_hVA));
    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_SHADER_THREAD_MEMORY, m_hVA));
    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_SHADER_THREAD_STACK, m_hVA));

    return rc;
}

//! \brief RC Stress ::Writeto3DSubchP2(UINT32 nSubCh)
//!  tests with 3d hwmethodes
//----------------------------------------------------------------------------
RC Stress::Writeto3DSubchP2(UINT32 nSubCh)
{
    RC rc;

    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_SHADER_PROGRAM, m_hVA));
    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_TEXTURE_SAMPLER, m_hVA));
    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_TEXTURE_HEADERS, m_hVA));
    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_TEXTURE, m_hVA));
    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_STREAMING, m_hVA));
    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_CLIP_ID, m_hVA));
    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_CTX_DMA_COLOR(0), m_hVA));

    return rc;
}

//! \brief RC Stress ::WriteNOPmethods(UINT32 nSubCh)
//!  tests with no operation
//----------------------------------------------------------------------------
RC Stress::WriteNOPmethods(UINT32 nSubCh)
{
    RC rc;

    for(UINT32 i = 0; i < 4; i++)
    {
        CHECK_RC(m_pCh->Write(nSubCh,LW5097_NO_OPERATION,0));
    }
    return rc;
}

//! \brief RC Stress ::Writeto3DSWmethodsP1(UINT32 nSubCh)
//!  tests with 3D SWmethods
//----------------------------------------------------------------------------
RC Stress::Writeto3DSWmethodsP1(UINT32 nSubCh)
{
    RC rc;

    //here it's giving error if we allocate more then one object for LW50_TESLA for channel
    //CHECK_RC(pLwRm->Alloc(m_hCh,&m_hObject[m_nObjRefCount],LW50_TESLA,NULL));

    CHECK_RC(m_pCh->SetObject(nSubCh,m_hObject[m_nObjRefCount]));

    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_FAST_POLYMODE,
        DRF_DEF(5097, _SET_FAST_POLYMODE, _ENABLE, _FALSE)));

    CHECK_RC(WriteNOPmethods(nSubCh));
    return rc;
}

//! \brief RC Stress ::Writeto3DSWmethodsP2(UINT32 nSubCh)
//!  tests with 3D SWmethods
//----------------------------------------------------------------------------
RC Stress::Writeto3DSWmethodsP2(UINT32 nSubCh)
{
    RC rc;

    CHECK_RC(m_pCh->Write(nSubCh, LW5097_SET_FAST_POLYMODE,
        DRF_DEF(5097, _SET_FAST_POLYMODE, _ENABLE, _TRUE)));

    CHECK_RC(WriteNOPmethods(nSubCh));
    return rc;
}

//! \brief RC Stress::Class9097Test()
//! tests with FERMI_A class for fermi
//  TBD : need  to be enhanced
//----------------------------------------------------------------------------
RC Stress::Class9097Test()
{
    RC rc;
    UINT32 nSubCh= 0;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    //
    // Allocate  object
    //
    if((rc = pLwRm->Alloc(m_hCh,&m_hObject[++m_nObjRefCount],FERMI_A,NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh,FERMI_A_NOTIFIERS_MAXCOUNT,&m_TestConfig));

    for(nSubCh = 0; nSubCh < MAX_SUBCHANNEL_COUNT - 1; nSubCh++)
    {
        //
        // SetObject method
        //
        CHECK_RC_CLEANUP(m_pCh->SetObject(nSubCh, m_hObject[m_nObjRefCount]));
        CHECK_RC_CLEANUP(m_pCh->Write(nSubCh, LW9097_NO_OPERATION, 0));
    }

    CHECK_RC_CLEANUP(m_pCh->SetObject(GPUFIFO_LASTSUBCH, m_hObject[m_nObjRefCount]));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(GPUFIFO_LASTSUBCH));

    m_Notifier.Clear(LW9097_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(GPUFIFO_LASTSUBCH));

    CHECK_RC_CLEANUP(m_pCh->Write(GPUFIFO_LASTSUBCH, LW9097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs =  m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LW9097_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassA097Test()
//! tests with KEPLER_A class for Kepler
//  TBD : need  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassA097Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if((rc = pLwRm->Alloc(m_hCh,&m_hObject[++m_nObjRefCount],KEPLER_A,NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh,KEPLER_A_NOTIFIERS_MAXCOUNT,&m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWA097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LW9097_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LW9097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs =  m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LW9097_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassA197Test()
//! tests with KEPLER_B class for Kepler
//  TBD : need  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassA197Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if((rc = pLwRm->Alloc(m_hCh,&m_hObject[++m_nObjRefCount],KEPLER_B,NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh,KEPLER_B_NOTIFIERS_MAXCOUNT,&m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWA197_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LW9097_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LW9097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs =  m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LW9097_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassA297Test()
//! tests with KEPLER_C class for Kepler
//  TBD : need  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassA297Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if((rc = pLwRm->Alloc(m_hCh,&m_hObject[++m_nObjRefCount],KEPLER_C,NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh,KEPLER_C_NOTIFIERS_MAXCOUNT,&m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWA297_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LW9097_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LW9097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs =  m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LW9097_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassB097Test()
//! tests with MAXWELL_A class for Maxwell
//  TBD : needs  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassB097Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if((rc = pLwRm->Alloc(m_hCh,&m_hObject[++m_nObjRefCount],MAXWELL_A,NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh,MAXWELL_A_NOTIFIERS_MAXCOUNT,&m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWB097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LWB097_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LW9097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs =  m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LWB097_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassB197Test()
//! tests with MAXWELL_B class for Maxwell
//  TBD : needs  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassB197Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if((rc = pLwRm->Alloc(m_hCh,&m_hObject[++m_nObjRefCount],MAXWELL_B,NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh,MAXWELL_B_NOTIFIERS_MAXCOUNT,&m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWB097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LWB197_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LW9097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs =  m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LWB197_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassC097Test()
//! tests with PASCAL_A class for Pascal
//  TBD : needs  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassC097Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if((rc = pLwRm->Alloc(m_hCh,&m_hObject[++m_nObjRefCount],PASCAL_A,NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh,PASCAL_A_NOTIFIERS_MAXCOUNT,&m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWC097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LWC097_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LW9097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs =  m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LWC097_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassC197Test()
//! tests with PASCAL_A class for Pascal
//  TBD : needs  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassC197Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if((rc = pLwRm->Alloc(m_hCh,&m_hObject[++m_nObjRefCount],PASCAL_B,NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh,PASCAL_B_NOTIFIERS_MAXCOUNT,&m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWC097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LWC197_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LW9097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs =  m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LWC197_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassC397Test()
//! tests with PASCAL_A class for Pascal
//  TBD : needs  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassC397Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if((rc = pLwRm->Alloc(m_hCh,&m_hObject[++m_nObjRefCount],VOLTA_A,NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh,PASCAL_A_NOTIFIERS_MAXCOUNT,&m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWC397_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LWC397_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LW9097_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs =  m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LWC397_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassC597Test()
//! tests with TURING_A class for Turing
//  TBD : needs  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassC597Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if((rc = pLwRm->Alloc(m_hCh,&m_hObject[++m_nObjRefCount],TURING_A,NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh,PASCAL_A_NOTIFIERS_MAXCOUNT,&m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWC597_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LWC597_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWC597_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs =  m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LWC597_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassC697Test()
//! tests with AMPERE_A class for Ampere
//  TBD : needs  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassC697Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if ((rc = pLwRm->Alloc(m_hCh, &m_hObject[++m_nObjRefCount], AMPERE_A, NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh, PASCAL_A_NOTIFIERS_MAXCOUNT, &m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWC697_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LWC697_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWC697_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs = m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LWC697_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassC797Test()
//! tests with AMPERE_B class
//  TBD : needs  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassC797Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if ((rc = pLwRm->Alloc(m_hCh, &m_hObject[++m_nObjRefCount], AMPERE_B, NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh, PASCAL_A_NOTIFIERS_MAXCOUNT, &m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWC797_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LWC797_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWC797_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs = m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LWC797_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassC997Test()
//! tests with ADA_A class for Ada
//  TBD : needs  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassC997Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if ((rc = pLwRm->Alloc(m_hCh, &m_hObject[++m_nObjRefCount], ADA_A, NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh, PASCAL_A_NOTIFIERS_MAXCOUNT, &m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWC997_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LWC997_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWC997_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs = m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LWC997_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}


//! \brief RC Stress::ClassCB97Test()
//! tests with HOPPER_A class
//  TBD : needs  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassCB97Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if ((rc = pLwRm->Alloc(m_hCh, &m_hObject[++m_nObjRefCount], HOPPER_A, NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh, PASCAL_A_NOTIFIERS_MAXCOUNT, &m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWCB97_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LWCB97_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWCB97_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs = m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LWCB97_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassCC97Test()
//! tests with HOPPER_B class
//  TBD : needs  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassCC97Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if ((rc = pLwRm->Alloc(m_hCh, &m_hObject[++m_nObjRefCount], HOPPER_B, NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh, PASCAL_A_NOTIFIERS_MAXCOUNT, &m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWCC97_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LWCC97_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWCC97_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs = m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LWCC97_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}

//! \brief RC Stress::ClassCD97Test()
//! tests with BLACKWELL_A class
//  TBD : needs  to be enhanced
//----------------------------------------------------------------------------
RC Stress::ClassCD97Test()
{
    RC rc;
    LwRmPtr pLwRm;
    FLOAT64 TimeoutMs = 0;

    if ((rc = pLwRm->Alloc(m_hCh, &m_hObject[++m_nObjRefCount], BLACKWELL_A, NULL)) != OK)
    {
        m_nObjRefCount--;
        return rc;
    }

    //
    // Allocate notifier
    //
    CHECK_RC(m_Notifier.Allocate(
        m_pCh, PASCAL_A_NOTIFIERS_MAXCOUNT, &m_TestConfig));

    CHECK_RC_CLEANUP(m_pCh->SetObject(LWA06F_SUBCHANNEL_3D, m_hObject[m_nObjRefCount]));
    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWCD97_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_Notifier.Instantiate(LWA06F_SUBCHANNEL_3D));

    m_Notifier.Clear(LWCD97_NOTIFIERS_NOTIFY);

    CHECK_RC_CLEANUP(m_Notifier.Write(LWA06F_SUBCHANNEL_3D));

    CHECK_RC_CLEANUP(m_pCh->Write(LWA06F_SUBCHANNEL_3D, LWCD97_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(m_pCh->Flush());

    TimeoutMs = m_TestConfig.TimeoutMs() * 10;

    m_Notifier.Wait(LWCD97_NOTIFIERS_NOTIFY, TimeoutMs);

    //
    // Check for the error on the channel through notifier
    //
    rc = m_pCh->CheckError();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
            "parseErrorTest RC recovery: notifier incorrect\n");
    }

Cleanup:

    m_Notifier.Free();

    pLwRm->Free(m_hObject[m_nObjRefCount]);
    m_nObjRefCount--;

    //
    // to switch to other threads. Returning to main thread
    //
    Tasker::Yield();
    return rc;
}
//----------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ AllocContextDma2Test
//!object.
//----------------------------------------------------------------------------
JS_CLASS_INHERIT(GpuFifoStressTest, RmTest,
                 "RmGpuFifoStressTest functionality test.");

CLASS_PROP_READWRITE(GpuFifoStressTest,StressTime, UINT32,
                     "Configurable RunTime of Stress test: 30=Default.");
