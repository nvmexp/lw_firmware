/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2016,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Evo Display Overlay test.

// TODOs:
// - Export Fancy pickers indexes
// - Add Setup/Cleanup functions (to allow for conlwrrent testing)
// - Multi head testing
// - Surface offset testing
// - TimeStamps/Bufferflipping testing
// - Semaphore testing
// - Tweak the random parameters to get more visible overlays if desired

#include <stdio.h>
#include "gputest.h"
#include "core/include/golden.h"
#include "gpu/utility/surf2d.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "core/include/display.h"
#include "gpu/display/evo_disp.h"
#include "core/include/platform.h"
#include "core/include/jscript.h"
#include "gpu/js/fpk_comm.h"
#include "lwos.h"
#include "Lwcm.h"
#include "core/include/utility.h"
#include "class/cl5070.h"
#include "class/cl8270.h"
#include "class/cl8370.h"
#include "class/cl8570.h"
#include "class/cl8870.h"
#include "class/cl9070.h"
#include "class/cl9170.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/cl9770.h"
#include "class/cl9870.h"
#include "class/cl507e.h"

static const UINT32 Disable = 0xFFFFFFFF;

class EvoOvrl : public GpuTest
{
    private:
        GpuGoldenSurfaces *     m_pGGSurfs;
        GpuTestConfiguration *  m_pTestConfig;
        Goldelwalues *          m_pGolden;

        // Surface parameters.
        Surface2D m_PrimarySurface;
        Surface2D m_OverlayImage[2];

        // Stuff copied from m_TestConfig for faster access.
        struct
        {
            UINT32 Width;
            UINT32 Height;
            UINT32 Depth;
            UINT32 RefreshRate;
        } m_Mode;
        FLOAT64 m_TimeoutMs;

        // Display and head we are testing.
        UINT32 m_OverlayDisplay;

        // Subdevice we are lwrrently testing
        UINT32 m_Subdevice;

        bool     m_ContinueOnFrameError;
        StickyRC m_ContinuationRC;

        vector<UINT32> m_LUTValues;

        bool   m_AtLeastGF11x;
        bool   m_AtLeastGK10x;

        // Flag to run test in a reduced operation mode
        bool m_EnableReducedMode;

        FancyPickerArray * m_pPickers;
        FancyPicker::FpContext * m_pFpCtx;  // loop, frame, rand

        // All the random parameters we picked for this overlay (keep on hand for reporting).
        struct PicksOverlay
        {
            UINT32 SurfaceWidth;
            UINT32 SurfaceHeight;
            UINT32 SurfaceFormat;
            UINT32 SurfaceLayout;
            UINT32 PointInX;
            UINT32 PointInY;
            UINT32 SizeInWidth;
            UINT32 SizeInHeight;
            INT32  PointOutX;
            INT32  PointOutY;
            UINT32 SizeOutWidth;
            UINT32 CompositionControlMode;
            UINT64 KeyColor;
            UINT64 KeyMask;
            bool   EnableLUT;
            Display::LUTMode LUTMode;
            bool   EnableGainOffset;
            float  Gains[Display::NUM_RGB];
            float  Offsets[Display::NUM_RGB];
            bool   EnableGamutColorSpaceColwersion;
            float  ColorSpaceColwersionMatrix[Display::NUM_RGBC][Display::NUM_RGB];
            bool   EnableStereo;
        };
        PicksOverlay m_Picks;

        // Pick random overlay state.
        RC PickOverlay();

        // Allocate and fill overlay image based on PickOverlay() selections
        RC PrepareOverlayImage();

        // Allocate the graphic resources.
        RC AllocateResources();

        // Initialize the graphic resources.
        RC InitializeResources();

        // Free all the allocated graphic resources.
        RC FreeResources();

        // Initialize the test properties.
        RC InitProperties();

        // Set default values for our FancyPickers
        RC SetDefaultsForPicker(UINT32 idx);

        // Run the random test.
        RC LoopRandomMethods();

    public:
        EvoOvrl();

        RC Setup();
        // Run the test on all displays
        RC Run();
        RC Cleanup();

        virtual bool IsSupported();

        //! This test operates on all GPUs of an SLI device.
        virtual bool GetTestsAllSubdevices() { return true; }

        SETGET_PROP(ContinueOnFrameError, bool);
        SETGET_PROP(EnableReducedMode, bool);
};

//----------------------------------------------------------------------------
// Script interface.

JS_CLASS_INHERIT(EvoOvrl, GpuTest,
                 "Evo Display Overlay test.");
CLASS_PROP_READWRITE(EvoOvrl, ContinueOnFrameError, bool,
                     "bool: continue running if an error oclwrred in a frame");
CLASS_PROP_READWRITE(EvoOvrl, EnableReducedMode, bool,
                     "bool: enable reduced mode of operation");

EvoOvrl::EvoOvrl()
: m_Mode()
, m_TimeoutMs(1000)
, m_OverlayDisplay(0)
, m_Subdevice(0)
, m_ContinueOnFrameError(false)
, m_AtLeastGF11x(false)
, m_AtLeastGK10x(false)
, m_EnableReducedMode(false)
, m_Picks()
{
    SetName("EvoOvrl");
    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(FPK_EVOOVRL_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
    m_pGGSurfs = NULL;
    m_pTestConfig = GetTestConfiguration();
    m_pGolden = GetGoldelwalues();
}

bool EvoOvrl::IsSupported()
{
    Display *pDisplay = GetDisplay();

    if (pDisplay->GetDisplayClassFamily() != Display::EVO)
    {
        // Need an EVO display.
        return false;
    }

    return true;
}

RC EvoOvrl::SetDefaultsForPicker(UINT32 idx)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();

    UINT32 MaxSurfaceWidth  = m_Mode.Width/2;
    UINT32 MaxSurfaceHeight = m_Mode.Height/2;

    switch(idx)
    {
        case FPK_EVOOVRL_SURFACE_WIDTH:
            (*pPickers)[FPK_EVOOVRL_SURFACE_WIDTH].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_SURFACE_WIDTH].AddRandRange(1, 1, MaxSurfaceWidth);
            (*pPickers)[FPK_EVOOVRL_SURFACE_WIDTH].CompileRandom();
            break;

        case FPK_EVOOVRL_SURFACE_HEIGHT:
            (*pPickers)[FPK_EVOOVRL_SURFACE_HEIGHT].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_SURFACE_HEIGHT].AddRandRange(1, 1, MaxSurfaceHeight);
            (*pPickers)[FPK_EVOOVRL_SURFACE_HEIGHT].CompileRandom();
            break;

        case FPK_EVOOVRL_SURFACE_FORMAT:
            (*pPickers)[FPK_EVOOVRL_SURFACE_FORMAT].ConfigList(FancyPicker::WRAP);
            (*pPickers)[FPK_EVOOVRL_SURFACE_FORMAT].AddListItem(ColorUtils::A1R5G5B5);
            (*pPickers)[FPK_EVOOVRL_SURFACE_FORMAT].AddListItem(ColorUtils::A8R8G8B8);
            (*pPickers)[FPK_EVOOVRL_SURFACE_FORMAT].AddListItem(ColorUtils::VE8YO8UE8YE8);
            (*pPickers)[FPK_EVOOVRL_SURFACE_FORMAT].AddListItem(ColorUtils::YO8VE8YE8UE8);
            if (m_AtLeastGF11x)
            {
                (*pPickers)[FPK_EVOOVRL_SURFACE_FORMAT].AddListItem(ColorUtils::A2B10G10R10);
                (*pPickers)[FPK_EVOOVRL_SURFACE_FORMAT].AddListItem(ColorUtils::X2BL10GL10RL10_XRBIAS);
                (*pPickers)[FPK_EVOOVRL_SURFACE_FORMAT].AddListItem(ColorUtils::RF16_GF16_BF16_AF16);
                (*pPickers)[FPK_EVOOVRL_SURFACE_FORMAT].AddListItem(ColorUtils::R16_G16_B16_A16);
                (*pPickers)[FPK_EVOOVRL_SURFACE_FORMAT].AddListItem(ColorUtils::R16_G16_B16_A16_LWBIAS);
            }
            if (m_AtLeastGK10x)
            {
                (*pPickers)[FPK_EVOOVRL_SURFACE_FORMAT].AddListItem(ColorUtils::A2R10G10B10);
            }
            break;

        case FPK_EVOOVRL_SURFACE_LAYOUT:
            (*pPickers)[FPK_EVOOVRL_SURFACE_LAYOUT].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_SURFACE_LAYOUT].AddRandItem(1, Surface2D::BlockLinear);
            (*pPickers)[FPK_EVOOVRL_SURFACE_LAYOUT].AddRandItem(1, Surface2D::Pitch);
            (*pPickers)[FPK_EVOOVRL_SURFACE_LAYOUT].CompileRandom();
            break;

        case FPK_EVOOVRL_USE_WHOLE_SURFACE:
            (*pPickers)[FPK_EVOOVRL_USE_WHOLE_SURFACE].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_USE_WHOLE_SURFACE].AddRandItem(1, true);
            (*pPickers)[FPK_EVOOVRL_USE_WHOLE_SURFACE].AddRandItem(1, false);
            (*pPickers)[FPK_EVOOVRL_USE_WHOLE_SURFACE].CompileRandom();
            break;

        case FPK_EVOOVRL_SCALE_OUTPUT:
            (*pPickers)[FPK_EVOOVRL_SCALE_OUTPUT].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_SCALE_OUTPUT].AddRandItem(1, true);
            (*pPickers)[FPK_EVOOVRL_SCALE_OUTPUT].AddRandItem(1, false);
            (*pPickers)[FPK_EVOOVRL_SCALE_OUTPUT].CompileRandom();
            break;

        case FPK_EVOOVRL_OVERLAY_POSITION:
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POSITION].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POSITION].AddRandItem(4, FPK_EVOOVRL_OVERLAY_POSITION_ScreenRandom);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POSITION].AddRandItem(1, FPK_EVOOVRL_OVERLAY_POSITION_Top);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POSITION].AddRandItem(1, FPK_EVOOVRL_OVERLAY_POSITION_Bottom);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POSITION].AddRandItem(1, FPK_EVOOVRL_OVERLAY_POSITION_Left);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POSITION].AddRandItem(1, FPK_EVOOVRL_OVERLAY_POSITION_Right);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POSITION].AddRandItem(1, FPK_EVOOVRL_OVERLAY_POSITION_TopLeft);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POSITION].AddRandItem(1, FPK_EVOOVRL_OVERLAY_POSITION_TopRight);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POSITION].AddRandItem(1, FPK_EVOOVRL_OVERLAY_POSITION_BottomLeft);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POSITION].AddRandItem(1, FPK_EVOOVRL_OVERLAY_POSITION_BottomRight);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POSITION].CompileRandom();
            break;

        case FPK_EVOOVRL_OVERLAY_POINT_IN_X:
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POINT_IN_X].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POINT_IN_X].AddRandRange(1, 0, MaxSurfaceWidth);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POINT_IN_X].CompileRandom();
            break;

        case FPK_EVOOVRL_OVERLAY_POINT_IN_Y:
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POINT_IN_Y].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POINT_IN_Y].AddRandRange(1, 0, MaxSurfaceHeight);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_POINT_IN_Y].CompileRandom();
            break;

        case FPK_EVOOVRL_OVERLAY_SIZE_IN_WIDTH:
            (*pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_IN_WIDTH].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_IN_WIDTH].AddRandRange(1, 1, MaxSurfaceWidth);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_IN_WIDTH].CompileRandom();
            break;

        case FPK_EVOOVRL_OVERLAY_SIZE_IN_HEIGHT:
            (*pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_IN_HEIGHT].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_IN_HEIGHT].AddRandRange(1, 1, MaxSurfaceHeight);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_IN_HEIGHT].CompileRandom();
            break;

        case FPK_EVOOVRL_OVERLAY_SIZE_OUT_WIDTH:
            (*pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_OUT_WIDTH].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_OUT_WIDTH].AddRandRange(1, 1, m_Mode.Width);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_OUT_WIDTH].CompileRandom();
            break;

        case FPK_EVOOVRL_OVERLAY_COMPOSITION_CONTROL_MODE:
            (*pPickers)[FPK_EVOOVRL_OVERLAY_COMPOSITION_CONTROL_MODE].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_OVERLAY_COMPOSITION_CONTROL_MODE].AddRandItem(1, LW507E_SET_COMPOSITION_CONTROL_MODE_SOURCE_COLOR_VALUE_KEYING);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_COMPOSITION_CONTROL_MODE].AddRandItem(1, LW507E_SET_COMPOSITION_CONTROL_MODE_DESTINATION_COLOR_VALUE_KEYING);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_COMPOSITION_CONTROL_MODE].AddRandItem(1, LW507E_SET_COMPOSITION_CONTROL_MODE_OPAQUE_SUSPEND_BASE);
            (*pPickers)[FPK_EVOOVRL_OVERLAY_COMPOSITION_CONTROL_MODE].CompileRandom();
            break;

        case FPK_EVOOVRL_OVERLAY_KEY_COLOR:
            (*pPickers)[FPK_EVOOVRL_OVERLAY_KEY_COLOR].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_OVERLAY_KEY_COLOR].AddRandRange(1, 0, UINT32(-1));
            (*pPickers)[FPK_EVOOVRL_OVERLAY_KEY_COLOR].CompileRandom();
            break;

        case FPK_EVOOVRL_OVERLAY_KEY_MASK:
            (*pPickers)[FPK_EVOOVRL_OVERLAY_KEY_MASK].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_OVERLAY_KEY_MASK].AddRandRange(1, 0, UINT32(-1));
            (*pPickers)[FPK_EVOOVRL_OVERLAY_KEY_MASK].CompileRandom();
            break;

        case FPK_EVOOVRL_OVERLAY_LUT_MODE:
            (*pPickers)[FPK_EVOOVRL_OVERLAY_LUT_MODE].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_OVERLAY_LUT_MODE].AddRandItem(1, Disable);
            if (m_AtLeastGF11x)
            {
                (*pPickers)[FPK_EVOOVRL_OVERLAY_LUT_MODE].AddRandItem(1, Display::LUTMODE_LORES);
                (*pPickers)[FPK_EVOOVRL_OVERLAY_LUT_MODE].AddRandItem(1, Display::LUTMODE_HIRES);
                (*pPickers)[FPK_EVOOVRL_OVERLAY_LUT_MODE].AddRandItem(1, Display::LUTMODE_INDEX_1025_UNITY_RANGE);
                (*pPickers)[FPK_EVOOVRL_OVERLAY_LUT_MODE].AddRandItem(1, Display::LUTMODE_INTERPOLATE_1025_UNITY_RANGE);
                (*pPickers)[FPK_EVOOVRL_OVERLAY_LUT_MODE].AddRandItem(1, Display::LUTMODE_INTERPOLATE_1025_XRBIAS_RANGE);
                (*pPickers)[FPK_EVOOVRL_OVERLAY_LUT_MODE].AddRandItem(1, Display::LUTMODE_INTERPOLATE_1025_XVYCC_RANGE);
                (*pPickers)[FPK_EVOOVRL_OVERLAY_LUT_MODE].AddRandItem(1, Display::LUTMODE_INTERPOLATE_257_UNITY_RANGE);
                (*pPickers)[FPK_EVOOVRL_OVERLAY_LUT_MODE].AddRandItem(1, Display::LUTMODE_INTERPOLATE_257_LEGACY_RANGE);
            }
            (*pPickers)[FPK_EVOOVRL_OVERLAY_LUT_MODE].CompileRandom();
            break;

        case FPK_EVOOVRL_USE_GAIN_OFFSET:
            (*pPickers)[FPK_EVOOVRL_USE_GAIN_OFFSET].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_USE_GAIN_OFFSET].AddRandItem(1, false);
            if (m_AtLeastGF11x)
                (*pPickers)[FPK_EVOOVRL_USE_GAIN_OFFSET].AddRandItem(1, true);
            (*pPickers)[FPK_EVOOVRL_USE_GAIN_OFFSET].CompileRandom();
            break;

        case FPK_EVOOVRL_USE_GAMUT_COLOR_SPACE_COLWERSION:
            (*pPickers)[FPK_EVOOVRL_USE_GAMUT_COLOR_SPACE_COLWERSION].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_USE_GAMUT_COLOR_SPACE_COLWERSION].AddRandItem(1, false);
            if (m_AtLeastGF11x)
                (*pPickers)[FPK_EVOOVRL_USE_GAMUT_COLOR_SPACE_COLWERSION].AddRandItem(1, true);
            (*pPickers)[FPK_EVOOVRL_USE_GAMUT_COLOR_SPACE_COLWERSION].CompileRandom();
            break;

        case FPK_EVOOVRL_USE_STEREO:
            (*pPickers)[FPK_EVOOVRL_USE_STEREO].ConfigRandom();
            (*pPickers)[FPK_EVOOVRL_USE_STEREO].AddRandItem(1, false);
            if (m_AtLeastGK10x)
                (*pPickers)[FPK_EVOOVRL_USE_STEREO].AddRandItem(1, true);
            (*pPickers)[FPK_EVOOVRL_USE_STEREO].CompileRandom();
            break;

        default:
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//----------------------------------------------------------------------------
// Initialize the properties.

RC EvoOvrl::InitProperties()
{
    RC rc;

    m_Mode.Width       = m_pTestConfig->DisplayWidth();
    m_Mode.Height      = m_pTestConfig->DisplayHeight();
    m_Mode.Depth       = m_pTestConfig->DisplayDepth();
    m_Mode.RefreshRate = m_pTestConfig->RefreshRate();
    m_TimeoutMs        = m_pTestConfig->TimeoutMs();

    // Reset the default values for pickers that require TestConfiguration and
    // other dynamic data
    if (!(*m_pPickers)[FPK_EVOOVRL_SURFACE_WIDTH].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOOVRL_SURFACE_WIDTH));

    if (!(*m_pPickers)[FPK_EVOOVRL_SURFACE_HEIGHT].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOOVRL_SURFACE_HEIGHT));

    if (!(*m_pPickers)[FPK_EVOOVRL_SURFACE_FORMAT].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOOVRL_SURFACE_FORMAT));

    if (!(*m_pPickers)[FPK_EVOOVRL_OVERLAY_POINT_IN_X].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOOVRL_OVERLAY_POINT_IN_X));

    if (!(*m_pPickers)[FPK_EVOOVRL_OVERLAY_POINT_IN_Y].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOOVRL_OVERLAY_POINT_IN_Y));

    if (!(*m_pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_IN_WIDTH].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOOVRL_OVERLAY_SIZE_IN_WIDTH));

    if (!(*m_pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_IN_HEIGHT].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOOVRL_OVERLAY_SIZE_IN_HEIGHT));

    if (!(*m_pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_OUT_WIDTH].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOOVRL_OVERLAY_SIZE_OUT_WIDTH));

    if (!(*m_pPickers)[FPK_EVOOVRL_OVERLAY_LUT_MODE].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOOVRL_OVERLAY_LUT_MODE));

    if (!(*m_pPickers)[FPK_EVOOVRL_USE_GAIN_OFFSET].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOOVRL_USE_GAIN_OFFSET));

    if (!(*m_pPickers)[FPK_EVOOVRL_USE_GAMUT_COLOR_SPACE_COLWERSION].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOOVRL_USE_GAMUT_COLOR_SPACE_COLWERSION));

    if (!(*m_pPickers)[FPK_EVOOVRL_USE_STEREO].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOOVRL_USE_STEREO));

    if (m_pTestConfig->DisableCrt())
          Printf(Tee::PriLow, "NOTE: ignoring TestConfiguration.DisableCrt.\n");

    m_pFpCtx->Rand.SeedRandom(m_pTestConfig->Seed());
    m_LUTValues.clear();
    UINT32 NumWords =
        2 * EvoDisplay::LUTMode2NumEntries(Display::LUTMODE_INDEX_1025_UNITY_RANGE);
    for (UINT32 idx = 0; idx < NumWords; idx++)
        m_LUTValues.push_back(m_pFpCtx->Rand.GetRandom());

    return OK;
}

RC EvoOvrl::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());

    // Reserve a Display object, disable UI, set default graphics mode.
    CHECK_RC(GpuTest::AllocDisplay());

    UINT32 DisplayClass = GetDisplay()->GetClass();
    switch (DisplayClass)
    {
        case LW9870_DISPLAY:
        case LW9770_DISPLAY:
        case LW9570_DISPLAY:
        case LW9470_DISPLAY:
        case LW9270_DISPLAY:
        case LW9170_DISPLAY:
            m_AtLeastGK10x = true; // fall through
        case LW9070_DISPLAY:
            m_AtLeastGF11x = true; // fall through
            break;
        case GT214_DISPLAY:
        case G94_DISPLAY:
        case GT200_DISPLAY:
        case G82_DISPLAY:
        case LW50_DISPLAY:
            break;
        default:
            MASSERT(!"Class not recognized!");
            return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(InitProperties());

    return rc;
}

RC EvoOvrl::Run()
{
    RC rc;
    LwU32 OrgDisplaySubdeviceInstance;
    m_ContinuationRC.Clear();

    // Tell Golden values object where the screen buffer is located.
    // The only parameter that metters is m_OverlayDisplay, since CRCs are from Evo
    vector<GpuGoldenSurfaces> GGSurfsVec(1, GpuGoldenSurfaces(GetBoundGpuDevice()));
    GpuGoldenSurfaces *LwrrValue = GGSurfsVec.data();
    Display           *pDisplay  = GetDisplay();

    CHECK_RC(pDisplay->GetDisplaySubdeviceInstance(&OrgDisplaySubdeviceInstance));

    m_OverlayDisplay = pDisplay->Selected();

    // Cleanup
    Utility::CleanupOnReturnArg<Display, RC, UINT32>
                RestoreSubdevice(pDisplay, &Display::SetMasterSubdevice, OrgDisplaySubdeviceInstance);

    // Free golden buffer.
    m_pGGSurfs = NULL;
    Utility::TempValue<GpuGoldenSurfaces *>
                        MakeNull(m_pGGSurfs, LwrrValue);
    Utility::CleanupOnReturn<Goldelwalues, void>
                        ClearSurf(m_pGolden, &Goldelwalues::ClearSurfaces);

    // Free resources here as well, just in case an error above caused the
    // call to be skipped
    Utility::CleanupOnReturn<EvoOvrl, RC>
                        FreeRes(this, &EvoOvrl::FreeResources);
    Display::Encoder primaryEncoder;
    Display::DisplayType primaryType;
    CHECK_RC(pDisplay->GetEncoder(m_OverlayDisplay, &primaryEncoder));
    primaryType = pDisplay->GetDisplayType(m_OverlayDisplay);

    for (UINT32 SubdeviceIdx = 0;
        SubdeviceIdx < GetBoundGpuDevice()->GetNumSubdevices(); SubdeviceIdx++)
    {
        UINT32 loopsForLwrrentDisplayID = 1;
        if ((primaryEncoder.Signal == Display::Encoder::TMDS)
                && (primaryEncoder.Link == Display::Encoder::DUAL)
                && (primaryType == Display::DFP)
                && (m_pGolden->GetAction() == Goldelwalues::Store))
        {
            loopsForLwrrentDisplayID = 2;
        }
        m_Subdevice = SubdeviceIdx;

        for (UINT32 loopIdx = 0; loopIdx < loopsForLwrrentDisplayID; ++loopIdx)
        {
            CHECK_RC(pDisplay->SetMasterSubdevice(m_Subdevice));
            const bool bOriginalTMDSBStatus = pDisplay->GetPreferTMDSB(m_OverlayDisplay);
            if (loopsForLwrrentDisplayID > 1)
            {
                pDisplay->SetPreferTMDSB(m_OverlayDisplay, loopIdx != 0);
            }
            DEFER
            {
                if (loopsForLwrrentDisplayID > 1)
                {
                    pDisplay->SetPreferTMDSB(m_OverlayDisplay,
                                             bOriginalTMDSBStatus);
                }
            };
            CHECK_RC(pDisplay->SetMode( m_Mode.Width,m_Mode.Height, m_Mode.Depth, m_Mode.RefreshRate));

            CHECK_RC(pDisplay->SetDistributedMode(pDisplay->Selected(), NULL, true));

            CHECK_RC(AllocateResources());
            CHECK_RC(InitializeResources());

            bool bEdidFaked = false;
            m_pGGSurfs->ClearSurfaces();
            m_pGGSurfs->AttachSurface(&m_PrimarySurface, "C", m_OverlayDisplay);
            CHECK_RC(SetupGoldens(m_OverlayDisplay,
                        m_pGGSurfs,
                        m_pTestConfig->StartLoop() | (m_Subdevice << 24),
                        false,
                        &bEdidFaked));

            // If the EDID is faked, setmode was repeated and the primary surface
            // needs to be enabled again:
            if (bEdidFaked)
            {
                CHECK_RC(pDisplay->SetImage(&m_PrimarySurface));
            }

            Printf(Tee::PriLow, "Starting EvoOvrl test on display %08x.\n", m_OverlayDisplay);
            rc = LoopRandomMethods();
            Printf(Tee::PriLow, "Completed EvoOvrl test on display %08x.\n", m_OverlayDisplay);

            m_pGolden->ClearSurfaces();
            FreeResources();

            if (m_ContinueOnFrameError)
            {
                m_ContinuationRC = rc;
                rc.Clear();
            }
            else
            {
                CHECK_RC(rc);
            }

        }
    }
    if (rc == OK)
        rc = m_ContinuationRC;
    return rc;
}

RC EvoOvrl::Cleanup()
{
    RC rc, firstRc;

    FIRST_RC(GpuTest::Cleanup());

    return firstRc;
}

//----------------------------------------------------------------------------

RC EvoOvrl::PickOverlay()
{
    // Pick the random overlay state.

    m_Picks.SurfaceWidth  = (*m_pPickers)[FPK_EVOOVRL_SURFACE_WIDTH].Pick();
    m_Picks.SurfaceHeight = (*m_pPickers)[FPK_EVOOVRL_SURFACE_HEIGHT].Pick();
    m_Picks.SurfaceFormat = (*m_pPickers)[FPK_EVOOVRL_SURFACE_FORMAT].Pick();
    m_Picks.SurfaceLayout = (*m_pPickers)[FPK_EVOOVRL_SURFACE_LAYOUT].Pick();

    UINT32 MinimumSize;
    switch (m_Picks.SurfaceFormat)
    {
        case ColorUtils::A1R5G5B5:
        case ColorUtils::A8R8G8B8:
        case ColorUtils::A2B10G10R10:
        case ColorUtils::X2BL10GL10RL10_XRBIAS:
        case ColorUtils::RF16_GF16_BF16_AF16:
        case ColorUtils::R16_G16_B16_A16:
        case ColorUtils::R16_G16_B16_A16_LWBIAS:
        case ColorUtils::A2R10G10B10:
            MinimumSize = 1;
            break;
        case ColorUtils::VE8YO8UE8YE8:
        case ColorUtils::YO8VE8YE8UE8:
            MinimumSize = 2;
            m_Picks.SurfaceWidth  = (m_Picks.SurfaceWidth + 1) & ~1;
            m_Picks.SurfaceHeight = (m_Picks.SurfaceHeight + 1) & ~1;
            break;
        default:
            MASSERT(!"Surface format not supported!");
            return RC::SOFTWARE_ERROR;
    }

    bool UseWholeSurface = (*m_pPickers)[FPK_EVOOVRL_USE_WHOLE_SURFACE].Pick() != 0;
    if (UseWholeSurface)
    {
        m_Picks.PointInX     = 0;
        m_Picks.PointInY     = 0;
        m_Picks.SizeInWidth  = m_Picks.SurfaceWidth;
        m_Picks.SizeInHeight = m_Picks.SurfaceHeight;
    }
    else
    {
        m_Picks.PointInX  = (*m_pPickers)[FPK_EVOOVRL_OVERLAY_POINT_IN_X].Pick();
        m_Picks.PointInX %= m_Picks.SurfaceWidth;
        if (m_Picks.PointInX > (m_Picks.SurfaceWidth - MinimumSize))
            m_Picks.PointInX = m_Picks.SurfaceWidth - MinimumSize;
        m_Picks.PointInX &= ~(MinimumSize-1);
        m_Picks.PointInY  = (*m_pPickers)[FPK_EVOOVRL_OVERLAY_POINT_IN_Y].Pick();
        m_Picks.PointInY %= m_Picks.SurfaceHeight;
        if (m_Picks.PointInY > (m_Picks.SurfaceHeight - MinimumSize))
            m_Picks.PointInY = m_Picks.SurfaceHeight - MinimumSize;
        m_Picks.PointInY &= ~(MinimumSize-1);

        m_Picks.SizeInWidth  = (*m_pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_IN_WIDTH].Pick();
        if (m_Picks.SizeInWidth > (m_Picks.SurfaceWidth - m_Picks.PointInX))
            m_Picks.SizeInWidth = m_Picks.SurfaceWidth - m_Picks.PointInX;
        m_Picks.SizeInWidth &= ~(MinimumSize-1);
        if (m_Picks.SizeInWidth == 0)
            m_Picks.SizeInWidth = MinimumSize;
        m_Picks.SizeInHeight  = (*m_pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_IN_HEIGHT].Pick();
        if (m_Picks.SizeInHeight > (m_Picks.SurfaceHeight - m_Picks.PointInY))
            m_Picks.SizeInHeight = m_Picks.SurfaceHeight - m_Picks.PointInY;
        m_Picks.SizeInHeight &= ~(MinimumSize-1);
        if (m_Picks.SizeInHeight == 0)
            m_Picks.SizeInHeight = MinimumSize;
    }

    constexpr bool ScaleOutput = false; // (*m_pPickers)[FPK_EVOOVRL_SCALE_OUTPUT].Pick() != 0;
                                        // Scaling not supported in hardware
    if (ScaleOutput)
    {
        // Only horizontal upscaling allowed in G80
        m_Picks.SizeOutWidth = m_Picks.SizeInWidth + (*m_pPickers)[FPK_EVOOVRL_OVERLAY_SIZE_OUT_WIDTH].Pick();
    }
    else
    {
        m_Picks.SizeOutWidth = m_Picks.SizeInWidth;
    }

    UINT32 XMax    = m_Mode.Width - 1;
    UINT32 YMax    = m_Mode.Height - 1;
    UINT32 XOffset = m_Picks.SizeOutWidth - 1;
    UINT32 YOffset = m_Picks.SizeInHeight - 1;
    if (XOffset > XMax) XOffset = XMax;
    if (YOffset > YMax) YOffset = YMax;

    const UINT32 KeyColorRectWidth  = 10;
    const UINT32 KeyColorRectHeight = 10;
    INT32 KeyColorRectX = 0;
    INT32 KeyColorRectY = 0;

    switch ((*m_pPickers)[FPK_EVOOVRL_OVERLAY_POSITION].Pick())
    {
        case FPK_EVOOVRL_OVERLAY_POSITION_ScreenRandom:
            m_Picks.PointOutX = m_pFpCtx->Rand.GetRandom(0, XMax);
            m_Picks.PointOutY = m_pFpCtx->Rand.GetRandom(0, YMax);
            KeyColorRectX = m_Picks.PointOutX;
            KeyColorRectY = m_Picks.PointOutY;
            break;

        case FPK_EVOOVRL_OVERLAY_POSITION_Top:
            m_Picks.PointOutX = m_pFpCtx->Rand.GetRandom(0, XMax);
            m_Picks.PointOutY = -static_cast<INT32>(m_pFpCtx->Rand.GetRandom(0, YOffset));
            KeyColorRectX = m_Picks.PointOutX;
            KeyColorRectY = m_Picks.PointOutY + YOffset - KeyColorRectHeight;
            break;

        case FPK_EVOOVRL_OVERLAY_POSITION_Bottom:
            m_Picks.PointOutX = m_pFpCtx->Rand.GetRandom(0, XMax);
            m_Picks.PointOutY = m_pFpCtx->Rand.GetRandom(YMax - YOffset, YMax);
            KeyColorRectX = m_Picks.PointOutX;
            KeyColorRectY = m_Picks.PointOutY;
            break;

        case FPK_EVOOVRL_OVERLAY_POSITION_Left:
            m_Picks.PointOutX = -static_cast<INT32>(m_pFpCtx->Rand.GetRandom(0, XOffset));
            m_Picks.PointOutY = m_pFpCtx->Rand.GetRandom(0, YMax);
            KeyColorRectX = m_Picks.PointOutX + XOffset - KeyColorRectWidth;
            KeyColorRectY = m_Picks.PointOutY;
            break;

        case FPK_EVOOVRL_OVERLAY_POSITION_Right:
            m_Picks.PointOutX = m_pFpCtx->Rand.GetRandom(XMax - XOffset, XMax);
            m_Picks.PointOutY = m_pFpCtx->Rand.GetRandom(0, YMax);
            KeyColorRectX = m_Picks.PointOutX;
            KeyColorRectY = m_Picks.PointOutY;
            break;

        case FPK_EVOOVRL_OVERLAY_POSITION_TopLeft:
            m_Picks.PointOutX = -static_cast<INT32>(m_pFpCtx->Rand.GetRandom(0, XOffset));
            m_Picks.PointOutY = -static_cast<INT32>(m_pFpCtx->Rand.GetRandom(0, YOffset));
            KeyColorRectX = m_Picks.PointOutX + XOffset - KeyColorRectWidth;
            KeyColorRectY = m_Picks.PointOutY + YOffset - KeyColorRectHeight;
            break;

        case FPK_EVOOVRL_OVERLAY_POSITION_TopRight:
            m_Picks.PointOutX = m_pFpCtx->Rand.GetRandom(XMax - XOffset, XMax);
            m_Picks.PointOutY = -static_cast<INT32>(m_pFpCtx->Rand.GetRandom(0, YOffset));
            KeyColorRectX = m_Picks.PointOutX;
            KeyColorRectY = m_Picks.PointOutY + YOffset - KeyColorRectHeight;
            break;

        case FPK_EVOOVRL_OVERLAY_POSITION_BottomLeft:
            m_Picks.PointOutX = -static_cast<INT32>(m_pFpCtx->Rand.GetRandom(0, XOffset));
            m_Picks.PointOutY = m_pFpCtx->Rand.GetRandom(YMax - YOffset, YMax);
            KeyColorRectX = m_Picks.PointOutX + XOffset - KeyColorRectWidth;
            KeyColorRectY = m_Picks.PointOutY;
            break;

        case FPK_EVOOVRL_OVERLAY_POSITION_BottomRight:
            m_Picks.PointOutX = m_pFpCtx->Rand.GetRandom(XMax - XOffset, XMax);
            m_Picks.PointOutY = m_pFpCtx->Rand.GetRandom(YMax - YOffset, YMax);
            KeyColorRectX = m_Picks.PointOutX;
            KeyColorRectY = m_Picks.PointOutY;
            break;

        default:
            MASSERT(!"Overlay position mode not implemented!");
            m_Picks.PointOutX = m_Mode.Width / 2;
            m_Picks.PointOutY = m_Mode.Height / 2;
            KeyColorRectX = m_Picks.PointOutX;
            KeyColorRectY = m_Picks.PointOutY;
            return RC::SOFTWARE_ERROR;
    }

    if (ScaleOutput)
    {
        m_Picks.CompositionControlMode = LW507E_SET_COMPOSITION_CONTROL_MODE_DESTINATION_COLOR_VALUE_KEYING;
    }
    else
    {
        switch (m_Picks.SurfaceFormat)
        {
            case ColorUtils::VE8YO8UE8YE8:
            case ColorUtils::YO8VE8YE8UE8:
                m_Picks.CompositionControlMode = LW507E_SET_COMPOSITION_CONTROL_MODE_DESTINATION_COLOR_VALUE_KEYING;
                break;
            default:
                m_Picks.CompositionControlMode = (*m_pPickers)[FPK_EVOOVRL_OVERLAY_COMPOSITION_CONTROL_MODE].Pick();
                break;
        }
    }

    m_Picks.KeyColor = (*m_pPickers)[FPK_EVOOVRL_OVERLAY_KEY_COLOR].Pick();
    m_Picks.KeyColor |= UINT64((*m_pPickers)[FPK_EVOOVRL_OVERLAY_KEY_COLOR].Pick())<<32;
    // TODO: Generate random KeyMask value in a better way so that overlay is
    // visible more often then just for a simple random
    //m_Picks.KeyMask  = (*m_pPickers)[FPK_EVOOVRL_OVERLAY_KEY_MASK].Pick();
    m_Picks.KeyMask  = 0x7F7F7F7F7F7F7F7FLL;

    if (m_Picks.CompositionControlMode == LW507E_SET_COMPOSITION_CONTROL_MODE_DESTINATION_COLOR_VALUE_KEYING)
    {
        if (KeyColorRectX < 0)
            KeyColorRectX = 0;
        if ((KeyColorRectX + KeyColorRectWidth + 1) > m_Mode.Width)
            KeyColorRectX = m_Mode.Width - KeyColorRectWidth - 1;
        if (KeyColorRectY < 0)
            KeyColorRectY = 0;
        if ((KeyColorRectY + KeyColorRectHeight) > m_Mode.Height)
            KeyColorRectY = m_Mode.Height - KeyColorRectHeight;
        for (UINT32 LineIdx = KeyColorRectY;
             LineIdx < KeyColorRectY + KeyColorRectHeight;
             LineIdx++)
        {
            Memory::Fill64((UINT08*)m_PrimarySurface.GetAddress() +
                LineIdx*m_PrimarySurface.GetPitch() +
                KeyColorRectX*m_PrimarySurface.GetBytesPerPixel(),
                m_Picks.KeyColor, KeyColorRectWidth);
        }
    }

    if (m_Picks.CompositionControlMode == LW507E_SET_COMPOSITION_CONTROL_MODE_SOURCE_COLOR_VALUE_KEYING)
    {
        switch (m_Picks.SurfaceFormat)
        {
            case ColorUtils::A1R5G5B5:
                m_Picks.KeyColor &= 0xffffLL;
                m_Picks.KeyMask  &= 0xffffLL;
                break;
            case ColorUtils::A8R8G8B8:
                m_Picks.KeyColor &= 0x80ffffffLL;
                m_Picks.KeyMask  &= 0x80ffffffLL;
                break;
            case ColorUtils::A2B10G10R10:
            case ColorUtils::X2BL10GL10RL10_XRBIAS:
            case ColorUtils::A2R10G10B10:
                m_Picks.KeyColor &= 0xbfffffffLL;
                m_Picks.KeyMask  &= 0xbfffffffLL;
                break;
            case ColorUtils::RF16_GF16_BF16_AF16:
            case ColorUtils::R16_G16_B16_A16:
            case ColorUtils::R16_G16_B16_A16_LWBIAS:
                m_Picks.KeyColor &= 0x8000ffffffffffffLL;
                m_Picks.KeyMask  &= 0x8000ffffffffffffLL;
                break;
            default:
                MASSERT(!"SurfaceFormat not implemented!");
                return RC::SOFTWARE_ERROR;
        }
    }

    UINT32 LutModePicker = (*m_pPickers)[FPK_EVOOVRL_OVERLAY_LUT_MODE].Pick();
    m_Picks.EnableLUT = (LutModePicker != Disable);
    if (m_Picks.EnableLUT)
    {
        m_Picks.LUTMode = (Display::LUTMode)LutModePicker;
    }

    m_Picks.EnableGainOffset = (*m_pPickers)[FPK_EVOOVRL_USE_GAIN_OFFSET].Pick() != 0;
    switch (m_Picks.SurfaceFormat)
    {
        case ColorUtils::A1R5G5B5:
        case ColorUtils::A8R8G8B8:
        case ColorUtils::A2B10G10R10:
        case ColorUtils::X2BL10GL10RL10_XRBIAS:
        case ColorUtils::VE8YO8UE8YE8:
        case ColorUtils::YO8VE8YE8UE8:
        case ColorUtils::A2R10G10B10:
            m_Picks.EnableGainOffset = false;
            break;
        case ColorUtils::RF16_GF16_BF16_AF16:
        case ColorUtils::R16_G16_B16_A16:
        case ColorUtils::R16_G16_B16_A16_LWBIAS:
            break;
        default:
            MASSERT(!"SurfaceFormat not implemented!");
            return RC::SOFTWARE_ERROR;
    }

    if (m_Picks.EnableGainOffset)
    {
        m_Picks.Gains  [Display::R] = m_pFpCtx->Rand.GetRandomFloat(0.5, 4.0);
        m_Picks.Offsets[Display::R] = m_pFpCtx->Rand.GetRandomFloat(0.0, 4.0);
        m_Picks.Gains  [Display::G] = m_pFpCtx->Rand.GetRandomFloat(0.5, 4.0);
        m_Picks.Offsets[Display::G] = m_pFpCtx->Rand.GetRandomFloat(0.0, 4.0);
        m_Picks.Gains  [Display::B] = m_pFpCtx->Rand.GetRandomFloat(0.5, 4.0);
        m_Picks.Offsets[Display::B] = m_pFpCtx->Rand.GetRandomFloat(0.0, 4.0);
    }

    m_Picks.EnableGamutColorSpaceColwersion = (*m_pPickers)[FPK_EVOOVRL_USE_GAMUT_COLOR_SPACE_COLWERSION].Pick() != 0;
    if (m_Picks.EnableGamutColorSpaceColwersion)
    {
        m_Picks.ColorSpaceColwersionMatrix[Display::R][Display::R] = m_pFpCtx->Rand.GetRandomFloat(-4.0, 3.999);
        m_Picks.ColorSpaceColwersionMatrix[Display::G][Display::R] = m_pFpCtx->Rand.GetRandomFloat(-4.0, 3.999);
        m_Picks.ColorSpaceColwersionMatrix[Display::B][Display::R] = m_pFpCtx->Rand.GetRandomFloat(-4.0, 3.999);
        m_Picks.ColorSpaceColwersionMatrix[Display::C][Display::R] = m_pFpCtx->Rand.GetRandomFloat(-4.0, 3.999);
        m_Picks.ColorSpaceColwersionMatrix[Display::R][Display::G] = m_pFpCtx->Rand.GetRandomFloat(-4.0, 3.999);
        m_Picks.ColorSpaceColwersionMatrix[Display::G][Display::G] = m_pFpCtx->Rand.GetRandomFloat(-4.0, 3.999);
        m_Picks.ColorSpaceColwersionMatrix[Display::B][Display::G] = m_pFpCtx->Rand.GetRandomFloat(-4.0, 3.999);
        m_Picks.ColorSpaceColwersionMatrix[Display::C][Display::G] = m_pFpCtx->Rand.GetRandomFloat(-4.0, 3.999);
        m_Picks.ColorSpaceColwersionMatrix[Display::R][Display::B] = m_pFpCtx->Rand.GetRandomFloat(-4.0, 3.999);
        m_Picks.ColorSpaceColwersionMatrix[Display::G][Display::B] = m_pFpCtx->Rand.GetRandomFloat(-4.0, 3.999);
        m_Picks.ColorSpaceColwersionMatrix[Display::B][Display::B] = m_pFpCtx->Rand.GetRandomFloat(-4.0, 3.999);
        m_Picks.ColorSpaceColwersionMatrix[Display::C][Display::B] = m_pFpCtx->Rand.GetRandomFloat(-4.0, 3.999);
    }

    m_Picks.EnableStereo = (*m_pPickers)[FPK_EVOOVRL_USE_STEREO].Pick() != 0;

    return OK;
}

//----------------------------------------------------------------------------

RC EvoOvrl::PrepareOverlayImage()
{
    RC rc = OK;

    UINT32 NumImages = m_Picks.EnableStereo ? 2 : 1;
    for (UINT32 ImageIdx = 0; ImageIdx < NumImages; ImageIdx++)
    {
        m_OverlayImage[ImageIdx].Free();

        m_OverlayImage[ImageIdx].SetWidth(m_Picks.SurfaceWidth);
        m_OverlayImage[ImageIdx].SetHeight(m_Picks.SurfaceHeight);
        m_OverlayImage[ImageIdx].SetType(LWOS32_TYPE_VIDEO);
        m_OverlayImage[ImageIdx].SetColorFormat((ColorUtils::Format)m_Picks.SurfaceFormat);
        m_OverlayImage[ImageIdx].SetLocation(Memory::Optimal);
        m_OverlayImage[ImageIdx].SetProtect(Memory::ReadWrite);
        m_OverlayImage[ImageIdx].SetDisplayable(true);
        m_OverlayImage[ImageIdx].SetLayout((Surface2D::Layout)m_Picks.SurfaceLayout);
        CHECK_RC(m_OverlayImage[ImageIdx].Alloc(GetBoundGpuDevice()));

        m_OverlayImage[ImageIdx].FillPattern(1, 1, "random",
            ImageIdx ? "seed=1" : "seed=0", NULL, m_Subdevice);

        LwRm::Handle hOverlayChan;
        if (GetDisplay()->
                GetEvoDisplay()->GetOverlayChannelHandle(m_OverlayDisplay, &hOverlayChan) == OK)
        {
            CHECK_RC(LwRmPtr()->BindContextDma(hOverlayChan, m_OverlayImage[ImageIdx].GetCtxDmaHandleIso()));
        }
    }

    return rc;
}

//----------------------------------------------------------------------------

RC EvoOvrl::LoopRandomMethods()
{
    RC rc;
    Display *pDisplay = GetDisplay();

    UINT32 StartLoop        = m_pTestConfig->StartLoop();
    UINT32 RestartSkipCount = m_pTestConfig->RestartSkipCount();
    UINT32 Loops            = (m_EnableReducedMode ? 1 : m_pTestConfig->Loops());
    UINT32 Seed             = m_pTestConfig->Seed();
    UINT32 EndLoop          = StartLoop + Loops;

    if ((StartLoop % RestartSkipCount) != 0)
    {
        Printf(Tee::PriNormal, "WARNING: StartLoop is not a restart point.\n");
    }

    // Encode the subdevice into the top 8 bits of the GV loop number so
    // that we can distinguish between the CRCs of different subdevices.
    m_pGolden->SetLoop(StartLoop | (m_Subdevice << 24));

    for (m_pFpCtx->LoopNum = StartLoop; m_pFpCtx->LoopNum < EndLoop; ++m_pFpCtx->LoopNum)
    {
        if ((m_pFpCtx->LoopNum == StartLoop) || ((m_pFpCtx->LoopNum % RestartSkipCount) == 0))
        {
            //-----------------------------------------------------------------
            // Restart point.

            Printf(Tee::PriLow,
                   "\n\tRestart: loop %d, seed %#010x\n", m_pFpCtx->LoopNum, Seed + m_pFpCtx->LoopNum);

            m_pFpCtx->RestartNum = m_pFpCtx->LoopNum / RestartSkipCount;
            m_pFpCtx->Rand.SeedRandom(Seed + m_pFpCtx->LoopNum);

            m_pPickers->Restart();
        }

        // Pick the random overlay state.
        CHECK_RC(PickOverlay());

        const char *LutModeDesc = "disabled";
        if (m_Picks.EnableLUT)
        {
            LutModeDesc = EvoDisplay::LUTMode2Name(m_Picks.LUTMode);
        }

        string GainOffsetDesc = "disabled";
        if (m_Picks.EnableGainOffset)
        {
            GainOffsetDesc = Utility::StrPrintf("RG=%.2f,RO=%.2f,GG=%.2f,GO=%.2f,BG=%.2f,BO=%.2f",
                m_Picks.Gains[Display::R], m_Picks.Offsets[Display::R],
                m_Picks.Gains[Display::G], m_Picks.Offsets[Display::G],
                m_Picks.Gains[Display::B], m_Picks.Offsets[Display::B]);
        }

        string GamutCSCDesc = "disabled";
        if (m_Picks.EnableGamutColorSpaceColwersion)
        {
            GamutCSCDesc = Utility::StrPrintf("RR=%.2f,GR=%.2f,BR=%.2f,CR=%.2f,"
                                              "RG=%.2f,GG=%.2f,BG=%.2f,CG=%.2f,"
                                              "RB=%.2f,GB=%.2f,BB=%.2f,CB=%.2f",
                m_Picks.ColorSpaceColwersionMatrix[Display::R][Display::R],
                m_Picks.ColorSpaceColwersionMatrix[Display::G][Display::R],
                m_Picks.ColorSpaceColwersionMatrix[Display::B][Display::R],
                m_Picks.ColorSpaceColwersionMatrix[Display::C][Display::R],
                m_Picks.ColorSpaceColwersionMatrix[Display::R][Display::G],
                m_Picks.ColorSpaceColwersionMatrix[Display::G][Display::G],
                m_Picks.ColorSpaceColwersionMatrix[Display::B][Display::G],
                m_Picks.ColorSpaceColwersionMatrix[Display::C][Display::G],
                m_Picks.ColorSpaceColwersionMatrix[Display::R][Display::B],
                m_Picks.ColorSpaceColwersionMatrix[Display::G][Display::B],
                m_Picks.ColorSpaceColwersionMatrix[Display::B][Display::B],
                m_Picks.ColorSpaceColwersionMatrix[Display::C][Display::B]);
        }

        Printf(Tee::PriLow,
            "   Loop:%u Format:%s Layout:%s X:%i Y:%i SurfW:%u SurfH:%u InX:%u InY:%u OvW:%u OvH:%u "
            "Composition:%u KeyColor:0x%08llx KeyMask:0x%08llx LUTMode:%s GainOffset:%s GamutCSC:%s "
            "Stereo:%s\n",
               m_pFpCtx->LoopNum,
               ColorUtils::FormatToString((ColorUtils::Format)m_Picks.SurfaceFormat).c_str(),
               m_OverlayImage[0].GetLayoutStr((Surface2D::Layout)m_Picks.SurfaceLayout),
               m_Picks.PointOutX,
               m_Picks.PointOutY,
               m_Picks.SurfaceWidth,
               m_Picks.SurfaceHeight,
               m_Picks.PointInX,
               m_Picks.PointInY,
               m_Picks.SizeInWidth,
               m_Picks.SizeInHeight,
               m_Picks.CompositionControlMode,
               m_Picks.KeyColor,
               m_Picks.KeyMask,
               LutModeDesc,
               GainOffsetDesc.c_str(),
               GamutCSCDesc.c_str(),
               m_Picks.EnableStereo ? "enabled" : "disabled"
        );

        CHECK_RC(PrepareOverlayImage());

        if (m_Picks.EnableStereo)
        {
            CHECK_RC(pDisplay->GetEvoDisplay()->SetOverlayImageStereo(m_OverlayDisplay,
                &m_OverlayImage[0], &m_OverlayImage[1]));
        }
        else
        {
            CHECK_RC(pDisplay->GetEvoDisplay()->SetOverlayImage(m_OverlayDisplay,
                &m_OverlayImage[0]));
        }

        // Switch to Manual update so we can use core channel notifier in SendUpdate
        // to wait for all the overlay changes to take effect:
        pDisplay->SetUpdateMode(Display::ManualUpdate);

        if (m_AtLeastGF11x)
        {
            if (m_Picks.EnableLUT)
                CHECK_RC(pDisplay->GetEvoDisplay()->SetOverlayLUT(m_OverlayDisplay, m_Picks.LUTMode,
                    &m_LUTValues[0], 2 * EvoDisplay::LUTMode2NumEntries(m_Picks.LUTMode)));

            if (m_Picks.EnableGainOffset)
                CHECK_RC(pDisplay->SetProcessingGainOffset(
                    m_OverlayDisplay, Display::OVERLAY_CHANNEL_ID, m_Picks.EnableGainOffset,
                    m_Picks.Gains, m_Picks.Offsets));

            if (m_Picks.EnableGamutColorSpaceColwersion)
                CHECK_RC(pDisplay->SetGamutColorSpaceColwersion(
                    m_OverlayDisplay, Display::OVERLAY_CHANNEL_ID,
                    m_Picks.ColorSpaceColwersionMatrix));
        }

        CHECK_RC(pDisplay->GetEvoDisplay()->SetOverlayParameters(m_OverlayDisplay,
            m_Picks.PointInX,
            m_Picks.PointInY,
            m_Picks.SizeInWidth,
            m_Picks.SizeInHeight,
            m_Picks.SizeOutWidth,
            m_Picks.CompositionControlMode,
            m_Picks.KeyColor,
            m_Picks.KeyMask));
        if (m_Picks.EnableStereo)
        {
            CHECK_RC(pDisplay->GetEvoDisplay()->SetOverlayPosStereo(m_OverlayDisplay,
                m_Picks.PointOutX, m_Picks.PointOutY,
                m_Picks.PointOutX+4, m_Picks.PointOutY+1));
        }
        else
        {
            CHECK_RC(pDisplay->GetEvoDisplay()->SetOverlayPos(m_OverlayDisplay,
                m_Picks.PointOutX, m_Picks.PointOutY));
        }

        CHECK_RC(pDisplay->SendUpdate
        (
            true,       // Core
            0xFFFFFFFF, // All bases
            0xFFFFFFFF, // All lwrsors
            0xFFFFFFFF, // All overlays
            0xFFFFFFFF, // All overlaysIMM
            true,       // Interlocked
            true        // Wait for notifier
        ));
        pDisplay->SetUpdateMode(Display::AutoUpdate);

        // It is assumed that "get CRC" function will allow for at least one
        // new frame to draw.
        // Otherwise we will need to wait for a new frame here.
        if (OK != (rc = m_pGolden->Run()))
        {
            Printf(Tee::PriHigh, "Golden miscompare after loop %i.\n", m_pFpCtx->LoopNum);
        }
        if (m_pTestConfig->Verbose() || (rc != OK))
        {
            pDisplay->DumpDisplayInfo();
        }

        // Clean up:
        if (m_AtLeastGF11x)
        {
            pDisplay->SetUpdateMode(Display::ManualUpdate);

            if (m_Picks.EnableLUT)
                pDisplay->GetEvoDisplay()->SetOverlayLUT(m_OverlayDisplay, Display::LUTMODE_LORES, NULL, 0);

            if (m_Picks.EnableGainOffset)
            {
                float ZeroRGB[Display::NUM_RGB] = {0, 0, 0};
                pDisplay->SetProcessingGainOffset(
                    m_OverlayDisplay, Display::OVERLAY_CHANNEL_ID, false, ZeroRGB, ZeroRGB);
            }

            if (m_Picks.EnableGamutColorSpaceColwersion)
            {
                float IdentityMatrix[Display::NUM_RGBC][Display::NUM_RGB] =
                {
                    { 1.0, 0.0, 0.0 },
                    { 0.0, 1.0, 0.0 },
                    { 0.0, 0.0, 1.0 },
                    { 0.0, 0.0, 0.0 }
                };
                pDisplay->SetGamutColorSpaceColwersion(
                    m_OverlayDisplay, Display::OVERLAY_CHANNEL_ID, IdentityMatrix);
            }

            // Overlay LUT has to be disabled in a separate update from disabling
            // the overlay image, from bug 1004132:
            pDisplay->SendUpdate
            (
                true,       // Core
                0xFFFFFFFF, // All bases
                0xFFFFFFFF, // All lwrsors
                0xFFFFFFFF, // All overlays
                0xFFFFFFFF, // All overlaysIMM
                true,       // Interlocked
                true        // Wait for notifier
            );
            pDisplay->SetUpdateMode(Display::AutoUpdate);
        }
        // Disable the overlay - overlay image will be changed/freed
        pDisplay->GetEvoDisplay()->SetOverlayImage(m_OverlayDisplay, NULL);

        if (rc != OK)
        {
            if (m_ContinueOnFrameError)
            {
                m_ContinuationRC = rc;
                rc.Clear();
            }
            else
            {
                return rc;
            }
        }
    }

    return rc;
}

//----------------------------------------------------------------------------

RC EvoOvrl::AllocateResources()
{
    RC rc = OK;

    // Allocate the primary surface.
    m_PrimarySurface.SetWidth(m_Mode.Width);
    m_PrimarySurface.SetHeight(m_Mode.Height);
    m_PrimarySurface.SetColorFormat(m_AtLeastGF11x ? ColorUtils::A2B10G10R10 : ColorUtils::A8R8G8B8);
    m_PrimarySurface.SetType(LWOS32_TYPE_PRIMARY);
    m_PrimarySurface.SetLocation(Memory::Optimal);
    m_PrimarySurface.SetProtect(Memory::Readable);
    m_PrimarySurface.SetLayout(Surface2D::Pitch);
    m_PrimarySurface.SetDisplayable(true);

    CHECK_RC(m_PrimarySurface.Alloc(GetBoundGpuDevice()));

    CHECK_RC(GetDisplay()->BindToPrimaryChannels(&m_PrimarySurface));

    return rc;
}

//----------------------------------------------------------------------------

RC EvoOvrl::InitializeResources()
{
    RC rc = OK;
    Display *pDisplay = GetDisplay();

    CHECK_RC(m_PrimarySurface.Map(m_Subdevice));
    CHECK_RC(Memory::FillRgbBars(m_PrimarySurface.GetAddress(),
                                 m_PrimarySurface.GetWidth(),
                                 m_PrimarySurface.GetHeight(),
                                 m_PrimarySurface.GetColorFormat(),
                                 m_PrimarySurface.GetPitch()));

    CHECK_RC(pDisplay->SetImage(&m_PrimarySurface));
    Printf(Tee::PriLow, "Displaying primary surface.\n");

    return rc;
}

//----------------------------------------------------------------------------
RC EvoOvrl::FreeResources()
{
    Display *pDisplay = GetDisplay();

    pDisplay->SetImage((Surface2D*)NULL);

    // Free the overlay image memory and its ctx dma.
    m_OverlayImage[0].Free();
    m_OverlayImage[1].Free();

    // Free the primary surface.
    m_PrimarySurface.Free();

    Printf(Tee::PriLow, "All resources freed.\n");

    return OK;
}
