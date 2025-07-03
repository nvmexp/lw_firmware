/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2015,2019-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Class9197 SW method test for Alpha and Beta CB, supported on gf108+.
//
// Allocate GR object, and send the following SW methods:
// - LW9197_SET_CIRLWLAR_BUFFER_SIZE
// - LW9197_SET_ALPHA_CIRLWLAR_BUFFER_SIZE
//

#include "gpu/include/gpudev.h"
#include "class/cl2080.h"

#include "fermi/gf108/dev_graphics_nobundle.h"
#include "rmt_alphabetacb.h"

bool isSemaReleaseSuccess = false;

//! \brief RmtPollFunc: Static function
//!
//! This function is a static one used for the poll and timeout.
//! Polling on the semaphore release condition, timeout condition
//! in case the sema isn't released.
//!
//! \return TRUE if the sema released else false.
//!
//! \sa Run
//-----------------------------------------------------------------------------
bool RmtPollFunc(void *pArg)
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

//!
//! \brief AlphaBetaCBTest (Fermi SW Method test) constructor
//!
//! AlphaBetaCBTest (Fermi SW Method test) constructor does not do much.  Functionality
//! mostly lies in Setup().
//!
//! \sa Setup
//------------------------------------------------------------------------------
AlphaBetaCBTest::AlphaBetaCBTest()
{
    UINT32 ch;

    SetName("AlphaBetaCBTest");

    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        m_3dAlloc[ch].SetOldestSupportedClass(FERMI_B);
    }
}

//!
//! \brief AlphaBetaCBTest (Fermi SW Method test) destructor
//!
//! AlphaBetaCBTest (Fermi SW Method test) destructor does not do much.  Functionality
//! mostly lies in Cleanup().
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
AlphaBetaCBTest::~AlphaBetaCBTest()
{
}

//!
//! \brief Is AlphaBetaCBTest (Fermi SW Method test) supported?
//!
//! Verify if AlphaBetaCBTest (Fermi SW Method test) is supported in the current
//! environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------
string AlphaBetaCBTest::IsTestSupported()
{
    m_bNeedWfi = LW_FALSE;

    if(IsClassSupported(PASCAL_CHANNEL_GPFIFO_A))
    {
        m_bNeedWfi = LW_TRUE;
    }

    if( m_3dAlloc[0].IsSupported(GetBoundGpuDevice()))
        return RUN_RMTEST_TRUE;
    return "None of FERMI_B+ classes is supported on current platform";
}

//!
//! \brief AlphaBetaCBTest (Fermi SW method test) Setup
//!
//! \return RC OK if all's well.
//------------------------------------------------------------------------------
RC AlphaBetaCBTest::Setup()
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
        nullptr, nullptr, nullptr,
        GetBoundGpuDevice()));

    // Create a mappable VM object
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
                              0,
                              GetBoundGpuSubdevice()));

    // Allocate resources for all test channels
    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        m_pCh[ch] = NULL;
        CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[ch],
                                              &m_hCh[ch],
                                              LW2080_ENGINE_TYPE_GRAPHICS));

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
RC AlphaBetaCBTest::Run()
{
    RC rc;

    CHECK_RC(VerifyCB());
    CHECK_RC(VerifyAlphaCB());

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
RC AlphaBetaCBTest::Cleanup()
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

//! \brief Check current Cirlwlar buffer register value
//!
//! \sa CheckCBValue
//------------------------------------------------------------------------
RC AlphaBetaCBTest::CheckCBValue(UINT32 ch, UINT32 expected)
{
    RC  rc;
    UINT32  actual;

    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    // Avoid race condition of reading register before method completion
    CHECK_RC(m_pCh[ch]->WaitIdle(m_TimeoutMs));

    // Read the current register value
    pSubdev->CtxRegRd32(m_hCh[ch], LW_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC, &actual);

    Printf(Tee::PriHigh,"%s: actual=0x%08x expected=0x%08x\n", __FUNCTION__,
        DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC, _BETA_CBSIZE, actual), (expected*4));

    // Register setting should be equal to (expected value * 4)
    KASSERT( ( DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC, _BETA_CBSIZE, actual) == (expected*4) ),
             "AlphaBetaCBTest::CheckCBValue: CB register value incorrect\n", rc );

    return rc;
}

//! \brief Check current Alpha Cirlwlar buffer register value
//!
//! \sa CheckAlphaCBValue
//------------------------------------------------------------------------
RC AlphaBetaCBTest::CheckAlphaCBValue(UINT32 ch, UINT32 expected)
{
    RC  rc;
    UINT32  actual;

    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    // Avoid race condition of reading register before method completion
    CHECK_RC(m_pCh[ch]->WaitIdle(m_TimeoutMs));

    // Read the current register value
    pSubdev->CtxRegRd32(m_hCh[ch], LW_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC, &actual);

    Printf(Tee::PriHigh,"%s: actual=0x%08x expected=0x%08x\n", __FUNCTION__,
        DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC, _ALPHA_CBSIZE, actual), (expected*4));

    // Register setting should be equal to (expected value * 4)
    KASSERT( ( DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC, _ALPHA_CBSIZE, actual) == (expected*4) ),
             "AlphaBetaCBTest::CheckAlphaCBValue: Alpha CB register value incorrect\n", rc );

    return rc;
}

//! \brief FermiInit(UINT32 ch) : Initialize the Fermi 3D class
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC AlphaBetaCBTest::FermiInit(UINT32 ch)
{
    RC rc;

    CHECK_RC(m_pCh[ch]->SetObject(CLASS9197_SUCH_FERMI, m_3dAlloc[ch].GetHandle()));

    CHECK_RC(m_Notifier[ch].Instantiate(0));

    m_pCh[ch]->SetSemaphoreOffset(m_gpuAddr);
    m_pCh[ch]->SemaphoreRelease(FE_SEMAPHORE_VALUE);
    CHECK_RC(m_pCh[ch]->Flush());

    // poll on event notification for semaphore release
    POLLWRAP(&RmtPollFunc, m_cpuAddr, m_TestConfig.TimeoutMs());

    return rc;
}

//! \brief FermiNotify(UINT32 ch) : Request notify and wait for completion
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC AlphaBetaCBTest::FermiNotify(UINT32 ch)
{
    RC rc;

    m_Notifier[ch].Clear(LW9097_NOTIFIERS_NOTIFY);
    CHECK_RC(m_pCh[ch]->Write(0,LW9197_NOTIFY,
                          LW9197_NOTIFY_TYPE_WRITE_ONLY));
    CHECK_RC(m_pCh[ch]->Write(0, LW9197_NO_OPERATION, 0));

    CHECK_RC(m_pCh[ch]->Flush());
    CHECK_RC(m_Notifier[ch].Wait(LW9097_NOTIFIERS_NOTIFY, m_TimeoutMs));

    return rc;
}

//! \brief FermiSetCBSize(UINT32 ch, UINT32 cbSize) : Set the CB Size
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC AlphaBetaCBTest::FermiSetCBSize(UINT32 ch, UINT32 cbSize, UINT32 gpuFamily)
{
    RC rc;

    if(gpuFamily >= GpuDevice::Pascal)
    {
        if(m_bNeedWfi)
        {
            CHECK_RC(m_pCh[ch]->Write(CLASS9197_SUCH_FERMI, LWC06F_WFI, DRF_DEF(C06F, _WFI, _SCOPE, _ALL)));
        }

        CHECK_RC(m_pCh[ch]->Write(CLASS9197_SUCH_FERMI, LWC097_SET_CIRLWLAR_BUFFER_SIZE,
                    DRF_NUM(C097, _SET_CIRLWLAR_BUFFER_SIZE, _CACHE_LINES_PER_SM, cbSize)));
    }
    else
    {
        CHECK_RC(m_pCh[ch]->Write(CLASS9197_SUCH_FERMI, LW9197_SET_CIRLWLAR_BUFFER_SIZE,
                    DRF_NUM(9197, _SET_CIRLWLAR_BUFFER_SIZE, _CACHE_LINES_PER_SM, cbSize)));
    }

    CHECK_RC_MSG(FermiNotify(ch),
                    "Notify failed setting LW9197_SET_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM");

    CHECK_RC_MSG(m_pCh[ch]->CheckError(),
                    "Channel error setting LW9197_SET_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM");

    CHECK_RC(m_pCh[ch]->Flush());

    return rc;
}

//! \brief FermiSetAlphaCBSize(UINT32 ch, UINT32 cbSize) : Set the Alpha CB Size
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC AlphaBetaCBTest::FermiSetAlphaCBSize(UINT32 ch, UINT32 cbSize, UINT32 gpuFamily)
{
    RC rc;

    if(gpuFamily >= GpuDevice::Pascal)
    {
        if(m_bNeedWfi)
        {
            CHECK_RC(m_pCh[ch]->Write(CLASS9197_SUCH_FERMI, LWC06F_WFI, DRF_DEF(C06F, _WFI, _SCOPE, _ALL)));
        }

        CHECK_RC(m_pCh[ch]->Write(CLASS9197_SUCH_FERMI, LWC097_SET_ALPHA_CIRLWLAR_BUFFER_SIZE,
                    DRF_NUM(C097, _SET_ALPHA_CIRLWLAR_BUFFER_SIZE, _CACHE_LINES_PER_SM, cbSize)));
    }
    else
    {
        CHECK_RC(m_pCh[ch]->Write(CLASS9197_SUCH_FERMI, LW9197_SET_ALPHA_CIRLWLAR_BUFFER_SIZE,
                    DRF_NUM(9197, _SET_ALPHA_CIRLWLAR_BUFFER_SIZE, _CACHE_LINES_PER_SM, cbSize)));
    }

    CHECK_RC_MSG(FermiNotify(ch),
                    "Notify failed setting LW9197_SET_ALPHA_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM");

    CHECK_RC_MSG(m_pCh[ch]->CheckError(),
                    "Channel error setting LW9197_SET_ALPHA_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM");

    CHECK_RC(m_pCh[ch]->Flush());

    return rc;
}

//! \brief VerifyChannelContext(UINT32 ch, bool typeFlag, UINT32 cbSize, UINT32 warpValue, UINT32 globalValue)
//! \ Verify GRCtx info depending on typeFlag
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC AlphaBetaCBTest::VerifyChannelContext(UINT32 ch, bool typeFlag, UINT32 cbSize, UINT32 warpValue, UINT32 globalValue, UINT32 gpuFamily)
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
            CHECK_RC(m_pCh[chIndex]->Write(0, LW9197_NO_OPERATION, 0));
            CHECK_RC(m_pCh[chIndex]->Flush());
        }
    }

    if (typeFlag == CIRLWLAR_BUFFER)
    {
        // Verify that the CB size for specified channel's GR context matches expected value.
        if (gpuFamily >= GpuDevice::Pascal)
        {
            CHECK_RC_MSG(CheckPascalCBValue(ch, cbSize),
                        "Channel's CB Size does not match expected value!");
        }
        else
        {
            CHECK_RC_MSG(CheckCBValue(ch, cbSize),
                        "Channel's CB Size does not match expected value!");
        }
    }
    else if (typeFlag == ALPHA_CIRLWLAR_BUFFER)
    {
        // Verify that the Alpha CB size for specified channel's GR context matches expected value.
        if (gpuFamily >= GpuDevice::Pascal)
        {
            CHECK_RC_MSG(CheckPascalAlphaCBValue(ch, cbSize),
                        "Channel's Alpha CB Size does not match expected value!");
        }
        else
        {
            CHECK_RC_MSG(CheckAlphaCBValue(ch, cbSize),
                        "Channel's Alpha CB Size does not match expected value!");
        }
    }

    // Release semaphore on channel to allow other channels to run again
    m_pCh[ch]->SemaphoreRelease(FE_SEMAPHORE_VALUE);
    CHECK_RC(m_pCh[ch]->Flush());

    // poll on event notification for semaphore release
    POLLWRAP(&RmtPollFunc, m_cpuAddr, m_TestConfig.TimeoutMs());

    return rc;
}

void AlphaBetaCBTest::FermiGetBetaInitValue(GpuSubdevice *pSubdev)
{
    m_cbInitial = DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC, _BETA_CBSIZE,
                    pSubdev->RegRd32( LW_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC )) / 4;
    Printf(Tee::PriHigh,"%s: m_cbInitial=0x%08x\n", __FUNCTION__, m_cbInitial);

}

void AlphaBetaCBTest::FermiGetAlphaInitValue(GpuSubdevice *pSubdev)
{
    m_alphaCbInitial = DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC, _ALPHA_CBSIZE,
                       pSubdev->RegRd32( LW_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC )) / 4;
    Printf(Tee::PriHigh,"%s: m_alphaCbInitial=0x%08x\n", __FUNCTION__, m_alphaCbInitial);
}

//! \brief VerifyCB) : Verify grSetCirlwlarBufferSize_GF100 Sw method of FERMI_B class.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC AlphaBetaCBTest::VerifyCB()
{
    RC rc;
    UINT32  ch, gpuFamily;
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
    gpuFamily = (GetBoundGpuDevice())->GetFamily();

    if(gpuFamily >= GpuDevice::Pascal)
    {
        PascalGetBetaInitValue(pSubdev);
    }
    else
    {
        FermiGetBetaInitValue(pSubdev);
    }

    // Test setting _SET_CIRLWLAR_BUFFER_SIZE DX9 setting (128)
    CHECK_RC_MSG(FermiSetCBSize(0, 128, gpuFamily),
                "Failed setting LW9197_SET_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM = 128");
    CHECK_RC_MSG(VerifyChannelContext(0, CIRLWLAR_BUFFER, 128, 0, 0, gpuFamily),
                "Channel 0 cbSize does != 128!");

    // Now check to make sure all other channels have original CB Size setting
    for (ch = 1; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(VerifyChannelContext(ch, CIRLWLAR_BUFFER, m_cbInitial , 0, 0, gpuFamily),
                        "Channel cbSize does not match initial cbSize!");
    }

    // Test setting _SET_CIRLWLAR_BUFFER_SIZE DX9 setting (142)
    CHECK_RC_MSG(FermiSetCBSize(0, 142, gpuFamily),
                    "Failed setting LW9197_SET_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM = 128");
    CHECK_RC_MSG(VerifyChannelContext(0, CIRLWLAR_BUFFER, 142, 0, 0, gpuFamily),
                    "Channel 0 cbSize does != 142!");

    // Now check to make sure all other channels have original CB Size setting
    for (ch = 1; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(VerifyChannelContext(ch, CIRLWLAR_BUFFER, m_cbInitial, 0, 0, gpuFamily),
                        "Channel cbSize does not match initial cbSize!");
    }

    // Test setting _SET_CIRLWLAR_BUFFER_SIZE DX9 setting (initial value)
    CHECK_RC_MSG(FermiSetCBSize(0, m_cbInitial, gpuFamily),
                    "Failed setting LW9197_SET_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM = Initial Value");
    CHECK_RC_MSG(VerifyChannelContext(0, CIRLWLAR_BUFFER, m_cbInitial, 0, 0, gpuFamily),
                    "Channel 0 cbSize does != Initial Value!");

    // Now check to make sure all other channels have original CB Size setting
    for (ch = 1; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(VerifyChannelContext(ch, CIRLWLAR_BUFFER, m_cbInitial, 0, 0, gpuFamily),
                        "Channel cbSize does not match initial cbSize!");
    }

    return rc;
}

//! \brief VerifyAlphaCB() : Verify grSetAlphaCirlwlarBufferSize Sw method of FERMI_B class.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC AlphaBetaCBTest::VerifyAlphaCB()
{
    RC rc;
    UINT32  ch, gpuFamily;
    GpuSubdevice   *pSubdev;

    // Setup all the test channels
    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC(FermiInit(ch));
        CHECK_RC_MSG(FermiNotify(ch), "Notify failed initializing channel");
    }

    //
    // Read the initial Alpha CB value from the register.
    // This will be used later to verify that the secondary channel's context does not
    // change when chaning the Alpha CB size on the first channel
    //
    pSubdev = GetBoundGpuSubdevice();
    gpuFamily = (GetBoundGpuDevice())->GetFamily();

    if(gpuFamily >= GpuDevice::Pascal)
    {
        PascalGetAlphaInitValue(pSubdev);
    }
    else
    {
        FermiGetAlphaInitValue(pSubdev);
    }

    if (m_alphaCbInitial == 0)
    {
        Printf(Tee::PriHigh,"%s: Initial value is 0, assumign no support on this chip so simply return OK\n",
            __FUNCTION__);
        return rc;
    }

    // Test setting _SET_ALPHA_CIRLWLAR_BUFFER_SIZE DX9 setting (128)
    CHECK_RC_MSG(FermiSetAlphaCBSize(0, 128, gpuFamily),
                    "Failed setting LW9197_SET_ALPHA_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM = 128");
    CHECK_RC_MSG(VerifyChannelContext(0, ALPHA_CIRLWLAR_BUFFER, 128, 0, 0, gpuFamily),
                    "Channel 0 alphaCbSize does != 128!");

    // Now check to make sure all other channels have original CB Size setting
    for (ch = 1; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(VerifyChannelContext(ch, ALPHA_CIRLWLAR_BUFFER, m_alphaCbInitial , 0, 0, gpuFamily),
                        "Channel alphaCbSize does not match initial alphaCbSize!");
    }

    // Test setting _SET_ALPHA_CIRLWLAR_BUFFER_SIZE DX9 setting (142)
    CHECK_RC_MSG(FermiSetAlphaCBSize(0, 142, gpuFamily),
                    "Failed setting LW9197_SET_ALPHA_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM = 128");
    CHECK_RC_MSG(VerifyChannelContext(0, ALPHA_CIRLWLAR_BUFFER, 142, 0, 0, gpuFamily),
                    "Channel 0 alphaCbSize does != 142!");

    // Now check to make sure all other channels have original CB Size setting
    for (ch = 1; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(VerifyChannelContext(ch, ALPHA_CIRLWLAR_BUFFER, m_alphaCbInitial, 0, 0, gpuFamily),
                        "Channel alphaCbSize does not match initial alphaCbSize!");
    }

    // Test setting _SET_ALPHA_CIRLWLAR_BUFFER_SIZE DX9 setting (initial value)
    CHECK_RC_MSG(FermiSetAlphaCBSize(0, m_alphaCbInitial, gpuFamily),
                    "Failed setting LW9197_SET_ALPHA_CIRLWLAR_BUFFER_SIZE_CACHE_LINES_PER_SM = Initial Value");
    CHECK_RC_MSG(VerifyChannelContext(0, ALPHA_CIRLWLAR_BUFFER, m_alphaCbInitial, 0, 0, gpuFamily),
                    "Channel 0 alphaCbSize does != Initial Value!");

    // Now check to make sure all other channels have original CB Size setting
    for (ch = 1; ch < NUM_TEST_CHANNELS; ch++)
    {
        CHECK_RC_MSG(VerifyChannelContext(ch, ALPHA_CIRLWLAR_BUFFER, m_alphaCbInitial, 0, 0, gpuFamily),
                        "Channel alphaCbSize does not match initial alphaCbSize!");
    }

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ AlphaBetaCBTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(AlphaBetaCBTest, RmTest,
                 "AlphaBetaCB RM test - Test Fermi SW methods");
