/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/abort.h"
#include "core/include/display.h"
#include "core/include/fileholder.h"
#include "core/include/regexp.h"
#include "core/include/tar.h"
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "gputest.h"
#include "gpu/utility/surfrdwr.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

#include "class/clc0b0.h"  // LWC0B0_VIDEO_DECODER
#include "class/cle26e.h"  // LWE2_CHANNEL_DMA
#include "tvmr.h"
#include "video_parser.h"

class TvmrDecoder;
#define CT_NUMELEMS(arr) (sizeof(arr)/sizeof((arr)[0]))
#define MEM_COPY(dst, src)                      \
    do                                          \
    {                                           \
        ct_assert(sizeof(dst) == sizeof(src));  \
        memcpy(&dst, &src, sizeof(dst));        \
    } while (0)

//--------------------------------------------------------------------
//! \brief Test for the LWDEC engine, using TVMR libraries
//!
//! The LWDEC engine is used to decode video streams in a variety of
//! formats, such as H264, H265 (aka HEVC), VP9, AV1 and MPEG4. This
//! test decodes a set of streams in an lwdec.bin tarball.
//!
//! The LWDEC engine does not take an entire video stream as input.
//! Rather, it just decodes a single frame at a time.  It is up to SW
//! to parse the encoded frames and header data from the video stream,
//! and then pass that data to LWDEC.  This test uses the following
//! TVMR libraries to facilitate that job:
//! * lwparser: Parses the frames and headers from a video stream.
//!   To use lwparser, the client must provide a set of callback
//!   functions such as "BeginSequence", "DecodePicture",
//!   "DisplayPicture", etc.  The client then passes a video bitstream
//!   to lwparser, and lwparser passes the parsed header/frame
//!   information to the callback functions.
//! * lwtvmr: Thin wrapper around an LWDEC channel.  Used to implement
//!   the callbacks ilwoked by lwparser.
//!
class TegraLwdecTest : public GpuTest
{
public:
    TegraLwdecTest();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual void PrintJsProperties(Tee::Priority pri);

    SETGET_PROP(TestStreams, string);
    SETGET_PROP(SkipStreams, string);
    SETGET_PROP(EngineSkipMask, UINT32);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(SaveDecodedImages, bool);

private:
    struct StreamDescriptor
    {
        const char* streamName;
        const LwVideoCompressionStd codec;
        // Some streams have large number of frames so limit max frames to decode
        // based on feedback from Arch team
        const UINT32 maxFramesToDecode;
    };
    RC DecodeStream(const StreamDescriptor& streamDesc, bool isBackground);
    RC ParseIvf(const char *streamName, const vector<LwU8>& streamData,
                const char *codecName, vector<bitstream_packet_s> *pPackets);
    RC ParseGeneric(const char* streamName, const vector<LwU8>& streamData,
                    vector<bitstream_packet_s> *pPackets);

    static TvmrDecoder *Upcast(void *pCtx)
        { return static_cast<TvmrDecoder*>(pCtx); }
    static UINT32 GetStreamHash(const string& streamName);

    // The lwparser callback functions.  Most of these functions
    // are thin wrappers around a corresponding m_pDecoder function.
    //
    static LwS32  BeginSequence(void *pCtx, const LWDSequenceInfo *pSeq);
    static LwBool DecodePicture(void *pCtx, LWDPictureData *pPictureData);
    static LwBool DisplayPicture(void *pCtx, LWDPicBuff *pPicBuff, LwS64 pts);
    static void   UnhandledNALU(void *pCtx, const LwU8 *p, LwS32 v);
    static LwBool AllocPictureBuffer(void *pCtx, LWDPicBuff **ppPicBuff);
    static void   Release(void *pCtx, LWDPicBuff *pPicBuff);
    static void   AddRef(void *pCtx, LWDPicBuff *pPicBuff);
    static LwBool CreateDecrypter(void *pCtx, LwU16 width, LwU16 height);
    static LwBool DecryptHdr(void *pCtx, LWDPictureData *pPictureData);
    static LwBool GetHdr(void *pCtx, LWDPictureData *pPic, LwU8 *pNalType);
    static LwBool UpdateHdr(void *pCtx, LWDPictureData *pPic, LwU8 nalType);
    static LwBool CopySliceData(void *pCtx, LWDPictureData *pPictureData);
    static LwBool GetClearHdr(void *pCtx, LWDPictureData *pPictureData);
    static LwBool GetBackwardUpdates(void *pCtx, LWDPictureData *pPictureData);

    using TvmrDecoders = vector<unique_ptr<TvmrDecoder>>;

    TarArchive m_TarArchive;          //!< Contains the video streams as files
    TvmrDecoders m_Decoders;          //!< Implements the lwparser callbacks
    unique_ptr<GpuGoldenSurfaces> m_pGoldenSurfaces;
    Tasker::Mutex m_GoldenMutex;      //!< Mutex for golden values
    string        m_TestStreams;      //!< Regex of streams to test
    string        m_SkipStreams;      //!< Regex of streams to skip
    UINT32        m_EngineSkipMask = 0; //!< Bitmask indicating which engines will be skipped
    bool          m_KeepRunning = false;
    bool          m_SaveDecodedImages = false;
};

//--------------------------------------------------------------------
//! \brief Class that implements the lwparser callbacks
//!
//! This class implements the body of the lwparser callback functions.
//! It sends the parsed data to the LWDEC engine to decode the video
//! frames into surfaces.
//!
//! For the most part, the callback methods take the same arguments as
//! the lwparser callbacks in video_parser.h, with two main exceptions:
//! (1) Lwparser is written in C, so it has no concept of a "this"
//!     pointer.  Instead, it passes an opaque "client context"
//!     pointer as the first arg of each callback.  This caller is
//!     expected to colwert the "client context" to a C++ "this"
//!     pointer, so this class does not take any "client context"
//!     pointers.
//! (2) The methods in this class return an RC, instead of a simple
//!     pass/fail boolean.  It is up to the caller to record the RC
//!     and colwert it to a boolean.
//!
//! Caveats:
//!
//! This class lwrrently uses the lwtvmr library to ilwoke the LWDEC
//! engine, but it is deliberately designed not to expose any lwtvmr
//! functionality in the public API.  The rationale is that lwtvmr is
//! very specific to cheetah, but lwparser has a decent chance of being
//! ported to the dGPU.  In that case, we might want to subclass this
//! class so that it can be implemented with the mods Channel class
//! for dGPU, and with lwtvmr for cheetah.
//!
//! This class is designed to *only* do the LWDEC decoding, and not
//! any other functionality specific to TegraLwdecTest.  Any other
//! functionality, such as generating golden-values, must be done in
//! the TegraLwdecTest wrapper functions.  The rationale is that, even
//! though mods lwrrently only uses lwparser for TegraLwdecTest, we
//! might want to use it for something else someday.  (I don't know
//! how likely that is.  But since the *only* extra functionality
//! needed by TegraLwdecTest was generating GVs and displaying the
//! current frame, and since it was just as easy to put that
//! functionality into TegraLwdecTest, I figured we might as well keep
//! our options open.)
//!
class TvmrDecoder
{
public:
    enum BitColwersion     // How to colwert 10-bit streams to 8-bit surfaces
    {
        BIT_COLWERT_NONE,  // Do not colwert; leave at 10-bit
        BIT_COLWERT_709,   // Colwert using Rec 709 standard
        BIT_COLWERT_2020   // Colwert using Rec 2020 standard
    };
    TvmrDecoder
    (
        FLOAT64               timeoutMs,
        BitColwersion         bitColwersion,
        TegraLwdecTest*       pTest,
        Goldelwalues*         pGoldelwalues,
        GpuGoldenSurfaces*    pGoldenSurfaces,
        Tasker::Mutex&        goldenMutex,
        UINT32                pitchAlign,
        TVMRDecoderInstanceId instance,
        Tee::Priority         pri
    );
    ~TvmrDecoder();

    // Frame information and errors
    //
    void SetStreamIndex(UINT32 idx) { m_FrameCountBase = idx; }
    void SetStreamName(const string& name) { m_LwrStreamName = name; }
    void SetMaxFramesToTest(UINT32 maxFrames) { m_MaxFramesToTest = maxFrames; }
    UINT32 GetMaxFramesToTest() const { return m_MaxFramesToTest; }
    void IncrementFrameCounter() { ++m_FrameCounter; }
    void ClearFrameCounter() { m_FrameCounter = 0; }
    UINT32 GetFrameCounter() const { return m_FrameCounter; }
    void ClearScheduledFramesCount() { m_FramesScheduled = 0; }
    void SetStopDecoding(bool stopDecoding) { m_StopDecoding = stopDecoding; }
    bool GetStopDecoding() const { return m_StopDecoding; }
    void SetRC(RC rc) { m_FirstRc = rc; }
    RC GetRC() const { return m_FirstRc; }
    void ClearRC() { m_FirstRc.Clear(); }

    RC WaitForPendingFrames();

    // Handle golden values
    //
    RC UpdateGoldens();

    // Body of the lwparser callback functions
    //
    RC BeginSequence(const LWDSequenceInfo *pSequenceInfo, LwS32 *pNumBuffers);
    RC DecodePicture(LWDPictureData *pPictureData);
    RC DisplayPicture(LWDPicBuff *pPicBuff, LwS64 pts);
    RC UnhandledNALU(const LwU8 *, LwS32);
    RC AllocPictureBuffer(LWDPicBuff **ppPicBuff);
    RC Release(LWDPicBuff *pPicBuff);
    RC AddRef(LWDPicBuff *pPicBuff);
    RC CreateDecrypter(LwU16 width, LwU16 height);
    RC DecryptHdr(LWDPictureData *pPictureData);
    RC GetHdr(LWDPictureData *pPictureData, LwU8 *pNalType);
    RC UpdateHdr(LWDPictureData *pPictureData, LwU8 nalType);
    RC CopySliceData(LWDPictureData *pPictureData);
    RC GetClearHdr(LWDPictureData *pPictureData);
    RC GetBackwardUpdates(LWDPictureData *pPictureData);

    // Methods for extracting data about the current frame
    //
    RC GetOutputFormat(UINT32 *pLumaWidth, UINT32 *pLumaHeight,
                       UINT32 *pLumaDepth, UINT32 *pChromaWidth,
                       UINT32 *pChromaHeight, UINT32 *pChromaDepth);
    RC GetOutputData(Surface2D *pLumaSurf, Surface2D *pChromaSurf,
                     Surface2D *pRgbSurf, const char* dumpFileName);

    static bool PollAllFramesDecoded(void* pPollArgs);

private:
    //! lwparser uses an "LWDPicBuff" pointer to store a semiplanar
    //! YUV surface.  lwparser decides when the LWDPicBuffs should be
    //! allocated and freed, and which LWDPicBuff(s) should be passed
    //! to LWDEC as the stream is parsed, but the contents of an
    //! LWDPicBuff are opaque to lwparser.  lwparser relies on the
    //! client to implement the contents of an LWDPicBuff.
    //!
    //! This is our local implementation of LWDPicBuff.
    //!
    class MyPicBuff
    {
    public:
        MyPicBuff(TVMRDevice *pDevice, TVMRSurfaceType type,
                LwU16 width, LwU16 height, TVMRFence tvmrFence,
                UINT32 picBuffIdx, UINT32 fenceIdx);
        ~MyPicBuff();
        TVMRVideoSurface  *GetSurface() const { return m_pSurface; }
        TVMRFence          GetTvmrFence() const { return m_TvmrFence; }
        void   AddRef() { MASSERT(m_RefCount > 0); ++m_RefCount; }
        UINT32 DelRef() { MASSERT(m_RefCount > 0); return --m_RefCount; }
        TVMRVideoSurface         *AddPrereq(LWDPicBuff *pPrereq);
        const vector<MyPicBuff*> &GetPrereqs() const { return m_Prereqs; }
        void                      ClearPrereqs()     { m_Prereqs.clear(); }
        TVMRFence                *GetPrereqFences();
        void                      SetFence(UINT32 fenceIdx, TVMRFence tvmrFence);
        static MyPicBuff *Upcast(LWDPicBuff *pPicBuff)
            { return static_cast<MyPicBuff*>(pPicBuff); }
        UINT32 GetPicBuffIdx() const { return m_PicBuffIdx; }
        UINT32 GetFenceIdx() const { return m_FenceIdx; }
    private:
        TVMRVideoSurface  *m_pSurface;     //!< The YUV surface
        TVMRFence          m_TvmrFence;    //!< Tells when m_pSurface is full
        UINT32             m_RefCount;     //!< Reference counter
        vector<MyPicBuff*> m_Prereqs;      //!< Prerequisite pictures
        vector<TVMRFence>  m_PrereqFences; //!< NULL-terminated prereq fences
        UINT32             m_PicBuffIdx;   //!< Index of picbuff in m_Pictures
        UINT32             m_FenceIdx;     //!< Index of fence in m_Fences
    };

    //! Dimensions of a color plane (i.e. luma or chroma).
    //
    //! "width" is in pixels.  To get the width in bytes, multiply by
    //! the bytes per pixel (depth/8, rounded up).  Additionaly
    //! multiply by 2 for UV chroma planes, because each pixel
    //! contains both U and V.
    //!
    //! Encoders tend to round the image dimensions up to a multiple
    //! of 16, which is why there are both "stream" dimensions (the
    //! rounded-up values) and "output" dimensions (the actual image).
    //!
    struct Dimensions
    {
        LwU16 streamWidth;
        LwU16 streamHeight;
        LwU16 streamDepth;
        UINT32 outputWidth;
        UINT32 outputHeight;
        UINT32 outputDepth;
    };

    class Fence
    {
    public:
        Fence();
        ~Fence();
        Fence(const Fence&) = delete;
        Fence& operator=(const Fence&) = delete;
        TVMRFence GetTvmrFence () const { return m_TvmrFence; }
        void AddRef();
        void DelRef();
        UINT32 GetRefCount() const { return m_RefCount; }

    private:
        UINT32       m_RefCount = 1;
        TVMRFence    m_TvmrFence;
    };

    void EndSequence();
    static RC StatusToRc(TVMRStatus status);
    static RC SetEncryptParams(TVMREncryptionParams *pOut,
                               const LWDEncryptParams &in);
    RC SetPictureInfoMpeg( TVMRPictureInfoMPEG   *pOut,
                           const LWDPictureData  &in,
                           MyPicBuff             *pLwrPic);
    RC SetPictureInfoMpeg4(TVMRPictureInfoMPEG4  *pOut,
                           const LWDPictureData  &in,
                           MyPicBuff             *pLwrPic);
    RC SetPictureInfoVc1(  TVMRPictureInfoVC1    *pOut,
                           const LWDPictureData  &in,
                           MyPicBuff             *pLwrPic);
    RC SetPictureInfoH264( TVMRPictureInfoH264   *pOut,
                           const LWDPictureData  &in,
                           MyPicBuff             *pLwrPic);
    RC SetPictureInfoVp8 ( TVMRPictureInfoVP8    *pOut,
                           const LWDPictureData  &in,
                           MyPicBuff             *pLwrPic);
    RC SetPictureInfoH265( TVMRPictureInfoHEVC   *pOut,
                           const LWDPictureData  &in,
                           MyPicBuff             *pLwrPic);
    RC SetPictureInfoVp9 ( TVMRPictureInfoVP9    *pOut,
                           const LWDPictureData  &in,
                           MyPicBuff             *pLwrPic);
    RC SetPictureInfoAv1 ( TVMRPictureInfoAV1    *pOut,
                           LWDPictureData        *pLwdPictureData,
                           MyPicBuff             *pLwrrPic);

    void AddFenceRef(UINT32 fenceIdx);
    void ReleaseFence(UINT32 fenceIdx);
    RC FreePicBuff(MyPicBuff *pMyPicBuff);
    RC ShareFence(MyPicBuff *pSrcPicBuff, MyPicBuff *pDstPicBuff);

    // The following members are set by the constructor
    //
    const TVMRTime              m_TimeoutTvmr;   //!< Timeout in ms
    const BitColwersion         m_BitColwersion; //!< How to colwert 10bit streams to
                                                 //!< 8bit.  Not used for 8bit streams.
    const TVMRDecoderInstanceId m_Instance;      //!< LWDEC instance to target
    const Tee::Priority         m_Pri;           //!< Print priority for debug messages
    TVMRDevice                 *m_pDevice;       //!< Used to alloc fences

    // The following members are set by BeginSequence
    //
    TVMRVideoCodec    m_Codec;        //!< Codec to use for stream (eg H264)
    Dimensions        m_LumaDims;     //!< Dimensions of the luma plane
    Dimensions        m_ChromaDims;   //!< Dimensions of the chroma plane
    TVMRSurfaceType   m_SurfaceType;  //!< LW12, LW24, etc
    TVMRVideoDecoder *m_pDecoder;     //!< TVMR object that does the decoding

    // The following members are updated during the decoding
    //
    using MyPicBuffs = vector<unique_ptr<MyPicBuff>>;
    MyPicBuffs      m_Pictures;       //!< Holds all pictures alloced by
                                      //!< AllocPictureBuffer()
    MyPicBuff      *m_pOutputPicture; //!< The current frame being displayed;
                                      //!< see GetOutputData()

    string          m_LwrStreamName;        //!< Name of current string, for logging purposes
    UINT32          m_FrameCountBase = 0;   //!< Loop number base for golden values
    UINT32          m_FrameCounter = 0;     //!< Counts num frames in stream
    UINT32          m_FramesScheduled = 0;  //!< Counts num frames scheduled for decode
    StickyRC        m_FirstRc;              //!< Any error found during parsing
    TegraLwdecTest *m_pTest = nullptr;      //!< Pointer to test, for updating GVs
    UINT32          m_MaxFramesToTest = _UINT32_MAX;
    bool            m_StopDecoding = false;

    using Fences = vector<unique_ptr<Fence>>;
    Fences          m_Fences;               //!< List of allocated fences

    Surface2D       m_LumaSurf;           //!< Luma (Y) of the current frame
    Surface2D       m_ChromaSurf;         //!< Chroma (U & V) of the current frame
    Surface2D       m_RgbSurf;            //!< Current frame as displayable A8R8G8B8
    Goldelwalues   *m_pGoldelwalues = nullptr;
    GpuGoldenSurfaces *m_pGoldenSurfaces = nullptr;
    Tasker::Mutex&  m_GoldenMutex;
    UINT32          m_PitchAlign = 0;
};

//--------------------------------------------------------------------
TegraLwdecTest::TegraLwdecTest() :
    m_GoldenMutex(Tasker::CreateMutex("LWDEC GV", Tasker::mtxUnchecked)),
    m_SkipStreams("sim_*")
{
    SetName("TegraLwdecTest");
}

//--------------------------------------------------------------------
/* virtual */ bool TegraLwdecTest::IsSupported()
{
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    LwdecAlloc lwdecAlloc;

    lwdecAlloc.SetOldestSupportedClass(LWC0B0_VIDEO_DECODER);
    return (GpuTest::IsSupported() &&
            pGpuSubdevice->IsSOC() &&
            lwdecAlloc.IsSupported(pGpuDevice));
}

//--------------------------------------------------------------------
/* virtual */ RC TegraLwdecTest::Setup()
{
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    RC rc;

    CHECK_RC(GpuTest::Setup());
    if (!CheetAh::SocPtr()->HasBug(3286489))
    {
        CHECK_RC(GpuTest::AllocDisplay());
    }

    m_pGoldenSurfaces = make_unique<GpuGoldenSurfaces>(GetBoundGpuDevice());

    // Cheat GpuGoldenSurfaces and set up 2 surfaces, the pointers
    // will dangle, but we will be calling ReplaceSurface() anyway.
    //
    Surface2D fakeLuma;
    fakeLuma.SetWidth(1);
    fakeLuma.SetHeight(1);
    fakeLuma.SetColorFormat(ColorUtils::R16);
    m_pGoldenSurfaces->AttachSurface(&fakeLuma, "L", 0);
    Surface2D fakeChroma;
    fakeChroma.SetWidth(1);
    fakeChroma.SetHeight(1);
    fakeChroma.SetColorFormat(ColorUtils::R16_G16);
    m_pGoldenSurfaces->AttachSurface(&fakeChroma, "C", 0);

    CHECK_RC(GetGoldelwalues()->SetSurfaces(m_pGoldenSurfaces.get()));

    UINT32 pitchAlign;
    CHECK_RC(m_pGoldenSurfaces->GetPitchAlignRequirement(&pitchAlign));

    // Load the tarball that contains the video streams
    //
    string tarballFilename = Utility::DefaultFindFile("lwdec.bin", true);
    if (!m_TarArchive.ReadFromFile(tarballFilename))
    {
        return RC::FILE_DOES_NOT_EXIST;
    }

    // Get the list of LWDEC engines
    //
    LwdecAlloc lwdecAlloc;
    lwdecAlloc.SetOldestSupportedClass(LWC0B0_VIDEO_DECODER);
    vector<CheetAh::SocDevice::HwInstance> hwInstanceIds =
        CheetAh::SocPtr()->GetHwInstanceIds(lwdecAlloc.GetSupportedClass(GetBoundGpuDevice()));

    MASSERT(!hwInstanceIds.empty());
    if (hwInstanceIds.empty())
    {
        Printf(Tee::PriError, "No Lwdec engines configured\n");
        return RC::SOFTWARE_ERROR;
    }

    // Load the decoder that handles the lwparser callbacks
    //
    for (const auto hwInstanceId : hwInstanceIds)
    {
        TVMRDecoderInstanceId instance = TVMR_DECODER_INSTANCE_0;

        if (hwInstanceId == CheetAh::SocDevice::HwInstance::LWDEC1)
        {
            instance = TVMR_DECODER_INSTANCE_1;
        }
        else
        {
            MASSERT(hwInstanceId == CheetAh::SocDevice::HwInstance::LWDEC0);
            if (hwInstanceId != CheetAh::SocDevice::HwInstance::LWDEC0)
            {
                Printf(Tee::PriError, "Unexpected hwInstanceId %u\n",
                    static_cast<UINT32>(hwInstanceId));
                return RC::SOFTWARE_ERROR;
            }
        }

        MASSERT(instance < 32);
        if (m_EngineSkipMask & (1U << instance))
        {
            Printf(Tee::PriNormal, "Skipping LWDEC %u\n", static_cast<UINT32>(instance));
            continue;
        }

        m_Decoders.push_back(make_unique<TvmrDecoder>(
                                     pTestConfig->TimeoutMs(),
                                     TvmrDecoder::BIT_COLWERT_NONE,
                                     this,
                                     GetGoldelwalues(),
                                     m_pGoldenSurfaces.get(),
                                     m_GoldenMutex,
                                     pitchAlign,
                                     instance,
                                     GetVerbosePrintPri()));

        // During gpugen run only on one engine
        //
        if (GetGoldelwalues()->GetAction() == Goldelwalues::Store)
        {
            break;
        }
    }
    return rc;
}

//--------------------------------------------------------------------
/* virtual */ RC TegraLwdecTest::Run()
{
    const bool isBackground = m_KeepRunning;
    bool isOneFrameScheduledForDecode = false;

    vector<StreamDescriptor> streamDescriptors = {
        // Streams intended for simulation only should start with sim_
        { "sim_bboxes-001.264", LWCS_H264, _UINT32_MAX },
        { "sim_soccer_sanity_192x192_420_8bit_2frames.265", LWCS_H265, _UINT32_MAX },
        { "in_to_tree-001.264", LWCS_H264, _UINT32_MAX },
        { "sunflower-007.264", LWCS_H264, _UINT32_MAX },
        { "sunflower-008.264", LWCS_H264, _UINT32_MAX },
        { "in_to_tree-002.265", LWCS_H265, _UINT32_MAX },
        { "old-town-cross-001.265", LWCS_H265, _UINT32_MAX },
        { "HMRExt7.2_MAIN444_akiyo_352x288_sanity_64.265", LWCS_H265, 15},
        { "case_15000_stream.vp9", LWCS_VP9, _UINT32_MAX },
        { "case_15002_stream.vp9", LWCS_VP9, _UINT32_MAX },
        { "VP9_HBD12b_perf_world_of_warcraft_1280x720_10bit.yuv_HD20Mbps_64.ivf", LWCS_VP9, 15 },
        { "HMRExt_MAIN444_Perf_10b_world_of_warcraft_1280x720_10bit_444.yuv_HD20Mbps_CTB64x64_64.bin",
            LWCS_H265, 15 },
        { "HMRExt_MAIN12_Perf_world_of_warcraft_1280x720.yuv_HD20Mbps_CTB64x64_64.bin", LWCS_H265, 15 },
        { "HMRExt_MAIN444_Perf_10b_AfterEarth_3840x2160_60frames.yuv_HD20Mbps_CTB32x32_8.bin",
            LWCS_H265, 15 },
        { "H264_Perf_world_of_warcraft_1280x720.yuv_HD15Mbps_30.264", LWCS_H264, 15 }
    };

    if (CheetAh::SocPtr()->HasFeature(Device::SOC_SUPPORTS_AV1_DECODE))
    {
        streamDescriptors.push_back({ "sim_foreman_qcif_av1_sb64_8bit_2frames.ivf", LWCS_AV1, _UINT32_MAX });
        streamDescriptors.push_back({ "AV1_MAIN10_Perf_world_of_warcraft_1280x720_10bit_HD15Mbps_SB128x128_64_fg13_aq2_AllOn.ivf",
            LWCS_AV1, 15 });
        streamDescriptors.push_back({ "AV1_MAIN8_Perf_cars_3840x2160_100frames.yuv_HD10Mbps_SB64x64_64_hus2_4k_AllOn.ivf",
            LWCS_AV1, 15 });
    }

    RegExp testRegexp;
    RegExp skipRegexp;
    RC rc;

    // Decode each stream in the tarball
    //
    CHECK_RC(testRegexp.Set(m_TestStreams));
    CHECK_RC(skipRegexp.Set(m_SkipStreams,
                            m_SkipStreams.empty() ? RegExp::ILWERT : 0));

    do
    {
        for (const auto& streamDesc: streamDescriptors)
        {
            if (testRegexp.Matches(streamDesc.streamName) &&
                !skipRegexp.Matches(streamDesc.streamName))
            {
                CHECK_RC(DecodeStream(streamDesc, isBackground));
                isOneFrameScheduledForDecode = true;
            }
            else
            {
                VerbosePrintf("Skipping stream %s\n", streamDesc.streamName);
            }
            // Guidance was to run something before exiting in case foreground
            // test was short and exited very quickly
            if (isBackground && !m_KeepRunning && isOneFrameScheduledForDecode)
            {
                break;
            }
        }
    } while (m_KeepRunning);

    if (!isOneFrameScheduledForDecode)
    {
        Printf(Tee::PriError, "No frames were scheduled for decode\n");
        return RC::UNEXPECTED_TEST_COVERAGE;
    }

    CHECK_RC(GetGoldelwalues()->ErrorRateTest(GetJSObject()));

    return OK;
}

//--------------------------------------------------------------------
/* virtual */ RC TegraLwdecTest::Cleanup()
{
    Goldelwalues *pGoldelwalues = GetGoldelwalues();
    Display      *pDisplay      = GetDisplay();
    StickyRC firstRc;

    if (!CheetAh::SocPtr()->HasBug(3286489))
    {
        pDisplay->SetImage(static_cast<Surface2D*>(nullptr));
    }
    pGoldelwalues->ClearSurfaces();
    m_pGoldenSurfaces.reset();
    m_Decoders.clear();
    firstRc = GpuTest::Cleanup();
    return firstRc;
}

//--------------------------------------------------------------------
/* virtual */ void TegraLwdecTest::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tTestStreams:\t\t\t%s\n", m_TestStreams.c_str());
    Printf(pri, "\tSkipStreams:\t\t\t%s\n", m_SkipStreams.c_str());
    Printf(pri, "\tEngineSkipMask:\t\t\t0x%x\n", m_EngineSkipMask);
    Printf(pri, "\tKeepRunning:\t\t\t%s\n", m_KeepRunning ? "true" : "false");
    Printf(pri, "\tSaveDecodedImages:\t\t%s\n", m_SaveDecodedImages ? "true" : "false");
}

//--------------------------------------------------------------------
//! \brief Decode one video stream in the tarball
//!
//! This is the heart of the TegraLwdecTest.  It extracts one video
//! stream from the tarball, and uses lwparser to parse the stream.
//!
//! During the parsing, lwparser will call the various callbacks
//! implemented in TvmrDecoder, which will pass the frames to the
//! LWDEC engine, and pass the decoded frames for GV processing.
//!
RC TegraLwdecTest::DecodeStream(const StreamDescriptor& streamDesc, bool isBackground)
{
    RC rc;

    Printf(GetVerbosePrintPri(), "Decoding %s...\n", streamDesc.streamName);

    // Run as much as we can on a detached thread.
    //
    Tasker::DetachThread detach;

    // Read the stream data from the tar archive
    //
    TarFile *pTarFile = m_TarArchive.Find(streamDesc.streamName);
    if (pTarFile == nullptr)
    {
        return RC::FILE_READ_ERROR;
    }
    const unsigned streamSize = pTarFile->GetSize();
    vector<LwU8> streamData(streamSize);
    pTarFile->Seek(0);
    if (pTarFile->Read(reinterpret_cast<char*>(&streamData[0]), streamSize) !=
        streamSize)
    {
        return RC::FILE_READ_ERROR;
    }

    // Split the stream data into one or more packets.
    //
    vector<bitstream_packet_s> packets;
    switch (streamDesc.codec)
    {
        case LWCS_VP8:
        {
            CHECK_RC(ParseIvf(streamDesc.streamName, streamData, "VP80", &packets));
            break;
        }
        case LWCS_VP9:
        {
            CHECK_RC(ParseIvf(streamDesc.streamName, streamData, "VP90", &packets));
            break;
        }
        case LWCS_AV1:
        {
            CHECK_RC(ParseIvf(streamDesc.streamName, streamData, "AV01", &packets));
            break;
        }
        default:
        {
            CHECK_RC(ParseGeneric(streamDesc.streamName, streamData, &packets));
            break;
        }
    }

    // Create the video parser
    //
    static LWDClientCb parserClient =
    {
        BeginSequence,
        DecodePicture,
        DisplayPicture,
        UnhandledNALU,
        AllocPictureBuffer,
        Release,
        AddRef,
        CreateDecrypter,
        DecryptHdr,
        GetHdr,
        UpdateHdr,
        CopySliceData,
        GetClearHdr,
        GetBackwardUpdates
    };

    struct Thread
    {
        Tasker::ThreadID threadId = Tasker::NULL_THREAD;
        StickyRC         rc;
    };
    vector<Thread> threads(m_Decoders.size());

    const auto JoinThreads = [&]()->RC
    {
        StickyRC rc;
        for (const auto& thread : threads)
        {
            if (thread.threadId != Tasker::NULL_THREAD)
            {
                Tasker::Join(thread.threadId);
            }
            rc = thread.rc;
        }
        threads.clear();
        return rc;
    };

    DEFER
    {
        JoinThreads();
    };

    for (UINT32 i = 0; i < m_Decoders.size(); i++)
    {
        auto threadFunc = [&, i]()->void
        {
            Tasker::DetachThread detach;

            auto  pDecoder = m_Decoders[i].get();
            auto& thread   = threads[i];

            pDecoder->SetStreamIndex(GetStreamHash(streamDesc.streamName));
            pDecoder->SetStreamName(streamDesc.streamName);
            pDecoder->SetMaxFramesToTest(streamDesc.maxFramesToDecode);
            pDecoder->SetStopDecoding(false);

            LWDParserParams parserParams = { };
            parserParams.pClient         = &parserClient;
            parserParams.pClientCtx      = pDecoder;
            parserParams.lErrorThreshold = 100;
            parserParams.eCodec          = streamDesc.codec;

            video_parser_context_s *pParser = video_parser_create(&parserParams);
            MASSERT(pParser != nullptr);
            DEFER
            {
                pDecoder->SetStopDecoding(true);
                video_parser_destroy(pParser);
            };

            // Parse the stream
            //
            pDecoder->ClearFrameCounter();
            pDecoder->ClearScheduledFramesCount();
            pDecoder->ClearRC();

            auto packetsCopy = packets;
            for (auto& packet : packetsCopy)
            {
                if (pDecoder->GetStopDecoding())
                {
                    bitstream_packet_s packet = {};
                    packet.bDiscontinuity = LW_TRUE;
                    video_parser_parse(pParser, &packet);
                    video_parser_flush(pParser);
                    break;
                }

                LwBool parseSucceeded = video_parser_parse(pParser, &packet);
                thread.rc = pDecoder->GetRC();
                if (thread.rc != OK)
                    return;
                if (!parseSucceeded)
                {
                    Printf(Tee::PriError, "Unknown parsing error in %s\n",
                           streamDesc.streamName);
                    thread.rc = RC::SOFTWARE_ERROR;
                    return;
                }

                // Check for Ctrl-C
                thread.rc = Abort::Check();
                if (thread.rc != OK)
                    return;

                // Exit early if the test runs in background and is requested to stop
                if (isBackground && ! m_KeepRunning)
                {
                    return;
                }
            }
            if (pDecoder->GetFrameCounter() == 0)
            {
                Printf(Tee::PriError, "No frames found in %s\n",
                       streamDesc.streamName);
                thread.rc = RC::SOFTWARE_ERROR;
                return;
            }

            if (!pDecoder->GetStopDecoding())
            {
                // If stop decoding is not set wait for any pending frames to complete
                thread.rc = pDecoder->WaitForPendingFrames();
            }
        };

        auto& thread = threads[i];
        if (m_Decoders.size() > 1)
        {
            thread.threadId = Tasker::CreateThread("LWDEC TVMR", threadFunc);
            if (thread.threadId == Tasker::NULL_THREAD)
                return RC::SOFTWARE_ERROR;
        }
        else
        {
            threadFunc();
        }
    }

    CHECK_RC(JoinThreads());

    return OK;
}

//--------------------------------------------------------------------
//! \brief Produce unique number referencing the stream
//!
//! Callwlates a CRC of the stream name to produce a unique number
//!
UINT32 TegraLwdecTest::GetStreamHash(const string& streamName)
{
    // djb2a hash
    UINT32 hash = 5381;
    for (const auto c : streamName)
    {
        hash = (hash * 33U) ^ static_cast<UINT32>(static_cast<UINT08>(c));
    }
    return hash;
}

//--------------------------------------------------------------------
//! \brief Parse the frames from an IVF file
//!
//! Called by DecodeStream() to extract the VP8/VP9 stream from an IVF
//! file.
//!
//! \param streamName The name of the IVF file
//! \param streamData The raw data read from the IVF file.
//! \param codecName The 4-character codec name expected in the IVF
//!     header, eg "VP80" or VP90"
//! \param[out] pPackets On exit, contains an array of packets parsed
//!     from streamData.  Each packet contains one frame.  Note
//!     pPackets contains pointers into streamData, so the caller must
//!     not free streamData until he is done with pPackets.
//!
RC TegraLwdecTest::ParseIvf
(
    const char* streamName,
    const vector<LwU8>& streamData,
    const char *codecName,
    vector<bitstream_packet_s> *pPackets
)
{
    MASSERT(codecName != nullptr);
    MASSERT(pPackets != nullptr);

    struct IvfHeader
    {
        char   signature[4];    // Must be 'DKIF'
        UINT16 version;         // Should be 0
        UINT16 headerLength;
        char   codec[4];        // "VP80" or "VP90"
        UINT16 widthInPixels;
        UINT16 heightInPixels;
        UINT32 frameRate;
        UINT32 timeScale;
        UINT32 numFrames;
    };
    struct IvfFrameHeader
    {
        UINT32 frameSize;       // Not including 12-byte header
        UINT32 timestampLo;
        UINT32 timestampHi;
    };

    // Read the IVF header and do sanity checks
    //
    IvfHeader header;
    memcpy(&header, &streamData[0], sizeof(header));
    if (memcmp(header.signature, "DKIF", 4) != 0)
    {
        Printf(Tee::PriError, "%s is not in IVF format\n",
               streamName);
        return RC::BAD_PARAMETER;
    }
    if (memcmp(header.codec, codecName, 4) != 0)
    {
        Printf(Tee::PriError, "%s is not a %s stream\n",
               streamName, codecName);
        return RC::BAD_PARAMETER;
    }

    // Read the IVF frames
    //
    IvfFrameHeader frameHeader;
    for (size_t framePos = header.headerLength;
         framePos < streamData.size();
         framePos += sizeof(frameHeader) + frameHeader.frameSize)
    {
        memcpy(&frameHeader, &streamData[framePos], sizeof(frameHeader));
        if (framePos + sizeof(frameHeader) + frameHeader.frameSize >
            streamData.size())
        {
            Printf(Tee::PriError,
                   "The data in %s goes past the end of the file\n",
                   streamName);
            return RC::BAD_PARAMETER;
        }
        bitstream_packet_s packet = {0};
        packet.pByteStream = &streamData[framePos + sizeof(frameHeader)];
        packet.nDataLength = frameHeader.frameSize;
        pPackets->push_back(packet);
    }
    if (!pPackets->empty())
    {
        pPackets->back().bEOS = LW_TRUE;
    }
    return OK;
}

//--------------------------------------------------------------------
//! \brief Split stream into a sequence of packets
//!
//! \param streamName The name of the stream file
//! \param streamData The raw data read from the stream file.
//! \param[out] pPackets On exit, contains an array of packets parsed
//!     from streamData.  Each packet contains up to 32 KB chunk. Note
//!     pPackets contains pointers into streamData, so the caller must
//!     not free streamData until he is done with pPackets.
//!
RC TegraLwdecTest::ParseGeneric
(
    const char* streamName,
    const vector<LwU8>& streamData,
    vector<bitstream_packet_s> *pPackets
)
{
    // Read stream into 32 KB packets similar to reference app (lwvideodecode_demo) instead
    // of one large packet. This will allow test to stop decoding after reaching ~ N frames
    // since control is at packet level
    constexpr UINT32 READ_SIZE = 32 * 1024;
    const UINT32 streamSize = streamData.size();
    UINT32 offset = 0;

    while (offset < streamSize)
    {
        bitstream_packet_s packet = {};
        const UINT32 residualDataSize = streamSize - offset;

        packet.nDataLength = (residualDataSize > READ_SIZE) ? READ_SIZE : residualDataSize;
        packet.pByteStream = &streamData[offset];
        offset += packet.nDataLength;
        pPackets->push_back(packet);
    }

    if (pPackets->empty())
    {
        Printf(Tee::PriError, "Unable to parse %s stream into packets. Stream size = %u\n",
            streamName, streamSize);
        return RC::ILWALID_INPUT;
    }

    pPackets->back().bEOS = LW_TRUE;
    return RC::OK;
}

//--------------------------------------------------------------------
//! \brief Update golden values and display the image
//!
//! This method is called from the DisplayPicture() callback.  It
//! reads the current picture (i.e. video frame) from TvmrDecoder, and
//! uses it to update the golden values and display the current frame
//! on the screen.
//!
RC TvmrDecoder::UpdateGoldens()
{
    RC rc;

    // Read the width/height/bitsPerPixel of the current frame.
    //
    UINT32 lumaWidth;
    UINT32 lumaHeight;
    UINT32 lumaDepth;
    UINT32 chromaWidth;
    UINT32 chromaHeight;
    UINT32 chromaDepth;
    CHECK_RC(GetOutputFormat(&lumaWidth, &lumaHeight, &lumaDepth,
                             &chromaWidth, &chromaHeight,
                             &chromaDepth));
    ColorUtils::Format lumaFormat = (lumaDepth <= 8 ?
                                     ColorUtils::Y8 : ColorUtils::R16);
    ColorUtils::Format chromaFormat = (chromaDepth <= 8 ?
                                       ColorUtils::U8V8 : ColorUtils::R16_G16);

    GpuDevice* const pGpuDevice = m_pTest->GetBoundGpuDevice();

    // Re-allocate our Surface2D objects if the width/height/depth have
    // changed, and call SetImage() on the new RGB surface.  This
    // should only happen at the start of a new stream, i.e. if the
    // BeginSequence() callback was ilwoked since the last time this
    // function was called.
    //
    if (!m_LumaSurf.IsAllocated() ||
        m_LumaSurf.GetWidth() != lumaWidth ||
        m_LumaSurf.GetHeight() != lumaHeight ||
        m_ChromaSurf.GetWidth() != chromaWidth ||
        m_ChromaSurf.GetHeight() != chromaHeight ||
        m_LumaSurf.GetColorFormat() != lumaFormat ||
        m_ChromaSurf.GetColorFormat() != chromaFormat)
    {
        Display *pDisplay = nullptr;

        if (m_Instance == TVMR_DECODER_INSTANCE_0 &&
            (!CheetAh::SocPtr()->HasBug(3286489)))
        {
            pDisplay = m_pTest->GetDisplay();
            pDisplay->SetImage(static_cast<Surface2D*>(nullptr));
        }

        m_LumaSurf.Free();
        m_LumaSurf.SetWidth(lumaWidth);
        m_LumaSurf.SetPitch(AlignUp(lumaWidth * ColorUtils::PixelBytes(lumaFormat),
                                    m_PitchAlign));
        m_LumaSurf.SetHeight(lumaHeight);
        m_LumaSurf.SetColorFormat(lumaFormat);
        m_LumaSurf.SetLocation(Memory::Coherent);
        CHECK_RC(m_LumaSurf.Alloc(pGpuDevice));

        m_ChromaSurf.Free();
        m_ChromaSurf.SetWidth(chromaWidth);
        m_ChromaSurf.SetPitch(AlignUp(chromaWidth * ColorUtils::PixelBytes(chromaFormat),
                                      m_PitchAlign));
        m_ChromaSurf.SetHeight(chromaHeight);
        m_ChromaSurf.SetColorFormat(chromaFormat);
        m_ChromaSurf.SetLocation(Memory::Coherent);
        CHECK_RC(m_ChromaSurf.Alloc(pGpuDevice));

        if (pDisplay && (!CheetAh::SocPtr()->HasBug(3286489)))
        {
            m_RgbSurf.Free();
            m_RgbSurf.SetWidth(lumaWidth);
            m_RgbSurf.SetHeight(lumaHeight);
            m_RgbSurf.SetColorFormat(ColorUtils::A8R8G8B8);
            m_RgbSurf.SetLocation(Memory::Fb);
            m_RgbSurf.SetDisplayable(true);
            CHECK_RC(m_RgbSurf.Alloc(pGpuDevice));

            CHECK_RC(pDisplay->SetImage(&m_RgbSurf));
        }
    }

    // Read the current frame into our Surface2D objects
    //
    const string dumpFileName = Utility::StrPrintf("Lwdec%u_Stream_%s_Loop%u.yuv",
        static_cast<UINT32>(m_Instance), m_LwrStreamName.c_str(), m_FrameCounter - 1);
    CHECK_RC(GetOutputData(&m_LumaSurf, &m_ChromaSurf,
                m_RgbSurf.IsAllocated() ? &m_RgbSurf : nullptr,
                m_pTest->GetSaveDecodedImages() ? dumpFileName.c_str() : nullptr));

    // Golden values are not thread-safe; some of the code below also
    // indirectly calls into JS, which is also not thread-safe.
    //
    Tasker::AttachThread attach;

    // Prevents both threads from accessing the same golden surfaces
    // object at the same time.  Golden values use DmaWrapper and Channel
    // class, which detach from the Tasker, so the above AttachThread
    // does not serve as a lock on its own.
    Tasker::MutexHolder lock(m_GoldenMutex);

    // Update the goldens
    //
    m_pGoldenSurfaces->ReplaceSurface(0, &m_LumaSurf);
    m_pGoldenSurfaces->ReplaceSurface(1, &m_ChromaSurf);
    const UINT32 loopHash = m_FrameCountBase + m_FrameCounter - 1;
    const string info = Utility::StrPrintf(
            "LWDEC %u, Stream %s, Loop %u, GV hash 0x%08x",
            static_cast<UINT32>(m_Instance), m_LwrStreamName.c_str(), m_FrameCounter - 1, loopHash);
    Printf(m_Pri, "%s\n", info.c_str());
    m_pGoldelwalues->SetLoop(loopHash);
    rc = m_pGoldelwalues->Run();
    if (rc != OK)
    {
        Printf(Tee::PriError, "Failed on %s rc = %s\n", info.c_str(), rc.Message());
        return rc;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief This callback is ilwoked at the start of a new video stream
//!
/* static */
LwS32 TegraLwdecTest::BeginSequence(void *pCtx, const LWDSequenceInfo *pSeq)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);
    LwS32 numBuffers = 0;
    const RC rc = pDecoder->BeginSequence(pSeq, &numBuffers);
    pDecoder->SetRC(rc);
    return (rc == OK) ? numBuffers : 0;
}

//--------------------------------------------------------------------
//! \brief This callback is ilwoked when there is a video frame to decode.
//!
//! This callback passes the frame to the LWDEC engine, but does not
//! wait for the frame to finish decoding.
//!
/* static */
LwBool TegraLwdecTest::DecodePicture(void *pCtx, LWDPictureData *pPictureData)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);

    // Once StopDecoding is set, we will flush video parser so frame data received could
    // be incomplete. Ignore any further DecodePicture calls
    if (pDecoder->GetStopDecoding())
        return LW_TRUE;

    const RC rc = pDecoder->DecodePicture(pPictureData);
    pDecoder->SetRC(rc);
    return (rc == OK) ? LW_TRUE : LW_FALSE;
}

//--------------------------------------------------------------------
//! \brief This callback is ilwoked when there is a video frame to display.
//!
/* static */ LwBool TegraLwdecTest::DisplayPicture
(
    void *pCtx,
    LWDPicBuff *pPicBuff,
    LwS64 pts
)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);

    // For non-ivf streams, single packet can translate to more than one frame. So it's possible
    // DisplayPicture is called for few more frames after Max frames to test. However, decoder
    // thread in DecodeStream could exit before these calls and hence won't be able to capture
    // RC. Hence avoid generating CRCs for these additional frames
    if (pDecoder->GetStopDecoding())
        return LW_TRUE;

    pDecoder->IncrementFrameCounter();

    // Wait for the LWDEC engine to finish rendering
    //
    RC rc = pDecoder->DisplayPicture(pPicBuff, pts);

    // Update golden values and the display
    //
    if (rc == OK)
        rc = pDecoder->UpdateGoldens();
    pDecoder->SetRC(rc);

    if (pDecoder->GetFrameCounter() == pDecoder->GetMaxFramesToTest())
    {
        pDecoder->SetStopDecoding(true);
    }
    return (rc == OK) ? LW_TRUE : LW_FALSE;
}

//--------------------------------------------------------------------
/* static */
void TegraLwdecTest::UnhandledNALU(void *pCtx, const LwU8 *p, LwS32 v)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);
    pDecoder->SetRC(pDecoder->UnhandledNALU(p, v));
}

//--------------------------------------------------------------------
//! \brief This callback allocates a new picture surface.
//!
//! lwparser uses reference counting to decide when to release a
//! picture.  Pictures are created with a reference count of 1.
//!
/* static */
LwBool TegraLwdecTest::AllocPictureBuffer(void *pCtx, LWDPicBuff **ppPicBuff)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);
    const RC rc = pDecoder->AllocPictureBuffer(ppPicBuff);
    pDecoder->SetRC(rc);
    return (rc == OK) ? LW_TRUE : LW_FALSE;
}

//--------------------------------------------------------------------
//! \brief This callback decrements a picture surface's reference count.
//!
//! The picture is freed when the reference count reaches 0.
//!
/* static */ void TegraLwdecTest::Release(void *pCtx, LWDPicBuff *pPicBuff)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);
    if (pDecoder->GetStopDecoding())
    {
        // WAR for Bug 200768466.
        // When AV1 decode is interrupted after N frames using video_parser_flush, lwparser seems
        // to be calling Release callback unexpectedly on an already released picbuff. This results
        // in asserts/crashes since resource is no longer available. So don't do actual release
        // once m_StopDecoding is set. Any pending resources are released as part of EndSequence
        return;
    }
    pDecoder->SetRC(pDecoder->Release(pPicBuff));
}

//--------------------------------------------------------------------
//! \brief This callback increments a picture surface's reference count.
//!
/* static */ void TegraLwdecTest::AddRef(void *pCtx, LWDPicBuff *pPicBuff)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);
    pDecoder->SetRC(pDecoder->AddRef(pPicBuff));
}

//--------------------------------------------------------------------
/* static */
LwBool TegraLwdecTest::CreateDecrypter(void *pCtx, LwU16 width, LwU16 height)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);
    const RC rc = pDecoder->CreateDecrypter(width, height);
    pDecoder->SetRC(rc);
    return (rc == OK) ? LW_TRUE : LW_FALSE;
}

//--------------------------------------------------------------------
/* static */
LwBool TegraLwdecTest::DecryptHdr(void *pCtx, LWDPictureData *pPictureData)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);
    const RC rc = pDecoder->DecryptHdr(pPictureData);
    pDecoder->SetRC(rc);
    return (rc == OK) ? LW_TRUE : LW_FALSE;
}

//--------------------------------------------------------------------
/* static */
LwBool TegraLwdecTest::GetHdr(void *pCtx, LWDPictureData *pPic, LwU8 *pNalType)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);
    const RC rc = pDecoder->GetHdr(pPic, pNalType);
    pDecoder->SetRC(rc);
    return (rc == OK) ? LW_TRUE : LW_FALSE;
}

//--------------------------------------------------------------------
/* static */
LwBool TegraLwdecTest::UpdateHdr(void *pCtx, LWDPictureData *pPic, LwU8 nalType)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);
    const RC rc = pDecoder->UpdateHdr(pPic, nalType);
    pDecoder->SetRC(rc);
    return (rc == OK) ? LW_TRUE : LW_FALSE;
}

//--------------------------------------------------------------------
/* static */
LwBool TegraLwdecTest::CopySliceData(void *pCtx, LWDPictureData *pPictureData)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);
    const RC rc = pDecoder->CopySliceData(pPictureData);
    pDecoder->SetRC(rc);
    return (rc == OK) ? LW_TRUE : LW_FALSE;
}

//--------------------------------------------------------------------
/* static */
LwBool TegraLwdecTest::GetClearHdr(void *pCtx, LWDPictureData *pPictureData)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);
    const RC rc = pDecoder->GetClearHdr(pPictureData);
    pDecoder->SetRC(rc);
    return (rc == OK) ? LW_TRUE : LW_FALSE;
}

//--------------------------------------------------------------------
/* static */ LwBool TegraLwdecTest::GetBackwardUpdates
(
    void *pCtx,
    LWDPictureData *pPictureData
)
{
    TvmrDecoder *pDecoder = Upcast(pCtx);
    const RC rc = pDecoder->GetBackwardUpdates(pPictureData);
    pDecoder->SetRC(rc);
    return (rc == OK) ? LW_TRUE : LW_FALSE;
}

// *******************************************************************
// TvmrDecoder
// *******************************************************************

//--------------------------------------------------------------------
TvmrDecoder::TvmrDecoder
(
    FLOAT64               timeoutMs,
    BitColwersion         bitColwersion,
    TegraLwdecTest*       pTest,
    Goldelwalues*         pGoldelwalues,
    GpuGoldenSurfaces*    pGoldenSurfaces,
    Tasker::Mutex&        goldenMutex,
    UINT32                pitchAlign,
    TVMRDecoderInstanceId instance,
    Tee::Priority         pri
) :
    m_TimeoutTvmr(timeoutMs + 0.5),
    m_BitColwersion(bitColwersion),
    m_Instance(instance),
    m_Pri(pri),
    m_pDevice(TVMRDeviceCreate(nullptr)),
    m_pDecoder(nullptr),
    m_pTest(pTest),
    m_pGoldelwalues(pGoldelwalues),
    m_pGoldenSurfaces(pGoldenSurfaces),
    m_GoldenMutex(goldenMutex),
    m_PitchAlign(pitchAlign)
{
    MASSERT(m_pDevice != nullptr);
    EndSequence();
}

//--------------------------------------------------------------------
TvmrDecoder::~TvmrDecoder()
{
    EndSequence();
    TVMRDeviceDestroy(m_pDevice);
}

//--------------------------------------------------------------------
//! \brief This callback is ilwoked at the start of a new video stream
//!
//! Internally, this callback extracts the stream parameters such as
//! width & height, and allocates a new TVMRVideoDecoder to process
//! the stream.
//!
RC TvmrDecoder::BeginSequence
(
    const LWDSequenceInfo *pSequenceInfo,
    LwS32 *pNumBuffers
)
{
    MASSERT(pSequenceInfo != nullptr);
    MASSERT(pNumBuffers != nullptr);
    EndSequence();

    // Get the stream parameters
    //
    m_LumaDims.streamWidth   = pSequenceInfo->nCodedWidth;
    m_LumaDims.streamHeight  = pSequenceInfo->nCodedHeight;
    m_LumaDims.outputWidth   = pSequenceInfo->nDisplayWidth;
    m_LumaDims.outputHeight  = pSequenceInfo->nDisplayHeight;
    m_LumaDims.streamDepth   = 8 + pSequenceInfo->uBitDepthLumaMinus8;
    m_ChromaDims.streamDepth = 8 + pSequenceInfo->uBitDepthChromaMinus8;

    // Derive m_ChromaDims, outputDepth, and m_SurfaceType
    // from chromaFormat and streamDepth
    //
    const TVMRSurfaceType BAD_SURFACE_TYPE =
                                      static_cast<TVMRSurfaceType>(0xffffffff);
    const UINT32 colwertedLumaDepth = (m_BitColwersion == BIT_COLWERT_NONE ?
                                       m_LumaDims.streamDepth : 8);
    const UINT32 colwertedChromaDepth = (m_BitColwersion == BIT_COLWERT_NONE ?
                                         m_ChromaDims.streamDepth : 8);
    const char *chromaFormatString = nullptr;  // For debug messages
    m_SurfaceType = BAD_SURFACE_TYPE;

    switch (pSequenceInfo->nChromaFormat)
    {
        case 1: // 4:2:0
            chromaFormatString        = "4:2:0";
            m_ChromaDims.streamWidth  = m_LumaDims.streamWidth  / 2;
            m_ChromaDims.streamHeight = m_LumaDims.streamHeight / 2;
            m_ChromaDims.outputWidth  = m_LumaDims.outputWidth  / 2;
            m_ChromaDims.outputHeight = m_LumaDims.outputHeight / 2;
            if (colwertedLumaDepth <= 8 && colwertedChromaDepth <= 8)
            {
                m_SurfaceType            = TVMRSurfaceType_Y_UV_420;
                m_LumaDims.outputDepth   = 8;
                m_ChromaDims.outputDepth = 8;
            }
            else if (colwertedLumaDepth <= 10 && colwertedChromaDepth <= 8)
            {
                m_SurfaceType            = TVMRSurfaceType_Y10_U8V8_420;
                m_LumaDims.outputDepth   = 10;
                m_ChromaDims.outputDepth = 8;
            }
            else if (colwertedLumaDepth <= 10 && colwertedChromaDepth <= 10)
            {
                m_SurfaceType            = TVMRSurfaceType_Y10_U10V10_420;
                m_LumaDims.outputDepth   = 10;
                m_ChromaDims.outputDepth = 10;
            }
            else if (colwertedLumaDepth <= 12 && colwertedChromaDepth <= 12)
            {
                m_SurfaceType            = TVMRSurfaceType_Y12_U12V12_420;
                m_LumaDims.outputDepth   = 12;
                m_ChromaDims.outputDepth = 12;
            }
            break;
        case 2: // 4:2:2
            chromaFormatString        = "4:2:2";
            m_ChromaDims.streamWidth  = m_LumaDims.streamWidth / 2;
            m_ChromaDims.streamHeight = m_LumaDims.streamHeight;
            m_ChromaDims.outputWidth  = m_LumaDims.outputWidth / 2;
            m_ChromaDims.outputHeight = m_LumaDims.outputHeight;
            if (colwertedLumaDepth <= 8 && colwertedChromaDepth <= 8)
            {
                m_SurfaceType            = TVMRSurfaceType_Y_UV_422;
                m_LumaDims.outputDepth   = 8;
                m_ChromaDims.outputDepth = 8;
            }
            else if (colwertedLumaDepth <= 10 && colwertedChromaDepth <= 10)
            {
                m_SurfaceType            = TVMRSurfaceType_Y10_U10V10_422;
                m_LumaDims.outputDepth   = 10;
                m_ChromaDims.outputDepth = 10;
            }
            else if (colwertedLumaDepth <= 12 && colwertedChromaDepth <= 12)
            {
                m_SurfaceType            = TVMRSurfaceType_Y12_U12V12_422;
                m_LumaDims.outputDepth   = 12;
                m_ChromaDims.outputDepth = 12;
            }
            break;
        case 3: // 4:4:4
            chromaFormatString = "4:4:4";
            m_ChromaDims.streamWidth  = m_LumaDims.streamWidth;
            m_ChromaDims.streamHeight = m_LumaDims.streamHeight;
            m_ChromaDims.outputWidth  = m_LumaDims.outputWidth;
            m_ChromaDims.outputHeight = m_LumaDims.outputHeight;
            if (colwertedLumaDepth <= 8 && colwertedChromaDepth <= 8)
            {
                m_SurfaceType            = TVMRSurfaceType_Y_UV_444;
                m_LumaDims.outputDepth   = 8;
                m_ChromaDims.outputDepth = 8;
            }
            else if (colwertedLumaDepth <= 10 && colwertedChromaDepth <= 10)
            {
                m_SurfaceType            = TVMRSurfaceType_Y10_U10V10_444;
                m_LumaDims.outputDepth   = 10;
                m_ChromaDims.outputDepth = 10;
            }
            else if (colwertedLumaDepth <= 12 && colwertedChromaDepth <= 12)
            {
                m_SurfaceType            = TVMRSurfaceType_Y12_U12V12_444;
                m_LumaDims.outputDepth   = 12;
                m_ChromaDims.outputDepth = 12;
            }
            break;
    }
    if (m_SurfaceType == BAD_SURFACE_TYPE)
    {
        Printf(Tee::PriError, "Unknown surface format\n");
        return RC::ILWALID_INPUT;
    }

    // Derive the lwtvmr codec from the lwparser codec
    //
    switch (pSequenceInfo->eCodec)
    {
        case LWCS_MPEG1:
            m_Codec = TVMR_VIDEO_CODEC_MPEG1;
            break;
        case LWCS_MPEG2:
            m_Codec = TVMR_VIDEO_CODEC_MPEG2;
            break;
        case LWCS_MPEG4:
            m_Codec = TVMR_VIDEO_CODEC_MPEG4;
            break;
        case LWCS_VC1:
            m_Codec = TVMR_VIDEO_CODEC_VC1;
            break;
        case LWCS_H264:
            m_Codec = TVMR_VIDEO_CODEC_H264;
            break;
        case LWCS_H264_MVC:
            m_Codec = TVMR_VIDEO_CODEC_H264_MVC;
            break;
        case LWCS_VP8:
            m_Codec = TVMR_VIDEO_CODEC_VP8;
            break;
        case LWCS_H265:
            m_Codec = TVMR_VIDEO_CODEC_HEVC;
            break;
        case LWCS_VP9:
            m_Codec = TVMR_VIDEO_CODEC_VP9;
            break;
        case LWCS_AV1:
            m_Codec = TVMR_VIDEO_CODEC_AV1;
            break;
        default:
            Printf(Tee::PriError, "Unknown codec\n");
            return RC::ILWALID_INPUT;
    }

    // Allocate the lwtvmr objects
    //
    const LwU16 maxReferences = 16; // Max allowed in hardware
    const LwU8 inputBuffering = 8;  // Stress HW with max pipeline
    LwU32 decoderFlags  = 0;
    if (m_LumaDims.streamDepth > 8 || m_ChromaDims.streamDepth > 8)
    {
        decoderFlags |= TVMR_VIDEO_DECODER_10BIT_DECODE;
        switch (m_BitColwersion)
        {
            case BIT_COLWERT_NONE:
                break;
            case BIT_COLWERT_709:
                decoderFlags |=
                    TVMR_VIDEO_DECODER_PIXEL_COLWERT_REC_709_10_TO_8_BIT;
                break;
            case BIT_COLWERT_2020:
                decoderFlags |=
                    TVMR_VIDEO_DECODER_PIXEL_COLWERT_REC_2020_10_TO_8_BIT;
                break;
        }
    }

    m_pDecoder = TVMRVideoDecoderCreate(m_Codec, m_LumaDims.streamWidth,
                                        m_LumaDims.streamHeight, maxReferences,
                                        pSequenceInfo->MaxBitstreamSize,
                                        inputBuffering, decoderFlags, m_Instance);
    if (m_pDevice == nullptr || m_pDecoder == nullptr)
    {
        Printf(Tee::PriError, "Decoder allocation failed");
        return RC::SOFTWARE_ERROR;
    }

    Printf(m_Pri, "LWDEC %u: %hux%hu %s, %u-bit luma, %u-bit chroma\n",
           m_Instance,
           m_LumaDims.streamWidth, m_LumaDims.streamHeight, chromaFormatString,
           m_LumaDims.streamDepth, m_ChromaDims.streamDepth);
    *pNumBuffers = pSequenceInfo->nDecodeBuffers;
    return OK;
}

//--------------------------------------------------------------------
//! \brief This callback is ilwoked when there is a video frame to decode.
//!
//! This callback passes the frame to the LWDEC engine, but does not
//! wait for the frame to finish decoding.
//!
RC TvmrDecoder::DecodePicture(LWDPictureData *pPictureData)
{
    MASSERT(pPictureData != nullptr);
    RC rc;

    // Copy the header provided by lwparser into a data struct used by
    // TVMRVideoDecoderRender().  Note that TVMRVideoDecoderRender()
    // takes a different struct for each codec.
    //
    union
    {
        TVMRPictureInfoMPEG  mpeg;
        TVMRPictureInfoMPEG4 mpeg4;
        TVMRPictureInfoVC1   vc1;
        TVMRPictureInfoH264  h264;
        TVMRPictureInfoVP8   vp8;
        TVMRPictureInfoHEVC  h265;
        TVMRPictureInfoVP9   vp9;
        TVMRPictureInfoAV1   av1;
    } pictureInfo;
    MyPicBuff *pLwrrPic = MyPicBuff::Upcast(pPictureData->pLwrrPic);
    switch (m_Codec)
    {
        case TVMR_VIDEO_CODEC_MPEG1:
        case TVMR_VIDEO_CODEC_MPEG2:
            CHECK_RC(SetPictureInfoMpeg(&pictureInfo.mpeg, *pPictureData, pLwrrPic));
            break;
        case TVMR_VIDEO_CODEC_MPEG4:
            CHECK_RC(SetPictureInfoMpeg4(&pictureInfo.mpeg4, *pPictureData, pLwrrPic));
            break;
        case TVMR_VIDEO_CODEC_VC1:
            CHECK_RC(SetPictureInfoVc1(&pictureInfo.vc1, *pPictureData, pLwrrPic));
            break;
        case TVMR_VIDEO_CODEC_H264:
        case TVMR_VIDEO_CODEC_H264_MVC:
            CHECK_RC(SetPictureInfoH264(&pictureInfo.h264, *pPictureData, pLwrrPic));
            break;
        case TVMR_VIDEO_CODEC_VP8:
            CHECK_RC(SetPictureInfoVp8(&pictureInfo.vp8, *pPictureData, pLwrrPic));
            break;
        case TVMR_VIDEO_CODEC_HEVC:
            CHECK_RC(SetPictureInfoH265(&pictureInfo.h265, *pPictureData, pLwrrPic));
            break;
        case TVMR_VIDEO_CODEC_VP9:
            CHECK_RC(SetPictureInfoVp9(&pictureInfo.vp9, *pPictureData, pLwrrPic));
            break;
        case TVMR_VIDEO_CODEC_AV1:
            CHECK_RC(SetPictureInfoAv1(&pictureInfo.av1, pPictureData, pLwrrPic));
            break;
        default:
            MASSERT(!"Unknown codec");
            return RC::SOFTWARE_ERROR;
    }

    // Create a lwtvmr wrapper around the encoded bitstream provided
    // by lwparser.
    //
    TVMRBitstreamBuffer bitstreamBuffer = {0};
    bitstreamBuffer.bitstream = pPictureData->pBitstreamData;
    bitstreamBuffer.bitstreamBytes = pPictureData->nBitstreamDataLen;

    // Use AddRef() to make sure the prerequisite frames don't get
    // freed until we're done rendering.
    //
    for (MyPicBuff *pPrereq: pLwrrPic->GetPrereqs())
    {
        CHECK_RC(AddRef(pPrereq));
    }

    // Call TVMRVideoDecoderRender() to pass the frame to the LWDEC
    // engine.
    //
    CHECK_RC(StatusToRc(TVMRVideoDecoderRender(m_pDecoder,
                                               pLwrrPic->GetSurface(),
                                               &pictureInfo,
                                               1,
                                               &bitstreamBuffer,
                                               pLwrrPic->GetPrereqFences(),
                                               pLwrrPic->GetTvmrFence(),
                                               m_Instance)));
    m_FramesScheduled++;
    return rc;
}

/* static */ bool TvmrDecoder::PollAllFramesDecoded(void* pPollArgs)
{
    TvmrDecoder *pDecoder = static_cast<TvmrDecoder*>(pPollArgs);
    return ((pDecoder->m_FramesScheduled == pDecoder->m_FrameCounter) ||
            (pDecoder->GetRC() != RC::OK));
}

RC TvmrDecoder::WaitForPendingFrames()
{
    RC rc;
    const UINT32 remainingFrames = m_FramesScheduled - m_FrameCounter;

    if (remainingFrames != 0)
    {
        // Wait for any pending frames to complete
        // We cannot wait on a fence because we don't know last pic to be displayed. Note that
        // decode order may not match with display order. Guidance is to use POLLWRAP_HW instead
        // of sleep to return quickly once decode is complete. Provide a generous timeout to allow
        // for processing (decode, CRC check etc.) to complete
        CHECK_RC(POLLWRAP_HW(PollAllFramesDecoded, this, remainingFrames * 10 * m_TimeoutTvmr));
    }

    CHECK_RC(GetRC());
    return rc;
}

//--------------------------------------------------------------------
//! \brief This callback is ilwoked when there is a video frame to display.
//!
//! This method waits for the LWDEC engine to finish rendering the
//! frame, and then makes the data available to GetOutputData().
//!
RC TvmrDecoder::DisplayPicture(LWDPicBuff *pPicBuff, LwS64 pts)
{
    MyPicBuff *pMyPicBuff = MyPicBuff::Upcast(pPicBuff);
    MASSERT(pPicBuff != nullptr);
    RC rc;

    // Wait for the LWDEC engine to finish rendering the picture
    //
    int status = TVMRFenceBlockTimeout(m_pDevice, pMyPicBuff->GetTvmrFence(),
                                       m_TimeoutTvmr);
    if (status == 0)
    {
        return RC::TIMEOUT_ERROR;
    }

    // Release the prerequisite images; we only needed them during
    // rendering.
    //
    for (MyPicBuff *pPrereq: pMyPicBuff->GetPrereqs())
    {
        CHECK_RC(Release(pPrereq));
    }
    pMyPicBuff->ClearPrereqs();

    // Replace m_pOutputPicture with the new picture.  Use AddRef() to
    // make sure we don't delete the picture until either the next
    // frame is available, or we're done with the stream.
    //
    if (m_pOutputPicture)
    {
        MyPicBuff *pOldOutputPicture = m_pOutputPicture;
        m_pOutputPicture = nullptr;
        CHECK_RC(Release(pOldOutputPicture));
    }
    CHECK_RC(AddRef(pMyPicBuff));
    m_pOutputPicture = pMyPicBuff;
    return rc;
}

//--------------------------------------------------------------------
RC TvmrDecoder::UnhandledNALU(const LwU8 *, LwS32)
{
    MASSERT(!"UnhandledNALU not implemented");
    return RC::UNSUPPORTED_FUNCTION;
}

void TvmrDecoder::ReleaseFence(UINT32 fenceIdx)
{
    m_Fences[fenceIdx]->DelRef();
}

void TvmrDecoder::AddFenceRef(UINT32 fenceIdx)
{
    m_Fences[fenceIdx]->AddRef();
}

//--------------------------------------------------------------------
//! \brief This callback allocates a new picture surface with a refcount of 1
//!
RC TvmrDecoder::AllocPictureBuffer(LWDPicBuff **ppPicBuff)
{
    MASSERT(ppPicBuff != nullptr);
    const UINT32 picIdx = m_Pictures.size();
    const UINT32 fenceIdx = m_Fences.size();

    m_Fences.push_back(make_unique<Fence>());
    m_Pictures.push_back(make_unique<MyPicBuff>(m_pDevice,
                                                m_SurfaceType,
                                                m_LumaDims.streamWidth,
                                                m_LumaDims.streamHeight,
                                                m_Fences[fenceIdx]->GetTvmrFence(),
                                                picIdx,
                                                fenceIdx));
    *ppPicBuff = m_Pictures[picIdx].get();
    return OK;
}

//--------------------------------------------------------------------
//! \brief This callback decrements a picture surface's reference count.
//!
//! The picture is freed when the reference count reaches 0.
//!
RC TvmrDecoder::Release(LWDPicBuff *pPicBuff)
{
    StickyRC rc;
    MASSERT(pPicBuff != nullptr);
    MyPicBuff *pMyPicBuff = MyPicBuff::Upcast(pPicBuff);
    if (pMyPicBuff->DelRef() == 0)
    {
        MASSERT(m_pOutputPicture != pMyPicBuff); // Should be impossible
        rc = FreePicBuff(pMyPicBuff);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Free PicBuff resource
//!
//! Free PicBuff resource. If fence has no references, release fence as well
RC TvmrDecoder::FreePicBuff(MyPicBuff *pMyPicBuff)
{
    MASSERT(pMyPicBuff != nullptr);
    if (pMyPicBuff == nullptr)
        return RC::SOFTWARE_ERROR;
    ReleaseFence(pMyPicBuff->GetFenceIdx());
    m_Pictures[pMyPicBuff->GetPicBuffIdx()].reset();
    return RC::OK;
}

//--------------------------------------------------------------------
//! \brief This callback increments a picture surface's reference count.
//!
RC TvmrDecoder::AddRef(LWDPicBuff *pPicBuff)
{
    MASSERT(pPicBuff != nullptr);
    MyPicBuff::Upcast(pPicBuff)->AddRef();
    return OK;
}

//--------------------------------------------------------------------
RC TvmrDecoder::CreateDecrypter(LwU16 width, LwU16 height)
{
    MASSERT(!"CreateDecrypter not implemented");
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------
RC TvmrDecoder::DecryptHdr(LWDPictureData *pPictureData)
{
    MASSERT(!"DecrypeHdr not implemented");
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------
RC TvmrDecoder::GetHdr(LWDPictureData *pPictureData, LwU8 *pNalType)
{
    MASSERT(!"GetHdr not implemented");
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------
RC TvmrDecoder::UpdateHdr(LWDPictureData *pPictureData, LwU8 nalType)
{
    MASSERT(!"UpdateHdr not implemented");
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------
RC TvmrDecoder::CopySliceData(LWDPictureData *pPictureData)
{
    MASSERT(!"CopySliceData not implemented");
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------
RC TvmrDecoder::GetClearHdr(LWDPictureData *pPictureData)
{
    MASSERT(!"GetClearHdr not implemented");
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------
RC TvmrDecoder::GetBackwardUpdates(LWDPictureData *pPictureData)
{
    RC rc;
    switch (m_Codec)
    {
        case TVMR_VIDEO_CODEC_VP9:
            CHECK_RC(StatusToRc(TVMRVideoBackwardUpdates(
                            m_pDecoder,
                            &pPictureData->CodecSpecific.vp9.buCounts)));
            break;
        default:
            MASSERT(!"GetBackwardUpdates not implemented for this codec");
            CHECK_RC(RC::UNSUPPORTED_FUNCTION);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Returns the format of the data returned by GetOutputData()
//!
//! This method returns the width, height, and depth of the surfaces
//! read by GetOutputData().
//!
//! The width, height, and depth can be read any time after
//! BeginSequence() is called.  They should not change until the next
//! BeginSequence(), i.e. until the next video stream begins.
//!
RC TvmrDecoder::GetOutputFormat
(
    UINT32 *pLumaWidth,
    UINT32 *pLumaHeight,
    UINT32 *pLumaDepth,
    UINT32 *pChromaWidth,
    UINT32 *pChromaHeight,
    UINT32 *pChromaDepth
)
{
    MASSERT(pLumaWidth != nullptr);
    MASSERT(pLumaHeight != nullptr);
    MASSERT(pLumaDepth != nullptr);
    MASSERT(pChromaWidth != nullptr);
    MASSERT(pChromaHeight != nullptr);
    MASSERT(pChromaDepth != nullptr);

    *pLumaWidth    = m_LumaDims.outputWidth;
    *pLumaHeight   = m_LumaDims.outputHeight;
    *pLumaDepth    = m_LumaDims.outputDepth;
    *pChromaWidth  = m_ChromaDims.outputWidth;
    *pChromaHeight = m_ChromaDims.outputHeight;
    *pChromaDepth  = m_ChromaDims.outputDepth;
    return OK;
}

//--------------------------------------------------------------------
//! \brief Copies the current frame into Surface2D objects
//!
//! This method may be called after DisplayPicture() to read the
//! contents of the frame being displayed.
//!
//! The caller should use GetOutputFormat() to determine the width,
//! height, and depth of the surfaces to pass to this method.  All of
//! the above surfaces are pitch-linear.  If depth > 8, then the YUV
//! components are left-aligned, i.e. the least-significant 6 bits are
//! 0.  Any of the arguments may be nullptr, in which case this method
//! will not fill those surfaces.
//!
//! \param[out] pLumaSurf Contains the Y components of the YUV image.
//! \param[out] pChromaSurf Contains the U and V of the YUV image,
//!     sampled at half the width and half the height of the Y image.
//!     Each row consists of alternating U and V, in that order.
//! \param[out] pRgbSurf Contains an A8R8G8B8 image suitable for
//!     display.  Note that LWDEC produces YUV output; pRgb is just a
//!     quick'n'dirty colwersion to RGB so that the user can see
//!     pretty pictures on the screen.  It should not be used for GVs
//!     and such.  We take time-saving shortlwts such as just copying
//!     YUV to GBR (in that order) instead of doing a full translation
//!     matrix.
//!
RC TvmrDecoder::GetOutputData
(
    Surface2D *pLumaSurf,
    Surface2D *pChromaSurf,
    Surface2D *pRgbSurf,
    const char* dumpFileName
)
{
    RC rc;

    if (m_pOutputPicture == nullptr)
    {
        MASSERT(!"No output data available");
        return RC::SOFTWARE_ERROR;
    }

    // Read the YUV data from m_pOutputPicture into lumaData[] and
    // chromaData[] vectors.
    //
    const UINT32 lumaBpp           = (m_LumaDims.outputDepth + 7) / 8;
    const UINT32 chromaBpp         = (m_ChromaDims.outputDepth + 7) / 8;
    const UINT32 lumaStreamPitch   = m_LumaDims.streamWidth * lumaBpp;
    const UINT32 chromaStreamPitch = m_ChromaDims.streamWidth * chromaBpp * 2;

    vector<UINT08> lumaData(lumaStreamPitch * m_LumaDims.streamHeight);
    vector<UINT08> chromaData(chromaStreamPitch * m_ChromaDims.streamHeight);
    void *dstPointers[2] = { &lumaData[0], &chromaData[0] };
    LwU32 dstPitches[2]  = { lumaStreamPitch, chromaStreamPitch };

    CHECK_RC(StatusToRc(TVMRVideoSurfaceGetBits(
                            m_pOutputPicture->GetSurface(),
                            dstPointers, dstPitches)));

    if (dumpFileName)
    {
        FileHolder file;
        CHECK_RC(file.Open(dumpFileName, "wb"));
        if (lumaData.size() != fwrite(lumaData.data(), 1, lumaData.size(), file.GetFile()))
        {
            Printf(Tee::PriError, "TegraLwdec: Failed to save Luma surface to %s\n",
                dumpFileName);
            return RC::FILE_WRITE_ERROR;
        }
        if (chromaData.size() != fwrite(chromaData.data(), 1, chromaData.size(), file.GetFile()))
        {
            Printf(Tee::PriError, "TegraLwdec: Failed to save Chroma surface to %s\n",
                dumpFileName);
            return RC::FILE_WRITE_ERROR;
        }
    }

    // Copy lumaData into *pLumaSurf
    //
    if (pLumaSurf != nullptr)
    {
        const UINT32 outputBytesPerRow = m_LumaDims.outputWidth * lumaBpp;
        if (m_LumaDims.outputHeight != pLumaSurf->GetHeight() ||
            outputBytesPerRow != (pLumaSurf->GetWidth() *
                                  pLumaSurf->GetBytesPerPixel()))
        {
            MASSERT(!"Bad luma dimensions passed to GetDisplayData");
            return RC::SOFTWARE_ERROR;
        }
        SurfaceUtils::MappingSurfaceWriter writer(*pLumaSurf);
        for (UINT32 row = 0; row < m_LumaDims.outputHeight; ++row)
        {
            CHECK_RC(writer.WriteBytes(row * pLumaSurf->GetPitch(),
                                       &lumaData[row * lumaStreamPitch],
                                       outputBytesPerRow));
        }
    }

    // Copy chromaData into *pChromaSurf
    //
    if (pChromaSurf != nullptr)
    {
        const UINT32 outputBytesPerRow =
            m_ChromaDims.outputWidth * chromaBpp * 2;
        if (m_ChromaDims.outputHeight != pChromaSurf->GetHeight() ||
            outputBytesPerRow != (pChromaSurf->GetWidth() *
                                  pChromaSurf->GetBytesPerPixel()))
        {
            MASSERT(!"Bad chroma dimensions passed to GetDisplayData");
            return RC::SOFTWARE_ERROR;
        }
        SurfaceUtils::MappingSurfaceWriter writer(*pChromaSurf);
        for (UINT32 row = 0; row < m_ChromaDims.outputHeight; ++row)
        {
            CHECK_RC(writer.WriteBytes(row * pChromaSurf->GetPitch(),
                                       &chromaData[row * chromaStreamPitch],
                                       outputBytesPerRow));
        }
    }

    // Do a quick'n'dirty copy of lumaData & chromaData into *pRgbSurf.
    //
    if (pRgbSurf != nullptr)
    {
        const UINT32 verticalChromaDownsampling =
            m_LumaDims.outputHeight / m_ChromaDims.outputHeight;
        const UINT32 horizontalChromaDownsampling =
            m_LumaDims.outputWidth / m_ChromaDims.outputWidth;
        const UINT32 lumaStride = (m_LumaDims.outputDepth + 7) / 8;
        const UINT32 chromaStride = (m_ChromaDims.outputDepth + 7) / 8;
        vector<UINT32> rgbData(m_LumaDims.outputWidth);

        if (4 * m_LumaDims.outputWidth != (pRgbSurf->GetWidth() *
                                           pRgbSurf->GetBytesPerPixel()) ||
            m_LumaDims.outputHeight != pRgbSurf->GetHeight())
        {
            MASSERT(!"Bad RGB dimensions passed to GetDisplayData");
            return RC::SOFTWARE_ERROR;
        }

        SurfaceUtils::MappingSurfaceWriter writer(*pRgbSurf);
        for (UINT32 row = 0; row < m_LumaDims.outputHeight; ++row)
        {
            UINT08 *pLuma = &lumaData[row * lumaStreamPitch + lumaStride - 1];
            UINT08 *pChroma = &chromaData[(row / verticalChromaDownsampling) *
                                          chromaStreamPitch + chromaStride - 1];
            if (horizontalChromaDownsampling == 1)
            {
                for (UINT32 col = 0; col < m_LumaDims.outputWidth; ++col)
                {
                    const UINT32 y = *pLuma;
                    const UINT32 u = *pChroma;
                    const UINT32 v = pChroma[chromaStride];
                    rgbData[col] = (0xff << 24) | (v << 16) | (y << 8) | u;
                    pLuma += lumaStride;
                    pChroma += chromaStride * 2;
                }
            }
            else if (horizontalChromaDownsampling == 2)
            {
                for (UINT32 col = 0; col < m_LumaDims.outputWidth; col += 2)
                {
                    const UINT32 y1 = *pLuma;
                    const UINT32 y2 = pLuma[lumaStride];
                    const UINT32 u = *pChroma;
                    const UINT32 v = pChroma[chromaStride];
                    rgbData[col]     = (0xff << 24) | (v << 16) | (y1 << 8) | u;
                    rgbData[col + 1] = (0xff << 24) | (v << 16) | (y2 << 8) | u;
                    pLuma += lumaStride * 2;
                    pChroma += chromaStride * 2;
                }
            }
            else
            {
                for (UINT32 col = 0; col < m_LumaDims.outputWidth; ++col)
                {
                    const UINT32 y1 = *pLuma;
                    const UINT32 u = *pChroma;
                    const UINT32 v = pChroma[chromaStride];
                    rgbData[col] = (0xff << 24) | (v << 16) | (y1 << 8) | u;
                    pLuma += lumaStride;
                    if ((col + 1) % horizontalChromaDownsampling == 0)
                        pChroma += chromaStride * 2;
                }
            }
            CHECK_RC(writer.WriteBytes(row * pRgbSurf->GetPitch(),
                                       &rgbData[0],
                                       sizeof(rgbData[0]) * rgbData.size()));
        }
    }

    return rc;
}

//--------------------------------------------------------------------
TvmrDecoder::MyPicBuff::MyPicBuff
(
    TVMRDevice *pDevice,
    TVMRSurfaceType type,
    LwU16 width,
    LwU16 height,
    TVMRFence tvmrFence,
    UINT32 picBuffIdx,
    UINT32 fenceIdx
) :
    m_pSurface(TVMRVideoSurfaceCreate(pDevice, type, width, height, 0)),
    m_TvmrFence(tvmrFence),
    m_RefCount(1),
    m_PicBuffIdx(picBuffIdx),
    m_FenceIdx(fenceIdx)
{
    MASSERT(m_pSurface != nullptr);
    MASSERT(m_TvmrFence != nullptr);
}

//--------------------------------------------------------------------
TvmrDecoder::MyPicBuff::~MyPicBuff()
{
    TVMRVideoSurfaceDestroy(m_pSurface);
}

TvmrDecoder::Fence::Fence() :
    m_TvmrFence(TVMRFenceCreate())
{
}

TvmrDecoder::Fence::~Fence()
{
    if (m_RefCount != 0)
    {
        TVMRFenceDestroy(m_TvmrFence);
    }
}

void TvmrDecoder::Fence::AddRef()
{
    MASSERT(m_RefCount > 0);
    ++m_RefCount;
}

void TvmrDecoder::Fence::DelRef()
{
    MASSERT(m_RefCount > 0);
    --m_RefCount;

    if (m_RefCount == 0)
    {
        TVMRFenceDestroy(m_TvmrFence);
    }
}

RC TvmrDecoder::ShareFence(MyPicBuff *pSrcPicBuff, MyPicBuff *pDstPicBuff)
{
    RC rc;
    MASSERT(pSrcPicBuff);
    MASSERT(pDstPicBuff);

    if (pSrcPicBuff == nullptr || pDstPicBuff == nullptr)
        return RC::SOFTWARE_ERROR;

    if (pSrcPicBuff->GetFenceIdx() == pDstPicBuff->GetFenceIdx())
    {
        return rc;
    }

    // Release destination picBuff's fence before reassigning to avoid resource leak
    ReleaseFence(pDstPicBuff->GetFenceIdx());
    pDstPicBuff->SetFence(pSrcPicBuff->GetFenceIdx(), pSrcPicBuff->GetTvmrFence());
    AddFenceRef(pDstPicBuff->GetFenceIdx());
    return rc;
}

//--------------------------------------------------------------------
void TvmrDecoder::MyPicBuff::SetFence(UINT32 fenceIdx, TVMRFence tvmrFence)
{
    MASSERT((m_RefCount == 1));
    m_FenceIdx = fenceIdx;
    m_TvmrFence = tvmrFence;
}

//--------------------------------------------------------------------
//! Add a prerequisite picture
//!
//! Add a picture that must be rendered before this frame can be
//! rendered; usually a reference frame.
//!
//! This routine is normally called from the SetPictureInfo*()
//! functions that translate lwparser data into TVMR data.  So to
//! reduce boilerplate code in the caller, it accepts the reference
//! frame as an lwparser pointer, returns the TVMR surface, and
//! ignores null pointers.
//!
TVMRVideoSurface *TvmrDecoder::MyPicBuff::AddPrereq(LWDPicBuff *pPrereq)
{
    if (pPrereq == nullptr)
        return nullptr;
    MyPicBuff *pMyPrereq = Upcast(pPrereq);
    m_Prereqs.push_back(pMyPrereq);
    return pMyPrereq->GetSurface();
}

//--------------------------------------------------------------------
//! Create a NULL-terminated array of fences for the prerequisite frames
//!
//! The output of this method is passed to TVMRVideoDecoderRender(),
//! to ensure that TVMR doesn't start rendering this frame until the
//! prerequisites are done rendering.
//!
TVMRFence *TvmrDecoder::MyPicBuff::GetPrereqFences()
{
    m_PrereqFences.resize(m_Prereqs.size() + 1);
    for (size_t ii = 0; ii < m_Prereqs.size(); ++ii)
        m_PrereqFences[ii] = m_Prereqs[ii]->GetTvmrFence();
    m_PrereqFences[m_Prereqs.size()] = nullptr;
    return m_PrereqFences.data();
}

//--------------------------------------------------------------------
//! \brief Cleanup everything set by BeginSequence()
//!
//! Free all the objects that were allocated by BeginSequence(), and
//! restore the variables that were set during decoding to their
//! quiescent post-constructor values.
//!
void TvmrDecoder::EndSequence()
{
    m_Codec       = TVMR_VIDEO_CODEC_H264;
    m_SurfaceType = TVMRSurfaceType_YV12;
    memset(&m_LumaDims,   0, sizeof(m_LumaDims));
    memset(&m_ChromaDims, 0, sizeof(m_ChromaDims));

    if (m_pDecoder)
    {
        TVMRVideoDecoderDestroy(m_pDecoder);
        m_pDecoder = nullptr;
    }

    m_Pictures.clear();
    m_Fences.clear();
    m_pOutputPicture = nullptr;
}

//--------------------------------------------------------------------
//! \brief Translate a TVMRStatus to a mods RC
//!
/* static */ RC TvmrDecoder::StatusToRc(TVMRStatus status)
{
    switch (status)
    {
        case TVMR_STATUS_OK:            return OK;
        case TVMR_STATUS_BAD_PARAMETER: return RC::BAD_PARAMETER;
        case TVMR_STATUS_PENDING:       return RC::CODEC_NOT_READY;
        case TVMR_STATUS_NONE_PENDING:  return RC::NO_LOOP_IN_PROGRESS;
        case TVMR_STATUS_INSUFFICIENT_BUFFERING:
                                        return RC::BUFFER_ALLOCATION_ERROR;
        case TVMR_STATUS_TIMED_OUT:     return RC::TIMEOUT_ERROR;
        case TVMR_STATUS_UNSUPPORTED:   return RC::UNSUPPORTED_FUNCTION;
        case TVMR_STATUS_BAD_ALLOC:     return RC::CANNOT_ALLOCATE_MEMORY;
        case TVMR_STATUS_SUBMIT_ERROR:  return RC::ILWALID_INPUT;
        default:                        return RC::SOFTWARE_ERROR;
    }
}

//--------------------------------------------------------------------
//! \brief Copy encryption params from lwparser format to lwtvmr format
//!
//! This method is used by SetPictureInfo*()
//!
/* static */ RC TvmrDecoder::SetEncryptParams
(
    TVMREncryptionParams *pOut,
    const LWDEncryptParams &in
)
{
    TVMRDrmType drmMode = TVMRDrmNone;
    switch (in.drmMode)
    {
        case LWDrmNetflix:          drmMode = TVMRDrmNetflix;      break;
        case LWDrmWidevine:         drmMode = TVMRDrmWidevine;     break;
        case LWDrmUltraviolet:      drmMode = TVMRDrmUltraviolet;  break;
        case LWDrmPiffCbc:          drmMode = TVMRDrmPiffCbc;      break;
        case LWDrmPiffCtr:          drmMode = TVMRDrmPiffCtr;      break;
        case LWDrmMarlinCbc:        drmMode = TVMRDrmMarlinCbc;    break;
        case LWDrmMarlinCtr:        drmMode = TVMRDrmMarlinCtr;    break;
        case LWDrmWidevineCtr:      drmMode = TVMRDrmWidevineCtr;  break;
        case LWDrmClearAsEncrypted: drmMode = TVMRDrmClrAsEncrypt; break;
        case LWDrmNone:             drmMode = TVMRDrmNone;         break;
        default:
            Printf(Tee::PriError, "Unknown DRM mode\n");
            return RC::ILWALID_INPUT;
    }

    pOut->enableEncryption        = in.enableEncryption;
    // pOut->KeySlotNumber TODO
    pOut->drmMode                 = drmMode;
    MEM_COPY(pOut->InitVector,      in.InitVector);
    MEM_COPY(pOut->IvValid,         in.IvValid);
    pOut->BytesOfEncryptedData    = in.BytesOfEncryptedData;
    MEM_COPY(pOut->ClearBytes,      in.BOCD);
    MEM_COPY(pOut->EncryptedBytes,  in.BOED);
    pOut->NumNals                 = in.numNals;
    // pOut->key[16]       TODO
    pOut->AesPass1OutputBuffer    = in.AesPass1OutputBuffer;
    return OK;
}

//--------------------------------------------------------------------
//! \brief Copy MPEG picture info from lwparser format to lwtvmr format
//!
//! This method is used by DecodePicture() for MPEG1 and MPEG2
//! streams, to translate the picture header parsed by lwparser into
//! the struct needed by lwtvmr.
//!
RC TvmrDecoder::SetPictureInfoMpeg
(
    TVMRPictureInfoMPEG  *pOut,
    const LWDPictureData &in,
    MyPicBuff            *pLwrrPic
)
{
    MASSERT(pOut != nullptr);
    const LWMPEG2PictureData &inMpg = in.CodecSpecific.mpeg2;
    RC rc;

    memset(pOut, 0, sizeof(*pOut));
    pOut->forwardReference  = pLwrrPic->AddPrereq(inMpg.pForwardRef);
    pOut->backwardReference = pLwrrPic->AddPrereq(inMpg.pBackwardRef);

    pOut->picture_structure          = inMpg.picture_structure;
    pOut->picture_coding_type        = inMpg.picture_coding_type;
    pOut->intra_dc_precision         = inMpg.intra_dc_precision;
    pOut->frame_pred_frame_dct       = inMpg.frame_pred_frame_dct;
    pOut->concealment_motion_vectors = inMpg.concealment_motion_vectors;
    pOut->intra_vlc_format           = inMpg.intra_vlc_format;
    pOut->alternate_scan             = inMpg.alternate_scan;
    pOut->q_scale_type               = inMpg.q_scale_type;
    pOut->top_field_first            = in.top_field_first;
    pOut->full_pel_forward_vector    = inMpg.full_pel_forward_vector;
    pOut->full_pel_backward_vector   = inMpg.full_pel_backward_vector;

    ct_assert(CT_NUMELEMS(pOut->f_code) == CT_NUMELEMS(inMpg.f_code));
    ct_assert(CT_NUMELEMS(pOut->f_code[0]) == CT_NUMELEMS(inMpg.f_code[0]));
    for (unsigned ii = 0; ii < NUMELEMS(pOut->f_code); ++ii)
    {
        for (unsigned jj = 0; jj < NUMELEMS(pOut->f_code[0]); ++jj)
        {
            pOut->f_code[ii][jj] = inMpg.f_code[ii][jj];
        }
    }

    MEM_COPY(pOut->intra_quantizer_matrix,          inMpg.QuantMatrixIntra);
    MEM_COPY(pOut->non_intra_quantizer_matrix,      inMpg.QuantMatrixInter);
    CHECK_RC(SetEncryptParams(&pOut->encryptParams, inMpg.encryptParams));
    // pOut->pAesData             TODO
    // pOut->LwrrentFreq_VDE      TODO
    // pOut->LwrrentFreq_EMC      TODO
    pOut->nNumSlices        = in.nNumSlices;
    pOut->pSliceDataOffsets = in.pSliceDataOffsets;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Copy MPEG4 picture info from lwparser format to lwtvmr format
//!
//! This method is used by DecodePicture() for MPEG4 streams, to
//! translate the picture header parsed by lwparser into the struct
//! needed by lwtvmr.
//!
RC TvmrDecoder::SetPictureInfoMpeg4
(
    TVMRPictureInfoMPEG4 *pOut,
    const LWDPictureData &in,
    MyPicBuff            *pLwrrPic
)
{
    MASSERT(pOut != nullptr);
    const LWMPEG4PictureData &inMp4 = in.CodecSpecific.mpeg4;
    RC rc;

    memset(pOut, 0, sizeof(*pOut));
    pOut->forwardReference  = pLwrrPic->AddPrereq(inMp4.pForwardRef);
    pOut->backwardReference = pLwrrPic->AddPrereq(inMp4.pBackwardRef);

    MEM_COPY(pOut->TRD,                   inMp4.trd);
    MEM_COPY(pOut->TRB,                   inMp4.trb);
    pOut->vop_time_increment_resolution = inMp4.vop_time_increment_resolution;
    pOut->vop_time_increment_bitcount   = inMp4.vop_time_increment_bitcount;
    pOut->vop_coding_type               = inMp4.vop_coding_type;
    pOut->vop_fcode_forward             = inMp4.vop_fcode_forward;
    pOut->vop_fcode_backward            = inMp4.vop_fcode_backward;
    pOut->resync_marker_disable         = inMp4.resync_marker_disable;
    pOut->interlaced                    = inMp4.interlaced;
    pOut->quant_type                    = inMp4.quant_type;
    pOut->quarter_sample                = inMp4.quarter_sample;
    pOut->short_video_header            = inMp4.short_video_header;
    pOut->rounding_control              = inMp4.vop_rounding_type;
    pOut->alternate_vertical_scan_flag  = inMp4.alternate_vertical_scan_flag;
    pOut->top_field_first               = in.top_field_first;
    MEM_COPY(pOut->intra_quant_mat,       inMp4.QuantMatrixIntra);
    MEM_COPY(pOut->nonintra_quant_mat,    inMp4.QuantMatrixInter);
    pOut->data_partitioned              = inMp4.data_partitioned;
    pOut->reversible_vlc                = inMp4.reversible_vlc;
    pOut->nNumSlices                    = in.nNumSlices;
    pOut->pSliceDataOffsets             = in.pSliceDataOffsets;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Copy VC1 picture info from lwparser format to lwtvmr format
//!
//! This method is used by DecodePicture() for VC1 streams, to
//! translate the picture header parsed by lwparser into the struct
//! needed by lwtvmr.
//!
RC TvmrDecoder::SetPictureInfoVc1
(
    TVMRPictureInfoVC1   *pOut,
    const LWDPictureData &in,
    MyPicBuff            *pLwrrPic
)
{
    MASSERT(pOut != nullptr);
    const LWVC1PictureData &ilwc1 = in.CodecSpecific.vc1;
    RC rc;

    memset(pOut, 0, sizeof(*pOut));

    pOut->forwardReference  = pLwrrPic->AddPrereq(ilwc1.pForwardRef);
    pOut->backwardReference = pLwrrPic->AddPrereq(ilwc1.pBackwardRef);
    pOut->rangeMappedOutput = pLwrrPic->AddPrereq(ilwc1.pRangeMapped);

    pOut->pictureType       = ilwc1.ptype;
    pOut->frameCodingMode   = ilwc1.fcm;
    pOut->bottom_field_flag = in.bottom_field_flag;
    pOut->postprocflag      = ilwc1.postprocflag;
    pOut->pulldown          = ilwc1.pulldown;
    pOut->interlace         = ilwc1.interlace;
    pOut->tfcntrflag        = ilwc1.tfcntrflag;
    pOut->finterpflag       = ilwc1.finterpflag;
    pOut->psf               = ilwc1.psf;
    pOut->dquant            = ilwc1.dquant;
    pOut->panscan_flag      = ilwc1.panscan_flag;
    pOut->refdist_flag      = ilwc1.refdist_flag;
    pOut->quantizer         = ilwc1.quantizer;
    pOut->extended_mv       = ilwc1.extended_mv;
    pOut->extended_dmv      = ilwc1.extended_dmv;
    pOut->overlap           = ilwc1.overlap;
    pOut->vstransform       = ilwc1.vstransform;
    pOut->loopfilter        = ilwc1.loopfilter;
    pOut->fastuvmc          = ilwc1.fastuvmc;
    pOut->range_mapy_flag   = ilwc1.range_mapy_flag;
    pOut->range_mapy        = ilwc1.range_mapy;
    pOut->range_mapuv_flag  = ilwc1.range_mapuv_flag;
    pOut->range_mapuv       = ilwc1.range_mapuv;
    pOut->multires          = ilwc1.multires;
    pOut->syncmarker        = ilwc1.syncmarker;
    pOut->rangered          = ilwc1.rangered;
    pOut->rangeredfrm       = ilwc1.rangeredfrm;
    pOut->maxbframes        = ilwc1.maxbframes;
    // pOut->sxes_enable          TODO
    CHECK_RC(SetEncryptParams(&pOut->encryptParams, ilwc1.encryptParams));
    // pOut->LwrrentFreq_VDE      TODO
    // pOut->LwrrentFreq_EMC      TODO
    pOut->nNumSlices        = in.nNumSlices;
    pOut->pSliceDataOffsets = in.pSliceDataOffsets;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Copy H264 picture info from lwparser format to lwtvmr format
//!
//! This method is used by DecodePicture() for H264 streams, to
//! translate the picture header parsed by lwparser into the struct
//! needed by lwtvmr.
//!
RC TvmrDecoder::SetPictureInfoH264
(
    TVMRPictureInfoH264  *pOut,
    const LWDPictureData &in,
    MyPicBuff            *pLwrrPic
)
{
    MASSERT(pOut != nullptr);
    const LWH264PictureData &inH264 = in.CodecSpecific.h264;
    const LWH264HeaderData  &inHdr  = in.CodecSpecificHeaders.h264_hdr;
    RC rc;

    memset(pOut, 0, sizeof(*pOut));
    pOut->FieldOrderCnt[0]              = inH264.LwrrFieldOrderCnt[0];
    pOut->FieldOrderCnt[1]              = inH264.LwrrFieldOrderCnt[1];
    pOut->is_reference                  = in.ref_pic_flag;
    pOut->chroma_format_idc             = in.chroma_format;
    pOut->frame_num                     = inH264.frame_num;
    pOut->field_pic_flag                = in.field_pic_flag;
    pOut->bottom_field_flag             = in.bottom_field_flag;
    pOut->num_ref_frames                = inH264.num_ref_frames;
    pOut->mb_adaptive_frame_field_flag  = inH264.MbaffFrameFlag;
    pOut->constrained_intra_pred_flag   = inH264.constrained_intra_pred_flag;
    pOut->weighted_pred_flag            = inH264.weighted_pred_flag;
    pOut->weighted_bipred_idc           = inH264.weighted_bipred_idc;
    pOut->frame_mbs_only_flag           = inH264.frame_mbs_only_flag;
    pOut->transform_8x8_mode_flag       = inH264.transform_8x8_mode_flag;
    pOut->chroma_qp_index_offset        = inH264.chroma_qp_index_offset;
    pOut->second_chroma_qp_index_offset = inH264.second_chroma_qp_index_offset;
    pOut->pic_init_qp_minus26           = inH264.pic_init_qp_minus26;
    pOut->num_ref_idx_l0_active_minus1  = inH264.num_ref_idx_l0_active_minus1;
    pOut->num_ref_idx_l1_active_minus1  = inH264.num_ref_idx_l1_active_minus1;
    pOut->log2_max_frame_num_minus4     = inH264.log2_max_frame_num_minus4;
    pOut->pic_order_cnt_type            = inH264.pic_order_cnt_type;
    pOut->log2_max_pic_order_cnt_lsb_minus4 =
                               inH264.log2_max_pic_order_cnt_lsb_minus4;
    pOut->delta_pic_order_always_zero_flag =
                               inH264.delta_pic_order_always_zero_flag;
    pOut->direct_8x8_inference_flag     = inH264.direct_8x8_inference_flag;
    pOut->entropy_coding_mode_flag      = inH264.entropy_coding_mode_flag;
    pOut->pic_order_present_flag        = inH264.pic_order_present_flag;
    pOut->deblocking_filter_control_present_flag =
                                inH264.deblocking_filter_control_present_flag;
    pOut->redundant_pic_cnt_present_flag= inH264.redundant_pic_cnt_present_flag;
    pOut->num_slice_groups_minus1       = inH264.num_slice_groups_minus1;
    pOut->slice_group_map_type          = inH264.slice_group_map_type;
    pOut->slice_group_change_rate_minus1= inH264.slice_group_change_rate_minus1;
    pOut->pMb2SliceGroupMap             = inH264.pMb2SliceGroupMap;
    pOut->fmo_aso_enable                = inH264.fmo_aso_enable;
    pOut->scaling_matrix_present        = inH264.scaling_matrix_present;

    // Note: scaling_lists_* is in raster order, not zig-zag
    MEM_COPY(pOut->scaling_lists_4x4, inH264.WeightScale4x4);
    MEM_COPY(pOut->scaling_lists_8x8, inH264.WeightScale8x8);

    for (size_t idxIn = 0, idxOut = 0;
         idxIn < NUMELEMS(inH264.dpb); ++idxIn)
    {
        TVMRReferenceFrameH264 *pOutFrame = &pOut->referenceFrames[idxOut];
        const LWH264DPBEntry   &inFrame   = inH264.dpb[idxIn];
        if (inFrame.pPicBuf == nullptr)
            continue;
        MASSERT(idxOut < NUMELEMS(pOut->referenceFrames));
        pOutFrame->surface             = pLwrrPic->AddPrereq(inFrame.pPicBuf);

        pOutFrame->is_long_term        = inFrame.is_long_term;
        pOutFrame->is_field            = inFrame.is_field;
        pOutFrame->top_is_reference    = (inFrame.used_for_reference & 1);
        pOutFrame->bottom_is_reference = (inFrame.used_for_reference >> 1) & 1;
        pOutFrame->FieldOrderCnt[0]    = inFrame.FieldOrderCnt[0];
        pOutFrame->FieldOrderCnt[1]    = inFrame.FieldOrderCnt[1];
        pOutFrame->FrameIdx            = inFrame.FrameIdx;
        ++idxOut;
    }

    if (m_Codec == TVMR_VIDEO_CODEC_H264_MVC)
    {
        pOut->mvcext.num_views_minus1 = inH264.ext.mvcext.num_views_minus1;
        pOut->mvcext.view_id          = inH264.ext.mvcext.view_id;
        pOut->mvcext.inter_view_flag  = inH264.ext.mvcext.inter_view_flag;
        pOut->mvcext.num_inter_view_refs_l0 =
            inH264.ext.mvcext.num_inter_view_refs_l0;
        pOut->mvcext.num_inter_view_refs_l1 =
            inH264.ext.mvcext.num_inter_view_refs_l1;
        pOut->mvcext.MVCReserved8Bits = inH264.ext.mvcext.MVCReserved8Bits;
        MEM_COPY(pOut->mvcext.InterViewRefsL0,
                 inH264.ext.mvcext.InterViewRefsL0);
        MEM_COPY(pOut->mvcext.InterViewRefsL1[0],
                 inH264.ext.mvcext.InterViewRefsL1[0]);
    }

    pOut->last_sps_id = inH264.last_sps_id;
    pOut->last_pps_id = inH264.last_pps_id;

    CHECK_RC(SetEncryptParams(&pOut->encryptParams, inH264.encryptParams));

    // pOut->pAesData             TODO
    pOut->slcgrp                        = inH264.slcgrp;
    pOut->pic_height_in_map_units_minus1= inH264.pic_height_in_map_units_minus1;
    pOut->pic_width_in_mbs_minus1       = inH264.pic_width_in_mbs_minus1;
    pOut->nal_ref_idc                   = inH264.nal_ref_idc;
    pOut->profile_idc                   = inHdr.h264_sps.profile_idc;
    // pOut->LwrrentFreq_VDE      TODO
    // pOut->LwrrentFreq_EMC      TODO
    // pOut->sxes_enable          TODO
    // pOut->sxep_slice_mode      TODO
    // pOut->decode_next_slice    TODO
    pOut->nNumSlices                    = in.nNumSlices;
    pOut->pSliceDataOffsets             = in.pSliceDataOffsets;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Copy VP8 picture info from lwparser format to lwtvmr format
//!
//! This method is used by DecodePicture() for VP8 streams, to
//! translate the picture header parsed by lwparser into the struct
//! needed by lwtvmr.
//!
RC TvmrDecoder::SetPictureInfoVp8
(
    TVMRPictureInfoVP8   *pOut,
    const LWDPictureData &in,
    MyPicBuff            *pLwrrPic
)
{
    MASSERT(pOut != nullptr);
    const LWVP8PictureData &ilwp8 = in.CodecSpecific.vp8;
    RC rc;

    memset(pOut, 0, sizeof(*pOut));
    pOut->LastReference   = pLwrrPic->AddPrereq(ilwp8.pLastRef);
    pOut->GoldenReference = pLwrrPic->AddPrereq(ilwp8.pGoldenRef);
    pOut->AltReference    = pLwrrPic->AddPrereq(ilwp8.pAltRef);

    pOut->key_frame                    = ilwp8.key_frame;
    pOut->version                      = ilwp8.version;
    pOut->show_frame                   = ilwp8.show_frame;
    pOut->clamp_type                   = ilwp8.clamp_type;
    pOut->segmentation_enabled         = ilwp8.segmentation_enabled;
    pOut->update_mb_seg_map            = ilwp8.update_mb_seg_map;
    pOut->update_mb_seg_data           = ilwp8.update_mb_seg_data;
    pOut->update_mb_seg_abs            = ilwp8.update_mb_seg_abs;
    pOut->filter_type                  = ilwp8.filter_type;
    pOut->loop_filter_level            = ilwp8.loop_filter_level;
    pOut->sharpness_level              = ilwp8.sharpness_level;
    pOut->mode_ref_lf_delta_enabled    = ilwp8.mode_ref_lf_delta_enabled;
    pOut->mode_ref_lf_delta_update     = ilwp8.mode_ref_lf_delta_update;
    pOut->num_of_partitions            = ilwp8.num_of_partitions;
    pOut->dequant_index                = ilwp8.dequant_index;
    MEM_COPY(pOut->deltaq,               ilwp8.deltaq);
    pOut->golden_ref_frame_sign_bias   = ilwp8.golden_ref_frame_sign_bias;
    pOut->alt_ref_frame_sign_bias      = ilwp8.alt_ref_frame_sign_bias;
    pOut->refresh_entropy_probs        = ilwp8.refresh_entropy_probs;
    pOut->CbrHdrBedValue               = ilwp8.CbrHdrBedValue;
    pOut->CbrHdrBedRange               = ilwp8.CbrHdrBedRange;
    MEM_COPY(pOut->mb_seg_tree_probs,    ilwp8.mb_seg_tree_probs);
    MEM_COPY(pOut->seg_feature,          ilwp8.seg_feature);
    MEM_COPY(pOut->ref_lf_deltas,        ilwp8.ref_lf_deltas);
    MEM_COPY(pOut->mode_lf_deltas,       ilwp8.mode_lf_deltas);
    pOut->BitsConsumed                 = ilwp8.BitsConsumed;
    MEM_COPY(pOut->AlignByte,            ilwp8.AlignByte);
    pOut->hdr_partition_size           = ilwp8.hdr_partition_size;
    pOut->hdr_start_offset             = ilwp8.hdr_start_offset;
    pOut->hdr_processed_offset         = ilwp8.hdr_processed_offset;
    MEM_COPY(pOut->coeff_partition_size, ilwp8.coeff_partition_size);
    MEM_COPY(pOut->coeff_partition_start_offset,
                                         ilwp8.coeff_partition_start_offset);
    CHECK_RC(SetEncryptParams(&pOut->encryptParams, ilwp8.encryptParams));
    pOut->lwp8CoeffPartitionDataLen    = in.lwp8CoeffPartitionDataLen;
    pOut->Vp8CoeffPartitionBuffer      = in.Vp8CoeffPartitionBuffer;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Copy H265 picture info from lwparser format to lwtvmr format
//!
//! This method is used by DecodePicture() for H265 streams, to
//! translate the picture header parsed by lwparser into the struct
//! needed by lwtvmr.
//!
RC TvmrDecoder::SetPictureInfoH265
(
    TVMRPictureInfoHEVC  *pOut,
    const LWDPictureData &in,
    MyPicBuff            *pLwrrPic
)
{
    MASSERT(pOut != nullptr);
    const LWHEVCPictureData &inH265 = in.CodecSpecific.hevc;
    RC rc;

    memset(pOut, 0, sizeof(*pOut));
    pOut->pic_width_in_luma_samples     = inH265.pic_width_in_luma_samples;
    pOut->pic_height_in_luma_samples    = inH265.pic_height_in_luma_samples;
    pOut->log2_min_luma_coding_block_size_minus3 =
                            inH265.log2_min_luma_coding_block_size_minus3;
    pOut->log2_diff_max_min_luma_coding_block_size =
                            inH265.log2_diff_max_min_luma_coding_block_size;
    pOut->log2_min_transform_block_size_minus2 =
                            inH265.log2_min_transform_block_size_minus2;
    pOut->log2_diff_max_min_transform_block_size =
                            inH265.log2_diff_max_min_transform_block_size;
    pOut->pcm_enabled_flag              = inH265.pcm_enabled_flag;
    pOut->log2_min_pcm_luma_coding_block_size_minus3 =
                            inH265.log2_min_pcm_luma_coding_block_size_minus3;
    pOut->log2_diff_max_min_pcm_luma_coding_block_size =
                            inH265.log2_diff_max_min_pcm_luma_coding_block_size;
    pOut->bit_depth_luma                = inH265.bit_depth_luma;
    pOut->bit_depth_chroma              = inH265.bit_depth_chroma;
    pOut->pcm_sample_bit_depth_luma_minus1 =
                            inH265.pcm_sample_bit_depth_luma_minus1;
    pOut->pcm_sample_bit_depth_chroma_minus1 =
                            inH265.pcm_sample_bit_depth_chroma_minus1;
    pOut->pcm_loop_filter_disabled_flag = inH265.pcm_loop_filter_disabled_flag;
    pOut->strong_intra_smoothing_enabled_flag =
                            inH265.strong_intra_smoothing_enabled_flag;
    pOut->max_transform_hierarchy_depth_intra =
                            inH265.max_transform_hierarchy_depth_intra;
    pOut->max_transform_hierarchy_depth_inter =
                            inH265.max_transform_hierarchy_depth_inter;
    pOut->amp_enabled_flag              = inH265.amp_enabled_flag;
    pOut->separate_colour_plane_flag    = inH265.separate_colour_plane_flag;
    pOut->log2_max_pic_order_cnt_lsb_minus4 =
                            inH265.log2_max_pic_order_cnt_lsb_minus4;
    pOut->num_short_term_ref_pic_sets   = inH265.num_short_term_ref_pic_sets;
    pOut->long_term_ref_pics_present_flag =
                            inH265.long_term_ref_pics_present_flag;
    pOut->num_long_term_ref_pics_sps    = inH265.num_long_term_ref_pics_sps;
    pOut->sps_temporal_mvp_enabled_flag = inH265.sps_temporal_mvp_enabled_flag;
    pOut->sample_adaptive_offset_enabled_flag =
                            inH265.sample_adaptive_offset_enabled_flag;
    pOut->scaling_list_enable_flag      = inH265.scaling_list_enable_flag;
    pOut->chroma_format_idc             = inH265.chroma_format_idc;
    pOut->dependent_slice_segments_enabled_flag =
                            inH265.dependent_slice_segments_enabled_flag;
    pOut->slice_segment_header_extension_present_flag =
                            inH265.slice_segment_header_extension_present_flag;
    pOut->sign_data_hiding_enabled_flag = inH265.sign_data_hiding_enabled_flag;
    pOut->lw_qp_delta_enabled_flag      = inH265.lw_qp_delta_enabled_flag;
    pOut->diff_lw_qp_delta_depth        = inH265.diff_lw_qp_delta_depth;
    pOut->init_qp_minus26               = inH265.init_qp_minus26;
    pOut->pps_cb_qp_offset              = inH265.pps_cb_qp_offset;
    pOut->pps_cr_qp_offset              = inH265.pps_cr_qp_offset;
    pOut->constrained_intra_pred_flag   = inH265.constrained_intra_pred_flag;
    pOut->weighted_pred_flag            = inH265.weighted_pred_flag;
    pOut->weighted_bipred_flag          = inH265.weighted_bipred_flag;
    pOut->transform_skip_enabled_flag   = inH265.transform_skip_enabled_flag;
    pOut->transquant_bypass_enabled_flag= inH265.transquant_bypass_enabled_flag;
    pOut->entropy_coding_sync_enabled_flag =
                            inH265.entropy_coding_sync_enabled_flag;
    pOut->log2_parallel_merge_level_minus2 =
                            inH265.log2_parallel_merge_level_minus2;
    pOut->num_extra_slice_header_bits   = inH265.num_extra_slice_header_bits;
    pOut->loop_filter_across_tiles_enabled_flag =
                            inH265.loop_filter_across_tiles_enabled_flag;
    pOut->loop_filter_across_slices_enabled_flag =
                            inH265.loop_filter_across_slices_enabled_flag;
    pOut->output_flag_present_flag      = inH265.output_flag_present_flag;
    pOut->num_ref_idx_l0_default_active_minus1 =
                            inH265.num_ref_idx_l0_default_active_minus1;
    pOut->num_ref_idx_l1_default_active_minus1 =
                            inH265.num_ref_idx_l1_default_active_minus1;
    pOut->lists_modification_present_flag =
                            inH265.lists_modification_present_flag;
    pOut->cabac_init_present_flag       = inH265.cabac_init_present_flag;
    pOut->pps_slice_chroma_qp_offsets_present_flag =
                            inH265.pps_slice_chroma_qp_offsets_present_flag;
    pOut->deblocking_filter_control_present_flag =
                            inH265.deblocking_filter_control_present_flag;
    pOut->deblocking_filter_override_enabled_flag =
                            inH265.deblocking_filter_override_enabled_flag;
    pOut->pps_deblocking_filter_disabled_flag =
                            inH265.pps_deblocking_filter_disabled_flag;
    pOut->pps_beta_offset_div2          = inH265.pps_beta_offset_div2;
    pOut->pps_tc_offset_div2            = inH265.pps_tc_offset_div2;
    pOut->tiles_enabled_flag            = inH265.tiles_enabled_flag;
    pOut->uniform_spacing_flag          = inH265.uniform_spacing_flag;
    pOut->num_tile_columns_minus1       = inH265.num_tile_columns_minus1;
    pOut->num_tile_rows_minus1          = inH265.num_tile_rows_minus1;

    MEM_COPY(pOut->column_width_minus1,   inH265.column_width_minus1);
    MEM_COPY(pOut->row_height_minus1,     inH265.row_height_minus1);

    pOut->iLwr                          = inH265.iLwr;
    pOut->IDRPicFlag                    = inH265.IDRPicFlag;
    pOut->RAPPicFlag                    = inH265.RAPPicFlag;
    pOut->NumDeltaPocsOfRefRpsIdx       = inH265.NumDeltaPocsOfRefRpsIdx;
    pOut->NumPocTotalLwrr               = inH265.NumPocTotalLwrr;
    pOut->NumPocStLwrrBefore            = inH265.NumPocStLwrrBefore;
    pOut->NumPocStLwrrAfter             = inH265.NumPocStLwrrAfter;
    pOut->NumPocLtLwrr                  = inH265.NumPocLtLwrr;
    pOut->NumBitsToSkip                 = inH265.NumBitsToSkip;
    pOut->LwrrPicOrderCntVal            = inH265.LwrrPicOrderCntVal;

    ct_assert(CT_NUMELEMS(pOut->RefPics) == CT_NUMELEMS(inH265.RefPics));
    for (size_t ii = 0; ii < NUMELEMS(pOut->RefPics); ++ii)
    {
        pOut->RefPics[ii] = pLwrrPic->AddPrereq(inH265.RefPics[ii]);
    }

    MEM_COPY(pOut->PicOrderCntVal,          inH265.PicOrderCntVal);
    MEM_COPY(pOut->IsLongTerm,              inH265.IsLongTerm);
    MEM_COPY(pOut->RefPicSetStLwrrBefore,   inH265.RefPicSetStLwrrBefore);
    MEM_COPY(pOut->RefPicSetStLwrrAfter,    inH265.RefPicSetStLwrrAfter);
    MEM_COPY(pOut->RefPicSetLtLwrr,         inH265.RefPicSetLtLwrr);
    MEM_COPY(pOut->ScalingList4x4,          inH265.ScalingList4x4);
    MEM_COPY(pOut->ScalingList8x8,          inH265.ScalingList8x8);
    MEM_COPY(pOut->ScalingList16x16,        inH265.ScalingList16x16);
    MEM_COPY(pOut->ScalingList32x32,        inH265.ScalingList32x32);
    MEM_COPY(pOut->ScalingListDCCoeff16x16, inH265.ScalingListDCCoeff16x16);
    MEM_COPY(pOut->ScalingListDCCoeff32x32, inH265.ScalingListDCCoeff32x32);
    MEM_COPY(pOut->NumDeltaPocs,            inH265.NumDeltaPocs);

    pOut->sps_range_extension_present_flag =
                            inH265.sps_range_extension_present_flag;
    pOut->transformSkipRotationEnableFlag =
                            inH265.transformSkipRotationEnableFlag;
    pOut->transformSkipContextEnableFlag= inH265.transformSkipContextEnableFlag;
    pOut->implicitRdpcmEnableFlag       = inH265.implicitRdpcmEnableFlag;
    pOut->explicitRdpcmEnableFlag       = inH265.explicitRdpcmEnableFlag;
    pOut->extendedPrecisionProcessingFlag =
                            inH265.extendedPrecisionProcessingFlag;
    pOut->intraSmoothingDisabledFlag    = inH265.intraSmoothingDisabledFlag;
    pOut->highPrecisionOffsetsEnableFlag= inH265.highPrecisionOffsetsEnableFlag;
    pOut->fastRiceAdaptationEnableFlag  = inH265.fastRiceAdaptationEnableFlag;
    pOut->cabacBypassAlignmentEnableFlag= inH265.cabacBypassAlignmentEnableFlag;
    pOut->intraBlockCopyEnableFlag      = inH265.intraBlockCopyEnableFlag;
    pOut->pps_range_extension_present_flag =
                            inH265.pps_range_extension_present_flag;
    pOut->log2MaxTransformSkipSize      = inH265.log2MaxTransformSkipSize;
    pOut->crossComponentPredictionEnableFlag =
                            inH265.crossComponentPredictionEnableFlag;
    pOut->chromaQpAdjustmentEnableFlag  = inH265.chromaQpAdjustmentEnableFlag;
    pOut->diffLwChromaQpAdjustmentDepth = inH265.diffLwChromaQpAdjustmentDepth;
    pOut->chromaQpAdjustmentTableSize   = inH265.chromaQpAdjustmentTableSize;
    pOut->log2SaoOffsetScaleLuma        = inH265.log2SaoOffsetScaleLuma;
    pOut->log2SaoOffsetScaleChroma      = inH265.log2SaoOffsetScaleChroma;
    MEM_COPY(pOut->cb_qp_adjustment,      inH265.cb_qp_adjustment);
    MEM_COPY(pOut->cr_qp_adjustment,      inH265.cr_qp_adjustment);
    CHECK_RC(SetEncryptParams(&pOut->encryptParams, inH265.encryptParams));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Copy AV1 picture info from lwparser format to lwtvmr format
//!
//! This method is used by DecodePicture() for AV1 streams, to
//! translate the picture header parsed by lwparser into the struct
//! needed by lwtvmr.
//!
RC TvmrDecoder::SetPictureInfoAv1
(
    TVMRPictureInfoAV1   *pOut,
    LWDPictureData       *pLwdPictureData,
    MyPicBuff            *pLwrrPic
)
{
    MASSERT(pOut != nullptr);
    LWAV1PictureData &inAv1 = pLwdPictureData->CodecSpecific.av1;
    RC rc;

    // TODO Below constants should ideally be exposed from tvrm header files
    constexpr UINT08 AV1_INTER_FRAME = 1;
    constexpr UINT08 MAX_SUPPORTED_REF_FRAMES = 7;

    memset(pOut, 0, sizeof(*pOut));

    // Basic sequence parameters
    pOut->width = inAv1.width;
    pOut->height = inAv1.height;
    pOut->superres_width = inAv1.superres_width;
    // pOut->max_width; TODO not supported in lwparser
    // pOut->max_height; TODO not supported in lwparser
    pOut->chroma_format_idc = inAv1.mono_chrome ? 0 : 1; // 0:400, 1:420
    // PicEntry_AV1_Short LwrrPic; TODO not supported in lwparser
    // PicEntry_AV1_Short FgsPic; TODO not supported in lwparser
    pOut->superres_denom = inAv1.coded_denom;
    pOut->BitDepth = inAv1.bit_depth_minus8 + 8;
    pOut->profile = inAv1.profile;

    // Tiles
    pOut->tiles.cols = inAv1.num_tile_cols;
    pOut->tiles.rows = inAv1.num_tile_rows;
    pOut->tiles.context_update_id = inAv1.context_update_tile_id;

    MASSERT(inAv1.num_tile_cols <= CT_NUMELEMS(pOut->tiles.widths));
    if (inAv1.num_tile_cols > CT_NUMELEMS(pOut->tiles.widths))
    {
        Printf(Tee::PriError, "num_tile_cols > size of tiles.widths array");
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(inAv1.num_tile_rows <= CT_NUMELEMS(pOut->tiles.heights));
    if (inAv1.num_tile_rows > CT_NUMELEMS(pOut->tiles.heights))
    {
        Printf(Tee::PriError, "num_tile_rows > size of tiles.heights array");
        return RC::SOFTWARE_ERROR;
    }

    for (UINT32 i = 0; i < inAv1.num_tile_cols; i++)
    {
        pOut->tiles.widths[i] = inAv1.tile_col_start_sb[i + 1] - inAv1.tile_col_start_sb[i];
    }

    for (UINT32 i = 0; i < inAv1.num_tile_rows; i++)
    {
        pOut->tiles.heights[i] = inAv1.tile_row_start_sb[i + 1] - inAv1.tile_row_start_sb[i];
    }

    const UINT64 totalTiles = inAv1.num_tile_cols * inAv1.num_tile_rows;
    if (totalTiles <= AV1_MAX_TILES)
    {
        memcpy(&(pOut->tiles.tile_info[0]), &(inAv1.tileInfo[0]),
            totalTiles * 2 * sizeof(pOut->tiles.tile_info[0]));
    }
    else
    {
        //will come  here for error streams
        memcpy(&(pOut->tiles.tile_info[0]), &(inAv1.tileInfo[0]),
            AV1_MAX_TILES * 2 * sizeof(pOut->tiles.tile_info[0]));
    }

    // Coding Tools
    pOut->use_128x128_superblock = inAv1.use_128x128_superblock; // 0:64x64, 1: 128x128
    pOut->intra_edge_filter = inAv1.enable_intra_edge_filter;
    pOut->interintra_compound = inAv1.enable_interintra_compound;
    pOut->masked_compound = inAv1.enable_masked_compound;
    pOut->warped_motion = inAv1.allow_warped_motion;
    pOut->dual_filter = inAv1.enable_dual_filter;
    pOut->jnt_comp = inAv1.enable_jnt_comp;
    pOut->screen_content_tools = inAv1.allow_screen_content_tools;
    pOut->integer_mv = inAv1.lwr_frame_force_integer_mv;
    pOut->cdef_enable = inAv1.enable_cdef;
    pOut->restoration = inAv1.enable_restoration;
    pOut->film_grain_enable = inAv1.enable_fgs;
    pOut->intrabc = inAv1.allow_intrabc;
    pOut->high_precision_mv = inAv1.allow_high_precision_mv;
    pOut->switchable_motion_mode = inAv1.switchable_motion_mode;
    pOut->filter_intra = inAv1.enable_filter_intra;
    pOut->disable_frame_end_update_cdf = inAv1.disable_frame_end_update_cdf;
    pOut->disable_cdf_update = inAv1.disable_cdf_update;
    pOut->reference_mode = inAv1.frame_reference_mode;
    pOut->skip_mode = inAv1.skip_mode_flag;
    pOut->reduced_tx_set = inAv1.reduced_tx_set_used;
    pOut->superres = inAv1.superres_is_scaled;
    pOut->tx_mode = inAv1.frame_tx_mode;
    pOut->use_ref_frame_mvs = inAv1.use_ref_frame_mvs;
    // pOut->reference_frame_update TODO not supported in lwparser

    // Format & Picture Info flags
    pOut->frame_type = inAv1.frame_type;
    pOut->show_frame = inAv1.show_frame;
    // pOut->showable_frame TODO not supported in lwparser
    pOut->subsampling_x = inAv1.subsampling_x;
    pOut->subsampling_y = inAv1.subsampling_y;
    pOut->mono_chrome = inAv1.mono_chrome;

    pOut->primary_ref_frame.Index = (UINT08)inAv1.primary_ref_frame_index;
    pOut->order_hint = inAv1.frame_offset;
    pOut->order_hint_bits_minus_1 = inAv1.order_hint_bits_minus1;

    // References
    // Driver supports a max of 7 reference frames. Slot index 8 is not used as ref frame
    ct_assert(MAX_SUPPORTED_REF_FRAMES <= CT_NUMELEMS(pOut->frame_refs));
    ct_assert(MAX_SUPPORTED_REF_FRAMES <= CT_NUMELEMS(inAv1.RefPicindices));
    ct_assert(CT_NUMELEMS(pOut->frame_refs) == CT_NUMELEMS(inAv1.ref_global_motion));
    ct_assert(CT_NUMELEMS(pOut->frame_refs[0].wmmat) == CT_NUMELEMS(inAv1.ref_global_motion[0].wmmat));

    for (size_t ii = 0; ii < MAX_SUPPORTED_REF_FRAMES; ii++)
    {
        pOut->frame_refs[ii].Index = (UINT08)inAv1.RefPicindices[ii];

        pOut->frame_refs[ii].width = inAv1.width;
        pOut->frame_refs[ii].height = inAv1.height;
        pOut->frame_refs[ii].invalid = inAv1.ref_global_motion[ii].invalid;
        pOut->frame_refs[ii].wmtype = inAv1.ref_global_motion[ii].wmtype;

        for (size_t jj = 0; jj < CT_NUMELEMS(pOut->frame_refs[ii].wmmat); jj++)
        {
            pOut->frame_refs[ii].wmmat[jj] = inAv1.ref_global_motion[ii].wmmat[jj];
        }
    }

    ct_assert(CT_NUMELEMS(pOut->ref_frame_map_index) == CT_NUMELEMS(inAv1.ref_frame_map_index));
    for (UINT32 i = 0; i < CT_NUMELEMS(pOut->ref_frame_map_index); i++)
    {
        pOut->ref_frame_map_index[i] = inAv1.ref_frame_map_index[i];
    }

    ct_assert(MAX_SUPPORTED_REF_FRAMES <= CT_NUMELEMS(pOut->RefPics));
    ct_assert(MAX_SUPPORTED_REF_FRAMES <= CT_NUMELEMS(inAv1.RefPics));
    for (UINT32 ii = 0; ii < MAX_SUPPORTED_REF_FRAMES; ++ii)
    {
        pOut->RefPics[ii] = pLwrrPic->AddPrereq(inAv1.RefPics[ii]);
    }

    // PicEntry_AV1_Short ref_frame_map[8]; TODO not supported in lwparser
    pOut->enable_order_hint = inAv1.enable_order_hint;

    // Loop filter parameters
    ct_assert(CT_NUMELEMS(pOut->loop_filter.filter_level) == CT_NUMELEMS(inAv1.loop_filter_level));
    for (size_t ii = 0; ii < CT_NUMELEMS(pOut->loop_filter.filter_level); ++ii)
    {
        pOut->loop_filter.filter_level[ii] = inAv1.loop_filter_level[ii];
    }
    pOut->loop_filter.filter_level_u = inAv1.loop_filter_level_u;
    pOut->loop_filter.filter_level_v = inAv1.loop_filter_level_v;
    pOut->loop_filter.sharpness_level = inAv1.loop_filter_sharpness;
    pOut->loop_filter.mode_ref_delta_enabled = inAv1.lf_mode_ref_delta_enabled;
    // pOut->loop_filter.mode_ref_delta_update; TODO not supported in lwparser
    pOut->loop_filter.delta_lf_present = inAv1.delta_lf_present_flag;
    pOut->loop_filter.delta_lf_multi = inAv1.delta_lf_multi;
    pOut->loop_filter.delta_lf_res = inAv1.delta_lf_res;

    ct_assert(CT_NUMELEMS(pOut->loop_filter.ref_deltas) == CT_NUMELEMS(inAv1.loop_filter_ref_deltas));
    ct_assert(CT_NUMELEMS(pOut->loop_filter.mode_deltas) == CT_NUMELEMS(inAv1.loop_filter_mode_deltas));

    for (size_t ii = 0; ii < CT_NUMELEMS(inAv1.loop_filter_ref_deltas); ii++)
    {
        pOut->loop_filter.ref_deltas[ii] = inAv1.loop_filter_ref_deltas[ii];
    }
    for (size_t ii = 0; ii < CT_NUMELEMS(inAv1.loop_filter_mode_deltas); ii++)
    {
        pOut->loop_filter.mode_deltas[ii] = inAv1.loop_filter_mode_deltas[ii];
    }

    ct_assert(CT_NUMELEMS(pOut->loop_filter.frame_restoration_type) == CT_NUMELEMS(inAv1.lr_type));
    ct_assert(CT_NUMELEMS(pOut->loop_filter.restoration_unit_size) == CT_NUMELEMS(inAv1.lr_unit_size));
    ct_assert(CT_NUMELEMS(inAv1.lr_type) == CT_NUMELEMS(inAv1.lr_unit_size));

    for (size_t ii = 0; ii < CT_NUMELEMS(inAv1.lr_type); ii++)
    {
        pOut->loop_filter.frame_restoration_type[ii] = inAv1.lr_type[ii];
        pOut->loop_filter.restoration_unit_size[ii] = inAv1.lr_unit_size[ii];
    }

    // Quantization
    pOut->quantization.delta_q_present = inAv1.delta_q_present_flag;
    pOut->quantization.delta_q_res = inAv1.delta_q_res;
    pOut->quantization.base_qindex = inAv1.base_qindex;

    pOut->quantization.y_dc_delta_q = inAv1.qp_y_dc_delta_q;
    pOut->quantization.u_dc_delta_q = inAv1.qp_u_dc_delta_q;
    pOut->quantization.v_dc_delta_q = inAv1.qp_v_dc_delta_q;
    pOut->quantization.u_ac_delta_q = inAv1.qp_u_ac_delta_q;
    pOut->quantization.v_ac_delta_q = inAv1.qp_v_ac_delta_q;

    pOut->quantization.qm_y = inAv1.using_qmatrix ? inAv1.qm_y : 0xff;
    pOut->quantization.qm_u = inAv1.using_qmatrix ? inAv1.qm_u : 0xff;
    pOut->quantization.qm_v = inAv1.using_qmatrix ? inAv1.qm_v : 0xff;

    // Cdef parameters
    pOut->cdef.damping = inAv1.cdef_damping_minus_3;
    pOut->cdef.bits = inAv1.cdef_bits;

    ct_assert(CT_NUMELEMS(pOut->cdef.y_strengths) == CT_NUMELEMS(inAv1.cdef_y_pri_strength));
    ct_assert(CT_NUMELEMS(pOut->cdef.uv_strengths) == CT_NUMELEMS(inAv1.cdef_uv_pri_strength));
    ct_assert(CT_NUMELEMS(inAv1.cdef_y_pri_strength) == CT_NUMELEMS(inAv1.cdef_y_sec_strength));
    ct_assert(CT_NUMELEMS(inAv1.cdef_uv_pri_strength) == CT_NUMELEMS(inAv1.cdef_uv_sec_strength));
    ct_assert(CT_NUMELEMS(pOut->cdef.y_strengths) == CT_NUMELEMS(pOut->cdef.uv_strengths));

    for (size_t ii = 0; ii < CT_NUMELEMS(pOut->cdef.y_strengths); ii++)
    {
        pOut->cdef.y_strengths[ii].primary = inAv1.cdef_y_pri_strength[ii];
        pOut->cdef.y_strengths[ii].secondary = inAv1.cdef_y_sec_strength[ii];
        pOut->cdef.uv_strengths[ii].primary = inAv1.cdef_uv_pri_strength[ii];
        pOut->cdef.uv_strengths[ii].secondary = inAv1.cdef_uv_sec_strength[ii];
    }
    pOut->interp_filter = inAv1.interp_filter;

    // Segmentation
    pOut->segmentation.enabled = inAv1.segmentation_enabled;
    pOut->segmentation.update_map = inAv1.segmentation_update_map;
    pOut->segmentation.update_data = inAv1.segmentation_update_data;
    pOut->segmentation.temporal_update = inAv1.segmentation_temporal_update;
    pOut->segmentation.segid_preskip = inAv1.segid_preskip;

    ct_assert(CT_NUMELEMS(pOut->segmentation.feature_data) == CT_NUMELEMS(inAv1.segmentation_feature_data));
    ct_assert(CT_NUMELEMS(pOut->segmentation.feature_data[0]) == CT_NUMELEMS(inAv1.segmentation_feature_data[0]));

    const size_t numFeatures = CT_NUMELEMS(pOut->segmentation.feature_data[0]);

    for (size_t ii = 0; ii < CT_NUMELEMS(inAv1.segmentation_feature_data); ii++)
    {
        for (size_t jj = 0; jj < numFeatures; jj++)
        {
            pOut->segmentation.feature_data[ii][jj] = inAv1.segmentation_feature_data[ii][jj];
            pOut->segmentation.feature_mask[ii] |=
                (inAv1.segmentation_feature_enable[ii][jj] & 1) << jj;
        }
    }

    // film grain
    if (pOut->film_grain_enable)
    {
        if (inAv1.pFgsPic == nullptr)
        {
            pOut->fgsPic = nullptr;
        }
        else
        {
            // With film grain enabled and applied, output data will be stored in below
            // surface rather than target surface. So DisplayPicture receives below
            // PicBuff. However, we need to still wait for the fence passed by hardware
            // so set the fence in pFpsPicBuff to point to the fence used by hardware
            MyPicBuff *pFpsPicBuff = static_cast<MyPicBuff*>(inAv1.pFgsPic);
            pOut->fgsPic = pFpsPicBuff->GetSurface();
            ShareFence(pLwrrPic, pFpsPicBuff);
        }

        pOut->film_grain.apply_grain = inAv1.fgs.apply_grain;
        pOut->film_grain.scaling_shift_minus8 = inAv1.fgs.scaling_shift_minus8;
        pOut->film_grain.chroma_scaling_from_luma = inAv1.fgs.chroma_scaling_from_luma;
        pOut->film_grain.ar_coeff_lag = inAv1.fgs.ar_coeff_lag;
        pOut->film_grain.ar_coeff_shift_minus6 = inAv1.fgs.ar_coeff_shift_minus6;
        pOut->film_grain.grain_scale_shift = inAv1.fgs.grain_scale_shift;
        pOut->film_grain.overlap_flag = inAv1.fgs.overlap_flag;
        pOut->film_grain.clip_to_restricted_range = inAv1.fgs.clip_to_restricted_range;
        pOut->film_grain.grain_seed = inAv1.fgs.grain_seed;

        pOut->film_grain.num_y_points = inAv1.fgs.num_y_points;
        pOut->film_grain.num_cb_points = inAv1.fgs.num_cb_points;
        pOut->film_grain.num_cr_points = inAv1.fgs.num_cr_points;


        MEM_COPY(pOut->film_grain.scaling_points_y, inAv1.fgs.scaling_points_y);
        MEM_COPY(pOut->film_grain.scaling_points_cb, inAv1.fgs.scaling_points_cb);
        MEM_COPY(pOut->film_grain.scaling_points_cr, inAv1.fgs.scaling_points_cr);
        MEM_COPY(pOut->film_grain.ar_coeffs_y, inAv1.fgs.ar_coeffs_y);
        MEM_COPY(pOut->film_grain.ar_coeffs_cb, inAv1.fgs.ar_coeffs_cb);
        MEM_COPY(pOut->film_grain.ar_coeffs_cr, inAv1.fgs.ar_coeffs_cr);

        pOut->film_grain.cb_mult = inAv1.fgs.cb_mult;
        pOut->film_grain.cb_luma_mult = inAv1.fgs.cb_luma_mult;
        pOut->film_grain.cb_offset = inAv1.fgs.cb_offset;
        pOut->film_grain.cr_mult = inAv1.fgs.cr_mult;
        pOut->film_grain.cr_luma_mult = inAv1.fgs.cr_luma_mult;
        pOut->film_grain.cr_offset = inAv1.fgs.cr_offset;

    }

    // AES encryption params
    CHECK_RC(SetEncryptParams(&pOut->encryptParams, inAv1.encryptParams));

    // TVMRFrameTypeInfo frameType; TODO
    // TVMRSliceInfo sliceInfo; TODO

    const size_t maxRefFrames = CT_NUMELEMS(pOut->RefPics);
    // Max Ref frames is 8 so brute force implementation is reasonable
    // Count number of unique reference frames
    if (pOut->frame_type == AV1_INTER_FRAME)
    {
        for (size_t ii = 1; ii < maxRefFrames; ii++)
        {
            size_t jj = 0;
            for (jj = 0; jj < ii; jj++)
            {
                if (pOut->RefPics[ii] == pOut->RefPics[jj])
                    break;
            }
            if (ii == jj)
            {
                pOut->numRefFrames++;
            }
        }
    }

    pOut->skip_ref0 = inAv1.skip_ref0;
    pOut->skip_ref1 = inAv1.skip_ref1;
    pOut->show_existing_frame_index = inAv1.show_existing_frame_index;
    pOut->refresh_frame_flags = inAv1.refresh_frame_flags;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Copy VP9 picture info from lwparser format to lwtvmr format
//!
//! This method is used by DecodePicture() for VP9 streams, to
//! translate the picture header parsed by lwparser into the struct
//! needed by lwtvmr.
//!
RC TvmrDecoder::SetPictureInfoVp9
(
    TVMRPictureInfoVP9   *pOut,
    const LWDPictureData &in,
    MyPicBuff            *pLwrrPic
)
{
    MASSERT(pOut != nullptr);
    const LWVP9PictureData           &ilwp9   = in.CodecSpecific.vp9;
    const lwparser_vp9EntropyProbs_t &inEnt   = ilwp9.entropy;
    tvmr_vp9EntropyProbs_t           *pOutEnt = &pOut->entropy;
    RC rc;

    memset(pOut, 0, sizeof(*pOut));
    if (ilwp9.pLastRef)
    {
        pOut->LastReference = pLwrrPic->AddPrereq(ilwp9.pLastRef);
        pOut->ref0_width    = pOut->LastReference->width;
        pOut->ref0_height   = pOut->LastReference->height;
    }
    if (ilwp9.pGoldenRef)
    {
        pOut->GoldenReference = pLwrrPic->AddPrereq(ilwp9.pGoldenRef);
        pOut->ref1_width      = pOut->GoldenReference->width;
        pOut->ref1_height     = pOut->GoldenReference->height;
    }
    if (ilwp9.pAltRef)
    {
        pOut->AltReference = pLwrrPic->AddPrereq(ilwp9.pAltRef);
        pOut->ref2_width   = pOut->AltReference->width;
        pOut->ref2_height  = pOut->AltReference->height;
    }

    pOut->width                               = ilwp9.width;
    pOut->height                              = ilwp9.height;
    pOut->keyFrame                            = ilwp9.keyFrame;
    pOut->bit_depth                           = ilwp9.bit_depth;
    pOut->prevIsKeyFrame                      = ilwp9.prevIsKeyFrame;
    pOut->PrevShowFrame                       = ilwp9.PrevShowFrame;
    pOut->resolutionChange                    = ilwp9.resolutionChange;
    pOut->errorResilient                      = ilwp9.errorResilient;
    pOut->intraOnly                           = ilwp9.intraOnly;
    pOut->frameContextIdx                     = ilwp9.frameContextIdx;
    MEM_COPY(pOut->refFrameSignBias,            ilwp9.refFrameSignBias);
    pOut->loopFilterLevel                     = ilwp9.loopFilterLevel;
    pOut->loopFilterSharpness                 = ilwp9.loopFilterSharpness;
    pOut->qpYAc                               = ilwp9.qpYAc;
    pOut->qpYDc                               = ilwp9.qpYDc;
    pOut->qpChAc                              = ilwp9.qpChAc;
    pOut->qpChDc                              = ilwp9.qpChDc;
    pOut->lossless                            = ilwp9.lossless;
    pOut->transform_mode                      = ilwp9.transform_mode;
    pOut->allow_high_precision_mv             = ilwp9.allow_high_precision_mv;
    pOut->allow_comp_inter_inter              = ilwp9.allow_comp_inter_inter;
    pOut->mcomp_filter_type                   = ilwp9.mcomp_filter_type;
    pOut->comp_pred_mode                      = ilwp9.comp_pred_mode;
    pOut->comp_fixed_ref                      = ilwp9.comp_fixed_ref;
    MEM_COPY(pOut->comp_var_ref,                ilwp9.comp_var_ref);
    pOut->log2_tile_columns                   = ilwp9.log2_tile_columns;
    pOut->log2_tile_rows                      = ilwp9.log2_tile_rows;
    pOut->segmentEnabled                      = ilwp9.segmentEnabled;
    pOut->segmentMapUpdate                    = ilwp9.segmentMapUpdate;
    pOut->segmentMapTemporalUpdate            = ilwp9.segmentMapTemporalUpdate;
    pOut->segmentFeatureMode                  = ilwp9.segmentFeatureMode;
    MEM_COPY(pOut->segmentFeatureEnable,        ilwp9.segmentFeatureEnable);
    MEM_COPY(pOut->segmentFeatureData,          ilwp9.segmentFeatureData);
    pOut->modeRefLfEnabled                    = ilwp9.modeRefLfEnabled;
    MEM_COPY(pOut->mbRefLfDelta,                ilwp9.mbRefLfDelta);
    MEM_COPY(pOut->mbModeLfDelta,               ilwp9.mbModeLfDelta);
    pOut->offsetToDctParts                    = ilwp9.offsetToDctParts;
    pOut->frameTagSize                        = ilwp9.frameTagSize;

    MEM_COPY(pOutEnt->kf_bmode_prob,            inEnt.kf_bmode_prob);
    MEM_COPY(pOutEnt->kf_bmode_probB,           inEnt.kf_bmode_probB);
    MEM_COPY(pOutEnt->ref_pred_probs,           inEnt.ref_pred_probs);
    MEM_COPY(pOutEnt->mb_segment_tree_probs,    inEnt.mb_segment_tree_probs);
    MEM_COPY(pOutEnt->segment_pred_probs,       inEnt.segment_pred_probs);
    MEM_COPY(pOutEnt->ref_scores,               inEnt.ref_scores);
    MEM_COPY(pOutEnt->prob_comppred,            inEnt.prob_comppred);
    MEM_COPY(pOutEnt->pad1,                     inEnt.pad1);
    MEM_COPY(pOutEnt->kf_uv_mode_prob,          inEnt.kf_uv_mode_prob);
    MEM_COPY(pOutEnt->kf_uv_mode_probB,         inEnt.kf_uv_mode_probB);
    MEM_COPY(pOutEnt->pad2,                     inEnt.pad2);
    MEM_COPY(pOutEnt->a.inter_mode_prob,        inEnt.a.inter_mode_prob);
    MEM_COPY(pOutEnt->a.intra_inter_prob,       inEnt.a.intra_inter_prob);
    MEM_COPY(pOutEnt->a.uv_mode_prob,           inEnt.a.uv_mode_prob);
    MEM_COPY(pOutEnt->a.tx8x8_prob,             inEnt.a.tx8x8_prob);
    MEM_COPY(pOutEnt->a.tx16x16_prob,           inEnt.a.tx16x16_prob);
    MEM_COPY(pOutEnt->a.tx32x32_prob,           inEnt.a.tx32x32_prob);
    MEM_COPY(pOutEnt->a.sb_ymode_probB,         inEnt.a.sb_ymode_probB);
    MEM_COPY(pOutEnt->a.sb_ymode_prob,          inEnt.a.sb_ymode_prob);
    MEM_COPY(pOutEnt->a.partition_prob,         inEnt.a.partition_prob);
    MEM_COPY(pOutEnt->a.uv_mode_probB,          inEnt.a.uv_mode_probB);
    MEM_COPY(pOutEnt->a.switchable_interp_prob, inEnt.a.switchable_interp_prob);
    MEM_COPY(pOutEnt->a.comp_inter_prob,        inEnt.a.comp_inter_prob);
    MEM_COPY(pOutEnt->a.mbskip_probs,           inEnt.a.mbskip_probs);
    MEM_COPY(pOutEnt->a.pad1,                   inEnt.a.pad1);
    MEM_COPY(pOutEnt->a.nmvc.joints,            inEnt.a.nmvc.joints);
    MEM_COPY(pOutEnt->a.nmvc.sign,              inEnt.a.nmvc.sign);
    MEM_COPY(pOutEnt->a.nmvc.class0,            inEnt.a.nmvc.class0);
    MEM_COPY(pOutEnt->a.nmvc.fp,                inEnt.a.nmvc.fp);
    MEM_COPY(pOutEnt->a.nmvc.class0_hp,         inEnt.a.nmvc.class0_hp);
    MEM_COPY(pOutEnt->a.nmvc.hp,                inEnt.a.nmvc.hp);
    MEM_COPY(pOutEnt->a.nmvc.classes,           inEnt.a.nmvc.classes);
    MEM_COPY(pOutEnt->a.nmvc.class0_fp,         inEnt.a.nmvc.class0_fp);
    MEM_COPY(pOutEnt->a.nmvc.bits,              inEnt.a.nmvc.bits);
    MEM_COPY(pOutEnt->a.single_ref_prob,        inEnt.a.single_ref_prob);
    MEM_COPY(pOutEnt->a.comp_ref_prob,          inEnt.a.comp_ref_prob);
    MEM_COPY(pOutEnt->a.pad2,                   inEnt.a.pad2);
    MEM_COPY(pOutEnt->a.probCoeffs,             inEnt.a.probCoeffs);
    MEM_COPY(pOutEnt->a.probCoeffs8x8,          inEnt.a.probCoeffs8x8);
    MEM_COPY(pOutEnt->a.probCoeffs16x16,        inEnt.a.probCoeffs16x16);
    MEM_COPY(pOutEnt->a.probCoeffs32x32,        inEnt.a.probCoeffs32x32);

    CHECK_RC(SetEncryptParams(&pOut->encryptParams, ilwp9.encryptParams));
    return rc;
}

//--------------------------------------------------------------------
// JavaScript interface
//
JS_CLASS_INHERIT(TegraLwdecTest, GpuTest, "TegraLwdecTest object");
CLASS_PROP_READWRITE(TegraLwdecTest, TestStreams, string,
    "Regex of streams to test.  Empty string (default) means 'all'.");
CLASS_PROP_READWRITE(TegraLwdecTest, SkipStreams, string,
    "Regex of streams to skip.  "
    "Default skips streams used only for simulation; empty string disables skip.");
CLASS_PROP_READWRITE(TegraLwdecTest, EngineSkipMask, UINT32,
    "Bitmask indicating which engines will be skipped (default=0).");
CLASS_PROP_READWRITE(TegraLwdecTest, KeepRunning, bool,
    "The test will keep running as long as this flag is set");
CLASS_PROP_READWRITE(TegraLwdecTest, SaveDecodedImages, bool,
    "Save decoded images");
