/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2013,2015-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_gpucachetest.cpp
//! \brief To verify basic functionality of new FBBA Cache (lwrrently igt21a
//!  specific) and its resman code paths.
//!
//! Lwrrently includes Basic API testing and PTE fields verificaiton for GPU
//! Cacheable bit. The APIs that are covered are VidHeapControl(),
//! AllocContextDam(), MapMemoryDma().

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string>          // Only use <> for built-in C++ stuff
#include <vector>
#include <algorithm>
#include "class/cl8697.h"
#include "core/utility/errloggr.h"
#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/notifier.h"
#include "lwRmApi.h"
#include "core/include/utility.h"
#include "class/cl007d.h"  // LW04_SOFTWARE_TEST
#include "class/cl0070.h"  // LW01_MEMORY_VIRTUAL
#include "class/cl826f.h"  // G82_CHANNEL_GPFIFO
#include "class/cl5097.h"  // LW50_TESLA
#include "class/cl8697.h"  // GT21A_TESLA
#include "class/cl8697sw.h"
#include "class/cl5097sw.h"
#include "class/cl2080.h"
#include "class/cl86b6.h"  // LW86B6_VIDEO_COMPOSITOR
#include "ctrl/ctrl5080.h"
#include "class/cl5080.h"
#include "class/cl866f.h"
#include "ctrl/ctrl208f.h"
#include "random.h"
#include "gpu/include/notifier.h"
#include "core/include/memcheck.h"

#define MAX_MEM_LIMIT            (1*1024)

#define GPU_CACHE_TEST_FILL_PATTERN 0xffffffff
//
// If this is going to be non-zero then you need to make sure the
// clear-value sent to HW is adjusted
//
#define GPU_CACHE_TEST_CLEAR_PATTERN 0x0

//
// Big enough to give us room to test, small enough for whole thing to fit in
// the cache
//
#define FLUSH_MEM_SIZE           (32*1024)
// We'll need to change this dynamically if we port test to future GPUs.
#define CACHE_LINE_SIZE          (256)

#define NOTIFIER_MAXCOUNT        1             //Maximum count for notifiers for class LW50_TESLA

#define CREATE_FLUSH_TEST(name) \
    static void name (Surface2D* pSurf, LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS* pParams, \
            LW0041_CTRL_SURFACE_FLUSH_GPU_CACHE_PARAMS *pParams0041, RC* rc, bool* bUseCl0041)

typedef void (FillFlushTestParams) (Surface2D*, LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS*,
        LW0041_CTRL_SURFACE_FLUSH_GPU_CACHE_PARAMS*, RC*, bool* );

static FillFlushTestParams flushTestNoType;
static FillFlushTestParams flushTestAddrArrayZero;
static FillFlushTestParams flushTestAddrArrayLimit;
static FillFlushTestParams flushTestMemBlockZero;
static FillFlushTestParams flushTestBadAperture;
static FillFlushTestParams flushTestOverlap;
static FillFlushTestParams flushTestOverlapGrowDown;
static FillFlushTestParams flushTestOverlapAlign;
static FillFlushTestParams flushTestEvictAll;
static FillFlushTestParams flushTestIlwalidateAll;
static FillFlushTestParams flushTestCleanAll;
static FillFlushTestParams flushTestEvictAllocation;
static FillFlushTestParams flushTestIlwalidateAllocation;
static FillFlushTestParams flushTestCleanAllocation;
static FillFlushTestParams flushTestEvictOneLine;
static FillFlushTestParams flushTestCleanOneLine;
static FillFlushTestParams flushTestIlwalidateOneLine;
static FillFlushTestParams flushTestEvict4kPagesLSBGarbage;
static FillFlushTestParams flushTestEvict256BContig;
static FillFlushTestParams flushTestClean256BContig;
static FillFlushTestParams flushTestEvict256BNonContig;
static FillFlushTestParams flushTestIlwalidate256BNonContig;
static FillFlushTestParams flushTestEvict256BContigGrowDown;
static FillFlushTestParams flushTestNoFlush;

FillFlushTestParams* flushTestFuncs[] =
{
    flushTestNoType,
    flushTestAddrArrayZero,
    flushTestAddrArrayLimit,
    flushTestMemBlockZero,
    flushTestBadAperture,
    flushTestOverlap,
    flushTestOverlapGrowDown,
    flushTestOverlapAlign,
    flushTestCleanOneLine,
    flushTestNoFlush,
    flushTestIlwalidateOneLine,
    flushTestEvictOneLine,
    flushTestEvictAll,
    flushTestIlwalidateAll,
    flushTestCleanAll,
    flushTestEvict4kPagesLSBGarbage,
    flushTestEvict256BContig,
    flushTestClean256BContig,
    flushTestEvict256BNonContig,
    flushTestIlwalidate256BNonContig,
    flushTestEvict256BContigGrowDown,
    flushTestEvictAllocation,
    flushTestIlwalidateAllocation,
    flushTestCleanAllocation,
    NULL,
};

static bool pollForZero(void*);
static PHYSADDR getPhysicalAddress(Surface2D*, LwU64);

class GpuCacheTest : public RmTest
{
public:
    GpuCacheTest();
    virtual ~GpuCacheTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    SETGET_PROP(printFlushParams, bool);

private:
    LwRm::Handle     m_hObj;
    LwRm::Handle     m_hObj5080;
    FLOAT64          m_TimeoutMs;
    LwRm::Handle     m_hSemCtxDma;
    LwRm::Handle     m_hSemMem;
    LwU32*           m_pSem;

    RC BasicApiTest();
    RC BasicApiTestForVidMem();
    RC BasicApiTestForSysMem();
    RC BasicAllocPolicyTestForGraphicsEngine();
    RC BasicAllocPolicyTestForVideoEngines();
    RC BasicPromotionPolicyTestForVideoEngines();
    RC AllocMemory
    (
        UINT32            type,
        UINT32            flags,
        UINT32            attr,
        UINT32            attr2,
        UINT64            memSize,
        UINT32          * pHandle
    );

    RC VerifyGpuCacheableBitInPte
    (
        UINT32          memHandle,
        UINT32          expectedValCacheableBit
    );

    RC GetGpuCacheableBitUsingMemHandle
    (
        UINT32          hMemHandle,
        UINT32        * gpuCacheableBit
    );

    RC BasicAllocPolicyTestForEngine
    (
        UINT32  engine
    );

    RC BasicPromotionPolicyTestForVideoEngine
    (
        UINT32  engine
    );

    RC BasicCacheFlushApiTest();

    RC BasicCacheFlushMethodTest();

    Surface2D *m_flushSurf;
    void *m_pFlushSurfDirect;
    bool m_verifyFlushes;
    LwRm::Handle m_subDevDiag;
    LwRm::Handle m_obj3d;

    const static LwU32 m_subCh = 1;

    RC prepareVerifyFlush();
    RC verifyFlush(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS*,
            LW0041_CTRL_SURFACE_FLUSH_GPU_CACHE_PARAMS*);
    RC doHwClear();

    bool m_printFlushParams;
    bool m_bVerifiedCaching;

    void printFlushParams(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS*,
            LW0041_CTRL_SURFACE_FLUSH_GPU_CACHE_PARAMS*, RC);

    void printSurface(Surface2D*, void*);

    LW2080_CTRL_FB_GET_GPU_CACHE_INFO_PARAMS m_cacheInfo;
    Notifier        m_Notifier;
};

//! \brief GpuCacheTest constructor
//!
//! Placeholder : doesnt do much, much funcationality in Setup()
//!
//! \sa Setup
//------------------------------------------------------------------------------
GpuCacheTest::GpuCacheTest()
{
    SetName("GpuCacheTest");
    m_printFlushParams = false;
    m_pFlushSurfDirect = NULL;
    m_flushSurf = NULL;
}

//! \brief GpuCacheTest destructor
//!
//! Placeholder : doesnt do much, much funcationality in Setup()
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
GpuCacheTest::~GpuCacheTest()
{

}

//! \brief IsTestSupported(), Looks for whether test can execute in current elw.
//!
//! Check for GT21A_TESLA class availibility (lwrrently wer are restricting this
//! this test for iGT21A, will add support for fermi later).
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string GpuCacheTest::IsTestSupported()
{
    bool       bIsSupported;

   // Returns true only if the class is supported.
   bIsSupported = (IsClassSupported(GT21A_TESLA) &&
                   IsClassSupported(G82_CHANNEL_GPFIFO) &&
                   IsClassSupported(LW01_MEMORY_VIRTUAL));

   Printf(Tee::PriLow,"%d:GpuCacheTest::IsSupported retured %d \n",
         __LINE__, bIsSupported);
   if(bIsSupported)
       return RUN_RMTEST_TRUE;
   return "One of these GT21A_TESLA, G82_CHANNEL_GPFIFO, LW01_MEMORY_VIRTUAL classes is not supported on current platform";
}

//! \brief  Setup(): Generally used for any test level allocation
//!
//! Allocating the channel and software object for use in the test
//
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC GpuCacheTest::Setup()
{
    RC         rc;
    LwRmPtr    pLwRm;
    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    m_TimeoutMs = m_TestConfig.TimeoutMs();
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh));
    CHECK_RC(m_Notifier.Allocate(m_pCh,
                                 NOTIFIER_MAXCOUNT,
                                 &m_TestConfig));
    CHECK_RC(pLwRm->AllocObject(m_hCh, &m_obj3d, GT21A_TESLA));
    // SW class
    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObj, LW04_SOFTWARE_TEST, NULL));

    CHECK_RC(AllocMemory(LWOS32_TYPE_IMAGE,
                         0,
                         0,
                         0,
                         MAX_MEM_LIMIT,
                         &m_hSemMem));

    CHECK_RC(pLwRm->MapMemory(
                m_hSemMem,
                0, MAX_MEM_LIMIT,
                ((void**)&m_pSem),
                0, GetBoundGpuSubdevice()));

    CHECK_RC(pLwRm->AllocContextDma(
                &m_hSemCtxDma,
                DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE) |
                DRF_DEF(OS03, _FLAGS, _HASH_TABLE, _DISABLE),
                m_hSemMem, 0, MAX_MEM_LIMIT-1));

    // 128x128x4 = 64KB - will fit completely in cache even when floorswept
    m_flushSurf = new Surface2D;
    m_flushSurf->SetWidth(128);
    m_flushSurf->SetHeight(128);
    m_flushSurf->SetColorFormat(ColorUtils::A8R8G8B8);
    m_flushSurf->SetLayout(Surface2D::Pitch);
    m_flushSurf->SetGpuCacheMode(Surface2D::GpuCacheOn);
    m_flushSurf->SetCompressed(false);
    //
    // Has to be sysmem until Bug 333648 is fixed so we can get a direct
    // mapping
    //
    m_flushSurf->SetLocation(Memory::NonCoherent);
    m_flushSurf->BindGpuChannel(m_hCh);

    CHECK_RC(m_flushSurf->Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_flushSurf->Map());

    //
    // VERIF only for LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR
    // SIM_BUILD only because nobody other than this test can touch fb/cache
    // while it is running if m_verifyFlushes is enabled.
    //
#if defined(LW_VERIF_FEATURES) && defined(SIM_BUILD)
    m_verifyFlushes = Platform::GetSimulationMode() != Platform::Fmodel;
#else
    m_verifyFlushes = false;
#endif

    // After this is only setup for verifying flushes
    if (m_verifyFlushes == false)
    {
        return OK;
    }

    CHECK_RC(pLwRm->MapMemory(
                m_flushSurf->GetMemHandle(),
                0, m_flushSurf->GetAllocSize()-1,
                &m_pFlushSurfDirect,
                DRF_DEF(OS33, _FLAGS, _MAPPING, _DIRECT),
                GetBoundGpuSubdevice()));

    m_subDevDiag = GetBoundGpuSubdevice()->GetSubdeviceDiag();

    // Get cache mode
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                LW2080_CTRL_CMD_FB_GET_GPU_CACHE_INFO,
                &m_cacheInfo, sizeof(m_cacheInfo)));

    if (m_cacheInfo.writeMode ==
            LW2080_CTRL_FB_GET_GPU_CACHE_INFO_WRITE_MODE_WRITETHROUGH)
    {
        LW208F_CTRL_FB_CTRL_GPU_CACHE_PARAMS setParams = {0};

        // Set cache mode to write-back
        setParams.writeMode = LW208F_CTRL_FB_CTRL_GPU_CACHE_WRITE_MODE_WRITEBACK;
        CHECK_RC(pLwRm->Control(m_subDevDiag,
                    LW208F_CTRL_CMD_FB_CTRL_GPU_CACHE,
                    &setParams, sizeof(setParams)));
    }

    m_bVerifiedCaching = false;

    return rc;
}

//! \brief Run:: Used generally for placing all the testing stuff.
//!
//! Runs the required tests. Lwrrently we have only Basic API testing in here.
//! Will be adding more tests.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC GpuCacheTest::Run()
{
    RC rc;

    CHECK_RC(BasicApiTest());
    CHECK_RC(BasicCacheFlushMethodTest());

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! Freeing the channel and the software object that we allocated in Setup().
//------------------------------------------------------------------------------
RC GpuCacheTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;
    m_Notifier.Free();
    pLwRm->Free(m_hObj);
    pLwRm->UnmapMemory(m_hSemMem, m_pSem, 0, GetBoundGpuSubdevice());
    pLwRm->Free(m_hSemCtxDma);
    pLwRm->Free(m_hSemMem);

    if (m_verifyFlushes)
    {
        pLwRm->Free(m_obj3d);

        pLwRm->UnmapMemory(m_flushSurf->GetMemHandle(), m_pFlushSurfDirect, 0, GetBoundGpuSubdevice());

        // Restore cache mode
        if (m_cacheInfo.writeMode ==
                LW2080_CTRL_FB_GET_GPU_CACHE_INFO_WRITE_MODE_WRITETHROUGH)
        {
            LW208F_CTRL_FB_CTRL_GPU_CACHE_PARAMS setParams = {0};

            setParams.writeMode =
                LW208F_CTRL_FB_CTRL_GPU_CACHE_WRITE_MODE_WRITETHROUGH;

            CHECK_RC(pLwRm->Control(m_subDevDiag,
                        LW208F_CTRL_CMD_FB_CTRL_GPU_CACHE,
                        &setParams, sizeof(setParams)));
        }
    }

    m_flushSurf->Free();
    delete m_flushSurf;

    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief BasicApiTest()
//!
//! This function calls BasicApiTestForVidMem() and BasicApiTestForSysMem()
//! which run the Basic API and PTE verification tests on vidmem and
//! sysmem respectively.

//! \return OK all the tests passed, specific RC if failed.
//! \sa Run()
//-----------------------------------------------------------------------------
RC GpuCacheTest::BasicApiTest()
{
    RC  rc;

    // Call BasicApiTestForVidMem() for running basic API test on video memory.
    CHECK_RC(BasicApiTestForVidMem());

    // Call BasicApiTestForSysMem() for running basic API test on system memory.
    CHECK_RC(BasicApiTestForSysMem());

    //
    // Call BasicAllocPolicyTestForGraphicsEngine() for running basic gpu cache
    // alloc policy API tests on graphics engine.
    //
    CHECK_RC(BasicAllocPolicyTestForGraphicsEngine());

    //
    // Call BasicAllocPolicyTestForVideoEngines() for running basic gpu cache
    // alloc policy API tests on video engines.
    //
    CHECK_RC(BasicAllocPolicyTestForVideoEngines());
    CHECK_RC(BasicPromotionPolicyTestForVideoEngines());

    CHECK_RC(BasicCacheFlushApiTest());

    Printf(Tee::PriHigh,"%d:GpuCacheTest:: BasicApiTest success.\n",__LINE__);

    return rc;
}

//! \brief BasicApiTestForVidMem()
//!
//! This function tests the following APIs lwrrently  for vidmem allocations:
//! VidHeapControl(), AllocContextDam(), MapMemoryDma().
//! It also verifies whether the GPU_CACHEABLE bit in the PTE is properly
//! set/unset for respective allocations.
//! The following verifications are made for PTEs:
//!  a. Allocate Cached   vidmem and verify GPU_CACHEABLE bit in PTE.
//!  b. Allocate uncached vidmem and verify GPU_CACHEABLE bit in PTE.
//!  c. Allocate vidmem with value _DEFAULT for cacheable and verify
//!     GPU_CACHEABLE bit in PTE.

//! \return OK all the tests passed, specific RC if failed,
//! \sa Run()
//-----------------------------------------------------------------------------
RC GpuCacheTest::BasicApiTestForVidMem()
{
    LwRmPtr          pLwRm;
    RC               rc;
    UINT32           attr, attr2;
    LwRm::Handle     hVidMemHandle = 0;

    // Test 1 : Allocate Cached vidmem and verify GPU_CACHEABLE bit in PTE.
    Printf(Tee::PriLow,"%d:GpuCacheTest:: BasicApiTestForVidMem Test 1: Will allocate cached vidmem and verify PTE.\n",__LINE__);

    // attr = _LOCATION_VIDMEM for this allocation
    attr  = DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM);

    // attr2 = _GPU_CACHEABLE_YES for this allocation
    attr2 = DRF_DEF(OS32, _ATTR2, _GPU_CACHEABLE, _YES);

    // Allocate this type of memory
    CHECK_RC(AllocMemory(LWOS32_TYPE_IMAGE,
                         LWOS32_ALLOC_FLAGS_TURBO_CIPHER_ENCRYPTED,
                         attr,
                         attr2,
                         MAX_MEM_LIMIT,
                         &hVidMemHandle));

    // Verify GPU CACHEABLE BIT says this allocation is cacheable
    CHECK_RC_CLEANUP(VerifyGpuCacheableBitInPte(hVidMemHandle,
                                                LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_GPU_CACHED_FALSE));

    Printf(Tee::PriLow,"%d:GpuCacheTest:: BasicApiTestForVidMem Test 1: Verified PTE has cached bit set for cached vidmem allocation.\n",__LINE__);

    // Now free the allocated memory.
    pLwRm->Free(hVidMemHandle);
    hVidMemHandle = 0;

    // Test 2 : Allocate uncached vidmem and verify GPU_CACHEABLE bit in PTE.
    Printf(Tee::PriLow,"%d:GpuCacheTest:: BasicApiTestForVidMem Test 2: Will allocate uncached vidmem and verify PTE.\n",__LINE__);

    // attr = _LOCATION_VIDMEM for this allocation
    attr  = DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM);

    // attr2 = _GPU_CACHEABLE_NO for this allocation
    attr2 = DRF_DEF(OS32, _ATTR2, _GPU_CACHEABLE, _NO);

    // Allocate this type of memory
    CHECK_RC(AllocMemory(LWOS32_TYPE_IMAGE,
                         LWOS32_ALLOC_FLAGS_TURBO_CIPHER_ENCRYPTED,
                         attr,
                         attr2,
                         MAX_MEM_LIMIT,
                         &hVidMemHandle));

    // Verify GPU CACHEABLE BIT says this allocation is uncacheable
    CHECK_RC_CLEANUP(VerifyGpuCacheableBitInPte(hVidMemHandle,
                                                LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_GPU_CACHED_TRUE));

    Printf(Tee::PriLow,"%d:GpuCacheTest:: BasicApiTestForVidMem Test 2: Verified PTE has cached bit not set for uncached vidmem allocation.\n",__LINE__);

    // Now free the allocated memory.
    pLwRm->Free(hVidMemHandle);
    hVidMemHandle = 0;

    // Test 3 : Allocate vidmem with value _DEFAULT for cacheable and verify GPU_CACHEABLE bit in PTE.
    Printf(Tee::PriLow,"%d:GpuCacheTest:: BasicApiTestForVidMem Test 3: Will allocate vidmem with value _DEFAULT for cacheable and verify PTE.\n",__LINE__);

    // attr = _LOCATION_VIDMEM for this allocation
    attr  = DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM);

    // attr2 = _GPU_CACHEABLE_DEFAULT for this allocation
    attr2 = DRF_DEF(OS32, _ATTR2, _GPU_CACHEABLE, _DEFAULT);

    // Allocate this type of memory
    CHECK_RC(AllocMemory(LWOS32_TYPE_IMAGE,
                         0,
                         attr,
                         attr2,
                         MAX_MEM_LIMIT,
                         &hVidMemHandle));
    //
    // Verify GPU CACHEABLE BIT says this allocation is cacheable
    // CACHEABLE_DEFAULT translates to CACHEABLE_YES for vidmem
    //
    CHECK_RC_CLEANUP(VerifyGpuCacheableBitInPte(hVidMemHandle,
                                                LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_GPU_CACHED_FALSE));

    Printf(Tee::PriLow,"%d:GpuCacheTest:: BasicApiTestForVidMem Test 3: Verified PTE has cached bit set for cached = _DEFAULT vidmem allocation.\n",__LINE__);

    Printf(Tee::PriHigh,"%d:GpuCacheTest:: BasicApiTestForVidMem success.\n",__LINE__);

    // Now go down the Cleanup path to free any resources and return
Cleanup:
    pLwRm->Free(hVidMemHandle);
    return rc;
}

//! \brief BasicApiTestForSysMem()
//!
//! This function tests the following APIs lwrrently for sysmem allocations:
//! VidHeapControl(), AllocContextDam(), MapMemoryDma().
//! It also verifies whether the GPU_CACHEABLE bit in the PTE is properly
//! set/unset for respective allocations.
//! The following verifications are made for PTEs:
//!  a. Allocate Cached   sysmem and verify GPU_CACHEABLE bit in PTE.
//!  b. Allocate uncached sysmem and verify GPU_CACHEABLE bit in PTE.
//!  c. Allocate sysmem with value _DEFAULT for cacheable and verify
//!     GPU_CACHEABLE bit in PTE.

//! \return OK all the tests passed, specific RC if failed,
//! \sa Run()
//-----------------------------------------------------------------------------
RC GpuCacheTest::BasicApiTestForSysMem()
{

    LwRmPtr          pLwRm;
    RC               rc;
    UINT32           attr, attr2;
    LwRm::Handle     hSysMemHandle = 0;

    // Test 1 : Allocate Cached sysmem and verify GPU_CACHEABLE bit in PTE.
    Printf(Tee::PriLow,"%d:GpuCacheTest:: BasicApiTestForSysMem Test 1: Will allocate cached sysmem and verify PTE.\n",__LINE__);

    // attr = _LOCATION_PCI for this allocation
    attr  = DRF_DEF(OS32, _ATTR, _LOCATION, _PCI);

    // attr2 = _GPU_CACHEABLE_YES for this allocation
    attr2 = DRF_DEF(OS32, _ATTR2, _GPU_CACHEABLE, _YES);

    // Allocate this type of memory
    CHECK_RC(AllocMemory(LWOS32_TYPE_IMAGE,
                         LWOS32_ALLOC_FLAGS_TURBO_CIPHER_ENCRYPTED,
                         attr,
                         attr2,
                         MAX_MEM_LIMIT,
                         &hSysMemHandle));

    // Verify GPU CACHEABLE BIT says this allocation is cacheable
    CHECK_RC_CLEANUP(VerifyGpuCacheableBitInPte(hSysMemHandle,
                                                LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_GPU_CACHED_FALSE));

    Printf(Tee::PriLow,"%d:GpuCacheTest:: BasicApiTestForSysMem Test 1: Verified PTE has cached bit set for cached sysmem allocation.\n",__LINE__);

    // Now free the allocated memory.
    pLwRm->Free(hSysMemHandle);

    // Test 2 : Allocate uncached sysmem and verify GPU_CACHEABLE bit in PTE.
    Printf(Tee::PriLow,"%d:GpuCacheTest:: BasicApiTestForSysMem Test 2: Will allocate uncached sysmem and verify PTE.\n",__LINE__);

    // attr = _LOCATION_PCI for this allocation
    attr  = DRF_DEF(OS32, _ATTR, _LOCATION, _PCI);

    // attr2 = _GPU_CACHEABLE_NO for this allocation
    attr2 = DRF_DEF(OS32, _ATTR2, _GPU_CACHEABLE, _NO);

    // Allocate this type of memory
    CHECK_RC(AllocMemory(LWOS32_TYPE_IMAGE,
                         LWOS32_ALLOC_FLAGS_TURBO_CIPHER_ENCRYPTED,
                         attr,
                         attr2,
                         MAX_MEM_LIMIT,
                         &hSysMemHandle));

    // Verify GPU CACHEABLE BIT says this allocation is uncacheable
    CHECK_RC_CLEANUP(VerifyGpuCacheableBitInPte(hSysMemHandle,
                                                LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_GPU_CACHED_TRUE));

    Printf(Tee::PriLow,"%d:GpuCacheTest:: BasicApiTestForSysMem Test 2: Verified PTE has cached bit not set for uncached sysmem allocation.\n",__LINE__);

    // Now free the allocated memory.
    pLwRm->Free(hSysMemHandle);
    hSysMemHandle = 0;

    // Test 3 : Allocate sysmem with value _DEFAULT for cacheable and verify GPU_CACHEABLE bit in PTE.
    Printf(Tee::PriLow,"%d:GpuCacheTest:: BasicApiTestForSysMem Test 3: Will allocate sysmem with value _DEFAULT for cacheable and verify PTE.\n",__LINE__);

    // attr = _LOCATION_PCI for this allocation
    attr  = DRF_DEF(OS32, _ATTR, _LOCATION, _PCI);

    // attr2 = _GPU_CACHEABLE_DEFAULT for this allocation
    attr2 = DRF_DEF(OS32, _ATTR2, _GPU_CACHEABLE, _DEFAULT);

    // Allocate this type of memory
    CHECK_RC(AllocMemory(LWOS32_TYPE_IMAGE,
                         0,
                         attr,
                         attr2,
                         MAX_MEM_LIMIT,
                         &hSysMemHandle));
    //
    // Verify GPU CACHEABLE BIT says this allocation is uncacheable
    // CACHEABLE_DEFAULT translates to CACHEABLE_NO for sysmem
    //
    CHECK_RC_CLEANUP(VerifyGpuCacheableBitInPte(hSysMemHandle,
                                                LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_GPU_CACHED_TRUE));

    Printf(Tee::PriLow,"%d:GpuCacheTest:: BasicApiTestForSysMem Test 3: Verified PTE has cached bit not set for cached = _DEFAULT sysmem allocation.\n",__LINE__);

    Printf(Tee::PriHigh,"%d:GpuCacheTest:: BasicApiTestForSysMem success.\n",__LINE__);

    // Now go down the Cleanup path to free any resources and return
Cleanup:
    pLwRm->Free(hSysMemHandle);
    return rc;
}

//! \brief VerifyGpuCacheableBitInPte()
//!
//! This function uses the memory handle given by the caller to get the value
//! of GPU cacheable bit for this allocated memory by calling the
//! GetGpuCacheableBitUsingMemHandle() function. It then compares the
//! expected value given by the caller against the actual value of GPU_CACHEABLE
//! bit returned by the function GetGpuCacheableBitUsingMemHandle().
//! If both the values are same, returns OK, else returns RC::DATA_MISMATCH.
//! If the GetGpuCacheableBitUsingMemHandle() call failed unexpectedly, then
//! returns the specific RC why this failed.
//!
//! \return OK if expected value and alwtal value of GPU_CACHEABLE bit match,
//! RC::DATA_MISMATCH if they dont, specific rc if any other error oclwred.
//! \sa BasicApiTest()
//-----------------------------------------------------------------------------
RC GpuCacheTest::VerifyGpuCacheableBitInPte
(
    UINT32          memHandle,
    UINT32          expectedValGpuCacheableBit
)
{
    RC                rc;
    UINT32            actualValGpuCacheableBit;

    // Get the actual value of the GPU cacheable bit for this memory allocation
    CHECK_RC(GetGpuCacheableBitUsingMemHandle(memHandle,
                                              &actualValGpuCacheableBit));
    //
    // if actual value doesnt match with the expected value sent to us
    // by the caller, then return RC::DATA_MISMATCH
    //
    if (actualValGpuCacheableBit != expectedValGpuCacheableBit)
    {
        Printf(Tee::PriHigh,"%d:GpuCacheTest::Actual value of GPU_CACHEABLE bit does not match with expected value.\n",__LINE__);
        return RC::DATA_MISMATCH;
    }
    return rc;
}

//! \brief GetGpuCacheableBitUsingMemHandle()
//!
//! This function uses the memory handle given by the caller to do MapMemoryDma()
//! and get get a GPU virtual addr of the memory. It then fetches the PTE
//! information for this GPU virtual address, extracts the GPU_CACHED bit from
//! it and saves it in the ptr *gpuCacheableBit.
//!
//! \return OK if successfully fetched PTE information and extracted GPU_CACHED
//! bit, specific rc if failed.
//! \sa VerifyGpuCacheableBitInPte()
//-----------------------------------------------------------------------------
RC GpuCacheTest::GetGpuCacheableBitUsingMemHandle
(
    UINT32          hMemHandle,
    UINT32        * gpuCacheableBit
)
{
    RC                rc;
    LwRmPtr           pLwRm;
    LwRm::Handle      hVA = 0;
    UINT64            gpuAddr = 0;
    LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS  getParams;

    memset(&getParams, 0, sizeof(getParams));

    // Allocate a mappable MV object.
    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    // Do MapMemoryDma() and get a GPU virtual address for
    // the hMemHandle passed by the client
    //
    CHECK_RC_CLEANUP(pLwRm->MapMemoryDma(hVA,
                                         hMemHandle,
                                         0,
                                         MAX_MEM_LIMIT,
                                         DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE),
                                         &gpuAddr,
                                         GetBoundGpuDevice()));

    // Get PTE info for this gpuAddr through GET_PTE_INFO ctrl cmd.
    getParams.gpuAddr = gpuAddr;
    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                    LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                                    &getParams,
                                    sizeof (getParams)));

    // Extract  and save the value of GPU_CACHED bit
    *gpuCacheableBit  = DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_GPU_CACHED, getParams.pteBlocks[0].pteFlags);

    // Now go down the Cleanup path, do cleanup and return.
Cleanup:
    if (hVA && gpuAddr)
    {
        pLwRm->UnmapMemoryDma(hVA,
                              hMemHandle,
                              LW04_MAP_MEMORY_FLAGS_NONE,
                              gpuAddr,
                              GetBoundGpuDevice());
    }
    pLwRm->Free(hVA);
    return rc;
}

//! \brief AllocMemory()
//!
//! This function allocates the memory as per the input params type,
//! flags, attr, attr2, memsize.
//! Also verifies cacheable bit in the return params : what we
//! requested sysmem/vidmem , caching required/not required/default
//! against what was actually allocated.
//! If allocation successful, *(pHandle) will contain the handle to the
//! allocated space for callers use.
//!
//! \return OK if allocation and verification of cacheable bit passed, specific
//! RC if allocation failed,and RC::PARAMETER_MISMATCH if allocation successful
//! but cacheable bit not in accordance of what it should be.
//! \sa BasicApiTest()
//-----------------------------------------------------------------------------
RC GpuCacheTest::AllocMemory
(
    UINT32            type,
    UINT32            flags,
    UINT32            attr,
    UINT32            attr2,
    UINT64            memSize,
    UINT32          * pHandle
)
{
    RC                rc;
    LwRmPtr           pLwRm;
    UINT32            attrSaved, attr2Saved;
    UINT32            requestedCacheable, allocatedCacheable;

    //
    // Save attr and attr2 before trying allocation as we have to pass ptr of
    // attr and attr2 for allocation and the values change to what gets allocated.
    //
    attrSaved       = attr;
    attr2Saved      = attr2;

    // Try to allocate what caller requested.
    CHECK_RC(pLwRm->VidHeapAllocSizeEx(type,   // type
                                       flags,  // flags
                                       &attr,  // pAttr
                                       &attr2, // pAttr2
                                       memSize,// Size
                                       1,      // alignment
                                       NULL,   // pFormat
                                       NULL,   // pCoverage
                                       NULL,   // pPartitionStride
                                       pHandle,// pHandle
                                       NULL,   // poffset,
                                       NULL,   // pLimit
                                       NULL,   // pRtnFree
                                       NULL,   // pRtnTotal
                                       0,      // width
                                       0,      // height
                                       GetBoundGpuDevice()));

    // Now verify the GPU CACHEABLE bit in return params.

    // extract what we requested for GPU_CACHEABLE_BIT
    requestedCacheable = DRF_VAL(OS32, _ATTR2, _GPU_CACHEABLE, attr2Saved);

    // extract what got allocated for GPU_CACHEABLE_BIT
    allocatedCacheable = DRF_VAL(OS32, _ATTR2, _GPU_CACHEABLE, attr2);

    // if vidmem was requested
    if (DRF_VAL(OS32, _ATTR, _LOCATION, attrSaved) == LWOS32_ATTR_LOCATION_VIDMEM)
    {
        // if CACHEABLE_YES got allocated, check whether we requested for it.
        if (allocatedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_YES)
        {
            // for vidmem, CACHEABLE_YES is allocated by default or if we ask for it.
            if ( requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_YES ||
                 requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_DEFAULT )
            {
                // Everything is fine, print we allocated cacheable vidmem
                Printf(Tee::PriLow,"%d:GpuCacheTest::Allocated cacheable vidmem.\n",__LINE__);
            }
            else
            {
                //
                // We have hit this else, so CACHEABLE_YES got allocated even
                // though we asked not to. So print error and return.
                //
                Printf(Tee::PriHigh,"%d:GpuCacheTest::Error : Return parameters mismatch with requested params,\
                                     Requested %s allocation for vidmem, got %s.\n",
                       __LINE__, "GPU_CACHEABLE_NO", "GPU_CACHEABLE_YES");
                CHECK_RC_CLEANUP(RC::PARAMETER_MISMATCH);
            }
        }
        // if CACHEABLE_NO got allocated, check what we had requested.
        else if (allocatedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_NO)
        {
            if (requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_NO)
            {
                //
                // Everything is fine, we had requested CACHEABLE_NO and thats
                // what got allocated.
                //
                Printf(Tee::PriLow,"%d:GpuCacheTest::Allocated non-cacheable vidmem.\n",__LINE__);
            }
            else
            {
                //
                // We have hit this else, so CACHEABLE_NO got allocated even
                // though we asked for CACHEABLE_YES or CACHEABLE_DEFAULT
                // _DEFAULT translates to _YES for vidmem.
                //
                Printf(Tee::PriHigh,"%d:GpuCacheTest::Error : Return parameters mismatch with requested params,\
                                     Requested %s allocation for vidmem, got %s.\n",
                       __LINE__,
                       requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_YES ? "GPU_CACHEABLE_YES" : "GPU_CACHEABLE_DEFAULT",
                       "GPU_CACHEABLE_NO");
                CHECK_RC_CLEANUP(RC::PARAMETER_MISMATCH);
            }
        }
        else
        {
            //
            // Hitting this else, that means the API returned _CACHEABLE_DEFAULT.
            // The API should only return CACHEABLE_YES or CACHEABLE_NO to indicate
            // what was allocated. _DEFAULT should never be returned. So print error.
            //
            Printf(Tee::PriHigh,"%d:GpuCacheTest::Error : Got GPU_CACHEABLE_DEFAULT in return params,\
                                 it should be either GPU_CACHEABLE_YES or GPU_CACHEABLE_NO.\n",
                   __LINE__);
            CHECK_RC_CLEANUP(RC::PARAMETER_MISMATCH);

        }
    }
    //
    // if _LOCATION is not vidmem, the rest all(_PCI/AGP/ANY) translate to sysmem.
    // so go down the else path and verify cacheable bit according to what should
    // be allocated for sysmem.
    //
    else
    {
        // if CACHEABLE_YES got allocated, check whether we requested for it.
        if (allocatedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_YES)
        {
            //
            // for sysmem, CACHEABLE mem is only allocated if we specifically ask for it.
            // CACHEABLE_DEFAULT translates to CACHEABLE_NO for sysmem.
            //
            if (requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_YES)
            {
                // Everything is fine, we got CACHEABLE_YES since we asked for it.
                Printf(Tee::PriLow,"%d:GpuCacheTest::Allocated cacheable sysmem.\n",__LINE__);
            }
            else
            {
                //
                // We have hit this else, so CACHEABLE_YES got allocated even though we asked
                // for _DEFAULT or _NO. _DEFAULT should translate to _NO for sysmem.
                // So print error and return.
                //
                Printf(Tee::PriHigh,"%d:GpuCacheTest::Error : Return parameters mismatch with requested params,\
                                     Requested %s allocation for sysmem, got %s.\n",
                       __LINE__,
                       requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_NO ? "GPU_CACHEABLE_NO" : "GPU_CACHEABLE_DEFAULT",
                       "GPU_CACHEABLE_YES");
                CHECK_RC_CLEANUP(RC::PARAMETER_MISMATCH);
            }
        }
        // if CACHEABLE_NO got allocated, check what we had requested.
        else if (allocatedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_NO)
        {
            // if CACHEABLE_NO or CACHEABLE_DEFAULT was requested.
            if (requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_NO ||
                requestedCacheable == LWOS32_ATTR2_GPU_CACHEABLE_DEFAULT)
            {
                // Everything is fine, we got CACHEABLE_NO because we asked for _NO or _DEFAULT.
                Printf(Tee::PriLow,"%d:GpuCacheTest::Allocated non-cacheable sysmem.\n",__LINE__);
            }
            else
            {
                //
                // We have hit this else, so CACHEABLE_NO got allocated even though we asked
                // for CACHEABLE_YES. So print error and return.
                //
                Printf(Tee::PriHigh,"%d:GpuCacheTest::Error : Return parameters mismatch with requested params,\
                                     Requested %s allocation for sysmem, got %s.\n",
                       __LINE__,
                       "GPU_CACHEABLE_YES",
                       "GPU_CACHEABLE_NO");
                CHECK_RC_CLEANUP(RC::PARAMETER_MISMATCH);
            }
        }
        else
        {
            //
            // Hitting this else, that means the API returned _CACHEABLE_DEFAULT.
            // The API should only return CACHEABLE_YES or CACHEABLE_NO to indicate
            // what was allocated. _DEFAULT should never be returned. So print error.
            //
            Printf(Tee::PriHigh,"%d:GpuCacheTest::Error : Got GPU_CACHEABLE_DEFAULT in return params,\
                                 it should be either GPU_CACHEABLE_YES or GPU_CACHEABLE_NO.\n",
                   __LINE__);
            CHECK_RC_CLEANUP(RC::PARAMETER_MISMATCH);
        }
    }
    return rc;

Cleanup:
    pLwRm->Free(*pHandle);
    *pHandle = 0;
    return rc;

}

//! \brief BasicAllocPolicyTestForGraphicsEngine()
//!
//! This function first checks for supporting graphics engine classes
//! then checks to see if gpu cache alloc policy settings are
//! availble, and if so, tests the rmcontrol and deferred API paths
//! to those controls.
//!
//! \sa BasicAllocPolicyTestForGraphicsEngine()
//-----------------------------------------------------------------------------
RC GpuCacheTest::BasicAllocPolicyTestForGraphicsEngine()
{
    RC      rc;
    if (IsClassSupported(GT21A_TESLA))
    {
        Printf(Tee::PriLow,"GpuCacheTest::BasicAllocPolicyTestForGraphicsEngine - Class GT21A_TESLA supported, testing LW2080_ENGINE_TYPE_GRAPHICS\n");
        CHECK_RC(BasicAllocPolicyTestForEngine(LW2080_ENGINE_TYPE_GRAPHICS));
    }
    return OK;
}

//!
//! This function checks for supporting video engine classes and then
//! tests various paths to the gpu cache alloc policy controls by calling
//! BasicApiTestForVideoEngine for related engines.
//!
//! \return OK all the tests passed, specific RC if failed,
//! \sa BasicAllocPolicyTestForVideoEngines()
//-----------------------------------------------------------------------------
RC GpuCacheTest::BasicAllocPolicyTestForVideoEngines()
{
    RC      rc;

    if (IsClassSupported(LW86B6_VIDEO_COMPOSITOR))
    {
        Printf(Tee::PriLow,"GpuCacheTest::BasicAllocPolicyTestForVideoEngines - Class LW86B6_VIDEO_COMPOSITOR supported, testing LW2080_ENGINE_TYPE_VIC\n");
        CHECK_RC(BasicAllocPolicyTestForEngine(LW2080_ENGINE_TYPE_VIC));
    }

    return rc;
}

//! \brief BasicAllocPolicyTestForEngine()
//!
//! This function first checks to see if gpu cache alloc policy settings are
//! availble, and if so, tests the rmcontrol and deferred API paths
//! to those controls.
//!
//! \sa BasicAllocPolicyTestForEngine()
//-----------------------------------------------------------------------------
RC GpuCacheTest::BasicAllocPolicyTestForEngine
(
    UINT32  engine
)
{
    RC      rc;
    LwRmPtr pLwRm;
    UINT32  i, j;
    UINT32  originalGpuCacheAllocPolicy[3];
    UINT32  status;

    LW5080_CTRL_DEFERRED_API_V2_PARAMS params = {0};
    LW2080_CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2_PARAMS  gpuCacheAllocParams = {0};

    UINT32 allocPolicies[] =
    {
        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _READS,        _ENABLE)  |
        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _WRITES,       _ENABLE)  |
        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _READS_ALLOW,  _NO)      |
        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _WRITES_ALLOW, _NO),

        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _READS,        _ENABLE)  |
        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _WRITES,       _ENABLE)  |
        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _READS_ALLOW,  _NO)      |
        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _WRITES_ALLOW, _YES),

        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _READS,        _ENABLE)  |
        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _WRITES,       _ENABLE)  |
        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _READS_ALLOW,  _YES)     |
        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _WRITES_ALLOW, _NO),

        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _READS,        _ENABLE)  |
        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _WRITES,       _ENABLE)  |
        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _READS_ALLOW,  _YES)     |
        DRF_DEF(2080, _CTRL_FB_GPU_CACHE_ALLOC_POLICY_V2, _WRITES_ALLOW, _YES),
    };

    switch (engine)
    {
    case LW2080_ENGINE_TYPE_GRAPHICS:
        gpuCacheAllocParams.count = 3;
        gpuCacheAllocParams.entry[0].client = LW2080_CLIENT_TYPE_TEX;
        gpuCacheAllocParams.entry[1].client = LW2080_CLIENT_TYPE_COLOR;
        gpuCacheAllocParams.entry[2].client = LW2080_CLIENT_TYPE_DEPTH;
    case LW2080_ENGINE_TYPE_BSP:
        gpuCacheAllocParams.count = 1;
        gpuCacheAllocParams.entry[0].client = LW2080_CLIENT_TYPE_MSVLD;
        break;
    case LW2080_ENGINE_TYPE_VP:
        gpuCacheAllocParams.count = 1;
        gpuCacheAllocParams.entry[0].client = LW2080_CLIENT_TYPE_MSPDEC;
        break;
    case LW2080_ENGINE_TYPE_PPP:
        gpuCacheAllocParams.count = 1;
        gpuCacheAllocParams.entry[0].client = LW2080_CLIENT_TYPE_MSPPP;
        break;
    case LW2080_ENGINE_TYPE_VIC:
        gpuCacheAllocParams.count = 1;
        gpuCacheAllocParams.entry[0].client = LW2080_CLIENT_TYPE_VIC;
        break;
    default:
        Printf(Tee::PriHigh,"GpuCacheTest::BasicAllocPolicyTestForEngine - unknown engine (%08x), failing.\n",
               engine);
        return RC::LWRM_ILWALID_ARGUMENT;
    }

    m_hObj5080 = 0;

    // Save off the current gpu cache alloc policy for this engine so we can restore later.
    status = LwRmControl(pLwRm->GetClientHandle(), pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                        LW2080_CTRL_CMD_FB_GET_GPU_CACHE_ALLOC_POLICY_V2,
                        &gpuCacheAllocParams,
                        sizeof (gpuCacheAllocParams));

    // If we dont support this control (not all engines do), we should _not_ fail the test.
    if (status == LW_ERR_NOT_SUPPORTED)
    {
        Printf(Tee::PriLow,"GpuCacheTest::BasicAllocPolicyTestForEngine - LW2080_CTRL_CMD_FB_GET_GPU_CACHE_ALLOC_POLICY_V2 not supported, skipping test.\n");
        return rc;
    }

    for (j = 0; j < gpuCacheAllocParams.count; j++)
    {
        originalGpuCacheAllocPolicy[j] = gpuCacheAllocParams.entry[j].allocPolicy;
    }

    Printf(Tee::PriLow,"GpuCacheTest::BasicAllocPolicyTestForEngine - Testing gpu cache alloc policy setting via rmcontrol path...\n");

    // Test the rmcontrol api path to the gpu cache allocation policy controls
    for (i = 0; i < sizeof(allocPolicies)/sizeof(UINT32); i++)
    {
        for (j = 0; j < gpuCacheAllocParams.count; j++)
        {
            gpuCacheAllocParams.entry[j].allocPolicy = allocPolicies[i];
        }

        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                        LW2080_CTRL_CMD_FB_SET_GPU_CACHE_ALLOC_POLICY_V2,
                                        &gpuCacheAllocParams,
                                        sizeof (gpuCacheAllocParams)));

        // Get the updated value and compare.
        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                            LW2080_CTRL_CMD_FB_GET_GPU_CACHE_ALLOC_POLICY_V2,
                                            &gpuCacheAllocParams,
                                            sizeof (gpuCacheAllocParams)));

        for (j = 0; j < gpuCacheAllocParams.count; j++)
        {
            if (gpuCacheAllocParams.entry[j].allocPolicy != allocPolicies[i])
            {
                // That failed, so spew something and fail
                Printf(Tee::PriHigh,"GpuCacheTest::BasicAllocPolicyTestForEngine - Value set via rmcontrol (%08x) does not match rmcontrol retrieved value (%08x), failing.\n",
                       allocPolicies[i], gpuCacheAllocParams.entry[j].allocPolicy);
                CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
            }
        }
    }

    //
    // Test the deferred api path to the gpu cache allocation policy controls.
    // If we made it this far, it should be supported, fail if not.
    //
    if (!IsClassSupported(LW50_DEFERRED_API_CLASS))
    {
        Printf(Tee::PriHigh,"GpuCacheTest::BasicAllocPolicyTestForEngine - LW50_DEFERRED_API_CLASS not supported, failing test.\n");
        CHECK_RC_CLEANUP(RC::LWRM_INSUFFICIENT_RESOURCES);
    }

    Printf(Tee::PriLow,"GpuCacheTest::BasicAllocPolicyTestForEngine - Testing gpu cache alloc policy setting via class 5080 deferred API path...\n");

    CHECK_RC_CLEANUP(pLwRm->Alloc(m_hCh, &m_hObj5080, LW50_DEFERRED_API_CLASS, NULL));

    for (i = 0; i < sizeof(allocPolicies)/sizeof(UINT32); i++)
    {
        // Setup the deferred api bundle
        params.hApiHandle = 0x10101010;
        params.cmd        = LW2080_CTRL_CMD_FB_SET_GPU_CACHE_ALLOC_POLICY_V2;

        params.api_bundle.CacheAllocPolicy.count = gpuCacheAllocParams.count;
        for (j = 0; j < gpuCacheAllocParams.count; j++)
        {
            params.api_bundle.CacheAllocPolicy.entry[j].client = gpuCacheAllocParams.entry[j].client;
            params.api_bundle.CacheAllocPolicy.entry[j].allocPolicy = allocPolicies[i];
        }

        CHECK_RC_CLEANUP(pLwRm->Control(m_hObj5080, LW5080_CTRL_CMD_DEFERRED_API_V2,
                                (void*)&params, sizeof(params)));

        // Trigger the deferred API bundle via the pushbuffer calls
        m_pCh->SetObject(0, m_hObj5080);
        m_pCh->Write(0, LW5080_DEFERRED_API, params.hApiHandle);

        m_pCh->Flush();
        CHECK_RC(m_pCh->SetObject(0, m_obj3d));
        m_pCh->Write(0, LW8697_SET_OBJECT, m_obj3d);
        CHECK_RC(m_Notifier.Instantiate(0, LW8697_SET_CONTEXT_DMA_NOTIFY));
        m_Notifier.Clear(LW8697_NOTIFIERS_NOTIFY);
        CHECK_RC(m_Notifier.Write(0));
        CHECK_RC(m_pCh->Write(0, LW8697_NO_OPERATION, 0));
        CHECK_RC(m_pCh->Flush());
        CHECK_RC(m_Notifier.Wait(LW8697_NOTIFIERS_NOTIFY, m_TimeoutMs));
        m_pCh->Flush();

        // Get the updated value (via our verified rmcontrol path) and compare.
        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                LW2080_CTRL_CMD_FB_GET_GPU_CACHE_ALLOC_POLICY_V2,
                                &gpuCacheAllocParams,
                                sizeof (gpuCacheAllocParams)));

        for (j = 0; j < gpuCacheAllocParams.count; j++)
        {
            if (gpuCacheAllocParams.entry[j].allocPolicy != allocPolicies[i])
            {
                // That failed, so spew something
                Printf(Tee::PriHigh,"GpuCacheTest::BasicAllocPolicyTestForEngine - Value set via deferred api (%08x) does not match rmcontrol retrieved value (%08x), failing.\n",
                       allocPolicies[i], gpuCacheAllocParams.entry[j].allocPolicy);
                CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
            }
        }
    }

Cleanup:

    if (m_hObj5080)
    {
        pLwRm->Free(m_hObj5080);
    }

    // Restore the original gpu cache alloc policy for this engine.
    for (j = 0; j < gpuCacheAllocParams.count; j++)
    {
        gpuCacheAllocParams.entry[j].allocPolicy = originalGpuCacheAllocPolicy[j];
    }

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_FB_SET_GPU_CACHE_ALLOC_POLICY_V2,
                            &gpuCacheAllocParams,
                            sizeof (gpuCacheAllocParams)));
    return rc;
}

//! \brief BasicPromotionPolicyTestForVideoEngines()
//!
//! This function checks for supporting video engine classes and then
//! tests various paths to the gpu cache promotion policy controls by calling
//! BasicPromotionPolicyTestForVideoEngine for related engines.
//!
//! \return OK all the tests passed, specific RC if failed,
//! \sa BasicPromotionPolicyTestForVideoEngines()
//-----------------------------------------------------------------------------
RC GpuCacheTest::BasicPromotionPolicyTestForVideoEngines()
{
    RC      rc;

    if (IsClassSupported(LW86B6_VIDEO_COMPOSITOR))
    {
        Printf(Tee::PriLow,"%s - Class LW86B6_VIDEO_COMPOSITOR supported, testing LW2080_ENGINE_TYPE_VIC\n",
                __FUNCTION__);
        CHECK_RC(BasicPromotionPolicyTestForVideoEngine(LW2080_ENGINE_TYPE_VIC));
    }

    return rc;
}

//! \brief BasicPromotionPolicyTestForVideoEngine()
//!
//! This function first checks to see if gpu cache promotion policy settings are
//! availble, and if so, tests the rmcontrol and deferred API paths
//! to those controls.
//!
//! \sa BasicPromotionPolicyTestForVideoEngine()
//-----------------------------------------------------------------------------
RC GpuCacheTest::BasicPromotionPolicyTestForVideoEngine
(
    UINT32  engine
)
{
    RC      rc;
    LwRmPtr pLwRm;
    UINT32  i;
    UINT32  originalGpuCachePromotionPolicy;
    UINT32  status;

    LW5080_CTRL_DEFERRED_API_PARAMS params = {0};
    LW2080_CTRL_FB_GPU_CACHE_PROMOTION_POLICY_PARAMS  gpuCachePromoteParams = {0};

    UINT32 promotionPolicies[] =
    {
        LW2080_CTRL_FB_GPU_CACHE_PROMOTION_POLICY_NONE,
        LW2080_CTRL_FB_GPU_CACHE_PROMOTION_POLICY_QUARTER,
        LW2080_CTRL_FB_GPU_CACHE_PROMOTION_POLICY_HALF,
        LW2080_CTRL_FB_GPU_CACHE_PROMOTION_POLICY_FULL,
    };

    m_hObj5080 = 0;

    //
    // Save off the current gpu cache promotion policy for this engine so we
    // can restore later.
    //
    gpuCachePromoteParams.engine = engine;
    status = LwRmControl(pLwRm->GetClientHandle(), pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                        LW2080_CTRL_CMD_FB_GET_GPU_CACHE_PROMOTION_POLICY,
                        &gpuCachePromoteParams,
                        sizeof (gpuCachePromoteParams));

    // If we dont support this control (not all engines do), we should _not_ fail the test.
    if (status == LW_ERR_NOT_SUPPORTED)
    {
        Printf(Tee::PriLow,"%s - LW2080_CTRL_CMD_FB_GET_GPU_CACHE_PROMOTION_POLICY not supported, skipping test.\n",
                __FUNCTION__);
        return rc;
    }
    originalGpuCachePromotionPolicy = gpuCachePromoteParams.promotionPolicy;

    Printf(Tee::PriLow,"%s - Testing gpu cache promotion policy setting via rmcontrol path...\n",
            __FUNCTION__);

    // Test the rmcontrol api path to the gpu cache promotion policy controls
    for (i = 0; i < sizeof(promotionPolicies)/sizeof(UINT32); i++)
    {
        gpuCachePromoteParams.promotionPolicy = promotionPolicies[i];
        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                        LW2080_CTRL_CMD_FB_SET_GPU_CACHE_PROMOTION_POLICY,
                                        &gpuCachePromoteParams,
                                        sizeof (gpuCachePromoteParams)));

        // Get the updated value and compare.
        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                            LW2080_CTRL_CMD_FB_GET_GPU_CACHE_PROMOTION_POLICY,
                                            &gpuCachePromoteParams,
                                            sizeof (gpuCachePromoteParams)));

        if (gpuCachePromoteParams.promotionPolicy != promotionPolicies[i])
        {
            // That failed, so spew something and fail
            Printf(Tee::PriHigh,"%s - Value set via rmcontrol (%08x) does not match rmcontrol retrieved value (%08x), failing.\n",
                    __FUNCTION__, promotionPolicies[i],
                    gpuCachePromoteParams.promotionPolicy);
            CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
        }
    }

    //
    // Test the deferred api path to the gpu cache promotion policy controls.
    // If we made it this far, it should be supported, fail if not.
    //
    if (!IsClassSupported(LW50_DEFERRED_API_CLASS))
    {
        Printf(Tee::PriHigh,"%s - LW50_DEFERRED_API_CLASS not supported, failing test.\n",
                __FUNCTION__);
        CHECK_RC_CLEANUP(RC::LWRM_INSUFFICIENT_RESOURCES);
    }

    Printf(Tee::PriLow,"%s - Testing gpu cache promotion policy setting via class 5080 deferred API path...\n",
            __FUNCTION__);

    CHECK_RC_CLEANUP(pLwRm->Alloc(m_hCh, &m_hObj5080, LW50_DEFERRED_API_CLASS, NULL));

    for (i = 0; i < sizeof(promotionPolicies)/sizeof(UINT32); i++)
    {
        // Setup the deferred api bundle
        params.hApiHandle = 0x10101010;
        params.cmd        = LW2080_CTRL_CMD_FB_SET_GPU_CACHE_PROMOTION_POLICY;
        params.api_bundle.CachePromotePolicy.engine = engine;
        params.api_bundle.CachePromotePolicy.promotionPolicy = promotionPolicies[i];

        CHECK_RC_CLEANUP(pLwRm->Control(m_hObj5080, LW5080_CTRL_CMD_DEFERRED_API,
                                (void*)&params, sizeof(params)));

        // Trigger the deferred API bundle via the pushbuffer calls
        m_pCh->SetObject(0, m_hObj5080);
        m_pCh->Write(0, LW5080_DEFERRED_API, params.hApiHandle);
        m_pCh->Flush();
        CHECK_RC(m_pCh->SetObject(0, m_obj3d));
        m_pCh->Write(0, LW8697_SET_OBJECT, m_obj3d);

        CHECK_RC(m_Notifier.Instantiate(0, LW8697_SET_CONTEXT_DMA_NOTIFY));
        m_Notifier.Clear(LW8697_NOTIFIERS_NOTIFY);
        CHECK_RC(m_Notifier.Write(0));
        CHECK_RC(m_pCh->Write(0, LW8697_NO_OPERATION, 0));
        CHECK_RC(m_pCh->Flush());
        CHECK_RC(m_Notifier.Wait(LW8697_NOTIFIERS_NOTIFY, m_TimeoutMs));
        m_pCh->Flush();

        // Get the updated value (via our verified rmcontrol path) and compare.
        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                LW2080_CTRL_CMD_FB_GET_GPU_CACHE_PROMOTION_POLICY,
                                &gpuCachePromoteParams,
                                sizeof (gpuCachePromoteParams)));

        if (gpuCachePromoteParams.promotionPolicy != promotionPolicies[i])
        {
            // That failed, so spew something
            Printf(Tee::PriHigh,"%s - Value set via deferred api (%08x) does not match rmcontrol retrieved value (%08x), failing.\n",
                    __FUNCTION__, promotionPolicies[i],
                    gpuCachePromoteParams.promotionPolicy);
            CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
        }
    }

Cleanup:

    if (m_hObj5080)
    {
        pLwRm->Free(m_hObj5080);
    }

    // Restore the original gpu cache promotion policy for this engine.
    gpuCachePromoteParams.promotionPolicy = originalGpuCachePromotionPolicy;
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_FB_SET_GPU_CACHE_PROMOTION_POLICY,
                            &gpuCachePromoteParams,
                            sizeof (gpuCachePromoteParams)));
    return rc;
}

//! \brief BasicCacheFlushApiTest()
//!
//! This function is the entry point for the test that exercises the cache
//! flush API.
//!
//! \returns OK if all tests passed
//! \sa BasicCacheFlushApiTest()
//-----------------------------------------------------------------------------
RC GpuCacheTest::BasicCacheFlushApiTest()
{
    RC rc;
    RC expectedRc;
    LwRmPtr pLwRm;
    LwU32 passCount = 0;
    LwU32 failCount = 0;
    LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS flushParams;
    LW0041_CTRL_SURFACE_FLUSH_GPU_CACHE_PARAMS flushParams0041;
    bool bUseCl0041;

    Printf(Tee::PriLow, "%s: Beginning cache flush API tests\n", __FUNCTION__);
    for (LwU32 i = 0; flushTestFuncs[i] != NULL; ++i)
    {
        bUseCl0041 = false;

        flushTestFuncs[i]( m_flushSurf, &flushParams, &flushParams0041, &expectedRc, &bUseCl0041);

        if (m_printFlushParams)
        {
            if (bUseCl0041)
            {
                printFlushParams(NULL, &flushParams0041, expectedRc);
            }
            else
            {
                printFlushParams(&flushParams, NULL, expectedRc);
            }
        }

        if (m_verifyFlushes && expectedRc == OK)
        {
            CHECK_RC(prepareVerifyFlush());
        }

        if (bUseCl0041)
        {
            rc = pLwRm->Control(m_flushSurf->GetMemHandle(),
                    LW0041_CTRL_CMD_SURFACE_FLUSH_GPU_CACHE,
                    (void*) &flushParams0041,
                    sizeof(flushParams0041));
        }
        else
        {
            rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                    LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
                    (void*) &flushParams,
                    sizeof(flushParams));

        }

        if (rc != expectedRc)
        {
            failCount++;

            Printf(Tee::PriHigh, "Return Code Mismatch for test %d (expected: \"%s\" got: \"%s\")\n",
                    i, expectedRc.Message(), rc.Message());
        }
        else if (expectedRc == OK)
        {
            if (m_verifyFlushes)
            {
                if (bUseCl0041)
                {
                    rc = verifyFlush(NULL, &flushParams0041);
                }
                else
                {
                    rc = verifyFlush(&flushParams, NULL);
                }

                if(rc != OK)
                {
                    Printf(Tee::PriHigh, "%s: Flush verification failed.\n",
                            __FUNCTION__);
                    failCount++;
                    if (bUseCl0041)
                    {
                        printFlushParams(NULL, &flushParams0041, expectedRc);
                    }
                    else
                    {
                        printFlushParams(&flushParams, NULL, expectedRc);
                    }
                    printSurface(m_flushSurf, m_pFlushSurfDirect);
                }
                else
                {
                    passCount++;
                }
            }
            else
            {
                passCount++;
            }
        }
        else
        {
            rc = OK;
            passCount++;
        }

    }

    Printf(Tee::PriHigh, "%s: Pass: %d Fail: %d\n", __FUNCTION__, passCount, failCount);
    return failCount == 0 ? OK : RC::DATA_MISMATCH;
}

//! \brief flushTestNoType() - No flush type
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestNoType)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));
    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE);

    *rc = RC::LWRM_ILWALID_ARGUMENT;
}

//! \brief flushTestAddrArrayZero() - 0 length address array
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestAddrArrayZero)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));
    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
    pParams->addressArraySize = 0;
    pParams->memBlockSizeBytes = 0x1000;

    *rc = RC::LWRM_ILWALID_ARGUMENT;
}

//! \brief flushTestAddrArrayLimit() - AddressArraySize > MAX
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestAddrArrayLimit)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
    pParams->addressArraySize = LW2080_CTRL_FB_FLUSH_GPU_CACHE_MAX_ADDRESSES * 2;
    pParams->memBlockSizeBytes = 0x1000;

    *rc = RC::LWRM_ILWALID_ARGUMENT;
}

//! \brief flushTestMemBlockZero() - 0 length memBlockSizeBytes
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestMemBlockZero)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    pParams->addressArraySize = 100;
    pParams->memBlockSizeBytes = 0;

    *rc = RC::LWRM_ILWALID_ARGUMENT;
}

//! \brief flushTestBadAperture() - Invalid flush aperture
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestBadAperture)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE) |
        DRF_SHIFTMASK(LW2080_CTRL_FB_FLUSH_GPU_CACHE_FLAGS_APERTURE) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);

    *rc = RC::LWRM_ILWALID_ARGUMENT;
}

//! \brief flushTestOverlap() - Overlapping memory blocks
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestOverlap)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);
    LwU64 surfBase = getPhysicalAddress(pSurf, 0);

    // Overlapping memory blocks
    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->addressArraySize = 8;
    for (UINT32 i = 0; i < pParams->addressArraySize; ++i)
    {
        pParams->addressArray[i] = (LwU64) ((LwU8*)surfBase + i*0x1000);
    }

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    pParams->memBlockSizeBytes = 0x10000;

    if (pSurf != NULL)
    {
        if (pSurf->GetLocation() == Memory::Fb)
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY);
        }
        else
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
        }

    }

    *rc = RC::LWRM_ILWALID_ARGUMENT;
}

//! \brief flushTestOverlapGrowDown() - Overlapping memory blocks, addresses grow down
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestOverlapGrowDown)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);
    LwU64 surfBase = getPhysicalAddress(pSurf, 0);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->addressArraySize = 8;
    for (UINT32 i = 0; i < pParams->addressArraySize; ++i)
    {
        pParams->addressArray[i] = surfBase + (32-i)*0x1000;
    }

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    pParams->memBlockSizeBytes = 0x10000;

    if (pSurf != NULL)
    {
        if (pSurf->GetLocation() == Memory::Fb)
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY);
        }
        else
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
        }

    }

    *rc = RC::LWRM_ILWALID_ARGUMENT;
}

//! \brief flushTestOverlapAlign() - Overlapping memory blocks after addresses have been aligned.
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestOverlapAlign)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);
    LwU64 surfBase = getPhysicalAddress(pSurf, 0);

    // Align creates overlapping memory blocks
    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->addressArraySize = 8;
    for (UINT32 i = 0; i < pParams->addressArraySize; ++i)
    {
        pParams->addressArray[i] = surfBase + i*0x1000+0xfff;
    }

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    pParams->addressAlign = 0x1000;
    pParams->memBlockSizeBytes = 0x1100;

    if (pSurf != NULL)
    {
        if (pSurf->GetLocation() == Memory::Fb)
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY);
        }
        else
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
        }

    }

    *rc = RC::LWRM_ILWALID_ARGUMENT;
}

//! \brief flushTestEvictAll() - Full cache flush-evict
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestEvictAll)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    *rc = OK;
}

//! \brief flushTestIlwalidateAll() - Full cache flush-ilwalidate
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestIlwalidateAll)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _NO) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    *rc = OK;
}

//! \brief flushTestEvictAllocation() - flush-evict an allocation
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestEvictAllocation)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);

    memset(pParams0041, 0, sizeof(LW0041_CTRL_SURFACE_FLUSH_GPU_CACHE_PARAMS));

    pParams0041->flags =
        DRF_DEF(0041, _CTRL_SURFACE_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(0041, _CTRL_SURFACE_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    *rc = OK;
    *bUseCl0041 = true;
}

//! \brief flushTestIlwalidateAllocation() - flush-ilwalidate an allocation
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestIlwalidateAllocation)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);

    memset(pParams0041, 0, sizeof(LW0041_CTRL_SURFACE_FLUSH_GPU_CACHE_PARAMS));

    pParams0041->flags =
        DRF_DEF(0041, _CTRL_SURFACE_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _NO) |
        DRF_DEF(0041, _CTRL_SURFACE_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    *rc = OK;
    *bUseCl0041 = true;
}

//! \brief flushTestCleanAllocation() - flush-clean an allocation
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestCleanAllocation)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);

    memset(pParams0041, 0, sizeof(LW0041_CTRL_SURFACE_FLUSH_GPU_CACHE_PARAMS));

    pParams0041->flags =
        DRF_DEF(0041, _CTRL_SURFACE_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(0041, _CTRL_SURFACE_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _NO);
    *rc = OK;
    *bUseCl0041 = true;
}

//! \brief flushTestCleanAll() - Full cache flush-clean
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestCleanAll)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _NO);
    *rc = OK;
}

//! \brief flushTestEvict256BContig() - Flush-evict array of contiguous 256B memory blocks
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestEvict256BContig)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);
    LwU64 surfBase = getPhysicalAddress(pSurf, 0);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->addressArraySize = 0x1000/0x100;
    for (UINT32 i = 0; i < pParams->addressArraySize; ++i)
    {
        pParams->addressArray[i] = surfBase + i*0x100;
    }

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    pParams->memBlockSizeBytes = 0x100;
    if (pSurf != NULL)
    {
        if (pSurf->GetLocation() == Memory::Fb)
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY);
        }
        else
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
        }

    }

    *rc = OK;
}

//! \brief flushTestClean256BContig() - Flush-clean array of contiguous 256B memory blocks
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestClean256BContig)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);
    LwU64 surfBase = getPhysicalAddress(pSurf, 0);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->addressArraySize = 0x1000/0x100;
    for (UINT32 i = 0; i < pParams->addressArraySize; ++i)
    {
        pParams->addressArray[i] = surfBase + i*0x100;
    }

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _NO);
    pParams->memBlockSizeBytes = 0x100;
    if (pSurf != NULL)
    {
        if (pSurf->GetLocation() == Memory::Fb)
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY);
        }
        else
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
        }

    }

    *rc = OK;
}

//! \brief flushTestEvict256BContigGrowDown() - Flush-evict array of contiguous 256B memory
//!                                             blocks w/addresses growing down
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestEvict256BContigGrowDown)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);
    LwU64 surfBase = getPhysicalAddress(pSurf, 0);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->addressArraySize = 0x1000/0x100;
    for (UINT32 i = 0; i < pParams->addressArraySize; ++i)
    {
        pParams->addressArray[i] = surfBase + (0x1000/0x100-i)*0x100;
    }

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    pParams->memBlockSizeBytes = 0x100;
    if (pSurf != NULL)
    {
        if (pSurf->GetLocation() == Memory::Fb)
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY);
        }
        else
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
        }

    }

    *rc = OK;
}

//! \brief flushTestEvict4kPagesLSBGarbage() - Flush-evict array of contiguous 4K memory blocks
//!                                            w/garbage in LSB
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestEvict4kPagesLSBGarbage)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);
    LwU64 surfBase = getPhysicalAddress(pSurf, 0);

    // try an array of 4k pages, this time with garbage bits in addresses
    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->addressArraySize = 8;
    for (UINT32 i = 0; i < pParams->addressArraySize; ++i)
    {
        pParams->addressArray[i] = surfBase + i*0x1000;
        pParams->addressArray[i] |= 0xFF;
    }

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    pParams->memBlockSizeBytes = 0x1000;
    pParams->addressAlign = 0x100;
    if (pSurf != NULL)
    {
        if (pSurf->GetLocation() == Memory::Fb)
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY);
        }
        else
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
        }

    }

    *rc = OK;
}

//! \brief flushTestEvict256BNonContig() - Flush-evict array of non-contiguous 256B memory blocks
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestEvict256BNonContig)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);
    LwU64 surfBase = getPhysicalAddress(pSurf, 0);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->addressArraySize = FLUSH_MEM_SIZE/0x1000;
    for (UINT32 i = 0; i < pParams->addressArraySize; ++i)
    {
        pParams->addressArray[i] = surfBase + i*0x1000;
    }

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    pParams->memBlockSizeBytes = 0x100;
    if (pSurf != NULL)
    {
        if (pSurf->GetLocation() == Memory::Fb)
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY);
        }
        else
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
        }

    }

    *rc = OK;
}

//! \brief flushTestIlwalidate256BContig() - Flush-ilwalidate array of non-contiguous
//!                                          256B memory blocks
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestIlwalidate256BNonContig)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);
    LwU64 surfBase = getPhysicalAddress(pSurf, 0);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->addressArraySize = FLUSH_MEM_SIZE/0x1000;
    for (UINT32 i = 0; i < pParams->addressArraySize; ++i)
    {
        pParams->addressArray[i] = surfBase + i*0x1000;
    }

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _NO) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);

    if (pSurf != NULL)
    {
        if (pSurf->GetLocation() == Memory::Fb)
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY);
        }
        else
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
        }

    }

    pParams->memBlockSizeBytes = 0x100;

    *rc = OK;
}

//! \brief flushTestEvictOneLine() - Flush-evict a single cache line
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestEvictOneLine)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);
    LwU64 surfBase = getPhysicalAddress(pSurf, 0);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->addressArray[0] = surfBase;
    pParams->addressArraySize = 1;

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    pParams->memBlockSizeBytes = 0x100;

    if (pSurf != NULL)
    {
        if (pSurf->GetLocation() == Memory::Fb)
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY);
        }
        else
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
        }

    }

    *rc = OK;
}

//! \brief flushTestCleanOneLine() - Flush-clean a single cache line
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestCleanOneLine)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);
    LwU64 surfBase = getPhysicalAddress(pSurf, 0);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->addressArray[0] = surfBase;
    pParams->addressArraySize = 1;

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _NO);
    pParams->memBlockSizeBytes = 0x100;

    if (pSurf != NULL)
    {
        if (pSurf->GetLocation() == Memory::Fb)
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY);
        }
        else
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
        }

    }

    *rc = OK;
}

//! \brief flushTestIlwalidateOneLine() - Flush-ilwalidate a single cache line
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestIlwalidateOneLine)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);
    LwU64 surfBase = getPhysicalAddress(pSurf, 0);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->addressArray[0] = surfBase;
    pParams->addressArraySize = 1;

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _NO) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    pParams->memBlockSizeBytes = 0x100;

    if (pSurf != NULL)
    {
        if (pSurf->GetLocation() == Memory::Fb)
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY);
        }
        else
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
        }

    }

    *rc = OK;
}

//! \brief flushTestNoFlush() - Won't flush memory (but flush call succeeds).  For checking verification algorithm.
//!
//! Will actually do a flush-clean of memory outside the surface where we
//! verify.  This way all cache lines within the surface will not be flushed.
//!
//! \return void
//-----------------------------------------------------------------------------
CREATE_FLUSH_TEST(flushTestNoFlush)
{
    Printf(Tee::PriLow, "%s\n", __FUNCTION__);
    LwU64 surfBase = getPhysicalAddress(pSurf, 0);

    memset(pParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    pParams->addressArray[0] = surfBase-0x1000;
    pParams->addressArraySize = 1;

    pParams->flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _ADDRESS_ARRAY) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _NO) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);
    pParams->memBlockSizeBytes = 0x100;

    if (pSurf != NULL)
    {
        if (pSurf->GetLocation() == Memory::Fb)
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY);
        }
        else
        {
            pParams->flags |= DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY);
        }

    }

    *rc = OK;
}

//! \brief perpareVerifyFlush() - Sets up cache/memory in order to verify flush oclwrred.
//!
//! \return OK if successful, appropriate error message otherwise
//-----------------------------------------------------------------------------
RC GpuCacheTest::prepareVerifyFlush()
{
    RC rc = OK;
    LwRmPtr pLwRm;
    LW208F_CTRL_FB_CTRL_GPU_CACHE_PARAMS ctrlParams = {0};
    vector<UINT32>  fillPatterlwec(1, GPU_CACHE_TEST_FILL_PATTERN);
    vector<UINT32>  clearPatterlwec(1, GPU_CACHE_TEST_CLEAR_PATTERN);

    ctrlParams.cacheReset = LW208F_CTRL_FB_CTRL_GPU_CACHE_CACHE_RESET_RESET;

    CHECK_RC(pLwRm->Control(m_subDevDiag,
                LW208F_CTRL_CMD_FB_CTRL_GPU_CACHE,
                &ctrlParams, sizeof(ctrlParams)));

    CHECK_RC(m_flushSurf->Fill(GPU_CACHE_TEST_FILL_PATTERN));
    CHECK_RC(doHwClear());

    //
    // Only do this check on the first run to make sure our method of filling
    // the cache is valid
    //
    if (m_bVerifiedCaching)
    {
        return rc;
    }
    Printf(Tee::PriLow, "Verifying caching....\n");

    // Verify everything is in the cache.
    for (LwU32 i = 0; i<m_flushSurf->GetAllocSize()/sizeof(LwU32); ++i)
    {
        // Only check the first dword of every cache line to speed things up...
        if (i%CACHE_LINE_SIZE != 0)
            continue;

        if (MEM_RD32(((LwU32*) m_pFlushSurfDirect) + i) != GPU_CACHE_TEST_FILL_PATTERN ||
                MEM_RD32(((LwU32*) m_flushSurf->GetAddress()) + i) != GPU_CACHE_TEST_CLEAR_PATTERN)
        {
            Printf(Tee::PriHigh, "Memory-mismatch at 0x%08x: direct: 0x%08x (expected 0x%x) reflected: 0x%08x(expected 0x%08x)\n",
                    i,
                    MEM_RD32(((LwU32*) m_pFlushSurfDirect) + i),
                    GPU_CACHE_TEST_FILL_PATTERN,
                    MEM_RD32(((LwU32*) m_flushSurf->GetAddress()) + i),
                    GPU_CACHE_TEST_CLEAR_PATTERN);
            return RC::DATA_MISMATCH;
        }
    }

    m_bVerifiedCaching = true;
    Printf(Tee::PriLow, "Done\n");

    return rc;
}

//! \brief verifyFlush() - Verify flush was properly exelwted
//!
//! Walks over the entire surface and verifies regions were either flushed or
//! not flushed based on the parameters passed to the flush API.  Also verifies
//! the proper type of flush oclwrred.
//!
//! \return OK if flush properly exelwted, approriate error message otherwise
//-----------------------------------------------------------------------------
RC GpuCacheTest::verifyFlush(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS* p2080Params,
        LW0041_CTRL_SURFACE_FLUSH_GPU_CACHE_PARAMS* p0041Params)
{
    RC rc = OK;
    vector<LwU64> *flushedAddrs = NULL;
    LwU32 flushBlockIndex = 0;
    bool bFlushOclwrred;
    bool bWriteback;
    bool bIlwalidate;
    bool bFullSurface;

    MASSERT((p2080Params == NULL) ^ (p0041Params == NULL));

    MASSERT(m_verifyFlushes == true);

    flushBlockIndex = 0;

    if (p2080Params)
    {
        if (p2080Params->addressAlign == 0)
        {
            p2080Params->addressAlign = 1;
        }
        bWriteback = FLD_TEST_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES, p2080Params->flags);
        bIlwalidate = FLD_TEST_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES, p2080Params->flags);
        bFullSurface = FLD_TEST_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE, p2080Params->flags);
        // Sorting the flushed addresses will make the verification below a lot simpler...
        flushedAddrs = new vector<LwU64>(p2080Params->addressArray,
                &p2080Params->addressArray[p2080Params->addressArraySize]);
        sort(flushedAddrs->begin(), flushedAddrs->end());
    }
    else
    {
        MASSERT(p0041Params);
        bWriteback = FLD_TEST_DRF(0041, _CTRL_SURFACE_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES, p0041Params->flags);
        bIlwalidate = FLD_TEST_DRF(0041, _CTRL_SURFACE_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES, p0041Params->flags);
        bFullSurface = true;
    }

    if (!bFullSurface)
    {
        Printf(Tee::PriDebug, "%s: Checking Block 0x%llx +0x%llx\n",
                __FUNCTION__, (*flushedAddrs)[flushBlockIndex], p2080Params->memBlockSizeBytes);
    }

    for (LwU64 i =  0; i < FLUSH_MEM_SIZE; i+=sizeof(LwU32))
    {
        // Only check the first dword of every cache line to speed things up...
        if (i%CACHE_LINE_SIZE != 0)
            continue;

        const PHYSADDR physAddr = getPhysicalAddress(m_flushSurf, i);
        const LwU32* addrDirect = (LwU32*) m_pFlushSurfDirect + (i/sizeof(LwU32));
        const LwU32* addrReflected = (LwU32*) m_flushSurf->GetAddress() + (i/sizeof(LwU32));

        if (bFullSurface)
        {
            bFlushOclwrred = true;
        }
        else
        {
            MASSERT(p2080Params != NULL);

            if ( physAddr >= (ALIGN_DOWN((*flushedAddrs)[flushBlockIndex],p2080Params->addressAlign)
                        + p2080Params->memBlockSizeBytes))
            {
                if (flushBlockIndex < flushedAddrs->size()-1)
                {
                    ++flushBlockIndex;
                    Printf(Tee::PriDebug, "%s: Checking Block 0x%llx +0x%llx\n",
                            __FUNCTION__, (*flushedAddrs)[flushBlockIndex], p2080Params->memBlockSizeBytes);
                }
            }

            bFlushOclwrred =  ((physAddr >= ALIGN_DOWN((*flushedAddrs)[flushBlockIndex],
                                                    p2080Params->addressAlign)) &&
                               (physAddr < ALIGN_DOWN((*flushedAddrs)[flushBlockIndex],
                                                     p2080Params->addressAlign) +
                                p2080Params->memBlockSizeBytes));
        }

        if (bFlushOclwrred)
        {
            Printf(Tee::PriDebug, "%s: Flush oclwrred for 0x%llx\n", __FUNCTION__, physAddr);

            if (bWriteback)
            {
                Printf(Tee::PriDebug, "%s:    CHECK memory is written\n", __FUNCTION__);
                // Data should be in memory..
                if (m_verifyFlushes && MEM_RD32(addrDirect) != GPU_CACHE_TEST_CLEAR_PATTERN)
                {
                    Printf(Tee::PriHigh, "%s: Data not flushed to memory!\n"
                            "    Expected: 0x%08x Observed: 0x%08x Surface Offset: 0x%llx\n",
                            __FUNCTION__,  GPU_CACHE_TEST_CLEAR_PATTERN, MEM_RD32(addrDirect), i);
                    CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
                }
            }
            else
            {
                // read through direct mapping to ensure cache was not flushed to memory
                Printf(Tee::PriDebug, "%s:    CHECK memory NOT written\n", __FUNCTION__);
                if (m_verifyFlushes)
                {
                    if (MEM_RD32(addrDirect) != GPU_CACHE_TEST_FILL_PATTERN)
                    {
                        Printf(Tee::PriHigh, "%s Data written to memory for ilwalidate flush!\n"
                                "    Expected: 0x%x Observed: 0x%08x Surface offset 0x%llx\n",
                                __FUNCTION__, GPU_CACHE_TEST_FILL_PATTERN, MEM_RD32(addrDirect), i);
                        CHECK_RC_CLEANUP(RC::DATA_MISMATCH);

                    }

                }
            }

            if (bIlwalidate)
            {

                Printf(Tee::PriDebug, "%s:    CHECK cache is invalid\n", __FUNCTION__);
                //
                // Data should not be in cache, write through direct mapping
                // and read through reflected to confirm we see it.
                //
                if (m_verifyFlushes)
                {
                    MEM_WR32(const_cast<LwU32*> (addrDirect), ~GPU_CACHE_TEST_FILL_PATTERN);
                    Platform::FlushCpuWriteCombineBuffer();
                    if (MEM_RD32(addrReflected) != (LwU32) ~GPU_CACHE_TEST_FILL_PATTERN)
                    {
                        Printf(Tee::PriHigh, "%s: Cache not ilwalidated\n"
                                "    Expected: 0x%08x Observed 0x%08x Surface Offset 0x%llx\n",
                                __FUNCTION__, ~GPU_CACHE_TEST_FILL_PATTERN, MEM_RD32(addrReflected), i);

                        CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
                    }
                }
            }
            else
            {
                Printf(Tee::PriDebug, "%s:    CHECK cache is valid\n", __FUNCTION__);
                // data should still be in cache..
                if (m_verifyFlushes)
                {
                    MEM_WR32(const_cast<LwU32*>(addrDirect), ~GPU_CACHE_TEST_FILL_PATTERN);
                    Platform::FlushCpuWriteCombineBuffer();
                    if (MEM_RD32(addrReflected) != GPU_CACHE_TEST_CLEAR_PATTERN)
                    {
                        Printf(Tee::PriHigh, "%s: Data not in cache after non-ilwalidate flush!\n"
                                "    Expected: 0x%08x Observed: 0x%08x Surface offset 0x%llx\n",
                                __FUNCTION__, GPU_CACHE_TEST_CLEAR_PATTERN, MEM_RD32(addrReflected), i);
                        CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
                    }
                }
            }
        }
        else
        {
            Printf(Tee::PriDebug, "%s: Flush did NOT occur for 0x%llx\n", __FUNCTION__, physAddr);
            Printf(Tee::PriDebug, "%s:    CHECK cache NOT flushed\n", __FUNCTION__);
            if (m_verifyFlushes)
            {
                // data should still be in cache..
                if (MEM_RD32(addrDirect) != GPU_CACHE_TEST_FILL_PATTERN)
                {
                    Printf(Tee::PriHigh, "%s Data flushed where it shouldn't be!\n"
                            "    Expected: 0x%x Observed: 0x%08x Surface offset 0x%llx\n",
                            __FUNCTION__, GPU_CACHE_TEST_FILL_PATTERN, MEM_RD32(addrDirect), i);
                    CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
                }
            }

            Printf(Tee::PriDebug, "%s:    CHECK data is cached\n", __FUNCTION__);
            if (m_verifyFlushes)
            {
                MEM_WR32(const_cast<LwU32*>(addrDirect), GPU_CACHE_TEST_FILL_PATTERN);
                Platform::FlushCpuWriteCombineBuffer();
                if (MEM_RD32(addrReflected) != GPU_CACHE_TEST_CLEAR_PATTERN)
                {
                    Printf(Tee::PriHigh, "%s: Data not in cache for memory that shouldn't have been flushed!\n"
                            "    Expected: 0x%08x Observed: 0x%08x Surface offset 0x%llx\n",
                            __FUNCTION__, GPU_CACHE_TEST_CLEAR_PATTERN, MEM_RD32(addrReflected), i);
                    CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
                }
            }
        }
    }

Cleanup:
    if (flushedAddrs)
    {
        delete flushedAddrs;
    }
    return rc;
}

//! \brief printFlushParams() - Prints parameters for the flush
//!
//! \return void
//-----------------------------------------------------------------------------
void
GpuCacheTest::printFlushParams(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS* pParams2080,
        LW0041_CTRL_SURFACE_FLUSH_GPU_CACHE_PARAMS* pParams0041, RC rc)
{
    if (pParams2080)
    {
        Printf(Tee::PriLow, "LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS: \n");
        Printf(Tee::PriLow, "    addressArraySize:  0x%x\n", pParams2080->addressArraySize);
        Printf(Tee::PriLow, "    addressAlign:      0x%x\n", pParams2080->addressAlign);
        Printf(Tee::PriLow, "    memBlockSizeBytes: 0x%llx\n", pParams2080->memBlockSizeBytes);
        Printf(Tee::PriLow, "    flags:             0x%x\n", pParams2080->flags);
        for (LwU32 i = 0; i < pParams2080->addressArraySize && i <
                LW2080_CTRL_FB_FLUSH_GPU_CACHE_MAX_ADDRESSES; ++i)
        {
            if (i%4 ==0)
            {
                Printf(Tee::PriLow, "\n    ");
            }
            Printf(Tee::PriLow, "0x%010llx ", pParams2080->addressArray[i]);
        }
    }
    else
    {
        MASSERT(pParams0041);
        Printf(Tee::PriLow, "LW0041_CTRL_SURFACE_FLUSH_GPU_CACHE_PARAMS: \n");
        Printf(Tee::PriLow, "    flags:             0x%x\n", pParams0041->flags);

    }

    Printf(Tee::PriLow, "\n    Expected RC: %s\n", rc.Message());

    if (m_flushSurf != NULL)
    {
        Printf(Tee::PriLow, "Flush Surface: \n");
        Printf(Tee::PriLow, "    Physical Offset: 0x%llx\n", getPhysicalAddress(m_flushSurf, 0));
        Printf(Tee::PriLow, "    Reflected CPU:   %p\n", m_flushSurf->GetAddress());
        Printf(Tee::PriLow, "    Direct CPU:      %p\n", m_pFlushSurfDirect);
        Printf(Tee::PriLow, "    Size:            0x%llx\n", m_flushSurf->GetAllocSize());
    }

    return;
}

//! \brief printSurface() - Prints a surface, both through the reflected and direct mapping to
//!                         allow for comparison.
//!
//! \return void
//-----------------------------------------------------------------------------
void
GpuCacheTest::printSurface(Surface2D* pSurf, void* pDirect)
{
    LwU32* pSurf32 = (LwU32*) pSurf->GetAddress();
    LwU32* pDirect32 = (LwU32*) pDirect;

    Printf(Tee::PriLow, "Direct Mapping:\n");
    for (LwU32 i = 0; i < pSurf->GetAllocSize()/sizeof(LwU32); ++i)
    {
        if (i%8==0)
        {
            Printf(Tee::PriLow, "\n 0x%08x    ", i);
        }
        Printf(Tee::PriLow, "0x%08x ", *pDirect32);
    }
    Printf(Tee::PriLow, "\n");

    Printf(Tee::PriLow, "Reflected Mapping:\n");
    for (LwU32 i = 0; i < pSurf->GetAllocSize()/sizeof(LwU32); ++i)
    {
        if (i%8==0)
        {
            Printf(Tee::PriLow, "\n 0x%08x    ", i);
        }
        Printf(Tee::PriLow, "0x%08x ", *pSurf32);
    }
    Printf(Tee::PriLow, "\n");

}

//! \brief pollForZero() - Helper function used to poll for a address to be 0.
//!
//! \return void
//-----------------------------------------------------------------------------
static bool
pollForZero(void* pArgs)
{
    return MEM_RD32((LwU32*) pArgs) == 0;
}

//! \brief  Helper function which uses 3d class to clear surface in order to allocate in cache
//!
//!
//! \return true if task is completed, false otherwise
//------------------------------------------------------------------------------
RC
GpuCacheTest::doHwClear()
{
    RC rc = OK;

    m_pCh->Write(m_subCh, LW8697_SET_OBJECT, m_obj3d);
    m_pCh->Write(m_subCh, LW8697_SET_SURFACE_CLIP_HORIZONTAL,
            DRF_NUM(8697, _SET_SURFACE_CLIP_HORIZONTAL, _WIDTH, 128));
    m_pCh->Write(m_subCh, LW8697_SET_SURFACE_CLIP_VERTICAL,
            DRF_NUM(8697, _SET_SURFACE_CLIP_VERTICAL, _HEIGHT, 128));
    m_pCh->Write(m_subCh, LW8697_SET_VIEWPORT_CLIP_HORIZONTAL(0),
            DRF_NUM(8697, _SET_VIEWPORT_CLIP_HORIZONTAL, _WIDTH, 128));
    m_pCh->Write(m_subCh, LW8697_SET_VIEWPORT_CLIP_VERTICAL(0),
            DRF_NUM(8697, _SET_VIEWPORT_CLIP_VERTICAL, _HEIGHT, 128));
    m_pCh->Write(m_subCh, LW8697_SET_CLIP_ID_TEST,0);
    m_pCh->Write(m_subCh, LW8697_SET_WINDOW_OFFSET_X, 0);
    m_pCh->Write(m_subCh, LW8697_SET_WINDOW_OFFSET_Y, 0);
    m_pCh->Write(m_subCh, LW8697_SET_STENCIL_MASK, 0xff);

    m_pCh->Write(m_subCh, LW8697_SET_CTX_DMA_COLOR(0), m_flushSurf->GetCtxDmaHandleGpu());
    m_pCh->Write(m_subCh, LW8697_SET_CT_A(0), LwU64_HI32(m_flushSurf->GetCtxDmaOffsetGpu()));
    m_pCh->Write(m_subCh, LW8697_SET_CT_B(0), LwU64_LO32(m_flushSurf->GetCtxDmaOffsetGpu()));
    m_pCh->Write(m_subCh, LW8697_SET_CT_FORMAT(0), LW8697_SET_CT_FORMAT_V_A8R8G8B8);
    m_pCh->Write(m_subCh, LW8697_SET_CT_ARRAY_PITCH(0), (LwU32) m_flushSurf->GetArrayPitch() >> 2);
    m_pCh->Write(m_subCh, LW8697_SET_CT_SIZE_A(0),
        DRF_DEF(8697, _SET_CT_SIZE_A, _LAYOUT_IN_MEMORY, _PITCH) |
        DRF_NUM(8697, _SET_CT_SIZE_A, _WIDTH, m_flushSurf->GetPitch()));
    m_pCh->Write(m_subCh, LW8697_SET_CT_SIZE_B(0),
        DRF_NUM(8697, _SET_CT_SIZE_B, _HEIGHT, m_flushSurf->GetHeight()));
    m_pCh->Write(m_subCh, LW8697_SET_CT_SIZE_C,
            DRF_NUM(8697, _SET_CT_SIZE_C, _THIRD_DIMENSION, 1) |
            DRF_DEF(8697, _SET_CT_SIZE_C, _CONTROL, _THIRD_DIMENSION_DEFINES_ARRAY_SIZE));

    m_pCh->Write(m_subCh, LW8697_SET_CLEAR_CONTROL,
            DRF_DEF(8697, _SET_CLEAR_CONTROL, _RESPECT_STENCIL_MASK, _FALSE) |
            DRF_DEF(8697, _SET_CLEAR_CONTROL, _USE_CLEAR_RECT, _FALSE));

    m_pCh->Write(m_subCh, LW8697_SET_COLOR_CLEAR_VALUE(0), 0);  // R
    m_pCh->Write(m_subCh, LW8697_SET_COLOR_CLEAR_VALUE(1), 0);  // G
    m_pCh->Write(m_subCh, LW8697_SET_COLOR_CLEAR_VALUE(2), 0);  // B
    m_pCh->Write(m_subCh, LW8697_SET_COLOR_CLEAR_VALUE(3), 0);  // A
    m_pCh->Write(m_subCh, LW8697_SET_CT_MARK(0), DRF_DEF(8697, _SET_CT_MARK, _IEEE_CLEAN, _TRUE));

    m_pCh->Write(m_subCh, LW8697_SET_CT_WRITE(0),
        DRF_DEF(8697, _SET_CT_WRITE, _R_ENABLE, _TRUE) |
        DRF_DEF(8697, _SET_CT_WRITE, _G_ENABLE, _TRUE) |
        DRF_DEF(8697, _SET_CT_WRITE, _B_ENABLE, _TRUE) |
        DRF_DEF(8697, _SET_CT_WRITE, _A_ENABLE, _TRUE));

    m_pCh->Write(m_subCh, LW8697_SET_RENDER_ENABLE_A, 0);
    m_pCh->Write(m_subCh, LW8697_SET_RENDER_ENABLE_B, 0);
    m_pCh->Write(m_subCh, LW8697_SET_RENDER_ENABLE_C, 1);

     // Assumes no 3D RT arrays
     UINT32 MaxZ = max(m_flushSurf->GetDepth(),
         m_flushSurf->GetArraySize());

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
        m_pCh->Write(m_subCh,LW8697_CLEAR_SURFACE, enables);
    }

    m_pCh->Flush();

    CHECK_RC(m_Notifier.Instantiate(m_subCh, LW8697_SET_CONTEXT_DMA_NOTIFY));
    m_Notifier.Clear(LW8697_NOTIFIERS_NOTIFY);
    CHECK_RC(m_Notifier.Write(m_subCh));
    CHECK_RC(m_pCh->Write(m_subCh, LW8697_NO_OPERATION, 0));
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_Notifier.Wait(LW8697_NOTIFIERS_NOTIFY, m_TimeoutMs));

    return rc;
}

//! \brief getPhysicalAddress() - Helper function to get physical address for offset into surface
//!
//! Queries RM to determine the physical address for a given offset into a surface.
//!
//! \returns the physical address
//-----------------------------------------------------------------------------
static PHYSADDR
getPhysicalAddress(Surface2D* pSurf, LwU64 offset)
{
    LwRmPtr pLwRm;

    if (pSurf != NULL)
    {
        LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
        params.memOffset = offset;
        pLwRm->Control(
                pSurf->GetMemHandle(),
                LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                &params, sizeof(params));

        return params.memOffset;
    }
    else
    {
        // zero based if there is no surface
        return offset;
    }

}

//! \brief BasicCacheFlushMethodTest()
//!
//! This function tests error recovery code paths for errors caused by method
//! based cache flushes.  Wraps each bad method sequence in a semaphore
//! acquire/release to ensure RC properly continues past faulting methods.
//!
//! \returns OK if all tests passed
//-----------------------------------------------------------------------------
RC GpuCacheTest::BasicCacheFlushMethodTest()
{
    RC rc;

    Printf(Tee::PriLow, "Running %s", __FUNCTION__);

    CHECK_RC(StartRCTest());

    // UNBOUND ctxdma
    MEM_WR32((LwU32*) m_pSem, 0xdeadbeef);
    m_pCh->Write(0, LW866F_SET_CONTEXT_DMA_SEMAPHORE, m_hSemCtxDma);
    m_pCh->Write(0, LW866F_SEMAPHORE_OFFSET, 0x0);
    m_pCh->Write(0, LW866F_SEMAPHORE_ACQUIRE, 0xdeadbeef);

    m_pCh->Write(0, LW866F_MEM_OP_A, 0);
    m_pCh->Write(0, LW866F_MEM_OP_B,
            DRF_DEF(866F, _MEM_OP_B, _OPERATION, _CLEAN) |
            DRF_NUM(866F, _MEM_OP_B, _NUM_LINES, MAX_MEM_LIMIT/256) |
            DRF_DEF(866F, _MEM_OP_B, _FLUSH_ALL, _DISABLED));
    m_pCh->Write(0, LW866F_SEMAPHORE_RELEASE, 1);

    // limit violation
    m_pCh->Write(0, LW866F_SEMAPHORE_ACQUIRE, 1);
    m_pCh->Write(0, LW866F_SYSMEM_FLUSH_CTXDMA, m_hSemCtxDma);
    m_pCh->Write(0, LW866F_MEM_OP_A, 0xffffffff);
    m_pCh->Write(0, LW866F_MEM_OP_B,
            DRF_DEF(866F, _MEM_OP_B, _OPERATION, _CLEAN) |
            DRF_NUM(866F, _MEM_OP_B, _NUM_LINES, MAX_MEM_LIMIT/256) |
            DRF_DEF(866F, _MEM_OP_B, _FLUSH_ALL, _DISABLED));
    m_pCh->Write(0, LW866F_SEMAPHORE_RELEASE, 2);

    // Bad args
    m_pCh->Write(0, LW866F_SEMAPHORE_ACQUIRE, 2);
    m_pCh->Write(0, LW866F_SYSMEM_FLUSH_CTXDMA, m_hSemCtxDma);
    m_pCh->Write(0, LW866F_MEM_OP_A, 0xffffffff);
    m_pCh->Write(0, LW866F_MEM_OP_B, 0xdeadbeef);
    m_pCh->Write(0, LW866F_SEMAPHORE_RELEASE, 0);

    m_pCh->Flush();

    CHECK_RC(POLLWRAP(pollForZero,
                      (void*) m_pSem,
                      m_TestConfig.TimeoutMs()));

    EndRCTest();

    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! SSet up a JavaScript class that creates and owns a C++ GpuCacheTest object
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(GpuCacheTest, RmTest,
    "GpuCacheTest RMTEST that tests GPU cahe and APIs, lwrrently specfic to iGT21A");
CLASS_PROP_READWRITE(GpuCacheTest, printFlushParams, bool,
        "Flag indicating whether or not to print parameters to flush api call.");

