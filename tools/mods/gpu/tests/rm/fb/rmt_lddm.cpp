/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef LW_LDDM

//
// Object allocation test
//

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "class/cl506f.h"
#include "core/utility/errloggr.h"
#include "core/include/memcheck.h"

class ClassLddmTest: public RmTest
{
public:
    ClassLddmTest();
    virtual ~ClassLddmTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwU32                          m_verif_flags;
    LwRm::Handle                   m_Ch[2];
    LwRm::Handle                   m_hErrMem[2];
    LwRm::Handle                   m_hErrCtxDma[2];
    LwRm::Handle                   m_hPbMem[2];
    LwRm::Handle                   m_hPbCtxDma[2];
    LwRm::Handle                   m_pbSize[2];
    LwRm::Handle                   m_gpEntries;
    LwRm::Handle                   m_gpMem[2];
    LwRm::Handle                   m_gpCtxDma[2];
    void*                          m_errorAddress[2];
    void*                          m_pbAddress[2];
    void*                          m_gpAddress[2];
    LwRm::Handle                   m_vpHandle[2];

    RC allocChannel(unsigned int);
};

//------------------------------------------------------------------------------
ClassLddmTest::ClassLddmTest()
{
    SetName("ClassLddmTest");
    m_gpEntries = 0x100;
}

//------------------------------------------------------------------------------
ClassLddmTest::~ClassLddmTest()
{
}

//------------------------------------------------------------------------------
string ClassLddmTest::IsTestSupported()
{
    // XXX Lwrrently report this test as "unsupported", since it's not known
    // to be reliable yet
    return "this test is unsupported, since it's not known to be reliable yet";
}

//------------------------------------------------------------------------------
RC ClassLddmTest::Setup()
{
    // TODO - Fill this in
    return OK;
}

//------------------------------------------------------------------------------
RC ClassLddmTest::Run()
{
    RC         rc;
    LwRmPtr    pLwRm;
    void*      RmHandle;

    CHECK_RC(allocChannel(0));

    CHECK_RC(pLwRm->VidHeapAllocHint(
        LWOS32_TYPE_IMAGE,
        DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM)      |
        DRF_DEF(OS32, _ATTR, _COMPR, _NONE)           |
        DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH),
        LWOS32_ALLOC_FLAGS_NO_SCANOUT | LWOS32_ALLOC_FLAGS_IGNORE_BANK_PLACEMENT,
        100, 100,
        10,
        100,
        RmHandle));

    CHECK_RC(pLwRm->VidHeapBindVirtualAddress(RmHandle, 0x40090000, 0x750000));

    CHECK_RC(pLwRm->VidHeapAllocHint(
        LWOS32_TYPE_DEPTH,
        DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM)      |
        DRF_DEF(OS32, _ATTR, _COMPR, _REQUIRED)       |
        DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR)  |
        DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z24S8)     |
        DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1)         |
        DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED),
        LWOS32_ALLOC_FLAGS_NO_SCANOUT | LWOS32_ALLOC_FLAGS_IGNORE_BANK_PLACEMENT,
        100, 100,
        10,
        100,
        RmHandle));

    CHECK_RC(pLwRm->VidHeapBindVirtualAddress(RmHandle, 0x40080000, 0x750000));

    #if 0
    CHECK_RC(pLwRm->VidHeapAllocHint(
        LWOS32_TYPE_DEPTH,
        DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM)      |
        DRF_DEF(OS32, _ATTR, _COMPR, _NONE)           |
        DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR)  |
        DRF_DEF(OS32, _ATTR, _ZLWLL, _REQUIRED),
        LWOS32_ALLOC_FLAGS_NO_SCANOUT | LWOS32_ALLOC_FLAGS_IGNORE_BANK_PLACEMENT,
        100, 100,
        10,
        100,
        RmHandle));

    CHECK_RC(pLwRm->VidHeapBindResource(
        RmHandle,
        TRUE));

    CHECK_RC(pLwRm->VidHeapSetOffset(
         RmHandle,
         75000));

    CHECK_RC(pLwRm->VidHeapEnableResource(
        RmHandle,
        TRUE));

    CHECK_RC(pLwRm->VidHeapResourceDelete(
        RmHandle));
    #endif

    return rc;
}

//------------------------------------------------------------------------------
RC ClassLddmTest::Cleanup()
{
    // TODO - Fill this in
    return OK;
}

RC ClassLddmTest::allocChannel(unsigned int id)
{
    LwRmPtr     pLwRm;
    RC          rc;
    LwU32       errFlags;
    LwU32       pbFlags;
    LwU32       gpFlags;
    LwU32       gpSize;
    LwU32       errSize;
    LwU32       dmaFlags;

    errSize  = 16;
    errFlags = DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI)           |
               DRF_DEF(OS02, _FLAGS, _COHERENCY, _WRITE_BACK)   |
               DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS);

    CHECK_RC(pLwRm->AllocSystemMemory(&m_hErrMem[id], errFlags, errSize));
    dmaFlags = DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE)       |
               DRF_DEF(OS03, _FLAGS, _GPU_ADDR_SPACE, _VIRTUAL);

    CHECK_RC(pLwRm->AllocContextDma(&m_hErrCtxDma[id], dmaFlags, m_hErrMem[id], 0, errSize - 1));
    CHECK_RC(pLwRm->MapMemory(m_hErrMem[id], 0, errSize, &m_errorAddress[id]));

    gpSize  = m_gpEntries * 8;
    gpFlags = DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI)            |
                DRF_DEF(OS02, _FLAGS, _COHERENCY, _WRITE_BACK)  |
                DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS);
    CHECK_RC(pLwRm->AllocSystemMemory(&m_gpMem[id], gpFlags, gpSize));
    dmaFlags = DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_ONLY)       |
               DRF_DEF(OS03, _FLAGS, _GPU_ADDR_SPACE, _VIRTUAL);

    CHECK_RC(pLwRm->AllocContextDma(&m_gpCtxDma[id], dmaFlags, m_gpMem[id], 0, gpSize - 1));
    CHECK_RC(pLwRm->MapMemory(m_gpMem[id], 0, gpSize, &m_gpAddress[id]));

    m_pbSize[id] = 1024*1024;
    pbFlags = DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI)            |
              DRF_DEF(OS02, _FLAGS, _COHERENCY, _WRITE_BACK)    |
              DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS);
    CHECK_RC(pLwRm->AllocSystemMemory(&m_hPbMem[id], pbFlags, m_pbSize[id]));
    dmaFlags = DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_ONLY)       |
               DRF_DEF(OS03, _FLAGS, _GPU_ADDR_SPACE, _VIRTUAL);

    CHECK_RC(pLwRm->AllocContextDma(&m_hPbCtxDma[id], dmaFlags, m_hPbMem[id], 0, m_pbSize[id] - 1));
    CHECK_RC(pLwRm->MapMemory(m_hPbMem[id], 0, m_pbSize[id], &m_pbAddress[id]));

    m_verif_flags = 0;

    rc = pLwRm->AllocChannelGpFifo(
          &m_Ch[id], LW50_CHANNEL_GPFIFO,
          m_hErrCtxDma[id],
          m_errorAddress[id],
          m_hPbCtxDma[id],
          m_pbAddress[id],
          0,
          m_pbSize[id],
          m_gpAddress[id],
          0,
          m_gpEntries,
          0,            // context object
          m_verif_flags // verifFlags
          0,            // verifFlags2
          0);           // flags

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ ClassLddmTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(ClassLddmTest, RmTest,
                 "LDDM test suite");
#endif
