/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/lwdastst.h"
#include "gpu/tests/lwca/ecc/lwdaeccl2.h"
#include "gpu/utility/ecccount.h"
#include "gpu/utility/mapfb.h"
#include "core/include/platform.h"

namespace
{
    enum CmdIndexes
    {
        SM_INIT_IDX = 0,
        HOST_PROCEED_IDX,
        SM_READY_IDX,
        SM_DISABLE_CHKBITS_IDX,
        SM_ENABLE_CHKBITS_IDX,
        SM_RESET_ERRS_IDX,
        SM_DONE_IDX
    };

    vector<UINT32> s_64x8SyndromeCommands =
    {
        SM_INIT,
        HOST_PROCEED_64x8,
        SM_READY_64x8,
        SM_DISABLE_CHKBITS_64x8,
        SM_ENABLE_CHKBITS_64x8,
        SM_RESET_ERRS_64x8,
        SM_DONE_64x8
    };

    vector<UINT32> s_256x16SyndromeCommands =
    {
        SM_INIT,
        HOST_PROCEED_256x16,
        SM_READY_256x16,
        SM_DISABLE_CHKBITS_256x16,
        SM_ENABLE_CHKBITS_256x16,
        SM_RESET_ERRS_256x16,
        SM_DONE_256x16
    };

};
//! LWCA-based test for testing L2 Data Cache ECC error detection
//!
//! Each LWCA block injects errors in a specific L2 slice and checks that
//! the number of detected errors is less than the number injected errors.
//!
//! Assumes the L2 Data Cache has a full ECC unit, not parity.
//!
class LwdaEccL2 : public LwdaStreamTest
{
public:
    LwdaEccL2();
    virtual ~LwdaEccL2() { }
    virtual bool IsSupported();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    // JS property accessors
    SETGET_PROP(IntrWaitTimeMs, UINT32);
    SETGET_PROP(Iterations, UINT64);
    SETGET_PROP(NumErrors, UINT32);

private:
    RC CheckErrCounts(UINT64 outerLoop, UINT64 innerLoop);
    RC SaveResetErrCounts(vector<GpuSubdevice::L2EccCounts> &l2EccCounts);
    RC RestoreErrCounts(vector<GpuSubdevice::L2EccCounts> &l2EccCounts);

    RC CheckInterrupts(LwRm::ExclusiveLockRm& lockRm, UINT64 outerLoop, UINT64 innerLoop);
    RC GetErrCountsRM(UINT64* pSbeCount, UINT64* pDbeCount);

    void SendMsg(UINT32 msg);
    UINT32 FetchMsg();

    RC TestLoop(LwRm::ExclusiveLockRm& lockRm, UINT64 outerLoop, bool testInterrupts);

    // JS properties
    UINT32 m_IntrWaitTimeMs;
    UINT64 m_Iterations;
    UINT32 m_NumErrors;

    UINT32 m_SlicesToTest;

    Lwca::Module        m_Module;
    Lwca::Function      m_LwdaEccL2;
    Lwca::Function      m_LwdaInitEccL2;
    MapFb               m_MapFb;
    Surface2D           m_InfoSurf;
    Surface2D           m_LwdaMemSurf;
    Lwca::ClientMemory  m_InfoMem;
    Lwca::ClientMemory  m_LwdaMem;
    UINT64              m_PrevSbeCount = 0;
    UINT64              m_PrevDbeCount = 0;
    vector<UINT32> *    m_KernelInterfaceCommands = nullptr;
};

JS_CLASS_INHERIT(LwdaEccL2, LwdaStreamTest, "LWCA ECC L2 Test");
CLASS_PROP_READWRITE(LwdaEccL2, IntrWaitTimeMs, UINT32,
                     "Time to wait when test starts/ends for pending ECC interrupts");
CLASS_PROP_READWRITE(LwdaEccL2, Iterations, UINT64,
                     "Number of error injection loops inside LwdaEccL2 kernel. "
                     "Iterations should be preferred over Loops for increasing coverage.");
CLASS_PROP_READWRITE(LwdaEccL2, NumErrors, UINT32,
                     "Number of SBE and number of DBE errors to inject - per L2 slice - per Iteration");

LwdaEccL2::LwdaEccL2()
: m_IntrWaitTimeMs(100)
, m_Iterations(10000)
, m_NumErrors(32)
, m_SlicesToTest(0)
{
    SetName("LwdaEccL2");
}

bool LwdaEccL2::IsSupported()
{
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }

    // GP100 and GV100 for now. CheetAh GP10b/GV10b should be able to work with minor modifications.
    // With more serious modifications GK110b/GK210 could also work.
    if (GetBoundLwdaDevice().GetCapability() != Lwca::Capability::SM_60 &&
        GetBoundLwdaDevice().GetCapability() != Lwca::Capability::SM_70 &&
        GetBoundLwdaDevice().GetCapability() != Lwca::Capability::SM_75 &&
        GetBoundLwdaDevice().GetCapability() != Lwca::Capability::SM_80)
    {
        Printf(Tee::PriLow, "This test is not supported on the GPU under test\n");
        return false;
    }

    // Check ECC L2 support
    bool eccSupported = false;
    UINT32 eccEnableMask = 0;
    if (OK != pSubdev->GetEccEnabled(&eccSupported, &eccEnableMask) || !eccSupported)
    {
        Printf(Tee::PriLow, "ECC not supported\n");
        return false;
    }
    if (!Ecc::IsUnitEnabled(Ecc::Unit::L2, eccEnableMask))
    {
        Printf(Tee::PriLow, "ECC L2 not enabled or supported\n");
        return false;
    }

    // Verify that checkbits can be toggled
    {
        LwRm::ExclusiveLockRm lockRm(GetBoundRmClient());
        lockRm.AcquireLock();

        // Save ECC checkbits state and restore on exit
        const bool bEccSaveState = pSubdev->GetL2EccCheckbitsEnabled();
        DEFER
        {
            pSubdev->SetL2EccCheckbitsEnabled(bEccSaveState);
        };

        // Flip ECC checkbits state
        if (OK != pSubdev->SetL2EccCheckbitsEnabled(!bEccSaveState))
        {
            return false;
        }

        // Confirm checkbits were changed
        const bool bLwrEccEnable = pSubdev->GetL2EccCheckbitsEnabled();
        if (bLwrEccEnable != !bEccSaveState)
        {
            Printf(Tee::PriLow, "Can't toggle L2 ECC checkbits state. Check your VBIOS!\n");
            return false;
        }
    }

    // PMU ucode accesses L2 regardless of whether we lock RM since it runs independently
    if (pSubdev->GetPerf()->HasPStates())
    {
        Printf(Tee::PriLow, "%s on the GPU under test requires that pstates be disabled\n",
               GetName().c_str());
        return false;
    }

    return GetBoundGpuSubdevice()->GetFB()->GetL2CacheSize() > 0 &&
           GetBoundGpuSubdevice()->GetFB()->GetL2EccSectorSize() > 0;
}

RC LwdaEccL2::InitFromJs()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::InitFromJs());

    // Error counters saturate at 255 errors, so only allow a maximum of 254 SBE or DBE to be injected
    if (m_NumErrors > 254)
    {
        Printf(Tee::PriError, "Too many errors injected: %d max %d\n", m_NumErrors, 254);
        rc = RC::ILWALID_ARGUMENT;
    }

    //
    // Warn on reduced coverage due to injecting too many errors.
    //
    // This is required due to Bug 1802052, the more errors that are injected into an L2 slice the higher
    // the chance that an ECC error is reported more than once.
    //
    // Maximum suggested number of errors to inject was arrived at via the following process:
    //
    // 1) Record the number of times ECC errors are erroneously reported on one GP100 L2 slice,
    //    running the test for 1,000,000 iterations
    //
    // 2) Fit an exponential lwrve to this test data
    //
    // 3) Use this to derive an estimate of the probability of an erroneous error report
    //    P(overcount) = (3E-17 * NumErrors^8.1297)
    //
    // 4) Assume likelihood of encountering HW issue increases linearly with number of injected errors
    //    P(hwerror) = (q * NumErrors) where q is some constant
    //
    // 5) Assuming that these two equations are independent
    //    P(detection) = P(hwerror) * (1 - P(overcount))
    //
    // 6) Optimizing this equation to maximize P(detection) results in
    //    NumErrors    = 82.0857...
    //    P(overcount) = 0.10953...
    //
    //    Thus, for this model, even though we have a 10% chance of missing an error the increased testing
    //    throughput makes up for it. Both P(overcount) and P(hwerror) are conservative estimations.
    //
    if (m_NumErrors > 80)
    {
        Printf(Tee::PriWarn, "Injecting more than ~80 errors may reduce coverage due to Bug 1802052\n");
    }

    return rc;
}

void LwdaEccL2::PrintJsProperties(Tee::Priority pri)
{
    LwdaStreamTest::PrintJsProperties(pri);

    Printf(pri, "LwdaEccL2 Js Properties:\n");
    Printf(pri, "\tIntrWaitTimeMs:\t\t%d\n", m_IntrWaitTimeMs);
    Printf(pri, "\tIterations:\t\t\t%llu\n", m_Iterations);
    Printf(pri, "\tNumErrors:\t\t%u\n", m_NumErrors);
}

RC LwdaEccL2::Setup()
{
    RC rc;
    Lwca::Device lwca = GetBoundLwdaDevice();
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    FrameBuffer* fb = pSubdev->GetFB();

    CHECK_RC(LwdaStreamTest::Setup());

    const UINT32 l2CacheSizeBytes = lwca.GetAttribute(LW_DEVICE_ATTRIBUTE_L2_CACHE_SIZE);
    VerbosePrintf("L2 Cache Size: %d KB\n", l2CacheSizeBytes / 1024);

    // Test all L2 slices
    m_SlicesToTest = fb->GetL2SliceCount();

    // Initialize module
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("eccl2", &m_Module));

    // Prepare Init kernel
    m_LwdaInitEccL2 = m_Module.GetFunction
    (
        "LwdaInitEccL2",
        lwca.GetShaderCount(),
        lwca.GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK)
    );
    CHECK_RC(m_LwdaInitEccL2.InitCheck());

    // Prepare Test Kernels
    m_LwdaEccL2 = m_Module.GetFunction("LwdaEccL2");

    // Check that test configuration allows for enough shared memory
    const UINT32 sectorsPerSlice = fb->GetL2SliceSize() / fb->GetL2EccSectorSize();
    const UINT32 mapSizePerBlock = sizeof(UINT32) * sectorsPerSlice;
    const UINT32 maxSharedPerBlock =
        lwca.GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK_OPTIN);
    const UINT32 staticShared = m_LwdaEccL2.GetAttribute(LW_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES);

    if (mapSizePerBlock > (maxSharedPerBlock - staticShared))
    {
        Printf(Tee::PriError,
           "Not enough shared memory in lwca block for allocation Need %u bytes > Max %u bytes\n",
           mapSizePerBlock, (maxSharedPerBlock - staticShared));
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    // We need to set this attribute if our dynamic shared mem is over the max static
    CHECK_RC(m_LwdaEccL2.SetAttribute(LW_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES,
                                      mapSizePerBlock));
    m_LwdaEccL2.SetSharedSize(mapSizePerBlock);
    CHECK_RC(m_LwdaEccL2.InitCheck());

    // Allocate and map host surface for storing which ECC sectors each LWCA SM can use
    const UINT32 mapSizeBytes = m_SlicesToTest * mapSizePerBlock;
    CHECK_RC(GetLwdaInstance().AllocSurfaceClientMem(mapSizeBytes, Memory::Coherent,
                                                     pSubdev, &m_InfoSurf, &m_InfoMem));
    CHECK_RC(GetLwdaInstance().AllocSurfaceClientMem(l2CacheSizeBytes, Memory::Fb,
                                                     pSubdev, &m_LwdaMemSurf, &m_LwdaMem));
    CHECK_RC(m_LwdaMem.InitCheck());

    UINT32 pteKind = m_LwdaMemSurf.GetPteKind();
    UINT32 pageSizeKB = m_LwdaMemSurf.GetActualPageSizeKB();

    // Each LWCA SM is assigned an L2 slice to inject errors into in order to
    // maximize the number of errors injected, since there is a fixed amount of ECC errors
    // that can be detected per slice.
    //
    // It also allows us to stress each slice the same for different floorsweepings without having
    // to change the test parameters.
    //
    vector<UINT32> sectorCount(m_SlicesToTest);
    for (UINT32 sector = 0; sector < l2CacheSizeBytes / fb->GetL2EccSectorSize(); sector++)
    {
        FbDecode decode;
        UINT64 physPtr = 0;
        CHECK_RC(m_LwdaMemSurf.GetPhysAddress(sector * fb->GetL2EccSectorSize(), &physPtr));
        CHECK_RC(fb->DecodeAddress(&decode, physPtr, pteKind, pageSizeKB));
        UINT32 ltcid = fb->VirtualFbioToVirtualLtc(decode.virtualFbio, decode.subpartition);
        UINT32 sliceIdx = fb->GetFirstGlobalSliceIdForLtc(ltcid) + decode.slice;

        if (sliceIdx < m_SlicesToTest)
        {
            m_InfoSurf.WritePixel(sliceIdx * sectorsPerSlice + sectorCount[sliceIdx]++, 0, sector);
        }
    }
    for (UINT32 sliceIdx = 0; sliceIdx < m_SlicesToTest; sliceIdx++)
    {
        UINT32 ltcId, localSliceId;

        CHECK_RC(pSubdev->GetFB()->GlobalToLocalLtcSliceId(sliceIdx, &ltcId, &localSliceId));
        if (sectorCount[sliceIdx] != sectorsPerSlice)
        {
            Printf(Tee::PriError, "(ltc.slice): %3u.%-3u %u sectors for slice, expected %u\n",
                   ltcId, localSliceId, sectorCount[sliceIdx], sectorsPerSlice);
            return RC::SOFTWARE_ERROR;
        }
    }

    if (GetBoundLwdaDevice().GetCapability() < Lwca::Capability::SM_75)
    {
        m_KernelInterfaceCommands = &s_64x8SyndromeCommands;
    }
    else
    {
        m_KernelInterfaceCommands = &s_256x16SyndromeCommands;
    }
    MASSERT((m_KernelInterfaceCommands->size() - 1) == CmdIndexes::SM_DONE_IDX);
    return OK;
}

RC LwdaEccL2::CheckErrCounts(UINT64 outerLoop, UINT64 innerLoop)
{
    RC rc;
    StickyRC stickyRc;
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();

    vector<GpuSubdevice::L2EccCounts> l2EccCounts(m_SlicesToTest);
    CHECK_RC(SaveResetErrCounts(l2EccCounts));
    for (UINT32 sliceIdx = 0; sliceIdx < m_SlicesToTest; sliceIdx++)
    {
        UINT32 ltcid = ~0, slice = ~0;

        pSubdev->GetFB()->GlobalToLocalLtcSliceId(sliceIdx, &ltcid, &slice);

        UINT32 sbe = l2EccCounts[sliceIdx].correctedTotal;
        UINT32 dbe = l2EccCounts[sliceIdx].uncorrectedTotal;

        // Injected errors have a small chance of being counted multiple times (Bug 1802052)
        // For ECC we really only care about missing the detection of errors, so only check for that.
        //
        // That being said, if the Bug was fixed we would have increased test coverage for L2 reads/writes.
        if (sbe < m_NumErrors || dbe < m_NumErrors)
        {
            Printf(Tee::PriError,
                   "(ltc.slice): %3u.%-3u Outer: %llu Inner: %llu Expected SBE/DBE: %u SBE: %u DBE: %u \n",
                   ltcid, slice, outerLoop, innerLoop, m_NumErrors, sbe, dbe);
            stickyRc = RC::L2_CACHE_ERROR;
        }

        bool bCorrInterrupt   = false;
        bool bUncorrInterrupt = false;
        CHECK_RC(pSubdev->GetAndClearL2EccInterrupts(ltcid,
                                                     slice,
                                                     &bCorrInterrupt,
                                                     &bUncorrInterrupt));

        if (!bCorrInterrupt)
        {
            Printf(Tee::PriError,
                   "(ltc.slice): %3u.%-3u Outer: %llu Inner: %llu SBE interrupt bit not set!\n",
                   ltcid, slice, outerLoop, innerLoop);
            stickyRc = RC::L2_CACHE_ERROR;
        }
        if (!bUncorrInterrupt)
        {
            Printf(Tee::PriError,
                   "(ltc.slice): %3u.%-3u Outer: %llu Inner: %llu DBE interrupt bit not set!\n",
                   ltcid, slice, outerLoop, innerLoop);
            stickyRc = RC::L2_CACHE_ERROR;
        }
    }
    return stickyRc;
}

RC LwdaEccL2::SaveResetErrCounts(vector<GpuSubdevice::L2EccCounts> &l2EccCounts)
{
    RC rc;
    MASSERT(l2EccCounts.size() >= m_SlicesToTest);

    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    for (UINT32 sliceIdx = 0; sliceIdx < m_SlicesToTest; sliceIdx++)
    {
        UINT32 ltcId, localSliceId;

        pSubdev->GetFB()->GlobalToLocalLtcSliceId(sliceIdx, &ltcId, &localSliceId);
        CHECK_RC(pSubdev->GetAndClearL2EccCounts(ltcId, localSliceId, &l2EccCounts[sliceIdx]));
    }
    return rc;
}

RC LwdaEccL2::RestoreErrCounts(vector<GpuSubdevice::L2EccCounts> &l2EccCounts)
{
    MASSERT(l2EccCounts.size() >= m_SlicesToTest);

    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    RC rc;
    for (UINT32 sliceIdx = 0; sliceIdx < m_SlicesToTest; sliceIdx++)
    {
        UINT32 ltcId, localSliceId;

        pSubdev->GetFB()->GlobalToLocalLtcSliceId(sliceIdx, &ltcId, &localSliceId);
        CHECK_RC(pSubdev->SetL2EccCounts(ltcId, localSliceId, l2EccCounts[sliceIdx]));
    }
    return rc;
}

RC LwdaEccL2::CheckInterrupts(LwRm::ExclusiveLockRm& lockRm, UINT64 outerLoop, UINT64 innerLoop)
{
    StickyRC stickyRc;

    // Release lock to handle interrupt
    lockRm.ReleaseLock();
    DEFER
    {
        lockRm.AcquireLock();
    };

    // Poll on L2 ECC error counts
    UINT64 sbeCount = 0;
    UINT64 dbeCount = 0;
    RC rc = Tasker::PollHw(GetTestConfiguration()->TimeoutMs(), [&]()->bool
    {
        // Get L2 ECC error counts that were reported by RM
        stickyRc = GetErrCountsRM(&sbeCount, &dbeCount);
        if (stickyRc != OK)
        {
            return true;
        }

        // Success if error counts are at least as expected
        //
        // Injected errors have a small chance of being counted multiple times (Bug 1802052)
        // For ECC we really only care about missing the detection of errors, so only check for that.
        if (sbeCount >= m_PrevSbeCount + m_SlicesToTest &&
            dbeCount >= m_PrevDbeCount + m_SlicesToTest)
        {
            return true;
        }
        else
        {
            return false;
        }
    }, __FUNCTION__);

    if (rc == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriError,
               "Timeout in Interrupt Test - Outer: %llu Inner: %llu\n"
               "Expected SBE: %llu Actual SBE: %llu\n"
               "Expected DBE: %llu Actual DBE: %llu\n",
                outerLoop, innerLoop,
                m_PrevSbeCount + m_SlicesToTest, sbeCount,
                m_PrevDbeCount + m_SlicesToTest, dbeCount);
        stickyRc = RC::L2_CACHE_ERROR;
    }
    else
    {
        stickyRc = rc;
    }

    m_PrevSbeCount = sbeCount;
    m_PrevDbeCount = dbeCount;
    return stickyRc;
}

RC LwdaEccL2::GetErrCountsRM(UINT64* pSbeCount, UINT64* pDbeCount)
{
    RC rc;
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    Ecc::DetailedErrCounts L2Params;

    Tasker::MutexHolder Lock(pSubdev->GetEccErrCounter()->GetMutex());
    CHECK_RC(pSubdev->GetSubdeviceFb()->GetLtcDetailedEccCounts(&L2Params));

    UINT64 corrCount = 0;
    UINT64 uncorrCount = 0;
    for (UINT32 ltcid = 0; ltcid < L2Params.eccCounts.size(); ltcid++)
    {
        for (UINT32 slice = 0; slice < L2Params.eccCounts[ltcid].size(); slice++)
        {
            corrCount += L2Params.eccCounts[ltcid][slice].corr;
            uncorrCount += L2Params.eccCounts[ltcid][slice].uncorr;
        }
    }

    // RM reports in terms of corr/uncorr. The test assumes that SBE/DBE maps to
    // corr/uncorr.
    *pSbeCount = corrCount;
    *pDbeCount = uncorrCount;

    return OK;
}

void LwdaEccL2::SendMsg(UINT32 msg)
{
    const UINT32 eccSectorSizeDwords = GetBoundGpuSubdevice()->GetFB()->GetL2EccSectorSize() / sizeof(UINT32);
    for (UINT32 sliceIdx = 0; sliceIdx < m_SlicesToTest; sliceIdx++)
    {
        m_LwdaMemSurf.WritePixel(sliceIdx * eccSectorSizeDwords, 0, msg);
    }
}

// Fetches message from LWCA blocks
// Only return when all LWCA blocks have the same message
UINT32 LwdaEccL2::FetchMsg()
{
    const UINT32 eccSectorSizeDwords = GetBoundGpuSubdevice()->GetFB()->GetL2EccSectorSize() / sizeof(UINT32);

    UINT32 lwrr = m_KernelInterfaceCommands->at(CmdIndexes::SM_INIT_IDX);
    UINT32 runningCount = 0;
    for (UINT32 sliceIdx = 0; runningCount < m_SlicesToTest; sliceIdx = (sliceIdx + 1) % m_SlicesToTest)
    {
        UINT32 msg = m_LwdaMemSurf.ReadPixel(sliceIdx * eccSectorSizeDwords, 0);
        if (msg == lwrr)
        {
            runningCount++;
        }
        else
        {
            lwrr = msg;
            runningCount = 1;
        }
    }
    return lwrr;
}

RC LwdaEccL2::Run()
{
    RC rc;
    StickyRC stickyRc;
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    EccErrCounter* pEccErrCounter = pSubdev->GetEccErrCounter();

    vector<GpuSubdevice::L2EccCounts> l2EccCounts(m_SlicesToTest);
    VerbosePrintf("LwdaEccL2 is testing LWCA device \"%s\"\n", GetBoundLwdaDevice().GetName().c_str());

    LwRm::ExclusiveLockRm lockRm(GetBoundRmClient());
    for (UINT64 outer = 0; outer < GetTestConfiguration()->Loops(); outer++)
    {
        // Part 1: Test the injection and detection of multiple ECC errors
        {
            lockRm.AcquireLock();
            CHECK_RC(SaveResetErrCounts(l2EccCounts));

            // On each loop iteration or on exit restore error counts and release RM lock
            DEFER
            {
                RestoreErrCounts(l2EccCounts);
                lockRm.ReleaseLock();
            };

            stickyRc = TestLoop(lockRm, outer, false);
            if (GetGoldelwalues()->GetStopOnError())
            {
                CHECK_RC(stickyRc);
            }
        }

        // Part 2: Test the Interrupt-handling pathway (RM) for ECC L2 errors.
        //         Here we inject one SBE and one DBE per L2 slice.
        {
            // RM uses an ISR to update the ECC error counts,
            // so there is a slight delay before the counts update.
            // In practice this wait might not be needed as long as we yield,
            // but better to be safe.
            Tasker::Sleep(m_IntrWaitTimeMs);

            // Get initial SBE/DBE counts from RM
            CHECK_RC(GetErrCountsRM(&m_PrevSbeCount, &m_PrevDbeCount));

            // Begin expecting errors to prevent the test from failing on exit
            CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::L2_CORR));
            CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::L2_UNCORR));

            // Acquire lock and start subtest
            lockRm.AcquireLock();
            DEFER
            {
                lockRm.ReleaseLock();
                Tasker::Sleep(m_IntrWaitTimeMs);
                pEccErrCounter->EndExpectingErrors(EccErrCounter::L2_CORR);
                pEccErrCounter->EndExpectingErrors(EccErrCounter::L2_UNCORR);
            };
            stickyRc = TestLoop(lockRm, outer, true);
            if (GetGoldelwalues()->GetStopOnError())
            {
                CHECK_RC(stickyRc);
            }
        }
    }
    return stickyRc;
}

RC LwdaEccL2::TestLoop(LwRm::ExclusiveLockRm& lockRm, UINT64 outerLoop, bool testInterrupts)
{
    RC rc;
    StickyRC stickyRc;
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();

    // Zero memory to start with
    const UINT32 l2SliceSizeDwords = pSubdev->GetFB()->GetL2SliceSize() / sizeof(UINT32);
    CHECK_RC(m_LwdaInitEccL2.Launch(m_LwdaMem.GetDevicePtr(), l2SliceSizeDwords * m_SlicesToTest));

    // Time kernel run
    Lwca::Event startEvent(GetLwdaInstance().CreateEvent());
    Lwca::Event stopEvent(GetLwdaInstance().CreateEvent());
    CHECK_RC(startEvent.Record());

    // Launch Kernel
    //
    // Half the threads inject SBE the other half DBE, so the number of threads per SM
    // must be divisible by two.
    if (testInterrupts)
    {
        m_LwdaEccL2.SetGridDim(m_SlicesToTest);
        m_LwdaEccL2.SetBlockDim(1 * 2);
    }
    else
    {
        m_LwdaEccL2.SetGridDim(m_SlicesToTest);
        m_LwdaEccL2.SetBlockDim(m_NumErrors * 2);
    }
    CHECK_RC(m_LwdaEccL2.Launch(m_LwdaMem.GetDevicePtr(),
                                m_InfoMem.GetDevicePtr(),
                                l2SliceSizeDwords,
                                pSubdev->GetFB()->GetL2EccSectorSize() / sizeof(UINT32),
                                m_Iterations,
                                GetTestConfiguration()->Seed()));

    // Wait until all SMs are ready
    while (m_KernelInterfaceCommands->at(CmdIndexes::SM_READY_IDX) != FetchMsg());
    SendMsg(m_KernelInterfaceCommands->at(CmdIndexes::HOST_PROCEED_IDX));

    UINT64 inner = 0;
    bool done = false;
    while (!done)
    {
        UINT32 msg = FetchMsg();
        
        if (msg == m_KernelInterfaceCommands->at(CmdIndexes::SM_DISABLE_CHKBITS_IDX))
        {
            pSubdev->SetL2EccCheckbitsEnabled(false);
            SendMsg(m_KernelInterfaceCommands->at(CmdIndexes::HOST_PROCEED_IDX));
        }
        else if (msg == m_KernelInterfaceCommands->at(CmdIndexes::SM_ENABLE_CHKBITS_IDX))
        {
            pSubdev->SetL2EccCheckbitsEnabled(true);
            SendMsg(m_KernelInterfaceCommands->at(CmdIndexes::HOST_PROCEED_IDX));
        }
        else if (msg == m_KernelInterfaceCommands->at(CmdIndexes::SM_RESET_ERRS_IDX))
        {
            if (testInterrupts)
            {
                stickyRc = CheckInterrupts(lockRm, outerLoop, inner++);
            }
            else
            {

                stickyRc = CheckErrCounts(outerLoop, inner++);
            }

            // Test starts to fail if we don't yield often
            Tasker::Yield();

            SendMsg(m_KernelInterfaceCommands->at(CmdIndexes::HOST_PROCEED_IDX));
        }
        else if (msg == m_KernelInterfaceCommands->at(CmdIndexes::SM_DONE_IDX))
        {
            done = true;
        }
    }
    CHECK_RC(GetLwdaInstance().Synchronize());

    CHECK_RC(stopEvent.Record());
    CHECK_RC(stopEvent.Synchronize());
    UINT32 kernelTimeMs = stopEvent - startEvent;
    VerbosePrintf("LwdaEccL2 Kernel runtime (%s): %.2f s\n",
                  testInterrupts ? "Interrupt Test" : "Error Count Test",
                  kernelTimeMs / 1000.0);
    return stickyRc;
}

RC LwdaEccL2::Cleanup()
{
    m_MapFb.UnmapFbRegion();
    m_InfoMem.Free();
    m_LwdaMem.Free();
    m_InfoSurf.Free();
    m_LwdaMemSurf.Free();
    m_Module.Unload();
    return LwdaStreamTest::Cleanup();
}
