/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_eccasyncscrubstresstest.cpp
//! \brief Stress ECC asynchronous scrubbing functionality
//!
//! Verifies that the interleaving of FB allocations with ECC asynchronous scrubbing
//! doesn't cause unexpected behaviour from ECC asynchronous scrubbing. Allocates
//! 2D surfaces of random size at random locations on FB. Also allocates as much
//! of FB as possible via AllocateEntireFramebuffer() to further stress ECC
//! asynchronous scrubbing.

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl90b5.h"       // GF100_DMA_COPY
#include "class/cl90b5sw.h"
#include "class/cla0b5.h"       // KEPLER_DMA_COPY_A
#include "class/cla06fsubch.h"  // LWA06F_SUBCHANNEL_COPY_ENGINE
#include "class/cl0000.h"       // LW01_NULL_OBJECT
#include "core/utility/errloggr.h"
#include "gpu/utility/surf2d.h"
#include <vector>
#include "gpu/tests/rmtest.h"
#include "core/include/memcheck.h"
#include "rmt_eccasyncscrubstresstest.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/include/gpudev.h"
#include "gpu/utility/bglogger.h"
#include "gpu/utility/modscnsl.h"
#include "gpu/utility/gpuutils.h"

class EccAsyncScrubStressTest  : public RmTest
{
public:
    EccAsyncScrubStressTest ();
    virtual ~EccAsyncScrubStressTest ();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    // engine/class information
    GpuSubdevice* masterGpu;
    LwRm::Handle m_hSubDevDiag;

    // test information
    Random* pRandom;
    LwU64 fbSize;

    RC AllocCeCompliantMem(LwU64 size, Memory::Location loc,
                                Surface2D** allocationPointer);
    RC FlushTo(LwU32 targetLoc);
};

//------------------------------------------------------------------------------
EccAsyncScrubStressTest ::EccAsyncScrubStressTest ()
{
    masterGpu = 0;
    pRandom = 0;
    fbSize = 0;
    m_hSubDevDiag = 0;

    SetName("EccAsyncScrubStressTest");
}

//------------------------------------------------------------------------------
EccAsyncScrubStressTest ::~EccAsyncScrubStressTest ()
{
}

//! \brief IsTestSupported() checks if the the master GPU has the hardware needed
//!        by this test
//!
//! EccAsyncScrubStressTest  needs: ECC enabled
//!
//! \return returns RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         else returns "NOT SUPPORTED"
//------------------------------------------------------------------------------
string EccAsyncScrubStressTest ::IsTestSupported()
{
    RC rc;
    LwRm *pLwRm = GetBoundRmClient();
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    LW2080_CTRL_GPU_QUERY_ECC_STATUS_PARAMS EccStatus;
    LW208F_CTRL_CMD_FB_ECC_SCRUB_DIAG_PARAMS eccScrubDiagParams = {0};

    // step 1: check if ECC is enabled
    memset(&EccStatus, 0, sizeof(EccStatus));
    if (OK != pLwRm->ControlBySubdevice(pGpuSubdevice,
                                        LW2080_CTRL_CMD_GPU_QUERY_ECC_STATUS,
                                        &EccStatus, sizeof(EccStatus)))
    {
        return "ECC not Enabled";
    }

    // step 2: check if asynchronous scrubbing is enabled
    rc = pLwRm->Control(GetBoundGpuSubdevice()->GetSubdeviceDiag(),
                                   LW208F_CTRL_CMD_FB_ECC_SCRUB_DIAG,
                                   (void*)&eccScrubDiagParams,
                                   sizeof(eccScrubDiagParams));
    if (LW_FALSE != eccScrubDiagParams.bAsyncScrubDisabled || OK != rc)
        return "Asynchronous scrubbing is disabled";

    return RUN_RMTEST_TRUE;
}

//! \brief Makes all the necessary allocations for EccAsyncScrubStressTest
//!
//! For this test, Setup() only needs to collect important information about the
//! test.
//!
//! \return RC structure to denote the error status of Setup()
//------------------------------------------------------------------------------
RC EccAsyncScrubStressTest ::Setup()
{
    RC rc;
    LwRmPtr pLwRm;

    // step 1: get the basic parameters of this test
    CHECK_RC(InitFromJs());
    masterGpu = GetBoundGpuSubdevice();
    pRandom = &GetFpContext()->Rand;
    m_hSubDevDiag = GetBoundGpuSubdevice()->GetSubdeviceDiag();

    //
    // step 2: get the size of FB (excluding sysmem dedicated to GPU) so we can
    // figure out the range our random numbers must fall in. Note that the size
    // of FB returned in fbInfo.data is in KB hence we multiply by 1024 to colwert
    // to bytes.
    //
    LW2080_CTRL_FB_INFO fbInfo = {0};
    LW2080_CTRL_FB_GET_INFO_PARAMS fbParams = {1, LW_PTR_TO_LwP64(&fbInfo)};
    fbInfo.index = LW2080_CTRL_FB_INFO_INDEX_RAM_SIZE;
    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(masterGpu),
                                    LW2080_CTRL_CMD_FB_GET_INFO,
                                    &fbParams, sizeof(fbParams)));
    fbSize = fbInfo.data << 10;

Cleanup:
    return rc;
}

//! \brief Runs the EccAsyncScrubStressTest
//!
//! Run() launches the scrubber and makes FB allocations entirely before, entirely
//! after, or intersecting where the scrubber has completed up to. To be thoroughly,
//! Run() also makes random allocations on FB.
//!
//! \return RC structure to denote the error status of Run()
//------------------------------------------------------------------------------
RC EccAsyncScrubStressTest ::Run()
{
    RC rc;
    LwRmPtr pLwRm;
    LW208F_CTRL_CMD_FB_ECC_SCRUB_DIAG_PARAMS eccScrubDiagParams = {0};
    LW208F_CTRL_CMD_FB_ECC_ASYNC_SCRUB_REGION_PARAMS eccAsyncScrubRegionParams = {0};
    LwU64 blockSize = 0;
    Surface2D *pTestSurface = 0;
    GpuUtility::MemoryChunks chunks;

    //
    // define the region that it is safe to unleash the scrubber on without clobbering
    // critical data (i.e. RM stack)
    //
    eccAsyncScrubRegionParams.startBlock = START_OF_SAFE_REGION;
    eccAsyncScrubRegionParams.endBlock   = END_OF_SAFE_REGION;

    for (LwU32 i = 0; i < NUM_TESTS; i++)
    {
        // step 1: re-launch the scrubber if scrubbing is already completed
        CHECK_RC_CLEANUP(pLwRm->Control(m_hSubDevDiag,
                                        LW208F_CTRL_CMD_FB_ECC_SCRUB_DIAG,
                                        &eccScrubDiagParams,
                                        sizeof(eccScrubDiagParams)));

        if (eccScrubDiagParams.fbOffsetCompleted == eccScrubDiagParams.fbEndOffset)
        {
            CHECK_RC_CLEANUP(pLwRm->Control(m_hSubDevDiag,
                                            LW208F_CTRL_CMD_FB_ECC_ASYNC_SCRUB_REGION,
                                            (void*)&eccAsyncScrubRegionParams,
                                            sizeof(eccAsyncScrubRegionParams)));
            CHECK_RC_CLEANUP(pLwRm->Control(m_hSubDevDiag,
                                            LW208F_CTRL_CMD_FB_ECC_SCRUB_DIAG,
                                            (void*)&eccScrubDiagParams,
                                            sizeof(eccScrubDiagParams)));
        }

        // step 2: choose a random memory size to allocate
        blockSize = pRandom->GetRandom(0, MAX_BLOCK_SIZE) ;

        // step 3: make the allocation
        CHECK_RC_CLEANUP(AllocCeCompliantMem(blockSize, Memory::Fb, &pTestSurface));

        FlushTo(FB);

        // step 4: deallocate the memory
        if (pTestSurface)
        {
            pTestSurface->Free();
            delete pTestSurface;
            pTestSurface = 0;

        }

        FlushTo(FB);

        //
        // HACK: Asynchronous scrub interrupt needs a moment to trigger, if we remove
        // this, all the allocations would happen together, not interleaving with
        // asynchronous scrubbing.
        //
        Tasker::Sleep(0);
    }

    return rc;

Cleanup:
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
RC EccAsyncScrubStressTest ::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief allocation a block of memory that a CE can understand.
//!
//! No CE's are used in EccAsyncScrubStressTest (). However, it is acceptable to allocate
//! memory that is compliant with CE's. AllocCeCompliantMem() is leveraged
//! from EccFbFullScanTest and was hacked to allocate physically contiguous memory.
//! See rmt_eccfbfullscantest.cpp.
//!
//! \return RC structure denoting the exit status of AllocCeCompliantMem()
//------------------------------------------------------------------------------
RC EccAsyncScrubStressTest ::AllocCeCompliantMem(LwU64 size, Memory::Location loc,
                                Surface2D** allocationPointer)
{
    RC rc;

    Surface2D* temp = new Surface2D;
    temp->SetForceSizeAlloc(true);
    temp->SetArrayPitch(1);
    temp->SetArraySize(UNSIGNED_CAST(UINT32, size));
    temp->SetColorFormat(ColorUtils::Y8); //each "pixel" is 8 bits
    temp->SetAddressModel(Memory::Paging);
    temp->SetLayout(Surface2D::Pitch);
    temp->SetLocation(loc);
    temp->SetPhysContig(false);

    // set the page size based on the array size
    if (size <= 0x1000)
    {
        // allocate 4K pages
        temp->SetPageSize(4);
    }
    else if (size <= 0x20000)
    {
        // allocate 64/128K pages
        temp->SetPageSize(128);
    }
    else
    {
        // allocate 2MB pages
        temp->SetPageSize(2048);
    }

    CHECK_RC_CLEANUP(temp->Alloc(GetBoundGpuDevice()));

    temp->Fill(0);
    Platform::FlushCpuWriteCombineBuffer();

    *allocationPointer = temp;
    return rc;

Cleanup:
    delete temp;
    *allocationPointer = 0;
    return rc;
}

//! \brief performs the necessary operations to flush to a given target
//!
//! NOTE: targetLoc should only be SM, L1_CACHE, L2_CACHE, or FB
//!
//! \return RC struct denoting the exit status of FlushTo()
//------------------------------------------------------------------------------
RC EccAsyncScrubStressTest ::FlushTo(LwU32 targetLoc)
{
    RC rc;
    LW0080_CTRL_DMA_FLUSH_PARAMS Params = {0};
    CHECK_RC(Platform::FlushCpuWriteCombineBuffer());

    switch (targetLoc)
    {
        case L2_CACHE:
            Params.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT, _L2, _ENABLE);
            CHECK_RC(GetBoundRmClient()->ControlByDevice(GetBoundGpuDevice(),
                                                        LW0080_CTRL_CMD_DMA_FLUSH,
                                                        &Params, sizeof(Params)));
            return rc;

        case FB:
            Params.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                        _L2, _ENABLE)
                              | DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                        _FB, _ENABLE);
            CHECK_RC(GetBoundRmClient()->ControlByDevice(GetBoundGpuDevice(),
                                                       LW0080_CTRL_CMD_DMA_FLUSH,
                                                       &Params, sizeof(Params)));
            CHECK_RC(masterGpu->IlwalidateL2Cache(0));
            return rc;

        default:
            return RC::BAD_PARAMETER;
    }

    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(EccAsyncScrubStressTest , RmTest, "Test that stresses ECC asynchronous scrubbing.");

