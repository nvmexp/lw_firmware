/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2011,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_safe64bitaddrop.cpp
//! \brief Safe 64 Bit Address Operations Test
//!
//! Tests that we can allocate FB above 4GB, map it into GPU and read/write from it.
//!
//! TODO this test should test RM APIs that have physical addresses as input
//! and output to verify no addresses are truncated.
//!

#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "class/cl9097.h" // FERMI_A
#include "class/cl906f.h"
#include "class/cla06fsubch.h"
#include "gpu/tests/rmtest.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"

#define GR_OBJ_SUBCH LWA06F_SUBCHANNEL_3D

class SafeRm64bitAddrOpTest : public RmTest
{
public:
    SafeRm64bitAddrOpTest();
    virtual ~SafeRm64bitAddrOpTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:

    PHYSADDR GetPhysicalAddress(Surface2D* pSurf, LwU64 offset);
    UINT64           m_gpuAddr = 0;
    UINT32 *         m_cpuAddr = nullptr;

    Channel *        m_pCh     = nullptr;
    LwRm::Handle     m_hCh     = 0;

    Surface2D m_fbSurf;
    ThreeDAlloc m_threedAlloc;
};

//! \brief SafeRm64bitAddrOpTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
SafeRm64bitAddrOpTest::SafeRm64bitAddrOpTest()
{
    SetName("SafeRm64bitAddrOpTest");
    m_threedAlloc.SetOldestSupportedClass(FERMI_A);
}

//! \brief SafeRm64bitAddrOpTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
SafeRm64bitAddrOpTest::~SafeRm64bitAddrOpTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True Fermi+
//-----------------------------------------------------------------------------
string SafeRm64bitAddrOpTest::IsTestSupported()
{
    if (m_threedAlloc.IsSupported(GetBoundGpuDevice()))
    {
        if (GetBoundGpuSubdevice()->FbHeapSize() >= (1ULL<<32))
            return RUN_RMTEST_TRUE;
        else
            return "Only w/4G+ of FB";

    }
    else
        return "Supported only on Fermi+";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC SafeRm64bitAddrOpTest::Setup()
{
    RC           rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh));

    m_fbSurf.SetForceSizeAlloc(true);
    m_fbSurf.SetArrayPitch(1);
    m_fbSurf.SetPageSize(4);
    m_fbSurf.SetLayout(Surface2D::Pitch);
    m_fbSurf.SetAddressModel(Memory::Paging);
    m_fbSurf.SetColorFormat(ColorUtils::VOID32);
    m_fbSurf.SetArraySize(0x1000);
    m_fbSurf.SetGpuPhysAddrHint(1ULL<<32);
    m_fbSurf.SetGpuPhysAddrHintMax(1ULL<<63);
    m_fbSurf.SetGpuVirtAddrHint(1ULL<<32);
    m_fbSurf.SetGpuVirtAddrHintMax(1ULL<<63);
    m_fbSurf.SetLocation(Memory::Fb);
    CHECK_RC(m_fbSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_fbSurf.Map());

    if (GetPhysicalAddress(&m_fbSurf, 0) < (1ULL<<32))
        CHECK_RC(RC::SOFTWARE_ERROR);

    if (m_fbSurf.GetCtxDmaOffsetGpu() < (1ULL<<32))
        CHECK_RC(RC::SOFTWARE_ERROR);

    m_cpuAddr = (UINT32*)m_fbSurf.GetAddress();
    m_gpuAddr = m_fbSurf.GetCtxDmaOffsetGpu();

    CHECK_RC(m_threedAlloc.Alloc(m_hCh, GetBoundGpuDevice()));

    return rc;
}

//! \brief Gets the physical address of an offset into a surface
//!
//! \return A physical address
//-----------------------------------------------------------------------------
PHYSADDR SafeRm64bitAddrOpTest::GetPhysicalAddress(Surface2D* pSurf, LwU64 offset)
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

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC SafeRm64bitAddrOpTest::Run()
{
    RC rc = OK;
    const UINT32 semExp = 0x12345678;
    UINT32       semGot;

    //!
    //! Host Semaphore testing
    //!
    // Set the semaphore location to zeros

    MEM_WR32(m_cpuAddr + 0x0, 0x00000000);
    MEM_WR32(m_cpuAddr + 0x1, 0x00000000);

    CHECK_RC(m_pCh->SetObject(GR_OBJ_SUBCH, m_threedAlloc.GetHandle()));
    CHECK_RC(m_pCh->SetSemaphoreOffset(m_gpuAddr));
    CHECK_RC(m_pCh->SemaphoreRelease(semExp));

    CHECK_RC(m_pCh->Write(GR_OBJ_SUBCH, LW9097_SET_REPORT_SEMAPHORE_A, 4,
                LwU64_HI32(m_gpuAddr+0x4), LwU64_LO32(m_gpuAddr+0x4), semExp,
                DRF_DEF(9097, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
                DRF_DEF(9097, _SET_REPORT_SEMAPHORE_D, _STRUCTURE_SIZE, _ONE_WORD) |
                DRF_DEF(9097, _SET_REPORT_SEMAPHORE_D, _PIPELINE_LOCATION, _ALL)
                ));

    m_pCh->Flush();
    // Don't wait on the test semaphores, if they do go wrong we want to detect and fail, not hang
    CHECK_RC(m_pCh->WaitIdle(m_TestConfig.TimeoutMs()));

    semGot = MEM_RD32(m_cpuAddr);
    if (semGot != semExp)
    {
        Printf(Tee::PriHigh, "SafeRm64bitAddrOpTest: host semaphore value incorrect\n");
        Printf(Tee::PriHigh, "               expected 0x%x, got 0x%x\n",  semExp, semGot);
        return RC::DATA_MISMATCH;
    }

    semGot = MEM_RD32(m_cpuAddr+0x1);
    if (semGot != semExp)
    {
        Printf(Tee::PriHigh, "SafeRm64bitAddrOpTest: graphics semaphore value incorrect\n");
        Printf(Tee::PriHigh, "               expected 0x%x, got 0x%x\n",  semExp, semGot);
        return RC::DATA_MISMATCH;
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC SafeRm64bitAddrOpTest::Cleanup()
{
    RC rc, firstRc;

    m_fbSurf.Free();

    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ SafeRm64bitAddrOpTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(SafeRm64bitAddrOpTest, RmTest,
                 "SafeRm64bitAddrOpTest Tests that RM can handle phys/virt memory above 32b.");
