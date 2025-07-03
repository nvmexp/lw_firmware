/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdamemtest.h"
#include "gpu/utility/chanwmgr.h"
#include "core/include/memerror.h"
#include "core/include/jsonlog.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/testdevice.h"

//! \brief Default name for LwdaTests
//------------------------------------------------------------------------------
LwdaMemTest::LwdaMemTest()
{
    SetName("LwdaMemTest");
}

//! \brief Determine whether a corresponding LWCA device has successfuly been bound
bool LwdaMemTest::IsSupported()
{
#ifndef USE_LWDA_SYSTEM_LIB
    if (!GpuMemTest::IsSupported())
        return false;

    if (ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
        return false;

    return m_LwdaDevice.IsValid();
#else
    return false;
#endif
}

/**
 * Override GpuMemTest's BindTestDevice, so we can figure out which LWCA
 * device instance matches this gpu device.
 */
RC LwdaMemTest::BindTestDevice(TestDevicePtr pTestDevice, JSContext* cx, JSObject* obj)
{
    RC rc;
    CHECK_RC(GpuMemTest::BindTestDevice(pTestDevice, cx, obj));

    auto pSubdev = pTestDevice->GetInterface<GpuSubdevice>();
    CHECK_RC(m_Lwda.Init());
    if (pSubdev)
    {
        m_Lwda.FindDevice(*(pSubdev->GetParentDevice()), &m_LwdaDevice);
    }
    return rc;
}

Lwca::Device LwdaMemTest::GetBoundLwdaDevice() const
{
    if (!m_LwdaDevice.IsValid())
    {
        MASSERT(!"A LWCA test should not be exelwted if LWCA device hasn't been successfuly identified!\n");
    }
    return m_LwdaDevice;
}

RC LwdaMemTest::Setup()
{
    RC rc;
    CHECK_RC(GpuMemTest::Setup());
    CHECK_RC(AllocDisplay());
    CHECK_RC(m_Lwda.InitContext(m_LwdaDevice));
    return OK;
}

RC LwdaMemTest::Cleanup()
{
    m_AllocatedBlocks.clear();
    StickyRC rc;
    rc = FreeEntireFramebuffer();
    m_Lwda.Free();
    rc = GpuMemTest::Cleanup();
    return rc;
}

RC LwdaMemTest::DisplayChunk
(
    GpuUtility::MemoryChunkDesc* pPreferredChunk,
    UINT64 preferredOffset
)
{
    // Attempt to obtain display pitch
    TestConfiguration* const pTestConfig = GetTestConfiguration();
    UINT32 pitch = (pTestConfig->SurfaceWidth() *
                    pTestConfig->DisplayDepth() / 8);
    if (OK != AdjustPitchForScanout(&pitch))
    {
        return OK;
    }

    GpuUtility::MemoryChunkDesc *pDisplayChunk = pPreferredChunk;
    UINT64 displayOffset = preferredOffset;
    const RC trc = GpuUtility::FindDisplayableChunk(
            GetChunks(),
            &pDisplayChunk,
            &displayOffset,
            GetStartLocation(),
            GetEndLocation(),
            pTestConfig->SurfaceHeight(),
            pitch,
            GetBoundGpuDevice());
    if (trc == OK)
    {
        RC rc;
        CHECK_RC(GetDisplayMgrTestContext()->DisplayMemoryChunk(
            pDisplayChunk,
            displayOffset,
            pTestConfig->SurfaceWidth(),
            pTestConfig->SurfaceHeight(),
            pTestConfig->DisplayDepth(),
            pitch));
    }
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Identify the type of ModsTest subclass
//!
/* virtual */ bool LwdaMemTest::IsTestType(TestType tt)
{
    return (tt == LWDA_MEM_TEST) || GpuMemTest::IsTestType(tt);
}

JS_CLASS(Dummy);

RC LwdaMemTest::AllocateEntireFramebuffer()
{
    GpuDevice* pGpuDevice = GetBoundGpuDevice();
    RC rc;

    // Get largest page possible to prevent performance issues due to
    // TLB ilwalidation when testing large regions of memory.
    // The LWCA driver lwrrently does not support pages bigger than HUGE.
    const UINT64 pageSize =
        pGpuDevice->GetMaxPageSize(0, GpuDevice::PAGESIZE_HUGE);

    const bool contiguous = !GetBoundGpuSubdevice()->IsSOC();
    CHECK_RC(AllocateEntireFramebuffer2(m_UseBlockLinear, pageSize, pageSize,
                                        0, contiguous));

    MASSERT(m_Chunks.empty());
    m_Chunks.clear();
    for (GpuUtility::MemoryChunks::const_iterator mit = GetChunks()->begin();
         mit != GetChunks()->end(); ++mit)
    {
        m_Chunks.push_back(m_Lwda.MapClientMem(mit->hMem, 0, mit->size, pGpuDevice));
        if (m_Chunks.back().GetSize() < mit->size)
        {
            Printf(Tee::PriError, "Failed to map memory into LWCA client\n");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
    }
    MASSERT(m_Chunks.size() == GetChunks()->size());

    // Initialize JS array with physical address ranges of allocated blocks
    m_AllocatedBlocks.clear();
    JavaScriptPtr pJs;
    for (GpuUtility::MemoryChunks::const_iterator mit = GetChunks()->begin();
         mit != GetChunks()->end(); ++mit)
    {
        const UINT64 StartPhys = mit->contiguous ? mit->fbOffset : 0;
        const UINT64 EndPhys = StartPhys + mit->size;
        JSContext *pContext;
        CHECK_RC(pJs->GetContext(&pContext));

        JSObject* pObj = JS_NewObject(pContext, &DummyClass, 0, 0);
        jsval val;
        CHECK_RC(pJs->ToJsval(StartPhys, &val));
        CHECK_RC(pJs->SetPropertyJsval(pObj, "StartPhys", val));
        CHECK_RC(pJs->ToJsval(EndPhys, &val));
        CHECK_RC(pJs->SetPropertyJsval(pObj, "EndPhys", val));
        CHECK_RC(pJs->SetProperty(pObj, "PteKind", mit->pteKind));
        CHECK_RC(pJs->SetProperty(pObj, "PageSizeKB", mit->pageSizeKB));
        val = OBJECT_TO_JSVAL(pObj);
        m_AllocatedBlocks.push_back(val);
    }
    return OK;
}

RC LwdaMemTest::FreeEntireFramebuffer()
{
    m_Chunks.clear();
    return GpuUtility::FreeEntireFramebuffer(GetChunks(), GetBoundGpuDevice());
}

const GpuUtility::MemoryChunkDesc& LwdaMemTest::GetChunkDesc(UINT32 i) const
{
    const GpuUtility::MemoryChunks &chunks = *GetChunks();
    return chunks[i];
}

GpuUtility::MemoryChunkDesc& LwdaMemTest::GetChunkDesc(UINT32 i)
{
    GpuUtility::MemoryChunks &chunks = *GetChunks();
    return chunks[i];
}

UINT32 LwdaMemTest::FindChunkByVirtualAddress(UINT64 virt) const
{
    for (UINT32 i=0; i < GetChunks()->size(); ++i)
    {
        const UINT64 devptr = m_Chunks[i].GetDevicePtr();
        const UINT64 size = m_Chunks[i].GetSize();
        if ((virt >= devptr) && (virt < devptr + size))
        {
            return i;
        }
    }
    return ~0U;
}

// If testing dGpu, directly getting phys addr from chunk desc. is a lot
// faster and reduces thrashing from multiple threads
UINT64 LwdaMemTest::ContiguousVirtualToPhysical(UINT64 virt) const
{
    const UINT32 i = FindChunkByVirtualAddress(virt);
    if (i == ~0U)
    {
        return ~0ULL;
    }

    const GpuUtility::MemoryChunkDesc& chunk = GetChunkDesc(i);
    MASSERT(chunk.contiguous);

    const UINT64 offs = virt - m_Chunks[i].GetDevicePtr();
    return chunk.fbOffset + offs;
}

UINT64 LwdaMemTest::VirtualToPhysical(UINT64 virt) const
{
    const UINT32 i = FindChunkByVirtualAddress(virt);
    if (i == ~0U)
    {
        return ~0ULL;
    }

    const GpuUtility::MemoryChunkDesc& chunk = GetChunkDesc(i);

    const UINT64 offs = virt - m_Chunks[i].GetDevicePtr();
    PHYSADDR phys = ~0ULL;
    if (OK != GpuUtility::GetPhysAddress(chunk, offs, &phys, nullptr))
    {
        Printf(Tee::PriNormal,
               "Unable to determine physical address for GPU virtual address 0x%llx\n",
               virt);
    }
    return phys;
}

UINT64 LwdaMemTest::PhysicalToVirtual(UINT64 phys) const
{
    GpuUtility::MemoryChunks::const_iterator mit = GetChunks()->begin();
    for (size_t i=0; mit != GetChunks()->end(); ++mit, ++i)
    {
        if (mit->contiguous)
        {
            const UINT64 physptr = mit->fbOffset;
            const UINT64 size = mit->size;
            if ((phys >= physptr) && (phys < physptr + size))
            {
                return m_Chunks[i].GetDevicePtr() + (phys - physptr);
            }
        }
    }
    return ~static_cast<UINT64>(0U);
}

RC LwdaMemTest::PhysicalToVirtual(UINT64* begin, UINT64* end) const
{
    MASSERT(begin);
    MASSERT(end);
    GpuUtility::MemoryChunks::const_iterator mit = GetChunks()->begin();
    for (size_t i=0; mit != GetChunks()->end(); ++mit, ++i)
    {
        if (mit->contiguous)
        {
            const UINT64 physptr = mit->fbOffset;
            const UINT64 size = mit->size;
            if ((*begin >= physptr) && (*begin <= physptr + size) &&
                (*end >= physptr) && (*end <= physptr + size))
            {
                *begin = m_Chunks[i].GetDevicePtr() + (*begin - physptr);
                *end = m_Chunks[i].GetDevicePtr() + (*end - physptr);
                return OK;
            }
        }
    }
    Printf(Tee::PriError,
           "Invalid virtual address - does not match any allocated chunk!\n");
    MASSERT(!"Invalid offset!");
    return RC::SOFTWARE_ERROR;
}

RC LwdaMemTest::SetAllocatedBlocks(JsArray* val)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

RC LwdaMemTest::GetAllocatedBlocks(JsArray* val) const
{
    MASSERT(val != NULL);
    *val = m_AllocatedBlocks;
    return OK;
}

/* virtual */ void LwdaMemTest::LogError
(
    UINT64 fbOffset,
    UINT32 actual,
    UINT32 expected,
    UINT32 reread1,
    UINT32 reread2,
    UINT64 iteration,
    const GpuUtility::MemoryChunkDesc& chunk
)
{
    ReportedError error;
    error.Address = fbOffset;
    error.Actual = actual;
    error.Expected = expected;
    error.Reread1 = reread1;
    error.Reread2 = reread2;
    error.PteKind = chunk.pteKind;
    error.PageSizeKB = chunk.pageSizeKB;
    m_ReportedErrors[error].insert(iteration);
    if (iteration > 0xFFFFFFFF)
        GetJsonExit()->AddFailLoop(0xFFFFFFFF);
    else
        GetJsonExit()->AddFailLoop((UINT32)iteration);
}

/* virtual */ RC LwdaMemTest::ReportErrors()
{
    RC rc;

    // Log all bad memory locations first
    for (const auto &err: m_ReportedErrors)
    {
        CHECK_RC(GetMemError(0).LogMemoryError(
                        32,
                        err.first.Address,
                        err.first.Actual,
                        err.first.Expected,
                        err.first.Reread1,
                        err.first.Reread2,
                        err.first.PteKind,
                        err.first.PageSizeKB,
                        *err.second.begin(),
                        "",
                        0));
    }

    // Log repeated memory failures
    for (const auto &err: m_ReportedErrors)
    {
        bool isFirstTimeStamp = true;
        for (const auto timeStamp: err.second)
        {
            if (!isFirstTimeStamp)
            {
                CHECK_RC(GetMemError(0).LogMemoryError(
                                32,
                                err.first.Address,
                                err.first.Actual,
                                err.first.Expected,
                                err.first.Reread1,
                                err.first.Reread2,
                                err.first.PteKind,
                                err.first.PageSizeKB,
                                timeStamp,
                                "",
                                0));
            }
            isFirstTimeStamp = false;
        }
    }

    m_ReportedErrors.clear();

    return rc;
}

//------------------------------------------------------------------------------
// JS linkage
//------------------------------------------------------------------------------
JS_CLASS_VIRTUAL_INHERIT(LwdaMemTest, GpuMemTest,
                         "LWCA memory test base class");

CLASS_PROP_READWRITE_JSARRAY(LwdaMemTest, AllocatedBlocks, JsArray,
                     "Array of physical addresses of testable memory blocks, read only, valid after Setup()");
CLASS_PROP_READWRITE(LwdaMemTest, UseBlockLinear, bool,
                     "Bool: Use BlockLinear memory allocation (default is false).");
