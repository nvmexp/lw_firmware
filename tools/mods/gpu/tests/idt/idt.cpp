/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "idt.h"
#include "idt_private_impl.h"
#include "gpu/display/evo_cctx.h"
#include "gpu/display/lwdisplay/lw_disp_utils.h"

//------------------------------------------------------------------------------
// IDT : The exposed test class which also acts as a builder for IDTPrivateImpl
//------------------------------------------------------------------------------
IDT::IDT()
    : m_pPrivateImpl(new IDTPrivateImpl(this, GetTestConfiguration()))
    , m_pTestArguments(new TestArguments())
{

}

IDT::~IDT()
{

}

bool IDT::IsSupported()
{
    // Till others are fully supported
    return GetDisplay()->GetDisplayClassFamily() == Display::LWDISPLAY ||
        GetDisplay()->GetDisplayClassFamily() == Display::EVO;
}

RC IDT::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::InitFromJs());

    CHECK_RC(GpuTest::Setup());
    CHECK_RC(GpuTest::AllocDisplay());

    if (m_EnableBkgndMode)
    {
        if (!m_PromptTimeout)
        {
            Printf(Tee::PriError, "IDT:: Please specify PromptTimeout "
                "along with EnableBkgndMode\n");
            return RC::ILWALID_ARGUMENT;
        }
        CHECK_RC(m_pPrivateImpl->ResetPromptWaitForTimeout(m_PromptTimeout, false));
    }

    std::unique_ptr<SurfaceFill> pSurfaceHandler;
    if (m_EnableDMA && !GetBoundGpuSubdevice()->IsDFPGA())
    {
        pSurfaceHandler = make_unique<SurfaceFillDMA>(false);
        Printf(GetVerbosePrintPri(), "IDT will use DMA for displaying the image\n");
    }
    else
    {
        pSurfaceHandler =  make_unique<SurfaceFillFB>(false);
        Printf(GetVerbosePrintPri(), "IDT will not use DMA for displaying the image\n");
    }

    Display *pDisplay = GetBoundGpuDevice()->GetDisplay();
    std::unique_ptr<IDTDisplayHAL> pDisplayHAL;

    if (pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY)
    {
        pDisplayHAL.reset(new IDTLwDisplayHAL(GetVerbosePrintPri(), pDisplay));
    }
    else
    {
        pDisplayHAL.reset(new IDTEvoDisplayHAL(pDisplay));
    }

    CHECK_RC(FillTestArguments());

    return m_pPrivateImpl->Setup(GetBoundGpuDevice()->GetDisplay(),
            std::move(pDisplayHAL), std::move(pSurfaceHandler),
            GetTestArguments());
}

RC IDT::Run()
{
    return m_pPrivateImpl->Run();

}

RC IDT::Cleanup()
{
    StickyRC rc;
    rc = m_pPrivateImpl->Cleanup();

    rc = GpuTest::Cleanup();

    return rc;
}

RC IDT::SetEnableDVSRun(bool val)
{
    RC rc;
    m_EnableDVSRun = val;
    if (m_EnableDVSRun)
    {
        const UINT32 WAIT_TIME_SEC = 2; //Empirically found
        CHECK_RC(m_pPrivateImpl->ResetPromptWaitForTimeout(WAIT_TIME_SEC, true));
    }
    return rc;
}

UINT32 IDT::GetORPixelDepth() const
{
    return static_cast <UINT32> (m_ORPixelDepth);
}

RC IDT::SetORPixelDepth(UINT32 depth)
{
    if (depth > LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_48_444)
    {
        Printf(Tee::PriError,
                "OrPixel Depth will not be overridden because %u is out of range.\n",
                depth);
        return RC::ILWALID_ARGUMENT;
    }
    m_ORPixelDepth = static_cast<ORPixelDepthBpp>(depth);
    return RC::OK;
}

UINT32 IDT::GetColorSpace() const
{
    return static_cast <UINT32>(m_ColorSpace);
}

RC IDT::SetColorSpace(UINT32 colorId)
{
    if (colorId >= static_cast<UINT32> (ORColorSpace::MAX_VAL))
    {
        Printf(Tee::PriError,
                "Colorspace and ProcampSettings will not be overridden because id %u"
                " is out of range.\n", colorId);
        return RC::ILWALID_ARGUMENT;
    }
    m_ColorSpace = static_cast<ORColorSpace>(colorId);
    return RC::OK;
}

RC IDT::FillTestArguments()
{
    if (m_pTestArguments->isInitialized)
        return RC::OK;

    m_pTestArguments->displays = m_Displays;
    m_pTestArguments->resolution = m_Resolution;
    m_pTestArguments->displaysEx = m_DisplaysEx;
    m_pTestArguments->image = m_Image;
    m_pTestArguments->rtImage = m_RtImage;
    m_pTestArguments->randomPrompts = m_RandomPrompts;
    m_pTestArguments->enableDMA = m_EnableDMA;
    m_pTestArguments->skipVisualText = m_SkipVisualText;
    m_pTestArguments->enableDVSRun = m_EnableDVSRun;
    m_pTestArguments->enableBkgndMode = m_EnableBkgndMode;
    m_pTestArguments->promptTimeoutMs = m_PromptTimeout * 1000;
    m_pTestArguments->assumeSuccessOnPromptTimeout = m_AssumeSuccessOnPromptTimeout;
    m_pTestArguments->testUnsupportedDisplays = m_TestUnsupportedDisplays;
    m_pTestArguments->skipSingleDisplays = m_SkipSingleDisplays;
    m_pTestArguments->oRPixelDepth = m_ORPixelDepth;
    m_pTestArguments->colorSpace = m_ColorSpace;

    JavaScriptPtr pJs;

    bool skipHDCP = false;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_SkipHdcp",
                &skipHDCP);
    m_pTestArguments->skipHDCP = skipHDCP | m_SkipHDCP;

    if (pJs->GetProperty(pJs->GetGlobalObject(), "g_HDCPDelay",
            &m_pTestArguments->hdcpDelay) != OK)
    {
        m_pTestArguments->hdcpDelay = 500;
    }

    if (pJs->GetProperty(pJs->GetGlobalObject(), "g_HDCPTimeout",
            &m_pTestArguments->hdcpTimeout) != OK)
    {
        m_pTestArguments->hdcpTimeout = 1000;
    }

    return RC::OK;
}

RC IDT::SetTestArguments(const TestArguments& testArguments)
{
    *m_pTestArguments = testArguments;
    m_pTestArguments->isInitialized = true;

    return RC::OK;
}

const TestArguments& IDT::GetTestArguments()
{
    return *m_pTestArguments;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ IDT object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(IDT, GpuTest, "Interactive Display Test");

CLASS_PROP_READWRITE(IDT, Displays, DisplaysArg, "Selects the displays");
CLASS_PROP_READWRITE(IDT, Resolution, ResolutionArg,
        "Selects a common resolution of selected displays");
CLASS_PROP_READWRITE(IDT, DisplaysEx, DisplaysExArg,
        "Selects displays and also resolution individually for each display");
CLASS_PROP_READWRITE(IDT, Image, std::string,
        "Selects the image to use when running IDT");
CLASS_PROP_READWRITE(IDT, RtImage, std::string,
        "Selects the right image to use when running IDT in stereo mode");
CLASS_PROP_READWRITE(IDT, SkipHDCP, bool, "Should IDT skip the HDCP tests?");
CLASS_PROP_READWRITE(IDT, RandomPrompts, bool, "Should the prompts be randomized?");
CLASS_PROP_READWRITE(IDT, EnableDMA, bool, "Should DMA be used for displaying the image?");
CLASS_PROP_READWRITE(IDT, SkipVisualText, bool,
        "Should on-screen resolution and prompt info be removed?");
CLASS_PROP_READWRITE(IDT, EnableDVSRun, bool, "Enable DVS (positive path) run for the test");
CLASS_PROP_READWRITE(IDT, PromptTimeout, UINT32,
        "Sets the timeout for the prompt in sec(0 means infinite)");
CLASS_PROP_READWRITE(IDT, EnableBkgndMode, bool,
        "Enable Bkgnd mode (positive path) run for the test");
CLASS_PROP_READWRITE(IDT, AssumeSuccessOnPromptTimeout, bool,
        "Marks test as successful in the event of timeout");
CLASS_PROP_READWRITE(IDT, TestUnsupportedDisplays, bool,
        "Force to run IDT for possibly unsupported displays");
CLASS_PROP_READWRITE(IDT, SkipSingleDisplays, bool,
        "Skips testing of displays individually");
CLASS_PROP_READWRITE(IDT, ORPixelDepth, UINT32, "Preferred OR pixel depth for the test.");
CLASS_PROP_READWRITE(IDT, ColorSpace, UINT32, "Colorspace procamp settings for the test.");

//------------------------------------------------------------------------------
// IDTPrivateImpl : The core logic of the test independent of platform
//------------------------------------------------------------------------------
IDTPrivateImpl::IDTPrivateImpl(IDT *pIDT, GpuTestConfiguration *pTestConfig)
    : m_pSelf(pIDT)
    , m_pTestConfig(pTestConfig)
    , m_pDisplay(nullptr)
    , m_pIdtJsonParser(new IDTJsonParser())
    , m_pDisplayHAL(nullptr)
    , m_pVisualComponent(new VisualComponent())
    , m_pSurfaceHandler(nullptr)
    , m_pPromptHandler(new Prompt())
    , m_TestAllDisplays(false)
{

}

IDTPrivateImpl::IDTPrivateImpl
(
    IDT *pIDT,
    GpuTestConfiguration *pTestConfig,
    Display *pDisplay,
    std::unique_ptr<IDTJsonParser> pIdtJsonParser,
    std::unique_ptr<VisualComponentBase> pVisualComponent,
    std::unique_ptr<PromptBase> pPromptHandler
)
    : m_pSelf(pIDT)
    , m_pTestConfig(pTestConfig)
    , m_pDisplay(pDisplay)
    , m_TestAllDisplays(false)
{
    m_pIdtJsonParser = std::move(pIdtJsonParser);
    m_pVisualComponent = std::move(pVisualComponent);
    m_pPromptHandler = std::move(pPromptHandler);
}

IDTPrivateImpl::~IDTPrivateImpl()
{

}

RC IDTPrivateImpl::Setup
(
    Display *display,
    std::unique_ptr<IDTDisplayHAL> pDisplayHAL,
    std::unique_ptr<SurfaceFill> pSurfaceHandler,
    const TestArguments& testArguments
)
{
    RC rc;

    m_pDisplayHAL = std::move(pDisplayHAL);
    m_pSurfaceHandler = std::move(pSurfaceHandler);
    m_TestArguments = testArguments;

    CHECK_RC(m_pIdtJsonParser->Init(this, m_pSurfaceHandler.get()));

    m_pDisplay = display;
    CHECK_RC(m_pVisualComponent->Init(m_pPromptHandler.get()));

    CHECK_RC(m_pSurfaceHandler->Setup(m_pSelf, m_pTestConfig));

    CHECK_RC(m_pDisplayHAL->Setup());

    if (!m_pIdtJsonParser->ValidateJson(testArguments))
    {
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    CHECK_RC(m_pDisplay->GetDetected(&m_DetectedDisplayIDs));

    if (!AreSpecifiedDisplaysConnected())
    {
        return RC::ILWALID_DISPLAY_MASK;
    }

    if (AreAllDisplaysToBeTested() || m_SpecifiedDisplays.empty())
    {
        CHECK_RC(PrepareSpecifiedDisplaysFromDetectedDisplays());
    }
    else if (!m_SpecifiedDisplays.empty() &&
            m_ResolutionInfoCommon.resolutionType != ResolutionInfo::ResType::ABSENT)
    {
        CHECK_RC(UpdateSpecifiedDisplaysWithSpecifiedResolution());
    }

    if (!AreEnoughHeadsAvailable())
    {
        return RC::DISPLAY_NO_FREE_HEAD;
    }

    if (!m_TestArguments.testUnsupportedDisplays)
    {
        CHECK_RC(RemoveUnsupportedDisplayIDs());
    }
    CHECK_RC(UpdateResolutionInfoWithHwResolution());

    CHECK_RC(PrepareAllSurfaces());
    CHECK_RC(SetupPrompts());

    return rc;
}

RC IDTPrivateImpl::Run()
{
    RC rc;
    CHECK_RC(PrepareHeadAssignmentConfig());

    for (auto& multiHeadAssignments: m_HeadAssignmentConfig)
    {
        CHECK_RC(HandleMultiHeadsRun(multiHeadAssignments));
    }

    return rc;
}

RC IDTPrivateImpl::Cleanup()
{
    StickyRC rc;
    rc = m_pSurfaceHandler->Cleanup();
    rc = m_pDisplayHAL->Cleanup();
    return rc;
}

RC IDTPrivateImpl::ResetPromptWaitForTimeout(UINT32 waitTimeSec, bool isDvsRun)
{
    m_pPromptHandler.reset(new PromptWaitForTimeout(waitTimeSec, isDvsRun));

    return RC::OK;
}

RC IDTPrivateImpl::SetAllDisplaysToBeTested(bool value)
{
    m_TestAllDisplays = value;
    return RC::OK;
}

bool IDTPrivateImpl::AreAllDisplaysToBeTested()
{
    return m_TestAllDisplays;
}

// missing unit test
bool IDTPrivateImpl::AddDisplayIDToBeTested
(
    const DisplayID& displayID,
    const ResolutionInfo& resolutionInfo
)
{
    auto itr = m_SpecifiedDisplays.find(displayID);
    if (itr != m_SpecifiedDisplays.end())
        return false;

    m_SpecifiedDisplays[displayID] = resolutionInfo;

    return true;
}

RC IDTPrivateImpl::SetCommonResolutionInfo(const ResolutionInfo& resolutionInfo)
{
    m_ResolutionInfoCommon = resolutionInfo;
    return RC::OK;
}

DisplayID IDTPrivateImpl::GetPrimaryDisplay()
{
    return m_pDisplay->GetPrimaryDisplay();
}

bool IDTPrivateImpl::AreSpecifiedDisplaysConnected()
{
    if (m_SpecifiedDisplays.empty())
        return true;

    auto itr = m_SpecifiedDisplays.begin();
    for (; itr != m_SpecifiedDisplays.end(); ++itr)
    {
        if (!binary_search(m_DetectedDisplayIDs.begin(), m_DetectedDisplayIDs.end(), itr->first))
        {
            break;
        }
    }
    return (itr == m_SpecifiedDisplays.end() ? true : false);
}

// Todo : missing unit tests when more displays are connected than Heads
RC IDTPrivateImpl::PrepareSpecifiedDisplaysFromDetectedDisplays()
{
    UINT32 usableHeadsCount = Utility::CountBits(m_pDisplay->GetUsableHeadsMask());
    UINT32 allocatedHeadCount = 0;
    for (auto& displayID : m_DetectedDisplayIDs)
    {
        allocatedHeadCount++;
        m_SpecifiedDisplays[displayID] = m_ResolutionInfoCommon;
        if (allocatedHeadCount >= usableHeadsCount)
        {
            break;
        }
    }

    return RC::OK;
}

RC IDTPrivateImpl::UpdateSpecifiedDisplaysWithSpecifiedResolution()
{
    for (auto itr = m_SpecifiedDisplays.begin(); itr != m_SpecifiedDisplays.end(); ++itr)
    {
        m_SpecifiedDisplays[itr->first] = m_ResolutionInfoCommon;
    }

    return RC::OK;
}

RC IDTPrivateImpl::UpdateResolutionInfoWithHwResolution()
{
    for (auto itr = m_SpecifiedDisplays.begin(); itr != m_SpecifiedDisplays.end(); ++itr)
    {
        if (itr->second.resolutionType == ResolutionInfo::ResType::MAX)
        {
            m_pDisplay->GetMaxResolution(itr->first, &itr->second.width,
                    &itr->second.height, &itr->second.refreshRate);
        }
        else if (itr->second.resolutionType == ResolutionInfo::ResType::NATIVE ||
                itr->second.resolutionType == ResolutionInfo::ResType::ABSENT)
        {
            itr->second.resolutionType = ResolutionInfo::ResType::NATIVE;

            m_pDisplay->GetNativeResolution(itr->first, &itr->second.width,
                    &itr->second.height, &itr->second.refreshRate);
        }
    }

    return RC::OK;
}

// Todo : missing unit test
RC IDTPrivateImpl::PrepareAllSurfaces()
{
    RC rc;

    for (auto itr = m_SpecifiedDisplays.begin(); itr != m_SpecifiedDisplays.end(); ++itr)
    {
        CHECK_RC(m_pSurfaceHandler->PrepareSurface(itr->first, itr->second));
        if (!m_TestArguments.rtImage.empty())
        {
            CHECK_RC(m_pSurfaceHandler->PrepareSurface(itr->first + 1, itr->second));
        }
    }

    return rc;
}

// Todo : missing unit test
RC IDTPrivateImpl::SetupPrompts()
{
    DisplayIDs displayIDs;
    auto itr = m_SpecifiedDisplays.begin();
    for (; itr != m_SpecifiedDisplays.end(); ++itr)
    {
        displayIDs.push_back(itr->first);
    }
    return m_pPromptHandler->SetupPrompts(displayIDs,
        m_TestArguments.randomPrompts,
        m_TestArguments.promptTimeoutMs,
        m_TestArguments.assumeSuccessOnPromptTimeout);
}

bool IDTPrivateImpl::AreEnoughHeadsAvailable()
{
    return (Utility::CountBits(m_pDisplay->GetUsableHeadsMask()) >=
            static_cast<INT32>(m_SpecifiedDisplays.size()));
}

RC IDTPrivateImpl::RemoveUnsupportedDisplayIDs()
{
    auto itr = m_SpecifiedDisplays.begin();
    while (itr != m_SpecifiedDisplays.end())
    {
        if (!m_pDisplay->IsSupportedDisplay(itr->first))
        {
            Printf(Tee::PriError, "IDT:: DisplayID 0x%x is an unsupported display\n",
               itr->first.Get());
            m_SpecifiedDisplays.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }

    return RC::OK;
}

RC IDTPrivateImpl::PrepareHeadAssignmentConfig()
{
    RC rc;

    DisplayIDs displayIDs;
    for (const auto&specifiedDisplaysElement : m_SpecifiedDisplays)
    {
        DisplayIDs displayIDsSingle;
        std::vector<UINT32> headsForSingleDisplayID;
        displayIDsSingle.push_back(specifiedDisplaysElement.first);
        CHECK_RC(m_pDisplay->GetHeadsForMultiDisplay(displayIDsSingle, &headsForSingleDisplayID));

        MultiHeadAssignments multiHeadAssignmentsForSingleDisplayID;
        multiHeadAssignmentsForSingleDisplayID[specifiedDisplaysElement.first]
            = headsForSingleDisplayID[0];
        displayIDs.push_back(specifiedDisplaysElement.first);

        if (!m_TestArguments.skipSingleDisplays)
        {
            m_HeadAssignmentConfig.push_back(multiHeadAssignmentsForSingleDisplayID);
        }
    }

    if (m_SpecifiedDisplays.size() > 1 ||
           (m_SpecifiedDisplays.size() == 1 &&
            m_TestArguments.skipSingleDisplays))
    {
        std::vector<UINT32> heads;
        MultiHeadAssignments multiHeadAssignments;
        CHECK_RC(m_pDisplay->GetHeadsForMultiDisplay(displayIDs, &heads));
        for (UINT32 i = 0; i < displayIDs.size(); i++)
        {
            multiHeadAssignments[displayIDs[i]] = heads[i];
        }
        m_HeadAssignmentConfig.push_back(multiHeadAssignments);
    }

    return rc;
}

RC IDTPrivateImpl::GetResolutionInfo
(
    const DisplayID& displayID,
    ResolutionInfo *resolutionInfo
) const
{
    StickyRC rc;
    auto itr = m_SpecifiedDisplays.find(displayID);
    if (itr != m_SpecifiedDisplays.end())
    {
        *resolutionInfo = itr->second;
    }
    else
    {
        rc = RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC IDTPrivateImpl::HandleMultiHeadsRun(const MultiHeadAssignments &multiHeadAssignments)
{
    StickyRC rc;

    CHECK_RC(m_pDisplayHAL->OnMultiHeadsRunStart(multiHeadAssignments));

    DisplayIDs testedDisplayIDs;

    OverrideValues pixelDepth;
    pixelDepth.val.dpth = m_TestArguments.oRPixelDepth;
    pixelDepth.kind = SettingOverrideKind::ORPIXELDEPTH;
    OverrideValues colorSpace;
    colorSpace.val.cs= m_TestArguments.colorSpace;
    colorSpace.kind = SettingOverrideKind::PROCAMPCOLORSPACE;
    OverrideValues stereoEnable;
    stereoEnable.val.stereo = !m_TestArguments.rtImage.empty();
    stereoEnable.kind = SettingOverrideKind::STEREO;

    for (auto& headAssignment : multiHeadAssignments)
    {
        ResolutionInfo resolutionInfo;
        CHECK_RC(GetResolutionInfo(headAssignment.first, &resolutionInfo));

        // LwDisplay error code 20:
        // Same stereo pin cannot be driven by more than one head.
        // Therefore only enable stereo pin on head 0 for multihead case
        if (multiHeadAssignments.size() > 1 && headAssignment.second != 0)
        {
            stereoEnable.val.stereo = false;
        }

        UINT32 refreshRate = resolutionInfo.refreshRate;
        vector<OverrideValues> overrideVals = { pixelDepth, colorSpace, stereoEnable };
        CHECK_RC(m_pDisplayHAL->SetMode(headAssignment.first, resolutionInfo,
                    overrideVals, headAssignment.second, &refreshRate));
        if (resolutionInfo.refreshRate != refreshRate)
        {
            Printf(Tee::PriWarn, "Requested refresh rate %u is not supported. Applied "
                    "refresh rate %u.\n", resolutionInfo.refreshRate, refreshRate);
            resolutionInfo.refreshRate = refreshRate;
        }

        Display::Encoder encoder;
        CHECK_RC(m_pDisplay->GetEncoder(headAssignment.first, &encoder));

        HDCPInfo hdcpKeys;
        CHECK_RC(GetHDCPInfo(headAssignment.first, &hdcpKeys));

        CHECK_RC(ShowGatheredInformationOnDisplay(
            headAssignment.first,
            headAssignment.second,
            resolutionInfo,
            static_cast<UINT32>(multiHeadAssignments.size()),
            encoder,
            hdcpKeys));

        testedDisplayIDs.push_back(headAssignment.first);
    }

    rc = m_pPromptHandler->PromptUser(testedDisplayIDs);

    CHECK_RC(m_pDisplayHAL->OnMultiHeadsRunEnd(multiHeadAssignments));

    return rc;
}

//Todo missing unit test
RC IDTPrivateImpl::GetHDCPInfo(const DisplayID& displayID, HDCPInfo *hdcpInfo)
{
    RC rc;
    if (m_TestArguments.skipHDCP)
        return rc;

    bool isHdcpEnabled;
    bool isHdcp22Capable;
    bool isHdcp22Enabled;

    CHECK_RC(m_pDisplay->GetHDCPInfo(displayID, &isHdcpEnabled, &hdcpInfo->isDisplayCapable,
            &hdcpInfo->isGpuCapable, &isHdcp22Capable, &isHdcp22Enabled, true));

    if (hdcpInfo->isGpuCapable)
    {
        CHECK_RC(m_pDisplay->RenegotiateHDCP(displayID, m_TestArguments.hdcpTimeout));
        CHECK_RC(WaitForSpecifiedHdcpDelay());
        CHECK_RC(m_pDisplay->GetHDCPInfo(displayID, &isHdcpEnabled, &hdcpInfo->isDisplayCapable,
                &hdcpInfo->isGpuCapable, &isHdcp22Capable, &isHdcp22Enabled, true));

        if (hdcpInfo->isDisplayCapable)
        {
            CHECK_RC(m_pDisplay->GetHDCPDeviceData(displayID, &hdcpInfo->hdcpDeviceData, true));
        }
    }

    rc = RunHDCPKeyTest(displayID);
    hdcpInfo->isHDCPKeyTestSuccessful = (rc == OK) ? true : false;

    return rc;
}

RC IDTPrivateImpl::RunHDCPKeyTest(const DisplayID& displayID)
{
    std::unique_ptr<HdcpKey> hdcpKeyTest(new HdcpKey());
    hdcpKeyTest->BindTestDevice(m_pSelf->GetBoundTestDevice());
    hdcpKeyTest->SetDisplayToTest(displayID);
    hdcpKeyTest->SetMaxKpToTest(2);

    return hdcpKeyTest->Run();
}

RC IDTPrivateImpl::SetupDisplayPrompt
(
    const DisplayID& displayID,
    UINT32 head,
    const ResolutionInfo& resolutionInfo,
    UINT32 displayCount,
    const Display::Encoder& encoder,
    const HDCPInfo& hdcpKeys,
    bool isLtImage,
    Surface2D **ppImage
)
{
    RC rc;
    // Since surface id's are displayId based, use next index for right surface
    UINT32 surfaceId = isLtImage ? displayID.Get() : displayID.Get() + 1;
    CHECK_RC(LwDispUtils::FillPatternByType(m_pSurfaceHandler.get(),
        isLtImage ? m_TestArguments.image : m_TestArguments.rtImage, surfaceId));

    Surface2D *surface = m_pSurfaceHandler->GetSurface(surfaceId);

    if (m_TestArguments.skipVisualText)
    {
        Printf(Tee::PriAlways, "Is this image okay (y for yes/n for no)?\n");
    }

    if (!m_TestArguments.skipVisualText)
    {
        CHECK_RC(m_pVisualComponent->DisplayCommonMsg(surface, displayID,
            resolutionInfo, head, displayCount,
            !m_TestArguments.rtImage.empty(), isLtImage ? true: false));

        CHECK_RC(m_pVisualComponent->DisplayEncoderInfo(surface, displayID,
            head, resolutionInfo, encoder));
        if (!m_TestArguments.skipHDCP)
        {
            CHECK_RC(m_pVisualComponent->DisplayHDCPMsg(surface, displayID,
                resolutionInfo, hdcpKeys));
        }
    }

    CHECK_RC(m_pSurfaceHandler->CopySurface(surfaceId));
    *ppImage = m_pSurfaceHandler->GetImage(surfaceId);

    return rc;
}

RC IDTPrivateImpl::ShowGatheredInformationOnDisplay
(
    const DisplayID& displayID,
    UINT32 head,
    const ResolutionInfo& resolutionInfo,
    UINT32 displayCount,
    const Display::Encoder& encoder,
    const HDCPInfo& hdcpKeys
)
{
    RC rc;

    Surface2D *pLtImage = nullptr;
    Surface2D *pRtImage = nullptr;
    CHECK_RC(SetupDisplayPrompt(displayID, head, resolutionInfo,
        displayCount, encoder, hdcpKeys, true, &pLtImage));

    if (!m_TestArguments.rtImage.empty())
    {
        CHECK_RC(SetupDisplayPrompt(displayID, head, resolutionInfo,
            displayCount, encoder, hdcpKeys, false, &pRtImage));
    }

    CHECK_RC(m_pDisplayHAL->DisplayImage(displayID, pLtImage, pRtImage));
    return rc;
}

RC IDTPrivateImpl::WaitForSpecifiedHdcpDelay()
{
    Tasker::Sleep(m_TestArguments.hdcpDelay);

    return RC::OK;
}

RC IDTPrivateImpl::SetTestArg(const TestArguments& testArguments)
{
    return m_pSelf->SetTestArguments(testArguments);
}

const DisplayIDs& IDTPrivateImpl::GetDetected() const
{
    return m_DetectedDisplayIDs;
}

bool IDTPrivateImpl::ShouldMaxResolutionBeUsedForAllSpecifiedDisplays() const
{
    return (m_ResolutionInfoCommon.resolutionType == ResolutionInfo::ResType::MAX
            ? true : false);
}

bool IDTPrivateImpl::ShouldNativeResolutionBeUsedForAllSpecifiedDisplays() const
{
    return (m_ResolutionInfoCommon.resolutionType == ResolutionInfo::ResType::NATIVE
            ? true : false);
}

const DisplayResolutionInfoMap& IDTPrivateImpl::GetSpecifiedDisplays() const
{
    return m_SpecifiedDisplays;
}

const HeadAssignmentConfig& IDTPrivateImpl::GetHeadAssignmentConfig() const
{
    return m_HeadAssignmentConfig;
}

//------------------------------------------------------------------------------
// IDTDisplayHAL : The abstract base class to abstract the platform differences
//------------------------------------------------------------------------------
IDTDisplayHAL::IDTDisplayHAL(Display *pDisplay)
    : m_pDisplay(pDisplay)
{

}

RC IDTDisplayHAL::OnMultiHeadsRunStart(const MultiHeadAssignments &multiHeadAssignments)
{
    // This is required for correct colorspace on HDMI and must be done before RenotiateHDCP
    return m_pDisplay->EnableHdmiInfoframes(true);
}

RC IDTDisplayHAL::OnMultiHeadsRunEnd(const MultiHeadAssignments &multiHeadAssignments)
{
    return m_pDisplay->DetachAllDisplays();
}

//------------------------------------------------------------------------------
// IDTEvoDisplayHAL : Concrete implementation of DisplayHAL for EvoDisplay
//------------------------------------------------------------------------------
IDTEvoDisplayHAL::IDTEvoDisplayHAL(Display *display)
    : IDTDisplayHAL(display)
{

}

RC IDTEvoDisplayHAL::Setup()
{
    const UINT32 selectedDisplays = m_pDisplay->Selected();
    const INT32  firstDisplay     = Utility::BitScanForward(selectedDisplays);

    if (firstDisplay >= 0)
    {
        this->m_OriginalDisplayID = BIT(firstDisplay);
    }
    return RC::OK;
}

RC IDTEvoDisplayHAL::OnMultiHeadsRunStart(const MultiHeadAssignments &multiHeadAssignments)
{
    RC rc;

    CHECK_RC(IDTDisplayHAL::OnMultiHeadsRunStart(multiHeadAssignments));

    DisplayIDs inUseDisplayIDs;
    for (const auto& headAssignment : multiHeadAssignments)
    {
        inUseDisplayIDs.push_back(headAssignment.first);
    }

    UINT32 selectedMask = 0;
    m_pDisplay->DispIDToDispMask(inUseDisplayIDs, selectedMask);

    CHECK_RC(m_pDisplay->Select(selectedMask));

    return rc;
}

/*virtual*/ RC IDTEvoDisplayHAL::SetMode
(
    const DisplayID& displayID,
    const ResolutionInfo& resolutionInfo,
    const vector<OverrideValues> &overrides,
    UINT32 head,
    UINT32 *pRefreshRate
)
{
    RC rc;
    for (const auto &overrideParam : overrides)
    {
        if (overrideParam.kind == SettingOverrideKind::ORPIXELDEPTH)
        {
            CHECK_RC(m_pDisplay->SetPreferredORPixelDepth(displayID, overrideParam.val.dpth));
        }
        else if (overrideParam.kind == SettingOverrideKind::PROCAMPCOLORSPACE)
        {
            EvoProcampSettings pcs;
            //We want to overrideParam color space only
            pcs.ColorSpace = overrideParam.val.cs;
            if (pcs.ColorSpace != ORColorSpace::RGB)
            {
                //These are default settings for YUV_609 and are tested on DP and HDMI
                pcs.ChromaLpf = EvoProcampSettings::CHROMA_LPF::ON;
                pcs.SatCos = 1024;
                pcs.SatSine = 0;
            }
            pcs.Dirty = true;
            CHECK_RC(m_pDisplay->SetProcampSettings(&pcs));
        }
        else if (overrideParam.kind == SettingOverrideKind::STEREO)
        {
            Printf(Tee::PriLow, "Ignoring override for Stereo Settings in EVO\n");
        }
        else
        {
            Printf(Tee::PriError, "Invalid overrideParam Kind Id %u\n",
                    static_cast<UINT32> (overrideParam.kind));
            return RC::ILWALID_ARGUMENT;
        }
    }
    return m_pDisplay->SetMode(displayID,
            resolutionInfo.width,
            resolutionInfo.height,
            resolutionInfo.depth,
            resolutionInfo.refreshRate,
            Display::FilterTapsNoChange,
            Display::ColorCompressionNoChange,
            0, // scaler mode
            (VIDEO_INFOFRAME_CTRL*)0,
            (AUDIO_INFOFRAME_CTRL *)0);
}

RC IDTEvoDisplayHAL::Cleanup()
{
    StickyRC rc;
    if (m_OriginalDisplayID)
    {
        ResolutionInfo safeResolution;
        rc = m_pDisplay->Select(m_OriginalDisplayID);
        m_pDisplay->ErasePreferredORPixelDepth(m_OriginalDisplayID);
        ORPixelDepthBpp pd = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_DEFAULT;
        ORColorSpace cs= ORColorSpace::MAX_VAL;
        OverrideValues pixelDepth;
        pixelDepth.val.dpth = pd;
        pixelDepth.kind = SettingOverrideKind::ORPIXELDEPTH;
        OverrideValues colorSpace;
        colorSpace.val.cs = cs;
        colorSpace.kind = SettingOverrideKind::PROCAMPCOLORSPACE;
        vector<OverrideValues> overrideVals = { pixelDepth, colorSpace };
        rc = SetMode(m_OriginalDisplayID,
                safeResolution,
                overrideVals,
                0,
                &safeResolution.refreshRate
                );
    }
    return rc;
}

RC IDTEvoDisplayHAL::DisplayImage
(
    const DisplayID& displayID,
    Surface2D *image,
    Surface2D *pRtImage
)
{
    return m_pDisplay->SetImage(displayID, image);
}
//------------------------------------------------------------------------------
// IDTLwDisplayHAL : Concrete implementation of DisplayHAL for LwDisplay
//------------------------------------------------------------------------------
IDTLwDisplayHAL::IDTLwDisplayHAL(Tee::Priority printPriority, Display *display)
    : IDTDisplayHAL(display)
    , m_PrintPriority(printPriority)
    , m_pLwDisplay(nullptr)
{
    m_pDisplay = static_cast<LwDisplay *>(display);
}

RC IDTLwDisplayHAL::Setup()
{
    m_pLwDisplay = static_cast<LwDisplay *>(m_pDisplay);
    if (m_pLwDisplay->GetDisplayClassFamily() != Display::LWDISPLAY)
        return RC::SOFTWARE_ERROR;

    return RC::OK;
}

RC IDTLwDisplayHAL::SetModeOverride(void * pCbArgs)
{
    RC rc;
    CBArgs cbArgs = *static_cast<CBArgs*>(pCbArgs);
    UINT32 headIdx = *static_cast<const UINT32*>(cbArgs["head"]);
    ORColorSpace proCampCS = *static_cast <const ORColorSpace*>(cbArgs["cs"]);
    bool stereo = *static_cast<const bool*>(cbArgs["stereo"]);

    LwDispHeadSettings *pHeadSettings = nullptr;
    CHECK_RC(GetHeadSettings(headIdx, &pHeadSettings));

    pHeadSettings->procampSettings.ColorSpace = proCampCS;
    pHeadSettings->procampSettings.dirty = true;
    if (proCampCS != ORColorSpace::RGB)
    {
        //These are default settings for YUV_609 and are tested on DP and HDMI
        pHeadSettings->procampSettings.ChromaLpf = LwDispProcampSettings::CHROMA_LPF::ENABLE;
        pHeadSettings->procampSettings.satCos = 1024;
        pHeadSettings->procampSettings.satSine = 0;
    }

    if (stereo)
    {
        pHeadSettings->controlSettings.dirty = true;
        pHeadSettings->controlSettings.stereoPin.lockPinType =
            LwDispControlSettings::LOCK_PIN_TYPE_PIN;
        CHECK_RC(m_pLwDisplay->GetStereoPinIdx(
            &pHeadSettings->controlSettings.stereoPin.lockPilwalue));
        pHeadSettings->controlSettings.slaveLockPin.lockPilwalue =
            LwDispControlSettings::LOCK_PIN_VALUE_NONE;
        pHeadSettings->controlSettings.masterLockPin.lockPilwalue =
            LwDispControlSettings::LOCK_PIN_VALUE_NONE;
    }
    return rc;
}

/*virtual*/ RC IDTLwDisplayHAL::SetMode
(
    const DisplayID& displayID,
    const ResolutionInfo& resolutionInfo,
    const vector<OverrideValues> &overrides,
    UINT32 head,
    UINT32 *pRefreshRate
)
{
    RC rc;
    std::function <RC(void *)>CallBack = nullptr;
    CBArgs cbArgs;
    cbArgs["head"] = &head;
    for (const auto &overrideParam : overrides)
    {
        if (overrideParam.kind == SettingOverrideKind::ORPIXELDEPTH)
        {
            CHECK_RC(m_pDisplay->SetPreferredORPixelDepth(displayID,
                        overrideParam.val.dpth));
        }
        else if (overrideParam.kind == SettingOverrideKind::PROCAMPCOLORSPACE)
        {
            cbArgs["cs"] = &overrideParam.val.cs;
        }
        else if (overrideParam.kind == SettingOverrideKind::STEREO)
        {
            cbArgs["stereo"] = &overrideParam.val.stereo;
        }
        else
        {
            Printf(Tee::PriError, "Invalid overrideParam Kind Id %u\n",
                    static_cast<UINT32> (overrideParam.kind));
            return RC::ILWALID_ARGUMENT;
        }
    }
    CallBack = std::bind(&IDTLwDisplayHAL::SetModeOverride,
            this,
            std::placeholders::_1);

    rc = m_pLwDisplay->SetModeWithSingleWindow(displayID,
            resolutionInfo,
            head,
            CallBack, &cbArgs);

    // Print Raster timings as this information can be used to
    // get the timings information required for DPC.
    LwDispHeadSettings *pHeadSettings = nullptr;
    CHECK_RC(GetHeadSettings(head, &pHeadSettings));

    pHeadSettings->rasterSettings.Print(m_PrintPriority);

    *pRefreshRate = round(pHeadSettings->rasterSettings.CallwlateRefreshRate());
    return rc;
}

RC IDTLwDisplayHAL::OnMultiHeadsRunEnd(const MultiHeadAssignments &multiHeadAssignments)
{
    RC rc;
    DisplayIDs inUseDisplayIDs;

    for (const auto& headAssignment : multiHeadAssignments)
    {
        inUseDisplayIDs.push_back(headAssignment.first);
    }

    CHECK_RC(m_pLwDisplay->DetachDisplay(inUseDisplayIDs));
    CHECK_RC(m_pLwDisplay->DetachAllWindows());

    return rc;
}

RC IDTLwDisplayHAL::Cleanup()
{
    return m_pLwDisplay->ResetEntireState();
}

RC IDTLwDisplayHAL::DisplayImage
(
    const DisplayID& displayID,
    Surface2D *image,
    Surface2D *pRtImage
)
{
    return m_pLwDisplay->DisplayImage(displayID, image, pRtImage);
}

RC IDTLwDisplayHAL::GetHeadSettings(HeadIndex headIndex, LwDispHeadSettings **pHeadSettings)
{
    RC rc;

    LwDispChnContext *pLwDispChnContext;
    CHECK_RC(m_pLwDisplay->
            GetLwDisplayChnContext(Display::CORE_CHANNEL_ID,
                &pLwDispChnContext,
                LwDisplay::ILWALID_WINDOW_INSTANCE,
                Display::ILWALID_HEAD));
    if (pLwDispChnContext == nullptr)
    {
        return RC::SOFTWARE_ERROR;
    }
    LwDispCoreChnContext *pCoreContext =
        static_cast<LwDispCoreChnContext *>(pLwDispChnContext);

    *pHeadSettings = &pCoreContext->DispLwstomSettings.HeadSettings[headIndex];

    return rc;
}
//------------------------------------------------------------------------------
// IDTJsonParser : Concrete implementation of for JSON parsing of "IDT::TestArg" string
//------------------------------------------------------------------------------
IDTJsonParser::IDTJsonParser(): m_pIDT(nullptr), m_pSurfaceHandler(nullptr)
{

}

RC IDTJsonParser::Init(IDTPrivateImpl *idt, SurfaceFill *pSurfaceHandler)
{
    m_pIDT = idt;
    this->m_pSurfaceHandler = pSurfaceHandler;

    return RC::OK;
}

bool IDTJsonParser::ValidateJson(const TestArguments& testArguments)
{
#define PARSE_PARAM_AND_BREAK_ON_ERROR(ParamName, ParserFn, ParamToTest, resultVar)    \
        JsonParseResult resultVar = ParserFn(ParamToTest);                             \
        if (resultVar == JsonParseResult::INVALID)                                     \
        {                                                                              \
            Printf(Tee::PriError, "IDT:: \"%s\" Parameter is not valid\n", ParamName); \
            break;                                                                     \
        }

    bool retVal = false;

    do
    {
        if (!(m_pSurfaceHandler->IsSupportedImage(testArguments.image) ||
            m_pSurfaceHandler->IsSupportedImageFile(testArguments.image)))
        {
           Printf(Tee::PriError, "IDT:: \"%s\" Parameter (mandatory) is not valid\n",
                   JSON_PARAM_IMAGE);
           break;
        }
        Printf(Tee::PriNormal, "IDT:: \"%s\" will be used for display\n",
            testArguments.image.c_str());

        PARSE_PARAM_AND_BREAK_ON_ERROR(JSON_PARAM_DISPLAYS, ParseDisplaysParameter,
            testArguments.displays, displaysParseResult);

        PARSE_PARAM_AND_BREAK_ON_ERROR(JSON_PARAM_RESOLUTION, ParseResolutionParameter,
            testArguments.resolution, resolutionParseResult);

        PARSE_PARAM_AND_BREAK_ON_ERROR(JSON_PARAM_DISPLAYS_EX, ParseDisplaysExParameter,
            testArguments.displaysEx, displaysExParseResult);

        if (displaysExParseResult == JsonParseResult::VALID &&
           (displaysParseResult == JsonParseResult::VALID ||
            resolutionParseResult == JsonParseResult::VALID))
        {
           Printf(Tee::PriError, "IDT:: \"%s\" OR \"%s\" must not be specified with \"%s\"\n",
               JSON_PARAM_DISPLAYS, JSON_PARAM_RESOLUTION, JSON_PARAM_DISPLAYS_EX);
           break;
        }

        retVal = true;
    } while (false);

    return retVal;

#undef PARSE_PARAM_AND_BREAK_ON_ERROR
}

IDTJsonParser::JsonParseResult IDTJsonParser:: ParseDisplaysParameter(const DisplaysArg& displays)
{
    JsonParseResult retVal = JsonParseResult::INVALID;

    do
    {
        if (displays.empty())
        {
            retVal = JsonParseResult::ABSENT;
            break;
        }
        Document displaysJson;
        displaysJson.Parse(displays.c_str());

        if (!displaysJson.IsArray()) break;

        if (!displaysJson.IsArray() || displaysJson.Size() == 0)
        {
            break;
        }

        if (!AreDisplayIDsValid(displaysJson))
        {
            break;
        }

        retVal = JsonParseResult::VALID;
    } while (false);

    return retVal;
}

bool IDTJsonParser::AreDisplayIDsValid(const Value& displays)
{
    SizeType i = 0;
    for ( ; i < displays.Size(); i++)
    {
        bool isDisplayIDAll = false;

        if (!IsDisplayIDValid(displays[i], &isDisplayIDAll, true))
        {
            break;
        }

        if (isDisplayIDAll)
        {
            if (displays.Size() > 1)
                break;
            m_pIDT->SetAllDisplaysToBeTested(true);
        }
    }

    return (i == displays.Size() ? true : false);
}

bool IDTJsonParser::IsDisplayIDValid(const Value& displayID, bool *isDisplayIDAll, bool addEntry)
{
    const char *const JSON_VALUE_DISPLAY_ALL = "all";
    const char *const JSON_VALUE_DISPLAY_PRIMARY = "primary";

    *isDisplayIDAll = false;

    bool retVal = false;
    do
    {
        if (!displayID.IsString())
        {
            break;
        }

        if (!strcmp(displayID.GetString(), JSON_VALUE_DISPLAY_ALL))
        {
            retVal = *isDisplayIDAll = true;
            break;
        }

        // Todo missing unit test
        if (!strcmp(displayID.GetString(), JSON_VALUE_DISPLAY_PRIMARY))
        {
            ResolutionInfo resolutionInfo;
            retVal = m_pIDT->AddDisplayIDToBeTested(m_pIDT->GetPrimaryDisplay(), resolutionInfo);
            break;
        }

        UINT32 displayIDNum = 0;
        if (Utility::StringToUINT32(displayID.GetString(), &displayIDNum) != RC::OK)
        {
            break;
        }

        if (addEntry)
        {
            ResolutionInfo resolutionInfo;
            retVal = m_pIDT->AddDisplayIDToBeTested(displayIDNum, resolutionInfo);
        }
        else
        {
            retVal = true;
        }
    } while (false);

    return retVal;
}

IDTJsonParser::JsonParseResult IDTJsonParser::ParseResolutionParameter
(
    const ResolutionArg& resolution
)
{
    JsonParseResult retVal = JsonParseResult::INVALID;

    do
    {
        if (resolution.empty())
        {
            retVal = JsonParseResult::ABSENT;
            break;
        }

        Value resolutionJsolwal;
        if (!PrepareValidJsonForResolution(resolution, &resolutionJsolwal))
        {
            break;
        }

        ResolutionInfo resolutionInfoCommon;
        if (!IsResolutiolwalid(resolutionJsolwal, &resolutionInfoCommon))
        {
            break;
        }
        m_pIDT->SetCommonResolutionInfo(resolutionInfoCommon);
        retVal = JsonParseResult::VALID;
    } while (false);

    return retVal;
}

bool IDTJsonParser::PrepareValidJsonForResolution
(
    const ResolutionArg& resolution,
    Value *resolutionJsolwal
)
{
    // Hack - Due to shortcoming of JS framework which can't handle
    // dual natured JSON values for same key which IDT uses. See below:
    //
    // {"resolution" : "max"} / {"resolution" : [1024, 768, 32, 60]}
    // So, from spec file we pass the the JSON value as a string and then
    // again prepare a JSON doc and then parse it
    bool retVal = true;

    Document root;
    const size_t maxLengthNonArray = strlen(JSON_VALUE_RESOLUTION_NATIVE);

    std::string dummyArg = Utility::StrPrintf("{\"%s\" : ", JSON_PARAM_DISPLAYS);
    if (resolution.length() <= maxLengthNonArray)
    {
        dummyArg += "\"" + resolution + "\"}";
    }
    else
    {
        dummyArg += resolution + "}";
    }

    root.Parse(dummyArg.c_str());
    if (!root.IsObject())
    {
        retVal = false;
    }

    *resolutionJsolwal = root[JSON_PARAM_DISPLAYS];

    return retVal;
}

bool IDTJsonParser::IsResolutiolwalid(const Value& resolution, ResolutionInfo *resolutionInfo)
{
    bool retVal = true;

    if (resolution.IsString())
    {
        if (!strcmp(resolution.GetString(), JSON_VALUE_RESOLUTION_MAX))
        {
            resolutionInfo->resolutionType = ResolutionInfo::ResType::MAX;
        }
        else if (!strcmp(resolution.GetString(), JSON_VALUE_RESOLUTION_NATIVE))
        {
            resolutionInfo->resolutionType = ResolutionInfo::ResType::NATIVE;
        }
        else
        {
            retVal = false;
        }
    }
    else if (resolution.IsArray())
    {
        if (!ParseLwstomResolution(resolution, resolutionInfo))
        {
            retVal = false;
        }
        resolutionInfo->resolutionType = ResolutionInfo::ResType::CUSTOM;
    }
    else
    {
        retVal = false;
    }

    return retVal;
}

bool IDTJsonParser::ParseLwstomResolution(const Value& resolution, ResolutionInfo *resolutionInfo)
{
    bool retVal = true;

    if (resolution.Size() != 4)
    {
        retVal = false;
    }
    else
    {
        std::vector<UINT32> resolutionParams;
        const Value& resolutionArray = resolution;
        for (SizeType i = 0; i < resolutionArray.Size() ; i++)
        {
            if (!resolutionArray[i].IsNumber())
            {
                retVal = false;
                break;
            }
            resolutionParams.push_back(resolutionArray[i].GetUint());
        }

        if (resolutionParams.size() == 4)
        {
            ResolutionInfo resolutionInfoLocal(ResolutionInfo::ResType::CUSTOM,
                                                resolutionParams[0],
                                                resolutionParams[1],
                                                resolutionParams[2],
                                                resolutionParams[3]);
            *resolutionInfo = resolutionInfoLocal;
        }
    }

    return retVal;
}

IDTJsonParser::JsonParseResult IDTJsonParser::ParseDisplaysExParameter
(
    const DisplaysExArg& displaysEx
)
{
    JsonParseResult retVal = JsonParseResult::INVALID;

    do
    {
        if (displaysEx.empty())
        {
            retVal = JsonParseResult::ABSENT;
            break;
        }

        Document displaysExJson;
        displaysExJson.Parse(displaysEx.c_str());

        if (!displaysExJson.IsArray() || displaysExJson.Size() == 0)
        {
            break;
        }

        if (!IsDisplayResolutionPairsArrayValid(displaysExJson))
        {
            break;
        }

        retVal = JsonParseResult::VALID;
    } while (false);

    return retVal;
}

bool IDTJsonParser::IsDisplayResolutionPairsArrayValid(const Value& displaysEx)
{
    SizeType i = 0;

    for ( ; i != displaysEx.Size(); ++i)
    {
        bool isDisplayIDAll = false;
        if (!IsDisplayResolutionPairValid(displaysEx[i], &isDisplayIDAll))
        {
            break;
        }

        if (isDisplayIDAll)
        {
            if (displaysEx.Size() > 1)
                break;
            m_pIDT->SetAllDisplaysToBeTested(true);
        }
    }

    return (i == displaysEx.Size() ? true : false);
}

bool IDTJsonParser::IsDisplayResolutionPairValid
(
    const Value& displayResolutionPair,
    bool *isDisplayIDAll
)
{
    *isDisplayIDAll = false;

    bool retVal = false;

    do
    {
        Value::ConstMemberIterator itr = displayResolutionPair.MemberBegin();
        if (itr == displayResolutionPair.MemberEnd())
        {
            break;
        }

        if (itr + 1 != displayResolutionPair.MemberEnd())
        {
            break;
        }

        if (!IsDisplayIDValid(itr->name, isDisplayIDAll, false))
        {
            break;
        }

        ResolutionInfo resolutionInfo;
        if (!IsResolutiolwalid(itr->value, &resolutionInfo))
        {
            break;
        }
        if (*isDisplayIDAll)
        {
            m_pIDT->SetCommonResolutionInfo(resolutionInfo);
            retVal = true;
        }
        else
        {
            UINT32 displayIDNum = 0;
            if (Utility::StringToUINT32(itr->name.GetString(), &displayIDNum) != RC::OK){

            }
            retVal = m_pIDT->AddDisplayIDToBeTested(displayIDNum, resolutionInfo);
        }
    } while (false);

    return retVal;
}
