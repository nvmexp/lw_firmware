/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Class95a1Sec2Apm test.
//

#include "rmt_cl95a1sec2.h"
#include <stdlib.h>

#define APM_METHOD_RECEIVED_ACK 0xABCDDCBA

/*!
 * Class95a1Sec2ApmTest
 *
 */
class Class95a1Sec2ApmTest: public RmTest
{
public:
    Class95a1Sec2ApmTest();
    virtual ~Class95a1Sec2ApmTest();
    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC AllocateMem(Sec2MemDesc *, LwU32);
    virtual RC FreeMem(Sec2MemDesc*);
    virtual RC SendSemaMethod();
    virtual RC PollSemaMethod();
    virtual RC StartMethodStream(const char*, LwU32);
    virtual RC EndMethodStream(const char*);

private:
    Sec2MemDesc m_semMemDesc;

    LwRm::Handle m_hObj;
    FLOAT64      m_TimeoutMs;

    GpuSubdevice *m_pSubdev;
};

//------------------------------------------------------------------------
Class95a1Sec2ApmTest::Class95a1Sec2ApmTest() :
    m_hObj(0),
    m_pSubdev(nullptr)
{
    SetName("Class95a1Sec2ApmTest");
}

//------------------------------------------------------------------------
Class95a1Sec2ApmTest::~Class95a1Sec2ApmTest()
{
}

//------------------------------------------------------------------------
string Class95a1Sec2ApmTest::IsTestSupported()
{
    if( IsClassSupported(LW95A1_TSEC) )
        return RUN_RMTEST_TRUE;
    return "LW95A1 class is not supported on current platform";
}

//------------------------------------------------------------------------
RC Class95a1Sec2ApmTest::AllocateMem(Sec2MemDesc *pMemDesc, LwU32 size)
{
    Surface2D *pVidMemSurface = &(pMemDesc->surf2d);
    pVidMemSurface->SetForceSizeAlloc(1);
    pVidMemSurface->SetLayout(Surface2D::Pitch);
    pVidMemSurface->SetColorFormat(ColorUtils::Y8);
    pVidMemSurface->SetLocation(Memory::Coherent);
    pVidMemSurface->SetArrayPitchAlignment(256);
    pVidMemSurface->SetArrayPitch(1);
    pVidMemSurface->SetArraySize(size);
    pVidMemSurface->SetAddressModel(Memory::Paging);
    pVidMemSurface->SetLayout(Surface2D::Pitch);

    pVidMemSurface->Alloc(GetBoundGpuDevice());
    pVidMemSurface->Map();

    pMemDesc->gpuAddrSysMem = pVidMemSurface->GetCtxDmaOffsetGpu();
    pMemDesc->pCpuAddrSysMem = (LwU32*)pVidMemSurface->GetAddress();
    pMemDesc->size = size;

    return OK;
}

//------------------------------------------------------------------------
RC Class95a1Sec2ApmTest::FreeMem(Sec2MemDesc *pMemDesc)
{
    (pMemDesc->surf2d).Free();
    return OK;
}

//------------------------------------------------------------------------
RC Class95a1Sec2ApmTest::Setup()
{
    LwRmPtr      pLwRm;
    RC           rc;

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_SEC2));

    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObj, LW95A1_TSEC, NULL));
    m_pCh->SetObject(0, m_hObj);

    Printf(Tee::PriHigh, "APM: Allocating surface memory....\n");

    AllocateMem(&m_semMemDesc, 0x4);

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2ApmTest::SendSemaMethod()
{
    m_pCh->Write(0, LW95A1_SEMAPHORE_A, DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (LwU32)(m_semMemDesc.gpuAddrSysMem >> 32LL)));
    m_pCh->Write(0, LW95A1_SEMAPHORE_B, (LwU32)(m_semMemDesc.gpuAddrSysMem & 0xffffffffLL));
    m_pCh->Write(0, LW95A1_SEMAPHORE_C, DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, 0x12345678));

    return OK;
}

//------------------------------------------------------------------------
RC Class95a1Sec2ApmTest::PollSemaMethod()
{
    RC            rc = OK;
    PollArguments args;

    args.value  = 0x12345678;
    args.pAddr  = (UINT32*)m_semMemDesc.pCpuAddrSysMem;
    Printf(Tee::PriHigh, "APM: Polling started for pAddr %p: Expecting 0x%0x\n", args.pAddr, args.value);
    rc = PollVal(&args, m_TimeoutMs);

    args.value = MEM_RD32(args.pAddr);
    Printf(Tee::PriHigh, "APM: Semaphore value found: 0x%0x\n", args.value);

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2ApmTest::StartMethodStream(const char *pId, LwU32 sendSBMthd)
{
    RC              rc = OK;

    Printf(Tee::PriHigh, "APM: Starting method stream for %s \n", pId);
    MEM_WR32(m_semMemDesc.pCpuAddrSysMem, 0x87654321);

    m_pCh->Write(0, LW95A1_SET_APPLICATION_ID, LW95A1_SET_APPLICATION_ID_ID_APM);
    m_pCh->Write(0, LW95A1_SET_WATCHDOG_TIMER, 0);
    SendSemaMethod();

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2ApmTest::EndMethodStream(const char *pId)
{
    RC    rc           = OK;
    LwU64 startTimeMS  = 0;
    LwU64 elapsedTimeMS= 0;

    m_pSubdev = GetBoundGpuSubdevice();

    m_pCh->Write(0, LW95A1_EXELWTE, 0x1); // Notify on End
    m_pCh->Flush();

    startTimeMS = Platform::GetTimeMS();
    rc = PollSemaMethod();
    elapsedTimeMS = (Platform::GetTimeMS() - startTimeMS);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "APM: Polling on %s FAILED !!!!\n", pId);
    }
    else
    {
        Printf(Tee::PriHigh, "APM: Polling on %s PASSED !!!!\n", pId);
    }

    Printf(Tee::PriHigh, "APM: Time taken for this method stream '%llu' milliseconds!\n", elapsedTimeMS);

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2ApmTest::Run()
{
    RC rc = OK;

    return rc;
}

//------------------------------------------------------------------------------
RC Class95a1Sec2ApmTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    FreeMem(&m_semMemDesc);

    pLwRm->Free(m_hObj);
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Class95a1Sec2ApmTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class95a1Sec2ApmTest, RmTest, "Class95a1Sec2ApmTest RM test.");
