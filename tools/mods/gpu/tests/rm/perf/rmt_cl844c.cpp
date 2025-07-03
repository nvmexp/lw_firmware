/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2013,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Object allocation test
//

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "class/cl9097.h" // FERMI_A
#include "class/cl9097sw.h"
#include "class/cl844c.h" // G84_PERFBUFFER
#include "core/utility/errloggr.h"
#include "core/include/refparse.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/memcheck.h"

// We test this much memory in bytes
#define BUF_SIZE 1024 * 10

class Class844cTest: public RmTest
{
public:
    Class844cTest();
    virtual ~Class844cTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    // Setter/Getter functions for properties
    SETGET_PROP(EnableLimitVTest, bool);
private:
    RC SetRegisterValue(const char *RegisterName,
                        const char *RegisterField,
                        const char *RegisterValue);
    bool m_EnableLimitVTest;
};

//------------------------------------------------------------------------------
Class844cTest::Class844cTest()
: m_EnableLimitVTest(false)
{
    SetName("Class844cTest");
}

//------------------------------------------------------------------------------
Class844cTest::~Class844cTest()
{
}

//------------------------------------------------------------------------------
string Class844cTest::IsTestSupported()
{
    // fermi only for now!
    if( IsClassSupported(G84_PERFBUFFER) )
        return RUN_RMTEST_TRUE;
    return "G84_PERFBUFFER class is not supported on current platform";
}

//------------------------------------------------------------------------------
RC Class844cTest::Setup()
{
    // TODO - Fill this in
    return OK;
}

//------------------------------------------------------------------------------
RC Class844cTest::SetRegisterValue(const char *RegisterName,
                                   const char *RegisterField,
                                   const char *RegisterValue)
{
    return OK;
}

//------------------------------------------------------------------------------
RC Class844cTest::Run()
{
    RC           rc;
    LwRmPtr      pLwRm;
    LwRm::Handle hPerf = 0;
    LwRm::Handle hMem = 0;
    UINT32       i;
    UINT64       Offset1;
    UINT64       Offset2;
    UINT64       Offset3;
    UINT08       *pMem;
    LwRm::Handle hMemPerf1 = 0;
    LwRm::Handle hMemPerf2 = 0;
    LwRm::Handle hMemPerf3 = 0;
    LwU64        perfOffset;
    UINT32       value;

    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    // Create the PERF object
    CHECK_RC(pLwRm->Alloc(
        pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        &hPerf,
        G84_PERFBUFFER,
        NULL));

    // allocate 3 chunks of mem, we will map the 3rd one
    CHECK_RC_CLEANUP(pLwRm->VidHeapAllocSize(
        LWOS32_TYPE_IMAGE, 0, LWOS32_ATTR2_NONE, BUF_SIZE, &hMemPerf1, &Offset1,
        nullptr, nullptr, nullptr,
        GetBoundGpuDevice()));
    CHECK_RC_CLEANUP(pLwRm->VidHeapAllocSize(
        LWOS32_TYPE_IMAGE, 0, LWOS32_ATTR2_NONE, BUF_SIZE, &hMemPerf2, &Offset2,
        nullptr, nullptr, nullptr,
        GetBoundGpuDevice()));
    CHECK_RC_CLEANUP(pLwRm->VidHeapAllocSize(
        LWOS32_TYPE_IMAGE, 0, LWOS32_ATTR2_NONE, BUF_SIZE, &hMemPerf3, &Offset3,
        nullptr, nullptr, nullptr,
        GetBoundGpuDevice()));

    // will use this to locally read perfbuffer memory
    CHECK_RC_CLEANUP(pLwRm->MapMemory(hMemPerf3, 0, BUF_SIZE, (void **)&pMem, 0, GetBoundGpuSubdevice()));

    // this will map our memory region into the perfbuffer vspace
    CHECK_RC_CLEANUP(pLwRm->MapMemoryDma(hPerf, hMemPerf3, 0, BUF_SIZE, 0, &perfOffset, GetBoundGpuDevice()));

    Printf(Tee::PriHigh, "PERF buffer address : 0x%llx\n", perfOffset);

    // read the first 20 words of the perfbuffer and print contents
    for (i = 0; i < 20; i++)
    {
        value = MEM_RD32(pMem + (i << 2));
        Printf(Tee::PriHigh, "PERF buffer Read : 0x%08x\n", value);
    }

    // Unmap the PERF buffer memory
    pLwRm->UnmapMemory(hMemPerf3, pMem, 0, GetBoundGpuSubdevice());
    pLwRm->UnmapMemoryDma(hPerf, hMemPerf3, 0, perfOffset, GetBoundGpuDevice());

Cleanup:
    pLwRm->Free(hMem);
    pLwRm->Free(hPerf);
    pLwRm->Free(hMemPerf1);
    pLwRm->Free(hMemPerf2);
    pLwRm->Free(hMemPerf3);

    ErrorLogger::TestCompleted();
    ErrorLogger::PrintErrors(Tee::PriHigh);

    return rc;
}

//------------------------------------------------------------------------------
RC Class844cTest::Cleanup()
{
    // TODO - Fill this in
    return OK;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Class844cTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class844cTest, RmTest,
                 "PERF buffer test.");
