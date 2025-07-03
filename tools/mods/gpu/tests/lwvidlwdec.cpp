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
#include "core/include/tar.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/onedgoldensurfaces.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/perf/pwrwait.h"
#include "gpu/tests/lwdastst.h"
#include "gputest.h"
#include "lwlwvid.h"
#include "platform/LwThreading/LwThreadingClasses.h"
#include "gpu/tests/lwca/lwdawrap.h"
#include "gpu/utility/surfrdwr.h"
#include <queue>

#if defined(DIRECTAMODEL)
#include "opengl/modsgl.h"
#include "common/amodel/IDirectAmodelCommon.h"
#endif

//---------------------------------------------------------------------------
// Local structures / variables
//---------------------------------------------------------------------------

class LwvidLWDEC;

static constexpr UINT32 EVENT_DISPLAY = 0;
static constexpr UINT32 EVENT_INIT = 1;

static constexpr INT32 MAX_FRM_CNT = 20;
static constexpr INT32 MAX_QUEUE_SIZE = 20;

struct FrameStats
{
    UINT32 picNumInDecodeOrder;
    UINT32 bytesInPicture;
};

struct DecodeSession
{
    LWvideosource           lwVidSrc                     = nullptr;
    LWvideoparser           lwParser                     = nullptr;
    LWvideodecoder          lwDecoder                    = nullptr;
    LWvideoctxlock          ctxLock                      = nullptr;
    // Output in decode order (ignore display callbacks)
    UINT32                  decoderOrder                 = 0;
    UINT32                  decodePicCnt                 = 0;
    UINT32                  loopCnt                      = 0;
    LWVIDDECODECREATEINFO   dci                          = { };
    // Queue event to synchronize EventInit event
    ModsEvent               *pEventCommand               = nullptr;
    // Critical section to protect display queue
    Tasker::Mutex           pMutex                       = Tasker::NoMutex();
    // Volatile state
    volatile UINT32         queuePosition                = 0;
    volatile UINT32         numEventsInQueue             = 0;
    LWVIDPARSERDISPINFO     displayQueue[MAX_QUEUE_SIZE] = { };
    volatile UINT32         eventType[MAX_QUEUE_SIZE]    = { };
    volatile UINT32         decodeEos                    = 0;
    volatile bool           frameInUse[MAX_FRM_CNT]      = { };
    volatile UINT32         reconfigReqd                 = 0;
    FrameStats              frmStats[MAX_FRM_CNT]        = { };
    FILE                    *fdElementary                = nullptr;
    UINT32                  numReconfigs                 = 0;
    UINT64                  hwCycleCountSum              = 0;
    UINT32                  sliceDecodeEnable            = 0;
    UINT32                  sliceMergeMode               = 0;
};

struct PerFrameGoldenParams
{
    UINT32                goldenFrameIdx;
    UINT32                width;
    UINT32                height;
    UINT32                pitch;
    vector<UINT08>        lumaData;
    vector<UINT08>        chromaUData;
    vector<UINT08>        chromaVData;
    lwdaVideoChromaFormat chromaFormat;
    lwdaVideoSurfaceFormat outputFormat;
};

//---------------------------------------------------------------------------
// Test class declaration
//---------------------------------------------------------------------------

class LwvidLWDEC : public LwdaStreamTest
{
public:
    LwvidLWDEC();

    bool IsSupported() override;
    bool IsSupportedVf() override { return true; }
    RC Setup() override;
    RC Run() override;
    RC InnerRun();
    RC Cleanup() override;
    void PrintJsProperties(Tee::Priority pri) override;
    RC InitFromJs() override;
    RC SetDefaultsForPicker(UINT32 idx) override;
    SETGET_PROP(GenYuvOutput, bool);
    SETGET_PROP(UseLoadBalancing, bool);
    SETGET_PROP(SkipEngineMask, UINT32);
    SETGET_PROP(SkipStreamMask, UINT32);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(RuntimeMs, UINT32);
    SETGET_PROP(Use2DSurfaceForGolden, bool);
    SETGET_PROP(DoRTD3Cycles, bool);
    SETGET_PROP(ForceGCxPstate, UINT32);
    SETGET_PROP(PciConfigSpaceRestoreDelayMs, UINT32);

private:
    struct BubbleParams
    {
        Goldelwalues *pGolden = nullptr;
        GCxBubble    *pBubble = nullptr;
        FancyPicker  *pFpDelay = nullptr;
    };
    struct EngineParameters
    {
        unsigned long lwdelwsageMode;
        LwvidLWDEC   *pTestObj;
        StickyRC      engineRc;
        BubbleParams  bubbleParams;
    };

    class EngineContext
    {
    public:
        explicit EngineContext(EngineParameters *pEP);
        RC Init();
        ~EngineContext();

        RC DecodeStream
        (
            const char *argInput
        );

        SETGET_PROP(StreamIdx, UINT32);

    private:
        DecodeSession                m_State;

        LwvidLWDEC                  *m_pTestObj;
        unsigned long                m_LwdelwsageMode;
        StickyRC                    *m_pEngineRc;

        bool                         m_DecodeInProgress = false;
        UINT32                       m_StreamIdx = 0;

        queue<unique_ptr<PerFrameGoldenParams>> m_GoldenQueue;
        Tasker::Mutex                m_GoldenQueueMutex = Tasker::NoMutex();
        SemaID                       m_GoldenSema = nullptr;

        Tasker::ThreadID             m_GldThread = Tasker::NULL_THREAD;
        StickyRC                     m_GoldenRc;

        Surface2D                    m_LumaSurf;
        Surface2D                    m_ChromaUSurf;
        Surface2D                    m_ChromaVSurf;
        SurfaceUtils::MappingSurfaceWriter m_LumaWriter;
        SurfaceUtils::MappingSurfaceWriter m_ChromaUWriter;
        SurfaceUtils::MappingSurfaceWriter m_ChromaVWriter;
        ColorUtils::Format m_lumaColorFormat = ColorUtils::LWFMT_NONE;
        ColorUtils::Format m_chromaUColorFormat = ColorUtils::LWFMT_NONE;
        ColorUtils::Format m_chromaVColorFormat = ColorUtils::LWFMT_NONE;

        bool                         m_TestedSomething = false;

        static void GoldenThread
        (
            void *engineContext
        );
        bool IsGoldenQueueEmpty() const;
        // Golden related functions
        RC GoldensPerFrame
        (
            PerFrameGoldenParams *params
        );
        RC CopyOutputToSurfaces
        (
            PerFrameGoldenParams *params
        );

        // Callback functions
        static void QueueEvent
        (
            DecodeSession *state,
            UINT32 event_type,
            LWVIDPARSERDISPINFO* pDispInfo
        );
        static INT32 LWDAAPI HandlePictureDisplay
        (
            void *pvUserData,
            LWVIDPARSERDISPINFO *pPicParams
        );
        static INT32 LWDAAPI HandleVideoData
        (
            void *pvUserData,
            LWVIDSOURCEDATAPACKET *pPacket
        );
        static INT32 LWDAAPI HandleVideoSequence
        (
            void *pvUserData,
            LWVIDEOFORMAT *pFormat
        );
        static INT32 LWDAAPI HandlePictureDecode
        (
            void *pvUserData,
            LWVIDPICPARAMS *pPicParams
        );

        // Utility functions
        template <typename T>
        static void SemiPlanarToPlanar
        (
            const T *py,
            const T *puv,
            T *pdst,
            UINT32 width,
            UINT32 height,
            UINT32 pitch,
            lwdaVideoChromaFormat format
        );
        static void CopyPlane
        (
            const unsigned char* pSrc,
            unsigned char* pDst,
            UINT32 width,
            UINT32 height,
            UINT32 pitch
        );
        static RC ColwertAndCopyOutput
        (
            unsigned char *dest,
            unsigned char *src,
            UINT32 width,
            UINT32 height,
            UINT32 pitch,
            UINT32 outputHeight,
            UINT32 lumaSize,
            lwdaVideoSurfaceFormat surfFormat,
            lwdaVideoChromaFormat outFormat
        );
        static string LwdelwsageModeToStr
        (
            unsigned long lwdelwsageMode
        );
        RC ValidateVideoStream
        (
            LWvideosource   *lwVidSrc,
            LWVIDEOFORMATEX *vidFormat
        ) const;
        RC InitDecodeSession
        (
            LWVIDEOFORMATEX *vidFormat,
            const char      *argInput
        );
        RC CopyOutputForGoldenGen
        (
            const UINT08 * const src,
            UINT32 height,
            UINT32 width,
            UINT32 pitch,
            UINT32 outputHeight,
            lwdaVideoSurfaceFormat surfFormat,
            vector<UINT08> *pDestLumaData,
            vector<UINT08> *pDestChromaUData,
            vector<UINT08> *pDestChromaVData
        );
    };

    RC CreateStreamFiles();

    static void EngineThread(void *pEngineParameters);
    RC DecodeStreams(EngineParameters *pEP);

    SETGET_PROP(Isreconfig, bool);
    SETGET_PROP(Dither, bool);

    // Class member variables
    vector<string>                          m_StreamNames;
    // We need to remember if test was started with KeepRunning set to true in
    // order to know whether to end the test early once KeepRunning has been
    // switched to false
    bool                                    m_PerpetualRun = false;
    UINT64                                  m_StartTimeMs = 0;
    UINT32                                  m_SkipStreamMask = 0;
    bool                                    m_Isreconfig = false;
    bool                                    m_Dither = false;
    bool                                    m_GenYuvOutput = false;
    bool                                    m_UseLoadBalancing = false;
    UINT32                                  m_SkipEngineMask = 0;
    bool                                    m_KeepRunning = false;
    unique_ptr<GpuGoldenSurfaces>           m_pGoldenSurfaces;
    Tasker::Mutex                           m_GoldenMutex = Tasker::NoMutex();
    UINT32                                  m_RuntimeMs = 0;
    bool                                    m_Use2DSurfaceForGolden = false;
    OnedGoldenSurfaces                      m_OnedGoldenSurfaces;
    vector<string>                          m_StreamsToTest;
    bool                                    m_DoRTD3Cycles = false;
    UINT32                                  m_ForceGCxPstate = Perf::ILWALID_PSTATE;
    UINT32                                  m_PciConfigSpaceRestoreDelayMs = 0;
    unique_ptr<GCxBubble>                   m_GCxBubble;
    UINT32                                  m_MaxNumStreamsDecoded = 0;
    UINT32                                  m_NumStreamsDecoded = 0;
    void *                                  m_PrintProgressMutex = nullptr;
};

enum StreamIndexes
{
    H264_4K_UHD             = 0,
    H264_HD_CABAC           = 1,
    H264_HD_CAVLC           = 2,
    H264_CRTPATH_CABAC      = 3,
    H265_IN_TO_TREE         = 4,
    H265_OLD_TOWN_CROSS     = 5,
    HMRExt_MAIN444_CTB32x32 = 6,
    HMRExt_MAIN444_CTB64x64 = 7,
    AV1_HD_10BIT            = 8,
    NUM_STREAMS
};

JS_CLASS(LwvidLwdecStrm);
static SObject LwvidLwdecStrm_Object
(
    "LwvidLwdecStrm",
    LwvidLwdecStrmClass,
    0,
    0,
    "LwvidLwdec test stream constants"
);
PROP_CONST(LwvidLwdecStrm, H264_4K_UHD,             H264_4K_UHD);
PROP_CONST(LwvidLwdecStrm, H264_HD_CABAC,           H264_HD_CABAC);
PROP_CONST(LwvidLwdecStrm, H264_HD_CAVLC,           H264_HD_CAVLC);
PROP_CONST(LwvidLwdecStrm, H264_CRTPATH_CABAC,      H264_CRTPATH_CABAC);
PROP_CONST(LwvidLwdecStrm, H265_IN_TO_TREE,         H265_IN_TO_TREE);
PROP_CONST(LwvidLwdecStrm, H265_OLD_TOWN_CROSS,     H265_OLD_TOWN_CROSS);
PROP_CONST(LwvidLwdecStrm, HMRExt_MAIN444_CTB32x32, HMRExt_MAIN444_CTB32x32);
PROP_CONST(LwvidLwdecStrm, HMRExt_MAIN444_CTB64x64, HMRExt_MAIN444_CTB64x64);
PROP_CONST(LwvidLwdecStrm, AV1_HD_10BIT,            AV1_HD_10BIT);
PROP_CONST(LwvidLwdecStrm, NUM_STREAMS,             NUM_STREAMS);

//---------------------------------------------------------------------------
// Utility functions
//---------------------------------------------------------------------------

template <typename T>
void LwvidLWDEC::EngineContext::SemiPlanarToPlanar
(
    const T *py,
    const T *puv,
    T *pdst,
    UINT32 width,
    UINT32 height,
    UINT32 pitch,
    lwdaVideoChromaFormat format
)
{
    UINT32 uvoffs;
    if (format == lwdaVideoChromaFormat_420)
    {
        uvoffs = (width / 2) * (height / 2);
    }
    else if (format == lwdaVideoChromaFormat_444)
    {
        uvoffs = width * height;
    }
    else
    {
        Printf(Tee::PriError, "unsupported chroma format\n");
        return;
    }

    // luma
    for (UINT32 y = 0; y < height; y++)
    {
        Platform::MemCopy(pdst, py, width * sizeof(T));
        py += pitch;
        pdst += width;
    }

    // De-interleave chroma
    if (format == lwdaVideoChromaFormat_420)
    {
        width >>= 1;
        height >>= 1;
    }

    for (UINT32 y = 0; y < height; y++)
    {
        for (UINT32 x = 0; x < width; x++)
        {
            pdst[x] = puv[x * 2];
            pdst[x + uvoffs] = puv[x * 2 + 1];
        }
        puv += pitch;
        pdst += width;
    }
}

void LwvidLWDEC::EngineContext::CopyPlane
(
    const unsigned char* pSrc,
    unsigned char* pDst,
    UINT32 width,
    UINT32 height,
    UINT32 pitch
)
{
    for (UINT32 y = 0; y < height; y++)
    {
        Platform::MemCopy(pDst, pSrc, width);
        pSrc += pitch;
        pDst += width;
    }
}

RC LwvidLWDEC::EngineContext::CopyOutputForGoldenGen
(
    const UINT08 * const src,
    UINT32 height,
    UINT32 width,
    UINT32 pitch,
    UINT32 outputHeight,
    lwdaVideoSurfaceFormat surfFormat,
    vector<UINT08> *pDestLumaData,
    vector<UINT08> *pDestChromaUData,
    vector<UINT08> *pDestChromaVData
)
{
    Tasker::DetachThread detach;
    RC rc;
    switch (surfFormat)
    {
        case lwdaVideoSurfaceFormat_YUV444:
        case lwdaVideoSurfaceFormat_YUV444_16Bit:
        {
            const UINT08 * const pU =  src + (outputHeight * pitch);
            const UINT08 * const pV =  src + (2 * outputHeight * pitch);
            if (m_pTestObj->m_Use2DSurfaceForGolden)
            {
                pDestLumaData->insert(pDestLumaData->end(), src, src + (height * pitch));
                pDestChromaUData->insert(pDestChromaUData->end(), pU, pU + (height * pitch));
                pDestChromaVData->insert(pDestChromaVData->end(), pV, pV + (height * pitch));
            }
            else
            {
                UINT32 surfSizeInBytes = height * width;

                if (surfFormat == lwdaVideoSurfaceFormat_YUV444_16Bit)
                {
                    width *= 2;
                    surfSizeInBytes *= 2;
                }
                pDestLumaData->resize(3 * surfSizeInBytes);
                UINT08 * dst = pDestLumaData->data();

                // Copy Luma
                for (UINT32 row = 0; row < height; ++row)
                {
                    std::copy(src + row * pitch, src + row * pitch + width, dst + row * width);
                }

                // Copy ChromaU
                dst += surfSizeInBytes;
                for (UINT32 row = 0; row < height; ++row)
                {
                    std::copy(pU + row * pitch, pU + row * pitch + width, dst + row * width);
                }

                // Copy ChromaV
                dst += surfSizeInBytes;
                for (UINT32 row = 0; row < height; ++row)
                {
                    std::copy(pV + row * pitch, pV + row * pitch + width, dst + row * width);
                }
            }
            break;
        }
        // YUV420 formats
        case lwdaVideoSurfaceFormat_LW12:
        case lwdaVideoSurfaceFormat_P016:
        {
            const UINT08 * const puv =  src + (outputHeight * pitch);
            if (m_pTestObj->m_Use2DSurfaceForGolden)
            {
                pDestLumaData->insert(pDestLumaData->end(), src, src + (height * pitch));
                pDestChromaUData->insert(pDestChromaUData->end(), puv, puv + (height/2 * pitch));
            }
            else
            {
                UINT32 lumaSizeInBytes = height * width;
                UINT32 chromaSizeInBytes = height/2 * width/2;

                if (surfFormat == lwdaVideoSurfaceFormat_P016)
                {
                    width = width * 2;
                    lumaSizeInBytes *= 2;
                    chromaSizeInBytes *= 2;
                }
                pDestLumaData->resize(lumaSizeInBytes + chromaSizeInBytes);
                UINT08 * dst = pDestLumaData->data();

                // Copy Luma
                for (UINT32 row = 0; row < height; ++row)
                {
                    std::copy(src + row * pitch, src + row * pitch + width, dst + row * width);
                }

                // Copy Chroma
                width /= 2;
                dst += lumaSizeInBytes;
                for (UINT32 row = 0; row < height / 2; ++row)
                {
                    std::copy(puv + row * pitch, puv + row * pitch + width, dst + row * width);
                }
            }
            break;
        }
        default:
            Printf(Tee::PriError, "Unknown output format!\n");
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC LwvidLWDEC::EngineContext::ColwertAndCopyOutput
(
    unsigned char *dest,
    unsigned char *src,
    UINT32 width,
    UINT32 height,
    UINT32 pitch,
    UINT32 outputHeight,
    UINT32 lumaSize,
    lwdaVideoSurfaceFormat surfFormat,
    lwdaVideoChromaFormat chromaFormat
)
{
    Tasker::DetachThread detach;

    RC rc;
    switch (surfFormat)
    {
        case lwdaVideoSurfaceFormat_YUV444:
            CopyPlane(src, dest, width, height, pitch);       // Y plane
            CopyPlane(src + outputHeight * pitch,
                      dest + lumaSize, width, height, pitch); // U plane
            CopyPlane(src + 2 * outputHeight * pitch,
                      dest + 2 * width * height, width,
                      height, pitch);                         // V plane
            break;
        case lwdaVideoSurfaceFormat_YUV444_16Bit:
            CopyPlane(src, dest, width * 2, height, pitch);   // Y plane
            CopyPlane(src + outputHeight * pitch,
                      dest + lumaSize, width * 2,
                      height, pitch);                         // U plane
            CopyPlane(src + 2 * outputHeight * pitch,
                      dest + 2 * lumaSize, width * 2,
                      height, pitch);                         // V plane
            break;
        case lwdaVideoSurfaceFormat_LW12:
            SemiPlanarToPlanar(src, src + outputHeight * pitch, dest,
                               width, height, pitch,
                               chromaFormat);
            break;
        case lwdaVideoSurfaceFormat_P016:
            SemiPlanarToPlanar((UINT16*)src,
                               (UINT16*)src + outputHeight * pitch / 2,
                               (UINT16*)dest,
                               width, height, pitch / 2,
                               chromaFormat);
            break;
        default:
           Printf(Tee::PriError, "Unknown output format!\n");
           return RC::SOFTWARE_ERROR;
    }
    return rc;
}

//---------------------------------------------------------------------------
// Callback functions
//---------------------------------------------------------------------------

void LwvidLWDEC::EngineContext::QueueEvent
(
    DecodeSession *state,
    UINT32 eventType,
    LWVIDPARSERDISPINFO* pDispInfo
)
{
    bool queued = false;
    do
    {
        {
            Tasker::MutexHolder lock(state->pMutex);
            if (state->numEventsInQueue < MAX_QUEUE_SIZE)
            {
                UINT32 iEntry = (state->queuePosition + state->numEventsInQueue) % MAX_QUEUE_SIZE;
                if (pDispInfo)
                {
                    state->displayQueue[iEntry] = *pDispInfo;
                }
                else
                {
                    memset(&state->displayQueue[iEntry], 0x0, sizeof(state->displayQueue[iEntry]));
                }
                state->eventType[iEntry] = eventType;
                state->numEventsInQueue++;
                queued = true;
            }
        }

        // Done
        if (queued)
        {
            break;
        }

        // Wait a bit
        Tasker::Sleep(1);
    } while (!state->decodeEos);
}

INT32 LWDAAPI LwvidLWDEC::EngineContext::HandleVideoData
(
    void *pvUserData,
    LWVIDSOURCEDATAPACKET *pPacket
)
{
    DecodeSession *state = static_cast<DecodeSession *>(pvUserData);
    bool decodeEOS = false;

    // Parser will call us back for decode & display within lwvidParseVideoData
    const LWresult result = LW_RUN(lwvidParseVideoData(state->lwParser, pPacket));

    // dump elementary stream
    if (state->fdElementary && pPacket->payload)
    {
        const size_t numBytes =fwrite(pPacket->payload, pPacket->payload_size,
            1, state->fdElementary);
        if (numBytes != 1)
        {
            Printf(Tee::PriError, "Failed to dump elementary stream!\n");
        }
    }

    if ((pPacket->flags & LWVID_PKT_ENDOFSTREAM) || (result != LWDA_SUCCESS))
    {
        if (result != LWDA_SUCCESS)
        {
            Printf(Tee::PriError, "lwvidParseVideoData (%d)\n", result);
        }
        state->decodeEos = decodeEOS = true;
    }

    return (!decodeEOS);
}

// Called when the decoder encounters a video format change (or initial sequence header)
// We already have the basic video format from the source, so not much to do here.
INT32 LWDAAPI LwvidLWDEC::EngineContext::HandleVideoSequence
(
    void *pvUserData,
    LWVIDEOFORMAT *pFormat
)
{
    EngineContext *ctx = static_cast<EngineContext*>(pvUserData);
    DecodeSession *state = &ctx->m_State;
    const FLOAT64 timeoutMs = static_cast<FLOAT64>(
        ctx->m_pTestObj->GetTestConfiguration()->TimeoutMs());
    if (!state->lwDecoder)
    {
        // Decoder is not created yet
        // update bitdepth and OutputFormat. webm or ivf header doesn't have bit depth info
        // so pick info from parsed first frame
        state->dci.bitDepthMinus8 = max(pFormat->bit_depth_luma_minus8,
                                        pFormat->bit_depth_chroma_minus8);
        if (pFormat->codec == lwdaVideoCodec_VP9)
        {
            Printf(Tee::PriLow, "\nbitdepth %lu", state->dci.bitDepthMinus8 + 8);
        }
        if (state->dci.ChromaFormat == lwdaVideoChromaFormat_420)
        {
            if (!ctx->m_pTestObj->GetDither() && state->dci.bitDepthMinus8 > 0)
            {
                state->dci.OutputFormat = lwdaVideoSurfaceFormat_P016;
            }
            else
            {
                state->dci.OutputFormat = lwdaVideoSurfaceFormat_LW12;
            }
        }
        else if (state->dci.ChromaFormat == lwdaVideoChromaFormat_444)
        {
            if (state->dci.bitDepthMinus8 > 0)
            {
                state->dci.OutputFormat = lwdaVideoSurfaceFormat_YUV444_16Bit;
            }
            else
            {
                state->dci.OutputFormat = lwdaVideoSurfaceFormat_YUV444;
            }
        }

        // Queue INIT event and wait
        QueueEvent(state, EVENT_INIT, nullptr);

        bool keepGoing = false;
        do
        {
            RC rc = Tasker::WaitOnEvent(state->pEventCommand, timeoutMs);
            keepGoing = (rc == RC::TIMEOUT_ERROR);
            if (rc != RC::OK && !keepGoing)
            {
                Printf(Tee::PriError, "WaitOnEvent failed\n");
                return 0;
            }
        } while (state->numEventsInQueue != 0 || keepGoing);
    }
    return 1;
}

// Called by the video parser to decode a single picture Since the parser
// will deliver data as fast as it can, we need to make sure that the picture
// index we're attempting to use for decode is no longer used for display
INT32 LWDAAPI LwvidLWDEC::EngineContext::HandlePictureDecode
(
    void *pvUserData,
    LWVIDPICPARAMS *pPicParams
)
{
    EngineContext *ctx = static_cast<EngineContext*>(pvUserData);
    DecodeSession *state = &ctx->m_State;

    LWVIDPICPARAMS *pOutPicParams = pPicParams;
    LWVIDPICPARAMS *pRefBaseParams = nullptr;

    if (state->dci.CodecType == lwdaVideoCodec_H264_SVC)
    {
        // if SVC, use target layer info for output pictures
        while (pOutPicParams->CodecSpecific.h264.svcext.pNextLayer)
        {
            pRefBaseParams = pOutPicParams->CodecSpecific.h264.svcext.bRefBaseLayer ?
                             pOutPicParams :
                             pRefBaseParams;
            pOutPicParams = pOutPicParams->CodecSpecific.h264.svcext.pNextLayer;
        }
        if (pRefBaseParams)
        {
            if ((pRefBaseParams->LwrrPicIdx < 0) ||
                (pRefBaseParams->LwrrPicIdx >= MAX_FRM_CNT))
            {
                Printf(Tee::PriError,
                    "Invalid picture index for ref base pic(%d)\n",
                    pRefBaseParams->LwrrPicIdx);
                return 0;
            }
            while ((state->frameInUse[pRefBaseParams->LwrrPicIdx]) &&
                   (!state->decodeEos))
            {
                // Decoder is getting too far ahead from display
                Tasker::Sleep(1);
            }
        }
    }

    if ((pOutPicParams->LwrrPicIdx < 0) ||
        (pOutPicParams->LwrrPicIdx >= MAX_FRM_CNT))
    {
        Printf(Tee::PriError,
            "Invalid picture index (%d)\n",
            pOutPicParams->LwrrPicIdx);
        return 0;
    }
    while ((state->frameInUse[pOutPicParams->LwrrPicIdx]) && (!state->decodeEos))
    {
        Tasker::Sleep(1);   // Decoder is getting too far ahead from display
    }
    if (!state->decodeEos)
    {
        if ((state->loopCnt > 1) && (!ctx->m_pTestObj->GetIsreconfig()))
            Printf(Tee::PriLow,
                "\rLoop count        %5d   decoding frame      %6d",
                state->loopCnt, state->decodePicCnt);

        if (state->dci.CodecType == lwdaVideoCodec_HEVC)
        {
            pPicParams->slice_decode_enable = state->sliceDecodeEnable;
            pPicParams->slice_merge_mode = state->sliceMergeMode;
        }

        const LWresult result = LW_RUN(lwvidDecodePicture(state->lwDecoder, pPicParams));
        if (result != LWDA_SUCCESS)
        {
            return 0;
        }

        state->frmStats[pOutPicParams->LwrrPicIdx].picNumInDecodeOrder = state->decodePicCnt++;
        state->frmStats[pOutPicParams->LwrrPicIdx].bytesInPicture =
            pOutPicParams->nBitstreamDataLen;

        if ((state->decoderOrder)
            && ((!pOutPicParams->field_pic_flag) || (pOutPicParams->second_field)))
        {
            LWVIDPARSERDISPINFO dispInfo;
            memset(&dispInfo, 0, sizeof(dispInfo));
            dispInfo.picture_index = pOutPicParams->LwrrPicIdx;
            dispInfo.progressive_frame = !pOutPicParams->field_pic_flag;
            dispInfo.top_field_first = pOutPicParams->bottom_field_flag ^ 1;
            HandlePictureDisplay(state, &dispInfo);
        }
        return (result == LWDA_SUCCESS);
    }
    else
    {
        return 0;   // flushing
    }
}

// Called by the video parser to display a video frame
// (in the case of field pictures, there may be 2 decode calls
// per 1 display call, since two fields make up one frame)
INT32 LWDAAPI LwvidLWDEC::EngineContext::HandlePictureDisplay
(
    void *pvUserData,
    LWVIDPARSERDISPINFO *pPicParams
)
{
    EngineContext *ctx = static_cast<EngineContext*>(pvUserData);
    DecodeSession *state = &ctx->m_State;

    // Mark the frame as 'in-use' so we don't re-use it for decoding
    // until it is no longer needed for display
    state->frameInUse[pPicParams->picture_index] = true;

    // Wait until we have a free entry in the display queue
    // (should never block if we have enough entries)
    QueueEvent(state, EVENT_DISPLAY, pPicParams);

    return 1;
}

LwvidLWDEC::LwvidLWDEC()
{
    CreateFancyPickerArray(FPK_LWDEC_GCX_NUM_PICKERS);
}

RC LwvidLWDEC::SetDefaultsForPicker(UINT32 idx)
{
    FancyPicker *fp = &(*GetFancyPickerArray())[idx];
    RC rc;

    switch (idx)
    {
        case FPK_LWDEC_GCX_ACTIVATE_BUBBLE:
            CHECK_RC(fp->ConfigRandom());
            // Using all cycles lwrrently
            fp->AddRandItem(100, true);
            fp->CompileRandom();
            break;
        case FPK_LWDEC_GCX_PWRWAIT_DELAY_MS:
            CHECK_RC(fp->ConfigRandom());
            fp->AddRandRange(80, 6, 25);
            fp->AddRandRange(20, 25, 100);
            fp->CompileRandom();
            break;
        default:
            MASSERT(!"Unknown picker");
            return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//---------------------------------------------------------------------------
// Test class function definitions
//---------------------------------------------------------------------------

RC LwvidLWDEC::Setup()
{
    RC rc;

    CHECK_RC(LwdaStreamTest::Setup());

    // Update the streams to user provided if available
    if (m_StreamsToTest.size() != 0)
    {
        m_StreamNames.clear();
        m_StreamNames = m_StreamsToTest;
        m_SkipStreamMask = 0x0;
    }
    else
    {
        m_StreamNames = 
        {
            "in_to_tree-001.264", // 4K UHD (2160p) resolution
            "sunflower-007.264",  // HD resolution CABAC
            "sunflower-008.264",  // HD resolution CAVLC
            "H264_Perf_world_of_warcraft_1280x720.yuv_HD15Mbps_30.264",
            "in_to_tree-002.265",
            "old-town-cross-001.265",
            "HMRExt_MAIN444_Perf_10b_AfterEarth_3840x2160_60frames.yuv_HD20Mbps_CTB32x32_8.bin",
            "HMRExt_MAIN444_Perf_10b_AfterEarth_3840x2160_60frames.yuv_HD20Mbps_CTB64x64_8.bin",
            // AV1
            "AV1_MAIN10_Perf_world_of_warcraft_1280x720_10bit_HD15Mbps_SB128x128_64_fg13_aq2_AllOn.ivf"
        };
    }

#if defined(DIRECTAMODEL)
    DirectAmodel::AmodelAttribute directAmodelKnobs[] = {
        { DirectAmodel::IDA_KNOB_STRING, 0 },
        { DirectAmodel::IDA_KNOB_STRING, reinterpret_cast<LwU64>(
            "ChipFE::argsForPlugInClassSim -im 0xC000 -dm 0xA000 -smax 0xC000 -fermi -ipm 0x00000c00 -cv 6 -sdk LwdecPlugin LwdecPlugin/lwdecFALCONOS ") }
    };
    if (GetBoundGpuDevice()->GetFamily() == GpuDevice::Ampere)
    {
        directAmodelKnobs[0].data = reinterpret_cast<LwU64>(
            "ChipFE::loadPlugInClassSim 1 ({ 0xC6B0 } LWDEC LwdecPlugin/pluginLwdec.dll )");
    }
    else if (GetBoundGpuDevice()->GetFamily() == GpuDevice::Hopper)
    {
        directAmodelKnobs[0].data = reinterpret_cast<LwU64>(
            "ChipFE::loadPlugInClassSim 1 ({ 0xB8B0 } LWDEC LwdecPlugin/pluginLwdec.dll )");
    }
    else // Ada:
    {
        bool is_h264_present = false;
        bool is_non_h264_present = false;
        for (size_t streamIdx = 0; streamIdx < m_StreamNames.size(); streamIdx++)
        {
            auto stream = m_StreamNames[streamIdx];
            if (m_SkipStreamMask & BIT(streamIdx))
                continue;
            
            if (stream.find(".264") != std::string::npos)   // Look for the streams which are H.264
            {
                is_h264_present = true;
            }
            else
            {
                is_non_h264_present = true;
            }
        }
        if (is_h264_present && is_non_h264_present) // If H.264 and non H.264 streams are present then give error
        {
            Printf(Tee::PriError, "Invalid Streams combination given, "
                   "Either select only H.264 streams or select non H.264 streams\n");
            return RC::SOFTWARE_ERROR;
        }
        else if (is_h264_present)   // Load legacy dll for H.264 support
        {
            directAmodelKnobs[0].data = reinterpret_cast<LwU64>(
                "ChipFE::loadPlugInClassSim 1 ({ 0xC9B0 } LWDEC LwdecPlugin/pluginLwdecLegacy.dll )");
        }
        else    // Load normal dll for non H.264 support. Lwrrently it supports HEVC(H.265), VP9, AV1
        {
            directAmodelKnobs[0].data = reinterpret_cast<LwU64>(
                "ChipFE::loadPlugInClassSim 1 ({ 0xC9B0 } LWDEC LwdecPlugin/pluginLwdec.dll )");
        }
    }
    dglDirectAmodelSetAttributes(directAmodelKnobs, static_cast<LwU32>(NUMELEMS(directAmodelKnobs)));
#endif

    CHECK_RC(CreateStreamFiles());

    if (!m_Use2DSurfaceForGolden)
    {
        CHECK_RC(m_OnedGoldenSurfaces.SetSurface("decodedstream", nullptr, 0));
        CHECK_RC(GetGoldelwalues()->SetSurfaces(&m_OnedGoldenSurfaces));
        GetGoldelwalues()->SetCheckCRCsUnique(true);
    }
    else
    {
        m_pGoldenSurfaces = make_unique<GpuGoldenSurfaces>(GetBoundGpuDevice());
    }
    m_GoldenMutex = Tasker::CreateMutex("LWVIDLWDEC GV",
                                        Tasker::mtxUnchecked);

    m_PrintProgressMutex = Tasker::AllocMutex("Progress Mutex",
                                          Tasker::mtxUnchecked);
    if (m_DoRTD3Cycles)
    {
        m_GCxBubble = make_unique<GCxBubble>(GetBoundGpuSubdevice());
        m_GCxBubble->SetGCxParams(
                GetBoundRmClient(),
                GetBoundGpuDevice()->GetDisplay(),
                GetDisplayMgrTestContext(),
                m_ForceGCxPstate,
                GetTestConfiguration()->Verbose(),
                GetTestConfiguration()->TimeoutMs(),
                GCxPwrWait::GCxModeRTD3);
        m_GCxBubble->SetPciCfgRestoreDelayMs(m_PciConfigSpaceRestoreDelayMs);
        CHECK_RC(m_GCxBubble->Setup());
    }

    return rc;
}

RC LwvidLWDEC::Run()
{
    // lwlwvid library assumes the context is pushed onto the stack
    // whereas, the existing lwca wrapper doesn't, so manually push it
    LWcontext primaryCtx = GetLwdaInstance().GetHandle(GetBoundLwdaDevice());
    m_PerpetualRun = m_KeepRunning;
    m_StartTimeMs = Platform::GetTimeMS();
    m_NumStreamsDecoded = 0;
    DEFER
    {
        lwCtxPopLwrrent(&primaryCtx);
    };

    const LWresult result = lwCtxPushLwrrent(primaryCtx);
    if (result != LWDA_SUCCESS)
    {
        Lwca::PrintResult("lwCtxPushLwrrent", result);
        return Lwca::LwdaErrorToRC(result);
    }

    StickyRC rc = InnerRun();
    rc = GetGoldelwalues()->ErrorRateTest(GetJSObject());

    return rc;
}

RC LwvidLWDEC::Cleanup()
{
    StickyRC rc;

    Goldelwalues *pGoldelwalues = GetGoldelwalues();
    pGoldelwalues->ClearSurfaces();
    m_pGoldenSurfaces.reset();
    Tasker::FreeMutex(m_PrintProgressMutex);
    if (m_DoRTD3Cycles)
    {
        m_GCxBubble->PrintStats();
        rc = m_GCxBubble->Cleanup();
        m_GCxBubble.reset();
    }

    rc = LwdaStreamTest::Cleanup();
    return rc;
}

void LwvidLWDEC::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "LwvidLWDEC test Js Properties:\n");
    Printf(pri, "\tGenYuvOutput:\t\t\t%s\n", m_GenYuvOutput ? "true" : "false");
    Printf(pri, "\tUseLoadBalancing:\t\t%s\n", m_UseLoadBalancing ? "true" : "false");
    Printf(pri, "\tSkipEngineMask:\t\t\t0x%0x\n", m_SkipEngineMask);
    Printf(pri, "\tSkipStreamMask:\t\t\t0x%0x\n", m_SkipStreamMask);
    Printf(pri, "\tKeepRunning:\t\t\t%s\n", m_KeepRunning ? "true" : "false");
    Printf(pri, "\tRuntimeMs:\t\t\t%u\n", m_RuntimeMs);
    Printf(pri, "\tUse2DSurfaceForGolden:\t\t%s\n", m_Use2DSurfaceForGolden? "true" : "false");
    string streams;
    for_each(m_StreamsToTest.begin(), m_StreamsToTest.end(), [&](const string &piece)
    {
        if (streams.length() == 0)
        {
            streams = piece;
        }
        else
        {
            streams = streams + ", " + piece;
        }
    });
    Printf(pri, "\tStreamsToTest:\t\t\t%s\n", streams.c_str());
}

RC LwvidLWDEC::InitFromJs()
{
    RC rc;
    const JavaScriptPtr pJs;
    CHECK_RC(LwdaStreamTest::InitFromJs());

    rc = pJs->GetProperty(GetJSObject(), "StreamsToTest", &m_StreamsToTest);
    if (rc != RC::OK && rc != RC::ILWALID_OBJECT_PROPERTY)
    {
        return rc;
    }
    // If the arg "StreamsToTest" is not passed then GetProperty will return
    // "RC::ILWALID_OBJECT_PROPERTY" by default, clearing the rc here.
    rc.Clear();

    return rc;
}

bool LwvidLWDEC::IsSupported()
{
#if defined(LWCPU_AARCH64)
    // From ausvrl8650(tu104_pg189_0600):
    // "ERROR: lwvidCreateDecoder returned error 2 (LWDA_ERROR_OUT_OF_MEMORY: out of memory)"
    Printf(Tee::PriLow, "LwvidLWDEC is not supported on aarch64\n");
    return false;
#else
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(Tee::PriLow, "LwvidLWDEC is not supported on SOC devices\n");
        return false;
    }

    if (GetBoundGpuDevice()->GetFamily() < GpuDevice::Volta)
    {
        Printf(Tee::PriLow, "LwvidLWDEC is not supported on pre-Volta devices\n");
        return false;
    }

    if (m_DoRTD3Cycles)
    {
        GCxBubble gcxBubble(GetBoundGpuSubdevice());
        if (!gcxBubble.IsGCxSupported(GCxPwrWait::GCxModeRTD3, m_ForceGCxPstate))
        {
            Printf(Tee::PriLow, "LwvidLWDEC RTD3 is not supported on this platform\n");
            return false;
        }
    }

    LwdecAlloc lwdecAlloc;
    const bool supported = lwdecAlloc.IsSupported(GetBoundGpuDevice());
    if (!supported)
    {
        Printf(Tee::PriLow, "LwvidLWDEC has not found supported LWDEC hw class\n");
    }
    return supported;
#endif
}

RC LwvidLWDEC::CreateStreamFiles()
{
    static bool s_StreamFilesCreated = false;

    if (s_StreamFilesCreated)
    {
        return RC::OK;
    }

    RC rc;
    TarArchive tarArchive;
    bool doesLwdecBinExists = false;

    // Load the tarball that contains the video streams
    const string tarballFilePath = Utility::DefaultFindFile("lwdec.bin", true);
    if (tarArchive.ReadFromFile(tarballFilePath))
    {
        doesLwdecBinExists = true;
    }
    for (auto stream : m_StreamNames)
    {
        // TODO: Check if lwlwvid implementation can be modified for this.
        // Right now, lwlwvid requires individual trace files to be present
        // in the same folder (Jira: MODSAMPERE-653)

        // Lwvid library expects the file to be in same dir as mods
        const string streamName = Utility::DefaultFindFile(stream.c_str(), false);

        if (Xp::DoesFileExist(streamName))
        {
            continue;
        }
        else if (!doesLwdecBinExists)
        {
            Printf(Tee::PriError, "Stream: %s and lwdec.bin does not exist "
                "in same dir as mods\n", stream.c_str());
            return RC::FILE_DOES_NOT_EXIST;
        }

        // Read the stream data from the tar archive
        TarFile *pTarFile = tarArchive.Find(stream);
        if (pTarFile == nullptr)
        {
            Printf(Tee::PriError, "Stream %s does not exists in same dir as mods "
                "and also in lwdec.bin\n", stream.c_str());
            return RC::FILE_READ_ERROR;
        }
        const unsigned streamSize = pTarFile->GetSize();
        vector<LwU8> streamData(streamSize);
        pTarFile->Seek(0);
        if (pTarFile->Read(reinterpret_cast<char*>(&streamData[0]), streamSize) !=
            streamSize)
        {
            Printf(Tee::PriError, "File read error for %s\n", stream.c_str());
            return RC::FILE_READ_ERROR;
        }

        FileHolder file;
        if (RC::OK != file.Open(stream, "wb"))
        {
            Printf(Tee::PriError, "Can't Create File: %s\n", stream.c_str());
            return RC::CANNOT_OPEN_FILE;
        }
        if (streamSize != fwrite(&streamData[0], 1, streamSize, file.GetFile()))
        {
            Printf(Tee::PriError, "Can't Write File %s\n", stream.c_str());
            return RC::FILE_WRITE_ERROR;
        }
    }

    s_StreamFilesCreated = true;

    return rc;
}

RC LwvidLWDEC::InnerRun()
{
    StickyRC rc;

    vector<UINT32> engines;
    CHECK_RC(GetBoundGpuDevice()->GetEngines(&engines));

    UINT32 presentEngineMask = 0;
    for (auto engine : engines)
    {
        if (LW2080_ENGINE_TYPE_IS_LWDEC(engine))
        {
            presentEngineMask |= 1 << LW2080_ENGINE_TYPE_LWDEC_IDX(engine);
        }
    }

    const UINT32 runEngineMask = presentEngineMask & ~m_SkipEngineMask;
    if (runEngineMask == 0)
    {
        Printf(Tee::PriError,
            "Found no engines to test with the skip mask = 0x%08x!\n",
            m_SkipEngineMask);
        return RC::DEVICE_NOT_FOUND;
    }

    UINT32 numEngines = Utility::CountBits(runEngineMask);
    MASSERT(numEngines <= LW2080_ENGINE_TYPE_LWDEC_SIZE);

    if (m_UseLoadBalancing)
    {
        numEngines = 1;
    }
    m_MaxNumStreamsDecoded = m_StreamNames.size() * numEngines;
    if (m_RuntimeMs != 0)
    {
        PrintProgressInit(m_RuntimeMs, "Test Configuration RuntimeMs");
    }
    else
    {
        PrintProgressInit(m_MaxNumStreamsDecoded, "Num of Engines * Num of Streams");
    }

    Tasker::ThreadID engineThreads[LW2080_ENGINE_TYPE_LWDEC_SIZE];
    EngineParameters engineParameters[LW2080_ENGINE_TYPE_LWDEC_SIZE];

    INT32 nextRunEngineIndex = -1;

    for (UINT32 engIndex = 0; engIndex < numEngines; engIndex++)
    {
        if (m_UseLoadBalancing)
        {
            engineParameters[engIndex].lwdelwsageMode = USE_LOADBALANCE;
        }
        else
        {
            nextRunEngineIndex = Utility::BitScanForward(
                runEngineMask, nextRunEngineIndex + 1);
            MASSERT(nextRunEngineIndex >= 0);
            engineParameters[engIndex].lwdelwsageMode = USE_LWDEC0 + nextRunEngineIndex;
        }

        engineParameters[engIndex].pTestObj = this;
        if (m_DoRTD3Cycles)
        {
            engineParameters[engIndex].bubbleParams.pGolden = GetGoldelwalues();
            engineParameters[engIndex].bubbleParams.pBubble = m_GCxBubble.get();
            engineParameters[engIndex].bubbleParams.pFpDelay =
                &(*GetFancyPickerArray())[FPK_LWDEC_GCX_PWRWAIT_DELAY_MS];
        }

        engineThreads[engIndex] = Tasker::CreateThread(EngineThread,
            &engineParameters[engIndex],
            Tasker::MIN_STACK_SIZE,
            "EngineThread");

        if (engineThreads[engIndex] == Tasker::NULL_THREAD)
        {
            Printf(Tee::PriError, "Unable to create EngineThread idx = %d\n", engIndex);
            rc = RC::SOFTWARE_ERROR;
        }
    }

    for (UINT32 engIndex = 0; engIndex < numEngines; engIndex++)
    {
        if (engineThreads[engIndex] != Tasker::NULL_THREAD)
        {
            // The default timeout is 1 second. Give each stream 3 seconds:
            constexpr UINT32 streamTimeoutSec = 3;
            const RC joinRC = Tasker::Join(engineThreads[engIndex],
                streamTimeoutSec * m_StreamNames.size() * GetTestConfiguration()->TimeoutMs());
            if (joinRC != RC::OK)
            {
                Printf(Tee::PriError, "Tasker::Join failure %s in engineIdx %d\n",
                    joinRC.Message(), engIndex);
                rc = joinRC;
            }
            if (engineParameters[engIndex].engineRc != RC::OK)
            {
                Printf(Tee::PriError, "%s in engineIdx %d\n",\
                    engineParameters[engIndex].engineRc.Message(), engIndex);
                rc = engineParameters[engIndex].engineRc;
            }
        }
    }

    return rc;
}

void LwvidLWDEC::EngineThread
(
    void *pEngineParameters
)
{
    EngineParameters *pEP = static_cast<EngineParameters*>(pEngineParameters);

    LWcontext primaryCtx = pEP->pTestObj->GetLwdaInstance().GetHandle(
        pEP->pTestObj->GetBoundLwdaDevice());

    const LWresult result = lwCtxPushLwrrent(primaryCtx);
    if (result != LWDA_SUCCESS)
    {
        Lwca::PrintResult("lwCtxPushLwrrent", result);
        pEP->engineRc = Lwca::LwdaErrorToRC(result);
        return;
    }

    DEFER
    {
        lwCtxPopLwrrent(&primaryCtx);
    };

    pEP->engineRc = pEP->pTestObj->DecodeStreams(pEP);
}

RC LwvidLWDEC::DecodeStreams(EngineParameters *pEP)
{
    RC rc;

    EngineContext engineContext(pEP);
    CHECK_RC(engineContext.Init());

    auto const& runtimeMs = pEP->pTestObj->m_RuntimeMs;
    auto GetElapsedTime = [&]() -> UINT64
    {
        return (Platform::GetTimeMS() - pEP->pTestObj->m_StartTimeMs);
    };

    do
    {
        for (size_t streamIdx = 0; streamIdx < m_StreamNames.size(); streamIdx++)
        {
            // TODO evaluate if there is a way to stop decoding in the middle of stream
            if ((pEP->pTestObj->m_PerpetualRun && !pEP->pTestObj->m_KeepRunning) ||
                (runtimeMs && (GetElapsedTime() >= runtimeMs)))
            {
                break;
            }
            CHECK_RC(Abort::Check());

            engineContext.SetStreamIdx(static_cast<UINT32>(streamIdx));
            if (!(pEP->pTestObj->m_SkipStreamMask & BIT(streamIdx)))
            {
                auto stream = m_StreamNames[streamIdx];
                CHECK_RC(engineContext.DecodeStream(stream.c_str()));
                Tasker::AcquireMutex(m_PrintProgressMutex);
                if (pEP->pTestObj->m_RuntimeMs != 0)
                {
                    PrintProgressUpdate(min(GetElapsedTime(), static_cast<UINT64>(runtimeMs)));
                }
                else
                {
                    ++m_NumStreamsDecoded;
                    PrintProgressUpdate(min(m_NumStreamsDecoded, m_MaxNumStreamsDecoded));
                }
                Tasker::ReleaseMutex(m_PrintProgressMutex);
                if (pEP->bubbleParams.pBubble)
                {
                    if (pEP->bubbleParams.pGolden->GetAction() != Goldelwalues::Store)
                    {
                        pEP->engineRc = pEP->bubbleParams.pBubble->ActivateBubble(
                            pEP->bubbleParams.pFpDelay->Pick());
                    }
                }
            }
        }
    } while (pEP->pTestObj->m_KeepRunning ||
            (runtimeMs && (GetElapsedTime() < runtimeMs)));

    return rc;
}

string LwvidLWDEC::EngineContext::LwdelwsageModeToStr
(
    unsigned long lwdelwsageMode
)
{
    if (lwdelwsageMode == USE_LOADBALANCE)
        return "LB";

    return Utility::StrPrintf("Engine%u", static_cast<unsigned int>(lwdelwsageMode - USE_LWDEC0));
}

RC LwvidLWDEC::EngineContext::ValidateVideoStream
(
    LWvideosource   *lwVidSrc,
    LWVIDEOFORMATEX *vidFormat
) const
{
    RC rc;

    // Get video format related information from the stream
    LWVIDEOFORMAT &videoFormat = vidFormat->format;
    LWresult result = LW_RUN(lwvidGetSourceVideoFormat(*lwVidSrc,
                                              &videoFormat,
                                              LWVID_FMT_EXTFORMATINFO));
    CHECK_RC(Lwca::LwdaErrorToRC(result));

    // Get decoder caps
    LWVIDDECODECAPS decodecaps;
    memset(&decodecaps, 0, sizeof(decodecaps));
    decodecaps.eCodecType = videoFormat.codec;
    decodecaps.eChromaFormat = videoFormat.chroma_format;
    decodecaps.nBitDepthMinus8 = videoFormat.bit_depth_luma_minus8;
    result = LW_RUN(lwvidGetDecoderCaps(&decodecaps));
    CHECK_RC(Lwca::LwdaErrorToRC(result));

    if (!decodecaps.bIsSupported)
    {
        Printf(Tee::PriError, "Codec not supported on this GPU\n");
        return RC::LWDA_ERROR;
    }

    if ((videoFormat.coded_width > decodecaps.nMaxWidth) ||
        (videoFormat.coded_height > decodecaps.nMaxHeight) ||
        ((videoFormat.coded_width >> 4) *
         (videoFormat.coded_height >> 4) > decodecaps.nMaxMBCount))
    {
        Printf(Tee::PriError,
            "Resolution/MBcount:          (%dx%d, %d) not supported on this GPU \n",
            videoFormat.coded_width,
            videoFormat.coded_height,
            (videoFormat.coded_width >> 4) * (videoFormat.coded_height >> 4));
        Printf(Tee::PriError,
            "Max Supported (wxh, mbcnt):  (%dx%d, %d)\n",
            decodecaps.nMaxWidth,
            decodecaps.nMaxHeight,
            decodecaps.nMaxMBCount);
        return RC::LWDA_ERROR;
    }

    return rc;
}

RC LwvidLWDEC::EngineContext::InitDecodeSession
(
    LWVIDEOFORMATEX *vidFormat,
    const char      *argInput
)
{
    RC rc;

    DecodeSession *state = &m_State;

    // Init the object
    *state = { };
    state->pEventCommand = Tasker::AllocEvent("queue event for init", false);
    state->pMutex = Tasker::CreateMutex("state mutex", Tasker::mtxUnchecked);
    state->loopCnt = 1;
    state->reconfigReqd = false;

    // Create video source from the input stream
    LWVIDSOURCEPARAMS srcInitParams;
    memset(&srcInitParams, 0, sizeof(srcInitParams));
    srcInitParams.pUserData = state;
    srcInitParams.pflwideoDataHandler = HandleVideoData;
    srcInitParams.pfnAudioDataHandler = nullptr;
    LWresult result = LW_RUN(lwvidCreateVideoSource(&state->lwVidSrc, argInput, &srcInitParams));
    CHECK_RC(Lwca::LwdaErrorToRC(result));

    // Make sure it has a valid video stream
    memset(vidFormat, 0, sizeof(*vidFormat));
    CHECK_RC(ValidateVideoStream(&state->lwVidSrc, vidFormat));

    // Fillup dci info for non-raw_input
    // Decoder will be created after first frame parse
    LWVIDEOFORMAT &videoFormat = vidFormat->format;
    state->dci.CodecType = videoFormat.codec;
    state->dci.bitDepthMinus8 = max(videoFormat.bit_depth_luma_minus8,
                                    videoFormat.bit_depth_chroma_minus8);
    state->dci.ulWidth = videoFormat.coded_width;
    state->dci.ulHeight = videoFormat.coded_height;
    state->dci.ulMaxWidth = static_cast<UINT64>(state->dci.ulWidth);
    state->dci.ulMaxHeight = static_cast<UINT64>(state->dci.ulHeight);

    // No scaling
    state->dci.ulTargetWidth = state->dci.ulWidth;
    state->dci.ulTargetHeight = state->dci.ulHeight;

    state->dci.ulNumDecodeSurfaces = 8;
    if ((state->dci.CodecType == lwdaVideoCodec_H264) ||
        (state->dci.CodecType == lwdaVideoCodec_H264_SVC) ||
        (state->dci.CodecType == lwdaVideoCodec_H264_MVC))
    {
        // Assume worst-case of 20 decode surfaces for H264
        state->dci.ulNumDecodeSurfaces = MAX_FRM_CNT;
    }

    if (state->dci.CodecType == lwdaVideoCodec_VP9)
        state->dci.ulNumDecodeSurfaces = 12;

    if (state->dci.CodecType == lwdaVideoCodec_AV1)
         state->dci.ulNumDecodeSurfaces = 18;

    if (state->dci.CodecType == lwdaVideoCodec_HEVC)
    {
        // ref HEVC spec: A.4.1 General tier and level limits
        INT32 maxLumaPS = 35651584; // lwrrently assuming level 6.2, 8Kx4K
        INT32 maxDpbPicBuf = 6;
        INT32 picSizeInSamplesY = state->dci.ulWidth * state->dci.ulHeight;
        INT32 maxDpbSize = 0;
        if (picSizeInSamplesY <= (maxLumaPS >> 2))
        {
            maxDpbSize = maxDpbPicBuf * 4;
        }
        else if (picSizeInSamplesY <= (maxLumaPS >> 1))
        {
            maxDpbSize = maxDpbPicBuf * 2;
        }
        else if (picSizeInSamplesY <= ((3 * maxLumaPS) >> 2))
        {
            maxDpbSize = (maxDpbPicBuf * 4) / 3;
        }
        else
        {
            maxDpbSize = maxDpbPicBuf;
        }
        maxDpbSize = min(maxDpbSize, 16);
        state->dci.ulNumDecodeSurfaces = maxDpbSize + 4;
    }

    state->dci.ulCreationFlags = 0;
    state->dci.ChromaFormat = videoFormat.chroma_format;
    state->dci.DeinterlaceMode = videoFormat.progressive_sequence ?
         lwdaVideoDeinterlaceMode_Weave :
         lwdaVideoDeinterlaceMode_Adaptive;
    state->dci.ulIntraDecodeOnly = 0;

    // We won't simultaneously map more than 2 surfaces
    state->dci.ulNumOutputSurfaces = 2;

    state->dci.lwdelwsageMode = m_LwdelwsageMode;

    // Create video parser
    LWVIDPARSERPARAMS parserInitParams;
    memset(&parserInitParams, 0, sizeof(parserInitParams));
    parserInitParams.CodecType = videoFormat.codec;
    parserInitParams.ulMaxNumDecodeSurfaces = state->dci.ulNumDecodeSurfaces;
    parserInitParams.ulMaxDisplayDelay = 2;
    parserInitParams.pUserData = this;
    parserInitParams.pfnSequenceCallback = HandleVideoSequence;
    parserInitParams.pfnDecodePicture = HandlePictureDecode;
    parserInitParams.pfnDisplayPicture = (state->decoderOrder) ? nullptr : HandlePictureDisplay;
    parserInitParams.pExtVideoInfo = vidFormat;
    parserInitParams.ulErrorThreshold = 0;
    result = LW_RUN(lwvidCreateVideoParser(&(state->lwParser), &parserInitParams));
    CHECK_RC(Lwca::LwdaErrorToRC(result));

    return rc;
}

bool LwvidLWDEC::EngineContext::IsGoldenQueueEmpty() const
{
    Tasker::MutexHolder lock(m_GoldenQueueMutex);
    return m_GoldenQueue.empty();
}

void LwvidLWDEC::EngineContext::GoldenThread
(
    void *engineContext
)
{
    EngineContext *ctx = static_cast<EngineContext*>(engineContext);

    while (ctx->m_DecodeInProgress || !ctx->IsGoldenQueueEmpty())
    {
        Tasker::AcquireSemaphore(ctx->m_GoldenSema, Tasker::NO_TIMEOUT);
        if (!ctx->IsGoldenQueueEmpty())
        {
            std::unique_ptr<PerFrameGoldenParams> lwrr;
            {
                Tasker::MutexHolder lock(ctx->m_GoldenQueueMutex);
                lwrr = std::move(ctx->m_GoldenQueue.front());
                ctx->m_GoldenQueue.pop();
            }
            ctx->GoldensPerFrame(lwrr.get());
        }
    }
}

RC LwvidLWDEC::EngineContext::CopyOutputToSurfaces
(
    PerFrameGoldenParams *params
)
{
    RC rc;
    switch (params->chromaFormat)
    {
        case lwdaVideoChromaFormat_420:
        {
            if (params->outputFormat == lwdaVideoSurfaceFormat_LW12)
            {
                m_lumaColorFormat = ColorUtils::Y8;
                m_chromaUColorFormat = ColorUtils::U8V8;
                m_chromaVColorFormat = ColorUtils::LWFMT_NONE;
            }
            else if (params->outputFormat == lwdaVideoSurfaceFormat_P016)
            {
                m_lumaColorFormat = ColorUtils::VOID16;
                m_chromaUColorFormat = ColorUtils::R16_G16;
                m_chromaVColorFormat = ColorUtils::LWFMT_NONE;
            }
            else
            {
                Printf(Tee::PriError, "Unknown output format!\n");
                return RC::SOFTWARE_ERROR;
            }
            break;
        }
        case lwdaVideoChromaFormat_444:
        {
            if (params->outputFormat == lwdaVideoSurfaceFormat_YUV444_16Bit)
            {
                m_lumaColorFormat = ColorUtils::VOID16;
                m_chromaUColorFormat = ColorUtils::VOID16;
                m_chromaVColorFormat = ColorUtils::VOID16;
            }
            else if(params->outputFormat == lwdaVideoSurfaceFormat_YUV444)
            {
                m_lumaColorFormat = ColorUtils::Y8;
                m_chromaUColorFormat = ColorUtils::U8;
                m_chromaVColorFormat = ColorUtils::V8;
            }
            else
            {
                Printf(Tee::PriError, "Unknown output format!\n");
                return RC::SOFTWARE_ERROR;
            }
            break;
        }
        default:
           Printf(Tee::PriError, "Unknown chroma format!\n");
           return RC::SOFTWARE_ERROR;
    }

    if (!m_LumaSurf.IsAllocated() ||
        (m_LumaSurf.GetWidth() != params->width) ||
        (m_LumaSurf.GetHeight() != params->height) ||
        (m_LumaSurf.GetPitch() != params->pitch) ||
        (m_LumaSurf.GetColorFormat() != m_lumaColorFormat))
    {
        m_LumaWriter.Unmap();
        // Create a luma surface for the current frame
        m_LumaSurf.Free();
        m_LumaSurf.SetWidth(params->width);
        m_LumaSurf.SetHeight(params->height);
        m_LumaSurf.SetPitch(params->pitch);
        m_LumaSurf.SetColorFormat(m_lumaColorFormat);
        m_LumaSurf.SetLocation(Memory::Coherent);
        CHECK_RC(m_LumaSurf.Alloc(m_pTestObj->GetBoundGpuDevice()));
    }

    // Write the data onto the surface
    for (UINT32 row = 0; row < static_cast<UINT32>(params->height); ++row)
    {
        CHECK_RC(m_LumaWriter.WriteBytes(row * m_LumaSurf.GetPitch(),
                                   &params->lumaData[row * params->pitch],
                                   params->pitch));
    }

    // Create a chroma surface for the current frame
    FLOAT32 multiFactor = 1.0;
    if (params->chromaFormat == lwdaVideoChromaFormat_420)
    {
        multiFactor = 0.5;
    }
    const UINT32 chromaWidth = UNSIGNED_CAST(UINT32, params->width * multiFactor);
    const UINT32 chromaHeight = UNSIGNED_CAST(UINT32, params->height * multiFactor);

    if (!m_ChromaUSurf.IsAllocated() ||
        (m_ChromaUSurf.GetWidth() != chromaWidth) ||
        (m_ChromaUSurf.GetHeight() != chromaHeight) ||
        (m_ChromaUSurf.GetPitch() != params->pitch) ||
        (m_ChromaUSurf.GetColorFormat() != m_chromaUColorFormat))
    {
        m_ChromaUWriter.Unmap();
        m_ChromaUSurf.Free();
        m_ChromaUSurf.SetWidth(chromaWidth);
        m_ChromaUSurf.SetHeight(chromaHeight);
        m_ChromaUSurf.SetPitch(params->pitch);
        m_ChromaUSurf.SetColorFormat(m_chromaUColorFormat);
        m_ChromaUSurf.SetLocation(Memory::Coherent);
        CHECK_RC(m_ChromaUSurf.Alloc(m_pTestObj->GetBoundGpuDevice()));
    }

    for (UINT32 row = 0; row < chromaHeight; ++row)
    {
        CHECK_RC(m_ChromaUWriter.WriteBytes(row * m_ChromaUSurf.GetPitch(),
                                   &params->chromaUData[row * params->pitch],
                                   params->pitch));
    }

    if (m_chromaVColorFormat != ColorUtils::LWFMT_NONE)
    {
        if (!m_ChromaVSurf.IsAllocated() ||
                (m_ChromaVSurf.GetWidth() != chromaWidth) ||
                (m_ChromaVSurf.GetHeight() != chromaHeight) ||
                (m_ChromaVSurf.GetPitch() != params->pitch) ||
                (m_ChromaVSurf.GetColorFormat() != m_chromaVColorFormat))
            {
                m_ChromaVWriter.Unmap();
                m_ChromaVSurf.Free();
                m_ChromaVSurf.SetWidth(chromaWidth);
                m_ChromaVSurf.SetHeight(chromaHeight);
                m_ChromaVSurf.SetPitch(params->pitch);
                m_ChromaVSurf.SetColorFormat(m_chromaVColorFormat);
                m_ChromaVSurf.SetLocation(Memory::Coherent);
                CHECK_RC(m_ChromaVSurf.Alloc(m_pTestObj->GetBoundGpuDevice()));
            }

            for (UINT32 row = 0; row < chromaHeight; ++row)
            {
                CHECK_RC(m_ChromaVWriter.WriteBytes(row * m_ChromaVSurf.GetPitch(),
                                           &params->chromaVData[row * params->pitch],
                                           params->pitch));
            }
    }
    else
    {
        m_ChromaVWriter.Unmap();
        m_ChromaVSurf.Free();
    }
    return rc;
}

RC LwvidLWDEC::EngineContext::GoldensPerFrame
(
    PerFrameGoldenParams *params
)
{
    RC rc;
    DEFER
    {
        if (rc != RC::OK)
        {
            Printf(Tee::PriError, "%s in golden frame idx %d !\n",
                rc.Message(), params->goldenFrameIdx);
        }
        m_GoldenRc = rc;
    };

    if (params == nullptr)
    {
        MASSERT(params);
        Printf(Tee::PriError, "Invalid parameters passed for goldens!\n");
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    {
        Tasker::DetachThread detach;
        if (m_pTestObj->m_Use2DSurfaceForGolden)
        {
            CHECK_RC(CopyOutputToSurfaces(params));
        }
    }

    // Replace the golden surface with this one
    Tasker::MutexHolder lock(m_pTestObj->m_GoldenMutex);
    if (m_pTestObj->m_Use2DSurfaceForGolden)
    {
        m_pTestObj->m_pGoldenSurfaces->ClearSurfaces();
        m_pTestObj->m_pGoldenSurfaces->AttachSurface(&m_LumaSurf, "L", 0);
        m_pTestObj->m_pGoldenSurfaces->AttachSurface(&m_ChromaUSurf, "Lw", 0);
        if (m_chromaVColorFormat != ColorUtils::LWFMT_NONE)
            m_pTestObj->m_pGoldenSurfaces->AttachSurface(&m_ChromaVSurf, "Cv", 0);
        CHECK_RC(m_pTestObj->GetGoldelwalues()->SetSurfaces(m_pTestObj->m_pGoldenSurfaces.get()));
    }
    else
    {
        CHECK_RC(m_pTestObj->m_OnedGoldenSurfaces.SetSurface("decodedstream",
                    &params->lumaData[0],
                    params->lumaData.size()));
    }
    m_pTestObj->GetGoldelwalues()->SetLoop(params->goldenFrameIdx);
    CHECK_RC(m_pTestObj->GetGoldelwalues()->Run());

    // This flag makes sure that some testing was done on each engine
    m_TestedSomething = true;

    return rc;
}

LwvidLWDEC::EngineContext::EngineContext(EngineParameters *pEP)
: m_pTestObj(pEP->pTestObj)
, m_LwdelwsageMode(pEP->lwdelwsageMode)
, m_pEngineRc(&pEP->engineRc)
, m_LumaWriter(m_LumaSurf)
, m_ChromaUWriter(m_ChromaUSurf)
, m_ChromaVWriter(m_ChromaVSurf)
{
    m_State = { };
    // The surfaces are always in the system memory - no need to use a small window:
    m_LumaWriter.SetMaxMappedChunkSize(0x80000000);
    m_ChromaUWriter.SetMaxMappedChunkSize(0x80000000);
    m_ChromaVWriter.SetMaxMappedChunkSize(0x80000000);
}

RC LwvidLWDEC::EngineContext::Init()
{
    m_StreamIdx = 0;

    m_GoldenQueueMutex = Tasker::CreateMutex("LWVIDLWDEC GQ",
        Tasker::mtxUnchecked);

    m_GoldenSema = Tasker::CreateSemaphore(0, "GoldenSema");
    if (m_GoldenSema == nullptr)
    {
        Printf(Tee::PriError, "Unable to create GoldenSema\n");
        return RC::SOFTWARE_ERROR;
    }

    // Start the golden thread
    m_DecodeInProgress = true;
    m_GldThread = Tasker::CreateThread(GoldenThread,
        this,
        Tasker::MIN_STACK_SIZE,
        "GldThread");

    if (m_GldThread == Tasker::NULL_THREAD)
    {
        Printf(Tee::PriError, "Unable to create GoldenThread\n");
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

LwvidLWDEC::EngineContext::~EngineContext()
{
    m_DecodeInProgress = false;

    // Add this extra semaphore release to avoid race condition
    // when golden generation thread is consuming faster than
    // the producer thread. In such scenarios, it will otherwise
    // get stuck in an infinite wait.
    Tasker::ReleaseSemaphore(m_GoldenSema);

    // Wait for the golden thread to finish
    if (m_GldThread != Tasker::NULL_THREAD)
    {
        *m_pEngineRc = Tasker::Join(m_GldThread, m_pTestObj->GetTestConfiguration()->TimeoutMs());
        m_GldThread = Tasker::NULL_THREAD;
    }

    *m_pEngineRc = m_GoldenRc;

    // Check if some testing was done on this engine
    if (!m_TestedSomething)
    {
        Printf(Tee::PriError,
               "%s: No testing was done\n",
               LwdelwsageModeToStr(m_LwdelwsageMode).c_str());
        *m_pEngineRc = RC::SOFTWARE_ERROR;
    }

    if (m_GoldenSema)
    {
        Tasker::DestroySemaphore(m_GoldenSema);
        m_GoldenSema = nullptr;
    }

    m_ChromaVWriter.Unmap();
    m_ChromaUWriter.Unmap();
    m_LumaWriter.Unmap();
    m_LumaSurf.Free();
    m_ChromaUSurf.Free();
    m_ChromaVSurf.Free();
}

RC LwvidLWDEC::EngineContext::DecodeStream
(
    const char *argInput
)
{
    RC rc;
    MASSERT(argInput);

    LWstream lwStream = nullptr;
    unsigned char* pOutputYUV = nullptr;
    DEFER
    {
        if (lwStream)
        {
            lwStreamDestroy(lwStream);
            lwStream = nullptr;
        }
        if (pOutputYUV)
        {
            lwMemFreeHost(pOutputYUV);
            pOutputYUV = nullptr;
        }

        // Delete all created objects
        if (m_State.lwVidSrc != nullptr)
        {
            lwvidDestroyVideoSource(m_State.lwVidSrc);
            m_State.lwVidSrc = nullptr;
        }
        if (m_State.lwParser != nullptr)
        {
            lwvidDestroyVideoParser(m_State.lwParser);
            m_State.lwParser = nullptr;
        }
        if (m_State.ctxLock)
        {
            lwvidCtxLock(m_State.ctxLock, 0);
        }
        if (m_State.lwDecoder != nullptr)
        {
            lwvidDestroyDecoder(m_State.lwDecoder);
            m_State.lwDecoder = nullptr;
        }
        if (m_State.ctxLock)
        {
            lwvidCtxLockDestroy(m_State.ctxLock);
            m_State.ctxLock = nullptr;
        }
        Tasker::FreeEvent(m_State.pEventCommand);
    };

    UINT64 freq = Xp::QueryPerformanceFrequency();

    m_pTestObj->VerbosePrintf(
           "%s (StreamIdx = %u): start decoding %s\n",
           LwdelwsageModeToStr(m_LwdelwsageMode).c_str(),
           m_StreamIdx, argInput);

    // Initialize the decode session
    LWVIDEOFORMATEX vidFormat;
    CHECK_RC(InitDecodeSession(&vidFormat, argInput));

    UINT32 displayWidth = vidFormat.format.display_area.right;
    UINT32 displayHeight = vidFormat.format.display_area.bottom;
    UINT32 outputHeight = m_State.dci.ulTargetHeight;

    // Create lwca stream
    LWresult result;
    result = LW_RUN(lwStreamCreate(&lwStream, 0));
    CHECK_RC(Lwca::LwdaErrorToRC(result));

    if (m_State.ctxLock)
    {
        lwvidCtxUnlock(m_State.ctxLock, 0);
    }

    result = LW_RUN(lwvidSetVideoSourceState(m_State.lwVidSrc,
                    lwdaVideoState_Started));
    CHECK_RC(Lwca::LwdaErrorToRC(result));

    // Will be allocated once we know the pitch
    UINT32 rawOutputYuvSize = 0;
    double totalDecodeTime = 0;
    UINT64 decodeStart = 0;

    // Start streaming
    m_pTestObj->VerbosePrintf(
           "%s (StreamIdx = %u): %s (%dx%d%c)...\n",
           LwdelwsageModeToStr(m_LwdelwsageMode).c_str(),
           m_StreamIdx,
           argInput,
           vidFormat.format.display_area.right,
           vidFormat.format.display_area.bottom,
           (vidFormat.format.progressive_sequence) ? 'p' : 'i');

    LWVIDPROCPARAMS vpp;
    memset(&vpp, 0, sizeof(LWVIDPROCPARAMS));
    // Index used to tag the golden values for each stream
    UINT32 frameIdx = 0;
    for (;;)
    {
        INT32 PicIdx = -1;
        INT32 numFields = 0;
        UINT32 eventType = EVENT_DISPLAY;

        {
            Tasker::MutexHolder lock(m_State.pMutex);
            if (m_State.numEventsInQueue > 0)
            {
                UINT32 iEntry = m_State.queuePosition;
                eventType = m_State.eventType[iEntry];
                m_State.eventType[iEntry] = 0;
                if (eventType == EVENT_DISPLAY)
                {
                    PicIdx = m_State.displayQueue[iEntry].picture_index;
                    vpp.progressive_frame = m_State.displayQueue[iEntry].progressive_frame;
                    vpp.top_field_first = m_State.displayQueue[iEntry].top_field_first;
                    vpp.second_field = 0;
                    vpp.unpaired_field = (m_State.displayQueue[iEntry].repeat_first_field < 0);
                    numFields = 2 + m_State.displayQueue[iEntry].repeat_first_field;
                }
                m_State.queuePosition = (iEntry + 1) % MAX_QUEUE_SIZE;
                m_State.numEventsInQueue--;
            }
        }

        vpp.output_stream = nullptr;

        if (eventType == EVENT_INIT)
        {
            result = LW_RUN(lwvidCreateDecoder(&m_State.lwDecoder, &m_State.dci));
            if (result != LWDA_SUCCESS)
            {
                // Let's unblock the decoder if it was waiting for display
                m_State.decodeEos = true;
                Tasker::SetEvent(m_State.pEventCommand);
                return Lwca::LwdaErrorToRC(result);
            }
            Tasker::SetEvent(m_State.pEventCommand);
            decodeStart = Xp::QueryPerformanceCounter();
        }

        if (PicIdx >= 0)
        {
            LWdeviceptr devPtr = 0;
            UINT32 pitch = 0;
            UINT32 outputYuvSize;
            INT32 startField;
            INT32 stopField;

            if (m_State.ctxLock)
            {
                result = LW_RUN(lwvidCtxLock(m_State.ctxLock, 0));
                CHECK_RC(Lwca::LwdaErrorToRC(result));
            }
            if (!vpp.unpaired_field)
            {
                stopField = numFields - 1;
                startField = stopField;
            }
            else
            {
                startField = 0;
                stopField = 0;
            }

            for (vpp.second_field = startField;
                 vpp.second_field <= stopField;
                 vpp.second_field++)
            {
                result = LW_RUN(
                    lwvidMapVideoFrame(m_State.lwDecoder, PicIdx, &devPtr, &pitch, &vpp));
                CHECK_RC(Lwca::LwdaErrorToRC(result));

                outputYuvSize = (m_State.dci.ChromaFormat == lwdaVideoChromaFormat_444) ?
                                 pitch * (3 * outputHeight) :
                                 pitch * (outputHeight + outputHeight / 2);

                if ((!pOutputYUV) || (outputYuvSize > rawOutputYuvSize))
                {
                    rawOutputYuvSize = 0;
                    if (pOutputYUV)
                    {
                        // Just to be safe (the pitch should be constant)
                        lwMemFreeHost(pOutputYUV);
                    }

                    result = LW_RUN(lwMemAllocHost(
                        reinterpret_cast<void**>(&pOutputYUV), outputYuvSize));
                    CHECK_RC(Lwca::LwdaErrorToRC(result));

                    rawOutputYuvSize = outputYuvSize;
                }

                if ((pOutputYUV) && (devPtr))
                {
                    result = LW_RUN(lwMemcpyDtoH(pOutputYUV, devPtr, outputYuvSize));
                    CHECK_RC(Lwca::LwdaErrorToRC(result));
                }

                result = LW_RUN(lwvidUnmapVideoFrame(m_State.lwDecoder, devPtr));
                CHECK_RC(Lwca::LwdaErrorToRC(result));

                // Colwert the output to standard IYUV and generate goldens
                UINT32 w2 = displayWidth;
                UINT32 h2 = displayHeight;
                UINT32 lumaSize = w2 * h2;
                UINT32 chromaSize = (m_State.dci.ChromaFormat == lwdaVideoChromaFormat_444) ?
                                      w2 * (h2 << 1) :
                                      w2 * (h2 >> 1);
                if (m_State.dci.OutputFormat == lwdaVideoSurfaceFormat_P016 ||
                    m_State.dci.OutputFormat == lwdaVideoSurfaceFormat_YUV444_16Bit)
                {
                    lumaSize *= 2;
                    chromaSize *= 2;
                }

                if (pOutputYUV)
                {
                    // Colwert and copy the output
                    auto params = make_unique<PerFrameGoldenParams>();
                    CHECK_RC(CopyOutputForGoldenGen(pOutputYUV,
                                                  h2, w2, pitch,
                                                  outputHeight,
                                                  m_State.dci.OutputFormat,
                                                  &params->lumaData,
                                                  &params->chromaUData,
                                                  &params->chromaVData));

                    // Push a request for updating goldens for this frame
                    params->goldenFrameIdx = static_cast<UINT32>(m_StreamIdx * 64_KB + frameIdx);
                    params->width = w2;
                    params->height = h2;
                    params->pitch = pitch;
                    params->chromaFormat = m_State.dci.ChromaFormat;
                    params->outputFormat = m_State.dci.OutputFormat;
                    {
                        Tasker::MutexHolder lock(m_GoldenQueueMutex);
                        m_GoldenQueue.push(std::move(params));
                        Tasker::ReleaseSemaphore(m_GoldenSema);
                    }

                    // If requested, dump the data to the YUV file
                    if (m_pTestObj->m_GenYuvOutput)
                    {
                        FileHolder yuvFile;
                        string filename = LwdelwsageModeToStr(m_LwdelwsageMode);
                        filename += Utility::StrPrintf("-stream%d-frame%d-output.yuv",
                            m_StreamIdx, frameIdx);
                        vector<UINT08> iyuv (lumaSize + chromaSize + 16);

                        CHECK_RC(ColwertAndCopyOutput(&iyuv[0], pOutputYUV,
                                                      w2, h2, pitch,
                                                      outputHeight,
                                                      lumaSize,
                                                      m_State.dci.OutputFormat,
                                                      m_State.dci.ChromaFormat));

                        CHECK_RC(yuvFile.Open(filename, "wb"));
                        CHECK_RC(Utility::FWrite(&iyuv[0], 1, (lumaSize + chromaSize),
                                                 yuvFile.GetFile()));
                    }
                    frameIdx++;
                }

            }

            if (m_State.ctxLock)
            {
                lwvidCtxUnlock(m_State.ctxLock, 0);
            }

            // We can now re-use this frame for decode
            m_State.frameInUse[PicIdx] = false;
        }
        else
        {
            if (((lwvidGetVideoSourceState(m_State.lwVidSrc) != lwdaVideoState_Started) ||
                (m_State.decodeEos))
                && (m_State.numEventsInQueue == 0))
            {
                // Reached EOS
                break;
            }

            // Wait for something to happen
            Tasker::Yield();
        }
    }

    UINT64 decodeEnd = Xp::QueryPerformanceCounter();
    totalDecodeTime = static_cast<double>(decodeEnd - decodeStart);

    result = LW_RUN(lwvidSetVideoSourceState(m_State.lwVidSrc, lwdaVideoState_Stopped));
    CHECK_RC(Lwca::LwdaErrorToRC(result));

    totalDecodeTime = (totalDecodeTime * 1000) / freq;
    m_pTestObj->VerbosePrintf(
           "%s (StreamIdx = %u): Decoded %d frames in %6.2f ms (%5.2f fps)\n",
           LwdelwsageModeToStr(m_LwdelwsageMode).c_str(),
           m_StreamIdx,
           m_State.decodePicCnt,
           totalDecodeTime,
           (static_cast<float>(m_State.decodePicCnt) * 1000.0f \
               / static_cast<float>(totalDecodeTime)));

    return rc;
}

//--------------------------------------------------------------------
// JavaScript interface
//--------------------------------------------------------------------
JS_CLASS_INHERIT(LwvidLWDEC, GpuTest, "New LWDEC test");
CLASS_PROP_READWRITE(LwvidLWDEC, GenYuvOutput, bool,
                     "The test will generate YUV file(s) per frame per stream per " \
                     " engine if this flag is set");
CLASS_PROP_READWRITE(LwvidLWDEC, UseLoadBalancing, bool,
                     "Use all existing engines at the video library discretion " \
                     " (SkipEngineMask argument is ignored)");
CLASS_PROP_READWRITE(LwvidLWDEC, SkipEngineMask, UINT32,
                     "Mask of engines to skip when Load Balancing is disabled");
CLASS_PROP_READWRITE(LwvidLWDEC, SkipStreamMask, UINT32,
                     "Mask of streams to skip, test all by default");
CLASS_PROP_READWRITE(LwvidLWDEC, KeepRunning, bool,
                     "Keep the test running in the background");
CLASS_PROP_READWRITE(LwvidLWDEC, RuntimeMs, UINT32,
                     "Run the test for atleast specified amount of time");
CLASS_PROP_READWRITE(LwvidLWDEC, Use2DSurfaceForGolden, bool,
                     "Use 2D surface to generate goldens. Useful for dump_png and debug purpose");
CLASS_PROP_READWRITE(LwvidLWDEC, DoRTD3Cycles, bool,
                     "Run RTD3 cycle after decoding a frame");
CLASS_PROP_READWRITE(LwvidLWDEC, PciConfigSpaceRestoreDelayMs, UINT32,
                     "Delay before accessing PCI config space, in ms (default is 50)");
