/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Class95a1Sec2Hwv test.
//

#include "rmt_cl95a1sec2.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "maxwell/gm107/dev_fuse.h"
#include "random.h"
#include "tsec_drv.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>

#define HWV_NANOBYTES_IN_BYTE      (1e9)
#define HWV_NANOTICKS_IN_TICK      (1e9)
#define HWV_HZ_IN_MEGAHZ           (1e6)
#define HWV_BYTES_IN_MEGABYTE      (1024 * 1024)

/*!
 * Class95a1Sec2HwvTest
 *
 */
class Class95a1Sec2HwvTest: public RmTest
{
public:
    Class95a1Sec2HwvTest();
    virtual ~Class95a1Sec2HwvTest();
    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC AllocateMem(Sec2MemDesc *, Memory::Location, LwU32);
    virtual RC FreeMem(Sec2MemDesc*);
    virtual RC SendSemaMethod();
    virtual RC PollSemaMethod();
    virtual RC StartMethodStream(const char*, LwU32);
    virtual RC EndMethodStream(const char*);
    virtual RC RunPerfTest();

    SETGET_PROP(dmaLocation, string);
    SETGET_PROP(dmaSize, LwU32);

private:
    Sec2MemDesc m_semMemDesc;
    Sec2MemDesc m_cmdMemDesc;
    Sec2MemDesc m_inputMemDesc;
    Sec2MemDesc m_outputMemDesc;

    FLOAT64          GetThroughputMBs(LwU32 bytesRead, LwU32 timeTakenNs);
    FLOAT64          GetAvgFreqMHz(LwU32 totalTicks, LwU32 timeTakenNs);
    void             PrintPerfResults(hwv_perf_eval_cmd *pPerfCmd);
    Memory::Location MemLocationFromString();

    LwRm::Handle m_hObj;
    FLOAT64      m_TimeoutMs;
    string       m_dmaLocation;
    LwU32        m_dmaSize;

    GpuSubdevice *m_pSubdev;
};

#define HWV_BUFFER_SIZE   (1024)

//------------------------------------------------------------------------
Class95a1Sec2HwvTest::Class95a1Sec2HwvTest() :
    m_hObj(0),
    m_TimeoutMs(Tasker::NO_TIMEOUT),
    m_pSubdev(nullptr)
{
    SetName("Class95a1Sec2HwvTest");

    m_dmaLocation = "";
    m_dmaSize = 0;
}

//------------------------------------------------------------------------
Class95a1Sec2HwvTest::~Class95a1Sec2HwvTest()
{
}

//------------------------------------------------------------------------
string Class95a1Sec2HwvTest::IsTestSupported()
{
    if( IsClassSupported(LW95A1_TSEC) )
        return RUN_RMTEST_TRUE;
    return "LW95A1 class is not supported on current platform";
}

//------------------------------------------------------------------------
RC Class95a1Sec2HwvTest::AllocateMem(Sec2MemDesc *pMemDesc, Memory::Location memLocation, LwU32 size)
{
    RC rc = OK;
    Surface2D *pVidMemSurface = &(pMemDesc->surf2d);
    pVidMemSurface->SetForceSizeAlloc(1);
    pVidMemSurface->SetLayout(Surface2D::Pitch);
    pVidMemSurface->SetColorFormat(ColorUtils::Y8);
    pVidMemSurface->SetLocation(memLocation);
    pVidMemSurface->SetArrayPitchAlignment(256);
    pVidMemSurface->SetArrayPitch(1);
    pVidMemSurface->SetArraySize(size);
    pVidMemSurface->SetAddressModel(Memory::Paging);
    pVidMemSurface->SetLayout(Surface2D::Pitch);

    CHECK_RC(pVidMemSurface->Alloc(GetBoundGpuDevice()));
    CHECK_RC(pVidMemSurface->Map());

    pMemDesc->gpuAddrSysMem  = pVidMemSurface->GetCtxDmaOffsetGpu();
    pMemDesc->pCpuAddrSysMem = (LwU32*)pVidMemSurface->GetAddress();
    pMemDesc->size = size;

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2HwvTest::FreeMem(Sec2MemDesc *pMemDesc)
{
    (pMemDesc->surf2d).Free();
    return OK;
}

//------------------------------------------------------------------------
RC Class95a1Sec2HwvTest::Setup()
{
    RC      rc = OK;
    Memory::Location location = Memory::Coherent;
    LwRmPtr pLwRm;

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_SEC2));
    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObj, LW95A1_TSEC, NULL));
    m_pCh->SetObject(0, m_hObj);

    Printf(Tee::PriHigh, "HWV: Allocating surface memory....\n");
    CHECK_RC(AllocateMem(&m_semMemDesc, Memory::Coherent, 0x4));
    CHECK_RC(AllocateMem(&m_cmdMemDesc, Memory::Coherent, sizeof(hwv_perf_eval_cmd)));

    // Initialize DMA buffers with specified size and locations
    if (m_dmaSize == 0)
    {
        m_dmaSize = HWV_BUFFER_SIZE;
        Printf(Tee::PriHigh, "HWV: Defaulting buffer size to %d bytes!\n",
               m_dmaSize);
    }

    location = MemLocationFromString();
    CHECK_RC(AllocateMem(&m_inputMemDesc,  location, m_dmaSize));
    CHECK_RC(AllocateMem(&m_outputMemDesc, location, m_dmaSize));

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2HwvTest::SendSemaMethod()
{
    m_pCh->Write(0, LW95A1_SEMAPHORE_A, DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (LwU32)(m_semMemDesc.gpuAddrSysMem >> 32LL)));
    m_pCh->Write(0, LW95A1_SEMAPHORE_B, (LwU32)(m_semMemDesc.gpuAddrSysMem & 0xffffffffLL));
    m_pCh->Write(0, LW95A1_SEMAPHORE_C, DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, 0x12345678));

    return OK;
}

//------------------------------------------------------------------------
RC Class95a1Sec2HwvTest::PollSemaMethod()
{
    RC            rc = OK;
    PollArguments args;

    args.value  = 0x12345678;
    args.pAddr  = (UINT32*)m_semMemDesc.pCpuAddrSysMem;
    Printf(Tee::PriHigh, "HWV: Polling started for pAddr %p Expecting 0x%0x\n", args.pAddr, args.value);
    rc = PollVal(&args, m_TimeoutMs);

    args.value = MEM_RD32(args.pAddr);
    Printf(Tee::PriHigh, "HWV: Semaphore value found: 0x%0x\n", args.value);

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2HwvTest::StartMethodStream(const char *pId, LwU32 sendSBMthd)
{
    RC rc = OK;

    Printf(Tee::PriHigh, "HWV: Starting method stream for %s \n", pId);
    MEM_WR32(m_semMemDesc.pCpuAddrSysMem, 0x87654321);

    m_pCh->Write(0, LW95A1_SET_APPLICATION_ID, LW95A1_SET_APPLICATION_ID_ID_HWV);
    m_pCh->Write(0, LW95A1_SET_WATCHDOG_TIMER, 0);
    SendSemaMethod();

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2HwvTest::EndMethodStream(const char *pId)
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
        Printf(Tee::PriHigh, "HWV: Polling on %s FAILED !!!!\n", pId);
    }
    else
    {
        Printf(Tee::PriHigh, "HWV: Polling on %s PASSED !!!!\n", pId);
    }

    Printf(Tee::PriHigh, "HWV: Time taken for this method stream '%llu' milliseconds!\n", elapsedTimeMS);

    return rc;
}

//------------------------------------------------------------------------
Memory::Location Class95a1Sec2HwvTest::MemLocationFromString()
{
    if (m_dmaLocation.compare("fb") == 0)
    {
        Printf(Tee::PriHigh, "HWV: Using FB.\n");
        return Memory::Fb;
    }
    else if (m_dmaLocation.compare("syscoh") == 0)
    {
        Printf(Tee::PriHigh, "HWV: Using coherent sysmem.\n");
        return Memory::Coherent;
    }
    else if (m_dmaLocation.compare("sysncoh") == 0)
    {
        Printf(Tee::PriHigh, "HWV: Using non-coherent sysmem.\n");
        return Memory::NonCoherent;
    }

    Printf(Tee::PriHigh, "HWV: Defaulting to coherent sysmem.\n");
    return Memory::Coherent;
}

//------------------------------------------------------------------------
FLOAT64 Class95a1Sec2HwvTest::GetThroughputMBs(LwU32 bytesRead, LwU32 timeTakenNs)
{
    FLOAT64 nBytesRead = bytesRead * HWV_NANOBYTES_IN_BYTE;
    FLOAT64 throughputMBs = (nBytesRead / timeTakenNs) / HWV_BYTES_IN_MEGABYTE;
    return throughputMBs;
}

//------------------------------------------------------------------------
FLOAT64 Class95a1Sec2HwvTest::GetAvgFreqMHz(LwU32 totalTicks, LwU32 timeTakenNs)
{
    FLOAT64 nTicks = totalTicks * HWV_NANOTICKS_IN_TICK;
    FLOAT64 totalFreqMHz = (nTicks / timeTakenNs) / HWV_HZ_IN_MEGAHZ;
    return totalFreqMHz;
}

//------------------------------------------------------------------------
void Class95a1Sec2HwvTest::PrintPerfResults(hwv_perf_eval_cmd *pPerfCmd)
{
    FLOAT64 avgThroughputMBs = 0;
    FLOAT64 avgFrequencyMHz  = 0;

    //
    // Validate proper input.  Include "no time taken" as invalid input,
    // as would result in divison by zero, and wouldn't provide useful results.
    //
    if (pPerfCmd == NULL ||
        pPerfCmd->perfResults.dmaReadTotalTicks == 0 ||
        pPerfCmd->perfResults.dmaWriteTotalTicks == 0)
    {
        Printf(Tee::PriHigh, "HWV: Invalid results struct.\n");
        return;
    }

    // Callwlate and print DMA read performance.
    avgThroughputMBs = GetThroughputMBs(pPerfCmd->inOutBufSize,
                                        pPerfCmd->perfResults.dmaReadTotalTimeNs);
    avgFrequencyMHz = GetAvgFreqMHz(pPerfCmd->perfResults.dmaReadTotalTicks,
                                    pPerfCmd->perfResults.dmaReadTotalTimeNs);

    Printf(Tee::PriHigh, "HWV: DMA read-to-SEC2 performance:\n");
    Printf(Tee::PriHigh, "HWV: %.3f MB/s at %.3f MHz average clock frequency.\n",
            avgThroughputMBs, avgFrequencyMHz);
    Printf(Tee::PriHigh, "HWV: %u bytes read taking %u nsecs\n",
            pPerfCmd->inOutBufSize, pPerfCmd->perfResults.dmaReadTotalTimeNs);

    // Callwlate and print DMA write performance.
    avgThroughputMBs = GetThroughputMBs(pPerfCmd->inOutBufSize,
                                        pPerfCmd->perfResults.dmaWriteTotalTimeNs);
    avgFrequencyMHz = GetAvgFreqMHz(pPerfCmd->perfResults.dmaWriteTotalTicks,
                                    pPerfCmd->perfResults.dmaWriteTotalTimeNs);

    Printf(Tee::PriHigh, "HWV: DMA write-from-SEC2 performance:\n");
    Printf(Tee::PriHigh, "HWV: %.3f MB/s at %.3f MHz average clock frequency.\n",
            avgThroughputMBs, avgFrequencyMHz);
    Printf(Tee::PriHigh, "HWV: %u bytes written taking %u nsecs\n",
            pPerfCmd->inOutBufSize, pPerfCmd->perfResults.dmaWriteTotalTimeNs);
}

//------------------------------------------------------------------------
RC Class95a1Sec2HwvTest::RunPerfTest()
{
    RC                rc          = OK;
    hwv_perf_eval_cmd *pPerfCmd   = (hwv_perf_eval_cmd *)m_cmdMemDesc.pCpuAddrSysMem;
    LwU8              *pIn        = NULL;
    LwU8              *pOut       = NULL;
    LwU32             i           = 0;

    // Initialize cmd structure
    memset(pPerfCmd, 0, sizeof(hwv_perf_eval_cmd));
    pPerfCmd->inGpuVA256   = (LwU32)(m_inputMemDesc.gpuAddrSysMem >> 8);
    pPerfCmd->outGpuVA256  = (LwU32)(m_outputMemDesc.gpuAddrSysMem >> 8);
    pPerfCmd->inOutBufSize = m_dmaSize;  

    //
    // Initialize input and output buffers.
    // Ensure output buffer does not match input initially.
    //
    pIn = (LwU8*)m_inputMemDesc.pCpuAddrSysMem;
    pOut = (LwU8*)m_outputMemDesc.pCpuAddrSysMem;

    for (i = 0; i < m_dmaSize; i++)
    {
        MEM_WR08(pIn + i, i);
        MEM_WR08(pOut + i, 0xFF);
    }

    // Send method data and run test
    Printf(Tee::PriHigh, "HWV: RunPerfTest start!\n");
    CHECK_RC(StartMethodStream("RunPerfTest", 1));
    m_pCh->Write(0, LW95A1_HWV_PERF_EVAL, (LwU32)(m_cmdMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("RunPerfTest"));

    // Validate input DMA buffer was correctly copied to output DMA buffer.
    for (i = 0; i < m_dmaSize; i++)
    {
        LwU8 inByte  = MEM_RD08(pIn + i);
        LwU8 outByte = MEM_RD08(pOut + i);
 
        if (outByte != inByte)
        {
            Printf(Tee::PriHigh, "HWV: WRONG! pIn[%d]=0x%0x while pOut[%d]=0x%0x\n",
                    i, inByte, i, outByte);
            rc = RC::SOFTWARE_ERROR;
            break;
        }
    }

    // If successful, callwlate and print performance measurements.
    if (i == m_dmaSize)
    {
        PrintPerfResults(pPerfCmd);
    }

    return rc;
}


//------------------------------------------------------------------------
RC Class95a1Sec2HwvTest::Run()
{
    RC rc = OK;

    CHECK_RC(RunPerfTest());

    return rc;
}

//------------------------------------------------------------------------------
RC Class95a1Sec2HwvTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    FreeMem(&m_semMemDesc);
    FreeMem(&m_cmdMemDesc);
    FreeMem(&m_inputMemDesc);
    FreeMem(&m_outputMemDesc);

    pLwRm->Free(m_hObj);
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Class95a1Sec2HwvTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class95a1Sec2HwvTest, RmTest, "Class95a1Sec2HwvTest RM test.");

CLASS_PROP_READWRITE(Class95a1Sec2HwvTest, dmaLocation, string,
                     "Location where DMA input/output buffers should be located. Options are 'fb', 'syscoh', 'sysncoh'.");

CLASS_PROP_READWRITE(Class95a1Sec2HwvTest, dmaSize, LwU32,
                     "Size of DMA buffers in bytes.");

