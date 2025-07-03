/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007,2009-2011,2013-2015,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_comprzbctest.cpp
//! \brief Test for sysmem compression and ZBC on iGT21A.
//!
//! This test tests compression and ZBC for all compressible page kinds by
//! overriding the kind choosing path. The kind choosing path is also covered
//! by not overriding the kind and hitting random combinations of various attributes.
//! For allocating compression, all the fallback options for all combinations of
//! ATTR_COMPR + ATTR2_ZBC + all cmpressible kinds is tested.
//!
//! Additionally, this test verifies that compression flushes are happening
//! when freeing compressed surfaces.  Compression flushes are necessary on
//! iGT21A to ensure that the ZBC RAMS are kept in a consistent state when
//! reusing compression tags.

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl8697.h" // GT21A_TESLA
#include "class/cl8697sw.h"
#include "gpu/include/gpusbdev.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl208f.h" // LW20_SUBDEVICE_DIAG CTRL
#include "lwos.h"
#include "lwRmApi.h"
#include "core/include/xp.h"
#include "gpu/tests/rm/utility/mmuutil.h"
#include "gpu/utility/surf2d.h"
#include "core/include/channel.h"
#include "gpu/include/notifier.h"
#include "core/include/memcheck.h"

#define MAX_LOOPS                           (1000)

// defining 64KB default allocation size
#define DEFAULT_ALLOCATION_SIZE             (1024*64)

// useful when debugging to print extensive amount of output
#define COMPR_TEST_DEBUG                    (0)

// 1 comptag line covers 64 KB memory
#define FB_COMPTAG_LINE_COVERAGE_LW50       (1024*64)

// compression allocation policy defines.
#define ALLOC_POLICY_ALLOWS_COMPR_0         BIT(0)
#define ALLOC_POLICY_ALLOWS_COMPR_1         BIT(1)
#define ALLOC_POLICY_ALLOWS_COMPR_2         BIT(2)
#define ALLOC_POLICY_ALLOWS_COMPR_4         BIT(3)

// Macro to return highest compression allocation option from the policy
#define HIGHEST_ALLOCATION_POLICY(policy)   (\
                                            (policy) & ALLOC_POLICY_ALLOWS_COMPR_4 ? ALLOC_POLICY_ALLOWS_COMPR_4 :\
                                            (policy) & ALLOC_POLICY_ALLOWS_COMPR_2 ? ALLOC_POLICY_ALLOWS_COMPR_2 :\
                                            (policy) & ALLOC_POLICY_ALLOWS_COMPR_1 ? ALLOC_POLICY_ALLOWS_COMPR_1 :\
                                            (policy) & ALLOC_POLICY_ALLOWS_COMPR_0 ? ALLOC_POLICY_ALLOWS_COMPR_0 : 0)

// Macro to return lowest compression allocation option from the policy
#define LOWEST_ALLOCATION_POLICY(policy)    (\
                                            (policy) == 0 ? 0 :\
                                            (policy) & ALLOC_POLICY_ALLOWS_COMPR_0 ? ALLOC_POLICY_ALLOWS_COMPR_0 :\
                                            (policy) & ALLOC_POLICY_ALLOWS_COMPR_1 ? ALLOC_POLICY_ALLOWS_COMPR_1 :\
                                            (policy) & ALLOC_POLICY_ALLOWS_COMPR_2 ? ALLOC_POLICY_ALLOWS_COMPR_2 :\
                                            (policy) & ALLOC_POLICY_ALLOWS_COMPR_4 ? ALLOC_POLICY_ALLOWS_COMPR_4 : 0)

// Macro to return comptags that will be allocated (uses top-down approach, highest first)
#define NUM_COMPTAGS_FROM_POLICY(policy)    (\
                                            (policy) == ALLOC_POLICY_ALLOWS_COMPR_4 ? 4 :\
                                            (policy) == ALLOC_POLICY_ALLOWS_COMPR_2 ? 2 :\
                                            (policy) == ALLOC_POLICY_ALLOWS_COMPR_1 ? 1 :\
                                            (policy) == ALLOC_POLICY_ALLOWS_COMPR_0 ? 0 : 0xffffffff)

// Macro to return LW0080 defines for comptags to be found in PTE.
#define PTE_COMPTAGS_FROM_POLICY(policy)    (\
                                            (policy) == ALLOC_POLICY_ALLOWS_COMPR_4 ? LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_COMPTAGS_4 :\
                                            (policy) == ALLOC_POLICY_ALLOWS_COMPR_2 ? LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_COMPTAGS_2 :\
                                            (policy) == ALLOC_POLICY_ALLOWS_COMPR_1 ? LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_COMPTAGS_1 : LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_COMPTAGS_NONE)

#define NOT_MEM_SIZE (4*1024)

class ComprZBCTest : public RmTest
{
public:
    ComprZBCTest();
    virtual ~ComprZBCTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwRm::Handle    m_hSubDevDiag;
    Notifier        m_Notifier;
    FLOAT64  m_TimeoutMs;
    RC      TestCompressionFlush();
    RC      TestAllCompressibleKinds();
    RC      TestCompressibleKindsRandomly();
    RC      Rulwalidation(const LwRm::Handle handle, const UINT32 requestedVidAttr, const UINT32 requestedVidAttr2, const UINT32 returnedVidAttr, const UINT32 returnedVidAttr2, const UINT64 size, const bool bKindOverride, const UINT32 kindToBeTested, const bool bAdditionalComptagsCheck, const UINT32 comptagsToCheck, const bool bFallback);
    RC      BuildAndTestAllocationPolicy(const UINT32 vidType, const UINT32 requestedVidAttr, const UINT32 requestedVidAttr2, const INT32 kindToBeTested);
    RC      TestAllocationPolicy(const UINT32 vidType, const UINT32 requestedVidAttr, const UINT32 requestedVidAttr2, const INT32 kindToBeTested, const UINT32 allocationPolicy);
    bool    SanityCheckerHeapParams(const UINT32 vidType, const UINT32 vidAttr);
    RC      ExhaustComptagLines(list<LwU32> *allocHandlesList, const LwU64 leaveFree);
    RC      FreeComptagsUsingHwFree(list<LwU32> *allocHandlesList);
    bool    IsKind(const LwU32 kind, const LwU32 operation);

    // For TestCompressionFlush()
    Surface2D m_cSurf;
    Surface2D m_cSurf2;
    Surface2D m_zSurf;
    LwRm::Handle m_obj3d;

    LwRm::Handle m_hNotCtxDma;
    LwRm::Handle m_hNotMem;
    LwRm::Handle m_hCh;
    Channel*     m_pCh;

    RC doFreeTest();
    RC doReleaseTest();

    RC doFastColorClear(Surface2D*, UINT32, UINT32, UINT32, UINT32);
    RC doFastZClear(Surface2D*, UINT32, UINT32);
    RC verifyZbcRefCount(UINT32, UINT32);
    RC getTagsForSurface(Surface2D*, UINT32*, UINT32*);

};

//
// Generic weights structure from mmu utility (Initializing weights here).
// Only the weights for attributes/flags that are needed for this test have
// been initialized to values needed, others that are not needed in this
// test are initialized to zero.
//
static MmuUtilWeights weights[] = {
{
    // Weights for AllocMemory() classes
    { {0, 0} },

    // Weights for AllocMemory() flags
    {
        {{0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0}},
    },

    //
    // Weights for type param in VidHeapAlloc()
    // Here, the probability of some of the types is
    // zero because they are not needed in this test
    //
    { {0.148, 0.142, 0.142, 0.142, 0, 0, 0, 0, 0.142, 0, 0.142, 0, 0.142, 0} },

    // Weights for flags in ATTR param in VidHeapAlloc()
    {
        {{0.166, 0.166, 0.166, 0, 0.166, 0.166, 0.17, 0, 0, 0, 0}}, // Weights for ATTR_DEPTH, none for DEPTH_24
        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},        // Weights for COMPR_COVG
        {{0.125, 0.125, 0.125, 0.125, 0, 0.125, 0, 0.125, 0.125, 0.125, 0}}, // Weights for ATTR_AA_SAMPLES
        {{0.2, 0.6, 0.2, 0, 0, 0, 0, 0, 0, 0, 0}},  // Weights for ATTR_COMPR
        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},        // Weights for ATTR_FORMAT
        {{0.5, 0.5, 0, 0, 0, 0, 0, 0, 0, 0, 0}},    // Weights for ATTR_Z_TPYE
        {{0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0, 0, 0}}, // Weights for ATTR_ZS_PACKING
        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},        // Weights for ATTR_COLOR_PACKING
        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},        // Weights for ATTR_FORMAT_OVERRIDE
        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},        // Weights for ATTR_PAGE_SIZE
        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},        // Weights for ATTR_LOCATION
        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},        // Weights for ATTR_PHYSICALITY
        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},        // Weights for ATTR_COHERENCY
    },

    // Weights for flags in ATTR2 param in VidHeapAlloc()
    {
        {{0.34, 0.33, 0.33}}, // Weights for ATTR2_ZBC
        {{0, 0, 0}}, // Weights for ATTR_GPU_CACHEABLE - This ATTR2 is lwrrently not in use
        {{0, 0, 0}}, // Weights for ATTR_P2P_GPU_CACHEABLE - This ATTR2 is lwrrently not in use
    },

    // Weights for flags in AllocContextDma()
    {
        {{0, 0, 0}},
        {{0, 0, 0}},
        {{0, 0, 0}},
        {{0, 0, 0}},
        {{0, 0}},
        {{0, 0}},
        {{0, 0, 0}},
    },

    // Weights for flags in MapMemory()
    {
        {{0, 0, 0}},
        {{0, 0, 0}},
        {{0, 0}},
        {{0, 0, 0}}
    },

    // Weights for flags in MapMemoryDma()
    {
        {{0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0, 0, 0, 0}},
    }
}
};

//!
//! @brief ComprZBCTest default constructor.
//!
//! Initializes class variables to default value, that's it.
//!
//! @param[in]  NONE
//! @param[out] NONE
//-----------------------------------------------------------------------------
ComprZBCTest::ComprZBCTest()
{
    SetName("ComprZBCTest");
    m_hSubDevDiag   = 0;
    m_TimeoutMs     = Tasker::NO_TIMEOUT;
    m_obj3d         = 0;
    m_hNotCtxDma    = 0;
    m_hNotMem       = 0;
    m_hCh           = 0;
    m_pCh           = nullptr;
}

//!
//! @brief ComprZBCTest default destructor.
//!
//! Placeholder : doesnt do much, much funcationality in Cleanup().
//!
//! @param[in]  NONE
//! @param[out] NONE
//-----------------------------------------------------------------------------
ComprZBCTest::~ComprZBCTest()
{
}

//!
//! @brief IsTestSupported(), Looks for whether test can execute in current elw.
//!
//! Check for GT21A_TESLA class availibility (lwrrently we are restricting this
//! this test for iGT21A, this can be further leveraged for GPUs that support
//! compression.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    True if the test can be run in the current environment,
//!             false otherwise.
//-----------------------------------------------------------------------------
string ComprZBCTest::IsTestSupported()
{
    bool       bIsSupported;

   // Returns true only if the class is supported.
   bIsSupported = (IsClassSupported(GT21A_TESLA));

   Printf(Tee::PriLow,"%d:ComprZBCTest::IsSupported retured %d \n",
         __LINE__, bIsSupported);

   if(bIsSupported)
       return RUN_RMTEST_TRUE;
   return "GT21A_TESLA class is not supported on current platform";
}

//!
//! @brief Setup(): Generally used for any test level allocation.
//! Call base class init, allocates subdevdiag handle, and disables
//! compression flush if enabled. Flushing comptags takes too long on
//! fmodel and we'll be allocating all of them many times!! So its best to
//! disable compression flush for this test, we won't need the flush anyway.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if all allocations passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC ComprZBCTest::Setup()
{
    RC rc;
    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();
    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    LwRmPtr pLwRm;

    // Get a subdevicediag handle
    m_hSubDevDiag = GetBoundGpuSubdevice()->GetSubdeviceDiag();

    m_TestConfig.AllocateChannel(&m_pCh, &m_hCh);

    // Alloc 3d object
    CHECK_RC(pLwRm->AllocObject(m_hCh, &m_obj3d, GT21A_TESLA));

    // Notifier
    CHECK_RC(pLwRm->VidHeapAllocSize(
        LWOS32_TYPE_IMAGE,
        DRF_DEF(OS32, _ATTR, _LOCATION, _PCI),
        0,
        NOT_MEM_SIZE,
        &m_hNotMem,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        GetBoundGpuDevice()));

    CHECK_RC(pLwRm->AllocContextDma(
            &m_hNotCtxDma,
            DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE),
            m_hNotMem, 0, NOT_MEM_SIZE-1));

    CHECK_RC(m_Notifier.Allocate(m_pCh, 1, &m_TestConfig));

    return rc;
}

//!
//! @brief Run(): Used generally for placing all the testing stuff.
//!
//! Runs the required tests. First, the compression-flush test is run.  After
//! that we have sysmem compression test here with all kinds + kind_override on
//! and kind_override off + random kinds selected through fbChooseKind()
//! function. All fallback options of allocation policy for all combinations of
//! ATTR_COMPR, ATTR2_ZBC and all compressible kinds is also tested.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if all tests passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC ComprZBCTest::Run()
{
    RC rc;
    LwRmPtr pLwRm;

    CHECK_RC(TestCompressionFlush());

    //
    // Test all compressible kinds between 0 to 0xff with all combinations
    // of ATTR_COMPR, ATTR2_ZBC and all compressible kinds. Also run the
    // allocation policy testing on all these combinations.
    //
    CHECK_RC(TestAllCompressibleKinds());

    //
    // Randomly tests different kinds by randomizing on ATTR_DEPTH,
    // AA_SAMPLES, Z_TYPE, ZS_PACKING, ATTR_COMPR and ATTR2_ZBC. This
    // will not overide the kind (let RM pick the kind based on requested
    // attributes), and cover the fbChooseKind() path.
    //
    CHECK_RC(TestCompressibleKindsRandomly());

    return rc;
}

//!
//! @brief Run(): Free any resources that this test selwred.
//!
//! Enables compression flush if disabled by this test and
//! frees the subdevdiag handle.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if the compression enable and deallocs pass, specific RC
//!             if failed.
//-----------------------------------------------------------------------------
RC ComprZBCTest::Cleanup()
{
    RC          rc = OK;
    LwRmPtr     pLwRm;
    m_Notifier.Free();

    // Free 3d object
    pLwRm->Free(m_obj3d);

    pLwRm->Free(m_hNotCtxDma);
    pLwRm->Free(m_hNotMem);
    m_TestConfig.FreeChannel(m_pCh);

    return rc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//!
//! @brief TestCompressionFlush()
//!
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if the test passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC ComprZBCTest::TestCompressionFlush()
{
    RC rc = OK;
    Printf(Tee::PriLow, "Setting up initial 3d state...\n");
    m_pCh->Write(1, LW8697_SET_OBJECT, m_obj3d);
    m_pCh->Write(1, LW8697_SET_SURFACE_CLIP_HORIZONTAL,
            DRF_NUM(8697, _SET_SURFACE_CLIP_HORIZONTAL, _WIDTH, 256));

    m_pCh->Write(1, LW8697_SET_SURFACE_CLIP_VERTICAL,
            DRF_NUM(8697, _SET_SURFACE_CLIP_VERTICAL, _HEIGHT, 256));

    m_pCh->Write(1, LW8697_SET_VIEWPORT_CLIP_HORIZONTAL(0),
            DRF_NUM(8697, _SET_VIEWPORT_CLIP_HORIZONTAL, _WIDTH, 256));
    m_pCh->Write(1, LW8697_SET_VIEWPORT_CLIP_VERTICAL(0),
            DRF_NUM(8697, _SET_VIEWPORT_CLIP_VERTICAL, _HEIGHT, 256));

    m_pCh->Write(1, LW8697_SET_CLIP_ID_TEST,0);
    m_pCh->Write(1, LW8697_SET_WINDOW_OFFSET_X, 0);
    m_pCh->Write(1, LW8697_SET_WINDOW_OFFSET_Y, 0);
    m_pCh->Write(1, LW8697_SET_STENCIL_MASK, 0xff);

    CHECK_RC(doFreeTest());
    CHECK_RC(doReleaseTest());

    return rc;
}

//!
//! @brief TestAllCompressibleKinds()
//!
//! Tests all compressible kinds between 0x00 and 0xff with all combinations of
//! ATTR_COMPR and ATTR2_ZBC. For each kind that is tested, the test first
//! checks with RM if the kind is supported on the current GPU, and if the kind
//! is compressible. If the kind is supported and compressible, the test then
//! allocates a memory chunk of this kind by overriding the kind selection
//! path. It then calls Rulwalidation() function to verify the return
//! attributes and PTE fields for this allocation.
//! While we're generating all combinations of compressible kind, ATTR_COMPR
//! and ATTR_ZBC, we also call BuildAndTestAllocationPolicy() for each
//! combination to test all the fallback options for the policy that gets
//! selected by this combination.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if the test passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC ComprZBCTest::TestAllCompressibleKinds()
{
    //
    // This test needs to make memory allocation calls with
    // LWOS32_ATTR_FORMAT_OVERRIDE = ON which is only defined under
    // LW_VERIF_FEATURES. So only run the test if LW_VERIF_FEATURES is defined.
    // In release builds, LW_VERIF_FEATURES is not defined, so this test fn will
    // not run on release builds
    //
#ifdef  LW_VERIF_FEATURES
    RC           rc;
    LwRmPtr      pLwRm;
    LwRm::Handle handle = 0;
    UINT64       size = 0, vidOffset;
    UINT32       vidType, requestedVidAttr, requestedVidAttr2, returnedVidAttr, returnedVidAttr2, comprAttr, zbcAttr, loopCount;
    INT32        kindToBeTested;

    Printf(Tee::PriLow, "%d: %s: The policy test will test all fallback options of a policy"
           " so allocations will keep failing which is expected, and should be ignored\n",
           __LINE__, __FUNCTION__);

    for (comprAttr = LWOS32_ATTR_COMPR_REQUIRED; comprAttr<= LWOS32_ATTR_COMPR_ANY; comprAttr++)
    for (zbcAttr = LWOS32_ATTR2_ZBC_DEFAULT; zbcAttr <= LWOS32_ATTR2_ZBC_PREFER_ZBC; zbcAttr++)
    {
        // We'll be testing all kinds between 0x0 to 0xff
        for (loopCount = 0; loopCount < 0xff; loopCount++)
        {
            kindToBeTested = loopCount;

            // First check if the kind is supported for this GPU
            if (!IsKind(kindToBeTested, LW2080_CTRL_FB_IS_KIND_OPERATION_SUPPORTED))
            {
                // if kind is not supported, then skip this one
                continue;
            }

            // Now check if the kind is compressible
            if (!IsKind(kindToBeTested, LW2080_CTRL_FB_IS_KIND_OPERATION_COMPRESSIBLE))
            {
                // if kind is not compressible, then skip this one
                continue;
            }

            //
            // keep these attributes constant, as we are anyways overriding the
            // fbChooseKind() path.
            //
            vidType = LWOS32_TYPE_VIDEO;
            requestedVidAttr = (DRF_DEF(OS32, _ATTR, _FORMAT_OVERRIDE, _ON)  |
                                DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR) |
                                DRF_DEF(OS32, _ATTR, _TILED, _NONE)          |
                                DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM)     );

            // Set compression attribute ATTR_COMPR from its lwr value in loop
            requestedVidAttr = FLD_SET_DRF_NUM(OS32, _ATTR, _COMPR, comprAttr, requestedVidAttr);

            // use uncached memory to speed up exelwtion
            requestedVidAttr2 = DRF_DEF(OS32, _ATTR2, _GPU_CACHEABLE,_NO);

            // Set compression attribute ATTR2_ZBC from its lwr value in loop
            requestedVidAttr2 = FLD_SET_DRF_NUM(OS32, _ATTR2, _ZBC, zbcAttr, requestedVidAttr2);

            // use default allocation size defined for this test
            size = DEFAULT_ALLOCATION_SIZE;
            vidOffset = 0;

            //
            // Copy ATTR and ATTR2 to returnedVidAttr and returnedVidAttr2.
            // These are changed by RM and reflect what was actually allocated.
            // We'll need to run validation on what we requested and what was
            // actually allocated so we will need both values.
            //
            returnedVidAttr  = requestedVidAttr;
            returnedVidAttr2 = requestedVidAttr2;

            // Now use the kind in the pFormat parameter for allocation
            CHECK_RC_CLEANUP(pLwRm->VidHeapAllocSizeEx(vidType,                 // Type
                                                       0,                       // Flags
                                                       &returnedVidAttr,        // pAttr
                                                       &returnedVidAttr2,       // pAttr2
                                                       size,                    // Size
                                                       0,                       // Alignment
                                                       &kindToBeTested,         // pFormat
                                                       NULL,                    // pCoverage
                                                       NULL,                    // pPartitionStride
                                                       &handle,                 // pHandle
                                                       &vidOffset,              // pOffset
                                                       NULL,                    // pLimit
                                                       NULL,                    // pRtnFree
                                                       NULL,                    // pRtnTotal
                                                       0,                       // width
                                                       0,                       // height
                                                       GetBoundGpuDevice()));

            // Verify return params and PTE fields for kind and COMPTAGS
            CHECK_RC_CLEANUP(Rulwalidation(handle, requestedVidAttr, requestedVidAttr2, returnedVidAttr, returnedVidAttr2, size, true, kindToBeTested, false, 0, false));

            // Free the memory.
            pLwRm->Free(handle);
            handle = 0;

            //
            // Also run this combination through policy test for testing all
            // fallback paths that the policy will take for this combination
            //
            CHECK_RC(BuildAndTestAllocationPolicy(vidType, requestedVidAttr, requestedVidAttr2, loopCount));
        }
    }
    // Test done, print message and return RC
    Printf(Tee::PriHigh, "%d:ComprZBCTest::%s test passed\n", __LINE__, __FUNCTION__);
    return rc;

Cleanup:
    pLwRm->Free(handle);
    // Use mmu utility's print functions to print the flags in string form
    MmuUtil mmuUtil;
    Printf(Tee::PriHigh,"%d:ComprZBCTest::%s: Error oclwred for kind = 0x%x, error = %s\n",
           __LINE__, __FUNCTION__, kindToBeTested, rc.Message());
    mmuUtil.PrintAllocHeapVidType(Tee::PriHigh, "heap with TYPE is : ", vidType);
    mmuUtil.PrintAllocHeapAttrFlags(Tee::PriHigh, "\nAnd requested ATTR  attributes are :", requestedVidAttr);
    mmuUtil.PrintAllocHeapAttr2Flags(Tee::PriHigh,"\nAnd requested ATTR2 attributes are :", requestedVidAttr2);
    mmuUtil.PrintAllocHeapAttrFlags(Tee::PriHigh, "\nAnd returned  ATTR  attributes are :", returnedVidAttr);
    mmuUtil.PrintAllocHeapAttr2Flags(Tee::PriHigh,"\nAnd returned  ATTR2 attributes are :", returnedVidAttr2);

    return rc;

#else
    //
    // LW_VERIF_FEATURES not defined. Just print that this test
    // has been skipped due to LW_VERIF_FEATURES missing
    //
    Printf(Tee::PriLow, "%d:ComprZBCTest::%s and alloc policy test skipped "
           " because LW_VERIF_FEATURES is missing\n", __LINE__, __FUNCTION__);
    return OK;
#endif
}

//!
//! @brief TestCompressibleKindsRandomly()
//!
//! This test tries heap allocation with randomly generated input parameters.
//! The parameters that are randomized for heap alloc are type, various flags
//! in ATTR param like ATTR_DEPTH/ATTR_AA_SAMPLES/ATTR_Z_TYPE/ATTR_ZS_PACKING/
//! ATTR_COMPR and flags in ATTR2 i.e. ATTR_ZBC. Before the allocation is tried,
//! the parameters are sanity checked to remove any illegal combinations. The
//! allocation is then done and the function Rulwalidation() is called
//! to verify the return attributes and PTE fields for this allocation.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if the test passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC ComprZBCTest::TestCompressibleKindsRandomly()
{
    LwRmPtr      pLwRm;
    LwRm::Handle handle = 0;
    UINT64       size = 0, vidOffset;
    RC           rc;
    UINT32       loopCount, rejectedIterations, vidType, requestedVidAttr, requestedVidAttr2, returnedVidAttr, returnedVidAttr2;
    UINT32       allocHeapAttrFlagsToRandomize, allocHeapAttr2FlagsToRandomize;

    // Initialize seed for mmu utility with a different seed everytime
    UINT32 seed = (UINT32)Xp::QueryPerformanceCounter();
    MmuUtil mmuUtil(seed, 1000);

    Printf(Tee::PriHigh, "%d: %s test using seed value = %u for mmu utility.\n",
           __LINE__, __FUNCTION__, seed);

    rejectedIterations = 0;
    for (loopCount = 0; loopCount < MAX_LOOPS; loopCount++)
    {
        // Randomize vidType using utility
        vidType = mmuUtil.RandomizeAllocHeapTypes(weights);

        // Choose which flags in ATTR parameter should be randomized
        allocHeapAttrFlagsToRandomize = (MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_DEPTH           |
                                         MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_AA_SAMPLES      |
                                         MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_Z_TYPE          |
                                         MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_COMPR           |
                                         MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_ZS_PACKING      );

        // Get randomized ATTR flags from utility
        requestedVidAttr = mmuUtil.RandomizeAllocHeapAttrFlags(weights, allocHeapAttrFlagsToRandomize);

        // keep these attributes constant for ATTR.
        requestedVidAttr |= (DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR) |
                             DRF_DEF(OS32, _ATTR, _TILED, _NONE)          |
                             DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM)     );

        // Choose which flags in ATTR2 to be randomized
        allocHeapAttr2FlagsToRandomize = MMUUTIL_ALLOC_HEAP_ATTR2_FLAGS_TO_RANDOMIZE_ZBC;

        // Get randomized ATTR2 flags from utility
        requestedVidAttr2 = mmuUtil.RandomizeAllocHeapAttr2Flags(weights, allocHeapAttr2FlagsToRandomize);

        // keep this allocation uncached to speed up exelwtion
        requestedVidAttr2 |= DRF_DEF(OS32, _ATTR2, _GPU_CACHEABLE,_NO);

        // Sanity checking the input parameters
        // if false, then skip the iteration
        if(!SanityCheckerHeapParams(vidType, requestedVidAttr))
        {
            rejectedIterations++;
            loopCount--;
            continue;
        }

        // Sanity passed, now try the allocation.
        if (COMPR_TEST_DEBUG)
        {
            mmuUtil.PrintAllocHeapVidType(Tee::PriHigh,"Trying allocation with heap TYPE = ", vidType);
            mmuUtil.PrintAllocHeapAttrFlags(Tee::PriHigh, "\nAnd ATTR =  ", requestedVidAttr);
            mmuUtil.PrintAllocHeapAttr2Flags(Tee::PriHigh, "\nAnd ATTR2 =  ", requestedVidAttr2);
        }
        size = DEFAULT_ALLOCATION_SIZE;

        //
        // Copy ATTR and ATTR2 to returnedVidAttr and returnedVidAttr2.
        // These are changed by RM and reflect what was actually allocated.
        // We'll need to run validation on what we requested and what was
        // actually allocated so we will need both values.
        //
        returnedVidAttr  = requestedVidAttr;
        returnedVidAttr2 = requestedVidAttr2;

        CHECK_RC_CLEANUP(pLwRm->VidHeapAllocSizeEx(vidType,                 // Type
                                                   0,                       // Flags
                                                   &returnedVidAttr,        // pAttr
                                                   &returnedVidAttr2,       // pAttr2
                                                   size,                    // Size
                                                   0,                       // Alignment
                                                   NULL,                    // pFormat
                                                   NULL,                    // pCoverage
                                                   NULL,                    // pPartitionStride
                                                   &handle,                 // pHandle
                                                   &vidOffset,              // pOffset
                                                   NULL,                    // pLimit
                                                   NULL,                    // pRtnFree
                                                   NULL,                    // pRtnTotal
                                                   0,                       // width
                                                   0,                       // height
                                                   GetBoundGpuDevice()));

        // TODORMT arrange all calls to Rulwalidation() in vertical fashion and explain each parameter
        // Verify return params and PTE fields for COMPTAGS
        CHECK_RC_CLEANUP(Rulwalidation(handle, requestedVidAttr, requestedVidAttr2, returnedVidAttr, returnedVidAttr2, size, false, 0, false, 0, false));

        // Free the memory and rerun the loop.
        pLwRm->Free(handle);
        handle = 0;
    }
    // Test done, print message and return RC
    Printf(Tee::PriHigh, "%d:ComprZBCTest::%s test passed, ran %d iterations, rejected %d combinations"
           " due to invalid combinations\n", __LINE__, __FUNCTION__, MAX_LOOPS, rejectedIterations);
    return rc;

Cleanup:
    pLwRm->Free(handle);
    Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Error oclwred on loop  %d ...\n", __LINE__, __FUNCTION__, loopCount);
    mmuUtil.PrintAllocHeapVidType(Tee::PriHigh, "heap with TYPE is : ", vidType);
    mmuUtil.PrintAllocHeapAttrFlags(Tee::PriHigh, "\nAnd requested ATTR  attributes are :", requestedVidAttr);
    mmuUtil.PrintAllocHeapAttr2Flags(Tee::PriHigh,"\nAnd requested ATTR2 attributes are :", requestedVidAttr2);
    mmuUtil.PrintAllocHeapAttrFlags(Tee::PriHigh, "\nAnd returned  ATTR  attributes are :", returnedVidAttr);
    mmuUtil.PrintAllocHeapAttr2Flags(Tee::PriHigh,"\nAnd returned  ATTR2 attributes are :", returnedVidAttr2);
    return rc;
}

//!
//! @brief BuildAndTestAllocationPolicy()
//!
//! This function will test all fallback options of the compression allocation
//! policy that RM selects for a particular combination of ATTR_COMPR, ATTR2_ZBC
//! and the page kind. For e.g., for COMPR_ANY, ZBC_DEFAULT and kind = 0x10
//! RM selects '4->2->0' policy. The test then will test all these fallback
//! compression options that the policy says by eating up comptags and making
//! sure that allocations will fail and fallback to the next compression option
//! This function only builds what should be the policy based on the input
//! params ATTR, ATTR2 and kind. For actually testing the policy,
//! TestAllocationPolicy() function is called.
//!
//! @param[in]  vidType
//!             The vidType parameter to use for policy testing.
//!
//! @param[in]  requestedVidAttr
//!             ATTR parameter to use for policy testing. This paramter has
//!             the compression attribute ATTR_COMPR which is used to build
//!             the policy.
//!
//! @param[in]  requestedVidAttr2
//!             ATTR2 parameter to use for policy testing. This paramter has
//!             the ZBC attribute ATTR2_ZBC which is used to build the policy.
//!
//! @param[in]  kindToBeTested
//!             The kind that is to be used for policy testing. This parameter
//!             also affects what policy is built.
//!
//! @param[out] NONE
//!
//! @returns    OK if the test passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC ComprZBCTest::BuildAndTestAllocationPolicy
(
    const UINT32      vidType,
    const UINT32      requestedVidAttr,
    const UINT32      requestedVidAttr2,
    const INT32       kindToBeTested
)
{
    RC          rc;
    UINT32      allocationPolicy;
    bool        bIsKindCompressible1, bIsKindCompressible2;
    bool        bCanPromoteZbc, bIsKindZbcAllows2, bIsKindZbcAllows4;

    // Extract if kind is 1 bit compressible
    bIsKindCompressible1 = IsKind(kindToBeTested, LW2080_CTRL_FB_IS_KIND_OPERATION_COMPRESSIBLE_1);

    // Extract if kind is 2 bit compressible
    bIsKindCompressible2 = IsKind(kindToBeTested, LW2080_CTRL_FB_IS_KIND_OPERATION_COMPRESSIBLE_2);

    if (!(bIsKindCompressible1 || bIsKindCompressible2))
    {
        Printf(Tee::PriHigh, "%d: %s: Kind 0x%x is not compressible, cannot test"
               " compression fallback policies\n", __LINE__, __FUNCTION__, kindToBeTested);
        CHECK_RC(RC::ILWALID_ARGUMENT);
    }

    // Extract whether kind supports ZBC_2
    bIsKindZbcAllows2 = IsKind(kindToBeTested, LW2080_CTRL_FB_IS_KIND_OPERATION_ZBC_ALLOWS_2);

    // Extract whether kind supports ZBC_4
    bIsKindZbcAllows4 = IsKind(kindToBeTested, LW2080_CTRL_FB_IS_KIND_OPERATION_ZBC_ALLOWS_4);

    bCanPromoteZbc = bIsKindZbcAllows2 || bIsKindZbcAllows4;

    //
    // Build allocation policy based on the kind, COMPR, and ZBC
    //
    allocationPolicy = 0;

    // Assign largest possible compression policy
    if (bIsKindCompressible2)
    {
        allocationPolicy = ALLOC_POLICY_ALLOWS_COMPR_2;
    }
    else
    {
        allocationPolicy = ALLOC_POLICY_ALLOWS_COMPR_1;
    }
    // now promote for ZBC if possible
    if (bCanPromoteZbc)
    {
        // only promote if user wants ZBC_DEFAULT or PREFER_ZBC
        if (FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _DEFAULT, requestedVidAttr2) ||
            FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _PREFER_ZBC, requestedVidAttr2))
        {
            if(FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _PREFER_ZBC, requestedVidAttr2))
            {
                // If _PREFER_ZBC, reset allocation policy to clear the compression policy
                allocationPolicy = 0;
            }

            if (bIsKindZbcAllows2)
            {
                // Promote to 2 bit if ZBC allows 2 bit
                allocationPolicy |= ALLOC_POLICY_ALLOWS_COMPR_2;
            }
            if (bIsKindZbcAllows4)
            {
                // Promote to 2 bit if ZBC allows 2 bit
                allocationPolicy |= ALLOC_POLICY_ALLOWS_COMPR_4;
            }
        }
    }

    // If COMPR_ANY, then add the 0 compbit policy
    if (FLD_TEST_DRF(OS32, _ATTR, _COMPR, _ANY, requestedVidAttr))
    {
        allocationPolicy |= ALLOC_POLICY_ALLOWS_COMPR_0;
    }

    if (COMPR_TEST_DEBUG)
    {
        Printf(Tee::PriHigh, "%d: %s : ATTR_COMPR = %d, ATTR2_ZBC = %d, "
               "kind = 0x%x, policy chosen by test is %s%s%s%s\n",
               __LINE__, __FUNCTION__, DRF_VAL(OS32, _ATTR, _COMPR, requestedVidAttr),
               DRF_VAL(OS32, _ATTR2, _ZBC, requestedVidAttr2), kindToBeTested,
               (allocationPolicy & ALLOC_POLICY_ALLOWS_COMPR_4) ? "4->"  : "",
               (allocationPolicy & ALLOC_POLICY_ALLOWS_COMPR_2) ? "2->"  : "",
               (allocationPolicy & ALLOC_POLICY_ALLOWS_COMPR_1) ? "1->"  : "",
               (allocationPolicy & ALLOC_POLICY_ALLOWS_COMPR_0) ? "0" : "FAIL");
    }

    // Call TestAllocationPolicy() function to actually test this policy
    CHECK_RC(TestAllocationPolicy(vidType, requestedVidAttr, requestedVidAttr2, kindToBeTested, allocationPolicy));

    return rc;
}

//!
//! @brief TestAllocationPolicy()
//!
//! This function works with another function BuildAndTestAllocationPolicy()
//! to test all fallback options of a particular alloc policy.
//! The function BuildAndTestAllocationPolicy() builds up a compression
//! allocation policy and this function tests that policy. Please also read
//! description of BuildAndTestAllocationPolicy() for more info on this test.
//!
//! @param[in]  vidType
//!             The vidType parameter to use for policy testing.
//!
//! @param[in]  requestedVidAttr
//!             ATTR parameter to use for policy testing.
//!
//! @param[in]  requestedVidAttr2
//!             ATTR2 parameter to use for policy testing.
//!
//! @param[in]  kindToBeTested
//!             The kind that is to be used for policy testing.
//!
//! @param[in]  allocationPolicyOriginal
//!             The allocation policy that should be tested for the supplied
//!             combination of ATTR, ATTR2 and kind.
//!
//! @param[out] NONE
//!
//! @returns    OK if the test passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC ComprZBCTest::TestAllocationPolicy
(
    UINT32      vidType,
    UINT32      requestedVidAttr,
    UINT32      requestedVidAttr2,
    INT32       kindToBeTested,
    UINT32      allocationPolicyOriginal
)
{
    UINT32       returnedVidAttr, returnedVidAttr2;
    LwRmPtr      pLwRm;
    UINT64       size = 0, vidOffset = 0;
    RC           rc;
    LwU64        leaveFree;
    UINT32       allocationPolicyTested = 0, allocationPolicyLeftToTest = 0, lwrrentAllocationPolicyToTest = 0, comptagsRequiredInPte = 0;
    LwRm::Handle lastAllocationHandle;
    list <LwU32> allocHandlesList;

    allocationPolicyLeftToTest = allocationPolicyOriginal;
    allocationPolicyTested = 0;

    // (loop until all are tested)
    while(allocationPolicyTested != allocationPolicyOriginal)
    {
        // Pick the highest option still left to test from the allocation policy
        lwrrentAllocationPolicyToTest = HIGHEST_ALLOCATION_POLICY(allocationPolicyLeftToTest);

        //
        // Based on the current allocation policy we're testing, callwlate the
        // comptags that should be found in the PTE
        //
        comptagsRequiredInPte = PTE_COMPTAGS_FROM_POLICY(lwrrentAllocationPolicyToTest);

        //
        // if this is not the first policy we are testing, we need to exhaust comptags
        // i.e. if the policy is 4->2->0 then there's no need to exhaust comptags for
        // testing the '4' option, i.e. the first option. But we'll need to exhaust
        // comptags for testing options '2' and '0' so RM fails to allocate comptags
        // for higher options and falls back to a lower compression option.
        //
        if (allocationPolicyTested != 0)
        {
            //
            // While exhausting comptags, how much comptags to leave free so that
            // the current option is tested..?
            // It is 1 less than the next higher option. i.e. if policy is 4->2->1->0,
            // for for testing the '2' comptags option, the next higher option is 4, so
            // leaveFree is next_higher - 1, i.e. 4 - 1 = 3.
            // Suppose that the policy is 4->1->0, then for testing the '1' comptags
            // option, leaveFree = next_higher - 1, i.e. 4 - 1 = 3.
            //

            leaveFree = NUM_COMPTAGS_FROM_POLICY(LOWEST_ALLOCATION_POLICY(allocationPolicyTested)) - 1;

            //
            // Now call ExhaustComptagLines to allocate comtags only (no FB
            // allocation as we wouldn't want to exhaust FB before comptags).
            // After this, the largest contiguous free comptagline chunk would be 'leaveFree'
            //
            CHECK_RC_CLEANUP(ExhaustComptagLines(&allocHandlesList, leaveFree));
        }

        //
        // Now that free comptaglines have been reduced to 'leaveFree'
        // we will allocate 1 page (FB_COMPTAG_LINE_COVERAGE_LW50) of heap
        // and it should succeed. Also verify that the PTE indicates correct
        // COMPTAGS have been allocated
        //

        size = FB_COMPTAG_LINE_COVERAGE_LW50;   // 1 page for igt21a
        vidOffset = 0;
        returnedVidAttr  = requestedVidAttr;
        returnedVidAttr2 = requestedVidAttr2;
        lastAllocationHandle = 0;
        CHECK_RC_CLEANUP(pLwRm->VidHeapAllocSizeEx(vidType,                 // Type
                                                   0,                       // Flags
                                                   &returnedVidAttr,        // pAttr
                                                   &returnedVidAttr2,       // pAttr2
                                                   size,                    // Size
                                                   0,                       // Alignment
                                                   &kindToBeTested,         // pFormat
                                                   NULL,                    // pCoverage
                                                   NULL,                    // pPartitionStride
                                                   &lastAllocationHandle,   // pHandle
                                                   &vidOffset,              // pOffset
                                                   NULL,                    // pLimit
                                                   NULL,                    // pRtnFree
                                                   NULL,                    // pRtnTotal
                                                   0,                       // width
                                                   0,                       // height
                                                   GetBoundGpuDevice()));

        //
        // If this is not the first option from the policy, then fallback is
        // happening. We'll pass this info to Rulwalidation() function, as
        // some checks present in Rulwalidation() might fail when fallback
        // happens and it needs to know that for not reporting a false error
        //
        bool bFallback = (allocationPolicyTested != 0);

        // Verify return params and PTE fields for COMPTAGS
        CHECK_RC_CLEANUP(Rulwalidation(lastAllocationHandle, requestedVidAttr, requestedVidAttr2, returnedVidAttr, returnedVidAttr2, size, true, kindToBeTested, true, comptagsRequiredInPte, bFallback));

        //
        // Remove the 'lwrrentAllocationPolicyToTest' from what is left
        // to be tested i.e. 'allocationPolicyLeftToTest' and add it in
        // what has already been tested 'allocationPolicyTested'
        //
        allocationPolicyTested |= lwrrentAllocationPolicyToTest;
        allocationPolicyLeftToTest &= ~lwrrentAllocationPolicyToTest;

        // Now free all memory and comptags allocated.
        pLwRm->Free(lastAllocationHandle);
        lastAllocationHandle = 0;

        //
        // No need to free the allocated comptags as we're testing policy
        // options in a top-down manner, We'll have to reallocate these if
        // we freed them here, so we'll only free in the end.
        //
    }

    // If alloc policy does not allow 0 comptags, then the last step to test in this policy is FAIL.
    if (!(allocationPolicyOriginal & ALLOC_POLICY_ALLOWS_COMPR_0))
    {
        leaveFree = NUM_COMPTAGS_FROM_POLICY(LOWEST_ALLOCATION_POLICY(allocationPolicyTested)) - 1;

        //
        // Now call ExhaustComptagLines to allocate comtags only (no FB
        // allocation as we wouldn't want to exhaust FB before comptags).
        // After this, the largest contiguous free comptagline chunk would be 'leaveFree'
        //
        CHECK_RC_CLEANUP(ExhaustComptagLines(&allocHandlesList, leaveFree));

        //
        // Now free comptaglines have been brought down to such a value that
        // it won't be possible for RM to select any of the options from the
        // allocation policy because enough comptag lines won't be present,
        // RM will eventually fail the allocation of even 1 page
        // (FB_COMPTAG_LINE_COVERAGE_LW50) with INSUFFICIENT_RESOURCES error.
        // This will verify the last policy, i.e.  if enough comptags are not
        // available then the allocation will fail
        //

        size = FB_COMPTAG_LINE_COVERAGE_LW50;   // 1 page for igt21a
        vidOffset = 0;
        returnedVidAttr  = requestedVidAttr;
        returnedVidAttr2 = requestedVidAttr2;
        lastAllocationHandle = 0;
        rc = pLwRm->VidHeapAllocSizeEx(vidType,                 // Type
                                       0,                       // Flags
                                       &returnedVidAttr,        // pAttr
                                       &returnedVidAttr2,       // pAttr2
                                       size,                    // Size
                                       0,                       // Alignment
                                       &kindToBeTested,         // pFormat
                                       NULL,                    // pCoverage
                                       NULL,                    // pPartitionStride
                                       &lastAllocationHandle,   // pHandle
                                       &vidOffset,              // pOffset
                                       NULL,                    // pLimit
                                       NULL,                    // pRtnFree
                                       NULL,                    // pRtnTotal
                                       0,                       // width
                                       0,                       // height
                                       GetBoundGpuDevice());
        if (rc == OK)
        {
            //
            // The alloc succeeded, even though enough free comptags
            // were not there. Free the memory and print error
            //
            Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Enough free comptags were not "
                   "present and allocation succeeded when it should have failed\n",
                   __LINE__, __FUNCTION__);

            rc = RC::LWRM_ERROR;

            // free the memory that just got allocated
            pLwRm->Free(lastAllocationHandle);
            lastAllocationHandle = 0;
        }
        else if (rc != RC::LWRM_INSUFFICIENT_RESOURCES)
        {
            Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Enough free comptags were not present "
                   "the allocation should have failed with INSUFFICIENT_RESOURCES error,"
                   "but it failed with error %s\n",
                   __LINE__, __FUNCTION__, rc.Message());
        }
        else
        {
            // RC == LWRM_INSUFFICIENT_RESOURCES, allocation failed as expected, so ok
            rc = OK;
        }
    }

    //
    // All compression options from the policy have been tested.
    // free all memory and comptags, then return
    //
    pLwRm->Free(lastAllocationHandle);
    lastAllocationHandle = 0;
    CHECK_RC_CLEANUP(FreeComptagsUsingHwFree(&allocHandlesList));

    return rc;

Cleanup:
    Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: failed, while testing %d COMPTAGS from policy %s%s%s%s\n",
           __LINE__, __FUNCTION__, comptagsRequiredInPte,
           (allocationPolicyOriginal & ALLOC_POLICY_ALLOWS_COMPR_4) ? "4->"  : "",
           (allocationPolicyOriginal & ALLOC_POLICY_ALLOWS_COMPR_2) ? "2->"  : "",
           (allocationPolicyOriginal & ALLOC_POLICY_ALLOWS_COMPR_1) ? "1->"  : "",
           (allocationPolicyOriginal & ALLOC_POLICY_ALLOWS_COMPR_0) ? "0" : "FAIL");

    // Now free all memory and comptags allocated in this function.
    pLwRm->Free(lastAllocationHandle);
    lastAllocationHandle = 0;
    FreeComptagsUsingHwFree(&allocHandlesList);
    return rc;
}

//!
//! @brief ExhaustComptagLines()
//!
//! This function keeps allocating comptags using HW_ALLOC function until free
//! comptaglines have been reduced to 'leaveFree' supplied by the caller.
//!
//! @param[in]  allocHandlesList
//!             A pointer to the list of handles for the comptags allocated.
//!             As comptags are allocated, the handles for those allocation
//!             are added to this list.
//!
//! @param[in]  leaveFree
//!             No of comptaglines to leave free. After this function is done,
//!             free comptag lines are reduced to this number.
//!
//!
//! @param[out] allocHandlesList
//!             The handles of the allocation are added to this list.
//!
//! @returns    OK if we successfully allocated enough comptags that the free
//!             comptaglines have been reduced to 'leaveFree'. Specific RC if
//!             failed.
//-----------------------------------------------------------------------------
RC ComprZBCTest::ExhaustComptagLines(list <LwU32> *allocHandlesList, const LwU64 leaveFree)
{
    // We're going to use LW_VERIF_FEATURES specific stuff, so make sure we have them
#ifdef LW_VERIF_FEATURES
    LW208F_CTRL_FB_GET_INFO_PARAMS  fbGetInfoParams = {0};
    RC                              rc;
    LwRmPtr                         pLwRm;

    // query for free comptags
    fbGetInfoParams.index = LW208F_CTRL_FB_INFO_INDEX_FREE_CONTIG_COMPRESSION_SIZE;

    INT32 status = LwRmControl(pLwRm->GetClientHandle(), m_hSubDevDiag,
                               LW208F_CTRL_CMD_FB_GET_INFO,
                               &fbGetInfoParams,
                               sizeof(fbGetInfoParams));

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh, "%d: %s: Get free comptag lines call failed\n",
               __LINE__, __FUNCTION__);

        rc = RmApiStatusToModsRC(status);
        CHECK_RC(rc);
    }

    LwU64 freeComptagLines = fbGetInfoParams.data / FB_COMPTAG_LINE_COVERAGE_LW50;

    while (freeComptagLines != leaveFree)
    {
        if (freeComptagLines < leaveFree)
        {
            //
            // This is an error case, either with RM management of free comptags
            // or the caller called this function with incorrect value of
            // leaveFree, i.e. it shouldn't give leaveFree value = 10 when there
            // are only 5 left!!. We're the only caller, so let's assume that
            // RM is at fault (some incorrect state of comptags management)
            //

            Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Free comptag lines = %d has fallen below what we wanted = %d\n",
                   __LINE__, __FUNCTION__, (UINT32)freeComptagLines, (UINT32)leaveFree);
            CHECK_RC_CLEANUP(RC::LWRM_ILWALID_STATE);
        }

        UINT64 toBeFilled = freeComptagLines - leaveFree;

        UINT32 vidType, vidAttr, vidAttr2, comprCovg;
        UINT64 size;
        vidType  =  LWOS32_TYPE_DEPTH;
        vidAttr  = (DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR) |
                    DRF_DEF(OS32, _ATTR, _TILED, _NONE)          |
                    DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED)        |
                    DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z24S8)    |
                    DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1)        |
                    DRF_DEF(OS32, _ATTR, _COMPR, _REQUIRED)      |
                    DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM)     |
                    DRF_DEF(OS32, _ATTR, _COMPR_COVG, _PROVIDED));

        vidAttr2 = LWOS32_ATTR2_GPU_CACHEABLE_NO;
        size = toBeFilled * FB_COMPTAG_LINE_COVERAGE_LW50;

        // Use comprCovg to specify comptag bits for this allocation.
        comprCovg = (DRF_DEF(OS32, _ALLOC, _COMPR_COVG_BITS,_1)  |
                     DRF_NUM(OS32, _ALLOC, _COMPR_COVG_MIN, 1)   |
                     DRF_NUM(OS32, _ALLOC, _COMPR_COVG_MAX, 1000));

        // Now allocate the required no. of comptags
        LWOS32_PARAMETERS hwAlloc;
        memset(&hwAlloc, 0, sizeof(hwAlloc));
        hwAlloc.function = LWOS32_FUNCTION_HW_ALLOC;
        hwAlloc.hRoot = pLwRm->GetClientHandle();
        hwAlloc.hObjectParent = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
        hwAlloc.data.HwAlloc.allocType       = vidType;
        hwAlloc.data.HwAlloc.allocAttr       = vidAttr;
        hwAlloc.data.HwAlloc.allocAttr2      = vidAttr2;
        hwAlloc.data.HwAlloc.allocInputFlags = LWOS32_ALLOC_FLAGS_IGNORE_BANK_PLACEMENT |
                                               LWOS32_ALLOC_FLAGS_FORCE_MEM_GROWS_DOWN;
        hwAlloc.data.HwAlloc.allocSize       = (LwU32)size;
        hwAlloc.data.HwAlloc.allocHeight     = 0;
        hwAlloc.data.HwAlloc.allocWidth      = 0;
        hwAlloc.data.HwAlloc.allocPitch      = 0;
        hwAlloc.data.HwAlloc.allocComprCovg  = comprCovg;

        CHECK_RC_CLEANUP(RmApiStatusToModsRC(LwRmVidHeapControl(&hwAlloc)));

        // Save the handle to the handles list
        allocHandlesList->push_front(hwAlloc.data.HwAlloc.hResourceHandle);

        // Again query RM for no. of free comptag lines, and rerun the loop if needed
        fbGetInfoParams.index = LW208F_CTRL_FB_INFO_INDEX_FREE_CONTIG_COMPRESSION_SIZE;
        INT32 status = LwRmControl(pLwRm->GetClientHandle(), m_hSubDevDiag,
                               LW208F_CTRL_CMD_FB_GET_INFO,
                               &fbGetInfoParams,
                               sizeof(fbGetInfoParams));

        if (status != LW_OK)
        {
            Printf(Tee::PriHigh, "%d: %s: Get free comptag lines call failed\n",
                   __LINE__, __FUNCTION__);
            rc = RmApiStatusToModsRC(status);
            CHECK_RC_CLEANUP(rc);
        }
        freeComptagLines = fbGetInfoParams.data / FB_COMPTAG_LINE_COVERAGE_LW50;
    }

    // Done, largest contiguous free comptaglines now are 'leavFree', so return
    return rc;

Cleanup:
    Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: failed when called with leaveFree value = %d\n",
           __LINE__, __FUNCTION__, (UINT32)leaveFree);
    // Now free all the comptags that we allocated in this function only
    RC rcCleanup = FreeComptagsUsingHwFree(allocHandlesList);
    if (OK != rcCleanup)
    {
        Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Failed while freeing comptags, rc = %s\n",
               __LINE__, __FUNCTION__, rc.Message());
    }
    return rc;
#else
    //
    // Shouldn't call this function if LW_VERIF_FEATURES not present, print message
    // and return error
    //
    Printf(Tee::PriHigh, "%d: %s: Called when LW_VERIF_FEATURES isn't present\n",
           __LINE__, __FUNCTION__);
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//!
//! @brief Rulwalidation()
//!
//! This function runs checks on the requested parameters v.s. returned
//! parameters from VidHeapAllocSizeEx() call and the PTE fields.
//! The verifications lwrrently performed by this function are :
//!   1.  Various values requested for ATTR_COMPR v.s. the value retuned
//!         for this attribute by RM i.e. what was requested v.s. what was
//!         actually allocated.
//!   2.  Various values requested for ATTR2_ZBC v.s. the value retuned
//!         for this attribute by RM i.e. what was requested v.s. what was
//!         actually allocated.
//!   3.  Kind allocated should be supported on the current GPU.
//!   4.  If the kind was over-rided at the time of allocation by a user
//!         specified kind, the user specified kind should match with the
//!         kind in the PTE.
//!   5.  If compression is allocated, the kind should be compressible.
//!   6.  The COMPTAGS field in PTE should match with compression allocation,
//!         i.e. if compression was not allocated, then PTE should have
//!         COMPTAGS_NONE, and if compression was allocated, PTE should have
//!         COMPTAGS1/COMPTAGS2/COMPTAGS4
//!   7.  If compression is allocated, the compression supported by the kind
//!         1-bit/2-bit/4-bit should match the COMPTAGS field in PTE
//!         COMPTAGS1/COMPTAGS2/COMPTAGS4 respectively.
//!   8.  If ZBC was allocated, the kind should support ZBC.
//!   9.  In case of policy testing, we exhaust comptags in such a way that
//!         the policy option we're testing is used. In this case, it is
//!         verified that the comptags in PTE are what we expect for the
//!         current allocation.
//!
//! @param[in]  handle
//!             The handle to the allocated memory.
//!
//! @param[in]  requestedVidAttr
//!             The ATTR paramter that was sent to VidHeapAllocSizeEx() call.
//!             This value is used to run checks on ATTR parameter returned by
//!             VidHeapAllocSizeEx() and on the kind allocated to the memory,
//!             i.e. if the kind complies with the ATTR_COMPR value.
//!
//! @param[in]  requestedVidAttr2
//!             The ATTR2 paramter that was sent to VidHeapAllocSizeEx() call.
//!             This value is used to run checks on ATTR2 parameter returned by
//!             VidHeapAllocSizeEx() and on the kind allocated to the memory,
//!             i.e. if the kind complies with the ATTR_ZBC value.
//!
//! @param[in]  returnedVidAttr
//!             The ATTR paramter returned from the VidHeapAllocSizeEx() call.
//!             We pass a pointer of this parameter to VidHeapAllocSizeEx() and
//!             RM changes it to reflect what actually gets allocated. This value
//!             is used to run a check against what we requested for this alloc.
//!
//! @param[in]  returnedVidAttr2
//!             The ATTR2 paramter returned from the VidHeapAllocSizeEx() call.
//!             We pass a pointer of this parameter to VidHeapAllocSizeEx() and
//!             RM changes it to reflect what actually gets allocated. This value
//!             is used to run a check against what we requested for this alloc.
//!
//! @param[in]  size
//!             The size of the memory allocation in bytes.
//!
//! @param[in]  bKindOverride
//!             A bool value to indicate that the kind selection algorithm was
//!             overrided at the time of the allocation, and the kind in the
//!             PTE also needs to be checked if it matches with the kind
//!             specified during the allocation.
//!
//! @param[in]  kindToBeTested
//!             The kind taht was specified during the allocation through kind
//!             override. This value is only used if bKindOverride is true.
//!
//! @param[in]  bAdditionalComptagsCheck
//!             A bool value to indicate that the caller will provide the
//!             COMPTAGS value, which should match with the COMPTAGS in the
//!             PTE. This is useful in the allocation policy test above, when
//!             the caller is testing fallback to each compression type and
//!             knows in advance the exact COMPTAGS value that should be found
//!             in the PTE.
//!
//! @param[in]  comptagsToCheck
//!             The COMPTAGS value that should be checked against COMPTAGS field
//!             in PTE and should match. This value is only used if
//!             bAdditionalComptagsCheck is true.
//!
//! @param[in]  bFallback
//!             A bool value to indicate that some of the checks we're going to
//!             run might fail because the policy test is testing fallback
//!             options from the allocation policy. This function needs this
//!             info to not report a false error due to those failed checks.
//!
//! @param[out] NONE
//!
//! @returns    OK if all the checks passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC ComprZBCTest::Rulwalidation
(
    const LwRm::Handle    handle,
    const UINT32          requestedVidAttr,
    const UINT32          requestedVidAttr2,
    const UINT32          returnedVidAttr,
    const UINT32          returnedVidAttr2,
    const UINT64          size,
    const bool            bKindOverride,
    const UINT32          kindToBeTested,
    const bool            bAdditionalComptagsCheck,
    const UINT32          comptagsToCheck,
    const bool            bFallback
)
{
    RC                rc;
    LwRmPtr           pLwRm;
    LwRm::Handle      hVA = 0;
    UINT64            gpuAddr = 0;
    UINT32            kindInPte;
    bool              bIsKindCompressible, bIsKindCompressible1, bIsKindCompressible2;
    bool              bIsKindZbc, bIsKindZbcAllows1, bIsKindZbcAllows2, bIsKindZbcAllows4;
    LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS  getParams;

    memset(&getParams, 0, sizeof(getParams));

    // Allocate a mappable virtual object
    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    //
    // Do MapMemoryDma() and get a GPU virtual address for
    // the handle passed by the client
    //
    CHECK_RC_CLEANUP(pLwRm->MapMemoryDma(hVA,
                                         handle,
                                         0,
                                         size,
                                         DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE),
                                         &gpuAddr,
                                         GetBoundGpuDevice()));

    // Get PTE info for this gpuAddr through GET_PTE_INFO ctrl cmd.
    getParams.gpuAddr = gpuAddr;
    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                    LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                                    &getParams,
                                    sizeof (getParams)));

    // Save the kind that we got from the PTE
    kindInPte = getParams.pteBlocks[0].kind;

    //
    // If we have kind override, then check if the kind
    // caller specified matches with the kind in the PTE
    //
    if (bKindOverride && (kindToBeTested != kindInPte))
    {
        Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: KIND overrided, but it does not match with the kind in the PTE,\
                             our kind = 0x%x, kind in PTE = 0x%x\n", __LINE__, __FUNCTION__, kindToBeTested, getParams.pteBlocks[0].kind);
        CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
    }

    // Verify that the kind is supported
    if (!IsKind(kindInPte, LW2080_CTRL_FB_IS_KIND_OPERATION_SUPPORTED))
    {
        Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: A non-supported kind 0x%x found in PTE\n",
               __LINE__, __FUNCTION__, kindInPte);
        CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
    }

    //
    // if we need to match the caller specified COMPTAGS value and
    // the COMPTAGS value in PTE, and they don't match, return error
    //
    if (bAdditionalComptagsCheck &&
            comptagsToCheck != DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_COMPTAGS, getParams.pteBlocks[0].pteFlags))
    {
        // Comptags donot match with expected value. Assign RC.
        Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Comptags check failed, we expected comptags = %d, comptags in PTE = %d\n",
               __LINE__, __FUNCTION__, comptagsToCheck, DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_COMPTAGS, getParams.pteBlocks[0].pteFlags));
        rc = RC::DATA_MISMATCH;
    }

    // Extract if kind is 1 bit compressible
    bIsKindCompressible1 = IsKind(kindInPte, LW2080_CTRL_FB_IS_KIND_OPERATION_COMPRESSIBLE_1);

    // Extract if kind is 2 bit compressible
    bIsKindCompressible2 = IsKind(kindInPte, LW2080_CTRL_FB_IS_KIND_OPERATION_COMPRESSIBLE_2);

    bIsKindCompressible = bIsKindCompressible1 | bIsKindCompressible2;

    // Extract whether kind supports ZBC_1
    bIsKindZbcAllows1 = IsKind(kindInPte, LW2080_CTRL_FB_IS_KIND_OPERATION_ZBC_ALLOWS_1);

    // Extract whether kind supports ZBC_2
    bIsKindZbcAllows2 = IsKind(kindInPte, LW2080_CTRL_FB_IS_KIND_OPERATION_ZBC_ALLOWS_2);

    // Extract whether kind supports ZBC_4
    bIsKindZbcAllows4 = IsKind(kindInPte, LW2080_CTRL_FB_IS_KIND_OPERATION_ZBC_ALLOWS_4);

    bIsKindZbc = bIsKindZbcAllows1 | bIsKindZbcAllows2 | bIsKindZbcAllows4;

    //!
    //! Checks for Requested ATTR_COMPR value v.s. returned ATTR_COMPR value.
    //! The expected behavior is depicted in this table:
    //!
    //!                                  +-----------------------------------------------------------------------------------+
    //!                                  |                                  Requested Value                                  |
    //!                                  +----------------+--------------------------------+---------------------------------+
    //!                                  |   COMPR_NONE   |           COMPR_ANY            |         COMPR_REQUIRED          |
    //! +------------+-------------------+----------------+--------------------------------+---------------------------------+
    //! |            |                   |                | OK if kind is non compressible | FAULT (The allocation should    |
    //! |            |    COMPR_NONE     |      OK        | GPU doesn't support compression|        have failed instead)     |
    //! |            |                   |                | or enuf comptags aren't avail  |                                 |
    //! |  Returned  |-------------------+----------------+--------------------------------+---------------------------------+
    //! |   Value    |  COMPR_REQUIRED   |     FAULT      |   OK if kind is compressible   | OK & kind should be compressible|
    //! |            |-------------------+----------------+--------------------------------+---------------------------------+
    //! |            | Any other value   |                                      FAULT                                        |
    //! +------------+-------------------+-----------------------------------------------------------------------------------+
    //!

    // if COMPR_NONE was returned..
    if (FLD_TEST_DRF(OS32, _ATTR, _COMPR, _NONE, returnedVidAttr))
    {
        if (FLD_TEST_DRF(OS32, _ATTR, _COMPR, _NONE, requestedVidAttr))
        {
            // ok because we had requested COMPR_NONE
        }
        else if (FLD_TEST_DRF(OS32, _ATTR, _COMPR, _ANY, requestedVidAttr))
        {
            //
            // We had requestd COMPR_ANY. RM should return COMPR_NONE if the
            // kind assigned was non-compressible.
            // This check might also fail if fallback is being tested, so
            // igore this failure in case of fallback
            //
            if (bIsKindCompressible && !bFallback)
            {
                //
                // We requested COMPR_ANY, and kind selected is also
                // compressible, but received COMPR_NONE, so fail
                //
                Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Kind 0x%x is compressible, we requested COMPR = ANY, \
                                      but received COMPR = NONE\n", __LINE__, __FUNCTION__, kindInPte);
                CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
            }
        }
        else if (FLD_TEST_DRF(OS32, _ATTR, _COMPR, _REQUIRED, requestedVidAttr))
        {
            //
            // We should never get COMPR_NONE if we requested COMPR_REQUIRED
            // The allocation should have failed instead.
            //
            Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: We requested COMPR_REQUIRED, but received COMPR_NONE\n",
                   __LINE__, __FUNCTION__);
            CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
        }
        else
        {
            // We should never hit this (requested COMPR value other than NONE/ANY/REQUIRED
            Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Something wrong here, requested ATTR_COMPR value is %d\n",
                   __LINE__, __FUNCTION__, DRF_VAL(OS32, _ATTR, _COMPR, requestedVidAttr));
            CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
        }

        //
        // Check for COMPTAGS field in PTE. Compression is not present, so
        // PTE comptags field should reflect COMPTAGS_NONE
        //
        if (!(FLD_TEST_DRF(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_COMPTAGS, _NONE, getParams.pteBlocks[0].pteFlags)))
        {
            Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Allocation has no compression but PTE does not have COMPTAGS-NONE\n",
                   __LINE__, __FUNCTION__);
            CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
        }
    }

    // if COMPR_REQUIRED was returned..
    else if (FLD_TEST_DRF(OS32, _ATTR, _COMPR, _REQUIRED, returnedVidAttr))
    {
        if (FLD_TEST_DRF(OS32, _ATTR, _COMPR, _NONE, requestedVidAttr))
        {
            //
            // We should never get COMPR_REQUIRED if we requested COMPR_NONE
            // so fail.
            //
            Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: We requested COMPR = NONE, but received COMPR_REQUIRED\n",
                   __LINE__, __FUNCTION__);
            CHECK_RC_CLEANUP(RC::DATA_MISMATCH);

        }
        else if (FLD_TEST_DRF(OS32, _ATTR, _COMPR, _ANY, requestedVidAttr))
        {
            //
            // We had requestd COMPR_ANY. RM should return COMPR_REQUIRED
            // if the kind assigned was compressible.
            //

            if (!bIsKindCompressible)
            {
                //
                // We requested COMPR_ANY, and kind selected was not compressible, but got
                // COMPR_REQUIRED, so fail
                //
                Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Kind 0x%x is not compressible, we requested COMPR = ANY, \
                                      but received COMPR = REQUIRED\n", __LINE__, __FUNCTION__, kindInPte);
                CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
            }
        }
        else if (FLD_TEST_DRF(OS32, _ATTR, _COMPR, _REQUIRED, requestedVidAttr))
        {
            //
            // requested COMPR=REQUIRED, received COMPR=REQUIRED, which is correct.
            // Just check if the kind is compressible, and fail if it isn't
            //
            if (!bIsKindCompressible)
            {
                Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Requested COMPR=REQUIRED and received COMPR=REQUIRED\
                                      which is correct, but kind 0x%x is not compressible!!\n",
                       __LINE__, __FUNCTION__, kindInPte);
                CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
            }
        }
        else
        {
            // We should never hit this (requested COMPR value other than NONE/ANY/REQUIRED
            Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Something wrong here, requested ATTR_COMPR value %d is illegal\n",
                   __LINE__, __FUNCTION__, DRF_VAL(OS32, _ATTR, _COMPR, requestedVidAttr));
            CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
        }

        //
        // Compression is present. Check for COMPTAGS field in PTE
        // PTE should have COMPTAGS1/COMPTAGS2/COMPTAGS4 value, and it should
        // match with the kind allocated, i.e. kind should be 1-bit/2-bit
        // compressible as per the COMPTAGS. In case of COMPTAGS4, it should
        // support 4 bit ZBC
        //
        switch(DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_COMPTAGS, getParams.pteBlocks[0].pteFlags))
        {
            case LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_COMPTAGS_1:
                if (!bIsKindCompressible1 &&
                    !(bIsKindZbcAllows1 && (FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _DEFAULT, requestedVidAttr2) ||
                                            FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _PREFER_ZBC, requestedVidAttr2))))
                {
                    Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Allocation has COMPTAGS_1 in \
                                          PTE but kind 0x%x is not 1-bit compressible\n",
                           __LINE__, __FUNCTION__, kindInPte);
                    CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
                }
                break;
            case LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_COMPTAGS_2:
                if (!bIsKindCompressible2 &&
                    !(bIsKindZbcAllows2 && (FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _DEFAULT, requestedVidAttr2) ||
                                            FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _PREFER_ZBC, requestedVidAttr2))))
                {
                    Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Allocation has COMPTAGS_2 in \
                                          PTE but kind 0x%x is not 2-bit compressible\n",
                           __LINE__, __FUNCTION__, kindInPte);
                    CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
                }
                break;
            case LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_COMPTAGS_4:
                if (!bIsKindZbcAllows4)
                {
                    Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Allocation has COMPTAGS_4 in \
                                          PTE but kind 0x%x does not support 4-bit ZBC\n",
                           __LINE__, __FUNCTION__, kindInPte);
                    CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
                }
                break;
            //
            // When compression is present, COMPTAGS fields can't have
            // COMPTAGS_NONE or any other illegal value. So fail.
            //
            case LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_COMPTAGS_NONE:
            default:
                {
                    Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Allocation has compression and \
                                          kind 0x%x but PTE has illegal value 0x%x for COMPTAGS\n",
                           __LINE__, __FUNCTION__, kindInPte, DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_COMPTAGS, getParams.pteBlocks[0].pteFlags));
                    CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
                }
                break;
        }
    }
    else
    {
        //
        // We should not have anything else other than
        // REQUIRED/NONE in COMPR returned param, so return error
        //
        Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Return params have illegal COMPR ATTR value 0x%x\n",
               __LINE__, __FUNCTION__, DRF_VAL(OS32, _ATTR, _COMPR, returnedVidAttr));
        CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
    }

    //!
    //! Check for Requested ATTR2_ZBC value v.s. returned ATTR2_ZBC value.
    //! The expected behavior is depicted in this table:
    //!
    //!                                  +--------------------------------------------------------------------------------------------------+
    //!                                  |                                        Requested Value                                           |
    //!                                  +-------------------------------+---------------------------------+--------------------------------+
    //!                                  |            DEFAULT            |         PREFER_NO_ZBC           |           PREFER_ZBC           |
    //! +------------+-------------------+-------------------------------+---------------------------------+--------------------------------+
    //! |            |                   | OK if kind is non-ZBC or      |                                 | OK if kind is non-ZBC or       |
    //! |            |   PREFER_NO_ZBC   | compression was not allocated |               OK                | compression was not allocated  |
    //! |            |                   | or GPU doesn't support ZBC    |                                 | or GPU doesn't support ZBC     |
    //! |            |                   | or enuf comptags aren't avail |                                 |                                |
    //! |  Returned  |-------------------+-------------------------------+---------------------------------+--------------------------------+
    //! |   Value    |                   | OK if kind supports ZBC and   | OK if we allocated a compression| OK and compression should have |
    //! |            |    PREFER_ZBC     | compression was allocated     | format that also supports ZBC   | been allocated and kind should |
    //! |            |                   |                               | for the chosen kind             | also support ZBC               |
    //! |            |-------------------+-------------------------------+---------------------------------+--------------------------------+
    //! |            |  Any other value  |                                              FAULT                                               |
    //! +------------+-------------------+--------------------------------------------------------------------------------------------------+
    //!

    // If PREFER_NO_ZBC was returned in the returned params..
    if (FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _PREFER_NO_ZBC, returnedVidAttr2))
    {
        //
        // If we had requested either PREFER_ZBC or DEFAULT, and the kind chosen
        // was not a ZBC kind, RM will return PREFER_NO_ZBC, but the allocation
        // will not fail.
        //
        if (FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _DEFAULT, requestedVidAttr2) ||
            FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _PREFER_ZBC, requestedVidAttr2))
        {
            //
            // if compression was allocated, and the kind also supports ZBC,
            // and we requested ZBC_DEFAULT/PREFER_ZBC, it should have returned
            // PREFER_ZBC but it returned PREFER_NO_ZBC, so return error.
            //
            if (bIsKindZbc && FLD_TEST_DRF(OS32, _ATTR, _COMPR, _REQUIRED, returnedVidAttr))
            {
                //
                // This check might also fail with ZBC_DEFAULT if fallback is being tested, so
                // ignore this failure in case of fallback being tested & ZBC_DEF in requested params
                //
                if ( !(bFallback && FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _DEFAULT, requestedVidAttr2)))
                {
                    Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Compression was allocated, kind 0x%x supports ZBC, and\
                                          we requested %s, so ZBC should have been allocated. PREFER_NO_ZBC\
                                          was returned in return params instead of PREFER_ZBC\n",
                           __LINE__, __FUNCTION__, kindInPte,
                           FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _DEFAULT, requestedVidAttr2)? "ZBC_DEFAULT" : "PREFER_ZBC");
                    CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
                }
            }
        }
        else if (FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _PREFER_NO_ZBC, requestedVidAttr2))
        {
            // ok
        }
        else
        {
            // We should never hit this (requested ZBC value other than DEFAULT/PREFER_ZBC/PREFER_NO_ZBC
            Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Something wrong here, requested ATTR2_ZBC value %d illegal\n",
                   __LINE__, __FUNCTION__, DRF_VAL(OS32, _ATTR2, _ZBC, requestedVidAttr));
            CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
        }
    }

    // If we received PREFER_ZBC in the returned params.
    else if (FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _PREFER_ZBC, returnedVidAttr2))
    {
        //
        // if we had requested DEFAULT or PREFER_ZBC, then its okay, and the
        // kind should be a ZBC kind
        //
        if (FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _DEFAULT, requestedVidAttr2) ||
            FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _PREFER_ZBC, requestedVidAttr2))
        {
            //
            // if we got PREFER_ZBC in return params, and we had requested ZBC_DEFAULT
            // or PREFER_ZBC then the kind should support ZBC and also compression
            // should have been allocated. Else return error
            //
            if (!bIsKindZbc || !FLD_TEST_DRF(OS32, _ATTR, _COMPR, _REQUIRED, returnedVidAttr))
            {
                Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: We got PREFER_ZBC in return params, \
                                      when ZBC_DEFAULT was requested, kind 0x%x does %s support ZBC,\
                                      and compression was %s allocated,\n",
                       __LINE__, __FUNCTION__, kindInPte, bIsKindZbc? "" : "not",
                       FLD_TEST_DRF(OS32, _ATTR, _COMPR, _REQUIRED, returnedVidAttr) ? "" : "not");
                CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
            }
        }
        else if (FLD_TEST_DRF(OS32, _ATTR2, _ZBC, _PREFER_NO_ZBC, requestedVidAttr2))
        {
            //
            // if we requested PREFER_NO_ZBC, we might get ZBC if we choose
            // a compression format that also supports ZBC for a given kind
            //
            if (!bIsKindZbc ||
                !FLD_TEST_DRF(OS32, _ATTR, _COMPR, _REQUIRED, returnedVidAttr) ||
                (FLD_TEST_DRF(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_COMPTAGS, _1, getParams.pteBlocks[0].pteFlags) && !bIsKindZbcAllows1) ||
                (FLD_TEST_DRF(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_COMPTAGS, _2, getParams.pteBlocks[0].pteFlags) && !bIsKindZbcAllows2) ||
                (FLD_TEST_DRF(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_COMPTAGS, _4, getParams.pteBlocks[0].pteFlags) && !bIsKindZbcAllows4))
            {
                Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: We got PREFER_ZBC in return params, \
                                      when PREFER_NO_ZBC was requested, kind 0x%x does %s support ZBC,\
                                      and compression was %s allocated, COMPTAGS allocated were ",
                       __LINE__, __FUNCTION__, kindInPte, bIsKindZbc? "": "not",
                       FLD_TEST_DRF(OS32, _ATTR, _COMPR, _REQUIRED, returnedVidAttr) ? "" : "not");
                switch (DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_COMPTAGS, getParams.pteBlocks[0].pteFlags))
                {
                    case LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_COMPTAGS_NONE:
                        Printf(Tee::PriHigh, "NONE");
                        break;
                    case LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_COMPTAGS_1:
                        Printf(Tee::PriHigh, "1");
                        break;
                    case LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_COMPTAGS_2:
                        Printf(Tee::PriHigh, "2");
                        break;
                    case LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_COMPTAGS_4:
                        Printf(Tee::PriHigh, "4");
                        break;
                    default:
                        Printf(Tee::PriHigh, "Illegal value of COMPTAGS.");
                        break;
                }
                CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
            }
        }
        else
        {
            // We should never hit this (requested ZBC value other than DEFAULT/PREFER_ZBC/PREFER_NO_ZBC
            Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Something wrong here, requested ATTR2_ZBC value %d illegal\n",
                   __LINE__, __FUNCTION__, DRF_VAL(OS32, _ATTR2, _ZBC, requestedVidAttr));
            CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
        }
    }
    else
    {
        //
        // We should not have anything else other than
        // PREFER_ZBC/PREFER_NO_ZBC in ZBC return param, so return error
        //
        Printf(Tee::PriHigh, "%d:ComprZBCTest::%s: Return params have illegal ZBC ATTR value 0x%x\n",
               __LINE__, __FUNCTION__, DRF_VAL(OS32, _ATTR2, _ZBC, returnedVidAttr2));
        CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
    }

    // Now go down the Cleanup path, do cleanup and return.
Cleanup:
    if (hVA && gpuAddr)
    {
        pLwRm->UnmapMemoryDma(hVA,
                              handle,
                              LW04_MAP_MEMORY_FLAGS_NONE,
                              gpuAddr,
                              GetBoundGpuDevice());
    }
    pLwRm->Free(hVA);
    return rc;
}

//!
//! @brief IsKind()
//!
//! This function takes two parameters kind and operation, and queries
//! RM for that that combination and returns the boolean result (true/false)
//!
//! @param[in]  kind
//!             The kind value for which the info is to be queried.
//!
//! @param[in]  operation
//!             The operation to run on kind value i.e. compressible.
//!
//! @param[out] NONE
//!
//! @returns    True if the RM call succeeds and kind supports the operation,
//!             false otherwise.
//-----------------------------------------------------------------------------
bool ComprZBCTest::IsKind(const LwU32 kind, const LwU32 operation)
{
    LW2080_CTRL_FB_IS_KIND_PARAMS fbIsKindParams = {0};
    fbIsKindParams.kind = kind;
    fbIsKindParams.operation = operation;
    RC rc = LwRmPtr()->ControlBySubdevice(GetBoundGpuSubdevice(),
                                          LW2080_CTRL_CMD_FB_IS_KIND,
                                          &fbIsKindParams,
                                          sizeof(fbIsKindParams));

    return ((rc == OK) && (fbIsKindParams.result));
}

//!
//! @brief FreeComptagsUsingHwFree()
//!
//! This function takes a list of handles that represent comptags allocated
//! through HW_ALLOC function. It goes through the list and calls HW_FREE
//! function for each handle to free the comptags.
//!
//! @param[in]  allocHandlesList
//!             A pointer to the list of handles.
//!
//! @param[out] allocHandlesList
//!             The list (empty) after freeing the comptags for each handle.
//!
//! @returns    OK if all the handles were freed successfully,
//!             specific RC otherwise.
//-----------------------------------------------------------------------------
RC ComprZBCTest::FreeComptagsUsingHwFree(list<LwU32> *allocHandlesList)
{
    RC                      rc;
    LwRmPtr                 pLwRm;
    LWOS32_PARAMETERS       hwFree;
    list<LwU32>::iterator   listIterator;

    while(allocHandlesList->size() > 0)
    {
        listIterator = allocHandlesList->begin();
        memset(&hwFree, 0, sizeof(hwFree));
        hwFree.hRoot         = pLwRm->GetClientHandle();
        hwFree.hObjectParent = pLwRm->GetDeviceHandle(GetBoundGpuDevice());

        // Use HW_FREE function to free comptags allocated through HW_ALLOC
        hwFree.function                    = LWOS32_FUNCTION_HW_FREE;
        hwFree.data.HwFree.hResourceHandle = *listIterator;
        hwFree.data.HwFree.flags           = LWOS32_DELETE_RESOURCES_ALL;
        CHECK_RC_CLEANUP(RmApiStatusToModsRC(LwRmVidHeapControl(&hwFree)));
        allocHandlesList->pop_front();
    }
    return rc;

Cleanup:
    Printf(Tee::PriHigh, "%d: %s: HW_FREE function failed!!\n", __LINE__, __FUNCTION__);
    return rc;
}

//!
//! @brief SanityHeapCheckerParams()
//!
//! This function checks the input parameters of heap allocation (vidType and
//! vidAttr) for invalid combinations. This sanity checker is a subset of a
//! similar sanity checker used in rmt_mmutester.cpp
//!
//! @param[in]  vidType
//!             The TYPE parameter of heap allocation.
//!
//! @param[in]  vidAttr
//!             The ATTR paramter of heap allocation.
//!
//! @param[out] NONE
//!
//! @returns    True if the sanity passed and allocation can be tried,
//!             False otherwise.
//-----------------------------------------------------------------------------
bool ComprZBCTest::SanityCheckerHeapParams(const UINT32 vidType, const UINT32 vidAttr)
{
    //
    // If FORMAT_OVERRIDE_OFF and FORMAT is BLOCK_LINEAR
    //
#ifdef  LW_VERIF_FEATURES
    if(DRF_VAL(OS32, _ATTR, _FORMAT_OVERRIDE, vidAttr)!= LWOS32_ATTR_FORMAT_OVERRIDE_ON)
#endif
    {
        if(DRF_VAL(OS32, _ATTR, _FORMAT, vidAttr)
            == LWOS32_ATTR_FORMAT_BLOCK_LINEAR)
        {
            //
            // If LOCATION is VIDMEM
            //
            if(DRF_VAL(OS32, _ATTR, _LOCATION, vidAttr)
                == LWOS32_ATTR_LOCATION_VIDMEM)
            {
                //
                // If TYPE is DEPTH && Z_TYPE is FIXED &&
                // if ZS_PACKING is either Z16/ Z32/ Z32_X24S8/ X8Z24_X24S8
                // the allocation fails, hence skipping
                //
                if(vidType == LWOS32_TYPE_DEPTH)
                {
                    if((DRF_VAL(OS32, _ATTR, _Z_TYPE, vidAttr) ==
                        LWOS32_ATTR_Z_TYPE_FIXED) &&
                       ((DRF_VAL(OS32, _ATTR, _ZS_PACKING, vidAttr) ==
                        LWOS32_ATTR_ZS_PACKING_Z16)||
                       (DRF_VAL(OS32, _ATTR, _ZS_PACKING, vidAttr) ==
                        LWOS32_ATTR_ZS_PACKING_Z32)||
                       (DRF_VAL(OS32, _ATTR, _ZS_PACKING, vidAttr) ==
                        LWOS32_ATTR_ZS_PACKING_Z32_X24S8)||
                       (DRF_VAL(OS32, _ATTR, _ZS_PACKING, vidAttr) ==
                        LWOS32_ATTR_ZS_PACKING_X8Z24_X24S8)))
                    {
                        if (COMPR_TEST_DEBUG)
                        {
                            Printf(Tee::PriHigh, "Sanity 1 hit\n");
                        }
                        return false;
                    }
                }
                else
                {
                    // because the fbCompressibleKind() will be 0
                    //for the following cases
                    // which results in ILWALID_ARGUMENT error
                    if((DRF_VAL(OS32, _ATTR, _DEPTH, vidAttr) ==
                        LWOS32_ATTR_DEPTH_24)||
                        ((DRF_VAL(OS32, _ATTR, _DEPTH, vidAttr) ==
                        LWOS32_ATTR_DEPTH_16)&&
                        (DRF_VAL(OS32, _ATTR, _COMPR, vidAttr) ==
                        LWOS32_ATTR_COMPR_REQUIRED))||
                        ((DRF_VAL(OS32, _ATTR, _DEPTH, vidAttr) ==
                        LWOS32_ATTR_DEPTH_128)&&
                        (DRF_VAL(OS32, _ATTR, _COMPR, vidAttr) ==
                        LWOS32_ATTR_COMPR_REQUIRED))||
                        ((DRF_VAL(OS32, _ATTR, _DEPTH, vidAttr) ==
                        LWOS32_ATTR_DEPTH_UNKNOWN)&&
                        (DRF_VAL(OS32, _ATTR, _COMPR, vidAttr) ==
                        LWOS32_ATTR_COMPR_REQUIRED))||
                        ((DRF_VAL(OS32, _ATTR, _DEPTH, vidAttr) ==
                        LWOS32_ATTR_DEPTH_8)&&
                        (DRF_VAL(OS32, _ATTR, _COMPR, vidAttr) ==
                        LWOS32_ATTR_COMPR_REQUIRED)))

                    {
                        if (COMPR_TEST_DEBUG)
                        {
                            Printf(Tee::PriHigh, "Sanity 2 hit\n");
                        }
                        return false;
                    }
                }
            }

            if(vidType == LWOS32_TYPE_DEPTH &&
                ((DRF_VAL(OS32, _ATTR, _Z_TYPE, vidAttr)!=
                            LWOS32_ATTR_Z_TYPE_FIXED) ||
                    (DRF_VAL(OS32, _ATTR, _ZS_PACKING, vidAttr)!=
                            LWOS32_ATTR_ZS_PACKING_Z16)))
            {
                if (COMPR_TEST_DEBUG)
                {
                    Printf(Tee::PriHigh, "Sanity 3 hit\n");
                }
                return false;
            }
        }

    }

    // All sanity passed, this combination is good to go, so return true
    return true;
}

//!
//! @brief doFreeTest() - Tests that surface free is properly exelwting compression-flush
//!
//! Tests whether or not compression-flushes are oclwrring during our free path
//! as expected by creating a situation where if the flush were not oclwrring
//! we would be able to detect it.
//!
//! @returns    OK if the test passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC ComprZBCTest::doFreeTest()
{
    RC rc = OK;
    UINT32 cSurfTagsMin;
    UINT32 cSurf2TagsMin;
    UINT32 cSurfTagsMax;
    UINT32 cSurf2TagsMax;
    UINT32 zSurfTagsMin;
    UINT32 zSurfTagsMax;

    Printf(Tee::PriLow, "Running Free test\n");

    //
    // Set up surfaces, do this during each test since the free clears most of
    // the settings.
    //
    m_cSurf.SetWidth(256);
    m_cSurf.SetHeight(256);
    m_cSurf.SetColorFormat(ColorUtils::A8R8G8B8);
    m_cSurf.SetLocation(Memory::Fb);
    m_cSurf.SetLayout(Surface2D::BlockLinear);
    m_cSurf.SetCompressed(true);
    m_cSurf.SetZbcMode(Surface2D::ZbcOn);
    m_cSurf.SetType(LWOS32_TYPE_IMAGE);

    m_cSurf2.SetWidth(256);
    m_cSurf2.SetHeight(256);
    m_cSurf2.SetColorFormat(ColorUtils::A8R8G8B8);
    m_cSurf2.SetLocation(Memory::Fb);
    m_cSurf2.SetLayout(Surface2D::BlockLinear);
    m_cSurf2.SetCompressed(true);
    m_cSurf2.SetZbcMode(Surface2D::ZbcOn);
    m_cSurf2.SetType(LWOS32_TYPE_IMAGE);

    m_zSurf.SetWidth(256);
    m_zSurf.SetHeight(256);
    m_zSurf.SetLayout(Surface2D::BlockLinear);
    m_zSurf.SetCompressed(true);
    m_zSurf.SetZbcMode(Surface2D::ZbcOn);
    m_zSurf.SetColorFormat(ColorUtils::Z24S8);
    m_zSurf.SetLocation(Memory::Fb);
    m_zSurf.SetType(LWOS32_TYPE_DEPTH);

    m_cSurf.BindGpuChannel(m_hCh);
    CHECK_RC_CLEANUP(m_cSurf.Alloc(GetBoundGpuDevice()));
    m_cSurf2.BindGpuChannel(m_hCh);
    CHECK_RC_CLEANUP(m_cSurf2.Alloc(GetBoundGpuDevice()));

    CHECK_RC_CLEANUP(getTagsForSurface(&m_cSurf, &cSurfTagsMin, &cSurfTagsMax));
    CHECK_RC_CLEANUP(getTagsForSurface(&m_cSurf2, &cSurf2TagsMin, &cSurf2TagsMax));

    Printf(Tee::PriLow, "cSurf ctags: 0x%x-0x%x cSurf2 ctags 0x%x-0x%x\n",
            cSurfTagsMin, cSurfTagsMax, cSurf2TagsMin, cSurf2TagsMax);

    // Do fast-clear using different clear values to use both clear values in ZBC rams
    Printf(Tee::PriLow, "Doing fast color-clears...\n");
    CHECK_RC_CLEANUP(doFastColorClear(&m_cSurf, 0, 0x3f800000, 0, 0x3f800000));
    CHECK_RC_CLEANUP(doFastColorClear(&m_cSurf2, 0x3f800000, 0, 0, 0x3f800000));

    Printf(Tee::PriLow, "Freeing surfaces...\n");
    m_cSurf.Free();
    m_cSurf2.Free();

    Printf(Tee::PriLow, "Verifying ZBC Refcounts...\n");
    // We rely on cSurf and cSurf2 getting back to back comptag allocations
    if (cSurfTagsMax+1 != cSurf2TagsMin)
    {
        Printf(Tee::PriHigh, "Warning: Color surfaces' comptag allocations not adjacent.  Results may not be reliable\n");
    }

    CHECK_RC(verifyZbcRefCount(cSurfTagsMin, cSurf2TagsMax-cSurfTagsMin+1));

    // Should get the same comptags
    m_zSurf.BindGpuChannel(m_hCh);
    CHECK_RC(m_zSurf.Alloc(GetBoundGpuDevice()));

    CHECK_RC_CLEANUP(getTagsForSurface(&m_zSurf, &zSurfTagsMin, &zSurfTagsMax));

    Printf(Tee::PriLow, "zSurf ctags: 0x%x-0x%x\n",
            zSurfTagsMin, zSurfTagsMax);

    if ((zSurfTagsMin != cSurfTagsMin) ||
            zSurfTagsMax != cSurf2TagsMax)
    {
        Printf(Tee::PriHigh, "Z-Surface comptags are not the same as previous C-Surface comptags.  Results may not be reliable\n");
    }

    Printf(Tee::PriLow, "Doing fast z-clear...\n");
    CHECK_RC_CLEANUP(doFastZClear(&m_zSurf, 0x12, 0x23));

    Printf(Tee::PriLow, "Freeing surface...\n");
    m_zSurf.Free();

    Printf(Tee::PriLow, "Verifying ZBC Refcounts...\n");
    CHECK_RC(verifyZbcRefCount(zSurfTagsMin, zSurfTagsMax-zSurfTagsMin));

Cleanup:
    if (rc != OK)
    {
        m_cSurf.Free();
        m_cSurf2.Free();
        m_zSurf.Free();
    }

    return rc;
}

//!
//! @brief doReleaseTest() - Tests that compresson-release is properly exelwting compression-flush
//!
//! Tests whether or not compression-flushes are oclwrring during our
//! compression release path as expected by creating a situation where if the
//! flush were not oclwrring we would be able to detect it.
//!
//! @returns    OK if the test passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC ComprZBCTest::doReleaseTest()
{
    LwRmPtr pLwRm;
    RC rc = OK;
    UINT32 cSurfTagsMin;
    UINT32 cSurf2TagsMin;
    UINT32 cSurfTagsMax;
    UINT32 cSurf2TagsMax;
    UINT32 zSurfTagsMin;
    UINT32 zSurfTagsMax;

    Printf(Tee::PriLow, "Running Release test\n");

    //
    // Set up surfaces, do this during each test since the free clears most of
    // the settings.
    //
    m_cSurf.SetWidth(256);
    m_cSurf.SetHeight(256);
    m_cSurf.SetColorFormat(ColorUtils::A8R8G8B8);
    m_cSurf.SetLocation(Memory::Fb);
    m_cSurf.SetLayout(Surface2D::BlockLinear);
    m_cSurf.SetCompressed(true);
    m_cSurf.SetZbcMode(Surface2D::ZbcOn);
    m_cSurf.SetType(LWOS32_TYPE_IMAGE);

    m_cSurf2.SetWidth(256);
    m_cSurf2.SetHeight(256);
    m_cSurf2.SetColorFormat(ColorUtils::A8R8G8B8);
    m_cSurf2.SetLocation(Memory::Fb);
    m_cSurf2.SetLayout(Surface2D::BlockLinear);
    m_cSurf2.SetCompressed(true);
    m_cSurf2.SetZbcMode(Surface2D::ZbcOn);
    m_cSurf2.SetType(LWOS32_TYPE_IMAGE);

    m_zSurf.SetWidth(256);
    m_zSurf.SetHeight(256);
    m_zSurf.SetLayout(Surface2D::BlockLinear);
    m_zSurf.SetCompressed(true);
    m_zSurf.SetZbcMode(Surface2D::ZbcOn);
    m_zSurf.SetColorFormat(ColorUtils::Z24S8);
    m_zSurf.SetLocation(Memory::Fb);
    m_zSurf.SetType(LWOS32_TYPE_DEPTH);

    m_cSurf.BindGpuChannel(m_hCh);
    CHECK_RC(m_cSurf.Alloc(GetBoundGpuDevice()));
    m_cSurf2.BindGpuChannel(m_hCh);
    CHECK_RC_CLEANUP(m_cSurf2.Alloc(GetBoundGpuDevice()));

    CHECK_RC_CLEANUP(getTagsForSurface(&m_cSurf, &cSurfTagsMin, &cSurfTagsMax));
    CHECK_RC_CLEANUP(getTagsForSurface(&m_cSurf2, &cSurf2TagsMin, &cSurf2TagsMax));

    Printf(Tee::PriLow, "cSurf ctags: 0x%x-0x%x cSurf2 ctags 0x%x-0x%x\n",
            cSurfTagsMin, cSurfTagsMax, cSurf2TagsMin, cSurf2TagsMax);

    // Do fast-clear using different clear values to use both clear values in ZBC rams
    Printf(Tee::PriLow, "Doing fast color-clears...\n");
    CHECK_RC_CLEANUP(doFastColorClear(&m_cSurf, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000));
    CHECK_RC_CLEANUP(doFastColorClear(&m_cSurf2, 0, 0, 0, 0));

    // Release should trigger compression flush
    Printf(Tee::PriLow, "Releasing compression...\n");
    pLwRm->VidHeapReleaseCompression(m_cSurf.GetMemHandle(), GetBoundGpuDevice());
    pLwRm->VidHeapReleaseCompression(m_cSurf2.GetMemHandle(), GetBoundGpuDevice());

    Printf(Tee::PriLow, "Verifying ZBC Refcounts...\n");
    // We rely on cSurf and cSurf2 getting back to back comptag allocations
    if (cSurfTagsMax+1 != cSurf2TagsMin)
    {
        Printf(Tee::PriHigh, "Warning: Color surfaces' comptag allocations not adjacent.  Results may not be reliable\n");
    }
    CHECK_RC_CLEANUP(verifyZbcRefCount(cSurfTagsMin, cSurf2TagsMax-cSurfTagsMin+1));

    // Should get the same comptags
    m_zSurf.BindGpuChannel(m_hCh);
    CHECK_RC_CLEANUP(m_zSurf.Alloc(GetBoundGpuDevice()));

    CHECK_RC_CLEANUP(getTagsForSurface(&m_zSurf, &zSurfTagsMin, &zSurfTagsMax));

    Printf(Tee::PriLow, "zSurf ctags: 0x%x-0x%x\n",
            zSurfTagsMin, zSurfTagsMax);

    if ((zSurfTagsMin != cSurfTagsMin) ||
            zSurfTagsMax != cSurf2TagsMax)
    {
        Printf(Tee::PriHigh, "Z-Surface comptags are not the same as previous C-Surface comptags.  Results may not be reliable\n");
    }

    Printf(Tee::PriLow, "Doing fast z-clear...\n");
    CHECK_RC_CLEANUP(doFastZClear(&m_zSurf, 0x12, 0x23));

    // Release should trigger compression flush
    Printf(Tee::PriLow, "Releasing compression...\n");
    pLwRm->VidHeapReleaseCompression(m_zSurf.GetMemHandle(), GetBoundGpuDevice());

    Printf(Tee::PriLow, "Verifying ZBC Refcounts...\n");
    CHECK_RC(verifyZbcRefCount(zSurfTagsMin, zSurfTagsMax-zSurfTagsMin));

Cleanup:
    m_cSurf.Free();
    m_cSurf2.Free();
    m_zSurf.Free();

    return rc;
}

//!
//! @brief doFastColorClear() - Helper function which does a fast color clear to put surface in ZBC state.
//!
//! @param[in] surf
//!            The surface to clear
//! @param[in] r
//!            The Red component of clear value
//! @param[in] g
//!            The Green component of clear value
//! @param[in] b
//!            The Blue component of clear value
//! @param[in] a
//!            The Alpha component of clear value
//!
//! @returns    OK if the clear was sucessful, an appropriate RC value otherwise
//-----------------------------------------------------------------------------
RC ComprZBCTest::doFastColorClear(Surface2D *surf, UINT32 r, UINT32 g, UINT32 b, UINT32 a)
{
    RC rc;

    m_pCh->Write(1, LW8697_SET_CTX_DMA_COLOR(0), surf->GetCtxDmaHandleGpu());
    m_pCh->Write(1, LW8697_SET_CT_A(0), LwU64_HI32(surf->GetCtxDmaOffsetGpu()));
    m_pCh->Write(1, LW8697_SET_CT_B(0), LwU64_LO32(surf->GetCtxDmaOffsetGpu()));
    m_pCh->Write(1, LW8697_SET_CT_FORMAT(0), LW8697_SET_CT_FORMAT_V_A8R8G8B8);
    m_pCh->Write(1, LW8697_SET_CT_BLOCK_SIZE(0),
        DRF_NUM(8697, _SET_CT_BLOCK_SIZE, _WIDTH, surf->GetLogBlockWidth()) |
        DRF_NUM(8697, _SET_CT_BLOCK_SIZE, _HEIGHT, surf->GetLogBlockHeight()) |
        DRF_NUM(8697, _SET_CT_BLOCK_SIZE, _DEPTH, surf->GetLogBlockDepth()));
    m_pCh->Write(1, LW8697_SET_CT_ARRAY_PITCH(0), (LwU32) (surf->GetArrayPitch() >> 2));
    m_pCh->Write(1, LW8697_SET_CT_SIZE_A(0),
        DRF_DEF(8697, _SET_CT_SIZE_A, _LAYOUT_IN_MEMORY, _BLOCKLINEAR) |
        DRF_NUM(8697, _SET_CT_SIZE_A, _WIDTH, surf->GetWidth()));
    m_pCh->Write(1, LW8697_SET_CT_SIZE_B(0),
        DRF_NUM(8697, _SET_CT_SIZE_B, _HEIGHT, surf->GetHeight()));
    m_pCh->Write(1, LW8697_SET_CT_SIZE_C,
            DRF_NUM(8697, _SET_CT_SIZE_C, _THIRD_DIMENSION, 1) |
            DRF_DEF(8697, _SET_CT_SIZE_C, _CONTROL, _THIRD_DIMENSION_DEFINES_ARRAY_SIZE));

    m_pCh->Write(1, LW8697_SET_CLEAR_CONTROL,
            DRF_DEF(8697, _SET_CLEAR_CONTROL, _RESPECT_STENCIL_MASK, _FALSE) |
            DRF_DEF(8697, _SET_CLEAR_CONTROL, _USE_CLEAR_RECT, _FALSE));

    m_pCh->Write(1, LW8697_SET_COLOR_CLEAR_VALUE(0), r);  // R
    m_pCh->Write(1, LW8697_SET_COLOR_CLEAR_VALUE(1), g);  // G
    m_pCh->Write(1, LW8697_SET_COLOR_CLEAR_VALUE(2), b);  // B
    m_pCh->Write(1, LW8697_SET_COLOR_CLEAR_VALUE(3), a);  // A

    m_pCh->Write(1, LW8697_SET_ANTI_ALIAS, DRF_DEF(8697, _SET_ANTI_ALIAS, _SAMPLES, _MODE_1X1));
    m_pCh->Write(1, LW8697_SET_CT_MARK(0), DRF_DEF(8697, _SET_CT_MARK, _IEEE_CLEAN, _TRUE));

    m_pCh->Write(1, LW8697_SET_CT_WRITE(0),
        DRF_DEF(8697, _SET_CT_WRITE, _R_ENABLE, _TRUE) |
        DRF_DEF(8697, _SET_CT_WRITE, _G_ENABLE, _TRUE) |
        DRF_DEF(8697, _SET_CT_WRITE, _B_ENABLE, _TRUE) |
        DRF_DEF(8697, _SET_CT_WRITE, _A_ENABLE, _TRUE));

    m_pCh->Write(1, LW8697_SET_RENDER_ENABLE_A, 0);
    m_pCh->Write(1, LW8697_SET_RENDER_ENABLE_B, 0);
    m_pCh->Write(1, LW8697_SET_RENDER_ENABLE_C, 1);

     // Assumes no 3D RT arrays
     UINT32 MaxZ = max(surf->GetDepth(),
         surf->GetArraySize());

    for (UINT32 z = 0; z < MaxZ; z++)
    {
        // Enable all the color planes
        UINT32 enables =
            DRF_NUM(8697, _CLEAR_SURFACE, _MRT_SELECT, 0) |
            DRF_NUM(8697, _CLEAR_SURFACE, _RT_ARRAY_INDEX, z) |
            DRF_DEF(8697, _CLEAR_SURFACE, _R_ENABLE, _TRUE) |
            DRF_DEF(8697, _CLEAR_SURFACE, _G_ENABLE, _TRUE) |
            DRF_DEF(8697, _CLEAR_SURFACE, _B_ENABLE, _TRUE) |
            DRF_DEF(8697, _CLEAR_SURFACE, _STENCIL_ENABLE, _FALSE) |
            DRF_DEF(8697, _CLEAR_SURFACE, _Z_ENABLE, _FALSE) |
            DRF_DEF(8697, _CLEAR_SURFACE, _A_ENABLE, _TRUE);

        // Trigger the clear
        m_pCh->Write(1,LW8697_CLEAR_SURFACE, enables);
    }
    CHECK_RC(m_Notifier.Instantiate(1, LW8697_SET_CONTEXT_DMA_NOTIFY));
    m_Notifier.Clear(LW8697_NOTIFIERS_NOTIFY);
    CHECK_RC(m_Notifier.Write(1));
    CHECK_RC(m_pCh->Write(1, LW8697_NO_OPERATION, 0));
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_Notifier.Wait(LW8697_NOTIFIERS_NOTIFY, m_TimeoutMs));

    return rc;
}

//!
//! @brief doFastZClear() - Helper function which does a fast Z clear to put surface in ZBC state.
//!
//! @param[in] surf
//!            The surface to clear
//! @param[in] zClear
//!            The Z clearvalue.
//! @param[in] sClear
//!            The Stencil clearvalue.
//!
//! @returns    OK if the clear was sucessful, an appropriate RC value otherwise
//-----------------------------------------------------------------------------
RC ComprZBCTest::doFastZClear(Surface2D *surf, UINT32 zClear, UINT32 sClear)
{
    RC rc;

    m_pCh->Write(1, LW8697_SET_CTX_DMA_ZETA, surf->GetCtxDmaHandleGpu());
    m_pCh->Write(1, LW8697_SET_ZT_A, LwU64_HI32(surf->GetCtxDmaOffsetGpu()));
    m_pCh->Write(1, LW8697_SET_ZT_B, LwU64_LO32(surf->GetCtxDmaOffsetGpu()));
    m_pCh->Write(1, LW8697_SET_ZT_FORMAT, LW8697_SET_ZT_FORMAT_V_Z24S8);
    m_pCh->Write(1, LW8697_SET_ZT_BLOCK_SIZE,
        DRF_NUM(8697, _SET_ZT_BLOCK_SIZE, _WIDTH, surf->GetLogBlockWidth()) |
        DRF_NUM(8697, _SET_ZT_BLOCK_SIZE, _HEIGHT, surf->GetLogBlockHeight()) |
        DRF_NUM(8697, _SET_ZT_BLOCK_SIZE, _DEPTH, surf->GetLogBlockDepth()));
    m_pCh->Write(1, LW8697_SET_ZT_ARRAY_PITCH, (LwU32) (surf->GetArrayPitch() >> 2));
    m_pCh->Write(1, LW8697_SET_ZT_SIZE_A,
        DRF_NUM(8697, _SET_ZT_SIZE_A, _WIDTH, surf->GetWidth()));
    m_pCh->Write(1, LW8697_SET_ZT_SIZE_B,
        DRF_NUM(8697, _SET_ZT_SIZE_B, _HEIGHT, surf->GetHeight()));
    m_pCh->Write(1, LW8697_SET_ZT_SIZE_C,
            DRF_NUM(8697, _SET_ZT_SIZE_C, _THIRD_DIMENSION, 1) |
            DRF_DEF(8697, _SET_ZT_SIZE_C, _CONTROL, _THIRD_DIMENSION_DEFINES_ARRAY_SIZE));
    m_pCh->Write(1, LW8697_SET_Z_CLEAR_VALUE, zClear);
    m_pCh->Write(1, LW8697_SET_STENCIL_CLEAR_VALUE, sClear);

    m_pCh->Write(1, LW8697_CLEAR_SURFACE,
        DRF_DEF(8697, _CLEAR_SURFACE, _Z_ENABLE, _TRUE) |
        DRF_DEF(8697, _CLEAR_SURFACE, _STENCIL_ENABLE, _TRUE));

    CHECK_RC(m_Notifier.Instantiate(1, LW8697_SET_CONTEXT_DMA_NOTIFY));
    m_Notifier.Clear(LW8697_NOTIFIERS_NOTIFY);
    CHECK_RC(m_Notifier.Write(1));
    CHECK_RC(m_pCh->Write(1, LW8697_NO_OPERATION, 0));
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_Notifier.Wait(LW8697_NOTIFIERS_NOTIFY, m_TimeoutMs));

    return rc;
}

//!
//! @brief getTagsForSurface() - Helper function which queries RM for compression tags associated with surface.
//!
//! @param[in] surf
//!            The surface to query
//! @param[out] start
//!             The first compression tag address associated with the surface.
//! @param[out] end
//!             The last compression tag address associated with the surface.
//!
//! @returns    OK if we successfully queried RM for compression information, an appropriate RC value otherwise
//-----------------------------------------------------------------------------
RC ComprZBCTest::getTagsForSurface(Surface2D* surf, UINT32* start, UINT32* end)
{
    LW0041_CTRL_GET_SURFACE_COMPRESSION_COVERAGE_PARAMS params = {0};
    LwRmPtr pLwRm;
    RC rc;

    CHECK_RC(pLwRm->Control(surf->GetMemHandle(),
                LW0041_CTRL_CMD_GET_SURFACE_COMPRESSION_COVERAGE,
                &params,
                sizeof(LW0041_CTRL_GET_SURFACE_COMPRESSION_COVERAGE_PARAMS)));

    if (params.format == 0)
    {
        Printf(Tee::PriHigh, "%s: memory 0x%x did not get compression\n",
                __FUNCTION__, surf->GetMemHandle());
        return RC::DATA_MISMATCH;
    }

    *start = params.lineMin;
    *end = params.lineMax;

    return rc;
}

//!
//! @brief verifyZbcRefCount() - Helper function which queries RM for ZBC refcounts and verifies they are zero.
//!
//! Verifying that the references counts are zero for the range of compression
//! addresses associated with the fast-cleared surfaces is how we verify that the
//! compression flush took place.  If it didn't the reference counts would be
//! non-zero.
//!
//! @param[in] surf
//!            The surface to query
//! @param[in] startComptag
//!             The first compression tag address associated with the surface.
//! @param[out] numLines
//!             The number of comptag lines to iterate over
//!
//! @returns    OK if all refcounts were 0, an appropriate error-code otherwise.
//-----------------------------------------------------------------------------
RC ComprZBCTest::verifyZbcRefCount(UINT32 startComptag, UINT32 numLines)
{
    LwRmPtr pLwRm;
    LW208F_CTRL_FB_GET_ZBC_REFCOUNT_PARAMS refCountParams = {0};
    RC rc = OK;

    for (refCountParams.compTagAddress = startComptag;
            refCountParams.compTagAddress <= startComptag+numLines;
            refCountParams.compTagAddress++)
    {

        CHECK_RC(pLwRm->Control(m_hSubDevDiag,
                LW208F_CTRL_CMD_FB_GET_ZBC_REFCOUNT,
                &refCountParams,
                sizeof(LW208F_CTRL_FB_GET_ZBC_REFCOUNT_PARAMS)));

        for (UINT32 i = 0 ; i < LW208F_CTRL_FB_GET_ZBC_REFCOUNT_MAX_REFCOUNTS; ++i)
        {

            if (refCountParams.zbcRefCount[i] != 0)
            {
                Printf(Tee::PriHigh, "Non-zero ZBC refcount (compTagLine = 0x%x refCount = 0x%x)!\n",
                        refCountParams.compTagAddress, refCountParams.zbcRefCount[i]);
                return RC::DATA_MISMATCH;
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(ComprZBCTest, RmTest,
    "ComprZBCTest RMTEST that tests sysmem compression and ZBC, lwrrently specfic to iGT21A");

