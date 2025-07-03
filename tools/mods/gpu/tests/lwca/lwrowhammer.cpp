/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/lwdamemtest.h"
#include "core/include/jscript.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/floorsweepimpl.h"
#include "gpu/tests/lwca/mats/lwrowhammer.h"
#include "core/utility/ptrnclss.h"
#include "core/include/xp.h"
#include "gpu/include/gpupm.h"
#include "core/include/jsonlog.h"
#include "core/include/utility.h"
#include <deque>

using namespace LwdaRowHammerTest;

//! LWCA-based row hammer test.
class LwdaRowHammer : public LwdaMemTest
{
public:
    LwdaRowHammer();
    virtual ~LwdaRowHammer() { }
    virtual bool IsSupported();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    // JS property accessors
    SETGET_PROP(Iterations,  UINT32);
    SETGET_PROP(MinRow,      UINT32);
    SETGET_PROP(MaxRow,      UINT32);
    SETGET_PROP(UsePerfMons, bool);
    SETGET_PROP(PerfL2Slice, UINT32);

private:
    struct DRAMGeometry
    {
        UINT32 partitions    = 0U;
        UINT32 subpartitions = 0U;
        UINT32 ranks         = 0U;
        UINT32 banks         = 0U;
    };
    DRAMGeometry ComputeDRAMGeometry();
    RC FillMemory(UINT32 pattern);
    UINT64 EncodeLocation
    (
        UINT32 index, // multiple burst orders in the same row
        UINT32 partition,
        UINT32 subpartition,
        UINT32 rank,
        UINT32 bank,
        UINT32 row
    );
    typedef map<UINT64, UINT64> LocMap;
    RC ExerciseBankRow
    (
        UINT32        loop,
        UINT32        bank,
        UINT32        row,
        UINT32        pattern,
        const LocMap& locations
    );
    void RecordRunTime(UINT32 timeMs);

    // Since LwdaMemTest defines virtual ReportErrors(), we need to add the
    // using here to ensure it doesn't get hidden by the overridden ReportErrors
    // defined below
    using LwdaMemTest::ReportErrors;
    bool ReportErrors(UINT32 loop, const Errors& errors, UINT32 pattern);

    // JS properties
    UINT32 m_Iterations  = 0U;
    UINT32 m_MinRow      = ~0U;
    UINT32 m_MaxRow      = ~0U;
    bool   m_UsePerfMons = false;
    UINT32 m_PerfL2Slice = 0U;

    Lwca::Module       m_Module;
    Lwca::Function     m_InitErrors;
    Lwca::Function     m_RowHammer;
    Lwca::Function     m_CheckMem;
    Lwca::DeviceMemory m_Pointers;
    Lwca::DeviceMemory m_Errors;

    DRAMGeometry m_Geometry;
    UINT32       m_MaxReportedErrors = 0U;
    UINT32       m_MaxRunTimeMs      = 0U;
    UINT32       m_ReadsPerRow       = 0U;
    UINT64       m_HitCount          = 0U;
    UINT64       m_MissCount         = 0U;
    UINT32       m_PerfRuns          = 0U;
};

JS_CLASS_INHERIT(LwdaRowHammer, LwdaMemTest, "LWCA-based row hammer test");

CLASS_PROP_READWRITE(LwdaRowHammer, Iterations, UINT32,
                     "Number of times to repeat test");
CLASS_PROP_READWRITE(LwdaRowHammer, MinRow, UINT32,
                     "First row to test");
CLASS_PROP_READWRITE(LwdaRowHammer, MaxRow, UINT32,
                     "Last row to test");
CLASS_PROP_READWRITE(LwdaRowHammer, UsePerfMons, bool,
                     "Set up performance monitors");
CLASS_PROP_READWRITE(LwdaRowHammer, PerfL2Slice, UINT32,
                     "L2 slice on which perf is measured");

LwdaRowHammer::LwdaRowHammer()
{
    SetName("LwdaRowHammer");
}

bool LwdaRowHammer::IsSupported()
{
    if (!LwdaMemTest::IsSupported())
        return false;

    // Not supported on CheetAh for now, no way to enable L2 bypass
    // This test needs L2 disabled
    if (GetBoundGpuSubdevice()->IsSOC() ||
            !GetBoundGpuSubdevice()->CanL2BeDisabled())
        return false;

    return true;
}

void LwdaRowHammer::PrintJsProperties(Tee::Priority pri)
{
    LwdaMemTest::PrintJsProperties(pri);

    Printf(pri, "LwdaRowHammer Js Properties:\n");
    Printf(pri, "\tIterations:\t\t\t%u\n",  m_Iterations);
    Printf(pri, "\tMinRow:\t\t\t\t%u\n",    m_MinRow);
    Printf(pri, "\tMaxRow:\t\t\t\t%u\n",    m_MaxRow);
    Printf(pri, "\tUsePerfMons:\t\t\t%s\n", m_UsePerfMons ? "true" : "false");
    Printf(pri, "\tPerfL2Slice:\t\t\t%u\n", m_PerfL2Slice);
}

RC LwdaRowHammer::Setup()
{
    RC rc;
    CHECK_RC(LwdaMemTest::Setup());

    const UINT32 numSMs     = GetBoundLwdaDevice().GetShaderCount();
    const UINT32 maxThreads = 1024; // must match the value in lwrowhammer.lw
    const UINT32 warpSize   = 32;
    const UINT32 maxWarps   = (2 * numSMs * maxThreads) / warpSize;

    // Initialize module
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("lwrowhammer", &m_Module));

    // Prepare kernels
    m_InitErrors = m_Module.GetFunction("InitErrors", 1, 1);
    CHECK_RC(m_InitErrors.InitCheck());
    m_RowHammer = m_Module.GetFunction("RowHammer");
    CHECK_RC(m_RowHammer.InitCheck());
    m_CheckMem = m_Module.GetFunction("CheckMem", numSMs, maxThreads);
    CHECK_RC(m_CheckMem.InitCheck());

    // Allocate memory for results
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(1024*1024, &m_Errors));

    // Allocate memory for driving the test
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(maxWarps*sizeof(UINT64)*2*READS_PER_ROW, &m_Pointers));

    // Set default number of iterations
    if (m_Iterations == 0)
    {
        FrameBuffer* const pFB = GetBoundGpuSubdevice()->GetFB();
        switch (pFB->GetRamProtocol())
        {
            case FrameBuffer::RamDDR3: m_Iterations =  420000; break;
            default:                   m_Iterations = 1300000; break;
        }
    }

    // Allocate memory to test
    CHECK_RC(AllocateEntireFramebuffer());

    // Display some chunk
    CHECK_RC(DisplayAnyChunk());

    // We need enough warps to cover each DRAM chip,
    // x2 because we will be switching between two rows on each DRAM chip.
    const DRAMGeometry geometry = ComputeDRAMGeometry();
    const UINT32       numWarps = geometry.partitions
                                * geometry.subpartitions
                                * geometry.ranks;
    MASSERT(numWarps > 0);
    m_Geometry = geometry;

    m_ReadsPerRow = 1;
    while (numWarps * m_ReadsPerRow * 2 <= maxWarps)
        m_ReadsPerRow *= 2;

    // Callwlate number of blocks and threads to launch with RowHammer
    UINT32 numBlocks  = 1;
    UINT32 numThreads = numWarps * warpSize * m_ReadsPerRow;
    while (numThreads > maxThreads)
    {
        MASSERT((numThreads & 1) == 0);
        if ((numThreads & 1) != 0)
        {
            return RC::SOFTWARE_ERROR;
        }
        numBlocks *= 2;
        numThreads /= 2;
    }
    m_RowHammer.SetGridDim(numBlocks);
    m_RowHammer.SetBlockDim(numThreads);

    const INT32 pri = GetVerbosePrintPri();
    Printf(pri, "LwdaRowHammer configuration:\n");
    Printf(pri, " - Partitions    %u\n", geometry.partitions);
    Printf(pri, " - Subpartitions %u\n", geometry.subpartitions);
    Printf(pri, " - Ranks         %u\n", geometry.ranks);
    Printf(pri, " - Banks         %u\n", geometry.banks);
    Printf(pri, " - Warps         %u\n", numWarps);
    Printf(pri, " - Blocks        %u\n", numBlocks);
    Printf(pri, " - Threads       %u\n", numThreads);
    Printf(pri, " - Iterations    %u\n", m_Iterations);

    return OK;
}

RC LwdaRowHammer::Cleanup()
{
    m_Pointers.Free();
    m_Errors.Free();
    m_Module.Unload();
    return LwdaMemTest::Cleanup();
}

LwdaRowHammer::DRAMGeometry LwdaRowHammer::ComputeDRAMGeometry()
{
    DRAMGeometry geometry = {};

#ifdef _DEBUG
    set<UINT32> allPartitions;
    set<UINT32> allSubpartitions;
    set<UINT32> allRanks;
    set<UINT32> allBanks;
#endif

    FrameBuffer* const pFB = GetBoundGpuSubdevice()->GetFB();

    MASSERT(NumChunks() > 0);
    const GpuUtility::MemoryChunkDesc& chunk = GetChunkDesc(0);

    const UINT64 physAddr = VirtualToPhysical(GetLwdaChunk(0).GetDevicePtr());

    const UINT32 testRange = 8*1024*1024;

    for (PHYSADDR offs = 0; offs < testRange; offs += sizeof(UINT32))
    {
        FbDecode decode;
        pFB->DecodeAddress(&decode,
                           physAddr+offs,
                           chunk.pteKind,
                           chunk.pageSizeKB);

        geometry.partitions    = max(decode.virtualFbio+1,  geometry.partitions);
        geometry.subpartitions = max(decode.subpartition+1, geometry.subpartitions);
        geometry.ranks         = max(decode.rank+1,         geometry.ranks);
        geometry.banks         = max(decode.bank+1,         geometry.banks);

#ifdef _DEBUG
        allPartitions   .insert(decode.virtualFbio);
        allSubpartitions.insert(decode.subpartition);
        allRanks        .insert(decode.rank);
        allBanks        .insert(decode.bank);
#endif
    }

#ifdef _DEBUG
    MASSERT(allPartitions.size()    == geometry.partitions);
    MASSERT(allSubpartitions.size() == geometry.subpartitions);
    MASSERT(allRanks.size()         == geometry.ranks);
    MASSERT(allBanks.size()         == geometry.banks);
#endif

    return geometry;
}

RC LwdaRowHammer::FillMemory(UINT32 pattern)
{
    RC rc;
    for (UINT32 ichunk = 0; ichunk < NumChunks(); ichunk++)
    {
        CHECK_RC(GetLwdaChunk(ichunk).Fill(pattern));
    }
    return rc;
}

namespace
{
    const UINT32 indexBits   = 4;
    const UINT32 partBits    = 3;
    const UINT32 subpartBits = 1;
    const UINT32 rankBits    = 1;
    const UINT32 bankBits    = 6;
}

UINT64 LwdaRowHammer::EncodeLocation
(
    UINT32 index,
    UINT32 partition,
    UINT32 subpartition,
    UINT32 rank,
    UINT32 bank,
    UINT32 row
)
{
    MASSERT(index        < (1U << indexBits));
    MASSERT(partition    < (1U << partBits));
    MASSERT(subpartition < (1U << subpartBits));
    MASSERT(rank         < (1U << rankBits));
    MASSERT(bank         < (1U << bankBits));
    return  (index        & ~(~0U << indexBits  )) |
           ((partition    & ~(~0U << partBits   )) << indexBits) |
           ((subpartition & ~(~0U << subpartBits)) << (indexBits+partBits)) |
           ((rank         & ~(~0U << rankBits   )) << (indexBits+partBits+subpartBits)) |
           ((bank         & ~(~0U << bankBits   )) << (indexBits+partBits+subpartBits+rankBits)) |
           (static_cast<UINT64>(row)               << (indexBits+partBits+subpartBits+rankBits+bankBits));
}

RC LwdaRowHammer::Run()
{
    RC rc;

    GpuSubdevice* const pSubdev = GetBoundGpuSubdevice();
    FrameBuffer*  const pFB     = pSubdev->GetFB();

    // Find all possible memory locations we want to touch
    Printf(GetVerbosePrintPri(), "Decoding memory layout...\n");
    LocMap locations;
    UINT32 minRow = ~0U;
    UINT32 maxRow = 0;
    for (UINT32 ichunk = 0; ichunk < NumChunks(); ichunk++)
    {
        const GpuUtility::MemoryChunkDesc& chunk = GetChunkDesc(ichunk);

        const UINT64   startVirtAddr = GetLwdaChunk(ichunk).GetDevicePtr();
        const PHYSADDR startPhysAddr = VirtualToPhysical(startVirtAddr);

        const UINT32 stepDwords = 8; // close to burst length

        for (UINT64 offs = 0; offs < chunk.size; offs += stepDwords * sizeof(UINT32))
        {
            PHYSADDR physAddr;
            if (chunk.contiguous)
                physAddr = startPhysAddr + offs;
            else
                physAddr = VirtualToPhysical(startVirtAddr + offs);

            FbDecode decode;
            CHECK_RC(pFB->DecodeAddress(&decode, physAddr,
                                        chunk.pteKind, chunk.pageSizeKB));

            const UINT64 location = EncodeLocation(decode.burst &
                                                   ~(~0U << indexBits),
                                                   decode.virtualFbio,
                                                   decode.subpartition,
                                                   decode.rank,
                                                   decode.bank,
                                                   decode.row);
            //Practically, row values will be less than UINT32 range max value

            if (locations.find(location) == locations.end())
                locations[location] = startVirtAddr + offs;

            minRow = min(minRow, decode.row);
            maxRow = max(maxRow, decode.row);

            // Don't starve other threads
            if ((offs & 1023) == 0)
            {
                Tasker::Yield();
            }
        }
    }
    if (m_MinRow != ~0U && m_MinRow > minRow)
        minRow = m_MinRow;
    if (m_MaxRow != ~0U && m_MaxRow < maxRow && m_MaxRow >= minRow)
        maxRow = m_MaxRow;

    // Disable the RC watchdog on the subdevice for the duration of the test
    LwRm::DisableRcWatchdog disableRcWatchdog(pSubdev);

    // Perf metrics
    m_MaxRunTimeMs = 0;
    m_HitCount     = 0;
    m_MissCount    = 0;
    m_PerfRuns     = 0;

    class RestoreFBConfig
    {
        public:
            explicit RestoreFBConfig(GpuSubdevice* pSubdev)
                : m_pSubdev(pSubdev)
            {
                pSubdev->GetL2Mode(&m_Mode);
                pSubdev->SaveFBConfig(&m_FbCfg);
            }
            ~RestoreFBConfig()
            {
                Restore();
            }
            void Restore()
            {
                if (m_NeedsRestore)
                {
                    m_pSubdev->RestoreFBConfig(m_FbCfg);
                    m_pSubdev->SetL2Mode(m_Mode);
                    m_NeedsRestore = false;
                }
            }

        private:
            GpuSubdevice*              m_pSubdev;
            GpuSubdevice::L2Mode       m_Mode         = GpuSubdevice::L2_DEFAULT;
            bool                       m_NeedsRestore = true;
            GpuSubdevice::FBConfigData m_FbCfg;
    };
    RestoreFBConfig restoreFBConfig(pSubdev);

    // Disable L2 during the test
    CHECK_RC(pSubdev->IlwalidateL2Cache(1));
    CHECK_RC(pSubdev->SetL2Mode(GpuSubdevice::L2_DISABLED));

    // Disable row coalescing and enable row activation for every access
    CHECK_RC(pSubdev->EnableFBRowDebugMode());

    // Run the actual test
    for (UINT32 loop = 0; loop < GetTestConfiguration()->Loops(); loop++)
    {
        Printf(GetVerbosePrintPri(), "Loop %u\n", loop);

        const UINT32 patterns[] = { 0U, ~0U };

        for (UINT32 ipat = 0; ipat < NUMELEMS(patterns); ipat++)
        {
            const UINT32 pattern = patterns[ipat];
            CHECK_RC(FillMemory(pattern));

            for (UINT32 bank = 0; bank < m_Geometry.banks; bank++)
            {
                UINT32 rowPrintStep = 50;
                UINT32 rowPrintA = ~0U;
                UINT32 rowPrintB = minRow;

                Printf(GetVerbosePrintPri(), " - bank %u, pattern 0x%08x\n", bank, pattern);

                for (UINT32 row = minRow; row <= maxRow; row++)
                {
                    if (row == rowPrintB)
                    {
                        rowPrintA = rowPrintB;
                        rowPrintB = min(maxRow+1, rowPrintA+rowPrintStep);
                        Printf(GetVerbosePrintPri(), "   rows %u..%u\n",
                               rowPrintA, rowPrintB-1);
                    }

                    CHECK_RC(ExerciseBankRow(loop,
                                             bank,
                                             row,
                                             pattern,
                                             locations));
                }
            }
        }

        EndLoop(loop);
    }

    // Restore L2 mode
    restoreFBConfig.Restore();

    Printf(GetVerbosePrintPri(),
           "Run time metrics: hammer time %u ms, %u accesses, %u ns per access\n",
           m_MaxRunTimeMs, m_Iterations, (m_MaxRunTimeMs * 1000000U)/m_Iterations);
    if (m_PerfRuns > 0)
    {
        Printf(GetVerbosePrintPri(), "%u hits, %u misses per run in slice %u (total %u slices)\n",
                                     static_cast<unsigned>(m_HitCount/m_PerfRuns),
                                     static_cast<unsigned>(m_MissCount/m_PerfRuns),
                                     m_PerfL2Slice,
                                     GetBoundGpuSubdevice()->GetFB()->GetMaxL2SlicesPerFbp());
    }

    return LwdaMemTest::ReportErrors();
}

RC LwdaRowHammer::ExerciseBankRow
(
    UINT32        loop,
    UINT32        bank,
    UINT32        row,
    UINT32        pattern,
    const LocMap& locations
)
{
    // Build table of pointers accessed by the LWCA kernel
    vector<UINT64, Lwca::HostMemoryAllocator<UINT64> >
        testPtrs(Lwca::HostMemoryAllocator<UINT64>(GetLwdaInstancePtr(),
                                                   GetBoundLwdaDevice()));
    testPtrs.reserve(m_Geometry.partitions
                   * m_Geometry.subpartitions
                   * m_Geometry.ranks
                   * m_ReadsPerRow
                   * READS_PER_ROW
                   * 2);
    UINT32 numSkips = 0;
    for (UINT32 col = 0; col < 2 * READS_PER_ROW * m_ReadsPerRow; col++)
    {
        for (UINT32 rank = 0; rank < m_Geometry.ranks; rank++)
        {
            for (UINT32 part = 0; part < m_Geometry.partitions; part++)
            {
                for (UINT32 subpart = 0; subpart < m_Geometry.subpartitions; subpart++)
                {
                    const UINT64 loc = EncodeLocation(
                            col & ~(~0U << indexBits), part, subpart, rank, bank, row);
                    const LocMap::const_iterator locIt = locations.find(loc);
                    if (locIt == locations.end())
                    {
                        ++numSkips;
                        testPtrs.push_back(0);
                    }
                    else
                    {
                        testPtrs.push_back(locIt->second);
                    }
                }
            }
        }
    }

    RC rc;
    Lwca::Event startEvent(GetLwdaInstance().CreateEvent());
    CHECK_RC(startEvent.InitCheck());
    Lwca::Event stopEvent(GetLwdaInstance().CreateEvent());
    CHECK_RC(stopEvent.InitCheck());

    MASSERT(m_Pointers.GetSize() >= testPtrs.size() * sizeof(testPtrs[0]));
    CHECK_RC(m_Pointers.Set(&testPtrs[0], testPtrs.size() * sizeof(testPtrs[0])));

    const UINT32 maxErrors = UNSIGNED_CAST(UINT32,
        (m_Errors.GetSize() - sizeof(Errors) + sizeof(BadValue)) / sizeof(BadValue));
    m_MaxReportedErrors = maxErrors;
    CHECK_RC(m_InitErrors.Launch(m_Errors.GetDevicePtr(), maxErrors));

    // Start perf mons
    vector<const GpuPerfmon::Experiment*> experiments;
    GpuSubdevice* const pSubdev = GetBoundGpuSubdevice();
    if (m_UsePerfMons && numSkips == 0)
    {
        CHECK_RC(GetLwdaInstance().Synchronize());
        CHECK_RC(pSubdev->IlwalidateL2Cache(0));

        GpuPerfmon* pPerfMon = 0;
        CHECK_RC(pSubdev->GetPerfmon(&pPerfMon));

        UINT32 slice = m_PerfL2Slice;
        for (UINT32 part = 0; part < m_Geometry.partitions; part++)
        {
            if (pSubdev->GetFB()->IsL2SliceValid(slice, part))
            {
                const GpuPerfmon::Experiment* pExperiment;
                CHECK_RC(pPerfMon->CreateL2CacheExperiment(part, slice, &pExperiment));
                experiments.push_back(pExperiment);
            }
        }

        if (!experiments.empty())
        {
            CHECK_RC(pPerfMon->BeginExperiments());
        }
    }

    MASSERT(m_ReadsPerRow > 0);
    CHECK_RC(startEvent.Record());
    {
        const RowHammerInput rhInput =
        {
            m_Errors.GetDevicePtr(),
            m_Pointers.GetDevicePtr(),
            m_Iterations / (2 * READS_PER_ROW * m_ReadsPerRow)
        };
        CHECK_RC(m_RowHammer.Launch(rhInput));
    }
    CHECK_RC(stopEvent.Record());

    // Stop perf mons
    if (!experiments.empty())
    {
        CHECK_RC(GetLwdaInstance().Synchronize());

        GpuPerfmon* pPerfMon = 0;
        CHECK_RC(pSubdev->GetPerfmon(&pPerfMon));

        CHECK_RC(pPerfMon->EndExperiments());

        for (UINT32 i = 0; i < experiments.size(); i++)
        {
            GpuPerfmon::Results results;
            pPerfMon->GetResults(experiments[i], &results);
            CHECK_RC(pPerfMon->DestroyExperiment(experiments[i]));
            m_HitCount  += results.Data.CacheResults.HitCount;
            m_MissCount += results.Data.CacheResults.MissCount;
        }

        ++m_PerfRuns;
    }

    for (UINT32 ichunk = 0; ichunk < NumChunks(); ichunk++)
    {
        const Lwca::ClientMemory& mem = GetLwdaChunk(ichunk);
        const CheckMemInput cmInput =
        {
            m_Errors.GetDevicePtr(),
            mem.GetDevicePtr(),
            mem.GetSize(),
            pattern
        };
        CHECK_RC(m_CheckMem.Launch(cmInput));
    }

    vector<char, Lwca::HostMemoryAllocator<char> >
        errorsBuf(m_Errors.GetSize(),
                  char(),
                  Lwca::HostMemoryAllocator<char>(GetLwdaInstancePtr(),
                                                  GetBoundLwdaDevice()));
    CHECK_RC(m_Errors.Get(&errorsBuf[0], errorsBuf.size()));
    const Errors& errors = *reinterpret_cast<Errors*>(&errorsBuf[0]);

    CHECK_RC(GetLwdaInstance().Synchronize());

    RecordRunTime(stopEvent - startEvent);
    const bool ok = ReportErrors(loop, errors, pattern);
    if (!ok)
    {
        if (GetGoldelwalues()->GetStopOnError())
        {
            LwdaMemTest::ReportErrors();
            rc = RC::BAD_MEMORY;
        }
        else
        {
            CHECK_RC(FillMemory(pattern));
        }
    }

    return rc;
}

void LwdaRowHammer::RecordRunTime(UINT32 timeMs)
{
    if (timeMs > m_MaxRunTimeMs)
        m_MaxRunTimeMs = timeMs;
}

bool LwdaRowHammer::ReportErrors(UINT32 loop, const Errors& errors, UINT32 pattern)
{
    class LogErrorsOnReturn
    {
        public:
            LogErrorsOnReturn(UINT32 loop, JsonItem* jsonItem, MemError& memError)
                : m_JsonItem(jsonItem)
                , m_Loop(loop)
                , m_MemError(memError)
                , m_HasErrors(false)
                , m_NumMysteryErrors(0)
                , m_MaxShownCorruptions(10)
            {
            }
            ~LogErrorsOnReturn()
            {
                if (m_HasErrors)
                {
                    m_JsonItem->AddFailLoop(m_Loop);
                }
            }
            void SetError()
            {
                m_HasErrors = true;
            }
            void LogMysteryError()
            {
                m_HasErrors = true;
                m_MemError.LogMysteryError();
                ++m_NumMysteryErrors;
            }
            UINT32 NumCorruptions() const
            {
                return m_NumMysteryErrors;
            }
            bool DisplayErrorInfo() const
            {
                return m_NumMysteryErrors <= m_MaxShownCorruptions;
            }

        private:
            JsonItem* m_JsonItem;
            UINT32    m_Loop;
            MemError& m_MemError;
            bool      m_HasErrors;
            UINT32    m_NumMysteryErrors;
            UINT32    m_MaxShownCorruptions;
    };
    LogErrorsOnReturn errlog(loop, GetJsonExit(), GetMemError(0));

    // Validate errors
    if ((errors.numReportedErrors     >  errors.maxReportedErrors)
         || (errors.maxReportedErrors != m_MaxReportedErrors)
         || (errors.numReportedErrors >  errors.numErrors))
    {
        errlog.LogMysteryError();
    }
    if ((errors.numErrors > 0) && (errors.badBits == 0))
    {
        errlog.LogMysteryError();
    }
    if (errlog.NumCorruptions() > 0)
    {
        const INT32 pri = Tee::PriNormal;
        Printf(pri, "Corrupted results detected\n");
        Printf(pri, " - numErrors         %llu\n",   errors.numErrors);
        Printf(pri, " - badBits           0x%08x\n", errors.badBits);
        Printf(pri, " - numReportedErrors %u\n",     errors.numReportedErrors);
        Printf(pri, " - maxReportedErrors %u (should be %u)\n",
                    errors.maxReportedErrors, m_MaxReportedErrors);
        return false;
    }

    if (errors.numReportedErrors <= m_MaxReportedErrors)
    {
        for (UINT32 ierr=0; ierr < errors.numReportedErrors; ierr++)
        {
            const BadValue& badValue = errors.errors[ierr];
            if (badValue.value == pattern)
            {
                errlog.LogMysteryError();
                if (errlog.DisplayErrorInfo())
                {
                    Printf(Tee::PriNormal, "Detected corruption: actual and expected value the same\n");
                }
            }
        }
    }
    if (errlog.NumCorruptions())
    {
        return false;
    }

    // Report unlogged errors
    const UINT64 numUnloggedErrors
        = errors.numErrors - errors.numReportedErrors;
    if (numUnloggedErrors > 0)
    {
        for (UINT64 i=0; i < numUnloggedErrors; i++)
        {
            errlog.LogMysteryError();
        }
    }

    if (errors.numErrors > 0)
    {
        errlog.SetError();
    }

    // Report logged errors
    for (UINT32 ierr=0; ierr < errors.numReportedErrors; ierr++)
    {
        const BadValue& badValue = errors.errors[ierr];

        // Colwert error's virtual address to physical address
        const UINT64 fbOffset = VirtualToPhysical(badValue.addr);
        if (fbOffset == ~0ULL)
        {
            errlog.LogMysteryError();
            if (errlog.DisplayErrorInfo())
            {
                Printf(Tee::PriNormal, "Couldn't find a memory allocation "
                    "matching error detected at virtual address 0x%llx\n",
                    badValue.addr);
            }
            continue;
        }

        LogError(fbOffset, badValue.value, pattern,
                badValue.reread1, badValue.reread2, 0,
                GetChunkDesc(FindChunkByVirtualAddress(badValue.addr)));
    }

    return errors.numErrors == 0 && errlog.NumCorruptions() == 0;
}
