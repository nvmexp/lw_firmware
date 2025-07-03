/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "CJPEGParser.h"
#include "core/include/abort.h"
#include "core/include/fileholder.h"
#include "core/include/utility.h"
#include "lwca/lwdawrap.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/onedgoldensurfaces.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/tests/lwdastst.h"
#include "gpu/utility/surfrdwr.h"
#include "gputest.h"
#include "lwlwvid.h"
#include "lwdecstill.h"

#if defined(DIRECTAMODEL)
#include "opengl/modsgl.h"
#include "common/amodel/IDirectAmodelCommon.h"
#endif

//---------------------------------------------------------------------------
// Test class declaration
//---------------------------------------------------------------------------

class LwvidLWJPG : public LwdaStreamTest
{
public:
    LwvidLWJPG();

    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    void PrintJsProperties(Tee::Priority pri) override;

    bool IsSupported() override;
    bool IsSupportedVf() override { return true; }

    RC SetDefaultsForPicker(UINT32 idx) override;
    RC UpdatePickers();

    SETGET_PROP(UseLwda, bool);
    SETGET_PROP(UseEvents, bool);
    SETGET_PROP(DumpImage, bool);
    SETGET_PROP(VerifyUsingRefData, bool);
    SETGET_PROP(NumCoresToTest, UINT32);
    SETGET_PROP(SkipEngineMask, UINT32);
    SETGET_PROP(StreamSkipMask, UINT64);
    SETGET_PROP(LoopsPerFrame, UINT32);
    SETGET_PROP(EnableYuv2Rgb, bool);
    SETGET_PROP(EnableSubsampleColwersion, bool);
    SETGET_PROP(Alpha, UINT32);
    SETGET_PROP(InjectErrors, bool);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(EnableDownscaling, bool);
    SETGET_PROP(UseOnedGoldenSurface, bool);
    SETGET_PROP(SkipLoopsInStream, string);

private:

    static constexpr UINT32 MAX_PLANES = 3;
    static constexpr UINT32 NUM_CORES_TO_TEST_UNSET = ~0U;
    static constexpr UINT32 ALPHA_UNSET = ~0U;
    static constexpr UINT32 LOOPS_PER_FRAME_UNSET = ~0U;

    // Max attempts to fancy pick an output color format colwertable from input color format before
    // giving up and selecting output format = input format (no colwersion)
    static constexpr UINT32 MAX_OUTPUT_FMT_PICKER_ATTEMPTS = 5;

    static constexpr char* LWJPG_TRACES_BIN = "lwjpg.bin";

    enum class Codec
    {
        JPEG,
        MJPEG
    };

    enum class Yuv2RgbType : UINT32
    {
        BT_601_RESTRICTED_RANGE,
        BT_601_FULL_RANGE,
        BT_709_FULL_RANGE,
        BT_709_RESTRICTED_RANGE
    };

    struct StreamDescriptor
    {
        // TODO Read from tar file and store golden crcs of decoded image
        const char* streamName;
        const Codec  codec;
        // Mjpg stream has 210 frames but ref file has only first 30 frames. Parameter indicates
        // max frames to decode from stream.
        const UINT32 maxFramesToDecode;
        const UINT08 streamId;
        // Each stream may have to be decoded multiple times with different parameters (output
        // format, sub-sampling, downscaling etc.). Certain streams needs to be looped more times
        // since lot of color colwersions are possible (e.g. YUV444 source color format). Based on
        // relative weight, each stream is run a percent of total loop count. We couldn't use
        // fancy pickers for selecting a stream in order to avoid parsing the header multiple times
        // We could cache the parsed header but that complicates the flow, specifically for mjpeg
        // stream and makes test flow a bit non-intuitive
        const UINT32 weight;
    };

    struct FrameDescriptor
    {
        UINT32 lwdaStreamIdx = 0;
        UINT32 hostBufferSize = 0;
        LWDECSTILLPicParams picParams = {};
        LWDECSTILLHandle hBitstream;
        LWDECSTILLHandle hFrameOut;
        LWDECSTILLUncompressedFormat fmt;
        Lwca::HostMemory hostFrameOut;
        UINT08* pDecData = nullptr;
        UINT32  outputWidth = 0;
        UINT32  outputHeight = 0;
        UINT32  loopHash = 0; // unique loop id for golden generation, random seed etc
#if defined(DIRECTAMODEL)
        LWDECSTILLHandle hHostFrameOut;
#endif
    };

    struct JpgTarArchive
    {
        bool isArchiveRead = false;
        TarArchive tarArchive;
    };

    RC DecodeStream(const StreamDescriptor& streamDesc);
    RC ValidatePicParams(const LWDECSTILLPicParams& picParams) const;
    RC ComputeHostBufferSize(FrameDescriptor *pFrameDesc);
    RC GetDecodedData(FrameDescriptor *pFrameDesc);
    RC ReadDataFromFile(const string& fileName, vector<UINT08> *pBuffer);
    RC DumpImageData(const string& fileName, const UINT08* pBuffer, UINT32 size);
    RC ComputeOutputFrameDimensions
    (
        const FrameDescriptor& frameDesc,
        UINT32 *pSurfWidth,
        UINT32 *pSurfHeight
    );
    RC Verify
    (
        const UINT08* pHostBuffer,
        UINT32 hostBufferSize,
        const UINT08* pRefBuffer,
        UINT32 refBufferSize
    ) const;
    RC AllocHostMemory(FrameDescriptor *pFrameDesc);
    RC CopyBitstreamToDevice(FrameDescriptor* pFrameDesc, UINT08* pJpegStream, UINT32 size);
    RC CopyFrameToHost(FrameDescriptor* pFrameDesc);
    RC DecodePictures(vector<LWDECSTILLPicParams*>& picParamList);
    RC ReleaseInputOutputBuffers(FrameDescriptor *pFrameDesc);
    RC ReleaseHostBuffer(FrameDescriptor* pFrameDesc);
    RC SetupControlParams
    (
        UINT08 *pJpegStream,
        UINT32 jpegStreamSize,
        vector<FrameDescriptor> *pFrameDescList,
        const LWDECSTILLPicParams& picHeaderParams
    );
    RC ProcessOutput
    (
        FrameDescriptor *pFrameDesc,
        const string& outFileName,
        const UINT08* pRefData,
        UINT32 refSize
    );
    RC DecodeMjpegStream(const StreamDescriptor& streamDesc);
    RC DecodeJpegStream(const StreamDescriptor& streamDesc);
    RC AllocateInputOutputBuffers(FrameDescriptor *pFrameDesc);
    RC AllocateHostBuffer(FrameDescriptor *pFrameDesc);
    RC CheckOrStoreGoldens(const FrameDescriptor& frameDesc);
    RC CheckOrStoreGoldensOneD(const FrameDescriptor& frameDesc);
    RC PickOutputFormat(LWDECSTILLPicParams *pPicParams, LWDECSTILLUncompressedFormat *pOutputFmt);
    RC PickDownscaleFactor
    (
        LWDECSTILLPicParams *pPicParams,
        const LWDECSTILLUncompressedFormat& outputFmt
    );
    RC ParseHeaderPicParams
    (
        UINT08 *pJpegStream,
        UINT32 jpegStreamSize,
        LWDECSTILLPicParams *pPicParams
    );
    RC VerifyInjectedErrorDetection(const RC& rc);
    UINT32 GenerateLoopHash(UINT08 streamId, UINT32 frameId, UINT32 loopId) const;

#if defined(DIRECTAMODEL)
    RC AmodCopyFrameToHost(FrameDescriptor* pFrameDesc);
#endif

    static const char* StringifyFormat(LWDECSTILLUncompressedFormat fmt);
    static UINT32 GetNumPlanesFromFormat(LWDECSTILLUncompressedFormat format);
    static UINT32 GetDownscaleFactor(LWDECSTILLDownScale downscale);
    static LWDECSTILLUncompressedFormat GetColorFmtFromChromaFmt(LWDECSTILLChromaFormat chromaFmt);
    static bool IsColorColwersionPossible
    (
        LWDECSTILLUncompressedFormat inputFmt,
        LWDECSTILLUncompressedFormat outputFmt
    );
    static bool IsRGBFormat(LWDECSTILLUncompressedFormat fmt);
    static const char* StringifyYuv2RgbType(Yuv2RgbType yuv2RgbType);
    static const char* StringifyDownscalingFactor(const LWDECSTILLDownScale& downscaleFactor);
    static RC ReadFileFromArchive(const string& fileName, vector<UINT08> *pContent);

    CJPEGParser       m_Parser;
    LWDECSTILLCapsV2  m_DecoderCaps;
    LWDECSTILLIMAGECB m_DecoderCB;
    LWDECSTILLHandle  m_Decoder;

    struct LwdaStreamResources
    {
        LWDECSTILLEvent   m_Event;
        LWstream          m_LwdaStream;
        Lwca::Stream      m_LwdaStreamWrapper;
    };
    vector<LwdaStreamResources> m_LwdaStreamResources;

    UINT32            m_TotalWeight = 0;

    // output Y, U and V or RGB surfaces based on color format
    vector<Surface2D>                          m_OutputSurfaces;
    unique_ptr<GpuGoldenSurfaces>              m_pGoldenSurfaces;
    vector<unique_ptr<SurfaceUtils::MappingSurfaceWriter>> m_OutputSurfWriters;

    OnedGoldenSurfaces                      m_OnedGoldenSurfaces;

    // Fancy picker related members
    FancyPicker::FpContext *m_pFpCtx = nullptr;

    const StreamDescriptor m_StreamDescriptors[14] =
    {
        {"176x144_422",                   Codec::JPEG,  _UINT32_MAX, 0,  5 },
        {"AVI_mjpeg_pcm",                 Codec::MJPEG, 30,          1,  1 },
        {"176x144_420",                   Codec::JPEG,  _UINT32_MAX, 2,  20},
        {"kitnpup_c444",                  Codec::JPEG,  _UINT32_MAX, 3,  30},
        {"DSC_001_c100",                  Codec::JPEG,  _UINT32_MAX, 4,  10},
        {"parrot_96x80_422T",             Codec::JPEG,  _UINT32_MAX, 5,  20},
        {"LMQM5107_4240x2832_444",        Codec::JPEG,  _UINT32_MAX, 6,  13},
        {"flower_2120x1416_420_ratio15",  Codec::JPEG,  _UINT32_MAX, 7,  20},
        {"flower_2120x1416_422H_ratio15", Codec::JPEG,  _UINT32_MAX, 8,  20},
        {"flower_2120x1416_422V_ratio15", Codec::JPEG,  _UINT32_MAX, 9,  20},
        {"flower_4240x2832_444",          Codec::JPEG,  _UINT32_MAX, 10, 13},
        {"flower_4240x2832_400",          Codec::JPEG,  _UINT32_MAX, 11, 10},
        {"flower_16Kx16K_444",            Codec::JPEG,  _UINT32_MAX, 12,  4},
        {"garden_1080x1920_420",          Codec::MJPEG, _UINT32_MAX, 13,  1}
    };

    // We need to remember if test was started with KeepRunning set to true in
    // order to know whether to end the test early once KeepRunning has been
    // switched to false by another test
    bool m_PerpetualRun = false;
    static JpgTarArchive m_JpgTarArchive;
    set<UINT32> m_SkipLoopIndices;
    bool m_AtleastOneFrameTested = false;

    // Test arguments
    bool m_UseLwda = true;
    bool m_UseEvents = true;
    bool m_DumpImage = false;
    // TODO remove reference data based verification once golden CRC generation and
    // comparision is enabled on PVS
    bool m_VerifyUsingRefData = false;
    bool m_MultiEngineMode = false;
    UINT32 m_NumCoresToTest = NUM_CORES_TO_TEST_UNSET;
    UINT32 m_RunEngineMask = 0;
    UINT32 m_SkipEngineMask = 0;
    UINT64 m_StreamSkipMask = 0;
    UINT32 m_LoopsPerFrame = LOOPS_PER_FRAME_UNSET;
    bool m_EnableYuv2Rgb = true;
    bool m_EnableSubsampleColwersion = true;
    UINT32 m_Alpha = ALPHA_UNSET;
    bool m_InjectErrors = false;
    bool m_KeepRunning = false;
    bool m_EnableDownscaling = true;
    bool m_UseOnedGoldenSurface = true;
    string m_SkipLoopsInStream;
};

LwvidLWJPG::JpgTarArchive LwvidLWJPG::m_JpgTarArchive = {};

//---------------------------------------------------------------------------
// Test class function definitions
//---------------------------------------------------------------------------

LwvidLWJPG::LwvidLWJPG()
{
    SetName("LwvidLWJPG");
    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(FPK_LWVID_JPG_NUM_PICKERS);
}

/* static */ const char* LwvidLWJPG::StringifyFormat(LWDECSTILLUncompressedFormat fmt)
{
    switch (fmt)
    {
        case LWDECSTILLFmt_ARRAY_LW12:      return "LW12 BL";
        case LWDECSTILLFmt_DEVICE_LW12:     return "LW12";
        case LWDECSTILLFmt_DEVICE_YUV400:   return "YUV400";
        case LWDECSTILLFmt_DEVICE_YUV420:   return "YUV420";
        case LWDECSTILLFmt_DEVICE_YUV422H:  return "YUV422H";
        case LWDECSTILLFmt_DEVICE_YUV422V:  return "YUV422V";
        case LWDECSTILLFmt_DEVICE_YUY2:     return "YUY2";
        case LWDECSTILLFmt_DEVICE_YUV444:   return "YUV444";
        case LWDECSTILLFmt_DEVICE_RGBA:     return "RGBA";
        case LWDECSTILLFmt_DEVICE_BGRA:     return "BGRA";
        case LWDECSTILLFmt_DEVICE_ABGR:     return "ABGR";
        case LWDECSTILLFmt_DEVICE_ARGB:     return "ARGB";
        default:                            MASSERT(!"Unknown color format");
                                            return "Unknown color format";
    }
}

/* static */ const char* LwvidLWJPG::StringifyYuv2RgbType(Yuv2RgbType yuv2RgbType)
{
    switch (yuv2RgbType)
    {
        case Yuv2RgbType::BT_601_RESTRICTED_RANGE: return "BT_601_RESTRICTED_RANGE";
        case Yuv2RgbType::BT_601_FULL_RANGE:       return "BT_601_FULL_RANGE";
        case Yuv2RgbType::BT_709_FULL_RANGE:       return "BT_709_FULL_RANGE";
        case Yuv2RgbType::BT_709_RESTRICTED_RANGE: return "BT_709_RESTRICTED_RANGE";
        default:                                   MASSERT(!"Unknown Yuv2RgbType");
                                                   return "Unknown Yuv2RgbType";
    }
}

/* static */ const char* LwvidLWJPG::StringifyDownscalingFactor
(
    const LWDECSTILLDownScale& downscaleFactor
)
{
    switch (downscaleFactor)
    {
        case LWDECSTILLDownScale_NONE:  return "No Downscaling";
        case LWDECSTILLDownScale_2X:    return "2x Downscaling";
        case LWDECSTILLDownScale_4X:    return "4x Downscaling";
        case LWDECSTILLDownScale_8X:    return "8x Downscaling";
        default:                        MASSERT(!"Unknown Downscaling factor");
                                        return "Unknown Downscaling factor";
    }
}

/* static */ bool LwvidLWJPG::IsRGBFormat(LWDECSTILLUncompressedFormat fmt)
{
    switch (fmt)
    {
        case LWDECSTILLFmt_DEVICE_RGBA:
        case LWDECSTILLFmt_DEVICE_BGRA:
        case LWDECSTILLFmt_DEVICE_ABGR:
        case LWDECSTILLFmt_DEVICE_ARGB:
            return true;
        default:
            return false;
    }
}

/* static */ UINT32 LwvidLWJPG::GetNumPlanesFromFormat(LWDECSTILLUncompressedFormat format)
{
    switch (format)
    {
        case LWDECSTILLFmt_DEVICE_YUV400:
        case LWDECSTILLFmt_DEVICE_YUY2:
        case LWDECSTILLFmt_DEVICE_RGBA:
        case LWDECSTILLFmt_DEVICE_BGRA:
        case LWDECSTILLFmt_DEVICE_ABGR:
        case LWDECSTILLFmt_DEVICE_ARGB:
            return 1;
        case LWDECSTILLFmt_DEVICE_LW12:
            return 2;
        case LWDECSTILLFmt_DEVICE_YUV420:
        case LWDECSTILLFmt_DEVICE_YUV422H:
        case LWDECSTILLFmt_DEVICE_YUV422V:
        case LWDECSTILLFmt_DEVICE_YUV444:
            return 3;
        default:
            MASSERT(!"Unsupported color format");
            return 0;
    }
}

/* static */ bool LwvidLWJPG::IsColorColwersionPossible
(
    LWDECSTILLUncompressedFormat inputFmt,
    LWDECSTILLUncompressedFormat outputFmt
)
{
    if (outputFmt == inputFmt)
    {
        return true;
    }

    // Refer Section 1.2.8 of IAS
    switch (inputFmt)
    {
        case LWDECSTILLFmt_DEVICE_YUV400:
            return IsRGBFormat(outputFmt);

        case LWDECSTILLFmt_DEVICE_YUV420:
        case LWDECSTILLFmt_DEVICE_YUV422H:
            return (outputFmt == LWDECSTILLFmt_DEVICE_YUV420)  ||
                   (outputFmt == LWDECSTILLFmt_DEVICE_YUV422H) ||
                   IsRGBFormat(outputFmt);

        case LWDECSTILLFmt_DEVICE_YUV422V:
        case LWDECSTILLFmt_DEVICE_YUV444:
            return (outputFmt == LWDECSTILLFmt_DEVICE_YUV422H) ||
                   (outputFmt == LWDECSTILLFmt_DEVICE_YUV420)  ||
                   IsRGBFormat(outputFmt);

        case LWDECSTILLFmt_ARRAY_LW12:
        case LWDECSTILLFmt_DEVICE_LW12:
        case LWDECSTILLFmt_DEVICE_YUY2:
        case LWDECSTILLFmt_DEVICE_RGBA:
        case LWDECSTILLFmt_DEVICE_BGRA:
        case LWDECSTILLFmt_DEVICE_ABGR:
        case LWDECSTILLFmt_DEVICE_ARGB:
            return false;

        default:
            MASSERT(!"Unknown input format");
            return false;
    }
    return false;
}

/* static */ RC LwvidLWJPG::ReadFileFromArchive
(
    const string& fileName,
    vector<UINT08> *pContent
)
{
    RC rc;

    MASSERT(pContent);
    if (!pContent)
        return RC::SOFTWARE_ERROR;

    TarFile *pTarFile = m_JpgTarArchive.tarArchive.Find(fileName.c_str());
    if (!pTarFile)
    {
        Printf(Tee::PriError, "%s does not contain %s\n", LWJPG_TRACES_BIN,
            fileName.c_str());
        return RC::FILE_READ_ERROR;
    }

    const unsigned filesize = pTarFile->GetSize();
    pContent->resize(filesize);
    pTarFile->Seek(0);
    if (pTarFile->Read(reinterpret_cast<char*>(pContent->data()), filesize) != filesize)
    {
        Printf(Tee::PriError, "File read error for %s\n", fileName.c_str());
        return RC::FILE_READ_ERROR;
    }

    return rc;
}

UINT32 LwvidLWJPG::GenerateLoopHash(UINT08 streamId, UINT32 frameId, UINT32 loopId) const
{
    // Most significant byte indicates stream ID, next 10 bits indicate frame Id and remaining
    // 14 bits indicate loop Id. We have to decode same frame multiple times with different
    // output formats and downscaling factors to get coverage for features. Loop id will be
    // used to identify these variations

    constexpr UINT08 frameIdBits = 10;
    constexpr UINT08 loopIdBits  = 14;
    static const UINT32 seed = GetTestConfiguration()->Seed();

    MASSERT(frameId < (1 << frameIdBits));
    if (!(frameId < (1 << frameIdBits)))
    {
        Printf(Tee::PriError, "frameId %u is too big for golden loop hash\n", frameId);
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(loopId < (1 << loopIdBits));
    if (!(loopId < (1 << loopIdBits)))
    {
        Printf(Tee::PriError, "loopId %u is too big for golden loop hash\n", loopId);
        return RC::SOFTWARE_ERROR;
    }
    return seed + ((streamId << (frameIdBits + loopIdBits)) | (frameId << loopIdBits) | loopId);
}

RC LwvidLWJPG::CheckOrStoreGoldensOneD(const FrameDescriptor& frameDesc)
{
    RC rc;
    UINT08 *pDecData  = frameDesc.pDecData;

    MASSERT(pDecData);
    if (pDecData == nullptr)
    {
        Printf(Tee::PriError, "Decoded data missing. Unable to compute CRCs\n");
        return RC::SOFTWARE_ERROR;
    }

    constexpr UINT32 offset = 0;
    if (m_InjectErrors && (Goldelwalues::Check == GetGoldelwalues()->GetAction()))
    {
        pDecData[offset] = (pDecData[offset] > 0) ? (pDecData[offset] - 1) : 1;
    }

    CHECK_RC(m_OnedGoldenSurfaces.SetSurface("decodedstream",
             pDecData,
             frameDesc.hostBufferSize));

    GetGoldelwalues()->SetLoop(frameDesc.loopHash);
    RC crcRc = GetGoldelwalues()->Run();
    if (m_InjectErrors && GetGoldelwalues()->GetStopOnError())
    {
       CHECK_RC(VerifyInjectedErrorDetection(crcRc));
    }
    else
    {
       CHECK_RC(crcRc);
    }
    return rc;
}

RC LwvidLWJPG::CheckOrStoreGoldens(const FrameDescriptor& frameDesc)
{
    RC rc;
    const UINT32 w = frameDesc.outputWidth;
    const UINT32 h = frameDesc.outputHeight;
    UINT08 *pDecData  = frameDesc.pDecData;
    bool hasFormatChanged = false;
    const INT32 numGoldSurfaces = m_pGoldenSurfaces->NumSurfaces();

    MASSERT(pDecData);
    if (pDecData == nullptr)
    {
        Printf(Tee::PriError, "Decoded data missing. Unable to compute CRCs\n");
        return RC::SOFTWARE_ERROR;
    }

    m_pGoldenSurfaces->ClearSurfaces();

    auto PopulateOutputSurface = [&]
    (
        UINT08 surfIdx,
        UINT08 *pData,
        UINT32 widthInBytes,
        UINT32 height,
        ColorUtils::Format format,
        const char * name
    ) -> RC
    {
        RC rc;

        UINT32 bpp = ColorUtils::PixelBytes(format);

        MASSERT(widthInBytes);
        MASSERT(height);
        if (!widthInBytes || !height)
        {
            Printf(Tee::PriError, "width %u and height %u are expected to be non-zero\n",
                widthInBytes, height);
            return RC::SOFTWARE_ERROR;
        }

        MASSERT(!(widthInBytes % bpp));
        if (widthInBytes % bpp)
        {
            Printf(Tee::PriError, "width %u is not a multiple of bytes per pixel %u\n",
                widthInBytes, bpp);
            return RC::SOFTWARE_ERROR;
        }

        auto& surf = m_OutputSurfaces[surfIdx];

        const UINT32 width = widthInBytes / bpp;

        if (!surf.IsAllocated() ||
            surf.GetWidth() != width ||
            surf.GetHeight() != height ||
            surf.GetColorFormat() != format)
        {
            m_OutputSurfWriters[surfIdx]->Unmap();
            surf.Free();
            surf.SetWidth(width);
            surf.SetPitch(widthInBytes);
            surf.SetHeight(height);
            surf.SetColorFormat(format);
            surf.SetLocation(Memory::Coherent);
            CHECK_RC(surf.Alloc(GetBoundGpuDevice()));
            hasFormatChanged = true;
        }

        UINT32 offset = 0;
        if (m_InjectErrors && (Goldelwalues::Check == GetGoldelwalues()->GetAction()))
        {
            pData[offset] = (pData[offset] > 0) ? (pData[offset] - 1) : 1;
        }

        // Write the data onto the surface
        for (UINT32 row = 0; row < height; ++row)
        {
            CHECK_RC(m_OutputSurfWriters[surfIdx]->WriteBytes(offset,
                                                             &pData[offset],
                                                             widthInBytes));
            offset += widthInBytes;
        }

        m_pGoldenSurfaces->AttachSurface(&surf, name, 0);
        return rc;
    };

    // Note LwJpg engine supports only 8bpc
    switch (frameDesc.fmt)
    {
        case LWDECSTILLFmt_DEVICE_LW12:
            CHECK_RC(PopulateOutputSurface(0, pDecData, w, h, ColorUtils::Format::Y8, "Y8"));
            CHECK_RC(PopulateOutputSurface(1, pDecData + w * h, w, (h + 1) >> 1,
                                           ColorUtils::Format::U8V8, "U8V8"));
            break;
        case LWDECSTILLFmt_DEVICE_YUV400:
            CHECK_RC(PopulateOutputSurface(0, pDecData, w, h, ColorUtils::Format::Y8, "Y8"));
            break;
        case LWDECSTILLFmt_DEVICE_YUV420:
            CHECK_RC(PopulateOutputSurface(0, pDecData, w, h, ColorUtils::Format::Y8, "Y8"));
            CHECK_RC(PopulateOutputSurface(1, pDecData + w * h, (w + 1) >> 1, (h + 1) >> 1,
                ColorUtils::Format::U8, "U8"));
            CHECK_RC(PopulateOutputSurface(2, pDecData + w * h + (((w + 1) >> 1) * ((h + 1) >> 1)),
                (w + 1) >> 1, (h + 1) >> 1, ColorUtils::Format::V8, "V8"));
            break;
        case LWDECSTILLFmt_DEVICE_YUV422H:
            CHECK_RC(PopulateOutputSurface(0, pDecData, w, h, ColorUtils::Format::Y8, "Y8"));
            CHECK_RC(PopulateOutputSurface(1, pDecData + w * h, (w + 1) >> 1, h,
                ColorUtils::Format::U8, "U8"));
            CHECK_RC(PopulateOutputSurface(2, pDecData +  w * h + (((w + 1) >> 1) * h),
                (w + 1) >> 1, h, ColorUtils::Format::V8, "V8"));
            break;
        case LWDECSTILLFmt_DEVICE_YUV422V:
            CHECK_RC(PopulateOutputSurface(0, pDecData, w, h, ColorUtils::Format::Y8, "Y8"));
            CHECK_RC(PopulateOutputSurface(1, pDecData + w * h, w, (h + 1) >> 1,
                ColorUtils::Format::U8, "U8"));
            CHECK_RC(PopulateOutputSurface(2, pDecData + w * h + (w * ((h + 1) >> 1)),
                w, (h + 1) >> 1, ColorUtils::Format::V8, "V8"));
            break;
        case LWDECSTILLFmt_DEVICE_YUY2:
            CHECK_RC(PopulateOutputSurface(0, pDecData, w * 2, h,
                ColorUtils::Format::Y8_U8__Y8_V8_N422, "YUY2"));
            break;
        case LWDECSTILLFmt_DEVICE_YUV444:
            CHECK_RC(PopulateOutputSurface(0, pDecData, w, h, ColorUtils::Format::Y8, "Y8"));
            CHECK_RC(PopulateOutputSurface(1, pDecData + w * h, w, h,
                ColorUtils::Format::U8, "U8"));
            CHECK_RC(PopulateOutputSurface(2, pDecData + 2 * w * h, w, h,
                ColorUtils::Format::V8, "V8"));
            break;
        // For RGBA formats, LwJpg H/W stores R, G, B, A components as bytes in that order
        // However, MODS interprets pixel data as one 32 bit word and and splits the word into
        // individual component. On little-endian platforms, this will reverse positions of
        // components within the pixel. Use appropriate color format to dump png correctly
        case LWDECSTILLFmt_DEVICE_RGBA:
            CHECK_RC(PopulateOutputSurface(0, pDecData, 4 * w, h,
                ColorUtils::Format::A8B8G8R8, "R8G8B8A8"));
            break;
        case LWDECSTILLFmt_DEVICE_BGRA:
            CHECK_RC(PopulateOutputSurface(0, pDecData, 4 * w, h,
                ColorUtils::Format::A8R8G8B8, "B8G8R8A8"));
            break;
        case LWDECSTILLFmt_DEVICE_ABGR:
            CHECK_RC(PopulateOutputSurface(0, pDecData, 4 * w, h,
                ColorUtils::Format::R8G8B8A8, "A8B8G8R8"));
            break;
        case LWDECSTILLFmt_DEVICE_ARGB:
            CHECK_RC(PopulateOutputSurface(0, pDecData, 4 * w, h,
                ColorUtils::Format::B8G8R8A8, "A8R8G8B8"));
            break;
        default:
            MASSERT(!"Unsupported format\n");
            return RC::SOFTWARE_ERROR;
    }

    if (hasFormatChanged || numGoldSurfaces != m_pGoldenSurfaces->NumSurfaces())
    {
        const string crcSuffix = Utility::StrPrintf("Fmt%s", StringifyFormat(frameDesc.fmt));
        // OverrideSuffix ilwoke Goldelwalues::OpenDb and CRC buffer with correct size &
        // element count is created
        GetGoldelwalues()->OverrideSuffix(crcSuffix);
    }

    GetGoldelwalues()->SetLoop(frameDesc.loopHash);
    RC crcRc = GetGoldelwalues()->Run();
    if (m_InjectErrors && GetGoldelwalues()->GetStopOnError())
    {
        CHECK_RC(VerifyInjectedErrorDetection(crcRc));
    }
    else
    {
        CHECK_RC(crcRc);
    }
    return rc;
}

RC LwvidLWJPG::SetDefaultsForPicker(UINT32 idx)
{
    RC rc;
    FancyPicker * pFp = &((*GetFancyPickerArray())[idx]);

    switch (idx)
    {
        case FPK_LWVID_JPG_COLOR_FORMAT:
            CHECK_RC(pFp->ConfigRandom());
            pFp->AddRandItem(1, LWDECSTILLFmt_DEVICE_YUV420);
            pFp->AddRandItem(1, LWDECSTILLFmt_DEVICE_YUV422H);
            pFp->AddRandItem(2, LWDECSTILLFmt_DEVICE_RGBA);
            pFp->AddRandItem(2, LWDECSTILLFmt_DEVICE_BGRA);
            pFp->AddRandItem(2, LWDECSTILLFmt_DEVICE_ABGR);
            pFp->AddRandItem(2, LWDECSTILLFmt_DEVICE_ARGB);
            pFp->CompileRandom();
            break;
        case FPK_LWVID_JPG_YUV_TO_RGB_PARAMS:
            CHECK_RC(pFp->ConfigRandom());
            pFp->AddRandItem(1, static_cast<UINT32>(Yuv2RgbType::BT_601_RESTRICTED_RANGE));
            pFp->AddRandItem(1, static_cast<UINT32>(Yuv2RgbType::BT_601_FULL_RANGE));
            pFp->AddRandItem(1, static_cast<UINT32>(Yuv2RgbType::BT_709_FULL_RANGE));
            pFp->AddRandItem(1, static_cast<UINT32>(Yuv2RgbType::BT_709_RESTRICTED_RANGE));
            pFp->CompileRandom();
            break;
        case FPK_LWVID_JPG_ALPHA:
            CHECK_RC(pFp->ConfigRandom());
            pFp->AddRandRange(1, 0x00, 0xFF);
            pFp->CompileRandom();
            break;
        case FPK_LWVID_JPG_DOWNSCALE_FACTOR:
            CHECK_RC(pFp->ConfigRandom());
            pFp->AddRandItem(1, LWDECSTILLDownScale_NONE);
            pFp->AddRandItem(1, LWDECSTILLDownScale_2X);
            pFp->AddRandItem(1, LWDECSTILLDownScale_4X);
            pFp->AddRandItem(1, LWDECSTILLDownScale_8X);
            pFp->CompileRandom();
            break;
        default:
            Printf(Tee::PriError, "Unknown fancy picker index %u\n", idx);
            MASSERT(!"Unknown fancy picker index");
            return RC::SOFTWARE_ERROR;
    }
    return RC::OK;
}

RC LwvidLWJPG::UpdatePickers()
{
    RC rc;
    FancyPickerArray *pPickers = GetFancyPickerArray();

    if (!(*pPickers)[FPK_LWVID_JPG_DOWNSCALE_FACTOR].WasSetFromJs())
    {
        FancyPicker *pPicker = &(*GetFancyPickerArray())[FPK_LWVID_JPG_DOWNSCALE_FACTOR];

        CHECK_RC(pPicker->ConfigRandom());
        pPicker->AddRandItem(1, LWDECSTILLDownScale_NONE);
        if (m_DecoderCaps.downscale & (1 << LWDECSTILLDownScale_2X))
        {
            pPicker->AddRandItem(1, LWDECSTILLDownScale_2X);
        }
        if (m_DecoderCaps.downscale & (1 << LWDECSTILLDownScale_4X))
        {
            pPicker->AddRandItem(1, LWDECSTILLDownScale_4X);
        }
        if (m_DecoderCaps.downscale & (1 << LWDECSTILLDownScale_8X))
        {
            pPicker->AddRandItem(1, LWDECSTILLDownScale_8X);
        }
        pPicker->CompileRandom();
    }

    return rc;
}

RC LwvidLWJPG::CopyFrameToHost(FrameDescriptor* pFrameDesc)
{
    RC rc;
    LWDA_MEMCPY2D m2d = {};
    LWDECSTILLUncompressedFrame frm;
    UINT08 *pHostFrameOut = static_cast<UINT08*>(pFrameDesc->hostFrameOut.GetPtr());

    MASSERT(pFrameDesc);
    if (!pFrameDesc)
        return RC::SOFTWARE_ERROR;

    const UINT32 w = pFrameDesc->outputWidth;
    const UINT32 h = pFrameDesc->outputHeight;

    CHECK_LW_RC(m_DecoderCB.MapUncompressedFrame(m_Decoder, &frm, pFrameDesc->hFrameOut));

    MASSERT(GetNumPlanesFromFormat(frm.fmt) == frm.numPlanes);
    if (GetNumPlanesFromFormat(frm.fmt) != frm.numPlanes)
    {
        Printf(Tee::PriError, "Format %s expects %u planes but decoded frame has only %u planes\n",
            StringifyFormat(frm.fmt),
            GetNumPlanesFromFormat(frm.fmt),
            frm.numPlanes);
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(pFrameDesc->fmt == frm.fmt);
    if (pFrameDesc->fmt != frm.fmt)
    {
        Printf(Tee::PriError, "Expected format %s but detected format for decoded buffer %s\n",
            StringifyFormat(pFrameDesc->fmt),
            StringifyFormat(frm.fmt));
        return RC::SOFTWARE_ERROR;
    }

    m2d.srcMemoryType = LW_MEMORYTYPE_DEVICE;
    m2d.dstMemoryType = LW_MEMORYTYPE_HOST;

    auto Do2DMemcpy = [&](UINT08 srcPlaneIdx, UINT08 *pDst, UINT32 dstWidth, UINT32 dstHeight) -> RC
    {
        RC rc;
        m2d.srcDevice    = frm.planes[srcPlaneIdx].mem.devPtr;
        m2d.dstHost      = pDst;
        m2d.srcPitch     = frm.planes[srcPlaneIdx].widthInBytes;
        m2d.dstPitch     = dstWidth;
        m2d.WidthInBytes = dstWidth;
        m2d.Height       = dstHeight;
        CHECK_LW_RC(lwMemcpy2DAsync(&m2d,
            m_LwdaStreamResources[pFrameDesc->lwdaStreamIdx].m_LwdaStream));
        return rc;
    };

    switch (frm.fmt)
    {
        case LWDECSTILLFmt_DEVICE_LW12:
            CHECK_RC(Do2DMemcpy(0, pHostFrameOut, w, h)); //luma
            CHECK_RC(Do2DMemcpy(1, pHostFrameOut + w * h, w, (h + 1) >> 1)); //Chroma
            break;
        case LWDECSTILLFmt_DEVICE_YUV400:
            CHECK_RC(Do2DMemcpy(0, pHostFrameOut, w, h));
            break;
        case LWDECSTILLFmt_DEVICE_YUV420:
            CHECK_RC(Do2DMemcpy(0, pHostFrameOut, w, h)); // Y
            CHECK_RC(Do2DMemcpy(1, pHostFrameOut + w * h, (w + 1) >> 1, (h + 1) >> 1)); // U
            CHECK_RC(Do2DMemcpy(2, pHostFrameOut + w * h + (((w + 1) >> 1) * ((h + 1) >> 1)),
                (w + 1) >> 1, (h + 1) >> 1)); // V
            break;
        case LWDECSTILLFmt_DEVICE_YUV422H:
            CHECK_RC(Do2DMemcpy(0, pHostFrameOut, w, h)); // Y
            CHECK_RC(Do2DMemcpy(1, pHostFrameOut + w * h, (w + 1) >> 1, h)); // U
            CHECK_RC(Do2DMemcpy(2, pHostFrameOut + w * h + (((w + 1) >> 1) * h),
               (w + 1) >> 1, h)); // V
            break;
        case LWDECSTILLFmt_DEVICE_YUV422V:
            CHECK_RC(Do2DMemcpy(0, pHostFrameOut, w, h)); // Y
            CHECK_RC(Do2DMemcpy(1, pHostFrameOut + w * h, w, (h + 1) >> 1)); // U
            CHECK_RC(Do2DMemcpy(2, pHostFrameOut + w * h + (w * ((h + 1) >> 1)),
                w, (h + 1) >> 1)); // V
            break;
        case LWDECSTILLFmt_DEVICE_YUY2:
            CHECK_RC(Do2DMemcpy(0, pHostFrameOut, w * 2, h));
            break;
        case LWDECSTILLFmt_DEVICE_YUV444:
            CHECK_RC(Do2DMemcpy(0, pHostFrameOut, w, h)); // Y
            CHECK_RC(Do2DMemcpy(1, pHostFrameOut + w * h, w, h)); // U
            CHECK_RC(Do2DMemcpy(2, pHostFrameOut + 2 * w * h, w, h)); // V
            break;
        case LWDECSTILLFmt_DEVICE_RGBA:
        case LWDECSTILLFmt_DEVICE_BGRA:
        case LWDECSTILLFmt_DEVICE_ABGR:
        case LWDECSTILLFmt_DEVICE_ARGB:
            CHECK_RC(Do2DMemcpy(0, pHostFrameOut, 4 * w, h));
            break;
        default:
            MASSERT(!"Unsupported format\n");
            return RC::SOFTWARE_ERROR;
    }
    CHECK_LW_RC(lwStreamSynchronize(m_LwdaStreamResources[pFrameDesc->lwdaStreamIdx].m_LwdaStream));
    pFrameDesc->pDecData = static_cast<UINT08*>(pFrameDesc->hostFrameOut.GetPtr());
    return rc;
}

#if defined(DIRECTAMODEL)
RC LwvidLWJPG::AmodCopyFrameToHost(FrameDescriptor* pFrameDesc)
{
    RC rc;
    LWDECSTILLUncompressedFrame frm;
    UINT08* dptr[MAX_PLANES] = {};

    MASSERT(pFrameDesc);
    if (!pFrameDesc)
        return RC::SOFTWARE_ERROR;

    CHECK_LW_RC(m_DecoderCB.LockSurface(m_Decoder, &(pFrameDesc->pDecData),
        pFrameDesc->hHostFrameOut));
    UINT08* pData = pFrameDesc->pDecData;
    const UINT32 w = pFrameDesc->outputWidth;
    const UINT32 h = pFrameDesc->outputHeight;
    const UINT32 numPlanes = GetNumPlanesFromFormat(pFrameDesc->fmt);

    DEFER
    {
        for (UINT32 i = 0; i < numPlanes; i++)
        {
            if (dptr[i] != nullptr)
            {
               RC rc = Lwca::LwdaErrorToRC(m_DecoderCB.
                         UnlockUncompressedFrame(m_Decoder, pFrameDesc->hFrameOut, i));
               if (rc != RC::OK)
                   Printf(Tee::PriError,
                       "UnlockUncompressedFrame failed for plane %u with rc %s\n",
                       i, rc.Message());
            }
        }
    };

    for (UINT32 i = 0; i < numPlanes; i++)
        CHECK_LW_RC(m_DecoderCB.LockUncompressedFrame(m_Decoder, &dptr[i],
            &frm, pFrameDesc->hFrameOut, i));

    MASSERT(GetNumPlanesFromFormat(frm.fmt) == frm.numPlanes);
    if (GetNumPlanesFromFormat(frm.fmt) != frm.numPlanes)
    {
        Printf(Tee::PriError, "Format %s expects %u planes but decoded frame has only %u planes\n",
            StringifyFormat(frm.fmt),
            GetNumPlanesFromFormat(frm.fmt),
            frm.numPlanes);
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(pFrameDesc->fmt == frm.fmt);
    if (pFrameDesc->fmt != frm.fmt)
    {
        Printf(Tee::PriError, "Expected format %s but detected format for decoded buffer %s\n",
            StringifyFormat(pFrameDesc->fmt),
            StringifyFormat(frm.fmt));
        return RC::SOFTWARE_ERROR;
    }

    // TODO Probably we can have separate surfaces for Y, U, V based on planar/semiplanar config
    auto CopyRows = [&](UINT08* pSrc, UINT08* pDst, UINT32 rowSize, UINT32 numRows, UINT32 surfWidth)
    {
        for (UINT32 row = 0; row < numRows; row++)
        {
            memcpy(pDst + (row * rowSize), pSrc + (row * surfWidth), rowSize);
        }
    };

    switch (frm.fmt)
    {
        case LWDECSTILLFmt_DEVICE_LW12:
            CopyRows(dptr[0], pData, w, h, frm.planes[0].widthInBytes); // Y
            CopyRows(dptr[1], pData + (w * h), w, ((h + 1) >> 1), frm.planes[1].widthInBytes); // U
            break;

        case LWDECSTILLFmt_DEVICE_YUV400:
            CopyRows(dptr[0], pData, w, h, frm.planes[0].widthInBytes); // Y
            break;

        case LWDECSTILLFmt_DEVICE_YUV420:
            CopyRows(dptr[0], pData, w, h, frm.planes[0].widthInBytes); // Y
            CopyRows(dptr[1], pData + (w * h), ((w + 1) >> 1),
                ((h + 1) >> 1), frm.planes[1].widthInBytes); // U
            CopyRows(dptr[2], pData + (w * h) + (((w + 1) >> 1)*((h + 1) >> 1)),
                ((w + 1) >> 1), ((h + 1) >> 1), frm.planes[2].widthInBytes); // V
            break;

        case LWDECSTILLFmt_DEVICE_YUV422H:
            CopyRows(dptr[0], pData, w, h, frm.planes[0].widthInBytes); // Y
            CopyRows(dptr[1], pData + (w * h), ((w + 1) >> 1), h, frm.planes[1].widthInBytes); // U
            CopyRows(dptr[2], pData + (w * h) + (((w + 1) >> 1) * h),
                ((w + 1) >> 1), h, frm.planes[2].widthInBytes); // V
            break;

        case LWDECSTILLFmt_DEVICE_YUV422V:
            CopyRows(dptr[0], pData, w, h, frm.planes[0].widthInBytes); // Y
            CopyRows(dptr[1], pData + (w * h), w, ((h + 1) >> 1), frm.planes[1].widthInBytes); // U
            CopyRows(dptr[2], pData + (w * h) + (w * ((h + 1) >> 1)),
                w, ((h + 1) >> 1), frm.planes[2].widthInBytes); // V
            break;

        case LWDECSTILLFmt_DEVICE_YUY2:
            CopyRows(dptr[0], pData, w, h, frm.planes[0].widthInBytes);
            break;

        case LWDECSTILLFmt_DEVICE_YUV444:
            CopyRows(dptr[0], pData, w, h, frm.planes[0].widthInBytes); // Y
            CopyRows(dptr[1], pData + (w * h), w, h, frm.planes[1].widthInBytes); // U
            CopyRows(dptr[2], pData + (2 * (w * h)), w, h, frm.planes[2].widthInBytes); // V
            break;

        case LWDECSTILLFmt_DEVICE_RGBA:
        case LWDECSTILLFmt_DEVICE_BGRA:
        case LWDECSTILLFmt_DEVICE_ABGR:
        case LWDECSTILLFmt_DEVICE_ARGB:
            CopyRows(dptr[0], pData, 4 * w, h, frm.planes[0].widthInBytes);
            break;

        default:
            MASSERT(!"Unsupported format\n");
            return RC::SOFTWARE_ERROR;
    }
    return rc;

}
#endif

RC LwvidLWJPG::GetDecodedData(FrameDescriptor *pFrameDesc)
{
    RC rc;

    MASSERT(pFrameDesc);

    if (!pFrameDesc)
        return RC::SOFTWARE_ERROR;

    const auto& picParams = pFrameDesc->picParams;
    UINT32 downScaleFactor = GetDownscaleFactor(picParams.CodecSpecific.jpeg.power2_downscale);
    pFrameDesc->outputWidth = CEIL_DIV(picParams.picWidth, downScaleFactor);
    pFrameDesc->outputHeight = CEIL_DIV(picParams.picHeight, downScaleFactor);

#if defined(DIRECTAMODEL)
    if (!m_UseLwda)
    {
        CHECK_RC(AmodCopyFrameToHost(pFrameDesc));
    }
    else
#endif
    {
        CHECK_RC(CopyFrameToHost(pFrameDesc));
    }
    return rc;
}

// TODO Can possibly move to common utils
RC LwvidLWJPG::ReadDataFromFile(const string& fileName, vector<UINT08> *pBuffer)
{
    RC rc;
    long fileSize = 0;
    // Avoid re-reading the same file contents for different perf points
    static map<string, vector<UINT08>> jpegFiles;

    MASSERT(pBuffer);
    if (pBuffer == nullptr)
        return RC::SOFTWARE_ERROR;

    if (jpegFiles.find(fileName) != jpegFiles.end())
    {
        *pBuffer = jpegFiles[fileName];
        return rc;
    }

    FileHolder fileHolder(fileName, "rb");
    FILE *pFile = fileHolder.GetFile();
    if (!pFile)
    {
        Printf(Tee::PriError, "Cannot open file %s\n", fileName.c_str());
        return RC::CANNOT_OPEN_FILE;
    }

    CHECK_RC(Utility::FileSize(pFile, &fileSize));
    pBuffer->resize(fileSize);

    size_t bytesRead = fread(&(*pBuffer)[0], 1, fileSize, pFile);
    if (bytesRead != static_cast<size_t>(fileSize))
    {
        Printf(Tee::PriError, "File '%s' read error\n", fileName.c_str());
        return RC::FILE_READ_ERROR;
    }
    jpegFiles[fileName] = vector<UINT08>(*pBuffer);
    return rc;
}

RC LwvidLWJPG::DumpImageData(const string& fileName, const UINT08* pBuffer, UINT32 size)
{
    MASSERT(pBuffer);
    if (!pBuffer)
        return RC::SOFTWARE_ERROR;

    FileHolder fileHolder(fileName, "wb");
    FILE *pFile = fileHolder.GetFile();
    if (!pFile)
    {
        Printf(Tee::PriError, "Cannot open file %s\n", fileName.c_str());
        return RC::CANNOT_OPEN_FILE;
    }

    if (fwrite(pBuffer, 1, size, pFile) != size)
    {
        Printf(Tee::PriError, "File '%s' write error\n", fileName.c_str());
        return RC::FILE_WRITE_ERROR;
    }
    return RC::OK;
}

/* static */ UINT32 LwvidLWJPG::GetDownscaleFactor(LWDECSTILLDownScale downscale)
{
    switch (downscale)
    {
        case LWDECSTILLDownScale_NONE:
            return 1;
        case LWDECSTILLDownScale_2X:
            return 2;
        case LWDECSTILLDownScale_4X:
            return 4;
        case LWDECSTILLDownScale_8X:
            return 8;
        default:
            MASSERT(!"Unknown downscaling factor\n");
    }
    return 1;
}

RC LwvidLWJPG::ComputeOutputFrameDimensions
(
    const FrameDescriptor& frameDesc,
    UINT32                 *pSurfWidth,
    UINT32                 *pSurfHeight
)
{
    UINT32 hmax = 0, vmax = 0;

    MASSERT(pSurfWidth);
    MASSERT(pSurfHeight);
    if (!pSurfHeight || !pSurfWidth)
        return RC::SOFTWARE_ERROR;

    const auto& jpegParams = frameDesc.picParams.CodecSpecific.jpeg;
    UINT32 downScaleFactor = GetDownscaleFactor(jpegParams.power2_downscale);

    for (UINT32 i = 0; i < jpegParams.numComponents; i++)
    {
        hmax = MAX(jpegParams.blkPar[i].hblock, hmax);
        vmax = MAX(jpegParams.blkPar[i].vblock, vmax);
    }

    *pSurfWidth = jpegParams.mlw_width * hmax * 8;
    *pSurfHeight = jpegParams.mlw_height * vmax * 8;

    *pSurfWidth = CEIL_DIV(*pSurfWidth, downScaleFactor);
    *pSurfHeight = CEIL_DIV(*pSurfHeight, downScaleFactor);

    return RC::OK;
}

/* static */ LWDECSTILLUncompressedFormat LwvidLWJPG::GetColorFmtFromChromaFmt
(
    LWDECSTILLChromaFormat chromaFmt
)
{
    switch (chromaFmt)
    {
        case LWDECSTILLChromaFormat_400:
            return LWDECSTILLFmt_DEVICE_YUV400;
        case LWDECSTILLChromaFormat_420:
            return LWDECSTILLFmt_DEVICE_YUV420;
        case LWDECSTILLChromaFormat_422H:
            return LWDECSTILLFmt_DEVICE_YUV422H;
        case LWDECSTILLChromaFormat_422V:
            return LWDECSTILLFmt_DEVICE_YUV422V;
        case LWDECSTILLChromaFormat_444:
            return LWDECSTILLFmt_DEVICE_YUV444;
        default:
            MASSERT(!"Unknown chroma format\n");
            return LWDECSTILLFmt_DEVICE_RGBA;
    }
}

RC LwvidLWJPG::ComputeHostBufferSize
(
    FrameDescriptor *pFrameDesc
)
{
    MASSERT(pFrameDesc);
    if (!pFrameDesc)
        return RC::SOFTWARE_ERROR;

    const auto& picParams = pFrameDesc->picParams;

    UINT32 downscaleFactor = GetDownscaleFactor(picParams.CodecSpecific.jpeg.power2_downscale);
    UINT32 w = CEIL_DIV(picParams.picWidth, downscaleFactor);
    UINT32 h = CEIL_DIV(picParams.picHeight, downscaleFactor);
    pFrameDesc->hostBufferSize = 0;

    switch (pFrameDesc->fmt)
    {
        case LWDECSTILLFmt_DEVICE_LW12:
            pFrameDesc->hostBufferSize = w*h + w*((h + 1) >> 1);
            break;
        case LWDECSTILLFmt_DEVICE_YUV400:
            pFrameDesc->hostBufferSize = w * h;
            break;
        case LWDECSTILLFmt_DEVICE_YUV420:
            pFrameDesc->hostBufferSize = w * h + 2 * (((w + 1) >> 1) * ((h + 1) >> 1));
            break;
        case LWDECSTILLFmt_DEVICE_YUV422H:
            pFrameDesc->hostBufferSize = w * h + 2 * (((w + 1) >> 1)*h);
            break;
        case LWDECSTILLFmt_DEVICE_YUV422V:
            pFrameDesc->hostBufferSize = w * h + 2 * (w*((h + 1) >> 1));
            break;
        case LWDECSTILLFmt_DEVICE_YUY2:
            pFrameDesc->hostBufferSize = 2 * w*h;
            break;
        case LWDECSTILLFmt_DEVICE_YUV444:
            pFrameDesc->hostBufferSize = 3 * w * h;
            break;
        case LWDECSTILLFmt_DEVICE_RGBA:
        case LWDECSTILLFmt_DEVICE_BGRA:
        case LWDECSTILLFmt_DEVICE_ABGR:
        case LWDECSTILLFmt_DEVICE_ARGB:
            pFrameDesc->hostBufferSize = 4 * w * h;
            break;
        default:
            MASSERT(!"Unsupported output format\n");
            pFrameDesc->hostBufferSize = 0;
            return RC::SOFTWARE_ERROR;
    }
    return RC::OK;
}

RC LwvidLWJPG::Verify
(
    const UINT08* pHostBuffer,
    UINT32        hostBufferSize,
    const UINT08* pRefBuffer,
    UINT32        refBufferSize
) const
{
    MASSERT(pHostBuffer);
    MASSERT(pRefBuffer);

    if (!pRefBuffer || !pHostBuffer)
        return RC::SOFTWARE_ERROR;

    if (hostBufferSize != refBufferSize)
    {
        Printf(Tee::PriError, "Decoded image size (%u) !=  reference image size (%u)\n",
            hostBufferSize, refBufferSize);
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    // TODO optimize these checks and move to golden based method
    UINT64 mse = 0;
    for (UINT32 i = 0; i < hostBufferSize; i++)
    {
        INT32 diff = pHostBuffer[i] - pRefBuffer[i];
        // ignore +- 1 differences oclwrred at IDCT stage
        if (diff != 0 && diff != 1 && diff != -1)
            mse += diff * diff;
    }

    if (mse != 0)
    {
        Printf(Tee::PriError, "Decoded image does not match with reference image\n");
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    return RC::OK;
}

RC LwvidLWJPG::AllocHostMemory(FrameDescriptor *pFrameDesc)
{
    RC rc;

    MASSERT(pFrameDesc);
    if (!pFrameDesc)
        return RC::SOFTWARE_ERROR;

#if defined(DIRECTAMODEL)
    if (!m_UseLwda)
    {
        CHECK_LW_RC(m_DecoderCB.AllocHostMem(m_Decoder, &(pFrameDesc->hHostFrameOut),
            pFrameDesc->hostBufferSize));
    }
    else
#endif
    {
        CHECK_RC(GetLwdaInstance().AllocHostMem(GetBoundLwdaDevice(),
            pFrameDesc->hostBufferSize, &(pFrameDesc->hostFrameOut)));
    }
    return rc;
}

RC LwvidLWJPG::CopyBitstreamToDevice(FrameDescriptor* pFrameDesc, UINT08* pJpegStream, UINT32 size)
{
    RC rc;

    MASSERT(pFrameDesc);
    MASSERT(pJpegStream);
    if (!pFrameDesc || !pJpegStream)
        return RC::SOFTWARE_ERROR;

#if defined(DIRECTAMODEL)
    if (!m_UseLwda)
    {
        UINT08* dptr = nullptr;
        DEFER
        {
            m_DecoderCB.UnlockSurface(m_Decoder, pFrameDesc->hBitstream);
        };
        CHECK_LW_RC(m_DecoderCB.LockSurface(m_Decoder, &dptr, pFrameDesc->hBitstream));
        MASSERT(dptr);
        if (!dptr)
            return RC::SOFTWARE_ERROR;
        memcpy(dptr, pJpegStream, size);
    }
    else
#endif
    {
        LWdeviceptr dptr = 0;
        CHECK_LW_RC(m_DecoderCB.MapCompressedFrame(m_Decoder, &dptr, pFrameDesc->hBitstream));
        MASSERT(dptr);
        if (!dptr)
        {
            Printf(Tee::PriError, "MapCompressedFrame failed \n");
            return RC::SOFTWARE_ERROR;
        }
        CHECK_LW_RC(lwMemcpyHtoDAsync(dptr, pJpegStream, size,
            m_LwdaStreamResources[pFrameDesc->lwdaStreamIdx].m_LwdaStream));
        CHECK_LW_RC(lwStreamSynchronize(m_LwdaStreamResources[pFrameDesc->lwdaStreamIdx].m_LwdaStream));
    }
    return rc;
}

RC LwvidLWJPG::ReleaseHostBuffer(FrameDescriptor* pFrameDesc)
{
    StickyRC rc;

    MASSERT(pFrameDesc);
    if (!pFrameDesc)
        return RC::OK;

    pFrameDesc->hostFrameOut.Free();

#if defined(DIRECTAMODEL)
    if (pFrameDesc->pDecData)
    {
        rc = Lwca::LwdaErrorToRC(m_DecoderCB.UnlockSurface(m_Decoder, pFrameDesc->hHostFrameOut));
    }
    if (pFrameDesc->hHostFrameOut)
    {
        rc = Lwca::LwdaErrorToRC(m_DecoderCB.FreeHostMem(m_Decoder, pFrameDesc->hHostFrameOut));
    }
#endif
    pFrameDesc->pDecData   = nullptr;
    return rc;
}

RC LwvidLWJPG::ReleaseInputOutputBuffers(FrameDescriptor* pFrameDesc)
{
    StickyRC rc;

    MASSERT(pFrameDesc);
    if (!pFrameDesc)
        return RC::OK;

    if (pFrameDesc->hBitstream)
        rc = Lwca::LwdaErrorToRC(m_DecoderCB.FreeCompressedFrame(m_Decoder, pFrameDesc->hBitstream));
    if (pFrameDesc->hFrameOut)
        rc = Lwca::LwdaErrorToRC(m_DecoderCB.FreeUncompressedFrame(m_Decoder, pFrameDesc->hFrameOut));

    pFrameDesc->hBitstream = 0;
    pFrameDesc->hFrameOut  = 0;

    return rc;
}

RC LwvidLWJPG::DecodePictures(vector<LWDECSTILLPicParams*>& picParamList)
{
    RC rc;
#if defined(DIRECTAMODEL)
    if (!m_UseLwda || !m_UseEvents)
    {
        if (m_MultiEngineMode)
        {
            const UINT32 numEngines = static_cast<UINT32>(picParamList.size());
            INT32 nextRunEngineIdx = -1;
            for (UINT32 engineIdx = 0; engineIdx < numEngines; engineIdx++)
            {
                nextRunEngineIdx = Utility::BitScanForward(
                    m_RunEngineMask, nextRunEngineIdx + 1);
                MASSERT(nextRunEngineIdx >= 0);
                const LWDECSTILLPicParams *picParamsList[1] = { picParamList[engineIdx] };
                const UINT32 engNumAndNumPics = (nextRunEngineIdx << 24) + 1;
                CHECK_LW_RC(m_DecoderCB.DecodePictureMultiple(m_Decoder,
                    picParamsList, engNumAndNumPics, NULL, NULL));
            }
        }
        else
        {
            CHECK_LW_RC(m_DecoderCB.DecodePictureMultiple(m_Decoder,
                const_cast<const LWDECSTILLPicParams**>(picParamList.data()),
                static_cast<UINT32>(picParamList.size()), NULL, NULL));
        }
    }
    else
#endif
    {
        const UINT32 numStreams = m_MultiEngineMode ? static_cast<UINT32>(picParamList.size()) : 1;
        for (UINT32 streamIdx = 0; streamIdx < numStreams; streamIdx++)
        {
            const LwdaStreamResources& csr = m_LwdaStreamResources[streamIdx];
            CHECK_LW_RC(m_DecoderCB.StreamEventSignal(m_Decoder, csr.m_Event, csr.m_LwdaStream));
        }
        INT32 nextRunEngineIdx = -1;
        for (UINT32 streamIdx = 0; streamIdx < numStreams; streamIdx++)
        {
            UINT32 engNumAndNumPics;
            const LWDECSTILLPicParams *picParamsList[1];
            const LWDECSTILLPicParams **picParams;
            if (m_MultiEngineMode)
            {
                nextRunEngineIdx = Utility::BitScanForward(
                    m_RunEngineMask, nextRunEngineIdx + 1);
                MASSERT(nextRunEngineIdx >= 0);
                engNumAndNumPics = (nextRunEngineIdx << 24) + 1;
                picParamsList[0] = picParamList[streamIdx];
                picParams = &picParamsList[0];
            }
            else
            {
                engNumAndNumPics = static_cast<UINT32>(picParamList.size());
                picParams = const_cast<const LWDECSTILLPicParams**>(picParamList.data());
            }
            const LwdaStreamResources& csr = m_LwdaStreamResources[streamIdx];
            CHECK_LW_RC(m_DecoderCB.DecodePictureMultiple(m_Decoder,
                picParams, engNumAndNumPics, csr.m_Event, csr.m_Event));
        }
        for (UINT32 streamIdx = 0; streamIdx < numStreams; streamIdx++)
        {
            const LwdaStreamResources& csr = m_LwdaStreamResources[streamIdx];
            CHECK_LW_RC(m_DecoderCB.StreamEventWait(m_Decoder, csr.m_Event, csr.m_LwdaStream));
        }
    }
    return rc;
}

RC LwvidLWJPG::AllocateInputOutputBuffers(FrameDescriptor *pFrameDesc)
{
    RC rc;
    MASSERT(pFrameDesc);
    if (!pFrameDesc)
        return RC::SOFTWARE_ERROR;

    // Prepare input buffers
    CHECK_LW_RC(m_DecoderCB.AllocCompressedFrame(m_Decoder, &(pFrameDesc->hBitstream),
        static_cast<UINT32>(pFrameDesc->picParams.CodecSpecific.jpeg.bitstream_size)));

    // Allocate output buffers
    UINT32 surfWidth = 0;
    UINT32 surfHeight = 0;
    CHECK_RC(ComputeOutputFrameDimensions(*pFrameDesc, &surfWidth, &surfHeight));
    CHECK_LW_RC(m_DecoderCB.AllolwncompressedFrame(m_Decoder, &(pFrameDesc->hFrameOut),
                                                   pFrameDesc->fmt, surfWidth, surfHeight, 0));
    return rc;
}

RC LwvidLWJPG::AllocateHostBuffer(FrameDescriptor *pFrameDesc)
{
    RC rc;
    MASSERT(pFrameDesc);
    if (!pFrameDesc)
        return RC::SOFTWARE_ERROR;

    // Allocate Host buffers
    CHECK_RC(ComputeHostBufferSize(pFrameDesc));
    CHECK_RC(AllocHostMemory(pFrameDesc));
    return rc;
}

RC LwvidLWJPG::DecodeMjpegStream(const StreamDescriptor& streamDesc)
{
    RC rc;
    vector<UINT08> mjpegStream;

    // Read Mjpeg Stream
    const string inputFileName = Utility::StrPrintf("%s.mjpg", streamDesc.streamName);
    CHECK_RC(ReadFileFromArchive(inputFileName, &mjpegStream));

    // Set Mjpeg type to A or B based on header
    UINT08* pLwrPos = mjpegStream.data();
    INT32 bytesToDecode = static_cast<INT32>(mjpegStream.size());
    m_Parser.setMjpegtype(pLwrPos, bytesToDecode);

    // Read reference data from the file
    UINT32 offset = 0;
    vector<UINT08> refYuvImage;
    if (m_VerifyUsingRefData)
    {
        string refFileName = Utility::StrPrintf("%s_ref.yuv", streamDesc.streamName);
        CHECK_RC(ReadDataFromFile(refFileName, &refYuvImage));
    }

    const UINT32 loopsPerFrame = (m_LoopsPerFrame != LOOPS_PER_FRAME_UNSET) ? m_LoopsPerFrame:
                                 CEIL_DIV((streamDesc.weight * GetTestConfiguration()->Loops()),
                                 m_TotalWeight);

    for (UINT32 frameCounter = 0;
        bytesToDecode > 0 && frameCounter < streamDesc.maxFramesToDecode;
        frameCounter++)
    {
        UINT32 startOffset = 0; // start of image offset relative to pLwrPos
        UINT32 endOffset = 0; // end of image offset relative to pLwrPos
        LWDECSTILLPicParams picHeaderParams = {};
        UINT32 decodedImageSize = 0;

        m_Parser.FindPictureOffsets(pLwrPos, bytesToDecode, &startOffset, &endOffset);

        // Didn't find any markers to indicate start of image. No more data to decode
        if (endOffset == 0)
        {
            bytesToDecode = 0;
            break;
        }

        pLwrPos += startOffset;
        bytesToDecode -= endOffset;

        // All cores decode same content
        const UINT32 baseLoopHash = GenerateLoopHash(streamDesc.streamId, frameCounter, 0);
        const UINT08 *pRefData = m_VerifyUsingRefData ? refYuvImage.data() + offset : nullptr;

        CHECK_RC(ParseHeaderPicParams(pLwrPos, endOffset - startOffset, &picHeaderParams));

        for (m_pFpCtx->LoopNum = baseLoopHash; m_pFpCtx->LoopNum < baseLoopHash + loopsPerFrame;
             m_pFpCtx->LoopNum++)
        {
            vector<FrameDescriptor> frameDescList;
            const UINT32 loopIdx = m_pFpCtx->LoopNum - baseLoopHash;

            if (m_PerpetualRun && !m_KeepRunning)
            {
                return rc;
            }

            if (m_SkipLoopIndices.find(loopIdx) != m_SkipLoopIndices.end())
            {
                VerbosePrintf("Skipping stream %s frame %u loop hash %u loop index %u\n",
                    streamDesc.streamName, frameCounter, m_pFpCtx->LoopNum, loopIdx);
                continue;
            }

            CHECK_RC(Abort::Check());

            m_pFpCtx->Rand.SeedRandom(m_pFpCtx->LoopNum);
            GetFancyPickerArray()->Restart();

            VerbosePrintf("Stream %s frame %u loop hash %u loop index %u\n", streamDesc.streamName,
                frameCounter, m_pFpCtx->LoopNum, loopIdx);
            m_AtleastOneFrameTested = true;

            DEFER
            {
                for (auto& frameDesc : frameDescList)
                {
                    ReleaseInputOutputBuffers(&frameDesc);
                }
            };

            // Setup control params
            CHECK_RC(SetupControlParams(pLwrPos, endOffset - startOffset,
                                        &frameDescList, picHeaderParams));

            // Decode images
            vector<LWDECSTILLPicParams*> picParamList;
            for (auto& frameDesc : frameDescList)
            {
                frameDesc.loopHash = m_pFpCtx->LoopNum;
                picParamList.push_back(&frameDesc.picParams);
            }

            CHECK_RC(DecodePictures(picParamList));

            // Process decoded output and verify decoded images match with reference image
            for (UINT32 coreIdx = 0; coreIdx < m_NumCoresToTest; coreIdx++)
            {
                const string outFileName = Utility::StrPrintf("%s_F%u_core%u_loop%x_out.raw",
                    streamDesc.streamName, frameCounter, coreIdx, m_pFpCtx->LoopNum);
                FrameDescriptor *pFrameDesc = &(frameDescList[coreIdx]);

                DEFER
                {
                    ReleaseHostBuffer(pFrameDesc);
                };

                CHECK_RC(AllocateHostBuffer(pFrameDesc));
                decodedImageSize = pFrameDesc->hostBufferSize;
                rc = ProcessOutput(pFrameDesc, outFileName, pRefData,
                                   decodedImageSize);

                if (rc != RC::OK)
                {
                    Printf(Tee::PriError,
                        "Verification failed for stream %s on core %u for loop %u\n",
                        streamDesc.streamName, coreIdx, m_pFpCtx->LoopNum);
                    return rc;
                }
            }
        }
        pLwrPos += (endOffset - startOffset);
        offset += decodedImageSize;
    }
    return rc;
}

RC LwvidLWJPG::ParseHeaderPicParams
(
    UINT08 *pJpegStream,
    UINT32 jpegStreamSize,
    LWDECSTILLPicParams *pPicParams
)
{
    RC rc;

    MASSERT(pPicParams);
    MASSERT(pJpegStream);
    if (!pPicParams || !pJpegStream)
        return RC::SOFTWARE_ERROR;

    *pPicParams = {};

    // Parse stream header data
    if (!m_Parser.ParseByteStream(pPicParams, pJpegStream, jpegStreamSize))
    {
        Printf(Tee::PriError, "Jpeg stream header parsing error \n");
        return RC::ILWALID_INPUT;
    }

    // Validate pic params read from the stream
    CHECK_RC(ValidatePicParams(*pPicParams));

    // Populate parameters which don't change across loops
    pPicParams->CodecSpecific.jpeg.bitstream_size = static_cast<UINT32>(jpegStreamSize);
    pPicParams->compressedFormat = LWDECSTILLCodec_JPEG;

    return RC::OK;
}

RC LwvidLWJPG::PickDownscaleFactor
(
    LWDECSTILLPicParams *pPicParams,
    const LWDECSTILLUncompressedFormat& outputFmt
)
{
    MASSERT(pPicParams);
    if (!pPicParams)
        return RC::SOFTWARE_ERROR;

    LWDECSTILLDownScale downscaleFactor = static_cast<LWDECSTILLDownScale>(
        ((*GetFancyPickerArray())[FPK_LWVID_JPG_DOWNSCALE_FACTOR]).Pick());

    const LWDECSTILLUncompressedFormat inputFmt =
        GetColorFmtFromChromaFmt(pPicParams->CodecSpecific.jpeg.ePicChromaFormat);

    // Downscaling cannot be enabled if sub-sampling colwersion is enabled (IAS section 1.2)
    if (m_EnableDownscaling && ((inputFmt == outputFmt) || IsRGBFormat(outputFmt)))
    {
        pPicParams->CodecSpecific.jpeg.power2_downscale = downscaleFactor;
    }

    VerbosePrintf("\t%s\n",
        StringifyDownscalingFactor(pPicParams->CodecSpecific.jpeg.power2_downscale));

    return RC::OK;
}

RC LwvidLWJPG::PickOutputFormat
(
    LWDECSTILLPicParams *pPicParams,
    LWDECSTILLUncompressedFormat *pOutputFmt
)
{
    MASSERT(pPicParams);
    MASSERT(pOutputFmt);

    RC rc;
    string coverageInfo;

    if (!pPicParams || !pOutputFmt)
        return RC::SOFTWARE_ERROR;

    UINT32 numAttempts = 0;
    const LWDECSTILLUncompressedFormat inputFmt =
        GetColorFmtFromChromaFmt(pPicParams->CodecSpecific.jpeg.ePicChromaFormat);

    do
    {
        *pOutputFmt =
            static_cast<LWDECSTILLUncompressedFormat>(((*GetFancyPickerArray())
                [FPK_LWVID_JPG_COLOR_FORMAT]).Pick());
        numAttempts++;
    } while (!IsColorColwersionPossible(inputFmt, *pOutputFmt) &&
             (numAttempts < MAX_OUTPUT_FMT_PICKER_ATTEMPTS));

    DEFER
    {
        if (rc == RC::OK)
        {
            coverageInfo = Utility::StrPrintf("%s\tInput color format %s, Output color format %s",
                               coverageInfo.c_str(), StringifyFormat(inputFmt),
                               StringifyFormat(*pOutputFmt));
            VerbosePrintf("%s\n", coverageInfo.c_str());
        }
    };

    // If color colwersion to picked format is not possible, use input
    // stream color format (no colwersion)
    if (!IsColorColwersionPossible(inputFmt, *pOutputFmt))
    {
        Printf(Tee::PriLow, "Color colwersion not possible. Using output format = input format\n");
        *pOutputFmt = inputFmt;
        return RC::OK;
    }

    if (IsRGBFormat(*pOutputFmt))
    {
        Yuv2RgbType yuv2RgbColwType =
            static_cast<Yuv2RgbType>(
            ((*GetFancyPickerArray())[FPK_LWVID_JPG_YUV_TO_RGB_PARAMS]).Pick());
        UINT32 alpha = static_cast<UINT32>(((*GetFancyPickerArray())[FPK_LWVID_JPG_ALPHA]).Pick());

        // Pick all features before applying overrides based on test args
        if (!m_EnableYuv2Rgb)
        {
            *pOutputFmt = inputFmt;
            return RC::OK;
        }

        pPicParams->CodecSpecific.jpeg.alpha_value = (m_Alpha != ALPHA_UNSET) ? m_Alpha : alpha;
        coverageInfo = Utility::StrPrintf("\tYUV2RGB colwersion type %s alpha %u\n",
            StringifyYuv2RgbType(yuv2RgbColwType), pPicParams->CodecSpecific.jpeg.alpha_value);

        switch (yuv2RgbColwType)
        {
            case Yuv2RgbType::BT_601_RESTRICTED_RANGE:
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[0] = 65536;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[1] = 91881;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[2] = -22554;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[3] = -46802;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[4] = 116130;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[5] = 0;
                break;
            case Yuv2RgbType::BT_601_FULL_RANGE:
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[0] = 76284;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[1] = 104595;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[2] = -25624;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[3] = -53280;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[4] = 132252;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[5] = 16;
                break;
            case Yuv2RgbType::BT_709_FULL_RANGE:
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[0] = 76284;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[1] = 117506;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[2] = -13958;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[3] = -34995;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[4] = 138609;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[5] = 16;
                break;
            case Yuv2RgbType::BT_709_RESTRICTED_RANGE:
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[0] = 65536;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[1] = 100925;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[2] = -11992;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[3] = -30080;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[4] = 119013;
                pPicParams->CodecSpecific.jpeg.yuv2rgb_param[5] = 0;
                break;
            default:
                Printf(Tee::PriError, "Unsupported YUV to RGB color colwersion format %u\n",
                    static_cast<UINT32>(yuv2RgbColwType));
                MASSERT(!"Unsupported format");
                return RC::SOFTWARE_ERROR;
        }
    }
    else if (!m_EnableSubsampleColwersion)
    {
        *pOutputFmt = inputFmt;
    }
    return RC::OK;
}

RC LwvidLWJPG::SetupControlParams
(
    UINT08 *pJpegStream,
    UINT32 jpegStreamSize,
    vector<FrameDescriptor> *pFrameDescList,
    const LWDECSTILLPicParams& picHeaderParams
)
{
    RC rc;

    MASSERT(pFrameDescList);
    MASSERT(pJpegStream);
    if (!pFrameDescList || !pJpegStream)
        return RC::SOFTWARE_ERROR;

    // Populate pic params
    LWDECSTILLPicParams picParams = picHeaderParams;
    auto& jpegParams = picParams.CodecSpecific.jpeg;
    jpegParams.power2_downscale = LWDECSTILLDownScale_NONE; // downscale_factor
    LWDECSTILLUncompressedFormat fmt = LWDECSTILLFmt_DEVICE_RGBA;

    CHECK_RC(PickOutputFormat(&picParams, &fmt));
    CHECK_RC(PickDownscaleFactor(&picParams, fmt));

    MASSERT(pFrameDescList->empty());
    pFrameDescList->resize(m_NumCoresToTest);

    for (UINT32 coreIdx = 0; coreIdx < m_NumCoresToTest; coreIdx++)
    {
        FrameDescriptor* pFrameDesc = &((*pFrameDescList)[coreIdx]);
        pFrameDesc->picParams = picParams;
        pFrameDesc->fmt = fmt;

        // Allocate Input and output buffers
        CHECK_RC(AllocateInputOutputBuffers(pFrameDesc));

        CHECK_RC(CopyBitstreamToDevice(pFrameDesc, pJpegStream, jpegStreamSize));

        // Fill pic params
        pFrameDesc->picParams.hBitstreamIn = pFrameDesc->hBitstream;
        pFrameDesc->picParams.hFrameOut = pFrameDesc->hFrameOut;

        if (m_MultiEngineMode)
        {
            pFrameDesc->lwdaStreamIdx = coreIdx;
        }
    }

    return rc;
}

RC LwvidLWJPG::ProcessOutput
(
    FrameDescriptor *pFrameDesc,
    const string& outFileName,
    const UINT08* pRefData,
    UINT32 refSize
)
{
    RC rc;

    MASSERT(pFrameDesc);
    if (!pFrameDesc)
        return RC::SOFTWARE_ERROR;

    // Copy decoded data to host and get a pointer
    CHECK_RC(GetDecodedData(pFrameDesc));

    if (m_DumpImage)
    {
        // write output to file
        CHECK_RC(DumpImageData(outFileName, pFrameDesc->pDecData, pFrameDesc->hostBufferSize));
    }

    // Generate goldens or compare decoded data matches with goldens
    if (m_UseOnedGoldenSurface)
    {
        CHECK_RC(CheckOrStoreGoldensOneD(*pFrameDesc));
    }
    else
    {
        CHECK_RC(CheckOrStoreGoldens(*pFrameDesc));
    }

    // Verify decoded image against reference image
    if (m_VerifyUsingRefData)
    {
        MASSERT(pRefData);
        if (!pRefData)
            return RC::SOFTWARE_ERROR;
        CHECK_RC(Verify(pFrameDesc->pDecData, pFrameDesc->hostBufferSize, pRefData, refSize));
    }

    return rc;
}

RC LwvidLWJPG::DecodeJpegStream(const StreamDescriptor& streamDesc)
{
    RC rc;
    LWDECSTILLPicParams picHeaderParams = {};
    vector<UINT08> jpegStream;

    // Read Jpeg Stream
    const string inputFileName = Utility::StrPrintf("%s.jpg", streamDesc.streamName);
    CHECK_RC(ReadFileFromArchive(inputFileName, &jpegStream));

    // Parse stream pic params from header
    CHECK_RC(ParseHeaderPicParams(jpegStream.data(),
        static_cast<UINT32>(jpegStream.size()), &picHeaderParams));

    // All cores decode same content
    const UINT32 baseLoopHash = GenerateLoopHash(streamDesc.streamId, 0, 0);
    const UINT32 loopsPerFrame = (m_LoopsPerFrame != LOOPS_PER_FRAME_UNSET) ? m_LoopsPerFrame:
                                 CEIL_DIV((streamDesc.weight * GetTestConfiguration()->Loops()),
                                 m_TotalWeight);

    for (m_pFpCtx->LoopNum = baseLoopHash; m_pFpCtx->LoopNum < baseLoopHash + loopsPerFrame;
         m_pFpCtx->LoopNum++)
    {
        vector<FrameDescriptor> frameDescList;
        const UINT32 loopIdx = m_pFpCtx->LoopNum - baseLoopHash;

        if (m_PerpetualRun && !m_KeepRunning)
        {
            return rc;
        }

        CHECK_RC(Abort::Check());

        if (m_SkipLoopIndices.find(loopIdx) != m_SkipLoopIndices.end())
        {
            VerbosePrintf("Skipping stream %s loop hash %u loop index %u\n",
                streamDesc.streamName, m_pFpCtx->LoopNum, loopIdx);
            continue;
        }

        VerbosePrintf("Stream %s loop hash %u loop index %u\n",
            streamDesc.streamName, m_pFpCtx->LoopNum, loopIdx);
        m_AtleastOneFrameTested = true;

        m_pFpCtx->Rand.SeedRandom(m_pFpCtx->LoopNum);
        GetFancyPickerArray()->Restart();

        DEFER
        {
            for (auto& frameDesc : frameDescList)
            {
                ReleaseInputOutputBuffers(&frameDesc);
            }
        };

        // Setup control params for all cores
        CHECK_RC(SetupControlParams(jpegStream.data(),
            static_cast<UINT32>(jpegStream.size()), &frameDescList, picHeaderParams));

        // Decode pictures
        vector<LWDECSTILLPicParams*> picParamList;
        for (auto& frameDesc : frameDescList)
        {
            frameDesc.loopHash = m_pFpCtx->LoopNum;
            picParamList.push_back(&frameDesc.picParams);
        }
        CHECK_RC(DecodePictures(picParamList));

        // Read reference image
        vector<UINT08> refYuvImage;
        if (m_VerifyUsingRefData)
        {
            string refFileName = Utility::StrPrintf("%s_ref.yuv", streamDesc.streamName);
            CHECK_RC(ReadDataFromFile(refFileName, &refYuvImage));
        }

        // Process decoded output and verify decoded images match with reference image
        for (UINT32 coreIdx = 0; coreIdx < m_NumCoresToTest; coreIdx++)
        {
            const string outFileName = Utility::StrPrintf("%s_F0_core%u_loop_%x_out.raw",
                streamDesc.streamName, coreIdx, m_pFpCtx->LoopNum);
            FrameDescriptor *pFrameDesc = &(frameDescList[coreIdx]);

            DEFER
            {
                ReleaseHostBuffer(pFrameDesc);
            };

            CHECK_RC(AllocateHostBuffer(pFrameDesc));

            rc = ProcessOutput(pFrameDesc, outFileName,
                     refYuvImage.data(), static_cast<UINT32>(refYuvImage.size()));

            if (rc != RC::OK)
            {
                Printf(Tee::PriError, "Verification failed for stream %s on core %u\n",
                    streamDesc.streamName, coreIdx);
                return rc;
            }
        }
    }
    return rc;
}

RC LwvidLWJPG::ValidatePicParams(const LWDECSTILLPicParams& picParams) const
{
    if (picParams.picWidth > m_DecoderCaps.maxWidth)
    {
        Printf(Tee::PriError,
            "Pic width = %u > max width (%u) allowed\n",
            picParams.picWidth, m_DecoderCaps.maxWidth);
        return RC::ILWALID_INPUT;
    }

    if (picParams.picHeight > m_DecoderCaps.maxHeight)
    {
        Printf(Tee::PriError,
            "Pic height = %u > max height (%u) allowed\n",
            picParams.picHeight, m_DecoderCaps.maxHeight);
        return RC::ILWALID_INPUT;
    }

    if (picParams.picWidth < m_DecoderCaps.minWidth)
    {
        Printf(Tee::PriError,
            "Pic width = %u < min width (%u) allowed\n",
            picParams.picWidth, m_DecoderCaps.minWidth);
        return RC::ILWALID_INPUT;
    }

    if (picParams.picHeight < m_DecoderCaps.minHeight)
    {
        Printf(Tee::PriError,
            "Pic height = %u < min height (%u) allowed\n",
            picParams.picHeight, m_DecoderCaps.minHeight);
        return RC::ILWALID_INPUT;
    }

    return RC::OK;
}

RC LwvidLWJPG::DecodeStream(const StreamDescriptor& streamDesc)
{
    if (streamDesc.codec ==  Codec::JPEG)
    {
        return DecodeJpegStream(streamDesc);
    }
    else if (streamDesc.codec == Codec::MJPEG)
    {
        return DecodeMjpegStream(streamDesc);
    }
    else
    {
        return RC::ILWALID_INPUT;
    }
}

RC LwvidLWJPG::Setup()
{
    RC rc;

#if defined(DIRECTAMODEL)
    DirectAmodel::AmodelAttribute directAmodelKnobs[] = {
        { DirectAmodel::IDA_KNOB_STRING, 0 },
        { DirectAmodel::IDA_KNOB_STRING, reinterpret_cast<LwU64>(
            "ChipFE::argsForPlugInClassSim -im 0x8000 -dm 0x8000 -ipm 0x500 -smax 0x8000 -cv 6 -sdk LwjpgPlugin LwjpgPlugin/lwjpgFALCONOS ") }
    };
    if (GetBoundGpuDevice()->GetFamily() == GpuDevice::Ampere)
    {
        directAmodelKnobs[0].data = reinterpret_cast<LwU64>(
            "ChipFE::loadPlugInClassSim 1 ( {0xC4D1} LWJPG LwjpgPlugin/pluginLwjpg.dll )");
    }
    else if (GetBoundGpuDevice()->GetFamily() == GpuDevice::Hopper)
    {
        directAmodelKnobs[0].data = reinterpret_cast<LwU64>(
            "ChipFE::loadPlugInClassSim 1 ( {0xB8D1} LWJPG LwjpgPlugin/pluginLwjpg.dll )");
    }
    else // Ada:
    {
        directAmodelKnobs[0].data = reinterpret_cast<LwU64>(
            "ChipFE::loadPlugInClassSim 1 ( {0xC9D1} LWJPG LwjpgPlugin/pluginLwjpg.dll )");
    }
    dglDirectAmodelSetAttributes(directAmodelKnobs, static_cast<LwU32>(NUMELEMS(directAmodelKnobs)));
#endif

    CHECK_RC(LwdaStreamTest::Setup());

    CHECK_LW_RC(lwCtxPushLwrrent(GetLwdaInstance().GetHandle(GetBoundLwdaDevice())));
    CHECK_LW_RC(InitLwdecStillImageInterface(&m_DecoderCB));

    CHECK_LW_RC(m_DecoderCB.CreateDecoder(&m_Decoder, LWDECSTILLCodec_JPEG,
        nullptr, 0, GetLwdaInstance().GetHandle(GetBoundLwdaDevice())));

    // Get decoder capabilities
    m_DecoderCaps.eCompressedFormat = LWDECSTILLCodec_JPEG;
    m_DecoderCaps.eChromaFormat = LWDECSTILLChromaFormat_420; // We don't use fields dependent on that value
    CHECK_LW_RC(m_DecoderCB.GetDecoderCapsV2(&m_DecoderCaps));
    CHECK_RC(UpdatePickers());

    m_MultiEngineMode = m_DecoderCaps.batchSizeGranularity == 1;
    UINT32 numLwdaStreams = 1;
    if (m_MultiEngineMode)
    {
        if (m_NumCoresToTest != NUM_CORES_TO_TEST_UNSET)
        {
            Printf(Tee::PriError, "NumCoresToTest can't be set in the Multi Engine Mode\n");
            return RC::ILWALID_ARGUMENT;
        }
        vector<UINT32> engines;
        CHECK_RC(GetBoundGpuDevice()->GetEngines(&engines));

        UINT32 presentEngineMask = 0;
        for (auto engine : engines)
        {
            if (LW2080_ENGINE_TYPE_IS_LWJPEG(engine))
            {
                presentEngineMask |= 1 << LW2080_ENGINE_TYPE_LWJPEG_IDX(engine);
            }
        }

        // Colwert the "physical" engine mask into "continuous" mask that the driver
        // uses internally and is required by DecodePictureMultiple(..., engNumAndNumPics, ...)
        // http://lwbugs/3340241/22
        m_RunEngineMask = 0;
        UINT32 continuousIdx = 0;
        for (UINT32 engineIdx = 0; engineIdx <  LW2080_ENGINE_TYPE_LWJPEG_SIZE; engineIdx++)
        {
            if ( presentEngineMask & (1 << engineIdx) )
            {
                if ( (m_SkipEngineMask & (1 << engineIdx)) == 0 )
                {
                    m_RunEngineMask |= 1 << continuousIdx;
                }
                continuousIdx++;
            }
        }

        if (m_RunEngineMask == 0)
        {
            Printf(Tee::PriError,
                "Found no engines to test with the skip mask = 0x%08x!\n",
                m_SkipEngineMask);
            return RC::DEVICE_NOT_FOUND;
        }

        if (GetGoldelwalues()->GetAction() == Goldelwalues::Store)
        {
            // Only generate goldens on the first core
            m_NumCoresToTest = 1;
        }
        else
        {
            const UINT32 numEngines = Utility::CountBits(m_RunEngineMask);
            MASSERT(numEngines <= LW2080_ENGINE_TYPE_LWJPEG_SIZE);
            m_NumCoresToTest = numEngines;
            numLwdaStreams = m_NumCoresToTest;
        }
    }
    else
    {
        if (m_SkipEngineMask != 0)
        {
            Printf(Tee::PriError, "SkipEngineMask can't be set in the non Multi Engine Mode\n");
            return RC::ILWALID_ARGUMENT;
        }
        const UINT08 supportedNumCores = m_DecoderCaps.batchSizeGranularity;
        if (m_NumCoresToTest == NUM_CORES_TO_TEST_UNSET)
        {
            // Only generate goldens on the first core
            if (GetGoldelwalues()->GetAction() == Goldelwalues::Store)
            {
                m_NumCoresToTest = 1;
            }
            else
            {
                m_NumCoresToTest = supportedNumCores;
            }
        }

        if (m_NumCoresToTest > supportedNumCores)
        {
            Printf(Tee::PriError, "NumCoresToTest = %u > Number of cores present in HW (%u)\n",
                m_NumCoresToTest, supportedNumCores);
            return RC::ILWALID_ARGUMENT;
        }
    }

    m_LwdaStreamResources.resize(numLwdaStreams);
    for (UINT32 lwdaStreamIdx = 0; lwdaStreamIdx < numLwdaStreams; lwdaStreamIdx++)
    {
        LwdaStreamResources& csr = m_LwdaStreamResources[lwdaStreamIdx];
        csr.m_LwdaStreamWrapper = GetLwdaInstance().CreateStream(GetBoundLwdaDevice());
        CHECK_RC(csr.m_LwdaStreamWrapper.InitCheck());
        csr.m_LwdaStream = csr.m_LwdaStreamWrapper.GetHandle();
        CHECK_LW_RC(m_DecoderCB.AllocEvent(m_Decoder, &csr.m_Event));
    }

    CHECK_RC_MSG(Utility::ParseIndices(m_SkipLoopsInStream, &m_SkipLoopIndices),
        Utility::StrPrintf("Invalid SkipLoopsInStream testarg: %s\n",
        m_SkipLoopsInStream.c_str()).c_str());

    m_OutputSurfaces.resize(MAX_PLANES);
    for (auto& surf: m_OutputSurfaces)
    {
        auto surfWriter = make_unique<SurfaceUtils::MappingSurfaceWriter>(surf);
        // The surfaces are always in the system memory - no need to use a small window
        surfWriter->SetMaxMappedChunkSize(0x80000000);
        m_OutputSurfWriters.push_back(std::move(surfWriter));
    }

    m_pGoldenSurfaces = make_unique<GpuGoldenSurfaces>(GetBoundGpuDevice());

    if (m_UseOnedGoldenSurface)
    {
        CHECK_RC(m_OnedGoldenSurfaces.SetSurface("decodedstream", nullptr, 0));
        CHECK_RC(GetGoldelwalues()->SetSurfaces(&m_OnedGoldenSurfaces));
    }
    else
    {
        // Setup fake surfaces based on max requirements. Create 3 surfaces since Y, U and V
        // can be in separate planes. RGBA represents 4 elements/CRCs per bin so use it
        // m_pGoldenSurfaces will be replaced by output surfaces in the test
        Surface2D fakeSurf;
        fakeSurf.SetWidth(1);
        fakeSurf.SetHeight(1);
        fakeSurf.SetColorFormat(ColorUtils::R8G8B8A8);
        m_pGoldenSurfaces->AttachSurface(&fakeSurf, "RGBA or Y", 0);
        m_pGoldenSurfaces->AttachSurface(&fakeSurf, "U", 0);
        m_pGoldenSurfaces->AttachSurface(&fakeSurf, "V", 0);
        CHECK_RC(GetGoldelwalues()->SetSurfaces(m_pGoldenSurfaces.get()));
    }
    m_TotalWeight = 0;
    for (const auto& streamDesc : m_StreamDescriptors)
    {
        m_TotalWeight += streamDesc.weight;
    }

    if (!m_JpgTarArchive.isArchiveRead)
    {
        // Load the tarball that contains all the input images into memory
        const string tarballFilePath = Utility::DefaultFindFile(LWJPG_TRACES_BIN, true);
        if (!m_JpgTarArchive.tarArchive.ReadFromFile(tarballFilePath))
        {
            Printf(Tee::PriError, "%s does not exist\n", LWJPG_TRACES_BIN);
            return RC::FILE_DOES_NOT_EXIST;
        }
        m_JpgTarArchive.isArchiveRead = true;
    }

    return rc;
}

RC LwvidLWJPG::VerifyInjectedErrorDetection(const RC& rc)
{
    // Error injection is not done while generating goldens (i.e) it's done
    // only for gputest not gpugen
    if (Goldelwalues::Check != GetGoldelwalues()->GetAction())
    {
        return rc;
    }

    if (rc == RC::OK)
    {
        Printf(Tee::PriError, "Test failed to catch an injected error\n");
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    else if (rc == RC::GOLDEN_VALUE_MISCOMPARE || rc == RC::GOLDEN_VALUE_MISCOMPARE_Z)
    {
        Printf(Tee::PriLow, "Test detected an injected error\n");
        return RC::OK;
    }
    return rc;
}

RC LwvidLWJPG::Run()
{
    RC rc;
    m_PerpetualRun = m_KeepRunning;

    do
    {
        for (auto& streamDesc : m_StreamDescriptors)
        {
            if (m_PerpetualRun && !m_KeepRunning)
            {
                break;
            }
            CHECK_RC(Abort::Check());

            if (m_StreamSkipMask & (1ULL << streamDesc.streamId))
            {
                VerbosePrintf("Skipping %s stream\n", streamDesc.streamName);
                continue;
            }

            VerbosePrintf("Decoding %s stream\n", streamDesc.streamName);
            rc = DecodeStream(streamDesc);

            if (rc != RC::OK)
            {
                Printf(Tee::PriError, "Decoding failed for stream %s\n",
                                      streamDesc.streamName);
                return rc;
            }
        }
    } while (m_KeepRunning);

    // Depending on output fmt (RGB/YUV semi-planar/planar), 1, 2 or 3 surfaces are needed.In test,
    // we adjust surface count based on output format for current loop. However, in ErrorRateTest,
    // when it reports the aclwmulated errors, it retrieves surface parameters such as name, etc.
    // from m_pGoldenSurfaces. So if the last loop was RGB (only 1 surface), this would fail. Hence
    // need to readjust it for the max case. Note that it's still tricky to report the color format
    // TODO Instead of variable surfaces, it may be better to compute CRCs on color
    // agnostic surfaces and then use offband mechanisms to dump png correctly
    Surface2D fakeSurf;
    fakeSurf.SetWidth(1);
    fakeSurf.SetHeight(1);
    fakeSurf.SetColorFormat(ColorUtils::R8G8B8A8);
    m_pGoldenSurfaces->ClearSurfaces();
    m_pGoldenSurfaces->AttachSurface(&fakeSurf, "RGBA or Y", 0);
    m_pGoldenSurfaces->AttachSurface(&fakeSurf, "U", 0);
    m_pGoldenSurfaces->AttachSurface(&fakeSurf, "V", 0);

    // Golden errors that are deferred due to "-run_on_error" can only be
    // retrieved by running Golden::ErrorRateTest().  This must be done
    // before clearing surfaces

    if (!m_AtleastOneFrameTested)
        return RC::NO_TESTS_RUN;

    RC ertRc = GetGoldelwalues()->ErrorRateTest(GetJSObject());
    if (m_InjectErrors && !GetGoldelwalues()->GetStopOnError())
    {
        CHECK_RC(VerifyInjectedErrorDetection(ertRc));
    }
    else
    {
        CHECK_RC(ertRc);
    }
    return rc;
}

RC LwvidLWJPG::Cleanup()
{
    StickyRC rc;
    LWcontext primaryCtx = GetLwdaInstance().GetHandle(GetBoundLwdaDevice());

    for (const auto& csr : m_LwdaStreamResources)
    {
        if (csr.m_Event)
        {
            rc = Lwca::LwdaErrorToRC(m_DecoderCB.FreeEvent(m_Decoder, csr.m_Event));
        }
    }
    m_LwdaStreamResources.clear();

    m_pGoldenSurfaces.reset();

    if (GetGoldelwalues())
        GetGoldelwalues()->ClearSurfaces();

    for (auto& surfWriter : m_OutputSurfWriters)
    {
        surfWriter->Unmap();
    }

    for (auto& surf : m_OutputSurfaces)
    {
        surf.Free();
    }

    // Destroy decoder
    rc = Lwca::LwdaErrorToRC(m_DecoderCB.DestroyDecoder(m_Decoder));
    rc = Lwca::LwdaErrorToRC(lwCtxPopLwrrent(&primaryCtx));

    rc = LwdaStreamTest::Cleanup();

    return rc;
}

void LwvidLWJPG::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "LwvidLWJPG test Js Properties:\n");
    Printf(pri, "\tUseLwda:\t\t\t%s\n", m_UseLwda ? "true" : "false");
    Printf(pri, "\tUseEvents:\t\t\t%s\n", m_UseEvents ? "true" : "false");
    Printf(pri, "\tDumpImage:\t\t\t%s\n", m_DumpImage ? "true" : "false");
    Printf(pri, "\tVerifyUsingRefData:\t\t%s\n", m_VerifyUsingRefData ? "true" : "false");
    Printf(pri, "\tNumCoresToTest:\t\t\t%u\n", m_NumCoresToTest);
    Printf(pri, "\tSkipEngineMask:\t\t\t0x%0x\n", m_SkipEngineMask);
    Printf(pri, "\tStreamSkipMask:\t\t\t0x%llx\n", m_StreamSkipMask);
    Printf(pri, "\tLoopsPerFrame:\t\t\t%u\n", m_LoopsPerFrame);
    Printf(pri, "\tEnableYuv2Rgb:\t\t\t%s\n", m_EnableYuv2Rgb ? "true": "false");
    Printf(pri, "\tEnableSubsampleColwersion:\t%s\n",
        m_EnableSubsampleColwersion ? "true": "false");
    Printf(pri, "\tAlpha:\t\t\t\t%u\n", m_Alpha);
    Printf(pri, "\tInjectErrors:\t\t\t%s\n", m_InjectErrors ? "true" : "false");
    Printf(pri, "\tKeepRunning:\t\t\t%s\n", m_KeepRunning ? "true" : "false");
    Printf(pri, "\tEnableDownscaling:\t\t%s\n", m_EnableDownscaling ? "true" : "false");
    Printf(pri, "\tUseOnedGoldenSurface:\t\t%s\n", m_UseOnedGoldenSurface ? "true" : "false");
    Printf(pri, "\tSkipLoopsInStream:\t\t%s\n", m_SkipLoopsInStream.empty() ? "None" :
        m_SkipLoopsInStream.c_str());
}

bool LwvidLWJPG::IsSupported()
{
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(Tee::PriLow, "LwvidLWJPG is not supported on SOC devices\n");
        return false;
    }

    LwjpgAlloc lwjpgAlloc;
    const bool supported = lwjpgAlloc.IsSupported(GetBoundGpuDevice());
    if (!supported)
    {
        Printf(Tee::PriLow, "LwvidLWJPG has not found supported LWJPG hw class\n");
    }
    return supported;
}

//--------------------------------------------------------------------
// JavaScript interface
//--------------------------------------------------------------------
JS_CLASS_INHERIT(LwvidLWJPG, GpuTest, "Lwvid library based LWJPG test");
CLASS_PROP_READWRITE(LwvidLWJPG, UseLwda, bool,
    "Use lwca for memory allocation and movement of data");
CLASS_PROP_READWRITE(LwvidLWJPG, UseEvents, bool, "Use LwdecStill events for synchronization");
CLASS_PROP_READWRITE(LwvidLWJPG, DumpImage, bool, "Dump Raw YUV output to file");
CLASS_PROP_READWRITE(LwvidLWJPG, VerifyUsingRefData, bool,
    "Verify decoded data against reference image");
CLASS_PROP_READWRITE(LwvidLWJPG, NumCoresToTest, UINT32, "Number of Jpeg cores to test");
CLASS_PROP_READWRITE(LwvidLWJPG, SkipEngineMask, UINT32, "Mask of engines to skip");
CLASS_PROP_READWRITE(LwvidLWJPG, StreamSkipMask, UINT64, "Bitmask of streams to skip. Use 1 to "
    "skip 1st stream, 2 to skip 2nd stream and 3 to skip first 2 streams etc.");
CLASS_PROP_READWRITE(LwvidLWJPG, LoopsPerFrame, UINT32, "Number of loops every frame is decoded"
    "Each loop picks different parameters (downscaling, output format etc.)");
CLASS_PROP_READWRITE(LwvidLWJPG, EnableYuv2Rgb, bool, "Enables YUV to RGB colwersion");
CLASS_PROP_READWRITE(LwvidLWJPG, EnableSubsampleColwersion, bool,
    "Enables YUV sub-sample colwersion");
CLASS_PROP_READWRITE(LwvidLWJPG, Alpha, UINT32, "Alpha value to use in RGB formats");
CLASS_PROP_READWRITE(LwvidLWJPG, InjectErrors, bool,
    "Debug feature to inject errors and verify test fails");
CLASS_PROP_READWRITE(LwvidLWJPG, KeepRunning, bool, "Keep the test running in the background");
CLASS_PROP_READWRITE(LwvidLWJPG, EnableDownscaling, bool, "Enable Downscaling");
CLASS_PROP_READWRITE(LwvidLWJPG, UseOnedGoldenSurface, bool, "Use OneD Golden surfaces");
CLASS_PROP_READWRITE(LwvidLWJPG, SkipLoopsInStream, string,
    "Indices of the loops to skip in the stream");
