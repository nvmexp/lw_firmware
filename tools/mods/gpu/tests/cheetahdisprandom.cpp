/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/abort.h"
#include "tegrawin.h"

class TegraDispRandom: public TegraWinTest
{
    public:
        TegraDispRandom();

        RC Run() override;
        RC Setup() override;
        bool IsSupported() override;
        void PrintJsProperties(Tee::Priority pri) override;

        SETGET_PROP(ForceInitialPclkMhz, UINT32);
        SETGET_PROP(FramesPerModeset, UINT32);
        SETGET_PROP(MaxPclkMhz, UINT32);

    private:
        RC RunDisplayTest(UINT32 head,
                          UINT32 display,
                          TegraDisplay::ConnectionType conType,
                          UINT32 startLoop,
                          UINT32 endLoop,
                          bool *pOneModeTested);
        RC PickRasterSettings();
        RC PickOpeFmt(TegraDisplay::ConnectionType conType);
        RC PickScanMethod(UINT32 selectedDisplay);
        RC PickInitialPclk(TegraDisplay::ConnectionType conType);
        RC RestartRndGenerator(UINT32 seed);
        RC SetTimings(UINT32 pclkMhz);
        RC PickSubTestMode();
        RC GetPclkRange
        (
            TegraDisplay::ConnectionType       connType,
            TegraDisplay::OutputPixelEncoding  ope,
            std::pair <UINT32, UINT32>         *pPclkRange
        );

        RC ValidateDisplay(UINT32 head, UINT32 display, bool bInterlaced) override;
        RC RunInnerLoops
        (
            UINT32          head,
            UINT32          display,
            const TestMode& tm,
            UINT32          startLoop,
            UINT32          endLoop,
            UINT32          baseSeed,
            bool            applyFrameSettings,
            bool           *pIsInnerLoopTested
        ) override;
        RC PerformModeSet
        (
            UINT32    head,
            UINT32    display,
            TestMode  tm,
            bool     *pbModeSkipped
        ) override;
        RC GetCRCSuffix
        (
            UINT32    head,
            UINT32    display,
            TestMode  tm,
            string   *pOverrideSuffix
        ) override;

        //! Properties set from Javascript
        UINT32         m_ForceInitialPclkMhz = 0;
        UINT32         m_FramesPerModeset    = 1;
        UINT32         m_MaxPclkMhz          = ~0U;

        //! private data members
        const INT32     m_Threshold       = 10;
        TestMode        m_Tm;
        UINT32          m_SelectedPclkMhz = 100;
        set<UINT32>     m_LoopsSkippedInIMPDryRun; // IMP failed even at min pclk
};

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
TegraDispRandom::TegraDispRandom()
{
    SetName("TegraDispRandom");
}

//-----------------------------------------------------------------------------
//! \brief Determine if the test is supported
//!
//! \return true if the test is supported, false otherwise
bool TegraDispRandom::IsSupported()
{
    if (GetBoundGpuDevice()->HasBug(2039187))
    {
        Printf(Tee::PriNormal, "TegraDispRandom test is not supported on T21x and T186\n");
        return false;
    }
    return TegraWinTest::IsSupported();
}

//----------------------------------------------------------------------------
//! \brief Setup TegraDispRandom test
//!
//! \return OK if setup succeeds, not OK otherwise
RC TegraDispRandom::Setup()
{
    RC rc;
    CHECK_RC(TegraWinTest::Setup());
    m_OverrideGoldenCrcSuffix = true;
    m_ApplyUpscalingConstraints = true;
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Compute and setup timings to use while performing a modeset
//!
//! \param pclkMhz    : Pixel clock to achieve in Mhz units
//!
//! \return OK if successful, not OK otherwise
RC TegraDispRandom::SetTimings(UINT32 pclkMhz)
{
    LWT_TIMING lwtTiming;
    UINT64 pclkHz = pclkMhz * 1000 * 1000;
    // Callwlate a 60Hz timing at the test resolution
    if (LwTiming_CalcCVT_RB(m_Tm.mode.width, m_Tm.mode.height, 60, 0,
                            &lwtTiming))
    {
        Printf(Tee::PriError, "Unable to generate CVT timings with "
                "reduced blanking for %ux%u @ 60Hz\n",
               m_Tm.mode.width,
               m_Tm.mode.height);
        return RC::SOFTWARE_ERROR;
    }

    // pclk is in 10 Khz units. Covert Mhz to 10 Khz
    lwtTiming.pclk = static_cast<LwU32>(pclkHz / 10000);

    m_Tm.mode.refreshRate = static_cast<UINT32>(pclkHz /
                               (lwtTiming.HTotal * lwtTiming.VTotal));

    return m_pDisplay->SetTimings(&lwtTiming);
}

//-----------------------------------------------------------------------------
//! \brief Exelwtes inner loop which verfies various frame level settings
//!
//! \param head                 : head to test
//! \param display              : display to test
//! \param tm                   : test mode
//! \param startLoop            : Start loop index
//! \param endLoop              : End loop index
//! \param baseSeed             : Base seed
//! \param applyFrameSettings   : apply settings in H/W
//! \param pIsInnerLoopTested   : return true if atleast one loop is tested
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC TegraDispRandom::RunInnerLoops
(
    UINT32            head,
    UINT32            display,
    const TestMode&   tm,
    UINT32            startLoop,
    UINT32            endLoop,
    UINT32            baseSeed,
    bool              applyFrameSettings,
    bool             *pIsInnerLoopTested
)
{
    RC rc;
    bool isBlendingDisabled = false;
    bool isHWLwrsorDisabled = false;
    TegraDisplay::UnderflowCounts initialUnderflowCount;
    const UINT32 restartSkipCount = m_pTestConfig->RestartSkipCount();
    UINT32 skippedLoops = 0;

    MASSERT(pIsInnerLoopTested);
    if (nullptr == pIsInnerLoopTested)
        return RC::SOFTWARE_ERROR;

    *pIsInnerLoopTested = false;

    const UINT32 seedOffset = (GetSeedOffset() == ~0U)
        ? startLoop % m_FramesPerModeset
        : GetSeedOffset();
    UINT32 seed = baseSeed + seedOffset;

    m_pFpCtx->Rand.SeedRandom(seed);
    m_pFpCtx->LoopNum = startLoop;
    m_pFpCtx->RestartNum = startLoop / restartSkipCount;
    m_pGolden->SetLoop(startLoop);

    VerbosePrintf("Seed offset: 0x%x\n", seedOffset);
    VerbosePrintf("refreshRate %u\n", tm.mode.refreshRate);

    if (applyFrameSettings)
    {
        CHECK_RC(SetupInnerLoop(head, display));
        CHECK_RC(m_pDisplay->GetUnderflowCounts(head, &initialUnderflowCount));
    }

    CHECK_RC(SetTimings(m_SelectedPclkMhz));

    string featureOverrideKey;
    CHECK_RC(FindKeyInFeatureOverideJson(head, display, tm, &featureOverrideKey));

    for (m_pFpCtx->LoopNum = startLoop; m_pFpCtx->LoopNum < endLoop; m_pFpCtx->LoopNum++)
    {
        CHECK_RC(Abort::Check());

        if ((m_pFpCtx->LoopNum % restartSkipCount) == 0)
        {
            m_pFpCtx->RestartNum = m_pFpCtx->LoopNum / restartSkipCount;
            const UINT32 seedOffset = (GetSeedOffset() == ~0U)
                ? m_pFpCtx->LoopNum % m_FramesPerModeset
                : GetSeedOffset();
            seed = baseSeed + seedOffset;
            m_pFpCtx->Rand.SeedRandom(seed);
        }

        if (IsLoopSkipped(m_pFpCtx->LoopNum))
        {
            ++skippedLoops;
            continue;
        }

        // Skipped it in IMP dry run. No need to TestFrameSettings
        if (m_LoopsSkippedInIMPDryRun.find(m_pFpCtx->LoopNum) != m_LoopsSkippedInIMPDryRun.end())
            continue;

        VerbosePrintf("Inner loop index = %u. seed = 0x%x\n", m_pFpCtx->LoopNum, seed);
        rc = TestFrameSettings(head, display, featureOverrideKey, &isBlendingDisabled,
                               &isHWLwrsorDisabled, applyFrameSettings, initialUnderflowCount, m_Tm);

        if (rc == OK)
        {
            *pIsInnerLoopTested = true;
        }
        else if (rc == RC::MODE_IS_NOT_POSSIBLE)
        {
            string connStr;
            if (!applyFrameSettings) // IMP dry run
            {
                m_LoopsSkippedInIMPDryRun.insert(m_pFpCtx->LoopNum);
            }
            CHECK_RC(GetConnectionTypeString(display, tm.connection, &connStr));
            VerbosePrintf("Skipping loop %u on head %u %s since IMP failed at min pclk "
                          "(seed = 0x%x) \n", m_pFpCtx->LoopNum, head, connStr.c_str(), seed);
            rc.Clear();
        }
        CHECK_RC(rc);
    }
    CHECK_RC(ResetTimings());

    if (applyFrameSettings)
    {
        CHECK_RC(UpdateTestCoverageInfo(head, display, tm));
    }

    if ((endLoop - startLoop) == skippedLoops)
    {
        // If all inner loops are skipped by user, don't fail
        *pIsInnerLoopTested = true;
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Validate the display configuration for current loop
//!
//! \param head           : head under test
//! \param display        : Display that is being randomized
//! \param bInterlaced    : True if the display is interlaced
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC TegraDispRandom::ValidateDisplay(UINT32 head, UINT32 display, bool bInterlaced)
{
    RC rc;
    bool bWinCfgPossible = false;
    TegraDisplay::WindowConfigInfo winInfo;
    std::pair <UINT32, UINT32> pclkRange;
    const UINT32 lwrsorSize =
        (GetLwrsorColorFmt() == ColorUtils::LWFMT_NONE) ? 0 :
         GetLwrsorSize();

    TegraDisplay::OutputPixelEncoding ope(m_Tm.mode.opeFormat, m_Tm.mode.opeYuvBpp);

    UINT32 pclkToTest = m_SelectedPclkMhz;
    CHECK_RC(SetTimings(pclkToTest));

    CHECK_RC(m_pDisplay->IsWindowConfigPossible(head,
                                                display,
                                                GetImageDescs(),
                                                GetImageConfigs(),
                                                GetWindowViewPorts(),
                                                GetCmuConfig(),
                                                GetBlendDesc(),
                                                GetEnableHWLwrsorPerLoop() ? lwrsorSize : 0,
                                                GetEnableHWLwrsorPerLoop() ? GetLwrsorColorFmt() :
                                                                   ColorUtils::LWFMT_NONE,
                                                m_Tm.mode.width,
                                                m_Tm.mode.height,
                                                m_Tm.mode.refreshRate,
                                                m_Tm.mode.bInterlaced,
                                                ope,
                                                &bWinCfgPossible,
                                                &winInfo));

    const Tee::Priority pri = GetVerbosePrintPri();
    TegraDisplay::ConnectionType connType = TegraDisplay::CT_NUM_TYPES;
    bool bConnectionSkipped;
    CHECK_RC(GetConnectionType(m_pDisplay,
                               display,
                               &connType,
                               NULL,
                               &bConnectionSkipped));
    MASSERT(!bConnectionSkipped);

    if (!bWinCfgPossible)
    {
        Printf(pri,
               "******* ValidateDisplay Initial Config Failed "
               "(Display 0x%08x head %1d loop %04d Cause %u pclkMHz %u)\n",
               display, head, m_pFpCtx->LoopNum, winInfo.FailureReason, pclkToTest);
        PrintLoopInfo(Tee::PriDebug, head);
        Printf(pri,
               "**********************************************"
               "*********************\n");

        TegraDisplay::OutputPixelEncoding ope(m_Tm.mode.opeFormat, m_Tm.mode.opeYuvBpp);
        CHECK_RC(GetPclkRange(connType, ope, &pclkRange)); // in Mhz units
        const UINT32 minPclkPossible = pclkRange.first;
        UINT32 low = minPclkPossible;
        UINT32 high = pclkToTest;
        INT32 delta = high - low;
        UINT32 passingPclk = 0;
        while ((delta > m_Threshold) && (low >= minPclkPossible))
        {
            pclkToTest = low + (delta/2);
            Printf(Tee::PriDebug, "pclkToTest = %u\n", pclkToTest);
            CHECK_RC(SetTimings(pclkToTest));

            CHECK_RC(m_pDisplay->IsWindowConfigPossible(head,
                                                        display,
                                                        GetImageDescs(),
                                                        GetImageConfigs(),
                                                        GetWindowViewPorts(),
                                                        GetCmuConfig(),
                                                        GetBlendDesc(),
                                                        GetEnableHWLwrsorPerLoop() ? lwrsorSize : 0,
                                                        GetEnableHWLwrsorPerLoop() ? GetLwrsorColorFmt() :
                                                                           ColorUtils::LWFMT_NONE,
                                                        m_Tm.mode.width,
                                                        m_Tm.mode.height,
                                                        m_Tm.mode.refreshRate,
                                                        m_Tm.mode.bInterlaced,
                                                        ope,
                                                        &bWinCfgPossible,
                                                        &winInfo));

            (bWinCfgPossible ? low : high) = pclkToTest;
            delta = high - low;
            // It is possible that IMP passes on an intermediate loop and while attempting
            // to explore higher pclks, we exit the loop because delta < threshold. In such
            // cases, we need to test at passing pclk
            if (bWinCfgPossible)
                passingPclk = pclkToTest;
        }

        // Retrieve stored passing pclk
        if (passingPclk)
        {
            bWinCfgPossible = true;
            pclkToTest = passingPclk;
        }
    }

    if (bWinCfgPossible)
    {
        Printf(pri, "ValidateDisplay final successful config "
                    "(Display 0x%08x head %1d loop %04d, pclkMHz %u)\n",
                    display, head, m_pFpCtx->LoopNum, pclkToTest);

        MASSERT(m_SelectedPclkMhz >= pclkToTest);
        m_SelectedPclkMhz = pclkToTest;
    }
    else
    {
        Printf(Tee::PriWarn,
               "ValidateDisplay : No valid pclk found for window configuration\n");
        return RC::MODE_IS_NOT_POSSIBLE;
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Picks raster settings to be used in current mode set
//!
//! \return OK if successful, not OK otherwise
RC TegraDispRandom::PickRasterSettings()
{
    RC rc;
    std::pair <UINT32, UINT32> widthRange;   // (minW, maxW)
    std::pair <UINT32, UINT32> heightRange;  // (minH, maxH)
    LWT_TIMING lwtTiming;

    CHECK_RC(m_pDisplay->GetRasterLimits(&widthRange, &heightRange));
    if (m_StressLevel == StressLevel::STRESSFUL)
    {
        m_Tm.mode.width  =  widthRange.second;
        m_Tm.mode.height =  heightRange.second;
    }
    else
    {
        RasterSizeMode rsMode = static_cast<RasterSizeMode>
                                ((*m_pPickers)[FPK_TEGWIN_RASTER_SIZE].Pick());
        m_Tm.mode.width  =  widthRange.second;
        m_Tm.mode.height =  heightRange.second;

        switch (rsMode)
        {
            case RasterSizeMode::MAX_HEIGHT :
                m_Tm.mode.width  = m_pFpCtx->Rand.GetRandom(widthRange.first, widthRange.second);
                break;
            case RasterSizeMode::MAX_WIDTH :
                m_Tm.mode.height = m_pFpCtx->Rand.GetRandom(heightRange.first, heightRange.second);
                break;
            case RasterSizeMode::RANDOM :
                m_Tm.mode.width  = m_pFpCtx->Rand.GetRandom(widthRange.first, widthRange.second);
                m_Tm.mode.height = m_pFpCtx->Rand.GetRandom(heightRange.first, heightRange.second);
                break;
            default:
                MASSERT(!"Unsupported raster size mode\n");
                return RC::SOFTWARE_ERROR;
        }
    }

    // Apply applicable HW constraints based on OPE
    CHECK_RC(m_pDisplay->ApplyOPEHwConstraints(&m_Tm.mode.width,
                                               &m_Tm.mode.height,
                                               m_Tm.mode.opeFormat));

    // Callwlate a 60Hz timing at the test resolution
    if (LwTiming_CalcCVT_RB(m_Tm.mode.width, m_Tm.mode.height, 60, 0,
                            &lwtTiming))
    {
        Printf(Tee::PriError, "Unable to generate CVT timings with "
                "reduced blanking for %ux%u @ 60Hz\n",
               m_Tm.mode.width,
               m_Tm.mode.height);
        return RC::SOFTWARE_ERROR;
    }

    // LwTiming_CalcCVT_RB rounds off visible width and height while generating timings
    // Update the test mode so that it is in sync with timings configured in mode set
    m_Tm.mode.width = lwtTiming.HVisible;
    m_Tm.mode.height = lwtTiming.VVisible;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Picks Output pixel encoding configuration to be used in current mode set
//!
//! \return OK if successful, not OK otherwise
RC TegraDispRandom::PickOpeFmt(TegraDisplay::ConnectionType conType)
{
    Display::OpeFmt opeFmt = static_cast<Display::OpeFmt>
                             ((*m_pPickers)[FPK_TEGWIN_OPE_FMT].Pick());
    Display::OpeYuvBpp yuvBpp = Display::OpeYuvBpp::BPP_NONE;

    if (opeFmt != Display::OpeFmt::RGB)
    {
        TegraDisplay::OutputPixelEncoding ope(opeFmt, Display::OpeYuvBpp::BPP_NONE);
        UINT32 retries = 15;
        do
        {
            yuvBpp = static_cast<Display::OpeYuvBpp>
                                ((*m_pPickers)[FPK_TEGWIN_OPE_YUV_BPP].Pick());
            ope.opeYuvBpp = yuvBpp;
        }
        while (!m_pDisplay->IsYUVFmtSupported(conType, ope) && --retries);

        // If retries are exhaused, fallback to RGB
        if (!retries)
        {
            opeFmt = Display::OpeFmt::RGB;
            yuvBpp = Display::OpeYuvBpp::BPP_NONE;
        }
    }

    // In order to retain sequence, first pick the values and then check for test argument
    if (!GetEnableYUVMode())
    {
        opeFmt = Display::OpeFmt::RGB;
        yuvBpp = Display::OpeYuvBpp::BPP_NONE;
    }

    m_Tm.mode.opeFormat = opeFmt;
    m_Tm.mode.opeYuvBpp = yuvBpp;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Picks scan method (progressive v/s interlaced) to use in current modeset
//!
//! \return OK if successful, not OK otherwise
RC TegraDispRandom::PickScanMethod(UINT32 selectedDisplay)
{
    ScanMethod scanMethod = static_cast<ScanMethod>
                            ((*m_pPickers)[FPK_TEGWIN_SCAN_METHOD].Pick());
    if (!m_pDisplay->InterlacedSupported(selectedDisplay))
    {
        m_Tm.mode.bInterlaced = false;
        return OK;
    }

    m_Tm.mode.bInterlaced = (scanMethod == ScanMethod::INTERLACED);
    return OK;
}

RC TegraDispRandom::RestartRndGenerator(UINT32 seed)
{
    m_pFpCtx->RestartNum = 0;
    m_pFpCtx->Rand.SeedRandom(seed);
    m_pPickers->Restart();
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Perform tests on selected display
//!
//! \param head                 : head to test
//! \param display              : display to test
//! \param conType              : connection type
//! \param startLoop            : Start loop index
//! \param endLoop              : End loop index
//! \param pOneModeTested       : return true if atleast one mode is tested
//!
//! \return OK if successful, not OK otherwise
RC TegraDispRandom::RunDisplayTest
(
    UINT32 head,
    UINT32 display,
    TegraDisplay::ConnectionType conType,
    UINT32 startLoop,
    UINT32 endLoop,
    bool *pOneModeTested
)
{
    RC rc;

    MASSERT(pOneModeTested);
    if (pOneModeTested == nullptr)
        return RC::SOFTWARE_ERROR;

    const UINT32 firstModesetIdx  = startLoop / m_FramesPerModeset;
    const UINT32 lastModesetIdx   = endLoop / m_FramesPerModeset;

    m_pGolden->SetLoop(startLoop);
    m_Tm.connection = conType;

    for (UINT32 index = firstModesetIdx; index < lastModesetIdx; ++index)
    {
        bool isModeSkipped = false;
        bool isOneLoopTested = false;
        // TODO add support for isolateLoops handling
        // Spread seed in different ranges so that we have unique seeds across different
        // displays both in outer and inner loop
        const UINT32 baseSeed = m_pTestConfig->Seed() + (sizeof(display) * 8 * index +
                            Utility::BitScanForward(display)) * m_FramesPerModeset;

        CHECK_RC(Abort::Check());

        VerbosePrintf("Starting modeset index %u, baseSeed 0x%x\n", index, baseSeed);

        CHECK_RC(RestartRndGenerator(baseSeed));

        CHECK_RC(PickSubTestMode());

        CHECK_RC(PickStressLevel());     // Pick between Normal v/s stressful settings

        CHECK_RC(PickOpeFmt(conType));   // Pick Output pixel encoding format

        CHECK_RC(PickRasterSettings());  // Pick Raster Settings

        CHECK_RC(PickScanMethod(display)); // Progress v/s interlaced

        // PickSOR();            // Pick sor index to use TODO
        // We cannot chose OR type since each display supports only one OR type.
        // Assign head to selected SOR
        // ApplyORConfig();

        // Dynamically assign windows to head under test
        CHECK_RC(RandomizeWinAssignment(head, m_Tm));

        CHECK_RC(PickInitialPclk(conType));

        // Route the tested head to the connector
        if (!GetUseRealDisplays())
        {
            CHECK_RC(m_pDisplay->SetupHeadRouting(display, head));
        }

        CHECK_RC(m_pDisplay->Select(display));

        Printf(Tee::PriDebug, "Test Mode (width = %u height = %u pclk = %u\n)",
                             m_Tm.mode.width, m_Tm.mode.height, m_SelectedPclkMhz);

        // Perform a dry run of IMP to arrive at max pclk that passes IMP for all loops
        CHECK_RC(RunInnerLoops(head,
                               display,
                               m_Tm,
                               index * m_FramesPerModeset,
                               ((index + 1) * m_FramesPerModeset),
                               baseSeed,
                               false, // dry run. Don't apply settings in H/W
                               &isOneLoopTested));

        if (!isOneLoopTested)
        {
            // IMP failed with mode not possible for all inner loops
            string connStr;
            CHECK_RC(GetConnectionTypeString(display, m_Tm.connection, &connStr));
            VerbosePrintf("Skipping %s Resolution %uX%u%s %u Hz with %s on Head %u "
                          "Display 0x%08x (mode not possible for all loops)\n",
                          connStr.c_str(),
                          m_Tm.mode.width, m_Tm.mode.height,
                          m_Tm.mode.bInterlaced ? "i" : "",
                          m_Tm.mode.refreshRate,
                          StringifyOPE(m_Tm.mode.opeFormat, m_Tm.mode.opeYuvBpp).c_str(),
                          head, display);
            rc.Clear();
            continue;
        }

        CHECK_RC(SetTimings(m_SelectedPclkMhz));
        CHECK_RC(PerformModeSet(head, display, m_Tm, &isModeSkipped));
        if (isModeSkipped)
        {
            // mode not possible. Skip this mode
            // Mode details are already logged in SetMode if isModeSkipped is set.
            continue;
        }

        CHECK_RC(RunInnerLoops(head,
                               display,
                               m_Tm,
                               index * m_FramesPerModeset,
                               ((index + 1) * m_FramesPerModeset),
                               baseSeed,
                               true, // Apply Frame Settings
                               &isOneLoopTested));

        m_LoopsSkippedInIMPDryRun.clear();

        MASSERT(isOneLoopTested);
        if (!isOneLoopTested)
            return RC::UNEXPECTED_TEST_COVERAGE;

        *pOneModeTested = true;
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Pick sub test mode
//!
//! \return OK if successful, not OK otherwise
RC TegraDispRandom::PickSubTestMode()
{
    m_Subtest = (*m_pPickers)[FPK_TEGWIN_SUBTEST].Pick();
    return OK;
}


//-----------------------------------------------------------------------------
//! \brief Query allowed pclk range
//!
//! \param connType   : Connection type
//! \param ope        : Output pixel encoding format
//! \param pPclkRange : Placeholder to return valid pclk range
//!
//! \return OK if successful, not OK otherwise
RC TegraDispRandom::GetPclkRange
(
    TegraDisplay::ConnectionType       connType,
    TegraDisplay::OutputPixelEncoding  ope,
    std::pair <UINT32, UINT32>         *pPclkRange
)
{
    RC rc;
    MASSERT(pPclkRange);
    if (pPclkRange == nullptr)
        return RC::SOFTWARE_ERROR;

    CHECK_RC(m_pDisplay->GetPclkRange(connType, ope, pPclkRange)); // in Mhz units

    if (m_MaxPclkMhz < pPclkRange->second)
    {
        // If user passes an artificial limit for max pclk, honor it
        pPclkRange->second = m_MaxPclkMhz;
    }
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Picks initial pixel clock for current mode
//!
//! \param connType  : Connection type
//!
//! \return OK if successful, not OK otherwise
RC TegraDispRandom::PickInitialPclk(TegraDisplay::ConnectionType connType)
{
    RC rc;
    std::pair <UINT32, UINT32> pclkRange;
    TegraDisplay::OutputPixelEncoding ope(m_Tm.mode.opeFormat, m_Tm.mode.opeYuvBpp);
    CHECK_RC(GetPclkRange(connType, ope, &pclkRange)); // in Mhz units

    UINT32 randPclk = m_pFpCtx->Rand.GetRandom(pclkRange.first, pclkRange.second);

    // To retain random sequence, first pick before making other decisions
    // If user passes a non-zero ForceInitialPclkMhz, use it. If Stressful mode
    // or Max pclk mode, use max pclk as initial pclk otherwise use random value
    m_SelectedPclkMhz =  m_ForceInitialPclkMhz ? m_ForceInitialPclkMhz :
                        ((m_StressLevel == StressLevel::STRESSFUL ||
                          m_Subtest == FPK_TEGWIN_SUBTEST_max_pclk) ?
                          pclkRange.second : randPclk);

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Run the test
//!
//! \return OK if successful, not OK otherwise
RC TegraDispRandom::Run()
{
    RC rc;
    bool  bUntestedDisplay = false;
    UINT32 displaysToTest = 0;
    UINT32 startFrame = m_pTestConfig->StartLoop();
    UINT32 endFrame = startFrame + m_pTestConfig->Loops();

    if (m_FramesPerModeset > m_pTestConfig->Loops())
    {
        Printf(Tee::PriWarn, "FramesPerModeset exceeds loop count in test config \n");
        m_FramesPerModeset = m_pTestConfig->Loops();
        Printf(Tee::PriWarn, "Adjusting FramesPerModeset = %u\n", m_FramesPerModeset);
    }

    if (startFrame % m_FramesPerModeset)
    {
        Printf(Tee::PriWarn, "StartLoop must be a multiple of FramesPerModeset\n");
        startFrame = (startFrame / m_FramesPerModeset) * m_FramesPerModeset;
    }
    if (endFrame % m_FramesPerModeset)
    {
        Printf(Tee::PriWarn, "StartLoop + Loops must be a multiple of FramesPerModeset\n");
        endFrame = ((endFrame / m_FramesPerModeset) + 1) * m_FramesPerModeset;
    }

    // Test every combination of Connector and head
    CHECK_RC(m_pDisplay->GetConnectors(&displaysToTest));
    displaysToTest &= ~NON_OR_DISPLAY;
    displaysToTest &= GetConnectorMask();

    while (displaysToTest)
    {
        UINT32 selectedDisplay = displaysToTest & ~(displaysToTest - 1);
        UINT32 possibleHeadMask = 0;
        UINT32 head = 0;

        displaysToTest ^= selectedDisplay;

        TegraDisplay::ConnectionType conType = TegraDisplay::CT_NUM_TYPES;
        bool bConnectionSkipped;
        CHECK_RC(GetConnectionType(m_pDisplay,
                                   selectedDisplay,
                                   &conType,
                                   NULL,
                                   &bConnectionSkipped));
        if (bConnectionSkipped)
        {
            VerbosePrintf("Skipping Display 0x%08x due to SkipConnectionType\n",
                           selectedDisplay);
            continue;
        }

        CHECK_RC(m_pDisplay->GetRoutableHeads(selectedDisplay, &possibleHeadMask));
        head = Utility::BitScanForward(possibleHeadMask);
        // In current design, each display to be associated with only one head
        MASSERT(!(possibleHeadMask ^ (1 << head)));

        if (IsHeadSkipped(head))
        {
            VerbosePrintf("Skipping Display 0x%08x since routable head is not part of HeadMask\n",
                         selectedDisplay);
            continue;
        }

        bool bOneModeTested = false;
        CHECK_RC(RunDisplayTest(head, selectedDisplay, conType,
                                startFrame, endFrame, &bOneModeTested));

        if (!bOneModeTested)
        {
            Printf(Tee::PriError,
                    "No Modes tested on Head %u (hw index %u) Display 0x%08x\n",
                     head, m_pDisplay->GetHeadHwIndex(head), selectedDisplay);
            if (m_pGolden->GetStopOnError())
                return RC::UNEXPECTED_TEST_COVERAGE;
            bUntestedDisplay = true;
        }
    }

    // Print the summary for test result
    PrintTestSummary(GetPrintCoverage() ? Tee::PriNormal : GetVerbosePrintPri());
    // Golden errors that are deferred due to "-run_on_error" can only be
    // retrieved by running Golden::ErrorRateTest().  This must be done before
    // clearing surfaces
    CHECK_RC(m_pGolden->ErrorRateTest(GetJSObject()));

    // TODO add more comprehensive coverage checks
    if (bUntestedDisplay)
        rc = RC::UNEXPECTED_TEST_COVERAGE;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Wrapper function to do a modeset
//!
//! \param head            : Head being tested
//! \param display         : display being tested
//! \param tm              : Test mode
//! \param pbModeSkipped   : placeholder to return if modeset has been skipped
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC TegraDispRandom::PerformModeSet
(
    UINT32 head,
    UINT32 display,
    TestMode tm,
    bool *pbModeSkipped
)
{
    RC rc;

    MASSERT(pbModeSkipped);
    if (pbModeSkipped == nullptr)
        return RC::SOFTWARE_ERROR;

    CHECK_RC(SetMode(m_pDisplay,
                     head,
                     display,
                     tm,
                     nullptr,
                     nullptr,
                     pbModeSkipped));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Signature used to identify CRC entries in golden value file
//!
//! \param head            : Head being tested
//! \param display         : Display being tested
//! \param tm              : Test mode
//! \param pOverrideSuffix : returned CRC signature
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC TegraDispRandom::GetCRCSuffix
(
    UINT32 head,
    UINT32 display,
    TestMode tm,
    string *pOverrideSuffix
)
{
    RC rc;
    string connStr;

    MASSERT(pOverrideSuffix);
    if (pOverrideSuffix == nullptr)
        return RC::SOFTWARE_ERROR;

    CHECK_RC(GetConnectionTypeString(display, tm.connection, &connStr));
    *pOverrideSuffix =
        Utility::StrPrintf("_%ux%u%s_%s_%s_on_%u",
                           tm.mode.width,
                           tm.mode.height,
                           tm.mode.bInterlaced ? "i" : "",
                           connStr.c_str(),
                           StringifyOPE(tm.mode.opeFormat, tm.mode.opeYuvBpp).c_str(),
                           m_pDisplay->GetHeadHwIndex(head));
    return rc;
}
//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
//! \return OK if successful, not OK otherwise
void TegraDispRandom::PrintJsProperties(Tee::Priority pri)
{
    TegraWinTest::PrintJsProperties(pri);
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tForceInitialPclkMhz:\t\t%u\n", m_ForceInitialPclkMhz);
    Printf(pri, "\tFramesPerModeset:\t\t%u\n", m_FramesPerModeset);
    Printf(pri, "\tMaxPclkMhz:\t\t\t%u\n", m_MaxPclkMhz);
}

JS_CLASS_INHERIT(TegraDispRandom, TegraWinTest,
                 "CheetAh Display engine test");
CLASS_PROP_READWRITE(TegraDispRandom, ForceInitialPclkMhz, UINT32,
                     "Initial pclk value to check against IMP before backoff is triggered");
CLASS_PROP_READWRITE(TegraDispRandom, FramesPerModeset, UINT32,
                     "Frames to test in each mode set");
CLASS_PROP_READWRITE(TegraDispRandom, MaxPclkMhz, UINT32,
                     "Max Pclk value to test in MHz"); // WAR to support FmaxDispVmin testing
