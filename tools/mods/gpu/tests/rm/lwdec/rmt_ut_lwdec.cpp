/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// RM test to test LWDEC class
//
//
//!
//! \file rmt_lwdec.cpp
//! \brief RM test to test LWDEC class

//!
//! This file tests LWDEC. The test allocates an object of the class, and tries to
//! write a value to a memory and read it back to make sure it went through. It
//! also checks that lwdec was able to notify the CPU about the semaphore release
//! using a nonstall interrupt
//!

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/jscript.h"
#include "gpu/include/gralloc.h"
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cla0b0.h" // LWA0B0_VIDEO_DECODER
#include "class/clb0b0.h" // LWB0B0_VIDEO_DECODER
#include "class/clb6b0.h" // LWB6B0_VIDEO_DECODER
#include "class/clb8b0.h" // LWB8B0_VIDEO_DECODER
#include "class/clc1b0.h" // LWC1B0_VIDEO_DECODER
#include "class/clc2b0.h" // LWC2B0_VIDEO_DECODER
#include "class/clc3b0.h" // LWC3B0_VIDEO_DECODER
#include "class/clc4b0.h" // LWC4B0_VIDEO_DECODER
#include "class/clc6b0.h" // LWC6B0_VIDEO_DECODER
#include "class/clc7b0.h" // LWC7B0_VIDEO_DECODER
#include "class/clc9b0.h" // LWC9B0_VIDEO_DECODER

#include "gpu/tests/rmtest.h"
#include "core/include/memcheck.h"
#include "gpu/utility/surf2d.h"

#include "hopper/gh100/dev_falcon_v4.h"

#define RMTEST_FMODEL_TESTING   /* This define is required in ucode-interface.h for print level control 
                                   This must be placed here before including ucode-interface.h */
#include "tests/ucode-interface.h"
#include "tests/ut_lwdec_fmodel_setup.h"

#define FILENAME_RELATIVE_PATH    "../gpu/tests/rm/lwdec/" /* relative to MODS runspace */
#define RESULTS_FILE              "out/ut_testresults.txt"
#define SUMMARY_FILE              "out/summarylog.txt"
#define TESTCONFIG_BUFFER_SIZE    (0x4000)
#define UCODE_DATA_BUFFER_SIZE    (0x60000) /* Using same space allocated in linker script for fmodel UT build */
#define UP_ALIGN16(addr, type)    (type)(((LwUPtr)(addr) + 0xf) & ~(0xfULL))

typedef struct
{
    Surface2D surf2d;
    LwU32 gpuAddrBlkFb;
    LwU32 *pCpuAddrFb;
} Mem_Desc;

static void WriteLwdecReg(GpuSubdevice* subdev, LwU32 address, LwU32 value)
{
    subdev->RegWr32(LW_FALCON_LWDEC_BASE + address, value);
    return;
}
static LwU32 ReadLwdecReg(GpuSubdevice* subdev, LwU32 address)
{
    return subdev->RegRd32(LW_FALCON_LWDEC_BASE + address);
}

static void MemcpyFromFBLwdec(void *dest, void *src, LwU32 len)
{    
    int len_words = len/4;
    int len_bytes = len%4;
    int i;
    LwU8 *_src = (LwU8*)src;
    LwU8 *_dest = (LwU8*)dest;

    for(i=0; i<len_words; i++){
        *(LwU32*)_dest = MEM_RD32(_src);
        _src += sizeof(LwU32);
        _dest += sizeof(LwU32);
    }
    for(i=0; i<len_bytes; i++){
        *(LwU8*)_dest = MEM_RD08(_src);
        _src += sizeof(LwU8);
        _dest += sizeof(LwU8);
    }

    return;
}

struct PollRegArgs
{
    UINT32 address;
    UINT32 value;
    bool   poll_low16;
    GpuSubdevice *subdev;

};

//
//! \brief Poll Function. Explicit function for polling
//!
//! Designed so as to remove the need for waitidle
//-----------------------------------------------------------------------------
static bool PollReg(void *pArgs)
{
    PollRegArgs Args = *(PollRegArgs *)pArgs;

    if(!Args.poll_low16){
        if (Args.value == ReadLwdecReg(Args.subdev, Args.address))
        {
            return true;
        }
    } else {
        if (Args.value == (ReadLwdecReg(Args.subdev, Args.address) & 0xFFFFU))
        {
            return true;
        }
    }

    return false;
}

static LwU64 GetPhysAddress(const Surface &surf, LwU64 offset)
{
    LwRmPtr pLwRm;
    LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0,0,0,0,0,0,0,0,0};

    params.memOffset = offset;
    RC rc = pLwRm->Control(surf.GetMemHandle(), LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR, &params,
                           sizeof(params));

    MASSERT(rc == OK);


    Printf(Tee::PriHigh, "Return address is :%016llx\n", params.memOffset);
           
    return params.memOffset;
}

static RC allocPhysicalBuffer(GpuDevice* pDev, Mem_Desc *pMem_Desc, LwU32 size)
{
    StickyRC rc;
    Surface2D *pVidMemSurface = &(pMem_Desc->surf2d);
    pVidMemSurface->SetForceSizeAlloc(1);
    pVidMemSurface->SetLayout(Surface2D::Pitch);
    pVidMemSurface->SetColorFormat(ColorUtils::Y8);
    pVidMemSurface->SetLocation(Memory::Fb);
    pVidMemSurface->SetArrayPitchAlignment(256);
    pVidMemSurface->SetArrayPitch(1);
    pVidMemSurface->SetArraySize(size);
    pVidMemSurface->SetAddressModel(Memory::Paging);
    pVidMemSurface->SetAlignment(256);
    pVidMemSurface->SetPhysContig(true);
    pVidMemSurface->SetLayout(Surface2D::Pitch);

    rc = pVidMemSurface->Alloc(pDev);
    if (rc != OK)
    {
        return rc;
    }
    rc = pVidMemSurface->Map();
    if (rc != OK)
    {
         pVidMemSurface->Free();
         return rc;
     }
 
     pMem_Desc->gpuAddrBlkFb = (LwU32)(GetPhysAddress(*pVidMemSurface, 0) >> 8);
     pMem_Desc->pCpuAddrFb = (LwU32 *)pVidMemSurface->GetAddress();
 
     return OK;
}

class LwdelwTest: public RmTest
{
public:
    LwdelwTest();
    virtual ~LwdelwTest();
    virtual string IsTestSupported();
    virtual bool   IsInstanceSupported(UINT32 instNum);
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(InstanceId, UINT32);
    SETGET_PROP(ACREnabled, bool);
    SETGET_PROP(DbgEnabled, bool);

private:
    int ParseConfigs(const char *filename, TestControls *tc);
    int ParseResults(const char *filename, const char *summary_filename, bool exceptionHit, UINT32 exceptionTestNo);
    void WriteLwdelwTConfigs();
    RC SetupLwdelwT();
    RC WaitForLwdelwTResults();
    RC CleanupLwdelwT(bool exceptionHit, UINT32 exceptionTestNo);

    UINT64       m_gpuAddr;
    UINT32 *     m_cpuAddr;
    UINT32       m_InstanceId;
    bool         m_ACREnabled;
    bool         m_DbgEnabled;

    /* Configs */
    TestControls m_testControls;
    TestRequests m_testRequests;
    TestResults m_testResults;
    EngineInfo m_engineInfo;

    GpuDevice *m_pGpuDev;
    GpuSubdevice *m_pSubDev;
    Mem_Desc m_Desc_scratch; // Used for configs/results passing
    Mem_Desc m_Desc_data;    // Used for Ucode data
    LwdelwtFmodelSetupStates_t m_state;
    UINT32 utFinalState;

    LwRm::Handle m_hObj;
    LwRm::Handle m_hVA;

    LwRm::Handle m_hSemMem;
    FLOAT64      m_TimeoutMs  = Tasker::NO_TIMEOUT;

    ModsEvent   *pNotifyEvent = NULL;

    GrClassAllocator *m_classAllocator;
};

//! \brief LwdelwTest constructor.
//!
//! \sa Setup
//------------------------------------------------------------------------------
LwdelwTest::LwdelwTest()
{
    m_gpuAddr = 0LL;
    m_cpuAddr = NULL;
    m_hObj    = 0;
    m_hVA     = 0;
    m_hSemMem = 0;
    m_InstanceId = 0;
    m_state = LWDEC_UT_FMODEL_ACR_ENABLED;
    m_ACREnabled = true;
    m_DbgEnabled = false;

    m_classAllocator = new LwdecAlloc();

    SetName("LwdelwTest");
}

//! \brief LwdelwTest destructor.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LwdelwTest::~LwdelwTest()
{
    delete m_classAllocator;
}

//! \brief Check if engine instance is supported
//!
//! \return true/false
//------------------------------------------------------------------------------
bool LwdelwTest::IsInstanceSupported(UINT32 instNum)
{
    RC     rc;
    UINT32 engineId = LW2080_ENGINE_TYPE_NULL;

    m_classAllocator->GetEngineId(GetBoundGpuDevice(),
                                  GetBoundRmClient(), instNum, &engineId);

    return (engineId == LW2080_ENGINE_TYPE_LWDEC(instNum));
}

//! \brief Make sure the class we're testing is supported by RM.
//!
//! \return True
//------------------------------------------------------------------------------
string LwdelwTest::IsTestSupported()
{
    GpuDevice *pGpuDev = GetBoundGpuDevice();

    if (m_classAllocator->IsSupported(pGpuDev))
    {
        if (IsInstanceSupported(GetInstanceId()))
        {
            Printf(Tee::PriHigh,
                   "Test is supported on the current GPU/platform(class id : 0x%x), preparing to run on LWDEC%d...\n",
                   m_classAllocator->GetClass(), GetInstanceId());
            return RUN_RMTEST_TRUE;
        }
        return "Engine instance Not supported on GPU";
    }
    else
    {
        switch(m_classAllocator->GetClass())
        {
            case LWA0B0_VIDEO_DECODER:
                return "Class LWA0B0_VIDEO_DECODER Not supported on GPU";
            case LWB0B0_VIDEO_DECODER:
                return "Class LWB0B0_VIDEO_DECODER Not supported on GPU";
            case LWB6B0_VIDEO_DECODER:
                return "Class LWB6B0_VIDEO_DECODER Not supported on GPU";
            case LWB8B0_VIDEO_DECODER:
                return "Class LWB8B0_VIDEO_DECODER Not supported on GPU";
            case LWC1B0_VIDEO_DECODER:
                return "Class LWC1B0_VIDEO_DECODER Not supported on GPU";
            case LWC2B0_VIDEO_DECODER:
                return "Class LWC2B0_VIDEO_DECODER Not supported on GPU";
            case LWC3B0_VIDEO_DECODER:
                return "Class LWC3B0_VIDEO_DECODER Not supported on GPU";
            case LWC4B0_VIDEO_DECODER:
                return "Class LWC4B0_VIDEO_DECODER Not supported on GPU";
            case LWC6B0_VIDEO_DECODER:
                return "Class LWC6B0_VIDEO_DECODER Not supported on GPU";
            case LWC7B0_VIDEO_DECODER:
                return "Class LWC7B0_VIDEO_DECODER Not supported on GPU";
            case LWC9B0_VIDEO_DECODER:
                return "Class LWC9B0_VIDEO_DECODER Not supported on GPU";
        }
        return "Invalid Class Specified";
    }
}

//! \brief Setup all necessary m_state before running the test.
//!
//! \sa IsSupported
//------------------------------------------------------------------------------
RC LwdelwTest::Setup()
{
    RC                                        rc = OK;
    //const UINT32                              memSize = 0x1000;
    //LwRmPtr                                   pLwRm;
    //GpuDevice                                *pGpuDev = GetBoundGpuDevice();
    //LwRm::Handle                              hNotifyEvent = 0;
    //void                                     *pEventAddr   = NULL;
    //LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventParams  = {0};

    CHECK_RC(InitFromJs());
    m_pSubDev = GetBoundGpuSubdevice();
    m_pGpuDev = GetBoundGpuDevice();
    memset(&m_testControls, 0, sizeof(m_testControls));
    memset(&m_testResults, 0, sizeof(m_testResults));
    memset(&m_testRequests, 0, sizeof(m_testRequests));
    memset(&m_engineInfo, 0, sizeof(m_engineInfo));

    CHECK_RC(allocPhysicalBuffer(m_pGpuDev, &m_Desc_scratch, TESTCONFIG_BUFFER_SIZE));   
    CHECK_RC(allocPhysicalBuffer(m_pGpuDev, &m_Desc_data, UCODE_DATA_BUFFER_SIZE));  

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    return rc;
}

RC LwdelwTest::SetupLwdelwT()
{
    PollRegArgs regargs;
    RC rc = OK;

    regargs.address = LW_PFALCON_FALCON_MAILBOX1; 
    regargs.subdev = m_pSubDev;
    regargs.poll_low16 = true;

    Printf(Tee::PriHigh, "LwdelwTest : Starting handshake with UT Ucode.\n");

    while(1){
        switch(m_state){
            case LWDEC_UT_FMODEL_ACR_ENABLED:
                /* Wait for ucode to reach this m_state */
                regargs.value = (LwU32)m_state; 
                CHECK_RC(POLLWRAP(&PollReg, &regargs, m_TimeoutMs));

                /* Write FB address to Mailbox */
                WriteLwdecReg(m_pSubDev, LW_PFALCON_FALCON_MAILBOX1, m_ACREnabled ? 1 : 0);
                Printf(Tee::PriHigh, "LwdelwTest : ACR %s info sent!\n", 
                                                m_ACREnabled ? "enabled" : "disabled");

                m_state = LWDEC_UT_FMODEL_DEBUG_ENABLED;
                break;

            case LWDEC_UT_FMODEL_DEBUG_ENABLED:
                /* Wait for ucode to reach this m_state */
                regargs.value = (LwU32)m_state; 
                CHECK_RC(POLLWRAP(&PollReg, &regargs, m_TimeoutMs));

                /* Write FB address to Mailbox */
                WriteLwdecReg(m_pSubDev, LW_PFALCON_FALCON_MAILBOX1, m_DbgEnabled ? 1 : 0);
                Printf(Tee::PriHigh, "LwdelwTest : Debug %s info sent!\n", 
                                                m_DbgEnabled ? "enabled" : "disabled");

                m_state = LWDEC_UT_FMODEL_FB_SCRATCH_ADDR;
                break;

            case LWDEC_UT_FMODEL_FB_SCRATCH_ADDR:
                /* Wait for ucode to reach this m_state */
                regargs.value = (LwU32)m_state; 
                CHECK_RC(POLLWRAP(&PollReg, &regargs, m_TimeoutMs));

                /* Write FB address to Mailbox */
                WriteLwdecReg(m_pSubDev, LW_PFALCON_FALCON_MAILBOX1, m_Desc_scratch.gpuAddrBlkFb);
                Printf(Tee::PriHigh, "LwdelwTest : FB scratch address sent!\n");

                m_state = LWDEC_UT_FMODEL_FB_DATA_ADDR;
                break;

            case LWDEC_UT_FMODEL_FB_DATA_ADDR:
                /* Wait for ucode to reach this m_state */
                regargs.value = (LwU32)m_state; 
                CHECK_RC(POLLWRAP(&PollReg, &regargs, m_TimeoutMs));

                /* Write FB address to Mailbox */
                WriteLwdecReg(m_pSubDev, LW_PFALCON_FALCON_MAILBOX1, m_Desc_data.gpuAddrBlkFb);
                Printf(Tee::PriHigh, "LwdelwTest : FB data address sent!\n");

                m_state = LWDEC_UT_FMODEL_COMPLETE_HANDSHAKE;
                break;

            case LWDEC_UT_FMODEL_COMPLETE_HANDSHAKE :
                regargs.value = (LwU32)m_state; 
                CHECK_RC(POLLWRAP(&PollReg, &regargs, m_TimeoutMs));
                Printf(Tee::PriHigh, "LwdelwTest : Handshake complete!\n");

                m_state = LWDEC_UT_FMODEL_SETUP_COMPLETE;
                break;

            case LWDEC_UT_FMODEL_SETUP_COMPLETE :
                regargs.value = (LwU32)m_state; 
                CHECK_RC(POLLWRAP(&PollReg, &regargs, m_TimeoutMs));
                Printf(Tee::PriHigh, "LwdelwTest : Setup complete!\n");

                m_state = LWDEC_UT_FMODEL_TESTS_COMPLETE;
                return OK;
                break;

            default : 
                break;
        }
    }

    return OK;
}

RC LwdelwTest::WaitForLwdelwTResults()
{
    RC rc;
    PollRegArgs regargs;

    regargs.address = LW_PFALCON_FALCON_MAILBOX1; 
    regargs.subdev = m_pSubDev;
    regargs.value = (LwU32)m_state; 
    regargs.poll_low16 = true;

    Printf(Tee::PriHigh, "LwdelwTest : Waiting for test results...\n");
    rc = POLLWRAP(&PollReg, &regargs, m_TimeoutMs);
    utFinalState = ReadLwdecReg(m_pSubDev, LW_PFALCON_FALCON_MAILBOX1);
    Printf(Tee::PriHigh, "LwdelwTest : Tests complete! Received results. State = %08x!\n", utFinalState);
    return rc;
}

int LwdelwTest::ParseResults(const char* filename, const char *summary_filename, bool exceptionHit, UINT32 exceptionTestNo)
{
    FILE *fp, *summary_fp;
    int i, passed=0, failed=0;
    char *results_hdr = "\
------------------------------------------\n\
LWDEC UT Tests Summary :\n\
------------------------------------------\n\n\
LWDEC Instance      = %d\n\n";

    char *results_output_hdr = "\
************ OUTPUTS ***************\n\
Num Tests Run       = %d\n\
Num Tests Passed    = %d\n\
Num Tests Failed    = %d\n\n\
** Refer to mods log for detailed info **\n\n\
RESULTS:\n";

    char *results_table_hdr_fmt = "%15s | %-10s |\n";
    char *results_table_fmt = "%15d | %-10s |\n";


    fp = fopen(filename, "w");
    if(fp == NULL) {
        Printf(Tee::PriHigh, "Error opening out file! Exiting...\n");
        return -1;
    }

    fprintf(fp, results_hdr, m_InstanceId);

    if(exceptionHit){
        fprintf(fp, 
"***************************************************************\n\
TESTS FAILED! Exception hit on test %d!\n\
Results could not be retreived due to unknown processor state!\n\
****************************************************************\n", 
        exceptionTestNo);
        fprintf(fp, results_table_hdr_fmt, "Test Index", "Status");
        fprintf(fp, results_table_fmt, exceptionTestNo, 
                            "FAIL(Exception Hit)");
        failed = 1; /* For summarylog.txt */
        goto write_summarylog;
    }

    fprintf(fp, results_output_hdr,
                    m_testResults.numRan, m_testResults.numPassed, m_testResults.numFailed);


    fprintf(fp, results_table_hdr_fmt, "Test Index", "Status");
    for(i=m_testControls.idxTestToStart; i<m_testControls.numTestsToRun; i++){
        fprintf(fp, results_table_fmt, m_testControls.testOrder[i], 
                            m_testResults.testPassed[i] == LW_TRUE ? "PASS" : "FAIL");
        if(m_testResults.testPassed[i] == LW_TRUE)
        {
            passed++;
        }
        else 
        {
            failed++;
        }
    }

write_summarylog:
    Printf(Tee::PriHigh, "Writing summarylog...\n");
    summary_fp = fopen(summary_filename, "w");
    if(summary_fp == NULL) {
        Printf(Tee::PriHigh, "Error opening out file! Exiting...\n");
        return -1;
    }
    fprintf(summary_fp, "RESULT\n");
    fprintf(summary_fp, "Passes     : %d\n", passed);
    fprintf(summary_fp, "Failures   : %d\n", failed);
    fprintf(summary_fp, "SANITY SCORE: %.2f\n", ((float)passed/(float)(passed+failed))*100.0);
    fclose(summary_fp);

    fclose(fp);
    return 0;
}

RC LwdelwTest::CleanupLwdelwT(bool exceptionHit, UINT32 exceptionTestNo)
{
    int ret = 0;

    /* Filename path relative to mods runspace */
    string filename = FILENAME_RELATIVE_PATH;
    string summary_filename = FILENAME_RELATIVE_PATH;
    filename += RESULTS_FILE;
    summary_filename += SUMMARY_FILE;
    
    if(exceptionHit){
        ret = ParseResults(filename.c_str(), summary_filename.c_str(), exceptionHit, exceptionTestNo);
        return ret ? !OK : OK;
    }

    LwU8 *src = UP_ALIGN16(m_Desc_scratch.pCpuAddrFb, LwU8*);

    /* Copy over test results from FB */
    MemcpyFromFBLwdec(&m_testResults, src, sizeof(TestResults));

    /* Copy over test configs from FB */
    src += sizeof(TestResults);
    src = UP_ALIGN16(src, LwU8*);
    MemcpyFromFBLwdec(&m_testControls, src, sizeof(TestControls));

    /* Ensure magic number matches. */
    if(m_testResults.magic != UCODE_MAGIC)
    {
        Printf(Tee::PriHigh, "LwdelwTest : Test result magic mismatch! Exiting...\n");
        return !OK;
    }

    /* Ensure all tests were actually completed */
    if(m_testResults.allDone != LW_TRUE)
    {
        Printf(Tee::PriHigh, "LwdelwTest : [WARNING] All tests not completed!\n");
    }

    ret = ParseResults(filename.c_str(), summary_filename.c_str(), exceptionHit, exceptionTestNo);
    if(ret){
        return !OK;
    }

    Printf(Tee::PriHigh, "LwdelwTest : Results written to %s\n", filename.c_str());
    return OK;
}

//! \brief Run test. Simply wait for all tests to complete 
//!
//! \return OK
//------------------------------------------------------------------------
RC LwdelwTest::Run()
{
    RC     rc = OK;
    UINT32 exceptionTestNo = 0;
    bool exceptionHit = false;

    ErrorLogger::StartingTest();

    /* Setup tests */
    CHECK_RC(SetupLwdelwT());

    /* Wait for tests to complete */
    CHECK_RC(WaitForLwdelwTResults());

    /* Check if exception is hit */
    if(LWDEC_UT_FMODEL_IS_EXCEPTION_HIT(utFinalState))
    {
        exceptionHit = true;
        exceptionTestNo = LWDEC_UT_FMODEL_GET_EXCEPTION_TEST_NO(utFinalState);
    } 

    /* Cleanup */
    CHECK_RC(CleanupLwdelwT(exceptionHit, exceptionTestNo));

    ErrorLogger::TestCompleted();
    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
RC LwdelwTest::Cleanup()
{
    m_Desc_scratch.surf2d.Free();
    m_Desc_data.surf2d.Free();
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ LwdelwTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(LwdelwTest, RmTest, "LWDEC RM unit test.");
CLASS_PROP_READWRITE(LwdelwTest, InstanceId, UINT32,
                     "Instance number to test");
CLASS_PROP_READWRITE(LwdelwTest, ACREnabled, bool,
                     "Inform ucode if ACR is enabled. If true, ucode booted by ACR");
CLASS_PROP_READWRITE(LwdelwTest, DbgEnabled, bool,
                     "Inform ucode if debug is enabled. If true, ucode wait after setup");
