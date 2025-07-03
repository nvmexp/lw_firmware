/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// PreemptCtrlsTest test.
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

#include "pascal/gp100/dev_graphics_nobundle.h"
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc097.h" // PASCAL_A
#include "class/clc097.h" // PASCAL_B
#include "class/clc0c0.h" // PASCAL_COMPUTE_A
#include "class/cla06fsubch.h"
#include "class/cla0b5.h"
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"

#define RM_PAGE_SIZE    (1024 * 4)

#define KASSERT( condition, errorMessage, rcVariable )                                   \
if ( ! ( condition ) )                                                                   \
{                                                                                        \
    Printf( Tee::PriHigh, errorMessage );                                                \
    rcVariable = RC::LWRM_ERROR;                                                         \
}

#define NUM_TEST_CHANNELS           2
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

class PreemptCtrlsTest: public RmTest
{
public:
    PreemptCtrlsTest();
    virtual ~PreemptCtrlsTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    UINT64           m_gpuAddr                     = 0;
    UINT32 *         m_cpuAddr                     = nullptr;

    Channel *       m_pCh[NUM_TEST_CHANNELS]       = {};

    LwRm::Handle    m_hCh[NUM_TEST_CHANNELS]       = {};
    LwRm::Handle    m_hVA                          = 0;
    LwRm::Handle    m_hSemMem                      = 0;

    Notifier        m_Notifier[NUM_TEST_CHANNELS];
    ThreeDAlloc     m_3dAlloc[NUM_TEST_CHANNELS];
    FLOAT64         m_TimeoutMs                    = Tasker::NO_TIMEOUT;

    LwBool          m_bNeedWfi                     = LW_FALSE;

    Surface2D m_semaSurf;
    bool m_bVirtCtx                                = false;

    RC AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj);
    void FreeObjects(Channel *pCh);

    RC WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwU32 subch, LwU32 data, bool bAwaken = false);
    RC WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwU32 subch, LwU64 offset);

    map<LwRm::Handle, ThreeDAlloc> m_3dAllocs;
    map<LwRm::Handle, ComputeAlloc> m_computeAllocs;

    RC Basic3dTest();
    RC Basic3dGroupTest();
};

//!
//! \brief PreemptCtrlsTest  constructor
//!
//! PreemptCtrlsTest constructor does not do much.  Functionality
//! mostly lies in Setup().
//!
//! \sa Setup
//------------------------------------------------------------------------------
PreemptCtrlsTest::PreemptCtrlsTest()
{
    UINT32 ch;

    SetName("PreemptCtrlsTest");

    // only supported on Pascal+ chips
    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        m_3dAlloc[ch].SetOldestSupportedClass(PASCAL_A);
    }
}

//!
//! \brief PreemptCtrlsTest destructor
//!
//! PreemptCtrlsTest destructor does not do much.  Functionality
//! mostly lies in Cleanup().
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
PreemptCtrlsTest::~PreemptCtrlsTest()
{
}

//!
//! \brief Is PreemptCtrlsTest supported?
//!
//! Verify if PreemptCtrlsTest is supported in the current
//! environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------
string PreemptCtrlsTest::IsTestSupported()
{
    LwRmPtr    pLwRm;

    m_bNeedWfi = LW_FALSE;

    if(IsClassSupported(PASCAL_CHANNEL_GPFIFO_A))
    {
        m_bNeedWfi = LW_TRUE;
    }

    if( !m_3dAlloc[0].IsSupported(GetBoundGpuDevice()) )
        return "Supported only on Pascal+";

    return RUN_RMTEST_TRUE;
}

//!
//! \brief PreemptCtrlsTest Setup and sending CILP delay timeout ctrl call to
//!        each channel that is setup
//!
//! \return RC OK if all's well.
//------------------------------------------------------------------------------
RC PreemptCtrlsTest::Setup()
{
    RC              rc;
    LwRmPtr         pLwRm;
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
        nullptr, nullptr, nullptr,
        GetBoundGpuDevice()));

    // Create an mappable vm object
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

    return rc;
}

//! \brief Run() : The body of this test.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC PreemptCtrlsTest::Run()
{
    RC rc;

    m_bVirtCtx = false;
    CHECK_RC(Basic3dTest());
    CHECK_RC(Basic3dGroupTest());

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
RC PreemptCtrlsTest::Cleanup()
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

/**
 * @brief Tests multiple graphics channels sending the CILP delay timeout ctrl call down for each.
 *
 * @return
 */
RC PreemptCtrlsTest::Basic3dTest()
{
    RC              rc;
    LwRmPtr         pLwRm;
    UINT32          ch;
    UINT64          Offset;
    LW2080_CTRL_GR_SET_DELAY_CILP_PREEMPT_PARAMS grCtrlParams = {0};
    LW2080_CTRL_GR_GET_CTXSW_STATS_PARAMS grGetCtxswStatsCtrlParams = {0};
    LW2080_CTRL_GR_SET_GFXP_TIMEOUT_PARAMS grSetGfxpTimeoutParams = {0};
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    // Allocate a page of Frame buffer memory for FE semaphore
    CHECK_RC(pLwRm->VidHeapAllocSize(LWOS32_TYPE_IMAGE,
        DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
        DRF_DEF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS) |
        DRF_DEF(OS32, _ATTR, _COHERENCY, _CACHED),
        LWOS32_ATTR2_NONE, RM_PAGE_SIZE, &m_hSemMem, &Offset,
        nullptr, nullptr, nullptr,
        GetBoundGpuDevice()));

    // Create an mappable vm object
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

        CHECK_RC(m_3dAlloc[ch].Alloc(m_hCh[ch], GetBoundGpuDevice()));

        CHECK_RC(m_Notifier[ch].Allocate(m_pCh[ch], 1, &m_TestConfig));

        // setup and make control call set the delay_cilp_preempt
        grCtrlParams.hChannel = m_hCh[ch];
        grCtrlParams.timeout = 0x800;

        CHECK_RC(pLwRm->Control(hSubdevice,
                                LW2080_CTRL_CMD_GR_SET_DELAY_CILP_PREEMPT,
                                (void*)&grCtrlParams,
                                sizeof(grCtrlParams)));

        // setup and make control call set the gfxp timeout
        grSetGfxpTimeoutParams.hChannel = m_hCh[ch];
        grSetGfxpTimeoutParams.timeout = 0x200;

        CHECK_RC(pLwRm->Control(hSubdevice,
                                LW2080_CTRL_CMD_GR_SET_GFXP_TIMEOUT,
                                (void*)&grSetGfxpTimeoutParams,
                                sizeof(grSetGfxpTimeoutParams)));
    }

    // Allocate resources for all test channels
    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC(m_pCh[ch]->SetObject(LWA06F_SUBCHANNEL_3D, m_3dAlloc[ch].GetHandle()));

        CHECK_RC(m_Notifier[ch].Instantiate(0));

        m_pCh[ch]->SetSemaphoreOffset(m_gpuAddr);
        m_pCh[ch]->SemaphoreRelease(FE_SEMAPHORE_VALUE);
        CHECK_RC(m_pCh[ch]->Flush());

        // poll on event notification for semaphore release
        POLLWRAP(&PollFunc, m_cpuAddr, m_TestConfig.TimeoutMs());
    }

    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        grGetCtxswStatsCtrlParams.hChannel = m_hCh[ch];
        CHECK_RC(pLwRm->Control(hSubdevice,
                                LW2080_CTRL_CMD_GR_GET_CTXSW_STATS,
                                (void*)&grGetCtxswStatsCtrlParams,
                                sizeof(grGetCtxswStatsCtrlParams)));
        Printf(Tee::PriHigh, "Basic3dTest : saves      0x%x\n", grGetCtxswStatsCtrlParams.saveCnt);
        Printf(Tee::PriHigh, "Basic3dTest : restores   0x%x\n", grGetCtxswStatsCtrlParams.restoreCnt);
        Printf(Tee::PriHigh, "Basic3dTest : WFI saves  0x%x\n", grGetCtxswStatsCtrlParams.wfiSaveCnt);
        Printf(Tee::PriHigh, "Basic3dTest : CTA saves  0x%x\n", grGetCtxswStatsCtrlParams.ctaSaveCnt);
        Printf(Tee::PriHigh, "Basic3dTest : CILP saves 0x%x\n", grGetCtxswStatsCtrlParams.cilpSaveCnt);
        Printf(Tee::PriHigh, "Basic3dTest : GfxP saves 0x%x\n", grGetCtxswStatsCtrlParams.gfxpSaveCnt);
    }

    return rc;
}

/**
 * @brief Tests multiple graphics channels in a TSG are properly sharing a context and
 * test sending the CILP delay timeout ctrl call down for the TSG.
 *
 * @return
 */
RC PreemptCtrlsTest::Basic3dGroupTest()
{
    RC rc;
    ChannelGroup chGrp(&m_TestConfig);
    ChannelGroup::SplitMethodStream stream(&chGrp);
    const LwU32 grpSz = 2;
    LW2080_CTRL_GR_SET_DELAY_CILP_PREEMPT_PARAMS grCtrlParams = {0};
    LW2080_CTRL_GR_GET_CTXSW_STATS_PARAMS grGetCtxswStatsCtrlParams = {0};
    LW2080_CTRL_GR_SET_GFXP_TIMEOUT_PARAMS grSetGfxpTimeoutParams = {0};
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

    CHECK_RC_CLEANUP(chGrp.Schedule());

    CHECK_RC_CLEANUP(WriteBackendOffset(&stream, LWA06F_SUBCHANNEL_3D, m_semaSurf.GetCtxDmaOffsetGpu()));
    CHECK_RC_CLEANUP(WriteBackendRelease(&stream, LWA06F_SUBCHANNEL_3D, 0));

    // setup and make control call set the delay_cilp_preempt
    grCtrlParams.hChannel = chGrp.GetHandle();
    grCtrlParams.timeout = 0x800;

    CHECK_RC(pLwRm->Control(hSubdevice,
                            LW2080_CTRL_CMD_GR_SET_DELAY_CILP_PREEMPT,
                            (void*)&grCtrlParams,
                            sizeof(grCtrlParams)));

    // setup and make control call set the gfxp timeout
    grSetGfxpTimeoutParams.hChannel = chGrp.GetHandle();
    grSetGfxpTimeoutParams.timeout = 0x200;

    CHECK_RC(pLwRm->Control(hSubdevice,
                            LW2080_CTRL_CMD_GR_SET_GFXP_TIMEOUT,
                            (void*)&grSetGfxpTimeoutParams,
                            sizeof(grSetGfxpTimeoutParams)));

    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    if (MEM_RD32(m_semaSurf.GetAddress()) != 0)
        rc = RC::DATA_MISMATCH;

    CHECK_RC_CLEANUP(chGrp.Schedule(false));
    CHECK_RC_CLEANUP(chGrp.Preempt(true));

    grGetCtxswStatsCtrlParams.hChannel = chGrp.GetHandle();
    grGetCtxswStatsCtrlParams.flags = FLD_SET_DRF(2080, _CTRL_GR_GET_CTXSW_STATS_FLAGS, _RESET, _TRUE, 0);
    CHECK_RC(pLwRm->Control(hSubdevice,
                            LW2080_CTRL_CMD_GR_GET_CTXSW_STATS,
                            (void*)&grGetCtxswStatsCtrlParams,
                            sizeof(grGetCtxswStatsCtrlParams)));
    Printf(Tee::PriHigh, "Basic3dGroupTest : saves      0x%x\n", grGetCtxswStatsCtrlParams.saveCnt);
    Printf(Tee::PriHigh, "Basic3dGroupTest : restores   0x%x\n", grGetCtxswStatsCtrlParams.restoreCnt);
    Printf(Tee::PriHigh, "Basic3dGroupTest : WFI saves  0x%x\n", grGetCtxswStatsCtrlParams.wfiSaveCnt);
    Printf(Tee::PriHigh, "Basic3dGroupTest : CTA saves  0x%x\n", grGetCtxswStatsCtrlParams.ctaSaveCnt);
    Printf(Tee::PriHigh, "Basic3dGroupTest : CILP saves 0x%x\n", grGetCtxswStatsCtrlParams.cilpSaveCnt);
    Printf(Tee::PriHigh, "Basic3dGroupTest : GfxP saves 0x%x\n", grGetCtxswStatsCtrlParams.gfxpSaveCnt);

Cleanup:
    m_semaSurf.Free();

    for (LwU32 i = 0; i < grpSz; i++)
        FreeObjects(chGrp.GetChannel(i));
    chGrp.Free();

    return rc;
}

void PreemptCtrlsTest::FreeObjects(Channel *pCh)
{
    LwRm::Handle hCh = pCh->GetHandle();

    m_3dAllocs[hCh].Free();
    m_computeAllocs[hCh].Free();
}

RC PreemptCtrlsTest::AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj)
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

RC  PreemptCtrlsTest::WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwU32 subch, LwU64 offset)
{
    RC rc;
    switch(subch)
    {
        case LWA06F_SUBCHANNEL_3D:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_3D, LWC097_SET_REPORT_SEMAPHORE_A, LwU64_HI32(offset)));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_3D, LWC097_SET_REPORT_SEMAPHORE_B, LwU64_LO32(offset)));
            break;
        case LWA06F_SUBCHANNEL_COMPUTE:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COMPUTE, LWC0C0_SET_REPORT_SEMAPHORE_A, LwU64_HI32(offset)));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COMPUTE, LWC0C0_SET_REPORT_SEMAPHORE_B, LwU64_LO32(offset)));
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }

    return rc;

}

RC  PreemptCtrlsTest::WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwU32 subch, LwU32 data, bool bAwaken)
{
    RC rc;

    switch(subch)
    {
        case LWA06F_SUBCHANNEL_3D:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_3D, LWC097_SET_REPORT_SEMAPHORE_C, data));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_3D, LWC097_SET_REPORT_SEMAPHORE_D,
                        DRF_DEF(C097, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
                        (bAwaken ? DRF_DEF(C097, _SET_REPORT_SEMAPHORE_D, _AWAKEN_ENABLE, _TRUE) : 0)));
            break;
        case LWA06F_SUBCHANNEL_COMPUTE:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COMPUTE, LWC0C0_SET_REPORT_SEMAPHORE_C, data));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COMPUTE, LWC0C0_SET_REPORT_SEMAPHORE_D,
                        DRF_DEF(C0C0, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
                        (bAwaken ? DRF_DEF(C0C0, _SET_REPORT_SEMAPHORE_D, _AWAKEN_ENABLE, _TRUE) : 0)));
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ PreemptCtrlsTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(PreemptCtrlsTest, RmTest,
                 "PreemptCtrls RM test - Test various preemption controls");
