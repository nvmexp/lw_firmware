/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2013,2015-2019 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/lwdastst.h"
#include "core/include/jsonlog.h"
#include "lwgoldensurfaces.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"

//! LWCA-based double precision stress test.
class DPStressTest : public LwdaStreamTest
{
public:
    DPStressTest();
    virtual ~DPStressTest() { }
    virtual bool IsSupported() override;
    virtual void PrintJsProperties(Tee::Priority pri) override;
    virtual RC Setup() override;
    virtual RC Run() override;
    virtual RC Cleanup() override;
    bool IsSupportedVf() override { return true; }

    // JS property accessors
    SETGET_PROP(InnerLoops, UINT32);
    SETGET_PROP(LoopMs, UINT32);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(MaxAllowedErrorBits, UINT32);
    SETGET_PROP(UseSharedMemory, bool);
    SETGET_PROP(BufferSize, UINT32);

private:
    struct LaunchParams
    {
        UINT32 seed    = 0U;
        int alpha      = 0;
        int beta       = 0;
        UINT32 loops   = 0U;
        UINT32 rowSize = 0U;
    };

    void Restart();
    void Randomize();
    RC Launch();
    RC InnerRun();
    void GenerateRandomData(UINT32* pSeed, double* pBegin, double* pEnd) const;
    RC Verify(const double* kernelOutput, const LaunchParams& launchParams);
    static void ErrorCallback(void* goldenSurfaces, UINT32 loop, RC rc, UINT32 surf);
    static void PrintCallback(void* test);
    void Print(INT32 pri);

    UINT32             m_MaxTotalThreads     = 16U * 1024U;
    UINT32             m_RandomGenBlocks     = 16U;
    UINT32             m_NumThreads          = 0U;
    UINT32             m_NumBlocks           = 0U;

    Random             m_Random;
    Lwca::Module       m_Module;
    Lwca::Function     m_DoublePrecisionStress;
    Lwca::Function     m_RandomFill;
    Lwca::Event        m_StartTimeEvent[2];
    Lwca::Event        m_StopTimeEvent[2];
    Lwca::DeviceMemory m_devMem;
    UINT32             m_LwrTimeEvent        = 0U;
    LwdaGoldenSurfaces m_GSurfs;
    LaunchParams       m_LaunchParams;
    UINT32             m_LwrLoop             = ~0U;
    UINT32             m_LwrBlock            = ~0U;

    // JS properties
    UINT32             m_InnerLoops          = 1U;
    UINT32             m_LoopMs              = 0U;
    bool               m_KeepRunning         = false;
    UINT32             m_MaxAllowedErrorBits = 24U;
    bool               m_UseSharedMemory     = false;
    UINT32             m_BufferSize          = 1024U * 1024U;
    UINT32             m_RowSize             = 0U;
};

JS_CLASS_INHERIT(DPStressTest, LwdaStreamTest, "LWCA-based double precision stress test");
CLASS_PROP_READWRITE(DPStressTest, InnerLoops, UINT32,
                     "Number of inner iterations performed inside the LWCA kernel");
CLASS_PROP_READWRITE(DPStressTest, LoopMs, UINT32,
                     "Duration of the test in ms, default is 0 and the test checks golden values");
CLASS_PROP_READWRITE(DPStressTest, KeepRunning, bool,
                     "Indicates whether the test should continue running, for background testing");
CLASS_PROP_READWRITE(DPStressTest, MaxAllowedErrorBits, UINT32,
                     "Number of least significant bits which can be different between the GPU and the CPU");
CLASS_PROP_READWRITE(DPStressTest, UseSharedMemory, bool,
                     "Use shared memory for randomized data instead of FB");
CLASS_PROP_READWRITE(DPStressTest, BufferSize, UINT32,
                     "Size of FB buffer in bytes where random double values are generated");

DPStressTest::DPStressTest()
{
    SetName("DPStressTest");
}

bool DPStressTest::IsSupported()
{
    // Check if compute is supported at all
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }

    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();

    // Not supported on T194+
    if (cap >= Lwca::Capability::SM_70 && GetBoundGpuSubdevice()->IsSOC())
        return false;

    return true;
}

void DPStressTest::PrintJsProperties(Tee::Priority pri)
{
    LwdaStreamTest::PrintJsProperties(pri);

    Printf(pri, "DPStressTest Js Properties:\n");
    Printf(pri, "\tInnerLoops:\t\t\t%u\n", m_InnerLoops);
    Printf(pri, "\tLoopMs:\t\t\t\t%u\n", m_LoopMs);
    Printf(pri, "\tKeepRunning:\t\t\t%s\n", m_KeepRunning?"true":"false");
    Printf(pri, "\tMaxAllowedErrorBits:\t\t%u\n", m_MaxAllowedErrorBits);
    Printf(pri, "\tUseSharedMemory:\t\t%s\n", m_UseSharedMemory?"true":"false");
    Printf(pri, "\tBufferSize:\t\t\t%u\n", m_BufferSize);
}

RC DPStressTest::Setup()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());

    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("dpstrs", &m_Module));

    m_NumThreads =
        GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);

    Printf(Tee::PriLow, "DpStress: MaxThreads= %d\n", m_NumThreads);

    // initialize the rowSize to the maximum setting in the kernel.
    m_RowSize = 2000; // can't exceed 48KB of shared memory. see maxRowSize in the kernel.

    UINT32 maxShaders = GetBoundGpuSubdevice()->GetMaxShaderCount();
    maxShaders = max<UINT32>(30, maxShaders);

    m_MaxTotalThreads = maxShaders * 1024; // support upto 24 SMs, each SM using 1024 threads
    m_RandomGenBlocks = maxShaders;

    m_NumBlocks = GetBoundLwdaDevice().GetShaderCount();

    if (GetTestConfiguration()->ShortenTestForSim())
    {
        // we only want to check the ability to run a few warps on each SM without exceeding the
        // maximum shared memory size.
        if(m_NumBlocks > 16)
            m_NumThreads = 2 * GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_WARP_SIZE);
        else
            m_NumThreads = 3 * GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_WARP_SIZE);

        m_RowSize = min(m_NumBlocks*m_NumThreads, m_RowSize);
        m_BufferSize = min(m_BufferSize, (UINT32)(m_NumBlocks*m_NumThreads*sizeof(double)));
    }

    // Buffer size must be configured such that the size of each block is a multiple of 8 to
    // prevent the kernels from generating SM_HWW_WARP_ESR_ERROR_MISALIGNED_ADDR interrupt when
    // filling each block with random data.
    m_BufferSize = (m_BufferSize/m_RandomGenBlocks) / 8;
    m_BufferSize = m_BufferSize * m_RandomGenBlocks * 8;

    if (m_NumBlocks*m_NumThreads > m_MaxTotalThreads)
    {
        MASSERT(m_NumThreads > 0);
        Printf(Tee::PriError, "This GPU has %u SMs, but the test supports only %u\n",
                m_NumBlocks, m_MaxTotalThreads/m_NumThreads);
        return RC::SOFTWARE_ERROR;
    }
    // Get loop parameters
    const UINT32 numLoops = GetTestConfiguration()->Loops();
    if (((numLoops & 1) != 0) || (numLoops == 0))
    {
        Printf(Tee::PriError, "Number of test loops must be even and greater than 0!\n");
        return RC::BAD_PARAMETER;
    }

    // Launch only one block during gpugen, because all blocks
    // generate the same values
    Goldelwalues& goldens = *GetGoldelwalues();
    if (goldens.GetAction() == Goldelwalues::Store)
    {
        m_NumBlocks = 1;
    }

    if (m_UseSharedMemory)
    {
        m_DoublePrecisionStress = m_Module["DoublePrecisionStress"];
        CHECK_RC(m_DoublePrecisionStress.InitCheck());
        m_DoublePrecisionStress.SetGridDim(m_NumBlocks);
        m_DoublePrecisionStress.SetBlockDim(m_NumThreads);
    }
    else
    {
        m_DoublePrecisionStress = m_Module["DoublePrecisionStressFB"];
        CHECK_RC(m_DoublePrecisionStress.InitCheck());
        m_DoublePrecisionStress.SetGridDim(m_NumBlocks);
        m_DoublePrecisionStress.SetBlockDim(m_NumThreads);

        m_RandomFill = m_Module["RandomFillFB"];
        CHECK_RC(m_RandomFill.InitCheck());
        m_RandomFill.SetGridDim(m_RandomGenBlocks);
        m_RandomFill.SetBlockDim(m_NumThreads);
    }

    for (UINT32 i=0; i < 2; i++)
    {
        m_StartTimeEvent[i] = GetLwdaInstance().CreateEvent();
        CHECK_RC(m_StartTimeEvent[i].InitCheck());
        m_StopTimeEvent[i] = GetLwdaInstance().CreateEvent();
        CHECK_RC(m_StopTimeEvent[i].InitCheck());
    }
    m_LwrTimeEvent = 0;

    if (!m_UseSharedMemory)
    {
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_BufferSize, &m_devMem));
    }

    Printf(Tee::PriLow, "Kernel configuration: %u blocks, %u threads\n",
            m_NumBlocks, m_NumThreads);

    m_GSurfs.SetNumSurfaces(1);
    goldens.SetSurfaces(&m_GSurfs);
    goldens.AddErrorCallback(&ErrorCallback, &m_GSurfs);
    goldens.SetPrintCallback(&PrintCallback, this);

    return OK;
}

RC DPStressTest::Cleanup()
{
    m_devMem.Free();
    for (UINT32 i=0; i < 2; i++)
    {
        m_StartTimeEvent[i].Free();
        m_StopTimeEvent[i].Free();
    }
    m_Module.Unload();
    GetGoldelwalues()->ClearSurfaces();
    return LwdaStreamTest::Cleanup();
}

RC DPStressTest::Run()
{
    StickyRC rc;
    rc = InnerRun();
    rc = GetGoldelwalues()->ErrorRateTest(GetJSObject());
    return rc;
}

void DPStressTest::Restart()
{
    m_Random.SeedRandom(GetTestConfiguration()->Seed());

    // Skip some loops if StartLoop is greater than 0
    for (UINT32 i=0; i < GetTestConfiguration()->StartLoop(); i++)
    {
        Randomize();
    }
}

void DPStressTest::Randomize()
{
    m_LaunchParams.seed = m_Random.GetRandom();
    static const int a[] = { -1, 0, 1, 2, 3 };
    m_LaunchParams.alpha = a[m_Random.GetRandom() % (sizeof(a)/sizeof(a[0]))];
    static const int b[] = { -1, 1, 2, 3 };
    m_LaunchParams.beta = b[m_Random.GetRandom() % (sizeof(b)/sizeof(b[0]))];
    m_LaunchParams.loops = m_InnerLoops;
    m_LaunchParams.rowSize = m_RowSize;

}

RC DPStressTest::Launch()
{
    Randomize();
    RC rc;
    if (!m_UseSharedMemory)
    {
        CHECK_RC(m_RandomFill.Launch(
                    m_devMem.GetDevicePtr(),
                    static_cast<UINT32>(m_devMem.GetSize()),
                    m_LaunchParams.seed));
    }
    CHECK_RC(m_StartTimeEvent[m_LwrTimeEvent].Record());
    if (m_UseSharedMemory)
    {
        CHECK_RC(m_DoublePrecisionStress.Launch(
                    m_LaunchParams.seed,
                    m_LaunchParams.alpha,
                    m_LaunchParams.beta,
                    m_LaunchParams.loops,
                    m_LaunchParams.rowSize));
    }
    else
    {
        CHECK_RC(m_DoublePrecisionStress.Launch(
                    m_devMem.GetDevicePtr(),
                    static_cast<UINT32>(m_devMem.GetSize()),
                    m_LaunchParams.alpha,
                    m_LaunchParams.beta));
    }
    CHECK_RC(m_StopTimeEvent[m_LwrTimeEvent].Record());
    m_LwrTimeEvent ^= 1;
    return OK;
}

RC DPStressTest::InnerRun()
{
    RC rc;

    // Get output buffer global from the module
    Lwca::Global output = m_Module["Output"];
    CHECK_RC(output.InitCheck());

    // Pretend the output is a rectangular surface for golden values
    const bool w32 = (m_NumThreads % 32) == 0;
    const UINT32 width = (w32 ? 32 : m_NumThreads) * sizeof(double) / sizeof(UINT32);
    const UINT32 height = w32 ? (m_NumThreads / 32) : 1;

    // Sysmem buffer for CRCing generated images
    vector<double, Lwca::HostMemoryAllocator<double> >
        buf(m_NumThreads*m_NumBlocks,
            double(),
            Lwca::HostMemoryAllocator<double>(GetLwdaInstancePtr(),
                                              GetBoundLwdaDevice()));

    // Create event for determining when the data for the current loop is available
    Lwca::Event ev(GetLwdaInstance().CreateEvent());
    CHECK_RC(ev.InitCheck());

    // Prepare random number generator for first loop
    Restart();

    // Launch the kernel for the first loop
    CHECK_RC(Launch());

    // Prepare for continous testing
    Goldelwalues& goldens = *GetGoldelwalues();
    const bool background = m_KeepRunning
        && (goldens.GetAction() != Goldelwalues::Store);
    const bool timed = (m_LoopMs > 0) && !background
        && (goldens.GetAction() != Goldelwalues::Store);
    const bool continuous = background || timed;
    const UINT64 startTime = Xp::GetWallTimeMS();

    // Get loop parameters
    const UINT32 startLoop = GetTestConfiguration()->StartLoop();
    const UINT32 numLoops = GetTestConfiguration()->Loops();
    MASSERT(numLoops > 0);
    UINT32 totalTime = 0;
    for (UINT32 i=0; (i < numLoops) || continuous; i++)
    {
        // Save launch parameters for verification
        LaunchParams launchParams = m_LaunchParams;

        // Copy results back to sysmem for the current loop
        CHECK_RC(output.Get(&buf[0], buf.size()*sizeof(double)));
        CHECK_RC(ev.Record());

        // Make sure i is within bounds when running the test continuously
        i = i % numLoops;

        // Launch kernel for next loop while we're handling the current loop
        if ((i+1 < numLoops) || continuous)
        {
            if (continuous && (i+1 == numLoops))
            {
                Restart();
            }
            CHECK_RC(Launch());
        }

        // Wait for the sysmem copy to become available
        CHECK_RC(ev.Synchronize());

        // Accumulate exelwtion time
        totalTime += m_StopTimeEvent[i&1] - m_StartTimeEvent[i&1];

        // Verify golden values against the CPU
        if (goldens.GetAction() == Goldelwalues::Store)
        {
            CHECK_RC(Verify(&buf[0], launchParams));
        }

        // Compute golden values for each block
        for (UINT32 j=0; j < m_NumBlocks; j++)
        {
            // Set pointer for subsequent SM, all SMs generate the same output
            m_GSurfs.DescribeSurface(width, height, width*sizeof(UINT32),
                    static_cast<void*>(&buf[m_NumThreads*j]), 0, "");

            // Process kernel output
            goldens.SetLoop(i+startLoop);
            m_LwrLoop = i+startLoop;
            m_LwrBlock = j;
            CHECK_RC(goldens.Run());
        }

        // Handle preset exit condition
        const UINT32 endLoopIndex = UNSIGNED_CAST(UINT32,
            timed ? (Xp::GetWallTimeMS() - startTime) : (startLoop + i));
        CHECK_RC(EndLoop(endLoopIndex));

        // Handle end of background test
        if (background && !m_KeepRunning)
        {
            break;
        }

        // Handle timed exelwtion
        if (timed && ((Xp::GetWallTimeMS() - startTime) >= m_LoopMs))
        {
            break;
        }
    }

    Printf(Tee::PriLow, "Total kernel exelwtion time %ums\n", totalTime);

    return goldens.ErrorRateTest(GetJSObject());
}

void DPStressTest::ErrorCallback(void* goldenSurfaces, UINT32 loop, RC rc, UINT32 surf)
{
    static_cast<LwdaGoldenSurfaces*>(goldenSurfaces)->
        SetRC(static_cast<int>(surf), rc);
}

void DPStressTest::PrintCallback(void* test)
{
    static_cast<DPStressTest*>(test)->Print(Tee::PriNormal);
}

void DPStressTest::Print(INT32 pri)
{
    Printf(pri, "CRC error on loop %u, block %u\n", m_LwrLoop, m_LwrBlock);
    GetJsonExit()->AddFailLoop(m_LwrLoop);
}

void DPStressTest::GenerateRandomData(UINT32* pSeed, double* pBegin, double* pEnd) const
{
    UINT32 newSeed = 0;
    for (UINT32 ithread=0; ithread < m_NumThreads; ithread++)
    {
        UINT32 localSeed = *pSeed + ithread;

        UINT32* const intBuf = reinterpret_cast<UINT32*>(pBegin);
        const size_t bufSize = (pEnd - pBegin) * sizeof(double) / sizeof(UINT32);
        for (UINT32 i=ithread; i < bufSize; i+=m_NumThreads)
        {
            // Generate 32-bit random number
            localSeed *= 0x8088405U;
            UINT32 val = localSeed & 0xFFFF0000U;
            localSeed *= 0x8088405U;
            val |= localSeed >> 16;

            // Callwlate valid exponent in range -1..2
            if (i & 1)
            {
                const UINT32 exp = (val & 0x00300000U) + 0x3FE00000U;
                val = exp | (val & 0x800FFFFFU);
            }

            // Store it in the output array
            intBuf[i] = val;
        }

        if (ithread == 0)
        {
            newSeed = localSeed;
        }
    }
    *pSeed = newSeed;
}

RC DPStressTest::Verify(const double* kernelOutput, const LaunchParams& launchParams)
{
    double x = 0;

    if (m_UseSharedMemory)
    {
        UINT32 seed = launchParams.seed;
        for (UINT32 iloop=0; iloop < launchParams.loops; iloop++)
        {
            const UINT32 rowSize = launchParams.rowSize;
            vector<double> array(rowSize*3);

            // Generate random data
            GenerateRandomData(&seed, &array[0], (&array[0])+array.size());

            // Perform computations
            double* tptr = &array[0];
            double* const tend = tptr + rowSize;
            do
            {
                x += launchParams.alpha * tptr[0] * tptr[rowSize];
                x += launchParams.beta * tptr[rowSize*2];
                tptr++;
            }
            while (tptr < tend);
        }
    }
    else
    {
        // Generate random data
        vector<double> array(m_BufferSize/sizeof(double));
        for (UINT32 iblk=0; iblk < m_RandomGenBlocks; iblk++)
        {
            UINT32 seed = launchParams.seed + (iblk*0x8088405U) + m_RandomGenBlocks;
            const size_t step = array.size() / m_RandomGenBlocks;
            double* const pBegin = &array[0] + step*iblk;
            GenerateRandomData(&seed, pBegin, pBegin+step);
        }

        // Perform computations
        double* tptr = &array[0];
        double* const tend = tptr + array.size();
        do
        {
            x += tptr[0] * tptr[1];
            x += launchParams.alpha * tptr[2];
            x += launchParams.beta * tptr[3];
            tptr += 4;
        }
        while (tptr+4 <= tend);
    }

    // Check results
    const UINT32 maxAllowedDifference = 1 << m_MaxAllowedErrorBits;
    bool failed = false;
    const INT64 cpuValue = *reinterpret_cast<INT64*>(&x);
    for (UINT32 iblock=0; iblock < m_NumBlocks; iblock++)
    {
        for (UINT32 ithread=0; ithread < m_NumThreads; ithread++)
        {
            const INT64 gpuValue = *reinterpret_cast<const INT64*>(
                    kernelOutput + iblock*m_NumThreads + ithread);
            const INT64 diff =
                (cpuValue > gpuValue) ? (cpuValue - gpuValue) : (gpuValue - cpuValue);
            if (diff > maxAllowedDifference)
            {
                if (!failed)
                {
                    failed = true;
                    Printf(Tee::PriError, "Invalid values computed by the GPU!\n");
                }
                bool allGpuValuesSame = false;
                if (ithread == 0)
                {
                    allGpuValuesSame = true;
                    const UINT32 offs = iblock * m_NumThreads;
                    for (UINT32 i=1; i < m_NumThreads; i++)
                    {
                        if (kernelOutput[offs+i-1] != kernelOutput[offs+i])
                        {
                            allGpuValuesSame = false;
                            break;
                        }
                    }
                }
                if (allGpuValuesSame)
                {
                    Printf(Tee::PriError,
                            "Block %u, all threads: actual=0x%llx, expected=0x%llx\n",
                            iblock, gpuValue, cpuValue);
                    break;
                }
                else
                {
                    Printf(Tee::PriError,
                            "Block %u, thread %u: actual=0x%llx, expected=0x%llx\n",
                            iblock, ithread, gpuValue, cpuValue);
                }
            }
        }
    }

    return failed ? RC::GOLDEN_VALUE_MISCOMPARE : OK;
}
