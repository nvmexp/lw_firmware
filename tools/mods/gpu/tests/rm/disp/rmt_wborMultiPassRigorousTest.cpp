/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_wborMultiHeadRigorous.cpp
//! \brief This test covers major use cases for WBOR. Test has multiple passes,
//!        each pass testing a different combination of WBOR functionality
//!
//!        For details on what each pass does, please see //sw/docs/resman/components/display/v0208/VWB/dwb_rm_testplan.docx
//!        Section 5
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "gpu/include/gpudev.h"

// DTI related headers
#include "lwtiming.h"
#include "gpu/include/dispchan.h"
#include "gpu/display/evo_disp.h"
#include "gpu/display/evo_chns.h"
#include "gpu/display/modeset_utils.h"
#include "gpu/tests/rm/utility/crccomparison.h"

// Display class headers
#include "disp/v02_08/dev_disp.h"
#include "class/cl9878.h"
#include "ctrl/ctrl0073.h"

#include <sstream>

#include "core/include/memcheck.h"

class WborMultiPassRigorousTest : public RmTest
{
public:
    WborMultiPassRigorousTest();
    virtual ~WborMultiPassRigorousTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();

    virtual RC Cleanup();

    SETGET_PROP(runPass, UINT32);
    SETGET_PROP(dwbEmu, bool);

private:
    UINT32      m_runPass;
    bool        m_dwbEmu;

    Display*    m_pDisplay;
    EvoDisplay *m_pEvoDisp;

    DisplayIDs  m_detectedIDs;
    DisplayIDs  m_detectedWbors;
    DisplayIDs  m_detectedSors;

    UINT32      m_width;
    UINT32      m_height;
    UINT32      m_depth;
    UINT32      m_refreshRate;
    UINT32      m_lwrsSize;
    UINT32      m_ovlySize;

    vector<Surface2D>     m_vOutputSurface;
    vector<Surface2D>     m_vBaseImage;
    vector<Surface2D>     m_vCoreImageWbor;
    vector<Surface2D>     m_vCoreImageSor;
    vector<Surface2D>     m_vOvlyImage;
    vector<Surface2D>     m_vLwrsImage;
    vector<std::string>   m_vBaseFile;

    vector<UINT32>        m_vFrameId;

    // Setup the 3 base images we will use
    UINT32                m_baseImageWidth;
    UINT32                m_baseImageHeight;

    // Cursor and Overlay image
    string                m_lwrsFile;
    string                m_ovlyFile;
    UINT32                m_lwrImageSize;
    UINT32                m_ovlyImageSize;

    // Steps for moving the overlay and cursor
    UINT32                m_ovlyStepX;
    UINT32                m_ovlyStepY;
    UINT32                m_lwrsStepX;
    UINT32                m_lwrsStepY;

    // Custom raster settings for SOR0 to show the whole wrbk surface
    // without scaling
    EvoRasterSettings     m_rasterSettings;

    // CRC Settings
    vector<EvoCRCSettings> m_vCrcSetting;
    vector<CrcComparison>  m_vCrcComp;
    vector<VerifyCrcTree>  m_vCrcVerifSetting;

    RC FreeResources();
    RC AllocResources(UINT32 numChn, UINT32 surfPerFrame);

    RC RunPassOne(UINT32 headWbor, UINT32 headSor);
    RC RunPassTwo(bool bUsePitch);
    RC RunPassThree();

    RC DumpFrame(
        Surface2D& surface,
        UINT32 headNum,
        UINT32 idx,
        UINT32 surfaceNum,
        bool bWriteTgaTxt
    );
};

RC WborMultiPassRigorousTest::DumpFrame
(
    Surface2D& surface,
    UINT32 headNum,
    UINT32 idx,
    UINT32 surfaceNum,
    bool bWriteTgaTxt
)
{
    RC rc;
    std::ostringstream os;

    os << "wrbkOnHead" << headNum << "_frame" << idx
       << "_surface" << surfaceNum;

    if (bWriteTgaTxt)
    {
        os << ".txt";
        CHECK_RC(surface.WriteText(os.str().c_str(),
            GetBoundGpuSubdevice()->GetSubdeviceInst()));

        os << ".tga";
        CHECK_RC(surface.WriteTga(os.str().c_str(),
            GetBoundGpuSubdevice()->GetSubdeviceInst()));
    }
    else
    {
        os << ".png";
        CHECK_RC(surface.WritePng(os.str().c_str(),
            GetBoundGpuSubdevice()->GetSubdeviceInst()));
    }

    return OK;
}

//! \brief WborMultiPassRigorousTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
WborMultiPassRigorousTest::WborMultiPassRigorousTest()
{
    SetName("WborMultiPassRigorousTest");

    m_runPass         = 0xFF;
    m_dwbEmu          = false;
    m_pDisplay        = nullptr;
    m_pEvoDisp        = nullptr;
    m_width           = 0;
    m_height          = 0;
    m_depth           = 0;
    m_refreshRate     = 0;
    m_lwrsSize        = 0;
    m_ovlySize        = 0;
    m_baseImageWidth  = 0;
    m_baseImageHeight = 0;
    m_lwrImageSize    = 0;
    m_ovlyImageSize   = 0;
    m_ovlyStepX       = 0;
    m_ovlyStepY       = 0;
    m_lwrsStepX       = 0;
    m_lwrsStepY       = 0;
    m_rasterSettings  = {};
}

//! \brief WborMultiPassRigorousTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
WborMultiPassRigorousTest::~WborMultiPassRigorousTest()
{
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string WborMultiPassRigorousTest::IsTestSupported()
{
    RC          rc          = OK;
    UINT32      wrbkClassId;

    m_pDisplay = GetDisplay();
    m_pEvoDisp = GetDisplay()->GetEvoDisplay();

    if (!m_pEvoDisp)
    {
        return "EVO Display is required to run this test";
    }

    // Initialize and detect display
    CHECK_RC_CLEANUP(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));
    CHECK_RC_CLEANUP(m_pDisplay->GetDetected(&m_detectedIDs));

    // Display must support a writeback class
    CHECK_RC_CLEANUP(m_pEvoDisp->GetClassAllocID(&wrbkClassId, Display::WRITEBACK_CHANNEL_ID));
    if (wrbkClassId == 0 ||
        !IsClassSupported(wrbkClassId))
    {
        return "Current platform does not support display writeback class";
    }

    // At least one of the ODs must be a WBOR
    for (UINT32 i=0; i < m_detectedIDs.size(); ++i)
    {
        LW0073_CTRL_SPECIFIC_OR_GET_INFO_PARAMS orGetInfoParams = {0};
        orGetInfoParams.displayId = m_detectedIDs[i];
        CHECK_RC_CLEANUP(m_pDisplay->RmControl(
                            LW0073_CTRL_CMD_SPECIFIC_OR_GET_INFO,
                            &orGetInfoParams,
                            sizeof(orGetInfoParams)
                        ));

        switch (orGetInfoParams.type)
        {
            case LW0073_CTRL_SPECIFIC_OR_TYPE_WBOR:
                m_detectedWbors.push_back(m_detectedIDs[i]);

                break;

            // Also save the SORs as we will be lighting those up to scan out
            // the written back surface
            case LW0073_CTRL_SPECIFIC_OR_TYPE_SOR:
                m_detectedSors.push_back(m_detectedIDs[i]);

                break;

            default:
                break;
        }
    }

    if (m_detectedWbors.size() == 0)
    {
        return "At least one of the ODs from VBIOS DCB must be a WBOR";
    }

Cleanup:
    if (rc == OK)
        return RUN_RMTEST_TRUE;
    else
        return rc.Message();
}

//! \brief Just doing Init from Js, nothing else.
//!
//! \return if InitFromJs passed, else corresponding rc
//------------------------------------------------------------------------------
RC WborMultiPassRigorousTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_pDisplay = GetDisplay();

    // Use smaller channel images will allow the fmodel runs to be much faster
    // Set up the active width and height based on platform
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        m_vBaseFile.push_back("dispinfradata/images/baseimage240x180.png");
        m_vBaseFile.push_back("dispinfradata/images/coreimage240x180.png");
        m_vBaseFile.push_back("dispinfradata/images/ovlyimage240x180.png");

        m_baseImageWidth  = 240;
        m_baseImageHeight = 180;

        // Smaller raster on fmodel for faster test run time
        m_width       = 160;
        m_height      = 120;
        m_depth       = 32;
        m_refreshRate = 30;
        m_lwrsSize    = 32;     // Both width and height
        m_ovlySize    = 64;     // Both width and height
    }
    else
    {
        // TODO: Add flag to support 4K based on flag
        m_vBaseFile.push_back("dispinfradata/images/baseimage640x480.PNG");
        m_vBaseFile.push_back("dispinfradata/images/coreimage640x480.PNG");
        m_vBaseFile.push_back("dispinfradata/images/ovlyimage640x480.PNG");

        m_baseImageWidth  = 640;
        m_baseImageHeight = 480;

        m_width       = 640;
        m_height      = 480;
        m_depth       = 32;
        m_refreshRate = 60;
        m_lwrsSize    = 32;
        m_ovlySize    = 64;
    }

    // Cursor and Overlay image
    m_lwrsFile      = "dispinfradata/images/image32x32.png";
    m_ovlyFile      = "dispinfradata/images/image64x64.png";
    m_lwrImageSize  = 32;
    m_ovlyImageSize = 64;

    // Steps for moving the overlay and cursor
    m_ovlyStepX = 30;
    m_ovlyStepY = 15;
    m_lwrsStepX = 30;
    m_lwrsStepY = 20;

    return rc;
}

//! \brief Run the test
//!
//! Call the SetMode() API with some default values
//!
//! \return OK if the SetMode call succeeded, else corresponding rc
//------------------------------------------------------------------------------
RC WborMultiPassRigorousTest::Run()
{
    RC rc;

    if (m_runPass & 0x1)
    {
        // --------------------------------------------------------
        // First run of pass1 connects WBOR0 to Head0 and SOR0 to Head1
        // --------------------------------------------------------
        UINT32 headWbor = 0;
        UINT32 headSor  = 1;

        CHECK_RC(RunPassOne(headWbor, headSor));

        // --------------------------------------------------------
        // Second run of pass1 connects WBOR0 to Head1 and SOR0 to Head0
        // --------------------------------------------------------
        headWbor = 1;
        headSor  = 0;

        CHECK_RC(RunPassOne(headWbor, headSor));
    }

    if (m_runPass & 0x2)
    {
        // --------------------------------------------------------
        // Run Pass2
        // --------------------------------------------------------
        CHECK_RC(RunPassTwo(false));
    }

    if (m_runPass & 0x4)
    {
        CHECK_RC(RunPassThree());
    }

    if (m_runPass & 0x8)
    {
        // --------------------------------------------------------
        // Pass4 is a debug run of Pass2 using ARGB format
        // useful on dFPGA where BL is not supported
        // --------------------------------------------------------
        CHECK_RC(RunPassTwo(true));
    }

    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC WborMultiPassRigorousTest::Cleanup()
{
    FreeResources();

    return OK;
}

RC WborMultiPassRigorousTest::AllocResources(UINT32 numChn, UINT32 surfPerFrame)
{
    UINT32 baseCount = static_cast<UINT32>(m_vBaseFile.size());
    UINT32 totalBase = numChn * baseCount;
    UINT32 surfReq   = numChn * baseCount * surfPerFrame;

    m_vOutputSurface.resize(surfReq);
    m_vBaseImage.resize(totalBase);
    m_vCoreImageSor.resize(baseCount);
    m_vCoreImageWbor.resize(baseCount);
    m_vLwrsImage.resize(numChn);
    m_vOvlyImage.resize(numChn);

    m_vFrameId.resize(numChn);

    m_vCrcSetting.resize(numChn);
    m_vCrcComp.resize(numChn);
    m_vCrcVerifSetting.resize(numChn);

    // Fill in some defaults
    for (UINT32 idx = 0; idx < numChn; ++idx)
    {
        // Fill in the CRC settings we want
        m_vCrcSetting.at(idx).ControlChannel    = EvoCRCSettings::CORE;
        m_vCrcSetting.at(idx).CaptureAllCRCs    = true;
        // Head with WBOR has loadv controlled by Wrbk.Udt+GetFrame so we
        // should not allow DTI to poll for completion notifier by default
        // as it may never return
        m_vCrcSetting.at(idx).PollCRCCompletion = false;

        // Because in WBOR we control the loadv, we are expecting each CRC will have
        // met the MPI from Core's perspective. So we need to set the relevant
        // flag so that the Crc comparison will be performed on CRCs that met MPI
        m_vCrcVerifSetting.at(idx).crcNotifierField.crcField.bCompEvenIfPresentIntervalMet = true;

        // Clearing the CRC notifier before detaching WBOR results in extra CRCs during detach
        // which triggers overflow warning for the first frame on next attach
        // According to arch we should ignore this status bit
        m_vCrcVerifSetting.at(idx).crcNotifierField.bIgnorePrimaryOverflow = true;

        // This test is expected to produce the same CRC across multiple platforms
        // and chips. We will reuse the CRC generated from a single chip
        m_vCrcVerifSetting.at(idx).crcHeaderField.bSkipPlatform = true;
        m_vCrcVerifSetting.at(idx).crcHeaderField.bSkipChip = true;
    }

    return OK;
}

RC WborMultiPassRigorousTest::FreeResources()
{
    m_vOutputSurface.clear();
    m_vBaseImage.clear();
    m_vCoreImageSor.clear();
    m_vCoreImageWbor.clear();
    m_vLwrsImage.clear();
    m_vOvlyImage.clear();

    m_vFrameId.clear();

    m_vCrcSetting.clear();
    m_vCrcComp.clear();
    m_vCrcVerifSetting.clear();

    return OK;
}

RC WborMultiPassRigorousTest::RunPassOne(UINT32 headWbor, UINT32 headSor)
{
    RC                    rc;

    if (m_detectedSors.size() == 0)
    {
        MASSERT(!"At least one of the ODs from VBIOS DCB must be a DFP to support loopback");
    }

    DisplayID             dispIDWbor  = m_detectedWbors.front();
    DisplayID             dispIDSor   = m_detectedSors.front();

    EvoDisplayChnContext *pBase1Ctx;

    // If we are not using a DTI special raster size then we need to compute our own
    // raster to insert the overscan for WBOR. The DTI special raster already has the
    // overscan we want for this test
    if (m_width != 160 && m_height != 120)
    {
        LWT_TIMING timing;
        UINT32 flags = 0;

        memset(&timing, 0 , sizeof(LWT_TIMING));
        CHECK_RC(ModesetUtils::CallwlateLWTTiming(
                    m_pEvoDisp,
                    dispIDWbor,
                    m_width,
                    m_height,
                    m_refreshRate,
                    Display::CVT_RB,
                    &timing,
                    flags));

        // Setup the raster (2px overscan top/bottom, 1px overscan left/right)
        timing.HBorder = 2;
        timing.VBorder = 1;
        EvoRasterSettings rs(&timing);

        CHECK_RC(m_pDisplay->SetupEvoLwstomTimings(dispIDWbor, m_width, m_height,
            m_refreshRate, Display::MANUAL, &rs, headWbor, 0, nullptr));
    }

    // Sizes used to setup raster
    UINT32               wrbkSurfaceWidth  = m_width + 80;
    UINT32               wrbkSurfaceHeight = m_height + 60;

    Printf(Tee::PriHigh, ">>>>>>> Running pass 1, WBOR => Head%d, SOR => Head%d <<<<<<<\n",
        headWbor, headSor);

    // Setup the raster for the SOR based on WBOR surface size
    // This is a fixed raster and it does not need to be very precise as
    // we are only scanning out to SOR to visually inspect the wrbk surface
    m_rasterSettings.Dirty        = true;
    m_rasterSettings.RasterWidth  = wrbkSurfaceWidth + 26;
    m_rasterSettings.ActiveX      = wrbkSurfaceWidth;
    m_rasterSettings.SyncEndX     = 15;
    m_rasterSettings.BlankStartX  = m_rasterSettings.RasterWidth - 6;
    m_rasterSettings.BlankEndX    = m_rasterSettings.SyncEndX + 5;
    m_rasterSettings.PolarityX    = false;
    m_rasterSettings.RasterHeight = wrbkSurfaceHeight + 30;
    m_rasterSettings.ActiveY      = wrbkSurfaceHeight;
    m_rasterSettings.SyncEndY     = 0;
    m_rasterSettings.BlankStartY  = m_rasterSettings.RasterHeight - 4;
    m_rasterSettings.BlankEndY    = m_rasterSettings.SyncEndY + 26;
    m_rasterSettings.Blank2StartY = 1;
    m_rasterSettings.Blank2EndY   = 0;
    m_rasterSettings.PolarityY    = false;
    m_rasterSettings.Interlaced   = false;
    m_rasterSettings.PixelClockHz = 165000000;
    m_rasterSettings.Dirty        = true;

    CHECK_RC(m_pDisplay->SetupEvoLwstomTimings(dispIDSor, m_width, m_height,
        m_refreshRate, Display::MANUAL, &m_rasterSettings, headSor, 0, nullptr));

    // Size the vectors, using 1 WBOR with ARGB so 1 surface per frame
    CHECK_RC(AllocResources(1, 1));

    // PassOne is frequently run without SOR to debug the WBOR itself and the SOR
    // is used as a visual inspection tool only on Silicon/Emulation with frame grabber.
    // The TAG number is different when running with and w/o SOR since running with SOR
    // produce extra tags. We are only interested here to see if the CRCs match in
    // sequence
    m_vCrcVerifSetting.at(0).crcNotifierField.bIgnoreTagNumber = true;

    // Setup the core surface for the wbor head
    CHECK_RC(m_pDisplay->SetupChannelImage(
        dispIDWbor,
        m_baseImageWidth, m_baseImageHeight, m_depth,
        Display::CORE_CHANNEL_ID,
        &m_vCoreImageWbor.at(0),
        "",                  // Image file
        0,                   // Do not fill the image
        headWbor,
        0, 0,                // No base/overscan color
        Surface2D::Pitch,
        ColorUtils::A8R8G8B8
    ));

    // Setup the core surface for the sor head
    CHECK_RC(m_pDisplay->SetupChannelImage(
        dispIDSor,
        wrbkSurfaceWidth, wrbkSurfaceHeight,    // We are scanning out entire wrbk surface
        m_depth,
        Display::CORE_CHANNEL_ID,
        &m_vCoreImageSor.at(0),
        "",                  // Image file
        0xFF00FF00,          // Fill the image so that bad scanout is easier to see
        headSor,
        0,                   // No base color
        0xFF00FFFF,          // Overscan on the SOR1 makes it easier to see the actual wrbk frame
        Surface2D::Pitch,
        ColorUtils::A8R8G8B8
    ));

    // Allocate WRBK surfaces and setup base images
    for (UINT32 i=0; i<m_vOutputSurface.size(); ++i)
    {
        CHECK_RC(m_pDisplay->SetupChannelImage(
            dispIDWbor,
            // Setting the output surface bigger than active region to
            // test wrbk overscan insertion per IAS
            wrbkSurfaceWidth, wrbkSurfaceHeight, m_depth,
            Display::WRITEBACK_CHANNEL_ID,
            &m_vOutputSurface.at(i),     // Surf2D object
            "",                // We are not loading an image for WRBK
            0xFFFF0000,        // Fill the surface with red
            headWbor,
            0, 0,              // No base/overscan color for WRBK
            Surface2D::Pitch,
            ColorUtils::A8R8G8B8
        ));

        CHECK_RC(m_pDisplay->SetupChannelImage(
            dispIDWbor,
            m_baseImageWidth, m_baseImageHeight, m_depth,
            Display::BASE_CHANNEL_ID,
            &m_vBaseImage.at(i),
            m_vBaseFile.at(i),     // Image file
            0,                   // Do not fill the image
            headWbor,
            0, 0,                // No base/overscan color
            Surface2D::Pitch,
            ColorUtils::A8R8G8B8
        ));
    }

    // Setup the cursor and overlay images
    CHECK_RC(m_pDisplay->SetupChannelImage(
        dispIDWbor,
        m_lwrImageSize, m_lwrImageSize, m_depth,
        Display::LWRSOR_IMM_CHANNEL_ID,
        &m_vLwrsImage.at(0),
        m_lwrsFile.c_str(),
        0,      // Do not fill the image
        headWbor,
        0, 0,   // No base/overscan color
        Surface2D::Pitch,
        ColorUtils::A8R8G8B8
    ));

    CHECK_RC(m_pDisplay->SetupChannelImage(
        dispIDWbor,
        m_ovlyImageSize, m_ovlyImageSize, m_depth,
        Display::OVERLAY_CHANNEL_ID,
        &m_vOvlyImage.at(0),
        m_ovlyFile.c_str(),
        0,      // Do not fill the image
        headWbor,
        0, 0,   // No base/overscan color
        Surface2D::Pitch,
        ColorUtils::A8R8G8B8
    ));

    // Setup the WRBK channel with a surface and correct settings
    CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vOutputSurface.front(),
        Display::WRITEBACK_CHANNEL_ID));

    // Modeset to attach WBOR to Head
    m_pDisplay->SetMode(dispIDWbor, m_width, m_height, m_depth, m_refreshRate,
        headWbor);

    // Modeset to attach SOR to Head
    m_pDisplay->SetMode(dispIDSor, wrbkSurfaceWidth, wrbkSurfaceHeight, m_depth,
        m_refreshRate, headSor);

    // Start the cursor at the top left corner
    CHECK_RC(m_pDisplay->SetLwrsorPos(dispIDWbor, 0, 0));

    // Start the overlay at the bottom right corner
    CHECK_RC(m_pDisplay->SetOverlayParameters(
        dispIDWbor,
        // X, Y, Width, Height
        0, 0, m_ovlySize, m_ovlySize,
        m_ovlySize, 0, 0, 0
    ));
    CHECK_RC(m_pDisplay->SetOverlayPos(dispIDWbor,
        m_baseImageWidth - m_ovlySize, m_baseImageHeight - m_ovlySize));

    // Get the base channel from sor head
    pBase1Ctx = m_pEvoDisp->GetEvoDisplayChnContext(dispIDSor,
        Display::BASE_CHANNEL_ID);
    MASSERT(pBase1Ctx);

    // Start the CRC
    CHECK_RC(m_pDisplay->SetupCrcBuffer(dispIDWbor, &m_vCrcSetting.at(0)));
    CHECK_RC(m_pDisplay->StartRunningCrc(dispIDWbor, &m_vCrcSetting.at(0)));

    // For each frame (3 frames total)
    for (UINT32 i = 0; i < m_vBaseFile.size(); ++i)
    {
        // Update the base image
        CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vBaseImage.at(i),
            Display::BASE_CHANNEL_ID));

        // Setup the new wrbk surface, skipping first surface as it must be
        // set before the initial modeset
        if (i > 0)
        {
            CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vOutputSurface.at(i),
                Display::WRITEBACK_CHANNEL_ID));
        }

        // For the first frame setup the cursor and overlay
        if (i == 0)
        {
            CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vLwrsImage.at(0),
                Display::LWRSOR_IMM_CHANNEL_ID));

            CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vOvlyImage.at(0),
                Display::OVERLAY_CHANNEL_ID));
        }
        // For other frames move them
        else
        {
            CHECK_RC(m_pDisplay->SetOverlayPos(dispIDWbor,
                m_baseImageWidth - m_ovlySize - m_ovlyStepX*(i+1),
                m_baseImageHeight - m_ovlySize - m_ovlyStepY*(i+1)));

            CHECK_RC(m_pDisplay->SetLwrsorPos(dispIDWbor,
                m_lwrsStepX*(i+1), m_lwrsStepY*(i+1)));
        }

        // Get a frame, this releases a loadv on the wbor connected head
        CHECK_RC(m_pEvoDisp->GetWritebackFrame(headWbor, false, &m_vFrameId[0]));
        CHECK_RC(m_pEvoDisp->PollWritebackFrame(headWbor, m_vFrameId[0], true));

        // Dump the frame
        CHECK_RC(DumpFrame(m_vOutputSurface.at(i), headWbor, i, 0, false));

        // Scan out the frame on SOR0
        Printf(Tee::PriHigh, "Scanning out wrbk surface to SOR\n");
        // Bind the ctxdma of the wrbk surface to base1
        CHECK_RC(m_vOutputSurface.at(i).BindIsoChannel(pBase1Ctx->GetDMAChanInfo()->hDispChan));

        // SetImage for base on SOR head
        CHECK_RC(m_pDisplay->SetImage(dispIDSor, &m_vOutputSurface.at(i),
            Display::BASE_CHANNEL_ID));
    }

    // Stop running CRC
    CHECK_RC(m_pDisplay->StopRunningCrc(dispIDWbor, &m_vCrcSetting.at(0)));

    // Detach the displays
    CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, dispIDSor)));
    // Loadv from this detach will flush out the CRC notifier
    CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, dispIDWbor)));

    std::ostringstream ossFile;
    std::ostringstream ossGolden;

    ossFile << "WborTestPass1_" << m_width << "x" << m_height << ".xml";
    ossGolden << "./DTI/Golden/WborMultiPassRigorous/" << ossFile.str();

    // Poll for CRC complete and dump the CRC
    CHECK_RC(m_pEvoDisp->PollCrcCompletion(&m_vCrcSetting.at(0)));
    CHECK_RC(m_vCrcComp.at(0).DumpCrcToXml(GetBoundGpuDevice(), ossFile.str().c_str(),
        &m_vCrcSetting.at(0), true, true));

    // Compare the CRCs
    // See verification of golden at //sw/docs/resman/components/display/v0208/VWB/golden_verif/WborMultiPassRigorousTest_Pass1/
    CHECK_RC(m_vCrcComp.at(0).CompareCrcFiles(ossFile.str().c_str(),
        ossGolden.str().c_str(), nullptr, &m_vCrcVerifSetting.at(0)));

    if (rc == OK)
        Printf(Tee::PriHigh, "Golden CRC compare matched\n");
    else
        Printf(Tee::PriHigh, "Golden CRC compare failed\n");

    // Free the per run surfaces allocated this run
    CHECK_RC(FreeResources());

    return rc;
}

RC WborMultiPassRigorousTest::RunPassTwo(bool bUsePitch)
{
    RC                    rc;

    // This pass requires us to use WBOR0
    DisplayID             dispIDWbor  = m_detectedWbors.front();

    // We will use head 0 for WBOR
    UINT32                headWbor    = 0;

    // Number of fames we will scanout
    UINT32                numFrames   = static_cast<UINT32>(m_vBaseFile.size());

    vector<Surface2D*>    tmpSurfaces;

    EvoWrbkChnContext    *pWrbkCtx    = dynamic_cast<EvoWrbkChnContext*>
        (m_pEvoDisp->GetEvoDisplayChnContext(dispIDWbor, Display::WRITEBACK_CHANNEL_ID));
    MASSERT(pWrbkCtx);

    // In this test we will not use core interlock with wrbk update
    pWrbkCtx->SetInterlockCore(false);

    // Default is pitch anyway
    if (!bUsePitch)
    {
        pWrbkCtx->SetSurfaceFormat(EvoWrbkChnContext::Y10_U10V10_N420);
    }

    // Used to track frame completion times
    UINT64                prevEndTs      = 0;
    UINT64                lwrrentStartTs = 0;
    UINT64                lwrrentEndTs   = 0;

    // We set the frame period to something large. Since the maximum
    // MinGrabInterval multiplier is 0xF, to really see the interval
    // we need a large frame period (otherwise it could just be noise)
    UINT64                framePeriod    = 0x428B00;
    UINT64                mgi            = 0xF;
    // Allow a 5% difference from what we set MGI/FP to be to what
    // we actually get
    UINT64                tolerance      = 5;

    // The bounds has a factor of x100 because the above are uS not nS
    UINT64 lowerBound = framePeriod * mgi * (100-tolerance);
    UINT64 upperBound = framePeriod * mgi * (100+tolerance);

    // Sizes used to setup raster
    UINT32               wrbkSurfaceWidth  = m_width;
    UINT32               wrbkSurfaceHeight = m_height;

    Printf(Tee::PriHigh, ">>>>>>> Running pass 2 <<<<<<<<<\n");

    // Setup the raster
    m_rasterSettings.Dirty        = true;
    m_rasterSettings.RasterWidth  = wrbkSurfaceWidth + 26;
    m_rasterSettings.ActiveX      = wrbkSurfaceWidth;
    m_rasterSettings.SyncEndX     = 15;
    m_rasterSettings.BlankStartX  = m_rasterSettings.RasterWidth - 6;
    m_rasterSettings.BlankEndX    = m_rasterSettings.SyncEndX + 5;
    m_rasterSettings.PolarityX    = false;
    m_rasterSettings.RasterHeight = wrbkSurfaceHeight + 30;
    m_rasterSettings.ActiveY      = wrbkSurfaceHeight;
    m_rasterSettings.SyncEndY     = 0;
    m_rasterSettings.BlankStartY  = m_rasterSettings.RasterHeight - 4;
    m_rasterSettings.BlankEndY    = m_rasterSettings.SyncEndY + 26;
    m_rasterSettings.Blank2StartY = 1;
    m_rasterSettings.Blank2EndY   = 0;
    m_rasterSettings.PolarityY    = false;
    m_rasterSettings.Interlaced   = false;
    m_rasterSettings.PixelClockHz = 165000000;

    CHECK_RC(m_pDisplay->SetupEvoLwstomTimings(dispIDWbor, m_width, m_height,
        m_refreshRate, Display::MANUAL, &m_rasterSettings, headWbor, 0, nullptr));

    // Size the vectors, for WBOR we are using Y10__U10V10 so we need 2 surfaces
    // per frame
    CHECK_RC(AllocResources(1, 2));

    // Setup the core surface for the wbor head
    CHECK_RC(m_pDisplay->SetupChannelImage(
        dispIDWbor,
        m_baseImageWidth, m_baseImageHeight, m_depth,
        Display::CORE_CHANNEL_ID,
        &m_vCoreImageWbor.at(0),
        "",                  // Image file
        0,                   // Do not fill the image
        headWbor,
        0, 0,                // No base/overscan color
        Surface2D::Pitch,
        ColorUtils::A8R8G8B8
    ));

    // Allocate WRBK surfaces and setup base images
    for (UINT32 i=0; i<numFrames; ++i)
    {
        if (bUsePitch)
        {
            CHECK_RC(m_pDisplay->SetupChannelImage(
                dispIDWbor,
                // Setting the output surface bigger than active region to
                // test wrbk overscan insertion per IAS
                wrbkSurfaceWidth, wrbkSurfaceHeight, m_depth,
                Display::WRITEBACK_CHANNEL_ID,
                &m_vOutputSurface.at(i),     // Surf2D object
                "",                // We are not loading an image for WRBK
                0xFFFF0000,        // Fill the surface with red
                headWbor,
                0, 0,              // No base/overscan color for WRBK
                Surface2D::Pitch,
                ColorUtils::A8R8G8B8
            ));
        }
        else
        {
            // Y10
            CHECK_RC(m_pDisplay->SetupChannelImage(
                dispIDWbor,
                wrbkSurfaceWidth, wrbkSurfaceHeight, m_depth,
                Display::WRITEBACK_CHANNEL_ID,
                &m_vOutputSurface.at(i*2),     // Surf2D object
                "",                // We are not loading an image for WRBK
                0,                 // No default fill
                headWbor,
                0, 0,              // No base/overscan color for WRBK
                Surface2D::BlockLinear,
                ColorUtils::R16    // This should be large enough for Y10
            ));

            // U10V10
            CHECK_RC(m_pDisplay->SetupChannelImage(
                dispIDWbor,
                wrbkSurfaceWidth, wrbkSurfaceHeight, m_depth,
                Display::WRITEBACK_CHANNEL_ID,
                &m_vOutputSurface.at(i*2+1),
                "",
                0,                 // No default fill
                headWbor,
                0, 0,
                Surface2D::BlockLinear,
                ColorUtils::R32     // This should be large enough for U10/V10
            ));
        }

        CHECK_RC(m_pDisplay->SetupChannelImage(
            dispIDWbor,
            m_baseImageWidth, m_baseImageHeight, m_depth,
            Display::BASE_CHANNEL_ID,
            &m_vBaseImage.at(i),
            m_vBaseFile.at(i),     // Image file
            0,                   // Do not fill the image
            headWbor,
            0, 0,                // No base/overscan color
            Surface2D::Pitch,
            ColorUtils::A8R8G8B8
        ));
    }

    // Setup the cursor and overlay images
    CHECK_RC(m_pDisplay->SetupChannelImage(
        dispIDWbor,
        m_lwrImageSize, m_lwrImageSize, m_depth,
        Display::LWRSOR_IMM_CHANNEL_ID,
        &m_vLwrsImage.at(0),
        m_lwrsFile.c_str(),
        0,      // Do not fill the image
        headWbor,
        0, 0,   // No base/overscan color
        Surface2D::Pitch,
        ColorUtils::A8R8G8B8
    ));

    CHECK_RC(m_pDisplay->SetupChannelImage(
        dispIDWbor,
        m_ovlyImageSize, m_ovlyImageSize, m_depth,
        Display::OVERLAY_CHANNEL_ID,
        &m_vOvlyImage.at(0),
        m_ovlyFile.c_str(),
        0,      // Do not fill the image
        headWbor,
        0, 0,   // No base/overscan color
        Surface2D::Pitch,
        ColorUtils::A8R8G8B8
    ));

    // Setup the WRBK channel with surfaces and correct settings
    if (bUsePitch)
    {
        CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vOutputSurface.front(),
            Display::WRITEBACK_CHANNEL_ID));
    }
    else
    {
        tmpSurfaces.push_back(&m_vOutputSurface[0]);
        tmpSurfaces.push_back(&m_vOutputSurface[1]);

        CHECK_RC(m_pDisplay->SetImage(
            dispIDWbor, tmpSurfaces,
            Display::WRITEBACK_CHANNEL_ID));
    }

    // Modeset to attach WBOR0 to Head0
    m_pDisplay->SetMode(dispIDWbor, m_width, m_height, m_depth, m_refreshRate,
        headWbor);

    // Start the cursor at the top left corner
    CHECK_RC(m_pDisplay->SetLwrsorPos(dispIDWbor, 0, 0));

    // Start the overlay at the bottom right corner
    CHECK_RC(m_pDisplay->SetOverlayParameters(
        dispIDWbor,
        // X, Y, Width, Height
        0, 0, m_ovlySize, m_ovlySize,
        m_ovlySize, 0, 0, 0
    ));
    CHECK_RC(m_pDisplay->SetOverlayPos(dispIDWbor,
        m_baseImageWidth - m_ovlySize, m_baseImageHeight - m_ovlySize));

    // Start the CRC
    CHECK_RC(m_pDisplay->SetupCrcBuffer(dispIDWbor, &m_vCrcSetting.at(0)));
    CHECK_RC(m_pDisplay->StartRunningCrc(dispIDWbor, &m_vCrcSetting.at(0)));

    // For each frame (3 frames total)
    for (UINT32 i = 0; i < numFrames; ++i)
    {
        // Update the base image
        CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vBaseImage.at(i),
            Display::BASE_CHANNEL_ID));

        // Setup the new wrbk surface, skipping first surface as it must be
        // set before the initial modeset
        if (i > 0)
        {
            if (bUsePitch)
            {
                CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vOutputSurface.at(i),
                    Display::WRITEBACK_CHANNEL_ID));
            }
            else
            {
                tmpSurfaces.clear();
                tmpSurfaces.push_back(&m_vOutputSurface[i*2]);
                tmpSurfaces.push_back(&m_vOutputSurface[i*2+1]);

                CHECK_RC(m_pDisplay->SetImage(
                    dispIDWbor, tmpSurfaces,
                    Display::WRITEBACK_CHANNEL_ID));
            }
        }

        // Change the MGI/PI settings (only in the first pass)
        // run this only on non-emulation mode, the frame time is otherwise way too long
        if (i == 0 && !m_dwbEmu)
        {
            CHECK_RC(m_pEvoDisp->SetPresentControlWriteback(dispIDWbor,
                UNSIGNED_CAST(UINT32, mgi), UNSIGNED_CAST(UINT32, framePeriod)));
        }

        // For the first frame setup the cursor and overlay
        if (i == 0)
        {
            CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vLwrsImage.at(0),
                Display::LWRSOR_IMM_CHANNEL_ID));

            CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vOvlyImage.at(0),
                Display::OVERLAY_CHANNEL_ID));
        }
        // For other frames move them
        else
        {
            CHECK_RC(m_pDisplay->SetOverlayPos(dispIDWbor,
                m_baseImageWidth - m_ovlySize - m_ovlyStepX*(i+1),
                m_baseImageHeight - m_ovlySize - m_ovlyStepY*(i+1)));

            CHECK_RC(m_pDisplay->SetLwrsorPos(dispIDWbor,
                m_lwrsStepX*(i+1), m_lwrsStepY*(i+1)));
        }

        // Get a frame, this releases a loadv on the wbor connected head
        CHECK_RC(m_pEvoDisp->GetWritebackFrame(headWbor, false, &m_vFrameId[0]));
        CHECK_RC(m_pEvoDisp->PollWritebackFrame(headWbor, m_vFrameId[0], true));

        // Check the MGI/PI Settings are met
        // note that we only get timestamps from HW in EMU/RTL/Si
        // skip this check in emulation mode since we will not be programming the MGI
        if (Platform::GetSimulationMode() == Platform::Hardware && !m_dwbEmu)
        {
            Surface2D *pFrameDesc = m_pEvoDisp->GetWritebackFrameDescriptor(headWbor, m_vFrameId[0]);

            UINT32 *pBuf = (UINT32*)(pFrameDesc->GetAddress());

            lwrrentStartTs = MEM_RD32(&pBuf[LW_DISP_WRITE_BACK_FRAME_DESCRIPTOR_1_BEGIN_TS_7]);
            lwrrentStartTs = (lwrrentStartTs << 32) |
                MEM_RD32(&pBuf[LW_DISP_WRITE_BACK_FRAME_DESCRIPTOR_1_BEGIN_TS_6]);

            lwrrentEndTs = MEM_RD32(&pBuf[LW_DISP_WRITE_BACK_FRAME_DESCRIPTOR_1_DONE_TS_9]);
            lwrrentEndTs = (lwrrentEndTs << 32) |
                MEM_RD32(&pBuf[LW_DISP_WRITE_BACK_FRAME_DESCRIPTOR_1_DONE_TS_8]);

            UINT64 interval   = lwrrentStartTs - prevEndTs;

            Printf(Tee::PriHigh, ">> Frame %d: Start: %llu, End: %llu, frame time=%llu, interval=%llu\n",
                    i, lwrrentStartTs, lwrrentEndTs, (lwrrentEndTs - lwrrentStartTs), interval);

            if (i > 0)
            {
                // Basic sanity: start time should be after prev. end time
                if (lwrrentStartTs <= prevEndTs)
                {
                    Printf(Tee::PriHigh,
                           ">> Error: current start time should not be before prev end time\n");

                    return RC::DATA_MISMATCH;
                }

                // Check that the current frame start time based on active MGI setting
                if ((lowerBound > interval) || (interval > upperBound))
                {
                    Printf(Tee::PriHigh,
                           ">> Error: Frame %d start time not within bounds (lower=%llu, upper=%llu)\n",
                           i, lowerBound, upperBound);

                    return RC::DATA_MISMATCH;
                }

                Printf(Tee::PriHigh, ">> MGI check for iteration %d passed\n", i);
            }
            prevEndTs = lwrrentEndTs;
        }
        else
        {
            Printf(Tee::PriHigh, "Skipping Timestamp check on fmodel (no support) or with dwbEmu flag (too slow)\n");
        }

        // Dump the frame
        if (!bUsePitch)
        {
            CHECK_RC(DumpFrame(m_vOutputSurface.at(i*2), headWbor, i, 0, true));
            CHECK_RC(DumpFrame(m_vOutputSurface.at(i*2+1), headWbor, i, 1, true));
        }
        else
        {
            CHECK_RC(DumpFrame(m_vOutputSurface.at(i), headWbor, i, 0, false));
        }
    }

    // Stop running CRC
    CHECK_RC(m_pDisplay->StopRunningCrc(dispIDWbor, &m_vCrcSetting.at(0)));

    // Detach the display, Loadv from this detach will flush out the CRC notifier
    CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, dispIDWbor)));

    std::ostringstream ossFile;
    std::ostringstream ossGolden;

    if (!bUsePitch)
    {
        ossFile << "WborTestPass2_" << m_width << "x" << m_height << ".xml";
    }
    else
    {
        ossFile << "WborTestPass4_" << m_width << "x" << m_height << ".xml";
    }
    ossGolden << "./DTI/Golden/WborMultiPassRigorous/" << ossFile.str();

    // Poll for CRC complete and dump the CRC
    CHECK_RC(m_pEvoDisp->PollCrcCompletion(&m_vCrcSetting.at(0)));
    CHECK_RC(m_vCrcComp.at(0).DumpCrcToXml(GetBoundGpuDevice(), ossFile.str().c_str(),
        &m_vCrcSetting.at(0), true, true));

    // Compare the CRCs
    // See verification of golden at //sw/docs/resman/components/display/v0208/VWB/golden_verif/WborMultiPassRigorousTest_Pass[1-3]/
    CHECK_RC(m_vCrcComp.at(0).CompareCrcFiles(ossFile.str().c_str(),
        ossGolden.str().c_str(), nullptr, &m_vCrcVerifSetting.at(0)));

    if (rc == OK)
        Printf(Tee::PriHigh, "Golden CRC compare matched\n");
    else
        Printf(Tee::PriHigh, "Golden CRC compare failed\n");

    // Free the per run surfaces allocated this run
    CHECK_RC(FreeResources());

    return rc;
}

RC WborMultiPassRigorousTest::RunPassThree()
{
    RC                    rc;

    // Number of channels/heads we will be using
    UINT32 numChn    = 2;
    UINT32 idx       = 0;

    // Number of fames we will scanout
    UINT32 numFrames = static_cast<UINT32>(m_vBaseFile.size());

    // Used to track frame start times
    UINT64 startTs0    = 0;
    UINT64 startTs1    = 0;
    // Time between the two WBOR's GetFrames, this should be large enough
    // to notice a significant difference
    UINT32 sleepTimeMs = 2000;

    vector<Surface2D*> tmpSurfaces;

    // Sizes used to setup raster
    UINT32 wrbkSurfaceWidth  = m_width;
    UINT32 wrbkSurfaceHeight = m_height;

    Printf(Tee::PriHigh, ">>>>>>> Running pass 3 <<<<<<<<<\n");

    // Setup the raster
    m_rasterSettings.Dirty        = true;
    m_rasterSettings.RasterWidth  = wrbkSurfaceWidth + 26;
    m_rasterSettings.ActiveX      = wrbkSurfaceWidth;
    m_rasterSettings.SyncEndX     = 15;
    m_rasterSettings.BlankStartX  = m_rasterSettings.RasterWidth - 6;
    m_rasterSettings.BlankEndX    = m_rasterSettings.SyncEndX + 5;
    m_rasterSettings.PolarityX    = false;
    m_rasterSettings.RasterHeight = wrbkSurfaceHeight + 30;
    m_rasterSettings.ActiveY      = wrbkSurfaceHeight;
    m_rasterSettings.SyncEndY     = 0;
    m_rasterSettings.BlankStartY  = m_rasterSettings.RasterHeight - 4;
    m_rasterSettings.BlankEndY    = m_rasterSettings.SyncEndY + 26;
    m_rasterSettings.Blank2StartY = 1;
    m_rasterSettings.Blank2EndY   = 0;
    m_rasterSettings.PolarityY    = false;
    m_rasterSettings.Interlaced   = false;
    m_rasterSettings.PixelClockHz = 165000000;

    // Size the vectors
    CHECK_RC(AllocResources(2, 1));

    // Override the CRC settings, for this test we need to ignore the
    // tag number comparison. We are scanning out 2 WBORs interlocked
    // to a core update, since the CRC is also core update controlled,
    // the tag numbers between the WBORs will alternate
    for (idx = 0; idx < numChn; ++idx)
    {
        m_vCrcVerifSetting.at(idx).crcNotifierField.bIgnoreTagNumber = true;
    }

    vector<EvoWrbkChnContext*> pWrbkCtxs;

    for (idx = 0; idx < numChn; ++idx)
    {
        // Head0, 1 for WBORs and 2, 3 for SORs
        // TODO: Add scanout to SORs
        UINT32             headWbor   = idx;
        DisplayID          dispIDWbor = m_detectedWbors[idx];

        EvoWrbkChnContext *pCtx       = dynamic_cast<EvoWrbkChnContext*>
            (m_pEvoDisp->GetEvoDisplayChnContext(dispIDWbor, Display::WRITEBACK_CHANNEL_ID));
        MASSERT(pCtx);

        pWrbkCtxs.push_back(pCtx);

        CHECK_RC(m_pDisplay->SetupEvoLwstomTimings(dispIDWbor, m_width, m_height,
            m_refreshRate, Display::MANUAL, &m_rasterSettings, headWbor, 0, nullptr));

        // Setup the core surface for the wbor head
        CHECK_RC(m_pDisplay->SetupChannelImage(
            dispIDWbor,
            m_baseImageWidth, m_baseImageHeight, m_depth,
            Display::CORE_CHANNEL_ID,
            &m_vCoreImageWbor.at(idx),
            "",                  // Image file
            0,                   // Do not fill the image
            headWbor,
            0, 0,                // No base/overscan color
            Surface2D::Pitch,
            ColorUtils::A8R8G8B8
        ));

        // Allocate WRBK surfaces and setup base images
        // each frame has 1 surfaces, we require numFrames * 1 per WBOR
        // 2nd WBOR offsets by numFrames*1
        UINT32 headOffset = idx * numFrames;

        for (UINT32 i=0; i<numFrames; ++i)
        {
            CHECK_RC(m_pDisplay->SetupChannelImage(
                dispIDWbor,
                // Setting the output surface bigger than active region to
                // test wrbk overscan insertion per IAS
                wrbkSurfaceWidth, wrbkSurfaceHeight, m_depth,
                Display::WRITEBACK_CHANNEL_ID,
                &m_vOutputSurface.at(headOffset + i),     // Surf2D object
                "",                // We are not loading an image for WRBK
                0xFFFF0000,        // Fill the surface with red
                headWbor,
                0, 0,              // No base/overscan color for WRBK
                Surface2D::Pitch,
                ColorUtils::A8R8G8B8
            ));

            CHECK_RC(m_pDisplay->SetupChannelImage(
                dispIDWbor,
                m_baseImageWidth, m_baseImageHeight, m_depth,
                Display::BASE_CHANNEL_ID,
                &m_vBaseImage.at(headOffset + i),
                m_vBaseFile.at(i),     // Image file
                0,                   // Do not fill the image
                headWbor,
                0, 0,                // No base/overscan color
                Surface2D::Pitch,
                ColorUtils::A8R8G8B8
            ));
        }

        // Setup the cursor and overlay images
        CHECK_RC(m_pDisplay->SetupChannelImage(
            dispIDWbor,
            m_lwrImageSize, m_lwrImageSize, m_depth,
            Display::LWRSOR_IMM_CHANNEL_ID,
            &m_vLwrsImage.at(idx),
            m_lwrsFile.c_str(),
            0,      // Do not fill the image
            headWbor,
            0, 0,   // No base/overscan color
            Surface2D::Pitch,
            ColorUtils::A8R8G8B8
        ));

        CHECK_RC(m_pDisplay->SetupChannelImage(
            dispIDWbor,
            m_ovlyImageSize, m_ovlyImageSize, m_depth,
            Display::OVERLAY_CHANNEL_ID,
            &m_vOvlyImage.at(idx),
            m_ovlyFile.c_str(),
            0,      // Do not fill the image
            headWbor,
            0, 0,   // No base/overscan color
            Surface2D::Pitch,
            ColorUtils::A8R8G8B8
        ));

        // Setup the WRBK channel with surfaces and correct settings
        CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vOutputSurface.at(headOffset),
            Display::WRITEBACK_CHANNEL_ID));

        // Modeset to attach WBOR to Head
        m_pDisplay->SetMode(dispIDWbor, m_width, m_height, m_depth, m_refreshRate,
            headWbor);

        // Start the cursor at the top left corner
        CHECK_RC(m_pDisplay->SetLwrsorPos(dispIDWbor, 0, 0));

        // Start the overlay at the bottom right corner
        CHECK_RC(m_pDisplay->SetOverlayParameters(
            dispIDWbor,
            // X, Y, Width, Height
            0, 0, m_ovlySize, m_ovlySize,
            m_ovlySize, 0, 0, 0
        ));
        CHECK_RC(m_pDisplay->SetOverlayPos(dispIDWbor,
            m_baseImageWidth - m_ovlySize, m_baseImageHeight - m_ovlySize));

        // Start the CRC
        CHECK_RC(m_pDisplay->SetupCrcBuffer(dispIDWbor, &m_vCrcSetting.at(idx)));
        CHECK_RC(m_pDisplay->StartRunningCrc(dispIDWbor, &m_vCrcSetting.at(idx)));
    } // By this point we have numChn amount of WBORs properly configured and attached

    // For each frame (3 frames total)
    for (UINT32 i = 0; i < numFrames; ++i)
    {
        // For each WBOR we will setup the images required
        for (idx = 0; idx < numChn; ++idx)
        {
            DisplayID          dispIDWbor = m_detectedWbors[idx];

            UINT32 headOffset = idx * numFrames;

            // Update the base image
            CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vBaseImage.at(i),
                Display::BASE_CHANNEL_ID));

            // Setup the new wrbk surface, skipping first surface as it must be
            // set before the initial modeset
            if (i > 0)
            {
                CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vOutputSurface.at(headOffset + i),
                    Display::WRITEBACK_CHANNEL_ID));
            }

            // For the first frame setup the cursor and overlay
            if (i == 0)
            {
                CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vLwrsImage.at(idx),
                    Display::LWRSOR_IMM_CHANNEL_ID));

                CHECK_RC(m_pDisplay->SetImage(dispIDWbor, &m_vOvlyImage.at(idx),
                    Display::OVERLAY_CHANNEL_ID));
            }
            // For other frames move them
            else
            {
                CHECK_RC(m_pDisplay->SetOverlayPos(dispIDWbor,
                    m_baseImageWidth - m_ovlySize - m_ovlyStepX*(i+1),
                    m_baseImageHeight - m_ovlySize - m_ovlyStepY*(i+1)));

                CHECK_RC(m_pDisplay->SetLwrsorPos(dispIDWbor,
                    m_lwrsStepX*(i+1), m_lwrsStepY*(i+1)));
            }
        }

        // For each WBOR we then send out the GetFrames
        // this should not be inter-twined with the above where we try to
        // setup the other channels, as the core updates sent there interferes
        // with the interlocking GetFrames
        for (idx = 0; idx < numChn; ++idx)
        {
            UINT32             headWbor   = idx;

            // On the 2nd WBOR, we want to delay for sometime before sending out
            // a GetFrame
            if (idx > 0)
            {
                Tasker::Sleep(sleepTimeMs); // ms
            }

            // Get a frame, this releases a loadv on the wbor connected head
            CHECK_RC(m_pEvoDisp->GetWritebackFrame(headWbor, false, &m_vFrameId[idx]));
        }

        // Release the interlocked GetFrames
        CHECK_RC(m_pEvoDisp->ReleaseWritebackIlkGetFrame());

        // For each WBOR we poll for the frame to complete
        for (idx = 0; idx < numChn; ++idx)
        {
            UINT32 headWbor   = idx;
            UINT32 headOffset = idx * numFrames;

            // Poll for completion of the notifier and the surface descriptor
            CHECK_RC(m_pEvoDisp->PollWritebackFrame(headWbor, m_vFrameId[idx], true));

            // Dump the frame
            CHECK_RC(DumpFrame(m_vOutputSurface.at(headOffset + i), headWbor, i, 0, false));
        }

        // Check that the frame start time are close to each other
        // they should be since they are interlocked to a core update
        if (Platform::GetSimulationMode() == Platform::Hardware)
        {
            // Get the frame descriptor for WBOR0 and 1
            Surface2D *pFrameDesc0 = m_pEvoDisp->GetWritebackFrameDescriptor(0, m_vFrameId[0]);;
            Surface2D *pFrameDesc1 = m_pEvoDisp->GetWritebackFrameDescriptor(1, m_vFrameId[1]);;
            UINT32 *pBuf0 = (UINT32*)(pFrameDesc0->GetAddress());
            UINT32 *pBuf1 = (UINT32*)(pFrameDesc1->GetAddress());

            startTs0 = MEM_RD32(&pBuf0[LW_DISP_WRITE_BACK_FRAME_DESCRIPTOR_1_BEGIN_TS_7]);
            startTs0 = (startTs0 << 32) |
                MEM_RD32(&pBuf0[LW_DISP_WRITE_BACK_FRAME_DESCRIPTOR_1_BEGIN_TS_6]);

            startTs1 = MEM_RD32(&pBuf1[LW_DISP_WRITE_BACK_FRAME_DESCRIPTOR_1_BEGIN_TS_7]);
            startTs1 = (startTs1 << 32) |
                MEM_RD32(&pBuf1[LW_DISP_WRITE_BACK_FRAME_DESCRIPTOR_1_BEGIN_TS_6]);

            // abs difference between the two
            UINT64 res = startTs0 - startTs1;
            if (startTs0 < startTs1)
                res = startTs1 - startTs0;

            Printf(Tee::PriHigh, ">> Frame %d: Start WBOR0=%llu, Start WBOR1=%llu, diff=%llu\n",
                    i, startTs0, startTs1, res);

            // We consider if the residual time is more than 1/2 of the timeDiff we sleeped for
            // then it failed, the sleep time is colwerted to ns
            if (res > ((sleepTimeMs/2+sleepTimeMs) * 1000000))
            {
                Printf(Tee::PriHigh,
                       ">> Error: start times of the two WBORs are not close enough when interlocked\n");

                return RC::DATA_MISMATCH;
            }
            else
            {
                Printf(Tee::PriHigh, ">> Interlock check for iteration %d passed\n", i);
            }
        }
        else
        {
            Printf(Tee::PriHigh, "Skipping Timestamp check on fmodel (no support)\n");
        }

    }

    // For each WBOR
    for (idx = 0; idx < numChn; ++idx)
    {
        DisplayID          dispIDWbor = m_detectedWbors[idx];

        // Stop running CRC
        CHECK_RC(m_pDisplay->StopRunningCrc(dispIDWbor, &m_vCrcSetting.at(idx)));

        // Detach the display, Loadv from this detach will flush out the CRC notifier
        CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, dispIDWbor)));

        std::ostringstream ossFile;
        std::ostringstream ossGolden;

        ossFile << "WborTestPass3_Wbor" << idx << ".xml";
        ossGolden << "./DTI/Golden/WborMultiPassRigorous/" << "WborTestPass3_" << m_width << "x" << m_height << ".xml";

        // Poll for CRC complete and dump the CRC
        CHECK_RC(m_pEvoDisp->PollCrcCompletion(&m_vCrcSetting.at(idx)));
        CHECK_RC(m_vCrcComp.at(idx).DumpCrcToXml(GetBoundGpuDevice(), ossFile.str().c_str(),
            &m_vCrcSetting.at(idx), true, true));

        // Compare the CRCs
        // See verification of golden at //sw/docs/resman/components/display/v0208/VWB/golden_verif/WborMultiPassRigorousTest_Pass1/
        // Both heads should match the same golden CRC
        CHECK_RC(m_vCrcComp.at(idx).CompareCrcFiles(ossFile.str().c_str(),
            ossGolden.str().c_str(), nullptr, &m_vCrcVerifSetting.at(idx)));

        if (rc == OK)
            Printf(Tee::PriHigh, "Golden CRC compare matched for WBOR%d\n", idx);
        else
            Printf(Tee::PriHigh, "Golden CRC compare failed for WBOR%d\n", idx);
    }

    // Free the per run surfaces allocated this run
    CHECK_RC(FreeResources());

    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(WborMultiPassRigorousTest, RmTest,
    "Functional test for multiple WBORs");

CLASS_PROP_READWRITE(WborMultiPassRigorousTest, runPass, UINT32, "Bitmask to determine which passes will run");
CLASS_PROP_READWRITE(WborMultiPassRigorousTest, dwbEmu, bool, "Indicating we are running on emulation, parts of the test like timestamp, MGI are skipped as they take too long");

