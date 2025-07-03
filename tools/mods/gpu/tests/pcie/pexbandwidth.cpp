/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2021 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file pexbandwidth.cpp
 * @brief: The test to test out PCIE bandwidth
 *
 */

#include "core/include/cnslmgr.h"
#include "core/include/mle_protobuf.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "class/cl90b5.h" // GF100_DMA_COPY
#include "class/cla0b5.h" // KEPLER_DMA_COPY_A
#include "class/clb0b5.h" // MAXWELL_DMA_COPY_A
#include "class/clc0b5.h" // PASCAL_DMA_COPY_A
#include "class/clc3b5.h" // VOLTA_DMA_COPY_A
#include "class/clc5b5.h" // TURING_DMA_COPY_A
#include "class/clc6b5.h" // AMPERE_DMA_COPY_A
#include "class/clc7b5.h" // AMPERE_DMA_COPY_B
#include "class/clc8b5.h" // HOPPER_DMA_COPY_A
#include "class/clc9b5.h" // BLACKWELL_DMA_COPY_A
#include "device/interface/pcie.h"
#include "gpu/include/dmawrap.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfsub.h"
#include "gpu/uphy/uphyreglogger.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/utility/surf2d.h"
#include "pextest.h"

#include <memory>

class PexBandwidth: public PexTest
{
public:
    PexBandwidth();
    ~PexBandwidth();

    virtual RC   Setup();
    virtual RC   RunTest();
    virtual RC   Cleanup();
    virtual bool IsSupported();
    virtual void PrintJsProperties(Tee::Priority pri);

    enum TRANSFER_DIRECTION_MASKS
    {
        TD_FB_TO_SSYSMEM = (0x1 << 0),
        TD_SYSMEM_TO_FB = (0x1 << 1),
        TD_BOTH = (TD_FB_TO_SSYSMEM | TD_SYSMEM_TO_FB)
    };

    SETGET_PROP(Use2D, bool);
    SETGET_PROP(NumErrorsToPrint, UINT32);
    SETGET_PROP(SurfSizeFactor, UINT32);
    SETGET_PROP(SurfHeight, UINT32);
    SETGET_PROP(SurfWidth, UINT32);
    SETGET_PROP(SurfDepth, UINT32);
    SETGET_PROP(LinkWidth, UINT32);
    SETGET_PROP(LinkSpeedsToTest, UINT32);
    SETGET_PROP(OnlyMaxSpeed, bool);
    SETGET_PROP(Gen1Threshold, UINT32);
    SETGET_PROP(Gen2Threshold, UINT32);
    SETGET_PROP(Gen3Threshold, UINT32);
    SETGET_PROP(Gen4Threshold, UINT32);
    SETGET_PROP(Gen5Threshold, UINT32);
    SETGET_PROP(AllowSpeedChange, bool);
    SETGET_PROP(IgnoreBwCheck, bool);
    SETGET_PROP(ShowBandwidthData, bool);
    SETGET_PROP(DisableSurfSwap, bool);
    SETGET_PROP(TransDir, UINT32);
    SETGET_PROP(PauseBeforeCopy, bool);
    SETGET_PROP(SrcSurfMode, UINT32);
    SETGET_PROP(SrcSurfModePercent, UINT32);
    SETGET_PROP(MatchTransData, bool);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(EnableUphyLogging, bool);
    SETGET_PROP(UseOneCeOnly, bool);

private:
    enum SURFACES
    {
        SURF_FB = 0,
        SURF_GOLD_A = 1,
        SURF_GOLD_B = 2,
        SURF_SYS_A = 3,
        SURF_SYS_B = 4, // Not used in single-direction tests
        SURF_MAX
    };

    enum LINK_SPEED_MASKS
    {
        TEST_GEN1 = (1<<0),
        TEST_GEN2 = (1<<1),
        TEST_GEN3 = (1<<2),
        TEST_GEN4 = (1<<3),
        TEST_GEN5 = (1<<4),
        TEST_ALL  = (TEST_GEN1|TEST_GEN2|TEST_GEN3|TEST_GEN4|TEST_GEN5),
    };

    //! Enum describing fill patterns available
    enum PatternType
    {
        PT_BARS
       ,PT_RANDOM
       ,PT_SOLID
       ,PT_LINES
       ,PT_ALTERNATE
    };

    typedef unique_ptr<Surface2D> Surface2DPtr;

    RC ValidateTestArgs();
    RC InitProperties();
    RC SetupSurfaces();
    RC FillGoldSurfaces();
    RC FillSurface
    (
        Surface2D* pSurf
        ,PatternType pt
        ,UINT32 patternData
        ,UINT32 patternData2
        ,UINT32 patternHeight
    );
    RC FillSurfaceHelper
    (
        Surface2D* pSurf
        , Surface2D* pPatSurf
        , PatternType pt
        , UINT32 patternData
        , UINT32 patternData2
        , UINT32 patternHeight
    );
    RC InnerTest();
    RC TestAtPexSpeed(Pci::PcieLinkSpeed LinkSpeed);
    RC DoOneTransferIter(Pci::PcieLinkSpeed LinkSpeed);
    RC DoTransfers(UINT32 *pFbToSysTimeUs, UINT32 *pSysToFbTimeUs);
    RC DoOneTransferLoop(UINT32 *pFbToSysTimeUs, UINT32 *pSysToFbTimeUs);
    RC DoTransfer(SURFACES src, SURFACES dst, UINT32 *time);
    RC CheckSurfaces();
    RC CheckSurface(SURFACES surf, SURFACES goldSurf);
    RC FindCopyEngineInstance(vector<UINT32> * pEngineList, UINT32 * pEngineInst);
    RC ResetSrcSurface(SURFACES src, SURFACES dst);
    void Pause(string linkSpeedStr);
    RC RetrieveCopyStats(UINT32 *pTimeUs);

    UINT32           m_SurfSizeFactor     = 10;
    UINT32           m_SurfHeight         = 1280;
    UINT32           m_SurfHeightSize     = m_SurfSizeFactor*m_SurfHeight;
    UINT32           m_SurfWidth          = 1024;
    UINT32           m_SurfDepth          = 32;
    UINT32           m_SurfPitch          = m_SurfWidth*sizeof(UINT32);
    UINT32           m_NumErrorsToPrint   = 0;
    UINT32           m_LinkWidth          = 0;
    UINT32           m_LinkSpeedsToTest   = TEST_ALL;    // this is a test mask selectable from JS
    bool             m_OnlyMaxSpeed       = false;
    UINT32           m_Gen1Threshold      = 85;
    UINT32           m_Gen2Threshold      = 85;
    UINT32           m_Gen3Threshold      = 75;
    UINT32           m_Gen4Threshold      = 75;
    UINT32           m_Gen5Threshold      = 75;
    bool             m_IgnoreBwCheck      = false;
    bool             m_ShowBandwidthData  = false;
    bool             m_Use2D              = false;
    bool             m_AllowSpeedChange   = true;
    FLOAT64          m_TimeoutMs          = Tasker::NO_TIMEOUT;
    DmaWrapper       m_DmaWrap;
    DmaWrapper       m_DmaWrap2;
    GpuTestConfiguration *m_pTestConfig   = nullptr;
    bool             m_DisableSurfSwap    = false;
    vector<SURFACES> m_LwrSurfs           = { SURF_FB, SURF_GOLD_A, SURF_GOLD_B, SURF_SYS_A, SURF_SYS_B }; //$
    vector<Surface2DPtr> m_Surfs;
    UINT32           m_TransDir           = TD_BOTH;
    bool             m_PauseBeforeCopy    = false;
    UINT32           m_SrcSurfMode        = 6;
    UINT32           m_SrcSurfModePercent = 100;
    bool             m_MatchTransData     = false;
    bool             m_KeepRunning        = false;
    bool             m_EnableUphyLogging  = false;
    bool             m_UseOneCeOnly       = true;
};

//-----------------------------------------------------------------------------
// Exposed JS properties
JS_CLASS_INHERIT(PexBandwidth, PexTest, "Pcie bandwidth test");

CLASS_PROP_READWRITE(PexBandwidth, Use2D, bool,
                     "bool: use 2D engine for one of the DMAs");

CLASS_PROP_READWRITE(PexBandwidth, NumErrorsToPrint, UINT32,
                     "UINT32: on miscompares, this is the number of errors printed");

CLASS_PROP_READWRITE(PexBandwidth, SurfSizeFactor, UINT32,
                     "UINT32: determines size of transfer - multiple of the default screen size");

CLASS_PROP_READWRITE(PexBandwidth, SurfHeight, UINT32,
                     "UINT32: surface height");

CLASS_PROP_READWRITE(PexBandwidth, SurfWidth, UINT32,
                     "UINT32: surface width");

CLASS_PROP_READWRITE(PexBandwidth, SurfDepth, UINT32,
                     "UINT32: surface depth");

CLASS_PROP_READWRITE(PexBandwidth, LinkWidth, UINT32,
                     "UINT32: forced link width");

CLASS_PROP_READWRITE(PexBandwidth, LinkSpeedsToTest, UINT32,
                     "UINT32: mask of link speeds to test");

CLASS_PROP_READWRITE(PexBandwidth, OnlyMaxSpeed, bool,
                     "bool: limit testing to max possible speed (default false)");

CLASS_PROP_READWRITE(PexBandwidth, Gen1Threshold, UINT32,
                     "UINT32: percentage threshold Gen1 BW must have");

CLASS_PROP_READWRITE(PexBandwidth, Gen2Threshold, UINT32,
                     "UINT32: percentage threshold Gen2 BW must have");

CLASS_PROP_READWRITE(PexBandwidth, Gen3Threshold, UINT32,
                     "UINT32: percentage threshold Gen3 BW must have");

CLASS_PROP_READWRITE(PexBandwidth, Gen4Threshold, UINT32,
                     "UINT32: percentage threshold Gen4 BW must have");

CLASS_PROP_READWRITE(PexBandwidth, Gen5Threshold, UINT32,
                     "UINT32: percentage threshold Gen5 BW must have");

CLASS_PROP_READWRITE(PexBandwidth, AllowSpeedChange, bool,
                     "bool: change to different link speeds to test out bandwidth for each");

CLASS_PROP_READWRITE(PexBandwidth, IgnoreBwCheck, bool,
                     "bool: allow test to not fail due to low bandwidth at higher link speeds");

CLASS_PROP_READWRITE(PexBandwidth, ShowBandwidthData, bool,
                     "bool: print out bandwidth data");

CLASS_PROP_READWRITE(PexBandwidth, DisableSurfSwap, bool,
                     "bool: diable use an alternating gold surface on each loop");

CLASS_PROP_READWRITE(PexBandwidth, TransDir, UINT32,
                     "UINT32: define whether to test Fb->Sys, Sys->Fb, or both");

CLASS_PROP_READWRITE(PexBandwidth, PauseBeforeCopy, bool,
                     "bool: pause before initiating copies");

CLASS_PROP_READWRITE(PexBandwidth, SrcSurfMode, UINT32,
                     "UINT32: Source surface pattern mode (0 = random & bars, 1 = all random, 2 = all zeroes,"
                     " 3 = alternate 0x0 & 0xFFFFFFFF, 4 = alternate 0xAAAAAAAA & 0x55555555,"
                     " 5 = random & zeroes, 6 = default = RGB bars & address");

CLASS_PROP_READWRITE(PexBandwidth, SrcSurfModePercent, UINT32,
                     "UINT32: What percent of the current gold surface to use in each loop."
                     " The remaining percent will be filled with the second defined"
                     " pattern in SrcSurfMode (if the current mode has a second pattern)."
                     " It has no effect if the mode only defines one pattern.");

CLASS_PROP_READWRITE(PexBandwidth, MatchTransData, bool,
                     "bool: Send the same data patterns in both directions."
                     " Has no effect if only sending data in one direction");

CLASS_PROP_READWRITE(PexBandwidth, KeepRunning, bool,
                     "bool: The test will keep running as long as this flag is set.");

CLASS_PROP_READWRITE(PexBandwidth, EnableUphyLogging, bool,
                     "bool: Enable logging of UPHY registers after each copy.");

CLASS_PROP_READWRITE(PexBandwidth, UseOneCeOnly, bool,
                     "bool: Force the test to use only a single copy engine.");

//-----------------------------------------------------------------------------

PexBandwidth::PexBandwidth()
{
    m_pTestConfig = GetTestConfiguration();
}
//-----------------------------------------------------------------------------
PexBandwidth::~PexBandwidth()
{
    m_LwrSurfs.clear();
}
//-----------------------------------------------------------------------------
bool PexBandwidth::IsSupported()
{
    // if the current chipset does not support, the test will fail
    if (GetBoundGpuSubdevice()->BusType() != Gpu::PciExpress)
    {
        Printf(Tee::PriLow, "PEX bandwidth not supported by iGPU\n");
        return false;
    }
    DmaCopyAlloc dmaCopyEng;
    if (!dmaCopyEng.IsSupported(GetBoundGpuDevice()))
    {
        return false;
    }
    if (m_Use2D)
    {
        TwodAlloc twoDEngine;
        if (!twoDEngine.IsSupported(GetBoundGpuDevice()))
        {
            return false;
        }
    }

    bool bSysmem = false;
    // Transactions will go over LwLink instead of PCIE
    if (OK != SysmemUsesLwLink(&bSysmem) || bSysmem)
        return false;

    return true;
}
//-----------------------------------------------------------------------------
RC PexBandwidth::Setup()
{
    RC rc;

    CHECK_RC(ValidateTestArgs());
    CHECK_RC(PexTest::Setup());
    CHECK_RC(InitProperties());
    CHECK_RC(GpuTest::AllocDisplay());
    CHECK_RC(SetupSurfaces());
    vector<UINT32> classIds;
    classIds.push_back(GF100_DMA_COPY);
    classIds.push_back(KEPLER_DMA_COPY_A);
    classIds.push_back(MAXWELL_DMA_COPY_A);
    classIds.push_back(PASCAL_DMA_COPY_A);
    classIds.push_back(VOLTA_DMA_COPY_A);
    classIds.push_back(TURING_DMA_COPY_A);
    classIds.push_back(AMPERE_DMA_COPY_A);
    classIds.push_back(AMPERE_DMA_COPY_B);
    classIds.push_back(HOPPER_DMA_COPY_A);
    classIds.push_back(BLACKWELL_DMA_COPY_A);

    vector<UINT32> engineList;
    Tee::Priority pri = GetVerbosePrintPri();

    CHECK_RC(GetBoundGpuDevice()->GetPhysicalEnginesFilteredByClass(classIds, &engineList));


    UINT32 firstCeInst = ~0U;
    UINT32 totalPces = 0U;
    if (m_Use2D)
    {
        Printf(Tee::PriNormal, "Using 2D Engine\n");
        Printf(Tee::PriNormal,
               " Warning: 2D copies don't always perform as well as Copy Engine copies;");
        Printf(Tee::PriNormal, " as a result this test may fail spuriously\n");
        if (!m_ShowBandwidthData)
        {
            Printf(Tee::PriNormal,
                   " Recommend using \"-testarg PexBandwidth ShowBandwidthData true\"\n");
        }
        CHECK_RC(m_DmaWrap.Initialize(m_pTestConfig,
                    m_TestConfig.NotifierLocation(),
                    DmaWrapper::TWOD));
    }
    else
    {
        CHECK_RC(FindCopyEngineInstance(&engineList, &firstCeInst));
        Printf(pri, "Using CE instance %d\n", firstCeInst);
        CHECK_RC(m_DmaWrap.Initialize(m_pTestConfig,
                    m_TestConfig.NotifierLocation(),
                    DmaWrapper::COPY_ENGINE,
                    firstCeInst));
        LW2080_CTRL_CE_GET_CE_PCE_MASK_PARAMS pceMaskParams = { };
        pceMaskParams.ceEngineType = LW2080_ENGINE_TYPE_COPY(firstCeInst);
        CHECK_RC(LwRmPtr()->ControlBySubdevice(GetBoundGpuSubdevice(),
                                               LW2080_CTRL_CMD_CE_GET_CE_PCE_MASK,
                                               &pceMaskParams,
                                               sizeof(pceMaskParams)));
        totalPces = Utility::CountBits(pceMaskParams.pceMask);
    }

    // If we use 2D then we still need to use a CE to saturate the PCIE,
    // but we don't need there to be more than the one, if there was only one
    // PCE attached to the LCE then we also need another LCE
    if (m_Use2D || ((totalPces < 2) && !m_UseOneCeOnly))
    {
        if (!engineList.empty())
        {
            UINT32 secondCeInst;
            CHECK_RC(FindCopyEngineInstance(&engineList, &secondCeInst));
            Printf(pri, "use new copy engine instance %d for the second DMA\n", secondCeInst);
            CHECK_RC(m_DmaWrap2.Initialize(m_pTestConfig,
                                           m_TestConfig.NotifierLocation(),
                                           DmaWrapper::COPY_ENGINE,
                                           secondCeInst));
        }
        else
        {
            if (firstCeInst == ~0U)
            {
                CHECK_RC(FindCopyEngineInstance(&engineList, &firstCeInst));
            }
            // use the same instance of CE for now
            Printf(pri, "using copy engine instance %d for the second DMA\n", firstCeInst);
            CHECK_RC(m_DmaWrap2.Initialize(m_pTestConfig,
                                           m_TestConfig.NotifierLocation(),
                                           DmaWrapper::COPY_ENGINE,
                                           firstCeInst));
        }
    }

    return rc;
}
//-----------------------------------------------------------------------------
RC PexBandwidth::Cleanup()
{
    StickyRC rc;

    rc = GetDisplayMgrTestContext()->DisplayImage(
        static_cast<Surface2D*>(nullptr), DisplayMgr::WAIT_FOR_UPDATE);

    rc = m_DmaWrap.Cleanup();
    rc = m_DmaWrap2.Cleanup();

    m_Surfs.clear();

    rc = PexTest::Cleanup();
    return rc;
}
//-----------------------------------------------------------------------------
RC PexBandwidth::ValidateTestArgs()
{
    RC rc;

    if (m_TransDir == 0 || m_TransDir > TD_BOTH)
    {
        Printf(Tee::PriError, "TransDir received invalid argument: %u. \
                               \lwalid arguments are: \
                               \n 1: FbToSysmem, \
                               \n 2: SysmemToFb, \
                               \n 3: Both\n", m_TransDir);
        rc = RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (m_SrcSurfModePercent > 100)
    {
        Printf(Tee::PriError, "SrcSurfModePercent must be between 0 and 100\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC PexBandwidth::InitProperties()
{
    RC rc;
    m_TimeoutMs  = m_pTestConfig->TimeoutMs();
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
//! \return OK if successful, not OK otherwise
void PexBandwidth::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tUse2D:             %s\n", m_Use2D ? "true" : "false");
    Printf(pri, "\tNumErrorsToPrint:  %d\n", m_NumErrorsToPrint);
    Printf(pri, "\tSurfSizeFactor:    %d\n", m_SurfSizeFactor);
    Printf(pri, "\tSurfHeight:        %d\n", m_SurfHeight);
    Printf(pri, "\tSurfWidth:         %d\n", m_SurfWidth);
    Printf(pri, "\tSurfDepth:         %d\n", m_SurfDepth);
    Printf(pri, "\tLinkWidth:         %d\n", m_LinkWidth);
    Printf(pri, "\tLinkSpeedsToTest:  0x%02x\n", m_LinkSpeedsToTest);
    Printf(pri, "\tOnlyMaxSpeed:      %s\n", m_OnlyMaxSpeed ? "true" : "false");
    Printf(pri, "\tGen1Threshold:     %d\n", m_Gen1Threshold);
    Printf(pri, "\tGen2Threshold:     %d\n", m_Gen2Threshold);
    Printf(pri, "\tGen3Threshold:     %d\n", m_Gen3Threshold);
    Printf(pri, "\tGen4Threshold:     %d\n", m_Gen4Threshold);
    Printf(pri, "\tGen5Threshold:     %d\n", m_Gen5Threshold);
    Printf(pri, "\tAllowSpeedChange:  %s\n",
           m_AllowSpeedChange ? "true" : "false");
    Printf(pri, "\tIgnoreBwCheck:     %s\n",
           m_IgnoreBwCheck? "true" : "false");
    Printf(pri, "\tShowBandwidthData: %s\n",
           m_ShowBandwidthData ? "true" : "false");
    Printf(pri, "\tDisableSurfSwap:   %s\n",
           m_DisableSurfSwap ? "true" : "false");
    Printf(pri, "\tTransDir:          0x%04x\n", m_TransDir);
    Printf(pri, "\tPauseBeforeCopy:   %s\n", m_PauseBeforeCopy ? "true" : "false");
    Printf(pri, "\tSrcSrfMode:        %d\n", m_SrcSurfMode);
    Printf(pri, "\tSrcSurfModePercent:%d%%\n", m_SrcSurfModePercent);
    Printf(pri, "\tMatchTransData:    %s\n", (m_MatchTransData) ? "true" : "false");
    Printf(pri, "\tKeepRunning:       %s\n", m_KeepRunning ? "true" : "false");
    Printf(pri, "\tEnableUphyLogging: %s\n", m_EnableUphyLogging ? "true" : "false");
    Printf(pri, "\tUseOneCeOnly:      %s\n", m_UseOneCeOnly ? "true" : "false");

    PexTest::PrintJsProperties(pri);
}

//-----------------------------------------------------------------------------
RC PexBandwidth::SetupSurfaces()
{
    RC rc;

    MASSERT(m_Surfs.empty());

    ColorUtils::Format colorFormat = ColorUtils::ColorDepthToFormat(m_SurfDepth);
    m_SurfHeightSize = m_SurfHeight*m_SurfSizeFactor;

    Printf(GetVerbosePrintPri(), "SurfaceHeight = %d\n", m_SurfHeightSize);

    unsigned int numSurfaces = SURF_MAX - (m_TransDir == TD_BOTH ? 0 : 1);
    for (unsigned int i = 0; i < numSurfaces; i++)
    {
        Surface2DPtr surf(new Surface2D());
        surf->SetWidth(m_SurfWidth);
        surf->SetHeight(m_SurfHeightSize);
        surf->SetColorFormat(colorFormat);
        surf->SetProtect(Memory::ReadWrite);
        surf->SetLayout(Surface2D::Pitch);

        switch (i)
        {
            case SURF_FB:
                surf->SetLocation(Memory::Fb);
                surf->SetDisplayable(true);
                break;
            case SURF_GOLD_A:
            case SURF_GOLD_B:
                surf->SetLocation(Memory::NonCoherent);
                break;
            case SURF_SYS_A:
            case SURF_SYS_B:
                surf->SetLocation(Memory::Coherent);
                break;
            default:
                MASSERT(!"Invalid number of surfaces");
                break;
        }

        m_Surfs.push_back(std::move(surf));
    }

    for (unsigned int i = 0; i < m_Surfs.size(); i++)
    {
        CHECK_RC(m_Surfs[i]->Alloc(GetBoundGpuDevice()));
    }
    m_SurfPitch = m_Surfs[0]->GetPitch();
    for (UINT32 i=1; i < m_Surfs.size(); i++)
    {
        if (m_Surfs[i]->GetLayout() == Surface2D::Pitch &&
            m_Surfs[i]->GetPitch() < m_SurfPitch)
        {
            m_SurfPitch = m_Surfs[i]->GetPitch();
            Printf(Tee::PriLow, "Adjusting SurfPitch to %u\n", m_SurfPitch);
        }
    }

    const auto imageToDisplay = m_Surfs[SURF_FB].get();
    GpuUtility::DisplayImageDescription desc;
    desc.CtxDMAHandle   = imageToDisplay->GetCtxDmaHandleIso();
    desc.Offset         = imageToDisplay->GetCtxDmaOffsetIso();
    desc.Height         = m_SurfHeight;
    desc.Width          = imageToDisplay->GetWidth();
    desc.AASamples      = 1;
    desc.Layout         = imageToDisplay->GetLayout();
    desc.Pitch          = imageToDisplay->GetPitch();
    desc.LogBlockHeight = imageToDisplay->GetLogBlockHeight();
    desc.NumBlocksWidth = imageToDisplay->GetBlockWidth();
    desc.ColorFormat    = imageToDisplay->GetColorFormat();

    CHECK_RC(GetDisplayMgrTestContext()->DisplayImage(&desc,
        DisplayMgr::DONT_WAIT_FOR_UPDATE));

    return rc;
}
//-----------------------------------------------------------------------------
RC PexBandwidth::RunTest()
{
    StickyRC rc;
    CHECK_RC(FillGoldSurfaces());

    Pcie *pGpuPcie = GetGpuPcie();
    Pci::PcieLinkSpeed origLinkSpeed = pGpuPcie->GetLinkSpeed(Pci::SpeedUnknown);
    UINT32 origLinkWidth = pGpuPcie->GetLinkWidth();

    if (m_LinkWidth)
    {
        rc = pGpuPcie->SetLinkWidth(m_LinkWidth);
    }

    // run for each PEX speed
    rc = InnerTest();

    if (m_LinkWidth)
    {
        rc = pGpuPcie->SetLinkWidth(origLinkWidth);
    }

    rc = pGpuPcie->SetLinkSpeed(origLinkSpeed);
    return rc;
}
//-----------------------------------------------------------------------------
RC PexBandwidth::InnerTest()
{
    StickyRC rc;
    bool bStopOnError = GetGoldelwalues()->GetStopOnError();

    if (!m_AllowSpeedChange)
    {
        CHECK_RC(DoOneTransferIter(GetGpuPcie()->GetLinkSpeed(Pci::SpeedUnknown)));
        return rc;
    }

    UINT32 maxSpeed = GetGpuPcie()->LinkSpeedCapability();
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    UINT32 LwrrPState;
    if (OK == pPerf->GetLwrrentPState(&LwrrPState))
    {
        if (Perf::ILWALID_PSTATE != LwrrPState)
        {
            UINT32 LwrMaxSpeed = 0;
            CHECK_RC(pPerf->GetPexSpeedAtPState(LwrrPState, &LwrMaxSpeed));
            if (maxSpeed > LwrMaxSpeed)
            {
                maxSpeed = LwrMaxSpeed;
            }

            maxSpeed = AllowCoverageHole(maxSpeed);
        }
    }

    if (m_OnlyMaxSpeed)
    {
        if (maxSpeed >= Pci::Speed32000MBPS)
        {
            m_LinkSpeedsToTest = TEST_GEN5;
        }
        if (maxSpeed >= Pci::Speed16000MBPS)
        {
            m_LinkSpeedsToTest = TEST_GEN4;
        }
        else if (maxSpeed >= Pci::Speed8000MBPS)
        {
            m_LinkSpeedsToTest = TEST_GEN3;
        }
        else if (maxSpeed >= Pci::Speed5000MBPS)
        {
            m_LinkSpeedsToTest = TEST_GEN2;
        }
        else
        {
            m_LinkSpeedsToTest = TEST_GEN1;
        }
    }

    bool bOneSpeedTested = false;
    do
    {
        if (m_LinkSpeedsToTest & TEST_GEN1)
        {
            rc = TestAtPexSpeed(Pci::Speed2500MBPS);
            bOneSpeedTested = true;
            if (bStopOnError)
                CHECK_RC(rc);
        }

        if ((maxSpeed >= Pci::Speed5000MBPS) && (m_LinkSpeedsToTest & TEST_GEN2))
        {
            rc = TestAtPexSpeed(Pci::Speed5000MBPS);
            bOneSpeedTested = true;
            if (bStopOnError)
                CHECK_RC(rc);
        }

        if ((maxSpeed >= Pci::Speed8000MBPS) && (m_LinkSpeedsToTest & TEST_GEN3))
        {
            rc = TestAtPexSpeed(Pci::Speed8000MBPS);
            bOneSpeedTested = true;
            if (bStopOnError)
                CHECK_RC(rc);
        }

        if ((maxSpeed >= Pci::Speed16000MBPS) && (m_LinkSpeedsToTest & TEST_GEN4))
        {
            rc = TestAtPexSpeed(Pci::Speed16000MBPS);
            bOneSpeedTested = true;
            if (bStopOnError)
                CHECK_RC(rc);
        }

        if ((maxSpeed >= Pci::Speed32000MBPS) && (m_LinkSpeedsToTest & TEST_GEN5))
        {
            rc = TestAtPexSpeed(Pci::Speed32000MBPS);
            bOneSpeedTested = true;
            if (bStopOnError)
                CHECK_RC(rc);
        }

        CHECK_RC(CheckPcieErrors(GetBoundGpuSubdevice()));

        if (!bOneSpeedTested)
        {
            Printf(Tee::PriError,
                   "No PCIE speeds found to test!!  MaxSpeed = %d, LinkSpeedsToTest = 0x%x\n",
                   maxSpeed, m_LinkSpeedsToTest);
            return RC::NO_TESTS_RUN;
        }
    } while (m_KeepRunning);

    return rc;
}
//-----------------------------------------------------------------------------
// TestAtPexSpeed
// 1) set a link speed

RC PexBandwidth::TestAtPexSpeed(Pci::PcieLinkSpeed LinkSpeed)
{
    RC rc;
    CHECK_RC(GetGpuPcie()->SetLinkSpeed(LinkSpeed));
    return DoOneTransferIter(LinkSpeed);
}

namespace
{
    string GetLinkSpeedStr(Pci::PcieLinkSpeed linkSpeed)
    {
        string pexGen = "Gen UNKNOWN";
        switch (linkSpeed)
        {
            case Pci::Speed2500MBPS:
                pexGen = "Gen 1";
                break;
            case Pci::Speed5000MBPS:
                pexGen = "Gen 2";
                break;
            case Pci::Speed8000MBPS:
                pexGen = "Gen 3";
                break;
            case Pci::Speed16000MBPS:
                pexGen = "Gen 4";
                break;
            case Pci::Speed32000MBPS:
                pexGen = "Gen 5";
                break;
            default:
                MASSERT(!"Unknown PEX Speed");
                break;
        }
        return pexGen;
    }
}

//-----------------------------------------------------------------------------
// DoOneTransferIter
// 2) Set up the ping-pong surfaces
// 3) Loops 3 surfaces
// 4) callwlate bandwidth
// 5) check surface
RC PexBandwidth::DoOneTransferIter(Pci::PcieLinkSpeed LinkSpeed)
{
    RC rc;
    const UINT32 linkWidth = GetGpuPcie()->GetLinkWidth();
    const FLOAT64 bandWidthGbitPerSec = LinkSpeed * linkWidth / 1000.0;

    Tee::Priority pri = GetVerbosePrintPri();
    if (m_ShowBandwidthData && (pri < Tee::PriNormal))
        pri = Tee::PriNormal;

    string pexGen = GetLinkSpeedStr(LinkSpeed);

    UINT32 threshold = 100;
    switch (LinkSpeed)
    {
        case Pci::Speed2500MBPS:
            threshold = m_Gen1Threshold;
            break;
        case Pci::Speed5000MBPS:
            threshold = m_Gen2Threshold;
            break;
        case Pci::Speed8000MBPS:
            threshold = m_Gen3Threshold;
            break;
        case Pci::Speed16000MBPS:
            threshold = m_Gen4Threshold;
            break;
        case Pci::Speed32000MBPS:
            threshold = m_Gen5Threshold;
            break;
        default:
            MASSERT(!"Unknown PEX Speed");
            break;
    }

    Pci::PcieLinkSpeed lwrLinkSpeed = GetGpuPcie()->GetLinkSpeed(Pci::SpeedUnknown);
    if (lwrLinkSpeed != LinkSpeed)
    {

        ByteStream bs;
        auto entry = Mle::Entry::pcie_link_speed_mismatch(&bs);

        Pcie *pPcie = GetGpuPcie();
        PexDevice* pPexDevUpstream;
        UINT32 port = 0;
        if ((RC::OK == pPcie->GetUpStreamInfo(&pPexDevUpstream, &port)) && pPexDevUpstream != NULL)
        {
            entry.link()
                .host()
                    .domain(pPexDevUpstream->GetDownStreamPort(port)->Domain)
                    .bus(pPexDevUpstream->GetDownStreamPort(port)->Bus)
                    .device(pPexDevUpstream->GetDownStreamPort(port)->Dev)
                    .function(pPexDevUpstream->GetDownStreamPort(port)->Func);
        }
        entry.link()
            .local()
                .domain(pPcie->DomainNumber())
                .bus(pPcie->BusNumber())
                .device(pPcie->DeviceNumber())
                .function(pPcie->FunctionNumber());
        entry.actual_link_speed_mbps(lwrLinkSpeed)
            .expected_min_link_speed_mbps(LinkSpeed)
            .expected_max_link_speed_mbps(LinkSpeed);
        entry.Finish();
        Tee::PrintMle(&bs);

        string lwrPexGen = GetLinkSpeedStr(lwrLinkSpeed);
        Printf(Tee::PriError, "Incorrect PEX link speed. Expected %s  Actual %s\n", pexGen.c_str(), lwrPexGen.c_str());
        return RC::PCIE_BUS_ERROR;
    }

    Printf(pri, "Testing PEX %s\n", pexGen.c_str());
    Printf(pri, "---------------------------------------------------\n");
    Printf(pri, "  Theoretical raw bandwidth  = %f Gbit/s\n",
           bandWidthGbitPerSec);
    Printf(pri, "  Min average raw bandwidth  = %f Gbit/s\n",
           bandWidthGbitPerSec / 100 * threshold);
    Mle::Print(Mle::Entry::pex_bw_info)
        .raw_bw_kbps(static_cast<UINT32>(bandWidthGbitPerSec * 1'000'000))
        .avg_bw_kbps(static_cast<UINT32>(bandWidthGbitPerSec * threshold * 10'000))
        .link_speed_mbps(static_cast<UINT32>(lwrLinkSpeed))
        .link_width(linkWidth);

    // Reset Initial src buffers to Gold A surface
    switch (m_TransDir)
    {
        case TD_FB_TO_SSYSMEM:
            m_DmaWrap.SetSurfaces(m_Surfs[SURF_GOLD_A].get(), m_Surfs[SURF_FB].get());
            CHECK_RC(m_DmaWrap.Copy(0, 0, m_SurfPitch, m_SurfHeightSize, 0, 0,
                m_TimeoutMs, GetBoundGpuSubdevice()->GetSubdeviceInst(), true));
            m_LwrSurfs[SURF_FB] = SURF_GOLD_A;
            break;
        case TD_SYSMEM_TO_FB:
            m_DmaWrap.SetSurfaces(m_Surfs[SURF_GOLD_A].get(), m_Surfs[SURF_SYS_A].get());
            CHECK_RC(m_DmaWrap.Copy(0, 0, m_SurfPitch, m_SurfHeightSize, 0, 0,
                m_TimeoutMs, GetBoundGpuSubdevice()->GetSubdeviceInst(), true));
            m_LwrSurfs[SURF_SYS_A] = SURF_GOLD_A;
            break;
        case TD_BOTH:
        {
            m_DmaWrap.SetSurfaces(m_Surfs[SURF_GOLD_A].get(), m_Surfs[SURF_FB].get());
            CHECK_RC(m_DmaWrap.Copy(0, 0, m_SurfPitch, m_SurfHeightSize, 0, 0,
                m_TimeoutMs, GetBoundGpuSubdevice()->GetSubdeviceInst(), false));
            m_LwrSurfs[SURF_FB] = SURF_GOLD_A;

            SURFACES type = (m_MatchTransData) ? SURF_GOLD_A : SURF_GOLD_B;
            m_DmaWrap.SetSurfaces(m_Surfs[type].get(), m_Surfs[SURF_SYS_B].get());
            CHECK_RC(m_DmaWrap.Copy(0, 0, m_SurfPitch, m_SurfHeightSize, 0, 0,
                m_TimeoutMs, GetBoundGpuSubdevice()->GetSubdeviceInst(), true));
            m_LwrSurfs[SURF_SYS_B] = type;
            break;
        }
        default:
            MASSERT(!"Unknown Transfer Direction");
            break;
    }

    UINT32 fbToSysTimeUs = 0;
    UINT32 sysToFbTimeUs = 0;

    if (m_PauseBeforeCopy)
        Pause(pexGen);

    m_DmaWrap.SaveCopyTime(true);
    m_DmaWrap2.SaveCopyTime(true);

    CHECK_RC(DoTransfers(&fbToSysTimeUs, &sysToFbTimeUs));

    m_DmaWrap.SaveCopyTime(false);
    m_DmaWrap2.SaveCopyTime(false);

    const UINT64 bytesPerLoop = static_cast<UINT64>(m_SurfPitch) * m_SurfHeightSize;
    Printf(pri, "  BytesPerLoop               = %lld bytes\n", bytesPerLoop);
    Printf(pri, "  FbToSysTimeUs              = %d Usec\n", fbToSysTimeUs);
    Printf(pri, "  SysToFbTimeUs              = %d Usec\n", sysToFbTimeUs);

    const UINT64 totalMBits = bytesPerLoop * 8 * m_pTestConfig->Loops() / 1024 / 1024;
    const FLOAT64 toSysGBitPerSec =
        (fbToSysTimeUs ? totalMBits * 1000000.0 / fbToSysTimeUs / 1024.0 : 0);
    const FLOAT64 toFbGBitPerSec  =
        (sysToFbTimeUs ? totalMBits * 1000000.0 / sysToFbTimeUs / 1024.0 : 0);

    FLOAT64 rawFbToSysBW, rawSysToFbBW;
    switch (LinkSpeed)
    {
        case Pci::Speed2500MBPS:
        case Pci::Speed5000MBPS:
            rawFbToSysBW = toSysGBitPerSec * 10/8;
            rawSysToFbBW = toFbGBitPerSec * 10/8;
            break;
        case Pci::Speed8000MBPS:
        case Pci::Speed16000MBPS:
        case Pci::Speed32000MBPS:
            rawFbToSysBW = toSysGBitPerSec * 130/128;
            rawSysToFbBW = toFbGBitPerSec * 130/128;
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }

    FLOAT64 bytesPerPacket = 128.0;

    // If the data being transfered is less than 64 bytes per line then PEX will
    // generate 64 byte packets, newer GPUs will generate 256 byte packets
    // if possible
    const UINT32 byteWidth = m_Surfs[0]->GetWidth() * m_Surfs[0]->GetBytesPerPixel();
    if (GetBoundTestDevice()->HasFeature(Device::GPUSUB_SUPPORTS_256B_PCIE_PACKETS) &&
        (byteWidth >= 256))
    {
        bytesPerPacket = 256.0;
    }
    else if (byteWidth <= 64)
    {
        bytesPerPacket = 64.0;
    }

    const FLOAT64 writeOverheadBytes = 24.0;
    const FLOAT64 readOverheadBytes  = 20.0;

    // including payload header (bytesPerPacket/(bytesPerPacket + overheadBytes)) and
    // 95% flow control overhead:
    rawFbToSysBW = rawFbToSysBW * (bytesPerPacket + writeOverheadBytes) / bytesPerPacket * 100.0 / 95.0;
    rawSysToFbBW = rawSysToFbBW * (bytesPerPacket + readOverheadBytes) / bytesPerPacket * 100.0 / 95.0;

    if (m_TransDir & TD_FB_TO_SSYSMEM)
    {
        Printf(pri, "  Fb to Sys                  = %f Gbit/s. Raw: %f Gbit/s\n",
            toSysGBitPerSec, rawFbToSysBW);
    }
    if (m_TransDir & TD_SYSMEM_TO_FB)
    {
        Printf(pri, "  Sys to Fb                  = %f Gbit/s. Raw: %f Gbit/s\n",
           toFbGBitPerSec, rawSysToFbBW);
    }

    FLOAT64 bandWidth = rawSysToFbBW + rawFbToSysBW;
    // If we only performed transfers in one direction, then we already have the
    // average since one of the time values will be zero. Otherwise, divide the
    // bandwidth by two to get the average
    if (m_TransDir == TD_BOTH)
    {
        bandWidth *= 0.5f;
    }
    Printf(pri, "  Average raw bandwidth      = %f Gbit/s\n\n", bandWidth);

    string perfMetricStr = "PCIE" + Utility::RemoveSpaces(pexGen) + "Bandwidth";
    SetPerformanceMetric(perfMetricStr.c_str(), bandWidth);

    if (!m_IgnoreBwCheck)
    {
        bool bwOutOfRange = false;
        FLOAT64 bwPercent = bandWidth/bandWidthGbitPerSec*100;
        MASSERT(bwPercent > 0.0 && bwPercent < 100.0);
        if (bwPercent < threshold)
        {
            Printf(Tee::PriError, "Bandwidth too low. Expected %d %%  Actual %.1f %%\n", threshold, bwPercent);
            bwOutOfRange = true;
        }
        else if (bwPercent > 100.0)
        {
            Printf(Tee::PriError, "Bandwidth too high. Expected %d %%  Actual %.1f %%\n", threshold, bwPercent);
            bwOutOfRange = true;
        }
        else
        {
            Printf(pri, "Bandwidth met expectations. Expected %d %%  Actual %.1f %%\n",
                   threshold, bwPercent);
        }

        if (bwOutOfRange)
        {
            if (JsonLogStream::IsOpen())
            {
                JsonItem jsi;
                jsi.SetCategory(JsonItem::JSONLOG_PEXINFO);
                jsi.SetJsonLimitedAllowAllFields(true);
                jsi.SetTag("pex_bandwidth_out_of_range");
                jsi.SetField("expected_bandwidth_pct", threshold);
                jsi.SetField("actual_bandwidth_pct", bwPercent);
                jsi.SetField("theoretical_raw_bandwidth_Gbps", bandWidthGbitPerSec);
                jsi.SetField("pex_generation", pexGen.c_str());
                jsi.WriteToLogfile();
            }
            return RC::PCIE_BUS_ERROR;
        }
    }

    CHECK_RC(CheckSurfaces());
    return rc;
}

//-----------------------------------------------------------------------------
// Reset the source surface to a gold surface (or sysmem B during
// two-directional test)
RC PexBandwidth::ResetSrcSurface(SURFACES src, SURFACES dst)
{
    MASSERT(src == SURF_GOLD_A || src == SURF_GOLD_B || src == SURF_SYS_A);
    MASSERT(dst == SURF_FB || dst == SURF_SYS_A || dst == SURF_SYS_B);

    RC rc;

    m_DmaWrap.SetSurfaces(m_Surfs[src].get(), m_Surfs[dst].get());
    CHECK_RC(m_DmaWrap.Copy(0, 0, m_SurfPitch, m_SurfHeightSize, 0, 0,
        m_TimeoutMs, GetBoundGpuSubdevice()->GetSubdeviceInst(), true)); // wait till done
    CHECK_RC(GetPcieErrors(GetBoundGpuSubdevice()));

    m_LwrSurfs[dst] = m_LwrSurfs[src];

    return rc;
}

//-----------------------------------------------------------------------------
// Basically swapping content of FB and Sysmem
// We use two DMA engines here so we can start transfer simultaneously
// Transfers can be:
//  FB <-> Sysmem A/B
//  Sysmem A <-> Sysmem B
RC PexBandwidth::DoTransfer(SURFACES src, SURFACES dst, UINT32 *time)
{
    MASSERT(src == SURF_FB || src == SURF_SYS_A || src == SURF_SYS_B);
    MASSERT(dst == SURF_FB || dst == SURF_SYS_A || dst == SURF_SYS_B);
    MASSERT(time);
    RC rc;

    m_DmaWrap.SetSurfaces(m_Surfs[src].get(), m_Surfs[dst].get());
    CHECK_RC(m_DmaWrap.Copy(0,
                            0,
                            m_SurfPitch,
                            m_SurfHeightSize / (m_DmaWrap2.IsInitialized() ? 2 : 1),
                            0,
                            0,
                            m_TimeoutMs,
                            GetBoundGpuSubdevice()->GetSubdeviceInst(),
                            false));

    if (m_DmaWrap2.IsInitialized())
    {
        m_DmaWrap2.SetSurfaces(m_Surfs[src].get(), m_Surfs[dst].get());
        CHECK_RC(m_DmaWrap2.Copy(0, m_SurfHeightSize/2, m_SurfPitch,
            m_SurfHeightSize/2, 0, m_SurfHeightSize/2, m_TimeoutMs,
            GetBoundGpuSubdevice()->GetSubdeviceInst(), false));
    }

    CHECK_RC(m_DmaWrap.Wait(m_TimeoutMs));
    if (m_DmaWrap2.IsInitialized())
    {
        CHECK_RC(m_DmaWrap2.Wait(m_TimeoutMs));
    }

    CHECK_RC(RetrieveCopyStats(time));
    CHECK_RC(GetPcieErrors(GetBoundGpuSubdevice()));

    m_LwrSurfs[dst] = m_LwrSurfs[src];

    return rc;
}

//-----------------------------------------------------------------------------
// Perform transfers then update src surface to alternating gold surfaces
RC PexBandwidth::DoTransfers(UINT32 *pFbToSysTimeUs, UINT32 *pSysToFbTimeUs)
{
    StickyRC rc;

    const bool bLogUphy = m_EnableUphyLogging ||
           (UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::BandwidthTest);
    for (UINT32 i = 0; i < m_pTestConfig->Loops(); i++)
    {
        rc = DoOneTransferLoop(pFbToSysTimeUs, pSysToFbTimeUs);
        if (bLogUphy)
        {
            rc = UphyRegLogger::LogRegs(GetBoundTestDevice(),
                                        UphyRegLogger::UphyInterface::Pcie,
                                        UphyRegLogger::LogPoint::BandwidthTest,
                                        rc);
        }
        CHECK_RC(rc);
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC PexBandwidth::DoOneTransferLoop(UINT32 *pFbToSysTimeUs, UINT32 *pSysToFbTimeUs)
{
    RC rc;

    switch (m_TransDir)
    {
        case TD_FB_TO_SSYSMEM:
            CHECK_RC(DoTransfer(SURF_FB, SURF_SYS_A, pFbToSysTimeUs));
            if (!m_DisableSurfSwap)
            {
                CHECK_RC(ResetSrcSurface(m_LwrSurfs[SURF_FB] == SURF_GOLD_A ?
                                             SURF_GOLD_B :
                                             SURF_GOLD_A,
                                         SURF_FB));
            }
            break;
        case TD_SYSMEM_TO_FB:
            CHECK_RC(DoTransfer(SURF_SYS_A, SURF_FB, pSysToFbTimeUs));
            if (!m_DisableSurfSwap)
            {
                CHECK_RC(ResetSrcSurface(m_LwrSurfs[SURF_SYS_A] == SURF_GOLD_A ?
                                             SURF_GOLD_B :
                                             SURF_GOLD_A,
                                         SURF_SYS_A));
            }
            break;
        case TD_BOTH:
            CHECK_RC(DoTransfer(SURF_FB, SURF_SYS_A, pFbToSysTimeUs));
            CHECK_RC(DoTransfer(SURF_SYS_B, SURF_FB, pSysToFbTimeUs));
            if (!m_DisableSurfSwap)
            {
                // When doing transfers in both directions, we reset Sysmem B so
                // that FB will be update to alternate surface in subsequent loop
                CHECK_RC(ResetSrcSurface(SURF_SYS_A, SURF_SYS_B));
            }
            break;
        default:
            MASSERT(!"Unknown Transfer Direction");
            break;
    }

    return rc;
}
//-----------------------------------------------------------------------------
// The surfaces are quite large (50Mb), so filling the memory using CPU is
// going to be slow. Attempt to do this faster using copy engine
// First create 1/4 the screen height of the surface with a pattern, then copy
// and paste the pattern to the rest of the surface.
RC PexBandwidth::FillGoldSurfaces()
{
    RC rc;

    vector<Surface2DPtr> tempSurfs(2);
    for (UINT32 i = 0; i < tempSurfs.size(); i++)
    {
        Surface2DPtr pTempSurf(new Surface2D);
        pTempSurf->SetWidth(m_Surfs[SURF_GOLD_A]->GetWidth());
        pTempSurf->SetHeight(m_Surfs[SURF_GOLD_A]->GetHeight());
        pTempSurf->SetLocation(m_Surfs[SURF_GOLD_A]->GetLocation());
        pTempSurf->SetColorFormat(m_Surfs[SURF_GOLD_A]->GetColorFormat());
        pTempSurf->SetProtect(Memory::ReadWrite);
        pTempSurf->SetLayout(Surface2D::Pitch);
        CHECK_RC(pTempSurf->Alloc(m_Surfs[SURF_GOLD_A]->GetGpuDev()));

        tempSurfs[i] = std::move(pTempSurf);
    }

    for (UINT32 i = 0; i < tempSurfs.size(); i++)
    {
        CHECK_RC(tempSurfs[i]->Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
    }

    const UINT32 NUM_PAT_PER_SCREEN = 4;
    UINT32 PatternHeight = m_SurfHeight/NUM_PAT_PER_SCREEN;
    ColorUtils::Format ColorFormat = ColorUtils::ColorDepthToFormat(m_SurfDepth);

    {
        Tasker::DetachThread detachThread;
        switch (m_SrcSurfMode)
        {
            case 0:
                CHECK_RC(FillSurface(tempSurfs[0].get(), PT_RANDOM, 0, 0, PatternHeight));
                CHECK_RC(FillSurface(tempSurfs[1].get(), PT_BARS, 0, 0, PatternHeight));
                break;
            case 1:
                CHECK_RC(FillSurface(tempSurfs[0].get(), PT_RANDOM, 0, 0, PatternHeight));
                CHECK_RC(FillSurface(tempSurfs[1].get(), PT_RANDOM, 0, 0, PatternHeight));
                break;
            case 2:
                PatternHeight = 1;
                CHECK_RC(FillSurface(tempSurfs[0].get(), PT_SOLID, 0, 0, PatternHeight));
                CHECK_RC(FillSurface(tempSurfs[1].get(), PT_SOLID, 0, 0, PatternHeight));
                break;
            case 3:
                PatternHeight = 1;
                CHECK_RC(FillSurface(tempSurfs[0].get(), PT_ALTERNATE, 0x00000000, 0xFFFFFFFF, PatternHeight));
                CHECK_RC(FillSurface(tempSurfs[1].get(), PT_ALTERNATE, 0x00000000, 0xFFFFFFFF, PatternHeight));
                break;
            case 4:
                PatternHeight = 1;
                CHECK_RC(FillSurface(tempSurfs[0].get(), PT_ALTERNATE, 0xAAAAAAAA, 0x55555555, PatternHeight));
                CHECK_RC(FillSurface(tempSurfs[1].get(), PT_ALTERNATE, 0xAAAAAAAA, 0x55555555, PatternHeight));
                break;
            case 5:
                CHECK_RC(FillSurface(tempSurfs[0].get(), PT_RANDOM, 0, 0, PatternHeight));
                CHECK_RC(FillSurface(tempSurfs[1].get(), PT_SOLID, 0, 0, PatternHeight));
                break;
            case 6:
                CHECK_RC(Memory::FillRgbBars(tempSurfs[0]->GetAddress(),
                                             m_SurfWidth,
                                             PatternHeight,
                                             ColorFormat,
                                             m_SurfPitch));
                CHECK_RC(Memory::FillAddress(tempSurfs[1]->GetAddress(),
                                             m_SurfWidth,
                                             PatternHeight,
                                             m_SurfDepth,
                                             m_SurfPitch));
                break;
            default:
                Printf(Tee::PriError, "Invalid SrcSurfMode = %u\n", m_SrcSurfMode);
                return RC::ILWALID_ARGUMENT;
        }
    }

    for (UINT32 y = 0;
         y < (m_SurfHeightSize - PatternHeight);
         y += PatternHeight)
    {
        for (UINT32 i = 0; i < tempSurfs.size(); i++)
        {
            m_DmaWrap.SetSurfaces(tempSurfs[i].get(), tempSurfs[i].get());
            CHECK_RC(m_DmaWrap.Copy(0,                     // Starting src X, in bytes not pixels.
                                    y,                     // Starting src Y, in lines.
                                    m_SurfPitch,           // Width of copied rect, in bytes.
                                    PatternHeight,         // Height of copied rect, in bytes
                                    0,                     // Starting dst X, in bytes not pixels.
                                    y+PatternHeight,       // Starting dst Y, in lines.
                                    m_TimeoutMs,
                                    GetBoundGpuSubdevice()->GetSubdeviceInst()));
        }
    }

    // Fill GOLD_A with percent worth of the first type, the remainder of the surface is the second type
    // Fill GOLD_B with percent worth of the second type, the remainder of the surface is the first type
    UINT32 percentHeight = (UINT32)(tempSurfs[0]->GetHeight() * (m_SrcSurfModePercent / 100.0));
    UINT32 remainingHeight = tempSurfs[0]->GetHeight() - percentHeight;

    if (percentHeight > 0)
    {
        m_DmaWrap.SetSurfaces(tempSurfs[0].get(), m_Surfs[SURF_GOLD_A].get());
        CHECK_RC(m_DmaWrap.Copy(0,                     // Starting src X, in bytes not pixels.
                                0,                     // Starting src Y, in lines.
                                m_SurfPitch,           // Width of copied rect, in bytes.
                                percentHeight,         // Height of copied rect, in bytes
                                0,                     // Starting dst X, in bytes not pixels.
                                0,                     // Starting dst Y, in lines.
                                m_TimeoutMs,
                                GetBoundGpuSubdevice()->GetSubdeviceInst()));

        m_DmaWrap.SetSurfaces(tempSurfs[1].get(), m_Surfs[SURF_GOLD_B].get());
        CHECK_RC(m_DmaWrap.Copy(0,                     // Starting src X, in bytes not pixels.
                                0,                     // Starting src Y, in lines.
                                m_SurfPitch,           // Width of copied rect, in bytes.
                                percentHeight,         // Height of copied rect, in bytes
                                0,                     // Starting dst X, in bytes not pixels.
                                0,                     // Starting dst Y, in lines.
                                m_TimeoutMs,
                                GetBoundGpuSubdevice()->GetSubdeviceInst()));
    }

    if (remainingHeight > 0)
    {
        m_DmaWrap.SetSurfaces(tempSurfs[1].get(), m_Surfs[SURF_GOLD_A].get());
        CHECK_RC(m_DmaWrap.Copy(0,                     // Starting src X, in bytes not pixels.
                                percentHeight,         // Starting src Y, in lines.
                                m_SurfPitch,           // Width of copied rect, in bytes.
                                remainingHeight,       // Height of copied rect, in bytes
                                0,                     // Starting dst X, in bytes not pixels.
                                0,                     // Starting dst Y, in lines.
                                m_TimeoutMs,
                                GetBoundGpuSubdevice()->GetSubdeviceInst()));

        m_DmaWrap.SetSurfaces(tempSurfs[0].get(), m_Surfs[SURF_GOLD_B].get());
        CHECK_RC(m_DmaWrap.Copy(0,                     // Starting src X, in bytes not pixels.
                                percentHeight,         // Starting src Y, in lines.
                                m_SurfPitch,           // Width of copied rect, in bytes.
                                remainingHeight,       // Height of copied rect, in bytes
                                0,                     // Starting dst X, in bytes not pixels.
                                0,                     // Starting dst Y, in lines.
                                m_TimeoutMs,
                                GetBoundGpuSubdevice()->GetSubdeviceInst()));
    }

    for (UINT32 i = 0; i < tempSurfs.size(); i++)
    {
        tempSurfs[i]->Unmap();
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC PexBandwidth::FillSurface
(
    Surface2D* pSurf
    , PatternType pt
    , UINT32 patternData
    , UINT32 patternData2
    , UINT32 patternHeight
)
{
    RC rc;

    if (patternHeight > pSurf->GetHeight())
        return RC::ILWALID_ARGUMENT;

    const bool isTegra = pSurf->GetGpuSubdev(0)->IsSOC();

    // Avoid reflected writes by creating a pitch linear version of the surface
    // where the pattern is written with CPU writes
    if (((pSurf->GetLayout() == Surface2D::BlockLinear) && (pSurf->GetLocation() != Memory::Fb))
        // Also avoid mmaping a huge surface on CheetAh
        || isTegra)
    {
        Surface2DPtr pTempSurf = make_unique<Surface2D>();
        pTempSurf->SetWidth(pSurf->GetWidth());
        pTempSurf->SetHeight(patternHeight);
        pTempSurf->SetLocation(pSurf->GetLocation());
        pTempSurf->SetColorFormat(pSurf->GetColorFormat());
        pTempSurf->SetProtect(Memory::ReadWrite);
        pTempSurf->SetLayout(Surface2D::Pitch);
        if (isTegra)
        {
            pTempSurf->SetVASpace(Surface2D::TegraVASpace);
        }
        CHECK_RC(pTempSurf->Alloc(pSurf->GetGpuDev()));

        Surface2D* pPatSurf = pTempSurf.get();
        CHECK_RC(FillSurfaceHelper(pSurf, pPatSurf, pt, patternData, patternData2, patternHeight));
    }
    else
    {
        Surface2D* pPatSurf = pSurf;
        CHECK_RC(FillSurfaceHelper(pSurf, pPatSurf, pt, patternData, patternData2, patternHeight));
    }
    return rc;
}
RC PexBandwidth::FillSurfaceHelper
(
    Surface2D* pSurf
    , Surface2D* pPatSurf
    , PatternType pt
    , UINT32 patternData
    , UINT32 patternData2
    , UINT32 patternHeight
){
    RC rc;
    switch (pt)
    {
        case PT_BARS:
            CHECK_RC(pPatSurf->FillPatternRect(0,
                                               0,
                                               pSurf->GetWidth(),
                                               patternHeight,
                                               1,
                                               1,
                                               "bars",
                                               "100",
                                               "horizontal"));
            break;
        case PT_LINES:
            for (UINT32 lwrY = 0; lwrY < patternHeight; lwrY+=8)
            {
                CHECK_RC(pPatSurf->FillPatternRect(0,
                                                   lwrY,
                                                   pSurf->GetWidth(),
                                                   8,
                                                   1,
                                                   1,
                                                   "bars",
                                                   "100",
                                                   "vertical"));
            }
            break;
        case PT_RANDOM:
            {
                string seed = Utility::StrPrintf("seed=%u", patternData);
                CHECK_RC(pPatSurf->FillPatternRect(0,
                                                   0,
                                                   pSurf->GetWidth(),
                                                   patternHeight,
                                                   1,
                                                   1,
                                                   "random",
                                                   seed.c_str(),
                                                   nullptr));
            }
            break;
        case PT_SOLID:
            CHECK_RC(pPatSurf->FillRect(0,
                                        0,
                                        pSurf->GetWidth(),
                                        patternHeight,
                                        patternData));
            break;
        case PT_ALTERNATE:
        {
            bool patternSelect = false;
            for (UINT32 lwrY = 0; lwrY < patternHeight; lwrY++)
            {
                for (UINT32 lwrX = 0; lwrX < pSurf->GetWidth(); lwrX += 4)
                {
                    patternSelect = !patternSelect;
                    CHECK_RC(pPatSurf->FillRect(lwrX,
                                                lwrY,
                                                4,
                                                1,
                                                (patternSelect) ? patternData : patternData2));
                }
            }
            break;
        }
        default:
            Printf(Tee::PriError, "%s : Unknown pattern type %d\n",
                   __FUNCTION__, pt);
            return RC::SOFTWARE_ERROR;
    }

    // Temporarily rebind the GPU so that DmaWrapper uses the correct device
    GpuDevice *pOrigGpuDev = m_pTestConfig->GetGpuDevice();
    Utility::CleanupOnReturnArg<GpuTestConfiguration,
                                void,
                                GpuDevice *>
        restoreGpuDev(m_pTestConfig,
                      &GpuTestConfiguration::BindGpuDevice,
                      pOrigGpuDev);
    m_pTestConfig->BindGpuDevice(pSurf->GetGpuDev());

    // Copy the pattern to the surface to be filled if the pattern surface is
    // different from the surface to be filled (used to avoid reflected writes)
    if (pPatSurf != pSurf)
    {
        m_DmaWrap.SetSurfaces(pPatSurf, pSurf);
        CHECK_RC(m_DmaWrap.Copy(0,                        // Starting src X, in bytes not pixels.
                                0,                        // Starting src Y, in lines.
                                pPatSurf->GetPitch(),     // Width of copied rect, in bytes.
                                patternHeight,            // Height of copied rect, in bytes
                                0,                        // Starting dst X, in bytes not pixels.
                                0,                        // Starting dst Y, in lines.
                                m_TimeoutMs,
                                0));
    }
    return OK;
}

//-----------------------------------------------------------------------------
// Error check:
// Check will be done with sysmem, therefore the FB surface will be copied once
// more into sysmem prior to checking for SymemToFb test of the src surface was
// FB
RC PexBandwidth::CheckSurfaces()
{
    RC rc;

    // If the test was checking Sys to Fb transfers, then we will do the reverse
    // here since our CheckSurface algorithm will be checking SysA. Doing this
    // reversal is especially important if this test was run with loop set to 1,
    // since we want to check that whatever was copied into FB during that first
    // loop is valid.
    if (m_TransDir & TD_SYSMEM_TO_FB)
    {
        m_DmaWrap.SetSurfaces(m_Surfs[SURF_FB].get(), m_Surfs[SURF_SYS_A].get());
        CHECK_RC(m_DmaWrap.Copy(0, 0,
                                m_SurfPitch,
                                m_SurfHeightSize,
                                0, 0, m_TimeoutMs,
                                GetBoundGpuSubdevice()->GetSubdeviceInst()));
        m_LwrSurfs[SURF_SYS_A] = m_LwrSurfs[SURF_FB];
    }

    CHECK_RC(CheckSurface(SURF_SYS_A, m_LwrSurfs[SURF_SYS_A]));
    if (m_TransDir == TD_BOTH)
    {
        CHECK_RC(CheckSurface(SURF_SYS_B, m_LwrSurfs[SURF_SYS_B]));
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Compare Sysmem surface with a golf surface by examining every byte
RC PexBandwidth::CheckSurface(SURFACES surf, SURFACES goldSurf)
{
    StickyRC rc;

    MASSERT(surf == SURF_SYS_A || surf == SURF_SYS_B);
    MASSERT(goldSurf == SURF_GOLD_A || goldSurf == SURF_GOLD_B);

    CHECK_RC(m_Surfs[surf]->Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
    CHECK_RC(m_Surfs[goldSurf]->Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));

    {
        Tasker::DetachThread detachThread;

        UINT32 ErrorCount = 0;
        for (UINT32 LwrY = 0; LwrY < m_SurfHeightSize; LwrY++)
        {
            vector<UINT32> SurfLine(m_SurfPitch);
            vector<UINT32> GoldLine(m_SurfPitch);

            Platform::VirtualRd((UINT08*)m_Surfs[surf]->GetAddress() +
                                         m_Surfs[surf]->GetPixelOffset(0, LwrY),
                                &SurfLine[0],
                                m_SurfWidth*sizeof(UINT32));
            Platform::VirtualRd((UINT08*)m_Surfs[goldSurf]->GetAddress() +
                                         m_Surfs[goldSurf]->GetPixelOffset(0, LwrY),
                                &GoldLine[0],
                                m_SurfWidth*sizeof(UINT32));

            for (UINT32 LwrX = 0; LwrX < m_SurfWidth; LwrX++)
            {
                if (SurfLine[LwrX] != GoldLine[LwrX])
                {
                    Printf(Tee::PriNormal,
                            "Error at SysSurf (%d, %d). Got 0x%x, expected 0x%x\n",
                            LwrX, LwrY, SurfLine[LwrX],
                            GoldLine[LwrX]);
                    rc = RC::MEM_TO_MEM_RESULT_NOT_MATCH;
                    ErrorCount++;
                }

                if (ErrorCount == m_NumErrorsToPrint)
                    break;
            }
        }
    }

    m_Surfs[surf]->Unmap();
    m_Surfs[goldSurf]->Unmap();

    return rc;
}

//------------------------------------------------------------------------------
RC PexBandwidth::FindCopyEngineInstance(vector<UINT32> * pEngineList, UINT32 * pEngineInst)
{
    MASSERT(pEngineList->size());
    if (pEngineList->empty())
    {
        Printf(Tee::PriError, "%s : Copy engine list is empty", GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    *pEngineInst = ~0U;

    RC rc;
    DmaCopyAlloc dmaCopyEng;

    UINT32 foundEngine = LW2080_ENGINE_TYPE_ALLENGINES;
    auto pLwrEng = pEngineList->begin();
    for (; pLwrEng != pEngineList->end(); ++pLwrEng)
    {
        bool bIsEngineGrce = false;
        CHECK_RC(dmaCopyEng.IsGrceEngine(*pLwrEng,
                                         GetBoundGpuSubdevice(),
                                         GetBoundRmClient(),
                                         &bIsEngineGrce));
        if (!bIsEngineGrce)
        {
            foundEngine = *pLwrEng;
            break;
        }
    }

    if (foundEngine == LW2080_ENGINE_TYPE_ALLENGINES)
    {
        foundEngine = pEngineList->at(0);
        pLwrEng = pEngineList->begin();
        Printf(Tee::PriWarn, "%s : GRCE copy engine being used, bandwidth check may fail\n",
               GetName().c_str());
    }

    *pEngineInst = foundEngine - LW2080_ENGINE_TYPE_COPY0;
    pEngineList->erase(pLwrEng);

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Pause test exelwtion
//!
//! linkSpeed : Link speed being paused before
void PexBandwidth::Pause(string linkSpeedStr)
{
    string transDirStr = "Unknown";
    switch (m_TransDir)
    {
        case TD_FB_TO_SSYSMEM:
            transDirStr = "FB to Sysmem";
            break;
        case TD_SYSMEM_TO_FB:
            transDirStr = "Sysmem to FB";
            break;
        case TD_BOTH:
            transDirStr = "Bidirectional";
            break;
        default:
            break;
    }
    Printf(Tee::PriNormal, "********************************\n"
                           "* Test paused before testing %s\n"
                           "* Direction : %s\n"
                           "* Press any key to continue\n"
                           "********************************\n",
           linkSpeedStr.c_str(), transDirStr.c_str());
    Tee::TeeFlushQueuedPrints(m_pTestConfig->TimeoutMs());

    ConsoleManager::ConsoleContext consoleCtx;
    Console *pConsole = consoleCtx.AcquireRealConsole(true);

    while (!pConsole->KeyboardHit())
        Tasker::Sleep(100);
    // Retrieve the key to ensure it isnt hanging around later
    (void)pConsole->GetKey();
}

RC PexBandwidth::RetrieveCopyStats(UINT32 *pTimeUs)
{
    RC rc;
    UINT64 startTime1, endTime1;

    CHECK_RC(m_DmaWrap.GetCopyTimeUs(&startTime1, &endTime1));

    if (m_DmaWrap2.IsInitialized())
    {
        UINT64 startTime2, endTime2;

        CHECK_RC(m_DmaWrap2.GetCopyTimeUs(&startTime2, &endTime2));

        //no overlapping of time segments
        if ((startTime1 > endTime2) ||
            (startTime2 > endTime1))
        {
            *pTimeUs += static_cast<UINT32>(endTime1 - startTime1);
            *pTimeUs += static_cast<UINT32>(endTime2 - startTime2);
        }
        else    //time segments overlap
        {
            UINT64 startTime, endTime;
            startTime = min(startTime1, startTime2);
            endTime = max(endTime1, endTime2);
            *pTimeUs += static_cast<UINT32>(endTime - startTime);
        }
    }
    else
    {
        *pTimeUs += static_cast<UINT32>(endTime1 - startTime1);
    }

    return rc;
}
