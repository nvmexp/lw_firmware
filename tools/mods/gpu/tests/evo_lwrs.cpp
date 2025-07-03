/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Evo Display Cursor test.

#include <stdio.h>
#include "gputest.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/golden.h"
#include "gpu/include/dmawrap.h"
#include "gpu/utility/surf2d.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "core/include/display.h"
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
#include "class/cl9271.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/cl9770.h"
#include "class/cl9870.h"
#include "class/cl507d.h"
#include "ctrl/ctrl0073.h"
#include "gpu/display/evo_disp.h" // EvoRasterSettings
#include "ctrl/ctrl5070.h"

#ifndef INCLUDED_STL_VECTOR_H
#include <vector>
#endif

#ifndef INCLUDED_STL_SET_H
#include <set>
#endif

#ifndef INCLUDED_STL_UTILITY_H
#include <utility> // pair
#endif

typedef struct ModeDesc
{
    UINT32 RasterWidth;
    UINT32 RasterHeight;
    UINT32 Depth;
    UINT32 RefreshRate;
    UINT32 ImageWidth;
    UINT32 ImageHeight;
} ModeDesc;

static const UINT32 Disable = 0xFFFFFFFF;
static const UINT32 GM10xMaxPRUResolutionWidth = 5120;
static const UINT32 DefaultSplitSOR = 0;
static const UINT32 TESTARG_UNSET = 0xFFFFFFFF;

// Use this to request SetRasterForMaxPclkPossible to find
// a max pclk. limit for given displayID
static const UINT32 NullMaxPclkLimit = 0;

class EvoLwrs : public GpuTest
{
    private:
        GpuGoldenSurfaces *     m_pGGSurfs;
        GpuTestConfiguration *  m_pTestConfig;
        Goldelwalues *          m_pGolden;

        // Surface parameters.
        Surface2D m_LwrsorImages[2][2];

        // Stuff copied from m_pTestConfig for faster access.
        ModeDesc m_InitMode, m_StressMode;
        bool m_Use16BitDepthAtOR;
        bool m_MultiHeadStressMode;

        vector<ModeDesc> m_StressModes;

        map<UINT64, pair<Surface2D*,Surface2D*> > m_Backgrounds;

        DmaWrapper m_DmaWrap;

        FLOAT64 m_TimeoutMs;

        // Displays we are testing.
        vector<UINT32> m_LwrsorDisplays;
        bool           m_LwrsorDisplayIsDP;
        bool           m_LwrsorDisplayIsHDMI;
        bool           m_LwrsorDisplayIsLVDS;

        // Cross function data passing:
        UINT32               m_PrimaryConnector;
        Display::DisplayType m_PrimaryType;
        UINT32               m_PrimaryProtocol;
        Display::Encoder     m_PrimaryEncoder;
        UINT32               m_SecondaryConnector;
        UINT32               m_SecondaryStreamDisplayID;
        EvoRasterSettings    m_EvoRaster;
        UINT32               m_MaxCoreVoltage;
        UINT32               m_MinCoreVoltage;
        Surface2D            m_DummyGoldenSurface;
        bool                 m_AtLeastOneRunSuccessful;
        bool                 m_TestSplitSors;
        Display::SplitSorReg m_LwrrSplitSorMode;
        UINT32               m_LwrrSorConfigIdx;
        UINT32               m_DisplaysToTest;
        UINT32               m_NumSorConfigurations;
        bool                 m_TestSingleHeadMultipleStreams;
        UINT32               m_DualLinkTMDSNotSupportedMask;
        UINT32               m_AllCrtsMask;
        UINT32               m_AllDPsMask;
        UINT32               m_SingleLinkEmbeddedDPsMask;
        UINT32               m_AllMultiStreamCapableDPs;
        map<UINT32,UINT32>   m_DualSSTPairs;
        UINT32               m_DPsTestedForDualSST;
        UINT32               m_SORsTestedForDualSST;
        UINT32               m_DPsTestedForDualMST;
        UINT32               m_SORsTestedForDualMST;
        UINT32               m_SORsTestedForEDP;
        UINT32               m_AllConnectors;
        UINT32               m_NumAdditionalHeads;
        bool                 m_IsLVDSInReducedMode;

        struct DispCoverageInfo
        {
            UINT32 Display;
            bool IsTestPassing;
        };
        // Coverage data is a map with the key as split SOR mode
        // and the value as a vector of all display IDs passing/failing in it
        map<UINT32, vector<DispCoverageInfo> > m_CoverageData;

        // Keeps track which ORs were untested at max pclk in each split SOR mode
        vector <UINT32> m_ORsUntestedAtMaxPclk;
        // List of heads to be tested, m_HeadMask holds the head mask
        // default is all available heads, otherwise derived from HeadMask arg
        vector<UINT32>       m_HeadRoutingMap;

        // Keeps track of whether multihead MST was tested on each SOR
        vector <bool>        m_MultiheadMSTTestedOnSOR;

        // Keeps track of which displays were tested on each OR
        vector <UINT32>      m_DisplaysTestedOnOR;

        // Keeps track of which displays were tested on the current stress mode
        UINT32               m_DisplaysTestedOnMode;

        // Tells if all heads were tested at max pclk already:
        bool                 m_AllHeadsTestedAtMaxPclk;
        enum VariantType
        {
            VariantNormal,
            VariantDPLanes,
            VariantTMDSBOnDualTMDS,
            VariantSingleHeadDualSSTMST,
        };
        VariantType m_Variant;

        // Index for different SetModes to be used in Goldens
        UINT32 m_SetModeIdx;

        StickyRC m_ContinuationRC;

        bool   m_ContinueOnFrameError;
        bool   m_EnableDMA;
        bool   m_EnableMultiHead;
        bool   m_EnableScaler;
        bool   m_ReportCoreVoltage;
        bool   m_AtLeastG8x;
        bool   m_AtLeastGT21x;
        bool   m_AtLeastGF11x;
        bool   m_AtLeastGK10x;
        bool   m_AtLeastGK11x;
        bool   m_AtLeastGM10x;
        bool   m_AtLeastGP10x;

        vector<UINT32> m_LUTValues;

        FancyPickerArray * m_pPickers;
        FancyPicker::FpContext * m_pFpCtx;  // loop, frame, rand

        enum LwrsorPosition
        {
           ScreenRandom // Test all valid visible positions
           ,Top
           ,Bottom
           ,Left
           ,Right
           ,TopLeft
           ,TopRight
           ,BottomLeft
           ,BottomRight
        };

        // All the random parameters we picked for this cursor (keep on hand for reporting).
        struct PicksLwrsor
        {
            INT32  X;
            INT32  Y;
            UINT32 HotSpotX;
            UINT32 HotSpotY;
            UINT32 Size;
            UINT32 Format;
            UINT32 Composition;
            bool   EnableGainOffset;
            float  Gains[Display::NUM_RGB];
            float  Offsets[Display::NUM_RGB];
            bool   EnableGamutColorSpaceColwersion;
            Display::ChannelID ColorSpaceColwersionChannel;
            float  ColorSpaceColwersionMatrix[Display::NUM_RGBC][Display::NUM_RGB];
            bool   EnableLUT;
            Display::LUTMode LUTMode;
            bool   EnableDither;
            UINT32 DitherBits;
            Display::DitherMode DitherMode;
            UINT32 DitherPhase;
            UINT32 BackgroundSurfaceFormat;
            float  ScalerH;
            float  ScalerV;
            bool   EnableStereoLwrsor;
            bool   EnableStereoBackground;
            bool   EnableDpFramePacking;
            EvoProcampSettings Eps;
            LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP ORPixelBpp;
        };
        PicksLwrsor m_Picks;

        // Don't run EvoLwrs on these displays
        UINT32 m_SkipDisplayMask;
        bool   m_SkipDisplayPort;

        UINT32 m_ForceDPLaneCount;
        UINT32 m_LwrrentDPLaneCount;
        UINT32 m_ForceDPLinkBW;

        UINT32 m_MaxPixelClockHz;

        UINT32 m_NumMultiHeadStressModes;
        UINT32 m_NoVariantsMask;
        UINT32 m_BPP16ModesMask;

        UINT32 m_ReorderBankWidthMax;

        // This is used to prevent other tests from changing p-states during this test
        PStateOwner m_PStateOwner;

        // Default split SOR config before EvoLwrs starts
        Display::SplitSorReg m_DefaultSplitSorReg;
        bool   m_EnableSplitSOR;

        // Don't run EvoLwrs on these split SOR combinations
        UINT32 m_SkipSplitSORMask;

        // Vector of skip display mask and split SOR config index
        string m_SkipDisplayMaskBySplitSOR;
        map<UINT32, UINT32> m_SkipDisplayMaskBySplitSORList;

        bool   m_EnableMaxPclkTestingOnEachHead;

        // If enabled we ensure each OR is tested at the max pclk possible
        // w.r.t the stress modes in EvoLwrs
        bool   m_EnableMaxPclkPossibleOnEachOR;

        bool   m_EnablePRU;

        bool   m_EnableDisplayRegistersDump;

        // Flag to indicate print log priority
        Tee::Priority m_LogPriority;

        // Flag to print coverage of EvoLwrs
        bool m_PrintCoverage;

        // Flag to run test in a reduced operation mode
        bool m_EnableReducedMode;

        // Flag to allow for all modeset skips on a given display ID
        bool m_AllowModesetSkipInReducedMode;

        // Enable mask indicating which heads to test
        UINT32 m_HeadMask;

        bool m_EnableXbarCombinations;
        bool m_EnableDualSST;
        bool m_UseOnlyDualSSTSiblings;
        bool m_EnableDualMST;

        // Flag to allow all possible OR pixel depths to be tested
        bool m_EnableAllORPixelDepths;

        // Flag to enable reduced # of loops when testing multiple SOR configs
        bool m_EnableMultiSorConfigReducedMode;

        // Flag to run LVDS in reduced mode
        bool m_EnableLVDSReducedMode;

        // Only run a certain percentage of the possible combinations of Displays and SORs
        UINT08 m_SkipXbarCombinationsPercent;
        set<pair<UINT32,UINT32> > m_XbarCombinationsToSkip;

        // Flag to use random data as the background surface.
        bool m_UseRandomBackgroundSurface;
        bool m_IsMultipleHeadsSingleDisplay;

        bool m_RunEachVariantOnce;

        // Pick random cursor state.
        RC PickLwrsor();

        // Allocate and fill cursor image based on PickLwrsor() selections
        RC PrepareLwrsorImage(UINT32 idx);

        RC PrepareBackground(UINT32 StressModeIdx, ColorUtils::Format Format);
        RC PrepareBackgroundSurface
        (
            UINT32 StressModeIdx,
            ColorUtils::Format Format,
            bool Left,
            Surface2D **Surface
        );

        RC ValidateConnectorsPresence(UINT32 Connectors);

        RC DetermineDualSSTPairs();
        RC ClearPreviousXBARMappings(UINT32 displayID, UINT32 sorIdxToRestore);

        RC SetDpFramePacking(bool enable);

        // Initialize the test properties.
        RC InitProperties();

        // Set all our FancyPickers to defaults
        RC SetDefaultsForPicker(UINT32 idx);

        // Run the random test.
        RC LoopRandomMethods();

        // Test a display configuration.  Called by RunWithoutFinalCleanup().
        RC RunDisplay();
        // Called to avoid CHECK_RC_CLEANUP.  Called by Run().
        RC RunWithoutFinalCleanup
        (
            UINT32 *pLVDSDisplayId,
            bool *pOrgLVDSDualLinkEnable
        );
        RC RunOneStressMode();

        RC DetermineVariants();
        RC Rulwariants();

        void CleanupDisplay();
        void DisableSimulatedDP();
        void DisableSingleHeadMultistream();

        bool IsDisplayIdSkipped(UINT32 displayID);

        // Parser function to map split SOR config index toi skip display mask
        RC ParseSkipDisplayMaskBySplitSOR();

        enum TmdsLinkType
        {
            TMDS_LINK_A,
            TMDS_LINK_B
        };

        // API to check if a display is TMDS single link capable in split SOR
        bool IsTmdsSingleLinkCapable(UINT32 displayID, TmdsLinkType LinkType);

        // Helper API to detect if image scaling needs to be performed
        bool IsImageScalingNeeded(UINT32 StressModeIdx);
        // Helper API to add/update existing coverage data on a per display, per
        // split SOR basis
        void AddOrUpdateCoverageData(UINT32 Display, bool IsTestPassing);

        void CleanSORExcludeMask(UINT32 displayID);

        // Possible connector variants that runs with test 4
        enum ConnectorVariant
        {
            VarDualSST  = 1 << 0,
            VarDualMST  = 1 << 1,
            VarNormalDP = 1 << 2,
            VarTmds     = 1 << 3,
            VarDualTmds = 1 << 4,
        };

        // Structure that will allow single connector variant type run per stress mode
        struct MinimalCVCoverage
        {
            UINT32 minCoverage = 0;
            Tee::Priority pri = Tee::PriLow;

            void Reset() { minCoverage = 0; }
            void SetPrintPri(Tee::Priority priority) { pri = priority; }
            // According to ConnectorVariant enum
            const char *variantString[5] =
                {"Dual SST", "Dual MST", "DP", "TMDS", "Dual TMDS"};

            const char *GetVariantName (ConnectorVariant variant)
            {
                UINT32 index = BIT_IDX_32(variant);
                if (index < (sizeof(variantString)/sizeof(variantString[0])))
                    return variantString[index];
                return "unknown protocol";
            }

            bool IsAlreadyRun(UINT32 displayId, ConnectorVariant variant)
            {
                if (minCoverage & variant)
                {
                    Printf(pri, "%s was already run on 0x%08x, skipping\n",
                            GetVariantName(variant), displayId);
                    return true;
                }
                return false;
            }

            void SetAlreadyRun(ConnectorVariant variant)
            {
                minCoverage |= variant;
            }
        };
        // Minimal connector variant coverage
        MinimalCVCoverage m_MinCVCoverage;

    public:
        EvoLwrs();

        RC Setup();
        // Run the test on all displays
        RC Run();
        RC Cleanup();

        virtual bool IsSupported();

        void AddStressMode(ModeDesc * pMode);
        void ClearStressModes();

        // See "CreateRaster" function to understand why we add to width/height
        static UINT32 GetEvoLwrsPclk
        (
            UINT32 Width,
            UINT32 Height,
            UINT32 RefreshRate
        )
            { return ((Width+200)*(Height+150)*RefreshRate);}

        static void CreateRaster
        (
            UINT32 ScaledWidth,
            UINT32 ScaledHeight,
            UINT32 StressModeWidth,
            UINT32 StressModeHeight,
            UINT32 RefreshRate,
            UINT32 LaneCount,
            UINT32 LinkRate,
            EvoRasterSettings *ers
        );

        SETGET_PROP(SkipDisplayMask, UINT32);
        SETGET_PROP(SkipDisplayPort, bool);
        SETGET_PROP(ForceDPLaneCount, UINT32);
        SETGET_PROP(ForceDPLinkBW, UINT32);
        SETGET_PROP(MaxPixelClockHz, UINT32);
        SETGET_PROP(NumMultiHeadStressModes, UINT32);
        SETGET_PROP(NoVariantsMask, UINT32);
        SETGET_PROP(BPP16ModesMask, UINT32);
        SETGET_PROP(ContinueOnFrameError, bool);
        SETGET_PROP(EnableDMA, bool);
        SETGET_PROP(EnableMultiHead, bool);
        SETGET_PROP(EnableScaler, bool);
        SETGET_PROP(ReportCoreVoltage, bool);
        SETGET_PROP(EnableSplitSOR, bool);
        SETGET_PROP(SkipSplitSORMask, UINT32);
        SETGET_PROP(SkipDisplayMaskBySplitSOR, string);
        SETGET_PROP(EnableMaxPclkTestingOnEachHead, bool);
        SETGET_PROP(EnableMaxPclkPossibleOnEachOR, bool);
        SETGET_PROP(EnablePRU, bool);
        SETGET_PROP(EnableDisplayRegistersDump, bool);
        SETGET_PROP(PrintCoverage, bool);
        SETGET_PROP(EnableReducedMode, bool);
        SETGET_PROP(AllowModesetSkipInReducedMode, bool);
        SETGET_PROP(HeadMask, UINT32);
        SETGET_PROP(EnableXbarCombinations, bool);
        SETGET_PROP(EnableDualSST, bool);
        SETGET_PROP(UseOnlyDualSSTSiblings, bool);
        SETGET_PROP(EnableDualMST, bool);
        SETGET_PROP(EnableAllORPixelDepths, bool);
        SETGET_PROP(EnableMultiSorConfigReducedMode, bool);
        SETGET_PROP(EnableLVDSReducedMode, bool);
        SETGET_PROP(SkipXbarCombinationsPercent, UINT08);
        SETGET_PROP(UseRandomBackgroundSurface, bool);
        SETGET_PROP(RunEachVariantOnce, bool);
};

//----------------------------------------------------------------------------
// Script interface.

JS_CLASS_INHERIT(EvoLwrs, GpuTest,
                 "Evo Display Cursor test.");

CLASS_PROP_READWRITE(EvoLwrs, SkipDisplayMask, UINT32,
                     "Do not run EvoLwrs on these display masks");
CLASS_PROP_READWRITE(EvoLwrs, SkipDisplayPort, bool,
                     "bool: if true skip testing Display Port outputs");
CLASS_PROP_READWRITE(EvoLwrs, ForceDPLaneCount, UINT32,
                     "Force number of lanes used on DP outputs, the Link BW has to be selected as well");
CLASS_PROP_READWRITE(EvoLwrs, ForceDPLinkBW, UINT32,
                     "Force link rate: 0x6 (1.62GBPS), 0xa (2.7GBPS), 0x14 (5.4GBPS); The number of lanes has to selected as well");
CLASS_PROP_READWRITE(EvoLwrs, MaxPixelClockHz, UINT32,
                     "Skip SetModes with Pixel Clock higher than specified - useful when DP is forced to work with lowered bandwidth");
CLASS_PROP_READWRITE(EvoLwrs, NumMultiHeadStressModes, UINT32,
                     "How many stress modes should be exelwted on multiple heads - they have to be last on the list");
CLASS_PROP_READWRITE(EvoLwrs, NoVariantsMask, UINT32,
                     "Mask of stress modes which will run in single SST mode");
CLASS_PROP_READWRITE(EvoLwrs, BPP16ModesMask, UINT32,
                     "Mask of stress modes for which OR Pixel Depth should be 16");
CLASS_PROP_READWRITE(EvoLwrs, ContinueOnFrameError, bool,
                     "bool: continue running if an error oclwrred in a frame");
CLASS_PROP_READWRITE(EvoLwrs, EnableDMA, bool,
                     "bool: enable mem2mem transfers");
CLASS_PROP_READWRITE(EvoLwrs, EnableMultiHead, bool,
                     "bool: allow for multi head runs when possible");
CLASS_PROP_READWRITE(EvoLwrs, EnableScaler, bool,
                     "bool: allow for random differences between active raster"
                     "size and the background image");
CLASS_PROP_READWRITE(EvoLwrs, ReportCoreVoltage, bool,
                     "bool: report min and max core voltage observed");
CLASS_PROP_READWRITE(EvoLwrs, EnableSplitSOR, bool,
                     "bool: enable testing of all possible split SOR combinations");
CLASS_PROP_READWRITE(EvoLwrs, SkipSplitSORMask, UINT32,
                     "bool: Do not run EvoLwrs on these split SOR combinations");
CLASS_PROP_READWRITE(EvoLwrs, SkipDisplayMaskBySplitSOR, string,
                             "string: Do not run EvoLwrs for specified split SOR configs");
CLASS_PROP_READWRITE(EvoLwrs, EnableMaxPclkTestingOnEachHead, bool,
                     "bool: when set to false only the default head will be tested at max pclk");
CLASS_PROP_READWRITE(EvoLwrs, EnableMaxPclkPossibleOnEachOR, bool,
                     "bool: when set to false only the first passing OR will be tested at max pclk");
CLASS_PROP_READWRITE(EvoLwrs, EnablePRU, bool,
                     "bool: enable testing of Pixel Reordering Unit");
CLASS_PROP_READWRITE(EvoLwrs, EnableDisplayRegistersDump, bool,
                     "bool: enable dumping display register just before CRC capture");
CLASS_PROP_READWRITE(EvoLwrs, PrintCoverage, bool,
                     "bool: Dump out test coverage report");
CLASS_PROP_READWRITE(EvoLwrs, EnableReducedMode, bool,
                     "bool: enable reduced mode of operation");
CLASS_PROP_READWRITE(EvoLwrs, AllowModesetSkipInReducedMode, bool,
                     "bool: continue running if an error oclwred due to all modesets being skipped in reduced mode");
CLASS_PROP_READWRITE(EvoLwrs, HeadMask, UINT32,
                     "Run EvoLwrs on just these heads");
CLASS_PROP_READWRITE(EvoLwrs, EnableXbarCombinations, bool,
                     "bool: enable testing of all possible SOR XBAR combinations");
CLASS_PROP_READWRITE(EvoLwrs, EnableDualSST, bool,
                     "bool: enable testing of single head dual SST mode");
CLASS_PROP_READWRITE(EvoLwrs, UseOnlyDualSSTSiblings, bool,
                     "bool: enable testing of single head dual SST mode only in pad pairs (A/B, C/D, etc)");
CLASS_PROP_READWRITE(EvoLwrs, EnableDualMST, bool,
                     "bool: enable testing of single head dual MST mode");
CLASS_PROP_READWRITE(EvoLwrs, EnableAllORPixelDepths, bool,
                     "bool: Enable all possible OR pixel depths to be tested");
CLASS_PROP_READWRITE(EvoLwrs, EnableMultiSorConfigReducedMode, bool,
                     "bool: Enable multi SOR config reduced mode");
CLASS_PROP_READWRITE(EvoLwrs, EnableLVDSReducedMode, bool,
                     "bool: Enable LVDS in reduced mode");
CLASS_PROP_READWRITE(EvoLwrs, SkipXbarCombinationsPercent, UINT08,
                     "Percent (0-100) of possible SOR XBAR combinations to skip.");
CLASS_PROP_READWRITE(EvoLwrs, UseRandomBackgroundSurface, bool,
                     "bool: Use random data as the background surface.");
CLASS_PROP_READWRITE(EvoLwrs, RunEachVariantOnce, bool,
                     "bool: Use minimum display id's to run per stress mode "
                     "and covering all possible variants.");

EvoLwrs::EvoLwrs()
: m_InitMode()
, m_StressMode()
, m_Use16BitDepthAtOR(false)
, m_MultiHeadStressMode(false)
, m_TimeoutMs(1000)
, m_LwrsorDisplayIsDP(false)
, m_LwrsorDisplayIsHDMI(false)
, m_LwrsorDisplayIsLVDS(false)
, m_PrimaryConnector(0)
, m_PrimaryType(Display::NONE)
, m_PrimaryProtocol(0)
, m_PrimaryEncoder()
, m_SecondaryConnector(0)
, m_SecondaryStreamDisplayID(0)
, m_MaxCoreVoltage(0)
, m_MinCoreVoltage(0)
, m_AtLeastOneRunSuccessful(false)
, m_TestSplitSors(false)
, m_LwrrSorConfigIdx(0)
, m_DisplaysToTest(0)
, m_NumSorConfigurations(0)
, m_TestSingleHeadMultipleStreams(false)
, m_DualLinkTMDSNotSupportedMask(0)
, m_AllCrtsMask(0)
, m_AllDPsMask(0)
, m_SingleLinkEmbeddedDPsMask(0)
, m_AllMultiStreamCapableDPs(0)
, m_DPsTestedForDualSST(0)
, m_SORsTestedForDualSST(0)
, m_DPsTestedForDualMST(0)
, m_SORsTestedForDualMST(0)
, m_SORsTestedForEDP(0)
, m_AllConnectors(0)
, m_NumAdditionalHeads(0)
, m_IsLVDSInReducedMode(false)
, m_DisplaysTestedOnMode(0)
, m_AllHeadsTestedAtMaxPclk(false)
, m_Variant(VariantNormal)
, m_SetModeIdx(0)
, m_ContinueOnFrameError(false)
, m_EnableDMA(true)
, m_EnableMultiHead(true)
, m_EnableScaler(true)
, m_ReportCoreVoltage(false)
, m_AtLeastG8x(false)
, m_AtLeastGT21x(false)
, m_AtLeastGF11x(false)
, m_AtLeastGK10x(false)
, m_AtLeastGK11x(false)
, m_AtLeastGM10x(false)
, m_AtLeastGP10x(false)
, m_SkipDisplayMask(0)
, m_SkipDisplayPort(false)
, m_ForceDPLaneCount(0)
, m_LwrrentDPLaneCount(0)
, m_ForceDPLinkBW(0)
, m_MaxPixelClockHz(0)
, m_NumMultiHeadStressModes(1)
, m_NoVariantsMask(1)
, m_BPP16ModesMask(1)
, m_ReorderBankWidthMax(0)
, m_EnableSplitSOR(false)
, m_SkipSplitSORMask(0)
, m_SkipDisplayMaskBySplitSOR("")
, m_EnableMaxPclkTestingOnEachHead(true)
, m_EnableMaxPclkPossibleOnEachOR(true)
, m_EnablePRU(true)
, m_EnableDisplayRegistersDump(false)
, m_LogPriority(Tee::PriLow)
, m_PrintCoverage(false)
, m_EnableReducedMode(false)
, m_AllowModesetSkipInReducedMode(false)
, m_HeadMask(TESTARG_UNSET)
, m_EnableXbarCombinations(false)
, m_EnableDualSST(true)
, m_UseOnlyDualSSTSiblings(true)
, m_EnableDualMST(true)
, m_EnableAllORPixelDepths(true)
, m_EnableMultiSorConfigReducedMode(true)
, m_EnableLVDSReducedMode(false)
, m_SkipXbarCombinationsPercent(0)
, m_UseRandomBackgroundSurface(true)
, m_IsMultipleHeadsSingleDisplay(false)
, m_RunEachVariantOnce(false)
{
    SetName("EvoLwrs");
    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(FPK_EVOLWRS_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
    m_pGGSurfs = NULL;
    m_pTestConfig = GetTestConfiguration();
    m_pGolden = GetGoldelwalues();

    ModeDesc basicMode = {1024, 768, 32, 60};
    m_StressModes.push_back(basicMode);
}

// Parser function to generate a map of split SOR config index to
// skip display mask
// Format: <Split SOR Config Index#1>:<Skip Display Mask#1>|...
RC EvoLwrs::ParseSkipDisplayMaskBySplitSOR()
{
    RC rc;

    if ((m_SkipDisplayMaskBySplitSOR.size() == 0) ||
        m_SkipDisplayMaskBySplitSORList.size())
    {
        // If there is no per Split SOR Skip Display Mask provided, or
        // the skip list has already been parsed then ignore
        return OK;
    }

    map<UINT32, UINT32> *pSkipList = &m_SkipDisplayMaskBySplitSORList;
    vector<string> tokens;
    CHECK_RC(Utility::Tokenizer(m_SkipDisplayMaskBySplitSOR, ":|", &tokens));
    if (tokens.size() % 2)
    {
        Printf(Tee::PriNormal,
               "Invalid no. of parameters in SkipDisplayMaskBySplitSOR: \n"
               "Expected <Split SOR Index#1>:<Skip Display Mask#1>|...\n");
        return RC::BAD_PARAMETER;
    }

    pSkipList->clear();
    UINT32 lwrrSplitSORIndex = 0xffffffff;
    UINT32 lwrrSkipDisplayMask = 0;
    for (UINT32 i = 0; i < tokens.size(); i+=2)
    {
        lwrrSplitSORIndex = strtoul(tokens[i].c_str(),NULL, 10);
        lwrrSkipDisplayMask = strtoul(tokens[i + 1].c_str(), NULL, 16);
        pSkipList->insert(pair<UINT32,UINT32>(lwrrSplitSORIndex, lwrrSkipDisplayMask));
    }

    return rc;
}

RC EvoLwrs::Setup()
{
    RC rc;

    if ((m_ForceDPLaneCount != 0) ^ (m_ForceDPLinkBW != 0))
    {
        Printf(Tee::PriNormal,
            "EvoLwrs test configuration error: ForceDPLaneCount and ForceDPLinkBW"
            " need to be specified at the same time.\n");
        return RC::ILWALID_ARGUMENT;
    }

    // Cache the existing SplitSor configuration
    Display *pDisplay = GetDisplay();

    m_DefaultSplitSorReg.clear();
    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_SPLIT_SOR))
        CHECK_RC(pDisplay->CaptureSplitSor(&m_DefaultSplitSorReg));

    CHECK_RC(GpuTest::Setup());
    // Reserve a Display object, disable UI, set default graphics mode.
    CHECK_RC(GpuTest::AllocDisplay());

    UINT32 DisplayClass = pDisplay->GetClass();
    switch (DisplayClass)
    {
        case LW9870_DISPLAY:
        case LW9770_DISPLAY:
            m_AtLeastGP10x = true; // fall through
        case LW9570_DISPLAY:
        case LW9470_DISPLAY:
            m_AtLeastGM10x = true; // fall through
        case LW9270_DISPLAY:
            m_AtLeastGK11x = true; // fall through
        case LW9170_DISPLAY:
            m_AtLeastGK10x = true; // fall through
        case LW9070_DISPLAY:
            m_AtLeastGF11x = true; // fall through
        case GT214_DISPLAY:
            m_AtLeastGT21x = true; // fall through
        case G94_DISPLAY:
        case GT200_DISPLAY:
        case G82_DISPLAY:
            m_AtLeastG8x = true;
            break;
        case LW50_DISPLAY:
            break;
        default:
            MASSERT(!"Class not recognized!");
            return RC::SOFTWARE_ERROR;
    }

    m_EnableDualSST &= GetBoundGpuSubdevice()->
        HasFeature(Device::GPUSUB_SUPPORTS_SINGLE_HEAD_DUAL_SST);

    m_EnableDualMST &= GetBoundGpuSubdevice()->
        HasFeature(Device::GPUSUB_SUPPORTS_SINGLE_HEAD_DUAL_MST);

    UINT32 usableHeadMask = pDisplay->GetUsableHeadsMask();
    if (m_HeadMask == TESTARG_UNSET)
        m_HeadMask = usableHeadMask;
    else if (m_HeadMask & ~usableHeadMask)
    {
        Printf(Tee::PriNormal,
               "Error: Head mask must be a subset of available displays\n");
        return RC::SOFTWARE_ERROR;
    }

    // Generate subset of head IDs to test
    UINT32 headMask = m_HeadMask;
    UINT32 headToTest = 0;
    m_HeadRoutingMap.clear();
    while (headMask)
    {
        if ((1 << headToTest) & headMask)
        {
            m_HeadRoutingMap.push_back(headToTest);
            headMask ^= (1 << headToTest);
        }
        headToTest++;
    }

    for (INT32 i = Utility::CountBits(m_HeadMask);
         i <  Utility::CountBits(usableHeadMask); i++)
        m_HeadRoutingMap.push_back(m_HeadRoutingMap[0]);

    m_ReorderBankWidthMax = 0;
    if (m_EnablePRU)
    {
        m_ReorderBankWidthMax = pDisplay->GetHeadReorderBankWidthSizeMax(0);
        const UINT32 headCount = pDisplay->GetHeadCount();
        for (UINT32 headIdx = 1; headIdx < headCount; headIdx++)
        {
            if (pDisplay->GetHeadReorderBankWidthSizeMax(headIdx) !=
                m_ReorderBankWidthMax)
            {
                Printf(Tee::PriNormal,
                    "Error: EvoLwrs doesn't support varying reorder bank width"
                    " size.\n");
                return RC::SOFTWARE_ERROR;
            }
        }
    }

    CHECK_RC(InitProperties());

    // Claim p-states on the subdevice we run on to prevent other tests from changing them
    CHECK_RC(m_PStateOwner.ClaimPStates(GetBoundGpuSubdevice()));

    if (m_EnableDMA)
        CHECK_RC(m_DmaWrap.Initialize(m_pTestConfig, Memory::Optimal));

    if (m_NumMultiHeadStressModes == 0)
        Printf(Tee::PriNormal,
        "Warning: Multiple heads won't be tested in EvoLwrs\n");

    if (m_SkipXbarCombinationsPercent > 100)
    {
        Printf(Tee::PriHigh, "Error: SkipXbarCombinationsPercent must be between 0 and 100\n");
        return RC::ILWALID_ARGUMENT;
    }

    return OK;
}

RC EvoLwrs::PrepareBackgroundSurface
(
    UINT32 StressModeIdx,
    ColorUtils::Format Format,
    bool Left,
    Surface2D **Surface
)
{
    RC rc;

    unique_ptr<Surface2D> NewBackground = make_unique<Surface2D>();
    MASSERT(NewBackground);

    const ModeDesc &StressMode = m_StressModes[StressModeIdx];
    UINT32 SurfaceWidth = 0, SurfaceHeight = 0;

    if (IsImageScalingNeeded(StressModeIdx))
    {
        SurfaceWidth = StressMode.ImageWidth;
        SurfaceHeight = StressMode.ImageHeight;
    }
    else
    {
        SurfaceWidth = StressMode.RasterWidth;
        SurfaceHeight = StressMode.RasterHeight;
    }

    NewBackground->SetWidth(SurfaceWidth);
    NewBackground->SetHeight(SurfaceHeight);
    NewBackground->SetColorFormat(Format);
    NewBackground->SetType(LWOS32_TYPE_PRIMARY);
    NewBackground->SetLocation(Memory::Optimal);
    NewBackground->SetDisplayable(true);

    CHECK_RC(NewBackground->Alloc(GetBoundGpuDevice()));

    if (m_EnableDMA)
    {
        Surface2D SysMemBackground;

        SysMemBackground.SetWidth(SurfaceWidth);
        SysMemBackground.SetHeight(SurfaceHeight);
        SysMemBackground.SetPitch(NewBackground->GetPitch());
        SysMemBackground.SetColorFormat(Format);
        SysMemBackground.SetType(LWOS32_TYPE_PRIMARY);
        SysMemBackground.SetLocation(Memory::Coherent);

        CHECK_RC(SysMemBackground.Alloc(GetBoundGpuDevice()));

        CHECK_RC(SysMemBackground.Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
        if (m_UseRandomBackgroundSurface)
        {
            CHECK_RC(Memory::FillRandom(SysMemBackground.GetAddress(),
                                        m_pTestConfig->Seed(),
                                        SysMemBackground.GetSize()));
        }
        else
        {
            CHECK_RC(Memory::FillRgbBars(SysMemBackground.GetAddress(),
                                         SysMemBackground.GetWidth(),
                                         SysMemBackground.GetHeight(),
                                         Format,
                                         SysMemBackground.GetPitch()));
        }
        m_DmaWrap.SetSurfaces(&SysMemBackground, NewBackground.get());

        CHECK_RC(m_DmaWrap.Copy(0,0, NewBackground->GetPitch(),
            SurfaceHeight, 0, 0, m_TimeoutMs, 0));

        SysMemBackground.Free();
    }
    else
    {
        CHECK_RC(NewBackground->Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
        if (m_UseRandomBackgroundSurface)
        {
            CHECK_RC(Memory::FillRandom(NewBackground->GetAddress(),
                                        m_pTestConfig->Seed(),
                                        NewBackground->GetSize()));
        }
        else
        {
            CHECK_RC(Memory::FillRgbBars(NewBackground->GetAddress(),
                                         NewBackground->GetWidth(),
                                         NewBackground->GetHeight(),
                                         Format,
                                         NewBackground->GetPitch()));
        }
        NewBackground->Unmap();
    }

    if (!Left)
    {
        MASSERT(NewBackground->GetWidth() >= 0x20);
        MASSERT(NewBackground->GetHeight() >= 0x4);
        CHECK_RC(NewBackground->Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
        CHECK_RC(Memory::FillRgbBars((UINT08*)NewBackground->GetAddress()+0x10,
                                     0x10,
                                     4,
                                     Format,
                                     NewBackground->GetPitch()));
        NewBackground->Unmap();
    }

    *Surface = NewBackground.release();

    return rc;
}

RC EvoLwrs::PrepareBackground(UINT32 StressModeIdx, ColorUtils::Format Format)
{
    RC rc;
    UINT64 BackgroundID = (UINT64(StressModeIdx)<<32) | Format;

    if (m_Backgrounds.find(BackgroundID) == m_Backgrounds.end())
    {
        Surface2D* NewBackground = NULL;
        CHECK_RC(PrepareBackgroundSurface(StressModeIdx, Format,
            true, &NewBackground));
        m_Backgrounds[BackgroundID] = make_pair(NewBackground, (Surface2D*)0);
    }

    if (m_Picks.EnableStereoBackground &&
        (m_Backgrounds[BackgroundID].second == NULL))
    {
        Surface2D* NewBackground = NULL;
        CHECK_RC(PrepareBackgroundSurface(StressModeIdx, Format,
            false, &NewBackground));
        m_Backgrounds[BackgroundID].second = NewBackground;
    }

    return rc;
}

RC EvoLwrs::ValidateConnectorsPresence(UINT32 Connectors)
{
    Display *pDisplay = GetDisplay();

    INT32 lastDfpConnectorIdx = -1;
    UINT32 firstConnectorsHalf = Connectors & 0xFF;
    do
    {
        lastDfpConnectorIdx = Utility::BitScanReverse(firstConnectorsHalf);
        if (lastDfpConnectorIdx >= 0)
        {
            if (pDisplay->GetDisplayType(1<<lastDfpConnectorIdx) != Display::DFP)
            {
                firstConnectorsHalf ^= 1<<lastDfpConnectorIdx;
                lastDfpConnectorIdx = -1;
            }
            else
            {
                break;
            }
        }
    } while (firstConnectorsHalf);

    if (lastDfpConnectorIdx == -1)
    {
        UINT32 secondConnectorsHalf = Connectors & 0xFFFFFF00;
        lastDfpConnectorIdx = Utility::BitScanReverse(secondConnectorsHalf);
    }

    if (lastDfpConnectorIdx == -1)
    {
        // No DFPs
        return OK;
    }

    UINT32 missingConnector = 0;

    if (lastDfpConnectorIdx < 8)
    {
        for (INT32 connectorIdx = 0; connectorIdx <= lastDfpConnectorIdx;
            connectorIdx++)
        {
            if ((1<<connectorIdx & Connectors) == 0)
            {
                missingConnector = 1<<connectorIdx;
                break;
            }
        }
        lastDfpConnectorIdx = 31;
    }

    if (missingConnector == 0)
    {
        for (INT32 connectorIdx = 8; connectorIdx <= lastDfpConnectorIdx;
            connectorIdx++)
        {
            if ((1<<connectorIdx & Connectors) == 0)
            {
                missingConnector = 1<<connectorIdx;
                break;
            }
        }
    }

    if (missingConnector)
    {
        Printf(Tee::PriNormal,
            "Connector validation error: expected 0x%08x when only "
            "0x%08x is present.\n", missingConnector, Connectors);
        Printf(Tee::PriNormal,
            "You can bypass this error by adding \"-testarg 4 SkipDisplayMask "
            "0x%x\"\n", missingConnector);
        if ((missingConnector & m_SkipDisplayMask) == 0)
            return RC::ILWALID_DISPLAY_MASK;
    }

    return OK;
}

RC EvoLwrs::DetermineDualSSTPairs()
{
    RC rc;

    if (!m_EnableDualSST)
        return OK;

    MASSERT(Utility::CountBits(m_AllMultiStreamCapableDPs) > 1);

    m_DualSSTPairs.clear();

    if (!m_UseOnlyDualSSTSiblings)
    {
        UINT32 connectorsToCheck = m_AllMultiStreamCapableDPs;
        while (connectorsToCheck)
        {
            UINT32 connectorToCheck = connectorsToCheck &
                ~(connectorsToCheck - 1);
            connectorsToCheck ^= connectorToCheck;

            // Pick secondary connector in a way so it is changing
            // whenever the primary connector is changing:
            UINT32 connectorsToTheLeft = m_AllMultiStreamCapableDPs &
                ~((connectorToCheck<<1)-1);
            if (connectorsToTheLeft)
            {
                m_DualSSTPairs[connectorToCheck] =
                    1<<Utility::BitScanForward(connectorsToTheLeft);
            }
            else
            {
                UINT32 connectorsToTheRight = m_AllMultiStreamCapableDPs ^
                    (connectorsToTheLeft | connectorToCheck);
                MASSERT(connectorsToTheRight);
                m_DualSSTPairs[connectorToCheck] =
                    1<<Utility::BitScanForward(connectorsToTheRight);
            }
        }

        return OK;
    }

    map<UINT32,UINT32> padToDisplayId;

    UINT32 connectorsToCheck = m_AllMultiStreamCapableDPs;
    while (connectorsToCheck)
    {
        UINT32 connectorToCheck = connectorsToCheck & ~(connectorsToCheck - 1);
        connectorsToCheck ^= connectorToCheck;

        LW0073_CTRL_DFP_GET_PADLINK_MASK_PARAMS getPadlinkMaskParams = {0};
        getPadlinkMaskParams.subDeviceInstance =
            GetBoundGpuSubdevice()->GetSubdeviceInst();
        getPadlinkMaskParams.displayId = connectorToCheck;
        CHECK_RC(GetDisplay()->RmControl(LW0073_CTRL_CMD_DFP_GET_PADLINK_MASK,
            &getPadlinkMaskParams, sizeof(getPadlinkMaskParams)));

        if (Utility::CountBits(getPadlinkMaskParams.padlinkMask) != 1)
            return RC::SOFTWARE_ERROR;

        UINT32 padIdx =
            Utility::BitScanForward(getPadlinkMaskParams.padlinkMask);

        padToDisplayId[padIdx] = connectorToCheck;
    }

    for (map<UINT32,UINT32>::const_iterator padIter = padToDisplayId.begin();
         padIter != padToDisplayId.end();
         padIter++)
    {
        UINT32 pairPadIdx = padIter->first ^ 1;
        if (padToDisplayId.find(pairPadIdx) != padToDisplayId.end())
        {
            m_DualSSTPairs[padIter->second] = padToDisplayId[pairPadIdx];
        }
    }

    return rc;
}

// Helper function that uses primary display ID to assign to every SOR
// in round-robin fashion, essentially stale XBAR assignments from
// previous protocol (helpful for DP->TMDS transition on multi-head)
RC EvoLwrs::ClearPreviousXBARMappings(UINT32 displayID, UINT32 sorIdxToRestore)
{
    RC rc;

    if (!GetBoundGpuSubdevice()->
        HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR))
        return OK;

    vector<UINT32> sorList;
    Display *pDisplay = GetDisplay();
    for (UINT32 idx = 0; idx < LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS; idx++)
    {
        UINT32 excludeMask = ~(1 << idx);

        // Call this function just to check if the assignment is
        // possible. The SOR will be assigned again in SetupXBar
        RC assignRc = pDisplay->AssignSOR(DisplayIDs(1, displayID),
                                          excludeMask, sorList,
                                          Display::AssignSorSetting::ONE_HEAD_MODE);

        if (assignRc == RC::LWRM_NOT_SUPPORTED)
            continue;

        CHECK_RC(assignRc);
    }

    // Now restore the assignement that should be tested:
    UINT32 excludeMask = ~(1 << sorIdxToRestore);
    CHECK_RC(pDisplay->AssignSOR(DisplayIDs(1,
        displayID), excludeMask, sorList, Display::AssignSorSetting::ONE_HEAD_MODE));

    return rc;
}

RC EvoLwrs::DetermineVariants()
{
    RC rc;
    Display *pDisplay = GetDisplay();
    m_Variant = VariantNormal;

    if (((1 << m_SetModeIdx) & m_NoVariantsMask) && !m_EnableReducedMode)
    {
        return OK;
    }

    if (m_LwrsorDisplayIsDP &&
        m_AtLeastGM10x &&
        !m_IsMultipleHeadsSingleDisplay &&
        !m_MultiHeadStressMode && // Per David Stears MST is not supported
                                  // in the 8 lanes mode
        (m_ForceDPLaneCount == 0))
    {
        // Only multi-stream capable connectors can operate in MST/SST mode
        if ((m_EnableDualSST || m_EnableDualMST) &&
            (m_AllMultiStreamCapableDPs & m_PrimaryConnector))
        {
            m_Variant = VariantSingleHeadDualSSTMST;
            return OK;
        }

        LW0073_CTRL_DFP_GET_INFO_PARAMS dFPInfo = {0};
        dFPInfo.subDeviceInstance =
            GetBoundGpuSubdevice()->GetSubdeviceInst();
        dFPInfo.displayId = m_PrimaryConnector;
        CHECK_RC(pDisplay->RmControl(LW0073_CTRL_CMD_DFP_GET_INFO,
                           &dFPInfo, sizeof(dFPInfo)));
    }

    if ((m_PrimaryType == Display::DFP) &&
        (m_PrimaryEncoder.Signal == Display::Encoder::TMDS) &&
        (m_PrimaryEncoder.Link == Display::Encoder::DUAL) &&
        (m_EvoRaster.PixelClockHz <= 165000000))
    {
        m_Variant = VariantTMDSBOnDualTMDS;
        return OK;
    }

    return OK;
}

RC EvoLwrs::Rulwariants()
{
    RC rc, sm_rc;
    Display *pDisplay = GetDisplay();

    UINT32 numVariants = 1;
    switch (m_Variant)
    {
        case VariantNormal:
            break;
        case VariantDPLanes:
        case VariantSingleHeadDualSSTMST:
            numVariants = 2;
            break;
        case VariantTMDSBOnDualTMDS:
            numVariants = 2;
            break;
        default:
            MASSERT(!"Unrecognized variant");
    }

    const UINT32 numHeads = Utility::CountBits(pDisplay->GetUsableHeadsMask());

    for (UINT32 variantIdx = 0; variantIdx < numVariants; variantIdx++)
    {
        // The detach is needed so that IMP below is performed against
        // current pstate Vmin and not some random dispclk Vmin.
        CHECK_RC(pDisplay->DetachAllDisplays());

        bool allHeadsTestedWithDualSSTandMST = true;

        m_SecondaryConnector = Display::NULL_DISPLAY_ID;
        Utility::CleanupOnReturn<EvoLwrs> restoreSingleHeadMode(this,
            &EvoLwrs::DisableSingleHeadMultistream);
        restoreSingleHeadMode.Cancel();
        Utility::CleanupOnReturn<EvoLwrs> restoreSimulatedDP(this,
            &EvoLwrs::DisableSimulatedDP);
        restoreSimulatedDP.Cancel();

        Display::SingleHeadMultiStreamMode mSM = Display::SHMultiStreamDisabled;

        switch (m_Variant)
        {
            case VariantNormal:
                CHECK_RC(pDisplay->SetForceDPLanes(0));
                break;
            case VariantSingleHeadDualSSTMST:
                {
                    MASSERT(m_PrimaryConnector & m_AllMultiStreamCapableDPs);

                    if (variantIdx == 0)
                    {
                        // Dual SST:
                        if (!m_EnableDualSST)
                            continue;
                        mSM = Display::SHMultiStreamSST;
                        MASSERT(Utility::CountBits(m_AllMultiStreamCapableDPs) > 1);

                        if (m_DualSSTPairs.find(m_PrimaryConnector) ==
                            m_DualSSTPairs.end())
                        {
                            Printf(Tee::PriNormal,
                                "Skipping dual SST coverage on 0x%08x due to "
                                "lack of a paired connector\n",
                                m_PrimaryConnector);
                            m_DPsTestedForDualSST |= m_PrimaryConnector;
                            m_SORsTestedForDualSST |= 1<<m_LwrrSorConfigIdx;
                            continue;
                        }

                        m_SecondaryConnector = m_DualSSTPairs[m_PrimaryConnector];

                        // Only for IMP:
                        m_SecondaryStreamDisplayID = m_SecondaryConnector;
                        if (m_RunEachVariantOnce &&
                                m_MinCVCoverage.IsAlreadyRun(m_PrimaryConnector, VarDualSST))
                        {
                            continue;
                        }
                    }
                    else if (variantIdx == 1)
                    {
                        // Dual MST:
                        if (!m_EnableDualMST)
                            continue;
                        mSM = Display::SHMultiStreamMST;

                        // IMP query will allocate temporary MST ids:
                        m_SecondaryStreamDisplayID = Display::NULL_DISPLAY_ID;
                        if (m_RunEachVariantOnce &&
                                m_MinCVCoverage.IsAlreadyRun(m_PrimaryConnector, VarDualMST))
                        {
                            continue;
                        }
                    }
                    else
                    {
                        MASSERT(!"variantIdx not recognized");
                        return RC::SOFTWARE_ERROR;
                    }

                    vector<Display::IMPQueryInput> impInputs;
                    Display::IMPQueryInput impInput = {
                        m_PrimaryConnector,
                        m_SecondaryStreamDisplayID,
                        mSM,
                        m_EvoRaster,
                        Display::FORCED_RASTER_ENABLED,
                        m_StressMode.Depth};

                    if (m_MultiHeadStressMode)
                    {
                        // Override format just to check IMP as one of the pixel
                        // formats used later during SetImage will be 64bpp
                        impInput.Timings.Format = m_EvoRaster.Format =
                            LW5070_CTRL_CMD_IS_MODE_POSSIBLE_PARAMS_FORMAT_RF16_GF16_BF16_AF16;
                    }
                    else
                    {
                        // In EvoLwrs::Rulwariants, surface format is forced
                        // to A8R8G8B8 when the mode is not Multi Head Stress mode
                        impInput.Timings.Format = m_EvoRaster.Format =
                            LW5070_CTRL_CMD_IS_MODE_POSSIBLE_PARAMS_FORMAT_A8R8G8B8;
                    }

                    bool IMPforMSM = false;
                    if (m_LwrsorDisplays.size() > 1)
                    {
                        for (size_t idx = 0; idx < m_LwrsorDisplays.size(); idx++)
                        {
                            impInput.PrimaryDisplay = m_LwrsorDisplays[idx];
                            impInputs.push_back(impInput);
                        }

                        // Retry IMP until 2 heads
                        INT32 numRetrys = static_cast<INT32>(m_LwrsorDisplays.size()) - 2;

                        while (numRetrys >= 0)
                        {
                            CHECK_RC(pDisplay->IsModePossible(&impInputs,
                                        &IMPforMSM));

                            if (IMPforMSM)
                                break;

                            if (numRetrys == 0 && m_MultiHeadStressMode)
                            {
                                Printf(Tee::PriError,
                                        "Mode is not Possible, can't set mode"
                                        "even with 2 heads.\n");
                                return RC::MODE_IS_NOT_POSSIBLE;
                            }
                            else
                            {
                                impInputs.pop_back();
                                if (restoreSimulatedDP.IsActive())
                                {
                                    DisableSimulatedDP();
                                }
                                m_LwrsorDisplays.pop_back();
                                --numRetrys;
                                Printf(Tee::PriLow,
                                        "Mode is not possible, will retry with"
                                        " %d heads now.\n",
                                        (UINT32)impInputs.size());
                            }
                        }

                        if (!IMPforMSM)
                        {
                            // Skip testing this display ID
                            Printf(m_LogPriority,
                                    "Skipping due to IMP: 0x%08x ... \n",
                                    m_LwrsorDisplays[0]);
                            continue;
                        }
                    }
                    else
                    {
                        impInputs.push_back(impInput);
                    }

                    if (mSM == Display::SHMultiStreamSST)
                    {
                        UINT32 simulatedDisplayIDsMask = 0;
                        CHECK_RC(pDisplay->EnableSimulatedDPDevices(
                            m_PrimaryConnector, m_SecondaryConnector,
                            Display::DPMultistreamDisabled, 1,
                            nullptr, 0, &simulatedDisplayIDsMask));
                        restoreSimulatedDP.Activate();
                        CHECK_RC(pDisplay->IsModePossible(&impInputs, &IMPforMSM));
                        if (!IMPforMSM)
                        {
                            // Skip testing this display ID
                            Printf(m_LogPriority,
                                    "Skipping dualSST due to IMP: 0x%08x ... \n",
                                    m_LwrsorDisplays[0]);
                            continue;
                        }
                        m_MinCVCoverage.SetAlreadyRun(VarDualSST);
                        m_LwrsorDisplays.clear();
                        m_LwrsorDisplays.push_back(simulatedDisplayIDsMask);

                        if (m_DPsTestedForDualSST == 0)
                        {
                            allHeadsTestedWithDualSSTandMST = false;
                        }
                    }
                    else
                    {
                        UINT32 simulatedDisplayIDsMask = 0;
                        CHECK_RC(pDisplay->EnableSimulatedDPDevices(
                            m_PrimaryConnector, Display::NULL_DISPLAY_ID,
                            Display::DPMultistreamEnabled, 2,
                            nullptr, 0, &simulatedDisplayIDsMask));
                        restoreSimulatedDP.Activate();
                        if (impInput.SingleHeadMode == Display::SHMultiStreamMST)
                        {
                            impInputs[0].SecondaryDisplay = simulatedDisplayIDsMask
                                                            & (simulatedDisplayIDsMask - 1);
                            impInputs[0].PrimaryDisplay = simulatedDisplayIDsMask
                                                          ^ impInputs[0].SecondaryDisplay;
                            CHECK_RC(pDisplay->IsModePossible(&impInputs, &IMPforMSM));
                            if (!IMPforMSM)
                            {
                                // Skip testing this display ID
                                Printf(m_LogPriority,
                                        "Skipping dualMST due to IMP: 0x%08x ... \n",
                                        m_LwrsorDisplays[0]);
                                continue;
                            }
                        }
                        m_MinCVCoverage.SetAlreadyRun(VarDualMST);
                        m_LwrsorDisplays.clear();
                        while (simulatedDisplayIDsMask)
                        {
                            UINT32 simulatedDisplayID = simulatedDisplayIDsMask
                                & ~(simulatedDisplayIDsMask - 1);
                            simulatedDisplayIDsMask ^= simulatedDisplayID;
                            if (m_TestSingleHeadMultipleStreams &&
                                (simulatedDisplayIDsMask == 0))
                            {
                                m_SecondaryStreamDisplayID = simulatedDisplayID;
                            }
                            else
                            {
                                m_LwrsorDisplays.push_back(simulatedDisplayID);
                            }
                        }

                        if (m_DPsTestedForDualMST == 0)
                        {
                            allHeadsTestedWithDualSSTandMST = false;
                        }
                    }
                    CHECK_RC(pDisplay->SetSingleHeadMultiStreamMode(
                        m_LwrsorDisplays[0], m_SecondaryStreamDisplayID, mSM));
                    restoreSingleHeadMode.Activate();
                }
                break;
            case VariantDPLanes:
                {
                    MASSERT(variantIdx < 2);
                    UINT32 DPLanes = (variantIdx == 0) ? 4 : 8;
                    if ((m_EvoRaster.PixelClockHz > 540000000) &&
                        (DPLanes < 8))
                        continue;
                    CHECK_RC(pDisplay->SetForceDPLanes(DPLanes));
                    Printf(m_LogPriority,
                            "Configuring connector 0x%08x with DP lane count ="
                            " %d\n", m_PrimaryConnector, DPLanes);
                    // Need to set it every time to reset DP lanes
                    // selection:
                    CHECK_RC(pDisplay->SetTimings(&m_EvoRaster));
                }
                break;
            case VariantTMDSBOnDualTMDS:
                {
                    MASSERT(variantIdx < 2);
                    // Skip testing unsupported TMDS single link configs
                    if (m_TestSplitSors &&
                        !IsTmdsSingleLinkCapable(
                                m_PrimaryConnector,
                                (variantIdx != 0) ? TMDS_LINK_A : TMDS_LINK_B))
                        continue;

                    string SplitSorMode = (m_TestSplitSors ?
                                                Display::GetSplitSorModeString(
                                                    (m_LwrrSplitSorMode))
                                                    : "default split SOR");
                    Printf(m_LogPriority,
                            "Configuring 0x%08x with TMDS_%s raster "
                            "settings in %s mode\n", m_PrimaryConnector,
                            (variantIdx == 0) ? "B" : "A",
                            SplitSorMode.c_str());

                    m_EvoRaster.PreferTMDSB = (variantIdx == 0);
                    CHECK_RC(pDisplay->SetTimings(&m_EvoRaster));
                }
                break;
            default:
                MASSERT(!"Unrecognized variant");
        }

        if (m_Variant != VariantSingleHeadDualSSTMST)
        {
            bool IsPrimaryConnectorPossible = false;
            Display::IMPQueryInput impInput = {m_PrimaryConnector,
                                               Display::NULL_DISPLAY_ID,
                                               mSM,
                                               m_EvoRaster,
                                               Display::FORCED_RASTER_ENABLED, m_StressMode.Depth};
            vector<Display::IMPQueryInput> impVec(1, impInput);
            CHECK_RC(pDisplay->IsModePossible(&impVec,
                                              &IsPrimaryConnectorPossible));
            if (!IsPrimaryConnectorPossible)
            {
                // Skip testing this display ID
                Printf(m_LogPriority,
                        "Skipping due to IMP: 0x%08x ... \n",
                        m_PrimaryConnector);
                continue;
            }

            if ((m_PrimaryType == Display::DFP) &&
                    (m_PrimaryEncoder.Signal == Display::Encoder::DP))
            {
                // Give priority for multihead configurations to DP 1.2 multistream.
                // Assume that with the same forced raster on all streams the CRC
                // values will be the same.
                // Assume that there are always enough heads for all MS IDs
                // per root:
                UINT32 SimulatedDisplayIDsMask = 0;
                CHECK_RC(pDisplay->EnableSimulatedDPDevices(m_PrimaryConnector,
                    Display::NULL_DISPLAY_ID,
                    m_NumAdditionalHeads ?
                        Display::DPMultistreamEnabled : Display::DPMultistreamDisabled,
                    m_NumAdditionalHeads + 1,
                    nullptr, 0, &SimulatedDisplayIDsMask));
                restoreSimulatedDP.Activate();
                bool impNonDualSSTDualMST = false;

                m_LwrsorDisplays.clear();
                impVec.clear();
                while (SimulatedDisplayIDsMask)
                {
                    UINT32 SimulatedDisplayID = SimulatedDisplayIDsMask &
                        ~(SimulatedDisplayIDsMask - 1);
                    SimulatedDisplayIDsMask ^= SimulatedDisplayID;
                    m_LwrsorDisplays.push_back(SimulatedDisplayID);
                    Display::IMPQueryInput tempIMPInput = {SimulatedDisplayID,
                                               Display::NULL_DISPLAY_ID,
                                               mSM,
                                               m_EvoRaster,
                                               Display::FORCED_RASTER_ENABLED, m_StressMode.Depth};

                   impVec.push_back(tempIMPInput);
                }
                CHECK_RC(pDisplay->IsModePossible(&impVec, &impNonDualSSTDualMST));
                if (!impNonDualSSTDualMST)
                {
                    // Skip testing this display ID
                    Printf(m_LogPriority,
                            "Skipping due to IMP: 0x%08x ... \n",
                            m_PrimaryConnector);
                    continue;
                }
                if (m_RunEachVariantOnce &&
                        m_MinCVCoverage.IsAlreadyRun(m_PrimaryConnector, VarNormalDP))
                {
                    continue;
                }
                m_MinCVCoverage.SetAlreadyRun(VarNormalDP);
            }
            else if (m_RunEachVariantOnce && m_PrimaryEncoder.Signal == Display::Encoder::TMDS)
            {
                if (m_PrimaryEncoder.Link == Display::Encoder::SINGLE &&
                        m_MinCVCoverage.IsAlreadyRun(m_PrimaryConnector, VarTmds))
                {
                    continue;
                }
                else if (m_PrimaryEncoder.Link == Display::Encoder::DUAL &&
                        m_MinCVCoverage.IsAlreadyRun(m_PrimaryConnector, VarDualTmds))
                {
                    continue;
                }
                if (m_PrimaryEncoder.Link == Display::Encoder::SINGLE)
                {
                    m_MinCVCoverage.SetAlreadyRun(VarTmds);
                }
                else if (m_PrimaryEncoder.Link == Display::Encoder::DUAL && variantIdx == 1)
                {
                    m_MinCVCoverage.SetAlreadyRun(VarDualTmds);
                }
            }
        }

        // This will skip percentage of runs from set that exelwtes
        // with RunEachVariantOnce testarg
        if (m_RunEachVariantOnce && (m_SkipXbarCombinationsPercent > 0))
        {
            UINT32 randi = m_pFpCtx->Rand.GetRandom() % 100;
            if (randi < m_SkipXbarCombinationsPercent)
                continue;
        }

        // When we query OR protocol from SelectOR function, we force
        // the secondary sublink based on the pclk and it setups the
        // Xbar accordingly. This causes the following run to start with OR
        // protocol set previously (TMDS->TMDS transition or TMDS->DP transition),
        // so it is safe to clear the XBAR mappings for each DFP entry in
        // order to start the run with correct OR protocol
        // Failing combo:
        // Consider DualTMDS variant: In first variant orProtocol=5, we force the sublink
        // secondary and setup the xbar(see EvoDisplay::SelectOR()) now orProtocol=2.
        // In second variant we start with orProtocol=2(due to OR protocol returned by
        // RM through control call LW0073_CTRL_CMD_SPECIFIC_OR_GET_INFO) and forces
        // secondary sublink again, however it should have returned 5 which is expected.
        // So with this change, second variant will start with orProtocol=5 which forces
        // any sublink.(It also helped in some golden record conflict)

        if (Display::DFP == m_PrimaryType)
        {
            CHECK_RC(ClearPreviousXBARMappings(m_PrimaryConnector,
                                               m_LwrrSorConfigIdx));
        }

        UINT32 CombinedLwrsorDisplays = 0;
        for (size_t Idx = 0; Idx < m_LwrsorDisplays.size(); Idx++)
            CombinedLwrsorDisplays |= m_LwrsorDisplays[Idx];
        CHECK_RC(pDisplay->Select(CombinedLwrsorDisplays));

        UINT32 orIndex;
        CHECK_RC(pDisplay->GetORProtocol(m_LwrsorDisplays[0],
            &orIndex, nullptr, nullptr));

        if ((GetBoundGpuSubdevice()->
                HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR)) &&
            (Display::CRT != m_PrimaryType) &&
            (orIndex != m_LwrrSorConfigIdx))
        {
            Printf(Tee::PriNormal,
                "EvoLwrs error: GetORProtocol returned "
                "unexpected SOR%d, when SOR%d was expected.\n",
                orIndex, m_LwrrSorConfigIdx);
            return RC::SOFTWARE_ERROR;
        }

        Printf(m_LogPriority, "Testing primary OR idx = %u\n", orIndex);

        UINT32 numPrimaryHeads = numHeads; // Number of heads that we want to
                                           // iterate over for max pclk testing
        if (!m_EnableMaxPclkTestingOnEachHead ||
            (m_pGolden->GetAction() == Goldelwalues::Store) ||
            (m_AllHeadsTestedAtMaxPclk && allHeadsTestedWithDualSSTandMST) ||
            (m_LwrsorDisplayIsDP && m_MultiHeadStressMode)) // All heads are used on
                                                            // DP anyway
        {
            numPrimaryHeads = 1;
        }

        for (UINT32 primaryHeadIdx = 0;
             primaryHeadIdx < numPrimaryHeads;
             primaryHeadIdx++)
        {
            // The detach is needed so that IMP below is performed against
            // current pstate Vmin and not some random dispclk Vmin.
            CHECK_RC(pDisplay->DetachAllDisplays());

            if (m_EnableMaxPclkTestingOnEachHead)
            {
                UINT32 routingMask = 0;
                for (UINT32 headIdx = 0; headIdx < numHeads; headIdx++)
                {
                    UINT32 headToTest = ((primaryHeadIdx+headIdx)%numHeads);
                    routingMask |= (m_HeadRoutingMap[headToTest] << (4 * headIdx));
                }
                CHECK_RC(pDisplay->ForceHeadRoutingMask(routingMask));
            }

            UINT32 origPixelClockHz = m_EvoRaster.PixelClockHz;
            Utility::CleanupValue<UINT32> restorePixelClockHz(m_EvoRaster.PixelClockHz, origPixelClockHz);
            restorePixelClockHz.Cancel();

            // In reduced mode just test the max pixel clock possible from IMP
            if (m_EnableReducedMode)
            {
                restorePixelClockHz.Activate();

                // Re-initialize pixel clock as it may have been set by the
                // previous display ID
                UINT32 startPclk = GetEvoLwrsPclk(m_StressMode.RasterWidth,
                    m_StressMode.RasterHeight, m_StressMode.RefreshRate);

                UINT32 endPclk = NullMaxPclkLimit;
                while (true)
                {
                    m_EvoRaster.PixelClockHz = startPclk;
                    // In the Reduced Mode there is only one stress mode.
                    // So the max pclk should always succeed:
                    Display::MaxPclkQueryInput maxPclkQuery = {pDisplay->Selected(),
                                                               &m_EvoRaster};
                    vector<Display::MaxPclkQueryInput> maxPclkQueryVector(1, maxPclkQuery);
                    CHECK_RC(pDisplay->SetRasterForMaxPclkPossible(&maxPclkQueryVector, endPclk));

                    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
                    if (pPerf->IsPState30Supported())
                    {
                        break;
                    }

                    if (OK != pPerf->SetRequiredDispClk(m_EvoRaster.PixelClockHz, false))
                    {
                        endPclk = m_EvoRaster.PixelClockHz - 1000000;
                        if (endPclk < startPclk)
                        {
                            Printf(Tee::PriNormal,
                                "EvoLwrs error: The reduced mode didn't find a working pclk\n");
                            return RC::MODE_IS_NOT_POSSIBLE;
                        }
                    }
                    else
                    {
                        // The rerun below is needed because the DispClk Volt Cap updated
                        // in SetRequiredDispClk above might have forced a lower clock
                        // and IMP requires 1MHz margin between pclk and dispclk. The below
                        // lowers pclk for example from 539.3 to 538.3.
                        Display::MaxPclkQueryInput maxPclkQuery = {pDisplay->Selected(),
                                                                   &m_EvoRaster};
                        m_EvoRaster.PixelClockHz = startPclk;
                        vector<Display::MaxPclkQueryInput> maxPclkQueryVector(1, maxPclkQuery);
                        CHECK_RC(pDisplay->SetRasterForMaxPclkPossible(&maxPclkQueryVector, endPclk));
                        break;
                    }
                }

                Printf(GetVerbosePrintPri(),
                       "Detected max pclk = %d on 0x%08x\n",
                       m_EvoRaster.PixelClockHz, pDisplay->Selected());
                CHECK_RC(pDisplay->SetTimings(&m_EvoRaster));
            }

            // Ensure that the first modeset always happens
            pDisplay->SetPendingSetModeChange();

            // This will reset the BankWidth to 0 which got set to non-zero value
            // from custom raster settings inside LoopRandomMethods
            CHECK_RC(pDisplay->SetTimings(&m_EvoRaster));
            sm_rc.Clear();
            sm_rc = pDisplay->SetMode(m_StressMode.RasterWidth,
                                      m_StressMode.RasterHeight,
                                      m_StressMode.Depth,
                                      m_StressMode.RefreshRate);

            if ((sm_rc == RC::MODE_IS_NOT_POSSIBLE) &&
                !m_EnableReducedMode && // this modeset should never fail in reduced mode
                (!m_MultiHeadStressMode ||
                 (variantIdx < (numVariants-1))))
            {
                continue;
            }

            CHECK_RC(sm_rc);

            if (m_ReportCoreVoltage)
            {
                UINT32 coreVoltage = 0;
                CHECK_RC(GetBoundGpuSubdevice()->GetPerf()->
                    GetCoreVoltageMv(&coreVoltage));
                if (coreVoltage > m_MaxCoreVoltage)
                    m_MaxCoreVoltage = coreVoltage;
                if (coreVoltage < m_MinCoreVoltage)
                    m_MinCoreVoltage = coreVoltage;
            }

            UINT32 incompatibleSecondaryDisplays = 0;
            if (m_PrimaryType == Display::DFP)
            {
                // Reread Protocol as it may have changed after first SetMode
                UINT32 OldPrimaryProtocol = m_PrimaryProtocol;
                CHECK_RC(pDisplay->GetEncoder(m_LwrsorDisplays[0],
                                              &m_PrimaryEncoder));
                m_PrimaryProtocol = m_PrimaryEncoder.Protocol;

                // In TMDS multihead mode, when testing TMDS_B raster there
                // cases when due to XBAR allocation some TMDS entries are
                // single link vs. some dual link
                // Avoid any secondary displays that are not compatible with
                // the primary display
                // Filter any secondary displays whose protocol has changed
                // after forcing TMDS raster
                if ((GetBoundGpuSubdevice()->
                     HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR)) &&
                    (m_LwrsorDisplays.size() > 1 ) &&
                    (m_PrimaryEncoder.Signal == Display::Encoder::TMDS))
                {
                    UINT32 PrimaryProtocol;
                    CHECK_RC(pDisplay->GetORProtocol(m_LwrsorDisplays[0],
                                                     nullptr,
                                                     nullptr,
                                                     &PrimaryProtocol));

                    UINT32 IncompatibleSecondaryIdxMask = 0x0;
                    for (UINT32 i = 1; i < m_LwrsorDisplays.size(); i++)
                    {
                        UINT32 SecondaryProtocol;
                        CHECK_RC(pDisplay->GetORProtocol(m_LwrsorDisplays[i],
                                                         nullptr,
                                                         nullptr,
                                                         &SecondaryProtocol));
                        if (SecondaryProtocol != PrimaryProtocol)
                        {
                            Printf(GetVerbosePrintPri(),
                                   "EvoLwrs: Skipping 0x%0x because protocol "
                                   "different from primary 0x%0x ...\n",
                                   m_LwrsorDisplays[i], m_LwrsorDisplays[0]);
                            IncompatibleSecondaryIdxMask |= 1 << i;
                        }
                    }
                    while (IncompatibleSecondaryIdxMask)
                    {
                        UINT32 idx = Utility::BitScanForward(IncompatibleSecondaryIdxMask);
                        m_LwrsorDisplays.erase(m_LwrsorDisplays.begin() + idx);
                        incompatibleSecondaryDisplays |= m_LwrsorDisplays[idx];
                        IncompatibleSecondaryIdxMask &= ~(1 << idx);
                    }
                    m_DisplaysToTest |= incompatibleSecondaryDisplays;
                }
                else if ( !m_LwrsorDisplayIsDP && // OK for DP as all heads use
                                             // the same SOR
                     (m_PrimaryProtocol != OldPrimaryProtocol) &&
                      m_LwrsorDisplays.size() > 1 )
                {
                    Printf(Tee::PriNormal,
                        "EvoLwrs: Protocol for primary display 0x%08x has changed"
                        " from %d to %d what is not supported when secondary"
                        " displays are used.\n",
                        m_LwrsorDisplays[0], OldPrimaryProtocol, m_PrimaryProtocol);
                    CHECK_RC(RC::SOFTWARE_ERROR);
                }
            }

            MASSERT(m_pGGSurfs == NULL);
            m_pGGSurfs = new GpuGoldenSurfaces(GetBoundGpuDevice());
            MASSERT(m_pGGSurfs);
            m_pGGSurfs->AttachSurface(&m_DummyGoldenSurface, "C",
                CombinedLwrsorDisplays & ~incompatibleSecondaryDisplays);
            CHECK_RC(m_pGolden->SetSurfaces(m_pGGSurfs));

            if (m_PrimaryType == Display::DFP)
            {
                const char *SignalName = "unknown";
                string variantSuffix;
                switch (m_PrimaryEncoder.Signal)
                {
                    case Display::Encoder::TMDS:
                        SignalName = "TMDS";
                        if (m_EvoRaster.PreferTMDSB)
                            variantSuffix += "_B";
                        break;
                    case Display::Encoder::LVDS:
                        SignalName = "LVDS";
                        break;
                    case Display::Encoder::DP:
                        SignalName = "DP";
                        m_LwrrentDPLaneCount = 0;
                        // Ignore RC to print "0 lanes" when function fails:
                        pDisplay->GetLwrrentDpLaneCount(m_PrimaryConnector,
                            &m_LwrrentDPLaneCount);
                        variantSuffix += Utility::StrPrintf("_%dlanes",
                            m_LwrrentDPLaneCount);

                        if ( mSM == Display::SHMultiStreamMST)
                            variantSuffix += "_MST";

                        break;
                    default:
                        MASSERT(!"Unrecognized signal");
                }

                string EncoderString =
                    Utility::StrPrintf("_%s_Protocol%d%s", SignalName,
                        m_PrimaryProtocol, variantSuffix.c_str());

                CHECK_RC(m_pGolden->OverrideSuffix(EncoderString.c_str()));
            }

            pDisplay->SetUpdateMode(Display::ManualUpdate);

            sm_rc.Clear();
            sm_rc = RunDisplay();

            if (sm_rc != RC::MODE_IS_NOT_POSSIBLE)
            {
                if (sm_rc == OK)
                {
                    UINT32 testedConnectors = pDisplay->Selected();
                    if (!(testedConnectors & m_PrimaryConnector))
                        testedConnectors =  m_PrimaryConnector;

                    m_AtLeastOneRunSuccessful = true;
                    if (!m_EnableReducedMode && // need max pclk tested each head
                        (primaryHeadIdx > 0) &&
                        (primaryHeadIdx == (numPrimaryHeads-1)))
                    {
                        m_AllHeadsTestedAtMaxPclk = true;
                    }

                    if (m_EnableMaxPclkPossibleOnEachOR)
                    {
                        m_ORsUntestedAtMaxPclk[m_LwrrSorConfigIdx] &=
                                                        ~testedConnectors;
                    }

                    if (m_Variant == VariantSingleHeadDualSSTMST)
                    {
                        switch (variantIdx)
                        {
                            case 0:
                                m_DPsTestedForDualSST |= m_PrimaryConnector;
                                m_SORsTestedForDualSST |= 1<<m_LwrrSorConfigIdx;
                                break;
                            case 1:
                                m_DPsTestedForDualMST |= m_PrimaryConnector;
                                m_SORsTestedForDualMST |= 1<<m_LwrrSorConfigIdx;
                                break;
                            default:
                                MASSERT(!"variantIdx not recognized");
                                return RC::SOFTWARE_ERROR;
                        }
                    }

                    if (m_PrimaryEncoder.EmbeddedDP)
                    {
                        m_SORsTestedForEDP |= 1<<m_LwrrSorConfigIdx;
                    }

                    m_DisplaysTestedOnMode |= testedConnectors;
                    m_DisplaysTestedOnOR[m_LwrrSorConfigIdx] |= testedConnectors;

                    if (m_MultiHeadStressMode && m_LwrsorDisplayIsDP)
                    {
                        m_MultiheadMSTTestedOnSOR[m_LwrrSorConfigIdx] = true;
                    }

                }

                Printf(m_LogPriority,
                    "Tested displays 0x%08x on primary head %u\n",
                    pDisplay->Selected(), m_HeadRoutingMap[primaryHeadIdx]);

                if (m_PrintCoverage)
                {
                    // Add display ID and split SOR setting to coverage data
                    AddOrUpdateCoverageData(pDisplay->Selected(),
                                            sm_rc == OK ? true : false);
                }

                if (m_ContinueOnFrameError)
                {
                    m_ContinuationRC = sm_rc;
                }
                else
                {
                    CHECK_RC(sm_rc);
                }
            }
            else
            {
                if (m_EnableReducedMode)
                {
                    if (!m_AllowModesetSkipInReducedMode)
                    {
                        // Zero modeset skips are expected in this test mode
                        CHECK_RC(sm_rc);
                    }
                    else
                    {
                        Printf(GetVerbosePrintPri(),
                               "\tSkipping because mode is not possible ...\n");
                    }
                }
            }

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

            // Free golden buffer.
            m_pGolden->ClearSurfaces();
            delete m_pGGSurfs;
            m_pGGSurfs = NULL;
        }
    }

    return rc;
}

RC EvoLwrs::RunOneStressMode()
{
    RC rc;
    Display *pDisplay = GetDisplay();
    UINT32 NumHeads = Utility::CountBits(pDisplay->GetUsableHeadsMask());
    Display::SplitSorConfig SorConfig;
    CHECK_RC(pDisplay->GetSupportedSplitSorConfigs(&SorConfig));
    m_DisplaysToTest = 0;
    m_DisplaysTestedOnMode = 0;

    Printf(m_LogPriority,
            "Using stress mode: Image (%d x %d),"
            "Raster (%d x %d) %dbpp @ %dHz\n",
            m_StressMode.ImageWidth,
            m_StressMode.ImageHeight,
            m_StressMode.RasterWidth,
            m_StressMode.RasterHeight,
            m_StressMode.Depth,
            m_StressMode.RefreshRate);

    for (UINT32 idx = 0; idx < m_NumSorConfigurations; idx++)
    {
        m_DisplaysToTest = m_AllConnectors & ~m_SkipDisplayMask;

        if (m_EvoRaster.PixelClockHz > 165000000)
            m_DisplaysToTest &= ~m_DualLinkTMDSNotSupportedMask;

        if (m_TestSplitSors)
        {
            // Skip split SOR configs based on the skip mask
            if (m_SkipSplitSORMask & (1 << idx))
                continue;

            // Switching between split SOR modes can hang display
            // Ensure that displays are not being driven from a
            // previous split SOR config
            pDisplay->DetachAllDisplays();

            CHECK_RC(pDisplay->ConfigureSplitSor(SorConfig[idx]));
            m_LwrrSplitSorMode = SorConfig[idx];
        }
        m_LwrrSorConfigIdx = idx;

        while (m_DisplaysToTest)
        {
            m_LwrsorDisplays.resize(1);
            m_LwrsorDisplays[0] = m_DisplaysToTest & ~(m_DisplaysToTest - 1);
            m_DisplaysToTest ^= m_LwrsorDisplays[0];

            const pair<UINT32,UINT32> comboToRun(m_LwrsorDisplays[0], idx);
            if (m_XbarCombinationsToSkip.find(comboToRun) != m_XbarCombinationsToSkip.end())
                continue;

            // If all ORs have not been tested at max pclk possible
            // skip the displays that have already been run, so we only run
            // the ones that are untested on middle resolutions *unless*
            // we are in a multi-head stress mode
            if ((m_pGolden->GetAction() == Goldelwalues::Check) &&
                (m_EnableMaxPclkPossibleOnEachOR && !m_MultiHeadStressMode &&
                 !m_IsMultipleHeadsSingleDisplay))
            {
                if (m_LwrsorDisplays[0] &
                    ~(m_ORsUntestedAtMaxPclk[m_LwrrSorConfigIdx]))
                {
                    bool missingSingleHeadMultistreamCoverage = false;
                    if (m_LwrsorDisplays[0] & m_AllMultiStreamCapableDPs)
                    {
                        if (m_EnableDualSST &&
                            (!(m_LwrsorDisplays[0] & m_DPsTestedForDualSST) ||
                             !(1<<idx & m_SORsTestedForDualSST)))
                        {
                            missingSingleHeadMultistreamCoverage = true;
                        }
                        if (m_EnableDualMST &&
                            (!(m_LwrsorDisplays[0] & m_DPsTestedForDualMST) ||
                             !(1<<idx & m_SORsTestedForDualMST)))
                        {
                            missingSingleHeadMultistreamCoverage = true;
                        }
                    }

                    if (!missingSingleHeadMultistreamCoverage && !m_RunEachVariantOnce)
                    {
                        Printf(m_LogPriority,
                                "Skipping : 0x%08x already tested @ max pclk\n",
                                m_LwrsorDisplays[0]);
                        continue;
                    }
                }
            }

            Display::ORCombinations ORCombos;
            if (m_TestSplitSors)
            {
                // Check if this is a valid display ID to test
                // and if the user has skipped this display ID
                // for the current split SOR mask
                if (IsDisplayIdSkipped(m_LwrsorDisplays[0]) ||
                    (m_SkipDisplayMaskBySplitSORList[idx] &
                                                        m_LwrsorDisplays[0]))
                {
                    // Skip testing this display ID
                    Printf(m_LogPriority,
                            "Skipping : 0x%08x ... \n", m_LwrsorDisplays[0]);
                    continue;
                }
            }

            m_PrimaryType = pDisplay->GetDisplayType(m_LwrsorDisplays[0]);

            if ((m_PrimaryType == Display::CRT) && (idx > 0))
                continue; // No need to retest CRT when only SORs are changing

            Utility::CleanupOnReturnArg<EvoLwrs, void, UINT32>
                restoreSORExcludeMask(this, &EvoLwrs::CleanSORExcludeMask,
                    m_LwrsorDisplays[0]);
            restoreSORExcludeMask.Cancel();

            if (GetBoundGpuSubdevice()->
                HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR))
            {
                // Need to detach to free up the the SOR xbar:
                pDisplay->DetachAllDisplays();

                // CRTs do not work with SOR XBAR
                if (Display::CRT != m_PrimaryType)
                {
                    UINT32 excludeMask = ~(1 << idx);
                    vector<UINT32> sorList;
                    // Call this function just to check if the assignment is
                    // possible. The SOR will be assigned again in SetupXBar
                    RC assignRc = pDisplay->AssignSOR(DisplayIDs(1,
                        m_LwrsorDisplays[0]), excludeMask, sorList,
                        Display::AssignSorSetting::ONE_HEAD_MODE);

                    if (assignRc == RC::LWRM_NOT_SUPPORTED)
                        continue;

                    CHECK_RC(assignRc);

                    restoreSORExcludeMask.Activate();
                    // Need to call SetSORExcludeMask here as GetORProtocol
                    // calls SetupXBar now.
                    CHECK_RC(pDisplay->SetSORExcludeMask(
                        m_LwrsorDisplays[0], excludeMask));

                    UINT32 sorIndex;
                    CHECK_RC(pDisplay->GetORProtocol(m_LwrsorDisplays[0],
                        &sorIndex, nullptr, nullptr));

                    if (sorIndex != idx)
                    {
                        Printf(Tee::PriNormal,
                            "EvoLwrs error: GetORProtocol returned "
                            "unexpected SOR%d, when SOR%d was expected.\n",
                            sorIndex, idx);
                        return RC::SOFTWARE_ERROR;
                    }
                }
            }

            // Need to set it every time for GetORNameIfUnique:
            CHECK_RC(pDisplay->SetTimings(&m_EvoRaster));

            m_PrimaryProtocol = LW0073_CTRL_SPECIFIC_OR_PROTOCOL_UNKNOWN;
            m_PrimaryConnector = m_LwrsorDisplays[0];
            string PrimaryCRCName;
            CHECK_RC(pDisplay->GetORNameIfUnique(m_LwrsorDisplays[0], PrimaryCRCName));

            bool isEDP = false;

            switch (m_PrimaryType)
            {
                case Display::CRT:
                    break;
                case Display::DFP:
                    CHECK_RC(pDisplay->GetEncoder(m_LwrsorDisplays[0],
                        &m_PrimaryEncoder));

                    if (m_PrimaryEncoder.Signal == Display::Encoder::LVDS)
                    {
                        // Ignore the error code as on some systems RM doesn't
                        // like forced SOR settings - bug 876125
                        pDisplay->SetLVDSDualLink(m_LwrsorDisplays[0], true);
                    }

                    m_PrimaryProtocol = m_PrimaryEncoder.Protocol;

                    isEDP = m_PrimaryEncoder.EmbeddedDP;

                    // Bug 1519247: Testing DUAL link TMDS at pclk = 1040 MHz can cause
                    // successive modesets to timeout on slow GM204 parts.
                    // Limit dual link TMDS to 680 MHz (i.e. HDMI 1.4 dual link speeds)
                    // Display HW team is okay with skipping dual link HDMI 2.0 resolutions
                    if (GetBoundGpuSubdevice()->HasBug(1519247) &&
                        (m_PrimaryEncoder.Signal == Display::Encoder::TMDS) &&
                        (m_PrimaryEncoder.Link == Display::Encoder::DUAL) &&
                        (m_EvoRaster.PixelClockHz > 680000000))
                    {
                        Printf(m_LogPriority,
                               "Skipping dual link TMDS for pclk > 680 MHz on 0x%08x"
                               " due to bug 1519247 ... \n",
                               m_PrimaryConnector);
                        continue;
                    }
                    break;
                default:
                    CHECK_RC(RC::SOFTWARE_ERROR);
            }

            m_NumAdditionalHeads =
                (m_EnableMultiHead && m_MultiHeadStressMode && m_AtLeastGT21x && !isEDP) ?
                    NumHeads-1 : 0;

            m_TestSingleHeadMultipleStreams = m_EnableDualMST &&
                (m_NumAdditionalHeads == 0) && !isEDP && !m_IsMultipleHeadsSingleDisplay;

            // Check which links does the primary display support
            // when testing single link TMDS in dual TMDS mode
            bool IsTMDSLinkASupported = false;
            bool IsTMDSLinkBSupported = false;
            if (m_TestSplitSors &&
                (m_PrimaryEncoder.Signal == Display::Encoder::TMDS) &&
                (m_PrimaryEncoder.Link == Display::Encoder::DUAL) &&
                (m_EvoRaster.PixelClockHz <= 165000000))
            {
                IsTMDSLinkASupported = IsTmdsSingleLinkCapable(
                                                            m_PrimaryConnector,
                                                            TMDS_LINK_A);

                IsTMDSLinkBSupported = IsTmdsSingleLinkCapable(
                                                            m_PrimaryConnector,
                                                            TMDS_LINK_B);
            }

            if ( (m_PrimaryType != Display::DFP) ||
                 (m_PrimaryEncoder.Signal != Display::Encoder::DP) )
            {
                UINT32 PotentialCombinedLwrsorDisplays = m_LwrsorDisplays[0];
                UINT32 SecondaryDisplays = m_DisplaysToTest;
                while (SecondaryDisplays && m_NumAdditionalHeads)
                {
                    UINT32 SecondaryDisplay =
                    SecondaryDisplays & ~(SecondaryDisplays - 1);
                    SecondaryDisplays ^= SecondaryDisplay;

                    if (pDisplay->GetDisplayType(SecondaryDisplay) !=
                        m_PrimaryType)
                        continue;

                    if (m_TestSplitSors)
                    {
                        // Check if this is a valid display ID to test
                        // and if the user has skipped this display ID
                        // for the current split SOR mask
                        if (IsDisplayIdSkipped(SecondaryDisplay) ||
                            (m_SkipDisplayMaskBySplitSORList[idx] &
                                                            SecondaryDisplay))
                        {
                            // Skip testing this display ID
                            Printf(m_LogPriority,
                                    "Skipping : 0x%08x ... \n",
                                    SecondaryDisplay);
                            continue;
                        }
                    }

                    string SecondaryCRCName;
                    CHECK_RC(pDisplay->GetORNameIfUnique(SecondaryDisplay, SecondaryCRCName));
                    if (PrimaryCRCName == SecondaryCRCName)
                    {
                        if (m_PrimaryType == Display::DFP)
                        {
                            Display::Encoder SecondaryEncoder;
                            CHECK_RC(pDisplay->GetEncoder(SecondaryDisplay, &SecondaryEncoder));

                            if ( (SecondaryEncoder.Signal !=
                                    m_PrimaryEncoder.Signal) ||
                                    (SecondaryEncoder.Protocol !=
                                    m_PrimaryProtocol) )
                                continue;

                            // In split SOR mode, all secondary displays
                            // may not support both single TMDS_A and
                            // TMDS_B. Skip those which are not TMDS single
                            // link compatible with primary display ID
                            if (m_TestSplitSors &&
                                (m_PrimaryEncoder.Signal ==
                                                    Display::Encoder::TMDS) &&
                                (m_PrimaryEncoder.Link ==
                                                    Display::Encoder::DUAL) &&
                                (m_EvoRaster.PixelClockHz <= 165000000))
                            {
                                if ((IsTMDSLinkASupported &&
                                    !IsTmdsSingleLinkCapable(
                                                            SecondaryDisplay,
                                                            TMDS_LINK_A)) ||
                                    (IsTMDSLinkBSupported &&
                                    !IsTmdsSingleLinkCapable(
                                                            SecondaryDisplay,
                                                            TMDS_LINK_B)))
                                    continue;
                            }
                        }
                        if (pDisplay->Select(PotentialCombinedLwrsorDisplays |
                                                SecondaryDisplay) != OK)
                        {
                            continue;
                        }
                        m_LwrsorDisplays.push_back(SecondaryDisplay);
                        PotentialCombinedLwrsorDisplays |= SecondaryDisplay;
                        m_DisplaysToTest ^= SecondaryDisplay;
                        m_NumAdditionalHeads--;
                    }
                }
            }

            m_LwrsorDisplayIsDP = (m_PrimaryType == Display::DFP) &&
                (m_PrimaryEncoder.Signal == Display::Encoder::DP);

            //Note: Display HW does not support dual link HDMI
            m_LwrsorDisplayIsHDMI = (m_PrimaryType == Display::DFP) &&
                (m_PrimaryEncoder.Signal == Display::Encoder::TMDS) &&
                ((m_PrimaryEncoder.Link == Display::Encoder::SINGLE) && (m_EvoRaster.PixelClockHz > 165000000));

            if (m_TestSplitSors)
            {
                Printf(m_LogPriority,
                        "Testing Split SOR mode %s \n",
                        Display::GetSplitSorModeString(
                                                m_LwrrSplitSorMode).c_str());
            }

            //Note: From HW documentation, DualTMDS works only with 24
            // and _DEFAULT pixel depths
            if (m_Use16BitDepthAtOR &&
                    (m_PrimaryEncoder.Signal == Display::Encoder::TMDS) &&
                    (m_PrimaryEncoder.Link == Display::Encoder::DUAL)
               )
            {
                continue;
            }
            m_LwrsorDisplayIsLVDS = (m_PrimaryType == Display::DFP) && (m_PrimaryEncoder.Signal == Display::Encoder::LVDS);

            if (!m_EnableXbarCombinations && (m_PrimaryType == Display::DFP))
            {
                if (m_DisplaysTestedOnMode & m_LwrsorDisplays[0])
                {
                    bool skipSor = false;
                    if (m_LwrsorDisplayIsDP && m_MultiHeadStressMode)
                    {
                        skipSor = m_MultiheadMSTTestedOnSOR[m_LwrrSorConfigIdx];
                    }
                    else
                    {
                        skipSor = (m_LwrsorDisplays[0] & m_DisplaysTestedOnOR[m_LwrrSorConfigIdx]) != 0;
                    }

                    if (skipSor)
                    {
                        Printf(m_LogPriority,
                                "Skipping : 0x%08x on or = %d\n",
                                m_LwrsorDisplays[0], idx);
                        continue;
                    }
                }
            }

            CHECK_RC(DetermineVariants());
            CHECK_RC(Rulwariants());

        } /* end of displays ID test loop */
    } /* end of split SOR test loop */

    return rc;
}

RC EvoLwrs::RunWithoutFinalCleanup
(
    UINT32 *pLVDSDisplayId,
    bool *pOrgLVDSDualLinkEnable
)
{
    RC rc;
    Display *pDisplay = GetDisplay();
    m_MaxCoreVoltage = 0;
    m_MinCoreVoltage = 0xFFFFFFFF;

    m_DummyGoldenSurface.SetWidth(160);
    m_DummyGoldenSurface.SetHeight(120);
    m_DummyGoldenSurface.SetColorFormat(ColorUtils::R8G8B8A8);
    CHECK_RC(m_DummyGoldenSurface.Alloc(GetBoundGpuDevice()));

    CHECK_RC(pDisplay->GetPrimaryConnectors(&m_AllConnectors));

    CHECK_RC(ValidateConnectorsPresence(m_AllConnectors));

    m_DualLinkTMDSNotSupportedMask = 0;
    m_AllCrtsMask = 0;
    m_AllDPsMask = 0;
    m_SingleLinkEmbeddedDPsMask = 0;
    m_AllMultiStreamCapableDPs = 0;
    m_DPsTestedForDualSST = 0;
    m_SORsTestedForDualSST = 0;
    m_DPsTestedForDualMST = 0;
    m_SORsTestedForDualMST = 0;
    m_SORsTestedForEDP = 0;
    UINT32 ConnectorsToCheck = m_AllConnectors & ~m_SkipDisplayMask;

    // Test the chip's split SOR cofiguration if supported
    Display::SplitSorConfig SorConfig;
    CHECK_RC(pDisplay->GetSupportedSplitSorConfigs(&SorConfig));

    m_TestSplitSors = ((SorConfig.size() != 0) && m_EnableSplitSOR);
    m_NumSorConfigurations =
        (m_TestSplitSors ?
            static_cast<UINT32>(SorConfig.size()) :
            ( GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR) ?
                LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS :
                1));

    if (m_TestSplitSors)
    {
        // Fetch any per Split SOR skip display masks in split SOR mode
        CHECK_RC(ParseSkipDisplayMaskBySplitSOR());

        // Bug 1414842
        // (split SOR does not repro failure, but disabling split SOR does)
        // Rootcause:
        // Sometimes a GPU may have a non-default split SOR before test 4 starts
        // This will cause "IsDisplayIdSkipped" to skip valid display IDs
        // because "GetValidORCombinations" depends on the current split SOR
        // mode of the GPU. The end result is that even *before* configuring
        // the 1st split SOR mode we have skipped some display IDs !
        // Solution:
        // Force the first split SOR config (default mode)
        CHECK_RC(pDisplay->ConfigureSplitSor(SorConfig[DefaultSplitSOR]));
    }

    vector<pair<UINT32, UINT32> > combosToRun;
    while (ConnectorsToCheck)
    {
        UINT32 ConnectorToCheck = ConnectorsToCheck & ~(ConnectorsToCheck - 1);
        ConnectorsToCheck ^= ConnectorToCheck;

        if (m_TestSplitSors)
        {
            // Skip invalid display IDs based on current split SOR config
            // Also assume the m_SkipDisplayMaskBySplitSORList[0]
            // is the default non-split SOR mode
            if (IsDisplayIdSkipped(ConnectorToCheck) ||
                (m_SkipDisplayMaskBySplitSORList[DefaultSplitSOR] &
                                                              ConnectorToCheck))
            {
                m_SkipDisplayMask |= ConnectorToCheck;
                Printf(m_LogPriority, "Adding 0x%08x to skip mask ... \n", ConnectorToCheck);
                continue;
            }
        }

        Display::DisplayType displayType = pDisplay->GetDisplayType(ConnectorToCheck);
        if (displayType == Display::CRT)
        {
            m_AllCrtsMask |= ConnectorToCheck;
        }
        else if (displayType == Display::DFP)
        {
            Display::Encoder Encoder;
            CHECK_RC(pDisplay->GetEncoder(ConnectorToCheck, &Encoder));

            if (Encoder.ORType != LW0073_CTRL_SPECIFIC_OR_TYPE_SOR)
            {
                // We don't test PIORs here
                m_SkipDisplayMask |= ConnectorToCheck;
                continue;
            }

            if (Encoder.Signal == Display::Encoder::DP)
            {
                if (!m_AtLeastGF11x || m_SkipDisplayPort)
                {
                    // Older GPUs don't support the faked DP link training
                    m_SkipDisplayMask |= ConnectorToCheck;
                }
                else
                {
                    m_AllDPsMask |= ConnectorToCheck;
                    if (Encoder.EmbeddedDP &&
                        (Encoder.Link != Display::Encoder::DUAL))
                    {
                        m_SingleLinkEmbeddedDPsMask |= ConnectorToCheck;
                    }
                }
            }

            // Bug 423590: Inconsistent CRC on SOR1 and SOR2 for dual-link TMDS
            if (GetBoundGpuSubdevice()->HasBug(423590))
            {
                if ( !((Encoder.ActiveLink.Primary == Display::Encoder::LINK_A) &&
                       (Encoder.ActiveLink.Secondary == Display::Encoder::LINK_B)) &&
                     (Encoder.Signal == Display::Encoder::TMDS) )
                    m_DualLinkTMDSNotSupportedMask |= ConnectorToCheck;
            }

            if (Encoder.Signal == Display::Encoder::LVDS)
            {
                *pLVDSDisplayId = ConnectorToCheck;
                CHECK_RC(pDisplay->GetLVDSDualLink(ConnectorToCheck,
                    pOrgLVDSDualLinkEnable));
            }
            else
            {
                for (UINT32 idx = 0; idx < m_NumSorConfigurations; idx++)
                {
                    const pair<UINT32,UINT32> comboToRun(ConnectorToCheck, idx);

                    // only use one SOR when generating goldens. for eDP, try all of
                    //   them since RM will only let you assign certain SORs.
                    if ( (m_pGolden->GetAction() == Goldelwalues::Store) &&
                         idx != 0 &&
                         !Encoder.EmbeddedDP
                       )
                    {
                        m_XbarCombinationsToSkip.insert( comboToRun );
                    }
                    else
                    {
                        combosToRun.push_back(comboToRun);
                    }
                }
            }
        }
    }

    const UINT32 skipSize = static_cast<UINT32>(combosToRun.size() *
        (m_SkipXbarCombinationsPercent/100.0f));
    for (UINT32 i = 0; i < skipSize; i++)
    {
        const UINT32 randi = m_pFpCtx->Rand.GetRandom() % combosToRun.size();
        vector<pair<UINT32, UINT32> >::iterator it = combosToRun.begin() + randi;
        m_XbarCombinationsToSkip.insert(*it);
        combosToRun.erase(it);
    }

    // Don't use predetermined set with RunEachVariantOnce, some combos
    // might not have run during gpugen
    if (m_RunEachVariantOnce && (m_SkipXbarCombinationsPercent > 0))
    {
        m_XbarCombinationsToSkip.clear();
    }

    m_AllMultiStreamCapableDPs = m_AllDPsMask ^ m_SingleLinkEmbeddedDPsMask;
    m_EnableDualSST &= (Utility::CountBits(m_AllMultiStreamCapableDPs) > 1);
    CHECK_RC(DetermineDualSSTPairs());
    m_AtLeastOneRunSuccessful = false;
    m_AllHeadsTestedAtMaxPclk = false;

    // Initialize the ORs that are yet untested
    m_ORsUntestedAtMaxPclk.clear();
    m_MultiheadMSTTestedOnSOR.clear();
    m_DisplaysTestedOnOR.clear();
    for (UINT32 idx = 0; idx < m_NumSorConfigurations; idx++)
    {
        m_MultiheadMSTTestedOnSOR.push_back(false);
        m_DisplaysTestedOnOR.push_back(0);

        if (m_EnableMaxPclkPossibleOnEachOR)
        {
            if (m_SkipSplitSORMask & (1 << idx))
            {
                // If this split SOR mode was skipped, there's nothing to test
                m_ORsUntestedAtMaxPclk.push_back(0);
                continue;
            }
            else
            {
                // Apply global and split-SOR mode specific masks
                UINT32 allConnectorsInThisConf = m_AllConnectors &
                    ~m_SkipDisplayMask & ~m_SkipDisplayMaskBySplitSORList[idx];
                if (idx != 0)
                    allConnectorsInThisConf &= ~m_AllCrtsMask;
                m_ORsUntestedAtMaxPclk.push_back(allConnectorsInThisConf);
            }
        }
    }

    Utility::CleanupOnReturnArg<Display, void, Display::ORColorSpace>
            RestoreDefaultColorspace(pDisplay, &Display::SetDefaultColorspace,
                                         Display::ORColorSpace::RGB);
    for (UINT32 StressModeIdx = 0;
         StressModeIdx < m_StressModes.size();
         StressModeIdx++)
    {
        m_StressMode = m_StressModes[StressModeIdx];
        m_Use16BitDepthAtOR = (((1 << StressModeIdx) & m_BPP16ModesMask)? true : false)
                                                     && (m_StressMode.Depth == 16); // NOT reviewed by kamild

        // From hardware documentation, 16 bit pixel depth at OR is not supported by
        // our default RGB colorspace. Modify it to YUV709
        if (m_Use16BitDepthAtOR)
        {
            pDisplay->SetDefaultColorspace(Display::ORColorSpace::YUV_709);
        }
        else
        {
            pDisplay->SetDefaultColorspace(Display::ORColorSpace::RGB);
        }

        m_MultiHeadStressMode = true;
        if (m_NumMultiHeadStressModes < m_StressModes.size())
            m_MultiHeadStressMode = (StressModeIdx >=
                (m_StressModes.size() - m_NumMultiHeadStressModes));

        if (m_StressMode.RasterWidth >= Display::HEAD_SCANOUT_LIMIT ||
                m_StressMode.RasterHeight >= Display::HEAD_SCANOUT_LIMIT)
            m_IsMultipleHeadsSingleDisplay = true;
        else
            m_IsMultipleHeadsSingleDisplay = false;

        if (m_IsMultipleHeadsSingleDisplay && !m_AtLeastGP10x)
            continue;

        // Skip if there is not enough head available to drive this stress mode
        if ((UINT32)Utility::CountBits(m_HeadMask) <
                (m_StressMode.RasterWidth / Display::HEAD_SCANOUT_LIMIT + 1) ||
            (UINT32)Utility::CountBits(m_HeadMask) <
                (m_StressMode.RasterHeight / Display::HEAD_SCANOUT_LIMIT + 1))
        {
            Printf(Tee::PriHigh, "Not sufficient heads available to drive the "
                    "raster %ux%u\n", m_StressMode.RasterWidth, m_StressMode.RasterHeight);
            continue;
        }

        if ((m_pGolden->GetAction() == Goldelwalues::Check) &&
            m_AtLeastOneRunSuccessful)
        {
            bool missingSingleHeadMultistreamCoverage = false;
            if (m_EnableDualSST &&
                (m_AllMultiStreamCapableDPs ^ m_DPsTestedForDualSST))
            {
                missingSingleHeadMultistreamCoverage = true;
            }
            if (m_EnableDualMST &&
                (m_AllMultiStreamCapableDPs ^ m_DPsTestedForDualMST))
            {
                missingSingleHeadMultistreamCoverage = true;
            }
            // Skip middle resolutions, test only the the highest pclk
            // and all multi head modes:
            if (!m_MultiHeadStressMode && !m_EnableMaxPclkPossibleOnEachOR &&
                !m_IsMultipleHeadsSingleDisplay)
                continue;

            if (!m_MultiHeadStressMode &&
                m_EnableMaxPclkPossibleOnEachOR &&
                !missingSingleHeadMultistreamCoverage &&
                !m_IsMultipleHeadsSingleDisplay)
            {
                UINT32 ORsUntestedAtMaxPclkAcrossModes = 0x0;
                for (UINT32 idx = 0; idx < m_NumSorConfigurations; idx++)
                {
                    ORsUntestedAtMaxPclkAcrossModes |=
                                                    m_ORsUntestedAtMaxPclk[idx];
                }
                // All ORs were tested on across all split SOR Modes
                if (ORsUntestedAtMaxPclkAcrossModes == 0x0)
                {
                    m_EnableMaxPclkPossibleOnEachOR = false;
                    continue;
                }
            }
        }

        // Custom raster to create display independent stressful conditions:
        // (no raster scaling)
        CreateRaster(m_StressMode.RasterWidth, m_StressMode.RasterHeight,
                     m_StressMode.RasterWidth, m_StressMode.RasterHeight,
                     m_StressMode.RefreshRate,
                     m_ForceDPLaneCount, m_ForceDPLinkBW, &m_EvoRaster);
        if (m_Use16BitDepthAtOR)
        {
            m_EvoRaster.ORPixelDepthBpp = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_16_422;
            m_EvoRaster.Format = LW5070_CTRL_CMD_IS_MODE_POSSIBLE_PARAMS_FORMAT_A1R5G5B5;
        }
        if ((m_MaxPixelClockHz > 0) &&
            (m_EvoRaster.PixelClockHz > m_MaxPixelClockHz))
            continue;

        // 5120 resolution fails on GT21x in PVS - it probably makes
        // no sense to spend time on debugging that:
        if ((m_StressMode.RasterWidth >= GM10xMaxPRUResolutionWidth) &&
            !m_AtLeastGF11x)
            continue;

        m_SetModeIdx = StressModeIdx;
        m_MinCVCoverage.Reset();
        CHECK_RC(RunOneStressMode());

    } /* end of stress mode test loop */

    if (m_ReportCoreVoltage)
    {
        Printf(Tee::PriNormal,
            "EvoLwrs: observed core voltage min = %d mV, max = %d mV\n",
            m_MinCoreVoltage, m_MaxCoreVoltage);
    }

    if (m_PrintCoverage)
    {
        Printf(Tee::PriNormal,
               "======== EvoLwrs test coverage ========\n");
        for (map<UINT32, vector<DispCoverageInfo> >::iterator SplitSorIt =
                                                         m_CoverageData.begin();
             SplitSorIt != m_CoverageData.end(); SplitSorIt++)
        {
            if (GetBoundGpuSubdevice()->HasFeature(
                    Device::GPUSUB_SUPPORTS_SOR_XBAR))
            {
                if (m_CoverageData.size() > 1)
                {
                    Printf(Tee::PriNormal, "SOR idx: %u\n", SplitSorIt->first);
                }
                else
                {
                    Printf(Tee::PriNormal, "SOR idx: unrestricted\n");
                }
            }
            else
            {
                string SplitSorMode = (m_TestSplitSors ?
                                   Display::GetSplitSorModeString(
                                                   SorConfig[SplitSorIt->first])
                                                   : "default split SOR");
                Printf(Tee::PriNormal, "SplitSORMode: %s\n", SplitSorMode.c_str());
            }

            for (vector<DispCoverageInfo>::iterator DispCovIt =
                                                   (SplitSorIt->second).begin();
                 DispCovIt != (SplitSorIt->second).end(); DispCovIt++)
            {
                Printf(Tee::PriNormal, "\tDisplay: 0x%08x", DispCovIt->Display);
                Printf(Tee::PriNormal,
                       DispCovIt->IsTestPassing ? " - PASS\n" : " - FAIL\n");
            }
        }
        Printf(Tee::PriNormal, "=======================================\n");
    }

    CHECK_RC(m_ContinuationRC);

    if (m_AtLeastOneRunSuccessful == false)
        CHECK_RC(RC::MODE_IS_NOT_POSSIBLE);

    if (m_EnableDualSST && !m_EnableReducedMode && (m_SkipXbarCombinationsPercent == 0) &&
            !m_RunEachVariantOnce)
    {
        if (m_AllMultiStreamCapableDPs ^ m_DPsTestedForDualSST)
        {
            Printf(Tee::PriNormal,
                "Coverage error: not all connectors tested for 2xSST\n");
            CHECK_RC(RC::MODE_IS_NOT_POSSIBLE);
        }
        if (m_EnableXbarCombinations &&
            ((m_pGolden->GetAction() == Goldelwalues::Store)? 1 : INT32(m_NumSorConfigurations)) >
                Utility::CountBits(m_SORsTestedForDualSST))
        {
            Printf(Tee::PriNormal,
                "Coverage error: not all SORs tested for 2xSST\n");
            CHECK_RC(RC::MODE_IS_NOT_POSSIBLE);
        }
    }
    if (m_EnableDualMST && !m_EnableReducedMode && (m_SkipXbarCombinationsPercent == 0) &&
            !m_RunEachVariantOnce)
    {
        if (m_AllMultiStreamCapableDPs ^ m_DPsTestedForDualMST)
        {
            Printf(Tee::PriNormal,
                "Coverage error: not all connectors tested for 2xMST\n");
            CHECK_RC(RC::MODE_IS_NOT_POSSIBLE);
        }
        if (m_AllMultiStreamCapableDPs && m_EnableXbarCombinations &&
            (((m_pGolden->GetAction() == Goldelwalues::Store)? 1 : INT32(m_NumSorConfigurations)) >
                Utility::CountBits(m_SORsTestedForDualMST|m_SORsTestedForEDP)))
        {
            Printf(Tee::PriNormal,
                "Coverage error: not all SORs tested for 2xMST\n");
            CHECK_RC(RC::MODE_IS_NOT_POSSIBLE);
        }
    }

    return rc;
}

RC EvoLwrs::Run()
{
    StickyRC rc;
    GpuDevice * pGpuDev = GetBoundGpuDevice();
    UINT32 origSimulatedDpDevices = 0;
    Display *pDisplay = GetDisplay();
    const UINT32 OrigDisplay = pDisplay->Selected();
    UINT32 OrigSorIdx;
    CHECK_RC(pDisplay->GetORProtocol(OrigDisplay, &OrigSorIdx, nullptr, nullptr));
    Display::UpdateMode OrigUpdateMode = pDisplay->GetUpdateMode();
    Tee::Priority OrigIMP = pDisplay->SetIsModePossiblePriority(Tee::PriLow);
    const UINT32 OrigHeadRoutingMask = pDisplay->GetForcedHeadRoutingMask();
    Display::Encoder origEncoder;
    CHECK_RC(pDisplay->GetEncoder(OrigDisplay,&origEncoder));

    m_LogPriority = (m_pTestConfig->Verbose() ? Tee::PriNormal : Tee::PriLow);
    if (m_RunEachVariantOnce)
        m_MinCVCoverage.SetPrintPri(m_LogPriority);

    UINT32 LVDSDisplayId = 0;
    bool OrgLVDSDualLinkEnable = true;
    bool OrgEnableDefaultImage = pDisplay->GetEnableDefaultImage();
    m_ContinuationRC.Clear();

    // In order to use resolutions like 5120x60 the default image has to be
    // disabled. Otherwise IMP returns false because scaling from 640x480
    // image to the above raster is beyond hw capabilities.
    rc = pDisplay->SetEnableDefaultImage(false);

    m_pGolden->SetCheckCRCsUnique(true);

    // CL 9735040 has brought optimization into DP SetModes to not detach SOR
    // and detrain it if the next SetMode stays with DP protocol on the same SOR.
    // This was needed for multistream support.
    // Bug 918271 has exposed a side effect of this that if a real DP panel is
    // connected and used as primary display, when EvoLwrs test starts and the
    // first tested connector happens to be same where the primary panel is,
    // the faked DP training is skipped, leaving for example only one DP lane
    // active, where 4 lanes are really needed during the test. This was causing
    // random values on the primary CRC.
    rc = pDisplay->DetachAllDisplays();

    rc = pDisplay->DisableAllSimulatedDPDevices(Display::SkipRealDisplayDetection,
                                                &origSimulatedDpDevices);

    rc = pDisplay->SetBlockHotPlugEvents(true);

    rc = RunWithoutFinalCleanup(&LVDSDisplayId, &OrgLVDSDualLinkEnable);

    // Final cleanup:
    rc = pDisplay->DetachAllDisplays();
    rc = pDisplay->SendUpdate
    (
        true,       // Core
        0xFFFFFFFF, // All bases
        0xFFFFFFFF, // All lwrsors
        0xFFFFFFFF, // All overlays
        0xFFFFFFFF, // All overlaysIMM
        true,       // Interlocked
        true        // Wait for notifier
    );
    pDisplay->SetUpdateMode(OrigUpdateMode);

    rc = pDisplay->SetBlockHotPlugEvents(false);

    rc = pDisplay->DetectRealDisplaysOnAllConnectors();

    // restart any simulated dp devices ('-simulate_dfp')
    pGpuDev->SetupSimulatedEdids(origSimulatedDpDevices);

    pDisplay->SetIsModePossiblePriority(OrigIMP);

    rc = pDisplay->SetEnableDefaultImage(OrgEnableDefaultImage);

    // Restore the default Split SOR configuration if needed
    if ((m_DefaultSplitSorReg.size() > 0) &&
        (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_SPLIT_SOR)))
    {
        Display *pDisplay = GetBoundGpuDevice()->GetDisplay();
        rc = pDisplay->ConfigureSplitSor(m_DefaultSplitSorReg);
        m_LwrrSplitSorMode.clear();
    }

    // Effectively resets all OR protocols so that OrigDisplay can come back
    // with the original OR protocol - see bug 1599130
    if ((pDisplay->GetDisplayType(OrigDisplay) != Display::CRT) &&
        !(GetBoundGpuSubdevice()->HasBug(200100122) &&
          origEncoder.EmbeddedDP))
    {
        rc = ClearPreviousXBARMappings(OrigDisplay, OrigSorIdx);
    }
    rc = pDisplay->Select(OrigDisplay);

    // Head routing restore has to be after the Select because it internally
    // calls Selected() and expect this value to be valid. Just after
    // DP MST testing the display ids are no longer valid.
    if (m_EnableMaxPclkTestingOnEachHead)
        rc = pDisplay->ForceHeadRoutingMask(OrigHeadRoutingMask);

    rc = pDisplay->SetTimings(NULL);

    if (LVDSDisplayId != 0)
    {
        // Ignore the error code as on some systems RM doesn't
        // like forced SOR settings - bug 876125
        pDisplay->SetLVDSDualLink(LVDSDisplayId,
            OrgLVDSDualLinkEnable);
    }

    rc = pDisplay->SetMode(m_InitMode.RasterWidth,
                           m_InitMode.RasterHeight,
                           m_InitMode.Depth,
                           m_InitMode.RefreshRate);

    // Free golden buffer.
    m_pGolden->ClearSurfaces();
    delete m_pGGSurfs;
    m_pGGSurfs = NULL;

    m_DummyGoldenSurface.Free();

    return rc;
}

RC EvoLwrs::Cleanup()
{
    RC rc, firstRc;

    for (map<UINT64, pair<Surface2D*,Surface2D*> >::const_iterator BgI
             = m_Backgrounds.begin();
         BgI != m_Backgrounds.end();
         BgI++)
    {
        delete (BgI->second.first);
        delete (BgI->second.second);
    }
    m_Backgrounds.clear();

    for (UINT32 i = 0; i < 2; i++)
    {
        for (UINT32 j = 0; j < 2; j++)
        {
            m_LwrsorImages[i][j].Free();
        }
    }

    if (m_EnableDMA)
        FIRST_RC(m_DmaWrap.Cleanup());

    m_PStateOwner.ReleasePStates();
    FIRST_RC(GpuTest::Cleanup());

    return firstRc;
}

bool EvoLwrs::IsSupported()
{
    GpuDevice * pGpuDev = GetBoundGpuDevice();
    Display *pDisplay = GetDisplay();

    if (pDisplay->GetDisplayClassFamily() != Display::EVO)
    {
        // Need an EVO display.
        return false;
    }

    if (GetBoundGpuSubdevice()->GetPerf()->IsOwned())
    {
        Printf(Tee::PriNormal, "Skipping %s because another test is holding pstates\n",
                GetName().c_str());
        return false;
    }

    if (pGpuDev->GetNumSubdevices() > 1)
    {
        return false;
    }

    return true;
}

RC EvoLwrs::SetDefaultsForPicker(UINT32 idx)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();

    switch (idx)
    {
        case FPK_EVOLWRS_LWRSOR_POSITION:
            // Position of the cursor.
            (*pPickers)[FPK_EVOLWRS_LWRSOR_POSITION].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_LWRSOR_POSITION].AddRandItem(2, ScreenRandom);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_POSITION].AddRandItem(1, Top);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_POSITION].AddRandItem(1, Bottom);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_POSITION].AddRandItem(1, Left);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_POSITION].AddRandItem(1, Right);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_POSITION].AddRandItem(1, TopLeft);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_POSITION].AddRandItem(1, TopRight);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_POSITION].AddRandItem(1, BottomLeft);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_POSITION].AddRandItem(1, BottomRight);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_POSITION].CompileRandom();
            break;

        case FPK_EVOLWRS_LWRSOR_SIZE:
            // Size of cursor.
            (*pPickers)[FPK_EVOLWRS_LWRSOR_SIZE].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_LWRSOR_SIZE].AddRandItem(1, 32);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_SIZE].AddRandItem(1, 64);
            if (m_AtLeastGK10x)
            {
                (*pPickers)[FPK_EVOLWRS_LWRSOR_SIZE].AddRandItem(1, 128);
                (*pPickers)[FPK_EVOLWRS_LWRSOR_SIZE].AddRandItem(1, 256);
            }
            (*pPickers)[FPK_EVOLWRS_LWRSOR_SIZE].CompileRandom();
            break;

        case FPK_EVOLWRS_LWRSOR_HOT_SPOT_X:
            // Hot spot. Full range, will be "modulo size" based on picked size
            (*pPickers)[FPK_EVOLWRS_LWRSOR_HOT_SPOT_X].ConfigRandom();
            {
                UINT32 MaxX = 63;
                if (m_AtLeastGK10x)
                {
                    MaxX = 255;
                }
                (*pPickers)[FPK_EVOLWRS_LWRSOR_HOT_SPOT_X].AddRandRange(1, 0, MaxX);
            }
            (*pPickers)[FPK_EVOLWRS_LWRSOR_HOT_SPOT_X].CompileRandom();
            break;

        case FPK_EVOLWRS_LWRSOR_HOT_SPOT_Y:
            (*pPickers)[FPK_EVOLWRS_LWRSOR_HOT_SPOT_Y].ConfigRandom();
            {
                UINT32 MaxY = 63;
                if (m_AtLeastGK10x)
                {
                    MaxY = 255;
                }
                (*pPickers)[FPK_EVOLWRS_LWRSOR_HOT_SPOT_Y].AddRandRange(1, 0, MaxY);
            }
            (*pPickers)[FPK_EVOLWRS_LWRSOR_HOT_SPOT_Y].CompileRandom();
            break;

        case FPK_EVOLWRS_LWRSOR_FORMAT:
            // Cursor color format.
            (*pPickers)[FPK_EVOLWRS_LWRSOR_FORMAT].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_LWRSOR_FORMAT].AddRandItem(1, ColorUtils::A1R5G5B5);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_FORMAT].AddRandItem(1, ColorUtils::A8R8G8B8);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_FORMAT].CompileRandom();
            break;

        case FPK_EVOLWRS_LWRSOR_COMPOSITION:
            // Cursor composition.
            (*pPickers)[FPK_EVOLWRS_LWRSOR_COMPOSITION].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_LWRSOR_COMPOSITION].AddRandItem(1, LW507D_HEAD_SET_CONTROL_LWRSOR_COMPOSITION_ALPHA_BLEND);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_COMPOSITION].AddRandItem(1, LW507D_HEAD_SET_CONTROL_LWRSOR_COMPOSITION_PREMULT_ALPHA_BLEND);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_COMPOSITION].AddRandItem(1, LW507D_HEAD_SET_CONTROL_LWRSOR_COMPOSITION_XOR);
            (*pPickers)[FPK_EVOLWRS_LWRSOR_COMPOSITION].CompileRandom();
            break;

        case FPK_EVOLWRS_USE_GAIN_OFFSET:
            (*pPickers)[FPK_EVOLWRS_USE_GAIN_OFFSET].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_USE_GAIN_OFFSET].AddRandItem(1, false);
            if (m_AtLeastGF11x)
                (*pPickers)[FPK_EVOLWRS_USE_GAIN_OFFSET].AddRandItem(1, true);
            (*pPickers)[FPK_EVOLWRS_USE_GAIN_OFFSET].CompileRandom();
            break;

        case FPK_EVOLWRS_USE_GAMUT_COLOR_SPACE_COLWERSION:
            (*pPickers)[FPK_EVOLWRS_USE_GAMUT_COLOR_SPACE_COLWERSION].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_USE_GAMUT_COLOR_SPACE_COLWERSION].AddRandItem(1, false);
            if (m_AtLeastGF11x)
                (*pPickers)[FPK_EVOLWRS_USE_GAMUT_COLOR_SPACE_COLWERSION].AddRandItem(1, true);
            (*pPickers)[FPK_EVOLWRS_USE_GAMUT_COLOR_SPACE_COLWERSION].CompileRandom();
            break;

        case FPK_EVOLWRS_CSC_CHANNEL:
            (*pPickers)[FPK_EVOLWRS_CSC_CHANNEL].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_CSC_CHANNEL].AddRandItem(1, Display::CORE_CHANNEL_ID);
            (*pPickers)[FPK_EVOLWRS_CSC_CHANNEL].AddRandItem(1, Display::BASE_CHANNEL_ID);
            (*pPickers)[FPK_EVOLWRS_CSC_CHANNEL].CompileRandom();
            break;

        case FPK_EVOLWRS_LUT_MODE:
            (*pPickers)[FPK_EVOLWRS_LUT_MODE].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_LUT_MODE].AddRandItem(1, Disable);
            if (m_AtLeastGF11x)
            {
                (*pPickers)[FPK_EVOLWRS_LUT_MODE].AddRandItem(1, Display::LUTMODE_LORES);
                (*pPickers)[FPK_EVOLWRS_LUT_MODE].AddRandItem(1, Display::LUTMODE_HIRES);
                (*pPickers)[FPK_EVOLWRS_LUT_MODE].AddRandItem(1, Display::LUTMODE_INDEX_1025_UNITY_RANGE);
                (*pPickers)[FPK_EVOLWRS_LUT_MODE].AddRandItem(1, Display::LUTMODE_INTERPOLATE_1025_UNITY_RANGE);
                (*pPickers)[FPK_EVOLWRS_LUT_MODE].AddRandItem(1, Display::LUTMODE_INTERPOLATE_1025_XRBIAS_RANGE);
                (*pPickers)[FPK_EVOLWRS_LUT_MODE].AddRandItem(1, Display::LUTMODE_INTERPOLATE_1025_XVYCC_RANGE);
                (*pPickers)[FPK_EVOLWRS_LUT_MODE].AddRandItem(1, Display::LUTMODE_INTERPOLATE_257_UNITY_RANGE);
                (*pPickers)[FPK_EVOLWRS_LUT_MODE].AddRandItem(1, Display::LUTMODE_INTERPOLATE_257_LEGACY_RANGE);
            }
            (*pPickers)[FPK_EVOLWRS_LUT_MODE].CompileRandom();
            break;

        case FPK_EVOLWRS_DITHER_BITS:
            (*pPickers)[FPK_EVOLWRS_DITHER_BITS].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_DITHER_BITS].AddRandItem(1, Display::DITHER_TO_6_BITS);
            (*pPickers)[FPK_EVOLWRS_DITHER_BITS].AddRandItem(1, Display::DITHER_TO_8_BITS);
            (*pPickers)[FPK_EVOLWRS_DITHER_BITS].AddRandItem(1, Display::DITHER_TO_10_BITS);
            (*pPickers)[FPK_EVOLWRS_DITHER_BITS].CompileRandom();
            break;

        case FPK_EVOLWRS_DITHER_MODE:
            (*pPickers)[FPK_EVOLWRS_DITHER_MODE].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_DITHER_MODE].AddRandItem(1, Disable);
            if (m_AtLeastGF11x)
            {
                (*pPickers)[FPK_EVOLWRS_DITHER_MODE].AddRandItem(1, Display::DITHERMODE_DYNAMIC_ERR_ACC);
                (*pPickers)[FPK_EVOLWRS_DITHER_MODE].AddRandItem(1, Display::DITHERMODE_STATIC_ERR_ACC);
                (*pPickers)[FPK_EVOLWRS_DITHER_MODE].AddRandItem(1, Display::DITHERMODE_DYNAMIC_2X2);
                (*pPickers)[FPK_EVOLWRS_DITHER_MODE].AddRandItem(1, Display::DITHERMODE_STATIC_2X2);
                (*pPickers)[FPK_EVOLWRS_DITHER_MODE].AddRandItem(1, Display::DITHERMODE_TEMPORAL);
            }
            (*pPickers)[FPK_EVOLWRS_DITHER_MODE].CompileRandom();
            break;

        case FPK_EVOLWRS_DITHER_PHASE:
            (*pPickers)[FPK_EVOLWRS_DITHER_PHASE].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_DITHER_PHASE].AddRandRange(1, 0, 3);
            (*pPickers)[FPK_EVOLWRS_DITHER_PHASE].CompileRandom();
            break;

        case FPK_EVOLWRS_BACKGROUND_FORMAT:
            (*pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].ConfigList(FancyPicker::WRAP);
            (*pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].AddListItem(ColorUtils::A8R8G8B8);
            // FP16 format is tested here to be picked when testing 2 loops
            if (m_AtLeastGF11x)
                (*pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].AddListItem(ColorUtils::RF16_GF16_BF16_AF16);
            if (m_AtLeastG8x)
            {
                (*pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].AddListItem(ColorUtils::A8B8G8R8);
                (*pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].AddListItem(ColorUtils::A1R5G5B5);
                (*pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].AddListItem(ColorUtils::R5G6B5);
            }
            if (m_AtLeastGF11x)
            {
                (*pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].AddListItem(ColorUtils::A2B10G10R10);
                (*pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].AddListItem(ColorUtils::X2BL10GL10RL10_XRBIAS);
                (*pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].AddListItem(ColorUtils::R16_G16_B16_A16);
                (*pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].AddListItem(ColorUtils::R16_G16_B16_A16_LWBIAS);
            }
            if (m_AtLeastGK10x)
            {
                (*pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].AddListItem(ColorUtils::A2R10G10B10);
            }
            if (m_AtLeastGK11x)
            {
                (*pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].AddListItem(ColorUtils::X2BL10GL10RL10_XVYCC);
            }
            // When adding a new format don't forget to update Loops in
            //  SetupEvoLwrs(gpuargs.js) to guarantee that all formats are tested.
            break;

        case FPK_EVOLWRS_BACKGROUND_SCALER_H:
            (*pPickers)[FPK_EVOLWRS_BACKGROUND_SCALER_H].FConfigRandom();
            (*pPickers)[FPK_EVOLWRS_BACKGROUND_SCALER_H].FAddRandItem(1, 1.0f);
            if (m_AtLeastGF11x)
            {
                (*pPickers)[FPK_EVOLWRS_BACKGROUND_SCALER_H].FAddRandRange(1, 0.6f, 1.9f);
            }
            (*pPickers)[FPK_EVOLWRS_BACKGROUND_SCALER_H].CompileRandom();
            break;

        case FPK_EVOLWRS_BACKGROUND_SCALER_V:
            (*pPickers)[FPK_EVOLWRS_BACKGROUND_SCALER_V].FConfigRandom();
            (*pPickers)[FPK_EVOLWRS_BACKGROUND_SCALER_V].FAddRandItem(1, 1.0f);
            if (m_AtLeastGF11x)
            {
                (*pPickers)[FPK_EVOLWRS_BACKGROUND_SCALER_V].FAddRandRange(1, 0.6f, 1.9f);
            }
            (*pPickers)[FPK_EVOLWRS_BACKGROUND_SCALER_V].CompileRandom();
            break;

        case FPK_EVOLWRS_USE_STEREO_LWRSOR:
            (*pPickers)[FPK_EVOLWRS_USE_STEREO_LWRSOR].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_USE_STEREO_LWRSOR].AddRandItem(1, false);
            if (m_AtLeastGK10x)
                (*pPickers)[FPK_EVOLWRS_USE_STEREO_LWRSOR].AddRandItem(1, true);
            (*pPickers)[FPK_EVOLWRS_USE_STEREO_LWRSOR].CompileRandom();
            break;

        case FPK_EVOLWRS_USE_STEREO_BACKGROUND:
            (*pPickers)[FPK_EVOLWRS_USE_STEREO_BACKGROUND].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_USE_STEREO_BACKGROUND].AddRandItem(1, false);
            if (m_AtLeastG8x)
                (*pPickers)[FPK_EVOLWRS_USE_STEREO_BACKGROUND].AddRandItem(1, true);
            (*pPickers)[FPK_EVOLWRS_USE_STEREO_BACKGROUND].CompileRandom();
            break;

        case FPK_EVOLWRS_USE_DP_FRAME_PACKING:
            (*pPickers)[FPK_EVOLWRS_USE_DP_FRAME_PACKING].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_USE_DP_FRAME_PACKING].AddRandItem(1, false);
            if (m_AtLeastGK11x)
                (*pPickers)[FPK_EVOLWRS_USE_DP_FRAME_PACKING].AddRandItem(1, true);
            (*pPickers)[FPK_EVOLWRS_USE_DP_FRAME_PACKING].CompileRandom();
            break;

        case FPK_EVOLWRS_DYNAMIC_RANGE:
            (*pPickers)[FPK_EVOLWRS_DYNAMIC_RANGE].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_DYNAMIC_RANGE].AddRandItem(1, EvoProcampSettings::VESA);
            (*pPickers)[FPK_EVOLWRS_DYNAMIC_RANGE].AddRandItem(1, EvoProcampSettings::CEA);
            (*pPickers)[FPK_EVOLWRS_DYNAMIC_RANGE].CompileRandom();
            break;

        case FPK_EVOLWRS_RANGE_COMPRESSION:
            (*pPickers)[FPK_EVOLWRS_RANGE_COMPRESSION].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_RANGE_COMPRESSION].AddRandItem(1, false);
            (*pPickers)[FPK_EVOLWRS_RANGE_COMPRESSION].AddRandItem(1, true);
            (*pPickers)[FPK_EVOLWRS_RANGE_COMPRESSION].CompileRandom();
            break;

        case FPK_EVOLWRS_COLOR_SPACE:
            {
                auto &picker = (*pPickers)[FPK_EVOLWRS_COLOR_SPACE];
                picker.ConfigRandom();
                picker.AddRandItem(1, static_cast<UINT32>(Display::ORColorSpace::RGB));
                picker.AddRandItem(1, static_cast<UINT32>(Display::ORColorSpace::YUV_601));
                picker.AddRandItem(1, static_cast<UINT32>(Display::ORColorSpace::YUV_709));
                if (m_AtLeastGP10x)
                {
                    picker.AddRandItem(1,
                            static_cast<UINT32>(Display::ORColorSpace::YUV_2020));
                }
                picker.CompileRandom();
            }
            break;

        case FPK_EVOLWRS_CHROMA_LPF:
            (*pPickers)[FPK_EVOLWRS_CHROMA_LPF].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_CHROMA_LPF].AddRandItem(1, EvoProcampSettings::AUTO);
            (*pPickers)[FPK_EVOLWRS_CHROMA_LPF].AddRandItem(1, EvoProcampSettings::ON);
            (*pPickers)[FPK_EVOLWRS_CHROMA_LPF].CompileRandom();
            break;

        case FPK_EVOLWRS_SAT_COS:
            (*pPickers)[FPK_EVOLWRS_SAT_COS].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_SAT_COS].AddRandRange(1, 0, (1<<12)-1);
            (*pPickers)[FPK_EVOLWRS_SAT_COS].CompileRandom();
            break;

        case FPK_EVOLWRS_SAT_SINE:
            (*pPickers)[FPK_EVOLWRS_SAT_SINE].ConfigRandom();
            (*pPickers)[FPK_EVOLWRS_SAT_SINE].AddRandRange(1, 0, (1<<12)-1);
            (*pPickers)[FPK_EVOLWRS_SAT_SINE].CompileRandom();
            break;

        case FPK_EVOLWRS_OR_PIXEL_BPP:
            (*pPickers)[FPK_EVOLWRS_OR_PIXEL_BPP].ConfigList(FancyPicker::WRAP);

            // 24 bpp is supported on ALL chips
            (*pPickers)[FPK_EVOLWRS_OR_PIXEL_BPP].AddListItem(LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444);
            if (m_AtLeastG8x)
            {
                // 36 bpp is supported on DP and HDMI
                (*pPickers)[FPK_EVOLWRS_OR_PIXEL_BPP].AddListItem(LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_36_444);

                // Following modes are supported ONLY on DP:
                // Note: 16 bpp causes DPLib to hang in MST mode on Kepler and GM107, skip it
                (*pPickers)[FPK_EVOLWRS_OR_PIXEL_BPP].AddListItem(LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_16_422);
                (*pPickers)[FPK_EVOLWRS_OR_PIXEL_BPP].AddListItem(LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_18_444);
                (*pPickers)[FPK_EVOLWRS_OR_PIXEL_BPP].AddListItem(LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_30_444);

                // Pixel modes not supported by any GPU, as per display HW team
                //(*pPickers)[FPK_EVOLWRS_OR_PIXEL_BPP].AddListItem(LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_20_422);
                //(*pPickers)[FPK_EVOLWRS_OR_PIXEL_BPP].AddListItem(LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_422);
                //(*pPickers)[FPK_EVOLWRS_OR_PIXEL_BPP].AddListItem(LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_32_422);
                //(*pPickers)[FPK_EVOLWRS_OR_PIXEL_BPP].AddListItem(LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_48_444);
            }
            break;

        default:
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//----------------------------------------------------------------------------
// Initialize the properties.

RC EvoLwrs::InitProperties()
{
    RC rc;

    m_InitMode.RasterWidth    = m_TestConfig.DisplayWidth();
    m_InitMode.RasterHeight   = m_TestConfig.DisplayHeight();
    m_InitMode.Depth          = m_TestConfig.DisplayDepth();
    m_InitMode.RefreshRate    = m_TestConfig.RefreshRate();
    m_InitMode.ImageWidth     = m_TestConfig.DisplayWidth();
    m_InitMode.ImageHeight    = m_TestConfig.DisplayHeight();

    // Use fixed mode for a consistent display independent stress test
    m_StressMode.RasterWidth  = 1600;
    m_StressMode.RasterHeight = 1200;
    m_StressMode.ImageWidth   = 1600;
    m_StressMode.ImageHeight  = 1200;
    m_StressMode.Depth        = 32;
    m_StressMode.RefreshRate  = 85;  // This should generate pclk around 206 MHz
    m_TimeoutMs               = m_pTestConfig->TimeoutMs();

    if (m_pTestConfig->DisableCrt())
          Printf(Tee::PriLow, "NOTE: ignoring TestConfiguration.DisableCrt.\n");

    // Reset the default values for pickers that require dynamic input data
    if (!(*m_pPickers)[FPK_EVOLWRS_LWRSOR_SIZE].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_LWRSOR_SIZE));

    if (!(*m_pPickers)[FPK_EVOLWRS_LWRSOR_HOT_SPOT_X].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_LWRSOR_HOT_SPOT_X));

    if (!(*m_pPickers)[FPK_EVOLWRS_LWRSOR_HOT_SPOT_Y].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_LWRSOR_HOT_SPOT_Y));

    if (!(*m_pPickers)[FPK_EVOLWRS_USE_GAIN_OFFSET].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_USE_GAIN_OFFSET));

    if (!(*m_pPickers)[FPK_EVOLWRS_USE_GAMUT_COLOR_SPACE_COLWERSION].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_USE_GAMUT_COLOR_SPACE_COLWERSION));

    if (!(*m_pPickers)[FPK_EVOLWRS_LUT_MODE].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_LUT_MODE));

    if (!(*m_pPickers)[FPK_EVOLWRS_DITHER_MODE].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_DITHER_MODE));

    if (!(*m_pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_BACKGROUND_FORMAT));

    if (!(*m_pPickers)[FPK_EVOLWRS_BACKGROUND_SCALER_H].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_BACKGROUND_SCALER_H));

    if (!(*m_pPickers)[FPK_EVOLWRS_BACKGROUND_SCALER_V].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_BACKGROUND_SCALER_V));

    if (!(*m_pPickers)[FPK_EVOLWRS_USE_STEREO_LWRSOR].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_USE_STEREO_LWRSOR));

    if (!(*m_pPickers)[FPK_EVOLWRS_USE_STEREO_BACKGROUND].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_USE_STEREO_BACKGROUND));

    if (!(*m_pPickers)[FPK_EVOLWRS_USE_DP_FRAME_PACKING].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_USE_DP_FRAME_PACKING));

    if (!(*m_pPickers)[FPK_EVOLWRS_DYNAMIC_RANGE].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_DYNAMIC_RANGE));

    if (!(*m_pPickers)[FPK_EVOLWRS_RANGE_COMPRESSION].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_RANGE_COMPRESSION));

    if (!(*m_pPickers)[FPK_EVOLWRS_COLOR_SPACE].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_COLOR_SPACE));

    if (!(*m_pPickers)[FPK_EVOLWRS_CHROMA_LPF].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_CHROMA_LPF));

    if (!(*m_pPickers)[FPK_EVOLWRS_SAT_COS].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_SAT_COS));

    if (!(*m_pPickers)[FPK_EVOLWRS_SAT_SINE].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_SAT_SINE));

    if (!(*m_pPickers)[FPK_EVOLWRS_OR_PIXEL_BPP].WasSetFromJs())
        CHECK_RC(SetDefaultsForPicker(FPK_EVOLWRS_OR_PIXEL_BPP));

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

RC EvoLwrs::RunDisplay()
{
    RC rc;

    string DisplaysStr;
    for (size_t Idx = 0; Idx < m_LwrsorDisplays.size(); Idx++)
        DisplaysStr += Utility::StrPrintf(" %08x", m_LwrsorDisplays[Idx]);

    Printf(m_LogPriority, "Starting EvoLwrs test on displays %s.\n", DisplaysStr.c_str());
    rc = LoopRandomMethods();
    Printf(m_LogPriority, "Completed EvoLwrs test on displays %s.\n", DisplaysStr.c_str());

    return rc;
}

//----------------------------------------------------------------------------

RC EvoLwrs::PickLwrsor()
{
    // Pick the random cursor state.

    m_Picks.Size = (*m_pPickers)[FPK_EVOLWRS_LWRSOR_SIZE].Pick();
    m_Picks.HotSpotX = (*m_pPickers)[FPK_EVOLWRS_LWRSOR_HOT_SPOT_X].Pick();
    m_Picks.HotSpotX %= m_Picks.Size;
    m_Picks.HotSpotY = (*m_pPickers)[FPK_EVOLWRS_LWRSOR_HOT_SPOT_Y].Pick();
    m_Picks.HotSpotY %= m_Picks.Size;
    m_Picks.Format = (*m_pPickers)[FPK_EVOLWRS_LWRSOR_FORMAT].Pick();
    if (m_Picks.Format == ColorUtils::A1R5G5B5)
        // Only XOR allowed for this format
        m_Picks.Composition = LW507D_HEAD_SET_CONTROL_LWRSOR_COMPOSITION_XOR;
    else
    {
        if (m_IsLVDSInReducedMode)
            m_Picks.Composition = LW507D_HEAD_SET_CONTROL_LWRSOR_COMPOSITION_PREMULT_ALPHA_BLEND;
        else
            m_Picks.Composition = (*m_pPickers)[FPK_EVOLWRS_LWRSOR_COMPOSITION].Pick();
    }

    UINT32 XMax = 0;
    if (m_StressMode.RasterWidth > m_Picks.HotSpotX)
        XMax = m_StressMode.RasterWidth - m_Picks.HotSpotX - 1;
    UINT32 YMax = 0;
    if (m_StressMode.RasterHeight > m_Picks.HotSpotY)
        YMax = m_StressMode.RasterHeight - m_Picks.HotSpotY - 1;
    UINT32 Offset = m_Picks.Size - 1;
    switch ((*m_pPickers)[FPK_EVOLWRS_LWRSOR_POSITION].Pick())
    {
        case ScreenRandom:
            m_Picks.X = m_pFpCtx->Rand.GetRandom(0, XMax);
            m_Picks.Y = m_pFpCtx->Rand.GetRandom(0, YMax);
            break;

        case Top:
            m_Picks.X = m_pFpCtx->Rand.GetRandom(0, XMax);
            m_Picks.Y = -static_cast<INT32>(m_pFpCtx->Rand.GetRandom(0, Offset))
                + m_Picks.HotSpotY;
            break;

        case Bottom:
            m_Picks.X = m_pFpCtx->Rand.GetRandom(0, XMax);
            m_Picks.Y = m_pFpCtx->Rand.GetRandom((Offset < YMax) ? (YMax - Offset) : 0, YMax);
            break;

        case Left:
            m_Picks.X = -static_cast<INT32>(m_pFpCtx->Rand.GetRandom(0, Offset))
                + m_Picks.HotSpotX;
            m_Picks.Y = m_pFpCtx->Rand.GetRandom(0, YMax);
            break;

        case Right:
            m_Picks.X = m_pFpCtx->Rand.GetRandom((Offset < XMax) ? (XMax - Offset) : 0, XMax);
            m_Picks.Y = m_pFpCtx->Rand.GetRandom(0, YMax);
            break;

        case TopLeft:
            m_Picks.X = -static_cast<INT32>(m_pFpCtx->Rand.GetRandom(0, Offset))
                + m_Picks.HotSpotX;
            m_Picks.Y = -static_cast<INT32>(m_pFpCtx->Rand.GetRandom(0, Offset))
                + m_Picks.HotSpotY;
            break;

        case TopRight:
            m_Picks.X = m_pFpCtx->Rand.GetRandom((Offset < XMax) ? (XMax - Offset) : 0, XMax);
            m_Picks.Y = -static_cast<INT32>(m_pFpCtx->Rand.GetRandom(0, Offset))
                + m_Picks.HotSpotY;
            break;

        case BottomLeft:
            m_Picks.X = -static_cast<INT32>(m_pFpCtx->Rand.GetRandom(0, Offset))
                + m_Picks.HotSpotX;
            m_Picks.Y = m_pFpCtx->Rand.GetRandom((Offset < YMax) ? (YMax - Offset) : 0, YMax);
            break;

        case BottomRight:
            m_Picks.X = m_pFpCtx->Rand.GetRandom((Offset < XMax) ? (XMax - Offset) : 0, XMax);
            m_Picks.Y = m_pFpCtx->Rand.GetRandom((Offset < YMax) ? (YMax - Offset) : 0, YMax);
            break;

        default:
            MASSERT(!"Cursor position mode not implemented!");
            m_Picks.X = m_StressMode.RasterWidth / 2;
            m_Picks.Y = m_StressMode.RasterHeight / 2;
            return RC::SOFTWARE_ERROR;
    }

    m_Picks.EnableGainOffset = (*m_pPickers)[FPK_EVOLWRS_USE_GAIN_OFFSET].Pick() != 0;
    if (m_Picks.EnableGainOffset)
    {
        m_Picks.Gains  [Display::R] = m_pFpCtx->Rand.GetRandomFloat(0.5, 4.0);
        m_Picks.Offsets[Display::R] = m_pFpCtx->Rand.GetRandomFloat(0.0, 4.0);
        m_Picks.Gains  [Display::G] = m_pFpCtx->Rand.GetRandomFloat(0.5, 4.0);
        m_Picks.Offsets[Display::G] = m_pFpCtx->Rand.GetRandomFloat(0.0, 4.0);
        m_Picks.Gains  [Display::B] = m_pFpCtx->Rand.GetRandomFloat(0.5, 4.0);
        m_Picks.Offsets[Display::B] = m_pFpCtx->Rand.GetRandomFloat(0.0, 4.0);
    }

    m_Picks.EnableGamutColorSpaceColwersion = (*m_pPickers)[FPK_EVOLWRS_USE_GAMUT_COLOR_SPACE_COLWERSION].Pick() != 0;
    if (m_IsLVDSInReducedMode)
        m_Picks.EnableGamutColorSpaceColwersion = false;
    m_Picks.ColorSpaceColwersionChannel = (Display::ChannelID)(*m_pPickers)[FPK_EVOLWRS_CSC_CHANNEL].Pick();
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

    UINT32 LutModePicker = (*m_pPickers)[FPK_EVOLWRS_LUT_MODE].Pick();
    m_Picks.EnableLUT = (LutModePicker != Disable);
    if (m_Picks.EnableLUT)
    {
        m_Picks.LUTMode = (Display::LUTMode)LutModePicker;
    }

    m_Picks.DitherBits = (*m_pPickers)[FPK_EVOLWRS_DITHER_BITS].Pick();
    m_Picks.DitherPhase = (*m_pPickers)[FPK_EVOLWRS_DITHER_PHASE].Pick();
    UINT32 DitherMode = (*m_pPickers)[FPK_EVOLWRS_DITHER_MODE].Pick();
    m_Picks.EnableDither = (DitherMode != Disable);
    if (m_IsLVDSInReducedMode)
        m_Picks.EnableDither = false;
    if (m_Picks.EnableDither)
    {
        m_Picks.DitherMode = (Display::DitherMode)DitherMode;
        if ((m_Picks.DitherMode == Display::DITHERMODE_TEMPORAL) &&
            (m_Picks.DitherPhase == 0))
        {
            // Phase 0 for temporal mode means leaving random number generator
            // free running, what likely will generate will make CRC non deterministic
            // on silicon. Any other phase value will reset the generator to
            // some known value.
            m_Picks.DitherPhase = 1;
        }
    }

    m_Picks.ScalerH = (*m_pPickers)[FPK_EVOLWRS_BACKGROUND_SCALER_H].FPick();
    m_Picks.ScalerV = (*m_pPickers)[FPK_EVOLWRS_BACKGROUND_SCALER_V].FPick();

    m_Picks.BackgroundSurfaceFormat = (*m_pPickers)[FPK_EVOLWRS_BACKGROUND_FORMAT].Pick();

    if (!m_MultiHeadStressMode)
    {
        // Do only simple things for high resolutions as we mostly should be
        // testing bandwidth/IMP there. Choosing a random 64bpp format or
        // high scaler values could could make us loose testing high PCLK
        // due to IMP always returning false.
        if (m_Use16BitDepthAtOR)
        {
            m_Picks.BackgroundSurfaceFormat = ColorUtils::A1R5G5B5;
        }
        else
        {
            m_Picks.BackgroundSurfaceFormat = ColorUtils::A8R8G8B8;
        }
        m_Picks.ScalerH = 1.0f;
        m_Picks.ScalerV = 1.0f;
        m_Picks.EnableGainOffset = false;
        m_Picks.EnableGamutColorSpaceColwersion = false;
    }

    if (m_StressMode.RasterWidth >= GM10xMaxPRUResolutionWidth)
    {
        // HW scaler has a limit for max res that depends on taps used and color
        // encoding. Hard code for now for simplicity. Otherwise processing of
        // MAX_PIXELSxTAPabc disp notifier would be needed.
        m_Picks.ScalerH = 1.0f;
        m_Picks.ScalerV = 1.0f;

        // For more than 4 bytes per pixel IMP sees: "Not enough mempool for
        // pixel fetch latency buffer"
        if (ColorUtils::PixelBytes(
            (ColorUtils::Format)m_Picks.BackgroundSurfaceFormat) > 4)
            m_Picks.BackgroundSurfaceFormat = ColorUtils::A8R8G8B8;

        // Primary CRC differs intermittently between multiple heads for
        // an unknown reason:
        m_Picks.EnableDither=false;
    }

    if (m_LwrsorDisplayIsLVDS && GetBoundGpuSubdevice()->HasBug(1517497))
    {
        // Enabling scaler on GM20x seems to induce CRC capture failure timeouts
        // Disable scaling on LVDS for GM20x until root-caused with display HW team
        m_Picks.ScalerH = 1.0f;
        m_Picks.ScalerV = 1.0f;
    }

    switch (m_Picks.BackgroundSurfaceFormat)
    {
        case ColorUtils::A1R5G5B5:
        case ColorUtils::R5G6B5:
        case ColorUtils::A8R8G8B8:
        case ColorUtils::A8B8G8R8:
        case ColorUtils::A2B10G10R10:
        case ColorUtils::A2R10G10B10:
        case ColorUtils::X2BL10GL10RL10_XVYCC:
        case ColorUtils::X2BL10GL10RL10_XRBIAS:
        case ColorUtils::R16_G16_B16_A16:
        case ColorUtils::R16_G16_B16_A16_LWBIAS:
            m_Picks.EnableGainOffset = false;
            break;
        case ColorUtils::RF16_GF16_BF16_AF16:
            break;
        default:
            MASSERT(!"SurfaceFormat not implemented!");
            return RC::SOFTWARE_ERROR;
    }

    if (!m_EnableScaler)
    {
        m_Picks.ScalerH = 1.0f;
        m_Picks.ScalerV = 1.0f;
    }

    m_Picks.EnableStereoLwrsor =
        (*m_pPickers)[FPK_EVOLWRS_USE_STEREO_LWRSOR].Pick() != 0;

    m_Picks.EnableStereoBackground =
        (*m_pPickers)[FPK_EVOLWRS_USE_STEREO_BACKGROUND].Pick() != 0;

    if (m_IsLVDSInReducedMode)
    {
        m_Picks.EnableStereoLwrsor = false;
        m_Picks.EnableStereoBackground = false;
    }

    m_Picks.EnableDpFramePacking =
        (*m_pPickers)[FPK_EVOLWRS_USE_DP_FRAME_PACKING].Pick() != 0;
    if (!m_LwrsorDisplayIsDP || m_Picks.EnableStereoBackground)
        m_Picks.EnableDpFramePacking = false;

    m_Picks.Eps.DynamicRange = static_cast<EvoProcampSettings::DYNAMIC_RANGE>
        ((*m_pPickers)[FPK_EVOLWRS_DYNAMIC_RANGE].Pick());

    m_Picks.Eps.RangeCompression =
        (*m_pPickers)[FPK_EVOLWRS_RANGE_COMPRESSION].Pick() != 0;

    m_Picks.Eps.ColorSpace = static_cast<Display::ORColorSpace>
        ((*m_pPickers)[FPK_EVOLWRS_COLOR_SPACE].Pick());

    if ((m_PrimaryType == Display::CRT) ||
        ((m_PrimaryType == Display::DFP) &&
             (m_PrimaryEncoder.Signal == Display::Encoder::LVDS)))
    {
        m_Picks.Eps.ColorSpace = Display::ORColorSpace::RGB;
    }

    if (m_Use16BitDepthAtOR)
    {
        m_Picks.Eps.ColorSpace = Display::ORColorSpace::YUV_709;
    }
    if (m_LwrsorDisplayIsDP &&
        (m_Picks.Eps.ColorSpace != Display::ORColorSpace::RGB))
    {
        m_Picks.Eps.DynamicRange = EvoProcampSettings::CEA;
    }

    if (!m_AtLeastGT21x)
    {
        // GT200 generates an exception for CEA:
        m_Picks.Eps.DynamicRange = EvoProcampSettings::VESA;
    }

    m_Picks.Eps.ChromaLpf = static_cast<EvoProcampSettings::CHROMA_LPF>
        ((*m_pPickers)[FPK_EVOLWRS_CHROMA_LPF].Pick());

    m_Picks.Eps.SatCos = (*m_pPickers)[FPK_EVOLWRS_SAT_COS].Pick();

    m_Picks.Eps.SatSine = (*m_pPickers)[FPK_EVOLWRS_SAT_SINE].Pick();

    m_Picks.ORPixelBpp = static_cast<LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP>(
                                (*m_pPickers)[FPK_EVOLWRS_OR_PIXEL_BPP].Pick());

    if (m_Use16BitDepthAtOR)
    {
        m_Picks.ORPixelBpp = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_16_422;
    }
    // Apply some defaults based on display protocol, pixel mode and color space
    if (m_LwrsorDisplayIsDP)
    {
        if ((m_Picks.Eps.ColorSpace == Display::ORColorSpace::RGB) &&
            (((m_Picks.ORPixelBpp != LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_18_444) &&
              (m_Picks.ORPixelBpp != LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444) &&
              (m_Picks.ORPixelBpp != LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_30_444) &&
              (m_Picks.ORPixelBpp != LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_36_444)) ||
             ((m_Picks.ORPixelBpp == LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_18_444) &&
              (m_Picks.Eps.DynamicRange == EvoProcampSettings::CEA))))
        {
            // RGB Display Port supports 18 (but not w/ CEA), 24, 30 or 36 bits per pixel
            m_Picks.ORPixelBpp = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_DEFAULT;
        }
        else if (((m_Picks.Eps.ColorSpace == Display::ORColorSpace::YUV_601) ||
                  (m_Picks.Eps.ColorSpace == Display::ORColorSpace::YUV_709) ||
                  (m_Picks.Eps.ColorSpace == Display::ORColorSpace::YUV_2020)) &&
                 (m_Picks.ORPixelBpp != LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_16_422) &&
                 (m_Picks.ORPixelBpp != LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444) &&
                 (m_Picks.ORPixelBpp != LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_30_444) &&
                 (m_Picks.ORPixelBpp != LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_36_444))
        {
            // YUV 601/709/2020 Display Port supports BPP_16_422, BPP_24_444, BPP_30_444 and BPP_36_444
            m_Picks.ORPixelBpp = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_DEFAULT;
        }
    }
    else
    {
        if (m_LwrsorDisplayIsHDMI &&
            (m_Picks.ORPixelBpp != LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444) &&
            (m_Picks.ORPixelBpp != LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_36_444))
        {
            // HDMI supports only 24 bpp and 36 bpp
            m_Picks.ORPixelBpp = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_DEFAULT;
        }
        else if (m_LwrsorDisplayIsLVDS)
        {
            // For LVDS only test the default BPP
            // There is a DSI RTL bug that does NOT inform RM to shutdown the head
            // if VPLL is changing on an active head for LVDS
            // Avoid changing pixel depth for LVDS
            m_Picks.ORPixelBpp = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_DEFAULT;
        }
        else
        {
            // With DVI in pre-GM20x chips test only 24 bpp
            // Possibly replace with DEFAULT if we hit issues in the future
            m_Picks.ORPixelBpp = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444;
        }
    }

    return OK;
}

//----------------------------------------------------------------------------

RC EvoLwrs::PrepareLwrsorImage(UINT32 idx)
{
    RC rc;

    UINT32 NumStereoImages = m_Picks.EnableStereoLwrsor ? 2 : 1;

    for (UINT32 StereoIdx = 0; StereoIdx < NumStereoImages; StereoIdx++)
    {
        m_LwrsorImages[idx][StereoIdx].Free();

        m_LwrsorImages[idx][StereoIdx].SetWidth(m_Picks.Size);
        m_LwrsorImages[idx][StereoIdx].SetHeight(m_Picks.Size);
        m_LwrsorImages[idx][StereoIdx].SetType(LWOS32_TYPE_LWRSOR);
        m_LwrsorImages[idx][StereoIdx].SetColorFormat((ColorUtils::Format)m_Picks.Format);
        m_LwrsorImages[idx][StereoIdx].SetLocation(Memory::Optimal);
        m_LwrsorImages[idx][StereoIdx].SetProtect(Memory::ReadWrite);
        m_LwrsorImages[idx][StereoIdx].SetDisplayable(true);
        CHECK_RC(m_LwrsorImages[idx][StereoIdx].Alloc(GetBoundGpuDevice()));

        m_LwrsorImages[idx][StereoIdx].FillPattern(1, 1, "random",
            StereoIdx ? "seed=1" : "seed=0", NULL, 0);

        // Assuming cursor surface is displayed on core channel(s)
        Display *pDisplay = GetBoundGpuDevice()->GetDisplay();
        LwRm::Handle hCoreChan;
        if (pDisplay->GetEvoDisplay()->LwrsorNeedsBind() &&
            pDisplay->GetCoreChannelHandle(0, &hCoreChan) == OK)
        {
            CHECK_RC(m_LwrsorImages[idx][StereoIdx].BindIsoChannel(hCoreChan));
        }
    }

    return rc;
}

namespace
{
    // RAII class to clean up the DispSfUserAlloc object created in
    // SetDpFramePacking
    class CleanupDispSfUserAlloc
    {
    public:
        CleanupDispSfUserAlloc(DispSfUserAlloc *pAlloc, UINT08 **ppMem,
                               GpuSubdevice *pSbdev, LwRm *pLwRm) :
            m_pAlloc(pAlloc), m_ppMem(ppMem), m_pSbdev(pSbdev), m_pLwRm(pLwRm)
        {
        }
        ~CleanupDispSfUserAlloc()
        {
            if (*m_ppMem)
                m_pLwRm->UnmapMemory(m_pAlloc->GetHandle(),
                                     *m_ppMem, 0, m_pSbdev);
            m_pAlloc->Free();
        }
    private:
        DispSfUserAlloc *m_pAlloc;
        UINT08 **m_ppMem;
        GpuSubdevice *m_pSbdev;
        LwRm *m_pLwRm;
    };
}

//----------------------------------------------------------------------------
//! \brief Enable or disable DisplayPort frame-packed stereo
//!
//! Regrettably, the HW CRC doesn't include the secondary-data packets
//! that include the frame-packing data, so this feature won't affect
//! the golden values.  We're just exercising the HW.
//!
RC EvoLwrs::SetDpFramePacking(bool enable)
{
    LwRm *pLwRm = GetBoundRmClient();
    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    Display *pDisplay = GetDisplay();
    DispSfUserAlloc dispSfUser;
    RC rc;

    dispSfUser.SetOldestSupportedClass(LW9271_DISP_SF_USER);
    if (!m_LwrsorDisplayIsDP || !dispSfUser.IsSupported(pGpuDevice))
       return OK;

    UINT08 *pSfMem = nullptr;
    CleanupDispSfUserAlloc cleanup(&dispSfUser, &pSfMem, pGpuSubdevice, pLwRm);
    CHECK_RC(dispSfUser.Alloc(pGpuSubdevice, pLwRm));
    CHECK_RC(pLwRm->MapMemory(dispSfUser.GetHandle(), 0,
                              sizeof(Lw9271DispSfUserMap),
                              reinterpret_cast<void**>(&pSfMem),
                              0, pGpuSubdevice));

    UINT32 selected = pDisplay->Selected();
    for (INT32 bit = Utility::BitScanForward(selected);
         bit >= 0; bit = Utility::BitScanForward(selected, bit + 1))
    {
        UINT32 displayId = 1 << bit;
        UINT32 head = 0;
        CHECK_RC(pDisplay->GetHead(displayId, &head));

        // Disable audio so that we don't transmit any secondary-data
        // packets while we're changing them
        //
        LW0073_CTRL_DFP_SET_AUDIO_ENABLE_PARAMS audioParams;
        memset(&audioParams, 0, sizeof(audioParams));
        audioParams.subDeviceInstance = pGpuSubdevice->GetSubdeviceInst();
        audioParams.displayId = displayId;
        audioParams.enable = LW_FALSE;
        CHECK_RC(pDisplay->RmControl(LW0073_CTRL_CMD_DFP_SET_AUDIO_ENABLE,
                                     &audioParams, sizeof(audioParams)));

        if (enable)
        {
            // Write VSC packet (taken from VESA DisplayPort Standard,
            // Version 1, Revision 2, section 2.2.5.6)
            //
            MEM_WR32(
                pSfMem + LW9271_SF_DP_GENERIC_INFOFRAME_HEADER(head),
                DRF_NUM(9271, _SF_DP_GENERIC_INFOFRAME_HEADER, _HB0, 0x00) |
                DRF_NUM(9271, _SF_DP_GENERIC_INFOFRAME_HEADER, _HB1, 0x07) |
                DRF_NUM(9271, _SF_DP_GENERIC_INFOFRAME_HEADER, _HB2, 0x01) |
                DRF_NUM(9271, _SF_DP_GENERIC_INFOFRAME_HEADER, _HB3, 0x01));
            MEM_WR32(
                pSfMem + LW9271_SF_DP_GENERIC_INFOFRAME_SUBPACK0(head),
                DRF_NUM(9271, _SF_DP_GENERIC_INFOFRAME_SUBPACK0, _DB0, 0x23));
            MEM_WR32(pSfMem + LW9271_SF_DP_GENERIC_INFOFRAME_SUBPACK1(head), 0);
            MEM_WR32(pSfMem + LW9271_SF_DP_GENERIC_INFOFRAME_SUBPACK2(head), 0);
            MEM_WR32(pSfMem + LW9271_SF_DP_GENERIC_INFOFRAME_SUBPACK3(head), 0);
            MEM_WR32(pSfMem + LW9271_SF_DP_GENERIC_INFOFRAME_SUBPACK4(head), 0);
            MEM_WR32(pSfMem + LW9271_SF_DP_GENERIC_INFOFRAME_SUBPACK5(head), 0);
            MEM_WR32(pSfMem + LW9271_SF_DP_GENERIC_INFOFRAME_SUBPACK6(head), 0);

            MEM_WR32(
                pSfMem + LW9271_SF_DP_GENERIC_INFOFRAME_CTRL(head),
                DRF_DEF(9271, _SF_DP_GENERIC_INFOFRAME_CTRL, _ENABLE, _YES) |
                DRF_DEF(9271, _SF_DP_GENERIC_INFOFRAME_CTRL,
                        _MSA_STEREO_OVERRIDE, _YES));
        }
        else
        {
            // Disable VSC packet and stereo override
            //
            MEM_WR32(
                pSfMem + LW9271_SF_DP_GENERIC_INFOFRAME_CTRL(head),
                DRF_DEF(9271, _SF_DP_GENERIC_INFOFRAME_CTRL, _ENABLE, _NO) |
                DRF_DEF(9271, _SF_DP_GENERIC_INFOFRAME_CTRL,
                        _MSA_STEREO_OVERRIDE, _NO));
        }

        // Re-enable audio
        //
        memset(&audioParams, 0, sizeof(audioParams));
        audioParams.subDeviceInstance = pGpuSubdevice->GetSubdeviceInst();
        audioParams.displayId = displayId;
        audioParams.enable = LW_TRUE;
        CHECK_RC(pDisplay->RmControl(LW0073_CTRL_CMD_DFP_SET_AUDIO_ENABLE,
                                     &audioParams, sizeof(audioParams)));
    }

    return rc;
}

//----------------------------------------------------------------------------

RC EvoLwrs::LoopRandomMethods()
{
    RC rc;
    Display *pDisplay = GetDisplay();

    // When all heads have been tested and we are in reduced mode run just 2 loops
    // for FP16 and RGb8 format
    bool bOptimizeLoops = (m_EnableMultiSorConfigReducedMode &&
                           m_AllHeadsTestedAtMaxPclk &&
                           (m_pTestConfig->Loops() > 2));
    m_IsLVDSInReducedMode = (m_LwrsorDisplayIsLVDS && m_EnableLVDSReducedMode);

    UINT32 StartLoop        = m_pTestConfig->StartLoop();
    UINT32 RestartSkipCount = m_pTestConfig->RestartSkipCount();
    UINT32 Loops = m_pTestConfig->Loops();
    if (m_EnableReducedMode || m_IsLVDSInReducedMode)
        Loops = 1;
    else if (bOptimizeLoops)
        Loops = 2;
    UINT32 Seed             = m_pTestConfig->Seed() + m_SetModeIdx;
    UINT32 EndLoop          = StartLoop + Loops;
    UINT32 LwrsorSurfaceIdx = 0;

    Utility::CleanupOnReturn<EvoLwrs> cleanup(this,
        &EvoLwrs::CleanupDisplay);

    if ((StartLoop % RestartSkipCount) != 0)
    {
        Printf(Tee::PriNormal, "WARNING: StartLoop is not a restart point.\n");
    }
    m_pGolden->SetLoop(StartLoop + (m_SetModeIdx << 16));

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

        // Pick the random cursor state.
        CHECK_RC(PickLwrsor());

        UINT32 ScaledModeWidth =
                             UINT32(m_Picks.ScalerH * m_StressMode.RasterWidth);
        UINT32 ScaledModeHeight =
                            UINT32(m_Picks.ScalerV * m_StressMode.RasterHeight);

        // h/w requires ActiveX to be a multiple of ReorderBankWidthSize.
        //   ensure we're using an even width before we set the bank width to half.
        if (m_LwrsorDisplayIsDP &&
            ((m_LwrrentDPLaneCount == 8) || (m_TestSingleHeadMultipleStreams && m_Variant != VariantNormal)))
        {
            ScaledModeWidth &= ~1;
        }

        // Ensure an even width for dual-link
        if (m_PrimaryType == Display::DFP &&
            m_PrimaryEncoder.Link == Display::Encoder::DUAL)
        {
            ScaledModeWidth &= ~1;
        }

        string GainOffsetDesc = "Disabled";
        if (m_Picks.EnableGainOffset)
        {
            GainOffsetDesc = Utility::StrPrintf("RG=%.2f,RO=%.2f,GG=%.2f,GO=%.2f,BG=%.2f,BO=%.2f",
                m_Picks.Gains[Display::R], m_Picks.Offsets[Display::R],
                m_Picks.Gains[Display::G], m_Picks.Offsets[Display::G],
                m_Picks.Gains[Display::B], m_Picks.Offsets[Display::B]);
        }

        string GamutCSCDesc = "Disabled";
        if (m_Picks.EnableGamutColorSpaceColwersion)
        {
            GamutCSCDesc = Utility::StrPrintf("Ch=%d,"
                                              "RR=%.2f,GR=%.2f,BR=%.2f,CR=%.2f,"
                                              "RG=%.2f,GG=%.2f,BG=%.2f,CG=%.2f,"
                                              "RB=%.2f,GB=%.2f,BB=%.2f,CB=%.2f",
                m_Picks.ColorSpaceColwersionChannel,
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

        string LutModeDesc = "Disabled";
        if (m_Picks.EnableLUT)
        {
            LutModeDesc = EvoDisplay::LUTMode2Name(m_Picks.LUTMode);
        }

        string DitherDesc = "Disabled";
        if (m_Picks.EnableDither)
        {
            DitherDesc = Utility::StrPrintf("Bits=%d,Mode=%s,Phase=%d",
                m_Picks.DitherBits, Display::DitherMode2Name(m_Picks.DitherMode),
                m_Picks.DitherPhase);
        }

        string colorSpaceDesc = "?";
        switch (m_Picks.Eps.ColorSpace)
        {
            case Display::ORColorSpace::RGB:
                colorSpaceDesc = "RGB";
                break;
            case Display::ORColorSpace::YUV_601:
                colorSpaceDesc = "YUV_601";
                break;
            case Display::ORColorSpace::YUV_709:
                colorSpaceDesc = "YUV_709";
                break;
            case Display::ORColorSpace::YUV_2020:
                colorSpaceDesc = "YUV_2020";
                break;
            default:
                MASSERT(!"Unrecognized color space");
        }

        Printf(Tee::PriLow,
            "   Loop:%u Format:%s X:%i Y:%i Size:%u HotX:%d HotY:%d Composition:%u "
            "GainOffset:%s GamutCSC:%s LUT:%s Dither:%s BgFormat:%s, BgScale:H=%.3f,V=%.3f "
            "StereoLwrsor:%s StereoBackground:%s DpFramePacking: %s "
            "DynamicRange:%s RangeCompression:%s ColorSpace:%s ChromaLpf:%s "
            "SatCos:%d SatSine:%d\n",
               m_pFpCtx->LoopNum,
               ColorUtils::FormatToString((ColorUtils::Format)m_Picks.Format).c_str(),
               m_Picks.X,
               m_Picks.Y,
               m_Picks.Size,
               m_Picks.HotSpotX,
               m_Picks.HotSpotY,
               m_Picks.Composition,
               GainOffsetDesc.c_str(),
               GamutCSCDesc.c_str(),
               LutModeDesc.c_str(),
               DitherDesc.c_str(),
               ColorUtils::FormatToString((ColorUtils::Format)m_Picks.BackgroundSurfaceFormat).c_str(),
               m_Picks.ScalerH,
               m_Picks.ScalerV,
               m_Picks.EnableStereoLwrsor ? "Enabled" : "Disabled",
               m_Picks.EnableStereoBackground ? "Enabled" : "Disabled",
               m_Picks.EnableDpFramePacking ? "Enabled" : "Disabled",
               m_Picks.Eps.DynamicRange == EvoProcampSettings::CEA ? "CEA" : "VESA",
               m_Picks.Eps.RangeCompression ? "Enabled" : "Disabled",
               colorSpaceDesc.c_str(),
               m_Picks.Eps.ChromaLpf == EvoProcampSettings::ON ? "ON" : "AUTO",
               m_Picks.Eps.SatCos,
               m_Picks.Eps.SatSine
        );

        // Custom raster to create EDID independent conditions:
        EvoRasterSettings ers;
        CreateRaster(ScaledModeWidth, ScaledModeHeight,
                     m_StressMode.RasterWidth, m_StressMode.RasterHeight,
                     m_StressMode.RefreshRate,
                     m_ForceDPLaneCount, m_ForceDPLinkBW, &ers);

        if (m_Use16BitDepthAtOR)
        {
            ers.ORPixelDepthBpp = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_16_422;
            ers.Format = LW5070_CTRL_CMD_IS_MODE_POSSIBLE_PARAMS_FORMAT_A1R5G5B5;
        }
        if (m_LwrsorDisplayIsDP &&
            ((m_LwrrentDPLaneCount == 8) || (m_TestSingleHeadMultipleStreams && m_Variant != VariantNormal)))
        {
            ers.ReorderBankWidthSize = ers.ActiveX/2;
        }
        else
        {
            ers.ReorderBankWidthSize = 0;
        }

        // In reduced mode just test the max pixel clock possible from IMP
        if (m_EnableReducedMode)
        {
            ers.PixelClockHz = GetEvoLwrsPclk(m_StressMode.RasterWidth,
                                              m_StressMode.RasterHeight,
                                              m_StressMode.RefreshRate);
            // Assuming that a SetMode is already active and dispclk vmin cap
            // is set to one of the dispclocks which should make the max
            // found pclk always valid.
            Display::MaxPclkQueryInput maxPclkQuery = {pDisplay->Selected(),
                                                       &ers};
            vector<Display::MaxPclkQueryInput> maxPclkQueryVector(1, maxPclkQuery);

            CHECK_RC(pDisplay->SetRasterForMaxPclkPossible(&maxPclkQueryVector, NullMaxPclkLimit));
            Printf(GetVerbosePrintPri(),
                    "\tDetected max pclk = %d on 0x%08x\n",
                    ers.PixelClockHz, pDisplay->Selected());
        }

        // Apply a randomly selected OR pixel depth
        if (m_EnableAllORPixelDepths &&
            (m_Picks.ORPixelBpp != LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_DEFAULT))
            ers.ORPixelDepthBpp = m_Picks.ORPixelBpp;

        CHECK_RC(pDisplay->SetTimings(&ers));
        Printf(GetVerbosePrintPri(),
               "Configured OR pixel depth = %d on 0x%08x\n",
               ers.ORPixelDepthBpp, pDisplay->Selected());

        ColorUtils::Format BgFormat = (ColorUtils::Format)m_Picks.BackgroundSurfaceFormat;
        UINT64 BgID = (UINT64(m_SetModeIdx)<<32) | BgFormat;

        CHECK_RC(PrepareBackground(m_SetModeIdx, BgFormat));
        CHECK_RC(PrepareLwrsorImage(LwrsorSurfaceIdx));

        CHECK_RC(pDisplay->SetProcampSettings(&m_Picks.Eps));

        // The following SetMode has to always occur (to make effective
        // the values from SetTimings, SetProcampSettings, etc):
        pDisplay->SetPendingSetModeChange();

        CHECK_RC(pDisplay->SetMode(ScaledModeWidth,
            ScaledModeHeight,
            ColorUtils::PixelBits(BgFormat),
            m_StressMode.RefreshRate));

        if (m_Picks.EnableStereoBackground)
        {
            CHECK_RC(pDisplay->SetImageStereo(m_Backgrounds[BgID].first,
                                              m_Backgrounds[BgID].second));
        }
        else
        {
            CHECK_RC(pDisplay->SetImage(m_Backgrounds[BgID].first));
            CHECK_RC(SetDpFramePacking(m_Picks.EnableDpFramePacking));
        }

        for (size_t Idx = 0; Idx < m_LwrsorDisplays.size(); Idx++)
        {
            UINT32 SingleDisplay = m_LwrsorDisplays[Idx];
            if (m_Picks.EnableGainOffset)
                CHECK_RC(pDisplay->SetProcessingGainOffset(
                    SingleDisplay, Display::BASE_CHANNEL_ID, m_Picks.EnableGainOffset,
                    m_Picks.Gains, m_Picks.Offsets));

            if (m_Picks.EnableGamutColorSpaceColwersion)
                CHECK_RC(pDisplay->SetGamutColorSpaceColwersion(
                    SingleDisplay, m_Picks.ColorSpaceColwersionChannel,
                    m_Picks.ColorSpaceColwersionMatrix));

            if (m_Picks.EnableLUT)
                CHECK_RC(pDisplay->GetEvoDisplay()->SetBaseLUT(SingleDisplay, m_Picks.LUTMode,
                    &m_LUTValues[0], 2 * EvoDisplay::LUTMode2NumEntries(m_Picks.LUTMode)));

            if (m_Picks.EnableDither)
                CHECK_RC(pDisplay->SetDitherMode(SingleDisplay, m_Picks.EnableDither,
                    (Display::DitherBits)m_Picks.DitherBits,
                    m_Picks.DitherMode, m_Picks.DitherPhase));
        }

        if (m_Picks.EnableStereoLwrsor)
        {
            CHECK_RC(pDisplay->GetEvoDisplay()->SetLwrsorImageStereo(
                &m_LwrsorImages[LwrsorSurfaceIdx][0],
                &m_LwrsorImages[LwrsorSurfaceIdx][1],
                m_Picks.HotSpotX,
                m_Picks.HotSpotY,
                m_Picks.Composition));
            CHECK_RC(pDisplay->GetEvoDisplay()->SetLwrsorPosStereo(m_Picks.X, m_Picks.Y,
                                                  m_Picks.X+4, m_Picks.Y+1));
        }
        else
        {
            CHECK_RC(pDisplay->GetEvoDisplay()->SetLwrsorImage(
                &m_LwrsorImages[LwrsorSurfaceIdx][0],
                m_Picks.HotSpotX,
                m_Picks.HotSpotY,
                m_Picks.Composition));
            CHECK_RC(pDisplay->GetEvoDisplay()->SetLwrsorPos(m_Picks.X, m_Picks.Y));
        }
        LwrsorSurfaceIdx ^= 1;

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

        if (m_EnableDisplayRegistersDump)
        {
            pDisplay->DumpDisplayRegisters(GetVerbosePrintPri());
        }

        // It is assumed that "get CRC" function will allow for at least one
        // new frame to draw.
        // Otherwise we will need to wait for a new frame here.

        if (OK != (rc = m_pGolden->Run()))
        {
            UINT32 CombinedLwrsorDisplays = 0;
            for (size_t Idx = 0; Idx < m_LwrsorDisplays.size(); Idx++)
                CombinedLwrsorDisplays |= m_LwrsorDisplays[Idx];
            Printf(Tee::PriHigh,
                "Golden miscompare after loop %i on displays %08x.\n",
                m_pFpCtx->LoopNum, CombinedLwrsorDisplays);

            AddOrUpdateCoverageData(CombinedLwrsorDisplays, false);
        }

        if (m_pTestConfig->Verbose() || (rc != OK))
        {
            pDisplay->DumpDisplayInfo();
        }

        CleanupDisplay();

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

        // Execute only one loop for high resolution stress modes:
        if (!m_MultiHeadStressMode)
            break;
    }

    return rc;
}

void EvoLwrs::CleanupDisplay()
{
    Display *pDisplay = GetDisplay();

    pDisplay->GetEvoDisplay()->SetLwrsorImage(NULL, 0, 0, 0);

    for (size_t Idx = 0; Idx < m_LwrsorDisplays.size(); Idx++)
    {
        UINT32 SingleDisplay = m_LwrsorDisplays[Idx];
        if (m_Picks.EnableGainOffset)
        {
            float ZeroRGB[Display::NUM_RGB] = {0, 0, 0};
            pDisplay->SetProcessingGainOffset(
                SingleDisplay, Display::BASE_CHANNEL_ID, false, ZeroRGB, ZeroRGB);
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
                SingleDisplay, Display::CORE_CHANNEL_ID, IdentityMatrix);
        }

        if (m_Picks.EnableLUT)
            pDisplay->GetEvoDisplay()->SetBaseLUT(SingleDisplay, Display::LUTMODE_LORES, NULL, 0);

        if (m_Picks.EnableDither)
            pDisplay->SetDitherMode(SingleDisplay, false, Display::DITHER_TO_6_BITS,
                Display::DITHERMODE_DYNAMIC_ERR_ACC, 0);
    }

    if (m_Picks.EnableDpFramePacking)
        SetDpFramePacking(false);

    pDisplay->SetImage((Surface2D*)NULL);

    pDisplay->SetProcampSettings(NULL);
}

void EvoLwrs::DisableSimulatedDP()
{
    Display *pDisplay = GetDisplay();

    pDisplay->DetachAllDisplays();

    pDisplay->DisableSimulatedDPDevices(m_PrimaryConnector,
        m_SecondaryConnector, Display::SkipRealDisplayDetection);
}

void EvoLwrs::DisableSingleHeadMultistream()
{
    Display *pDisplay = GetDisplay();

    pDisplay->SetSingleHeadMultiStreamMode(m_LwrsorDisplays[0], 0,
        Display::SHMultiStreamDisabled);
}

//----------------------------------------------------------------------------
void EvoLwrs::ClearStressModes()
{
    m_StressModes.clear();
}

//----------------------------------------------------------------------------
void EvoLwrs::AddStressMode(ModeDesc * pMode)
{
    if (pMode)
        m_StressModes.push_back(*pMode);
}

//-----------------------------------------------------------------------------
bool EvoLwrs::IsDisplayIdSkipped(UINT32 displayID)
{
    Display *pDisplay = GetDisplay();
    Display::ORCombinations ORCombos;

    // Check if the current OR/protocol needs to be skipped
    ORCombos.clear();
    if (OK != pDisplay->GetValidORCombinations(&ORCombos,
                                               false))
        return false;

    UINT32 ORIndex = 0xFFFFFFFF, ORType = 0xFFFFFFFF;
    UINT32 ORProtocol = 0xFFFFFFFF;
    if (OK != pDisplay->GetORProtocol(displayID,
                                      &ORIndex, &ORType,
                                      &ORProtocol))
        return false;

    vector <Display::ORInfo>::iterator ORInfoIter;
    for (ORInfoIter = ORCombos.begin();
         ORInfoIter != ORCombos.end(); ORInfoIter++)
    {
        // Auto-skip OR/protocol combos that are redundant with
        // the non-split SOR mode *if* the non-split SOR mode is
        // not being already skipped
        if ((ORInfoIter->IsRedundant) && !(m_SkipSplitSORMask & 0x1))
            continue;

        if (ORIndex == ORInfoIter->ORIndex &&
            ORType == ORInfoIter->ORType)
        {
            if ((ORProtocol ==
                              LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DUAL_TMDS) &&
                (m_EvoRaster.PixelClockHz <= 165000000) &&
                (ORInfoIter->Protocol ==
                           LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A ||
                 ORInfoIter->Protocol ==
                            LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_B))
            {
                // If DUAL_TMDS is being tested, and pixel clock <= 165 MHz,
                // TMDS_A or TMDS_B may also be possible on the split SOR combos
                break;
            }
            else if (ORProtocol == ORInfoIter->Protocol)
            {
                // Found a perfect OR/protocol match
                break;
            }
        }
    }

    return (ORInfoIter == ORCombos.end());
}

//-----------------------------------------------------------------------------
// Helper API to check if a display is TMDS single link capable in split SOR
//-----------------------------------------------------------------------------
bool EvoLwrs::IsTmdsSingleLinkCapable(UINT32 displayID, TmdsLinkType LinkType)
{
    MASSERT(m_TestSplitSors);
    Display *pDisplay = GetDisplay();
    Display::ORCombinations ORCombos;

    // Check if the current OR/protocol needs to be skipped
    ORCombos.clear();
    if (OK != pDisplay->GetValidORCombinations(&ORCombos,
                                               false))
        return false;

    UINT32 ORIndex = 0xFFFFFFFF, ORType = 0xFFFFFFFF;
    UINT32 ORProtocol = 0xFFFFFFFF;
    if (OK != pDisplay->GetORProtocol(displayID,
                                      &ORIndex, &ORType,
                                      &ORProtocol))
        return false;

    vector <Display::ORInfo>::iterator ORInfoIter;
    for (ORInfoIter = ORCombos.begin();
         ORInfoIter != ORCombos.end(); ORInfoIter++)
    {
        if (ORIndex != ORInfoIter->ORIndex ||
            ORType != ORInfoIter->ORType)
        continue;

        if ((LinkType == TMDS_LINK_A) &&
            ORInfoIter->Protocol ==
                             LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A)
            break;

        if ((LinkType == TMDS_LINK_B) &&
            ORInfoIter->Protocol ==
                             LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_B)
            break;
    }

    return (ORInfoIter != ORCombos.end());
}

//-----------------------------------------------------------------------------
// Helper API to detect if image scaling needs to be performed
bool EvoLwrs::IsImageScalingNeeded(UINT32 StressModeIdx)
{
    const ModeDesc &StressMode = m_StressModes[StressModeIdx];

    if ((StressMode.RasterWidth != StressMode.ImageWidth) ||
        (StressMode.RasterHeight != StressMode.ImageHeight))
    {
        return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
// Helper API to add/update existing coverage data on a per display, per
// split SOR basis
void EvoLwrs::AddOrUpdateCoverageData(UINT32 Display, bool IsTestPassing)
{
    UINT32 SplitSorIdx = m_LwrrSorConfigIdx;

    vector<DispCoverageInfo>::iterator DispCovIt;
    for (DispCovIt = m_CoverageData[SplitSorIdx].begin();
         DispCovIt != m_CoverageData[SplitSorIdx].end();
         DispCovIt++)
    {
        if (DispCovIt->Display == Display)
        {
            // If the display has been passing, mark the latest pass/fail
            if (DispCovIt->IsTestPassing)
                DispCovIt->IsTestPassing = IsTestPassing;
            break;
        }
    }
    if (DispCovIt == m_CoverageData[SplitSorIdx].end())
    {
        DispCoverageInfo NewEntry = {Display, IsTestPassing};
        m_CoverageData[SplitSorIdx].push_back(NewEntry);
    }
    return;
}

void EvoLwrs::CleanSORExcludeMask(UINT32 displayID)
{
    StickyRC rc;
    Display *pDisplay = GetDisplay();
    rc = pDisplay->DetachAllDisplays();
    rc = pDisplay->SetSORExcludeMask(displayID, 0);
}

//-----------------------------------------------------------------------------
/* static */ void EvoLwrs::CreateRaster
(
    UINT32 ScaledWidth,
    UINT32 ScaledHeight,
    UINT32 StressModeWidth,
    UINT32 StressModeHeight,
    UINT32 RefreshRate,
    UINT32 LaneCount,
    UINT32 LinkRate,
    EvoRasterSettings *ers
)
{
    // Values in the raster were determined by experiments so they work
    // both for DACs and SORs in various modes.
    // They should also have plenty of vblank margin across the full
    // range of pclks used, so that there are no problems callwlating
    // the DMI duration value.
    *ers = EvoRasterSettings(ScaledWidth+200,  101, ScaledWidth+100+49, 149,
                             ScaledHeight+150,  50, ScaledHeight+50+50, 100,
                             1, 0,
                             GetEvoLwrsPclk(StressModeWidth,
                                            StressModeHeight,
                                            RefreshRate),
                             LaneCount, LinkRate,
                             false);
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(EvoLwrs,
                  SetStressModes,
                  1,
                  "Set the stress modes that EvoLwrs will use")
{
    JavaScriptPtr pJs;

    // Check the arguments.
    JsArray modes;
    if
    (
            (NumArguments != 1)
            || (OK != pJs->FromJsval(pArguments[0], &modes))
    )
    {
        JS_ReportError(pContext, "Usage: EvoLwrs.SetStressModes([modes])");
        return JS_FALSE;
    }

    EvoLwrs * pEvoLwrs;
    if ((pEvoLwrs = JS_GET_PRIVATE(EvoLwrs, pContext, pObject, "EvoLwrs")) != 0)
    {
        pEvoLwrs->ClearStressModes();
        for (UINT32 i = 0; i < modes.size(); i++)
        {
            JsArray jsa;
            ModeDesc tmpMode;
            bool paramError = false;

            if (
               (OK != pJs->FromJsval(modes[i], &jsa)) ||
               ((jsa.size() != 4) && (jsa.size() != 6)))
            {
                JS_ReportError(pContext, "Usage: EvoLwrs.SetStressModes([modes])");
                return JS_FALSE;
            }

            if ((jsa.size() == 4) &&
                ((OK != pJs->FromJsval(jsa[0], &tmpMode.RasterWidth)) ||
                 (OK != pJs->FromJsval(jsa[1], &tmpMode.RasterHeight)) ||
                 (OK != pJs->FromJsval(jsa[2], &tmpMode.Depth)) ||
                 (OK != pJs->FromJsval(jsa[3], &tmpMode.RefreshRate))))
            {
                paramError = true;
            }
            else if ((jsa.size() == 6) &&
                ((OK != pJs->FromJsval(jsa[0], &tmpMode.RasterWidth)) ||
                 (OK != pJs->FromJsval(jsa[1], &tmpMode.RasterHeight)) ||
                 (OK != pJs->FromJsval(jsa[2], &tmpMode.Depth)) ||
                 (OK != pJs->FromJsval(jsa[3], &tmpMode.RefreshRate)) ||
                 (OK != pJs->FromJsval(jsa[4], &tmpMode.ImageWidth)) ||
                 (OK != pJs->FromJsval(jsa[5], &tmpMode.ImageHeight))))
                paramError = true;

            // Flag any parameter errors detected
            if (paramError)
            {
                JS_ReportError(pContext,
                               "Usage: EvoLwrs.SetStressModes([modes])"
                               "invalid parameters");
                return JS_FALSE;
            }

            // If there is no image width/height provided,
            // assume raster width and height
            if (jsa.size() == 4)
            {
                tmpMode.ImageWidth = tmpMode.RasterWidth;
                tmpMode.ImageHeight = tmpMode.RasterHeight;
            }

            pEvoLwrs->AddStressMode(&tmpMode);
        }

        return JS_TRUE;
    }

    return JS_FALSE;
}
