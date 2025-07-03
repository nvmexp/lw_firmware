/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "class/clc5b6.h"
#include "class/cle26e.h"
#include "core/include/channel.h"
#include "core/include/fileholder.h"
#include "core/include/lwrm.h"
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/utility/surfrdwr.h"
#include "cheetah/dev/vic/vicctrlmgr.h"
#include "vic_ldc_tnr.h"

static const UINT32 MIN_RECT_WIDTH     = 128;
static const UINT32 MIN_RECT_HEIGHT    = 128;
static const UINT32 MAX_IMAGE_WIDTH    = 1024;
static const UINT32 MAX_IMAGE_HEIGHT   = 1024;
static const UINT32 PITCH_STRIDE_ALIGN = 256;
static const UINT32 PITCH_HEIGHT_ALIGN = 16;
static const UINT32 BLOCK_STRIDE_ALIGN = 64;
static const UINT32 BLOCK_HEIGHT_ALIGN = 64;
static const UINT08 TESTMODE_TNR = 0;
static const UINT08 TESTMODE_LDC = 1;
static const UINT08 TESTMODE_LDCTNR = 2;
static const UINT32 MAX_SCALING_FACTOR = 16;
static const UINT32 MAX_VIC_BYTESPERPIXEL = 2;

const UINT32 VicLdcTnr::s_SupportedClasses[] =
{
    LWC5B6_VIDEO_COMPOSITOR
};

/* static */ const UINT64 VicLdcTnrHelper::ALL_ONES = 0xffffffffffffffffULL;
/* static */ const UINT64 VicLdcTnrHelper::IGNORED = 0;
/* static */ const UINT32 VicLdcTnrHelper::MAXPLANES = 4;

//--------------------------------------------------------------------
VicLdcTnr::VicLdcTnr() :
    m_DumpFiles(0)
    ,m_TestSampleTNR3Image(false)
    ,m_TestSampleLDCImage(false)
{
    SetName("VicLDCTNR");
    CreateFancyPickerArray(FPK_VICLDCTNR_NUM_PICKERS);
}

//--------------------------------------------------------------------
/* virtual */ bool VicLdcTnr::IsSupported()
{
    return (GpuTest::IsSupported() &&
            GetBoundGpuSubdevice()->IsSOC() &&
            GetFirstSupportedClass() != 0);
}

//--------------------------------------------------------------------
/* virtual */ RC VicLdcTnr::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    // Allocate the VicLDCTNRHelper object that does most of the work
    switch (GetFirstSupportedClass())
    {
        case LWC5B6_VIDEO_COMPOSITOR:
            m_pHelper.reset(new VicLdcTnr42Helper(this));
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(m_pHelper->Setup());
    return rc;
}

//--------------------------------------------------------------------
/* virtual */ RC VicLdcTnr::Run()
{
    return m_pHelper->Run();
}

//--------------------------------------------------------------------
/* virtual */ RC VicLdcTnr::Cleanup()
{
    StickyRC firstRc;
    firstRc = m_pHelper->Cleanup();
    firstRc = GpuTest::Cleanup();
    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Set defaults for the fancy pickers
//!
/* virtual */ RC VicLdcTnr::SetDefaultsForPicker(UINT32 idx)
{
    FancyPicker *pPicker = &(*GetFancyPickerArray())[idx];
    RC rc;

    CHECK_RC(pPicker->ConfigRandom());

    switch (idx)
    {
        case FPK_VICLDCTNR_IMAGE_WIDTH:
            pPicker->AddRandRange(1, MIN_RECT_WIDTH, MAX_IMAGE_WIDTH);
            break;
        case FPK_VICLDCTNR_IMAGE_HEIGHT:
            pPicker->AddRandRange(1, MIN_RECT_HEIGHT, MAX_IMAGE_HEIGHT);
            break;
        case FPK_VICLDCTNR_RECTX:
            pPicker->AddRandRange(1, 0, MAX_IMAGE_WIDTH - 1);
            break;
        case FPK_VICLDCTNR_RECTY:
            pPicker->AddRandRange(1, 0, MAX_IMAGE_HEIGHT - 1);
            break;
        case FPK_VICLDCTNR_PIXEL_FORMAT:
            // TODO: Add other YUV formats
            // RGB not supported
            pPicker->AddRandItem(1, Vic42::T_Y8___V8U8_N420);
            pPicker->AddRandItem(1, Vic42::T_Y10___V10U10_N444);
            break;
        case FPK_VICLDCTNR_BLOCK_KIND:
            //pPicker->AddRandItem(1, Vic42::BLK_KIND_PITCH);
            pPicker->AddRandItem(1, Vic42::BLK_KIND_GENERIC_16Bx2);
            break;
        case FPK_VICLDCTNR_FRAME_FORMAT:
            // TODO: Add more formats
            pPicker->AddRandItem(1, Vic42::DXVAHD_FRAME_FORMAT_PROGRESSIVE);
            break;
        case FPK_VICLDCTNR_PIXEL_FILTERTYPE:
            pPicker->AddRandItem(1, Vic42::FILTER_TYPE_NORMAL);
            pPicker->AddRandItem(1, Vic42::FILTER_TYPE_NOISE);
            pPicker->AddRandItem(1, Vic42::FILTER_TYPE_DETAIL);
            break;
        case FPK_VICLDCTNR_TESTMODE:
            pPicker->AddRandItem(1, TESTMODE_TNR);
            pPicker->AddRandItem(1, TESTMODE_LDC);
            pPicker->AddRandItem(1, TESTMODE_LDCTNR);
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
/* virtual */ void VicLdcTnr::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    string desc;
    if (m_DumpFiles & DUMP_TEXT)
        desc += "Text,";

    if (m_DumpFiles & DUMP_BINARY)
        desc += "Binary,";

    if (m_DumpFiles & DUMP_PNG)
        desc += "PNG";

    if (desc.empty())
        desc = "None";
    else
        desc.resize(desc.size() - 1);

    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tKeepRunning:\t\t%s\n", m_KeepRunning ? "true" : "false");
    Printf(pri, "\tDumpFiles:\t\t%d (%s)\n", m_DumpFiles, desc.c_str());
    Printf(pri, "\tTestSampleTNR3Image:\t\t%s\n", m_TestSampleTNR3Image ? "true" : "false");
    Printf(pri, "\tTestSampleLDCImage:\t\t%s\n", m_TestSampleLDCImage ? "true" : "false");
}

//--------------------------------------------------------------------
//! \brief Colwert a version number to a human-readable string
//!
/* static */ const char *VicLdcTnr::GetVersionName(Version version)
{
    switch (version)
    {
        case VERSION_VIC42:
            return "4.2";
        default:
            MASSERT(!"Illegal version passed to GetVersionName");
            return "";
    }
}

//--------------------------------------------------------------------
//! \brief Return the VIC class supported by this GPU, or 0 if none.
//!
UINT32 VicLdcTnr::GetFirstSupportedClass()
{
    LwRm *pLwRm = GetBoundRmClient();
    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    UINT32 myClass = 0;

    RC rc = pLwRm->GetFirstSupportedClass(
            static_cast<UINT32>(NUMELEMS(s_SupportedClasses)),
            &s_SupportedClasses[0], &myClass, pGpuDevice);
    if (rc != OK)
        myClass = 0;
    return myClass;
}

//--------------------------------------------------------------------
VicLdcTnrHelper::VicLdcTnrHelper(VicLdcTnr *pTest) :
    m_pTest(pTest)
    ,m_pLwRm(pTest->GetBoundRmClient())
    ,m_pGpuDevice(pTest->GetBoundGpuDevice())
    ,m_pGpuSubdevice(pTest->GetBoundGpuSubdevice())
    ,m_pTestConfig(pTest->GetTestConfiguration())
    ,m_pGoldelwalues(pTest->GetGoldelwalues())
    ,m_pFpCtx(pTest->GetFpContext())
    ,m_Pri(pTest->GetVerbosePrintPri())
    ,m_pCh(nullptr)
{
    memset(&m_InputParams, 0, sizeof(m_InputParams));
    memset(&m_OutputParams, 0, sizeof(m_OutputParams));
}

//--------------------------------------------------------------------
//! \brief Allocate the channel & class used by this test
//!
RC VicLdcTnrHelper::Setup()
{
    LwRm::Handle hChannel;
    RC rc;

    UINT32 destFrameWidthAlign = 64;
    UINT32 destFrameWidthAliglwal = ALIGN_UP(MAX_IMAGE_WIDTH, destFrameWidthAlign);
    UINT32 destFrameHeightAlign = 16;
    UINT32 destFrameHeightAliglwal = ALIGN_UP(MAX_IMAGE_HEIGHT, destFrameHeightAlign);
    UINT32 neighDataSizeAlign = 256;
    UINT32 neighDataSize = 0;
    neighDataSize = ALIGN_UP(
            (destFrameHeightAliglwal/16)*TNR3LEFTCOLTILESIZE, neighDataSizeAlign) +
            ALIGN_UP((destFrameWidthAliglwal/64)*TNR3ABOVEROWTILESIZE, neighDataSizeAlign);
    UINT32 alphaSize = ALIGN_UP
                (destFrameWidthAliglwal/2 * destFrameHeightAliglwal/2, neighDataSizeAlign);
    UINT32 maxImageSize = MAX_IMAGE_WIDTH * MAX_VIC_BYTESPERPIXEL * MAX_IMAGE_WIDTH * MAX_VIC_BYTESPERPIXEL;

    CHECK_RC(m_pTestConfig->AllocateChannel(&m_pCh, &hChannel, LW2080_ENGINE_TYPE_VIC));
    CHECK_RC(m_pCh->SetClass(0, LWE26E_CMD_SETCL_CLASSID_GRAPHICS_VIC));
    CHECK_RC(AllocDataSurface(&m_CfgSurface, NULL, sizeof(Vic42::GeoTranConfigStruct), false));
    CHECK_RC(AllocDataSurface(&m_CrcStruct, NULL, sizeof(Vic42::InterfaceCrcStruct), false));
    // This buffer is needed by hardware when enabling tnr3
    CHECK_RC(AllocDataSurface(&m_Tnr3NeighBuffer, NULL, neighDataSize, false));
    // This buffer is needed by hardware when enabling tnr3
    CHECK_RC(AllocDataSurface(&m_Tnr3LwrrAlphaBuffer, NULL, alphaSize, false));
    CHECK_RC(AllocDataSurface(&m_Tnr3PreAlphaBuffer, NULL, alphaSize, true));

    // Input and Output image's Luma, ChromaU and ChromaV surfaces
    CHECK_RC(AllocDataSurface(&m_LwrrentImageLuma, NULL, maxImageSize, false));
    CHECK_RC(AllocDataSurface(&m_LwrrentImageChromaU, NULL, maxImageSize, false));
    CHECK_RC(AllocDataSurface(&m_LwrrentImageChromaV, NULL, maxImageSize, false));
    CHECK_RC(AllocDataSurface(&m_PreviousImageLuma, NULL, maxImageSize, false));
    CHECK_RC(AllocDataSurface(&m_PreviousImageChromaU, NULL, maxImageSize, false));
    CHECK_RC(AllocDataSurface(&m_PreviousImageChromaV, NULL, maxImageSize, false));
    CHECK_RC(AllocDataSurface(&m_DestImageLuma, NULL, maxImageSize, false));
    CHECK_RC(AllocDataSurface(&m_DestImageChromaU, NULL, maxImageSize, false));
    CHECK_RC(AllocDataSurface(&m_DestImageChromaV, NULL, maxImageSize, false));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Run the test.  This is the main body of VicLDCTNR::Run().
//!
RC VicLdcTnrHelper::Run()
{
    UINT32 startLoop = m_pTestConfig->StartLoop();
    UINT32 endLoop = startLoop + m_pTestConfig->Loops();
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
        CHECK_RC(PickLoopParams());

        // Run the test
        SrcImages srcImages;
        DstImage dstImage;
        vector<UINT08> cfgBuffer;
        {
            Tasker::DetachThread detach;

            CHECK_RC(FillSrcImages(&srcImages, &dstImage));
            CHECK_RC(FillDstImage(&dstImage));
            CHECK_RC(FillConfigStruct(&cfgBuffer, srcImages, dstImage));
            // TODO Add checkConfigStruct
            //MASSERT(CheckConfigStruct(&cfgBuffer[0]));
            CHECK_RC(DumpSrcFilesIfNeeded(cfgBuffer, srcImages));
            CHECK_RC(WriteMethods(&cfgBuffer[0], srcImages, dstImage.image));
            CHECK_RC(DumpDstFilesIfNeeded(dstImage.image));
        }

        // Goldelwalues call JS, so it cannot be detached from Tasker
        CHECK_RC(m_pGoldelwalues->Run());
    }
    rc = m_pGoldelwalues->ErrorRateTest(m_pTest->GetJSObject());
    if (rc == OK)
        printState = false;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Clean up the data allocated for this test.
//!
//! This is the main body of VicLDCTNR::Cleanup().
//!
RC VicLdcTnrHelper::Cleanup()
{
    StickyRC firstRc;

    m_pGoldelwalues->ClearSurfaces();
    if (m_pCh)
    {
        firstRc = m_pTestConfig->FreeChannel(m_pCh);
        m_pCh = nullptr;
    }
    m_CfgSurface.Free();
    m_CrcStruct.Free();
    m_Tnr3NeighBuffer.Free();
    m_Tnr3LwrrAlphaBuffer.Free();
    m_Tnr3PreAlphaBuffer.Free();
    m_LwrrentImageLuma.Free();
    m_LwrrentImageChromaU.Free();
    m_LwrrentImageChromaV.Free();
    m_PreviousImageLuma.Free();
    m_PreviousImageChromaU.Free();
    m_PreviousImageChromaV.Free();
    m_DestImageLuma.Free();
    m_DestImageChromaU.Free();
    m_DestImageChromaV.Free();
    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Fill the image's surface(s) with a value
//!
RC VicLdcTnrHelper::Image::Fill(UINT32 value)
{
    RC rc;
    if (pLuma->IsAllocated())
        CHECK_RC(pLuma->Fill(value));
    if (pChromaU->IsAllocated())
        CHECK_RC(pChromaU->Fill(value));
    if (pChromaV->IsAllocated())
        CHECK_RC(pChromaV->Fill(value));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Fill the image's surface(s) with content from file
//!
RC VicLdcTnrHelper::Image::FillFromFile(const string &fileName)
{
    RC rc;

    Surface2D *pSurfaces[] = { pLuma, pChromaU, pChromaV };
    for (size_t ii = 0; ii < NUMELEMS(pSurfaces); ++ii)
    {
        Surface2D *pSurface = pSurfaces[ii];
        if (pSurface->IsAllocated())
        {
            FileHolder fileHolder;
            CHECK_RC(fileHolder.Open(fileName, "rb"));
            vector<UINT08> data(pSurface->GetSize());

            size_t bytesRead = fread(&data[0], 1, pSurface->GetSize(), fileHolder.GetFile());
            if (bytesRead != pSurface->GetSize())
            {
                Printf(Tee::PriError, "Failed to read file %s\n", fileName.c_str());
                Printf(Tee::PriError, "Feof %d\n", feof(fileHolder.GetFile()));
                return RC::FILE_READ_ERROR;
            }
            SurfaceUtils::MappingSurfaceWriter surfaceWriter(*pSurface);
            CHECK_RC(surfaceWriter.WriteBytes(0, &data[0], data.size()));
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Fill the image's surface(s) with random data
//!
//! \param seed Seed for the random-number generator.  This method
//!     tweaks the seed a bit to provide a different seed to each of
//!     the image's surfaces.
//!
RC VicLdcTnrHelper::Image::FillRandom(UINT32 seed)
{
    Surface2D *pSurfaces[] = { pLuma, pChromaU, pChromaV };
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
RC VicLdcTnrHelper::Image::Write
(
    Channel *pCh
    ,UINT32 lumaMethod
    ,UINT32 chromaUMethod
    ,UINT32 chromaVMethod
) const
{
    const Surface2D *pSurfaces[] = { pLuma, pChromaU, pChromaV };
    UINT32 methods[] = { lumaMethod, chromaUMethod, chromaVMethod };
    MASSERT(pCh);
    RC rc;

    MASSERT(NUMELEMS(pSurfaces) == NUMELEMS(methods));
    for (size_t ii = 0; ii < NUMELEMS(pSurfaces); ++ii)
    {
        if (pSurfaces[ii]->IsAllocated() &&
                pSurfaces[ii]->GetColorFormat() != ColorUtils::LWFMT_NONE)
        {
            CHECK_RC(pCh->WriteWithSurface(0, methods[ii], *pSurfaces[ii],
                                           0, Vic42::SURFACE_SHIFT));
        }
        else
        {
            CHECK_RC(pCh->Write(0, methods[ii], 0));
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Set the width of a rectangle of pixels
//!
//! This method can be called from ApplyConstraints() to adjust the
//! rectangles chosen by fancyPicker.  It changes the width of the
//! rectangle, without exceeding the bounds of the image that the
//! rectangle is in.
//!
//! \param newWidth The new width the rectangle should have.  The
//!     caller must ensure this width fits in the surrounding image.
//! \param imageWidth The width of the surrounding image.
//!
void VicLdcTnrHelper::Rect::SetWidth(UINT32 newWidth, UINT32 imageWidth)
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
void VicLdcTnrHelper::Rect::SetHeight(UINT32 newHeight, UINT32 imageHeight)
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
bool VicLdcTnrHelper::InputParams::IsProgressive() const
{
    return (frameFormat == Vic42::DXVAHD_FRAME_FORMAT_PROGRESSIVE ||
            frameFormat == Vic42::DXVAHD_FRAME_FORMAT_SUBPIC_PROGRESSIVE);
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
bool VicLdcTnrHelper::InputParams::IsFrameBased() const
{
    switch (frameFormat)
    {
        case Vic42::DXVAHD_FRAME_FORMAT_PROGRESSIVE:
        case Vic42::DXVAHD_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST:
        case Vic42::DXVAHD_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST:
        case Vic42::DXVAHD_FRAME_FORMAT_SUBPIC_PROGRESSIVE:
        case Vic42::DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_TOP_FIELD_FIRST:
        case Vic42::DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_BOTTOM_FIELD_FIRST:
            return true;
        default:
            return false;
    }
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
/* static */ VicLdcTnrHelper::ImageDimensions VicLdcTnrHelper::GetImageDimensions
(
    UINT32 width,
    UINT32 frameHeight,
    bool isInterlaced,
    Vic42::PIXELFORMAT pixelFormat,
    Vic42::BLK_KIND blockKind
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
        case Vic42::BLK_KIND_PITCH:
            strideAlign = PITCH_STRIDE_ALIGN;
            // VIC needs the pitch height to align to multiple of 16
            // http://lwbugs/1838387/10
            heightAlign = PITCH_HEIGHT_ALIGN;
            break;
        case Vic42::BLK_KIND_GENERIC_16Bx2:
            strideAlign = BLOCK_STRIDE_ALIGN;
            heightAlign = BLOCK_HEIGHT_ALIGN;
            break;
        default:
            MASSERT(!"Unknown block kind");
            break;
    }

    switch (pixelFormat)
    {
        case Vic42::T_Y8___V8U8_N420:
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
            break;
        case Vic42::T_Y10___V10U10_N444:
            dims.widthGranularity       = 2;
            dims.frameHeightGranularity = 2 * fieldsPerFrame;
            dims.lumaPitch      = ALIGN_UP(width, strideAlign);
            dims.lumaWidth      = dims.lumaPitch;
            dims.lumaHeight     = ALIGN_UP(dims.fieldHeight, heightAlign);
            dims.chromaPitch    = ALIGN_UP(width, strideAlign);
            dims.chromaWidth    = dims.chromaPitch;
            dims.chromaHeight   = ALIGN_UP(dims.fieldHeight, heightAlign);
            break;
        default:
            MASSERT(!"Unknown pixel format");
            break;
    }

    return dims;
}

//--------------------------------------------------------------------
//! \brief Allocate a surface filled with the indicated data,
//! if the data is provided or random value or zero.
//!
RC VicLdcTnrHelper::AllocDataSurface
(
    Surface2D *pSurface,
    const void *pData,
    size_t size,
    bool isFillRandom
) const
{
    MASSERT(pSurface);
    RC rc;

    pSurface->SetWidth(static_cast<UINT32>(size));
    pSurface->SetHeight(1);
    pSurface->SetColorFormat(ColorUtils::Y8);
    pSurface->SetVASpace(Surface2D::TegraVASpace);
    pSurface->SetAlignment(Vic42::SURFACE_ALIGN);
    CHECK_RC(pSurface->Alloc(m_pGpuDevice));

    if (pData)
    {
        SurfaceUtils::MappingSurfaceWriter surfaceWriter(*pSurface);
        CHECK_RC(surfaceWriter.WriteBytes(0, pData, size));
    }
    else if (isFillRandom)
    {
        const UINT32 fillSeed = m_pFpCtx->Rand.GetRandom();
        vector<UINT08> data(pSurface->GetSize());
        CHECK_RC(Memory::FillRandom(
                &data[0],
                static_cast<UINT32>(fillSeed),
                static_cast<UINT32>(data.size())));
        SurfaceUtils::MappingSurfaceWriter surfaceWriter(*pSurface);
        CHECK_RC(surfaceWriter.WriteBytes(0, &data[0], data.size()));
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
RC VicLdcTnrHelper::AllocDataSurface
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
            Printf(Tee::PriError, "Error reading file %s\n", filename.c_str());
            return RC::FILE_READ_ERROR;
        }

        // This function is lwrrently only used for loading the microcode.
        // Align the size of this surface up so it is a multiple of 256B,
        // which is a VIC requirement.
        pBuffer->resize(ALIGN_UP(pBuffer->size(), 256U));
    }
    CHECK_RC(AllocDataSurface(pSurface, &(*pBuffer)[0], pBuffer->size(), false));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Allocate a surface with the size specified.
//! Then fill the data from the file
//! Here the size of the file might not be equal to surface size
//!
RC VicLdcTnrHelper::AllocDataSurface
(
    Surface2D *pSurface,
    size_t size,
    const string &filename
) const
{
    RC rc;
    FileHolder fileHolder;

    CHECK_RC(AllocDataSurface(pSurface, NULL, size, false));

    CHECK_RC(fileHolder.Open(filename, "rb"));
    if (pSurface->IsAllocated())
    {
        vector<UINT08> data(size);
        if (fread(&data[0], size, 1, fileHolder.GetFile()) != 1)
        {
            Printf(Tee::PriError, "Failed to read file %s\n", filename.c_str());
            return RC::FILE_READ_ERROR;
        }
        SurfaceUtils::MappingSurfaceWriter surfaceWriter(*pSurface);
        CHECK_RC(surfaceWriter.WriteBytes(0, &data[0], size));
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Allocate an image based on an ImageDimensions struct
//!
RC VicLdcTnrHelper::FillImageParams(unique_ptr<Image> &pImage, const ImageDimensions &dims) const
{
    MASSERT(pImage);
    RC rc;

    if (dims.lumaFormat != ColorUtils::LWFMT_NONE)
    {
        pImage->pLuma->SetWidth(dims.lumaWidth);
        pImage->pLuma->SetHeight(dims.lumaHeight);
        pImage->pLuma->SetPitch(dims.lumaPitch);
        pImage->pLuma->SetColorFormat(dims.lumaFormat);
        pImage->pLuma->SetVASpace(Surface2D::TegraVASpace);
        pImage->pLuma->SetAlignment(Vic42::SURFACE_ALIGN);
    }
    else
    {
        pImage->pLuma->SetColorFormat(ColorUtils::LWFMT_NONE);
    }

    if (dims.chromaUFormat != ColorUtils::LWFMT_NONE)
    {
        pImage->pChromaU->SetWidth(dims.chromaWidth);
        pImage->pChromaU->SetHeight(dims.chromaHeight);
        pImage->pChromaU->SetPitch(dims.chromaPitch);
        pImage->pChromaU->SetColorFormat(dims.chromaUFormat);
        pImage->pChromaU->SetVASpace(Surface2D::TegraVASpace);
        pImage->pChromaU->SetAlignment(Vic42::SURFACE_ALIGN);
    }
    else
    {
        pImage->pChromaU->SetColorFormat(ColorUtils::LWFMT_NONE);
    }

    if (dims.chromaVFormat != ColorUtils::LWFMT_NONE)
    {
        pImage->pChromaV->SetWidth(dims.chromaWidth);
        pImage->pChromaV->SetHeight(dims.chromaHeight);
        pImage->pChromaV->SetPitch(dims.chromaPitch);
        pImage->pChromaV->SetColorFormat(dims.chromaVFormat);
        pImage->pChromaV->SetVASpace(Surface2D::TegraVASpace);
        pImage->pChromaV->SetAlignment(Vic42::SURFACE_ALIGN);
    }
    else
    {
        pImage->pChromaV->SetColorFormat(ColorUtils::LWFMT_NONE);
    }
    return rc;
}

//---------------------------------------------------------------------
//! \brief Pick the parameters used by this loop of the test
//!
//! Use fancyPickers to get the parameters for this loop, and call
//! ApplyConstraints() to tweak the values as needed.
//!
RC VicLdcTnrHelper::PickLoopParams()
{
    RC rc;

    if (m_pTest->m_TestSampleLDCImage)
    {
        // TODO add code for sample LDC image
    }
    else if (m_pTest->m_TestSampleTNR3Image)
    {
        m_InputParams.imageWidth = 256;
        m_InputParams.imageHeight = 256;
        m_InputParams.srcRect.left = 0;
        m_InputParams.srcRect.right = 255;
        m_InputParams.srcRect.top = 0;
        m_InputParams.srcRect.bottom = 255;
        m_InputParams.pixelFormat = Vic42::T_Y8___V8U8_N420;
        m_InputParams.blockKind = Vic42::BLK_KIND_GENERIC_16Bx2;
        m_InputParams.frameFormat = Vic42::DXVAHD_FRAME_FORMAT_PROGRESSIVE;
        m_InputParams.pixelFilterType = Vic42::FILTER_TYPE_DETAIL;

        // TNR3 Related Values
        m_InputParams.tnr3Params.tnr3En = 1;
        m_InputParams.tnr3Params.betaBlendingEn = IGNORED;
        m_InputParams.tnr3Params.alphaBlendingEn = IGNORED;
        m_InputParams.tnr3Params.alphaSmoothEn = 1;
        m_InputParams.tnr3Params.tempAlphaRestrictEn = IGNORED;
        m_InputParams.tnr3Params.alphaClipEn = IGNORED;
        m_InputParams.tnr3Params.bFRangeEn = 1;
        m_InputParams.tnr3Params.bFDomainEn = 1;
        m_InputParams.tnr3Params.rangeSigmaChroma = 200;
        m_InputParams.tnr3Params.rangeSigmaLuma = 200;
        m_InputParams.tnr3Params.domainSigmaChroma = 6;
        m_InputParams.tnr3Params.domainSigmaLuma = 6;
        m_InputParams.tnr3Params.SADMultiplier             = 10;
        m_InputParams.tnr3Params.SADWeightLuma             = 16;
        m_InputParams.tnr3Params.TempAlphaRestrictIncCap   = 64;
        m_InputParams.tnr3Params.AlphaScaleIIR             = 768;
        m_InputParams.tnr3Params.AlphaClipMaxLuma          = 1000;
        m_InputParams.tnr3Params.AlphaClipMinLuma          = 0;
        m_InputParams.tnr3Params.AlphaClipMaxChroma        = 1000;
        m_InputParams.tnr3Params.AlphaClipMinChroma        = 200;
        m_InputParams.tnr3Params.BetaCalcMaxBeta           = 1024;
        m_InputParams.tnr3Params.BetaCalcMinBeta           = 400;
        m_InputParams.tnr3Params.BetaCalcBetaX1            = 400;
        m_InputParams.tnr3Params.BetaCalcBetaX2            = 1024;

        m_OutputParams.dstRect.left = 0;
        m_OutputParams.dstRect.right = 255;
        m_OutputParams.dstRect.top = 0;
        m_OutputParams.dstRect.bottom = 255;
        m_OutputParams.imageWidth = m_OutputParams.dstRect.GetWidth();
        m_OutputParams.imageHeight = m_OutputParams.dstRect.GetHeight();
        m_OutputParams.pixelFormat = Vic42::T_Y8___V8U8_N420;
        m_OutputParams.blockKind = Vic42::BLK_KIND_GENERIC_16Bx2;
    }
    else
    {
        FancyPickerArray &fpk = *m_pTest->GetFancyPickerArray();
        m_InputParams.imageWidth     = fpk[FPK_VICLDCTNR_IMAGE_WIDTH].Pick();
        m_InputParams.imageHeight    = fpk[FPK_VICLDCTNR_IMAGE_HEIGHT].Pick();
        m_InputParams.srcRect.left   = fpk[FPK_VICLDCTNR_RECTX].Pick();
        m_InputParams.srcRect.right  = fpk[FPK_VICLDCTNR_RECTX].Pick();
        m_InputParams.srcRect.top    = fpk[FPK_VICLDCTNR_RECTY].Pick();
        m_InputParams.srcRect.bottom = fpk[FPK_VICLDCTNR_RECTY].Pick();
        m_InputParams.pixelFormat    = static_cast<Vic42::PIXELFORMAT>(
                fpk[FPK_VICLDCTNR_PIXEL_FORMAT].Pick());
        m_InputParams.blockKind      = static_cast<Vic42::BLK_KIND>(
                fpk[FPK_VICLDCTNR_BLOCK_KIND].Pick());
        m_InputParams.frameFormat    = static_cast<Vic42::DXVAHD_FRAME_FORMAT>(
                fpk[FPK_VICLDCTNR_FRAME_FORMAT].Pick());
        m_InputParams.pixelFilterType = static_cast<Vic42::FILTER_TYPE>(
                fpk[FPK_VICLDCTNR_PIXEL_FILTERTYPE].Pick());

        m_OutputParams.imageWidth  = m_InputParams.imageWidth;
        m_OutputParams.imageHeight = m_InputParams.imageHeight;
        m_OutputParams.pixelFormat = m_InputParams.pixelFormat;
        m_OutputParams.blockKind   = m_InputParams.blockKind;

        UINT08 testMode = fpk[FPK_VICLDCTNR_TESTMODE].Pick();
        if (testMode == TESTMODE_TNR || testMode == TESTMODE_LDCTNR)
        {
            m_InputParams.tnr3Params.tnr3En = 1;
            m_InputParams.tnr3Params.betaBlendingEn = IGNORED;
            m_InputParams.tnr3Params.alphaBlendingEn = IGNORED;
            m_InputParams.tnr3Params.alphaSmoothEn = 1;
            m_InputParams.tnr3Params.tempAlphaRestrictEn = IGNORED;
            m_InputParams.tnr3Params.alphaClipEn = IGNORED;
            m_InputParams.tnr3Params.bFRangeEn = 1;
            m_InputParams.tnr3Params.bFDomainEn = 1;
            m_InputParams.tnr3Params.rangeSigmaChroma = 200;
            m_InputParams.tnr3Params.rangeSigmaLuma = 200;
            m_InputParams.tnr3Params.domainSigmaChroma = 6;
            m_InputParams.tnr3Params.domainSigmaLuma = 6;
            m_InputParams.tnr3Params.SADMultiplier             = 10;
            m_InputParams.tnr3Params.SADWeightLuma             = 16;
            m_InputParams.tnr3Params.TempAlphaRestrictIncCap   = 64;
            m_InputParams.tnr3Params.AlphaScaleIIR             = 768;
            m_InputParams.tnr3Params.AlphaClipMaxLuma          = 1000;
            m_InputParams.tnr3Params.AlphaClipMinLuma          = 0;
            m_InputParams.tnr3Params.AlphaClipMaxChroma        = 1000;
            m_InputParams.tnr3Params.AlphaClipMinChroma        = 200;
            m_InputParams.tnr3Params.BetaCalcMaxBeta           = 1024;
            m_InputParams.tnr3Params.BetaCalcMinBeta           = 400;
            m_InputParams.tnr3Params.BetaCalcBetaX1            = 400;
            m_InputParams.tnr3Params.BetaCalcBetaX2            = 1024;
        }

        if (testMode == TESTMODE_LDC || testMode == TESTMODE_LDCTNR)
        {
            m_InputParams.ldcParams.geoTranEn = 1;
        }
    }
    // Adjust the picked values to meet constraints.
    //
    CHECK_RC(ApplyConstraints());

    Printf(m_Pri, "Picks:\n");

    const char *fmt  = "    %-24s = %u\n";

    Printf(m_Pri, fmt, "TNR Enable",           m_InputParams.tnr3Params.tnr3En);
    Printf(m_Pri, fmt, "LDC Enable",           m_InputParams.ldcParams.geoTranEn);
    Printf(m_Pri, fmt, "InputImageWidth",      m_InputParams.imageWidth);
    Printf(m_Pri, fmt, "InputImageHeight",     m_InputParams.imageHeight);
    Printf(m_Pri, fmt, "InputRectleft",        m_InputParams.srcRect.left);
    Printf(m_Pri, fmt, "InputRectRight",       m_InputParams.srcRect.right);
    Printf(m_Pri, fmt, "InputRectTop",         m_InputParams.srcRect.top);
    Printf(m_Pri, fmt, "InputRectBottom",      m_InputParams.srcRect.bottom);
    Printf(m_Pri, fmt, "InputPixelFormat",     m_InputParams.pixelFormat);
    Printf(m_Pri, fmt, "InputBlockKind",       m_InputParams.blockKind);
    Printf(m_Pri, fmt, "InputFrameFormat",     m_InputParams.frameFormat);
    Printf(m_Pri, fmt, "InputPixelFilterType", m_InputParams.pixelFilterType);
    Printf(m_Pri, fmt, "OutputImageWidth",     m_OutputParams.imageWidth);
    Printf(m_Pri, fmt, "OutputImageHeight",    m_OutputParams.imageHeight);
    Printf(m_Pri, fmt, "OutputPixelFormat",    m_OutputParams.pixelFormat);
    Printf(m_Pri, fmt, "OutputBlockKind",      m_OutputParams.blockKind);

    return OK;
}

//---------------------------------------------------------------------
//! \brief Apply constraints on the fancypicker values
//!
RC VicLdcTnrHelper::ApplyConstraints()
{
    // Apply constraints on the width/height of the dst image.
    //
    m_OutputParams.imageWidth = max(m_OutputParams.imageWidth, MIN_RECT_WIDTH);
    m_OutputParams.imageHeight = max(m_OutputParams.imageHeight, MIN_RECT_HEIGHT);
    ImageDimensions tmpDims = GetImageDimensions(
            m_OutputParams.imageWidth, m_OutputParams.imageHeight, false,
            m_OutputParams.pixelFormat, m_OutputParams.blockKind);
    m_OutputParams.imageWidth = ALIGN_UP(m_OutputParams.imageWidth,
                                       tmpDims.widthGranularity);
    m_OutputParams.imageHeight = ALIGN_UP(m_OutputParams.imageHeight,
                                        tmpDims.frameHeightGranularity);

    // Apply constraints on the width/height
    //
    bool srcRectUsesFieldCoords = (m_InputParams.IsInterlaced() &&
            m_InputParams.IsFrameBased());
    m_InputParams.imageWidth = max(m_InputParams.imageWidth, MIN_RECT_WIDTH);
    m_InputParams.imageHeight = max(
            m_InputParams.imageHeight,
            srcRectUsesFieldCoords ? 2 * MIN_RECT_HEIGHT : MIN_RECT_HEIGHT);

    tmpDims = GetImageDimensions(
            m_InputParams.imageWidth, m_InputParams.imageHeight,
            m_InputParams.IsInterlaced(), m_InputParams.pixelFormat,
            m_InputParams.blockKind);

    m_InputParams.imageWidth = ALIGN_UP(m_InputParams.imageWidth,
                                      tmpDims.widthGranularity);
    m_InputParams.imageHeight = ALIGN_UP(m_InputParams.imageHeight,
                                       tmpDims.frameHeightGranularity);

    // Adjust the source and dest rectangles
    //
    Rect *pSrcRect = &m_InputParams.srcRect;
    Rect *pDstRect = &m_OutputParams.dstRect;

    UINT32 effSrcImageHeight = m_InputParams.imageHeight;
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
    pSrcRect->left   = min(pSrcRect->left,   m_InputParams.imageWidth - 1);
    pSrcRect->right  = min(pSrcRect->right,  m_InputParams.imageWidth - 1);
    pSrcRect->top    = min(pSrcRect->top,    effSrcImageHeight - 1);
    pSrcRect->bottom = min(pSrcRect->bottom, effSrcImageHeight - 1);
    pDstRect->left   = 0;
    pDstRect->right  = m_OutputParams.imageWidth - 1;
    pDstRect->top    = 0;
    pDstRect->bottom = m_OutputParams.imageHeight - 1;

    if (pSrcRect->left > pSrcRect->right)
        swap(pSrcRect->left, pSrcRect->right);
    if (pSrcRect->top > pSrcRect->bottom)
        swap(pSrcRect->top, pSrcRect->bottom);

    // (2) Make sure the rectangles meet the minimum width/height
    //     specs.  SetWidth() & SetHeight() will assert if the
    //     surrounding image is too small, so we checked earlier
    //     to make sure the images are big enough to hold the
    //     smallest possible rectangle.
    //
    if (pSrcRect->GetWidth() < MIN_RECT_WIDTH)
       pSrcRect->SetWidth(MIN_RECT_WIDTH, m_InputParams.imageWidth);
    if (pSrcRect->GetHeight() < MIN_RECT_HEIGHT)
       pSrcRect->SetHeight(MIN_RECT_HEIGHT, effSrcImageHeight);

    // (3) Make sure the rectangles are within MAX_SCALING_FACTOR
    //     of each other.  We do this by shrinking the larger
    //     rectangle, since that's easier than checking to make
    //     sure the surrounding image is big enough to expand the
    //     smaller rectangle.
    //
    if (pSrcRect->GetWidth() > pDstRect->GetWidth() * MAX_SCALING_FACTOR)
       pSrcRect->SetWidth(pDstRect->GetWidth() * MAX_SCALING_FACTOR,
                          m_InputParams.imageWidth);
    if (pSrcRect->GetHeight() > pDstRect->GetHeight() * MAX_SCALING_FACTOR)
       pSrcRect->SetHeight(pDstRect->GetHeight() * MAX_SCALING_FACTOR,
                           effSrcImageHeight);
    if (pDstRect->GetWidth() > pSrcRect->GetWidth() * MAX_SCALING_FACTOR)
    {
       pDstRect->SetWidth(pSrcRect->GetWidth() * MAX_SCALING_FACTOR,
                          m_OutputParams.imageWidth);
       m_OutputParams.imageWidth = pDstRect->GetWidth();
    }
    if (pDstRect->GetHeight() > pSrcRect->GetHeight() * MAX_SCALING_FACTOR)
    {
       pDstRect->SetHeight(pSrcRect->GetHeight() * MAX_SCALING_FACTOR,
               m_OutputParams.imageHeight);
       m_OutputParams.imageHeight = pDstRect->GetHeight();
    }
    return OK;
}

RC VicLdcTnrHelper::FillSrcImages(SrcImages *pSrcImages, DstImage *pDstImage)
{
    RC rc;
    const UINT32 fillSeed = m_pFpCtx->Rand.GetRandom();

    // First, determine which images (LWRRENT_FIELD, PREV_FIELD,
    // need to be allocated for the input, based on the
    // algorithms selected by the fancypickers.
    set<Vic42::SurfaceIndex> imagesToAllocate;

    imagesToAllocate.insert(Vic42::LWRRENT_FIELD);

    if (m_InputParams.tnr3Params.tnr3En)
    {
        // if TNR3 is enabled Previous Denoise Image is needed
        imagesToAllocate.insert(Vic42::PREV_FIELD);
    }

    // Allocate the images.
    //
    for (set<Vic42::SurfaceIndex>::const_iterator pSurfaceIdx =
         imagesToAllocate.begin();
         pSurfaceIdx != imagesToAllocate.end(); ++pSurfaceIdx)
    {
        if (*pSurfaceIdx == Vic42::LWRRENT_FIELD)
        {
            CHECK_RC(CalcBufferPadding(&pSrcImages->dimensions, m_InputParams.imageWidth,
                m_InputParams.imageHeight, m_InputParams.pixelFormat, false,
                m_InputParams.blockKind, 4, 0, m_InputParams.IsInterlaced(), 256));

            unique_ptr<Image> pNewImage = make_unique<Image>();
            pNewImage->pLuma = &m_LwrrentImageLuma;
            pNewImage->pChromaU = &m_LwrrentImageChromaU;
            pNewImage->pChromaV = &m_LwrrentImageChromaV;

            CHECK_RC(FillImageParams(pNewImage, pSrcImages->dimensions));
            if (m_pTest->GetTestSampleLDCImage() || m_pTest->GetTestSampleTNR3Image())
            {
                m_pTest->VerbosePrintf("Reading from Sample Image vic_tnr3_input_image.bin\n");
                CHECK_RC(pNewImage->FillFromFile("vic_tnr3_input_image.bin"));
            }
            else
            {
                CHECK_RC(pNewImage->FillRandom(fillSeed + *pSurfaceIdx));
            }
            pSrcImages->images[*pSurfaceIdx] = move(pNewImage);
        }
        else if (*pSurfaceIdx == Vic42::PREV_FIELD)
        {
            // Prev field is the previous run output image,
            // so it will be the same size as output image not input image
            CHECK_RC(CalcBufferPadding(&pDstImage->dimensions, m_OutputParams.imageWidth,
                    m_OutputParams.imageHeight, m_OutputParams.pixelFormat,
                    false, m_OutputParams.blockKind, 4, 0, false, 256));

            unique_ptr<Image> pNewImage = make_unique<Image>();
            pNewImage->pLuma = &m_PreviousImageLuma;
            pNewImage->pChromaU = &m_PreviousImageChromaU;
            pNewImage->pChromaV = &m_PreviousImageChromaV;
            CHECK_RC(FillImageParams(pNewImage, pDstImage->dimensions));
            if (m_pTest->GetTestSampleLDCImage() || m_pTest->GetTestSampleTNR3Image())
            {
                m_pTest->VerbosePrintf("Reading from Sample vic_tnr3_denoise_input_image.bin\n");
                CHECK_RC(pNewImage->FillFromFile("vic_tnr3_denoise_input_image.bin"));
            }
            else
            {
                CHECK_RC(pNewImage->FillRandom(fillSeed + *pSurfaceIdx));
            }
            pSrcImages->images[*pSurfaceIdx] = move(pNewImage);
        }
    }
    return rc;
}

//---------------------------------------------------------------------
//! \brief Allocate the destination image, and configure goldens for it
//!
RC VicLdcTnrHelper::FillDstImage(DstImage *pDstImage)
{
    RC rc;

    // Allocate the dst image
    //
    CHECK_RC(CalcBufferPadding(&pDstImage->dimensions, m_OutputParams.imageWidth,
        m_OutputParams.imageHeight, m_OutputParams.pixelFormat, false,
        m_OutputParams.blockKind, 4, 0, false, 256));

    unique_ptr<Image> pNewImage = make_unique<Image>();
    pNewImage->pLuma    = &m_DestImageLuma;
    pNewImage->pChromaU = &m_DestImageChromaU;
    pNewImage->pChromaV = &m_DestImageChromaV;

    CHECK_RC(FillImageParams(pNewImage, pDstImage->dimensions));
    pDstImage->image = move(pNewImage);
    CHECK_RC(pDstImage->image->Fill(0));

    // Configure the golden-values objects to capture GVs from the dst
    // image.  Each loop captures 1-3 golden values, depending on how
    // many components the image has.
    //
    Surface2D *pSurfaces[] =
        { pDstImage->image->pLuma, pDstImage->image->pChromaU, pDstImage->image->pChromaV };
    const char *surfaceNames[] = { "DstLuma", "DstChromaU", "DstChromaV" };
    MASSERT(NUMELEMS(pSurfaces) == NUMELEMS(surfaceNames));
    UINT32 numAllocatedSurfaces = 0;
    for (size_t ii = 0; ii < NUMELEMS(pSurfaces); ++ii)
    {
        if (pSurfaces[ii]->IsAllocated() &&
                pSurfaces[ii]->GetColorFormat() != ColorUtils::LWFMT_NONE)
            ++numAllocatedSurfaces;
    }
    m_GoldenSurfaces.SetNumSurfaces(numAllocatedSurfaces);
    UINT32 surfaceNum = 0;
    for (size_t ii = 0; ii < NUMELEMS(pSurfaces); ++ii)
    {
        if (pSurfaces[ii]->IsAllocated() &&
                pSurfaces[ii]->GetColorFormat() != ColorUtils::LWFMT_NONE)
        {
            Printf(Tee::PriDebug, "DST Image %ld Size: : %lld\n", ii, pSurfaces[ii]->GetSize());
            if (pSurfaces[ii]->IsMapped())
                pSurfaces[ii]->Unmap();
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
RC VicLdcTnrHelper::DumpSrcFilesIfNeeded
(
    const vector<UINT08> &cfgBuffer,
    const SrcImages &srcImages
) const
{
    RC rc;

    // Optionally dump the config buffer as a text file
    //
    if (m_pTest->m_DumpFiles & VicLdcTnr::DUMP_TEXT)
    {
        string cfgName = Utility::StrPrintf("InputConfig-Loop%d.txt",
                                            m_pFpCtx->LoopNum);
        FileHolder cfgFile;
        CHECK_RC(cfgFile.Open(cfgName, "w"));
        CHECK_RC(DumpConfigStruct(cfgFile.GetFile(), &cfgBuffer[0]));
    }

    // Optionally dump the config buffer and slot-specific surfaces as
    // binary files
    //
    if (m_pTest->m_DumpFiles & (VicLdcTnr::DUMP_BINARY | VicLdcTnr::DUMP_PNG))
    {
        for (SrcImages::ImageMap::const_iterator
             iter = srcImages.images.begin();
             iter != srcImages.images.end(); ++iter)
        {
            UINT32 surfaceIdx = iter->first;
            Image &image = *iter->second;

            if (m_pTest->m_DumpFiles & VicLdcTnr::DUMP_BINARY)
            {
                string suffix = Utility::StrPrintf("Loop%dSurf%d.bin",
                                                              m_pFpCtx->LoopNum,
                                                              surfaceIdx);
                CHECK_RC(DumpSurfaceFile("InputLuma-" + suffix, image.pLuma));
                CHECK_RC(DumpSurfaceFile("InputChromaU-" + suffix,
                        image.pChromaU));
                CHECK_RC(DumpSurfaceFile("InputChromaV-" + suffix,
                        image.pChromaV));
            }

            if (m_pTest->m_DumpFiles & VicLdcTnr::DUMP_PNG)
            {
                string suffix = Utility::StrPrintf("Loop%dSurf%d.png",
                        m_pFpCtx->LoopNum,
                        surfaceIdx);
                CHECK_RC(DumpPng("InputLuma-"+suffix, image.pLuma));
                CHECK_RC(DumpPng("InputChromaU-"+suffix, image.pChromaU));
                CHECK_RC(DumpPng("InputChromaV-"+suffix, image.pChromaV));
            }
        }
    }
    return rc;
}

RC VicLdcTnrHelper::DumpPng(const string &fileName, Surface2D *surface) const
{
    RC rc;
    if (surface->IsAllocated())
    {
        UINT32 subdev =  m_pGpuDevice->GetDisplaySubdeviceInst();

        FileHolder file(fileName.c_str(), "wb");
        if (file.GetFile() == 0)
        {
            Printf(Tee::PriError, "Error: Unable to open file %s for write!\n",
                                 fileName.c_str());
            return RC::FILE_WRITE_ERROR;
        }
        Surface2D::MappingSaver oldMapping(*surface);
        if (!surface->IsMapped() && OK != surface->Map())
        {
            Printf(Tee::PriError,
                "Error: Unable to dump SurfaceName surface - unable to map the surface!\n");
            return RC::FILE_WRITE_ERROR;
        }

        if (surface->WritePng(fileName.c_str(), subdev) != OK)
        {
            Printf(Tee::PriError,
                    "Error: Unable to Write PNG file\n");
            return RC::FILE_WRITE_ERROR;
        }
    }
    return OK;
}

//---------------------------------------------------------------------
//! \brief Optionally dump dst image to files, based on DumpFiles testarg
//!
RC VicLdcTnrHelper::DumpDstFilesIfNeeded(unique_ptr<Image> &dstImage) const
{
    RC rc;
    if (m_pTest->m_DumpFiles & VicLdcTnr::DUMP_BINARY)
    {
        const string suffix = Utility::StrPrintf("Loop:%d.bin", m_pFpCtx->LoopNum);
        CHECK_RC(DumpSurfaceFile("OutputLuma-" + suffix,
                                 dstImage->pLuma));
        CHECK_RC(DumpSurfaceFile("OutputChromaU-" + suffix,
                                 dstImage->pChromaU));
        CHECK_RC(DumpSurfaceFile("OutputChromaV-" + suffix,
                                 dstImage->pChromaV));
    }

    if (m_pTest->m_DumpFiles & VicLdcTnr::DUMP_PNG)
    {
        const string suffix = Utility::StrPrintf("Loop:%d.png", m_pFpCtx->LoopNum);
        CHECK_RC(DumpPng("OutputLuma-" + suffix,
                                 dstImage->pLuma));
        CHECK_RC(DumpPng("OutputChromaU-" + suffix,
                                 dstImage->pChromaU));
        CHECK_RC(DumpPng("OutputChromaV-" + suffix,
                                 dstImage->pChromaV));
    }

    return rc;
}

//---------------------------------------------------------------------
//! \brief Dump a surface as a binary file
//!
//! Used by DumpSrcFilesIfNeeded() and DumpDstFilesIfNeeded()
//!
/* static */ RC VicLdcTnrHelper::DumpSurfaceFile
(
    const string &filename,
    const Surface2D *surface
)
{
    RC rc;
    if (surface->IsAllocated())
    {
        vector<UINT08> data;
        SurfaceUtils::MappingSurfaceReader surfaceReader(*surface);
        data.resize(surface->GetSize());
        CHECK_RC(surfaceReader.ReadBytes(0, &data[0], data.size()));

        FileHolder fileHolder;
        CHECK_RC(fileHolder.Open(filename, "wb"));
        if (fwrite(&data[0], data.size(), 1, fileHolder.GetFile()) != 1)
        {
            Printf(Tee::PriError, "Error writing file %s\n", filename.c_str());
            return RC::FILE_WRITE_ERROR;
        }
    }
    return rc;
}

//---------------------------------------------------------------------
//! \brief Print the state of the VIC engine.  Called if Run() fails.
//!
/* static */ RC VicLdcTnrHelper::PrintVicEngineState(Tee::Priority pri)
{
    RC rc;
    Controller *pVicController;
    CHECK_RC(VicControllerMgr::Mgr()->Get(0, &pVicController));
    if (pVicController->GetInitState() < Controller::INIT_DONE)
        CHECK_RC(pVicController->Init());
    CHECK_RC(pVicController->PrintState(pri));
    return rc;
}

// These are copied from //hw/gpu_ip/unit/vic/4.2/cmod/geotran/geoDriver.cpp
// Allocation used in Tegravic.cpp is different from the one used in above
RC VicLdcTnrHelper::GetSurfParams
(
    Vic42::PIXELFORMAT format,
    bool isLW24,
    UINT32 *numPlanes,
    UINT32 *xShift,
    UINT32 *dx,
    UINT32 *dy
) const
{
    RC rc;
    for (UINT32 i = 0; i < maxSurfaces; i++)
    {
        if (xShift)
        {
            xShift[i] = 0;
        }
        if (dx && dy)
        {
            dx[i] = dy[i] = 0;
        }
    }

    switch (format)
    {
        // half horizontal, half vertical resolutions
        case Vic42::T_Y8___V8U8_N420:
            if (numPlanes)
            {
                *numPlanes = 2;
            }
            if (xShift)
            {
                xShift[lwrrentFieldLuma] = 0;
                xShift[lwrrentFieldChroma] = 1;
            }
            if (dx && dy)
            {
                dx[lwrrentFieldLuma] = dy[lwrrentFieldLuma] = 0;
                dx[lwrrentFieldChroma] = dy[lwrrentFieldChroma] = 1;
            }
            if (isLW24)
            {
                if (numPlanes)
                {
                    *numPlanes = 4;
                }
                if (xShift)
                {
                    xShift[bottomFieldLuma] = 0;     // Bottom field luma
                    xShift[bottomFieldChroma] = 1;     // Bottom field chroma
                }
                dx[bottomFieldLuma] = dy[bottomFieldLuma] = 0;
                dx[bottomFieldChroma] = dy[bottomFieldChroma] = 1;
            }
            break;
        case Vic42::T_Y10___V10U10_N444:
            if (numPlanes)
            {
                *numPlanes = 2;
            }
            if (xShift)
            {
                xShift[lwrrentFieldLuma] = 1;
                xShift[lwrrentFieldChroma] = 2;
            }
            if (dx && dy)
            {
                dx[lwrrentFieldLuma] = dy[lwrrentFieldLuma] = 0;
                dx[lwrrentFieldChroma] = dy[lwrrentFieldChroma] = 0;
            }
            break;
        default:
            Printf(Tee::PriError, "Unknown pixel format\n");
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC VicLdcTnrHelper::GetFetchParams
(
    Vic42::BLK_KIND blk_kind,
    UINT32 blk_height,
    UINT32 *fetch_width,
    UINT32 *fetch_height
)
{
    RC rc;
    switch (blk_kind)
    {
       case Vic42::BLK_KIND_PITCH:
          *fetch_width  = 4; // 256
          *fetch_height = 0; // 1
          break;
       case Vic42::BLK_KIND_GENERIC_16Bx2:
       case Vic42::BLK_KIND_BL_NAIVE:
          *fetch_width  = 2; // 64
          *fetch_height = 3 + blk_height;
          break;
       case Vic42::BLK_KIND_VP2_TILED:
          *fetch_width  = 0; // 16
          *fetch_height = 4; // 16
          break;
       default:
           Printf(Tee::PriError, "Unknown Block Kind\n");
           return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC VicLdcTnrHelper::CalcBufferPadding
(
    VicLdcTnrHelper::ImageDimensions* dims,
    UINT32 width,
    UINT32 height,
    Vic42::PIXELFORMAT pixelFormat,
    bool isLW24,
    Vic42::BLK_KIND blockKind,
    UINT32 blkHeight,
    UINT32 orgCacheWidth,
    bool isInterlaced,
    UINT32 align_height
)
{
    RC rc;
    UINT32 fieldsPerFrame = isInterlaced ? 2 : 1;
    dims->width       = width;
    dims->frameHeight = height;
    dims->fieldHeight = height / fieldsPerFrame;
    dims->pixelFormat = pixelFormat;
    dims->blockKind   = blockKind;

    UINT32 loop_count;             // number of surfaces
    UINT32 x_shift[4];             // bytes per pixel, log2 encoded
    UINT32 dx[4], dy[4];           // decimation factor, log2 encoded
    UINT32 fetch_width=0;
    UINT32 fetch_height=0;

    CHECK_RC(GetSurfParams(pixelFormat, isLW24, &loop_count, x_shift, dx, dy));

    if ((width & dx[1])!=0)
    {
        // checking if surface or file width is multiple of 2
        Printf(Tee::PriError, "XML-Error: Image width has to be a multiple of 2.\n");
        return RC::SOFTWARE_ERROR;
    }

    if ((height & dy[1])!=0)
    {
        // checking if surface or file height multiple of 2
        Printf(Tee::PriError, "XML-Error: Image height has to be a multiple of 2.\n");
        return RC::SOFTWARE_ERROR;
    }

    if (pixelFormat == Vic42::T_Y8___V8U8_N420 && isLW24)
        height >>= 1;

    CHECK_RC(GetFetchParams(blockKind, blkHeight, &fetch_width, &fetch_height));
    fetch_width = 1 << (fetch_width+4);
    fetch_height = 1 << fetch_height;

    UINT32 cache_width=0, cache_height=0;

    if (orgCacheWidth <= 4)
    {
        // Output surface uses an invalid cache_width
        cache_width  = (UINT32)(1<<(4+orgCacheWidth));
        cache_height = std::max(align_height, 256 / cache_width);
        fetch_width  = std::max(fetch_width, cache_width);
        fetch_height = std::max(fetch_height, cache_height);
    }

    dims->lumaHeight   = ALIGN_UP((height >> dy[0]), fetch_height);
    dims->lumaWidth    = ALIGN_UP(((width << x_shift[0]) >> dx[0]), fetch_width) >> x_shift[0];
    if (loop_count > 1)
    {
        dims->chromaHeight = ALIGN_UP((height >> dy[1]), fetch_height);
        dims->chromaWidth  = ALIGN_UP(((width << x_shift[1]) >> dx[1]), fetch_width) >> x_shift[1];
    }
    else
    {
        dims->chromaHeight = 0;
        dims->chromaWidth  = 0;
    }

    dims->lumaWidth     = dims->lumaWidth << x_shift[0];
    dims->lumaPitch     = dims->lumaWidth;
    dims->chromaWidth   = dims->chromaWidth << x_shift[1];
    dims->chromaPitch   = dims->chromaWidth;

    switch (pixelFormat)
    {
        // half horizontal, half vertical resolutions
        case Vic42::T_Y8___V8U8_N420:
            dims->lumaFormat    = ColorUtils::Y8;
            dims->chromaUFormat = ColorUtils::V8U8;
            dims->chromaVFormat = ColorUtils::LWFMT_NONE;
            break;
        case Vic42::T_Y10___V10U10_N444:
            // VIC engine uses 16 bits for Y10 instead of 10 bits
            // So using VOID16 instead of Y10. Also this color format is used internally by MODS
            // For Vic the color format is specified as T_Y10___V10U10_N444 in control structures
            dims->lumaFormat    = ColorUtils::VOID16;
            dims->chromaUFormat = ColorUtils::VOID16;
            dims->chromaVFormat = ColorUtils::VOID16;
            break;
        default:
            Printf(Tee::PriError, "Unknown pixel format\n");
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

//--------------------------------------------------------------------
// JavaScript interface
//
JS_CLASS_INHERIT(VicLdcTnr, GpuTest, "VicLDCTNR object");
CLASS_PROP_READWRITE
(
    VicLdcTnr,
    KeepRunning,
    bool,
    "The test will keep running as long as this flag is set"
);
CLASS_PROP_READWRITE
(
    VicLdcTnr,
    DumpFiles,
    UINT32,
    "1 = Dump the config struct to  a text file, 2 = Dump binary, 3 = Dump both"
);
CLASS_PROP_READWRITE
(
    VicLdcTnr,
    TestSampleTNR3Image,
    bool,
    "test only the predefined sample Image for TNR3"
);
CLASS_PROP_READWRITE
(
    VicLdcTnr,
    TestSampleLDCImage,
    bool,
    "test only the predefined sample Image for LDC"
);
