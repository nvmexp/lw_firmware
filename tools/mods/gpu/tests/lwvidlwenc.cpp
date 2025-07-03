/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/abort.h"
#include "core/include/fileholder.h"
#include "core/utility/ptrnclss.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/onedgoldensurfaces.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/tests/lwdastst.h"
#include "codecs/lwca/lwlwvid/encoder/inc/ILwEncodeInterface.h"

#if defined(DIRECTAMODEL)
#include "opengl/modsgl.h"
#include "common/amodel/IDirectAmodelCommon.h"
#endif

#ifdef _WIN32
// Unfortunately indirectly included winnt.h has a define for that:
#undef BitScanForward
#endif

class LwvidLWENC : public LwdaStreamTest
{
public:
    LwvidLWENC();

    bool IsSupported() override;

    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;

    void PrintJsProperties(Tee::Priority pri) override;

    RC SetDefaultsForPicker(UINT32 idx) override;

    SETGET_PROP(DisableReverseFrames, bool);
    SETGET_PROP(PrintOutputBitstreams, bool);
    SETGET_PROP(SaveInputSurfaces, bool);
    SETGET_PROP(SaveStreams, bool);
    SETGET_PROP(SkipLoops, string);
    SETGET_PROP(WidthMin, UINT32);
    SETGET_PROP(HeightMin, UINT32);
    SETGET_PROP(WidthMax, UINT32);
    SETGET_PROP(HeightMax, UINT32);
    SETGET_PROP(SizeMax, UINT32);
    SETGET_PROP(ForceBufferFormat, UINT32);
    SETGET_PROP(KeepRunning, bool);

private:
    // We need to remember if test was started with KeepRunning set to true in
    // order to know whether to end the test early once KeepRunning has been
    // switched to false by another test
    bool m_PerpetualRun = false;

    bool m_DisableReverseFrames = false; // Disable running frames in reverse with gpugen.js
    bool m_PrintOutputBitstreams = false;
    bool m_SaveInputSurfaces = false;
    bool m_SaveStreams = false;
    bool m_KeepRunning = false;
    string m_SkipLoops;
    set<UINT32> m_SkipLoopsIndices;
    UINT32 m_WidthMin = 0;
    UINT32 m_HeightMin = 0;
    UINT32 m_WidthMax = _UINT32_MAX;
    UINT32 m_HeightMax = _UINT32_MAX;
    UINT32 m_SizeMax = 1'048'576;
    UINT32 m_ForceBufferFormat = LW_ENC_BUFFER_FORMAT_UNDEFINED;

    GpuTestConfiguration *m_pTestConfig = GetTestConfiguration();
    ILwEncoderInterface *m_pEncoderInterface = nullptr;
    OnedGoldenSurfaces m_OnedGoldenSurfaces;

    PatternClass m_PatternClass;

    enum EncCodec
    {
        ENC_CODEC_H264,
        ENC_CODEC_HEVC,
        ENC_CODEC_AV1,
        NUM_ENC_CODECS
    };
    static constexpr UINT32 EncCodec2Bitmask(EncCodec encCodec) { return 1 << encCodec; }
    UINT32 m_AvailableEncCodecs = 0;

    struct CodecCaps
    {
        UINT32 widthMin;
        UINT32 heightMin;
        UINT32 widthMax;
        UINT32 heightMax;
        vector<GUID> presets;
        vector<LW_ENC_BUFFER_FORMAT> bufferFormats;
        bool isMultipleRefFramesSupported = false;
    };
    CodecCaps m_CodecCaps[NUM_ENC_CODECS];

    struct EncodeSession
    {
        UINT32 streamLoopIdx;
        EncCodec encCodec;
        UINT32 numFrames;
        LW_ENC_INITIALIZE_PARAMS initializeParams;
        LW_ENC_CONFIG encConfig;
        LW_ENC_CREATE_INPUT_BUFFER createInputBuffer;
        LW_ENC_LOCK_INPUT_BUFFER lockInputBuffer;
        LW_ENC_CREATE_BITSTREAM_BUFFER createBitstreamBuffer;
        LW_ENC_PIC_PARAMS encPicParams;
        FileHolder bitstreamFile;
    };

    RC Enc2RC(LWENCSTATUS lwencstatus) const;

    RC DetermineAvailableCodecs();
    RC GetCodecCaps();
    static RC CodecIdToGuid(EncCodec codec, GUID *guid);
    static bool Is444Format(LW_ENC_BUFFER_FORMAT format);
    static UINT32 PixelBitDepthMinus8(LW_ENC_BUFFER_FORMAT format);
    RC UpdatePickers();

    RC PickConfiguration(EncodeSession *pES);
    RC PickConfigurationPreset(EncodeSession * pES);
    RC PickConfigurationTuningInfo(EncodeSession * pES);
    RC PickConfigurationSize(EncodeSession *pES);
    static bool IsLoseLessPreset(const GUID &presetGUID);
    static bool IsPXPreset(const GUID &presetGUID);
    static bool Is444FormatRequired(EncodeSession *pES);
    RC PickBufferFormat(EncodeSession *pES);
    RC AllocateResources(EncodeSession *pES) const;
    RC FreeResources(EncodeSession *pES) const;
    static RC PrepareFrameParameters(EncodeSession *pES, UINT32 frameIdx);
    RC PrepareInputContent
    (
        EncodeSession *pES,
        UINT32 frameIdx,
        UINT32 inputSize,
        UINT32 pitch
    );
    RC PrepareInputContentActionMove
    (
        EncodeSession *pES,
        UINT32 inputSize,
        UINT32 pitch
    );
    RC PrepareInputSurface(EncodeSession *pES, UINT32 frameIdx);
    RC ProcessOutputBitstream(EncodeSession *pES, UINT32 frameIdx);
    RC RunConfiguration(EncodeSession *pES);
};

LwvidLWENC::LwvidLWENC()
{
    SetName("LwvidLWENC");
    m_PatternClass.DisableRandomPattern();
    CreateFancyPickerArray(FPK_LWVIDLWENC_NUM_PICKERS);
}

RC LwvidLWENC::Setup()
{
    RC rc;

#if defined(DIRECTAMODEL)
    DirectAmodel::AmodelAttribute directAmodelKnobs[] = {
        { DirectAmodel::IDA_KNOB_STRING, 0 },
        { DirectAmodel::IDA_KNOB_STRING, reinterpret_cast<LwU64>(
            "ChipFE::argsForPlugInClassSim -im 0x8000 -dm 0x8000 -ipm 0x800 -smax 0x8000 -cv 6 -sdk LwencPlugin LwencPlugin/lwencFALCONOS ") }
    };
    if (GetBoundGpuDevice()->GetFamily() == GpuDevice::Ampere)
    {
        directAmodelKnobs[0].data = reinterpret_cast<LwU64>(
            "ChipFE::loadPlugInClassSim 1 ({ 0xC7B7 } LWENC LwencPlugin/pluginLwenc.dll )");
    }
    else if (GetBoundGpuDevice()->GetFamily() == GpuDevice::Hopper)
    {
        return RC::SOFTWARE_ERROR;
    }
    else // Ada:
    {
        directAmodelKnobs[0].data = reinterpret_cast<LwU64>(
            "ChipFE::loadPlugInClassSim 1 ({ 0xC9B7 } LWENC LwencPlugin/pluginLwenc.dll )");
    }
    dglDirectAmodelSetAttributes(directAmodelKnobs, static_cast<LwU32>(NUMELEMS(directAmodelKnobs)));
#endif

    CHECK_RC(LwdaStreamTest::Setup());

    CHECK_RC_MSG(Utility::ParseIndices(m_SkipLoops, &m_SkipLoopsIndices),
        Utility::StrPrintf("Invalid SkipLoops testarg: %s\n",
            m_SkipLoops.c_str()).c_str());

    const LWcontext primaryCtx = GetLwdaInstance().GetHandle(GetBoundLwdaDevice());
    CHECK_LW_RC(lwCtxPushLwrrent(primaryCtx));

    CHECK_RC_MSG(Enc2RC(CreateEncoderInterface(&m_pEncoderInterface)),
        "Failed to create encoder interface!\n");

    CHECK_RC_MSG(Enc2RC(m_pEncoderInterface->InitializeEncoderInterface(primaryCtx, false)),
        "Failed to initialize encoder interface!\n");

    CHECK_RC(DetermineAvailableCodecs());
    CHECK_RC(GetCodecCaps());
    CHECK_RC(UpdatePickers());

     // Dummy values to select proper number of surfaces with goldens:
    CHECK_RC(m_OnedGoldenSurfaces.SetSurface("bitstream", nullptr, 0));
    CHECK_RC(GetGoldelwalues()->SetSurfaces(&m_OnedGoldenSurfaces));
    GetGoldelwalues()->SetCheckCRCsUnique(true);

    return rc;
}

RC LwvidLWENC::DetermineAvailableCodecs()
{
    RC rc;

    m_AvailableEncCodecs = 0;

    uint32_t encodeGUIDCount = 0;
    CHECK_RC(Enc2RC(m_pEncoderInterface->GetEncodeGUIDCount(&encodeGUIDCount)));
    if (encodeGUIDCount == 0)
    {
        Printf(Tee::PriError, "Zero encoder GUIDs.\n");
        return RC::SOFTWARE_ERROR;
    }
    vector<GUID> encodeGUIDs(encodeGUIDCount);

    uint32_t returnedGUIDCount = 0;
    CHECK_RC(Enc2RC(m_pEncoderInterface->GetEncodeGUIDs(encodeGUIDs.data(),
        encodeGUIDCount, &returnedGUIDCount)));
    if (returnedGUIDCount != encodeGUIDCount)
    {
        Printf(Tee::PriError, "Unexpected number of encoder GUIDs.\n");
        return RC::SOFTWARE_ERROR;
    }

    for (auto &guid : encodeGUIDs)
    {
        if (memcmp(&guid, &LW_ENC_CODEC_H264_GUID, sizeof(guid)) == 0)
        {
            m_AvailableEncCodecs |= EncCodec2Bitmask(ENC_CODEC_H264);
        }
        else if (memcmp(&guid, &LW_ENC_CODEC_HEVC_GUID, sizeof(guid)) == 0)
        {
            m_AvailableEncCodecs |= EncCodec2Bitmask(ENC_CODEC_HEVC);
        }
        else if (memcmp(&guid, &LW_ENC_CODEC_AV1_GUID, sizeof(guid)) == 0)
        {
            m_AvailableEncCodecs |= EncCodec2Bitmask(ENC_CODEC_AV1);
        }
    }

    if (static_cast<INT32>(encodeGUIDs.size()) != Utility::CountBits(m_AvailableEncCodecs))
    {
        Printf(Tee::PriError, "Detected unsupported codecs.\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_AvailableEncCodecs & EncCodec2Bitmask(ENC_CODEC_AV1))
    {
        Printf(Tee::PriWarn, "Skipping unimplemented AV1 codec.\n");
        m_AvailableEncCodecs ^= EncCodec2Bitmask(ENC_CODEC_AV1);
    }

    return rc;
}

RC LwvidLWENC::GetCodecCaps()
{
    RC rc;

    UINT32 availableCodecs = m_AvailableEncCodecs;

    while (availableCodecs)
    {
        const EncCodec encCodec = static_cast<EncCodec>(
            Utility::BitScanForward(availableCodecs));
        availableCodecs ^= EncCodec2Bitmask(encCodec);

        GUID guid;
        CHECK_RC(CodecIdToGuid(encCodec, &guid));

        auto &codecCaps = m_CodecCaps[encCodec];

        CHECK_RC(Enc2RC(m_pEncoderInterface->GetEncoderCaps(guid, LW_ENC_CAPS_WIDTH_MIN,
            reinterpret_cast<int32_t*>(&codecCaps.widthMin), 0, 0)));
        CHECK_RC(Enc2RC(m_pEncoderInterface->GetEncoderCaps(guid, LW_ENC_CAPS_HEIGHT_MIN,
            reinterpret_cast<int32_t*>(&codecCaps.heightMin), 0, 0)));
        CHECK_RC(Enc2RC(m_pEncoderInterface->GetEncoderCaps(guid, LW_ENC_CAPS_WIDTH_MAX,
            reinterpret_cast<int32_t*>(&codecCaps.widthMax), 0, 0)));
        CHECK_RC(Enc2RC(m_pEncoderInterface->GetEncoderCaps(guid, LW_ENC_CAPS_HEIGHT_MAX,
            reinterpret_cast<int32_t*>(&codecCaps.heightMax), 0, 0)));

        {
            UINT32 multiRefFramesSupported = 0;
            CHECK_RC(Enc2RC(m_pEncoderInterface->GetEncoderCaps(guid,
                LW_ENC_CAPS_SUPPORT_MULTIPLE_REF_FRAMES,
                reinterpret_cast<int32_t*>(&multiRefFramesSupported), 0, 0)));
            codecCaps.isMultipleRefFramesSupported = !(!multiRefFramesSupported);
        }

        UINT32 inputFmtCount = 0;
        CHECK_RC(Enc2RC(m_pEncoderInterface->GetInputFormatCount(guid, &inputFmtCount)));
        if (inputFmtCount == 0)
        {
            Printf(Tee::PriError, "Zero input formats.\n");
            return RC::SOFTWARE_ERROR;
        }
        codecCaps.bufferFormats.resize(inputFmtCount);
        UINT32 returnedInputFmtCount = 0;
        CHECK_RC(Enc2RC(m_pEncoderInterface->GetInputFormats(guid,
            codecCaps.bufferFormats.data(), inputFmtCount, &returnedInputFmtCount)));
        if (returnedInputFmtCount != inputFmtCount)
        {
            Printf(Tee::PriError, "Unexpected number of input formats.\n");
            return RC::SOFTWARE_ERROR;
        }

        for (auto &format : codecCaps.bufferFormats)
        {
            // Tiled formats are not supported since Pascal,
            // but some part of LWVID still returns it.
            // Hopefully it will be fixed in library in the future.
            switch (format)
            {
                case LW_ENC_BUFFER_FORMAT_YUV420_10BIT_TILED16x16:
                    format = LW_ENC_BUFFER_FORMAT_YUV420_10BIT;
                    break;
                case LW_ENC_BUFFER_FORMAT_YUV444_TILED16x16:
                    format = LW_ENC_BUFFER_FORMAT_YUV444;
                    break;
                case LW_ENC_BUFFER_FORMAT_YUV444_10BIT_TILED16x16:
                    format = LW_ENC_BUFFER_FORMAT_YUV444_10BIT;
                    break;
                default:
                    break;

            }
        }

        UINT32 presetCount = 0;
        CHECK_RC(Enc2RC(m_pEncoderInterface->GetEncodePresetCount(guid, &presetCount)));
        if (presetCount == 0)
        {
            Printf(Tee::PriError, "Zero codec presets.\n");
            return RC::SOFTWARE_ERROR;
        }
        codecCaps.presets.resize(presetCount);
        UINT32 returnedPresetCount = 0;
        CHECK_RC(Enc2RC(m_pEncoderInterface->GetEncodePresetGUIDs(guid,
            codecCaps.presets.data(), presetCount, &returnedPresetCount)));
        if (returnedPresetCount != presetCount)
        {
            Printf(Tee::PriError, "Unexpected number of codec presets.\n");
            return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

RC LwvidLWENC::CodecIdToGuid(EncCodec codec, GUID *guid)
{
    switch (codec)
    {
        case ENC_CODEC_H264:
            *guid = LW_ENC_CODEC_H264_GUID;
            break;
        case ENC_CODEC_HEVC:
            *guid = LW_ENC_CODEC_HEVC_GUID;
            break;
        default:
            MASSERT(!"Unexpected codec");
            return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

bool LwvidLWENC::Is444Format(LW_ENC_BUFFER_FORMAT format)
{
    switch (format)
    {
        case LW_ENC_BUFFER_FORMAT_YUV444:
        case LW_ENC_BUFFER_FORMAT_YUV444_TILED16x16:
        case LW_ENC_BUFFER_FORMAT_YUV444_10BIT:
        case LW_ENC_BUFFER_FORMAT_YUV444_10BIT_TILED16x16:
            return true;
        default:
            return false;
    }
}

UINT32 LwvidLWENC::PixelBitDepthMinus8(LW_ENC_BUFFER_FORMAT format)
{
    switch (format)
    {
        case LW_ENC_BUFFER_FORMAT_LW12:
        case LW_ENC_BUFFER_FORMAT_YUV444:
        case LW_ENC_BUFFER_FORMAT_YUV444_TILED16x16:
            return 0;

        case LW_ENC_BUFFER_FORMAT_YUV420_10BIT:
        case LW_ENC_BUFFER_FORMAT_YUV420_10BIT_TILED16x16:
        case LW_ENC_BUFFER_FORMAT_YUV444_10BIT:
        case LW_ENC_BUFFER_FORMAT_YUV444_10BIT_TILED16x16:
            return 2;

        default:
            MASSERT(!"Unknown depth");
            return 0;
    }
}

RC LwvidLWENC::PickConfiguration(EncodeSession *pES)
{
    RC rc;
    FancyPickerArray *fpk = GetFancyPickerArray();

    memset(&pES->initializeParams, 0, sizeof(pES->initializeParams));

    pES->encCodec = static_cast<EncCodec>((*fpk)[FPK_LWVIDLWENC_CODEC].Pick());
    CHECK_RC(CodecIdToGuid(pES->encCodec, &pES->initializeParams.encodeGUID));

    CHECK_RC(PickConfigurationPreset(pES));

    CHECK_RC(PickConfigurationTuningInfo(pES));

    pES->numFrames = (*fpk)[FPK_LWVIDLWENC_NUM_FRAMES].Pick();

    CHECK_RC(PickConfigurationSize(pES));

    pES->initializeParams.frameRateNum = 30;
    pES->initializeParams.frameRateDen = 1;

    pES->initializeParams.enablePTD = 1;

    // The driver sizes the output bitstream based on the maxEncodeWidth/Height.
    // Make it bigger than our surface needs so the random input content
    // doesn't overflow the output. Unfortunately there is no known mechanism
    // to stop the hw from overflowing and there is only the hard way:
    // kernel exception, mods hangs and when lucky the guard bands asserts.
    pES->initializeParams.maxEncodeWidth = 2 * pES->initializeParams.encodeWidth;
    pES->initializeParams.maxEncodeHeight = 2 * pES->initializeParams.encodeHeight;

    pES->initializeParams.encodeConfig = &pES->encConfig;

    memset(&pES->createInputBuffer, 0, sizeof(pES->createInputBuffer));
    pES->createInputBuffer.width = pES->initializeParams.encodeWidth;
    pES->createInputBuffer.height = pES->initializeParams.encodeHeight;
    pES->createInputBuffer.memoryHeap = LW_ENC_MEMORY_HEAP_SYSMEM_UNCACHED;
    CHECK_RC(PickBufferFormat(pES));

    memset(&pES->encConfig, 0, sizeof(pES->encConfig));
    pES->encConfig.gopLength = LWENC_INFINITE_GOPLENGTH;
    pES->encConfig.frameIntervalP = 1;
    pES->encConfig.frameFieldMode = LW_ENC_PARAMS_FRAME_FIELD_MODE_FRAME;
    pES->encConfig.rcParams.constQP.qpInterP = 28;
    pES->encConfig.rcParams.constQP.qpInterB = 31;
    pES->encConfig.rcParams.constQP.qpIntra = 25;
    pES->encConfig.rcParams.initialRCQP = pES->encConfig.rcParams.constQP;

    const UINT32 chromaFormatIDC = Is444Format(pES->createInputBuffer.bufferFmt) ? 3 : 1;
    const auto &caps = m_CodecCaps[pES->encCodec];
    switch (pES->encCodec)
    {
        case ENC_CODEC_H264:
            pES->encConfig.encodeCodecConfig.h264Config.idrPeriod  = LWENC_INFINITE_GOPLENGTH;
            pES->encConfig.encodeCodecConfig.h264Config.chromaFormatIDC = chromaFormatIDC;

            if (IsLoseLessPreset(pES->initializeParams.presetGUID) ||
                (pES->initializeParams.tuningInfo == LW_ENC_TUNING_INFO_LOSSLESS))
            {
                // As required by CMSENCH264Encoder::ValidateEncodeParams:
                pES->encConfig.encodeCodecConfig.h264Config.separateColourPlaneFlag = 1;
                pES->encConfig.encodeCodecConfig.h264Config.qpPrimeYZeroTransformBypassFlag = 1;
            }
            if (caps.isMultipleRefFramesSupported)
            {
                // Even though H/W supports 8 reference frames according to IAS, LWVID APIs only
                // expose max of 7 frames using front door API. Also, these refer to max frames.
                // If stream has < 7 frames, it only uses whatever possible
                pES->encConfig.encodeCodecConfig.h264Config.numRefL0 = LW_ENC_NUM_REF_FRAMES_7;
            }
            break;
        case ENC_CODEC_HEVC:
            pES->encConfig.encodeCodecConfig.hevcConfig.minLWSize  = LW_ENC_HEVC_LWSIZE_16x16;
            pES->encConfig.encodeCodecConfig.hevcConfig.maxLWSize  = LW_ENC_HEVC_LWSIZE_32x32;
            pES->encConfig.encodeCodecConfig.hevcConfig.idrPeriod  = LWENC_INFINITE_GOPLENGTH;
            pES->encConfig.encodeCodecConfig.hevcConfig.chromaFormatIDC = chromaFormatIDC;
            pES->encConfig.encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 =
                PixelBitDepthMinus8(pES->createInputBuffer.bufferFmt);
            if (caps.isMultipleRefFramesSupported)
            {
                // Even though H/W supports 8 reference frames according to IAS, LWVID APIs only
                // expose max of 7 frames using front door API. Also, these refer to max frames.
                // If stream has < 7 frames, it only uses whatever possible
                pES->encConfig.encodeCodecConfig.hevcConfig.numRefL0 = LW_ENC_NUM_REF_FRAMES_7;
            }
            break;
        default:
            MASSERT(!"Unexpected codec");
            return RC::SOFTWARE_ERROR;
    }

    memset(&pES->lockInputBuffer, 0, sizeof(pES->lockInputBuffer));

    memset(&pES->createBitstreamBuffer, 0, sizeof(pES->createBitstreamBuffer));
    pES->createBitstreamBuffer.memoryHeap = LW_ENC_MEMORY_HEAP_SYSMEM_CACHED;

    memset(&pES->encPicParams, 0, sizeof(pES->encPicParams));
    pES->encPicParams.inputWidth = pES->initializeParams.encodeWidth;
    pES->encPicParams.inputHeight = pES->initializeParams.encodeHeight;
    pES->encPicParams.bufferFmt = pES->createInputBuffer.bufferFmt;
    pES->encPicParams.pictureStruct = LW_ENC_PIC_STRUCT_FRAME;

    switch (pES->encCodec)
    {
        case ENC_CODEC_H264:
            pES->encPicParams.codecPicParams.h264PicParams.refPicFlag = 1;
            break;
        case ENC_CODEC_HEVC:
            pES->encPicParams.codecPicParams.hevcPicParams.refPicFlag = 1;
            break;
        default:
            MASSERT(!"Unexpected codec");
            return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC LwvidLWENC::PickConfigurationPreset(EncodeSession *pES)
{
    FancyPicker::FpContext* pFpCtx = GetFpContext();

    const auto &caps = m_CodecCaps[pES->encCodec];

    const UINT32 presetIdx = pFpCtx->Rand.GetRandom(0,
        static_cast<UINT32>(caps.presets.size()) - 1);

    pES->initializeParams.presetGUID = caps.presets[presetIdx];

    return RC::OK;
}

RC LwvidLWENC::PickConfigurationTuningInfo(EncodeSession *pES)
{
    FancyPicker::FpContext* pFpCtx = GetFpContext();

    if (!IsPXPreset(pES->initializeParams.presetGUID))
    {
        return RC::OK;
    }

    // TODO: Use LW_ENC_TUNING_INFO_COUNT once it becomes available
    pES->initializeParams.tuningInfo = static_cast<LW_ENC_TUNING_INFO>(
        pFpCtx->Rand.GetRandom(LW_ENC_TUNING_INFO_HIGH_QUALITY,
            LW_ENC_TUNING_INFO_LOSSLESS));

    return RC::OK;
}

RC LwvidLWENC::PickConfigurationSize(EncodeSession *pES)
{
    FancyPicker::FpContext* pFpCtx = GetFpContext();

    const auto &caps = m_CodecCaps[pES->encCodec];

    const UINT32 noPaddingWidth = 64;
    const UINT32 noPaddingHeight = 16;

    UINT32 widthMin = max(caps.widthMin, m_WidthMin);
    UINT32 heightMin = max(caps.heightMin, m_HeightMin);
    widthMin = ALIGN_UP(widthMin, noPaddingWidth);
    heightMin = ALIGN_UP(heightMin, noPaddingHeight);

    UINT32 widthMax = min(caps.widthMax, m_WidthMax);
    UINT32 heightMax = min(caps.heightMax, m_HeightMax);

    widthMax = max(widthMin + noPaddingWidth, widthMax);
    heightMax = max(heightMin + noPaddingHeight, heightMax);
    widthMax = ALIGN_DOWN(widthMax, noPaddingWidth);
    heightMax = ALIGN_DOWN(heightMax, noPaddingHeight);

    UINT32 width;
    UINT32 height;

    if (pFpCtx->Rand.GetRandom() & 1)
    {
        width = pFpCtx->Rand.GetRandom(widthMin, widthMax);
        width = ALIGN_DOWN(width + noPaddingWidth/2, noPaddingWidth);

        UINT32 sizedHeightMax = m_SizeMax / width;
        sizedHeightMax = ALIGN_UP(sizedHeightMax, noPaddingHeight);
        sizedHeightMax = min(sizedHeightMax, heightMax);
        if (sizedHeightMax > heightMin)
        {
            height = pFpCtx->Rand.GetRandom(heightMin, sizedHeightMax);
            height = ALIGN_DOWN(height + noPaddingHeight/2, noPaddingHeight);
        }
        else
        {
            height = heightMin;
        }
    }
    else
    {
        height = pFpCtx->Rand.GetRandom(heightMin, heightMax);
        height = ALIGN_DOWN(height + noPaddingHeight/2, noPaddingHeight);

        UINT32 sizedWidthMax = m_SizeMax / height;
        sizedWidthMax = ALIGN_UP(sizedWidthMax, noPaddingWidth);
        sizedWidthMax = min(sizedWidthMax, widthMax);
        if (sizedWidthMax > widthMin)
        {
            width = pFpCtx->Rand.GetRandom(widthMin, sizedWidthMax);
            width = ALIGN_DOWN(width + noPaddingWidth/2, noPaddingWidth);
        }
        else
        {
            width = widthMin;
        }
    }

    pES->initializeParams.encodeWidth = width;
    pES->initializeParams.encodeHeight = height;

    return RC::OK;
}

bool LwvidLWENC::IsLoseLessPreset(const GUID &presetGUID)
{
    if ((memcmp(&presetGUID, &LW_ENC_PRESET_LOSSLESS_DEFAULT_GUID, sizeof(GUID)) == 0) ||
        (memcmp(&presetGUID, &LW_ENC_PRESET_LOSSLESS_HP_GUID, sizeof(GUID)) == 0) ||
        (memcmp(&presetGUID, &LW_ENC_PRESET_LOSSLESS_DEFAULT_GUID, sizeof(GUID)) == 0))
    {
        return true;
    }

    return false;
}

bool LwvidLWENC::IsPXPreset(const GUID &presetGUID)
{
    if ((memcmp(&presetGUID, &LW_ENC_PRESET_P1_GUID, sizeof(GUID)) == 0) ||
        (memcmp(&presetGUID, &LW_ENC_PRESET_P2_GUID, sizeof(GUID)) == 0) ||
        (memcmp(&presetGUID, &LW_ENC_PRESET_P3_GUID, sizeof(GUID)) == 0) ||
        (memcmp(&presetGUID, &LW_ENC_PRESET_P4_GUID, sizeof(GUID)) == 0) ||
        (memcmp(&presetGUID, &LW_ENC_PRESET_P5_GUID, sizeof(GUID)) == 0) ||
        (memcmp(&presetGUID, &LW_ENC_PRESET_P6_GUID, sizeof(GUID)) == 0) ||
        (memcmp(&presetGUID, &LW_ENC_PRESET_P7_GUID, sizeof(GUID)) == 0))
    {
        return true;
    }

    return false;
}

bool LwvidLWENC::Is444FormatRequired(EncodeSession *pES)
{
    return IsLoseLessPreset(pES->initializeParams.presetGUID) ||
        (pES->initializeParams.tuningInfo == LW_ENC_TUNING_INFO_LOSSLESS);
}

RC LwvidLWENC::PickBufferFormat(EncodeSession *pES)
{
    const auto &caps = m_CodecCaps[pES->encCodec];

    const bool format444Required = Is444FormatRequired(pES);

    do
    {
        pES->createInputBuffer.bufferFmt = caps.bufferFormats[
            GetFpContext()->Rand.GetRandom(0,
                static_cast<UINT32>(caps.bufferFormats.size() - 1))];
    } while (format444Required && !Is444Format(pES->createInputBuffer.bufferFmt));

    if (m_ForceBufferFormat != LW_ENC_BUFFER_FORMAT_UNDEFINED)
    {
        pES->createInputBuffer.bufferFmt = static_cast<LW_ENC_BUFFER_FORMAT>(m_ForceBufferFormat);
    }

    return RC::OK;
}

RC LwvidLWENC::AllocateResources(EncodeSession *pES) const
{
    RC rc;

    CHECK_RC(Enc2RC(m_pEncoderInterface->CreateEncoder(&pES->initializeParams)));

    CHECK_RC(Enc2RC(m_pEncoderInterface->CreateInputSurface(&pES->createInputBuffer, 1)));
    pES->lockInputBuffer.inputBuffer = pES->createInputBuffer.inputBuffer;
    pES->encPicParams.inputBuffer = pES->createInputBuffer.inputBuffer;

    void *ppOutputStatusBufferPtr = nullptr;
    CHECK_RC(Enc2RC(m_pEncoderInterface->CreateBitstreamBuffer(
        &pES->createBitstreamBuffer, &ppOutputStatusBufferPtr)));
    pES->encPicParams.outputBitstream = pES->createBitstreamBuffer.bitstreamBuffer;

    if (m_SaveStreams)
    {
        const char *ext;
        switch (pES->encCodec)
        {
            case ENC_CODEC_H264: ext = "264"; break;
            case ENC_CODEC_HEVC: ext = "265"; break;
            default: MASSERT(!"Unexpected codec"); return RC::SOFTWARE_ERROR;
        }

        CHECK_RC(pES->bitstreamFile.Open(
            Utility::StrPrintf("lwvidlwenc_stream_%d.%s", pES->streamLoopIdx, ext), "wb"));
    }

    return rc;
}

RC LwvidLWENC::FreeResources(EncodeSession *pES) const
{
    StickyRC rc;

    if (pES->createInputBuffer.inputBuffer)
    {
        rc = Enc2RC(m_pEncoderInterface->ReleaseInputSurface(
            pES->createInputBuffer.inputBuffer));
        pES->createInputBuffer.inputBuffer = nullptr;
    }

    if (pES->createBitstreamBuffer.bitstreamBuffer)
    {
        rc = Enc2RC(m_pEncoderInterface->ReleaseBitstreamBuffer(
            pES->createBitstreamBuffer.bitstreamBuffer));
        pES->createBitstreamBuffer.bitstreamBuffer = nullptr;
    }

    return rc;
}

RC LwvidLWENC::PrepareFrameParameters(EncodeSession *pES, UINT32 frameIdx)
{
    pES->encPicParams.frameIdx = frameIdx;
    pES->encPicParams.inputTimeStamp = frameIdx;
    pES->encPicParams.pictureType = (frameIdx == 0) ? LW_ENC_PIC_TYPE_IDR : LW_ENC_PIC_TYPE_P;

    switch (pES->encCodec)
    {
        case ENC_CODEC_H264:
            break;
        case ENC_CODEC_HEVC:
            pES->encPicParams.codecPicParams.hevcPicParams.displayPOCSyntax = frameIdx;
            break;
        default:
            MASSERT(!"Unexpected codec");
            return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

RC LwvidLWENC::PrepareInputContent
(
    EncodeSession *pES,
    UINT32 frameIdx,
    UINT32 inputSize,
    UINT32 pitch
)
{
    RC rc;
    FancyPickerArray *fpk = GetFancyPickerArray();
    UINT32 action;

    do
    {
        action = (*fpk)[FPK_LWVIDLWENC_CONTENT_ACTION].Pick();
    } while ((frameIdx == 0) &&
        // Only these fill the whole frame:
        (action != FPK_LWVIDLWENC_CONTENT_ACTION_FILL_RANDOM) &&
        (action != FPK_LWVIDLWENC_CONTENT_ACTION_FILL_PATTERN));

    switch (action)
    {
        case FPK_LWVIDLWENC_CONTENT_ACTION_FILL_RANDOM:
        {
            auto &rand = GetFpContext()->Rand;
            UINT32 *pData = static_cast<UINT32*>(pES->lockInputBuffer.bufferDataPtr);
            const UINT32 *pDataEnd = pData + inputSize/4;
            for (; pData < pDataEnd; pData++)
            {
                *pData = rand.GetRandom();
            }
            break;
        }

        case FPK_LWVIDLWENC_CONTENT_ACTION_FILL_PATTERN:
        {
            const UINT32 patternIdx = (*fpk)[FPK_LWVIDLWENC_CONTENT_PATTERN_IDX].Pick();
            CHECK_RC(m_PatternClass.FillMemoryWithPattern(
                pES->lockInputBuffer.bufferDataPtr, inputSize, 1, inputSize,
                32, patternIdx, nullptr));
            break;
        }

        case FPK_LWVIDLWENC_CONTENT_ACTION_MOVE:
            CHECK_RC(PrepareInputContentActionMove(pES, inputSize, pitch));
            break;

        case FPK_LWVIDLWENC_CONTENT_ACTION_SPRINKLE:
        {
            const UINT32 sprinkleOffset = (*fpk)[FPK_LWVIDLWENC_CONTENT_SPRINKLE_OFFSET].Pick();
            const UINT08 sprinkleAnd = (*fpk)[FPK_LWVIDLWENC_CONTENT_SPRINKLE_AND_VALUE].Pick();
            const UINT08 sprinkleXor = (*fpk)[FPK_LWVIDLWENC_CONTENT_SPRINKLE_XOR_VALUE].Pick();

            UINT08 *pData = static_cast<UINT08*>(pES->lockInputBuffer.bufferDataPtr);
            const UINT08 *pDataEnd = pData + inputSize;

            pData += GetFpContext()->Rand.GetRandom(0, sprinkleOffset);
            while (pData < pDataEnd)
            {
                *pData &= sprinkleAnd;
                *pData ^= sprinkleXor;
                pData += sprinkleOffset;
            }
            break;
        }

        default:
            return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC LwvidLWENC::PrepareInputContentActionMove
(
    EncodeSession *pES,
    UINT32 inputSize,
    UINT32 pitch
)
{
    FancyPickerArray *fpk = GetFancyPickerArray();

    const UINT32 numBlocksToMove = (*fpk)[FPK_LWVIDLWENC_CONTENT_MOVE_NUM_BLOCKS].Pick();

    for (UINT32 blockIdx = 0; blockIdx < numBlocksToMove; blockIdx++)
    {
        const INT32 moveX = static_cast<INT32>((*fpk)[FPK_LWVIDLWENC_CONTENT_MOVE_X].FPick());
        const INT32 moveY = static_cast<INT32>((*fpk)[FPK_LWVIDLWENC_CONTENT_MOVE_Y].FPick());
        const UINT32 moveWidth = (*fpk)[FPK_LWVIDLWENC_CONTENT_MOVE_WIDTH].Pick();
        const UINT32 moveHeight = (*fpk)[FPK_LWVIDLWENC_CONTENT_MOVE_HEIGHT].Pick();
        const UINT32 moveLocationPct = (*fpk)[FPK_LWVIDLWENC_CONTENT_MOVE_LOCATION].Pick();
        MASSERT(moveLocationPct < 100);

        const INT64 stepX = moveX > 0 ? -1LL : 1LL;
        const INT64 stepY = moveY > 0 ? -1LL*pitch : pitch;

        UINT08* const lowerLimit = static_cast<UINT08*>(pES->lockInputBuffer.bufferDataPtr);
        UINT08* const upperLimit = lowerLimit + inputSize - 1;

        UINT08 *srcData = lowerLimit;
        srcData += moveLocationPct * inputSize / 100;

        srcData += (moveX > 0 ? moveWidth - 1 : 0) +
                   (moveY > 0 ? (moveHeight - 1) * pitch : 0);

        UINT08 *dstData = srcData +
            (moveX > 0 ? moveX : 1LL * moveX) +
            (moveY > 0 ? moveY * pitch : 1LL * moveY * pitch);

        const INT64 stepYcompensated = stepY + ((stepX  > 0) ? -1LL*moveWidth : moveWidth);

        for (UINT32 y = 0; y < moveHeight; y++)
        {
            for (UINT32 x = 0; x < moveWidth; x++)
            {
                if (dstData < lowerLimit || dstData > upperLimit)
                    continue;
                if (srcData < lowerLimit || srcData > upperLimit)
                    continue;
                *dstData = *srcData;
                dstData += stepX;
                srcData += stepX;
            }
            dstData += stepYcompensated;
            srcData += stepYcompensated;
        }
    }

    return RC::OK;
}

RC LwvidLWENC::PrepareInputSurface(EncodeSession *pES, UINT32 frameIdx)
{
    StickyRC rc;

    CHECK_RC(Enc2RC(m_pEncoderInterface->LockInputSurface(&pES->lockInputBuffer)));
    {
        DEFER
        {
            rc = Enc2RC(m_pEncoderInterface->UnlockInputSurface(
                pES->createInputBuffer.inputBuffer));
        };

        UINT32 inputSize;
        switch (pES->createInputBuffer.bufferFmt)
        {
            case LW_ENC_BUFFER_FORMAT_LW12:
            case LW_ENC_BUFFER_FORMAT_YUV420_10BIT:
                inputSize = 3 * pES->createInputBuffer.height * pES->lockInputBuffer.pitch / 2;
                break;

            case LW_ENC_BUFFER_FORMAT_YUV444:
            case LW_ENC_BUFFER_FORMAT_YUV444_10BIT:
                inputSize = 3 * pES->createInputBuffer.height * pES->lockInputBuffer.pitch;
                break;

            default:
                MASSERT(!"Missing surface size formula");
                return RC::SOFTWARE_ERROR;
        }

        CHECK_RC(PrepareInputContent(pES, frameIdx, inputSize, pES->lockInputBuffer.pitch));

        if (m_SaveInputSurfaces)
        {
            FileHolder file;
            CHECK_RC(file.Open(Utility::StrPrintf(
                "lwvidlwenc_input_stream_%d_frame_%d.bin",
                pES->streamLoopIdx, frameIdx), "wb"));
            const size_t sizeWritten = fwrite(pES->lockInputBuffer.bufferDataPtr,
                1, inputSize, file.GetFile());
            if (sizeWritten != inputSize)
            {
                Printf(Tee::PriError, "LwvidLWENC unable to write input surfaces.\n");
                return RC::FILE_WRITE_ERROR;
            }
        }
    }

    return rc;
}

RC LwvidLWENC::ProcessOutputBitstream(EncodeSession *pES, UINT32 frameIdx)
{
    StickyRC rc;

    LW_ENC_LOCK_BITSTREAM lockBitstream;
    memset(&lockBitstream, 0, sizeof(lockBitstream));
    lockBitstream.outputBitstream = pES->encPicParams.outputBitstream;

    UINT32 encodeStatusWaitCounter = 0;
    while (true)
    {
        CHECK_RC(rc); // For previous loop's deferred UnlockBitstream
        CHECK_RC(Enc2RC(m_pEncoderInterface->LockBitstream(&lockBitstream)));
        DEFER
        {
            rc = Enc2RC(m_pEncoderInterface->UnlockBitstream(lockBitstream.outputBitstream));
        };

        VerbosePrintf("   lockBitstream.hwEncodeStatus = %d\n", lockBitstream.hwEncodeStatus);

        if (lockBitstream.hwEncodeStatus != 2)
        {
            encodeStatusWaitCounter++;
            if (encodeStatusWaitCounter > 100)
            {
                Printf(Tee::PriError, "Timed out waiting for encode status 2\n");
                return RC::TIMEOUT_ERROR;
            }
            Tasker::Sleep(1);
            continue;
        }

        VerbosePrintf("   lockBitstream.bitstreamSizeInBytes = %d\n",
            lockBitstream.bitstreamSizeInBytes);
        VerbosePrintf("   pictureType = 0x%02x\n", lockBitstream.pictureType);

        CHECK_RC(m_OnedGoldenSurfaces.SetSurface("bitstream",
            lockBitstream.bitstreamBufferPtr,
            lockBitstream.bitstreamSizeInBytes));
        GetGoldelwalues()->SetLoop(1'000'000 * pES->streamLoopIdx + frameIdx);
        CHECK_RC(GetGoldelwalues()->Run());

        if (m_PrintOutputBitstreams)
        {
            for (UINT32 idx = 0; idx < lockBitstream.bitstreamSizeInBytes; idx++)
            {
                Printf(Tee::PriNormal, " %02x",
                    static_cast<UINT08*>(lockBitstream.bitstreamBufferPtr)[idx]);
                if ((idx % 16) == 15)
                {
                    Printf(Tee::PriNormal, "\n");
                }
            }
            Printf(Tee::PriNormal, "\n");
        }

        if (pES->bitstreamFile.GetFile())
        {
            const size_t sizeWritten = fwrite(lockBitstream.bitstreamBufferPtr, 1,
                lockBitstream.bitstreamSizeInBytes, pES->bitstreamFile.GetFile());
            if (sizeWritten != lockBitstream.bitstreamSizeInBytes)
            {
                Printf(Tee::PriError, "LwvidLWENC unable to write bitstream.\n");
                return RC::FILE_WRITE_ERROR;
            }
        }
        break;
    }

    return rc;
}

RC LwvidLWENC::RunConfiguration(EncodeSession *pES)
{
    StickyRC rc;

    for (UINT32 frameIdx = 0; frameIdx < pES->numFrames; frameIdx++)
    {
        if (m_PerpetualRun && !m_KeepRunning)
        {
            return rc;
        }

        CHECK_RC(Abort::Check());

        CHECK_RC(PrepareFrameParameters(pES, frameIdx));

        CHECK_RC(PrepareInputSurface(pES, frameIdx));

        // Do not call PreProcessFrame on purpose as for most formats
        // it is not needed anyway and for the rest it is just LWCA
        // function call. Since our input data is random it should not
        // make a difference.

        CHECK_RC(Enc2RC(m_pEncoderInterface->EncodePicture(&pES->encPicParams)));

        CHECK_RC(ProcessOutputBitstream(pES, frameIdx));
    }

    return rc;
}

RC LwvidLWENC::Run()
{
    StickyRC rc;

    INT32 startLoop;
    INT32 endLoop;
    INT32 step;
    m_PerpetualRun = m_KeepRunning;

    if (GetGoldelwalues()->GetAction() == Goldelwalues::Store && !m_DisableReverseFrames)
    {
        startLoop = m_pTestConfig->StartLoop() + m_pTestConfig->Loops() - 1;
        endLoop =  m_pTestConfig->StartLoop() - 1;
        step = -1;
    }
    else
    {
        startLoop = m_pTestConfig->StartLoop();
        endLoop = startLoop + m_pTestConfig->Loops();
        step = 1;
    }

    PrintProgressInit(m_pTestConfig->Loops(), "Test Configuration Loops");
    UINT32 numStreamsEncoded = 0;
    do
    {
        for (INT32 streamLoopIdx = startLoop; streamLoopIdx != endLoop; streamLoopIdx += step)
        {
            CHECK_RC(rc); // For previous loop's deferred FreeResources call

            if (m_PerpetualRun && !m_KeepRunning)
            {
                break;
            }
            CHECK_RC(Abort::Check());

            if (m_SkipLoopsIndices.find(streamLoopIdx) != m_SkipLoopsIndices.end())
            {
                continue;
            }

            VerbosePrintf("Stream index = %d\n", streamLoopIdx);

            GetFpContext()->Rand.SeedRandom(m_pTestConfig->Seed() ^ (123*streamLoopIdx));
            GetFancyPickerArray()->Restart();

            EncodeSession encodeSession;
            encodeSession.streamLoopIdx = streamLoopIdx;
            CHECK_RC(PickConfiguration(&encodeSession));

            DEFER
            {
                rc = FreeResources(&encodeSession);
            };

            CHECK_RC(AllocateResources(&encodeSession));
            CHECK_RC(RunConfiguration(&encodeSession));
            PrintProgressUpdate(std::min(++numStreamsEncoded, m_pTestConfig->Loops()));
        }
    } while (m_KeepRunning);

    CHECK_RC(GetGoldelwalues()->ErrorRateTest(GetJSObject()));

    return rc;
}

RC LwvidLWENC::Cleanup()
{
    LWcontext primaryCtx = GetLwdaInstance().GetHandle(GetBoundLwdaDevice());

    if (m_pEncoderInterface)
    {
        m_pEncoderInterface->ReleaseDevice();
        m_pEncoderInterface = nullptr;
    }

    StickyRC rc = Lwca::LwdaErrorToRC(lwCtxPopLwrrent(&primaryCtx));

    rc = LwdaStreamTest::Cleanup();

    return rc;
}

void LwvidLWENC::PrintJsProperties(Tee::Priority pri)
{
    const char * TF[] = { "false", "true" };

    GpuTest::PrintJsProperties(pri);
    Printf(pri, "LwvidLWENC test Js Properties:\n");
    Printf(pri, "\tDisableReverseFrames:\t\t%s\n", TF[m_DisableReverseFrames]);
    Printf(pri, "\tPrintOutputBitstreams:\t\t%s\n", TF[m_PrintOutputBitstreams]);
    Printf(pri, "\tSaveInputSurfaces:\t\t%s\n", TF[m_SaveInputSurfaces]);
    Printf(pri, "\tSaveStreams:\t\t\t%s\n", TF[m_SaveStreams]);
    Printf(pri, "\tSkipLoops:\t\t\t%s\n", m_SkipLoops.c_str());
    Printf(pri, "\tWidthMin:\t\t\t%d\n", m_WidthMin);
    Printf(pri, "\tHeightMin:\t\t\t%d\n", m_HeightMin);
    Printf(pri, "\tWidthMax:\t\t\t%d\n", m_WidthMax);
    Printf(pri, "\tHeightMax:\t\t\t%d\n", m_HeightMax);
    Printf(pri, "\tSizeMax:\t\t\t%d\n", m_SizeMax);
    Printf(pri, "\tForceBufferFormat:\t\t0x%08x\n", m_ForceBufferFormat);
    Printf(pri, "\tKeepRunning:\t\t\t%s\n", m_KeepRunning ? "true" : "false");
}

RC LwvidLWENC::SetDefaultsForPicker(UINT32 idx)
{
    FancyPicker *pPicker = &(*GetFancyPickerArray())[idx];
    RC rc;

    switch (idx)
    {
        case FPK_LWVIDLWENC_CODEC:
            CHECK_RC(pPicker->ConfigRandom());
            pPicker->AddRandItem(1, ENC_CODEC_H264);
            break;

        case FPK_LWVIDLWENC_NUM_FRAMES:
            CHECK_RC(pPicker->ConfigRandom());
            // Change num frames to 8 to test 7 ref frames scenario (I P P P P P P P)
            pPicker->AddRandItem(25, 8); // Skew towards testing max frames
            pPicker->AddRandRange(75, 1, 7);
            break;

        case FPK_LWVIDLWENC_CONTENT_ACTION:
            CHECK_RC(pPicker->ConfigRandom());
            pPicker->AddRandItem(1, FPK_LWVIDLWENC_CONTENT_ACTION_FILL_RANDOM);
            pPicker->AddRandItem(1, FPK_LWVIDLWENC_CONTENT_ACTION_FILL_PATTERN);
            pPicker->AddRandItem(2, FPK_LWVIDLWENC_CONTENT_ACTION_MOVE);
            pPicker->AddRandItem(2, FPK_LWVIDLWENC_CONTENT_ACTION_SPRINKLE);
            break;

        case FPK_LWVIDLWENC_CONTENT_PATTERN_IDX:
        {
            CHECK_RC(pPicker->ConfigRandom());
            const UINT32 numPat = m_PatternClass.GetLwrrentNumOfSelectedPatterns();
            if (numPat == 0)
            {
                Printf(Tee::PriError, "LwvidLWENC requires at least one pattern.\n");
                return RC::ILWALID_INPUT;
            }
            pPicker->AddRandRange(1, 0, numPat - 1);
            break;
        }

        case FPK_LWVIDLWENC_CONTENT_MOVE_NUM_BLOCKS:
            CHECK_RC(pPicker->ConfigRandom());
            pPicker->AddRandRange(1, 1, 20);
            break;

        case FPK_LWVIDLWENC_CONTENT_MOVE_X:
            CHECK_RC(pPicker->FConfigRandom());
            pPicker->FAddRandRange(1, -7.0f, +7.0f);
            break;

        case FPK_LWVIDLWENC_CONTENT_MOVE_Y:
            CHECK_RC(pPicker->FConfigRandom());
            pPicker->FAddRandRange(1, -7.0f, +7.0f);
            break;

        case FPK_LWVIDLWENC_CONTENT_MOVE_WIDTH:
            CHECK_RC(pPicker->ConfigRandom());
            pPicker->AddRandRange(1, 1, 63);
            break;

        case FPK_LWVIDLWENC_CONTENT_MOVE_HEIGHT:
            CHECK_RC(pPicker->ConfigRandom());
            pPicker->AddRandRange(1, 1, 26);
            break;

        case FPK_LWVIDLWENC_CONTENT_MOVE_LOCATION:
            CHECK_RC(pPicker->ConfigRandom());
            pPicker->AddRandRange(1, 1, 99); // % of memory allocation
            break;

        case FPK_LWVIDLWENC_CONTENT_SPRINKLE_OFFSET:
            CHECK_RC(pPicker->ConfigRandom());
            pPicker->AddRandRange(1, 15, 127);
            break;

        case FPK_LWVIDLWENC_CONTENT_SPRINKLE_AND_VALUE:
            CHECK_RC(pPicker->ConfigRandom());
            pPicker->AddRandRange(1, 1, 255);
            break;

        case FPK_LWVIDLWENC_CONTENT_SPRINKLE_XOR_VALUE:
            CHECK_RC(pPicker->ConfigRandom());
            pPicker->AddRandRange(1, 0, 255);
            break;

        default:
            MASSERT(!"Unknown picker");
            return RC::SOFTWARE_ERROR;
    }
    pPicker->CompileRandom();
    return rc;
}

RC LwvidLWENC::UpdatePickers()
{
    RC rc;
    FancyPickerArray *pPickers = GetFancyPickerArray();

    if (!(*pPickers)[FPK_LWVIDLWENC_CODEC].WasSetFromJs())
    {
        FancyPicker *pPicker = &(*GetFancyPickerArray())[FPK_LWVIDLWENC_CODEC];

        CHECK_RC(pPicker->ConfigRandom());
        if (m_AvailableEncCodecs & EncCodec2Bitmask(ENC_CODEC_H264))
        {
            pPicker->AddRandItem(1, ENC_CODEC_H264);
        }
        if (m_AvailableEncCodecs & EncCodec2Bitmask(ENC_CODEC_HEVC))
        {
            pPicker->AddRandItem(1, ENC_CODEC_HEVC);
        }
        pPicker->CompileRandom();
    }

    return rc;
}

bool LwvidLWENC::IsSupported()
{
#if defined(LWCPU_AARCH64)
    // From ausvrl8622(gp104_pg418_0503):
    // "ERROR: Encoder status = 10"
    Printf(Tee::PriLow, "LwvidLWENC is not supported on aarch64\n");
    return false;
#else
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(Tee::PriLow, "LwvidLWENC is not supported on SOC devices\n");
        return false;
    }

    if (GetBoundGpuDevice()->GetFamily() < GpuDevice::Pascal)
    {
        Printf(Tee::PriLow, "LwvidLWENC is not supported on pre-Pascal devices\n");
        return false;
    }

    const EncoderAlloc encoderAlloc;
    const bool supported = encoderAlloc.IsSupported(GetBoundGpuDevice());
    if (!supported)
    {
        Printf(Tee::PriLow, "LwvidLWENC has not found supported LWENC hw class\n");
    }
    return supported;
#endif
}

RC LwvidLWENC::Enc2RC(LWENCSTATUS lwencstatus) const
{
    switch (lwencstatus)
    {
        case LW_ENC_SUCCESS:
            return RC::OK;

        case LW_ENC_ERR_NO_ENCODE_DEVICE:
            return RC::DEVICE_NOT_FOUND;

        case LW_ENC_ERR_UNSUPPORTED_DEVICE:
            return RC::UNSUPPORTED_HARDWARE_FEATURE;

        default:
            Printf(Tee::PriError, "Encoder status = %d (%s):\n",
                lwencstatus,
                m_pEncoderInterface ?
                    m_pEncoderInterface->GetLastErrorString() :
                    "unknown");
            return RC::SOFTWARE_ERROR;
    }
}

JS_CLASS_INHERIT(LwvidLWENC, GpuTest, "Lwvid library based LWENC test");

CLASS_PROP_READWRITE(LwvidLWENC, DisableReverseFrames, bool,
    "Disable running frames in reverse during gpugen.js (default=false)");

CLASS_PROP_READWRITE(LwvidLWENC, PrintOutputBitstreams, bool,
    "Print content of generated bitstream as hex");

CLASS_PROP_READWRITE(LwvidLWENC, SaveInputSurfaces, bool,
    "Save content of input surface for each frame");

CLASS_PROP_READWRITE(LwvidLWENC, SaveStreams, bool,
    "Save resulting streams to files");

CLASS_PROP_READWRITE(LwvidLWENC, SkipLoops, string,
    "Indices of the loops to skip");

CLASS_PROP_READWRITE(LwvidLWENC, WidthMin, UINT32,
    "User adjustable limit on top of codec capabilities");

CLASS_PROP_READWRITE(LwvidLWENC, HeightMin, UINT32,
    "User adjustable limit on top of codec capabilities");

CLASS_PROP_READWRITE(LwvidLWENC, WidthMax, UINT32,
    "User adjustable limit on top of codec capabilities");

CLASS_PROP_READWRITE(LwvidLWENC, HeightMax, UINT32,
    "User adjustable limit on top of codec capabilities");

CLASS_PROP_READWRITE(LwvidLWENC, SizeMax, UINT32,
    "Maximum frame size (it has lower priority than Width/Height Min - both from user and codec)");

CLASS_PROP_READWRITE(LwvidLWENC, ForceBufferFormat, UINT32,
    "Use only specified input buffer format");

CLASS_PROP_READWRITE(LwvidLWENC, KeepRunning, bool,
    "Keep the test running in the background");
