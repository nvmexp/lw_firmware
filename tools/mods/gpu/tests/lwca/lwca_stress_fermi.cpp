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

#include "gpu/tests/lwdamemtest.h"
#include "core/include/jscript.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/lwca/stress/lwda_stress_fermi.h"
#include "core/include/platform.h"
#include "gpu/utility/eclwtil.h"
#include <float.h>

//-----------------------------------------------------------------------------
//! LWCA-based Stress test for Fermi+ (max power & heat).
//!
class LwdaStress2 : public LwdaMemTest
{
public:
    LwdaStress2();
    virtual ~LwdaStress2();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    struct KernelArgs
    {
        UINT64 dest;
        UINT64 src;
        UINT64 results;
        UINT64 size;
        UINT32 iterations;
    };

private:
    // JS controls:
    bool    m_KeepRunning;
    UINT32  m_LoopMs;
    UINT32  m_NumThreads;
    UINT32  m_NumBlocks;
    UINT32  m_BlockSize;
    double  m_FailThreshold;
    UINT32  m_EndLoopPeriodMs;
    UINT32  m_Iterations;
    bool    m_MinFbTraffic;

    //Setting upper cap on max blocks to the value from Fermi
    static const UINT32 s_maxLwdaBlocks = 0x10000;
    // Lwca events are really semaphores, I think.
    // We use a bunch of them to keep cpu & gpu in sync and to record how much
    // time has elapsed.
    enum EventIdxs
    {
        EvBegin,
        EvEnd,
        EvPing,
        EvPong,
        EvNumEvents
    };

    // Lwca resources (alloc in Setup, free in Cleanup).
    Lwca::Module       m_Module;
    Lwca::DeviceMemory m_ResultsMem;
    Lwca::DeviceMemory m_GoldenMem;
    Lwca::DeviceMemory m_TestMem;
    Lwca::Function     m_Function;
    Lwca::Function     m_Randomize;
    Lwca::Event        m_Events[EvNumEvents];

public:
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(LoopMs,      UINT32);
    SETGET_PROP(NumThreads,  UINT32);
    SETGET_PROP(NumBlocks,   UINT32);
    SETGET_PROP(FailThreshold, double);
    SETGET_PROP(EndLoopPeriodMs, UINT32);
    SETGET_PROP(Iterations, UINT32);
    SETGET_PROP(MinFbTraffic,bool);
};

//-----------------------------------------------------------------------------
// JS linkage for test.
JS_CLASS_INHERIT(LwdaStress2, LwdaMemTest,
                 "LWCA Stress test (max power & heat)");
CLASS_PROP_READWRITE(LwdaStress2, KeepRunning, bool,
                     "Test keeps looping while this is true.");
CLASS_PROP_READWRITE(LwdaStress2, LoopMs, UINT32,
                     "Test keeps looping this many msec.");
CLASS_PROP_READWRITE(LwdaStress2, NumThreads, UINT32,
                     "Width of block (default 16 threads).");
CLASS_PROP_READWRITE(LwdaStress2, NumBlocks, UINT32,
                     "Size of grid (default 512 blocks).");
CLASS_PROP_READWRITE(LwdaStress2, FailThreshold, double,
                     "Miscompare threshold.");
CLASS_PROP_READWRITE(LwdaStress2, EndLoopPeriodMs, UINT32,
        "Time in milliseconds between calls to EndLoop (default = 10000).");
CLASS_PROP_READWRITE(LwdaStress2, Iterations, UINT32,
                     "Number of FMA iterations per LD/ST.");
CLASS_PROP_READWRITE(LwdaStress2, MinFbTraffic, bool,
                     "Minimizes FB traffic (default = false).");
//-----------------------------------------------------------------------------
LwdaStress2::LwdaStress2()
:   m_KeepRunning(false),
    m_LoopMs(15*1000),
    m_NumThreads(256),
    m_NumBlocks(0),
    m_BlockSize(LwdaStressConstants::randomBlockSize),
    m_FailThreshold((double)(2 * FLT_EPSILON)),
    m_EndLoopPeriodMs(10000),
    m_Iterations(0),
    m_MinFbTraffic(false)
{
    SetName("LwdaStress2");
}

LwdaStress2::~LwdaStress2()
{
}

//-----------------------------------------------------------------------------
bool LwdaStress2::IsSupported()
{
    if (!LwdaMemTest::IsSupported())
        return false;

    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();

    // Not supported on T194+
    if (cap >= Lwca::Capability::SM_70 && GetBoundGpuSubdevice()->IsSOC())
        return false;

    return true;
}

//-----------------------------------------------------------------------------
void LwdaStress2::PrintJsProperties(Tee::Priority pri)
{
    LwdaMemTest::PrintJsProperties(pri);

    Printf(pri, "LwdaStress2 Js Properties:\n");
    Printf(pri, "\tLoopMs:\t\t\t\t%u\n", m_LoopMs);
    Printf(pri, "\tNumThreads:\t\t\t%u\n", m_NumThreads);
    Printf(pri, "\tNumBlocks:\t\t\t%u\n", m_NumBlocks);
    Printf(pri, "\tFailThreshold:\t\t\t%g\n", m_FailThreshold);
    Printf(pri, "\tIterations:\t\t\t%u\n", m_Iterations);
    Printf(pri, "\tMinFbTraffic:\t\t\t%s\n", m_MinFbTraffic?"true":"false");
}

//-----------------------------------------------------------------------------
RC LwdaStress2::Setup()
{
    RC rc;
    CHECK_RC(LwdaMemTest::Setup());

    if (GetTestConfiguration()->ShortenTestForSim() == true)
    {
        m_BlockSize = 1;
    }
    MASSERT(m_BlockSize > 0);

    // Set default number of iterations
    if (m_MinFbTraffic && (m_Iterations == 0))
    {
        m_Iterations = LwdaStressConstants::maxIterations;
    }
    if (m_Iterations == 0)
    {
        // Tuned on GF100
        // With ECC FB is slower, so we can perform more FMAs
        bool eccSupported = false;
        UINT32 enableMask = 0;
        CHECK_RC(GetBoundGpuSubdevice()->GetEccEnabled(&eccSupported,
                                                       &enableMask));
        Printf(Tee::PriLow,
               "LwdaStress2 detected ECC %ssupported, FB ECC %sabled\n",
                eccSupported?"":"not ",
                Ecc::IsUnitEnabled(Ecc::Unit::DRAM, enableMask)
                ? "en" : "dis");
        if (eccSupported && Ecc::IsUnitEnabled(Ecc::Unit::DRAM, enableMask))
        {
            m_Iterations = 19;
        }
        else
        {
            m_Iterations = 14;
        }
    }
    if (m_Iterations > LwdaStressConstants::maxIterations)
    {
        Printf(Tee::PriError,
               "LwdaStress2: The maximum number of iterations is 20\n");
        return RC::BAD_PARAMETER;
    }

    // Load kernel
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("cstrsf", &m_Module));

    // Set default number of blocks
    if (m_NumBlocks == 0)
    {
        const UINT32 maxBlocks = GetBoundLwdaDevice().GetAttribute(
                LW_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X);

        // scale down m_NumBlocks. This test was originally written
        // for GF100 which has 16 SMs
        const UINT32 smCount = GetBoundLwdaDevice().GetShaderCount();
        const UINT32 FULL_SM_ON_GF100 = 16;
        m_NumBlocks = (maxBlocks * smCount) / FULL_SM_ON_GF100;

        m_NumBlocks = m_NumBlocks - m_NumBlocks % m_BlockSize;
        if (m_NumBlocks > s_maxLwdaBlocks)
        {
            Printf(Tee::PriLow,
                    "Callwlated NumBlocks[%u] > Upper Cap of NumBlocks;"
                    " continuing with %u blocks\n",
                    m_NumBlocks, s_maxLwdaBlocks);
            m_NumBlocks = s_maxLwdaBlocks;
        }
    }

    // Check number of blocks
    if ((m_NumBlocks < m_BlockSize) || (m_NumBlocks % m_BlockSize != 0))
    {
        Printf(Tee::PriError, "Unsupported number of blocks\n");
        return RC::BAD_PARAMETER;
    }

    // Allocate memory for results
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(
                 m_NumThreads*m_NumBlocks*sizeof(UINT32), &m_ResultsMem));

    // Allocate memory for golden values
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(
                 m_NumThreads * m_BlockSize * sizeof(UINT32), &m_GoldenMem));

    // Allocate some events.
    for (UINT32 i = 0; i < EvNumEvents; i++)
    {
        m_Events[i] = GetLwdaInstance().CreateEvent();
        CHECK_RC(m_Events[i].InitCheck());
    }

    // Allocate memory for the test
    if (m_MinFbTraffic)
    {
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(
                     m_NumThreads * m_NumBlocks * sizeof(float) * 2,
                     &m_TestMem));
    }
    else if (GetBoundGpuSubdevice()->IsSOC())
    {
        FLOAT64 sizeMB = GetMaxFbMb();
        if (sizeMB == 0)
        {
            if (Platform::HasClientSideResman())
            {
                sizeMB = 512;
            }
            else
            {
                // No high memory allocations on Android yet
                sizeMB = 128;
            }
            Printf(Tee::PriLow,
                "MaxFbMb not specified - LwdaStress2 test defaulting to %uMB\n",
                static_cast<UINT32>(sizeMB));
        }
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(
                     static_cast<UINT64>(sizeMB * (1024U*1024U)), &m_TestMem));
    }
    else
    {
        CHECK_RC(AllocateEntireFramebuffer());
    }

    // Set up the kernel which randomizes memory
    m_Randomize = m_Module.GetFunction("Randomize");
    CHECK_RC(m_Randomize.InitCheck());
    m_Randomize.SetOptimalGridDim(GetLwdaInstance());
    m_Randomize.SetBlockDim(1024);

    // Set up the kernel we will run
    m_Function = m_Module.GetFunction("Run", m_NumBlocks, m_NumThreads);
    CHECK_RC(m_Function.InitCheck());
    Printf(Tee::PriLow,
           "LwdaStress2: Set up to run with %u blocks, %u threads, %u iterations \n",
           m_NumBlocks, m_NumThreads, m_Iterations);

    return rc;
}

//-----------------------------------------------------------------------------
RC LwdaStress2::Run()
{
    RC rc;

    // Find the biggest chunk for testing
    UINT32 biggestChunk = 0;
    UINT64 biggestChunkSize = 0;
    for (UINT32 i=0; i < NumChunks(); i++)
    {
        const UINT64 chunkSize = GetChunkDesc(i).size;
        if (chunkSize > biggestChunkSize)
        {
            biggestChunk = i;
            biggestChunkSize = chunkSize;
        }
    }
    const Lwca::Global& testMem =
        m_MinFbTraffic || GetBoundGpuSubdevice()->IsSOC()
        ? static_cast<const Lwca::Global&>(m_TestMem)
        : static_cast<const Lwca::Global&>(GetLwdaChunk(biggestChunk));

    // Make the memory size divisible by total number of threads
    UINT64 testedSize = testMem.GetSize();
    const UINT32 threadRange = m_NumThreads * m_NumBlocks * sizeof(float);
    if (2*threadRange > testedSize)
    {
        Printf(Tee::PriError,
               "LwdaStress2: Not enough FB available to perform the test\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    testedSize -= testedSize % (2*threadRange);
    Printf(Tee::PriLow, "LwdaStress2 will use %.3fMB of FB for testing\n",
            static_cast<double>(testedSize)/(1024.0*1024.0));

    // Randomize source
    const UINT64 sourceBlockSize = testedSize / 2;
    const UINT32 seed = GetTestConfiguration()->Seed();
    const UINT32 launchesPerYield = 10;
    UINT32 launchCount = 0;
    for (UINT64 offs = 0; offs < sourceBlockSize;
         offs += m_GoldenMem.GetSize(), launchCount++)
    {
        CHECK_RC(m_Randomize.Launch(
                    testMem.GetDevicePtr()+offs,
                    m_GoldenMem.GetSize(),
                    seed));
        if (0 == (launchCount % launchesPerYield))
            Tasker::Yield();
    }

    // Clear results
    CHECK_RC(m_ResultsMem.Clear());

    // Perform a small run to callwlate "golden" values
    CHECK_RC(m_Function.Launch(
                m_GoldenMem.GetDevicePtr(),
                testMem.GetDevicePtr(),
                m_GoldenMem.GetDevicePtr(),
                m_ResultsMem.GetDevicePtr(),
                m_GoldenMem.GetSize() / sizeof(float),
                m_Iterations,
                m_BlockSize));

    // Clear results again to discard errors produced by comparing against
    // uninitialized values
    CHECK_RC(m_ResultsMem.Clear());

    // Record start time.
    CHECK_RC(m_Events[EvBegin].Record());

    // Launch N times.  N may be indefinite if using KeepRunning.
    const UINT32 minLaunches = GetTestConfiguration()->Loops();
    UINT32 numLaunches;
    UINT32 msecSoFar;
    UINT32 nextEndLoopMs = m_EndLoopPeriodMs;

    // Run the gpu "kernel" many times.
    //
    // We'll run at least least obj.Testconfiguration.Loops times,
    // and for at least obj.LoopMs milliseconds.
    //
    // In addtion, you can set obj.KeepRunning to true before calling Run,
    // in which case, we'll run until another JS thread sets it false.
    // This is handy for running this test in a background test.
    //
    // We use "ping-pong" events below to make sure we get no more than 2
    // kernel-launches ahead of the GPU in the lwca pushbuffer.
    // If we just filled the pushbuffer, we would have no way to stop promptly
    // when LoopMs runs out or KeepRunning goes false.
    //
    for (numLaunches = 0, msecSoFar = 0;
         (numLaunches < minLaunches) || m_KeepRunning || (msecSoFar < m_LoopMs);
         numLaunches++)
    {
        UINT32 eventIdx = EvPing + (numLaunches & 1);

        if (numLaunches > 1)
        {
            CHECK_RC(m_Events[eventIdx].Synchronize());
            msecSoFar = m_Events[eventIdx] - m_Events[EvBegin];
        }
        CHECK_RC(m_Function.Launch(
                    testMem.GetDevicePtr() + sourceBlockSize,
                    testMem.GetDevicePtr(),
                    m_GoldenMem.GetDevicePtr(),
                    m_ResultsMem.GetDevicePtr(),
                    sourceBlockSize/sizeof(float),
                    m_Iterations,
                    m_BlockSize));
        CHECK_RC(m_Events[eventIdx].Record());

        if (msecSoFar >= nextEndLoopMs)
        {
            CHECK_RC(EndLoop(msecSoFar));
            nextEndLoopMs += m_EndLoopPeriodMs;
        }
    }

    // Record event indicating end of the test
    CHECK_RC(m_Events[EvEnd].Record());

    // Let GPU catch up and report benchmark numbers.
    CHECK_RC(m_Events[EvEnd].Synchronize());
    const float  Mthreads   = 1e-6F * numLaunches * m_NumBlocks * m_NumThreads;
    const float  elapsed    = 1e-3F * (m_Events[EvEnd] - m_Events[EvBegin]);
    const float  MTPS       = elapsed ? Mthreads / elapsed : 0.0F;

    Printf(Tee::PriLow,
            "  %d launches, total %.3f Mthreads in %.3f Sec --> %.3f Mthreads/sec.\n",
            numLaunches,
            Mthreads,
            elapsed,
            MTPS);

    // Check results for aclwmulated errors
    const size_t numResults =
        static_cast<size_t>(m_ResultsMem.GetSize() / sizeof(UINT32));
    vector<UINT32, Lwca::HostMemoryAllocator<UINT32> >
        resultsBuff(0x10000,
                    UINT32(),
                    Lwca::HostMemoryAllocator<UINT32>(GetLwdaInstancePtr(),
                                                      GetBoundLwdaDevice()));
    const unsigned maxToReport   = 10;
    unsigned numNonZeroErrCounts = 0;
    UINT64 totalErrCount         = 0;
    for (size_t i=0; i < numResults; i++)
    {
        // Get the i-th element of m_ResultsMem, using resultsBuff to
        // buffer the downloaded data.
        if ((i % resultsBuff.size()) == 0)
        {
            const size_t copySize = sizeof(UINT32) * min(numResults - i,
                                                         resultsBuff.size());
            CHECK_RC(m_ResultsMem.Get(&resultsBuff[0], i, copySize));
            CHECK_RC(GetLwdaInstance().Synchronize());
        }
        UINT32 result = resultsBuff[i % resultsBuff.size()];

        // Report an error if the i-th element is nonzero
        if (result > 0)
        {
            numNonZeroErrCounts++;
            totalErrCount += result;

            Tee::Priority pri;

            if (numNonZeroErrCounts <= maxToReport)
            {
                pri = Tee::PriLow;
            }
            else if (numNonZeroErrCounts <= (10*maxToReport))
            {
                pri = Tee::PriSecret;
            }
            else
            {
                continue;
            }

            const UINT32 iBlock = UNSIGNED_CAST(UINT32, i / m_NumThreads);
            const UINT32 iThread = i % m_NumThreads;

            Printf(pri,
                "  ErrCount[%5u] = %u at block %u, thread %u\n",
                static_cast<UINT32>(i),
                result,
                iBlock,
                iThread);
        }
    }
    if (totalErrCount > 0)
    {
        rc = RC::GPU_STRESS_TEST_FAILED;
        Printf(Tee::PriError, "Total errors: %llu\n", totalErrCount);
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC LwdaStress2::Cleanup()
{
    m_TestMem.Free();
    m_GoldenMem.Free();
    m_ResultsMem.Free();
    m_Module.Unload();
    for (UINT32 i = 0; i < EvNumEvents; i++)
        m_Events[i].Free();
    return LwdaMemTest::Cleanup();
}
