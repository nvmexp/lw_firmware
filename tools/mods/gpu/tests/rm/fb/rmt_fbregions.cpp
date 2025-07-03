/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2012,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_fbregions.cpp
//! \brief Tests fb regions
//!
#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX
#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "gpu/utility/surf2d.h"
#include <stack>
#include "core/include/memcheck.h"

class FbRegionsTest: public RmTest
{
public:
    FbRegionsTest();
    virtual ~FbRegionsTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    RC ProtectedRegionTest(const LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS*);
    RC MultiRegionsTest(const LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS*);
    RC PerfFallbackTest(const LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS*);

    PHYSADDR GetPhysicalAddress(Surface2D* pSurf, LwU64 offset);

    const LW2080_CTRL_CMD_FB_GET_FB_REGION_FB_REGION_INFO* GetRegion(const LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS* pParams, Surface2D *pSurf);

    void SetupSurface(Surface2D *pSurf, LwU64 size, bool bDisplayable = false, bool bProtected = false, bool bCompressed = false, LwU64 rangeLo = 0, LwU64 rangeHi = 0);
};

//! \brief FbRegionsTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
FbRegionsTest::FbRegionsTest()
{
    SetName("FbRegionsTest");
}

//! \brief FbRegionsTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
FbRegionsTest::~FbRegionsTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True
//-----------------------------------------------------------------------------
string FbRegionsTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC FbRegionsTest::Setup()
{
    return OK;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC FbRegionsTest::Run()
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS params = {0};
    bool bHasVpr = false;
    LwU32 nonProtectedCount = 0;

    rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
            LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO,
            &params, sizeof(params));

    if (rc == RC::LWRM_NOT_SUPPORTED)
        return OK;

    CHECK_RC(rc);

    for (LwU32 i = 0; i < params.numFBRegions; i++)
    {
        Printf(Tee::PriLow, "%s: FB Region %d:\n"
                            "    Base:          0x%llx\n"
                            "    Limit:         0x%llx\n"
                            "    Reserved:      0x%llx\n"
                            "    Performance:   0x%x\n"
                            "    ISO:           %s\n"
                            "    Compression:   %s\n"
                            "    Protected:     %s\n",
                            __FUNCTION__, i,
                            params.fbRegion[i].base,
                            params.fbRegion[i].limit,
                            params.fbRegion[i].reserved,
                            params.fbRegion[i].performance,
                            params.fbRegion[i].supportISO ? "TRUE" : "FALSE",
                            params.fbRegion[i].supportCompressed ? "TRUE" : "FALSE",
                            params.fbRegion[i].bProtected ? "TRUE" : "FALSE");

        bHasVpr = bHasVpr || params.fbRegion[i].bProtected;

        if (!params.fbRegion[i].bProtected)
            nonProtectedCount++;
    }

    if (bHasVpr)
        CHECK_RC(ProtectedRegionTest(&params));

    if (params.numFBRegions > 1)
        CHECK_RC(MultiRegionsTest(&params));

    if (nonProtectedCount > 1)
        CHECK_RC(PerfFallbackTest(&params));

    return rc;
}

//! \brief Gets the physical address of an offset into a surface
//!
//! \return A physical address
//-----------------------------------------------------------------------------
PHYSADDR FbRegionsTest::GetPhysicalAddress(Surface2D* pSurf, LwU64 offset)
{
    LwRmPtr pLwRm;
    LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};

    MASSERT(pSurf);

    params.memOffset = offset;
    RC rc = pLwRm->Control(
            pSurf->GetMemHandle(),
            LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
            &params, sizeof(params));

    MASSERT(rc == OK);
    if (rc != OK)
        Printf(Tee::PriHigh, "GetPhysicalAddress failed: %s\n", rc.Message());

    return params.memOffset;
}

//! \brief Gets the region where a given surface has been allocated
//!
//! \return Pointer to the info for the region.
//-----------------------------------------------------------------------------
const LW2080_CTRL_CMD_FB_GET_FB_REGION_FB_REGION_INFO*
FbRegionsTest::GetRegion(const LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS* pParams, Surface2D *pSurf)
{
    MASSERT(pParams);
    MASSERT(pSurf);

    LwU64 address = GetPhysicalAddress(pSurf, 0);
    for (LwU32 i = 0; i < pParams->numFBRegions; i++)
    {
        if (address >= pParams->fbRegion[i].base &&
                address <= pParams->fbRegion[i].limit)
            return &pParams->fbRegion[i];
    }

    MASSERT(0);
    return NULL;
}

//! \brief Helper function to setup common parameters
//!
//! \return OK if allocation was successful
//-----------------------------------------------------------------------------
void FbRegionsTest::SetupSurface
(
    Surface2D *pSurf,
    LwU64 size,
    bool bDisplayable,
    bool bProtected,
    bool bCompressed,
    LwU64 rangeLo,
    LwU64 rangeHi
)
{
    MASSERT(pSurf);

    pSurf->SetForceSizeAlloc(true);
    pSurf->SetArrayPitch(1);
    pSurf->SetPageSize(4);
    pSurf->SetLayout(Surface2D::Pitch);
    pSurf->SetAddressModel(Memory::Paging);
    pSurf->SetColorFormat(ColorUtils::VOID32);

    pSurf->SetArraySize(LwU64_LO32(size));

    pSurf->SetDisplayable(bDisplayable);
    pSurf->SetVideoProtected(bProtected);
    pSurf->SetCompressed(bCompressed);

    pSurf->SetGpuPhysAddrHint(rangeLo);
    pSurf->SetGpuPhysAddrHintMax(rangeHi);

}

//! \brief Tests generic multiple region functionality
//!
//! \return OK if all tests are successful.
//-----------------------------------------------------------------------------
RC FbRegionsTest::MultiRegionsTest(const LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS* pParams)
{

    stack<Surface2D*> surfs;
    Surface2D surf;
    RC rc;
    LwU32 nonProtectedCount = 0;
    const LW2080_CTRL_CMD_FB_GET_FB_REGION_FB_REGION_INFO *pRegion = NULL;

    Printf(Tee::PriHigh, "%s\n", __FUNCTION__);

    for (LwU32 i = 0; i < pParams->numFBRegions; i++)
    {
        if (!pParams->fbRegion[i].bProtected)
            nonProtectedCount++;
    }

    if (!nonProtectedCount)
    {
        Printf(Tee::PriHigh, "%s: >1 region and no non-protected regions to allocate out of!\n",
                __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    // check we can allocate ISO
    SetupSurface(&surf, 0x1000, true);
    CHECK_RC(surf.Alloc(GetBoundGpuDevice()));

    pRegion = GetRegion(pParams, &surf);
    if (!pRegion->supportISO)
    {
        Printf(Tee::PriHigh, "%s: ISO allocation in non-ISO region\n",
                __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    surf.Free();

    // check we can allocate compression
    SetupSurface(&surf, 0x1000, false, false, true);
    CHECK_RC(surf.Alloc(GetBoundGpuDevice()));

    pRegion = GetRegion(pParams, &surf);
    if (!pRegion->supportCompressed)
    {
        Printf(Tee::PriHigh, "%s: Compressed allocation in non-compressible region\n",
                __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    surf.Free();

    // check that range based allocs and ISO/compressed flags work
    for (LwU32 i = 0; i < pParams->numFBRegions; i++)
    {
        pRegion = &pParams->fbRegion[i];
        const LW2080_CTRL_CMD_FB_GET_FB_REGION_FB_REGION_INFO *pTmpRegion = NULL;

        // Check ISO
        SetupSurface(&surf, 0x1000, true, pRegion->bProtected != LW_FALSE, false, pRegion->base, pRegion->limit);
        rc = surf.Alloc(GetBoundGpuDevice());

        //
        // First check that if the alloc succeeded then it is in the region
        // we're testing
        //
        if (rc == OK)
        {
            pTmpRegion = GetRegion(pParams, &surf);
            if (pTmpRegion != pRegion)
            {
                Printf(Tee::PriHigh, "%s: Range is ignored for ISO allocs\n", __FUNCTION__);
                CHECK_RC(RC::SOFTWARE_ERROR);
            }
        }

        //
        // Next if the region being tested supports ISO then make sure the
        // alloc succeeded
        //
        if (pRegion->supportISO)
        {
            CHECK_RC(rc);
        }
        else
        {
            // If it doesn't support ISO then the allcoation better fail.
            if (rc == OK)
            {
                Printf(Tee::PriHigh, "%s: ISO allowed in non-ISO region\n", __FUNCTION__);
                CHECK_RC(RC::SOFTWARE_ERROR);
            }
            rc.Clear();
        }

        surf.Free();

        // Check Compressed
        SetupSurface(&surf, 0x1000, false, pRegion->bProtected != LW_FALSE, true, pRegion->base, pRegion->limit);
        rc = surf.Alloc(GetBoundGpuDevice());

        if (rc == OK)
        {
            pTmpRegion = GetRegion(pParams, &surf);
            if (pTmpRegion != pRegion)
            {
                Printf(Tee::PriHigh, "%s: Range is ignored for compressed allocs\n", __FUNCTION__);
                CHECK_RC(RC::SOFTWARE_ERROR);
            }
        }

        if (pRegion->supportCompressed)
        {
            CHECK_RC(rc);
        }
        else
        {
            if (rc == OK)
            {
                Printf(Tee::PriHigh, "%s: Compression allowed in non-compressible region\n", __FUNCTION__);
                CHECK_RC(RC::SOFTWARE_ERROR);
            }
            rc.Clear();
        }

        surf.Free();

    }

    return OK;
}

//!
//! \brief Allocate until we spill over into next region to make sure we alloc high perf first
//!
//! Do it twice, once allocating from top of memory and once from bottom.
//! This makes sure that if the regions are in the same order when sorted by
//! performance and address we still correctly place based on region
//! performance
//!
//! Only do this test if we have more than 1 non-protected region
//!
//! \return OK if successful, error otherwise
//-----------------------------------------------------------------------------
RC FbRegionsTest::PerfFallbackTest(const LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS* pParams)
{
    stack<Surface2D*> surfs;
    Surface2D surf;
    RC rc;
    const LW2080_CTRL_CMD_FB_GET_FB_REGION_FB_REGION_INFO *pRegion = NULL;

    Printf(Tee::PriHigh, "%s\n", __FUNCTION__);

    for (bool bReverse = false; ; bReverse = !bReverse)
    {
        pRegion = NULL;
        while (rc == OK)
        {
            const LW2080_CTRL_CMD_FB_GET_FB_REGION_FB_REGION_INFO *pTmpRegion = NULL;
            Surface2D *pSurf = new Surface2D;

            SetupSurface(pSurf, 1<<20);
            pSurf->SetPaReverse(bReverse);
            rc = pSurf->Alloc(GetBoundGpuDevice());

            if (rc == OK)
                surfs.push(pSurf);
            else
                delete pSurf;

            CHECK_RC(rc);

            pTmpRegion = GetRegion(pParams, pSurf);

            if (!pRegion)
                pRegion = pTmpRegion;

            // We've spilled over, compare perf
            if (pRegion != pTmpRegion)
            {
                if (pRegion->performance < pTmpRegion->performance)
                {
                    Printf(Tee::PriHigh, "%s Spilled into higher perf range\n", __FUNCTION__);
                    CHECK_RC(RC::SOFTWARE_ERROR);
                }
                break;
            }
        }

        while (!surfs.empty())
        {
            Surface2D *pSurf = surfs.top();
            pSurf->Free();
            delete pSurf;
            surfs.pop();
        }

        if (bReverse)
            break;
    }

    //
    // Check that range takes precedence over perf.
    // Find a region that isn't the lowest perf, then force an allocation in that region.
    // If all regions have same perf we'll just end up doing a normal range alloc
    //
    pRegion = NULL;
    for (LwU32 i = 0; i < pParams->numFBRegions; i++)
    {
        if (!pRegion)
            pRegion = &pParams->fbRegion[i];

        if (!pRegion || pRegion->performance > pParams->fbRegion[i].performance)
        {
            pRegion = &pParams->fbRegion[i];
            break;
        }
    }

    SetupSurface(&surf, 0x1000, false, false, false, pRegion->base, pRegion->limit);
    rc = surf.Alloc(GetBoundGpuDevice());

    if (rc == OK)
    {
        const LW2080_CTRL_CMD_FB_GET_FB_REGION_FB_REGION_INFO *pTmpRegion = NULL;

        pTmpRegion = GetRegion(pParams, &surf);
        if (pTmpRegion != pRegion)
        {
            Printf(Tee::PriHigh, "%s: Range is incorrectly overridden by perf\n", __FUNCTION__);
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }
    CHECK_RC(rc);
    surf.Free();

    return OK;
}

//! \brief tests specific to protected regions
//!
//! \return OK if all tests succeed.
//-----------------------------------------------------------------------------
RC FbRegionsTest::ProtectedRegionTest(const LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS* pParams)
{
    RC rc;
    const LW2080_CTRL_CMD_FB_GET_FB_REGION_FB_REGION_INFO *pProtectedRegion = NULL;
    Surface2D surf;

    Printf(Tee::PriHigh, "%s\n", __FUNCTION__);

    // Assume only a single protected region is allowed.
    for (LwU32 i = 0; i < pParams->numFBRegions; i++)
    {
        if (pParams->fbRegion[i].bProtected)
        {
            if (pProtectedRegion)
            {
                Printf(Tee::PriHigh, "%s: More than 1 protected region reported\n",
                        __FUNCTION__);
                CHECK_RC(RC::SOFTWARE_ERROR);
            }

            pProtectedRegion = &pParams->fbRegion[i];
        }
    }

    MASSERT(pProtectedRegion);

    // RM should never need reserved VPR memory
    if (pProtectedRegion->reserved)
    {
        Printf(Tee::PriHigh, "%s: No protected memory should be reserved\n", __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    // Test that protected allocations end up in right region
    SetupSurface(&surf, 0x1000, false, true);
    CHECK_RC(surf.Alloc(GetBoundGpuDevice()));

    if (GetRegion(pParams, &surf) != pProtectedRegion)
    {
        Printf(Tee::PriHigh, "%s: Protected allocation not in protected region\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    surf.Free();

    // Test allocations == size of region
    SetupSurface(&surf, pProtectedRegion->limit - pProtectedRegion->base + 1, false, true);
    rc = surf.Alloc(GetBoundGpuDevice());
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "%s: Could not allocate entire protected region\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    if (GetRegion(pParams, &surf) != pProtectedRegion)
    {
        Printf(Tee::PriHigh, "%s: Protected allocation not in protected region\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    surf.Free();

    // Test allocations > size of region
    SetupSurface(&surf, pProtectedRegion->limit - pProtectedRegion->base + 2, false, true);
    rc = surf.Alloc(GetBoundGpuDevice());
    if (rc == OK)
    {
        Printf(Tee::PriHigh, "%s: Allocation > protected size succeeded\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    else
        rc.Clear();

    surf.Free();

    // Test fixed address allocations w/protected flag set
    SetupSurface(&surf, 0x1000, false, true, false, pProtectedRegion->base);
    rc = surf.Alloc(GetBoundGpuDevice());

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "%s: Could not make fixed address allocation in protected region.\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    if (GetRegion(pParams, &surf) != pProtectedRegion)
    {
        Printf(Tee::PriHigh, "%s: Protected allocation not in protected region\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    if (GetPhysicalAddress(&surf, 0) != pProtectedRegion->base)
    {
        Printf(Tee::PriHigh, "%s: Fixed address allocation at wrong address\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    surf.Free();

    // Test fixed address allocations w/o protected flag set
    SetupSurface(&surf, 0x1000, false, false, false, pProtectedRegion->base ? pProtectedRegion->base : pProtectedRegion->base + 0x1000);
    rc = surf.Alloc(GetBoundGpuDevice());

    if (rc == OK)
    {
        Printf(Tee::PriHigh, "%s: Fixed address allocation in protected region incorrectly succeeded\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    else
        rc.Clear();

    surf.Free();

    // Test range based allocs w/protected flag set
    SetupSurface(&surf, 0x1000, false, true, false, pProtectedRegion->base, pProtectedRegion->limit);
    rc = surf.Alloc(GetBoundGpuDevice());

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "%s: Could not make range allocation in protected region.\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    if (GetRegion(pParams, &surf) != pProtectedRegion)
    {
        Printf(Tee::PriHigh, "%s: Protected allocation not in protected region\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    if (GetPhysicalAddress(&surf, 0) < pProtectedRegion->base ||
            GetPhysicalAddress(&surf, 0) > pProtectedRegion->limit)
    {
        Printf(Tee::PriHigh, "%s: Range address allocation at wrong address\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    surf.Free();

    // Test range based allocs w/o protected flag set
    SetupSurface(&surf, 0x1000, false, false, false, pProtectedRegion->base, pProtectedRegion->limit);
    rc = surf.Alloc(GetBoundGpuDevice());

    if (rc == OK)
    {
        Printf(Tee::PriHigh, "%s: range allocation in protected region incorrectly succeeded\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    else
        rc.Clear();

    surf.Free();

    // Test range based allocs that cross region boundaries w/protected set
    SetupSurface(&surf, 0x1000, false, true, false, 0, pProtectedRegion->limit * 2);
    rc = surf.Alloc(GetBoundGpuDevice());

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "%s: Could not make range allocation crossing regions w/protected set.\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    if (GetRegion(pParams, &surf) != pProtectedRegion)
    {
        Printf(Tee::PriHigh, "%s: Protected allocation not in protected region\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    surf.Free();

    // Test range based allocs that cross region boundaries w/o protected set
    SetupSurface(&surf, 0x1000, false, false, false, 0, pProtectedRegion->limit * 2);
    rc = surf.Alloc(GetBoundGpuDevice());

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "%s: Could not make range allocation crossing regions w/o protected set.\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    if (GetRegion(pParams, &surf) == pProtectedRegion)
    {
        Printf(Tee::PriHigh, "%s: non-protected allocation in protected region\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    surf.Free();

    // Test ISO allocations.
    SetupSurface(&surf, 0x1000, true, true);
    rc = surf.Alloc(GetBoundGpuDevice());

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "%s: Could not allocate ISO surface in protected region\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    if (GetRegion(pParams, &surf) != pProtectedRegion)
    {
        Printf(Tee::PriHigh, "%s: Protected allocation not in protected region\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    surf.Free();

    // Test that sysmem allocs to VPR work.
    SetupSurface(&surf, 0x1000, false, true);
    surf.SetLocation(Memory::NonCoherent);
    rc = surf.Alloc(GetBoundGpuDevice());
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "%s: Unable to create protected sysmem memory\n",
                __FUNCTION__);
        surf.Free();
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    surf.Free();

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC FbRegionsTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ FbRegionsTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(FbRegionsTest, RmTest,
                 "FbRegionsTest RM test.");
