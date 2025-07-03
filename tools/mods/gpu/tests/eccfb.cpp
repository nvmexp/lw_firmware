/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/utility/ecccount.h"
#include "gpu/include/gpudev.h"
#include "gpumtest.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/jsonlog.h"
#include "gpu/utility/mapfb.h"
#include "core/include/platform.h"
#include "sim/IChip.h"
#include "gpu/utility/surf2d.h"
#include "core/include/utility.h"
#include <cmath>
#include <deque>
#include "gpu/utility/bglogger.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/utility/modscnsl.h"
#include "core/include/cnslmgr.h"

//--------------------------------------------------------------------
//! \brief ECC FrameBuffer Test
//!
//! Test the ECC (Error Correcting Code) feature in the FB.  The ECC
//! can correct SBEs (Single-Bit Errors) and detect DBEs (Double-Bit
//! Errors).  The ECC checkbits have a granularity of a "sector",
//! which is 32 bytes in GF100.
//!
//! This test uses a variant of Mats.  It divides the FB into boxes,
//! which are 1 sector wide.  Then it fills a random box with random
//! data, disables the ECC checkbits, and toggles 1-2 random bits in
//! each row of the box.  Then it reads the data back, to make sure
//! that the checkbits have corrected the SBE and/or detected the DBE.
//! Finally, it re-enables the checkbits, and writes the original data
//! so that the checkbits are valid.
//!
//! This test can only test a small percentage of the ECC memory in
//! reasonable time, due to several limitations:
//! - Unless we run all of mods in -inst_in_sys mode, instance memory
//!   is stored in FB.  So we can't use DMA while the checkbits are
//!   disabled.  So this test uses slow memory-mapping.
//! - The L2 has to be flushed and/or ilwalidated frequently, to make
//!   sure the data is written/read from FB.  Attempts to use
//!   L2_BYPASS as a speedier alternative have failed so far.  The
//!   main reason for using "boxes" is to minimize the flushing.
//! - The most reliable way of getting the error count is to wait for
//!   the ECC interrupt.  Otherwise, two ECC errors in rapid
//!   succession can be counted as one.  This requires a polling loop
//!   after each ECC error.
//!
class EccFbTest : public GpuMemTest
{
public:
    EccFbTest();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual void PrintJsProperties(Tee::Priority pri);

    SETGET_PROP(RuntimeMs, UINT64);
    SETGET_PROP(IntrWaitTimeMs, UINT32);
    SETGET_PROP(PercentMiscountsAllowed, FLOAT64)
    SETGET_PROP(PercentMiscorrectsAllowed, FLOAT64)
    SETGET_PROP(BoxHeight, UINT32);

private:
    RC GetBoxInfo(UINT64 boxNum, UINT64 *pNumBoxes, UINT64 *pFbOffset);
    RC InjectAndCheckErrorsInOneBox(bool isSbe);
    RC GetErrCounts(UINT64 *pSbeCount, UINT64 *pDbeCount,
                    UINT64 minSbeCount, UINT64 minDbeCount);
    RC Flush();
    RC ErrPrintf(bool isWarning, const char *format, ...);

    struct InjectedError          //!< Describes one error to inject in a box
    {
        bool isSbe;               //!< Tells whether to inject SBEs or DBEs
    };

    UINT64 m_RuntimeMs;         
    UINT32 m_IntrWaitTimeMs;      //!< Hacky wait time at start/end of test
    FLOAT64 m_PercentMiscountsAllowed;//!< Max % loops w/bad HW error-count
    FLOAT64 m_PercentMiscorrectsAllowed;
                                  //!< Max % loops w/bad SBE error-correction
    UINT32 m_MaxMiscounts;        //!< Max num loops w/bad HW error-count
    UINT32 m_MaxMiscorrects;      //!< Max num loops w/SBE not corrected
    Random *m_pRandom;            //!< Random-number generator
    UINT32 m_BoxWidth;            //!< Size of an ECC sector, in bytes
    UINT32 m_BoxHeight;           //!< Height of each box
    UINT32 m_BoxPitch;            //!< Pitch of each box
    UINT64 m_NumBoxes;            //!< Number of boxes in the FB

    vector<InjectedError> m_InjectedErrors;  //!< List of errors to inject
    UINT32 m_LoopNum;             //!< Iterates over m_InjectedErrors
    MapFb m_MapFb;                //!< Used to map the error region
    UINT32 m_NumMiscounts;        //!< Number of loops w/bad HW error-counts
    UINT32 m_NumMiscorrects;      //!< Number of loops w/SBE not corrected
    StickyRC m_ErrorRc;           //!< Used to store error if !StopOnError

    UINT32 m_SavedFbBackdoor;     //!< To restore BACKDOOR_ACCESS in cleanup
    UINT32 m_SavedHostFbBackdoor; //!< To restore BACKDOOR_ACCESS in cleanup
    bool m_RestoreBackdoorInCleanup; //!< m_Saved*Backdoor has valid data

    bool m_UseWriteKillPtrs;      //!< Use writekill pointers on HBM
};

//--------------------------------------------------------------------
//! \brief constructor
//!
EccFbTest::EccFbTest() :
    m_RuntimeMs(0),
    m_IntrWaitTimeMs(500),
    m_PercentMiscountsAllowed(0.05),
    m_PercentMiscorrectsAllowed(0.0),
    m_MaxMiscounts(0),
    m_MaxMiscorrects(0),
    m_pRandom(&GetFpContext()->Rand),
    m_BoxWidth(0),
    m_BoxHeight(8),
    m_BoxPitch(0),
    m_NumBoxes(0),
    m_LoopNum(0),
    m_NumMiscounts(0),
    m_NumMiscorrects(0),
    m_SavedFbBackdoor(0),
    m_SavedHostFbBackdoor(0),
    m_RestoreBackdoorInCleanup(false),
    m_UseWriteKillPtrs(false)
{
    // Disable the RC watchdog on the subdevice for the duration of the test
    SetDisableWatchdog(true);
}

//--------------------------------------------------------------------
//! \brief Test whether FB ECC is supported and enabled
//!
/* virtual */ bool EccFbTest::IsSupported()
{
    if (!GpuMemTest::IsSupported())
    {
        return false;
    }

    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();

    bool   eccSupported  = 0;
    UINT32 eccEnableMask = 0;
    switch (pGpuSubdevice->GetEccEnabled(&eccSupported, &eccEnableMask))
    {
        case RC::OK:
            if (!eccSupported)
            {
                Printf(Tee::PriLow, "ECC is not enabled\n");
                return false;
            }
            break;

        default:
            Printf(Tee::PriLow, "Unable to get ECC enable status");
            return false;
    }

    if (!Ecc::IsUnitEnabled(Ecc::Unit::DRAM, eccEnableMask))
    {
        Printf(Tee::PriLow, "FB ECC not enabled\n");
        return false;
    }

    if (!pGpuSubdevice->GetEccErrCounter()->IsInitialized())
    {
        Printf(Tee::PriLow, "EccErrCounter is not initialized\n");
        return false;
    }

    if (pGpuSubdevice->GetFB()->GetFbEccSectorSize() == 0)
    {
        Printf(Tee::PriLow, "Invalid FB ECC sector size\n");
        return false;
    }

    return true;
}

//--------------------------------------------------------------------
//! \brief Set up the test
//!
//! Note: In addition to the setups done in this function, this test
//! assumes that the EccErrCounter class already waited for the RM to
//! finish scrubbing the ECC, and added an ErrorLogger filter to
//! ignore ECC interrupts.
//!
/* virtual */ RC EccFbTest::Setup()
{
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    RC rc;
    CHECK_RC(GpuMemTest::Setup());
    m_pRandom->SeedRandom(pTestConfig->Seed());

    // Check that ECC checkbits can be modified
    //
    if (!pGpuSubdevice->GetFB()->CanToggleFbEccCheckbits())
    {
        using namespace Utility;

        if (GetSelwrityUnlockLevel() >= SelwrityUnlockLevel::SUL_LWIDIA_NETWORK)
        {
            // More information at https://wiki.lwpu.com/engwiki/index.php/MODS/Information_on_specific_GPU_tests/EccFbTest#Enabling_ECC
            Printf(Tee::PriError, "Setup does not support ECC checkbits override by mods "
                   "client. Please use appropriate hulk license.\n");
            return RC::HULK_LICENSE_REQUIRED;
        }
        else
        {
            return RC::SOFTWARE_ERROR; // Hide HULK req from external clients
        }
    }

    UINT32 displayPitch =
        pTestConfig->SurfaceWidth() * pTestConfig->DisplayDepth() / 8;
    CHECK_RC(AdjustPitchForScanout(&displayPitch));

    // Find the dimensions of the boxes.  (Note: If the screen isn't
    // an even number of sectors wide, then the boxes will look
    // slanted.  This is acceptable; the ECC sector info is more
    // important than making the test look pretty.)
    //
    m_BoxWidth = pGpuSubdevice->GetFB()->GetFbEccSectorSize();
    m_BoxPitch = ALIGN_UP(displayPitch, m_BoxWidth);

    // Allocate the framebuffer and boxes
    //
    CHECK_RC(AllocateEntireFramebuffer(false, 0));
    CHECK_RC(GetBoxInfo(0, &m_NumBoxes, NULL));

    if (m_NumBoxes == 0)
    {
        Printf(Tee::PriError,
               "%s: No FB memory between StartLocation and EndLocation\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    // Create a randomly-sorted vector of errors to inject.  The size
    // of the vector is determined by TestConfiguration.Loops.
    //
    UINT32 loops = pTestConfig->Loops();

    if (loops <= 0)
    {
        Printf(Tee::PriError, "%s.TestConfiguration.Loops must be > 0\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    vector<InjectedError> injectedErrors;
    injectedErrors.reserve(loops);
    while (injectedErrors.size() < loops)
    {
        InjectedError sbeErr = { true };
        InjectedError dbeErr = { false };
        injectedErrors.push_back(sbeErr);
        injectedErrors.push_back(dbeErr);
    }

    vector<UINT32> deck(loops);
    for (UINT32 ii = 0; ii < loops; ++ii)
        deck[ii] = ii;
    m_pRandom->Shuffle(loops, &deck[0]);
    m_InjectedErrors.resize(loops);
    for (UINT32 ii = 0; ii < loops; ++ii)
    {
        m_InjectedErrors[ii] = injectedErrors[deck[ii]];
    }

    // Disable backdoor accesses
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

    m_NumMiscounts = 0;
    m_NumMiscorrects = 0;

    UINT32 numSbes = loops * m_BoxHeight / 2; // Half of injected errs are SBEs
    m_MaxMiscounts = static_cast<UINT32>(ceil(m_PercentMiscountsAllowed *
                                              loops / 100.0));
    m_MaxMiscorrects = static_cast<UINT32>(ceil(m_PercentMiscorrectsAllowed *
                                                numSbes / 100.0));

    if (pGpuSubdevice->GetFB()->GetRamProtocol() == FrameBuffer::RamHBM1 ||
        pGpuSubdevice->GetFB()->GetRamProtocol() == FrameBuffer::RamHBM2)
    {
        m_UseWriteKillPtrs = pGpuSubdevice->Regs().Test32(MODS_PFB_FBPA_ECC_CONTROL_SIDEBAND_EN_ENABLED);
        if (!m_UseWriteKillPtrs)
        {
            Printf(Tee::PriError, "SideBand ecc must be enabled for HBM\n");
            return RC::BAD_MEMORY;
        }
    }
    else
    {
        m_UseWriteKillPtrs = false;
    }
    m_ErrorRc.Clear();

    return rc;
}

//--------------------------------------------------------------------
//! \brief Run the test
//!
/* virtual */ RC EccFbTest::Run()
{
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    EccErrCounter *pEccErrCounter = pGpuSubdevice->GetEccErrCounter();
    GpuUtility::RmMsgFilter rmMsgFilter;
    BgLogger::PauseBgLogger disableBgLogger;
    LwRm::DisableControlTrace disableControlTrace;
    Tee::SetLowAssertLevel lowAssertLevel;
    RC rc;

    CHECK_RC(PrintProgressInit(m_RuntimeMs ? m_RuntimeMs : m_InjectedErrors.size()));

    // Need to suspend the MODS console to prevent ECC miscounts / miscompares (Bug 200673948)
    //
    // The previous method of pausing the console wasn't sufficient
    CHECK_RC(ConsoleManager::Instance()->PrepareForSuspend(GetBoundGpuDevice()));
    DEFER
    {
        ConsoleManager::Instance()->ResumeFromSuspend(GetBoundGpuDevice());
    };

    CHECK_RC(rmMsgFilter.Set(GetBoundRmClient(),
                 "!Lw01Free,!Lw04Control,!rmControl,!gpuControlDevice,"
                 "!Lw04Alloc,!Lw04MapMemory,!Lw04UnmapMemory,"
                 "!fbEccServicePartition,!mmuDumpPDE,!mmuDumpPTE"));

    if (!pGpuSubdevice->IsEccInfoRomReportingDisabled())
    {
        // Running this test with dynamic page-retirement enabled
        // would make the RM fill inforom with phony errors.
        Printf(Tee::PriError,
            "ERROR: Cannot run EccFbTest with ECC Inforom Reporting enabled\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (m_UseWriteKillPtrs)
    {
        Printf(GetVerbosePrintPri(), "%s: Sideband ecc is enabled\n", __FUNCTION__);
    }

    // Flush all writes to FB and ilwalidate L2 cache
    //
    CHECK_RC(Flush());
    CHECK_RC(pGpuSubdevice->IlwalidateL2Cache(GpuSubdevice::L2_ILWALIDATE_CLEAN_FB_FLUSH));

    // Tell the EccErrCounter that this test is about to start
    // injecting errors.  (Note: The RM uses an ISR to update the ECC
    // error counts, so there is a slight delay before the counts
    // update.  Hence the hacky m_IntrWaitTimeMs sleep.)
    //
    Tasker::Sleep(m_IntrWaitTimeMs);
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::FB_CORR));
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::FB_UNCORR));
    Utility::CleanupOnReturn<EccErrCounter, RC>
        ExpectingErrors(pEccErrCounter, &EccErrCounter::EndExpectingAllErrors);

    // Run the test
    //
    UINT64 startTimeMs = Xp::GetWallTimeMS();
    UINT64 elapsedTimeMs = 0;
    UINT32 printPeriod = (UINT32)(m_InjectedErrors.size() + 9) / 10;
    UINT64 lwrStep = 0;
    UINT64 endStep = m_RuntimeMs ? m_RuntimeMs : m_InjectedErrors.size();
    m_LoopNum = 0;

    while (lwrStep < endStep)
    {
        if (m_RuntimeMs)
        {
            elapsedTimeMs = Xp::GetWallTimeMS() - startTimeMs;
            lwrStep = std::min(elapsedTimeMs, m_RuntimeMs);
            m_LoopNum = (m_LoopNum + 1) % m_InjectedErrors.size();  
        } 
        else
        {
            lwrStep = ++m_LoopNum;
        }
        if ((m_LoopNum % printPeriod) == 0)
        {
            Printf(Tee::PriLow, "    Injecting error %llu/%llu (%llu%% done)\n",
                    lwrStep, endStep, (100 * lwrStep / endStep));
            CHECK_RC(PrintProgressUpdate(lwrStep));
        }
        InjectedError *pError = &m_InjectedErrors[m_LoopNum];
        CHECK_RC(InjectAndCheckErrorsInOneBox(pError->isSbe));
        Tasker::Yield();
    }

    // Tell the EccErrCounter that this test is done injecting errors.
    //
    Tasker::Sleep(m_IntrWaitTimeMs);
    ExpectingErrors.Cancel();
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::FB_CORR));
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::FB_UNCORR));

    // Check whether any errors happened with StopOnError disabled
    //
    if (m_ErrorRc != OK)
        return m_ErrorRc;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Clean up the test
//!
/* virtual */ RC EccFbTest::Cleanup()
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

    // Free memory allocated to m_InjectedErrors
    vector<InjectedError> dummy2;
    m_InjectedErrors.swap(dummy2);

    m_MapFb.UnmapFbRegion();
    firstRc = GpuMemTest::Cleanup();
    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Print the test's JavaScript properties
//!
/* virtual */ void EccFbTest::PrintJsProperties(Tee::Priority pri)
{
    GpuMemTest::PrintJsProperties(pri);
    Printf(pri, "EccFbTest Js Properties:\n");
    Printf(pri, "\tRuntimeMs:\t\t%llu\n", m_RuntimeMs);
    Printf(pri, "\tIntrWaitTimeMs:\t\t%d\n", m_IntrWaitTimeMs);
    Printf(pri, "\tPercentMiscountsAllowed:\t\t%g\n",
           m_PercentMiscountsAllowed);
    Printf(pri, "\tPercentMiscorrectsAllowed:\t\t%g\n",
           m_PercentMiscorrectsAllowed);
    Printf(pri, "\tBoxHeight:\t\t%d\n", m_BoxHeight);
}

//--------------------------------------------------------------------
//! \brief Get info about a box
//!
//! \param BoxNum A box number from 0 to numBoxes-1.
//! \param pNumBoxes If non-null, holds the total number of boxes on exit.
//! \param pFbOffset If non-null, holds the framebuffer offset of the
//!     box indicated by the BoxNum parameter.  This function returns
//!     an error if pFbOffset is non-null and BoxNum is out of range.
//!
RC EccFbTest::GetBoxInfo(UINT64 boxNum, UINT64 *pNumBoxes, UINT64 *pFbOffset)
{
    GpuUtility::MemoryChunks *pChunks = GetChunks();
    UINT32 boxesPerRow = m_BoxPitch / m_BoxWidth;
    UINT32 bytesPerRow = m_BoxPitch * m_BoxHeight;
    UINT64 firstBoxInChunk = 0;

    for (GpuUtility::MemoryChunks::iterator iter = pChunks->begin();
         iter != pChunks->end(); ++iter)
    {
        UINT64 startFbOffset = max(GetStartLocation() * 1_MB,
                                   iter->fbOffset);
        UINT64 endFbOffset = min(GetEndLocation() * 1_MB,
                                 iter->fbOffset + iter->size);
        startFbOffset = ALIGN_UP(startFbOffset, m_BoxWidth);
        endFbOffset = ALIGN_DOWN(endFbOffset, m_BoxWidth);

        if (endFbOffset > startFbOffset)
        {
            UINT64 numRows = (endFbOffset - startFbOffset) / bytesPerRow;
            UINT64 numBoxesInChunk = numRows * boxesPerRow;
            if ((boxNum >= firstBoxInChunk) &&
                (boxNum < firstBoxInChunk + numBoxesInChunk) &&
                (pFbOffset))
            {
                UINT64 row = (boxNum - firstBoxInChunk) / boxesPerRow;
                UINT64 col = (boxNum - firstBoxInChunk) % boxesPerRow;
                *pFbOffset = (startFbOffset +
                              row * bytesPerRow +
                              col * m_BoxWidth);
                if (!pNumBoxes)
                    return OK;
            }
            firstBoxInChunk += numBoxesInChunk;
        }
    }
    if (pFbOffset)
    {
        Printf(Tee::PriError, "Invalid box num passed to GetBoxFbOffset()\n");
        return RC::BAD_PARAMETER;
    }

    if (pNumBoxes)
        *pNumBoxes = firstBoxInChunk;
    return OK;
}

//--------------------------------------------------------------------
//! Inject a random SBE or DBE error into each row of a random box,
//! and check the results.
//!
RC EccFbTest::InjectAndCheckErrorsInOneBox(bool isSbe)
{
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    FrameBuffer  *pFb           = pGpuSubdevice->GetFB();
    const UINT32  eccSectorSize = pFb->GetFbEccSectorSize();
    RC rc;

    // Randomly choose a box to insert errors into
    //
    UINT64 boxNum = m_pRandom->GetRandom64(0, m_NumBoxes - 1);
    UINT64 boxFbOffset;
    CHECK_RC(GetBoxInfo(boxNum, NULL, &boxFbOffset));

    // Randomly choose an injected error (i.e. which bits to toggle
    // while the checkbits are disabled) for a random 32-bit word in
    // each row.
    //
    vector<UINT08*> pRowData(m_BoxHeight);
    vector<UINT64> rowFbOffset(m_BoxHeight);
    vector<UINT32> originalData(m_BoxHeight);
    vector<UINT32> toggleBits(m_BoxHeight);
    UINT08 *pBoxData = (UINT08*)m_MapFb.MapFbRegion(
                    GetChunks(), boxFbOffset,
                    m_BoxPitch * (m_BoxHeight-1) + m_BoxWidth, pGpuSubdevice);

    // Fill the box with random data
    //
    vector<UINT08> randomData(m_BoxWidth);
    for (UINT32 ii = 0; ii < m_BoxHeight; ++ii)
    {
        for (UINT32 jj = 0; jj < m_BoxWidth; ++jj)
            randomData[jj] = m_pRandom->GetRandom(0, 0xff);
        Platform::MemCopy(pBoxData + ii * m_BoxPitch, &randomData[0],
                          m_BoxWidth);
    }

    for (UINT32 ii = 0; ii < m_BoxHeight; ++ii)
    {
        pRowData[ii] = pBoxData +
            (ii * m_BoxPitch) +
            (sizeof(UINT32) *
             m_pRandom->GetRandom(0, m_BoxWidth/sizeof(UINT32) - 1));
        rowFbOffset[ii] = boxFbOffset + (pRowData[ii] - pBoxData);
        originalData[ii] = MEM_RD32(pRowData[ii]);
        if (isSbe)
        {
            toggleBits[ii] = 1 << m_pRandom->GetRandom(0, 31);
        }
        else
        {
            const UINT08 bitSeparation = pGpuSubdevice->GetFbEclwncorrInjectBitSeparation();
            UINT32 bit1 = m_pRandom->GetRandom(0, 31);
            UINT32 bit2 = (ALIGN_DOWN(m_pRandom->GetRandom(bitSeparation, 31), bitSeparation) + bit1) % 32;
            toggleBits[ii] = (1 << bit1) | (1 << bit2);
        }
    }

    // Record the number of SBE and DBE errors before injecting the
    // error.
    //
    CHECK_RC(Flush());
    UINT64 oldSbeCount;
    UINT64 oldDbeCount;
    CHECK_RC(GetErrCounts(&oldSbeCount, &oldDbeCount, 0, 0));

    // Here's the heart of the test:
    // (1) Disable the checkbits
    // (2) Write an injected error into one word in each row of the FB
    //     box.
    // (3) Re-enable the checkbits.
    // (4) Read the words back out of the box.  Make sure they
    //     contains the expected data, and that the ECC error counts
    //     are updated correctly.
    // (5) Write the original words back into the box so that the
    //     checkbits are valid again.
    //
    {
        unique_ptr<FrameBuffer::CheckbitsHolder> checkbitsHolder(
                pFb->SaveFbEccCheckbits());

        for (UINT32 ii = 0; ii < m_BoxHeight; ++ii)
        {
            const GpuUtility::MemoryChunkDesc *pChunk =
                GetChunk(rowFbOffset[ii]);
            MASSERT(pChunk);
            UINT64 alignedFbOffset = ALIGN_DOWN(rowFbOffset[ii], eccSectorSize);
            if (m_UseWriteKillPtrs)
            {
                INT32 kptr0 = Utility::BitScanForward(toggleBits[ii]);
                INT32 kptr1 = Utility::BitScanForward(toggleBits[ii], kptr0+1);
                CHECK_RC(pFb->ApplyFbEccWriteKillPtr(
                                alignedFbOffset, pChunk->pteKind,
                                pChunk->pageSizeKB, kptr0, kptr1));
                MEM_WR32(pRowData[ii], originalData[ii]); // Rewrite same data
            }
            else
            {
                CHECK_RC(pFb->DisableFbEccCheckbitsAtAddress(
                                alignedFbOffset, pChunk->pteKind,
                                pChunk->pageSizeKB));
                MEM_WR32(pRowData[ii], originalData[ii] ^ toggleBits[ii]);
            }
            CHECK_RC(Flush());
        }
        CHECK_RC(pGpuSubdevice->IlwalidateL2Cache(GpuSubdevice::L2_ILWALIDATE_CLEAN_FB_FLUSH));
    }
    for (UINT32 ii = 0; ii < m_BoxHeight; ++ii)
    {
        UINT32 newData = MEM_RD32(pRowData[ii]);

        if (isSbe)
        {
            if (newData != originalData[ii])
            {
                ++m_NumMiscorrects;
                CHECK_RC(ErrPrintf(
                             m_NumMiscorrects <= m_MaxMiscorrects,
                             "Bad data after FB SBE injection:"
                             " data = 0x%08x, expected = 0x%08x,"
                             " offset = 0x%llx, toggled = 0x%08x\n",
                             newData, originalData[ii], rowFbOffset[ii],
                             toggleBits[ii]));
            }
        }
        MEM_WR32(pRowData[ii], originalData[ii]);
    }

    CHECK_RC(Flush());

    // Check ECC counts
    UINT64 newSbeCount = 0;
    UINT64 newDbeCount = 0;
    if (isSbe)
    {
        CHECK_RC(GetErrCounts(&newSbeCount, &newDbeCount,
                    oldSbeCount + m_BoxHeight, oldDbeCount));
        if (newSbeCount != oldSbeCount + m_BoxHeight)
        {
            ++m_NumMiscounts;
            CHECK_RC(ErrPrintf(
                         m_NumMiscounts <= m_MaxMiscounts,
                         "Bad FB SBE error count:"
                         " count = %llu, expected %llu\n",
                         newSbeCount, oldSbeCount + m_BoxHeight));
        }
        if (newDbeCount != oldDbeCount)
        {
            ++m_NumMiscounts;
            CHECK_RC(ErrPrintf(
                         m_NumMiscounts <= m_MaxMiscounts,
                         "Bad FB DBE error count (expected only SBE):"
                         " count = %llu, expected %llu\n",
                         newDbeCount, oldDbeCount));
        }
    }
    else
    {
        CHECK_RC(GetErrCounts(&newSbeCount, &newDbeCount,
                    oldSbeCount, oldDbeCount + m_BoxHeight));
        if (newDbeCount != oldDbeCount + m_BoxHeight)
        {
            ++m_NumMiscounts;
            CHECK_RC(ErrPrintf(
                         m_NumMiscounts <= m_MaxMiscounts,
                         "Bad FB DBE error count:"
                         " count = %llu, expected %llu\n",
                         newDbeCount, oldDbeCount + m_BoxHeight));
        }
    }
    Printf(GetVerbosePrintPri(), "%s: New Sbe [%llu], Old Sbe [%llu], New Dbe [%llu], Old Dbe [%llu]\n",
            __FUNCTION__, newSbeCount, oldSbeCount, newDbeCount, oldDbeCount);

    return rc;
}

// Parameters passed to PollForErrCounts()
//
struct PollErrCountsParams
{
    GpuSubdevice *pGpuSubdevice;
    UINT64 minSbeCount;
    UINT64 minDbeCount;
    UINT64 sbeCount;
    UINT64 dbeCount;
};

// Polling function used by EccFbTest::GetErrCounts()
//
static bool PollForErrCounts(void *pPollForErrCountsParams)
{
    PollErrCountsParams *pParams =
        (PollErrCountsParams*)pPollForErrCountsParams;
    GpuSubdevice *pGpuSubdevice = pParams->pGpuSubdevice;
    Tasker::MutexHolder lock(pGpuSubdevice->GetEccErrCounter()->GetMutex());
    RC rc;

    Ecc::DetailedErrCounts fbParams;
    rc = pGpuSubdevice->GetSubdeviceFb()->GetDramDetailedEccCounts(&fbParams);
    if (rc != OK)
    {
        return true;
    }

    pParams->sbeCount = 0;
    pParams->dbeCount = 0;
    for (UINT32 part = 0; part < fbParams.eccCounts.size(); ++part)
    {
        for (UINT32 subPart = 0;
             subPart < fbParams.eccCounts[part].size(); ++subPart)
        {
            pParams->sbeCount += fbParams.eccCounts[part][subPart].corr;
            pParams->dbeCount += fbParams.eccCounts[part][subPart].uncorr;
        }
    }
    bool bReturn = (pParams->sbeCount >= pParams->minSbeCount &&
                    pParams->dbeCount >= pParams->minDbeCount);

    // To avoid hammering the RM mutex and possibly preventing RM from
    // running and actually detecting the errors that were just injected,
    // pause for a very short time here (time determined empirically by
    // running conlwrrently on 4 GPUs)
    if (!bReturn)
        Utility::SleepUS(10);
    return bReturn;
}

//--------------------------------------------------------------------
//! \brief Retrieve the FB ECC error counts
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
//! NOTE: assuming the FB has an ECC unit (uncorr=DBE), not parity (uncorr=SBE)
//!
RC EccFbTest::GetErrCounts
(
    UINT64 *pSbeCount,
    UINT64 *pDbeCount,
    UINT64 minSbeCount,
    UINT64 minDbeCount
)
{
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    RC rc;

    PollErrCountsParams params = {0};
    params.pGpuSubdevice = GetBoundGpuSubdevice();
    params.minSbeCount = minSbeCount;
    params.minDbeCount = minDbeCount;
    POLLWRAP_HW(PollForErrCounts, &params, pTestConfig->TimeoutMs());
    *pSbeCount = params.sbeCount;
    *pDbeCount = params.dbeCount;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Flush the GPU L2 cache and write combine buffer
//!
RC EccFbTest::Flush()
{
    // Flush WC buffers on the CPU
    RC rc;
    CHECK_RC(Platform::FlushCpuWriteCombineBuffer());

    // Force all BAR1 writes to L2 and to FB to complete
    LW0080_CTRL_DMA_FLUSH_PARAMS params = {0};
    params.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                _L2, _ENABLE)
                      | DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                _FB, _ENABLE);
    CHECK_RC(GetBoundRmClient()->ControlByDevice(GetBoundGpuDevice(),
                                               LW0080_CTRL_CMD_DMA_FLUSH,
                                               &params, sizeof(params)));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Print and/or record an ECC memory error
//!
//! This method should be called when an ECC error oclwrs.  It prints
//! the error message, determines the print priority, decides whether
//! to stop the test immediately or keep running, records the error
//! code if we're not stopping immediately, and records the error in
//! JSON.
//!
//! When determining how to handle the error, there's basically four
//! different cases: StopOnError (the opposite of -run_on_error) can
//! be true or false, and the isWarning arg can be true or false.
//!
//! \param isWarning If true, this message is treated as a mere
//!     warning, generally because we haven't exceeded an
//!     allowed-error threshold.
//! \param Format A printf() format string containing the error
//!     message.  The remaining arguments are identical to printf().
//! \return RC::BAD_MEMORY if we should abort the test right now, or
//!     OK if we should keep running.
//!
RC EccFbTest::ErrPrintf(bool isWarning, const char *format, ...)
{
    bool stopOnError = GetGoldelwalues()->GetStopOnError();

    // Decide the print priority.  Print everything at PriNormal for
    // -run_on_error, otherwise suppress error messages that fall
    // below the allowed-error threshold.
    //
    INT32 priority;
    if (!stopOnError)
    {
        priority = Tee::PriNormal;
    }
    else if (isWarning)
    {
        priority = Tee::PriLow;
    }
    else
    {
        priority = Tee::PriError;
    }

    // Print the error message
    //
    va_list args;
    va_start(args, format);
    ModsExterlwAPrintf(priority, Tee::GetTeeModuleCoreCode(), Tee::SPS_NORMAL,
                       format, args);
    va_end(args);

    // Write the JSON record
    //
    if (!isWarning)
        GetJsonExit()->AddFailLoop(m_LoopNum);

    // Record or return the error code
    //
    if (isWarning)
    {
        return OK;
    }
    else if (stopOnError)
    {
        return RC::BAD_MEMORY;
    }
    m_ErrorRc = RC::BAD_MEMORY;  // Record the error code so we fail later
    return OK;
}

//--------------------------------------------------------------------
// JavaScript interface
//
JS_CLASS_INHERIT(EccFbTest, GpuMemTest, "EccFbTest object");
CLASS_PROP_READWRITE(EccFbTest, RuntimeMs, UINT64, 
    "Run the loop for at least the specified amount of time if RuntimeMs != 0.");
CLASS_PROP_READWRITE(EccFbTest, IntrWaitTimeMs, UINT32,
    "Time to wait when test starts/ends for pending ECC interrupts");
CLASS_PROP_READWRITE(EccFbTest, PercentMiscountsAllowed, FLOAT64,
    "Percent of injected ECC errors in which HW error-counter can be off.");
CLASS_PROP_READWRITE(EccFbTest, PercentMiscorrectsAllowed, FLOAT64,
    "Percent of injected ECC errors in which SBE error-correction can fail.");
CLASS_PROP_READWRITE(EccFbTest, BoxHeight, UINT32,
        "Height of a memory box in which to insert ECC errors, one per row.");
