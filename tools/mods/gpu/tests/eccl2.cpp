/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/utility/ecccount.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gputest.h"
#include "core/include/jsonlog.h"
#include "core/include/platform.h"
#include "sim/IChip.h"
#include "gpu/utility/surf2d.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "gpu/utility/bglogger.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/utility/modscnsl.h"
#include "cheetah/include/teggpurgown.h"

//--------------------------------------------------------------------
//! \brief ECC L2 Test
//!
//! Test the ECC (Error Correcting Code) feature in the L2 cache.  The
//! ECC can correct SBEs (Single-Bit Errors) and detect DBEs
//! (Double-Bit Errors).  The ECC checkbits have a granularity of a
//! "sector", which is 32 bytes in GF100.
//!
//! We don't have direct access to the L2 cache, so this test
//! allocates an FB surface as big as the L2, on the theory
//! that reading/writing the entire surface should hit all the L2
//! cache lines.
//!
//! This test chooses N random sectors in each surface (where N
//! depends on coverage).  For each sector, it disables the ECC
//! checkbits, toggles 1-2 random bits in the sector, and then reads
//! the data back to make sure that an SBE is corrected and a DBE is
//! detected.  It only tests one sector at a time, to make sure that
//! the line containing sector doesn't get flushed.
//!
class EccL2Test : public GpuTest
{
public:
    EccL2Test();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual void PrintJsProperties(Tee::Priority Pri);

    SETGET_PROP(IntrWaitTimeMs, UINT32);

private:
    RC InjectAndCheckOneError(Surface2D *pSurface, bool IsSbe);
    RC Flush();
    RC GetErrCounts(UINT64 *pSbeCount, UINT64 *pDbeCount,
                    UINT64 MinSbeCount, UINT64 MinDbeCount);
    RC ErrPrintf(const char *Format, ...);

    struct injectedError          //!< Describes one error to inject
    {
        Surface2D *pSurface;      //!< Which surface to inject the error into
        bool IsSbe;               //!< Tells whether to inject an SBE or DBE
    };

    UINT32 m_IntrWaitTimeMs;      //!< Hacky wait time at start/end of test
    Random *m_pRandom;            //!< Random number generator
    UINT32 m_SectorSize;          //!< Size of an ECC sector, in bytes
    UINT32 m_SurfaceSize;         //!< Size of the test surfaces, in bytes

    vector<injectedError> m_InjectedErrors;  //!< List of errors to inject
    UINT32 m_LoopNum;             //!< Iterates over m_InjectedErrors
    Surface2D m_FbSurface;        //!< Framebuffer surface used to test L2
    StickyRC m_ErrorRc;           //!< Used to store error if !StopOnError

    UINT32 m_SavedFbBackdoor;     //!< To restore BACKDOOR_ACCESS in cleanup
    UINT32 m_SavedHostFbBackdoor; //!< To restore BACKDOOR_ACCESS in cleanup
    bool m_RestoreBackdoorInCleanup; //!< m_Saved*Backdoor has valid data
};

//--------------------------------------------------------------------
//! \brief constructor
//!
EccL2Test::EccL2Test() :
    m_IntrWaitTimeMs(500),
    m_pRandom(&GetFpContext()->Rand),
    m_SectorSize(0),
    m_SurfaceSize(0),
    m_LoopNum(0),
    m_SavedFbBackdoor(0),
    m_SavedHostFbBackdoor(0),
    m_RestoreBackdoorInCleanup(false)
{
    // Disable the RC watchdog on the subdevice for the duration of the test
    SetDisableWatchdog(true);
}

//--------------------------------------------------------------------
//! \brief Test whether L2 ECC is supported and enabled
//!
/* virtual */ bool EccL2Test::IsSupported()
{
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    // Only supported on Maxwell
    if (GetBoundGpuDevice()->GetFamily() != GpuDevice::Maxwell)
    {
        Printf(Tee::PriLow, "EccL2 Test supported on Maxwell only\n");
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

    return pSubdev->GetEccErrCounter()->IsInitialized() &&
           pSubdev->GetFB()->GetL2CacheSize() > 0       &&
           pSubdev->GetFB()->GetL2EccSectorSize() > 0;
}

//--------------------------------------------------------------------
//! \brief Set up the test
//!
//! Note: In addition to the setups done in this function, this test
//! assumes that the EccErrCounter class already waited for the RM to
//! finish scrubbing the ECC, and added an ErrorLogger filter to
//! ignore ECC interrupts.
//!
/* virtual */ RC EccL2Test::Setup()
{
    LwRm *pLwRm = GetBoundRmClient();
    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    RC rc;

    CHECK_RC(GpuTest::Setup());
    m_pRandom->SeedRandom(pTestConfig->Seed());

    // Callwlate basic parameters
    //
    m_SectorSize = pGpuSubdevice->GetFB()->GetL2EccSectorSize();
    MASSERT(m_SectorSize > 0);

    m_SurfaceSize = pGpuSubdevice->GetFB()->GetL2CacheSize();
    m_SurfaceSize = ALIGN_UP(m_SurfaceSize, m_SectorSize);
    MASSERT(m_SurfaceSize > 0);

    // Create a randomly-sorted vector of errors to inject.  The size
    // of the vector is determined by TestConfiguration.Loops.
    //
    UINT32 Loops = pTestConfig->Loops();

    if (Loops <= 0)
    {
        Printf(Tee::PriHigh, "%s.TestConfiguration.Loops must be > 0\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    vector<injectedError> injectedErrors;
    injectedErrors.reserve(Loops);
    while (injectedErrors.size() < Loops)
    {
        injectedError FbSbeErr  = { &m_FbSurface,  true };
        injectedErrors.push_back(FbSbeErr);
        injectedError FbDbeErr  = { &m_FbSurface,  false };
        injectedErrors.push_back(FbDbeErr);
    }

    vector<UINT32> Deck(Loops);
    for (UINT32 ii = 0; ii < Loops; ++ii)
        Deck[ii] = ii;
    m_pRandom->Shuffle(Loops, &Deck[0]);
    m_InjectedErrors.resize(Loops);
    for (UINT32 ii = 0; ii < Loops; ++ii)
        m_InjectedErrors[ii] = injectedErrors[Deck[ii]];

    // Allocate the surfaces.
    //
    m_FbSurface.SetWidth(m_SurfaceSize / sizeof(UINT32));
    m_FbSurface.SetHeight(1);
    m_FbSurface.SetColorFormat(ColorUtils::VOID32);
    m_FbSurface.SetAlignment(m_SectorSize);
    m_FbSurface.SetLocation(Memory::Fb);
    CHECK_RC(m_FbSurface.Alloc(pGpuDevice, pLwRm));
    CHECK_RC(m_FbSurface.Map(pGpuSubdevice->GetSubdeviceInst()));

    for (UINT32 y = 0; y < m_FbSurface.GetHeight(); y++)
    {
        for (UINT32 x = 0; x < m_FbSurface.GetWidth(); x++)
        {
            m_FbSurface.WritePixel(x, y, m_pRandom->GetRandom(0, 0xffffffff));
        }
    }

    // Disable backdoor accesses so the fault injections are visible to L2
    //
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        Platform::EscapeRead("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4,
                             &m_SavedFbBackdoor);
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
        Platform::EscapeRead("BACKDOOR_ACCESS", IChip::EBACKDOOR_HOSTFB, 4,
                             &m_SavedHostFbBackdoor);
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_HOSTFB, 4, 0);
        m_RestoreBackdoorInCleanup = true;
    }

    m_ErrorRc.Clear();

    return rc;
}

//--------------------------------------------------------------------
//! \brief Run the test
//!
/* virtual */ RC EccL2Test::Run()
{
    LwRm *pLwRm = GetBoundRmClient();
    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    EccErrCounter *pEccErrCounter = pGpuSubdevice->GetEccErrCounter();
    LW0080_CTRL_DMA_FLUSH_PARAMS FlushParams = {0};
    GpuUtility::RmMsgFilter rmMsgFilter;
    BgLogger::PauseBgLogger DisableBgLogger;
    LwRm::DisableControlTrace DisableControlTrace;
    Tee::SetLowAssertLevel lowAssertLevel;
    RC rc;

    // Disable GPU railgating on CheetAh
    TegraGpuRailGateOwner gpuRg;
    CHECK_RC(gpuRg.SetDelayMs(3600000U));

    if (!pGpuSubdevice->IsSOC())
    {
        CHECK_RC(rmMsgFilter.Set(GetBoundRmClient(),
                 "!Lw01Free,!Lw04Control,!rmControl,!gpuControlDevice,"
                 "!Lw04Alloc,!Lw04MapMemory,!Lw04UnmapMemory,"
                 "!fbEccServiceLtcSlice"));
    }

    // Flush the L2 cache
    //
    FlushParams.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                     _L2, _ENABLE)
                           | DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                     _FB, _ENABLE);
    CHECK_RC(pLwRm->ControlByDevice(pGpuDevice, LW0080_CTRL_CMD_DMA_FLUSH,
                                    &FlushParams, sizeof(FlushParams)));

    // Tell the EccErrCounter that this test is about to start
    // injecting errors.  (Note: The RM uses an ISR to update the ECC
    // error counts, so there is a slight delay before the counts
    // update.  Hence the hacky m_IntrWaitTimeMs sleep, which should
    // ensure that any errors that oclwrred before this test are
    // cleared.)
    //
    Tasker::Sleep(m_IntrWaitTimeMs);
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::L2_CORR));
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::L2_UNCORR));
    Utility::CleanupOnReturn<EccErrCounter, RC>
        ExpectingErrors(pEccErrCounter, &EccErrCounter::EndExpectingAllErrors);

    // Run the test
    //
    UINT32 PrintPeriod = (UINT32)((m_InjectedErrors.size() + 9)/ 10);

    for (m_LoopNum = 0; m_LoopNum < m_InjectedErrors.size(); ++m_LoopNum)
    {
        if ((m_LoopNum % PrintPeriod) == 0)
        {
            Printf(Tee::PriLow, "    Injecting error %d/%d (%d%% done)\n",
                   m_LoopNum + 1, int(m_InjectedErrors.size()),
                   int(100 * m_LoopNum / m_InjectedErrors.size()));
        }
        injectedError *pError = &m_InjectedErrors[m_LoopNum];
        CHECK_RC(InjectAndCheckOneError(pError->pSurface, pError->IsSbe));
        Tasker::Yield();
    }

    // Flush the L2 cache
    //
    FlushParams.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                     _L2, _ENABLE)
                           | DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                     _FB, _ENABLE);
    CHECK_RC(pLwRm->ControlByDevice(pGpuDevice, LW0080_CTRL_CMD_DMA_FLUSH,
                                    &FlushParams, sizeof(FlushParams)));

    // Tell the EccErrCounter that this test is done injecting errors.
    //
    Tasker::Sleep(m_IntrWaitTimeMs);
    ExpectingErrors.Cancel();
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::L2_CORR));
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::L2_UNCORR));

    // Check whether any errors happened with StopOnError disabled
    //
    if (m_ErrorRc != OK)
        return m_ErrorRc;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Clean up the test
//!
/* virtual */ RC EccL2Test::Cleanup()
{
    StickyRC firstRc;

    if (m_RestoreBackdoorInCleanup)
    {
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4,
                              m_SavedFbBackdoor);
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_HOSTFB, 4,
                              m_SavedHostFbBackdoor);
        m_RestoreBackdoorInCleanup = false;
    }

    m_FbSurface.Free();
    firstRc = GpuTest::Cleanup();
    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Print the test's JavaScript properties
//!
/* virtual */ void EccL2Test::PrintJsProperties(Tee::Priority Pri)
{
    GpuTest::PrintJsProperties(Pri);
    Printf(Pri, "EccL2Test Js Properties:\n");
    Printf(Pri, "\tIntrWaitTimeMs:\t\t%d\n", m_IntrWaitTimeMs);
}

//--------------------------------------------------------------------
//! \brief Make sure that everything was written to GPU's L2.
//!
RC EccL2Test::Flush()
{
    // Flush WC buffers on the CPU
    RC rc;
    CHECK_RC(Platform::FlushCpuWriteCombineBuffer());

    // Force all BAR1 writes to L2 to complete
    LW0080_CTRL_DMA_FLUSH_PARAMS FlushParams = {0};
    FlushParams.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                     _L2, _ENABLE);
    CHECK_RC(LwRmPtr()->ControlByDevice(GetBoundGpuDevice(), LW0080_CTRL_CMD_DMA_FLUSH,
                                    &FlushParams, sizeof(FlushParams)));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Inject one random SBE or DBE error, and check the results.
//!
RC EccL2Test::InjectAndCheckOneError(Surface2D *pSurface, bool IsSbe)
{
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    RC rc;

    // Select a random 32-bit word in the surface
    //
    UINT32 SurfaceOffset = (UINT32)sizeof(UINT32) *
        m_pRandom->GetRandom(0, m_SurfaceSize/sizeof(UINT32) - 1);
    UINT32 SectorOffset = SurfaceOffset % m_SectorSize;
    UINT32 *pSurfaceData =
        (UINT32*)((UINT08*)m_FbSurface.GetAddress() + SurfaceOffset);

    // Randomly select the bit(s) to toggle while the checkbits are
    // disabled.
    //
    UINT32 ToggleBits;

    if (IsSbe)
    {
        ToggleBits = 1 << m_pRandom->GetRandom(0, 31);
    }
    else
    {
        UINT32 Bit1 = m_pRandom->GetRandom(0, 31);
        UINT32 Bit2 = (m_pRandom->GetRandom(1, 31) + Bit1) % 32;
        ToggleBits = (1 << Bit1) | (1 << Bit2);
    }

    // Record the number of SBE and DBE errors before injecting the
    // error.
    //
    UINT64 OldSbeCount;
    UINT64 OldDbeCount;
    CHECK_RC(GetErrCounts(&OldSbeCount, &OldDbeCount, 0, 0));

    // Here's the heart of the test:
    // (1) Read the selected word from the surface.  This should load
    //     it into an L2 cache line.
    // (2) Disable the checkbits
    // (3) Write the word with the injected error
    // (4) Read the word back, and make sure it contains the expected data.
    // (5) Restore the checkbits, and write the original data back
    //     into the word so that the checkbits are valid again.
    //
    CHECK_RC(Flush());
    UINT32 OriginalData = MEM_RD32(pSurfaceData);
    UINT32 TestData;
    {
        LwRm::ExclusiveLockRm lockRm(GetBoundRmClient());
        ModsConsole::PauseConsoleUpdates pauseModsConsole;
        CHECK_RC(lockRm.AcquireLock());
        CHECK_RC(pauseModsConsole.Pause());

        bool OldCheckbitsValue = pGpuSubdevice->GetL2EccCheckbitsEnabled();
        CHECK_RC(pGpuSubdevice->SetL2EccCheckbitsEnabled(false));

        MEM_WR32(pSurfaceData, OriginalData ^ ToggleBits);
        CHECK_RC(Flush());
        TestData = MEM_RD32(pSurfaceData);

        CHECK_RC(pGpuSubdevice->SetL2EccCheckbitsEnabled(OldCheckbitsValue));
    }
    MEM_WR32(pSurfaceData, OriginalData);
    CHECK_RC(Flush());
    // to remove clang error
    volatile UINT32 dummy = MEM_RD32(pSurfaceData);
    (void)dummy ;

    if (IsSbe)
    {
        if (TestData != OriginalData)
        {
            CHECK_RC(ErrPrintf(
                         "Bad data after L2 SBE injection:"
                         " data = 0x%08x, expected = 0x%08x,"
                         " sector offset = 0x%x, toggled = 0x%08x\n",
                         TestData, OriginalData, SectorOffset, ToggleBits));
        }
    }

    // Make sure the ECC error counts have been updated correctly.
    //
    UINT64 NewSbeCount;
    UINT64 NewDbeCount;

    if (IsSbe)
    {
        CHECK_RC(GetErrCounts(&NewSbeCount, &NewDbeCount,
                              OldSbeCount + 1, OldDbeCount));
        if (NewSbeCount <= OldSbeCount)
        {
            CHECK_RC(ErrPrintf(
                         "Bad L2 SBE error count: count = %lld,"
                         " expected >= %lld, sector offset = 0x%x\n",
                         NewSbeCount, OldSbeCount + 1, SectorOffset));
        }
        if (NewDbeCount != OldDbeCount)
        {
            CHECK_RC(ErrPrintf(
                         "Bad L2 DBE error count: count = %lld,"
                         " expected = %lld, sector offset = 0x%x\n",
                         NewDbeCount, OldDbeCount, SectorOffset));
        }
    }
    else
    {
        CHECK_RC(GetErrCounts(&NewSbeCount, &NewDbeCount,
                              OldSbeCount, OldDbeCount + 1));
        if (NewDbeCount <= OldDbeCount)
        {
            CHECK_RC(ErrPrintf(
                         "Bad L2 DBE error count: count = %lld,"
                         " expected >= %lld, sector offset = 0x%x\n",
                         NewDbeCount, OldDbeCount + 1, SectorOffset));
        }
    }

    return rc;
}

// Args passed to PollErrCounts()
//
struct PollErrCountsParams
{
    GpuSubdevice *pGpuSubdevice;
    UINT64 MinSbeCount;
    UINT64 MinDbeCount;
    UINT64 SbeCount;
    UINT64 DbeCount;
};

// Polling function used by EccL2Test::GetErrCounts()
//
static bool PollForErrCounts(void *pPollForErrCountsParams)
{
    PollErrCountsParams *pParams =
        (PollErrCountsParams*)pPollForErrCountsParams;
    GpuSubdevice *pGpuSubdevice = pParams->pGpuSubdevice;
    Tasker::MutexHolder Lock(pGpuSubdevice->GetEccErrCounter()->GetMutex());
    RC rc;

    Ecc::DetailedErrCounts L2Params;
    rc = pGpuSubdevice->GetSubdeviceFb()->GetLtcDetailedEccCounts(&L2Params);
    if (rc != OK)
    {
        return true;
    }

    pParams->SbeCount = 0;
    pParams->DbeCount = 0;
    for (UINT32 part = 0; part < L2Params.eccCounts.size(); ++part)
    {
        for (UINT32 slice = 0;
             slice < L2Params.eccCounts[part].size(); ++slice)
        {
            pParams->SbeCount += L2Params.eccCounts[part][slice].corr;
            pParams->DbeCount += L2Params.eccCounts[part][slice].uncorr;
        }
    }
    bool   bReturn = (pParams->SbeCount >= pParams->MinSbeCount &&
                      pParams->DbeCount >= pParams->MinDbeCount);

    // To avoid hammering the RM mutex and possibly preventing RM from
    // running and actually detecting the errors that were just injected,
    // pause for a very short time here (time determined empirically by
    // running conlwrrently on 4 GPUs)
    if (!bReturn)
        Utility::SleepUS(10);
    return bReturn;
}

//--------------------------------------------------------------------
//! \brief Retrieve the L2 ECC error counts
//!
//! \param pSbeCount On exit, holds the SBE error count
//! \param pDbeCount On exit, holds the DBE error count
//! \param MinSbeCount The minimum SBE count we expect.  This method
//!     polls until the RM tells us the SBE count has reached this
//!     number, or until we time out.  This is useful because there is
//!     a HW delay between the ECC error oclwrring, and the error
//!     propagating everywhere.
//! \param MinDbeCount Like MinSbeCount, but for DBEs.
//!
RC EccL2Test::GetErrCounts
(
    UINT64 *pSbeCount,
    UINT64 *pDbeCount,
    UINT64 MinSbeCount,
    UINT64 MinDbeCount
)
{
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    RC rc;

    PollErrCountsParams Params = {0};
    Params.pGpuSubdevice = GetBoundGpuSubdevice();
    Params.MinSbeCount = MinSbeCount;
    Params.MinDbeCount = MinDbeCount;
    POLLWRAP_HW(PollForErrCounts, &Params, pTestConfig->TimeoutMs());
    *pSbeCount = Params.SbeCount;
    *pDbeCount = Params.DbeCount;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Print and/or record an error
//!
//! This method takes the same arguments as printf().  The caller
//! normally ilwokes it as CHECK_RC(ErrPrintf(printf_args)).  It works
//! differently depending on the StopOnError flag:
//!
//! - StopOnError is true [default]: Print the message at PriHigh and
//!     return BAD_MEMORY.  So the usual invocation would abort the
//!     test.
//! - StopOnError is false: Print the message at PriNormal, record the
//!     error, and return OK.  So the usual invocation would keep
//!     running, but the test would later return BAD_MEMORY when
//!     we check the recorded error at the end of Run().
//!
//! This method also records the error in JSON.
//!
RC EccL2Test::ErrPrintf(const char *Format, ...)
{
    bool StopOnError = GetGoldelwalues()->GetStopOnError();

    // Print the error message
    //
    INT32 Priority = (StopOnError ? Tee::PriHigh : Tee::PriNormal);
    va_list Args;
    va_start(Args, Format);
    ModsExterlwAPrintf(Priority, Tee::GetTeeModuleCoreCode(), Tee::SPS_NORMAL,
                       Format, Args);
    va_end(Args);

    // Write the JSON record
    //
    GetJsonExit()->AddFailLoop(m_LoopNum);

    // Record or return the error code
    //
    if (StopOnError)
    {
        return RC::BAD_MEMORY;
    }
    m_ErrorRc = RC::BAD_MEMORY;
    return OK;
}

//--------------------------------------------------------------------
// JavaScript interface
//
JS_CLASS_INHERIT(EccL2Test, GpuTest, "EccL2Test object");
CLASS_PROP_READWRITE(EccL2Test, IntrWaitTimeMs, UINT32,
            "Time to wait when test starts/ends for pending ECC interrupts");
