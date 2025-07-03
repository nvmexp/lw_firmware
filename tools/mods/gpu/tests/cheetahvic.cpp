/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "tegravic.h"
#include "core/include/channel.h"
#include "class/clb0b6.h"
#include "class/clb1b6.h"
#include "class/clc5b6.h"
#include "class/cle26e.h"
#include "core/include/fileholder.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "gpu/utility/surfrdwr.h"
#include "core/include/utility.h"
#ifndef VARIADIC_MACROS_NOT_SUPPORTED
#include "cheetah/dev/vic/vicctrlmgr.h"
#endif

static const UINT32 MIN_RECT_WIDTH     = 2;
static const UINT32 MIN_RECT_HEIGHT    = 4;
static const UINT32 MAX_IMAGE_WIDTH    = 1024;
static const UINT32 MAX_IMAGE_HEIGHT   = 1024;
static const UINT32 MAX_SCALING_FACTOR = 16;
static const UINT32 PITCH_STRIDE_ALIGN = 256;
static const UINT32 PITCH_HEIGHT_ALIGN = 16;
static const UINT32 BLOCK_STRIDE_ALIGN = 64;
static const UINT32 BLOCK_HEIGHT_ALIGN = 64;
static const UINT32 MAX_LIGHT_LEVEL    = 15;
static const UINT32 MAX_GAMUT_MATRIX_VALUE = 0xFFFFF;

const UINT32 TegraVic::s_SupportedClasses[] =
{
    LWC5B6_VIDEO_COMPOSITOR,
    LWB1B6_VIDEO_COMPOSITOR,
    LWB0B6_VIDEO_COMPOSITOR
};

/* static */ const UINT64 TegraVicHelper::ALL_ONES = 0xffffffffffffffffULL;
/* static */ const UINT64 TegraVicHelper::IGNORED = 0;

//--------------------------------------------------------------------
TegraVic::TegraVic() :
    m_DumpFiles(0),
    m_IsBackCompatible(false),
    m_FirstBackCompatibleVersion(static_cast<Version>(0)),
    m_LastBackCompatibleVersion(static_cast<Version>(NUM_VERSIONS - 1))
{
    SetName("TegraVic");
    CreateFancyPickerArray(FPK_TEGRAVIC_NUM_PICKERS);
}

//--------------------------------------------------------------------
/* virtual */ bool TegraVic::IsSupported()
{
    return (GpuTest::IsSupported() &&
            GetBoundGpuSubdevice()->IsSOC() &&
            GetFirstSupportedClass() != 0);
}

//--------------------------------------------------------------------
/* virtual */ RC TegraVic::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    // Allocate the TegraVicHelper object that does 99% of the work
    //
    switch (GetFirstSupportedClass())
    {
        case LWB0B6_VIDEO_COMPOSITOR:
            m_pHelper.reset(new TegraVic4Helper(this));
            break;
        case LWB1B6_VIDEO_COMPOSITOR:
            m_pHelper.reset(new TegraVic41Helper(this));
            break;
        case LWC5B6_VIDEO_COMPOSITOR:
            m_pHelper.reset(new TegraVic42Helper(this));
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }

    // Do the rest of the setup in TegraVicHelper
    //
    CHECK_RC(m_pHelper->Setup());

    CHECK_RC(ValidateTestArgs());
    return rc;
}

//--------------------------------------------------------------------
/* virtual */ RC TegraVic::Run()
{
    return m_pHelper->Run();
}

//--------------------------------------------------------------------
/* virtual */ RC TegraVic::Cleanup()
{
    StickyRC firstRc;
    firstRc = m_pHelper->Cleanup();
    firstRc = GpuTest::Cleanup();
    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Set defaults for the fancy pickers
//!
//! Implementation note: This is one of the few TegraVic methods that
//! isn't offloaded to TegraVicHelper.  Mostly because we need a
//! GpuDevice in order to know which helper subclass to allocate.  The
//! current MODS architecture requires this method to be called from
//! the constructor if somebody overrides a picker from JS, and we
//! don't have a bound GpuDevice yet.
//!
/* virtual */ RC TegraVic::SetDefaultsForPicker(UINT32 idx)
{
    FancyPicker *pPicker = &(*GetFancyPickerArray())[idx];
    RC rc;

    switch (idx)
    {
        case FPK_TEGRAVIC_PERCENT_SLOTS:
            CHECK_RC(pPicker->FConfigRandom());
            break;
        default:
            CHECK_RC(pPicker->ConfigRandom());
            break;
    }

    switch (idx)
    {
        case FPK_TEGRAVIC_PERCENT_SLOTS:
            pPicker->FAddRandRange(1, 0, 100);
            pPicker->FAddRandItem(1, 100); // Bump up using all slots
            break;
        case FPK_TEGRAVIC_FRAME_FORMAT:
            // TODO: Add more formats
            pPicker->AddRandItem(1, Vic3::DXVAHD_FRAME_FORMAT_PROGRESSIVE);
            pPicker->AddRandItem(1, Vic3::DXVAHD_FRAME_FORMAT_BOTTOM_FIELD);
            pPicker->AddRandItem(1,
                    Vic3::DXVAHD_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST);
            break;
        case FPK_TEGRAVIC_DEINTERLACE_MODE:
            // TODO: Add more modes
            pPicker->AddRandItem(1, Vic3::DXVAHD_DEINTERLACE_MODE_PRIVATE_BOB);
            pPicker->AddRandItem(1, Vic3::DXVAHD_DEINTERLACE_MODE_PRIVATE_WEAVE);
            pPicker->AddRandItem(1, Vic3::DXVAHD_DEINTERLACE_MODE_PRIVATE_DISI1);
            break;
        case FPK_TEGRAVIC_PIXEL_FORMAT:
            // TODO: Add more formats
            pPicker->AddRandItem(1, Vic3::T_A8R8G8B8);
            pPicker->AddRandItem(2, Vic3::T_Y8___V8U8_N420);  // Bump up chroma
            break;
        case FPK_TEGRAVIC_BLOCK_KIND:
            // TODO: Add more formats
            pPicker->AddRandItem(1, Vic3::BLK_KIND_PITCH);
            pPicker->AddRandItem(1, Vic3::BLK_KIND_GENERIC_16Bx2);
            break;
        case FPK_TEGRAVIC_FLIPX:
            pPicker->AddRandItem(1, false);
            pPicker->AddRandItem(1, true);
            break;
        case FPK_TEGRAVIC_FLIPY:
            pPicker->AddRandItem(1, false);
            pPicker->AddRandItem(1, true);
            break;
        case FPK_TEGRAVIC_TRANSPOSE:
            pPicker->AddRandItem(1, false);
            pPicker->AddRandItem(1, true);
            break;
        case FPK_TEGRAVIC_IMAGE_WIDTH:
            pPicker->AddRandRange(1, MIN_RECT_WIDTH, MAX_IMAGE_WIDTH);
            break;
        case FPK_TEGRAVIC_IMAGE_HEIGHT:
            pPicker->AddRandRange(1, MIN_RECT_HEIGHT, MAX_IMAGE_HEIGHT);
            break;
        case FPK_TEGRAVIC_DENOISE:
            pPicker->AddRandItem(1, false);
            pPicker->AddRandItem(1, true);
            break;
        case FPK_TEGRAVIC_CADENCE_DETECT:
            pPicker->AddRandItem(1, false);
            pPicker->AddRandItem(1, true);
            break;
        case FPK_TEGRAVIC_RECTX:
            pPicker->AddRandRange(1, 0, MAX_IMAGE_WIDTH - 1);
            break;
        case FPK_TEGRAVIC_RECTY:
            pPicker->AddRandRange(1, 0, MAX_IMAGE_HEIGHT - 1);
            break;
        case FPK_TEGRAVIC_LIGHT_LEVEL:
            pPicker->AddRandRange(1, 0, MAX_LIGHT_LEVEL);
            break;
        case FPK_TEGRAVIC_RGB_DEGAMMAMODE:
            pPicker->AddRandItem(1, Vic42::GAMMA_MODE_NONE);
            pPicker->AddRandItem(1, Vic42::GAMMA_MODE_SRGB);
            break;
        case FPK_TEGRAVIC_YUV_DEGAMMAMODE:
            pPicker->AddRandItem(1, Vic42::GAMMA_MODE_NONE);
            pPicker->AddRandItem(1, Vic42::GAMMA_MODE_REC709);
            pPicker->AddRandItem(1, Vic42::GAMMA_MODE_REC2020);
            break;
        case FPK_TEGRAVIC_GAMUT_MATRIX:
            pPicker->AddRandRange(1, 0, MAX_GAMUT_MATRIX_VALUE);
            break;
        default:
            MASSERT(!"Unknown picker");
            return RC::SOFTWARE_ERROR;
    }
    pPicker->CompileRandom();
    return rc;
}

//--------------------------------------------------------------------
//! \brief Print the JS properties.
//!
/* virtual */ void TegraVic::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    string desc;
    if (m_DumpFiles & DUMP_TEXT)
        desc += "Text,";
    if (m_DumpFiles & DUMP_BINARY)
        desc += "Binary,";
    if (desc.empty())
        desc = "None";
    else
        desc.resize(desc.size() - 1);
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tKeepRunning:\t\t\t%s\n", m_KeepRunning ? "true" : "false");
    Printf(pri, "\tDumpFiles:\t\t\t%d (%s)\n", m_DumpFiles, desc.c_str());
    Printf(pri, "\tBackCompatibleMode:\t\t\"%s\"\n",
           GetBackCompatibleMode().c_str());
    Printf(pri, "\tEnableFlipX:\t\t\t0x%0x\n", m_EnableFlipX);
    Printf(pri, "\tEnableFlipY:\t\t\t0x%0x\n", m_EnableFlipY);
    Printf(pri, "\tEnableCadenceDetection:\t\t0x%0x\n", m_EnableCadenceDetection);
    Printf(pri, "\tEnableNoiseReduction:\t\t0x%0x\n", m_EnableNoiseReduction);
    Printf(pri, "\tFrameFormat:\t\t\t0x%0x\n", m_FrameFormat);
    Printf(pri, "\tInputPixelFormat:\t\t0x%0x\n", m_InputPixelFormat);
    Printf(pri, "\tOutputPixelFormat:\t\t0x%0x\n", m_OutputPixelFormat);
    Printf(pri, "\tInputBlockKind:\t\t\t0x%0x\n", m_InputBlockKind);
    Printf(pri, "\tOutputBlockKind:\t\t0x%0x\n", m_OutputBlockKind);
    Printf(pri, "\tDeInterlaceMode:\t\t0x%0x\n", m_DeInterlaceMode);
}

//--------------------------------------------------------------------
//! \brief Colwert a version number to a human-readable string
//!
/* static */ const char *TegraVic::GetVersionName(Version version)
{
    switch (version)
    {
        case VERSION_VIC3:
            return "3";
        case VERSION_VIC4:
            return "4";
        case VERSION_VIC41:
            return "4.1";
        case VERSION_VIC42:
            return "4.2";
        default:
            MASSERT(!"Illegal version passed to GetVersionName");
            return "";
    }
}

//--------------------------------------------------------------------
//! \brief Set the BackCompatibleMode testarg
//!
//! BackCompatibleMode must either be an empty string, or a range of
//! VIC versions of the form "[version1]:[version2]".A
//!
//! If non-empty, then TegraVic only uses the features common to the
//! requested range of versions, so that they all produce the same
//! output surface (at least within a pixel or two).  This is useful
//! for double-checking the results on a new VIC version.
//!
//! The BackCompatibleMode string gets parsed into m_IsBackCompatible,
//! m_FirstBackCompatibleVersion, and m_LastBackCompatibleVersion.
//! Return RC::BAD_PARAMETER on a parse error.
//!
RC TegraVic::SetBackCompatibleMode(string backCompatibleMode)
{
    RC rc;

    m_IsBackCompatible           = (backCompatibleMode != "");
    m_FirstBackCompatibleVersion = static_cast<Version>(0);
    m_LastBackCompatibleVersion  = static_cast<Version>(NUM_VERSIONS - 1);

    if (m_IsBackCompatible)
    {
        size_t colonPos = backCompatibleMode.find(':');
        if (colonPos == string::npos)
        {
            colonPos = 0;
            rc = RC::BAD_PARAMETER;
        }
        string tokens[2] = { backCompatibleMode.substr(0, colonPos),
                             backCompatibleMode.substr(colonPos + 1) };
        for (int tokenIdx = 0; tokenIdx < 2; ++tokenIdx)
        {
            if (tokens[tokenIdx] != "")
            {
                bool found = false;
                for (UINT32 ii = 0; ii < NUM_VERSIONS; ++ii)
                {
                    Version version = static_cast<Version>(ii);
                    if (tokens[tokenIdx] == GetVersionName(version))
                    {
                        if (tokenIdx == 0)
                            m_FirstBackCompatibleVersion = version;
                        else
                            m_LastBackCompatibleVersion  = version;
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    rc = RC::BAD_PARAMETER;
                }
            }
        }

        if (rc == RC::BAD_PARAMETER)
        {
            Printf(Tee::PriHigh,
                   "Error: BackCompatibilyMode must be of the form version1:version2,\n");
            Printf(Tee::PriHigh, "where each version is one of:\n");
            Printf(Tee::PriHigh, "  - empty string\n");
            for (UINT32 ii = 0; ii < NUM_VERSIONS; ++ii)
            {
                Printf(Tee::PriHigh, "  - \"%s\"\n",
                       GetVersionName(static_cast<Version>(ii)));
            }
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Return the BackCompatibleMode testarg
//!
string TegraVic::GetBackCompatibleMode() const
{
    if (m_IsBackCompatible)
    {
        return Utility::StrPrintf("%s:%s",
                                  GetVersionName(m_FirstBackCompatibleVersion),
                                  GetVersionName(m_LastBackCompatibleVersion));
    }
    else
    {
        return "";
    }
}

//--------------------------------------------------------------------
//! \brief Return the VIC class supported by this GPU, or 0 if none.
//!
UINT32 TegraVic::GetFirstSupportedClass()
{
    LwRm *pLwRm = GetBoundRmClient();
    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    UINT32 myClass = 0;

    RC rc = pLwRm->GetFirstSupportedClass(
            static_cast<UINT32>(NUMELEMS(s_SupportedClasses)),
            &s_SupportedClasses[0], &myClass, pGpuDevice);
    if (rc != RC::OK)
        myClass = 0;
    return myClass;
}

RC TegraVic::ValidateBlockKind(const UINT08 blockKind)
{
    RC rc;

    switch (blockKind)
    {
        case _UINT08_MAX:
        case Vic3::BLK_KIND_PITCH:
        case Vic3::BLK_KIND_GENERIC_16Bx2:
            break;
        default:
            Printf(Tee::PriError, "Unsupported BlockKind %d\n", blockKind);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return rc;
}

RC TegraVic::ValidatePixelFormat(const UINT32 pixelFormat)
{
    RC rc;

    switch (pixelFormat)
    {
        case _UINT32_MAX:
        case Vic3::T_A8R8G8B8:
        case Vic3::T_Y8___V8U8_N420:
            break;
        default:
            Printf(Tee::PriError, "Unsupported PixelFormat %d\n", pixelFormat);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return rc;
}

RC TegraVic::ValidateTestArgs()
{
    RC rc;

    CHECK_RC(ValidateBlockKind(m_InputBlockKind));
    CHECK_RC(ValidateBlockKind(m_OutputBlockKind));
    CHECK_RC(ValidatePixelFormat(m_InputPixelFormat));
    CHECK_RC(ValidatePixelFormat(m_OutputPixelFormat));

    switch (m_FrameFormat)
    {
        case _UINT32_MAX:
        case Vic3::DXVAHD_FRAME_FORMAT_PROGRESSIVE:
        case Vic3::DXVAHD_FRAME_FORMAT_BOTTOM_FIELD:
        case Vic3::DXVAHD_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST:
            break;
        default:
            Printf(Tee::PriError, "Unsupported FrameFormat %d\n", m_FrameFormat);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    switch (m_DeInterlaceMode)
    {
        case _UINT08_MAX:
        case Vic3::DXVAHD_DEINTERLACE_MODE_PRIVATE_BOB:
        case Vic3::DXVAHD_DEINTERLACE_MODE_PRIVATE_WEAVE:
        case Vic3::DXVAHD_DEINTERLACE_MODE_PRIVATE_DISI1:
            break;
        default:
            Printf(Tee::PriError, "Unsupported DeInterlaceMode %d\n", m_DeInterlaceMode);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return rc;
}

//--------------------------------------------------------------------
TegraVicHelper::TegraVicHelper(TegraVic *pTest) :
    m_pTest(pTest),
    m_pLwRm(pTest->GetBoundRmClient()),
    m_pGpuDevice(pTest->GetBoundGpuDevice()),
    m_pGpuSubdevice(pTest->GetBoundGpuSubdevice()),
    m_pTestConfig(pTest->GetTestConfiguration()),
    m_pGoldelwalues(pTest->GetGoldelwalues()),
    m_pFpCtx(pTest->GetFpContext()),
    m_IsBackCompatible(pTest->m_IsBackCompatible),
    m_FirstBackCompatibleVersion(pTest->m_FirstBackCompatibleVersion),
    m_LastBackCompatibleVersion(pTest->m_LastBackCompatibleVersion),
    m_Pri(pTest->GetVerbosePrintPri()),
    m_pCh(nullptr),
    m_DstPixelFormat(static_cast<Vic3::PIXELFORMAT>(0)),
    m_DstBlockKind(static_cast<Vic3::BLK_KIND>(0)),
    m_FlipX(false),
    m_FlipY(false),
    m_Transpose(false),
    m_DstImageWidth(0),
    m_DstImageHeight(0),
    m_PercentSlots(0)
{
    memset(&m_TrDstDimensions, 0, sizeof(m_TrDstDimensions));
}

//--------------------------------------------------------------------
//! \brief Allocate the channel & class used by this test
//!
RC TegraVicHelper::Setup()
{
    LwRm::Handle hChannel;
    RC rc;

    if (m_IsBackCompatible)
    {
        if (GetVersion() < m_FirstBackCompatibleVersion ||
            GetVersion() > m_LastBackCompatibleVersion)
        {
            Printf(Tee::PriHigh,
                   "Error: This GPU is VIC version %s, but you specified a\n"
                   "     BackCompatibleMode that only supports %s to %s\n",
                   TegraVic::GetVersionName(GetVersion()),
                   TegraVic::GetVersionName(m_FirstBackCompatibleVersion),
                   TegraVic::GetVersionName(m_LastBackCompatibleVersion));
            return RC::BAD_PARAMETER;
        }
        MASSERT(GetEffMaxSlots() <= GetMaxSlots());
    }

    CHECK_RC(m_pTestConfig->AllocateChannel(&m_pCh, &hChannel, LW2080_ENGINE_TYPE_VIC));
    CHECK_RC(m_pCh->SetClass(0, LWE26E_CMD_SETCL_CLASSID_GRAPHICS_VIC));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Run the test.  This is the main body of TegraVic::Run().
//!
RC TegraVicHelper::Run()
{
    const UINT32 startLoop = m_pTestConfig->StartLoop();
    const UINT32 endLoop = startLoop + m_pTestConfig->Loops();
    bool printState = true;
    DEFER
    {
        if (printState)
        {
            this->PrintVicEngineState(Tee::PriNormal);
        }
    };
    RC rc;

    const bool isBackground = m_pTest->m_KeepRunning;

    // Main loop
    //
    for (UINT32 loopNum = startLoop;
         (isBackground && m_pTest->m_KeepRunning) || (loopNum < endLoop);
         ++loopNum)
    {
        if (loopNum >= endLoop)
        {
            loopNum = startLoop;
        }

        Printf(m_Pri, "== Loop %d ==\n", loopNum);
        m_pFpCtx->LoopNum = loopNum;
        m_pFpCtx->Rand.SeedRandom(m_pTestConfig->Seed() + loopNum);

        // Use FancyPickers to get the loop parameters
        //
        CHECK_RC(PickLoopParams());

        // Run the test
        //
        SrcImages srcImages;
        Image dstImage;
        vector<UINT08> cfgBuffer;
        {
            Tasker::DetachThread detach;

            CHECK_RC(AllocSrcImages(&srcImages));
            CHECK_RC(AllocDstImage(&dstImage));
            CHECK_RC(FillConfigStruct(&cfgBuffer, srcImages));
            MASSERT(CheckConfigStruct(&cfgBuffer[0]));
            CHECK_RC(DumpSrcFilesIfNeeded(cfgBuffer, srcImages));
            CHECK_RC(WriteMethods(&cfgBuffer[0], srcImages, dstImage));
            CHECK_RC(DumpDstFilesIfNeeded(dstImage));
        }

        // Goldelwalues call JS, so it cannot be detached from Tasker
        CHECK_RC(m_pGoldelwalues->Run());
    }
    rc = m_pGoldelwalues->ErrorRateTest(m_pTest->GetJSObject());
    if (rc == RC::OK)
        printState = false;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Clean up the data allocated for this test.
//!
//! This is the main body of TegraVic::Cleanup().
//!
RC TegraVicHelper::Cleanup()
{
    StickyRC firstRc;

    m_pGoldelwalues->ClearSurfaces();
    if (m_pCh)
    {
        firstRc = m_pTestConfig->FreeChannel(m_pCh);
        m_pCh = nullptr;
    }
    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Fill the image's surface(s) with a value
//!
RC TegraVicHelper::Image::Fill(UINT32 value)
{
    RC rc;
    if (luma.IsAllocated())
        CHECK_RC(luma.Fill(value));
    if (chromaU.IsAllocated())
        CHECK_RC(chromaU.Fill(value));
    if (chromaV.IsAllocated())
        CHECK_RC(chromaV.Fill(value));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Fill the image's surface(s) with random data
//!
//! \param seed Seed for the random-number generator.  This method
//!     tweaks the seed a bit to provide a different seed to each of
//!     the image's surfaces.
//!
RC TegraVicHelper::Image::FillRandom(UINT32 seed)
{
    Surface2D *pSurfaces[] = { &luma, &chromaU, &chromaV };
    RC rc;

    for (size_t ii = 0; ii < NUMELEMS(pSurfaces); ++ii)
    {
        Surface2D *pSurface = pSurfaces[ii];
        if (pSurface->IsAllocated())
        {
            vector<UINT08> data(pSurface->GetSize());
            CHECK_RC(Memory::FillRandom(
                        &data[0],
                        static_cast<UINT32>(seed * NUMELEMS(pSurfaces) + ii),
                        static_cast<UINT32>(data.size())));
            SurfaceUtils::MappingSurfaceWriter surfaceWriter(*pSurface);
            CHECK_RC(surfaceWriter.WriteBytes(0, &data[0], data.size()));
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write this image's surface(s) to a channel
//!
//! This is just a colwenience method, for consolidating the code that
//! need the following logic:  "If this image has a luma component,
//! then write method XXX_LUMA with the luma surface, else write
//! method XXX_LUMA with 0.  Now do the same for chroma_u and chroma_v
//! components."
//!
RC TegraVicHelper::Image::Write
(
    Channel *pCh,
    UINT32 lumaMethod,
    UINT32 chromaUMethod,
    UINT32 chromaVMethod
) const
{
    const Surface2D *pSurfaces[] = { &luma, &chromaU, &chromaV };
    UINT32 methods[] = { lumaMethod, chromaUMethod, chromaVMethod };
    MASSERT(pCh);
    RC rc;

    MASSERT(NUMELEMS(pSurfaces) == NUMELEMS(methods));
    for (size_t ii = 0; ii < NUMELEMS(pSurfaces); ++ii)
    {
        if (pSurfaces[ii]->IsAllocated())
        {
            CHECK_RC(pCh->WriteWithSurface(0, methods[ii], *pSurfaces[ii],
                                           0, Vic3::SURFACE_SHIFT));
        }
        else
        {
            CHECK_RC(pCh->Write(0, methods[ii], 0));
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Free this image's surface(s).
//!
void TegraVicHelper::Image::Free()
{
    if (luma.IsAllocated())
        luma.Free();
    if (chromaU.IsAllocated())
        chromaU.Free();
    if (chromaV.IsAllocated())
        chromaV.Free();
}

//--------------------------------------------------------------------
//! \brief Free all the stored Image objects
//!
TegraVicHelper::SrcImagesForSlot::~SrcImagesForSlot()
{
    for (SrcImagesForSlot::ImageMap::iterator iter = images.begin();
         iter != images.end(); ++iter)
    {
        if (iter->second)
            delete iter->second;
        iter->second = nullptr;
    }
    images.clear();
}

//--------------------------------------------------------------------
//! \brief Set the width of a rectangle of pixels
//!
//! This method is called from ApplyConstraints() to adjust the
//! rectangles chosen by fancyPicker.  It changes the width of the
//! rectangle, without exceeding the bounds of the image that the
//! rectangle is in.
//!
//! \param newWidth The new width the rectangle should have.  The
//!     caller must ensure this width fits in the surrounding image.
//! \param imageWidth The width of the surrounding image.
//!
//! \sa SetHeight()
//!
void TegraVicHelper::Rect::SetWidth(UINT32 newWidth, UINT32 imageWidth)
{
    MASSERT(newWidth > 0);
    MASSERT(imageWidth > 0);
    MASSERT(newWidth <= imageWidth);
    if (newWidth < GetWidth())
    {
        left += (GetWidth() - newWidth) / 2;
        right = left + newWidth - 1;
    }
    else
    {
        left -= min((newWidth - GetWidth()) / 2, left);
        right = min(left + newWidth - 1, imageWidth - 1);
        left = right - newWidth + 1;
    }
}

//--------------------------------------------------------------------
//! \brief Set the height of a rectangle of pixels
//!
//! \sa SetWidth()
//!
void TegraVicHelper::Rect::SetHeight(UINT32 newHeight, UINT32 imageHeight)
{
    MASSERT(newHeight > 0);
    MASSERT(imageHeight > 0);
    MASSERT(newHeight <= imageHeight);
    if (newHeight < GetHeight())
    {
        top += (GetHeight() - newHeight) / 2;
        bottom = top + newHeight - 1;
    }
    else
    {
        top -= min((newHeight - GetHeight()) / 2, top);
        bottom = min(top + newHeight - 1, imageHeight - 1);
        top = bottom - newHeight + 1;
    }
}

//--------------------------------------------------------------------
//! \brief Tells if the slot is progressive or interlaced, based on frameFormat
//!
//! In an interlaced slot, each image (field) contains either even or
//! odd scanlines, so the image is only half the height of a full
//! frame.  A progressive slot contains a full set of scanlines in
//! each image.  We ignore deinterlaceMode if IsProgressive().
//!
//! \sa IsFrameBased()
//!
bool TegraVicHelper::SlotPicks::IsProgressive() const
{
    return (frameFormat == Vic3::DXVAHD_FRAME_FORMAT_PROGRESSIVE ||
            frameFormat == Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_PROGRESSIVE);
}

//--------------------------------------------------------------------
//! Tells whether the slot is frame-based or field-based, based on frameFormat
//!
//! In a field-based slot, two conselwtive images need to be combined
//! in order to create one frame.  A frame-based slot maps each slot
//! image to a separate destination frame, so "field" and "frame" are
//! the same thing.
//!
//! This is closely related to IsProgressive().  Progressive slots are
//! always frame-based, and *most* interlaced slots are field-based.
//! The exception is BOB deinterlacing, in which an interlaced stream
//! is deinterlaced by stretching it vertically, thereby doubling the
//! frame rate but halving the resolution (and making the video "bob"
//! up and down by about half a pixel).  BOB slots are interlaced and
//! frame-based.
//!
//! \sa IsProgressive()
//!
bool TegraVicHelper::SlotPicks::IsFrameBased() const
{
    switch (frameFormat)
    {
        case Vic3::DXVAHD_FRAME_FORMAT_PROGRESSIVE:
        case Vic3::DXVAHD_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST:
        case Vic3::DXVAHD_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST:
        case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_PROGRESSIVE:
        case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_TOP_FIELD_FIRST:
        case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_BOTTOM_FIELD_FIRST:
            return true;
        default:
            return false;
    }
}

//--------------------------------------------------------------------
//! \brief Tells if the slot's algorithm requires a motion map
//!
bool TegraVicHelper::SlotPicks::HasMotionMap() const
{
    return (IsInterlaced() &&
            deinterlaceMode == Vic3::DXVAHD_DEINTERLACE_MODE_PRIVATE_DISI1);
}

//--------------------------------------------------------------------
//! \brief Return the number of slots supported by the given VIC version
//!
/* static */ UINT32 TegraVicHelper::GetMaxSlotsForVersion
(
    TegraVic::Version version
)
{
    switch (version)
    {
        case TegraVic::VERSION_VIC3:
            return Vic3::MAX_SLOTS;
        case TegraVic::VERSION_VIC4:
            return Vic4::MAX_SLOTS;
        case TegraVic::VERSION_VIC41:
            return Vic41::MAX_SLOTS;
        case TegraVic::VERSION_VIC42:
            return Vic42::MAX_SLOTS;
        default:
            MASSERT(!"Illegal version passed to GetMaxSlotsForVersion");
            return 0;
    }
}

//--------------------------------------------------------------------
//! \brief Return the effective maximum number of slots
//!
//! Normally, this method returns the same value as MaxSlots(): the
//! maximum number of slots supported by this version of VIC.  But in
//! back-compatible mode, it returns the number of lowest number of
//! slots supported by any VIC version in the range.
//!
UINT32 TegraVicHelper::GetEffMaxSlots() const
{
    return GetMaxSlotsForVersion(m_IsBackCompatible ?
                                 m_FirstBackCompatibleVersion :
                                 GetVersion());
}

//--------------------------------------------------------------------
//! \brief Create a ImageDimensions struct
//!
//! \param width The width of the image, in pixels
//! \param frameHeight The height of the deinterlaced frame
//! \param isInterlaced If true, each field is half the frame height.
//!     If false, they have the same height.
//! \param pixelFormat Gives the RGB or YUV format
//! \param blockKind Gives the pitch-linear or block-linear format
//!
/* static */ TegraVicHelper::ImageDimensions TegraVicHelper::GetImageDimensions
(
    UINT32 width,
    UINT32 frameHeight,
    bool isInterlaced,
    Vic3::PIXELFORMAT pixelFormat,
    Vic3::BLK_KIND blockKind
)
{
    ImageDimensions dims = {0};
    UINT32 fieldsPerFrame = isInterlaced ? 2 : 1;

    dims.width       = width;
    dims.frameHeight = frameHeight;
    dims.fieldHeight = frameHeight / fieldsPerFrame;
    dims.pixelFormat = pixelFormat;
    dims.blockKind   = blockKind;

    UINT32 strideAlign = 0;
    UINT32 heightAlign = 0;
    switch (blockKind)
    {
        case Vic3::BLK_KIND_PITCH:
            strideAlign = PITCH_STRIDE_ALIGN;

            // Note: Align allocated surface height up to 4, because we setup VIC with
            // cache width 64Bx4 in source slots.
            // This is also important for destination surface, which is copied with
            // DmaWrapper for CRC-ing, and DmaWrapper can use 32Bx8 cache width for
            // BlockLinear surfaces, however it automatically falls back to 64Bx4 if
            // the surface height is not divisible by 8.
            // VIC needs the pitch height to align to multiple of 16
            // http://lwbugs/1838387/10
            heightAlign = PITCH_HEIGHT_ALIGN;
            break;
        case Vic3::BLK_KIND_GENERIC_16Bx2:
            strideAlign = BLOCK_STRIDE_ALIGN;
            heightAlign = BLOCK_HEIGHT_ALIGN;
            break;
        default:
            MASSERT(!"Unknown block kind");
            break;
    }

    switch (pixelFormat)
    {
        case Vic3::T_A8R8G8B8:
            dims.widthGranularity       = 1;
            dims.frameHeightGranularity = fieldsPerFrame;
            dims.lumaPitch     = ALIGN_UP(width * 4, strideAlign);
            dims.lumaWidth     = dims.lumaPitch / 4;
            dims.lumaHeight    = ALIGN_UP(dims.fieldHeight, heightAlign);
            dims.lumaFormat    = ColorUtils::A8R8G8B8;
            dims.chromaUFormat = ColorUtils::LWFMT_NONE;
            dims.chromaVFormat = ColorUtils::LWFMT_NONE;
            dims.hasAlpha      = true;
            break;
        case Vic3::T_Y8___V8U8_N420:
            dims.widthGranularity       = 2;
            dims.frameHeightGranularity = 2 * fieldsPerFrame;
            dims.lumaPitch      = ALIGN_UP(width, strideAlign);
            dims.lumaWidth      = dims.lumaPitch;
            dims.lumaHeight     = ALIGN_UP(dims.fieldHeight, heightAlign);
            dims.lumaFormat     = ColorUtils::Y8;
            dims.chromaPitch    = ALIGN_UP(width, strideAlign);
            dims.chromaWidth    = dims.chromaPitch / 2;
            dims.chromaHeight   = ALIGN_UP(dims.fieldHeight / 2, heightAlign);
            dims.chromaUFormat  = ColorUtils::V8U8;
            dims.chromaVFormat  = ColorUtils::LWFMT_NONE;
            dims.chromaLocHoriz = Vic3::CHROMA_LOC_EVEN_LUMA;
            dims.chromaLocVert  = Vic3::CHROMA_LOC_EVEN_ODD_LUMA;
            break;
        default:
            MASSERT(!"Unknown pixel format");
            break;
    }

    return dims;
}

//--------------------------------------------------------------------
//! \brief Used to transform fprintf() to Printf() in CheckConfigStruct()
//!
//! The CheckConfigStruct() implementations are mostly copied from the
//! //hw tree.  In order to be able to re-copy the code as needed, the
//! copied code was changed as little as possible.  But the code
//! frequently uses fprintf() to report errors, and for MODS it makes
//! more sense to use Printf().  We can't override the global
//! fprintf(), since we need it elsewhere.  So, as a blatant hack, we
//! use #define/#undef to replace fprintf with this method in the
//! copied code.
//!
/* static */ void TegraVicHelper::FprintfForCheckConfig
(
    FILE*,
    const char *format,
    ...
)
{
    va_list args;
    va_start(args, format);
    ModsExterlwAPrintf(Tee::PriHigh, Tee::GetTeeModuleCoreCode(),
                       Tee::SPS_NORMAL, format, args);
    va_end(args);
}

//--------------------------------------------------------------------
//! \brief Used to transform printf() to Printf() in CheckConfigStruct()
//!
//! See FprintfForCheckConfig for explanation.
//! \sa FprintfForCheckConfig()
//!
/* static */ void TegraVicHelper::PrintfForCheckConfig(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    ModsExterlwAPrintf(Tee::PriHigh, Tee::GetTeeModuleCoreCode(),
                       Tee::SPS_NORMAL, format, args);
    va_end(args);
}

//--------------------------------------------------------------------
//! \brief Allocate a surface filled with the indicated data
//!
//! \param[out] pSurface The surface to allocate
//! \param pData The data to write into the surface.  If nullptr, fill
//!     the surface with 0.
//! \param size The size of the data to write.  Also indicates the
//!     size of the surface to allocate.
//!
RC TegraVicHelper::AllocDataSurface
(
    Surface2D *pSurface,
    const void *pData,
    size_t size
) const
{
    MASSERT(pSurface);
    RC rc;

    pSurface->SetWidth(static_cast<UINT32>(size));
    pSurface->SetHeight(1);
    pSurface->SetColorFormat(ColorUtils::Y8);
    pSurface->SetVASpace(Surface2D::TegraVASpace);
    pSurface->SetAlignment(Vic3::SURFACE_ALIGN);
    CHECK_RC(pSurface->Alloc(m_pGpuDevice));

    if (pData)
    {
        SurfaceUtils::MappingSurfaceWriter surfaceWriter(*pSurface);
        CHECK_RC(surfaceWriter.WriteBytes(0, pData, size));
    }
    else
    {
        CHECK_RC(pSurface->Fill(0));
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Allocate a surface filled with data read from a file
//!
//! \param[out] pSurface The surface to allocate
//! \param filename The file to copy into the surface.  The size of
//!     the file determines the surface size.
//! \param[in, out] pBuffer This is a minor optimization to keep us
//!     from reading the same file over and over.  The data is stored
//!     in this buffer the first time we read the file, and re-used
//!     subsequent times.  The caller must use a different buffer for
//!     each file.
//!
RC TegraVicHelper::AllocDataSurface
(
    Surface2D *pSurface,
    const string &filename,
    vector<UINT08> *pBuffer
) const
{
    MASSERT(pBuffer);
    RC rc;

    if (pBuffer->empty())
    {
        FileHolder fileHolder;
        long fileSize;

        CHECK_RC(fileHolder.Open(filename, "r"));
        CHECK_RC(Utility::FileSize(fileHolder.GetFile(), &fileSize));
        pBuffer->resize(fileSize);
        if (fread(&(*pBuffer)[0], fileSize, 1, fileHolder.GetFile()) != 1)
        {
            Printf(Tee::PriHigh, "Error reading file %s\n", filename.c_str());
            return RC::FILE_READ_ERROR;
        }

        // This function is lwrrently only used for loading the microcode.
        // Align the size of this surface up so it is a multiple of 256B,
        // which is a VIC requirement.
        pBuffer->resize(ALIGN_UP(pBuffer->size(), 256U));
    }
    CHECK_RC(AllocDataSurface(pSurface, &(*pBuffer)[0], pBuffer->size()));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Allocate an image based on an ImageDimensions struct
//!
RC TegraVicHelper::AllocImage(Image *pImage, const ImageDimensions &dims) const
{
    MASSERT(pImage);
    RC rc;

    if (dims.lumaFormat != ColorUtils::LWFMT_NONE)
    {
        pImage->luma.SetWidth(dims.lumaWidth);
        pImage->luma.SetHeight(dims.lumaHeight);
        pImage->luma.SetPitch(dims.lumaPitch);
        pImage->luma.SetColorFormat(dims.lumaFormat);
        pImage->luma.SetVASpace(Surface2D::TegraVASpace);
        pImage->luma.SetAlignment(Vic3::SURFACE_ALIGN);
        CHECK_RC(pImage->luma.Alloc(m_pGpuDevice));
    }
    if (dims.chromaUFormat != ColorUtils::LWFMT_NONE)
    {
        pImage->chromaU.SetWidth(dims.chromaWidth);
        pImage->chromaU.SetHeight(dims.chromaHeight);
        pImage->chromaU.SetPitch(dims.chromaPitch);
        pImage->chromaU.SetColorFormat(dims.chromaUFormat);
        pImage->chromaU.SetVASpace(Surface2D::TegraVASpace);
        pImage->chromaU.SetAlignment(Vic3::SURFACE_ALIGN);
        CHECK_RC(pImage->chromaU.Alloc(m_pGpuDevice));
    }
    if (dims.chromaVFormat != ColorUtils::LWFMT_NONE)
    {
        pImage->chromaV.SetWidth(dims.chromaWidth);
        pImage->chromaV.SetHeight(dims.chromaHeight);
        pImage->chromaV.SetPitch(dims.chromaPitch);
        pImage->chromaV.SetColorFormat(dims.chromaVFormat);
        pImage->chromaV.SetVASpace(Surface2D::TegraVASpace);
        pImage->chromaV.SetAlignment(Vic3::SURFACE_ALIGN);
        CHECK_RC(pImage->chromaV.Alloc(m_pGpuDevice));
    }
    return rc;
}

RC TegraVicHelper::PickBoolParams
(
    const UINT32 pickerParam,
    PickOption pickOption,
    FancyPickerArray &fpk,
    bool *pParam
)
{
    RC rc;
    MASSERT(pParam);
    if (!pParam)
        return RC::SOFTWARE_ERROR;

    // By default enable it.
    *pParam =  true;

    if (pickOption == random)
    {
        *pParam = fpk[pickerParam].Pick();
    }
    else if (pickOption == disable)
    {
        *pParam = false;
    }
    return rc;
}

//---------------------------------------------------------------------
//! \brief Pick the parameters used by this loop of the test
//!
//! Use fancyPickers to get the parameters for this loop, and call
//! ApplyConstraints() to tweak the values as needed.
//!
RC TegraVicHelper::PickLoopParams()
{
    FancyPickerArray &fpk = *m_pTest->GetFancyPickerArray();
    RC rc;

    // Get "global" values from fancyPickers
    //
    m_DstPixelFormat = (m_pTest->m_OutputPixelFormat == _UINT32_MAX) ?
        static_cast<Vic3::PIXELFORMAT>(fpk[FPK_TEGRAVIC_PIXEL_FORMAT].Pick()) :
        static_cast<Vic3::PIXELFORMAT>(m_pTest->m_OutputPixelFormat);
    m_DstBlockKind   = (m_pTest->m_OutputBlockKind == _UINT08_MAX) ?
        static_cast<Vic3::BLK_KIND>(fpk[FPK_TEGRAVIC_BLOCK_KIND].Pick()) :
        static_cast<Vic3::BLK_KIND>(m_pTest->m_OutputBlockKind);
    CHECK_RC(PickBoolParams(FPK_TEGRAVIC_FLIPX, (m_pTest->m_EnableFlipX == _UINT08_MAX) ? random :
        (m_pTest->m_EnableFlipX == 0) ? disable : enable,
        *m_pTest->GetFancyPickerArray(), &m_FlipX));
    CHECK_RC(PickBoolParams(FPK_TEGRAVIC_FLIPY, (m_pTest->m_EnableFlipY == _UINT08_MAX) ? random :
        (m_pTest->m_EnableFlipY == 0) ? disable : enable,
        *m_pTest->GetFancyPickerArray(), &m_FlipY));
    m_Transpose      = (fpk[FPK_TEGRAVIC_TRANSPOSE].Pick() != 0);
    m_DstImageWidth  =  fpk[FPK_TEGRAVIC_IMAGE_WIDTH].Pick();
    m_DstImageHeight =  fpk[FPK_TEGRAVIC_IMAGE_HEIGHT].Pick();
    m_PercentSlots   =  fpk[FPK_TEGRAVIC_PERCENT_SLOTS].FPick();

    // Get slot-specific values from fancyPickers.
    //
    // This code picks values for the maximum number of slots, even
    // though we expect to only use m_PercentSlots % of them.  The
    // rationale is that (a) we might adjust m_PercentSlots in
    // ApplyConstraints, and (b) we want this code to always select
    // the same number of random values, so that overriding one picker
    // during debugging doesn't change all the other random numbers
    // after that point.
    //
    m_SlotPicks.resize(GetEffMaxSlots());
    for (vector<SlotPicks>::iterator pSlotPicks = m_SlotPicks.begin();
         pSlotPicks != m_SlotPicks.end(); ++pSlotPicks)
    {
        pSlotPicks->frameFormat     = (m_pTest->m_FrameFormat == _UINT32_MAX) ?
            static_cast<Vic3::DXVAHD_FRAME_FORMAT>(fpk[FPK_TEGRAVIC_FRAME_FORMAT].Pick()) :
            static_cast<Vic3::DXVAHD_FRAME_FORMAT>(m_pTest->m_FrameFormat);
        pSlotPicks->deinterlaceMode = (m_pTest->m_DeInterlaceMode == _UINT08_MAX) ?
            static_cast<Vic3::DXVAHD_DEINTERLACE_MODE_PRIVATE>(
            fpk[FPK_TEGRAVIC_DEINTERLACE_MODE].Pick()) :
            static_cast<Vic3::DXVAHD_DEINTERLACE_MODE_PRIVATE>(m_pTest->m_DeInterlaceMode);
        pSlotPicks->pixelFormat    = (m_pTest->m_InputPixelFormat == _UINT32_MAX) ?
            static_cast<Vic3::PIXELFORMAT>(fpk[FPK_TEGRAVIC_PIXEL_FORMAT].Pick()) :
            static_cast<Vic3::PIXELFORMAT>(m_pTest->m_InputPixelFormat);
        pSlotPicks->blockKind      = (m_pTest->m_InputBlockKind == _UINT08_MAX) ?
            static_cast<Vic3::BLK_KIND>(fpk[FPK_TEGRAVIC_BLOCK_KIND].Pick()) :
            static_cast<Vic3::BLK_KIND>(m_pTest->m_InputBlockKind);
        pSlotPicks->imageWidth     =  fpk[FPK_TEGRAVIC_IMAGE_WIDTH].Pick();
        pSlotPicks->imageHeight    =  fpk[FPK_TEGRAVIC_IMAGE_HEIGHT].Pick();
        CHECK_RC(PickBoolParams(FPK_TEGRAVIC_DENOISE,
            (m_pTest->m_EnableNoiseReduction == _UINT08_MAX) ? random :
            (m_pTest->m_EnableNoiseReduction == 0) ? disable : enable,
            *m_pTest->GetFancyPickerArray(), &pSlotPicks->deNoise));
        CHECK_RC(PickBoolParams(FPK_TEGRAVIC_CADENCE_DETECT,
            (m_pTest->m_EnableCadenceDetection == _UINT08_MAX) ? random :
            (m_pTest->m_EnableCadenceDetection == 0) ? disable : enable,
            *m_pTest->GetFancyPickerArray(), &pSlotPicks->cadenceDetect));
        pSlotPicks->srcRect.left   =  fpk[FPK_TEGRAVIC_RECTX].Pick();
        pSlotPicks->srcRect.right  =  fpk[FPK_TEGRAVIC_RECTX].Pick();
        pSlotPicks->srcRect.top    =  fpk[FPK_TEGRAVIC_RECTY].Pick();
        pSlotPicks->srcRect.bottom =  fpk[FPK_TEGRAVIC_RECTY].Pick();
        pSlotPicks->dstRect.left   =  fpk[FPK_TEGRAVIC_RECTX].Pick();
        pSlotPicks->dstRect.right  =  fpk[FPK_TEGRAVIC_RECTX].Pick();
        pSlotPicks->dstRect.top    =  fpk[FPK_TEGRAVIC_RECTY].Pick();
        pSlotPicks->dstRect.bottom =  fpk[FPK_TEGRAVIC_RECTY].Pick();
        pSlotPicks->lightLevel     =  fpk[FPK_TEGRAVIC_LIGHT_LEVEL].Pick();
        // pSlotPicks->deGammaMode is picked in ApplyConstraints
        // Since it might change with change in pixel formats
        for (UINT32 xIndex = 0; xIndex < 3; xIndex++)
        {
            for (UINT32 yIndex = 0; yIndex < 4; yIndex++)
            {
                pSlotPicks->gamutMatrix[xIndex][yIndex] = fpk[FPK_TEGRAVIC_GAMUT_MATRIX].Pick();
            }
        }
    }

    // Adjust the picked values to meet constraints.
    //
    CHECK_RC(ApplyConstraints());

    // Print the picked values
    //
    Printf(m_Pri, "Picks:\n");

    const char *fmt  = "    %-24s = %u\n";
    const char *fmt2 = "    %-24s = %s\n";
    string percentSlotsDesc = Utility::StrPrintf(
            "%u%% (%u slots)", m_PercentSlots,
            static_cast<unsigned>(GetNumSlots()));
    Printf(m_Pri, fmt,  "DstPixelFormat", m_DstPixelFormat);
    Printf(m_Pri, fmt,  "DstBlockKind",   m_DstBlockKind);
    Printf(m_Pri, fmt,  "FlipX",          m_FlipX);
    Printf(m_Pri, fmt,  "FlipY",          m_FlipY);
    Printf(m_Pri, fmt,  "Transpose",      m_Transpose);
    Printf(m_Pri, fmt,  "DstImageWidth",  m_DstImageWidth);
    Printf(m_Pri, fmt,  "DstImageHeight", m_DstImageHeight);
    Printf(m_Pri, fmt2, "PercentSlots",   percentSlotsDesc.c_str());

    for (UINT32 slotIdx = 0; slotIdx < GetNumSlots(); ++slotIdx)
    {
        string fmtString =
            Utility::StrPrintf("    Slot[%u].%%-23s = %%u\n", slotIdx);
        const char *fmt = fmtString.c_str();
        SlotPicks *pSlotPicks = &m_SlotPicks[slotIdx];
        Printf(m_Pri, fmt, "FrameFormat",     pSlotPicks->frameFormat);
        Printf(m_Pri, fmt, "DeinterlaceMode", pSlotPicks->deinterlaceMode);
        Printf(m_Pri, fmt, "PixelFormat",     pSlotPicks->pixelFormat);
        Printf(m_Pri, fmt, "BlockKind",       pSlotPicks->blockKind);
        Printf(m_Pri, fmt, "ImageWidth",      pSlotPicks->imageWidth);
        Printf(m_Pri, fmt, "ImageHeight",     pSlotPicks->imageHeight);
        Printf(m_Pri, fmt, "DeNoise",         pSlotPicks->deNoise);
        Printf(m_Pri, fmt, "LightLevel",      pSlotPicks->lightLevel);
        Printf(m_Pri, fmt, "CadenceDetect",   pSlotPicks->cadenceDetect);
        Printf(m_Pri, fmt, "SrcRect.left",    pSlotPicks->srcRect.left);
        Printf(m_Pri, fmt, "SrcRect.right",   pSlotPicks->srcRect.right);
        Printf(m_Pri, fmt, "SrcRect.top",     pSlotPicks->srcRect.top);
        Printf(m_Pri, fmt, "SrcRect.bottom",  pSlotPicks->srcRect.bottom);
        Printf(m_Pri, fmt, "DstRect.left",    pSlotPicks->dstRect.left);
        Printf(m_Pri, fmt, "DstRect.right",   pSlotPicks->dstRect.right);
        Printf(m_Pri, fmt, "DstRect.top",     pSlotPicks->dstRect.top);
        Printf(m_Pri, fmt, "DstRect.bottom",  pSlotPicks->dstRect.bottom);
        Printf(m_Pri, fmt, "DeGammaMode",     pSlotPicks->deGammaMode);
        for (UINT32 xIndex = 0; xIndex < 3; xIndex++)
        {
            for (UINT32 yIndex = 0; yIndex < 4; yIndex++)
            {
                Printf(m_Pri, "    Slot[%u].GamutMatrix.Coeff[%u][%u] = 0x%x\n",
                    slotIdx, xIndex, yIndex, pSlotPicks->gamutMatrix[xIndex][yIndex]);
            }
        }
    }

    return rc;
}

//---------------------------------------------------------------------
//! \brief Apply constraints on the fancypicker values
//!
//! Long rant on implementation details:
//!
//! In order to keep the network of constraints from getting too
//! complicated, the code attempts to apply constraints in a
//! propagation order.  For the most part, the pickers in fpk_comm.h
//! and the variables in tegravic.h are sorted by propagation order.
//!
//! Global (non-slot-specific) picks come before slot-specific picks
//! in the propagation order.
//!
//! The picks for an image (either slot or dst) propagate in the
//! following order:
//!        IsProgressive (derived from frameFormat)
//!     => deinterlaceMode
//!     => frameFormat (without changing IsProgressive)
//!     => pixelFormat
//!     => blockKind
//!     => flipX, flipY, transpose
//!     => imageWidth, imageHeight
//!     => deNoise, cadenceDetect
//!     => srcRect, dstRect
//!
//! Some ratonale for the above order:
//! * DeinterlaceMode doesn't apply for progressive slots, and should
//!   be zeroed-out.  Hence IsProgressive() comes before
//!   deinterlaceMode.
//! * Aside from the previous bullet point, deinterlaceMode is
//!   probably the most "interesting" pick, with the biggest effect on
//!   slot behavior and constraints.  So it goes near the top.  It
//!   would be at the very top if VIC had implemented "progressive" as
//!   one type of deinterlaceMode, which it arguably can be viewed as.
//! * Aside from deciding the all-important "progressive vs
//!   interlaced" question, frameFormat isn't as important as
//!   deinterlaceMode.  Which is why it sort of appears twice in the
//!   propagation order.
//! * pixelFormat is also pretty interesting, with almost as many
//!   constraints as deinterlaceMode.  So it went next.
//!
//! After those first few items, the propagation order gets more
//! arbitrary:
//! * imageWidth and imageHeight get some of their padding &
//!   granularity requirements from pixelFormat, blockKind, and
//!   transpose.
//! * srcRect and dstRect are constrained to lie within their
//!   corresponding images, and are also constrained to be within
//!   about an order of magnitude of each other.  The fact that
//!   dstRect has constraints with both srcRect (a slot-specific
//!   value) and dstImageWidth/dstImageHeight (global values) was the
//!   major reason why global values are before slot-specific ones in
//!   the propagation: so that both would be stable before we
//!   constrained dstRect.
//!
RC TegraVicHelper::ApplyConstraints()
{
    FancyPickerArray &fpk = *m_pTest->GetFancyPickerArray();
    // Apply pixelFormat => blockKind constraints on the dst image
    //
    switch (m_DstPixelFormat)
    {
        case Vic3::T_A8R8G8B8:
            m_DstBlockKind = Vic3::BLK_KIND_PITCH;
            break;
        case Vic3::T_Y8___V8U8_N420:
            break;
        default:
            MASSERT(!"Unknown pixelFormat");
            return RC::SOFTWARE_ERROR;
    }

    // Apply pixelFormat => transpose constraints on dst image.
    //
    if (m_IsBackCompatible &&
        m_FirstBackCompatibleVersion < TegraVic::VERSION_VIC41 &&
        m_LastBackCompatibleVersion >= TegraVic::VERSION_VIC41 &&
        m_DstPixelFormat == Vic3::T_Y8___V8U8_N420)
    {
        // The algorithm for interpolating U & V in transposed images
        // changed in 4.1.
        //
        m_Transpose = false;
    }

    // Apply constraints on the width/height of the dst image.
    //
    m_DstImageWidth = max(m_DstImageWidth, MIN_RECT_WIDTH);
    m_DstImageHeight = max(m_DstImageHeight, MIN_RECT_HEIGHT);
    if (m_Transpose)
    {
        ImageDimensions tmpDims = GetImageDimensions(
                m_DstImageHeight, m_DstImageWidth, false,
                m_DstPixelFormat, m_DstBlockKind);
        m_DstImageWidth = ALIGN_UP(m_DstImageWidth,
                                   tmpDims.frameHeightGranularity);
        m_DstImageHeight = ALIGN_UP(m_DstImageHeight,
                                    tmpDims.widthGranularity);
    }
    else
    {
        ImageDimensions tmpDims = GetImageDimensions(
                m_DstImageWidth, m_DstImageHeight, false,
                m_DstPixelFormat, m_DstBlockKind);
        m_DstImageWidth = ALIGN_UP(m_DstImageWidth,
                                   tmpDims.widthGranularity);
        m_DstImageHeight = ALIGN_UP(m_DstImageHeight,
                                    tmpDims.frameHeightGranularity);
    }

    // Resize m_SlotPicks to match the percentage of slots we chose
    //
    UINT32 numSlots =
        static_cast<UINT32>(GetEffMaxSlots() * m_PercentSlots / 100.0 + 0.5);
    numSlots = min(max<UINT32>(numSlots, 1), GetEffMaxSlots());
    MASSERT(numSlots <= m_SlotPicks.size());
    m_SlotPicks.resize(numSlots);

    // Apply slot-specific constraints.
    //
    for (vector<SlotPicks>::iterator pSlotPicks = m_SlotPicks.begin();
         pSlotPicks != m_SlotPicks.end(); ++pSlotPicks)
    {
        // Apply global constraints on deinterlaceMode
        //
        if (m_IsBackCompatible &&
            m_FirstBackCompatibleVersion < TegraVic::VERSION_VIC41 &&
            m_LastBackCompatibleVersion >= TegraVic::VERSION_VIC41 &&
            pSlotPicks->IsInterlaced())
        {
            // The algorithm for interpolating U & V components in
            // source images changed in 4.1.  YUV is mandatory for a
            // lot of interlaced-related features, so force
            // progressive here.
            //
            switch (pSlotPicks->frameFormat)
            {
                case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_TOP_FIELD_FIRST:
                case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_BOTTOM_FIELD_FIRST:
                case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_TOP_FIELD:
                case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_BOTTOM_FIELD:
                case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_TOP_FIELD_CHROMA_BOTTOM:
                case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_BOTTOM_FIELD_CHROMA_TOP:
                    pSlotPicks->frameFormat =
                        Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_PROGRESSIVE;
                    break;
                default:
                    pSlotPicks->frameFormat =
                        Vic3::DXVAHD_FRAME_FORMAT_PROGRESSIVE;
                    break;
            }
        }

        // Apply all frameFormat <=> deinterlaceMode constraints.
        //
        if (pSlotPicks->IsProgressive())
        {
            pSlotPicks->deinterlaceMode =
                static_cast<Vic3::DXVAHD_DEINTERLACE_MODE_PRIVATE>(0);
        }
        else if (pSlotPicks->deinterlaceMode ==
                 Vic3::DXVAHD_DEINTERLACE_MODE_PRIVATE_BOB)
        {
            // For BOB deinterlacing, find a frame-based frameFormat
            // that's similar to the picked value.
            //
            switch (pSlotPicks->frameFormat)
            {
                case Vic3::DXVAHD_FRAME_FORMAT_TOP_FIELD:
                case Vic3::DXVAHD_FRAME_FORMAT_TOP_FIELD_CHROMA_BOTTOM:
                    pSlotPicks->frameFormat =
                        Vic3::DXVAHD_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
                    break;
                case Vic3::DXVAHD_FRAME_FORMAT_BOTTOM_FIELD:
                case Vic3::DXVAHD_FRAME_FORMAT_BOTTOM_FIELD_CHROMA_TOP:
                    pSlotPicks->frameFormat =
                        Vic3::DXVAHD_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
                    break;
                case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_TOP_FIELD:
                case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_TOP_FIELD_CHROMA_BOTTOM:
                    pSlotPicks->frameFormat =
                        Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_TOP_FIELD_FIRST;
                    break;
                case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_BOTTOM_FIELD:
                case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_BOTTOM_FIELD_CHROMA_TOP:
                    pSlotPicks->frameFormat =
                        Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_BOTTOM_FIELD_FIRST;
                    break;
                default:
                    break;
            }
            MASSERT(pSlotPicks->IsFrameBased());
        }
        else
        {
            // For all deinterlacing except BOB, find a field-based
            // frameFormat that's similar to the picked value.
            //
            switch (pSlotPicks->frameFormat)
            {
                case Vic3::DXVAHD_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST:
                    pSlotPicks->frameFormat =
                        Vic3::DXVAHD_FRAME_FORMAT_TOP_FIELD;
                    break;
                case Vic3::DXVAHD_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST:
                    pSlotPicks->frameFormat =
                        Vic3::DXVAHD_FRAME_FORMAT_BOTTOM_FIELD;
                    break;
                case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_TOP_FIELD_FIRST:
                    pSlotPicks->frameFormat =
                        Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_TOP_FIELD;
                    break;
                case Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_BOTTOM_FIELD_FIRST:
                    pSlotPicks->frameFormat =
                        Vic3::DXVAHD_FRAME_FORMAT_SUBPIC_BOTTOM_FIELD;
                    break;
                default:
                    break;
            }
            MASSERT(pSlotPicks->IsFieldBased());
        }

        // Apply constraints on pixelFormat
        //
        bool forceYUV = false;
        bool forceRGB = false;
        if (GetVersion() < TegraVic::VERSION_VIC4 &&
            pSlotPicks->IsInterlaced())
        {
            // YUV was mandatory for interlaced mode before VIC4
            forceYUV = true;
        }
        if (pSlotPicks->HasMotionMap())
        {
            forceYUV = true;
        }
        if (m_IsBackCompatible &&
            m_FirstBackCompatibleVersion < TegraVic::VERSION_VIC41 &&
            m_LastBackCompatibleVersion >= TegraVic::VERSION_VIC41)
        {
            // The algorithm for interpolating U & V components in
            // source images changed in 4.1.
            forceRGB = true;
        }
        MASSERT(!(forceYUV && forceRGB));

        switch (pSlotPicks->pixelFormat)
        {
            case Vic3::T_A8R8G8B8:
                if (forceYUV)
                    pSlotPicks->pixelFormat = Vic3::T_Y8___V8U8_N420;
                break;
            case Vic3::T_Y8___V8U8_N420:
                if (forceRGB)
                    pSlotPicks->pixelFormat = Vic3::T_A8R8G8B8;
                break;
            default:
                MASSERT(!"Unknown pixelFormat");
                return RC::SOFTWARE_ERROR;
        }

        // Propagate constraints from pixelFormat to items further down
        //
        switch (pSlotPicks->pixelFormat)
        {
            case Vic3::T_A8R8G8B8:
                pSlotPicks->blockKind = Vic3::BLK_KIND_PITCH;
                pSlotPicks->deNoise = false;
                pSlotPicks->deGammaMode = fpk[FPK_TEGRAVIC_RGB_DEGAMMAMODE].Pick();
                break;
            case Vic3::T_Y8___V8U8_N420:
                if (GetVersion() < TegraVic::VERSION_VIC42)
                    pSlotPicks->deGammaMode = Vic42::GAMMA_MODE_REC709;
                else
                    pSlotPicks->deGammaMode = fpk[FPK_TEGRAVIC_YUV_DEGAMMAMODE].Pick();
                break;
            default:
                MASSERT(!"Unknown pixelFormat");
                return RC::SOFTWARE_ERROR;
        }

        // Apply constraints on the width/height
        //
        bool srcRectUsesFieldCoords = (pSlotPicks->IsInterlaced() &&
                                       pSlotPicks->IsFrameBased());
        pSlotPicks->imageWidth = max(pSlotPicks->imageWidth, MIN_RECT_WIDTH);
        pSlotPicks->imageHeight = max(
                pSlotPicks->imageHeight,
                srcRectUsesFieldCoords ? 2 * MIN_RECT_HEIGHT : MIN_RECT_HEIGHT);

        ImageDimensions tmpDims = GetImageDimensions(
                pSlotPicks->imageWidth, pSlotPicks->imageHeight,
                pSlotPicks->IsInterlaced(), pSlotPicks->pixelFormat,
                pSlotPicks->blockKind);
        pSlotPicks->imageWidth = ALIGN_UP(pSlotPicks->imageWidth,
                                          tmpDims.widthGranularity);
        pSlotPicks->imageHeight = ALIGN_UP(pSlotPicks->imageHeight,
                                           tmpDims.frameHeightGranularity);

        // Apply constraints on cadence detection
        //
        if (pSlotPicks->IsFrameBased() ||
            pSlotPicks->pixelFormat == Vic3::T_A8R8G8B8)
        {
            pSlotPicks->cadenceDetect = false;
        }

        // Adjust the source and dest rectangles
        //
        Rect *pSrcRect = &pSlotPicks->srcRect;
        Rect *pDstRect = &pSlotPicks->dstRect;

        UINT32 effSrcImageHeight = pSlotPicks->imageHeight;
        if (srcRectUsesFieldCoords)
        {
            // Handle the oddball case where the source rectangle is
            // in field coordinates instead of frame coordinates.
            // This should only affect BOB deinterlacing.
            //
            pSrcRect->top     /= 2;
            pSrcRect->bottom  /= 2;
            effSrcImageHeight /= 2;
        }

        // (1) Basic sanity: make sure left <= right, top <= bottom,
        //     and that all coordinates lie within the surrounding image.
        //
        pSrcRect->left   = min(pSrcRect->left,   pSlotPicks->imageWidth - 1);
        pSrcRect->right  = min(pSrcRect->right,  pSlotPicks->imageWidth - 1);
        pSrcRect->top    = min(pSrcRect->top,    effSrcImageHeight - 1);
        pSrcRect->bottom = min(pSrcRect->bottom, effSrcImageHeight - 1);
        pDstRect->left   = min(pDstRect->left,   m_DstImageWidth - 1);
        pDstRect->right  = min(pDstRect->right,  m_DstImageWidth - 1);
        pDstRect->top    = min(pDstRect->top,    m_DstImageHeight - 1);
        pDstRect->bottom = min(pDstRect->bottom, m_DstImageHeight - 1);

        if (pSrcRect->left > pSrcRect->right)
            swap(pSrcRect->left, pSrcRect->right);
        if (pSrcRect->top > pSrcRect->bottom)
            swap(pSrcRect->top, pSrcRect->bottom);
        if (pDstRect->left > pDstRect->right)
            swap(pDstRect->left, pDstRect->right);
        if (pDstRect->top > pDstRect->bottom)
            swap(pDstRect->top, pDstRect->bottom);

        // (2) Make sure the rectangles meet the minimum width/height
        //     specs.  SetWidth() & SetHeight() will assert if the
        //     surrounding image is too small, so we checked earlier
        //     to make sure the images are big enough to hold the
        //     smallest possible rectangle.
        //
        if (pSrcRect->GetWidth() < MIN_RECT_WIDTH)
            pSrcRect->SetWidth(MIN_RECT_WIDTH, pSlotPicks->imageWidth);
        if (pSrcRect->GetHeight() < MIN_RECT_HEIGHT)
            pSrcRect->SetHeight(MIN_RECT_HEIGHT, effSrcImageHeight);
        if (pDstRect->GetWidth() < MIN_RECT_WIDTH)
            pDstRect->SetWidth(MIN_RECT_WIDTH, m_DstImageWidth);
        if (pDstRect->GetHeight() < MIN_RECT_HEIGHT)
            pDstRect->SetHeight(MIN_RECT_HEIGHT, m_DstImageHeight);

        // (3) Make sure the rectangles are within MAX_SCALING_FACTOR
        //     of each other.  We do this by shrinking the larger
        //     rectangle, since that's easier than checking to make
        //     sure the surrounding image is big enough to expand the
        //     smaller rectangle.
        //
        if (pSrcRect->GetWidth() > pDstRect->GetWidth() * MAX_SCALING_FACTOR)
            pSrcRect->SetWidth(pDstRect->GetWidth() * MAX_SCALING_FACTOR,
                               pSlotPicks->imageWidth);
        if (pSrcRect->GetHeight() > pDstRect->GetHeight() * MAX_SCALING_FACTOR)
            pSrcRect->SetHeight(pDstRect->GetHeight() * MAX_SCALING_FACTOR,
                                effSrcImageHeight);
        if (pDstRect->GetWidth() > pSrcRect->GetWidth() * MAX_SCALING_FACTOR)
            pDstRect->SetWidth(pSrcRect->GetWidth() * MAX_SCALING_FACTOR,
                               m_DstImageWidth);
        if (pDstRect->GetHeight() > pSrcRect->GetHeight() * MAX_SCALING_FACTOR)
            pDstRect->SetHeight(pSrcRect->GetHeight() * MAX_SCALING_FACTOR,
                                m_DstImageHeight);
    }

    return RC::OK;
}

//---------------------------------------------------------------------
//! \brief Allocate all images for the slots
//!
//! This method is called once per loop to allocate the images used by
//! the slots, including the Surface2Ds in the images.  The
//! fancypicker selections determine the number of slots, the number
//! of images per slot, the image dimensions, etc.
//!
//! \param[out] pSrcImages On exit, holds the images.
//!
RC TegraVicHelper::AllocSrcImages(SrcImages *pSrcImages)
{
    MASSERT(pSrcImages);
    const UINT32 fillSeed = m_pFpCtx->Rand.GetRandom();
    RC rc;

    pSrcImages->clear();
    pSrcImages->resize(GetNumSlots());

    // Loop through the slots
    //
    for (size_t slotIdx = 0; slotIdx < GetNumSlots(); ++slotIdx)
    {
        const SlotPicks &slotPicks = m_SlotPicks[slotIdx];

        // First, determine which images (LWRRENT_FIELD, PREV_FIELD,
        // etc) need to be allocated for the slot, based on the
        // algorithms selected by the fancypickers.
        //
        set<Vic3::SurfaceIndex> imagesToAllocate;

        imagesToAllocate.insert(Vic3::LWRRENT_FIELD);
        if (slotPicks.cadenceDetect)
        {
            imagesToAllocate.insert(Vic3::PREV_FIELD);
            imagesToAllocate.insert(Vic3::NEXT_FIELD);
        }
        if (slotPicks.deNoise)
        {
            imagesToAllocate.insert(Vic3::PREV_FIELD);
            imagesToAllocate.insert(Vic3::NEXT_FIELD);
            if (slotPicks.IsInterlaced())
                imagesToAllocate.insert(Vic3::NEXT_FIELD_N);
        }
        if (slotPicks.HasMotionMap())
        {
            imagesToAllocate.insert(Vic3::PREV_FIELD);
            imagesToAllocate.insert(Vic3::NEXT_FIELD);
            imagesToAllocate.insert(Vic3::LWRR_M_FIELD);
            imagesToAllocate.insert(Vic3::PREV_M_FIELD);
            imagesToAllocate.insert(Vic3::COMB_M_FIELD);
        }
        if (slotPicks.IsFieldBased())
        {
            imagesToAllocate.insert(Vic3::PREV_FIELD);
        }

        // Get the image dimensions.  All images in the slot have the
        // same dimensions.
        //
        ImageDimensions &dimensions = (*pSrcImages)[slotIdx].dimensions;
        dimensions = GetImageDimensions(slotPicks.imageWidth,
                                        slotPicks.imageHeight,
                                        slotPicks.IsInterlaced(),
                                        slotPicks.pixelFormat,
                                        slotPicks.blockKind);

        // Allocate the images.
        //
        for (set<Vic3::SurfaceIndex>::const_iterator pSurfaceIdx =
             imagesToAllocate.begin();
             pSurfaceIdx != imagesToAllocate.end(); ++pSurfaceIdx)
        {
            Image *pNewImage = new Image();
            (*pSrcImages)[slotIdx].images[*pSurfaceIdx] = pNewImage;
            CHECK_RC(AllocImage(pNewImage, dimensions));
            CHECK_RC(pNewImage->FillRandom(fillSeed +
                                           *pSurfaceIdx * GetEffMaxSlots() +
                                           slotIdx));
        }
    }
    return rc;
}

//---------------------------------------------------------------------
//! \brief Allocate the destination image, and configure goldens for it
//!
RC TegraVicHelper::AllocDstImage(Image *pDstImage)
{
    MASSERT(pDstImage);
    RC rc;

    // Allocate the dst image
    //
    m_TrDstDimensions = GetImageDimensions(
            m_Transpose ? m_DstImageHeight : m_DstImageWidth,
            m_Transpose ? m_DstImageWidth : m_DstImageHeight,
            false, m_DstPixelFormat, m_DstBlockKind);
    CHECK_RC(AllocImage(pDstImage, m_TrDstDimensions));
    CHECK_RC(pDstImage->Fill(0));

    // Configure the golden-values objects to capture GVs from the dst
    // image.  Each loop captures 1-3 golden values, depending on how
    // many components the image has.
    //
    Surface2D *pSurfaces[] =
        { &pDstImage->luma, &pDstImage->chromaU, &pDstImage->chromaV };
    const char *surfaceNames[] = { "DstLuma", "DstChromaU", "DstChromaV" };
    MASSERT(NUMELEMS(pSurfaces) == NUMELEMS(surfaceNames));
    UINT32 numAllocatedSurfaces = 0;
    for (size_t ii = 0; ii < NUMELEMS(pSurfaces); ++ii)
    {
        if (pSurfaces[ii]->IsAllocated())
            ++numAllocatedSurfaces;
    }
    m_GoldenSurfaces.SetNumSurfaces(numAllocatedSurfaces);
    UINT32 surfaceNum = 0;
    for (size_t ii = 0; ii < NUMELEMS(pSurfaces); ++ii)
    {
        if (pSurfaces[ii]->IsAllocated())
        {
            CHECK_RC(pSurfaces[ii]->Map());
            CHECK_RC(m_GoldenSurfaces.DescribeSurface(
                        0,
                        pSurfaces[ii]->GetWidth(),
                        pSurfaces[ii]->GetHeight(),
                        pSurfaces[ii]->GetPitch(),
                        pSurfaces[ii]->GetColorFormat(),
                        pSurfaces[ii]->GetAddress(),
                        surfaceNum,
                        surfaceNames[ii]));
            ++surfaceNum;
        }
    }
    CHECK_RC(m_pGoldelwalues->SetSurfaces(&m_GoldenSurfaces));
    m_pGoldelwalues->SetLoop(m_pFpCtx->LoopNum);

    return rc;
}

//---------------------------------------------------------------------
//! \brief Optionally dump source images to files, based on DumpFiles testarg
//!
//! \param cfgBuffer Buffer containing the config struct
//! \param srcImages The source images for each slot
//!
RC TegraVicHelper::DumpSrcFilesIfNeeded
(
    const vector<UINT08> &cfgBuffer,
    const SrcImages &srcImages
) const
{
    RC rc;

    // Optionally dump the config buffer as a text file
    //
    if (m_pTest->m_DumpFiles & TegraVic::DUMP_TEXT)
    {
        string cfgName = Utility::StrPrintf("TegraVicConfig%d.txt",
                                            m_pFpCtx->LoopNum);
        FileHolder cfgFile;
        CHECK_RC(cfgFile.Open(cfgName, "w"));
        CHECK_RC(DumpConfigStruct(cfgFile.GetFile(), &cfgBuffer[0]));
        cfgFile.Close();
    }

    // Optionally dump the config buffer and slot-specific surfaces as
    // binary files
    //
    if (m_pTest->m_DumpFiles & TegraVic::DUMP_BINARY)
    {
        string cfgName = Utility::StrPrintf("TegraVicConfig%d.bin",
                                            m_pFpCtx->LoopNum);
        FileHolder cfgFile;
        CHECK_RC(cfgFile.Open(cfgName, "w"));
        if (fwrite(&cfgBuffer[0], cfgBuffer.size(), 1, cfgFile.GetFile()) != 1)
        {
            Printf(Tee::PriHigh, "Error writing file %s\n", cfgName.c_str());
            return RC::FILE_WRITE_ERROR;
        }
        cfgFile.Close();
        for (UINT32 slotIdx = 0; slotIdx < srcImages.size(); ++slotIdx)
        {
            for (SrcImagesForSlot::ImageMap::const_iterator
                 iter = srcImages[slotIdx].images.begin();
                 iter != srcImages[slotIdx].images.end(); ++iter)
            {
                UINT32 surfaceIdx = iter->first;
                const Image &image = *iter->second;
                string suffix = Utility::StrPrintf("%dSlot%dSurf%d.bin",
                                                   m_pFpCtx->LoopNum,
                                                   slotIdx, surfaceIdx);
                CHECK_RC(DumpSurfaceFile("TegraVicLuma" + suffix, image.luma));
                CHECK_RC(DumpSurfaceFile("TegraVicChromaU" + suffix,
                                         image.chromaU));
                CHECK_RC(DumpSurfaceFile("TegraVicChromaV" + suffix,
                                         image.chromaV));
            }
        }
    }
    return rc;
}

//---------------------------------------------------------------------
//! \brief Optionally dump dst image to files, based on DumpFiles testarg
//!
RC TegraVicHelper::DumpDstFilesIfNeeded(const Image &dstImage) const
{
    RC rc;
    if (m_pTest->m_DumpFiles & TegraVic::DUMP_BINARY)
    {
        const string suffix = Utility::StrPrintf("%d.bin", m_pFpCtx->LoopNum);
        CHECK_RC(DumpSurfaceFile("TegraVicDstLuma" + suffix,
                                 dstImage.luma));
        CHECK_RC(DumpSurfaceFile("TegraVicDstChromaU" + suffix,
                                 dstImage.chromaU));
        CHECK_RC(DumpSurfaceFile("TegraVicDstChromaV" + suffix,
                                 dstImage.chromaV));
    }
    return rc;
}

//---------------------------------------------------------------------
//! \brief Dump a surface as a binary file
//!
//! Used by DumpSrcFilesIfNeeded() and DumpDstFilesIfNeeded()
//!
/* static */ RC TegraVicHelper::DumpSurfaceFile
(
    const string &filename,
    const Surface2D &surface
)
{
    RC rc;
    if (surface.IsAllocated())
    {
        vector<UINT08> data;
        SurfaceUtils::MappingSurfaceReader surfaceReader(surface);
        data.resize(surface.GetSize());
        CHECK_RC(surfaceReader.ReadBytes(0, &data[0], data.size()));

        FileHolder fileHolder;
        CHECK_RC(fileHolder.Open(filename, "w"));
        if (fwrite(&data[0], data.size(), 1, fileHolder.GetFile()) != 1)
        {
            Printf(Tee::PriHigh, "Error writing file %s\n", filename.c_str());
            return RC::FILE_WRITE_ERROR;
        }
        fileHolder.Close();
    }
    return rc;
}

//---------------------------------------------------------------------
//! \brief Print the state of the VIC engine.  Called if Run() fails.
//!
/* static */ RC TegraVicHelper::PrintVicEngineState(Tee::Priority pri)
{
#ifndef VARIADIC_MACROS_NOT_SUPPORTED
    RC rc;
    Controller *pVicController;
    CHECK_RC(VicControllerMgr::Mgr()->Get(0, &pVicController));
    if (pVicController->GetInitState() < Controller::INIT_DONE)
        CHECK_RC(pVicController->Init());
    CHECK_RC(pVicController->PrintState(pri));
    return rc;
#else
    return RC::OK;
#endif
}

//--------------------------------------------------------------------
// JavaScript interface
//
// Debugging note: BackCompatibleMode is for sanity-checking the
// different VIC versions during development, so that you can use
// "-testarg 416 DumpFiles 2 -testargstr 416 BackCompatibleMode :" on
// different VIC versions and compare the resulting TegraVicDst*.bin
// files.  They won't use *exactly* the same compositing algorithm,
// but you can do a sloppy comparison with something like this:
//     #!/usr/bin/elw python3
//     import collections
//     import sys
//     bytes1 = open(sys.argv[1], "rb").read()
//     bytes2 = open(sys.argv[2], "rb").read()
//     if len(bytes1) != len(bytes2):
//         print("Files have different sizes!")
//     else:
//         deltas = [abs(ii[1] - ii[0]) for ii in zip(bytes1, bytes2)]
//         deltas = [0x80 - abs(0x80 - ii) for ii in deltas]
//         deltaCounts = collections.Counter(deltas)
//         for ii in sorted(deltaCounts.keys()):
//             print("Bytes differing by {}: {}".format(ii, deltaCounts[ii]))
// Nearly-identical files should produce an output like this:
//     Bytes differing by 0: 1323718
//     Bytes differing by 1: 31032
//     Bytes differing by 2: 2
//
JS_CLASS_INHERIT(TegraVic, GpuTest, "TegraVic object");
CLASS_PROP_READWRITE
(
    TegraVic,
    KeepRunning,
    bool,
    "The test will keep running as long as this flag is set"
);
CLASS_PROP_READWRITE
(
    TegraVic,
    DumpFiles,
    UINT32,
    "1 = Dump the config struct to  a text file, 2 = Dump binary, 3 = Dump both"
);
CLASS_PROP_READWRITE
(
    TegraVic,
    BackCompatibleMode,
    string,
    "Only test features common to all VIC versions in the range version1:version2,"
    "so the output surfaces are mostly the same"
);
CLASS_PROP_READWRITE
(
    TegraVic,
    EnableFlipX,
    UINT08,
    "Flag to enable/disable FlipX. Default will randomize it"
);
CLASS_PROP_READWRITE
(
    TegraVic,
    EnableFlipY,
    UINT08,
    "Flag to enable/disable FlipY. Default will randomize it"
);
CLASS_PROP_READWRITE
(
    TegraVic,
    EnableCadenceDetection,
    UINT08,
    "Flag to enable/disable cadence detection. Default will randomize it"
);
CLASS_PROP_READWRITE
(
    TegraVic,
    EnableNoiseReduction,
    UINT08,
    "Flag to enable/disable noise reduction (DeNoise). Default will randomize it"
);
CLASS_PROP_READWRITE
(
    TegraVic,
    FrameFormat,
    UINT32,
    "Flag to set the input frame format. Default will randomize it"
);
CLASS_PROP_READWRITE
(
    TegraVic,
    InputPixelFormat,
    UINT32,
    "Flag to set the input pixel format. Default will randomize it"
);
CLASS_PROP_READWRITE
(
    TegraVic,
    OutputPixelFormat,
    UINT32,
    "Flag to set the output pixel format. Default will randomize it"
);
CLASS_PROP_READWRITE
(
    TegraVic,
    InputBlockKind,
    UINT08,
    "Flag to set the input block kind. Default will randomize it"
);
CLASS_PROP_READWRITE
(
    TegraVic,
    OutputBlockKind,
    UINT08,
    "Flag to set the output block kind. Default will randomize it"
);
CLASS_PROP_READWRITE
(
    TegraVic,
    DeInterlaceMode,
    UINT08,
    "Flag to set the DeInterlaceMode. Default will randomize it"
);
