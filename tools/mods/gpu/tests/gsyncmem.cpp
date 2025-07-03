/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// GsyncMemTest : Test to excercise Gsync hardware on DP panels

#include "core/include/tee.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gputest.h"
#include "gputestc.h"
#include "core/include/golden.h"
#include "core/include/golddb.h"
#include "core/include/jscript.h"
#include "core/include/utility.h"
#include "core/include/massert.h"
#include "core/include/display.h"
#include "gpu/display/lwdisplay/lw_disp_utils.h"
#include "gpu/utility/surffill.h"
#include "ctrl/ctrl0073.h"
#include "core/include/tasker.h"
#include "core/include/modsedid.h"
#include "core/include/xp.h"
#include "gpu/include/gsyncdev.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "gpu/display/evo_cctx.h"
#include "gpu/include/gsyncdisp.h"

#include <set>

typedef Display::ORColorSpace COLOR_SPACE;
// GsyncMemTest Class
//
// Test a Gsync enabled DP display by displaying various test patterns and then
// reading and checking CRCs from the Gsync device against golden values
//
class GsyncMemTest : public GpuTest
{
public:
    GsyncMemTest();

    bool IsSupported();
    RC Setup();
    RC Run();
    RC Cleanup();
    RC InitFromJs();
    void PrintJsProperties(Tee::Priority pri);

    SETGET_PROP(NumFrameCrcs, UINT32);
    SETGET_PROP(GsyncDebug, UINT32);
    SETGET_PROP(GsyncCrcIdlePollHz, UINT32);
    SETGET_PROP(DpAuxRetryDelayMs, UINT32);
    SETGET_PROP(DpAuxRetries, UINT32);
    SETGET_PROP(SelfGild, bool);
    SETGET_PROP(GsyncImageSettleFrames, UINT32);
    SETGET_PROP(GsyncModesetSettleFrames, UINT32);
    SETGET_PROP(GsyncCrcIdleTimeoutMs, UINT32);
    SETGET_PROP(GsyncTempLoopInterval, UINT32);
    SETGET_PROP(UseRandomImages, bool);
    SETGET_PROP(RandomImageCount, UINT32);
    SETGET_PROP(AllowedCRCMiscompares, UINT32);
    SETGET_PROP(NumRRQueryRetries, UINT32);
    SETGET_PROP(DisplayWidth, UINT32);
    SETGET_PROP(DisplayHeight, UINT32);
    SETGET_PROP(DisplayDepth, UINT32);
    SETGET_PROP(RefreshRate, UINT32);
    SETGET_PROP_LWSTOM(ORPixelDepth, UINT32);
    SETGET_PROP_LWSTOM(ColorSpace, UINT32);
    SETGET_PROP(FlipAlternateFrames, bool);
    SETGET_PROP(DumpImages, bool);
    SETGET_PROP(BypassCRC, bool);
    SETGET_PROP(VcpMode, bool);
    SETGET_PROP(VcpFile, string);
    SETGET_PROP(GsyncDisplayId, UINT32);
    SETGET_PROP(EnableDMA, bool);
    SETGET_PROP(UseMemoryFillFns, bool);

private:
    RC WaitPattern(UINT32 pattern);
    RC ProcessCrcsOfLwrrentLoop(GsyncDevice *pGsyncDev, UINT32 pattern,
            set<UINT32> *pPatternsToCRC);
    RC VerifyCrcs(GsyncDevice *pGsyncDev);
    RC SetMode();
    RC EvoDispSetMode();
    RC LwDispSetMode();
    RC SetupSurfaceFill();
    RC SetupRandomDataSurfaces(const Display::Mode &surfaceRes);
    RC SetupImageDataSurfaces(const Display::Mode &surfaceRes);
    RC SetupRegularDataSurfaces(const Display::Mode &surfaceRes);
    RC SetupVcpMode();
    RC ParseVcpFile();
    RC RulwcpCommandsList();
    RC RulwcpCommand(const vector<string> &vcpCommand);
    RC ValidateInputVcpCommand(const vector<string> &vcpCommand, UINT32 numParams);
    RC DumpSurfaceInPng
    (
        UINT32 frame,
        UINT32 loop,
        string suffix,
        SurfaceUtils::SurfaceFill::SurfaceId surfaceId
    );

    GpuTestConfiguration *  m_pTestConfig;  //!< Pointer to test configuration
    GpuGoldenSurfaces *     m_pGGSurfs;     //!< Pointer to Gpu golden surfaces
    Goldelwalues *          m_pGolden;      //!< Pointer to golden values
    FancyPicker::FpContext *m_pFpCtx;       //!< FP context for looping
    UINT32                  m_OrigDisplays; //!< Originally connected display
    Display *               m_pDisplay;     //!< Pointer to real display
    vector<GsyncDevice>     m_GsyncDevices;
    GsyncDevice *           m_pSelectedGsyncDev //!< Selected Gsyncdevice to run specific
                                 = nullptr;     //!< vcp commands in VcpMode 
    vector <string> m_VcpCommandsList;

    //! Structure containing surfaces and the patterns they are filled with
    struct SurfaceData
    {
        SurfaceUtils::SurfaceFill::SurfaceId surfaceId; //!< Surface id of surface containing 
                                                        //!< the pattern
        vector<UINT32> pattern; //!< Pattern description
    };
    vector<SurfaceData>  m_SurfaceData; //!< Test surfaces and patterns
    std::unique_ptr<SurfaceUtils::SurfaceFill> m_pSurfaceFill; // Handler for filling surfaces with
                                                               // patterns

    UINT32 m_GoldenMiscompares; //!< To keep count of golden miscompares
    // Variables set from JS
    UINT32 m_NumFrameCrcs; //!< Number of frames to capture CRCs
    UINT32 m_GsyncDebug;     //!< Gsync Debug Level (0 = off, 1 = some, 2 = all)
    UINT32 m_GsyncCrcIdlePollHz; //!< Gsync CRC idle poll rate in Hz (default=20)
    UINT32 m_DpAuxRetries;//!< Number of retries for DP AUX transactions
                          //!< (default = 3)
    UINT32 m_DpAuxRetryDelayMs; //!< Delay time in ms between DP AUX retries
                                //!< (default=10)
    bool   m_SelfGild;    //!< Self gild mode (SU CRCs match PPU CRCs)
    UINT32 m_GsyncImageSettleFrames; //!< Number of frames to wait after image
                                     //!< changes for the Gsync CRC to settle
                                     //!< (default = 2)
    UINT32 m_GsyncModesetSettleFrames; //!< Number of frames to wait after mode
                                       //!< changes for the Gsync CRC to settle
                                       //!< (default = 10)
    UINT32 m_GsyncCrcIdleTimeoutMs;    //!< Timeout in ms for polling Gsync CRC
                                       //!< idle (default = 5000)
    UINT32 m_GsyncTempLoopInterval;     //!< Number of loops in between dumps
                                       //!< of the Gsync silicon die temperature
                                       //!< (default = 0 (disabled))
    bool   m_UseRandomImages;          //!< True to use random images instead of
                                       //!< test patterns
    UINT32 m_RandomImageCount = 4;     //!< Number of different random images to
                                       //!< use (when enabled)

    bool m_FlipAlternateFrames = false; //!< if true then S<i+1>.Pixel[j] = ~(S<i>.Pixel[j])
                                       //!< Number of images = 2*TestPatterns or 2*RandomImageCount
    UINT32 m_AllowedCRCMiscompares;
    UINT32 m_NumRRQueryRetries;
    UINT32 m_DisplayWidth = 0;
    UINT32 m_DisplayHeight = 0;
    UINT32 m_DisplayDepth = 0;
    UINT32 m_RefreshRate = 0;
    LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP m_ORPixelDepth = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_DEFAULT;
    map <UINT32, LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP> m_ORPixelDepthBackup;
    COLOR_SPACE m_ColorSpace = COLOR_SPACE::RGB;
    static const UINT32 s_DefaultPatterns[][4];
    bool m_DumpImages = false; //If true dump surface contents after setting up.
    bool m_BypassCRC = false; //If true dump ignore CRC mechanism for GSYNC module.

    bool m_VcpMode = false; //!< If true, run the test in a separate mode in which we 
                                          //!< can send a set of vcp commands to the gsync device

    string m_VcpFile = "";           //!< Input file containing a list of vcp commands
                                          //!< Sample format of supported vcp commands
                                          //!< w <op_code> <bytes to be written>
                                          //!< r <op_code

    DisplayID m_GsyncDisplayId = Display::NULL_DISPLAY_ID; //!< DisplayMask of the selected Gsync  
                                                           //!< device to run tests exclusively on
                                                           //!< that display 
    vector <string> m_ImageFilesArray; // Input *.png files to be used for testing.
    bool   m_EnableDMA; // Use DmaWrapper to fill window surfaces
    bool   m_UseMemoryFillFns; // Use the memoryFill fns from memory api directly
};

//! Test patterns specifically designed to stress Gsync
const UINT32 GsyncMemTest::s_DefaultPatterns[][4] =
{
    { 0xFF55A995, 0xFFAA566A, 0xFF55A995, 0xFFAA566A },
    { 0xFF565656, 0xFFA9A9A9, 0xFF565656, 0xFFA9A9A9 },
    { 0xFFAAAAAA, 0xFFAAAAAA, 0xFF555555, 0xFF555555 },
    { 0xFFFFFFFF, 0xFFFFFFFF, 0xFF000000, 0xFF000000 },
};

//------------------------------------------------------------------------------
// Method implementation
//------------------------------------------------------------------------------

//----------------------------------------------------------------------------
//! \brief Constructor
//!
GsyncMemTest::GsyncMemTest()
 : m_pGGSurfs(nullptr)
  ,m_OrigDisplays(0)
  ,m_pDisplay(nullptr)
  ,m_pSurfaceFill(nullptr)
  ,m_GoldenMiscompares(0)
  ,m_NumFrameCrcs(10)
  ,m_GsyncDebug(false)
  ,m_GsyncCrcIdlePollHz(20)
  ,m_DpAuxRetries(3)
  ,m_DpAuxRetryDelayMs(10)
  ,m_SelfGild(true)
  ,m_GsyncImageSettleFrames(2)
  ,m_GsyncModesetSettleFrames(10)
  ,m_GsyncCrcIdleTimeoutMs(5000)
  ,m_GsyncTempLoopInterval(0)
  ,m_UseRandomImages(true)
  ,m_AllowedCRCMiscompares(0)
  ,m_NumRRQueryRetries(1000)
  ,m_EnableDMA(true)
  ,m_UseMemoryFillFns(true)
{
    SetName("GsyncMem");
    m_pTestConfig = GetTestConfiguration();
    m_pGolden     = GetGoldelwalues();
    m_pFpCtx      = GetFpContext();
}

UINT32 GsyncMemTest::GetORPixelDepth() const
{
    return m_ORPixelDepth;
}

RC GsyncMemTest::SetORPixelDepth(UINT32 depth)
{
    if (depth > LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_48_444)
    {
        Printf(Tee::PriWarn, "OrPixel Depth will not be overridden because %u is out of range.\n",
                depth);
        m_ORPixelDepth = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_DEFAULT;
    }
    else
    {
        m_ORPixelDepth = static_cast<LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP>(depth);
    }
    return OK;
}

UINT32 GsyncMemTest::GetColorSpace() const
{
    return static_cast<UINT32>(m_ColorSpace);
}

RC GsyncMemTest::SetColorSpace(UINT32 colorId)
{
    if (colorId > static_cast<UINT32>(COLOR_SPACE::YUV_2020))
    {
        Printf(Tee::PriWarn, "Colorspace and ProcampSettings will not be overridden because id %u"
                " is bigger than YUV_2020\n", colorId);
    }
    else
    {
        m_ColorSpace = static_cast<COLOR_SPACE>(colorId);
    }
    return OK;
}

//----------------------------------------------------------------------------
//! \brief Returns true if Gsync test is supported
//!
bool GsyncMemTest::IsSupported()
{
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(Tee::PriLow, "Skipping GsyncMemTest on SOC GPU\n");
        return false;
    }

    return true;
}

RC GsyncMemTest::DumpSurfaceInPng(UINT32 frame,
        UINT32 loop,
        string suffix,
        SurfaceUtils::SurfaceFill::SurfaceId surfaceId)
{
    RC rc;
    string imgName = Utility::StrPrintf("F_%u.L_%u.%s.png", frame, loop, suffix.c_str());
    CHECK_RC(m_pSurfaceFill->GetSurface(surfaceId)->WritePng(imgName.c_str()));
    return rc;
}

RC GsyncMemTest::SetupSurfaceFill()
{
    RC rc;
    MASSERT(m_pTestConfig && m_pDisplay);
    if (m_EnableDMA)
    {
        // UseMemoryFillFns = true by default
        m_pSurfaceFill = make_unique<SurfaceUtils::SurfaceFillDMA>(m_UseMemoryFillFns);
        Printf(GetVerbosePrintPri(), "GsyncMem test will use DMA for displaying the image\n");
    }
    else
    {
        // UseMemoryFillFns = true by default
        m_pSurfaceFill =  make_unique<SurfaceUtils::SurfaceFillFB>(m_UseMemoryFillFns);
        Printf(GetVerbosePrintPri(), "GsyncMem test will not use DMA for displaying the"
                " image\n");
    }
    CHECK_RC(m_pSurfaceFill->Setup(this, m_pTestConfig));
    return rc;
}

RC GsyncMemTest::SetupRandomDataSurfaces(const Display::Mode &surfaceRes)
{
    RC rc;
    MASSERT(m_UseRandomImages);
    ColorUtils::Format format = ColorUtils::A8B8G8R8;
    // TODO: Supporting pitch by default now, later BlockLinear can be added too
    m_pSurfaceFill->SetLayout(Surface2D::Pitch);
    for (size_t surfIdx = 0; surfIdx < m_SurfaceData.size();)
    {
        //Number of test surface created = m_RandomImageCount(4 by default)
        SurfaceUtils::SurfaceFill::SurfaceId surfaceId =
            static_cast<SurfaceUtils::SurfaceFill::SurfaceId>(surfIdx);
        CHECK_RC(m_pSurfaceFill->PrepareSurface(surfaceId, surfaceRes, format));
        m_SurfaceData[surfIdx].surfaceId = surfaceId;
        CHECK_RC(m_pSurfaceFill->FillPatternRect(surfaceId,
                   "random",
                   nullptr,
                   nullptr));
        CHECK_RC(m_pSurfaceFill->CopySurface(surfaceId));
        surfIdx++;
        if (m_FlipAlternateFrames)
        {
            SurfaceUtils::SurfaceFill::SurfaceId flipSurfaceId =
                static_cast<SurfaceUtils::SurfaceFill::SurfaceId>(surfIdx);
            CHECK_RC(m_pSurfaceFill->PrepareSurface(flipSurfaceId, surfaceRes, format));
            m_SurfaceData[surfIdx].surfaceId = surfaceId;
            // Flipping the RGB components of the same surface created above.
            CHECK_RC(m_pSurfaceFill->FillPatternLwstom(surfaceId,
                       flipSurfaceId,
                       "flipRGB"));
            // copying sysmem surface of previous surfaceId after flipping RGB components to fb 
            // surface of current surfaceId(i.e flipSurfaceId)
            CHECK_RC(m_pSurfaceFill->CopySurface(flipSurfaceId));
            surfIdx++;
        }
    }
    return rc;
}

RC GsyncMemTest::SetupImageDataSurfaces(const Display::Mode &surfaceRes)
{
    RC rc;
    MASSERT(!m_ImageFilesArray.empty());
    ColorUtils::Format format = ColorUtils::A8B8G8R8;
    // TODO: Supporting pitch by default now, later BlockLinear can be added too
    m_pSurfaceFill->SetLayout(Surface2D::Pitch);
    // We will not flip image if this test mode is used.
    for (size_t surfIdx = 0; surfIdx < m_ImageFilesArray.size(); surfIdx++)
    {
        SurfaceUtils::SurfaceFill::SurfaceId surfaceId =
            static_cast<SurfaceUtils::SurfaceFill::SurfaceId>(surfIdx);
        CHECK_RC(m_pSurfaceFill->PrepareSurface(surfaceId, surfaceRes, format));
        m_SurfaceData[surfIdx].surfaceId = surfaceId;
        CHECK_RC(LwDispUtils::FillPatternByType(m_pSurfaceFill.get(),
                    m_ImageFilesArray[surfIdx].c_str(),
                    surfaceId));

        CHECK_RC(m_pSurfaceFill->CopySurface(surfaceId));
    }

    return rc;
}

RC GsyncMemTest::SetupRegularDataSurfaces(const Display::Mode &surfaceRes)
{
    RC rc;
    ColorUtils::Format format = ColorUtils::A8B8G8R8;
    // TODO: Supporting pitch by default now, later BlockLinear can be added too
    m_pSurfaceFill->SetLayout(Surface2D::Pitch);
    for (size_t surfIdx = 0; surfIdx < m_SurfaceData.size(); surfIdx++)
    {
        //Number of surfaces created = num_patterns in "TestPatterns" * 2 if Flipping is enabled
        SurfaceUtils::SurfaceFill::SurfaceId surfaceId =
            static_cast<SurfaceUtils::SurfaceFill::SurfaceId>(surfIdx);
        CHECK_RC(m_pSurfaceFill->PrepareSurface(surfaceId, surfaceRes, format));
        m_SurfaceData[surfIdx].surfaceId = surfaceId;
        // Fill surface with user defined pattern data
        CHECK_RC(m_pSurfaceFill->FillPattern(surfaceId,
                    m_SurfaceData[surfIdx].pattern));
        CHECK_RC(m_pSurfaceFill->CopySurface(surfaceId));
    }
    return rc;
}

//----------------------------------------------------------------------------
//
RC GsyncMemTest::SetupVcpMode()
{
    RC rc;

    if (m_GsyncDisplayId == Display::NULL_DISPLAY_ID)
    {
        Printf(Tee::PriError, "Selected Gsync device: 0x%x for VcpMode "
                "not specified!!!", m_GsyncDisplayId.Get());
        return RC::ILWALID_DISPLAY_MASK;
    }

    for (size_t gsyncIdx = 0; gsyncIdx < m_GsyncDevices.size(); gsyncIdx++)
    {
        if (m_GsyncDevices[gsyncIdx].GetDisplayMask() == m_GsyncDisplayId)
        {
            m_pSelectedGsyncDev = &m_GsyncDevices[gsyncIdx];
            break;
        }
    }

    if (m_pSelectedGsyncDev == nullptr)
    {
        Printf(Tee::PriError, "Selected Gsync device: 0x%x for VcpMode"
                "not found!!!", m_GsyncDisplayId.Get());
        // Selected device was not found because the value of m_GsyncDisplayId might be
        // invalid
        return RC::ILWALID_DISPLAY_MASK;
    }

    // Parse the vcp input file to fetch vcp commands
    CHECK_RC(ParseVcpFile()); 
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Setup the GsyncMemTest
//!
//! \return OK if setup was successful, not OK otherwise
//!
RC GsyncMemTest::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    // Reserve a Display object, disable UI, set default graphics mode.
    CHECK_RC(GpuTest::AllocDisplay());
    m_pDisplay = GetDisplay();
    m_OrigDisplays = m_pDisplay->Selected();

    m_GsyncDevices.clear();
    // In VcpMode, GsynDisplayId is an essential parameter but in case of generic test 189
    // we can use it to filter out gsyncdevices so that test runs only on the selected device.
    CHECK_RC(GsyncDevice::FindGsyncDevices(m_pDisplay,
                                           GetBoundGpuSubdevice(),
                                           m_GsyncDisplayId,
                                           &m_GsyncDevices));

    if (m_GsyncDevices.size() == 0)
    {
        Printf(Tee::PriError,
               "No Gsync capable display found on any connected display!\n");
        return RC::EXT_DEV_NOT_FOUND;
    }
    
    if (m_VcpMode)
    {
        CHECK_RC(SetupVcpMode());

        // Return from here and skip the remaining setup for generic test 189 mode
        return rc;
    }

    // Setup for the generic test 189 mode (Gsyncmem test)
    m_pDisplay->GetPreferredORPixelDepthMap(&m_ORPixelDepthBackup);

    m_DisplayWidth = (m_DisplayWidth == 0) ? m_pTestConfig->DisplayWidth(): m_DisplayWidth;
    m_DisplayHeight = (m_DisplayHeight == 0) ? m_pTestConfig->DisplayHeight(): m_DisplayHeight;
    m_RefreshRate = (m_RefreshRate == 0) ? m_pTestConfig->RefreshRate(): m_RefreshRate;
    m_DisplayDepth = (m_DisplayDepth == 0) ? m_pTestConfig->DisplayDepth(): m_DisplayDepth;
    const Display::Mode surfaceRes(m_DisplayWidth,
                                   m_DisplayHeight,
                                   m_DisplayDepth,
                                   m_RefreshRate);

    CHECK_RC(SetupSurfaceFill());
    // Using the abstraction of surfaceFill api, Copy patterns to sysmem surf2d.
    // From sysmem copy surface to Fb using dma. 
    // In case of Random surfaces if FlipSurface is needed, then we add an additional
    // frame with flipped RGB values too.

    if (m_UseRandomImages)
    {
        CHECK_RC(SetupRandomDataSurfaces(surfaceRes));
    }
    else if (!m_ImageFilesArray.empty())
    {
        CHECK_RC(SetupImageDataSurfaces(surfaceRes));
    }
    else
    {
        CHECK_RC(SetupRegularDataSurfaces(surfaceRes));
    }

    // Test captures CRCs for same number of frames across all Gsync devices. Hence
    // need to check if requested number of frames can be captured on all detected
    // Gsync devices

    if (m_BypassCRC)
    {
        m_NumFrameCrcs = 0;
    }
    else
    {
        UINT32 maxAllowedCrcs = ~0;
        for (auto gsyncDevice : m_GsyncDevices)
        {
            if (gsyncDevice.GetMaxCrcFrames(GsyncDevice::CRC_PRE_AND_POST_MEM) < maxAllowedCrcs)
            {
                maxAllowedCrcs = gsyncDevice.GetMaxCrcFrames(GsyncDevice::CRC_PRE_AND_POST_MEM);
            }
        }
        if ((m_NumFrameCrcs == 0) || (m_NumFrameCrcs > maxAllowedCrcs))
        {
            UINT32 defaultNumFrameCrcs = (maxAllowedCrcs > 10) ? 10: maxAllowedCrcs;
            Printf(Tee::PriNormal,
                    "%s : NumFrameCrcs (%u) out of range (1, %u), defaulting to %u\n",
                    GetName().c_str(), m_NumFrameCrcs, maxAllowedCrcs, defaultNumFrameCrcs);
            m_NumFrameCrcs = defaultNumFrameCrcs;
        }

        if (!m_SelfGild)
            m_pGGSurfs = new GpuGoldenSurfaces(GetBoundGpuDevice());
    }

    return rc;
}

//----------------------------------------------------------------------------
RC GsyncMemTest::SetMode()
{
    RC rc;
    CHECK_RC(GsyncDisplayHelper::SetMode(m_GsyncDevices,
                 m_DisplayWidth,
                 m_DisplayHeight,
                 m_RefreshRate,
                 m_DisplayDepth,
                 m_ColorSpace));   
    return rc;
}

//----------------------------------------------------------------------------
//! \brief function to parse Vcp Commands file
//!
RC GsyncMemTest::ParseVcpFile()
{
    RC rc;

    // Right now we are exclusively parsing vcp commands from an input file
    if (!Xp::DoesFileExist(m_VcpFile))
    {
        Printf(Tee::PriError, "VCP commands input file %s was not found !!!\n", 
                m_VcpFile.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }
    string vcpCommands;
    CHECK_RC(Xp::InteractiveFileRead(m_VcpFile.c_str(), &vcpCommands,
                GetVerbosePrintPri()));
    if (vcpCommands.empty())
    {
        Printf(Tee::PriError, "VCP commands input file was empty! Nothing to run!!!\n");
        return RC::BAD_FORMAT;
    }
    CHECK_RC(Utility::Tokenizer(vcpCommands, "\n", &m_VcpCommandsList));
    return rc;
}

//----------------------------------------------------------------------------
//! \brief function to parse list of vcp commands
//
RC GsyncMemTest::RulwcpCommandsList()
{
    RC rc;
    for (size_t i = 0; i < m_VcpCommandsList.size(); i++)
    {
        vector <string> vcpCommand;
        // Assuming that the vcp command params are separated by single spaces
        CHECK_RC(Utility::Tokenizer(m_VcpCommandsList[i], " ", &vcpCommand));
        CHECK_RC(RulwcpCommand(vcpCommand));
    }
    return rc;
}

//------------------------------------------------------------------------------
//
RC GsyncMemTest::ValidateInputVcpCommand(const vector<string> &vcpCommand, UINT32 numParams)
{
    RC rc;
    MASSERT(numParams);
    if (vcpCommand.size() != numParams)
    {
        Printf(Tee::PriError, "Unsupported format for the vcp Command: \"%s\"\n"
                 "Supported Command Formats\n"
                 "----------------------------------------------\n"
                 "1. Generic write: w <op_code> <bytes to be written> (eg: w e0 4e 56)\n"
                 "2. Generic read: r <op_code> (eg: r fa)\n",
                vcpCommand[0].c_str());
        return RC::BAD_FORMAT;
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief run the individual vcp commands
//
RC GsyncMemTest::RulwcpCommand(const vector<string> &vcpCommand)
{
    RC rc;
    UINT32 numParams;

    // For now we are supporting only generic vcp commands which aren't 'Gsync
    // Aware' i.e. commands are not gsync page aware nor they unlock the pages
    // automatically
    if (vcpCommand[0] == "w")
    {   
        numParams = 4; // Number of params in write command
                       // For eg: w e0 4e 56
        CHECK_RC(ValidateInputVcpCommand(vcpCommand, numParams));
        GsyncDevice::VCPFeatureData vcpData = { 0 };
        UINT08 vcpOpcode = Utility::ColwertStrToHex(vcpCommand[1]);
        vcpData.sh = Utility::ColwertStrToHex(vcpCommand[2]);
        vcpData.sl = Utility::ColwertStrToHex(vcpCommand[3]);
        Printf(Tee::PriNormal, "Setting VCP feature with opcode = %2x, "
            "vcpData = { %2x, %2x }\n",
            vcpOpcode, vcpData.sh, vcpData.sl);
        CHECK_RC(m_pSelectedGsyncDev->SetVCPFeature(vcpOpcode, vcpData));
    }
    else if (vcpCommand[0] == "r")
    {
        numParams = 2; // Number of params in read command
                       // For eg: r fa
        CHECK_RC(ValidateInputVcpCommand(vcpCommand, numParams));
        GsyncDevice::VCPFeatureData vcpData = { 0 };
        UINT08 vcpOpcode = Utility::ColwertStrToHex(vcpCommand[1]);
        CHECK_RC(m_pSelectedGsyncDev->GetVCPFeature(vcpOpcode, &vcpData, Tee::PriNormal));
        Printf(Tee::PriNormal, "vcp opcode = %2x\nMH= %2x ML= %2x SH= %2x SL= %2x\n"
            "M= %2x S= %2x\nMS= %2x\n",
            vcpOpcode, vcpData.mh, vcpData.ml, vcpData.sh, vcpData.sl, 
            (vcpData.mh << 8) + vcpData.ml,
            (vcpData.sh << 8) + vcpData.sl,
            (vcpData.mh << 24) + (vcpData.ml << 16) + (vcpData.sh << 8) + vcpData.sl);
    }       
    else
    {
        Printf(Tee::PriError, "Unsupported format for the vcp Command!\n"
                 "Supported Command Formats\n"
                 "----------------------------------------------\n"
                 "1. Generic write: w <op_code> <bytes to be written> (eg: w e0 4e 56)\n"
                 "2. Generic read: r <op_code> (eg: r fa)\n");
        return RC::BAD_FORMAT;
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Run the GsyncMemTest
//!
//! \return OK if running was successful, not OK otherwise
//!
RC GsyncMemTest::Run()
{
    StickyRC rc;

    if (m_VcpMode)
    {
        CHECK_RC(RulwcpCommandsList());   

        // Return from here as we don't need to run generic test 189 in VcpMode
        return rc;
    }

    // When generating goldens it is only necessary to generate goldens once per
    // pattern (rather than using loop number to store CRCs in the database,
    // pattern number is used instead)
    set<UINT32> patternsToCrc;
    if (m_pGolden->GetAction() == Goldelwalues::Store)
    {
        for (size_t ii = 0; ii < m_SurfaceData.size(); ii++)
        {
            patternsToCrc.insert(static_cast<UINT32>(ii));
        }
    }

    // Set to false here so multiple calls to Run are possible
    m_GoldenMiscompares = 0;
    for (size_t gsyncIdx = 0; gsyncIdx < m_GsyncDevices.size(); gsyncIdx++)
    {
        CHECK_RC(rc);
        DEFER { rc = GsyncDisplayHelper::DetachAllDisplays(m_GsyncDevices); };
        const UINT32 displayMask = m_GsyncDevices[gsyncIdx].GetDisplayMask();
        Printf(GetVerbosePrintPri(), "Testing Gsync display 0x%08x Gsync Device %u\n",
               displayMask, static_cast <UINT32>(gsyncIdx));

        GsyncDevice::DebugLevel gsyncDbgLev =
            static_cast<GsyncDevice::DebugLevel>(m_GsyncDebug);
        if (m_pTestConfig->Verbose() && (m_GsyncDebug == GsyncDevice::DBG_NONE))
            gsyncDbgLev = GsyncDevice::DBG_SOME;

        m_GsyncDevices[gsyncIdx].SetDebugLevel(gsyncDbgLev);
        m_GsyncDevices[gsyncIdx].SetCrcIdlePollHz(m_GsyncCrcIdlePollHz);
        m_GsyncDevices[gsyncIdx].SetCrcIdleTimeout(m_GsyncCrcIdleTimeoutMs);
        m_GsyncDevices[gsyncIdx].SetDpAuxRetries(m_DpAuxRetries);
        m_GsyncDevices[gsyncIdx].SetDpAuxRetryDelayMs(m_DpAuxRetryDelayMs);
        m_GsyncDevices[gsyncIdx].SetImageChangeSettleFrames(m_GsyncImageSettleFrames);
        m_GsyncDevices[gsyncIdx].SetModesetSettleFrames(m_GsyncModesetSettleFrames);

        if (!m_SelfGild && !m_BypassCRC)
        {
            m_GsyncDevices[gsyncIdx].SetNumCrcFrames(m_NumFrameCrcs);
            m_pGolden->ClearSurfaces();
            m_pGGSurfs->AttachSurface(m_pSurfaceFill->GetSurface(m_SurfaceData[0].surfaceId),
                                      "C",
                                      displayMask,
                                      &m_GsyncDevices[gsyncIdx]);
            CHECK_RC(m_pGolden->SetSurfaces(m_pGGSurfs));
        }
        // Override ORPixelDepth if it is not equal to default value
        if (m_ORPixelDepth != LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_DEFAULT)
        {
            UINT32 dispId = static_cast<UINT32>(m_GsyncDevices[gsyncIdx].GetDisplayID());
            CHECK_RC(m_GsyncDevices[gsyncIdx].
                GetPDisplay()->SetPreferredORPixelDepth(dispId,
                static_cast<LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP>(m_ORPixelDepth)));
        }
        CHECK_RC(SetMode());
        Printf(Tee::PriDebug, "device[%u] display 0x%x SetMode done.\n",
                static_cast<UINT32>(gsyncIdx), displayMask);

        CHECK_RC(m_GsyncDevices[gsyncIdx].FactoryReset());
        Printf(Tee::PriDebug, "Factory Reset GsyncDev %u done.\n",
                static_cast<UINT32>(gsyncIdx));
        CHECK_RC(m_GsyncDevices[gsyncIdx].NotifyModeset());
        Printf(Tee::PriDebug, "NotifyModeset to GsyncDev %u is done.\n",
                static_cast<UINT32>(gsyncIdx));

        UINT32 refRate = 0;
        UINT32 numRetriesLeft = m_NumRRQueryRetries;
        while (refRate == 0 && numRetriesLeft != 0)
        {
            CHECK_RC(m_GsyncDevices[gsyncIdx].GetRefreshRate(&refRate));
            --numRetriesLeft;
        }
        if (refRate == 0)
        {
            Printf(Tee::PriError, "Gsync display 0x%08x disconnected!\n", displayMask);
            return RC::HW_STATUS_ERROR;
        }
        if (refRate > (m_RefreshRate + 1) ||
                (refRate < (m_RefreshRate - 1)))
        {
            Printf(Tee::PriError,
                    "Gsync display 0x%08x Refresh Rate is %dHz. Could not be set to %dHz\n",
                   displayMask, refRate, m_RefreshRate);
            return RC::HW_STATUS_ERROR;
        }

        UINT32 StartLoop = m_pTestConfig->StartLoop();
        UINT32 EndLoop   = m_pTestConfig->Loops();
        UINT32 loopsRun  = 0;
        for (m_pFpCtx->LoopNum = StartLoop; m_pFpCtx->LoopNum < EndLoop;
             ++m_pFpCtx->LoopNum, ++loopsRun)
        {
            DEFER
            {
                if (rc != OK)
                {
                   Printf(Tee::PriError,
                           " Test detected failure in Innerloop %u\n", m_pFpCtx->LoopNum);
                }
            };
            UINT32 pattern  = m_pFpCtx->LoopNum % m_SurfaceData.size();
            Printf(GetVerbosePrintPri(), "Test Inner loop:patternIdx [%u : %u]\n",
                    m_pFpCtx->LoopNum,
                    pattern);

            if (m_GsyncTempLoopInterval &&
                ((loopsRun % m_GsyncTempLoopInterval) == 0))
            {
                UINT32 tempC;
                CHECK_RC(m_GsyncDevices[gsyncIdx].GetTemperature(&tempC));
                Printf(Tee::PriNormal, "Gsync Silicon Temperature : %uC at loop %u\n",
                        tempC, loopsRun);
            }

            if (m_DumpImages)
            {

                string type = m_FlipAlternateFrames ?
                                pattern%2 == 1 ?
                                    "flipped" : "original"
                                : "generated";
                string method = m_UseRandomImages ? "Random" : "Fixed";
                string suffix = type + method;
                CHECK_RC(DumpSurfaceInPng(pattern, m_pFpCtx->LoopNum, suffix.c_str(),
                            m_SurfaceData[pattern].surfaceId));
            }
            CHECK_RC(GsyncDisplayHelper::DisplayImage(m_GsyncDevices[gsyncIdx],
                                                      m_pSurfaceFill->GetImage
                                                          (m_SurfaceData[pattern].surfaceId)));
            if (!m_SelfGild && !m_BypassCRC)
                m_pGGSurfs->ReplaceSurface(0, m_pSurfaceFill->GetSurface(m_SurfaceData[pattern].
                                                                            surfaceId));

            CHECK_RC(ProcessCrcsOfLwrrentLoop(&m_GsyncDevices[gsyncIdx], pattern,
                    &patternsToCrc));

        }
        if (!m_SelfGild && !m_BypassCRC)
        {
            // Golden errors that are deferred due to "-run_on_error" can only
            // be retrieved by running Golden::ErrorRateTest().  This must be
            // done before clearing surfaces
            CHECK_RC(m_pGolden->ErrorRateTest(GetJSObject()));
        }
    }
    if (!m_BypassCRC)
    {
        if (m_GoldenMiscompares > m_AllowedCRCMiscompares)
        {
            return RC::GOLDEN_VALUE_MISCOMPARE;
        }
        else if (m_GoldenMiscompares != 0)
        {
            Printf(Tee::PriNormal, "GsyncMemTest: %d CRC miscompares, %d allowed.\n",
                    m_GoldenMiscompares,
                    m_AllowedCRCMiscompares);
        }
    }
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Cleanup the GsyncMemTest
//!
//! \return OK if running was successful, not OK otherwise
//!
RC GsyncMemTest::Cleanup()
{
    StickyRC rc;

    if (m_pDisplay != nullptr)
    {
        m_pDisplay->SetPreferredORPixelDepthMap(&m_ORPixelDepthBackup);
        m_pDisplay = nullptr;
    }

    // Free golden buffer.
    if (m_pGGSurfs)
    {
        m_pGolden->ClearSurfaces();
        delete m_pGGSurfs;
        m_pGGSurfs = NULL;
    }

    // Deallocate surfaces.
    if (m_pSurfaceFill.get())
    {
        rc = m_pSurfaceFill->Cleanup();
    }

    m_SurfaceData.clear();
    m_ImageFilesArray.clear();
    m_GoldenMiscompares = 0;
    m_VcpCommandsList.clear();

    rc = GpuTest::Cleanup();
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Initialize test properties from JS
//!
//! \return OK if successful, not OK otherwise
//!
RC GsyncMemTest::InitFromJs()
{
    RC rc;

    // Call base-class function (for TestConfiguration and Golden properties).
    CHECK_RC(GpuTest::InitFromJs());

    // If using random images instead of patterns, just resize the surface data
    // and do not fill in the pattern
    if (m_UseRandomImages)
    {
        if (m_FlipAlternateFrames)
        {
            //Account for addtional frames with binary complement pixeldata
            m_RandomImageCount *= 2;
        }
        m_SurfaceData.resize(m_RandomImageCount);
        return rc;
    }

    // Initialize TestPatterns by getting the actual propery from JS
    // ratherthan by exporting a C++ JS property.  This is due to the nature of
    // how JS properties are copied onto the test object from test arguments
    // properties / arrays are copied individually rather than an entire
    // array assignment at once (what the C++ interface expects)
    JsArray jsaTestPatterns;
    JsArray jsImageFiles;
    JavaScriptPtr pJs;
    rc = pJs->GetProperty(GetJSObject(),
                          "TestPatterns",
                          &jsaTestPatterns);
    if (OK == rc)
    {
        for (size_t patIdx = 0; patIdx < jsaTestPatterns.size(); ++patIdx)
        {
            JsArray innerArray;
            rc = pJs->FromJsval(jsaTestPatterns[patIdx], &innerArray);
            if (OK == rc)
            {
                SurfaceData surfData;
                SurfaceData flippedSurfData;
                for (size_t pix = 0; (pix < innerArray.size()) && (OK == rc);
                      ++pix)
                {
                    UINT32 lwrPix;
                    rc = pJs->FromJsval(innerArray[pix], &lwrPix);
                    if (OK == rc)
                    {
                        // Pixels are specified with just RGB values, force
                        // Alpha to be opaque
                        surfData.pattern.push_back(lwrPix | 0xFF000000);
                        if (m_FlipAlternateFrames)
                        {
                            flippedSurfData.pattern.push_back(~lwrPix | 0xFF000000);
                        }
                    }
                    else
                    {
                        Printf(Tee::PriNormal,
                               "%s : Unable to get custom pattern pixel\n",
                               GetName().c_str());
                    }
                }
                if (OK == rc)
                {
                    m_SurfaceData.push_back(surfData);
                    if (m_FlipAlternateFrames)
                    {
                        m_SurfaceData.push_back(flippedSurfData);
                    }
                }
            }
            else
            {
                Printf(Tee::PriNormal,
                       "%s : Unable to get custom pattern\n",
                       GetName().c_str());
            }
        }
    }
    else if (OK == pJs->GetProperty(GetJSObject(), "ImageFilesArray", &jsImageFiles))
    {
        rc.Clear();
        m_ImageFilesArray.clear();
        for (size_t imgIdx = 0; imgIdx < jsImageFiles.size(); ++imgIdx)
        {
            string fPath;
            CHECK_RC(pJs->FromJsval(jsImageFiles[imgIdx], &fPath));
            m_ImageFilesArray.push_back(fPath);
        }
        m_SurfaceData.resize(m_ImageFilesArray.size());
        return rc;

    }
    else if (RC::ILWALID_OBJECT_PROPERTY == rc)
    {
        rc.Clear();
        // Use the static patterns that declared in test.

        const UINT32 numPatterns = sizeof(s_DefaultPatterns) /
            sizeof(s_DefaultPatterns[0]);
        const UINT32 numPixels = sizeof(s_DefaultPatterns[0]) / sizeof(UINT32);

        for (UINT32 lwrPtIdx = 0; lwrPtIdx < numPatterns; lwrPtIdx++)
        {
            SurfaceData surfData;
            SurfaceData flippedSurfData;
            for (UINT32 lwrPix = 0; lwrPix < numPixels; lwrPix++)
            {
                UINT32 pix = s_DefaultPatterns[lwrPtIdx][lwrPix];
                surfData.pattern.push_back(pix);
                if (m_FlipAlternateFrames)
                {
                    flippedSurfData.pattern.push_back((~pix) | 0xFF000000);
                }
            }
            m_SurfaceData.push_back(surfData);
            if (m_FlipAlternateFrames)
            {
                m_SurfaceData.push_back(flippedSurfData);
            }
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
//! \return OK if successful, not OK otherwise
void GsyncMemTest::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tDisplayWidth:\t\t\t%u\n", m_DisplayWidth);
    Printf(pri, "\tDisplayHeight:\t\t\t%u\n", m_DisplayHeight);
    Printf(pri, "\tDisplayDepth:\t\t\t%u\n", m_DisplayDepth);
    Printf(pri, "\tRefreshRate:\t\t\t%u\n", m_RefreshRate);
    Printf(pri, "\tORPixelDepth:\t\t\t%u\n", m_ORPixelDepth);
    Printf(pri, "\tColorSpace:\t\t\t%u\n", static_cast<UINT32>(m_ColorSpace));
    Printf(pri, "\tNumFrameCrcs:\t\t\t%u\n", m_NumFrameCrcs);
    Printf(pri, "\tSelfGild:\t\t\t%s\n", m_SelfGild ? "true" : "false");
    Printf(pri, "\tGsyncCrcIdlePollHz:\t\t%u\n", m_GsyncCrcIdlePollHz);
    Printf(pri, "\tDpAuxRetryDelayMs:\t\t%u\n", m_DpAuxRetryDelayMs);
    Printf(pri, "\tDpAuxRetries:\t\t\t%u\n", m_DpAuxRetries);
    Printf(pri, "\tGsyncModesetSettleFrames:\t%u\n", m_GsyncModesetSettleFrames);
    Printf(pri, "\tGsyncImageSettleFrames:\t\t%u\n", m_GsyncImageSettleFrames);
    Printf(pri, "\tGsyncCrcIdleTimeoutMs:\t\t%u\n", m_GsyncCrcIdleTimeoutMs);
    Printf(pri, "\tGsyncDebug:\t\t\t%u\n", m_GsyncDebug);
    Printf(pri, "\tUseRandomImages:\t\t%s\n", m_UseRandomImages ? "true" : "false");
    Printf(pri, "\tRandomImageCount:\t\t%u\n", m_RandomImageCount);
    Printf(pri, "\tAlternateFramesFlipped:\t\t%s\n", m_FlipAlternateFrames ? "true" : "false");
    Printf(pri, "\tDumpImages:\t\t%s\n", m_DumpImages ? "true" : "false");
    Printf(pri, "\tBypassCRC:\t\t%s\n", m_BypassCRC ? "true" : "false");
    Printf(pri, "\tVcpMode:\t\t%s\n", m_VcpMode ? "true" : "false");
    Printf(pri, "\tVcpFile:\t\t%s\n", m_VcpFile.c_str());
    Printf(pri, "\tGsyncDisplayId:\t\t%u\n", m_GsyncDisplayId.Get());
    Printf(pri, "\tEnableDMA:\t\t%s\n", m_EnableDMA ? "true" : "false");
    Printf(pri, "\tUseMemoryFillFns:\t\t%s\n", m_UseMemoryFillFns ? "true" : "false");
    if (!m_UseRandomImages)
    {
        Printf(pri, "\tTestPatterns:\t\t\t");
        for (size_t pat = 0; pat < m_SurfaceData.size(); pat++)
        {
            if (pat != 0)
            {
                Printf(pri, "\t\t\t\t\t");
            }
            Printf(pri, "[");
            for (size_t pix = 0; pix < m_SurfaceData[pat].pattern.size(); pix++)
            {
                // Patterns are specified with RGB values only so when printing
                // them mask out and do not print the alpha values
                Printf(pri, "0x%06x",
                       m_SurfaceData[pat].pattern[pix] & ~0xff000000);
                if (pix != (m_SurfaceData[pat].pattern.size() - 1))
                    Printf(pri, ",");
            }
            Printf(pri, "]\n");
        }
    }

    GpuTest::PrintJsProperties(pri);
}

RC GsyncMemTest::ProcessCrcsOfLwrrentLoop(GsyncDevice *pGsyncDev, UINT32 pattern,
        set <UINT32> *pPatternsToCrc)
{
    RC rc;
    if (m_BypassCRC)
    {
        return rc;
    }
    CHECK_RC(pGsyncDev->NotifyImageChange());
    if (m_SelfGild)
    {
        CHECK_RC(VerifyCrcs(pGsyncDev));
    }
    else
    {
        m_pGolden->SetLoop(pattern);
        if (OK != (rc = m_pGolden->Run()))
        {
            Printf(Tee::PriError, "Golden error in Inner loop %u (pattern %u)\n",
                    m_pFpCtx->LoopNum, pattern);
        }
        CHECK_RC(rc);

        // When generating goldens, exit once all patterns have
        // been CRCd
        if (m_pGolden->GetAction() == Goldelwalues::Store)
        {
            pPatternsToCrc->erase(pattern);
            if (pPatternsToCrc->empty())
                return rc;
        }
    }
    return rc;
}
//-----------------------------------------------------------------------------
//! \brief Verify the the SU (pre-memory) CRCs match the PPU (post-memory) CRCs
//!
//! \param display : display to verify CRCs on
//!
//! \return OK if CRC check passed, not OK otherwise
RC GsyncMemTest::VerifyCrcs(GsyncDevice *pGsyncDev)
{
    vector<GsyncDevice::CrcData> suCrcs;
    vector<GsyncDevice::CrcData> ppuCrcs;
    RC rc;

    CHECK_RC(pGsyncDev->GetCrcs(true, m_NumFrameCrcs, &suCrcs, &ppuCrcs));

    // An even frames worth of CRCs should have been retrieved
    MASSERT(suCrcs.size() == ppuCrcs.size());

    for (size_t frameIdx = 0; frameIdx < suCrcs.size(); frameIdx++)
    {
        GsyncDevice::CrcData &suFrameCrcs  = suCrcs[frameIdx];
        GsyncDevice::CrcData &ppuFrameCrcs = ppuCrcs[frameIdx];

        for (size_t crcIdx = 0; crcIdx < suFrameCrcs.size(); crcIdx++)
        {
            if (suFrameCrcs[crcIdx] != ppuFrameCrcs[crcIdx])
            {
                Printf(Tee::PriError,
                       "!!! %s Loop %d, Frame %u, CRC %u SU/PPU mismatch! "
                       "(SU CRC 0x%x, PPU CRC 0x%x)\n",
                       GetName().c_str(),
                       m_pFpCtx->LoopNum,
                       static_cast<UINT32>(frameIdx),
                       static_cast<UINT32>(crcIdx),
                       suFrameCrcs[crcIdx],
                       ppuFrameCrcs[crcIdx]);
                m_GoldenMiscompares++;
                if (m_pGolden->GetStopOnError() || m_GoldenMiscompares > m_AllowedCRCMiscompares)
                {
                    return RC::GOLDEN_VALUE_MISCOMPARE;
                }
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
// GsyncMemTest object, properties and methods.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(GsyncMemTest, GpuTest, "Gsync Test");
CLASS_PROP_READWRITE(GsyncMemTest, NumFrameCrcs, UINT32,
        "Number of frames to capture CRC for (default = 10)");
CLASS_PROP_READWRITE(GsyncMemTest, GsyncDebug, UINT32,
        "Debug Gsync communications (0 = off, 1 = some, 2 = all)");
CLASS_PROP_READWRITE(GsyncMemTest, GsyncCrcIdlePollHz, UINT32,
        "Polling rate in Hz for Gsync CRC idle (default = 100)");
CLASS_PROP_READWRITE(GsyncMemTest, DpAuxRetries, UINT32,
        "Number of retries for DP AUX transactions (default=3)");
CLASS_PROP_READWRITE(GsyncMemTest, DpAuxRetryDelayMs, UINT32,
        "Delay time in ms between DP AUX retries (default=10)");
CLASS_PROP_READWRITE(GsyncMemTest, SelfGild, bool,
        "Set to true to enable self gild mode (SU CRCs match PPU CRCs)");
CLASS_PROP_READWRITE(GsyncMemTest, GsyncImageSettleFrames, UINT32,
        "Number of frames to wait after image changes before CRC acquisition "
        "(default = 2)");
CLASS_PROP_READWRITE(GsyncMemTest, GsyncModesetSettleFrames, UINT32,
        "Number of frames to wait after modeset (default = 10)");
CLASS_PROP_READWRITE(GsyncMemTest, GsyncCrcIdleTimeoutMs, UINT32,
        "Timeout in ms for Gsync CRC idling (default = 5000)");
CLASS_PROP_READWRITE(GsyncMemTest, GsyncTempLoopInterval, UINT32,
        "Loop interval between Gsync silicon temperature readings, 0=disabled (default = 0)");
CLASS_PROP_READWRITE(GsyncMemTest, UseRandomImages, bool,
        "Use random images instead of predefined patterns (default = true)");
CLASS_PROP_READWRITE(GsyncMemTest, RandomImageCount, UINT32,
        "Number of different random images to use when UseRandomImages=true (default = 4)");
CLASS_PROP_READWRITE(GsyncMemTest, AllowedCRCMiscompares, UINT32,
        "For debugging purposes, allow a fixed number of CRC miscompares(default = 0)");
CLASS_PROP_READWRITE(GsyncMemTest, NumRRQueryRetries, UINT32,
        "Number of times we need to check whether SetMode has successfully attained required "
        "refresh rate. This can be increased if we expect that SetMode will take more time.");
CLASS_PROP_READWRITE(GsyncMemTest, DisplayWidth, UINT32, "Image width");
CLASS_PROP_READWRITE(GsyncMemTest, DisplayHeight, UINT32, "Image height");
CLASS_PROP_READWRITE(GsyncMemTest, DisplayDepth, UINT32, "Image depth");
CLASS_PROP_READWRITE(GsyncMemTest, RefreshRate, UINT32, "Set Mode refresh rate");
CLASS_PROP_READWRITE(GsyncMemTest, ORPixelDepth, UINT32, "Test OR pixel depth");
CLASS_PROP_READWRITE(GsyncMemTest, ColorSpace, UINT32,
        "Test Procamp settings Color Space. 0:RGB, 1:YUV601, 2:YUV709, 3:YUV2020");
CLASS_PROP_READWRITE(GsyncMemTest, FlipAlternateFrames, bool,
        "Odd ordered frame pixels will be binary complement of previous even numbered frame.");
CLASS_PROP_READWRITE(GsyncMemTest, DumpImages, bool,
        "Dump Images in F_<frameNum>.L_<LoopNum>.<Flipped/Original/Generated>.<Random/Fixed>.png");
CLASS_PROP_READWRITE(GsyncMemTest, BypassCRC, bool, "Ignore CRC checks and storage");
CLASS_PROP_READWRITE(GsyncMemTest, VcpMode, bool,
        "Run t189 in an alternate mode where we can send vcp commands read from a file.");
CLASS_PROP_READWRITE(GsyncMemTest, VcpFile, string,
        "While in Vcp Parser Mode, read vcp commands from this input file.");
CLASS_PROP_READWRITE(GsyncMemTest, GsyncDisplayId, UINT32,
        "Selected Gsync device to send vcp commands in VcpMode.");
CLASS_PROP_READWRITE(GsyncMemTest, EnableDMA, bool,
                "Use DmaWrapper to fill window surfaces");
CLASS_PROP_READWRITE(GsyncMemTest, UseMemoryFillFns, bool,
                "Use Memory fill functions to fill surfaces");

