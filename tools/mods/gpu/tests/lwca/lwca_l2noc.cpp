/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpusbdev.h"
#include "gpu/tests/lwdastst.h"
#include "gpu/tests/lwca/l2noc/l2noc.h"
#include "gpu/include/gpupm.h"

class LwdaL2Noc : public LwdaStreamTest
{
public:
    LwdaL2Noc()
    : LwdaStreamTest()
    {
        SetName("LwdaL2Noc");
    }

    bool IsSupported() override;
    void PrintJsProperties(Tee::Priority pri) override;
    RC InitFromJs() override;
    RC Setup() override;
    RC Cleanup() override;
    RC Run() override;

    // JS property accessors
    SETGET_PROP(RuntimeMs, UINT32);
    SETGET_PROP(InnerIterations, UINT32);
    SETGET_PROP(MaxErrors, UINT64);
    SETGET_PROP(ReadVerify, bool);
    SETGET_PROP(RunExperiments, bool);
    SETGET_PROP(L2AllocationPercent, FLOAT32);
    SETGET_PROP(BypassNoc, bool);

private:
    RC ReportErrors();
    void SetRandomPatterns();

    L2NocParams m_Params = {};
    std::array<UINT32, 2> m_UGpuSmCount = {{0, 0}};
    Lwca::Module        m_Module;
    Lwca::ClientMemory  m_LwdaMem;
    Surface2D           m_LwdaMemSurf;
    Lwca::HostMemory    m_LwdaResultsMem;
    Lwca::DeviceMemory  m_NumErrorsPtr;
    Lwca::HostMemory    m_HostNumErrors;
    Random              m_Random;

    // Map SMID to ID within uGPU
    Lwca::HostMemory    m_SmidMap;

    // List of offsets that belong to each uGPU
    Lwca::HostMemory m_Ugpu0ChunkArr;
    Lwca::HostMemory m_Ugpu1ChunkArr;

    UINT32 m_NumThreads = 0;
    UINT32 m_SmCount = 0;
    UINT32 m_DwordPerChunk = 0;
    UINT32 m_PteKind = 0;
    UINT32 m_PageSizeKB = 0;

    // JS properties
    UINT32 m_RuntimeMs = 10000;
    UINT32 m_InnerIterations = 8192;
    UINT64 m_MaxErrors = 1024;
    bool   m_ReadVerify = false;
    bool   m_RunExperiments = false;
    FLOAT32 m_L2AllocationPercent = 1.0; // 100% by default
    bool   m_BypassNoc = false;
};

JS_CLASS_INHERIT(LwdaL2Noc, LwdaStreamTest, "L2-NOC Stress Test");

bool LwdaL2Noc::IsSupported()
{
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }

    // This test only supports L2-NOC connecting 2 uGPUs.
    const UINT32 mask = GetBoundGpuSubdevice()->GetUGpuMask(); 
    if (Utility::CountBits(mask) != 2)
    {
        Printf(Tee::PriLow, "L2-NOC not present or cannot be tested on this chip!\n");
        return false;
    }

    return true;
}

void LwdaL2Noc::PrintJsProperties(Tee::Priority pri)
{
    LwdaStreamTest::PrintJsProperties(pri);

    Printf(pri, "LwdaL2Noc Js Properties:\n");
    Printf(pri, "    RuntimeMs:                      %u\n", m_RuntimeMs);
    Printf(pri, "    InnerIterations:                %u\n", m_InnerIterations);
    Printf(pri, "    WritePattern: ");
    for (UINT32 i = 0; i < MAX_PATS; i++)
    {
        Printf(pri, "0x%08X ", m_Params.patterns.p[i]);
    }
    Printf(pri, "\n");
    Printf(pri, "    ReadVerify:                    %s\n", m_ReadVerify ? "true" : "false");
    Printf(pri, "    RunExperiments:                %s\n", m_RunExperiments ? "true" : "false");
}

RC LwdaL2Noc::InitFromJs()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::InitFromJs());

    // Check if write patterns were specified
    JavaScriptPtr pJs;
    vector<UINT32> propVals;
    rc = pJs->GetProperty(GetJSObject(), "WritePattern", &propVals);
    
    if (rc == RC::ILWALID_OBJECT_PROPERTY)
    {
        // If no pattern was provided, we'll exit early and use default patterns
        return RC::OK;
    }

    CHECK_RC(rc);

    if (propVals.size() != MAX_PATS)
    {
        Printf(Tee::PriError, "An incorrect number of WritePattern values (%zu) was provided, "
                              "expected: %u\n", propVals.size(), MAX_PATS);
        return RC::ILWALID_ARGUMENT;
    }

    // Copy over the patterns in reverse order due to endianess
    for (UINT32 i = 0; i < propVals.size(); i++)
    {
        m_Params.patterns.p[i] = propVals[propVals.size() - i - 1];
    }

    return rc;
}

void LwdaL2Noc::SetRandomPatterns()
{
    // Seed the RNG
    m_Random.SeedRandom(GetTestConfiguration()->Seed());

    // Initialize the write patterns with random data
    for (UINT32 i = 0; i < MAX_PATS; i++)
    {
        m_Params.patterns.p[i] = m_Random.GetRandom();
    }
}

RC LwdaL2Noc::Setup()
{
    RC rc;

    // Set random write patterns as default
    SetRandomPatterns();

    CHECK_RC(LwdaStreamTest::Setup());

    // Initialize module
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("l2noc", &m_Module));

    // To saturate, start with L2 size
    UINT32 allocMemSizeBytes =
            GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_L2_CACHE_SIZE);
    VerbosePrintf("L2 Cache size: %d\n", allocMemSizeBytes);

    // Split between the UGPUs
    if (!m_BypassNoc)
    {
        allocMemSizeBytes /= 2;
    }
    allocMemSizeBytes = (INT32)((FLOAT32)allocMemSizeBytes * m_L2AllocationPercent);
    
    const UINT32 allocMemSizeDword = allocMemSizeBytes / sizeof(UINT32);

    m_SmCount = GetBoundLwdaDevice().GetShaderCount();
    m_NumThreads = GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);

    VerbosePrintf("SmCount = %d, NumThreads = %d\n", m_SmCount, m_NumThreads);

    CHECK_RC(GetLwdaInstance().AllocSurfaceClientMem(allocMemSizeBytes, Memory::Fb,
                                                     GetBoundGpuSubdevice(),
                                                     &m_LwdaMemSurf, &m_LwdaMem));
    UINT32 m_PteKind = m_LwdaMemSurf.GetPteKind();
    UINT32 m_PageSizeKB = m_LwdaMemSurf.GetActualPageSizeKB();

    VerbosePrintf("Allocated %d bytes\n", allocMemSizeBytes);

    // Step 1: Create list of offsets that belong on each uGPU
    UINT64 physPtr = 0;
    CHECK_RC(m_LwdaMemSurf.GetPhysAddress(0, &physPtr, GetBoundRmClient()));
    Printf(Tee::PriLow, "Physical pointer: 0x%0llx\n", physPtr);

    // Verify that memory is DWORD aligned
    MASSERT(!(physPtr & 0x3));

    std::vector<UINT32> uGpu0ChunkArr;
    std::vector<UINT32> uGpu1ChunkArr;

    FbDecode fbDecode;
    FrameBuffer *fb = GetBoundGpuSubdevice()->GetFB();
    std::array<UINT32, 2> uGpuChunkCount = {{0, 0}};

    // Get the stride for m_DwordPerChunk
    INT32 firstUGpu = 0;
    for (UINT32 i = 0; i < allocMemSizeDword; i++)
    {
        UINT64 byteOffset = i * sizeof(UINT32);
        CHECK_RC(m_LwdaMemSurf.GetPhysAddress(byteOffset, &physPtr, GetBoundRmClient()));
        CHECK_RC(fb->DecodeAddress(&fbDecode, physPtr,
                                   m_PteKind, m_PageSizeKB));
        UINT32 ltcid = fb->VirtualFbioToVirtualLtc(
                                fbDecode.virtualFbio, fbDecode.subpartition);

        INT32 lwrrUGpu = GetBoundGpuSubdevice()->GetUGpuFromLtc(ltcid);
        if (lwrrUGpu < 0)
        {
            // Either no uGPUs present or something is wrong with FB registers
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
        }
        if (i == 0)
        {
            firstUGpu = lwrrUGpu;
        }
        else if (firstUGpu != lwrrUGpu)
        {
            m_DwordPerChunk = i;
            break;
        }
    }

    VerbosePrintf("uGPU stride is %d dwords\n", m_DwordPerChunk);
    MASSERT(m_DwordPerChunk);

    // Walk through the entire allocated surface to categorize the uGPU each chunk
    // of memory belongs to.
    for (UINT32 offset = 0; offset < allocMemSizeDword; offset += m_DwordPerChunk)
    {
        UINT64 byteOffset = offset * sizeof(UINT32);
        CHECK_RC(m_LwdaMemSurf.GetPhysAddress(byteOffset, &physPtr, GetBoundRmClient()));
        CHECK_RC(fb->DecodeAddress(&fbDecode, physPtr,
                                   m_PteKind, m_PageSizeKB));
        UINT32 ltcid = fb->VirtualFbioToVirtualLtc(
                                    fbDecode.virtualFbio, fbDecode.subpartition);

        INT32 lwrrUGpu = GetBoundGpuSubdevice()->GetUGpuFromLtc(ltcid);
        if (lwrrUGpu < 0)
        {
            // Either no uGPUs present or something is wrong with FB registers
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
        }
        Printf(Tee::PriLow, "uGpu %d at byte offset 0x%llux and ltcid %d \n",
                            lwrrUGpu, byteOffset, ltcid);
        if (lwrrUGpu == 0)
        {
            uGpu0ChunkArr.push_back(offset);
        }
        else
        {
            uGpu1ChunkArr.push_back(offset);
        }
    }

    uGpuChunkCount[0] = static_cast<UINT32>(uGpu0ChunkArr.size());
    uGpuChunkCount[1] = static_cast<UINT32>(uGpu1ChunkArr.size());

    VerbosePrintf("UGPU0 has %d mem chunks, UGPU1 has %d mem chunks\n",
                   uGpuChunkCount[0],  uGpuChunkCount[1]);

    // Copy the chunk arrays to host mem
    CHECK_RC(GetLwdaInstance().AllocHostMem(uGpuChunkCount[0] * sizeof(UINT32), &m_Ugpu0ChunkArr));
    std::copy(uGpu0ChunkArr.begin(), uGpu0ChunkArr.end(),
                static_cast<UINT32*>(m_Ugpu0ChunkArr.GetPtr()));

    CHECK_RC(GetLwdaInstance().AllocHostMem(uGpuChunkCount[1] * sizeof(UINT32), &m_Ugpu1ChunkArr));
    std::copy(uGpu1ChunkArr.begin(), uGpu1ChunkArr.end(),
                static_cast<UINT32*>(m_Ugpu1ChunkArr.GetPtr()));

    // Step 2: create SMID map. For each SM, give it a new SMID within the uGPU it belongs to.
    // Also embed the uGPU id within this new SMID.
    CHECK_RC(GetLwdaInstance().AllocHostMem(m_SmCount * sizeof(UINT32), &m_SmidMap));
    UINT32* pSmidMap = static_cast<UINT32*>(m_SmidMap.GetPtr());

    for (UINT32 smid = 0; smid < m_SmCount; smid++)
    {
        UINT32 hwGpcNum = -1;
        UINT32 hwTpcNum = -1;
        CHECK_RC(GetBoundGpuSubdevice()->SmidToHwGpcHwTpc(smid, &hwGpcNum, &hwTpcNum));
        INT32 uGpu = GetBoundGpuSubdevice()->GetUGpuFromGpc(hwGpcNum);

        if (uGpu < 0)
        {
            // Either no uGPUs present or something wrong with gpc/tpc mappings
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
        }

        INT32 uGpuRemote = ((uGpu == 0) ? 1 : 0);

        // Each SM needs a remote offset into FB.
        // If we already have more SMs than chunks for this uGPU then set to ILWALID_UGPU_SMID
        if (m_UGpuSmCount[uGpu] >= uGpuChunkCount[uGpuRemote])
        {
            pSmidMap[smid] = ILWALID_UGPU_SMID;
            continue;
        }

        // embed uGPU id in msb
        pSmidMap[smid] = m_UGpuSmCount[uGpu] | (uGpu << UGPU_INDEX);
        m_UGpuSmCount[uGpu]++;
        Printf(Tee::PriLow, "SM %d has uGpu-SMID 0x%x\n", smid,  pSmidMap[smid]);
    }

    // Step 3: Set kernel parameters
    // The actual pointer to memory
    m_Params.dataPtr            = m_LwdaMem.GetDevicePtr();
    // The map of regular SMID to uGPU-SMID
    m_Params.smidMap            = m_SmidMap.GetDevicePtr(GetBoundLwdaDevice());
    // array of chunks (offsets) on each uGPU
    // The kernel expects to be accessing the partner uGpu's LTCs, if we want to bypass Noc
    // we need to swap the Gpu pointers.
    if (m_BypassNoc)
    {
        m_Params.uGpu0ChunkArr      = m_Ugpu1ChunkArr.GetDevicePtr(GetBoundLwdaDevice());
        m_Params.uGpu1ChunkArr      = m_Ugpu0ChunkArr.GetDevicePtr(GetBoundLwdaDevice());
    }
    else
    {
        m_Params.uGpu0ChunkArr      = m_Ugpu0ChunkArr.GetDevicePtr(GetBoundLwdaDevice());
        m_Params.uGpu1ChunkArr      = m_Ugpu1ChunkArr.GetDevicePtr(GetBoundLwdaDevice());
        
    }
    // Number of chunks each SM will write.
    // Handle the case when we have weird floorsweeping and all TPCs on one UGPU is disabled.
    m_Params.chunksPerSMUGpu0 = (m_UGpuSmCount[0] == 0) ? 0 : MAX(1, uGpuChunkCount[1] / m_UGpuSmCount[0]);
    m_Params.chunksPerSMUGpu1 = (m_UGpuSmCount[1] == 0) ? 0 : MAX(1, uGpuChunkCount[0] / m_UGpuSmCount[1]);
    m_Params.dwordPerChunk      = m_DwordPerChunk;
    m_Params.innerIterations    = m_InnerIterations;

    VerbosePrintf("UGPU0: %u SMs each writing %d chunks of %zu bytes\n",
                  m_UGpuSmCount[0], m_Params.chunksPerSMUGpu0, (m_DwordPerChunk * sizeof(UINT32)));
    VerbosePrintf("UGPU1: %u SMs each writing %d chunks of %zu bytes\n",
                  m_UGpuSmCount[1], m_Params.chunksPerSMUGpu1, (m_DwordPerChunk * sizeof(UINT32)));

    // Error reporting setup
    // Allocate memory for results
    const UINT64 errorSizeBytes = m_MaxErrors * sizeof(L2NocBadValue);
    CHECK_RC(GetLwdaInstance().AllocHostMem(errorSizeBytes, &m_LwdaResultsMem));
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(sizeof(UINT64), &m_NumErrorsPtr));
    CHECK_RC(GetLwdaInstance().AllocHostMem(sizeof(UINT64), &m_HostNumErrors));

    m_Params.errorPtr = m_LwdaResultsMem.GetDevicePtr(GetBoundLwdaDevice());
    m_Params.maxErrors = m_MaxErrors;
    m_Params.numErrorsPtr = m_NumErrorsPtr.GetDevicePtr();
    m_Params.readVerify = m_ReadVerify;

    return rc;
}

RC LwdaL2Noc::Run()
{
    RC rc;
    double durationMs = 0.0;

    // Sanity check to make sure memory chunks will saturate threads
    MASSERT((m_DwordPerChunk % m_NumThreads) == 0);

    // Get Run function
    Lwca::Function runFunc = m_Module.GetFunction("L2NocTest", m_SmCount, m_NumThreads);
    CHECK_RC(runFunc.InitCheck());

    if (m_RuntimeMs)
    {
        CHECK_RC(PrintProgressInit(m_RuntimeMs));
    }
    else
    {
        CHECK_RC(PrintProgressInit(GetTestConfiguration()->Loops()));
    }

    // Create events for timing kernel
    Lwca::Event startEvent(GetLwdaInstance().CreateEvent());
    Lwca::Event stopEvent(GetLwdaInstance().CreateEvent());

    if (m_RunExperiments)
    {
        // Ilwalidate L2 cache before clearing tested memory
        // so that the clearing will fully initialize L2
        // to minimize number of L2 misses during the test
        CHECK_RC(GetBoundGpuSubdevice()->IlwalidateL2Cache(0));

        GpuPerfmon* pPerfMon = nullptr;
        CHECK_RC(GetBoundGpuSubdevice()->GetPerfmon(&pPerfMon));
        if (!pPerfMon)
        {
            Printf(Tee::PriError, "Missing perfmon pointer.\n");
            return RC::SOFTWARE_ERROR;
        }
        CHECK_RC(pPerfMon->BeginExperiments());
    }
    // Run for RuntimeMs if it is set otherwise Run for TestConfiguration.Loops loops.
    UINT64 loop = 0;
    for
    (
        loop = 0;
        m_RuntimeMs ?
            durationMs < static_cast<double>(m_RuntimeMs) :
            loop < GetTestConfiguration()->Loops();
        loop++
    )
    {
        // Clear errors before each loop
        CHECK_RC(m_NumErrorsPtr.Clear());

        // Launch kernel, recording elaspsed time with events
        CHECK_RC(startEvent.Record());
        CHECK_RC(runFunc.Launch(m_Params));
        CHECK_RC(stopEvent.Record());

        // Synchronize and get time for kernel completion
        GetLwdaInstance().Synchronize();
        durationMs += stopEvent.TimeMsElapsedSinceF(startEvent);

        // Update test progress
        if (m_RuntimeMs)
        {
            CHECK_RC(PrintProgressUpdate(std::min(static_cast<UINT64>(durationMs),
                                                  static_cast<UINT64>(m_RuntimeMs))));
        }
        else
        {
            CHECK_RC(PrintProgressUpdate(loop + 1));
        }
        
        if (m_ReadVerify)
        {
            CHECK_RC(ReportErrors());
        }
    }
    if (m_RunExperiments)
    {
        GpuPerfmon* pPerfMon = nullptr;
        CHECK_RC(GetBoundGpuSubdevice()->GetPerfmon(&pPerfMon));
        if (!pPerfMon)
        {
            Printf(Tee::PriError, "Missing perfmon pointer.\n");
            return RC::SOFTWARE_ERROR;
        }
        CHECK_RC(pPerfMon->EndExperiments());
    }

    // Callwlate total bandwidth
    UINT64 chunksTransferred  = 
        (static_cast<UINT64>(m_Params.chunksPerSMUGpu0) * m_UGpuSmCount[0]) + 
        (static_cast<UINT64>(m_Params.chunksPerSMUGpu1) * m_UGpuSmCount[1]);
    UINT64 bytesTransferred = 
        chunksTransferred  * loop * m_InnerIterations * m_DwordPerChunk * sizeof(UINT32);

    if (durationMs > 0.0)
    {
        GpuTest::RecordBps(static_cast<double>(bytesTransferred),
                            durationMs, (Tee::Priority)(GetVerbosePrintPri()));
    }

    return rc;
}

RC LwdaL2Noc::ReportErrors()
{
    RC rc;

    FbDecode fbDecode;
    FrameBuffer *fb = GetBoundGpuSubdevice()->GetFB();

    Printf(Tee::PriLow, "ReportErrors\n");

    CHECK_RC(m_NumErrorsPtr.Get(m_HostNumErrors.GetPtr(), m_HostNumErrors.GetSize()));
    GetLwdaInstance().Synchronize();

    UINT64 numErrors = *static_cast<UINT64*>(m_HostNumErrors.GetPtr());
    const L2NocBadValue *errors = (const L2NocBadValue *)(m_LwdaResultsMem.GetPtr());
    UINT64 numReportedErrors = min(numErrors, m_MaxErrors);

    // if no errors, exit early
    if (numErrors == 0)
    {
        return RC::OK;
    }

    VerbosePrintf("Number of errors: %llu, number of reported errors %llu\n",
                   numErrors, numReportedErrors);

    UINT64 physPtr = 0;

    for (UINT64 i = 0; i < numReportedErrors; i++)
    {
        CHECK_RC(m_LwdaMemSurf.GetPhysAddress(errors[i].offset, &physPtr, GetBoundRmClient()));
        CHECK_RC(fb->DecodeAddress(&fbDecode, physPtr, m_PteKind, m_PageSizeKB));
        const UINT32 ltcid = fb->VirtualFbioToVirtualLtc(
                             fbDecode.virtualFbio, fbDecode.subpartition);
        const UINT32 l2Slice = fb->GetFirstGlobalSliceIdForLtc(ltcid) + fbDecode.slice;

        VerbosePrintf(" #%llu: Addr=0x%010llX Offset=0x%08llX Exp=0x%08X Act=0x%08X "
                        "B=%u T=%u Bits=0x%08X Smid=%u Slice=%u\n",
                        i,
                        physPtr,
                        errors[i].offset,
                        errors[i].expected,
                        errors[i].actual,
                        errors[i].block,
                        errors[i].thread,
                        errors[i].failingBits,
                        errors[i].smid,
                        l2Slice);
    }

    if (numErrors)
    {
        Printf(Tee::PriError, "LwdaL2Noc detected memory miscompares!\n");
        rc = RC::BAD_MEMORY;
    }

    return rc;
}

RC LwdaL2Noc::Cleanup()
{

    m_LwdaMem.Free();
    m_LwdaMemSurf.Free();
    m_SmidMap.Free();
    m_Ugpu0ChunkArr.Free();
    m_Ugpu1ChunkArr.Free();
    m_LwdaResultsMem.Free();

    m_NumErrorsPtr.Free();
    m_HostNumErrors.Free();

    m_Module.Unload();

    return LwdaStreamTest::Cleanup();
}

CLASS_PROP_READWRITE(LwdaL2Noc, RuntimeMs, UINT32,
                     "Maximum amount of time the test can run for");
CLASS_PROP_READWRITE(LwdaL2Noc, InnerIterations, UINT32,
                     "Number of times to repeatedly perform reads/writes inside LwdaL2Noc kernel");
CLASS_PROP_READWRITE(LwdaL2Noc, MaxErrors, UINT64,
                     "Max number of errors reported (0=default)");
CLASS_PROP_READWRITE(LwdaL2Noc, ReadVerify, bool,
                     "Read back to verify data after writes");
CLASS_PROP_READWRITE(LwdaL2Noc, RunExperiments, bool,
                     "Runs perfmon experiments along with the test.");
CLASS_PROP_READWRITE(LwdaL2Noc, L2AllocationPercent, FLOAT32,
                    "Percentage 0.1 - 1.0 of the total L2 cache to allocate.");
CLASS_PROP_READWRITE(LwdaL2Noc, BypassNoc, bool,
                    "Bypass L2 Noc access on partner GPU."); 
