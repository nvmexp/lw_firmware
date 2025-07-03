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

//
// Compute class SW method test.
//
// Allocate a compute object, and send the following SW methods:
// - LWA0CO_SET_SHADER_EXCEPTION
// - LWA1C0_ACTIVATE_PERF_SETTINGS_FOR_COMPUTE_CONTEXT
//
#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/tests/rmtest.h"

#include "gpu/tests/gputestc.h"
#include "gpu/include/notifier.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/xp.h"
#include "core/include/tasker.h"

#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cla0c0.h" // KEPLER_COMPUTE_A
#include "class/cla0c0sw.h"
#include "class/cla1c0.h" // KEPLER_COMPUTE_B
#include "class/cla06fsubch.h"
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/cla06c.h" // KEPLER_CHANNEL_GROUP_A
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"
#include "gpu/tests/rm/utility/smutil.h"
#include "gpu/utility/tsg.h"

#define RM_PAGE_SIZE    (1024 * 4)

#define KASSERT( condition, errorMessage, rcVariable )                                   \
if ( ! ( condition ) )                                                                   \
{                                                                                        \
    Printf( Tee::PriHigh, errorMessage );                                                \
    rcVariable = RC::LWRM_ERROR;                                                         \
}

//!
//! Flag to indicate Successful semaphore releases
//!
#define FE_SEMAPHORE_VALUE  0xF000F000
static bool isSemaReleaseSuccess = false;

//! \brief PollFunc: Static function
//!
//! This function is a static one used for the poll and timeout.
//! Polling on the semaphore release condition, timeout condition
//! in case the sema isn't released.
//!
//! \return TRUE if the sema released else false.
//!
//! \sa Run
//-----------------------------------------------------------------------------
static bool PollFunc(void *pArg)
{
    UINT32 data = MEM_RD32(pArg);

    if (data == FE_SEMAPHORE_VALUE)
    {
        Printf(Tee::PriLow, "Sema exit Success \n");
        isSemaReleaseSuccess = true;
        return true;
    }
    else
    {
        isSemaReleaseSuccess = false;
        return false;
    }

}

#define COMPUTE_SUBCHANNEL        LWA06F_SUBCHANNEL_COMPUTE
#define NUM_TEST_CHANNELS           2

#define SHADER_EXCEPTION            0

class ComputeSwMethodTest : public RmTest
{
public:
    ComputeSwMethodTest();
    virtual ~ComputeSwMethodTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    UINT64          m_gpuAddr                         = 0;
    UINT32 *        m_cpuAddr                         = nullptr;

    Tsg *           m_pTsg[NUM_TEST_CHANNELS]         = {};
    Channel *       m_pCh[NUM_TEST_CHANNELS]          = {};

    LwRm::Handle    m_hCh[NUM_TEST_CHANNELS]          = {};
    LwRm::Handle    m_hVirtMem                        = 0;
    LwRm::Handle    m_hSemMem                         = 0;

    Notifier        m_Notifier[NUM_TEST_CHANNELS]     = {};
    ComputeAlloc    m_computeAlloc[NUM_TEST_CHANNELS] = {};
    FLOAT64         m_TimeoutMs                       = 0;

    UINT32          m_warpInitial                     = 0;
    UINT32          m_globalInitial                   = 0;
    UINT32          m_warpMaxEnabled                  = 0;
    UINT32          m_globalMaxEnabled                = 0;

    LwBool          m_bNeedWfi                        = LW_FALSE;
    LwBool          m_bChannelGroup                   = LW_FALSE;

    RC ChannelInit(UINT32 ch);
    RC ChannelNotify(UINT32 ch);
    RC SetupChannels();

    RC VerifyChannelContext(UINT32 ch, bool typeFlag, UINT32 cbSize, UINT32 warpValue, UINT32 globalValue);

    RC VerifyShaderException();
    RC EnableShaderExceptions(UINT32 ch, bool enable);
    RC CheckShaderExceptiolwalue(UINT32 ch, UINT32 warpExpected, UINT32 globalExpected);

    RC VerifyActivateComputeContextPerformanceSettings();
    RC GetActivateComputeContextPerformanceSettingsMethod(UINT32 *pMethod);
    RC ActivateComputeContextPerformanceSettings(UINT32 ch, UINT32 data);
};

//!
//! \brief ComputeSwMethodTest constructor
//!
//! ComputeSwMethodTest constructor does not do much.  Functionality
//! mostly lies in Setup().
//!
//! \sa Setup
//------------------------------------------------------------------------------
ComputeSwMethodTest::ComputeSwMethodTest()
{
    UINT32 ch;

    SetName("ComputeSwMethodTest");

    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        m_computeAlloc[ch].SetOldestSupportedClass(KEPLER_COMPUTE_A);
    }
}

//!
//! \brief ComputeSwMethodTest destructor
//!
//! ComputeSwMethodTest destructor does not do much.  Functionality
//! mostly lies in Cleanup().
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
ComputeSwMethodTest::~ComputeSwMethodTest()
{
}

//!
//! \brief Is ComputeSwMethodTest supported?
//!
//! Verify if ComputeSwMethodTest is supported in the current
//! environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------
string ComputeSwMethodTest::IsTestSupported()
{
    m_bNeedWfi = LW_FALSE;

    if (IsClassSupported(KEPLER_CHANNEL_GROUP_A))
    {
        m_bChannelGroup = LW_TRUE;
    }

    if (IsClassSupported(PASCAL_CHANNEL_GPFIFO_A))
    {
        m_bNeedWfi = LW_TRUE;
    }

    if (!(m_computeAlloc[0].IsSupported(GetBoundGpuDevice())))
        return "None of KEPLER_A+ class supported on current platform";

    return RUN_RMTEST_TRUE;
}

//!
//! \brief ComputeSwMethodTest Setup
//!
//! \return RC OK if all's well.
//------------------------------------------------------------------------------
RC ComputeSwMethodTest::Setup()
{
    RC              rc;
    LwRmPtr         pLwRm;
    UINT32          ch;
    UINT64          Offset;

    CHECK_RC(InitFromJs());

    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();

    // We will be using multiple channels in this test
    m_TestConfig.SetAllowMultipleChannels(true);

    // Allocate a page of Frame buffer memory for FE semaphore
    CHECK_RC(pLwRm->VidHeapAllocSize(LWOS32_TYPE_IMAGE,
        DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
        DRF_DEF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS) |
        DRF_DEF(OS32, _ATTR, _COHERENCY, _CACHED),
        LWOS32_ATTR2_NONE, RM_PAGE_SIZE, &m_hSemMem, &Offset,
        nullptr, nullptr, nullptr, GetBoundGpuDevice()));

    // Create an handle to virtual address space
    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS params = {};
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &m_hVirtMem, LW01_MEMORY_VIRTUAL, &params));

    //
    // Add the FB memory to the address space.  The "offset" returned
    // is actually the GPU virtual address in this case.
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hVirtMem,
                                 m_hSemMem,
                                 0,
                                 RM_PAGE_SIZE - 1,
                                 DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                 &m_gpuAddr,
                                 GetBoundGpuDevice()));

    // Get a CPU address as well
    CHECK_RC(pLwRm->MapMemory(m_hSemMem, 0,
                              RM_PAGE_SIZE - 1,
                              (void **)&m_cpuAddr,
                              0,
                              GetBoundGpuSubdevice()));

    // Allocate resources for all test channels
    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS chGrpParams = {0};
        chGrpParams.engineType = LW2080_ENGINE_TYPE_GR(0);

        if (m_bChannelGroup)
        {
            m_pTsg[ch] = new Tsg(&chGrpParams);

            m_pTsg[ch]->SetTestConfiguration(GetTestConfiguration());
            CHECK_RC(m_pTsg[ch]->Alloc());

            m_pCh[ch] = NULL;
            CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[ch], &m_hCh[ch], m_pTsg[ch],
                        nullptr, 0, 0, m_pTsg[ch]->GetEngineId()));
        }
        else
        {
            m_pCh[ch] = NULL;
            CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[ch], &m_hCh[ch], LW2080_ENGINE_TYPE_GR(0)));
        }

        CHECK_RC(m_computeAlloc[ch].Alloc(m_hCh[ch], GetBoundGpuDevice()));

        CHECK_RC(m_Notifier[ch].Allocate(m_pCh[ch], 1, &m_TestConfig));
    }

    return rc;
}

//! \brief Run() : The body of this test.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC ComputeSwMethodTest::Run()
{
    RC rc;

    CHECK_RC(SetupChannels());
    CHECK_RC(VerifyActivateComputeContextPerformanceSettings());
    CHECK_RC(VerifyShaderException());

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
RC ComputeSwMethodTest::Cleanup()
{
    RC      rc, firstRc;
    UINT32  ch;
    LwRmPtr pLwRm;

    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        // Free resources for all channels
        m_Notifier[ch].Free();
        m_computeAlloc[ch].Free();

        if (m_pCh[ch] != NULL)
            FIRST_RC(m_TestConfig.FreeChannel(m_pCh[ch]));

        if (m_pTsg[ch] != NULL)
        {
            m_pTsg[ch]->Free();
            delete m_pTsg[ch];
        }
    }

    pLwRm->UnmapMemory(m_hSemMem, m_cpuAddr, 0, GetBoundGpuSubdevice());
    pLwRm->UnmapMemoryDma(m_hVirtMem,
                          m_hSemMem,
                          DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                          m_gpuAddr,
                          GetBoundGpuDevice());
    pLwRm->Free(m_hVirtMem);
    pLwRm->Free(m_hSemMem);

    return firstRc;
}

//! \brief Check Shader Exceptions relevant register values
//!
//! \sa CheckShaderExceptiolwalue
//------------------------------------------------------------------------
RC ComputeSwMethodTest::CheckShaderExceptiolwalue(UINT32 ch, UINT32 warpExpected, UINT32 globalExpected)
{
    RC  rc;
    UINT32  warpValue;
    UINT32  globalValue;
    SMUtil  smUtil;

    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    Tasker::Sleep(100);

    // Avoid race condition of reading register before method completion
    CHECK_RC(m_pCh[ch]->WaitIdle(m_TimeoutMs));

    smUtil.ReadWarpGlobalMaskValues(pSubdev, m_hCh[ch], &warpValue, &globalValue, LW_TRUE);

    Printf(Tee::PriHigh,"%s: warp=0x%08x expected=0x%08x\n", __FUNCTION__, warpValue, warpExpected);
    Printf(Tee::PriHigh,"%s: global=0x%08x expected=0x%08x\n", __FUNCTION__, globalValue, globalExpected);

    // Register values should be equal to expected value
    if (warpValue != warpExpected || globalValue != globalExpected)
    {
        Printf(Tee::PriHigh,"%s: LWA0C0_SET_SHADER_EXCEPTIONS is not able to Enable all Shader Exceptions\n", __FUNCTION__);
        Printf(Tee::PriHigh,"    warp=0x%08x expected=0x%08x\n", warpValue, warpExpected);
        Printf(Tee::PriHigh,"    global=0x%08x expected=0x%08x\n", globalValue, globalExpected);
        //return RC::DATA_MISMATCH;
    }
    KASSERT( (warpValue == warpExpected && globalValue == globalExpected),
              "LWA0C0_SET_SHADER_EXCEPTIONS is not able to Perfrom expected operation\n", rc );

    return rc;
}

//! \brief ChannelInit(UINT32 ch) : Initialize the channel
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC ComputeSwMethodTest::ChannelInit(UINT32 ch)
{
    RC rc;

    CHECK_RC(m_pCh[ch]->SetObject(COMPUTE_SUBCHANNEL, m_computeAlloc[ch].GetHandle()));

    CHECK_RC(m_Notifier[ch].Instantiate(COMPUTE_SUBCHANNEL));

    m_pCh[ch]->SetSemaphoreOffset(m_gpuAddr);
    m_pCh[ch]->SemaphoreRelease(FE_SEMAPHORE_VALUE);
    CHECK_RC(m_pCh[ch]->Flush());

    // poll on event notification for semaphore release
    POLLWRAP(&PollFunc, m_cpuAddr, m_TestConfig.TimeoutMs());

    return rc;
}

//! \brief ChannelNotify(UINT32 ch) : Request notify and wait for completion
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC ComputeSwMethodTest::ChannelNotify(UINT32 ch)
{
    RC rc;

    m_Notifier[ch].Clear(LWA0C0_NOTIFIERS_NOTIFY);
    CHECK_RC(m_pCh[ch]->Write(COMPUTE_SUBCHANNEL, LWA0C0_NOTIFY,
                          LWA0C0_NOTIFY_TYPE_WRITE_ONLY));
    CHECK_RC(m_pCh[ch]->Write(COMPUTE_SUBCHANNEL, LWA0C0_NO_OPERATION, 0));

    CHECK_RC(m_pCh[ch]->Flush());
    CHECK_RC(m_Notifier[ch].Wait(LWA0C0_NOTIFIERS_NOTIFY, m_TimeoutMs));

    return rc;
}

//! \brief EnableShaderExceptions(UINT32 ch, bool enable) : Enable Shader Exceptions
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC ComputeSwMethodTest::EnableShaderExceptions(UINT32 ch, bool enable)
{
    RC rc;

    if (m_bNeedWfi)
    {
        CHECK_RC(m_pCh[ch]->Write(COMPUTE_SUBCHANNEL, LWC06F_WFI, DRF_DEF(C06F, _WFI, _SCOPE, _ALL)));
    }

    CHECK_RC(m_pCh[ch]->Write(COMPUTE_SUBCHANNEL, LWA0C0_SET_SHADER_EXCEPTIONS, enable));

    CHECK_RC_MSG(ChannelNotify(ch),
                    "Notify failed setting LWA0C0_SET_SHADER_EXCEPTIONS");

    CHECK_RC_MSG(m_pCh[ch]->CheckError(),
                    "Channel error setting LWA0C0_SET_SHADER_EXCEPTIONS");

    CHECK_RC(m_pCh[ch]->Flush());

    return rc;
}

//! \brief VerifyChannelContext(UINT32 ch, bool typeFlag, UINT32 cbSize, UINT32 warpValue, UINT32 globalValue)
//! \ Verify GRCtx info depending on typeFlag
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC ComputeSwMethodTest::VerifyChannelContext(UINT32 ch, bool typeFlag, UINT32 cbSize, UINT32 warpValue, UINT32 globalValue)
{
    RC      rc;
    UINT32  chIndex;

    // Acquire semaphore on the specified channel to block other channels from running
    m_pCh[ch]->SemaphoreAcquire(FE_SEMAPHORE_VALUE);
    CHECK_RC(m_pCh[ch]->Flush());

    // Request/wait for notify on specified channel to force channel's GR context to be loaded
    CHECK_RC_MSG(ChannelNotify(ch), "Notify failed verifying channel context");

    //
    // Send a NOPs on all other channel's just to prove that they are blocked waiting for the
    // semaphore release.
    // This isn't really needed but added for extra "stess"
    //
    for (chIndex = 0; chIndex < NUM_TEST_CHANNELS; chIndex++)
    {
        if (chIndex != ch)
        {
            CHECK_RC(m_pCh[chIndex]->Write(COMPUTE_SUBCHANNEL, LWA0C0_NO_OPERATION, 0));
            CHECK_RC(m_pCh[chIndex]->Flush());
        }
    }

    if (typeFlag == SHADER_EXCEPTION)
    {
        // Verify that the shader exception relevent register value for channel's GR context matches expected value.
        CHECK_RC_MSG(CheckShaderExceptiolwalue(ch, warpValue, globalValue),
                        "Channel's HWW_WARP_ERS and HWW_GLOBAL_ERS register value does not match expected value!");
    }

    // Release semaphore on channel to allow other channels to run again
    m_pCh[ch]->SemaphoreRelease(FE_SEMAPHORE_VALUE);
    CHECK_RC(m_pCh[ch]->Flush());

    // poll on event notification for semaphore release
    POLLWRAP(&PollFunc, m_cpuAddr, m_TestConfig.TimeoutMs());

    CHECK_RC_MSG(m_pCh[ch]->CheckError(), "Verify Channel");

    return rc;
}

//! \brief SetupChannels() :
//! \Prepare channels for for method activity
//!
//! \return RC -> OK if successful, specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC ComputeSwMethodTest::SetupChannels()
{
    RC rc;
    UINT32  ch;

    // Setup all the test channels
    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC(ChannelInit(ch));
        CHECK_RC_MSG(ChannelNotify(ch), "Notify failed initializing channel");
    }

    return rc;
}

//! \brief VerifyShaderException() :
//! \Verify grSetShaderExceptions_GF100 Sw method of FERMI_A class.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC ComputeSwMethodTest::VerifyShaderException()
{
    RC rc;
    UINT32  ch;
    GpuSubdevice   *pSubdev;
    SMUtil  smUtil;

    //
    // Read the initial shader exception values from the register.
    // This will be used later to verify that the secondary channel's context does not
    // change when chaning the shader exception on the first channel
    //
    pSubdev = GetBoundGpuSubdevice();

    //
    // - get the initial HW values to check un-changed and ending channel state
    // - program regs to full on, then read back which bits are actually capable of being set in HW,
    //   this will be used to test the SW method to enable all, safe for different HW versions
    // - set the chip back to the original state
    //
    smUtil.ReadWarpGlobalMaskValues(pSubdev, 0, &m_warpInitial, &m_globalInitial, LW_FALSE);
    smUtil.WriteWarpGlobalMaskValues(pSubdev, 0, 0xffffffff, 0xffffffff, LW_FALSE);
    smUtil.ReadWarpGlobalMaskValues(pSubdev, 0, &m_warpMaxEnabled, &m_globalMaxEnabled, LW_FALSE);
    smUtil.WriteWarpGlobalMaskValues(pSubdev, 0, m_warpInitial, m_globalInitial, LW_FALSE);

    Printf(Tee::PriHigh,"%s: m_warpInitial=0x%08x m_globalInitial=0x%08x\n", __FUNCTION__,
        m_warpInitial, m_globalInitial);
    Printf(Tee::PriHigh,"%s: m_warpMaxEnabled=0x%08x m_globalMaxEnabled=0x%08x\n", __FUNCTION__,
        m_warpMaxEnabled, m_globalMaxEnabled);

    // Disable shader Exceptions For Channel 0
    CHECK_RC_MSG(EnableShaderExceptions(0, false),
                    "Failed to disable Shader Exceptions");

    // Verify Shader exceptions are disabled on Channel 0
    CHECK_RC_MSG(VerifyChannelContext(0, SHADER_EXCEPTION, 0, 0, 0),
                    "Shader Exceptions not disabled on Channel 0");

    // Now check to make sure all other channels have original Shader Exception setting
    for (ch = 1; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(VerifyChannelContext(ch, SHADER_EXCEPTION, 0 , m_warpInitial, m_globalInitial),
                            "Channel Shader Exception setting is not same as initial!");
    }

    // Enable Shader Exceptions for Channel 0
    CHECK_RC_MSG(EnableShaderExceptions(0, true),
                    "Failed to enable Shader Exception for Channel 0");

    // Verify Shader exceptions are enabled on Channel 0
    CHECK_RC_MSG(VerifyChannelContext(0, SHADER_EXCEPTION, 0, m_warpMaxEnabled, m_globalMaxEnabled),
                    "Shader Exceptions not enabled on channel 0");

    // Now check to make sure all other channels have original Shader Exception setting
    for (ch = 1; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(VerifyChannelContext(ch, SHADER_EXCEPTION, 0 , m_warpInitial, m_globalInitial),
                            "Channel Shader Exception setting is not same as initial!");
    }

    return rc;
}

//! \brief GetActivateComputeContextPerformanceSettingsMethod(UINT32 ch, UINT32 data)
//!
//! \return RC -> OK if successful, specific RC on failure
//------------------------------------------------------------------------------
RC ComputeSwMethodTest::GetActivateComputeContextPerformanceSettingsMethod(UINT32 *pMethod)
{
    RC rc;

    if (IsClassSupported(KEPLER_COMPUTE_B))
    {
        *pMethod = LWA1C0_ACTIVATE_PERF_SETTINGS_FOR_COMPUTE_CONTEXT;
        return rc;
    }

    rc  =  RC::SOFTWARE_ERROR;

    KASSERT( (0), "Failed to determine class-specific ACTIVATE_PERF_SETTINGS method offset for unknown compute class.\n", rc );

    return rc;
}

//! \brief ActivateComputeContextPerformanceSettings(UINT32 ch, UINT32 data)
//!
//! \return RC -> OK if successful, specific RC on failure
//------------------------------------------------------------------------------
RC ComputeSwMethodTest::ActivateComputeContextPerformanceSettings(UINT32 ch, UINT32 data)
{
    RC rc;
    UINT32  method;

    CHECK_RC(GetActivateComputeContextPerformanceSettingsMethod(&method));

    if (m_bNeedWfi)
    {
        CHECK_RC(m_pCh[ch]->Write(COMPUTE_SUBCHANNEL, LWC06F_WFI, DRF_DEF(C06F, _WFI, _SCOPE, _ALL)));
    }

    CHECK_RC(m_pCh[ch]->Write(COMPUTE_SUBCHANNEL, method, data));

    CHECK_RC_MSG(ChannelNotify(ch),
                    "Notify failed setting ACTIVATE_PERF_SETTINGS");

    CHECK_RC_MSG(m_pCh[ch]->CheckError(),
                    "Channel error setting ACTIVATE_PERF_SETTINGS");

    CHECK_RC(m_pCh[ch]->Flush());

    return rc;
}

//! \brief VerifyActivateComputeContextPerformanceSettings() :
//! \Verify compute conetxt performance settings on supporting
//!  classes using sw methods.
//!
//! \return RC -> OK if successful, specific RC on failure
//------------------------------------------------------------------------------
RC ComputeSwMethodTest::VerifyActivateComputeContextPerformanceSettings()
{
    RC rc;
    UINT32  ch;

    // This method is a WAR and only supported on KEPLER_COMPUTE_B
    if (!IsClassSupported(KEPLER_COMPUTE_B))
    {
         return rc;
    }

    //
    // Send ACTIVATE_PERF_SETTINGS=0 (no-op) method to all channels.
    // This should cause an assert if the sw method handling is not
    // working on supporting classes.
    //
    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(ActivateComputeContextPerformanceSettings(ch, 0),
                        "Failed ActivateComputeContextPerformanceSettings call");
    }

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ ComputeSwMethodTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(ComputeSwMethodTest, RmTest,
                 "Compute SW method RM test");
