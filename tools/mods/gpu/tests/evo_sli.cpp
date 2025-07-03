/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Evo Display SLI test.
// Partially based on //arch/traces/non3d/iso/disp/class010X/multichip/disp_dr_syseng/test.js

#include "gputest.h"
#include "core/include/display.h"
#include "core/include/platform.h"
#include "core/include/version.h"
#include "core/include/fileholder.h"
#include "core/include/jscript.h"
#include "lwos.h"
#include "gpu/utility/surf2d.h"
#include "core/include/utility.h"
#include "gpu/include/displaycleanup.h"
#include "gpu/include/gpudev.h"
#include "gpu/display/evo_disp.h" // EvoRasterSettings
#include "ctrl/ctrl5070.h"

#define NUM_PATTERNS 8

class EvoSli : public GpuTest
{
    private:
        GpuTestConfiguration *  m_pTestConfig;

        // Stuff copied from m_pTestConfig for faster access.
        struct
        {
            UINT32 Width        = 0;
            UINT32 Height       = 0;
            UINT32 Depth        = 0;
            UINT32 RefreshRate  = 0;
        } m_Mode;
        FLOAT64 m_TimeoutMs;

        // Pixel clock in Hz
        UINT64 m_PixelClock;
        bool   m_AdjustPclk; //!< Attempt to auto adjust pclk downwards if the
                             //!< mode is not supported

        bool   m_TestConnectorA;
        bool   m_TestConnectorB;
        bool   m_IgnoreCRCMiscompares;
        bool   m_IgnoreCRCNotDifferent;
        bool   m_EnableFlipLock;
        bool   m_ForceRaster;
        bool   m_UseGreenBlueRandom;

        LwU32  m_OrigMasterSubdevice;

        enum { MasterSubdeviceOverrideNonActive = 0xFFFFFFFF };
        UINT32 m_MasterSubdeviceOverride;

        UINT32 m_MasterPatternOverride;
        UINT32 m_SlavePatternOverride;

        UINT32 m_ClkDlyOverride;
        UINT32 m_ClkPhsOverride;
        UINT32 m_DataDlyOverride;
        UINT32 m_DataDlyS0;
        UINT32 m_DataDlyS1;
        UINT32 m_DataDlyS2;
        UINT32 m_DataDlyS3;
        UINT32 m_SyncAdvanceOverride;
        UINT32 m_PadCtlOverride;
        UINT32 m_GenMode;
        bool   m_GenIlwertData;

        UINT32 m_FillColors[NUM_PATTERNS];
        UINT32 m_FillColorBit;
        UINT32 m_ToggleColorBit;
        vector<UINT32> m_UserColorPattern;

        typedef vector<Surface2D*> SurfacesVector;
        SurfacesVector m_Surfaces;

        UINT32 m_ColorFormat;

        UINT32 m_SleepMsAfterCRCMiscompare;
        UINT32 m_SleepMsAfterCRCGeneration;
        UINT32 m_SleepMsAfterCRCCheck;

        UINT32 m_DisplayOverride;
        UINT32 m_SkipDisplayMask;

        enum { PatternSetOverrideDisabled = 0xFFFFFFFF };
        UINT32 m_PatternSetOverride;
        //! Option to skip a fill pattern, this works independent of SetOverride
        //! Override Pattern set should not be set equal to SkipPatternIdx
        UINT32 m_SkipPatternIdx;

        UINT32 m_NumCRCCaptures;

        struct TestModeDescription
        {
            UINT32 PatternIdx[LW2080_MAX_SUBDEVICES];
            UINT32 FrameInterleaveSubdevice;
            Display::DistributedModeDescription DMD;
        };
        vector<TestModeDescription> m_TestModes;
        UINT32 m_ForceTypeOfInterleave;
        UINT32 m_PatternBitAndMask;
        UINT32 m_PatternBitOrMask;
        enum { IlwalidTestModeIdx = 0xFFFFFFFF };
        UINT32 m_SingleTestModeOverride;
        bool   m_DumpGoldenImages;
        bool   m_TestDualMio;

        UINT32 m_NumHeads;
        vector<UINT32> m_HeadDisplayIds;
        UINT32 m_ConnectorIdOfSimulatedDP;

        Tee::Priority m_VerbosePri;

        bool   m_PrintedOneTimePclkWarning;
        bool   m_UseDualSST;
        UINT32 m_DualSSTPair;

        //! Set the minimum pixel clock to use when auto adjusting the pixel
        //! clock due to mode not being supported in the current pstate
        static const UINT32 s_PixelClockMin = 100000000;

        // Initialize the test properties.
        RC InitProperties();

        // Run the random test.
        RC RunSliTest();
        RC RunSliTest
        (
            UINT32 MasterGPU,
            bool GenerateCRCs, // Verify on false
            vector<vector<UINT32> >& CRCValues
        );
        RC CheckCRCs
        (
            UINT32 TestModeIdx,
            UINT32 LoopIdx,
            UINT32 MasterGPU,
            bool GenerateCRCs,
            vector<vector<UINT32> >& CRCValues
        );

        RC SetMode(UINT32 MasterSubdevice, bool LwstomTimings);
        RC GetCRC(UINT32 DisplayId, UINT32 *Compositor, UINT32 *OutputResource);

        RC SetVideoBridgeOpMode(bool enableDualMio);

        RC SetDistributedMode(const Display::DistributedModeDescription *Mode);

        RC SetImages(UINT32 *Subdevices, Surface2D **Surfaces, UINT32 NumSurfaces);

        RC EnableLocks(bool FlipLock, bool RasterLock);

        RC CallwlateFillColors();
        RC PreparePattern();
        RC AllocateGoldenImage(Surface2D *pSurface);
        RC CopySurface2D
        (
            Surface2D *pSource,
            Surface2D *pDestination,
            UINT32 X, UINT32 Y,
            UINT32 Width, UINT32 Height
        );
        RC FillGoldenImage
        (
            UINT32 DeviceNum,
            const TestModeDescription *pTestMode,
            SurfacesVector &SourceSurfaces,
            Surface2D *pGoldenSurface
        );
        RC DumpGoldenImage(Surface2D *pGoldenSurface, UINT32 Index);
        RC FillUserPattern(Surface2D *pSurface);
        RC GenerateTestModes();

        RC CleanGpu();

        // Test a display configuration.  Called by Run().
        RC RunDisplay();
        RC SetDualSSTPair();

    public:
        EvoSli();

        RC Setup();
        // Run the test on all displays
        RC Run();
        RC Cleanup();

        virtual bool IsSupported();

        virtual bool GetTestsAllSubdevices() { return true; }

        SETGET_PROP(MasterSubdeviceOverride, UINT32);
        SETGET_PROP(PixelClock, UINT64);
        SETGET_PROP(AdjustPclk, bool);
        SETGET_PROP(TestConnectorA, bool);
        SETGET_PROP(TestConnectorB, bool);
        SETGET_PROP(IgnoreCRCMiscompares, bool);
        SETGET_PROP(IgnoreCRCNotDifferent, bool);
        SETGET_PROP(EnableFlipLock, bool);
        SETGET_PROP(ForceRaster, bool);
        SETGET_PROP(UseGreenBlueRandom, bool);
        SETGET_PROP(MasterPatternOverride, UINT32);
        SETGET_PROP(SlavePatternOverride, UINT32);
        SETGET_PROP(ClkDlyOverride, UINT32);
        SETGET_PROP(ClkPhsOverride, UINT32);
        SETGET_PROP(DataDlyOverride, UINT32);
        SETGET_PROP(DataDlyS0, UINT32);
        SETGET_PROP(DataDlyS1, UINT32);
        SETGET_PROP(DataDlyS2, UINT32);
        SETGET_PROP(DataDlyS3, UINT32);
        SETGET_PROP(PadCtlOverride, UINT32);
        SETGET_PROP(SyncAdvanceOverride, UINT32);
        SETGET_PROP(GenMode, UINT32);
        SETGET_PROP(GenIlwertData, bool);
        SETGET_PROP(FillColorBit, UINT32);
        SETGET_PROP(ToggleColorBit, UINT32);
        SETGET_PROP(ColorFormat, UINT32);
        SETGET_PROP(SleepMsAfterCRCMiscompare, UINT32);
        SETGET_PROP(SleepMsAfterCRCGeneration, UINT32);
        SETGET_PROP(SleepMsAfterCRCCheck, UINT32);
        SETGET_PROP(DisplayOverride, UINT32);
        SETGET_PROP(SkipDisplayMask, UINT32);
        SETGET_PROP(PatternSetOverride, UINT32);
        SETGET_PROP(SkipPatternIdx, UINT32);
        SETGET_PROP(NumCRCCaptures, UINT32);
        SETGET_PROP(ForceTypeOfInterleave, UINT32);
        SETGET_PROP(PatternBitAndMask, UINT32);
        SETGET_PROP(PatternBitOrMask, UINT32);
        SETGET_PROP(SingleTestModeOverride, UINT32);
        SETGET_PROP(DumpGoldenImages, bool);
        SETGET_PROP(UseDualSST, bool);
        SETGET_PROP(TestDualMio, bool);

        RC GetUserColorPattern(JsArray *jsa);
        RC SetUserColorPattern(JsArray *jsa);

        // Helper Functions
        //! \brief Helper function to clean all resources related to m_Surfaces.
        void CleanSurfaces();

        // Cleanup classes
        class SetImagesCleanup;
        class EnableLocksCleanup;

        friend class SetImagesCleanup;
        friend class EnableLocksCleanup;
};

//----------------------------------------------------------------------------
// Script interface.

JS_CLASS_INHERIT(EvoSli, GpuTest,
                 "Evo Display SLI test.");

CLASS_PROP_READWRITE(EvoSli, MasterSubdeviceOverride, UINT32,
                     "Force selected subdevice to be always the Master");

CLASS_PROP_READWRITE(EvoSli, PixelClock, UINT64,
                     "Pixel clock in Hz");

CLASS_PROP_READWRITE(EvoSli, AdjustPclk, bool,
                     "Auto adjust pixel clock when the mode is not supported");

CLASS_PROP_READWRITE(EvoSli, IgnoreCRCMiscompares, bool,
                     "Ignore CRC miscompares (for debugging)");

CLASS_PROP_READWRITE(EvoSli, IgnoreCRCNotDifferent, bool,
                     "Ignores check that CRCs are different between subsequent frames (permanent underflow detection)");

CLASS_PROP_READWRITE(EvoSli, EnableFlipLock, bool,
                     "Enables FlipLock testing (SWAP_READY line) - true by default");

CLASS_PROP_READWRITE(EvoSli, ForceRaster, bool,
                      "Use a custom, display independent, forced raster - true by default");

CLASS_PROP_READWRITE(EvoSli, UseGreenBlueRandom, bool,
                      "Use Green, Blue and Random surface content for alternative visual inspection (underflows are red)");

CLASS_PROP_READWRITE(EvoSli, MasterPatternOverride, UINT32,
                     "Index of master data pattern to be always used in test");

CLASS_PROP_READWRITE(EvoSli, SlavePatternOverride, UINT32,
                     "Index of slave data pattern to be always used in test");

CLASS_PROP_READWRITE(EvoSli, ClkDlyOverride, UINT32,
                     "ClkDly override value");

CLASS_PROP_READWRITE(EvoSli, ClkPhsOverride, UINT32,
                     "ClkPhs override value");

CLASS_PROP_READWRITE(EvoSli, DataDlyOverride, UINT32,
                     "DataDly override value");

CLASS_PROP_READWRITE(EvoSli, DataDlyS0, UINT32,
                     "DataDly override value in S0");
CLASS_PROP_READWRITE(EvoSli, DataDlyS1, UINT32,
                     "DataDly override value in S1");
CLASS_PROP_READWRITE(EvoSli, DataDlyS2, UINT32,
                     "DataDly override value in S2");
CLASS_PROP_READWRITE(EvoSli, DataDlyS3, UINT32,
                     "DataDly override value in S3");
CLASS_PROP_READWRITE(EvoSli, SyncAdvanceOverride, UINT32,
                     "SyncAdvance override value");

CLASS_PROP_READWRITE(EvoSli, PadCtlOverride, UINT32,
                     "PadCtl register override value");

CLASS_PROP_READWRITE(EvoSli, GenMode, UINT32,
                     "Select mode for the hardware pattern generator");

CLASS_PROP_READWRITE(EvoSli, GenIlwertData, bool,
                     "Ilwert output of the hardware pattern generator");

CLASS_PROP_READWRITE(EvoSli, FillColorBit, UINT32,
                     "Index of a bit to test used for data pattern color generation");

CLASS_PROP_READWRITE(EvoSli, ToggleColorBit, UINT32,
                     "Index of the bit to toggle in a clock cycle in data pattern color generation");

CLASS_PROP_READWRITE(EvoSli, ColorFormat, UINT32,
                     "Code of pixel format to use - default=A2B10G10R10");

CLASS_PROP_READWRITE(EvoSli, SleepMsAfterCRCMiscompare, UINT32,
                     "Sleep in ms after a CRC miscompare");

CLASS_PROP_READWRITE(EvoSli, SleepMsAfterCRCGeneration, UINT32,
                     "Sleep in ms after each frame used for CRC generation");

CLASS_PROP_READWRITE(EvoSli, SleepMsAfterCRCCheck, UINT32,
                     "Sleep in ms after each frame used for CRC check");

CLASS_PROP_READWRITE_JSARRAY(EvoSli, UserColorPattern, JsArray,
                             "Array of color values for User Data Pattern.");

CLASS_PROP_READWRITE(EvoSli, DisplayOverride, UINT32,
                     "Use specified display connector");

CLASS_PROP_READWRITE(EvoSli, SkipDisplayMask, UINT32,
                     "Do not run the test on specified display connectors");

CLASS_PROP_READWRITE(EvoSli, PatternSetOverride, UINT32,
                     "Run only specified PatternSet");

CLASS_PROP_READWRITE(EvoSli, SkipPatternIdx, UINT32,
                     "Skip a pattern index in range of all listed Patterns");

CLASS_PROP_READWRITE(EvoSli, NumCRCCaptures, UINT32,
                     "How many times CRC should be captured per PatternSet (min 1)");

CLASS_PROP_READWRITE(EvoSli, ForceTypeOfInterleave, UINT32,
        "Select either AFR(2) or SFR(1) only mode. Default state is disabled (0)");

CLASS_PROP_READWRITE(EvoSli, PatternBitAndMask, UINT32,
        "Pattern AND bitmask to clear some bits from fillcolor in all running Testmodes");

CLASS_PROP_READWRITE(EvoSli, PatternBitOrMask, UINT32,
        "Pattern OR bitmask to set some bits in fillcolor in all running Testmodes. This is applied after PatternBitAndMask");

CLASS_PROP_READWRITE(EvoSli, SingleTestModeOverride, UINT32,
        "Run a particular test mode for debugging only. Valid index lies in set of Generated test modes");

CLASS_PROP_READWRITE(EvoSli, DumpGoldenImages, bool,
                     "Write content of the golden images to text files");

CLASS_PROP_READWRITE(EvoSli, UseDualSST, bool,
                     "Use DualSST mode for doubling pixel rate");

CLASS_PROP_READWRITE(EvoSli, TestDualMio, bool, "Dual MIO enable");

PROP_CONST(EvoSli, PatternGeneratorLo,       Display::DistributedModeTrims::PatternLo);
PROP_CONST(EvoSli, PatternGeneratorTDat,     Display::DistributedModeTrims::PatternTDat);
PROP_CONST(EvoSli, PatternGeneratorRamp,     Display::DistributedModeTrims::PatternRamp);
PROP_CONST(EvoSli, PatternGeneratorWalk,     Display::DistributedModeTrims::PatternWalk);
PROP_CONST(EvoSli, PatternGeneratorMaxStep,  Display::DistributedModeTrims::PatternMaxStep);
PROP_CONST(EvoSli, PatternGeneratorMinStep,  Display::DistributedModeTrims::PatternMinStep);
PROP_CONST(EvoSli, PatternGeneratorDisabled, Display::DistributedModeTrims::PatternDisabled);

EvoSli::EvoSli()
: m_TimeoutMs(1000)
, m_PixelClock(0)
, m_AdjustPclk(false)
, m_TestConnectorA(true)
, m_TestConnectorB(true)
, m_IgnoreCRCMiscompares(false)
, m_IgnoreCRCNotDifferent(false)
, m_EnableFlipLock(true)
, m_ForceRaster(true)
, m_UseGreenBlueRandom(false)
, m_OrigMasterSubdevice(0)
, m_MasterSubdeviceOverride(MasterSubdeviceOverrideNonActive)
, m_MasterPatternOverride(NUM_PATTERNS)  // Invalid value - no override
, m_SlavePatternOverride(NUM_PATTERNS)   // Invalid value - no override
, m_ClkDlyOverride(Display::DistributedModeTrims::NotActiveValue)
, m_ClkPhsOverride(Display::DistributedModeTrims::NotActiveValue)
, m_DataDlyOverride(Display::DistributedModeTrims::NotActiveValue)
, m_DataDlyS0(Display::DistributedModeTrims::NotActiveValue)
, m_DataDlyS1(Display::DistributedModeTrims::NotActiveValue)
, m_DataDlyS2(Display::DistributedModeTrims::NotActiveValue)
, m_DataDlyS3(Display::DistributedModeTrims::NotActiveValue)
, m_SyncAdvanceOverride(Display::DistributedModeTrims::NotActiveValue)
, m_PadCtlOverride(Display::DistributedModeTrims::NotActiveValue)
, m_GenMode(Display::DistributedModeTrims::PatternDisabled)
, m_GenIlwertData(false)
, m_FillColorBit(0)
, m_ToggleColorBit(0)
, m_ColorFormat(ColorUtils::A2B10G10R10)
, m_SleepMsAfterCRCMiscompare(0)
, m_SleepMsAfterCRCGeneration(0)
, m_SleepMsAfterCRCCheck(0)
, m_DisplayOverride(0)
, m_SkipDisplayMask(0)
, m_PatternSetOverride(PatternSetOverrideDisabled)
, m_SkipPatternIdx(NUM_PATTERNS) // invalid value - no override
, m_NumCRCCaptures(100)
, m_ForceTypeOfInterleave(Display::DistributedModeDescription::Disabled)
, m_PatternBitAndMask(0xFFFFFFFF)
, m_PatternBitOrMask(0)
, m_SingleTestModeOverride(IlwalidTestModeIdx)
, m_DumpGoldenImages(false)
, m_TestDualMio(false)
, m_NumHeads(1)
, m_ConnectorIdOfSimulatedDP(0)
, m_VerbosePri(Tee::PriLow)
, m_PrintedOneTimePclkWarning(false)
, m_UseDualSST(false)
, m_DualSSTPair(0x0)
{
    SetName("EvoSli");
    m_pTestConfig = GetTestConfiguration();
}

RC EvoSli::Setup()
{
    RC rc;
    // Do not skip the pattern which is planned to be overridden
    if (
            m_SkipPatternIdx != NUM_PATTERNS
            && (m_SkipPatternIdx == m_SlavePatternOverride
            || m_SkipPatternIdx == m_MasterPatternOverride)
       )
    {
        Printf(Tee::PriHigh, "User has requested to force as well as remove pattern %u\n",
                m_SkipPatternIdx);
        return RC::BAD_COMMAND_LINE_ARGUMENT;

    }

    CHECK_RC(GpuTest::Setup());
    CHECK_RC(InitProperties());

    if (GetBoundGpuDevice()->GetNumSubdevices() < 2)
    {
        Printf(Tee::PriNormal,
            "EvoSli test requires GPUs to be linked - please use \"-sli\" argument.\n");
        return RC::SLI_ERROR;
    }

    // Reserve a Display object, disable UI, set default graphics mode.
    CHECK_RC(GpuTest::AllocDisplay());

    m_NumHeads = GetDisplay()->GetNumSLIHeads();
    if (m_NumHeads < 1)
    {
        Printf(Tee::PriNormal,
            "EvoSli test requires at least one head to be DR capable.\n");
        return RC::SLI_ERROR;
    }

    return rc;
}

RC EvoSli::Run()
{
    RC rc;
    Display *pDisplay = GetDisplay();
    UINT32 OrigDisplay = pDisplay->Selected();
    Display::ColorTranscodeMode OrigColorTranscoding =
                            pDisplay->GetColorTranscoding(pDisplay->Selected());
    if (OrigColorTranscoding == Display::IlwalidMode)
        return RC::SOFTWARE_ERROR;

    CHECK_RC(pDisplay->GetDisplaySubdeviceInstance(&m_OrigMasterSubdevice));
    bool OrigEnableDefaultImage = pDisplay->GetEnableDefaultImage();

    // Cleanup
    DisplayCleanup::SelectColorTranscodingCleanup
                        RestoreColorTranscoding(pDisplay,
                                                pDisplay->Selected(),
                                                OrigColorTranscoding);

    DisplayCleanup::SetModeCleanup
                RestoreMode(pDisplay, m_Mode.Width, m_Mode.Height, m_Mode.Depth, m_Mode.RefreshRate);

    Utility::CleanupOnReturnArg<Display, RC, bool>
                UnblockHotPlugEvents(pDisplay, &Display::SetBlockHotPlugEvents, false);
    CHECK_RC(pDisplay->SetBlockHotPlugEvents(true));

    Utility::CleanupOnReturnArg<Display, RC, UINT32>
                RestoreDisplay(pDisplay, &Display::Select, OrigDisplay);

    Utility::CleanupOnReturnArg<Display, RC, UINT32>
                RestoreDispSubdevice(pDisplay, &Display::SetMasterSubdevice, m_OrigMasterSubdevice);

    Utility::CleanupOnReturnArg<Display, RC, bool>
                RestoreEnableDefaultImage(pDisplay, &Display::SetEnableDefaultImage, OrigEnableDefaultImage);

    // In order to reach highest pclks disable the default image as it is making
    // IMP in RM believe that up scaling will be used (from the img 6x4 to the raster)
    // which requires higher dispclk to work. Since dispclk is at 540 MHz these days
    // pclk has to be much lower than that to satisfy IMP. When no scaling need is detected
    // pclk can reach dispclk.
    CHECK_RC(pDisplay->SetEnableDefaultImage(false));

    CHECK_RC(GenerateTestModes());

    CHECK_RC(RunDisplay());

    return rc;
}

RC EvoSli::Cleanup()
{
    RC rc, firstRc;

    FIRST_RC(GpuTest::Cleanup());

    return firstRc;
}

bool EvoSli::IsSupported()
{
    Display *pDisplay = GetDisplay();

    // Don't check for number of subdevices so that the test will fail
    // when "-mboard x" argument is not present

    if (pDisplay->GetDisplayClassFamily() != Display::EVO)
    {
        // Need an EVO display.
        return false;
    }

    return true;
}

RC EvoSli::InitProperties()
{
    m_Mode.Width      = m_pTestConfig->DisplayWidth();
    m_Mode.Height     = m_pTestConfig->DisplayHeight();
    m_Mode.Depth      = 32;
    m_Mode.RefreshRate= 60;
    m_TimeoutMs       = m_pTestConfig->TimeoutMs();

    if (m_pTestConfig->DisableCrt())
        Printf(Tee::PriLow, "NOTE: ignoring TestConfiguration.DisableCrt.\n");

    return OK;
}

RC EvoSli::GetUserColorPattern(JsArray *ptnArray)
{
    JavaScriptPtr pJavaScript;
    return pJavaScript->ToJsArray(m_UserColorPattern, ptnArray);
}

RC EvoSli::SetUserColorPattern(JsArray *ptnArray)
{
    JavaScriptPtr pJavaScript;
    return pJavaScript->FromJsArray(*ptnArray, &m_UserColorPattern);
}

RC EvoSli::FillUserPattern(Surface2D *pSurface)
{
    RC rc;
    UINT32 Width           = pSurface->GetWidth();
    UINT32 Height          = pSurface->GetHeight();
    UINT32 PatternLength   = (UINT32)m_UserColorPattern.size();
    UINT32 PatternColorIdx = 0;
    UINT32 NumSubdevices   = GetBoundGpuDevice()->GetNumSubdevices();

    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < NumSubdevices; SubdeviceIdx++)
    {
        CHECK_RC(pSurface->Map(SubdeviceIdx));

        for (UINT32 Y = 0; Y < Height; Y++)
        {
            for (UINT32 X = 0; X < Width; X++)
            {
                pSurface->WritePixel(X, Y, m_UserColorPattern[PatternColorIdx]);
                PatternColorIdx++;
                PatternColorIdx %= PatternLength;
            }
        }

        pSurface->Unmap();
    }

    return OK;
}

void EvoSli::CleanSurfaces()
{
    for (UINT32 SurfaceIdx = 0; SurfaceIdx < m_Surfaces.size(); SurfaceIdx++)
    {
        delete m_Surfaces[SurfaceIdx];
    }
    m_Surfaces.clear();
    CleanGpu();
}

RC EvoSli::RunDisplay()
{
    RC rc;
    vector<vector<UINT32> > CRCs;
    UINT32 NumSubdevices = GetBoundGpuDevice()->GetNumSubdevices();
    const Tee::Priority pri = GetVerbosePrintPri();

    // Cleanup
    Utility::CleanupOnReturn<EvoSli, void>
            CleanSurfs(this, &EvoSli::CleanSurfaces);

    CHECK_RC(CallwlateFillColors());
    CHECK_RC(PreparePattern());

    CHECK_RC(RunSliTest(m_OrigMasterSubdevice, true, CRCs));

    if (m_MasterSubdeviceOverride != MasterSubdeviceOverrideNonActive)
    {
        CHECK_RC(RunSliTest(m_MasterSubdeviceOverride, false, CRCs));
    }
    else
    {
        UINT32 NumSubdevicesTested = 0;

        for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < NumSubdevices; SubdeviceIdx++)
        {
            Display *pDisplay = GetDisplay();
            UINT32 Connectors = 0;

            RC rc_sms = pDisplay->SetMasterSubdevice(SubdeviceIdx);
            if (rc_sms == RC::MODE_NOT_SUPPORTED)
            {
                Printf(pri,
                       "EvoSli : Unable to set subdevice %d as master, skipping\n",
                       SubdeviceIdx);
                // This subdevice is not at the end of linear video bridge, skip it
                continue;
            }

            pDisplay->GetConnectors(&Connectors); // Ignore the error about no connectors
            if (Connectors == 0)
            {
                Printf(pri,
                       "EvoSli : No display output defined for subdevice %d, skipping\n",
                       SubdeviceIdx);
                continue; // We need a display output to run a test, this is probably one
                          // of the Gemini GPUs
            }

            RC rc_sub = RunSliTest(SubdeviceIdx, false, CRCs);
            if (rc_sub == RC::MODE_NOT_SUPPORTED)
            {
                // Hardware doesn't support monitor output on this subdevice
                Printf(pri,
                       "EvoSli : No monitor output on subdevice %d, skipping\n",
                       SubdeviceIdx);
                continue;
            }
            if (rc_sub != OK)
            {
                Printf(Tee::PriNormal, "EvoSli: Test failed on master %d.\n",
                    SubdeviceIdx);
            }
            CHECK_RC(rc_sub);
            NumSubdevicesTested++;
        }

        if (NumSubdevicesTested < 1)
        {
            Printf(Tee::PriNormal,
                "EvoSli error: No subdevices were tested\n");
            CHECK_RC(RC::MODE_NOT_SUPPORTED);
        }
    }

    return rc;
}

RC EvoSli::SetDualSSTPair()
{
    RC rc;

    UINT32 allDFPs = 0;
    Display *pDisplay = GetDisplay();
    CHECK_RC(pDisplay->GetAllDFPs(&allDFPs));
    while (allDFPs)
    {
        UINT32 remainingDFP = allDFPs & (allDFPs - 1);
        UINT32 thisDFP   = remainingDFP ^ allDFPs;
        allDFPs = remainingDFP;
        if (thisDFP == pDisplay->Selected())
        {
            continue;
        }
        Display::Encoder EncoderInfo;
        CHECK_RC(pDisplay->GetEncoder(thisDFP, &EncoderInfo));

        if (EncoderInfo.Signal == Display::Encoder::DP)
        {
            m_DualSSTPair = thisDFP;
            break;
        }
    }
    if (m_DualSSTPair == 0)
    {
        Printf(Tee::PriHigh, "DP display not found for DualSST pair.\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    return OK;
}

RC EvoSli::SetMode(UINT32 MasterSubdevice, bool LwstomTimings)
{
    const UINT64 MaxCRTPclk = 400000000;
    RC rc;

    Display *pDisplay = GetDisplay();

    EvoRasterSettings ers(m_Mode.Width+200,  101, m_Mode.Width+100+49, 149,
                          m_Mode.Height+150,  50, m_Mode.Height+50+50, 100,
                          1, 0,
                          0, // To be determined later
                          0, 0,
                          false);

    Display::DistributedModeTrims Trims =
    {
        m_ClkDlyOverride,
        m_ClkPhsOverride,
        {0},
        m_SyncAdvanceOverride,
        m_PadCtlOverride,
        (Display::DistributedModeTrims::PatternGenMode)m_GenMode,
        m_GenIlwertData,
    };
    if (m_DataDlyOverride != Display::DistributedModeTrims::NotActiveValue)
    {
        m_DataDlyS0 = m_DataDlyOverride;
        m_DataDlyS1 = m_DataDlyOverride;
        m_DataDlyS2 = m_DataDlyOverride;
        m_DataDlyS3 = m_DataDlyOverride;
    }

    Trims.DataDly[0] = m_DataDlyS0;
    Trims.DataDly[1] = m_DataDlyS1;
    Trims.DataDly[2] = m_DataDlyS2;
    Trims.DataDly[3] = m_DataDlyS3;

    m_HeadDisplayIds.clear();

    CHECK_RC(pDisplay->SetMasterSubdevice(MasterSubdevice));

    if (m_DisplayOverride != 0)
    {
        CHECK_RC(pDisplay->Select(m_DisplayOverride));

        UINT32 SelectedConnectors = m_DisplayOverride;
        while (true)
        {
            INT32 ConnectorIdx = Utility::BitScanForward(SelectedConnectors);
            if (ConnectorIdx == -1)
                break;
            SelectedConnectors ^= 1<<ConnectorIdx;
            m_HeadDisplayIds.push_back(1<<ConnectorIdx);
        }
    }
    else
    {
        UINT32 AvailConnectors = 0;
        CHECK_RC(pDisplay->GetConnectors(&AvailConnectors));

        // Find a connector that supports max pclk:
        UINT64 MaxPclkThusFar = 0;
        UINT32 MaxPclkConnectorThusFar = 0;
        UINT32 ConnectorsToCheck = AvailConnectors & ~m_SkipDisplayMask;
        while (ConnectorsToCheck)
        {
            UINT32 ConnectorToCheck = ConnectorsToCheck & ~(ConnectorsToCheck - 1);
            ConnectorsToCheck ^= ConnectorToCheck;

            switch (pDisplay->GetDisplayType(ConnectorToCheck))
            {
                case Display::CRT:
                {
                    if (MaxPclkThusFar < MaxCRTPclk)
                    {
                        MaxPclkThusFar = MaxCRTPclk;
                        MaxPclkConnectorThusFar = ConnectorToCheck;
                    }
                    break;
                }
                case Display::DFP:
                {
                    Display::Encoder EncoderInfo;
                    CHECK_RC(pDisplay->GetEncoder(ConnectorToCheck, &EncoderInfo));

                    if (EncoderInfo.Signal != Display::Encoder::DP)
                        continue; // Only DP among DFPs allows to use 10bpc

                    if (EncoderInfo.MaxLinkClockHz == 0)
                    {
                        // On older GPUs the capabilities notifier doesn't report
                        // the max values, start with max plck of DUAL TMDS:
                        EncoderInfo.MaxLinkClockHz = 330000000;
                    }
                    if (MaxPclkThusFar < EncoderInfo.MaxLinkClockHz)
                    {
                        MaxPclkThusFar = EncoderInfo.MaxLinkClockHz;
                        MaxPclkConnectorThusFar = ConnectorToCheck;
                    }
                    break;
                }
                default:
                    break;
            }
        }

        if (MaxPclkConnectorThusFar == 0)
        {
            Printf(Tee::PriNormal, "EvoSli: Error running on MasterSubdevice %d - "
                                   "not enough display connectors\n",
                                    MasterSubdevice);
            return RC::ILWALID_DISPLAY_TYPE;
        }

        m_HeadDisplayIds.push_back(MaxPclkConnectorThusFar);
        CHECK_RC(pDisplay->Select(MaxPclkConnectorThusFar));
    }

    if (m_ConnectorIdOfSimulatedDP == 0)
    {
        Display::Encoder EncoderInfo;
        UINT32 SelectedDisplayId = pDisplay->Selected();
        if (Utility::CountBits(SelectedDisplayId) != 1)
        {
            Printf(Tee::PriHigh, "Exactly one display should be "
                "selected at this point, but we have %u.\n",
                Utility::CountBits(SelectedDisplayId));
            return RC::ILWALID_DISPLAY_MASK;
        }

        CHECK_RC(pDisplay->GetEncoder(SelectedDisplayId, &EncoderInfo));

        if (EncoderInfo.Signal == Display::Encoder::DP)
        {
            if (m_UseDualSST)
            {
                CHECK_RC(SetDualSSTPair());
            }

            UINT32 SimulatedDisplayIDMask = 0;
            CHECK_RC(pDisplay->EnableSimulatedDPDevices(SelectedDisplayId,
                        m_UseDualSST? m_DualSSTPair : Display::NULL_DISPLAY_ID,
                        Display::DPMultistreamDisabled,
                        1,
                        nullptr,
                        0,
                        &SimulatedDisplayIDMask));

            m_ConnectorIdOfSimulatedDP = SelectedDisplayId;
            MASSERT(m_HeadDisplayIds.size() == 1);
            m_HeadDisplayIds.clear();
            m_HeadDisplayIds.push_back(SimulatedDisplayIDMask);
            CHECK_RC(pDisplay->Select(SimulatedDisplayIDMask));

            if (m_UseDualSST)
            {
                CHECK_RC(pDisplay->SetSingleHeadMultiStreamMode(
                         SelectedDisplayId, m_DualSSTPair,  Display::SHMultiStreamSST));
            }
        }
    }

    if (Display::DFP == pDisplay->GetDisplayType(pDisplay->Selected()))
    {
        Display::Encoder EncoderInfo;
        CHECK_RC(pDisplay->GetEncoder(pDisplay->Selected(), &EncoderInfo));
        switch (EncoderInfo.Signal)
        {
            case Display::Encoder::DP:
                // DP outputs need explicit BPP value. Override the default 24_444 used
                // with SetTimings as the test needs to cover all 15 data lines
                // in video bridge:
                ers.ORPixelDepthBpp = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_30_444;
                break;
            case Display::Encoder::TMDS:
                if (EncoderInfo.MaxLinkClockHz >= 340000000) // Is high pclk HDMI supported
                {
                    // 30_444 would be ideal for the video bridge, but it is not
                    // supported on TMDS, 36_444 also doesn't look to be supported,
                    // so just leave the default 8bpc. TMDS is no longer automatically
                    // selected anyway.
                    //ers.ORPixelDepthBpp = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_36_444;

                    // Dual link TMDS doesn't support BPC=12, only 8
                    ers.ForceSingleTMDSLink = true;
                }
                break;
            default:
                // Leave BPP at the default for others (it is 30 for CRT)
                break;
        }
    }

    if (m_ForceRaster)
    {
        bool bPossible;
        UINT64 StartPixelClock = m_PixelClock;
        if (StartPixelClock == 0)
        {
            switch (pDisplay->GetDisplayType(pDisplay->Selected()))
            {
                case Display::CRT:
                    StartPixelClock = MaxCRTPclk;
                    break;
                case Display::DFP:
                {
                    Display::Encoder EncoderInfo;
                    CHECK_RC(pDisplay->GetEncoder(pDisplay->Selected(), &EncoderInfo));
                    if (EncoderInfo.MaxLinkClockHz == 0)
                    {
                        // On older GPUs the capabilities notifier doesn't report
                        // the max values, start with max plck of DUAL TMDS:
                        EncoderInfo.MaxLinkClockHz = 330000000;
                    }

                    StartPixelClock = EncoderInfo.MaxLinkClockHz;
                    break;
                }
                default:
                    Printf(Tee::PriNormal, "EvoSli: Unknown connector type\n");
                    return RC::SOFTWARE_ERROR;
            }
        }

        pDisplay->DetachAllDisplays();

        // Pixel depth is required only for DP devices and is a no-op for other displays.
        // As LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_30_444 is used for DP in the test, we give depth 30.
        UINT32 impDepth = 0;
        if (Display::DFP == pDisplay->GetDisplayType(pDisplay->Selected()))
        {
            Display::Encoder EncoderInfo;
            CHECK_RC(pDisplay->GetEncoder(pDisplay->Selected(), &EncoderInfo));
            if (EncoderInfo.Signal == Display::Encoder::DP)
            {
                switch (ers.ORPixelDepthBpp)
                {
                    case LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_30_444:
                        impDepth = 30;
                        break;
                    default:
                        Printf(Tee::PriHigh, "Unexpected pixel depth for DP\n");
                        return RC::SOFTWARE_ERROR;
                }
            }
        }

        Display::IMPQueryInput impQueryInput = {pDisplay->Selected(),
                                           m_UseDualSST? m_DualSSTPair : Display::NULL_DISPLAY_ID,
                                           m_UseDualSST? Display::SHMultiStreamSST : Display::SHMultiStreamDisabled,
                                           ers,
                                           Display::FORCED_RASTER_ENABLED,
                                           impDepth};

        vector<Display::IMPQueryInput> impVec(1, impQueryInput);

        Display::IMPQueryInput &impInput = impVec[0];
        UINT64 nextPixelClockHz = StartPixelClock;
        const UINT64 pclkAdjust = 1000000;
        do
        {
            impInput.Timings.PixelClockHz = static_cast<UINT32>(nextPixelClockHz);
            CHECK_RC(pDisplay->IsModePossible(&impVec, &bPossible));
            if (bPossible &&
                (OK != GetBoundGpuSubdevice()->GetPerf()->
                SetRequiredDispClk(impInput.Timings.PixelClockHz, false)))
            {
                bPossible = false;
            }
            nextPixelClockHz -= pclkAdjust;
            if (nextPixelClockHz < s_PixelClockMin)
                nextPixelClockHz = s_PixelClockMin;
        } while (m_AdjustPclk && !bPossible && (impInput.Timings.PixelClockHz > s_PixelClockMin));

        if (!bPossible)
            return RC::MODE_IS_NOT_POSSIBLE;

        ers.PixelClockHz = impInput.Timings.PixelClockHz;
        if (ers.PixelClockHz != StartPixelClock)
        {
            Printf(Tee::PriHigh,
                   "%s : %llu Hz pixel clock is not possible on 0x%08x, using %d Hz\n",
                   GetName().c_str(),
                   StartPixelClock,
                   pDisplay->Selected(),
                   ers.PixelClockHz);
        }

        CHECK_RC(pDisplay->SetTimings(&ers));
    }

    for (UINT32 ConnectorIdx = 0; ConnectorIdx < m_HeadDisplayIds.size();
         ConnectorIdx++)
    {
        CHECK_RC(pDisplay->SetDistributedModeTrims(
            m_HeadDisplayIds[ConnectorIdx], &Trims));
    }

    // Disable underreplication (aka WidePipeCRC) in display pipe - bug 909110
    CHECK_RC(pDisplay->SelectColorTranscoding(pDisplay->Selected(),
                                              Display::ZeroFill));

    CHECK_RC(pDisplay->SetMode(m_Mode.Width,
                               m_Mode.Height,
                               m_Mode.Depth,
                               m_Mode.RefreshRate));

    if (!m_PrintedOneTimePclkWarning &&
        (ers.PixelClockHz > 535000000))
    {
        Printf(Tee::PriHigh,
               "Warning: EvoSli is using pclk=%d MHz, which is very high."
               " Consider using \"-sli_pixel_clock x\" to limit the"
               " video-bridge to POR frequency.\n",
                ers.PixelClockHz/1000000);
        m_PrintedOneTimePclkWarning = true;
    }

    return rc;
}

RC EvoSli::SetDistributedMode(const Display::DistributedModeDescription *Mode)
{
    RC rc;

    Display *pDisplay = GetDisplay();

    for (UINT32 ConnectorIdx = 0; ConnectorIdx < m_HeadDisplayIds.size();
         ConnectorIdx++)
    {
        CHECK_RC(pDisplay->SetDistributedMode(
            m_HeadDisplayIds[ConnectorIdx], Mode, true));
    }

    return rc;
}

RC EvoSli::GetCRC(UINT32 DisplayId, UINT32 *Compositor, UINT32 *OutputResource)
{
    RC rc;

    Display *pDisplay = GetDisplay();

    UINT32 DummyCRC;

    CHECK_RC(pDisplay->GetCrcValues(
        DisplayId,
        OutputResource,
        &DummyCRC,
        Compositor));

    return rc;
}

RC EvoSli::SetVideoBridgeOpMode(bool enableDualMio)
{
    //We are going disabling Dual mode until we get a method to get the current
    //videobridge mode from RM.
    Display *pDisplay = GetBoundGpuDevice()->GetDisplay();
    LW5070_CTRL_SET_PIOR_VIDEOBRIDGE_OP_MODE_PARAMS params;
    memset(&params, 0, sizeof(params));
    RC rc;

    for (UINT32 i = 0; i < GetBoundGpuDevice()->GetNumSubdevices(); i++)
    {
        params.base.subdeviceIndex = i;
        params.orNumber = 2; //*Always* only PIOR 2 is active. Confirmed this with CS team
        if (enableDualMio)
        {
            params.operationMode = LW5070_CTRL_CMD_SET_PIOR_VIDEOBRIDGE_OP_MODE_DUALMIO;
        }
        else
        {
            params.operationMode = LW5070_CTRL_CMD_SET_PIOR_VIDEOBRIDGE_OP_MODE_NONE;
        }
        CHECK_RC (pDisplay->RmControl5070(LW5070_CTRL_CMD_SET_PIOR_VIDEOBRIDGE_OP_MODE,
                    &params, sizeof(params)));
    }
    return rc;
}

static bool IsOverrideActive(UINT32 PatternIdx)
{
    return (PatternIdx < NUM_PATTERNS);
}

RC EvoSli::CallwlateFillColors()
{
    UINT32 Reds[NUM_PATTERNS];
    UINT32 Grns[NUM_PATTERNS];
    UINT32 Blus[NUM_PATTERNS];
    UINT32 Bit = m_FillColorBit;

    // All bits toggling, 1 bit low
    Reds[1] = (Bit < 10) ? 0x3FF ^ (1 << (Bit -  0)) : 0x3FF;
    Grns[1] = (Bit >  9) ? 0x01F ^ (1 << (Bit - 10)) : 0x01F;
    Blus[1] = 0x000;

    // All bits toggling, 1 bit high
    Reds[2] = 0x3FF;
    Grns[2] = (Bit <  5) ? 0x01F | (1 << (Bit +  5)) : 0x01F;
    Blus[2] = (Bit >  4) ? 0x000 | (1 << (Bit -  5)) : 0x000;

    // All bits toggling, 1 bit toggling opposite
    Reds[3] = (Bit < 10) ? 0x3FF ^ (1 << (Bit -  0)) : 0x3FF;
    Grns[3] = (Bit <  5) ? 0x01F | (1 << (Bit +  5)) : ((Bit >  9) ? 0x01F ^ (1 << (Bit - 10)) : 0x01F);
    Blus[3] = (Bit >  4) ? 0x000 | (1 << (Bit -  5)) : 0x000;

    // All bits toggling, alternating bits
    Reds[4] = 0x155;
    Grns[4] = 0x155;
    Blus[4] = 0x155;

    // All bits toggling, same direction as clock
    Reds[5] = 0x000;
    Grns[5] = 0x3E0;
    Blus[5] = 0x3FF;

    m_FillColors[0] = 0;
    m_FillColors[1] = 0x0 << 30 | Blus[1] << 20 | Grns[1] << 10 | Reds[1] << 0;
    m_FillColors[2] = 0x0 << 30 | Blus[2] << 20 | Grns[2] << 10 | Reds[2] << 0;
    m_FillColors[3] = 0x0 << 30 | Blus[3] << 20 | Grns[3] << 10 | Reds[3] << 0;
    m_FillColors[4] = 0x0 << 30 | Blus[4] << 20 | Grns[4] << 10 | Reds[4] << 0;
    m_FillColors[5] = 0x0 << 30 | Blus[5] << 20 | Grns[5] << 10 | Reds[5] << 0;

    // Single bit toggled patterns
    // 1 bit toggle in between 15 bits (10 reds + 5 grns) and next 15 bits (5 grns + 10 blus)
    if (m_ToggleColorBit> 29)
        m_ToggleColorBit= 0; // Reset because Bit 30 and 31 do not fall under any colour value
    m_FillColors[6] = 1 << m_ToggleColorBit;
    // In pattern 7, bit 30 and 31 are 0 and all other bits except the toggle bits are 1
    m_FillColors[7] = (~(1 << m_ToggleColorBit)) ^ (3 << 30);

    if (m_UseGreenBlueRandom)
    {
        m_FillColors[1] = (UINT32)ColorUtils::EncodePixel((ColorUtils::Format)m_ColorFormat, 0, 0xFFFFFFFF, 0, 0);
        m_FillColors[2] = (UINT32)ColorUtils::EncodePixel((ColorUtils::Format)m_ColorFormat, 0, 0, 0xFFFFFFFF, 0);
        m_FillColors[3] = (UINT32)ColorUtils::EncodePixel((ColorUtils::Format)m_ColorFormat, 0, 0xFFFFFFFF, 0, 0);
        m_FillColors[4] = (UINT32)ColorUtils::EncodePixel((ColorUtils::Format)m_ColorFormat, 0, 0, 0xFFFFFFFF, 0);
        m_FillColors[5] = (UINT32)ColorUtils::EncodePixel((ColorUtils::Format)m_ColorFormat, 0, 0xFFFFFFFF, 0, 0);
    }

    for (UINT32 ColorIdx = 1; ColorIdx < NUM_PATTERNS; ColorIdx++)
    {
        m_FillColors[ColorIdx] = ( m_FillColors[ColorIdx] & m_PatternBitAndMask) | m_PatternBitOrMask;
        Printf(Tee::PriLow, "EvoSli: FillColors[%d] = 0x%08x\n",
            ColorIdx, m_FillColors[ColorIdx]);
    }

    return OK;
}

RC EvoSli::PreparePattern()
{
    RC rc;

    GpuDevice *GpuDev = GetBoundGpuDevice();
    UINT32 NumSubdevices = GpuDev->GetNumSubdevices();

    Surface2D *pNewSurface = NULL;

    Surface2D WorkAround;
    WorkAround.SetWidth(m_Mode.Width);
    WorkAround.SetHeight(m_Mode.Height);
    WorkAround.SetColorFormat((ColorUtils::Format)m_ColorFormat);
    WorkAround.SetType(LWOS32_TYPE_PRIMARY);
    WorkAround.SetLocation(Memory::Optimal);
    WorkAround.SetProtect(Memory::Readable);
    WorkAround.SetAddressModel(Memory::Paging);
    WorkAround.SetDisplayable(true);

    CHECK_RC(WorkAround.Alloc(GpuDev));

    for (UINT32 PatternIdx = 0; PatternIdx < NUM_PATTERNS; PatternIdx++)
    {
        pNewSurface = new Surface2D;
        if (pNewSurface == NULL)
            CHECK_RC(RC::CANNOT_ALLOCATE_MEMORY);

        pNewSurface->SetWidth(m_Mode.Width);
        pNewSurface->SetHeight(m_Mode.Height);
        pNewSurface->SetColorFormat((ColorUtils::Format)m_ColorFormat);
        pNewSurface->SetType(LWOS32_TYPE_PRIMARY);
        pNewSurface->SetLocation(Memory::Optimal);
        pNewSurface->SetProtect(Memory::Readable);
        pNewSurface->SetAddressModel(Memory::Paging);
        pNewSurface->SetDisplayable(true);

        CHECK_RC(pNewSurface->Alloc(GpuDev));

        if (m_UserColorPattern.size() != 0)
            CHECK_RC(FillUserPattern(pNewSurface));
        else if (PatternIdx == 0)
        {
            for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < NumSubdevices; SubdeviceIdx++)
            {
                CHECK_RC(pNewSurface->FillPattern(1, 1, "random", "", "", SubdeviceIdx));
                if ((m_PatternBitAndMask != 0xFFFFFFFF) || (m_PatternBitOrMask))
                {
                    if (!pNewSurface->IsMapped())
                        CHECK_RC(pNewSurface->Map(SubdeviceIdx));
                    for (UINT32 x = 0; x < m_Mode.Width; x++)
                    {
                        for (UINT32 y = 0; y < m_Mode.Height; y++)
                        {
                            UINT32 val = pNewSurface->ReadPixel(x,y);
                            pNewSurface->WritePixel(x, y, (val & m_PatternBitAndMask) | m_PatternBitOrMask);
                            MASSERT(((val & m_PatternBitAndMask) | m_PatternBitOrMask) == pNewSurface->ReadPixel(x,y));
                        }
                    }
                    if (pNewSurface->IsMapped())
                        pNewSurface->Unmap();
                }
            }
        }
        else
        {
            for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < NumSubdevices; SubdeviceIdx++)
                CHECK_RC(pNewSurface->Fill(m_FillColors[PatternIdx], SubdeviceIdx));
        }

        m_Surfaces.push_back(pNewSurface);
    }

    return rc;
}

RC EvoSli::SetImages
(
    UINT32 *Subdevices,
    Surface2D **Surfaces,
    UINT32 NumSurfaces
)
{
    RC rc;

    Display *pDisplay = GetDisplay();

    for (UINT32 ConnectorIdx = 0; ConnectorIdx < m_HeadDisplayIds.size();
         ConnectorIdx++)
    {
        CHECK_RC(pDisplay->SetImages(m_HeadDisplayIds[ConnectorIdx], Surfaces,
            Subdevices, NumSurfaces));
    }

    return rc;
}

//! Implements cleanup class for SetImages.
class EvoSli::SetImagesCleanup : public CleanupBase
{
public:
    SetImagesCleanup(EvoSli *pThis, UINT32 *Subdevices, Surface2D **Surfaces, UINT32 NumSurfaces)
    :m_pThis(pThis), m_Subdevices(Subdevices), m_Surfaces(Surfaces), m_NumSurfaces(NumSurfaces)
    {
    }

    ~SetImagesCleanup()
    {
        if (IsActive() && m_pThis)
            m_pThis->SetImages(m_Subdevices, m_Surfaces, m_NumSurfaces);
    }

private:
    EvoSli *    m_pThis;
    UINT32 *    m_Subdevices;
    Surface2D **m_Surfaces;
    UINT32      m_NumSurfaces;
};

RC EvoSli::EnableLocks(bool FlipLock, bool RasterLock)
{
    RC rc;

    Display *pDisplay = GetDisplay();

    for (UINT32 ConnectorIdx = 0; ConnectorIdx < m_HeadDisplayIds.size();
         ConnectorIdx++)
    {
        CHECK_RC(pDisplay->EnableLocks(m_HeadDisplayIds[ConnectorIdx],
            FlipLock, RasterLock));
    }

    return rc;
}

//! Implements cleanup class for EnableLocks
class EvoSli::EnableLocksCleanup : public CleanupBase
{
public:
    EnableLocksCleanup(EvoSli *pThis, bool FlipLock, bool RasterLock)
    :m_pThis(pThis), m_FlipLock(FlipLock), m_RasterLock(RasterLock)
    {
    }

    ~EnableLocksCleanup()
    {
        if (IsActive() && m_pThis)
            m_pThis->EnableLocks(m_FlipLock, m_RasterLock);
    }

private:
    EvoSli *m_pThis;
    bool    m_FlipLock;
    bool    m_RasterLock;
};

RC EvoSli::CleanGpu()
{
    RC rc;

    Display *pDisplay = GetDisplay();

    for (UINT32 ConnectorIdx = 0; ConnectorIdx < m_HeadDisplayIds.size();
         ConnectorIdx++)
    {
        Printf(Tee::PriLow,
            "EvoSli: Real Pixel clock on connector 0x%08x = %d Hz.\n",
            m_HeadDisplayIds[ConnectorIdx],
            pDisplay->GetPixelClock(m_HeadDisplayIds[ConnectorIdx]));
    }

    SetDistributedMode(NULL);

    pDisplay->SetTimings(NULL);

    if (m_ConnectorIdOfSimulatedDP != 0)
    {
        pDisplay->DetachAllDisplays();
        pDisplay->DisableSimulatedDPDevices(m_ConnectorIdOfSimulatedDP,
            Display::NULL_DISPLAY_ID, Display::DetectRealDisplays);
        m_ConnectorIdOfSimulatedDP = 0;
    }

    return rc;
}

RC EvoSli::AllocateGoldenImage(Surface2D *pSurface)
{
    RC rc;

    pSurface->SetWidth(m_Mode.Width);
    pSurface->SetHeight(m_Mode.Height);
    pSurface->SetColorFormat((ColorUtils::Format)m_ColorFormat);
    pSurface->SetType(LWOS32_TYPE_PRIMARY);
    pSurface->SetLocation(Memory::Optimal);
    pSurface->SetProtect(Memory::Readable);
    pSurface->SetAddressModel(Memory::Paging);
    pSurface->SetDisplayable(true);

    CHECK_RC(pSurface->Alloc(GetBoundGpuDevice()));

    CHECK_RC(GetBoundGpuDevice()->GetDisplay()->BindToPrimaryChannels(pSurface));

    return rc;
}

RC EvoSli::CopySurface2D
(
    Surface2D *pSource,
    Surface2D *pDestination,
    UINT32 X, UINT32 Y,
    UINT32 Width, UINT32 Height
)
{
    MASSERT(pSource != NULL);
    MASSERT(pDestination != NULL);
    RC rc;

    // Cleanup
    Utility::CleanupOnReturn<Surface2D, void>
                UnmapDest(pDestination, &Surface2D::Unmap);
    Utility::CleanupOnReturn<Surface2D, void>
                UnmapSource(pSource, &Surface2D::Unmap);

    CHECK_RC(pSource->Map(m_OrigMasterSubdevice));
    CHECK_RC(pDestination->Map(m_OrigMasterSubdevice));

    for (UINT32 y = Y; y < Y + Height; y++)
        for (UINT32 x = X; x < X + Width; x++)
            pDestination->WritePixel(x, y, pSource->ReadPixel(x, y));

    return rc;
}

RC EvoSli::FillGoldenImage
(
    UINT32 DeviceNum,
    const TestModeDescription *pTestMode,
    SurfacesVector &SourceSurfaces,
    Surface2D *pGoldenSurface
)
{
    MASSERT(pTestMode != NULL);
    RC rc;

    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < LW2080_MAX_SUBDEVICES; SubdeviceIdx++)
    {
        switch (pTestMode->DMD.Mode[SubdeviceIdx])
        {
            case Display::DistributedModeDescription::FrameInterleave:
                if (pTestMode->FrameInterleaveSubdevice != SubdeviceIdx)
                    break;
                CHECK_RC(CopySurface2D(
                    SourceSurfaces[pTestMode->PatternIdx[SubdeviceIdx]],
                    pGoldenSurface,
                    0, 0,
                    pGoldenSurface->GetWidth(), pGoldenSurface->GetHeight()));
                break;
            case Display::DistributedModeDescription::StripInterleave:
                CHECK_RC(CopySurface2D(
                    SourceSurfaces[pTestMode->PatternIdx[SubdeviceIdx]],
                    pGoldenSurface,
                    0,
                    pTestMode->DMD.RenderLineStart[SubdeviceIdx],
                    pGoldenSurface->GetWidth(),
                    pTestMode->DMD.RenderLineStop[SubdeviceIdx]
                        - pTestMode->DMD.RenderLineStart[SubdeviceIdx]+1));
                break;
            default:
                break;
        }
    }

    return rc;
}

RC EvoSli::DumpGoldenImage(Surface2D *pGoldenSurface, UINT32 Index)
{
    if (!m_DumpGoldenImages)
        return OK;

    const UINT32 bytesPerPixel = pGoldenSurface->GetBytesPerPixel();

    if (bytesPerPixel != 4)
    {
        Printf(Tee::PriNormal,
            "EvoSli::DumpGoldenImage error - only 4 bytes per pixel surfaces are supported.\n");
        return RC::SOFTWARE_ERROR;
    }

    RC rc;

    Utility::CleanupOnReturn<Surface2D, void>
                UnmapSurface(pGoldenSurface, &Surface2D::Unmap);
    CHECK_RC(pGoldenSurface->Map(m_OrigMasterSubdevice));

    vector <UINT08> pitchBuffer;
    CHECK_RC(pGoldenSurface->CreatePitchImage(&pitchBuffer, 0));

    string textFileName = Utility::StrPrintf("TestMode%d.txt", Index);
    FileHolder textFile;
    CHECK_RC(textFile.Open(textFileName.c_str(), "wt"));

    Printf(Tee::PriNormal, "Dumping golden image to file %s.\n",
        textFileName.c_str());

    const UINT32 width = pGoldenSurface->GetWidth();
    const UINT32 height = pGoldenSurface->GetHeight();
    string colorFormat = FormatToString(pGoldenSurface->GetColorFormat());

    fprintf(textFile.GetFile(), "Width = %d, Height = %d, Color Format = %s\n",
        width, height, colorFormat.c_str());

    const UINT32 *pitchBuffer32 = (UINT32*)&pitchBuffer[0];

    for (UINT32 y = 0; y < height; y++)
    {
        for (UINT32 x = 0; x < width; x++)
        {
            UINT32 pixelData = *pitchBuffer32;
            fprintf(textFile.GetFile(), "0x%04x\n", pixelData & 0x7FFF);
            fprintf(textFile.GetFile(), "0x%04x\n", (pixelData >> 15) & 0x7FFF);
            pitchBuffer32++;
        }
        Tasker::Yield();
    }

    return OK;
}

RC EvoSli::RunSliTest
(
    UINT32 MasterGPU,
    bool GenerateCRCs,
    vector<vector<UINT32> >& CRCValues
)
{
    RC rc;

    UINT32 NumTestModes = (UINT32)m_TestModes.size();
    UINT32 NumLoops = m_pTestConfig->Loops();
    UINT32 NumSubdevices = GetBoundGpuDevice()->GetNumSubdevices();
    UINT32 AllSubdev = ((1 << NumSubdevices) - 1);
    Display *pDisplay = GetBoundGpuDevice()->GetDisplay();

    Surface2D GoldenImage;

    CHECK_RC(SetMode(MasterGPU, true));

    if (GenerateCRCs)
    {
        CRCValues.clear();
        CRCValues.resize(m_HeadDisplayIds.size());

        CHECK_RC(AllocateGoldenImage(&GoldenImage));

        NumLoops = 1;
    }
    else
    {
        for (UINT32 SurfaceIdx = 0; SurfaceIdx < m_Surfaces.size(); SurfaceIdx++)
        {
            CHECK_RC(pDisplay->BindToPrimaryChannels(m_Surfaces[SurfaceIdx]));
        }
    }

    // Cleanup
    // Make sure that the next SetMode will occur without the base channel
    // scanning out. Disabling images after setting SetMasterSubdevice
    // to a new value is causing troubles (asserts in RM).
    Surface2D* NullSurface[1] = {0};
    SetImagesCleanup ResetImages(this, &AllSubdev, NullSurface, 1);

    Utility::CleanupOnReturnArg<EvoSli, RC, const Display::DistributedModeDescription *>
                    ResetDistributedMode(this, &EvoSli::SetDistributedMode, NULL);

    Utility::CleanupOnReturnArg<EvoSli, RC, bool>
                    RestoreDualMio(this, &EvoSli::SetVideoBridgeOpMode, false);
    CHECK_RC(SetVideoBridgeOpMode(m_TestDualMio));

    EnableLocksCleanup DisengageFlipLock(this, false, true);
    if (GenerateCRCs || !m_EnableFlipLock)
        DisengageFlipLock.Cancel();

    if (!GenerateCRCs && m_EnableFlipLock)
    {
        CHECK_RC(SetDistributedMode(NULL));
        CHECK_RC(SetImages(&AllSubdev,
                           &m_Surfaces[m_TestModes[0].PatternIdx[0]],
                           1));
        CHECK_RC(EnableLocks(true, true)); // Engage Fliplock
    }

    for (UINT32 LoopIdx = 0; LoopIdx < NumLoops; LoopIdx++)
    {
        for (UINT32 TestModeIdx = 0; TestModeIdx < NumTestModes; TestModeIdx++)
        {
            if ((m_SingleTestModeOverride != IlwalidTestModeIdx) && (TestModeIdx != m_SingleTestModeOverride))
            {
                continue;
            }
            if ( (m_PatternSetOverride != PatternSetOverrideDisabled) &&
                 (TestModeIdx != m_PatternSetOverride) )
                continue;
            Printf(Tee::PriDebug, "Run Testmode [%u], Generate Crc %u\n", TestModeIdx, GenerateCRCs);

            if (GenerateCRCs)
            {
                CHECK_RC(FillGoldenImage(MasterGPU,
                                         &m_TestModes[TestModeIdx],
                                         m_Surfaces, &GoldenImage));
                CHECK_RC(SetDistributedMode(NULL));
                UINT32 Subdevices = 1<<MasterGPU;
                Surface2D *Images = &GoldenImage;
                CHECK_RC(SetImages(&Subdevices, &Images, 1));
                CHECK_RC(DumpGoldenImage(&GoldenImage, TestModeIdx));
            }
            else
            {
                UINT32 SubdevicesMasks[LW2080_MAX_SUBDEVICES];
                Surface2D *Surfaces[LW2080_MAX_SUBDEVICES];
                TestModeDescription &TestMode = m_TestModes[TestModeIdx];
                for (UINT32 SubdeviceIdx = 0;
                     SubdeviceIdx < NumSubdevices; SubdeviceIdx++)
                {
                    SubdevicesMasks[SubdeviceIdx] = 1<<SubdeviceIdx;
                    Surfaces[SubdeviceIdx] =
                        m_Surfaces[TestMode.PatternIdx[SubdeviceIdx]];

                    if ((TestMode.DMD.Mode[SubdeviceIdx] ==
                             Display::DistributedModeDescription::FrameInterleave) &&
                        (TestMode.FrameInterleaveSubdevice != SubdeviceIdx))
                    {
                        Surfaces[SubdeviceIdx] = NULL;
                    }
                }

                rc = SetImages(SubdevicesMasks, Surfaces, NumSubdevices);
                if (rc == RC::TIMEOUT_ERROR)
                {
                    Printf(Tee::PriNormal,
                        "EvoSli: SetImage timeout - likely a problem with the Swap Ready signal\n");
                }
                CHECK_RC(rc);

                CHECK_RC(SetDistributedMode(&TestMode.DMD));
            }

            CHECK_RC(CheckCRCs(TestModeIdx, LoopIdx, MasterGPU,
                     GenerateCRCs, CRCValues));
        }
    }

    if (m_ConnectorIdOfSimulatedDP != 0)
    {
        // The faked DP device has to be deleted here so it can be recreated
        // on the other subdevice:

        pDisplay->DetachAllDisplays();
        pDisplay->DisableSimulatedDPDevices(m_ConnectorIdOfSimulatedDP,
            m_UseDualSST? m_DualSSTPair : Display::NULL_DISPLAY_ID,
            Display::DetectRealDisplays);
        if (m_UseDualSST)
        {
            CHECK_RC(pDisplay->SetSingleHeadMultiStreamMode(
                        m_ConnectorIdOfSimulatedDP, 0,  Display::SHMultiStreamDisabled));
        }
        m_ConnectorIdOfSimulatedDP = 0;
    }

    return rc;
}

RC EvoSli::CheckCRCs
(
    UINT32 TestModeIdx,
    UINT32 LoopIdx,
    UINT32 MasterGPU,
    bool GenerateCRCs,
    vector<vector<UINT32> >& CRCValues
)
{
    RC rc;

    for (UINT32 ConnectorIdx = 0; ConnectorIdx < m_HeadDisplayIds.size();
         ConnectorIdx++)
    {
        UINT32 DisplayId = m_HeadDisplayIds[ConnectorIdx];

        UINT32 CompositorCRC = 0xbad1;
        UINT32 OutputResourceCRC = 0xbad2;

        if (GenerateCRCs)
        {
            CHECK_RC(GetCRC(DisplayId, &CompositorCRC, &OutputResourceCRC));

            if ( !IsOverrideActive(m_MasterPatternOverride) &&
                 !IsOverrideActive(m_SlavePatternOverride) &&
                 !IsOverrideActive(m_SkipPatternIdx) &&
                 (m_UserColorPattern.size() == 0) &&
                 (CRCValues[ConnectorIdx].size() > 0) )
            {
                if (CRCValues[ConnectorIdx][TestModeIdx-1] == OutputResourceCRC)
                {
                    Printf(Tee::PriNormal,
                        "EvoSli: Generated CRC is no different at test mode idx = %d\n",
                        TestModeIdx);
                    if (!m_IgnoreCRCNotDifferent)
                        return RC::SLI_ERROR;
                }
            }
            if (CRCValues[ConnectorIdx].size() < (TestModeIdx+1))
                CRCValues[ConnectorIdx].resize(TestModeIdx+1);
            CRCValues[ConnectorIdx][TestModeIdx] = OutputResourceCRC;

            if (m_SleepMsAfterCRCGeneration != 0)
                Tasker::Sleep(m_SleepMsAfterCRCGeneration);
        }
        else
        {
            if (m_NumCRCCaptures == 0) return RC::SOFTWARE_ERROR;

            UINT32 Miscompares = 0;
            for (UINT32 CaptureIdx = 0; CaptureIdx < m_NumCRCCaptures; CaptureIdx++)
            {
                CHECK_RC(GetCRC(DisplayId, &CompositorCRC, &OutputResourceCRC));
                if (OutputResourceCRC != CRCValues[ConnectorIdx][TestModeIdx])
                {
                    Printf(Tee::PriNormal,
                        "EvoSli: CRC capture %6d miscompare 0x%08x != Expected 0x%08x\n",
                        CaptureIdx+1, OutputResourceCRC, CRCValues[ConnectorIdx][TestModeIdx]);
                    Miscompares++;
                    if (m_SleepMsAfterCRCMiscompare != 0)
                        Tasker::Sleep(m_SleepMsAfterCRCMiscompare);
                }
            }
            if (m_SleepMsAfterCRCCheck != 0)
                Tasker::Sleep(m_SleepMsAfterCRCCheck);
            if (Miscompares > 0)
            {
                Printf(Tee::PriNormal,
                    "EvoSli: CRC capture mismatch on display 0x%08x in loop %d, PatternSet %2d, Master %d = %d/%d\n",
                    DisplayId, LoopIdx, TestModeIdx, MasterGPU, Miscompares, m_NumCRCCaptures);

                if (!m_IgnoreCRCMiscompares)
                    return RC::SLI_ERROR;
            }
        }
    }

    return rc;
}

RC EvoSli::GenerateTestModes()
{
    const UINT32 MaxSplitLine = m_Mode.Height - 1;
    const UINT32 TotalNumOfSubdevices = GetBoundGpuDevice()->GetNumSubdevices();
    RC rc;
    Random Random;
    UINT32 Pattern = 0;
    UINT32 ActiveSubdevices[LW2080_MAX_SUBDEVICES];
    UINT32 MasterSubdevice = 0; // For the purpose of pattern overrides

    if (m_MasterSubdeviceOverride != MasterSubdeviceOverrideNonActive)
        MasterSubdevice = m_MasterSubdeviceOverride;

    Random.SeedRandom(m_pTestConfig->Seed());
    m_TestModes.clear();

    do
    {
        for (UINT32 NumActiveSubdevices = 1;
             NumActiveSubdevices <= TotalNumOfSubdevices;
             NumActiveSubdevices++)
        {
            if (m_ForceTypeOfInterleave == Display::DistributedModeDescription::StripInterleave
                    && NumActiveSubdevices == 1)
                continue;  // Skip because StripInterleave needs more than 1 active subdevices
            if (m_ForceTypeOfInterleave == Display::DistributedModeDescription::FrameInterleave
                    && NumActiveSubdevices > 1)
                continue;  // Skip because AFR needs 1 active subdevice at a one time.

            for (UINT32 Index = 0; Index < NumActiveSubdevices; Index++)
                ActiveSubdevices[Index] = Index;

            bool NewCombination = false;
            do
            {
                UINT32 LwrrentSplitLine = 0;

                TestModeDescription NewTestMode;
                memset(&NewTestMode, 0, sizeof(NewTestMode));

                // Init FrameInterleaveSubdevice to an invalid value
                //   so when StripInterleave mode is used and some subdevices
                //   are left in FrameInterleave mode so that they don't display
                //   anything, they won't be regognized later as the the
                //   single subdevice that should display something
                NewTestMode.FrameInterleaveSubdevice = TotalNumOfSubdevices;

                for (UINT32 Index = 0; Index < TotalNumOfSubdevices; Index++)
                    NewTestMode.DMD.Mode[Index] =
                        Display::DistributedModeDescription::FrameInterleave;

                // ActiveSubdevices show a result now:
                for (UINT32 ResultIdx = 0; ResultIdx < NumActiveSubdevices;
                     ResultIdx++)
                {
                    UINT32 ActiveSubdevice = ActiveSubdevices[ResultIdx];
                    if (NumActiveSubdevices > 1)
                    {
                        NewTestMode.DMD.Mode[ActiveSubdevice] =
                            Display::DistributedModeDescription::StripInterleave;
                        NewTestMode.DMD.RenderLineStart[ActiveSubdevice] = LwrrentSplitLine;
                        if (ResultIdx == (NumActiveSubdevices - 1))
                        {
                            NewTestMode.DMD.RenderLineStop[ActiveSubdevice] = MaxSplitLine;
                        }
                        else
                        {
                            LwrrentSplitLine = Random.GetRandom(LwrrentSplitLine, MaxSplitLine);
                            NewTestMode.DMD.RenderLineStop[ActiveSubdevice] = LwrrentSplitLine;
                        }
                    }
                    else
                    {
                        NewTestMode.FrameInterleaveSubdevice = ActiveSubdevice;
                    }

                    NewTestMode.PatternIdx[ActiveSubdevice] = (Pattern == m_SkipPatternIdx)
                        ? (Pattern + 1) % NUM_PATTERNS : Pattern;
                    Pattern++;
                    Pattern %= NUM_PATTERNS;

                    if (IsOverrideActive(m_MasterPatternOverride) &&
                        (ActiveSubdevice == MasterSubdevice))
                    {
                        NewTestMode.PatternIdx[ActiveSubdevice] = m_MasterPatternOverride;
                    }
                    else if (IsOverrideActive(m_SlavePatternOverride) &&
                             (ActiveSubdevice != MasterSubdevice))
                    {
                        NewTestMode.PatternIdx[ActiveSubdevice] = m_SlavePatternOverride;
                    }
                }
                m_TestModes.push_back(NewTestMode);

                // Look for next combination:
                NewCombination = false;
                for (INT32 Index = NumActiveSubdevices - 1; Index >= 0; Index--)
                {
                    if (ActiveSubdevices[Index] <
                        (TotalNumOfSubdevices - (NumActiveSubdevices - Index)))
                    {
                        ActiveSubdevices[Index]++;

                        for (UINT32 RemainingIdx = Index + 1;
                             RemainingIdx < NumActiveSubdevices;
                             RemainingIdx++)
                        {
                            ActiveSubdevices[RemainingIdx] =
                                ActiveSubdevices[RemainingIdx-1] + 1;
                        }

                        NewCombination = true;
                        break;
                    }
                }
            } while (NewCombination);
        }
    } while (m_TestModes.size() < 10);

    Printf(Tee::PriLow, "Distributed test modes:\n");
    for (UINT32 ModeIdx = 0; ModeIdx < m_TestModes.size(); ModeIdx++)
    {
        TestModeDescription &TestMode = m_TestModes[ModeIdx];

        Printf(Tee::PriLow, "Mode%02d", ModeIdx);

        for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < TotalNumOfSubdevices;
             SubdeviceIdx++)
        {
            char PatternID = '0' + TestMode.PatternIdx[SubdeviceIdx];
            if ((TestMode.DMD.Mode[SubdeviceIdx] ==
                 Display::DistributedModeDescription::FrameInterleave) &&
                (TestMode.FrameInterleaveSubdevice != SubdeviceIdx))
            {
                PatternID = 'X';
            }

            Printf(Tee::PriLow, " %d:%s%c[%04d,%04d]",
                SubdeviceIdx,
                (TestMode.DMD.Mode[SubdeviceIdx] == Display::DistributedModeDescription::StripInterleave) ? "S" :
                (TestMode.DMD.Mode[SubdeviceIdx] == Display::DistributedModeDescription::FrameInterleave) ? "F" : "D",
                PatternID,
                TestMode.DMD.RenderLineStart[SubdeviceIdx],
                TestMode.DMD.RenderLineStop[SubdeviceIdx]);
        }
        Printf(Tee::PriLow, "\n");
    }

    return rc;
}
