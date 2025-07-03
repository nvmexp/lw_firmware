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

#ifndef IDT_PRIVATE_IMPL_H_
#define IDT_PRIVATE_IMPL_H_

#include "document.h"
#include "writer.h"

#include <map>
#include <vector>

#include "core/include/cnslmgr.h"
#include "core/include/display.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "ctrl/ctrl0073.h"
#include "gpu/display/disp_visualcomponent.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/dmawrap.h"
#include "gpu/tests/hdcpkey.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/utility/surffill.h"
#include "random.h"

#include "idt.h"

class IDTDisplayHAL;
class IDTJsonParser;

typedef RC (IDTPrivateImpl::*SetValueFnPtr)(bool);
typedef std::map<DisplayID, UINT32> MultiHeadAssignments;
typedef std::vector<MultiHeadAssignments> HeadAssignmentConfig;

using namespace rapidjson;
using namespace SurfaceUtils;

struct TestArguments
{
    bool isInitialized = false;
    std::string image;
    std::string rtImage;
    DisplaysArg displays;
    ResolutionArg resolution;
    DisplaysExArg displaysEx;
    bool skipHDCP = false;
    bool randomPrompts = false;
    bool enableDMA = true;
    bool skipVisualText = false;
    bool enableDVSRun = false;
    bool enableBkgndMode = false;
    UINT32 promptTimeoutMs = 0;
    bool assumeSuccessOnPromptTimeout = false;
    bool testUnsupportedDisplays = false;
    bool skipSingleDisplays = false;
    ORPixelDepthBpp oRPixelDepth = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_DEFAULT;
    ORColorSpace colorSpace = ORColorSpace::RGB;

    UINT32 hdcpDelay = 0;
    UINT32 hdcpTimeout = 0;
};

typedef std::map<DisplayID, ResolutionInfo> DisplayResolutionInfoMap;

class IDTPrivateImpl
{
public:
    IDTPrivateImpl(IDT *pIDT, GpuTestConfiguration *testConfig);

    ~IDTPrivateImpl();

    RC Setup
    (
        Display *display,
        std::unique_ptr<IDTDisplayHAL> pDisplayHAL,
        std::unique_ptr<SurfaceFill> pSurfaceHandler,
        const TestArguments& testArguments
    );

    RC Run();

    RC Cleanup();

    RC ResetPromptWaitForTimeout(UINT32 waitTimeSec, bool isDvsRun);

    RC SetAllDisplaysToBeTested(bool value);

    bool AreAllDisplaysToBeTested();
    bool AddDisplayIDToBeTested(const DisplayID& displayID, const ResolutionInfo& resolutionInfo);

    RC SetCommonResolutionInfo(const ResolutionInfo& resolutionInfo);

    DisplayID GetPrimaryDisplay();

    // The below functions are supposed to be used only during Unit Testing
    IDTPrivateImpl
    (
        IDT *pIDT,
        GpuTestConfiguration *pTestConfig,
        Display *display,
        std::unique_ptr<IDTJsonParser> pIdtJsonParser,
        std::unique_ptr<VisualComponentBase> pVisualComponent,
        std::unique_ptr<PromptBase> pPromptHandler
    );

    RC SetTestArg(const TestArguments& testArguments);
    const DisplayIDs& GetDetected() const;

    bool ShouldMaxResolutionBeUsedForAllSpecifiedDisplays() const;
    bool ShouldNativeResolutionBeUsedForAllSpecifiedDisplays() const;
    const DisplayResolutionInfoMap& GetSpecifiedDisplays() const;
    const HeadAssignmentConfig& GetHeadAssignmentConfig()const;

private:
    enum class JsonParseResult
    {
        ABSENT,
        VALID,
        INVALID
    };

    IDT *m_pSelf;
    GpuTestConfiguration *m_pTestConfig;
    TestArguments m_TestArguments;

    Display *m_pDisplay;
    std::unique_ptr<IDTJsonParser> m_pIdtJsonParser;
    std::unique_ptr<IDTDisplayHAL> m_pDisplayHAL;
    std::unique_ptr<VisualComponentBase> m_pVisualComponent;
    std::unique_ptr<SurfaceFill> m_pSurfaceHandler;

    std::unique_ptr<PromptBase> m_pPromptHandler;

    bool m_TestAllDisplays;

    ResolutionInfo m_ResolutionInfoCommon;
    DisplayIDs m_DetectedDisplayIDs;

    DisplayResolutionInfoMap m_SpecifiedDisplays;
    HeadAssignmentConfig m_HeadAssignmentConfig;

    DisplayIDs m_OriginalDisplayIDs;

    bool AreSpecifiedDisplaysConnected();
    RC PrepareSpecifiedDisplaysFromDetectedDisplays();
    RC UpdateSpecifiedDisplaysWithSpecifiedResolution();
    RC UpdateResolutionInfoWithHwResolution();

    RC PrepareAllSurfaces();

    RC SetupPrompts();

    bool AreEnoughHeadsAvailable();
    RC RemoveUnsupportedDisplayIDs();

    RC PrepareHeadAssignmentConfig();
    RC GetResolutionInfo(const DisplayID& displayID, ResolutionInfo *resolutionInfo) const;

    RC HandleMultiHeadsRun(const MultiHeadAssignments &multiHeadAssignments);

    RC GetHDCPInfo(const DisplayID& displayID, HDCPInfo *hdcpInfo);

    RC RunHDCPKeyTest(const DisplayID& displayID);

    RC ShowGatheredInformationOnDisplay
    (
        const DisplayID& displayID,
        UINT32 head,
        const ResolutionInfo& resolutionInfo,
        UINT32 displayCount,
        const Display::Encoder& encoder,
        const HDCPInfo& hdcpKeys
    );

    RC SetupDisplayPrompt
    (
        const DisplayID& displayID,
        UINT32 head,
        const ResolutionInfo& resolutionInfo,
        UINT32 displayCount,
        const Display::Encoder& encoder,
        const HDCPInfo& hdcpKeys,
        bool isLtImage,
        Surface2D **ppImage
    );

    RC WaitForSpecifiedHdcpDelay();
};

enum class SettingOverrideKind
{
    ORPIXELDEPTH, PROCAMPCOLORSPACE, STEREO, MAX_VAL
};

union SettingOverrideValue
{
    ORPixelDepthBpp dpth;
    ORColorSpace    cs;
    bool            stereo;
};
struct OverrideValues
{
    SettingOverrideValue val;
    SettingOverrideKind kind;
};

class IDTDisplayHAL
{
public:
    IDTDisplayHAL(Display *pDisplay);
    virtual ~IDTDisplayHAL() = default;

    virtual RC Setup() = 0;

    virtual RC OnMultiHeadsRunStart(const MultiHeadAssignments &multiHeadAssignments);

    virtual RC SetMode
    (
        const DisplayID& displayID,
        const ResolutionInfo& resolutionInfo,
        const vector<OverrideValues> &overrideVals,
        UINT32 head,
        UINT32 *pRefreshRate
    ) = 0;

    virtual RC OnMultiHeadsRunEnd(const MultiHeadAssignments &multiHeadAssignments);

    virtual RC Cleanup() = 0;

    virtual RC DisplayImage(const DisplayID& displayID, Surface2D *image, Surface2D *pRtImage) = 0;

protected:
    Display *m_pDisplay;
};

class IDTEvoDisplayHAL : public IDTDisplayHAL
{
public:
    IDTEvoDisplayHAL(Display *display);

    RC Setup() override;
    RC OnMultiHeadsRunStart(const MultiHeadAssignments &multiHeadAssignments) override;
    RC SetMode
    (
        const DisplayID& displayID,
        const ResolutionInfo& resolutionInfo,
        const vector<OverrideValues> &overrideVals,
        UINT32 head,
        UINT32 *pRefreshRate
    ) override;
    RC Cleanup() override;
    RC DisplayImage(const DisplayID& displayID, Surface2D *image, Surface2D *pRtImage) override;
private:
    DisplayID m_OriginalDisplayID;
};

class IDTLwDisplayHAL : public IDTDisplayHAL
{
public:
    IDTLwDisplayHAL(Tee::Priority printPriority, Display *display);

    RC SetModeOverride(void *pCbArgs);
    RC Setup() override;
    RC SetMode
    (
        const DisplayID& displayID,
        const ResolutionInfo& resolutionInfo,
        const vector<OverrideValues> &overrideVals,
        UINT32 head,
        UINT32 *pRefreshRate
    ) override;
    RC OnMultiHeadsRunEnd(const MultiHeadAssignments &multiHeadAssignments) override;
    RC Cleanup() override;
    RC DisplayImage(const DisplayID& displayID, Surface2D *image, Surface2D *pRtImage) override;
    typedef map <string, const void*> CBArgs;

private:
    Tee::Priority m_PrintPriority = Tee::PriLow;
    LwDisplay *m_pLwDisplay;
    RC GetHeadSettings(HeadIndex headIndex, LwDispHeadSettings **pHeadSettings);
};

class IDTJsonParser
{
public:
    enum class JsonParseResult
    {
        ABSENT,
        VALID,
        INVALID
    };

    IDTJsonParser();

    RC Init(IDTPrivateImpl *idt, SurfaceFill *pSurfaceHandler);
    bool ValidateJson(const TestArguments& testArguments);

private:
    const char *const JSON_PARAM_IMAGE = "Image";
    const char *const JSON_PARAM_DISPLAYS = "Displays";
    const char *const JSON_PARAM_RESOLUTION = "Resolution";
    const char *const JSON_PARAM_DISPLAYS_EX = "DisplaysEx";
    const char *const JSON_VALUE_RESOLUTION_MAX = "max";
    const char *const JSON_VALUE_RESOLUTION_NATIVE = "native";

    IDTPrivateImpl *m_pIDT;
    SurfaceFill *m_pSurfaceHandler;

    bool IsResolutiolwalid(const Value& resolution, ResolutionInfo *resolutionInfo);
    bool ParseLwstomResolution(const Value& resolution, ResolutionInfo *resolutionInfo);

    JsonParseResult ParseDisplaysParameter(const DisplaysArg& displays);
    bool AreDisplayIDsValid(const Value& displays);
    bool IsDisplayIDValid(const Value& displayID, bool *isDisplayIDAll, bool addEntry);

    JsonParseResult ParseResolutionParameter(const ResolutionArg& resolution);
    bool PrepareValidJsonForResolution(const ResolutionArg& resolution, Value *resolutionJsolwal);

    JsonParseResult ParseDisplaysExParameter(const DisplaysExArg& displaysEx);
    bool IsDisplayResolutionPairsArrayValid(const Value& displaysEx);
    bool IsDisplayResolutionPairValid(const Value& displayResolutionPair, bool *isDisplayIDAll);
};

#endif /* IDT_PRIVATE_IMPL_H_ */
