/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2015,2019-2020 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// SMPCCtrl test.
//
// Allocate FERMI_A+ object, and send the following SW methods:
// - LW9097_SET_CIRLWLAR_BUFFER_SIZE
//

#include <string>          // Only use <> for built-in C++ stuff
#include <vector>
#include <algorithm>
#include <map>

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
#include "gpu/tests/rm/utility/changrp.h"

#include "ctrl/ctrla06f.h"
#include "ctrl/ctrla06c.h"
#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX

#include "fermi/gf100/dev_graphics_nobundle.h"
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl9097.h" // FERMI_A
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/cl9097sw.h"
#include "class/cla06fsubch.h"
#include "class/cla197.h"
#include "class/cla1c0.h"
#include "class/cla0b5.h"
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"

#include "gpu/tests/rm/utility/smutil.h"

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

    if(data == FE_SEMAPHORE_VALUE)
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

#define CLASS9097_SUCH_FERMI        LWA06F_SUBCHANNEL_3D
#define NUM_TEST_CHANNELS           2

#define SHADER_EXCEPTION            0
#define CIRLWLAR_BUFFER             1

class SMPCCtrlTest: public RmTest
{
public:
    SMPCCtrlTest();
    virtual ~SMPCCtrlTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    UINT64           m_gpuAddr                    = 0;
    UINT32 *         m_cpuAddr                    = nullptr;

    Channel *       m_pCh[NUM_TEST_CHANNELS]      = {};

    LwRm::Handle    m_hCh[NUM_TEST_CHANNELS]      = {};
    LwRm::Handle    m_hVA                         = 0;
    LwRm::Handle    m_hSemMem                     = 0;

    Notifier        m_Notifier[NUM_TEST_CHANNELS];
    ThreeDAlloc     m_3dAlloc[NUM_TEST_CHANNELS];
    FLOAT64         m_TimeoutMs                   = Tasker::NO_TIMEOUT;

    UINT32          m_cbInitial                   = 0;
    UINT32          m_warpInitial                 = 0;
    UINT32          m_globalInitial               = 0;
    UINT32          m_warpMaxEnabled              = 0;
    UINT32          m_globalMaxEnabled            = 0;

    LwBool          m_bNeedWfi                    = 0;

    Surface2D m_semaSurf;
    bool m_bVirtCtx                               = 0;

    RC AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj);
    void FreeObjects(Channel *pCh);

    RC WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwU32 subch, LwU32 data, bool bAwaken = false);
    RC WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwU32 subch, LwU64 offset);

    map<LwRm::Handle, ThreeDAlloc> m_3dAllocs;
    map<LwRm::Handle, ComputeAlloc> m_computeAllocs;

    RC FermiVerifyCB();
    RC FermiInit(UINT32 ch);
    RC FermiNotify(UINT32 ch);
    RC FermiVerifyShaderException();
    RC CheckCBValue(UINT32 ch, UINT32 expected);
    RC CheckSEValue(UINT32 ch, UINT32 warpExpected, UINT32 globalExpected);
    RC FermiSetCBSize(UINT32 ch, UINT32 cbSize);
    RC FermiEnableSE(UINT32 ch, bool enable);
    RC FermiVerifyChannelContext(UINT32 ch, bool typeFlag, UINT32 cbSize, UINT32 warpValue, UINT32 globalValue);
    RC Basic3dGroupTest();
};

//!
//! \brief SMPCCtrlTest (SMPC WAR control test) constructor
//!
//! SMPCCtrlTest (SMPC WAR control test) constructor does not do much.  Functionality
//! mostly lies in Setup().
//!
//! \sa Setup
//------------------------------------------------------------------------------
SMPCCtrlTest::SMPCCtrlTest()
{
    UINT32 ch;

    SetName("SMPCCtrlTest");

    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        m_3dAlloc[ch].SetOldestSupportedClass(FERMI_A);
    }
}

//!
//! \brief SMPCCtrlTest (SMPC WAR control test) destructor
//!
//! SMPCCtrlTest (SMPC WAR control test) destructor does not do much.  Functionality
//! mostly lies in Cleanup().
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
SMPCCtrlTest::~SMPCCtrlTest()
{
}

//!
//! \brief Is SMPCCtrlTest (SMPC WAR control test) supported?
//!
//! Verify if SMPCCtrlTest (SMPC WAR control test) is supported in the current
//! environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------
string SMPCCtrlTest::IsTestSupported()
{
    LwRmPtr    pLwRm;

    m_bNeedWfi = LW_FALSE;

    if(IsClassSupported(PASCAL_CHANNEL_GPFIFO_A))
    {
        m_bNeedWfi = LW_TRUE;
    }

    if( !m_3dAlloc[0].IsSupported(GetBoundGpuDevice()) )
        return "Supported only on Fermi+";
    else if ( !pLwRm->IsClassSupported(KEPLER_CHANNEL_GROUP_A, GetBoundGpuDevice()) )
        return "Channel groups not supported";

    return RUN_RMTEST_TRUE;
}

//!
//! \brief SMPCCtrlTest (SMPC WAR control test) Setup
//!
//! \return RC OK if all's well.
//------------------------------------------------------------------------------
RC SMPCCtrlTest::Setup()
{
    RC              rc;
    LwRmPtr         pLwRm;
    UINT32          ch;
    UINT64          Offset;
    LW2080_CTRL_GR_CTXSW_SMPC_MODE_PARAMS grCtxswSmpcModeParams = {0};
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

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
        nullptr, nullptr, nullptr,
        GetBoundGpuDevice()));

    // Create a mappable vm object
    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &m_hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    //
    // Add the FB memory to the address space.  The "offset" returned
    // is actually the GPU virtual address in this case.
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hVA,
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
                              0, GetBoundGpuSubdevice()));

    // Allocate resources for all test channels
    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        m_pCh[ch] = NULL;
        CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[ch],
                                              &m_hCh[ch],
                                              LW2080_ENGINE_TYPE_GRAPHICS));

        // setup and make control call to trigger SMPC WAR
        grCtxswSmpcModeParams.hChannel = m_hCh[ch];
        grCtxswSmpcModeParams.smpcMode = LW2080_CTRL_CTXSW_SMPC_MODE_CTXSW;

        CHECK_RC(pLwRm->Control(hSubdevice,
                                LW2080_CTRL_CMD_GR_CTXSW_SMPC_MODE,
                                (void*)&grCtxswSmpcModeParams,
                                sizeof(grCtxswSmpcModeParams)));

        CHECK_RC(m_3dAlloc[ch].Alloc(m_hCh[ch], GetBoundGpuDevice()));

        CHECK_RC(m_Notifier[ch].Allocate(m_pCh[ch], 1, &m_TestConfig));
    }

    return rc;
}

//! \brief Run() : The body of this test.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC SMPCCtrlTest::Run()
{
    RC rc;

    CHECK_RC(FermiVerifyCB());
    CHECK_RC(FermiVerifyShaderException());

    m_bVirtCtx = false;
    CHECK_RC(Basic3dGroupTest());

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
RC SMPCCtrlTest::Cleanup()
{
    RC      rc, firstRc;
    UINT32  ch;
    LwRmPtr pLwRm;

    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        // Free resources for all channels
        m_Notifier[ch].Free();
        m_3dAlloc[ch].Free();

        if (m_pCh[ch] != NULL)
            FIRST_RC(m_TestConfig.FreeChannel(m_pCh[ch]));
    }

    pLwRm->UnmapMemory(m_hSemMem, m_cpuAddr, 0, GetBoundGpuSubdevice());
    pLwRm->UnmapMemoryDma(m_hVA,
                          m_hSemMem,
                          DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                          m_gpuAddr,
                          GetBoundGpuDevice());
    pLwRm->Free(m_hVA);
    pLwRm->Free(m_hSemMem);

    return firstRc;
}

//! \brief Check Shader Exceptions relevant register values
//!
//! \sa CheckSEValue
//------------------------------------------------------------------------
RC SMPCCtrlTest::CheckSEValue(UINT32 ch, UINT32 warpExpected, UINT32 globalExpected)
{
    RC  rc;
    UINT32  warpValue;
    UINT32  globalValue;
    SMUtil  smUtil;

    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    Tasker::Sleep(100);

    // Avoid race condition of reading register before method completion
    CHECK_RC(m_pCh[ch]->WaitIdle(1000));

    smUtil.ReadWarpGlobalMaskValues(pSubdev, m_hCh[ch], &warpValue, &globalValue, LW_TRUE);

    Printf(Tee::PriHigh,"%s: warp=0x%08x expected=0x%08x\n", __FUNCTION__, warpValue, warpExpected);
    Printf(Tee::PriHigh,"%s: global=0x%08x expected=0x%08x\n", __FUNCTION__, globalValue, globalExpected);

    // Register values should be equal to expected value
    if (warpValue != warpExpected || globalValue != globalExpected)
    {
        Printf(Tee::PriHigh,"%s: LW9097_SET_SHADER_EXCEPTIONS is not able to Enable all Shader Exceptions\n", __FUNCTION__);
        Printf(Tee::PriHigh,"    warp=0x%08x expected=0x%08x\n", warpValue, warpExpected);
        Printf(Tee::PriHigh,"    global=0x%08x expected=0x%08x\n", globalValue, globalExpected);
        return RC::DATA_MISMATCH;
    }
    KASSERT( (warpValue == warpExpected && globalValue == globalExpected),
              "LW9097_SET_SHADER_EXCEPTIONS is not able to Perfrom expected operation\n", rc );

    return rc;
}

//! \brief Check current Cirlwlar buffer register value
//!
//! \sa CheckCBValue
//------------------------------------------------------------------------
RC SMPCCtrlTest::CheckCBValue(UINT32 ch, UINT32 expected)
{
    RC  rc;
    UINT32  actual;

    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    // Avoid race condition of reading register before method completion
    CHECK_RC(m_pCh[ch]->WaitIdle(1000));

    // Read the current register value
    pSubdev->CtxRegRd32(m_hCh[ch], LW_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC, &actual);

    Printf(Tee::PriHigh,"%s: actual=0x%08x expected=0x%08x\n", __FUNCTION__,
        DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC, _CBSIZE, actual), (expected*4));

    // Register setting should be equal to (expected value * 4)
    KASSERT( ( DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC, _CBSIZE, actual) == (expected*4) ),
             "SMPCCtrlTest::CheckCBValue: CB register value incorrect\n", rc );

    return rc;
}

//! \brief FermiInit(UINT32 ch) : Initialize the Fermi 3D class
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC SMPCCtrlTest::FermiInit(UINT32 ch)
{
    RC rc;

    CHECK_RC(m_pCh[ch]->SetObject(CLASS9097_SUCH_FERMI, m_3dAlloc[ch].GetHandle()));

    CHECK_RC(m_Notifier[ch].Instantiate(0));

    m_pCh[ch]->SetSemaphoreOffset(m_gpuAddr);
    m_pCh[ch]->SemaphoreRelease(FE_SEMAPHORE_VALUE);
    CHECK_RC(m_pCh[ch]->Flush());

    // poll on event notification for semaphore release
    POLLWRAP(&PollFunc, m_cpuAddr, m_TestConfig.TimeoutMs());

    return rc;
}

//! \brief FermiNotify(UINT32 ch) : Request notify and wait for completion
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC SMPCCtrlTest::FermiNotify(UINT32 ch)
{
    RC rc;

    m_Notifier[ch].Clear(LW9097_NOTIFIERS_NOTIFY);
    CHECK_RC(m_pCh[ch]->Write(0,LW9097_NOTIFY,
                          LW9097_NOTIFY_TYPE_WRITE_ONLY));
    CHECK_RC(m_pCh[ch]->Write(0, LW9097_NO_OPERATION, 0));

    CHECK_RC(m_pCh[ch]->Flush());
    CHECK_RC(m_Notifier[ch].Wait(LW9097_NOTIFIERS_NOTIFY, m_TimeoutMs));

    return rc;
}

//! \brief FermiSetCBSize(UINT32 ch, UINT32 cbSize) : Set the CB Size
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC SMPCCtrlTest::FermiSetCBSize(UINT32 ch, UINT32 cbSize)
{
    RC rc;

    if(m_bNeedWfi)
    {
        CHECK_RC(m_pCh[ch]->Write(CLASS9097_SUCH_FERMI, LWC06F_WFI, DRF_DEF(C06F, _WFI, _SCOPE, _ALL)));
    }

    CHECK_RC(m_pCh[ch]->Write(CLASS9097_SUCH_FERMI, LW9097_SET_CIRLWLAR_BUFFER_SIZE,
                 DRF_NUM(9097, _SET_CIRLWLAR_BUFFER_SIZE, _CACHE_LINES_PER_SM, cbSize)));

    CHECK_RC_MSG(FermiNotify(ch),
                    "Notify failed setting LW9097_SET_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM");

    CHECK_RC_MSG(m_pCh[ch]->CheckError(),
                    "Channel error setting LW9097_SET_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM");

    CHECK_RC(m_pCh[ch]->Flush());

    return rc;
}

//! \brief FermiEnableSE(UINT32 ch, bool enable) : Enable Shader Exceptions
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC SMPCCtrlTest::FermiEnableSE(UINT32 ch, bool enable)
{
    RC rc;

    if(m_bNeedWfi)
    {
        CHECK_RC(m_pCh[ch]->Write(CLASS9097_SUCH_FERMI, LWC06F_WFI, DRF_DEF(C06F, _WFI, _SCOPE, _ALL)));
    }

    CHECK_RC(m_pCh[ch]->Write(CLASS9097_SUCH_FERMI, LW9097_SET_SHADER_EXCEPTIONS, enable));

    CHECK_RC_MSG(FermiNotify(ch),
                    "Notify failed setting LW9097_SET_SHADER_EXCEPTIONS");

    CHECK_RC_MSG(m_pCh[ch]->CheckError(),
                    "Channel error setting LW9097_SET_SHADER_EXCEPTIONS");

    CHECK_RC(m_pCh[ch]->Flush());

    return rc;
}

//! \brief FermiVerifyChannelContext(UINT32 ch, bool typeFlag, UINT32 cbSize, UINT32 warpValue, UINT32 globalValue)
//! \ Verify GRCtx info depending on typeFlag
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC SMPCCtrlTest::FermiVerifyChannelContext(UINT32 ch, bool typeFlag, UINT32 cbSize, UINT32 warpValue, UINT32 globalValue)
{
    RC      rc;
    UINT32  chIndex;

    // Acquire semaphore on the specified channel 1 to block other channels from running
    m_pCh[ch]->SemaphoreAcquire(FE_SEMAPHORE_VALUE);
    CHECK_RC(m_pCh[ch]->Flush());

    // Request/wait for notify on specified channel to force channel's GR context to be loaded
    CHECK_RC_MSG(FermiNotify(ch), "Notify failed verifying channel context");

    //
    // Send a NOPs on all other channel's just to prove that they are blocked waiting for the
    // semaphore release.
    // This isn't really needed but added for extra "stess"
    //
    for (chIndex = 0; chIndex < NUM_TEST_CHANNELS; chIndex++)
    {
        if (chIndex != ch)
        {
            CHECK_RC(m_pCh[chIndex]->Write(0, LW9097_NO_OPERATION, 0));
            CHECK_RC(m_pCh[chIndex]->Flush());
        }
    }

    if (typeFlag == CIRLWLAR_BUFFER)
    {
        // Verify that the CB size for specified channel's GR context matches expected value.
        CHECK_RC_MSG(CheckCBValue(ch, cbSize),
                        "Channel's CB Size does not match expected value!");
    }
    else if (typeFlag == SHADER_EXCEPTION)
    {

        // Verify that the shader exception relevent register value for channel's GR context matches expected value.
        CHECK_RC_MSG(CheckSEValue(ch, warpValue, globalValue),
                        "Channel's HWW_WARP_ERS and HWW_GLOBAL_ERS register value does not match expected value!");
    }

    // Release semaphore on channel to allow other channels to run again
    m_pCh[ch]->SemaphoreRelease(FE_SEMAPHORE_VALUE);
    CHECK_RC(m_pCh[ch]->Flush());

    // poll on event notification for semaphore release
    POLLWRAP(&PollFunc, m_cpuAddr, m_TestConfig.TimeoutMs());

    return rc;
}

//! \brief FermiVerifyCB() : Verify grSetCirlwlarBufferSize_GF100 Sw method of FERMI_A class.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC SMPCCtrlTest::FermiVerifyCB()
{
    RC rc;
    UINT32  ch;
    GpuSubdevice   *pSubdev;

    // Setup all the test channels
    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC(FermiInit(ch));
        CHECK_RC_MSG(FermiNotify(ch), "Notify failed initializing channel");
    }

    //
    // Read the initial CB value from the register.
    // This will be used later to verify that the secondary channel's context does not
    // change when chaning the CB size on the first channel
    //
    pSubdev = GetBoundGpuSubdevice();

    m_cbInitial = DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC, _CBSIZE,
                    pSubdev->RegRd32( LW_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC )) / 4;
    Printf(Tee::PriHigh,"%s: m_cbInitial=0x%08x\n", __FUNCTION__, m_cbInitial);

    // Test setting _SET_CIRLWLAR_BUFFER_SIZE DX9 setting (128)
    CHECK_RC_MSG(FermiSetCBSize(0, 128),
                    "Failed setting LW9097_SET_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM = 128");
    CHECK_RC_MSG(FermiVerifyChannelContext(0, CIRLWLAR_BUFFER, 128, 0, 0),
                    "Channel 0 cbSize does != 128!");

    // Now check to make sure all other channels have original CB Size setting
    for (ch = 1; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(FermiVerifyChannelContext(ch, CIRLWLAR_BUFFER, m_cbInitial , 0, 0),
                        "Channel cbSize does not match initial cbSize!");
    }

    // Test setting _SET_CIRLWLAR_BUFFER_SIZE DX9 setting (142)
    CHECK_RC_MSG(FermiSetCBSize(0, 142),
                    "Failed setting LW9097_SET_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM = 128");
    CHECK_RC_MSG(FermiVerifyChannelContext(0, CIRLWLAR_BUFFER, 142, 0, 0),
                    "Channel 0 cbSize does != 142!");

    // Now check to make sure all other channels have original CB Size setting
    for (ch = 1; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(FermiVerifyChannelContext(ch, CIRLWLAR_BUFFER, m_cbInitial, 0, 0),
                        "Channel cbSize does not match initial cbSize!");
    }

    // Test setting _SET_CIRLWLAR_BUFFER_SIZE DX9 setting (initial value)
    CHECK_RC_MSG(FermiSetCBSize(0, m_cbInitial),
                    "Failed setting LW9097_SET_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM = Initial Value");
    CHECK_RC_MSG(FermiVerifyChannelContext(0, CIRLWLAR_BUFFER, m_cbInitial, 0, 0),
                    "Channel 0 cbSize does != Initial Value!");

    // Now check to make sure all other channels have original CB Size setting
    for (ch = 1; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(FermiVerifyChannelContext(ch, CIRLWLAR_BUFFER, m_cbInitial, 0, 0),
                        "Channel cbSize does not match initial cbSize!");
    }

    return rc;
}

//! \brief FermiVerifyShaderException() :
//! \Verify grSetShaderExceptions_GF100 Sw method of FERMI_A class.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC SMPCCtrlTest::FermiVerifyShaderException()
{

    RC rc;
    UINT32  ch;
    GpuSubdevice   *pSubdev;
    SMUtil  smUtil;

    // Setup all the test channels
    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC(FermiInit(ch));
        CHECK_RC_MSG(FermiNotify(ch), "Notify failed initializing channel");
    }

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
    CHECK_RC_MSG(FermiEnableSE(0, false),
                    "Failed to disable Shader Exceptions");

    // Verify Shader exceptions are disabled on Channel 0
    CHECK_RC_MSG(FermiVerifyChannelContext(0, SHADER_EXCEPTION, 0, 0, 0),
                    "Shader Exceptions not disabled on Channel 0");

    // Now check to make sure all other channels have original Shader Exception setting
    for (ch = 1; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(FermiVerifyChannelContext(ch, SHADER_EXCEPTION, 0 , m_warpInitial, m_globalInitial),
                            "Channel Shader Exception setting is not same as initial!");
    }

    // Enable Shader Exceptions for Channel 0
    CHECK_RC_MSG(FermiEnableSE(0, true),
                    "Failed to enable Shader Exception for Channel 0");

    // Verify Shader exceptions are enabled on Channel 0
    CHECK_RC_MSG(FermiVerifyChannelContext(0, SHADER_EXCEPTION, 0, m_warpMaxEnabled, m_globalMaxEnabled),
                    "Shader Exceptions not enabled on channel 0");

    // Now check to make sure all other channels have original Shader Exception setting
    for (ch = 1; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(FermiVerifyChannelContext(ch, SHADER_EXCEPTION, 0 , m_warpInitial, m_globalInitial),
                            "Channel Shader Exception setting is not same as initial!");
    }

    return rc;
}

/**
 * @brief Tests multiple graphics channels in a TSG are properly sharing a context.
 *
 * @return
 */
RC SMPCCtrlTest::Basic3dGroupTest()
{
    RC rc;
    ChannelGroup chGrp(&m_TestConfig);
    ChannelGroup::SplitMethodStream stream(&chGrp);
    const LwU32 grpSz = 2;
    LW2080_CTRL_GR_CTXSW_SMPC_MODE_PARAMS grCtxswSmpcModeParams = {0};
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    m_semaSurf.SetForceSizeAlloc(true);
    m_semaSurf.SetArrayPitch(1);
    m_semaSurf.SetArraySize(0x1000);
    m_semaSurf.SetColorFormat(ColorUtils::VOID32);
    m_semaSurf.SetAddressModel(Memory::Paging);
    m_semaSurf.SetLayout(Surface2D::Pitch);
    m_semaSurf.SetLocation(Memory::Fb);
    CHECK_RC(m_semaSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_semaSurf.Map());

    MEM_WR32(m_semaSurf.GetAddress(), 0xdeadbeef);

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    CHECK_RC(chGrp.Alloc());

    for (LwU32 i = 0; i < grpSz; i++)
    {
        Channel *pCh;
        LwRm::Handle hObj;
        CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
        if(i == 0)
        {
            CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_3D, &hObj));
            CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_3D, hObj));
        }
        else
        {
            CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_COMPUTE, &hObj));
            CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_COMPUTE, hObj));
        }
    }

    // setup and make control call to trigger SMPC WAR
    grCtxswSmpcModeParams.hChannel = chGrp.GetHandle();
    grCtxswSmpcModeParams.smpcMode = LW2080_CTRL_CTXSW_SMPC_MODE_CTXSW;

    CHECK_RC(pLwRm->Control(hSubdevice,
                            LW2080_CTRL_CMD_GR_CTXSW_SMPC_MODE,
                            (void*)&grCtxswSmpcModeParams,
                            sizeof(grCtxswSmpcModeParams)));

    CHECK_RC_CLEANUP(chGrp.Schedule());

    CHECK_RC_CLEANUP(WriteBackendOffset(&stream, LWA06F_SUBCHANNEL_3D, m_semaSurf.GetCtxDmaOffsetGpu()));
    CHECK_RC_CLEANUP(WriteBackendRelease(&stream, LWA06F_SUBCHANNEL_3D, 0));

    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    if (MEM_RD32(m_semaSurf.GetAddress()) != 0)
        rc = RC::DATA_MISMATCH;

Cleanup:
    m_semaSurf.Free();

    for (LwU32 i = 0; i < grpSz; i++)
        FreeObjects(chGrp.GetChannel(i));
    chGrp.Free();

    return rc;
}

void SMPCCtrlTest::FreeObjects(Channel *pCh)
{
    LwRm::Handle hCh = pCh->GetHandle();

    m_3dAllocs[hCh].Free();
    m_computeAllocs[hCh].Free();
}

RC SMPCCtrlTest::AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj)
{
    RC rc;
    LwRm::Handle hCh = pCh->GetHandle();
    MASSERT(hObj);

    switch (subch)
    {
        case LWA06F_SUBCHANNEL_3D:
            CHECK_RC(m_3dAllocs[hCh].Alloc(hCh, GetBoundGpuDevice()));
            *hObj = m_3dAllocs[hCh].GetHandle();
            break;
        case LWA06F_SUBCHANNEL_COMPUTE:
            CHECK_RC(m_computeAllocs[hCh].Alloc(hCh, GetBoundGpuDevice()));
            *hObj = m_computeAllocs[hCh].GetHandle();
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }
    return rc;
}

RC  SMPCCtrlTest::WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwU32 subch, LwU64 offset)
{
    RC rc;
    switch(subch)
    {
        case LWA06F_SUBCHANNEL_3D:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_3D, LWA197_SET_REPORT_SEMAPHORE_A, LwU64_HI32(offset)));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_3D, LWA197_SET_REPORT_SEMAPHORE_B, LwU64_LO32(offset)));
            break;
        case LWA06F_SUBCHANNEL_COMPUTE:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_REPORT_SEMAPHORE_A, LwU64_HI32(offset)));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_REPORT_SEMAPHORE_B, LwU64_LO32(offset)));
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }

    return rc;

}

RC  SMPCCtrlTest::WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwU32 subch, LwU32 data, bool bAwaken)
{
    RC rc;

    switch(subch)
    {
        case LWA06F_SUBCHANNEL_3D:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_3D, LWA197_SET_REPORT_SEMAPHORE_C, data));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_3D, LWA197_SET_REPORT_SEMAPHORE_D,
                        DRF_DEF(A197, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
                        (bAwaken ? DRF_DEF(A197, _SET_REPORT_SEMAPHORE_D, _AWAKEN_ENABLE, _TRUE) : 0)));
            break;
        case LWA06F_SUBCHANNEL_COMPUTE:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_REPORT_SEMAPHORE_C, data));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_REPORT_SEMAPHORE_D,
                        DRF_DEF(A1C0, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
                        (bAwaken ? DRF_DEF(A1C0, _SET_REPORT_SEMAPHORE_D, _AWAKEN_ENABLE, _TRUE) : 0)));
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ SMPCCtrlTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(SMPCCtrlTest, RmTest,
                 "SMPCCtrl RM test - Test SMPC WAR control");
