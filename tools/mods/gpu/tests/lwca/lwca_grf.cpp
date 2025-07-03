/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Object allocation test
//

#include "gpu/tests/lwdastst.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwca.h"
#include "class/cl50c0.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/jsonlog.h"
#include "core/include/utility.h"
#include <algorithm>
#include "lwmisc.h"

#define DRF_READV(a, drf)       (((a) >> DRF_SHIFT(drf)) & DRF_MASK(drf))

using namespace std;

class LwdaGrfTest : public LwdaStreamTest
{
public:
    LwdaGrfTest();
    ~LwdaGrfTest();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(Iterations, UINT32);
    SETGET_PROP(PrintDebugInfo, bool);

private:
    RC Run30AndUp();

    RC Run30Mats();
    RC FermiKeplerInitCheck();
    RC GenerateErrorCode(UINT32 NumRunningSMs,
                         bool   failedSW,
                         int    failedLoop,
                         bool   failed);
    RC Run30Random();
    RC Run30_CpuOperations();
    RC Run30_CheckResults();

    struct ThreadReport30
    {
        UINT32 NumRuns;
        UINT32 FailedBits;
        UINT32 LastCompletedStage;
        UINT32 DbgReg;
    };
    struct ThreadReport30_Rnd
    {
        UINT32 Dbg1;
        UINT32 Dbg2;
        UINT32 LocalCheckSum;
        UINT32 SharedCheckSum;
    };

    UINT32 m_Iterations     = 128;
    bool   m_PrintDebugInfo = false;

    // parameters for Run30Mats
    UINT32 m_TotalThreads = 96; // Default to 3 CTAs of 32 threads each for 1 SM
    vector <UINT64> m_Patterns;

    struct OperationInfo
    {
        UINT32   IsLocal;
        UINT32   ToL1;
        UINT32   DstOffset;
        UINT32   SrcOffset;
    };
    vector <OperationInfo> m_TestOps;
    Lwca::Module m_Module;

    vector <UINT32> m_FakeFb;
    struct FakeThreadMem
    {
        UINT32 FakeLocalSum;
        UINT32 FakeSharedSum;
        vector <UINT32> FakeLocal;
        vector <UINT32> FakeShared;
    };
    // state of each thread's L1 for one SM
    vector <FakeThreadMem> m_ThreadMem;

    // parameters for Run30_L1Random
    UINT32 m_NumSm              = 0;
    UINT32 m_CtaSize            = 0;
    UINT32 m_NumLocal           = 0;
    UINT32 m_NumShared          = 0;
    UINT32 m_NumCtaPerSm        = 0; // Num CTA per SM
    UINT32 m_NumUINT32PerThread = 0;
    UINT32 m_NumUINT32PerCta    = 0;
    UINT32 m_NumUINT32PerSm     = 0;
};

//------------------------------------------------------------------------------
LwdaGrfTest::LwdaGrfTest()
{
    SetName("LwdaGrfTest");
    m_Patterns.push_back(0x0000000000000000ULL);
    m_Patterns.push_back(0x5555555555555555ULL);
    m_Patterns.push_back(0xAAAAAAAAAAAAAAAAULL);
    m_Patterns.push_back(0xFFFFFFFFFFFFFFFFULL);
}

//------------------------------------------------------------------------------
LwdaGrfTest::~LwdaGrfTest()
{
}

//------------------------------------------------------------------------------
bool LwdaGrfTest::IsSupported()
{
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    // Deprecating on Pascal+ because this test has never produced a failure
    // and we are trying to reduce test time.
    if (GetBoundGpuSubdevice()->IsSOC() || cap >= Lwca::Capability::SM_60)
        return false;

    // This test does not work on CheetAh, it fails, and then every test which
    // runs after it fails as well.  This test has never been brought up on
    // CheetAh and since it is deprecated for Pascal, we do not have any plans
    // to fix it.
    if (Platform::IsTegra())
    {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
RC LwdaGrfTest::Setup()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());
    if (GetTestConfiguration()->ShortenTestForSim())
    {
        m_Iterations = 1;
    }

    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("lwgrf", &m_Module));
    return OK;
}

//------------------------------------------------------------------------------
RC LwdaGrfTest::Cleanup()
{
    m_TestOps.clear();
    m_FakeFb.clear();
    m_ThreadMem.clear();
    m_Module.Unload();
    return LwdaStreamTest::Cleanup();
}

//------------------------------------------------------------------------------
RC LwdaGrfTest::Run()
{
    return Run30AndUp();
}

RC LwdaGrfTest::FermiKeplerInitCheck()
{
    Lwca::Device device = GetBoundLwdaDevice();
    // The kernel assumes warp size of 32
    if (device.GetAttribute(LW_DEVICE_ATTRIBUTE_WARP_SIZE) != 32)
    {
        Printf(Tee::PriError, "Invalid warp size for LwdaGrfTest on device %s\n",
                device.GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    // The kernel assumes 48KB of shared memory
    if (device.GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK)
            != 48*1024)
    {
        Printf(Tee::PriError, "Invalid amount of shared memory for LwdaGrfTest on device %s\n",
                device.GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    // Check whether SM count matches between MODS and LWCA
    if (GetBoundGpuSubdevice()->ShaderCount() != device.GetShaderCount())
    {
        Printf(Tee::PriError, "Invalid number of SMs for LwdaGrfTest on device %s\n",
                device.GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC LwdaGrfTest::GenerateErrorCode
(
    UINT32 NumRunningSMs,
    bool   failedSW,
    int    failLoop,
    bool   failed
)
{
    const UINT32 NumSMs = GetBoundGpuSubdevice()->ShaderCount();
    // Check the number of actually tested SMs
    if (NumRunningSMs > NumSMs)
    {
        Printf(Tee::PriError,
               "Too many SMs reported to be running (%u, but there are only %u SMs enabled)\n",
               NumRunningSMs, NumSMs);
        failedSW = true;
    }
    else if (NumRunningSMs < NumSMs)
    {
        Printf(Tee::PriError, "Only %u SMs were scheduled to run (there are total %u SMs)\n",
               NumRunningSMs, NumSMs);
        failedSW = true;
    }

    // Report failed loop
    if (failLoop >= 0)
    {
        GetJsonExit()->AddFailLoop(failLoop);
    }

    // Return error code
    if (failedSW)
    {
        return RC::SOFTWARE_ERROR;
    }
    if (failed)
    {
        return RC::GPU_COMPUTE_MISCOMPARE;
    }
    return OK;
}

RC LwdaGrfTest::Run30AndUp()
{
    RC rc;
    m_NumSm = GetBoundGpuSubdevice()->ShaderCount();
    CHECK_RC(Run30Mats());

    // Todo: add 8bb shared memory access
    // Do we want to add a test for 8 bytes per bank mode shared memory
    // Lwrrently, LWCA driver doesn't support this (for fear that Fermi based
    // kernel would no longer work).

    CHECK_RC(Run30Random());

    return rc;
}

RC LwdaGrfTest::Run30Mats()
{
    RC rc;
    CHECK_RC(FermiKeplerInitCheck());
    Lwca::Device device = GetBoundLwdaDevice();
    const UINT32 NumSMs = GetBoundGpuSubdevice()->ShaderCount();
    const UINT32 MaxSMs = GetBoundGpuSubdevice()->GetMaxShaderCount();

    // Configure test kernels
    const UINT32 WarpSize = device.GetAttribute(LW_DEVICE_ATTRIBUTE_WARP_SIZE);

    // Each CTA tests 16KB of shared memory
    const UINT32 SharedMemTestedPerCta = 16 * 1024;

    UINT32 MaxSharedMemPerSM = 0;
    MaxSharedMemPerSM = device.GetAttribute(
                      LW_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_MULTIPROCESSOR);

    const UINT32 NumCtasPerSm = (MaxSharedMemPerSM / SharedMemTestedPerCta) +
                                 ((MaxSharedMemPerSM % SharedMemTestedPerCta) ?
                                  1 : 0);

    // Each CTA is assumed to be 32 threads (warp wide) in LWCA kernel
    m_TotalThreads = NumSMs * NumCtasPerSm * WarpSize;
    Lwca::Function L1MemTest = m_Module.GetFunction("L1MemoryTest");

    // Configure the grid per the number of CTAs being run
    L1MemTest.SetGridDim(NumCtasPerSm * NumSMs);
    L1MemTest.SetBlockDim(WarpSize);

    // Clear report
    Lwca::Global DevReport = m_Module["Report"];
    CHECK_RC(DevReport.Clear());

    // Placeholder for data returned from the kernel
    const UINT32 MaxCtasPerSM = 7; // to test 112KB with 16KB per CTA, we need 7 CTAs
    const UINT32 ReportSize = WarpSize * MaxCtasPerSM * MaxSMs;
    vector<ThreadReport30, Lwca::HostMemoryAllocator<ThreadReport30> >
        Report(ReportSize,
               ThreadReport30(),
               Lwca::HostMemoryAllocator<ThreadReport30>(GetLwdaInstancePtr(),
                                                         GetBoundLwdaDevice()));

    UINT32 devMemSize = UNSIGNED_CAST(UINT32, m_Patterns.size() * sizeof(UINT64));
    Lwca::DeviceMemory devMem;
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(devMemSize, &devMem));
    CHECK_RC(devMem.InitCheck());
    CHECK_RC(devMem.Set(&m_Patterns[0], devMemSize));
    CHECK_RC(GetLwdaInstance().Synchronize());

    // Perform specified number of loops
    Lwca::Event readEvent(GetLwdaInstance().CreateEvent());
    CHECK_RC(readEvent.InitCheck());
    const UINT32 NumLoops =
        max<UINT32>(1, GetTestConfiguration()->Loops() / 16);
    UINT32 iloop = 0;
    int failLoop = -1;
    for ( ; iloop < NumLoops; iloop++)
    {
        CHECK_RC(L1MemTest.Launch((UINT64)devMem.GetDevicePtr(),
                                  (UINT32)m_Patterns.size(),
                                  (UINT32)m_Iterations,
                                  (UINT32)m_TotalThreads));

        // Check for errors
        if (iloop > 0)
        {
            CHECK_RC(readEvent.Synchronize());
            if (failLoop < 0)
            {
                for (UINT32 i=0; i < Report.size(); i++)
                {
                    if (Report[i].FailedBits)
                    {
                        failLoop = iloop;
                        break;
                    }
                }
            }
        }

        // Read results
        CHECK_RC(DevReport.Get(&Report[0], sizeof(Report[0])*ReportSize));
        CHECK_RC(readEvent.Record());

        // Early termination
        if ((failLoop >= 0) && GetGoldelwalues()->GetStopOnError())
        {
            iloop++;
            break;
        }
        CHECK_RC(EndLoop(iloop));
    }

    // Wait for final results to be available
    CHECK_RC(readEvent.Synchronize());

    // Digest results
    bool failed = false;
    bool failedSW = false;
    UINT32 NumRunningSMs = 0;

    vector<UINT32> SMArray;
    SMArray.clear();

    // Scan the entire grid for each CTA
    for (UINT32 cta=0, i=0; cta < MaxCtasPerSM * MaxSMs; cta++)
    {
        // Get the per CTA offset
        i = (cta * 32);
        // Find the SM we are running from the 1st thread on this CTA
        UINT32 sm = ((Report[i].LastCompletedStage >> 16) & 0xFFFF);

        // Determine whether the number of runs was consistent across all lanes
        bool Consistent = true;
        for (UINT32 lane=1; lane < WarpSize; lane++)
        {
            if (Report[i].NumRuns != Report[i+lane].NumRuns)
            {
                Consistent = false;
                break;
            }
        }

        // Check particular lanes for this SM
        bool Running = false;
        for (UINT32 lane=0; lane < WarpSize; lane++, i++)
        {
            // Verify that the kernel has been run the expected number of times
            if (Report[i].NumRuns != 0)
            {
                Printf(GetVerbosePrintPri(), "Ran: SM %u, CTA %u, lane%s%u\n",
                       sm,
                       cta,
                       (Consistent?"s 0-":" "),
                       (Consistent?(WarpSize-1):lane));
                Running = true;
                if ((Report[i].NumRuns != iloop) && ((lane == 0) || !Consistent))
                {
                    Printf(Tee::PriError,
                           "SM %u, CTA %u, lane%s%u exelwted %u times (expected %u)\n",
                           sm,
                           cta,
                           (Consistent?"s 0-":" "),
                           (Consistent?(WarpSize-1):lane),
                           Report[i].NumRuns,
                           iloop);
                    failedSW = true;
                }
            }

            // Check if there were any failures
            if (Report[i].FailedBits != 0)
            {
                Printf(Tee::PriError, "SM %u, CTA %u, lane %u encountered failing bits 0x%08x\n",
                       sm, cta, lane, Report[i].FailedBits);
                failed = true;
                if (failLoop < 0)
                {
                    failLoop = NumLoops - 1;
                }
            }

            if (m_PrintDebugInfo)
            {
                Printf(Tee::PriNormal,
                       "Debug: SM %u, CTA %u, lane %u: StageCompleted: %d, DbgReg=0x%x\n",
                       sm, cta, lane, Report[i].LastCompletedStage, Report[i].DbgReg);
            }
        }

        // Count SMs on which the kernel ran
        if (Running)
        {
            vector<UINT32>::iterator it;
            it = std::find(SMArray.begin(), SMArray.end(), sm);
            if (it == SMArray.end())
            {
                NumRunningSMs++;
                SMArray.push_back(sm);
            }
        }
    }
    return GenerateErrorCode(NumRunningSMs, failedSW, failLoop, failed);
}

//! Brief: random access on L1 (local and shared)
// Note: in this scenario, we don't really care about what banks
// This subtest will create a random sequence of operations load/store to
// either local or shared memory. The offsets will also be randomized.
// Sequence:
// 1) create a FB surface that will be the total L1 size (combine all SMs)
//    Fill the FB with random data -> same for each SM so that goldens are
//    easy to generate
// 2) create a copy of this for CPU verifcation
// 3) Create a vector of OperationInfo -> store this in another FB surface
// 4) Simulate the operation in CPU first (store as golden)
// 5) Launch Kernel (do what CPU do)
// 6) compare results
// --
// On operations to shared memory: we can subdivide local and shared evenly
// With shared, we can subdivide among each threads in the CTA such that each
// thread would have 128 bytes of local and shared (4 CTA per SM)
RC LwdaGrfTest::Run30Random()
{
    RC rc;
    // Use 32 Threads / CTA -> pack one full WARP for a CTA
    // We have total of 64Kb of L1 to test. Let's divide this evenly for local
    // and shared, so 32Kb each.
    // With 64 words of local and shared per thread, this adds up to 512 bytes
    // of L1 tested per thread.
    // 64Kb / 512bytes per thread => need 128 threads in total. So launch
    // 4 CTAs at 32 threads/CTA for each SM.
    m_CtaSize     = 32;
    m_NumLocal    = 64;
    m_NumShared   = 64;
    m_NumCtaPerSm = 4;
    m_NumUINT32PerThread = m_NumLocal    + m_NumShared;
    m_NumUINT32PerCta    = m_CtaSize     * m_NumUINT32PerThread;
    m_NumUINT32PerSm     = m_NumCtaPerSm * m_NumUINT32PerCta;

    m_FakeFb.resize(m_NumSm * m_NumUINT32PerSm);

    // fill random pattern (for one SM)
    ModsTest::GetFpContext()->Rand.SeedRandom(GetTestConfiguration()->Seed());
    for (UINT32 i = 0; i < m_NumUINT32PerSm; i++)
    {
        m_FakeFb[i] = ModsTest::GetFpContext()->Rand.GetRandom();
    }
    // copy the pattern for the remaining SM
    for (UINT32 i = 1; i < m_NumSm; i++)
    {
        memcpy(&m_FakeFb[i*m_NumUINT32PerSm],
               &m_FakeFb[0],
               m_NumUINT32PerSm * sizeof(UINT32));
    }

    // Create LWCA Kernel
    Lwca::Function L1Random = m_Module.GetFunction("L1Random",
                                                   m_NumSm * m_NumCtaPerSm,
                                                   m_CtaSize);
    CHECK_RC(L1Random.InitCheck());
    CHECK_RC(L1Random.SetL1Distribution(LW_FUNC_CACHE_PREFER_EQUAL));

    // Copy this to FB
    UINT32 FbPatternSize = UNSIGNED_CAST(UINT32, m_FakeFb.size() * sizeof(UINT32));
    Lwca::DeviceMemory FbPattern;
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(FbPatternSize, &FbPattern));
    CHECK_RC(FbPattern.Set(&m_FakeFb[0], FbPatternSize));
    CHECK_RC(GetLwdaInstance().Synchronize());

    CHECK_RC(Run30_CpuOperations());

    UINT32 SeqArraySize = UNSIGNED_CAST(UINT32, m_TestOps.size() * sizeof(OperationInfo));
    Lwca::DeviceMemory SeqArray;
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(SeqArraySize, &SeqArray));
    CHECK_RC(SeqArray.Set(&m_TestOps[0], SeqArraySize));

    Lwca::Global DevReport = m_Module["Report"];
    CHECK_RC(DevReport.Clear());

    CHECK_RC(L1Random.Launch((UINT64)FbPattern.GetDevicePtr(),
                             (UINT64)SeqArray.GetDevicePtr(),
                             (UINT32)m_TestOps.size()));

    CHECK_RC(GetLwdaInstance().Synchronize());

    CHECK_RC(Run30_CheckResults());
    return rc;
}

RC LwdaGrfTest::Run30_CpuOperations()
{
    // Create the Random sequence of operations. Note m_NumLocal==m_NumShared
    const UINT32 SEQ_LENGTH = (m_NumLocal + m_NumShared) * m_Iterations;
    for (UINT32 i = 0; i < SEQ_LENGTH; i++)
    {
        OperationInfo NewOp;
        Random &ModsTestRnd = ModsTest::GetFpContext()->Rand;
        NewOp.IsLocal   = ModsTestRnd.GetRandom() % 2;
        NewOp.ToL1      = ModsTestRnd.GetRandom() % 2;
        NewOp.DstOffset = (ModsTestRnd.GetRandom() % m_NumLocal) * sizeof(UINT32);
        NewOp.SrcOffset = (ModsTestRnd.GetRandom() % m_NumLocal) * sizeof(UINT32);
        m_TestOps.push_back(NewOp);
        if (m_PrintDebugInfo)
        {
            Printf(Tee::PriNormal,
                   "i= %3d| IsLoc: %d ToL1: %d, Dst: %5d  Src: %5d\n",
                   i,
                   NewOp.IsLocal,
                   NewOp.ToL1,
                   NewOp.DstOffset,
                   NewOp.SrcOffset);
        }
    }

    // compute goldens here: only run the below during gpugen:
    // Simulate for just one SM (the others are the same)

    // How do the 'operations' work?
    //
    // This is a test of exchanges between Local/Shared memory and FB
    // For each of local/shared, we have to test load/store
    //
    // Load a value read from L1, then store into FB, or store a value that was
    // read from FB pattern and write it into L1.
    //
    // The FB had been initialized such it can be subdivdied into each SM, and
    // the FB content for each SM is identical. Within each SM's FB region, the
    // FB region can be subdivided for each thread. The size of FB region for
    // each thread is exactly the sum of the size of Local and Shared memory
    // of each thread.
    //   |--------------|--------------|
    //   |  Local       |     Shared   |
    //   -------------------------------
    //   |      FB-l    |      FB-s    |
    // Each Thread will only operate within its own region of FB to prevent
    // contention.
    // In addition, the FB region for each thread can be subivided into two portion
    // for ease of opeartion. So operations will only be between Local and FB-l, or
    // Shared and FB-s.

    // initialize the local and shared memory with FB content first. The data
    // that each thread operate on inside one SM (not CTA) will be different
    // mabe add XOR??
    m_ThreadMem.resize(m_CtaSize * m_NumCtaPerSm);
    for (UINT32 i = 0; i < (m_CtaSize * m_NumCtaPerSm); i++)
    {
        const UINT32 FB_START_IDX = i * m_NumUINT32PerThread;
        m_ThreadMem[i].FakeLocal.resize(m_NumLocal);
        m_ThreadMem[i].FakeShared.resize(m_NumShared);
        // fill Local and then shared
        for (UINT32 j = 0; j < m_NumLocal; j++)
        {
            m_ThreadMem[i].FakeLocal[j] = m_FakeFb[FB_START_IDX + j];
        }
        for (UINT32 j = 0; j < m_NumShared; j++)
        {
            m_ThreadMem[i].FakeShared[j] = m_FakeFb[FB_START_IDX + m_NumLocal + j];
        }
    }

    // do the operations: Attempt to start at a different sequence number for
    // each thread
    for (UINT32 i = 0; i < (m_CtaSize * m_NumCtaPerSm); i++)
    {
        const UINT32 FB_START_IDX = i * m_NumUINT32PerThread;
        for (UINT32 j = 0; j < SEQ_LENGTH; j++)
        {
            // Each thread in each CTA starts with the same OpId (to prevent
            // divergent threads)
            // todo hewu: I was planning on making each CTA start with a different
            // operation sequence to create more randomness. However, I'm running
            // into a problem (load/store) that I can't figure out. See
            // ***unknown*** in the ptx file.
            // const UINT32 OpId = (j + (i/m_CtaSize)) % SEQ_LENGTH;
            const UINT32 OpId = j % SEQ_LENGTH;
            if (m_TestOps[OpId].IsLocal)
            {
                if (m_TestOps[OpId].ToL1)
                {
                    UINT32 DstIdx = m_TestOps[OpId].DstOffset / sizeof(UINT32);
                    UINT32 SrcIdx = FB_START_IDX;
                    SrcIdx += (m_TestOps[OpId].SrcOffset / sizeof(UINT32));
                    m_ThreadMem[i].FakeLocal[DstIdx] = m_FakeFb[SrcIdx];
                }
                else
                {
                    // read from L1
                    UINT32 SrcIdx = m_TestOps[OpId].SrcOffset / sizeof(UINT32);
                    UINT32 DstIdx = FB_START_IDX;
                    DstIdx += (m_TestOps[OpId].DstOffset / sizeof(UINT32));
                    m_FakeFb[DstIdx] = m_ThreadMem[i].FakeLocal[SrcIdx];
                }
            }
            else
            {
                // shared:
                if (m_TestOps[OpId].ToL1)
                {
                    UINT32 DstIdx = m_TestOps[OpId].DstOffset / sizeof(UINT32);
                    UINT32 SrcIdx = FB_START_IDX + m_NumLocal;
                    SrcIdx += (m_TestOps[OpId].SrcOffset / sizeof(UINT32));
                    m_ThreadMem[i].FakeShared[DstIdx] = m_FakeFb[SrcIdx];
                }
                else
                {
                    // read from L1
                    UINT32 SrcIdx = m_TestOps[OpId].SrcOffset / sizeof(UINT32);
                    UINT32 DstIdx = FB_START_IDX + m_NumLocal;
                    DstIdx += (m_TestOps[OpId].DstOffset / sizeof(UINT32));
                    m_FakeFb[DstIdx] = m_ThreadMem[i].FakeShared[SrcIdx];
                }
            }
        }
    }

    // Compute check sum on m_Fake Local and m_FakeShared
    for (UINT32 i = 0; i < m_ThreadMem.size(); i++)
    {
        m_ThreadMem[i].FakeLocalSum = 0;
        for (UINT32 j = 0; j < m_ThreadMem[i].FakeLocal.size(); j++)
        {
            m_ThreadMem[i].FakeLocalSum += m_ThreadMem[i].FakeLocal[j];
        }
        m_ThreadMem[i].FakeSharedSum = 0;
        for (UINT32 j = 0; j < m_ThreadMem[i].FakeShared.size(); j++)
        {
            m_ThreadMem[i].FakeSharedSum += m_ThreadMem[i].FakeShared[j];
        }
    }

    return OK;
}

RC LwdaGrfTest::Run30_CheckResults()
{
    RC rc;
    const UINT32 ReportSize = m_CtaSize * m_NumCtaPerSm * 64;    // 64 is a constant for MaxSM - set in the PTX file
    vector<ThreadReport30_Rnd, Lwca::HostMemoryAllocator<ThreadReport30_Rnd> >
        Report(ReportSize,
               ThreadReport30_Rnd(),
               Lwca::HostMemoryAllocator<ThreadReport30_Rnd>(GetLwdaInstancePtr(),
                                                             GetBoundLwdaDevice()));

    // Read results
    Lwca::Global DevReport = m_Module["Report"];
    CHECK_RC(DevReport.Get(&Report[0], sizeof(Report[0])*ReportSize));
    CHECK_RC(GetLwdaInstance().Synchronize());

    if (m_PrintDebugInfo)
    {
        for (UINT32 i = 0; i < (m_NumSm * m_NumCtaPerSm); i++)
        {
            Printf(Tee::PriNormal, "Report[%d]0x%08x  0x%08x, .0x%08x 0x%08x\n",
                   i*m_CtaSize,
                   Report[i*m_CtaSize].Dbg1,
                   Report[i*m_CtaSize].Dbg2,
                   Report[i*m_CtaSize].LocalCheckSum,   // check sum of local
                   Report[i*m_CtaSize].SharedCheckSum); // check sum of shared

            Printf(Tee::PriNormal, "FakeFb:  0x%08x  0x%08x  ",
                   m_FakeFb[i*m_NumUINT32PerCta],
                   m_FakeFb[i*m_NumUINT32PerCta + 1]);

            Printf(Tee::PriNormal, " 0x%08x 0x%08x \n",
                   m_ThreadMem[0].FakeLocalSum,
                   m_ThreadMem[0].FakeSharedSum);

            Printf(Tee::PriNormal, "Report[%d]0x%08x  0x%08x, .0x%08x 0x%08x\n",
                   i*m_CtaSize + 1,
                   Report[i*m_CtaSize + 1].Dbg1,
                   Report[i*m_CtaSize + 1].Dbg2,
                   Report[i*m_CtaSize + 1].LocalCheckSum,
                   Report[i*m_CtaSize + 1].SharedCheckSum);

            Printf(Tee::PriNormal, "FakeFb:  0x%08x  0x%08x  ",
                   m_FakeFb[i*m_NumUINT32PerCta + m_NumUINT32PerThread],
                   m_FakeFb[i*m_NumUINT32PerCta + m_NumUINT32PerThread + 1]);

            Printf(Tee::PriNormal, " 0x%08x 0x%08x \n",
                   m_ThreadMem[1].FakeLocalSum,
                   m_ThreadMem[1].FakeSharedSum);

            Printf(Tee::PriNormal, "Report[%d]0x%08x  0x%08x, .0x%08x 0x%08x\n",
                   i*m_CtaSize + 2,
                   Report[i*m_CtaSize + 2].Dbg1,
                   Report[i*m_CtaSize + 2].Dbg2,
                   Report[i*m_CtaSize + 2].LocalCheckSum,
                   Report[i*m_CtaSize + 2].SharedCheckSum);

            Printf(Tee::PriNormal, "FakeFb:  0x%08x  0x%08x  ",
                   m_FakeFb[i*m_NumUINT32PerCta + m_NumUINT32PerThread*2],
                   m_FakeFb[i*m_NumUINT32PerCta + m_NumUINT32PerThread*2 + 1]);

            Printf(Tee::PriNormal, " 0x%08x 0x%08x \n",
                   m_ThreadMem[2].FakeLocalSum,
                   m_ThreadMem[2].FakeSharedSum);
        }

    }

    for (UINT32 i = 0; i < m_NumSm; i++)
    {
        for (UINT32 j = 0; j < (m_CtaSize * m_NumCtaPerSm); j++)
        {
            const UINT32 ReportIdx = i*m_CtaSize*m_NumCtaPerSm + j;
            if (m_ThreadMem[j].FakeLocalSum != Report[ReportIdx].LocalCheckSum)
            {
                Printf(Tee::PriNormal,
                       "CTA: %d Thd: %d. Expected local 0x%08x, got 0x%08x\n",
                       i*m_NumCtaPerSm + j/m_CtaSize,
                       j%m_CtaSize,
                       m_ThreadMem[j].FakeLocalSum,
                       Report[ReportIdx].LocalCheckSum);
                rc = RC::GPU_COMPUTE_MISCOMPARE;
            }
            if (m_ThreadMem[j].FakeSharedSum != Report[ReportIdx].SharedCheckSum)
            {
                Printf(Tee::PriNormal,
                       "CTA: %d Thd: %d. Expected shared 0x%08x, got 0x%08x\n",
                       i*m_NumCtaPerSm + j/m_CtaSize,
                       j%m_CtaSize,
                       m_ThreadMem[j].FakeSharedSum,
                       Report[ReportIdx].SharedCheckSum);
                rc = RC::GPU_COMPUTE_MISCOMPARE;
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ LwdaGrfTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(LwdaGrfTest, LwdaStreamTest,
                 "Run the compute GRF test.");

CLASS_PROP_READWRITE(LwdaGrfTest, Iterations, UINT32,
                     "Num of internal iterations inside the kernel");

CLASS_PROP_READWRITE(LwdaGrfTest, PrintDebugInfo, bool,
                     "enable debug spews for kernel");

