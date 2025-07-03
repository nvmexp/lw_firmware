/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/// @file Evo Display test for use along with stress threads.

#include "gputest.h"
#include "core/include/display.h"
#include "core/include/platform.h"
#include "core/include/jscript.h"
#include "lwos.h"
#include "gpu/utility/surf2d.h"
#include "core/include/utility.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/include/displaycleanup.h"
#include "gpu/include/gpudev.h"
#include "gpu/display/evo_disp.h" // EvoRasterSettings
#include "class/cl507dcrcnotif.h"   // layout of EVO display crc data
#include "gpu/include/displaycleanup.h"
#include "core/include/jsonlog.h"
#include "gpu/include/dmawrap.h"

namespace // anonymous namespace
{
//! Struct to match the layout of one EVO display CRC record in
//! the LW507D notifier.
class CrcEntry
{
private:
    UINT32 m_MetaData;        //!< AUDIT_TIMESTAMP, TAG, TIMESTAMP_MODE, etc.
    UINT32 m_CompositorCrc;
    UINT32 m_PrimaryCrc;
    UINT32 m_SecondaryCrc;

public:
    CrcEntry()
        : m_MetaData(0), m_CompositorCrc(0),
          m_PrimaryCrc(0), m_SecondaryCrc(0)
    {}
    CrcEntry(const CrcEntry & x)
        : m_MetaData(x.m_MetaData), m_CompositorCrc(x.m_CompositorCrc),
          m_PrimaryCrc(x.m_PrimaryCrc), m_SecondaryCrc(x.m_SecondaryCrc)
    {}
    ~CrcEntry()
    {}
    string ToString() const
    {
        return Utility::StrPrintf(
            "comp=%08x,pri=%08x,sec=%08x (tag=%08x)",
            m_CompositorCrc,
            m_PrimaryCrc,
            m_SecondaryCrc,
            m_MetaData);
    }
    bool IsZero() const
    {
        return m_CompositorCrc == 0 && m_PrimaryCrc == 0;
    }
    friend bool operator!=(const CrcEntry& a, const CrcEntry& b);
    friend class RunningCrc;
};
bool operator!=(const CrcEntry& a, const CrcEntry& b)
{
    return (a.m_CompositorCrc != b.m_CompositorCrc ||
        a.m_PrimaryCrc != b.m_PrimaryCrc ||
        a.m_SecondaryCrc != b.m_SecondaryCrc);
}

class RunningCrc
{
private:
    Display * m_pDisplay;
    UINT32 m_DispMask;
    bool m_IsRunning;
    bool m_Overrun;
    UINT64 m_StartUS;
    vector<CrcEntry> m_CopiedCrcs;

public:
    RunningCrc(Display * pDisplay, UINT32 mask)
        : m_pDisplay(pDisplay), m_DispMask(mask),
          m_IsRunning(false), m_Overrun(false),
          m_StartUS(0)
    {}
    ~RunningCrc()
    {
        StopAndCopy();
    }
    RC Start()
    {
        RC rc = m_pDisplay->StartRunningCrc(
            m_DispMask, Display::CRC_DURING_SNOOZE_DISABLE);
        m_StartUS = Platform::GetTimeUS();
        if (OK == rc)
            m_IsRunning = true;
        return rc;
    }
    RC StopAndCopy()
    {
        RC rc;
        if (!m_IsRunning)
            return OK;
        m_IsRunning = false;
        CHECK_RC(m_pDisplay->StopRunningCrc(m_DispMask));

        const UINT32 * pBuf = reinterpret_cast<const UINT32*>
            (m_pDisplay->GetCrcBuffer(m_DispMask));
        const UINT32 status = MEM_RD32(&pBuf[0]);
        const unsigned numCrcs = DRF_VAL(507D, _NOTIFIER_CRC_1, _STATUS_0_COUNT, status);
        m_Overrun = 0 != DRF_VAL(507D, _NOTIFIER_CRC_1, _STATUS_0_OVERRUN, status);
        const CrcEntry * pCrcs = reinterpret_cast<const CrcEntry*>
            (pBuf + LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2);

        m_CopiedCrcs.resize(numCrcs);
        Platform::MemCopy(&m_CopiedCrcs[0], pCrcs, numCrcs*sizeof(CrcEntry));
        return rc;
    }
    void ForceError(unsigned index)
    {
        if (index < m_CopiedCrcs.size())
            m_CopiedCrcs[index].m_CompositorCrc ^= 1;
    }
    UINT64                   StartTimeUS() const  { return m_StartUS; }
    const vector<CrcEntry> & GetCrcs() const      { return m_CopiedCrcs; }
    const bool               Overrun() const      { return m_Overrun; }
};

} // anonymous namespace

//------------------------------------------------------------------------------
class EvoThread : public GpuTest
{
public:
    EvoThread();
    virtual ~EvoThread();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    // JS-settable properties
    UINT32 m_MaxPclkHz;       //!< Max pclk to use, 0 for max HW allows.
    bool   m_UseMaxPclk;      //!< If true, use max possible pixel clock
    bool   m_KeepRunning;     //!< If true, continue running past normal loopcount
    UINT32 m_ForceErrorFrame; //!< Frame at which to force an error (if !0).
    FLOAT64 m_MaxPclkReductionPct; //!< Specifies a % of max pclk that it should be reduced by
public:
    SETGET_PROP(MaxPclkHz, UINT32);
    SETGET_PROP(UseMaxPclk, bool);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(ForceErrorFrame, UINT32);
    SETGET_PROP(MaxPclkReductionPct, FLOAT64);

private:
    // Other test state
    Surface2D m_Surf;
    bool m_OldEnableDefaultImage;
    UINT32 m_OrgDispMask;
    UINT32 m_DispMask;
    bool m_SimulatedDPEnabled;

    // Private methods
    void RestoreStuffVoid();
    RC RestoreStuff();
};

//----------------------------------------------------------------------------
// Script interface.

JS_CLASS_INHERIT(EvoThread, GpuTest,
                 "Evo Display continuous CRC test for use with stress thread.");
CLASS_PROP_READWRITE(EvoThread, MaxPclkHz, UINT32,
                     "Max pclk to use, 0 for max HW allows.");
CLASS_PROP_READWRITE(EvoThread, UseMaxPclk, bool,
                     "Force a non-displayable output mode to use max pclk.");
CLASS_PROP_READWRITE(EvoThread, KeepRunning, bool,
                     "Continue running past end of TestConfiguration.Loops while this is true");
CLASS_PROP_READWRITE(EvoThread, ForceErrorFrame, UINT32,
                     "Frame at which to force a CRC miscompare, if !0");
CLASS_PROP_READWRITE(EvoThread, MaxPclkReductionPct, FLOAT64,
                     "Specifies a percentage of max pclk that it should be reduced by");

EvoThread::EvoThread()
    : m_MaxPclkHz(0)
    ,m_UseMaxPclk(true)
    ,m_KeepRunning(false)
    ,m_ForceErrorFrame(0)
    ,m_MaxPclkReductionPct(0.0)
    ,m_OldEnableDefaultImage(false)
    ,m_OrgDispMask(0)
    ,m_DispMask(0)
    ,m_SimulatedDPEnabled(false)
{
    SetName("EvoThread");
}
/*virtual*/
EvoThread::~EvoThread()
{
}

bool EvoThread::IsSupported()
{
    Display *pDisplay = GetDisplay();

    if (pDisplay->GetDisplayClassFamily() != Display::EVO)
    {
        return false;
    }
    return true;
}

RC EvoThread::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());
    CHECK_RC(GpuTest::AllocDisplay());

    if (GetDisplay()->GetDisplayClassFamily() != Display::EVO)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    GpuTestConfiguration * tc = GetTestConfiguration();
    m_Surf.SetWidth(tc->SurfaceWidth());
    m_Surf.SetHeight(tc->SurfaceHeight());
    m_Surf.SetColorFormat(ColorUtils::A8R8G8B8);
    m_Surf.SetType(LWOS32_TYPE_PRIMARY);
    m_Surf.SetLocation(Memory::Optimal);
    m_Surf.SetDisplayable(true);
    CHECK_RC(m_Surf.Alloc(GetBoundGpuDevice()));

    Surface2D tmpSurf;
    tmpSurf.SetWidth(m_Surf.GetWidth());
    tmpSurf.SetHeight(m_Surf.GetHeight());
    tmpSurf.SetColorFormat(m_Surf.GetColorFormat());
    tmpSurf.SetLocation(Memory::Coherent);
    CHECK_RC(tmpSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(tmpSurf.Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
    CHECK_RC(Memory::FillRgbBars(tmpSurf.GetAddress(),
                                 tmpSurf.GetWidth(),
                                 tmpSurf.GetHeight(),
                                 tmpSurf.GetColorFormat(),
                                 tmpSurf.GetPitch()));
    CHECK_RC(GpuUtility::PutChars(
                 tmpSurf.GetWidth()/10,  // starting X (near left edge)
                 tmpSurf.GetHeight()/2,  // starting Y (center)
                 string("Running CRCs..."),
                 2,                     // scale
                 0xffffffff,            // foreground color
                 0,                     // background color
                 false,                 // underline
                 &tmpSurf));

    DmaWrapper dmaWrap;
    CHECK_RC(dmaWrap.Initialize(tc, Memory::Optimal));
    dmaWrap.SetSurfaces(&tmpSurf, &m_Surf);
    CHECK_RC(dmaWrap.Copy(0,0, m_Surf.GetPitch(), m_Surf.GetHeight(), 0, 0,
                          tc->TimeoutMs(), 0));
    tmpSurf.Free();

    Tee::Priority pri = GetVerbosePrintPri();
    Display *pDisplay = GetDisplay();
    const UINT32 width   = tc->DisplayWidth();
    const UINT32 height  = tc->DisplayHeight();
    const UINT32 depth   = 32;
    const UINT32 refrate = tc->RefreshRate();

    m_OrgDispMask = m_DispMask = pDisplay->Selected();
    m_OldEnableDefaultImage = pDisplay->GetEnableDefaultImage();
    m_SimulatedDPEnabled = false;

    Utility::CleanupOnReturn<EvoThread> cleanup(this, &EvoThread::RestoreStuffVoid);

    CHECK_RC(pDisplay->SetEnableDefaultImage(false));

    if (m_UseMaxPclk)
    {
        if (pDisplay->GetDisplayType(m_OrgDispMask) == Display::DFP)
        {
            Display::Encoder encoderInfo;
            CHECK_RC(pDisplay->GetEncoder(m_OrgDispMask, &encoderInfo));
            if (encoderInfo.Signal == Display::Encoder::DP)
            {
                UINT32 rootID = pDisplay->GetMultiStreamDPRootID(m_OrgDispMask);
                if (rootID == 0)
                    rootID = m_OrgDispMask;
                UINT32 simulatedDisplayIDMask = 0;
                CHECK_RC(pDisplay->SetBlockHotPlugEvents(true));
                CHECK_RC(pDisplay->EnableSimulatedDPDevices(rootID,
                    Display::NULL_DISPLAY_ID,
                    Display::DPMultistreamDisabled, 1, nullptr, 0,
                    &simulatedDisplayIDMask));
                m_SimulatedDPEnabled = true;
                CHECK_RC(pDisplay->Select(simulatedDisplayIDMask));
                m_DispMask = simulatedDisplayIDMask;
            }
        }

        // Copied without understanding from other evo tests.
        EvoRasterSettings ers(width+200,  101, width+100+49, 149,
                              height+150,  50, height+50+50, 100,
                              1, 0,
                              0,
                              0, 0,
                              false);

        // Starting guess at minimum pixel clock.
        ers.PixelClockHz = ers.RasterWidth * ers.RasterHeight * refrate;

        Display::MaxPclkQueryInput maxPclkQuery = {m_DispMask,
                                                   &ers};
        vector<Display::MaxPclkQueryInput> maxPclkQueryVector(1, maxPclkQuery);
        CHECK_RC(pDisplay->SetRasterForMaxPclkPossible(&maxPclkQueryVector, m_MaxPclkHz));

        // Apply any user-specified max pclk reduction %
        if (m_MaxPclkReductionPct != 0.0)
            ers.PixelClockHz = static_cast<UINT32>(ers.PixelClockHz *
                                       (100.0 - m_MaxPclkReductionPct) / 100.0);
        else
        {
            // Bug 200022562 GM20x WAR
            // The issue is that certain dispclk transitions like:
            // 405->1080, 1080->405, 648->405 etc cause RG underflow
            // Skipping dispclk transitions works on *most* boards but we
            // are seeing some system dependent failures that require test 245
            // to not test max pclk too close to disppclk
            // This WAR tunes the max pclk down by 2% to improve test 207 stability
            // (making such changes to IMP means we change max pclk coverage on ALL
            // display tests -- which is not a good idea)
            if (GetBoundGpuSubdevice()->HasBug(200022562))
                ers.PixelClockHz = static_cast<UINT32>(ers.PixelClockHz * 0.98);
        }
        Printf(pri, "UseMaxPclk, choosing %.1fmhz\n", ers.PixelClockHz / 1e6);
        CHECK_RC(pDisplay->SetTimings(&ers));
    }

    CHECK_RC(pDisplay->SetMode(
                 m_DispMask,
                 width,
                 height,
                 depth,
                 refrate,
                 Display::FilterTapsNoChange,
                 Display::ColorCompressionNoChange,
                 0, // scaler mode
                 (VIDEO_INFOFRAME_CTRL*)0,
                 (AUDIO_INFOFRAME_CTRL *)0));

    CHECK_RC(pDisplay->SetImage(m_DispMask, &m_Surf));

    // On success, defer restoring Display until after Run (i.e. Cleanup).
    cleanup.Cancel();

    return rc;
}

RC EvoThread::Run()
{
    RC rc;
    StickyRC finalRc;
    Display *pDisplay = GetDisplay();
    Tee::Priority pri = GetVerbosePrintPri();

    GpuTestConfiguration * tc = GetTestConfiguration();
    const UINT32 refrate = tc->RefreshRate();
    const double frameMs  = 1000.0 / refrate;
    const UINT64 runStartUS = Platform::GetTimeUS();

    UINT32 head = 0;
    CHECK_RC(pDisplay->GetHead(m_DispMask, &head));
    CHECK_RC(pDisplay->ClearUnderflow(head));

    RunningCrc runningCrc(pDisplay, m_DispMask);

    // Loop until done, checking that we don't see any CRC mismatches.
    int numCrcErrs = 0;
    CrcEntry firstCrc;
    unsigned frame = 0;
    while (frame < tc->Loops() || m_KeepRunning)
    {
        Printf(pri, "Start running crc frame=%u\n", frame);
        CHECK_RC(runningCrc.Start());

        // We can't check results at all until we stop the running CRC process.
        // But each time we stop and restart the running CRC, we will let at
        // least one frame go by unchecked.
        // So we will sleep as long as possible before stopping the running CRC.
        const unsigned crcBufMaxCapacity = 255;
        unsigned sleeps = 0;
        while (sleeps < crcBufMaxCapacity &&
               ((frame + sleeps < tc->Loops()) || m_KeepRunning))
        {
            Tasker::Sleep(frameMs);
            sleeps++;

            if (pDisplay->GetUnderflow(head))
            {
                Printf(pri,
                       "Underflow: head %d frame %d (about %.0f ms from start of test).\n",
                       head, frame + sleeps,
                       (runningCrc.StartTimeUS() - runStartUS)/1000.0 +
                       sleeps * frameMs + frameMs/2);
                finalRc = RC::DISPLAY_UNDERFLOW;
                if (GetGoldelwalues()->GetStopOnError())
                    return finalRc;
            }

            // Check for ErrCounter-based early-exit conditions once per second.
            if ((frame + sleeps) % refrate == refrate)
                CHECK_RC(EndLoop(frame+sleeps));
        }

        Printf(pri, "Stop running crc frame=%u after sleeping %u frames\n",
               frame, sleeps);
        CHECK_RC(runningCrc.StopAndCopy());
        const vector<CrcEntry>& crcs = runningCrc.GetCrcs();
        Printf(pri, "Collected %u crcs (expected %u) overrun=%s\n",
               static_cast<unsigned>(crcs.size()),
               sleeps,
               runningCrc.Overrun() ? "true" : "false");

        if (m_ForceErrorFrame &&
            m_ForceErrorFrame >= frame &&
            m_ForceErrorFrame < frame + crcs.size())
        {
            // Test error reporting software.
            runningCrc.ForceError(m_ForceErrorFrame - frame);
        }

        for (unsigned ii = 0; ii < crcs.size(); ++ii)
        {
            if (firstCrc.IsZero())
            {
                firstCrc = crcs[0];
                if (firstCrc.IsZero())
                {
                    Printf(Tee::PriHigh, "invalid crcs: %s\n",
                           firstCrc.ToString().c_str());
                    return RC::GOLDEN_VALUE_MISCOMPARE;
                }
                else
                {
                    Printf(pri, "First CRC = %s\n", firstCrc.ToString().c_str());
                }
            }
            else if (firstCrc != crcs[ii])
            {
                if (numCrcErrs++ < 10)
                {
                    const double badFrameApproxMs =
                        (runningCrc.StartTimeUS() - runStartUS)/1000.0 +
                        ii * frameMs + frameMs/2;

                    Printf(pri,
                           "Crc mismatch at frame %d (frameOfRun %d).\n"
                           " at about %.0f ms since start of test.\n"
                           " First CRC entry: %s\n"
                           " This CRC entry : %s\n",
                           frame + ii, ii,
                           badFrameApproxMs,
                           firstCrc.ToString().c_str(),
                           crcs[ii].ToString().c_str());
                    GetJsonExit()->AddFailLoop(frame + ii);
                }
                finalRc = RC::GOLDEN_VALUE_MISCOMPARE;
            }
        }
        frame += static_cast<UINT32>(crcs.size());
        if (finalRc && GetGoldelwalues()->GetStopOnError())
            break;
    }

    Printf(pri, "%d of %d frames had mismatching CRCs.\n", numCrcErrs, frame);
    return finalRc;
}

RC EvoThread::RestoreStuff()
{
    Display * pDisplay = GetDisplay();
    if ((nullptr == pDisplay) ||
        (pDisplay->GetDisplayClassFamily() != Display::EVO))
    {
        return OK; // Nothing to do
    }

    StickyRC rc;
    rc = pDisplay->SetImage(m_DispMask, (Surface2D*)0);
    rc = pDisplay->SetEnableDefaultImage(m_OldEnableDefaultImage);
    pDisplay->SetTimings(NULL);
    rc = pDisplay->SetBlockHotPlugEvents(false);

    if (m_SimulatedDPEnabled)
    {
        rc = pDisplay->DetachAllDisplays();
        rc = pDisplay->DisableSimulatedDPDevices(m_DispMask,
            Display::NULL_DISPLAY_ID, Display::DetectRealDisplays);
        m_SimulatedDPEnabled = false;
        rc = pDisplay->Select(m_OrgDispMask);
        GpuTestConfiguration * tc = GetTestConfiguration();
        rc = pDisplay->SetMode(tc->DisplayWidth(),
                               tc->DisplayHeight(),
                               32,
                               tc->RefreshRate());
    }

    return rc;
}
void EvoThread::RestoreStuffVoid()
{
    RC rc = RestoreStuff();
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "EvoThread: error restoring display state %d\n",
               rc.Get());
    }
}

RC EvoThread::Cleanup()
{
    StickyRC rc = RestoreStuff();
    rc = GpuTest::Cleanup();
    m_Surf.Free();
    return rc;
}

