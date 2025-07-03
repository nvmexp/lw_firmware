/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <algorithm>
#include <map>
#include <string>

#include <boost/container/vector.hpp>

// Undefine our ALIGN_UP to avoid compiler error,
// because c4b0_drv.h defines its own copy
#ifdef ALIGN_UP
#   undef ALIGN_UP
#endif
#include "class/c4b0_drv.h"
// Undefine ALIGN_UP coming from c4b0_drv.h
#ifdef ALIGN_UP
#   undef ALIGN_UP
#endif

#include "ctr64encryptor.h"

#include "core/include/channel.h"
#include "core/include/golden.h"
#include "core/include/massert.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/rc.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surffill.h"

#include "h264parser.h"
#include "h265parser.h"
#include "h265syntax.h"
#include "h265dpbmanager.h"

#include "clc4b0engine.h"
#include "class/clc6b0.h"

using Utility::AlignUp;
using Utility::StrPrintf;
using Platform::VirtualRd;

class ClC4B0Engine::H264Ctrl
{
public:
    H264Ctrl();

    void SetSlicesSize(unsigned int slicesSize);
    unsigned int GetSlicesSize(void const *buf) const;
    void SetSlicesCount(unsigned int slicesCount);
    void SetMBHistBufSize(unsigned int mbHistBufSize);
    void const * GetSliceOffsets() const;
    size_t GetSliceOffsetsSize() const;
    string SliceOffsetsToText(void const *buf, size_t numSlices) const;

    void const * GetPicSetup() const;
    size_t GetPicSetupSize() const;

    size_t GetMaxDPBSize() const;
    unsigned int GetLwrrPicIdx() const;
    unsigned int GetLwrrColIdx() const;
    unsigned int GetDPBPicIdx(size_t idx) const;
    unsigned int GetDPBColIdx(size_t idx) const;
    bool IsDPBEntryUsed(size_t idx) const;

    void SetLwrrPicIdx(unsigned int val);
    void UpdateDPB
    (
        const H264::Picture &pic,
        size_t whereIdx,
        H264::DPB::const_iterator fromWhat,
        unsigned int lwdecRefIdx,
        MemLayout memLayout
    );
    void SetOutputMemoryLayout(MemLayout memLayout);
    void SetHistBufferSize(unsigned int size);
    void EnableHistogram(short startX, short startY, short endX, short endY);
    void SetPitchOffset(const Surface2D &surfY, const Surface2D &surfUV);

    void UpdateFromPicture(const H264::Picture &pic);
    void EnableEncryption
    (
        unsigned int keyIncrement,
        const UINT32 sessionKey[4],
        const UINT32 contentKey[4],
        const UINT32 initVector[4]
    );
    string PicSetupToText(void const *buf) const;

private:
    lwdec_h264_pic_s m_ps;
    vector<UINT32>   m_sliceOffsets;
};

class ClC4B0Engine::H265Ctrl
{
public:
    H265Ctrl();

    void const* GetPicSetup() const;
    size_t      GetPicSetupSize() const;
    void const* GetScalingMatrices() const;
    size_t      GetScalingMatricesSize() const;
    void const* GetTileInfo() const;
    size_t      GetTileInfoSize() const;

    void UpdateFromPicture(const H265::Picture &pic);
    void UpdateDPB
    (
        INT32 lwrrPicPoc,
        const UINT08 l0[H265::maxNumRefPics],
        const UINT08 l1[H265::maxNumRefPics],
        const INT32 pocs[H265::maxMaxDpbSize],
        UINT16 ltMask,
        unsigned char numRefFrames
    );
    void SetLwrrPicIdx(unsigned int lwrPicIdx);
    unsigned int GetLwrrPicIdx() const { return m_ps.lwrr_pic_idx; }
    void SetPitch(const Surface2D &surfY, const Surface2D &surfUV, OutputSize outSize);
    void SetColMvBufSize(unsigned int size);
    void SetSaoBufferOffset(unsigned int offset);
    void SetBsdCtrlOffset(unsigned int offset);
    void SetFilterAboveBufferOffset(unsigned int offset);
    void SetSaoAboveBufferOffset(unsigned int offset);
    void SetSlicesSize(unsigned int slicesSize);
    unsigned int GetSlicesSize(void const *buf) const;
    string PicSetupToText(void const * buf) const;
    string ScalingMatricesToText(void const *buf) const;
    string TileInfoToText(void const * buf, const H265::PicParameterSet &pps) const;

private:
    struct ScalingMatrices
    {
        ScalingMatrices();

        UINT08 m_scalingFactorDC16x16[6];
        UINT08 m_scalingFactorDC32x32[2];
        UINT08 reserved[8];

        // Values in the following matrices are assumed to be stored column by
        // column, i.e. for example m_scalingFactor16x16[*][x][y].
        UINT08 m_scalingFactor4x4[6][4][4];
        UINT08 m_scalingFactor8x8[6][8][8];
        UINT08 m_scalingFactor16x16[6][8][8];
        UINT08 m_scalingFactor32x32[2][8][8];
    };

    typedef vector<UINT16> TileInfoBuf;

    static const UINT32 maxTileCols = 20;
    static const UINT32 maxTileRows = 22;

    static const UINT32 TileInfoValSize = sizeof(TileInfoBuf::value_type);

    // from //hw/gpu_ip/unit/lwdec/2.0_gm206/cmod/gip_cmod/software/source/hevc/hevc_asic.c
    static const UINT32 pxlColwOffset =
        ((((maxTileCols * maxTileRows * 2 * TileInfoValSize + 15 + 16) & ~0xF) + 255) & ~0xFF)
        / TileInfoValSize;

    lwdec_hevc_pic_s m_ps;
    ScalingMatrices  m_sm;
    TileInfoBuf      m_ti;
};

ClC4B0Engine::ClC4B0Engine
(
    Channel* lwCh,
    LwRm::Handle hLwdec,
    Memory::Location semLoc,
    Tee::Priority printPri,
    bool useFeSem,
    bool useIntraFrame
)
  : m_LwdecHandle(hLwdec)
  , m_PrintPri(printPri)
  , m_SemLoc(semLoc)
  , m_LwCh(lwCh)
  , m_UseIntraFrame(useIntraFrame)
{
    MASSERT(m_LwCh);

    memset(m_WrappedSessionKey, 0, sizeof(m_WrappedSessionKey));
    memset(m_WrappedContentKey, 0, sizeof(m_WrappedContentKey));
    memset(m_ContentKey, 0, sizeof(m_ContentKey));
    memset(m_InitVector, 0, sizeof(m_InitVector));

    if (nullptr != m_LwCh)
    {
        m_Semaphore.Setup(NotifySemaphore::ONE_WORD, m_SemLoc, 1);
        m_SemAllocResult = m_Semaphore.Allocate(m_LwCh, 0);
        m_Semaphore.SetPayload(0);
    }
}

ClC4B0Engine::ClC4B0Engine(const ClC4B0Engine &x)
  : m_InStreamFileName(x.m_InStreamFileName)
  , m_InStreamGoldSuffix(x.m_InStreamGoldSuffix)
  , m_H264parser(x.m_H264parser)
  , m_H265parser(x.m_H265parser)
  , m_LwrPic(x.m_LwrPic)
  , m_OutputPics(x.m_OutputPics)
  , m_LastOutPics(x.m_LastOutPics)
  , m_H264MemLayout(x.m_H264MemLayout)
  , m_H264EncMemLayout(x.m_H264EncMemLayout)
  , m_H265MemLayout(x.m_H265MemLayout)
  , m_GoldOutSurfLoopCnt(x.m_GoldOutSurfLoopCnt)
  , m_GoldHistSurfLoopCnt(x.m_GoldHistSurfLoopCnt)
  , m_OutFramesCount(x.m_OutFramesCount)
  , m_InSurf(x.m_InSurf)
  , m_HistogramSurf(x.m_HistogramSurf)
  , m_OutSurfsY(x.m_OutSurfsY)
  , m_OutSurfsUV(x.m_OutSurfsUV)
  , m_H264Ctrl(x.m_H264Ctrl)
  , m_H265Ctrl(x.m_H265Ctrl)
  , m_H264HistBufSize(x.m_H264HistBufSize)
  , m_H264ColocBufSize(x.m_H264ColocBufSize)
  , m_H264MbHistBufSize(x.m_H264MbHistBufSize)
  , m_H265ColMvBufSize(x.m_H265ColMvBufSize)
  , m_TotFilterBufferSize(x.m_TotFilterBufferSize)
  , m_SaoBufferOffset(x.m_SaoBufferOffset)
  , m_BsdBufferOffset(x.m_BsdBufferOffset)
  , m_FilterAboveBufferOffset(x.m_FilterAboveBufferOffset)
  , m_SaoAboveBufferOffset(x.m_SaoAboveBufferOffset)
  , m_LwdecHandle(x.m_LwdecHandle)
  , m_Encryption(x.m_Encryption)
  , m_OutputSize(x.m_OutputSize)
  , m_PrintPri(x.m_PrintPri)
  , m_InitedTo(x.m_InitedTo)
  , m_KeyIncrement(x.m_KeyIncrement)
  , m_SemPayload(x.m_SemPayload)
  , m_SemLoc(x.m_SemLoc)
  , m_LwCh(x.m_LwCh)
  , m_UseIntraFrame(x.m_UseIntraFrame)
{
    memcpy(m_WrappedSessionKey, x.m_WrappedSessionKey, sizeof(m_WrappedSessionKey));
    memcpy(m_WrappedContentKey, x.m_WrappedContentKey, sizeof(m_WrappedContentKey));
    memcpy(m_ContentKey, x.m_ContentKey, sizeof(m_ContentKey));
    memcpy(m_InitVector, x.m_InitVector, sizeof(m_InitVector));

    if (x.m_Semaphore.IsAllocated())
    {
        m_Semaphore.Setup(NotifySemaphore::ONE_WORD, m_SemLoc, 1);
        m_SemAllocResult = m_Semaphore.Allocate(m_LwCh, 0);
        m_Semaphore.SetPayload(0); // zero is not a typo
    }
    else
    {
        m_SemAllocResult = x.m_SemAllocResult;
    }
}

ClC4B0Engine::~ClC4B0Engine()
{
}

template <class H264Layouter>
void ClC4B0Engine::BuildH264Layout(H264Layouter *layouter)
{
    MASSERT(layouter);

    H264::Parser::pic_iterator picIt;
    H264CtrlCont::iterator ctrlIt;

    // Layout slices and control structures
    for (ctrlIt = m_H264Ctrl.begin(), picIt = m_H264parser->pic_begin();
         m_H264parser->pic_end() != picIt; ++picIt, ++ctrlIt)
    {
        layouter->AddPicture(picIt->slices_begin(), picIt->slices_end());
        if (m_Encryption)
        {
            ctrlIt->EnableEncryption(
                m_KeyIncrement,
                GetWrappedSessionKey(),
                GetWrappedContentKey(),
                layouter->GetInitializatiolwector()
            );
        }
        layouter->AddPicSetup(ctrlIt->GetPicSetup(), ctrlIt->GetPicSetupSize());
        layouter->AddSliceOffsets(ctrlIt->GetSliceOffsets(), ctrlIt->GetSliceOffsetsSize());
    }

    layouter->AddHistBuffer(m_H264HistBufSize);
    layouter->AddColocBuffer(m_H264ColocBufSize);
    layouter->AddMBHistBuffer(m_H264MbHistBufSize);
}

template <class H265Layouter>
void ClC4B0Engine::BuildH265Layout(H265Layouter *layouter)
{
    MASSERT(layouter);

    H265::Parser::pic_iterator picIt;
    H265CtrlCont::iterator ctrlIt;

    for (ctrlIt = m_H265Ctrl.begin(), picIt = m_H265parser->pic_begin();
         m_H265parser->pic_end() != picIt; ++picIt, ++ctrlIt)
    {
        layouter->AddPicture(picIt->slices_begin(), picIt->slices_end());
        layouter->AddPicSetup(ctrlIt->GetPicSetup(), ctrlIt->GetPicSetupSize());
        layouter->AddScalingMtx(ctrlIt->GetScalingMatrices(), ctrlIt->GetScalingMatricesSize());
        layouter->AddTilesInfo(ctrlIt->GetTileInfo(), ctrlIt->GetTileInfoSize());
    }

    layouter->AddColocBuffer(m_H265ColMvBufSize);
    layouter->AddFilterBuffer(m_TotFilterBufferSize);
}

UINT32 ClC4B0Engine::CalcH264LayoutSize()
{
    if (!m_Encryption)
    {
        typedef H264Layouter<SizeCounter, OffsetsNilStorage, NilEncryptor> SizeEstimator;
        SizeEstimator sizeEstimator;
        sizeEstimator.SetSliceAlign(GetH264SlicesAlignment());
        sizeEstimator.SetControlAlign(GetControlStructsAlignment());
        BuildH264Layout(&sizeEstimator);
        return sizeEstimator.SizeInBytes();
    }
    else
    {
        typedef H264Layouter<SizeCounter, OffsetsNilStorage, CTR64Encryptor> SizeEstimator;
        SizeEstimator sizeEstimator;
        sizeEstimator.SetSliceAlign(GetH264EncSlicesAlignment());
        sizeEstimator.SetControlAlign(GetControlStructsAlignment());
        BuildH264Layout(&sizeEstimator);
        return sizeEstimator.SizeInBytes();
    }
}

UINT32 ClC4B0Engine::CalcH265LayoutSize()
{
    typedef H265Layouter<SizeCounter, OffsetsNilStorage> SizeEstimator;
    SizeEstimator sizeEstimator;
    sizeEstimator.SetSliceAlign(GetH265SlicesAlignment());
    sizeEstimator.SetControlAlign(GetControlStructsAlignment());
    BuildH265Layout(&sizeEstimator);
    return sizeEstimator.SizeInBytes();
}

RC ClC4B0Engine::InitH264(GpuDevice *dev, GpuSubdevice *subDev, LwRm::Handle chHandle)
{
    RC rc;

    m_H264Ctrl.clear();
    m_H265Ctrl.clear();

    m_InitedTo = h264;
    UpdateControlStructsFromPics();

    // Sizes of the buffers below are taken from
    // //hw/gpu_ip/unit/msdec/5.0_merge/cmod/h264/drv and
    // //sw/dev/gpu_drv/chips_a/drivers/video/decode/src. There is no
    // documentation, explaining these values.

    // From CLWDECH264Decoder::Initialize in
    // //sw/dev/gpu_drv/chips_a/drivers/video/decode/src/lwLWDECH264.cpp.
    m_H264HistBufSize = 3 * 256 * m_H264parser->GetWidthInMb();

    // From tvecdrvgen_c::coloc_init_sequence in
    // //hw/gpu_ip/unit/msdec/5.0_merge/cmod/h264/drv/tvecdrvgen.cpp.
    unsigned int colocBufSizeInMb;
    unsigned int widthInMb  = m_H264parser->GetWidthInMb();
    unsigned int heightInMb = m_H264parser->GetHeightInMb();
    colocBufSizeInMb = widthInMb * AlignUp<2>(heightInMb);
    colocBufSizeInMb = AlignUp<4>(colocBufSizeInMb);
    m_H264ColocBufSize = static_cast<UINT32>(
        64 * colocBufSizeInMb * (m_H264parser->GetMaxDpbSize() + 1)
    );
    m_H264ColocBufSize = AlignUp<256>(m_H264ColocBufSize);

    // From //hw/lwgpu_gmlit1/class/mfs/class/lwdec/gmlit1_lwdec_decode.mfs.
    m_H264MbHistBufSize = AlignUp<256>(widthInMb * 104);

    m_InSurf.SetColorFormat(ColorUtils::Y8);
    m_InSurf.SetWidth(CalcH264LayoutSize());
    m_InSurf.SetHeight(1);
    m_InSurf.SetProtect(Memory::ReadWrite);
    m_InSurf.SetLayout(Surface2D::Pitch);
    m_InSurf.SetDisplayable(false);
    m_InSurf.SetType(LWOS32_TYPE_IMAGE);
    m_InSurf.SetLocation(Memory::Optimal);
    CHECK_RC(m_InSurf.Alloc(dev));
    CHECK_RC(m_InSurf.BindGpuChannel(chHandle));
    CHECK_RC(m_InSurf.Fill(0, subDev->GetSubdeviceInst()));

    return rc;
}

RC ClC4B0Engine::InitH265(GpuDevice *dev, GpuSubdevice *subDev, LwRm::Handle chHandle)
{
    RC rc;

    m_H264Ctrl.clear();
    m_H265Ctrl.clear();

    m_InitedTo = h265;
    UpdateControlStructsFromPics();

    UINT32 dpbSize = m_H265parser->GetMaxDecPicBuffering();

    // In H.265 the granularity of co-located MV data is 16x16 units (regardless
    // of CTB size). The code below aligning to max CTB size (64x64), dividing
    // by 256 (to get the number of 16x16 units), and multiplying by 16 (16
    // bytes per 16x16 unit).
    m_H265ColMvBufSize = AlignUp<256>(
        AlignUp<64>(m_H265parser->GetWidth()) * AlignUp<64>(m_H265parser->GetHeight()) / 16 * dpbSize
    );

    UINT32 aligned16Height = AlignUp<16>(m_H265parser->GetHeight());
    UINT32 filterBufferSize = 2 * 16 * 15 * aligned16Height;
    UINT32 saoBufferSize = 2 * 128 * 15 * aligned16Height;
    UINT32 bsdBufferSize = 4 * 15 * aligned16Height;

    m_TotFilterBufferSize = AlignUp<256>(filterBufferSize) + AlignUp<256>(saoBufferSize) +
                            AlignUp<256>(bsdBufferSize);
    m_SaoBufferOffset = AlignUp<256>(filterBufferSize) / 256;
    m_BsdBufferOffset = (AlignUp<256>(filterBufferSize) + AlignUp<256>(saoBufferSize)) / 256;

    // from drivers/multimedia/codecs/lwca/lwlwvid/src/LWDEC3Decode.cpp

    constexpr unsigned hevcBurstLengthInBytes = 16;
    static const unsigned hevcBurstLenFlt[2][2][3] =
    {
        // YUV 4:2:0
        {
            // 8 bits
            {11, 22, 44}, // CTB16/32/64
            {15, 30, 60}  // CTB16/32/64
        },
        // YUV 4:4:4
        {
            // 12 bits
            {15, 30, 60}, // CTB16/32/64
            {21, 42, 84}  // CTB16/32/64
        }
    };
    static const unsigned hevcBurstLenSao[2][2][3] =
    {
        // YUV 4:2:0
        {
            // 8 bits
            {5, 7, 11},  // CTB16/32/64
            {7, 10, 16}  // CTB16/32/64
        },
        // YUV 4:4:4
        {
            // 12 bits
            {6, 9, 15}, // CTB16/32/64
            {8, 13, 23} // CTB16/32/64
        }

    };

    size_t bitDepthIdx = 8 < m_H265parser->GetBitDepthLuma() ? 1 : 0;
    size_t chromaIdx = 1 == m_H265parser->GetChromaFormatIdc() ? 0 :1;
    size_t ctbIdx = m_H265parser->CtbLog2SizeY() - 4;

    UINT32 filterAboveBufferSize = AlignUp<256>
    (
        AlignUp<8>(hevcBurstLenFlt[chromaIdx][bitDepthIdx][ctbIdx]) *
        hevcBurstLengthInBytes *
        (m_H265parser->PicWidthInCtbsY() + 1) * m_H265parser->PicHeightInCtbsY()
    );

    UINT32 saoAboveBufferSize = AlignUp<256>
    (
        AlignUp<8>(hevcBurstLenSao[chromaIdx][bitDepthIdx][ctbIdx]) *
        hevcBurstLengthInBytes *
        m_H265parser->PicWidthInCtbsY() * m_H265parser->PicHeightInCtbsY()
    );

    m_FilterAboveBufferOffset = m_TotFilterBufferSize / 256;
    m_TotFilterBufferSize += filterAboveBufferSize;
    m_SaoAboveBufferOffset = m_TotFilterBufferSize / 256;
    m_TotFilterBufferSize += saoAboveBufferSize;

    m_InSurf.SetColorFormat(ColorUtils::Y8);
    m_InSurf.SetWidth(CalcH265LayoutSize());
    m_InSurf.SetHeight(1);
    m_InSurf.SetProtect(Memory::ReadWrite);
    m_InSurf.SetLayout(Surface2D::Pitch);
    m_InSurf.SetDisplayable(false);
    m_InSurf.SetType(LWOS32_TYPE_IMAGE);
    m_InSurf.SetLocation(Memory::Optimal);
    CHECK_RC(m_InSurf.Alloc(dev));
    CHECK_RC(m_InSurf.BindGpuChannel(chHandle));
    CHECK_RC(m_InSurf.Fill(0, subDev->GetSubdeviceInst()));

    return rc;
}

RC ClC4B0Engine::DoInitFromH264File
(
    const char *fileName,
    const TarFile *tarFile,
    GpuDevice *dev,
    GpuSubdevice *subDev,
    LwRm::Handle chHandle
)
{
    using Utility::AlignUp;

    MASSERT(nullptr != dev);
    MASSERT(nullptr != subDev);

    RC rc;

    vector<UINT08> stream;
    stream.resize(tarFile->GetSize());
    tarFile->Read(reinterpret_cast<char *>(&stream[0]), static_cast<unsigned>(stream.size()));

    const char *dotPos = find(
        reverse_iterator<const char *>(&fileName[0] + strlen(fileName)),
        reverse_iterator<const char *>(&fileName[0]),
        '.').base();
    if (&fileName[0] != dotPos)
    {
        m_InStreamFileName.assign(&fileName[0], dotPos - 1);
    }
    else
    {
        m_InStreamFileName = fileName;
    }

    m_H264parser = make_shared<H264::Parser>();
    m_H265parser.reset();

    CHECK_RC(m_H264parser->ParseStream(stream.begin(), stream.end()));
    CHECK_RC(InitH264(dev, subDev, chHandle));

    return rc;
}

RC ClC4B0Engine::DoInitFromH265File
(
    const char *fileName,
    const TarFile *tarFile,
    GpuDevice *dev,
    GpuSubdevice *subDev,
    LwRm::Handle chHandle
)
{
    using Utility::AlignUp;

    MASSERT(nullptr != dev);
    MASSERT(nullptr != subDev);

    RC rc;

    vector<UINT08> stream;
    stream.resize(tarFile->GetSize());
    tarFile->Read(reinterpret_cast<char *>(&stream[0]), static_cast<unsigned>(stream.size()));

    const char *dotPos = find(
        reverse_iterator<const char *>(&fileName[0] + strlen(fileName)),
        reverse_iterator<const char *>(&fileName[0]),
        '.').base();
    if (&fileName[0] != dotPos)
    {
        m_InStreamFileName.assign(&fileName[0], dotPos - 1);
    }
    else
    {
        m_InStreamFileName = fileName;
    }

    m_H264parser.reset();
    m_H265parser = make_shared<H265::Parser>();

    CHECK_RC(m_H265parser->ParseStream(stream.begin(), stream.end()));
    CHECK_RC(InitH265(dev, subDev, chHandle));

    return rc;
}

RC ClC4B0Engine::DoInitFromAnother
(
    GpuDevice *dev,
    GpuSubdevice *subDev,
    LwRm::Handle chHandle,
    Engine &another
)
{
    RC rc;

    auto &ae = dynamic_cast<ClC4B0Engine &>(another);
    m_InStreamFileName = ae.GetInStreamFileName();
    m_H264parser = ae.GetH264Parser();
    m_H265parser = ae.GetH265Parser();

    if (m_H264parser)
    {
        CHECK_RC(InitH264(dev, subDev, chHandle));
    }
    else if (m_H265parser)
    {
        CHECK_RC(InitH265(dev, subDev, chHandle));
    }

    return rc;
}

RC ClC4B0Engine::DoInitOutput
(
    GpuDevice *dev,
    GpuSubdevice *subDev,
    LwRm::Handle chHandle,
    FLOAT64 skTimeout
)
{
    RC rc;

    UINT32 numOutSurf = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    bool hasMoreThan8Bits = false;

    if (h264 == m_InitedTo)
    {
        numOutSurf = static_cast<UINT32>(m_H264parser->GetMaxDpbSize() + 1);
        width = m_H264parser->GetWidth();
        height = m_H264parser->GetHeight();
    }
    else if (h265 == m_InitedTo)
    {
        // in H.265 DPB includes the current picture, so we don't have to add 1
        // to get the total number of output surfaces needed for decoding
        numOutSurf = m_H265parser->GetMaxDecPicBuffering();
        width = m_H265parser->GetWidth();
        height = m_H265parser->GetHeight();

        H265::Parser::pic_const_iterator picIt;
        bool firstIter = true;
        for (picIt = m_H265parser->pic_begin(); m_H265parser->pic_end() != picIt; ++picIt)
        {
            if (firstIter)
            {
                hasMoreThan8Bits = 0 < picIt->GetSPS().bit_depth_luma_minus8 ||
                                   0 < picIt->GetSPS().bit_depth_chroma_minus8;
                firstIter = false;
            }
            else
            {
                // We do not support changing of bit depth of the output surfaces,
                // therefore all pictures must use either exactly 8 bits per each
                // channel or all have at least one channel with more than 8 bits.
                MASSERT(hasMoreThan8Bits == (0 < picIt->GetSPS().bit_depth_luma_minus8 ||
                                             0 < picIt->GetSPS().bit_depth_chroma_minus8));
            }
        }
    }
    else
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_OutputSize = hasMoreThan8Bits ? OutputSize::_16bits : OutputSize::_8bits;

    CHECK_RC(InitOutSurfaces(width, height, numOutSurf, dev, subDev, chHandle));

    if (h264 == m_InitedTo)
    {
        m_HistogramSurf.SetColorFormat(ColorUtils::Y8);
        m_HistogramSurf.SetWidth(maxHistBins * sizeof(UINT32));
        m_HistogramSurf.SetHeight(1);
        m_HistogramSurf.SetProtect(Memory::ReadWrite);
        m_HistogramSurf.SetLayout(Surface2D::Pitch);
        m_HistogramSurf.SetDisplayable(false);
        m_HistogramSurf.SetType(LWOS32_TYPE_IMAGE);
        CHECK_RC(m_HistogramSurf.Alloc(dev));
        CHECK_RC(m_HistogramSurf.Fill(0, subDev->GetSubdeviceInst()));
        CHECK_RC(m_HistogramSurf.BindGpuChannel(chHandle));

        H264CtrlCont::iterator ctrlIt;
        for (ctrlIt = m_H264Ctrl.begin(); m_H264Ctrl.end() != ctrlIt; ++ctrlIt)
        {
            ctrlIt->SetPitchOffset(m_OutSurfsY.front(), m_OutSurfsUV.front());
        }
        BuildH264Picsetup();
        if (m_Encryption)
        {
            // arbitrary numbers, Pi for the content key and Euler's e for the init vector
            static const UINT32 contentKey[] = { 0x54442d18, 0x400921fb, 0x54442d18, 0x400921fb };
            static const UINT32 initVector[] = { 0x8b145769, 0x4005bf0a, 0x8b145769, 0x4005bf0a };

            SetContentKey(contentKey);
            SetInitVector(initVector);

            CHECK_RC(SetupContentKey(subDev, skTimeout));
            m_H264EncMemLayout.SetContentKey(GetContentKey());
            m_H264EncMemLayout.SetInitializatiolwector(GetInitVector());
        }

        Surface2D::MappingSaver oldMapping(m_InSurf);
        if (m_InSurf.IsMapped() || OK == (rc = m_InSurf.Map()))
        {
            UINT08 *surfBegin = static_cast<UINT08*>(m_InSurf.GetAddress());
            if (m_Encryption)
            {
                m_H264EncMemLayout.SetStartAddress(surfBegin);
                m_H264EncMemLayout.SetSliceAlign(GetH264EncSlicesAlignment());
                m_H264EncMemLayout.SetControlAlign(GetControlStructsAlignment());
                BuildH264Layout(&m_H264EncMemLayout);
            }
            else
            {
                m_H264MemLayout.SetStartAddress(surfBegin);
                m_H264MemLayout.SetSliceAlign(GetH264SlicesAlignment());
                m_H264MemLayout.SetControlAlign(GetControlStructsAlignment());
                BuildH264Layout(&m_H264MemLayout);
            }
        }
    }
    else if (h265 == m_InitedTo)
    {
        H265CtrlCont::iterator ctrlIt;
        for (ctrlIt = m_H265Ctrl.begin(); m_H265Ctrl.end() != ctrlIt; ++ctrlIt)
        {
            ctrlIt->SetPitch(m_OutSurfsY.front(), m_OutSurfsUV.front(), m_OutputSize);
        }
        BuildH265Picsetup();
        Surface2D::MappingSaver oldMapping(m_InSurf);
        if (m_InSurf.IsMapped() || OK == (rc = m_InSurf.Map()))
        {
            UINT08 *surfBegin = static_cast<UINT08*>(m_InSurf.GetAddress());

            m_H265MemLayout.SetStartAddress(surfBegin);
            m_H265MemLayout.SetSliceAlign(GetH265SlicesAlignment());
            m_H265MemLayout.SetControlAlign(GetControlStructsAlignment());
            BuildH265Layout(&m_H265MemLayout);
        }
    }
    else
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_LwrPic = 0;
    m_GoldOutSurfLoopCnt = 0;
    m_GoldHistSurfLoopCnt = 0;
    m_OutFramesCount = 0;

    return rc;
}

RC ClC4B0Engine::DoUpdateInDataGoldens(GpuGoldenSurfaces *goldSurfs, Goldelwalues *goldValues)
{
    MASSERT(goldSurfs);
    MASSERT(goldValues);

    RC rc;

    if (m_Encryption)
    {
        // We cannot check the consistency of the encrypted data, because
        // the input data is encrypted with the current DH key. The DH key is
        // different every time.
       return rc;
    }

    if (0 == goldSurfs->NumSurfaces())
    {
        goldSurfs->AttachSurface(&m_InSurf, "A", 0);
    }
    else
    {
        goldSurfs->ReplaceSurface(0, &m_InSurf);
    }
    CHECK_RC(goldValues->SetSurfaces(goldSurfs));
    CHECK_RC(goldValues->OverrideSuffix(m_InStreamGoldSuffix + "-indata"));
    goldValues->SetLoop(0);

    CHECK_RC(goldValues->Run());

    return rc;
}

RC ClC4B0Engine::DoUpdateOutPicsGoldens(GpuGoldenSurfaces *goldSurfs, Goldelwalues *goldValues)
{
    MASSERT(goldSurfs);
    MASSERT(goldValues);

    RC rc;

    const OutSurfIDsCont *outPics = nullptr;
    if (GetNumPics() > m_LwrPic)
    {
        size_t lwrPicIdx;
        if (h264 == m_InitedTo)
        {
            const H264::Picture &pic = *(m_H264parser->pic_begin() + m_LwrPic);
            lwrPicIdx = pic.GetPicIdx();
        }
        else if (h265 == m_InitedTo)
        {
            const H265::Picture &pic = *(m_H265parser->pic_begin() + m_LwrPic);
            lwrPicIdx = pic.GetPicIdx();
        }
        else
        {
            return RC::UNSUPPORTED_FUNCTION;
        }

        outPics = &m_OutputPics[lwrPicIdx];
    }
    else
    {
        outPics = &m_LastOutPics;
    }

    for (OutSurfIDsCont::const_iterator it = outPics->begin(); outPics->end() != it; ++it)
    {
        if (0 == goldSurfs->NumSurfaces())
        {
            goldSurfs->AttachSurface(&m_OutSurfsY[*it], "A", 0);
            goldSurfs->AttachSurface(&m_OutSurfsUV[*it], "A", 0);
        }
        else
        {
            goldSurfs->ReplaceSurface(0, &m_OutSurfsY[*it]);
            goldSurfs->ReplaceSurface(1, &m_OutSurfsUV[*it]);
        }
        CHECK_RC(goldValues->SetSurfaces(goldSurfs));
        CHECK_RC(goldValues->OverrideSuffix(m_InStreamGoldSuffix));
        goldValues->SetLoop(m_GoldOutSurfLoopCnt++);

        CHECK_RC(goldValues->Run());
    }

    return rc;
}

RC ClC4B0Engine::DoUpdateHistGoldens(GpuGoldenSurfaces* goldSurfs, Goldelwalues* goldValues)
{
    MASSERT(goldSurfs);
    MASSERT(goldValues);

    RC rc;

    if (h264 != m_InitedTo)
    {
        return rc;
    }

    if (0 == goldSurfs->NumSurfaces())
    {
        goldSurfs->AttachSurface(&m_HistogramSurf, "A", 0);
    }
    else
    {
        goldSurfs->ReplaceSurface(0, &m_HistogramSurf);
    }
    CHECK_RC(goldValues->SetSurfaces(goldSurfs));
    CHECK_RC(goldValues->OverrideSuffix(m_InStreamGoldSuffix + "-hist"));
    goldValues->SetLoop(m_GoldHistSurfLoopCnt++);

    CHECK_RC(goldValues->Run());

    return rc;
}

RC ClC4B0Engine::DoSaveFrames(const string &nameTemplate, UINT32 instNum)
{
    RC rc;

    string lumaFileName;
    string chromaFileName;
    enum {normal, format} state = normal;
    for (auto it = nameTemplate.cbegin(); nameTemplate.cend() != it; ++it)
    {
        switch (state)
        {
            case normal:
                if ('%' == *it)
                {
                    state = format;
                    continue;
                }
                else
                {
                    lumaFileName += *it;
                    chromaFileName += *it;
                }
                break;
            case format:
                switch (*it)
                {
                    case 'i':
                        lumaFileName += StrPrintf("%02u", instNum);
                        chromaFileName += StrPrintf("%02u", instNum);
                        break;
                    case 'f':
                        lumaFileName += m_InStreamFileName;
                        chromaFileName += m_InStreamFileName;
                        break;
                    case 'c':
                        lumaFileName += 'Y';
                        chromaFileName += "UV";
                        break;
                    case 'n':
                        lumaFileName += StrPrintf("%03u", m_OutFramesCount);
                        chromaFileName += StrPrintf("%03u", m_OutFramesCount);
                        break;
                    case '%':
                        lumaFileName += '%';
                        chromaFileName += '%';
                        break;
                }
                state = normal;
                break;
        }
    }

    const OutSurfIDsCont *outPics = nullptr;
    if (GetNumPics() > m_LwrPic)
    {
        size_t lwrPicIdx;
        if (h264 == m_InitedTo)
        {
            const H264::Picture &pic = *(m_H264parser->pic_begin() + m_LwrPic);
            lwrPicIdx = pic.GetPicIdx();
        }
        else if (h265 == m_InitedTo)
        {
            const H265::Picture &pic = *(m_H265parser->pic_begin() + m_LwrPic);
            lwrPicIdx = pic.GetPicIdx();
        }
        else
        {
            return RC::UNSUPPORTED_FUNCTION;
        }

        outPics = &m_OutputPics[lwrPicIdx];
    }
    else
    {
        outPics = &m_LastOutPics;
    }

    for (OutSurfIDsCont::const_iterator it = outPics->begin(); outPics->end() != it; ++it)
    {
        CHECK_RC(m_OutSurfsY[*it].WritePng(lumaFileName.c_str()));
        CHECK_RC(m_OutSurfsUV[*it].WritePng(chromaFileName.c_str()));
        ++m_OutFramesCount;
    }

    return rc;
}

RC ClC4B0Engine::DoFree()
{
    RC rc;

    m_InSurf.Free();
    OutSurfCont::iterator surfIt;
    for (surfIt = m_OutSurfsY.begin(); m_OutSurfsY.end() != surfIt; ++surfIt)
    {
        surfIt->Free();
    }
    m_OutSurfsY.clear();

    for (surfIt = m_OutSurfsUV.begin(); m_OutSurfsUV.end() != surfIt; ++surfIt)
    {
        surfIt->Free();
    }
    m_OutSurfsUV.clear();

    if (h264 == m_InitedTo)
    {
        m_HistogramSurf.Free();
    }

    m_H264MemLayout.SetStartAddress(0);
    m_H264EncMemLayout.SetStartAddress(0);

    m_H265MemLayout.SetStartAddress(0);

    m_H264parser.reset();
    m_H265parser.reset();

    m_H264Ctrl.clear();
    m_H265Ctrl.clear();

    m_Semaphore.Free();

    return rc;
}

size_t ClC4B0Engine::DoGetNumPics() const
{
    return h264 == m_InitedTo ? m_H264parser->GetNumFrames() : m_H265parser->GetNumFrames();
}

RC ClC4B0Engine::DoInitChannel()
{
    RC rc;

    CHECK_RC(m_LwCh->SetObject(0, m_LwdecHandle));

    return rc;
}

RC ClC4B0Engine::DoDecodeOneFrame(FLOAT64 timeOut)
{
    RC rc;

    if (GetNumPics() <= m_LwrPic)
    {
        return rc;
    }

    if (!m_Semaphore.IsAllocated())
    {
        Printf(Tee::PriError, "%s: semaphore is not allocated\n", MODS_FUNCTION);
        if (OK != m_SemAllocResult)
        {
            return m_SemAllocResult;
        }
        else
        {
            return RC::SOFTWARE_ERROR;
        }
    }

    if (h264 == m_InitedTo)
    {
        CHECK_RC(CreateH264PushBuffer());
    }
    else if (h265 == m_InitedTo)
    {
        CHECK_RC(CreateH265PushBuffer());
    }
    else
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_Semaphore.SetTriggerPayload(m_SemPayload++);
    CHECK_RC(SemaphoreAB(m_Semaphore.GetSurface(0)));
    CHECK_RC(SemaphoreC(m_Semaphore.GetTriggerPayload(0)));
    CHECK_RC(Execute(true, notifyOnEnd, false));

    CHECK_RC(Flush());

    CHECK_RC(m_Semaphore.Wait(timeOut));

    ++m_LwrPic;

    return rc;
}

template <class Layouter>
RC ClC4B0Engine::PrintH264InputData(const Layouter &layouter)
{
    RC rc;

    const H264::Picture &pic = *(m_H264parser->pic_begin() + m_LwrPic);
    const UINT32 numSlices = static_cast<UINT32>(pic.GetNumSlices());
    const H264Ctrl &ctrl = m_H264Ctrl[m_LwrPic];
    const UINT32 psOffs = *(layouter.pic_set_begin() + m_LwrPic);
    const UINT32 sliceOffsetsOffs = *(layouter.sliceofs_begin() + m_LwrPic);

    Printf(m_PrintPri, "Control structures and input data for picture %u\n",
           static_cast<unsigned int>(m_LwrPic));

    Surface2D::MappingSaver oldMapping(m_InSurf);
    if (m_InSurf.IsMapped() || OK == (rc = m_InSurf.Map()))
    {
        UINT08* inAddr = static_cast<UINT08*>(m_InSurf.GetAddress());

        vector<UINT08> ps(ctrl.GetPicSetupSize());
        vector<UINT08> sliceOffsets(ctrl.GetSliceOffsetsSize());

        VirtualRd(inAddr + psOffs, &ps[0], static_cast<UINT32>(ps.size()));
        VirtualRd(inAddr + sliceOffsetsOffs, &sliceOffsets[0], static_cast<UINT32>(sliceOffsets.size()));

        string psStr = ctrl.PicSetupToText(&ps[0]);
        string sliceOffsetsStr = ctrl.SliceOffsetsToText(&sliceOffsets[0], numSlices);

        Printf(m_PrintPri, "%s", psStr.c_str());
        Printf(m_PrintPri, "%s", sliceOffsetsStr.c_str());

        string slicesDataStr;
        vector<UINT08> slicesData(ctrl.GetSlicesSize(&ps[0]));
        size_t slicesDataStart = *(layouter.frames_begin() + m_LwrPic);
        VirtualRd(inAddr + slicesDataStart, &slicesData[0],
            static_cast<UINT32>(slicesData.size()));

        slicesDataStr += StrPrintf("Slices data for picture %u = \n{\n", m_LwrPic);

        size_t numLines = AlignUp<16>(slicesData.size()) / 16;
        const size_t maxLines = 100;
        bool skipLines = numLines > maxLines;
        size_t firstLineToSkip = 0;
        size_t lastLineToSkip = 0;
        if (skipLines)
        {
            firstLineToSkip = maxLines / 2;
            lastLineToSkip = numLines - firstLineToSkip;
        }

        for (size_t j = 0; numLines > j; ++j)
        {
            if (skipLines && (firstLineToSkip <= j && j < lastLineToSkip))
            {
                continue;
            }

            slicesDataStr += StrPrintf("    %08x: ", static_cast<unsigned int>(j * 16));
            for (size_t k = 0; 16 > k && slicesData.size() > j * 16 + k; ++k)
            {
                size_t offset = j * 16 + k;
                if (0 == k)
                {
                    slicesDataStr += StrPrintf("%02x", static_cast<unsigned int>(slicesData[offset]));
                }
                else
                {
                    slicesDataStr += StrPrintf(" %02x", static_cast<unsigned int>(slicesData[offset]));
                }
            }
            slicesDataStr += '\n';
        }
        slicesDataStr += "}\n";
        Printf(m_PrintPri, "%s", slicesDataStr.c_str());
    }

    return rc;
}

RC ClC4B0Engine::PrintH265InputData(const H265MemLayouter &layouter)
{
    RC rc;

    const H265::Picture &pic = *(m_H265parser->pic_begin() + m_LwrPic);
    const H265Ctrl &ctrl = m_H265Ctrl[m_LwrPic];
    const UINT32 psOffs = *(layouter.pic_set_begin() + m_LwrPic);
    const UINT32 slOffs = *(layouter.scale_mtx_begin() + m_LwrPic);
    const UINT32 tiOffs = *(layouter.tiles_info_begin() + m_LwrPic);

    Printf(m_PrintPri, "Control structures and input data for picture %u\n",
           static_cast<unsigned int>(m_LwrPic));

    Surface2D::MappingSaver oldMapping(m_InSurf);
    if (m_InSurf.IsMapped() || OK == (rc = m_InSurf.Map()))
    {
        UINT08* inAddr = static_cast<UINT08*>(m_InSurf.GetAddress());

        vector<UINT08> ps(ctrl.GetPicSetupSize());
        vector<UINT08> sl(ctrl.GetScalingMatricesSize());
        vector<UINT08> ti(ctrl.GetTileInfoSize());

        VirtualRd(inAddr + psOffs, &ps[0], static_cast<UINT32>(ps.size()));
        VirtualRd(inAddr + slOffs, &sl[0], static_cast<UINT32>(sl.size()));
        VirtualRd(inAddr + tiOffs, &ti[0], static_cast<UINT32>(ti.size()));

        string psStr = ctrl.PicSetupToText(&ps[0]);
        string slStr = ctrl.ScalingMatricesToText(&sl[0]);
        string tiStr = ctrl.TileInfoToText(&ti[0], pic.GetPPS());

        Printf(m_PrintPri, "%s", psStr.c_str());
        Printf(m_PrintPri, "%s", slStr.c_str());
        Printf(m_PrintPri, "%s", tiStr.c_str());

        string slicesDataStr;
        vector<UINT08> slicesData(ctrl.GetSlicesSize(&ps[0]));
        size_t slicesDataStart = *(layouter.frames_begin() + m_LwrPic);
        VirtualRd(inAddr + slicesDataStart, &slicesData[0],
            static_cast<UINT32>(slicesData.size()));

        slicesDataStr += StrPrintf("Slices data for picture %u = \n{\n", m_LwrPic);

        size_t numLines = AlignUp<16>(slicesData.size()) / 16;
        const size_t maxLines = 100;
        bool skipLines = numLines > maxLines;
        size_t firstLineToSkip = 0;
        size_t lastLineToSkip = 0;
        if (skipLines)
        {
            firstLineToSkip = maxLines / 2;
            lastLineToSkip = numLines - firstLineToSkip;
        }

        for (size_t j = 0; numLines > j; ++j)
        {
            if (skipLines && (firstLineToSkip <= j && j < lastLineToSkip))
            {
                continue;
            }

            slicesDataStr += StrPrintf("    %08x: ", static_cast<unsigned int>(j * 16));
            for (size_t k = 0; 16 > k && slicesData.size() > j * 16 + k; ++k)
            {
                size_t offset = j * 16 + k;
                if (0 == k)
                {
                    slicesDataStr += StrPrintf("%02x", static_cast<unsigned int>(slicesData[offset]));
                }
                else
                {
                    slicesDataStr += StrPrintf(" %02x", static_cast<unsigned int>(slicesData[offset]));
                }
            }
            slicesDataStr += '\n';
        }
        slicesDataStr += "}\n";
        Printf(m_PrintPri, "%s", slicesDataStr.c_str());
    }

    return rc;
}

RC ClC4B0Engine::DoPrintInputData()
{
    RC rc;

    if (h264 == m_InitedTo)
    {
        if (m_Encryption)
        {
            CHECK_RC(PrintH264InputData(m_H264EncMemLayout));
        }
        else
        {
            CHECK_RC(PrintH264InputData(m_H264MemLayout));
        }
    }
    else if (h265 == m_InitedTo)
    {
        CHECK_RC(PrintH265InputData(m_H265MemLayout));
    }
    else
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

RC ClC4B0Engine::DoPrintHistogram()
{
    RC rc;

    if (h264 != m_InitedTo)
    {
        return rc;
    }

    Surface2D::MappingSaver oldMapping(m_HistogramSurf);
    if (m_HistogramSurf.IsMapped() || OK == (rc = m_HistogramSurf.Map()))
    {
        vector<UINT32> histData(maxHistBins);
        Platform::VirtualRd(
            m_HistogramSurf.GetAddress(),
            &histData[0],
            static_cast<UINT32>(histData.size() * sizeof(UINT32))
        );

        size_t width = Sqrt<maxHistBins>::Result();
        size_t count = 0;
        string histStr;
        while (maxHistBins != count)
        {
            histStr += "    ";
            for (size_t i = 0; width > i && maxHistBins > count; ++i, ++count)
            {
                histStr += StrPrintf("%8d", histData[count]);
            }
            histStr += '\n';
        }
        Printf(m_PrintPri, "Picture histogram = {\n%s}\n", histStr.c_str());
    }

    return rc;
}

RC ClC4B0Engine::DoSetupContentKey(GpuSubdevice* subDev, FLOAT64 skTimeout)
{
    return Engine::DoSetupContentKey(subDev, skTimeout);
}

const UINT32* ClC4B0Engine::DoGetWrappedSessionKey() const
{
    return m_WrappedSessionKey;
}

void ClC4B0Engine::DoSetWrappedSessionKey(const UINT32 wsk[AES::DW])
{
    memcpy(m_WrappedSessionKey, wsk, sizeof(m_WrappedSessionKey));
}

const UINT32* ClC4B0Engine::DoGetWrappedContentKey() const
{
    return m_WrappedContentKey;
}

void ClC4B0Engine::DoSetWrappedContentKey(const UINT32 wck[AES::DW])
{
    memcpy(m_WrappedContentKey, wck, sizeof(m_WrappedContentKey));
}

const UINT32* ClC4B0Engine::DoGetContentKey() const
{
    return m_ContentKey;
}

void ClC4B0Engine::DoSetContentKey(const UINT32 ck[AES::DW])
{
    memcpy(m_ContentKey, ck, sizeof(m_ContentKey));
}

const UINT32* ClC4B0Engine::DoGetInitVector() const
{
    return m_InitVector;
}

void ClC4B0Engine::DoSetInitVector(const UINT32 iv[AES::DW])
{
    memcpy(m_InitVector, iv, sizeof(m_InitVector));
}

UINT08 ClC4B0Engine::DoGetKeyIncrement() const
{
    return m_KeyIncrement;
}

void ClC4B0Engine::DoSetKeyIncrement(UINT08 ki)
{
    m_KeyIncrement = ki;
}

LWDEC::Engine* ClC4B0Engine::DoClone() const
{
    return new ClC4B0Engine(*this);
}

RC ClC4B0Engine::SetApplicationID(ApplicationId id)
{
    RC rc;

    Printf(m_PrintPri, "SetApplicationID, id = %u\n", static_cast<unsigned int>(id));

    CHECK_RC(m_LwCh->Write(
        0, LWC4B0_SET_APPLICATION_ID,
        DRF_NUM(C4B0, _SET_APPLICATION_ID, _ID, static_cast<unsigned int>(id))));

    return rc;
}

RC ClC4B0Engine::SetControlParams
(
    CodecType codecType,
    bool gpTimerOn,
    bool retErr,
    bool errConcealOn,
    unsigned int errorFrmIdx,
    bool mbTimerOn
)
{
    return SetControlParams(codecType, gpTimerOn, retErr, errConcealOn, 
                            errorFrmIdx, mbTimerOn, false);
}

RC ClC4B0Engine::SetControlParams
(
    CodecType codecType,
    bool gpTimerOn,
    bool retErr,
    bool errConcealOn,
    unsigned int errorFrmIdx,
    bool mbTimerOn,
    bool intraFrameOn
)
{
    RC rc;

    Printf(m_PrintPri,
        "SetControlParams, codecType = %u, "
        "gpTimerOn = %u, "
        "retErr = %u, "
        "errConcealOn = %u, "
        "errorFrmIdx = %u, "
        "mbTimerOn = %u,"
        "intraFrameOn = %u\n",
        static_cast<unsigned int>(codecType),
        gpTimerOn ? 1 : 0,
        retErr ? 1 : 0,
        errConcealOn ? 1 : 0,
        errorFrmIdx,
        mbTimerOn ? 1 : 0,
        intraFrameOn ? 1 : 0
    );

    UINT32 controlParams;

    controlParams  = DRF_NUM(C4B0, _SET_CONTROL_PARAMS, _CODEC_TYPE, codecType);
    controlParams |= DRF_NUM(C4B0, _SET_CONTROL_PARAMS, _GPTIMER_ON, gpTimerOn);
    controlParams |= DRF_NUM(C4B0, _SET_CONTROL_PARAMS, _MBTIMER_ON, mbTimerOn);
    controlParams |= DRF_NUM(C4B0, _SET_CONTROL_PARAMS, _RET_ERROR, retErr);
    controlParams |= DRF_NUM(C4B0, _SET_CONTROL_PARAMS, _ERR_CONCEAL_ON, errConcealOn);
    controlParams |= DRF_NUM(C4B0, _SET_CONTROL_PARAMS, _ERROR_FRM_IDX, errorFrmIdx);

    // For class 0xC6B0 and 0xC7B0
    if (m_UseIntraFrame)
    {
        controlParams |= DRF_NUM(C6B0, _SET_CONTROL_PARAMS, _ALL_INTRA_FRAME, intraFrameOn);
    }

    CHECK_RC(m_LwCh->Write(0, LWC4B0_SET_CONTROL_PARAMS, controlParams));

    return rc;
}

RC ClC4B0Engine::SetInBufBaseOffset(const Surface2D& surf, UINT32 offs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + offs) >> 8);
    Printf(m_PrintPri, "SetInBufBaseOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_LwCh->WriteWithSurface(0, LWC4B0_SET_IN_BUF_BASE_OFFSET, surf, offs, 8));

    return rc;
}

RC ClC4B0Engine::SetPictureIndex(UINT32 idx)
{
    RC rc;

    Printf(m_PrintPri, "SetPictureIndex, idx = 0x%08x\n", idx);

    CHECK_RC(m_LwCh->Write(
        0, LWC4B0_SET_PICTURE_INDEX,
        DRF_NUM(C4B0, _SET_PICTURE_INDEX, _INDEX, idx)));

    return rc;
}

RC ClC4B0Engine::SetDrvPicSetupOffset(const Surface2D& surf, UINT32 ofs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + ofs) >> 8);
    Printf(m_PrintPri, "SetDrvPicSetupOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_LwCh->WriteWithSurface(0, LWC4B0_SET_DRV_PIC_SETUP_OFFSET, surf, ofs, 8));

    return rc;
}

RC ClC4B0Engine::SetHistoryOffset(const Surface2D& surf, UINT32 ofs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + ofs) >> 8);
    Printf(m_PrintPri, "SetHistoryOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_LwCh->WriteWithSurface(0, LWC4B0_SET_HISTORY_OFFSET, surf, ofs, 8));

    return rc;
}

RC ClC4B0Engine::SetColocDataOffset(const Surface2D& surf, UINT32 ofs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + ofs) >> 8);
    Printf(m_PrintPri, "SetColocDataOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_LwCh->WriteWithSurface(0, LWC4B0_SET_COLOC_DATA_OFFSET, surf, ofs, 8));

    return rc;
}

RC ClC4B0Engine::HEVCSetScalingListOffset(const Surface2D& surf, UINT32 ofs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + ofs) >> 8);
    Printf(m_PrintPri, "HEVCSetScalingListOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_LwCh->WriteWithSurface(0, LWC4B0_HEVC_SET_SCALING_LIST_OFFSET, surf, ofs, 8));

    return rc;
}

RC ClC4B0Engine::HEVCSetFilterBufferOffset(const Surface2D& surf, UINT32 ofs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + ofs) >> 8);
    Printf(m_PrintPri, "HEVCSetFilterBufferOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_LwCh->WriteWithSurface(0, LWC4B0_HEVC_SET_FILTER_BUFFER_OFFSET, surf, ofs, 8));

    return rc;
}

RC ClC4B0Engine::HEVCSetTileSizesOffset(const Surface2D& surf, UINT32 ofs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + ofs) >> 8);
    Printf(m_PrintPri, "HEVCSetTileSizesOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_LwCh->WriteWithSurface(0, LWC4B0_HEVC_SET_TILE_SIZES_OFFSET, surf, ofs, 8));

    return rc;
}

RC ClC4B0Engine::SetHistogramOffset(const Surface2D& surf)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>(surf.GetCtxDmaOffsetGpu() >> 8);
    Printf(m_PrintPri, "SetHistogramOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_LwCh->WriteWithSurface(0, LWC4B0_SET_HISTOGRAM_OFFSET, surf, 0, 8));

    return rc;
}

RC ClC4B0Engine::H264SetMBHistBufOffset(const Surface2D& surf, UINT32 ofs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + ofs) >> 8);
    Printf(m_PrintPri, "H264SetMBHistBufOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_LwCh->WriteWithSurface(0, LWC4B0_H264_SET_MBHIST_BUF_OFFSET, surf, ofs, 8));

    return rc;
}

RC ClC4B0Engine::SetPictureLumaOffset(size_t picIdx, const Surface2D& surf)
{
    static const UINT32 method[] =
    {
        LWC4B0_SET_PICTURE_LUMA_OFFSET0
      , LWC4B0_SET_PICTURE_LUMA_OFFSET1
      , LWC4B0_SET_PICTURE_LUMA_OFFSET2
      , LWC4B0_SET_PICTURE_LUMA_OFFSET3
      , LWC4B0_SET_PICTURE_LUMA_OFFSET4
      , LWC4B0_SET_PICTURE_LUMA_OFFSET5
      , LWC4B0_SET_PICTURE_LUMA_OFFSET6
      , LWC4B0_SET_PICTURE_LUMA_OFFSET7
      , LWC4B0_SET_PICTURE_LUMA_OFFSET8
      , LWC4B0_SET_PICTURE_LUMA_OFFSET9
      , LWC4B0_SET_PICTURE_LUMA_OFFSET10
      , LWC4B0_SET_PICTURE_LUMA_OFFSET11
      , LWC4B0_SET_PICTURE_LUMA_OFFSET12
      , LWC4B0_SET_PICTURE_LUMA_OFFSET13
      , LWC4B0_SET_PICTURE_LUMA_OFFSET14
      , LWC4B0_SET_PICTURE_LUMA_OFFSET15
      , LWC4B0_SET_PICTURE_LUMA_OFFSET16
    };

    if (picIdx >= NUMELEMS(method))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    const UINT32 data = static_cast<UINT32>(surf.GetCtxDmaOffsetGpu() >> 8);
    Printf(m_PrintPri,
        "SetPictureLumaOffset, picIdx = %u, offset = 0x%08x\n",
        static_cast<unsigned int>(picIdx),
        data
    );

    return m_LwCh->WriteWithSurface(0, method[picIdx], surf, 0, 8);
}

RC ClC4B0Engine::SetPictureChromaOffset(size_t picIdx, const Surface2D& surf)
{
    static const UINT32 method[] =
    {
        LWC4B0_SET_PICTURE_CHROMA_OFFSET0
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET1
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET2
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET3
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET4
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET5
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET6
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET7
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET8
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET9
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET10
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET11
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET12
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET13
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET14
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET15
      , LWC4B0_SET_PICTURE_CHROMA_OFFSET16
    };

    if (picIdx >= NUMELEMS(method))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    const UINT32 data = static_cast<UINT32>(surf.GetCtxDmaOffsetGpu() >> 8);
    Printf(m_PrintPri,
        "SetPictureChromaOffset, picIdx = %u, offset = 0x%08x\n",
        static_cast<unsigned int>(picIdx),
        data
    );

    return m_LwCh->WriteWithSurface(0, method[picIdx], surf, 0, 8);
}

RC ClC4B0Engine::SetSliceOffsetsBufOffset(const Surface2D& surf, UINT32 ofs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + ofs) >> 8);
    Printf(m_PrintPri, "SetSliceOffsetsBufOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_LwCh->WriteWithSurface(0, LWC4B0_SET_SLICE_OFFSETS_BUF_OFFSET, surf, ofs, 8));

    return rc;
}

RC ClC4B0Engine::SemaphoreAB(const Surface2D& surf)
{
    RC rc;

    UINT64 offs = surf.GetCtxDmaOffsetGpu();
    const UINT32 a = static_cast<UINT32>(offs >> 32);
    const UINT32 b = static_cast<UINT32>(offs);
    Printf(m_PrintPri, "SemaphoreAB, A = %#.8x, B = %#.8x\n", a, b);

    CHECK_RC(m_LwCh->WriteWithSurface(0, LWC4B0_SEMAPHORE_A, surf, 0, 32));
    CHECK_RC(m_LwCh->WriteWithSurface(0, LWC4B0_SEMAPHORE_B, surf, 0, 0));

    return rc;
}

RC ClC4B0Engine::SemaphoreC(UINT32 payload)
{
    RC rc;

    Printf(m_PrintPri, "SemaphoreC, payload = %#.8x\n", payload);

   CHECK_RC(m_LwCh->Write(0, LWC4B0_SEMAPHORE_C, DRF_NUM(C4B0, _SEMAPHORE_C, _PAYLOAD, payload)));

   return rc;
}

RC ClC4B0Engine::Execute(bool notify, NotifyOn notifyOn, bool awaken)
{
    RC rc;

    Printf(m_PrintPri,
        "Execute, notify = %u, "
        "notifyOn = %u, "
        "awaken = %u\n",
        notify ? 1 : 0,
        static_cast<unsigned int>(notifyOn),
        awaken ? 1 : 0
    );

    CHECK_RC(m_LwCh->Write(
        0, LWC4B0_EXELWTE,
        (notify ?
            DRF_DEF(C4B0, _EXELWTE, _NOTIFY, _ENABLE) :
            DRF_DEF(C4B0, _EXELWTE, _NOTIFY, _DISABLE))
        |
        (awaken ?
            DRF_DEF(C4B0, _EXELWTE, _AWAKEN, _ENABLE) :
            DRF_DEF(C4B0, _EXELWTE, _AWAKEN, _DISABLE))
        |
        (notifyOnBeg == notifyOn ?
            DRF_DEF(C4B0, _EXELWTE, _NOTIFY_ON, _BEGIN) :
            DRF_DEF(C4B0, _EXELWTE, _NOTIFY_ON, _END))
    ));

    return rc;
}

RC ClC4B0Engine::Flush()
{
    RC rc;

    CHECK_RC(m_LwCh->Flush());

    return rc;
}

void ClC4B0Engine::UpdateControlStructsFromPics()
{
    if (h264 == m_InitedTo)
    {
        m_H264Ctrl.resize(GetNumPics());
        H264::Parser::pic_const_iterator it;
        size_t picNum;
        for (picNum = 0, it = m_H264parser->pic_begin(); m_H264parser->pic_end() != it; ++it, ++picNum)
        {
            m_H264Ctrl[picNum].UpdateFromPicture(*it);
        }
    }
    else if (h265 == m_InitedTo)
    {
        m_H265Ctrl.resize(GetNumPics());
        H265::Parser::pic_const_iterator it;
        size_t picNum;
        for (picNum = 0, it = m_H265parser->pic_begin(); m_H265parser->pic_end() != it; ++it, ++picNum)
        {
            m_H265Ctrl[picNum].UpdateFromPicture(*it);
        }
    }
    else
    {
        MASSERT(false);
    }
}

RC ClC4B0Engine::InitOutSurfaces
(
    UINT32 width,
    UINT32 height,
    UINT32 numPics,
    GpuDevice *dev,
    GpuSubdevice *subDev,
    LwRm::Handle chHandle
)
{
    RC rc;

    m_OutSurfsY.resize(numPics);
    m_OutSurfsUV.resize(numPics);
    OutSurfCont::iterator outSurfIt;

    for (outSurfIt = m_OutSurfsY.begin(); m_OutSurfsY.end() != outSurfIt; ++outSurfIt)
    {
        outSurfIt->SetWidth(width);
        outSurfIt->SetHeight(height);
        outSurfIt->SetColorFormat(m_OutputSize == OutputSize::_16bits ?
                                  ColorUtils::VOID16 : ColorUtils::Y8);
        outSurfIt->SetLocation(Memory::Optimal);
        outSurfIt->SetProtect(Memory::ReadWrite);
        outSurfIt->SetLayout(Surface2D::BlockLinear);
        outSurfIt->SetLogBlockHeight(1);
        outSurfIt->SetType(LWOS32_TYPE_IMAGE);
        CHECK_RC(outSurfIt->Alloc(dev));
        CHECK_RC(outSurfIt->BindGpuChannel(chHandle));

        CHECK_RC(SurfaceUtils::FillSurface(&(*outSurfIt), 0, outSurfIt->GetBitsPerPixel(),
                                            subDev));
    }

    for (outSurfIt = m_OutSurfsUV.begin(); m_OutSurfsUV.end() != outSurfIt; ++outSurfIt)
    {
        outSurfIt->SetWidth(width);
        outSurfIt->SetHeight(height / 2);
        outSurfIt->SetColorFormat(m_OutputSize == OutputSize::_16bits ?
                                  ColorUtils::VOID16 : ColorUtils::Y8);
        outSurfIt->SetLocation(Memory::Optimal);
        outSurfIt->SetProtect(Memory::ReadWrite);
        outSurfIt->SetLayout(Surface2D::BlockLinear);
        outSurfIt->SetLogBlockHeight(1);
        outSurfIt->SetType(LWOS32_TYPE_IMAGE);
        CHECK_RC(outSurfIt->Alloc(dev));
        CHECK_RC(outSurfIt->BindGpuChannel(chHandle));

        CHECK_RC(SurfaceUtils::FillSurface(&(*outSurfIt), 0, outSurfIt->GetBitsPerPixel(),
                                            subDev));
    }

    return rc;
}

struct CompPOC : binary_function<size_t, size_t, bool>
{
public:
    CompPOC(const H264::Parser &si)
        : m_si(si)
    {}

    bool operator()(size_t idx1, size_t idx2)
    {
        const H264::Picture *pic1 = m_si.GetPicture(idx1);
        const H264::Picture *pic2 = m_si.GetPicture(idx2);

        return pic1->PicOrderCnt() < pic2->PicOrderCnt();
    }

private:
    const H264::Parser &m_si;
};

void ClC4B0Engine::BuildH264Picsetup()
{
    using Utility::StrPrintf;

    // A map from indices that H.264 parser assigns to pictures to LWDEC
    // output surfaces.
    typedef map<size_t, size_t> MapToLWDECIdx;
    MapToLWDECIdx               mapToLWDECIdx;

    // A map from indices that H.264 parser assigns to pictures to indices in
    // mspdec_h264_picture_setup_s::dpb
    typedef map<size_t, size_t> MapToLWDECDPBIdx;
    MapToLWDECDPBIdx            mapToLWDECDPBIdx;

    // A pool of output surfaces indices that are not lwrrently in use
    typedef set<size_t> SurfaceIndicesPool;
    SurfaceIndicesPool  surfIndicesPool;

    // A pool of indices in mspdec_h264_picture_setup_s::dpb that are not
    // lwrrently in use
    typedef set<size_t> LWDECDPBIndicesPool;
    LWDECDPBIndicesPool lwdecDPBIndicesPool;

    size_t outSurfsSize = m_OutSurfsY.size();
    for (size_t i = 0; outSurfsSize > i; ++i)
    {
        surfIndicesPool.insert(i);
    }

    size_t maxDPBSize = m_H264Ctrl.front().GetMaxDPBSize();
    for (size_t i = 0; maxDPBSize > i; ++i)
    {
        lwdecDPBIndicesPool.insert(i);
    }

    H264::Parser::pic_iterator picIt;
    H264CtrlCont::iterator ctrlIt;
    for (ctrlIt = m_H264Ctrl.begin(), picIt = m_H264parser->pic_begin();
         m_H264parser->pic_end() != picIt; ++picIt, ++ctrlIt)
    {
        size_t lwrPicIdx = picIt->GetPicIdx();
        const H264::DPB &dpb = picIt->GetDPB();

        // memorize what surfaces can be output
        OutSurfIDsCont &outPics = m_OutputPics[lwrPicIdx];
        outPics.clear();
        for (const size_t *p = picIt->out_begin(); picIt->out_end() != p; ++p)
        {
            MapToLWDECIdx::const_iterator idxIt;
            idxIt = mapToLWDECIdx.find(*p);
            MASSERT(mapToLWDECIdx.end() != idxIt);
            outPics.push_back(idxIt->second);
        }

        // return picture indices removed from DPB to the pool
        for (const size_t *p = picIt->rm_begin(); picIt->rm_end() != p; ++p)
        {
            MapToLWDECIdx::const_iterator idxIt;
            idxIt = mapToLWDECIdx.find(*p);
            MASSERT(mapToLWDECIdx.end() != idxIt);
            surfIndicesPool.insert(idxIt->second);
            mapToLWDECIdx.erase(*p);

            MapToLWDECDPBIdx::const_iterator dpbIdxIt;
            dpbIdxIt = mapToLWDECDPBIdx.find(*p);
            if (mapToLWDECDPBIdx.end() != dpbIdxIt) // it's ok, if we can't find
            {                                       // it, not all pics are there
                lwdecDPBIndicesPool.insert(dpbIdxIt->second);
                mapToLWDECDPBIdx.erase(*p);
            }
        }

        // borrow an index from the pool for the new picture
        size_t lwdecIdx = *surfIndicesPool.begin();
        surfIndicesPool.erase(lwdecIdx);

        // remember what picture it corresponds to
        mapToLWDECIdx[lwrPicIdx] = lwdecIdx;

        // apprise LWDEC about our choice of the index
        ctrlIt->SetLwrrPicIdx(static_cast<unsigned int>(lwdecIdx));

        // fill in decode picture buffer
        H264::DPB::const_iterator dpbIt;
        for (dpbIt = dpb.begin(); dpb.end() != dpbIt; ++dpbIt)
        {
            // LWDEC doesn't need to know about not reference pictures
            if (!dpbIt->IsUsedForReference())
            {
                continue;
            }

            // find LWDEC index for the reference picture
            MapToLWDECIdx::const_iterator idxIt;
            idxIt = mapToLWDECIdx.find(dpbIt->GetPicIdx());
            MASSERT(mapToLWDECIdx.end() != idxIt);

            // entries in mspdec_h264_picture_setup_s::dpb shouldn't shift
            // between pictures, let's recall where this entry was before or
            // allocate a space for it, if it's new
            size_t lwdecDpbIdx;
            MapToLWDECDPBIdx::const_iterator lwdecDpbIdxIt;
            lwdecDpbIdxIt = mapToLWDECDPBIdx.find(dpbIt->GetPicIdx());
            if (mapToLWDECDPBIdx.end() == lwdecDpbIdxIt)
            {
                // borrow an index from the pool for the new picture
                lwdecDpbIdx = *lwdecDPBIndicesPool.begin();
                lwdecDPBIndicesPool.erase(lwdecDpbIdx);
                mapToLWDECDPBIdx[dpbIt->GetPicIdx()] = lwdecDpbIdx;
            }
            else
            {
                lwdecDpbIdx = lwdecDpbIdxIt->second;
            }

            ctrlIt->UpdateDPB(*picIt, lwdecDpbIdx, dpbIt,
                static_cast<UINT32>(idxIt->second), MemLayout::LW12);
        }

        ctrlIt->SetOutputMemoryLayout(MemLayout::LW12);
        ctrlIt->SetHistBufferSize(m_H264HistBufSize / 256);
        ctrlIt->EnableHistogram(0, 0, m_H264parser->GetWidth() - 1, m_H264parser->GetHeight() - 1);
        ctrlIt->SetSlicesSize(static_cast<unsigned int>(picIt->GetTotalNalusSize()));
        ctrlIt->SetSlicesCount(static_cast<unsigned int>(picIt->GetNumSlices()));
        ctrlIt->SetMBHistBufSize(static_cast<unsigned int>(m_H264MbHistBufSize));
    }
    const H264::DPB &dpb = m_H264parser->pic_back().GetDPB();
    m_LastOutPics.clear();
    // add pictures remained in the last DPB
    for (H264::DPB::const_iterator it = dpb.begin(); dpb.end() != it; ++it)
    {
        if (!it->IsOutput())
        {
            m_LastOutPics.push_back(it->GetPicIdx());
        }
    }
    // add the last picture itself
    m_LastOutPics.push_back(m_H264parser->pic_back().GetPicIdx());

    // sort the remaining output pictures according to the display order
    sort(m_LastOutPics.begin(), m_LastOutPics.end(), CompPOC(*m_H264parser));
    // map H.264 parser indices to output surface indices
    for (OutSurfIDsCont::iterator it = m_LastOutPics.begin(); m_LastOutPics.end() != it; ++it)
    {
        *it = mapToLWDECIdx[*it];
    }

    // print out the result
    size_t list0MaxSize = 0;
    size_t list1MaxSize = 0;
    size_t outPicsMax = 0;
    for (picIt = m_H264parser->pic_begin(); m_H264parser->pic_end() != picIt; ++picIt)
    {
        size_t list0Size = picIt->slices_front().l0_size();
        size_t list1Size = picIt->slices_front().l1_size();

        list0MaxSize = max(list0MaxSize, list0Size);
        list1MaxSize = max(list1MaxSize, list1Size);

        const OutSurfIDsCont &outPics = m_OutputPics[picIt->GetPicIdx()];
        outPicsMax = max(outPicsMax, outPics.size());
    }
    string picListStr = "       # POC type|";
    picListStr += StrPrintf("%*s", static_cast<int>(list0MaxSize * 4), "l0");
    picListStr += '|';
    picListStr += StrPrintf("%*s", static_cast<int>(list1MaxSize * 4), "l1");
    picListStr += "| idx col|";
    if (0 < outPicsMax)
    {
        picListStr += StrPrintf
        (
            "%*s",
            -static_cast<int>(maxDPBSize * 6),
            " DPB indices and colocation indices"
        );
        picListStr += "| Output";
    }
    else
    {
        picListStr += " DPB indices and colocation indices";
    }
    picListStr += '\n';
    size_t count = 0;
    for (picIt = m_H264parser->pic_begin(); m_H264parser->pic_end() != picIt; ++picIt, ++count)
    {
        const H264::DPB &dpb = picIt->GetDPB();
        H264::Slice &frontSlice = picIt->slices_front();
        const H264Ctrl &ctrl = m_H264Ctrl[count];

        // print picture number, POC, slice type
        picListStr += StrPrintf(
            "    %4d%4d%5s%s",
            static_cast<int>(count),
            static_cast<int>(picIt->GetPicOrderCnt() / 2),
            frontSlice.GetSliceTypeStr(),
            "|");
        H264::Slice::ref_iterator l0it;
        H264::Slice::ref_iterator l1it;
        // print reference list 0
        size_t refCount = 0;
        if (!frontSlice.l0_empty())
        {
            for (l0it = frontSlice.l0_begin();
                 frontSlice.l0_end() != l0it;
                 ++l0it, ++refCount)
            {
                int poc = picIt->GetPicOrderCnt(dpb[*l0it].GetPicIdx()) / 2;
                picListStr += StrPrintf("%4d", poc);
            }
        }
        for (size_t i = refCount; list0MaxSize > i; ++i)
        {
            picListStr += StrPrintf("%4s", "*");
        }
        picListStr += '|';
        // print reference list 1
        refCount = 0;
        if (!frontSlice.l1_empty())
        {
            for (l1it = frontSlice.l1_begin();
                 frontSlice.l1_end() != l1it;
                 ++l1it, ++refCount)
            {
                int poc = picIt->GetPicOrderCnt(dpb[*l1it].GetPicIdx()) / 2;
                picListStr += StrPrintf("%4d", poc);
            }
        }
        for (size_t i = refCount; list1MaxSize > i; ++i)
        {
            picListStr += StrPrintf("%4s", "*");
        }
        picListStr += '|';
        // current picture index and colocation index
        picListStr += StrPrintf("%4u%4u|", ctrl.GetLwrrPicIdx(), ctrl.GetLwrrColIdx());
        // picture and colocation indices from DPB
        for (size_t i = 0; ctrl.GetMaxDPBSize() > i; ++i)
        {
            if (ctrl.IsDPBEntryUsed(i))
            {
                picListStr += StrPrintf("%3u%3u", ctrl.GetDPBPicIdx(i), ctrl.GetDPBColIdx(i));
            }
            else
            {
                picListStr += StrPrintf("%3s%3s", "*", "*");
            }
        }
        if (0 < outPicsMax)
        {
            picListStr += '|';
            // output pictures
            size_t outPicsCount = 0;
            const OutSurfIDsCont &outPics = m_OutputPics[picIt->GetPicIdx()];
            if (!outPics.empty())
            {
                OutSurfIDsCont::const_iterator it;
                for (it = outPics.begin(); outPics.end() != it; ++it, ++outPicsCount)
                {
                    picListStr += StrPrintf("%3d", static_cast<int>(*it));
                }
            }
            for (size_t i = outPicsCount; outPicsMax > i; ++i)
            {
                picListStr += StrPrintf("%3s", "*");
            }
        }

        picListStr += '\n';
    }
    Printf(
        m_PrintPri,
        "Reference lists, DPB and output order of to be decoded pictures:\n%s",
        picListStr.c_str());
    picListStr = "";
    if (!m_LastOutPics.empty())
    {
        OutSurfIDsCont::const_iterator it;
        for (it = m_LastOutPics.begin(); m_LastOutPics.end() != it; ++it)
        {
            picListStr += StrPrintf("%3d", static_cast<int>(*it));
        }
    }
    else
    {
        picListStr = "none";
    }
    Printf(m_PrintPri, "Remaining output pictures: %s\n", picListStr.c_str());
}

void ClC4B0Engine::BuildH265Picsetup()
{
    H265DPBManager dpb(m_H265parser->GetMaxDecPicBuffering());

    H265::Parser::pic_iterator picIt;
    H265CtrlCont::iterator ctrlIt;
    for (ctrlIt = m_H265Ctrl.begin(), picIt = m_H265parser->pic_begin();
         m_H265parser->pic_end() != picIt; ++picIt, ++ctrlIt)
    {
        H265::PicIdx lwrPicIdx = picIt->GetPicIdx();

        // This call will check if there is space in the DPB for a new (the one
        // that is about to be decoded) picture. If the DPB is fully oclwpied,
        // it will purge pictures that are not used for reference anymore in
        // the presentation order. Then it will return the index of the first
        // available slot in the DPB.
        int lwdecIdx = dpb.GetSlotForNewPic(*picIt, back_inserter(m_OutputPics[lwrPicIdx]));
        MASSERT(0 <= lwdecIdx);

        // apprise LWDEC about our choice of the index
        ctrlIt->SetLwrrPicIdx(lwdecIdx);

        UINT08 l0[H265::maxNumRefPics] = {};
        UINT08 l1[H265::maxNumRefPics] = {};
        INT32 pocs[H265::maxMaxDpbSize] = {};

        for (size_t i = 0; picIt->NumPocTotalLwrr > i; ++i)
        {
            l0[i] = dpb.GetLwdecIdx(picIt->GetRefPicListTemp0()[i]);
            pocs[l0[i]] = m_H265parser->PicOrderCnt(picIt->GetRefPicListTemp0()[i]);
            l1[i] = dpb.GetLwdecIdx(picIt->GetRefPicListTemp1()[i]);
        }
        if (0 < picIt->NumPocTotalLwrr)
        {
            for (size_t i = picIt->NumPocTotalLwrr; H265::maxNumRefPics > i; ++i)
            {
                l0[i] = l0[i - picIt->NumPocTotalLwrr];
                l1[i] = l1[i - picIt->NumPocTotalLwrr];
            }
        }

        ctrlIt->UpdateDPB(picIt->PicOrderCntVal, l0, l1, pocs, dpb.GetLtMask(),
                          picIt->NumPocTotalLwrr);

        ctrlIt->SetColMvBufSize(m_H265ColMvBufSize);
        ctrlIt->SetSaoBufferOffset(m_SaoBufferOffset);
        ctrlIt->SetBsdCtrlOffset(m_BsdBufferOffset);
        ctrlIt->SetFilterAboveBufferOffset(m_FilterAboveBufferOffset);
        ctrlIt->SetSaoAboveBufferOffset(m_SaoAboveBufferOffset);
        ctrlIt->SetSlicesSize(picIt->GetSlicesEBSPSize());
    }
    dpb.Clear(back_inserter(m_LastOutPics));

    // The following code will print out a table like this:
    //        # POC type|  l0|  l1| idx| DPB        | Output
    //        0   0  IDR|   *|   *|   0|  *  *  *  *|  *
    //        1   4    P|   0|   *|   1|  0  *  *  *|  *
    //        2   2    b|   0|   4|   2|  0  4  *  *|  *
    //        3   1    b|   0|   2|   3|  0  4  2  *|  *
    //        4   3    b|   2|   4|   0|  0  4  2  1|  0
    //        5   8    P|   4|   *|   3|  3  4  2  1|  3
    //        6   6    b|   4|   8|   2|  3  4  2  8|  2
    //        7   5    b|   4|   6|   0|  3  4  6  8|  0
    //        8   7    b|   6|   8|   1|  5  4  6  8|  1
    //        9  12    P|   8|   *|   0|  5  7  6  8|  0
    //       10  10    b|   8|  12|   2| 12  7  6  8|  2
    //       11   9    b|   8|  10|   1| 12  7 10  8|  1
    //       12  11    b|  10|  12|   3| 12  9 10  8|  3
    //       13  16    P|  12|   *|   1| 12  9 10 11|  1
    //       14  14    b|  12|  16|   2| 12 16 10 11|  2
    //       15  13    b|  12|  14|   3| 12 16 14 11|  3
    //       16  15    b|  14|  16|   0| 12 16 14 13|  0
    //       17  20    P|  16|   *|   3| 15 16 14 13|  3
    //       18  18    b|  16|  20|   2| 15 16 14 20|  2
    //       19  17    b|  16|  18|   0| 15 16 18 20|  0
    //       20  19    b|  18|  20|   1| 17 16 18 20|  1
    //       21  24    P|  20|   *|   0| 17 19 18 20|  0
    //       22  22    b|  20|  24|   2| 24 19 18 20|  2
    //       23  21    b|  20|  22|   1| 24 19 22 20|  1
    //       24  23    b|  22|  24|   3| 24 21 22 20|  3
    //       25  28    P|  24|   *|   1| 24 21 22 23|  1
    //       26  26    b|  24|  28|   2| 24 28 22 23|  2
    //       27  25    b|  24|  26|   3| 24 28 26 23|  3
    //       28  27    b|  26|  28|   0| 24 28 26 25|  0
    //       29  29    b|  28|  28|   3| 27 28 26 25|  3
    // Remaining output pictures:   2  0  1  3
    //
    // #, POC and 'type' describe a picture to be decoded. # is its number in
    // the decoding order, POC is Picture Order Count, i.e. the picture number
    // in the displaying order. 'type' is either IDR (Instantaneous Decoding
    // Refresh, i.e. it doesn't depend on anything previously decoded), P -- the
    // picture depends on pictures located before it in the displaying order,
    // and B -- the picture depends both on pictures located before and after it
    // in the displaying order. Calling it a picture type is not technically
    // correct, since a picture can have an arbitrary number of slices, each
    // having its own type. However it's an accepted in the company slang, since
    // most of the time a picture has just one type of slices. Lowercase p and b
    // mean that these pictures are not used as a reference.
    //
    // 'l0' and 'l1' are the lists of POCs of pictures used as a reference to
    // decode this picture.
    //
    // 'idx' is where the picture is going to be placed into DPB (Decoded
    // Picture Buffer) after decoding.
    //
    // 'DPB' shows POCs of the pictures in the DPB before decoding this picture.
    //
    // 'Output' shows the indices of DPB slots that can be output before
    // decoding this picture.

    size_t list0MaxSize = 0;
    size_t list1MaxSize = 0;
    size_t outPicsMax = 0;
    for (picIt = m_H265parser->pic_begin(); m_H265parser->pic_end() != picIt; ++picIt)
    {
        size_t list0Size = picIt->slices_front().GetL0Size();
        size_t list1Size = picIt->slices_front().GetL1Size();

        list0MaxSize = max(list0MaxSize, list0Size);
        list1MaxSize = max(list1MaxSize, list1Size);

        const OutSurfIDsCont &outPics = m_OutputPics[picIt->GetPicIdx()];
        outPicsMax = max(outPicsMax, outPics.size());
    }
    string picListStr = "       # POC type|";
    picListStr += StrPrintf("%*s", static_cast<int>(list0MaxSize * 4), "l0");
    picListStr += '|';
    picListStr += StrPrintf("%*s", static_cast<int>(list1MaxSize * 4), "l1");
    picListStr += "| idx|";
    if (0 < outPicsMax)
    {
        picListStr += StrPrintf
        (
            "%*s",
            -static_cast<int>(m_H265parser->GetMaxDecPicBuffering() * 3),
            " DPB"
        );
        picListStr += "| Output";
    }
    else
    {
        picListStr += " DPB";
    }

    size_t count = 0;
    picListStr += '\n';
    for (picIt = m_H265parser->pic_begin(); m_H265parser->pic_end() != picIt; ++picIt, ++count)
    {
        auto &frontSlice = picIt->slices_front();
        const H265Ctrl &ctrl = m_H265Ctrl[count];

        picListStr += StrPrintf(
            "    %4d%4d%5s%s",
            static_cast<int>(count),
            static_cast<int>(picIt->PicOrderCntVal),
            frontSlice.GetSliceTypeStr(),
            "|");

        H265::Slice::ref_iterator l0it;
        H265::Slice::ref_iterator l1it;
        // print reference list 0
        size_t refCount = 0;
        if (!frontSlice.l0_empty())
        {
            for (l0it = frontSlice.l0_begin(); frontSlice.l0_end() != l0it; ++l0it, ++refCount)
            {
                picListStr += StrPrintf("%4d", m_H265parser->GetPicture(*l0it)->PicOrderCntVal);
            }
        }
        for (size_t i = refCount; list0MaxSize > i; ++i)
        {
            picListStr += StrPrintf("%4s", "*");
        }
        picListStr += '|';
        // print reference list 1
        refCount = 0;
        if (!frontSlice.l1_empty())
        {
            for (l1it = frontSlice.l1_begin();
                 frontSlice.l1_end() != l1it;
                 ++l1it, ++refCount)
            {
                picListStr += StrPrintf("%4d", m_H265parser->GetPicture(*l1it)->PicOrderCntVal);
            }
        }
        for (size_t i = refCount; list1MaxSize > i; ++i)
        {
            picListStr += StrPrintf("%4s", "*");
        }
        picListStr += '|';

        // current picture index in DPB
        picListStr += StrPrintf("%4u|", ctrl.GetLwrrPicIdx());

        // In H.265 there is no explicit DPB entity that would point us to what
        // pictures are stored in the DPB at each particular moment. All we know
        // is that `lwdec_hevc_pic_s::lwrr_pic_idx` is a DPB index where the
        // current picture is going to be saved. So if we check all previous
        // pictures `lwrr_pic_idx`, we will know what is in the DPB at the
        // moment.
        std::vector<int> lwrDpb(m_H265parser->GetMaxDecPicBuffering(), -1);
        for (size_t i = 0; count > i; ++i)
        {
            const H265Ctrl &prevCtrl = m_H265Ctrl[i];
            lwrDpb[prevCtrl.GetLwrrPicIdx()] = m_H265parser->GetPicture(i)->PicOrderCntVal;
        }
        for (size_t i = 0; m_H265parser->GetMaxDecPicBuffering() > i; ++i)
        {
            if (0 > lwrDpb[i])
            {
                picListStr += StrPrintf("%3s", "*");
            }
            else
            {
                picListStr += StrPrintf("%3u", lwrDpb[i]);
            }
        }
        if (0 < outPicsMax)
        {
            picListStr += '|';
            // output pictures
            size_t outPicsCount = 0;
            const OutSurfIDsCont &outPics = m_OutputPics[picIt->GetPicIdx()];
            if (!outPics.empty())
            {
                OutSurfIDsCont::const_iterator it;
                for (it = outPics.begin(); outPics.end() != it; ++it, ++outPicsCount)
                {
                    picListStr += StrPrintf("%3d", static_cast<int>(*it));
                }
            }
            for (size_t i = outPicsCount; outPicsMax > i; ++i)
            {
                picListStr += StrPrintf("%3s", "*");
            }
        }
        picListStr += '\n';
    }
    Printf(
        m_PrintPri,
        "Reference lists, DPB and output order of to be decoded pictures.\n%s",
        picListStr.c_str());
    picListStr = "";
    if (!m_LastOutPics.empty())
    {
        OutSurfIDsCont::const_iterator it;
        for (it = m_LastOutPics.begin(); m_LastOutPics.end() != it; ++it)
        {
            picListStr += StrPrintf("%3d", static_cast<int>(*it));
        }
    }
    else
    {
        picListStr = "none";
    }
    Printf(m_PrintPri, "Remaining output pictures: %s\n", picListStr.c_str());
}

RC ClC4B0Engine::CreateH264PushBuffer()
{
    RC rc;

    UINT32 frameOffset;
    UINT32 sliceOffsetsOffset;
    UINT32 ps;
    UINT32 colocBuf;
    UINT32 histBuf;
    UINT32 mbHistBuf;

    if (m_Encryption)
    {
        frameOffset = *(m_H264EncMemLayout.frames_begin() + m_LwrPic);
        sliceOffsetsOffset = *(m_H264EncMemLayout.sliceofs_begin() + m_LwrPic);
        ps = *(m_H264EncMemLayout.pic_set_begin() + m_LwrPic);
        colocBuf = m_H264EncMemLayout.GetColocBufOffset();
        histBuf = m_H264EncMemLayout.GetHistBufOffset();
        mbHistBuf = m_H264EncMemLayout.GetMBHistBufOffset();
    }
    else
    {
        frameOffset = *(m_H264MemLayout.frames_begin() + m_LwrPic);
        sliceOffsetsOffset = *(m_H264MemLayout.sliceofs_begin() + m_LwrPic);
        ps = *(m_H264MemLayout.pic_set_begin() + m_LwrPic);
        colocBuf = m_H264MemLayout.GetColocBufOffset();
        histBuf = m_H264MemLayout.GetHistBufOffset();
        mbHistBuf = m_H264MemLayout.GetMBHistBufOffset();
    }

    CHECK_RC(SetApplicationID(APPLICATION_ID_H264));
    CHECK_RC(SetControlParams(CODEC_TYPE_H264, false, false, false, 0, false));
    CHECK_RC(SetPictureIndex(m_LwrPic));
    CHECK_RC(SetInBufBaseOffset(m_InSurf, frameOffset));
    CHECK_RC(SetDrvPicSetupOffset(m_InSurf, ps));
    CHECK_RC(SetColocDataOffset(m_InSurf, colocBuf));
    CHECK_RC(SetHistoryOffset(m_InSurf, histBuf));
    CHECK_RC(H264SetMBHistBufOffset(m_InSurf, mbHistBuf));
    CHECK_RC(SetSliceOffsetsBufOffset(m_InSurf, sliceOffsetsOffset));
    OutSurfCont::const_iterator osIt;
    size_t surfCount = 0;
    for (osIt = m_OutSurfsY.begin(); m_OutSurfsY.end() != osIt; ++osIt)
    {
        CHECK_RC(SetPictureLumaOffset(surfCount++, *osIt));
    }
    surfCount = 0;
    for (osIt = m_OutSurfsUV.begin(); m_OutSurfsUV.end() != osIt; ++osIt)
    {
        CHECK_RC(SetPictureChromaOffset(surfCount++, *osIt));
    }
    CHECK_RC(SetHistogramOffset(m_HistogramSurf));

    return rc;
}

RC ClC4B0Engine::CreateH265PushBuffer()
{
    RC rc;

    UINT32 frameOffset = *(m_H265MemLayout.frames_begin() + m_LwrPic);
    UINT32 psOffset = *(m_H265MemLayout.pic_set_begin() + m_LwrPic);
    UINT32 slOffset = *(m_H265MemLayout.scale_mtx_begin() + m_LwrPic);
    UINT32 colocBufOffset = m_H265MemLayout.GetColocBufOffset();
    UINT32 filterBufOffset = m_H265MemLayout.GetFilterBufOffset();
    UINT32 tiOffset = *(m_H265MemLayout.tiles_info_begin() + m_LwrPic);

    CHECK_RC(SetApplicationID(APPLICATION_ID_HEVC));
    CHECK_RC(SetControlParams(CODEC_TYPE_HEVC, false, false, false, 0, false));
    CHECK_RC(SetPictureIndex(m_LwrPic));
    CHECK_RC(SetInBufBaseOffset(m_InSurf, frameOffset));
    CHECK_RC(SetDrvPicSetupOffset(m_InSurf, psOffset));
    CHECK_RC(HEVCSetScalingListOffset(m_InSurf, slOffset));
    CHECK_RC(SetColocDataOffset(m_InSurf, colocBufOffset));
    CHECK_RC(HEVCSetFilterBufferOffset(m_InSurf, filterBufOffset));
    CHECK_RC(HEVCSetTileSizesOffset(m_InSurf, tiOffset));

    OutSurfCont::const_iterator osIt;
    size_t surfCount = 0;
    for (osIt = m_OutSurfsY.begin(); m_OutSurfsY.end() != osIt; ++osIt)
    {
        CHECK_RC(SetPictureLumaOffset(surfCount++, *osIt));
    }
    surfCount = 0;
    for (osIt = m_OutSurfsUV.begin(); m_OutSurfsUV.end() != osIt; ++osIt)
    {
        CHECK_RC(SetPictureChromaOffset(surfCount++, *osIt));
    }

    return rc;
}

ClC4B0Engine::H264Ctrl::H264Ctrl()
{
    static const unsigned char eos[16] =
    {
        0x00, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00, 0x00
    };

    memset(&m_ps, 0, sizeof(m_ps));

    m_ps.explicitEOSPresentFlag = 1;
    memcpy(m_ps.eos, eos, sizeof(eos));
}

void ClC4B0Engine::H264Ctrl::UpdateFromPicture(const H264::Picture &pic)
{
    const H264::SeqParameterSet &sps = pic.GetSPS();
    const H264::PicParameterSet &pps = pic.GetPPS();

    // assignments are sorted according to the order of fields in lwdec_h264_pic_s
    m_ps.log2_max_pic_order_cnt_lsb_minus4      = sps.log2_max_pic_order_cnt_lsb_minus4;
    m_ps.delta_pic_order_always_zero_flag       = sps.delta_pic_order_always_zero_flag ? 1 : 0;
    m_ps.frame_mbs_only_flag                    = sps.frame_mbs_only_flag ? 1 : 0;
    m_ps.PicWidthInMbs                          = sps.PicWidthInMbs();
    m_ps.FrameHeightInMbs                       = sps.FrameHeightInMbs();
    m_ps.tileFormat                             = 1;
    m_ps.entropy_coding_mode_flag               = pps.entropy_coding_mode_flag ? 1 : 0;
    m_ps.pic_order_present_flag                 = pps.bottom_field_pic_order_in_frame_present_flag ? 1 : 0;
    m_ps.num_ref_idx_l0_active_minus1           = pps.num_ref_idx_l0_default_active_minus1;
    m_ps.num_ref_idx_l1_active_minus1           = pps.num_ref_idx_l1_default_active_minus1;
    m_ps.deblocking_filter_control_present_flag = pps.deblocking_filter_control_present_flag ? 1 : 0;
    m_ps.redundant_pic_cnt_present_flag         = pps.redundant_pic_cnt_present_flag ? 1 : 0;
    m_ps.transform_8x8_mode_flag                = pps.transform_8x8_mode_flag ? 1 : 0;
    m_ps.MbaffFrameFlag                         = pic.MbaffFrameFlag();
    m_ps.direct_8x8_inference_flag              = sps.direct_8x8_inference_flag ? 1 : 0;
    m_ps.weighted_pred_flag                     = pps.weighted_pred_flag ? 1 : 0;
    m_ps.constrained_intra_pred_flag            = pps.constrained_intra_pred_flag ? 1 : 0;
    m_ps.ref_pic_flag                           = pic.IsReference();
    m_ps.field_pic_flag                         = pic.FieldPicFlag();
    m_ps.bottom_field_flag                      = pic.BottomFieldFlag();
    m_ps.second_field                           = 0; // not supported
    m_ps.log2_max_frame_num_minus4              = sps.log2_max_frame_num_minus4;
    m_ps.chroma_format_idc                      = sps.chroma_format_idc;
    m_ps.pic_order_cnt_type                     = sps.pic_order_cnt_type;
    m_ps.pic_init_qp_minus26                    = pps.pic_init_qp_minus26;
    m_ps.chroma_qp_index_offset                 = pps.chroma_qp_index_offset;
    m_ps.second_chroma_qp_index_offset          = pps.second_chroma_qp_index_offset;
    m_ps.weighted_bipred_idc                    = pps.weighted_bipred_idc;
    m_ps.frame_num                              = pic.FrameNum();
    m_ps.LwrrFieldOrderCnt[0]                   = pic.TopFieldOrderCnt();
    m_ps.LwrrFieldOrderCnt[1]                   = pic.BottomFieldOrderCnt();
    m_ps.qpprime_y_zero_transform_bypass_flag   = sps.qpprime_y_zero_transform_bypass_flag ? 1 : 0;

    m_sliceOffsets.resize(pic.GetNumSlices() + 1);
    size_t i;
    m_sliceOffsets[0] = 0;
    // the size of the slice_offset array is intentionally made one element
    // bigger than the number of slices, since some software estimates
    // the size of all slices by callwlating
    // m_sliceOffsets[number of slices] - m_sliceOffsets[0]
    H264::Picture::slice_const_iterator slIt;
    for (slIt = pic.slices_begin(), i = 1; pic.slices_end() != slIt; ++slIt, ++i)
    {
        m_sliceOffsets[i] = static_cast<UINT32>(m_sliceOffsets[i - 1] + slIt->GetEbspSize());
    }

    H264::CreateWeightMatrices(sps, pps, &m_ps.WeightScale, &m_ps.WeightScale8x8);
}

void ClC4B0Engine::H264Ctrl::EnableEncryption(unsigned int keyIncrement, const UINT32 sessionKey[4],
                                              const UINT32 contentKey[4], const UINT32 initVector[4])
{
    m_ps.encryption_params.enable_encryption = 1;
    m_ps.encryption_params.key_increment = keyIncrement;
    m_ps.encryption_params.encryption_mode = 0;
    memcpy(&m_ps.encryption_params.wrapped_session_key[0], &sessionKey[0], 16);
    memcpy(&m_ps.encryption_params.wrapped_content_key[0], &contentKey[0], 16);
    memcpy(&m_ps.encryption_params.initialization_vector[0], &initVector[0], 16);
}

void const * ClC4B0Engine::H264Ctrl::GetPicSetup() const
{
    return &m_ps;
}

size_t ClC4B0Engine::H264Ctrl::GetPicSetupSize() const
{
    return sizeof(m_ps);
}

size_t ClC4B0Engine::H264Ctrl::GetMaxDPBSize() const
{
    return NUMELEMS(m_ps.dpb);
}

unsigned int ClC4B0Engine::H264Ctrl::GetLwrrPicIdx() const
{
    return m_ps.LwrrPicIdx;
}

unsigned int ClC4B0Engine::H264Ctrl::GetLwrrColIdx() const
{
    return m_ps.LwrrColIdx;
}

unsigned int ClC4B0Engine::H264Ctrl::GetDPBPicIdx(size_t idx) const
{
    return m_ps.dpb[idx].index;
}

unsigned int ClC4B0Engine::H264Ctrl::GetDPBColIdx(size_t idx) const
{
    return m_ps.dpb[idx].col_idx;
}

bool ClC4B0Engine::H264Ctrl::IsDPBEntryUsed(size_t idx) const
{
    return 0 != m_ps.dpb[idx].state;
}

void ClC4B0Engine::H264Ctrl::SetLwrrPicIdx(unsigned int val)
{
    m_ps.LwrrPicIdx = val;
    m_ps.LwrrColIdx = val;
}

void ClC4B0Engine::H264Ctrl::UpdateDPB
(
    const H264::Picture &pic,
    size_t whereIdx,
    H264::DPB::const_iterator fromWhat,
    unsigned int lwdecRefIdx,
    MemLayout memLayout
)
{
    lwdec_dpb_entry_s &lwdecDpb = m_ps.dpb[whereIdx];

    lwdecDpb.index = lwdecRefIdx;
    lwdecDpb.col_idx = lwdecRefIdx;
    lwdecDpb.state = 3;
    lwdecDpb.is_long_term = fromWhat->IsLongTerm() ? 1 : 0;
    lwdecDpb.is_field = 0;
    lwdecDpb.top_field_marking = fromWhat->IsLongTerm() ? 2 : 1;
    lwdecDpb.bottom_field_marking = fromWhat->IsLongTerm() ? 2 : 1;
    lwdecDpb.output_memory_layout = MemLayout::LW12 == memLayout ? 0 : 1;

    const H264::Picture *refPic = pic.GetDpbPic(*fromWhat);

    lwdecDpb.FieldOrderCnt[0] = refPic->TopFieldOrderCnt();
    lwdecDpb.FieldOrderCnt[1] = refPic->BottomFieldOrderCnt();
    lwdecDpb.FrameIdx =
        fromWhat->IsLongTerm() ? fromWhat->GetLongTermFrameIdx() :
                                 refPic->FrameNum();
}

void ClC4B0Engine::H264Ctrl::SetOutputMemoryLayout(MemLayout memLayout)
{
    m_ps.output_memory_layout = MemLayout::LW12 == memLayout ? 0 : 1;
}

void ClC4B0Engine::H264Ctrl::SetHistBufferSize(unsigned int size)
{
    m_ps.HistBufferSize = size;
}

void ClC4B0Engine::H264Ctrl::EnableHistogram(short startX, short startY, short endX, short endY)
{
    m_ps.displayPara.enableHistogram = 1;
    m_ps.displayPara.HistogramStartX = startX;
    m_ps.displayPara.HistogramStartY = startY;
    m_ps.displayPara.HistogramEndX = endX;
    m_ps.displayPara.HistogramEndY = endY;
}

void ClC4B0Engine::H264Ctrl::SetPitchOffset(const Surface2D &surfY, const Surface2D &surfUV)
{
    m_ps.pitch_luma = surfY.GetPitch();
    m_ps.pitch_chroma = surfUV.GetPitch();
    m_ps.luma_bot_offset = 0;
    m_ps.chroma_bot_offset = 0;
    m_ps.luma_frame_offset = m_ps.luma_top_offset = 0;
    m_ps.chroma_frame_offset = m_ps.chroma_top_offset = 0;
}

void ClC4B0Engine::H264Ctrl::SetSlicesSize(unsigned int slicesSize)
{
    m_ps.stream_len = slicesSize;
}

unsigned int ClC4B0Engine::H264Ctrl::GetSlicesSize(void const *buf) const
{
    const lwdec_h264_pic_s *ps = reinterpret_cast<const lwdec_h264_pic_s *>(buf);

    return ps->stream_len;
}

void ClC4B0Engine::H264Ctrl::SetSlicesCount(unsigned int slicesCount)
{
    m_ps.slice_count = slicesCount;
}

void ClC4B0Engine::H264Ctrl::SetMBHistBufSize(unsigned int mbHistBufSize)
{
    m_ps.mbhist_buffer_size = mbHistBufSize;
}

void const * ClC4B0Engine::H264Ctrl::GetSliceOffsets() const
{
    return &m_sliceOffsets[0];
}

size_t ClC4B0Engine::H264Ctrl::GetSliceOffsetsSize() const
{
    return sizeof(m_sliceOffsets[0]) * m_sliceOffsets.size();
}

string ClC4B0Engine::H264Ctrl::PicSetupToText(void const *buf) const
{
    const lwdec_h264_pic_s *ps = reinterpret_cast<const lwdec_h264_pic_s *>(buf);

    string dpbStr;
    for (size_t i = 0; NUMELEMS(ps->dpb) > i; ++i)
    {
        dpbStr += StrPrintf(
            "        [%2d] = { "
            "index = %3d, "
            "col_idx = %3d, "
            "state = %2d, "
            "is_long_term = %2d, "
            "is_field = %2d, "
            "top_field_marking = %2d, "
            "bottom_field_marking = %2d, "
            "output_memory_layout = %2d, "
            "FieldOrderCnt[0] = %3d, "
            "FieldOrderCnt[1] = %3d, "
            "FrameIdx = %3d }\n",
            static_cast<int>(i),
            ps->dpb[i].index,
            ps->dpb[i].col_idx,
            ps->dpb[i].state,
            ps->dpb[i].is_long_term,
            ps->dpb[i].is_field,
            ps->dpb[i].top_field_marking,
            ps->dpb[i].bottom_field_marking,
            ps->dpb[i].output_memory_layout,
            ps->dpb[i].FieldOrderCnt[0],
            ps->dpb[i].FieldOrderCnt[1],
            ps->dpb[i].FrameIdx
        );
    }

    string weightScaleStr;
    for (size_t i = 0; 4 > i; ++i)
    {
        weightScaleStr += "        ";
        for (size_t j = 0; 6 > j; ++j)
        {
            if (0 != j)
            {
                weightScaleStr += "    ";
            }

            for (size_t k = 0; 4 > k; ++k)
            {
                weightScaleStr += StrPrintf("%4d", ps->WeightScale[j][i][k]);
            }
        }
        weightScaleStr += "\n";
    }

    string weightScale8x8Str;
    for (size_t i = 0; 8 > i; ++i)
    {
        weightScale8x8Str += "        ";
        for (size_t j = 0; 2 > j; ++j)
        {
            if (0 != j)
            {
                weightScale8x8Str += "    ";
            }

            for (size_t k = 0; 8 > k; ++k)
            {
                weightScale8x8Str += StrPrintf("%4d", ps->WeightScale8x8[j][i][k]);
            }
        }
        weightScale8x8Str += "\n";
    }

    string ret = StrPrintf(
        "lwdec_h264_pic_s =\n"
        "{\n"
        "    encryption_params =\n"
        "    {\n"
        "        wrapped_session_key[0] = 0x%08X\n"
        "        wrapped_session_key[1] = 0x%08X\n"
        "        wrapped_session_key[2] = 0x%08X\n"
        "        wrapped_session_key[3] = 0x%08X\n"
        "        wrapped_content_key[0] = 0x%08X\n"
        "        wrapped_content_key[1] = 0x%08X\n"
        "        wrapped_content_key[2] = 0x%08X\n"
        "        wrapped_content_key[3] = 0x%08X\n"
        "        initialization_vector[0] = 0x%08X\n"
        "        initialization_vector[1] = 0x%08X\n"
        "        initialization_vector[2] = 0x%08X\n"
        "        initialization_vector[3] = 0x%08X\n"
        "        enable_encryption = %u\n"
        "        key_increment = %u\n"
        "        encryption_mode = %u\n"
        "        key_slot_index = %u\n"
        "    }\n"
        "    eos[16] = { %02X %02X %02X %02X %02X %02X %02X %02X"
                       " %02X %02X %02X %02X %02X %02X %02X %02X }\n"
        "    explicitEOSPresentFlag = %u\n"
        "    stream_len = %u\n"
        "    slice_count = %u\n"
        "    mbhist_buffer_size = %u\n"
        "    gptimer_timeout_value = %u\n"
        "    log2_max_pic_order_cnt_lsb_minus4 = %d\n"
        "    delta_pic_order_always_zero_flag = %d\n"
        "    frame_mbs_only_flag = %d\n"
        "    PicWidthInMbs = %d\n"
        "    FrameHeightInMbs = %d\n"
        "    tileFormat = %d\n"
        "    entropy_coding_mode_flag = %d\n"
        "    pic_order_present_flag = %d\n"
        "    num_ref_idx_l0_active_minus1 = %d\n"
        "    num_ref_idx_l1_active_minus1 = %d\n"
        "    deblocking_filter_control_present_flag = %d\n"
        "    redundant_pic_cnt_present_flag = %d\n"
        "    transform_8x8_mode_flag = %d\n"
        "    pitch_luma = %u\n"
        "    pitch_chroma = %u\n"
        "    luma_top_offset = %u\n"
        "    luma_bot_offset = %u\n"
        "    luma_frame_offset = %u\n"
        "    chroma_top_offset = %u\n"
        "    chroma_bot_offset = %u\n"
        "    chroma_frame_offset = %u\n"
        "    HistBufferSize = %u\n"
        "    MbaffFrameFlag = %u\n"
        "    direct_8x8_inference_flag = %u\n"
        "    weighted_pred_flag = %u\n"
        "    constrained_intra_pred_flag = %u\n"
        "    ref_pic_flag = %u\n"
        "    field_pic_flag = %u\n"
        "    bottom_field_flag = %u\n"
        "    second_field = %u\n"
        "    log2_max_frame_num_minus4 = %u\n"
        "    chroma_format_idc = %u\n"
        "    pic_order_cnt_type = %u\n"
        "    pic_init_qp_minus26 = %d\n"
        "    chroma_qp_index_offset = %d\n"
        "    second_chroma_qp_index_offset = %d\n"
        "    weighted_bipred_idc = %u\n"
        "    LwrrPicIdx = %u\n"
        "    LwrrColIdx = %u\n"
        "    frame_num = %u\n"
        "    frame_surfaces = %u\n"
        "    output_memory_layout = %u\n"
        "    LwrrFieldOrderCnt[0] (top) = %d\n"
        "    LwrrFieldOrderCnt[1] (bottom) = %d\n",
        ps->encryption_params.wrapped_session_key[0],
        ps->encryption_params.wrapped_session_key[1],
        ps->encryption_params.wrapped_session_key[2],
        ps->encryption_params.wrapped_session_key[3],
        ps->encryption_params.wrapped_content_key[0],
        ps->encryption_params.wrapped_content_key[1],
        ps->encryption_params.wrapped_content_key[2],
        ps->encryption_params.wrapped_content_key[3],
        ps->encryption_params.initialization_vector[0],
        ps->encryption_params.initialization_vector[1],
        ps->encryption_params.initialization_vector[2],
        ps->encryption_params.initialization_vector[3],
        ps->encryption_params.enable_encryption,
        ps->encryption_params.key_increment,
        ps->encryption_params.encryption_mode,
        ps->encryption_params.key_slot_index,
        ps->eos[0],  ps->eos[1],  ps->eos[2],  ps->eos[3],
        ps->eos[4],  ps->eos[5],  ps->eos[6],  ps->eos[7],
        ps->eos[8],  ps->eos[9],  ps->eos[10], ps->eos[11],
        ps->eos[12], ps->eos[13], ps->eos[14], ps->eos[15],
        ps->explicitEOSPresentFlag,
        ps->stream_len,
        ps->slice_count,
        ps->mbhist_buffer_size,
        ps->gptimer_timeout_value,
        ps->log2_max_pic_order_cnt_lsb_minus4,
        ps->delta_pic_order_always_zero_flag,
        ps->frame_mbs_only_flag,
        ps->PicWidthInMbs,
        ps->FrameHeightInMbs,
        ps->tileFormat,
        ps->entropy_coding_mode_flag,
        ps->pic_order_present_flag,
        ps->num_ref_idx_l0_active_minus1,
        ps->num_ref_idx_l1_active_minus1,
        ps->deblocking_filter_control_present_flag,
        ps->redundant_pic_cnt_present_flag,
        ps->transform_8x8_mode_flag,
        ps->pitch_luma,
        ps->pitch_chroma,
        ps->luma_top_offset,
        ps->luma_bot_offset,
        ps->luma_frame_offset,
        ps->chroma_top_offset,
        ps->chroma_bot_offset,
        ps->chroma_frame_offset,
        ps->HistBufferSize,
        ps->MbaffFrameFlag,
        ps->direct_8x8_inference_flag,
        ps->weighted_pred_flag,
        ps->constrained_intra_pred_flag,
        ps->ref_pic_flag,
        ps->field_pic_flag,
        ps->bottom_field_flag,
        ps->second_field,
        ps->log2_max_frame_num_minus4,
        ps->chroma_format_idc,
        ps->pic_order_cnt_type,
        ps->pic_init_qp_minus26,
        ps->chroma_qp_index_offset,
        ps->second_chroma_qp_index_offset,
        ps->weighted_bipred_idc,
        ps->LwrrPicIdx,
        ps->LwrrColIdx,
        ps->frame_num,
        ps->frame_surfaces,
        ps->output_memory_layout,
        ps->LwrrFieldOrderCnt[0],
        ps->LwrrFieldOrderCnt[1]
    );

    ret +=
        "    dpb =\n"
        "    {\n" +
        dpbStr +
        "    }\n";

    ret +=
        "    WeightScale =\n"
        "    {\n" +
        weightScaleStr +
        "    }\n";

    ret +=
        "    WeightScale8x8 =\n"
        "    {\n" +
        weightScale8x8Str +
        "    }\n";

    ret += StrPrintf(
        "    num_inter_view_refs_lX[0] = %u\n"
        "    num_inter_view_refs_lX[1] = %u\n"
        "    inter_view_refidx_lX[0] = { %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d }\n"
        "    inter_view_refidx_lX[1] = { %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d }\n"
        "    lossless_ipred8x8_filter_enable = %d\n"
        "    qpprime_y_zero_transform_bypass_flag = %d\n"
        "    displayPara =\n"
        "    {\n"
        "        enableTFOutput = %d\n"
        "        VC1MapYFlag = %d\n"
        "        MapYValue = %d\n"
        "        VC1MapUVFlag = %d\n"
        "        MapUVValue = %d\n"
        "        OutStride = %d\n"
        "        TilingFormat = %d\n"
        "        OutputStructure = %d\n"
        "        OutputTop[0] = %d\n"
        "        OutputTop[1] = %d\n"
        "        OutputBottom[0] = %d\n"
        "        OutputBottom[1] = %d\n"
        "        enableHistogram = %d\n"
        "        HistogramStartX = %d\n"
        "        HistogramStartY = %d\n"
        "        HistogramEndX = %d\n"
        "        HistogramEndY = %d\n"
        "    }\n"
        "}\n",
        static_cast<unsigned int>(ps->num_inter_view_refs_lX[0]),
        static_cast<unsigned int>(ps->num_inter_view_refs_lX[1]),
        static_cast<int>(ps->inter_view_refidx_lX[0][0]),  static_cast<int>(ps->inter_view_refidx_lX[0][1]),
        static_cast<int>(ps->inter_view_refidx_lX[0][2]),  static_cast<int>(ps->inter_view_refidx_lX[0][3]),
        static_cast<int>(ps->inter_view_refidx_lX[0][4]),  static_cast<int>(ps->inter_view_refidx_lX[0][5]),
        static_cast<int>(ps->inter_view_refidx_lX[0][6]),  static_cast<int>(ps->inter_view_refidx_lX[0][7]),
        static_cast<int>(ps->inter_view_refidx_lX[0][8]),  static_cast<int>(ps->inter_view_refidx_lX[0][9]),
        static_cast<int>(ps->inter_view_refidx_lX[0][10]), static_cast<int>(ps->inter_view_refidx_lX[0][11]),
        static_cast<int>(ps->inter_view_refidx_lX[0][12]), static_cast<int>(ps->inter_view_refidx_lX[0][13]),
        static_cast<int>(ps->inter_view_refidx_lX[0][14]), static_cast<int>(ps->inter_view_refidx_lX[0][15]),
        static_cast<int>(ps->inter_view_refidx_lX[1][0]),  static_cast<int>(ps->inter_view_refidx_lX[1][1]),
        static_cast<int>(ps->inter_view_refidx_lX[1][2]),  static_cast<int>(ps->inter_view_refidx_lX[1][3]),
        static_cast<int>(ps->inter_view_refidx_lX[1][4]),  static_cast<int>(ps->inter_view_refidx_lX[1][5]),
        static_cast<int>(ps->inter_view_refidx_lX[1][6]),  static_cast<int>(ps->inter_view_refidx_lX[1][7]),
        static_cast<int>(ps->inter_view_refidx_lX[1][8]),  static_cast<int>(ps->inter_view_refidx_lX[1][9]),
        static_cast<int>(ps->inter_view_refidx_lX[1][10]), static_cast<int>(ps->inter_view_refidx_lX[1][11]),
        static_cast<int>(ps->inter_view_refidx_lX[1][12]), static_cast<int>(ps->inter_view_refidx_lX[1][13]),
        static_cast<int>(ps->inter_view_refidx_lX[1][14]), static_cast<int>(ps->inter_view_refidx_lX[1][15]),
        ps->lossless_ipred8x8_filter_enable,
        ps->qpprime_y_zero_transform_bypass_flag,
        ps->displayPara.enableTFOutput,
        ps->displayPara.VC1MapYFlag,
        ps->displayPara.MapYValue,
        ps->displayPara.VC1MapUVFlag,
        ps->displayPara.MapUVValue,
        ps->displayPara.OutStride,
        ps->displayPara.TilingFormat,
        ps->displayPara.OutputStructure,
        ps->displayPara.OutputTop[0],
        ps->displayPara.OutputTop[1],
        ps->displayPara.OutputBottom[0],
        ps->displayPara.OutputBottom[1],
        ps->displayPara.enableHistogram,
        ps->displayPara.HistogramStartX,
        ps->displayPara.HistogramStartY,
        ps->displayPara.HistogramEndX,
        ps->displayPara.HistogramEndY
    );

    return ret;
}

std::string ClC4B0Engine::H264Ctrl::SliceOffsetsToText(void const *buf, size_t numSlices) const
{
    using Utility::StrPrintf;

    const UINT32 *sliceOffsets = reinterpret_cast<const UINT32 *>(buf);
    string sliceOffsetsStr = "Slice offstes = { ";
    for (size_t i = 0; numSlices >= i; ++i)
    {
        if (0 != i)
        {
            sliceOffsetsStr += StrPrintf(", ");
        }
        sliceOffsetsStr += StrPrintf("%d", sliceOffsets[i]);
    }

    sliceOffsetsStr += " }\n";

    return sliceOffsetsStr;
}

ClC4B0Engine::H265Ctrl::H265Ctrl()
{
    memset(&m_ps, 0, sizeof(m_ps));

    // Kepler block linear
    m_ps.tileformat = 1;
    // stream has start codes
    m_ps.sw_start_code_e = 1;
    // 8 bits
    m_ps.disp_output_mode = 0;
    // disable dithering
    m_ps.pattern_id = 2;

    // The tile size info buffer is taken from CModel
    // //hw/gpu_ip/unit/lwdec/2.0_gm206/cmod/gip_cmod/software/source/hevc/hevc_asic.c
    // Comment from hevc_asi.c says: "max number of tiles times width and height
    // (2 bytes each), rounding up to next 16 bytes boundary + one extra 16 byte
    // chunk (HW guys wanted to have this)".
    // size = (MAX_TILE_COLS * MAX_TILE_ROWS * 2 * sizeof(u16) + 15 + 16) & ~0xF;
    // This gives us size of 0x6f0 in bytes or 888 in 16-bit words. Then
    // hevc_asic.c adds 256 bytes for pixel colwersion and aligns up to the next
    // 256 bytes boundary. Which gives us 0x800 in bytes or 1024 in 16-bit
    // words.
    UINT32 sizeInBytes = (maxTileCols * maxTileRows * 2 * TileInfoValSize + 15 + 16) & ~0xF;
    sizeInBytes = (sizeInBytes + 255 + 256) & ~0xFF;

    m_ti.resize(sizeInBytes / TileInfoValSize);
}

void const* ClC4B0Engine::H265Ctrl::GetPicSetup() const
{
    return &m_ps;
}

size_t ClC4B0Engine::H265Ctrl::GetPicSetupSize() const
{
    return sizeof(m_ps);
}

void const* ClC4B0Engine::H265Ctrl::GetScalingMatrices() const
{
    return &m_sm;
}

size_t ClC4B0Engine::H265Ctrl::GetScalingMatricesSize() const
{
    return sizeof(m_sm);
}

void const* ClC4B0Engine::H265Ctrl::GetTileInfo() const
{
    return &m_ti[0];
}

size_t ClC4B0Engine::H265Ctrl::GetTileInfoSize() const
{
    return m_ti.size() * TileInfoValSize;
}

void ClC4B0Engine::H265Ctrl::UpdateFromPicture(const H265::Picture &pic)
{
    const H265::SeqParameterSet &sps = pic.GetSPS();
    const H265::PicParameterSet &pps = pic.GetPPS();

    lwdec_hevc_main10_444_ext_s &thisExt = m_ps.v1.hevc_main10_444_ext;
    const H265::SpsRangeExtension &spsExt = sps.sps_range_extension;
    const H265::PpsRangeExtension &ppsExt = pps.pps_range_extension;

    // SPS
    m_ps.pic_width_in_luma_samples           = sps.pic_width_in_luma_samples;
    m_ps.pic_height_in_luma_samples          = sps.pic_height_in_luma_samples;
    m_ps.chroma_format_idc                   = sps.chroma_format_idc;
    m_ps.separate_colour_plane_flag          = sps.separate_colour_plane_flag ? 1 : 0;
    m_ps.bit_depth_luma                      = sps.bit_depth_luma_minus8 + 8;
    m_ps.bit_depth_chroma                    = sps.bit_depth_chroma_minus8 + 8;
    if (8 < m_ps.bit_depth_luma || 8 < m_ps.bit_depth_chroma)
    {
        // Rec.709 10 bit
        m_ps.disp_output_mode = 1;
    }
    else
    {
        // Rec.709 8 bit
        m_ps.disp_output_mode = 0;
    }
    m_ps.log2_max_pic_order_cnt_lsb_minus4   = sps.log2_max_pic_order_cnt_lsb_minus4;
    m_ps.log2_min_luma_coding_block_size     = sps.log2_min_luma_coding_block_size_minus3 + 3;
    m_ps.log2_max_luma_coding_block_size     = sps.log2_min_luma_coding_block_size_minus3 + 3 +
                                               sps.log2_diff_max_min_luma_coding_block_size;
    m_ps.log2_min_transform_block_size       = sps.log2_min_transform_block_size_minus2 + 2;
    m_ps.log2_max_transform_block_size       = sps.log2_min_transform_block_size_minus2 + 2 +
                                               sps.log2_diff_max_min_transform_block_size;
    m_ps.max_transform_hierarchy_depth_inter = sps.max_transform_hierarchy_depth_inter;
    m_ps.max_transform_hierarchy_depth_intra = sps.max_transform_hierarchy_depth_intra;
    m_ps.scalingListEnable                   = sps.scaling_list_enabled_flag ? 1 : 0;
    m_ps.amp_enable_flag                     = sps.amp_enabled_flag ? 1 : 0;
    m_ps.sample_adaptive_offset_enabled_flag = sps.sample_adaptive_offset_enabled_flag ? 1 :0;
    m_ps.pcm_enabled_flag                    = sps.pcm_enabled_flag ? 1 :0;
    if (sps.pcm_enabled_flag)
    {
        m_ps.pcm_sample_bit_depth_luma           = sps.pcm_sample_bit_depth_luma_minus1 + 1;
        m_ps.pcm_sample_bit_depth_chroma         = sps.pcm_sample_bit_depth_chroma_minus1 + 1;
        m_ps.log2_min_pcm_luma_coding_block_size = sps.log2_min_pcm_luma_coding_block_size_minus3 + 3;
        m_ps.log2_max_pcm_luma_coding_block_size = sps.log2_min_pcm_luma_coding_block_size_minus3 + 3  +
                                                   sps.log2_diff_max_min_pcm_luma_coding_block_size;
        m_ps.pcm_loop_filter_disabled_flag       = sps.pcm_loop_filter_disabled_flag ? 1 : 0;
    }
    m_ps.num_short_term_ref_pic_sets         = sps.GetNumShortTermRefPicSets();
    m_ps.num_long_term_ref_pics_sps          = sps.num_long_term_ref_pics_sps;
    m_ps.sps_temporal_mvp_enabled_flag       = sps.sps_temporal_mvp_enabled_flag ? 1 : 0;
    m_ps.strong_intra_smoothing_enabled_flag = sps.strong_intra_smoothing_enabled_flag ? 1 : 0;

    thisExt.transformSkipRotationEnableFlag = spsExt.transform_skip_context_enabled_flag ? 1 : 0;
    thisExt.transformSkipContextEnableFlag  = spsExt.transform_skip_context_enabled_flag ? 1 : 0;
    thisExt.intraBlockCopyEnableFlag        = 0; // according to c2b0_drv.h
    thisExt.implicitRdpcmEnableFlag         = spsExt.implicit_rdpcm_enabled_flag ? 1 : 0;
    thisExt.explicitRdpcmEnableFlag         = spsExt.explicit_rdpcm_enabled_flag ? 1 : 0;
    thisExt.extendedPrecisionProcessingFlag = spsExt.extended_precision_processing_flag ? 1 : 0;
    thisExt.intraSmoothingDisabledFlag      = spsExt.intra_smoothing_disabled_flag ? 1 : 0;
    thisExt.highPrecisionOffsetsEnableFlag  = spsExt.high_precision_offsets_enabled_flag ? 1 : 0;
    thisExt.fastRiceAdaptationEnableFlag    = spsExt.persistent_rice_adaptation_enabled_flag ? 1 : 0;
    thisExt.cabacBypassAlignmentEnableFlag  = spsExt.cabac_bypass_alignment_enabled_flag ? 1 : 0;

    // PPS
    m_ps.dependent_slice_segments_enabled_flag    = pps.dependent_slice_segments_enabled_flag ? 1 : 0;
    m_ps.output_flag_present_flag                 = pps.output_flag_present_flag ? 1: 0;
    m_ps.num_extra_slice_header_bits              = pps.num_extra_slice_header_bits;
    m_ps.sign_data_hiding_enabled_flag            = pps.sign_data_hiding_enabled_flag ? 1 : 0;
    m_ps.cabac_init_present_flag                  = pps.cabac_init_present_flag ? 1 : 0;
    m_ps.num_ref_idx_l0_default_active            = pps.num_ref_idx_l0_default_active_minus1 + 1;
    m_ps.num_ref_idx_l1_default_active            = pps.num_ref_idx_l1_default_active_minus1 + 1;
    m_ps.init_qp                                  = pps.init_qp_minus26 + 26 + 6 * sps.bit_depth_luma_minus8;
    m_ps.constrained_intra_pred_flag              = pps.constrained_intra_pred_flag ? 1 : 0;
    m_ps.transform_skip_enabled_flag              = pps.transform_skip_enabled_flag ? 1 : 0;
    m_ps.lw_qp_delta_enabled_flag                 = pps.lw_qp_delta_enabled_flag ? 1 : 0;
    m_ps.diff_lw_qp_delta_depth                   = pps.diff_lw_qp_delta_depth;
    m_ps.pps_cb_qp_offset                         = pps.pps_cb_qp_offset;
    m_ps.pps_cr_qp_offset                         = pps.pps_cr_qp_offset;
    m_ps.pps_beta_offset                          = pps.pps_beta_offset_div2 * 2;
    m_ps.pps_tc_offset                            = pps.pps_tc_offset_div2 * 2;
    m_ps.pps_slice_chroma_qp_offsets_present_flag = pps.pps_slice_chroma_qp_offsets_present_flag ? 1 : 0;
    m_ps.weighted_pred_flag                       = pps.weighted_pred_flag ? 1 : 0;
    m_ps.weighted_bipred_flag                     = pps.weighted_bipred_flag ? 1 : 0;
    m_ps.transquant_bypass_enabled_flag           = pps.transquant_bypass_enabled_flag ? 1 : 0;
    m_ps.tiles_enabled_flag                       = pps.tiles_enabled_flag ? 1 : 0;
    m_ps.entropy_coding_sync_enabled_flag         = pps.entropy_coding_sync_enabled_flag ? 1 : 0;
    if (pps.tiles_enabled_flag)
    {
        m_ps.num_tile_columns = pps.num_tile_columns_minus1 + 1;
        m_ps.num_tile_rows = pps.num_tile_rows_minus1 + 1;
    }
    m_ps.loop_filter_across_tiles_enabled_flag       = pps.loop_filter_across_tiles_enabled_flag ? 1 : 0;
    m_ps.loop_filter_across_slices_enabled_flag      = pps.pps_loop_filter_across_slices_enabled_flag ? 1 : 0;
    m_ps.deblocking_filter_control_present_flag      = pps.deblocking_filter_control_present_flag ? 1 : 0;
    m_ps.deblocking_filter_override_enabled_flag     = pps.deblocking_filter_override_enabled_flag ? 1 : 0;
    m_ps.pps_deblocking_filter_disabled_flag         = pps.pps_deblocking_filter_disabled_flag ? 1 : 0;
    m_ps.lists_modification_present_flag             = pps.lists_modification_present_flag ? 1 : 0;
    m_ps.log2_parallel_merge_level                   = pps.log2_parallel_merge_level_minus2 + 2;
    m_ps.slice_segment_header_extension_present_flag = pps.slice_segment_header_extension_present_flag ? 1 : 0;

    m_ps.num_ref_frames                        = pic.NumPocTotalLwrr;
    m_ps.IDR_picture_flag                      = pic.IsIDR();
    m_ps.RAP_picture_flag                      = pic.IsIRAP();
    m_ps.sw_hdr_skip_length                    = pic.sw_hdr_skip_length;
    m_ps.num_bits_short_term_ref_pics_in_slice = pic.num_bits_short_term_ref_pics_in_slice;

    thisExt.log2MaxTransformSkipSize = pps.Log2MaxTransformSkipSize;
    thisExt.crossComponentPredictionEnableFlag = ppsExt.cross_component_prediction_enabled_flag ? 1 : 0;
    thisExt.chromaQpAdjustmentEnableFlag = ppsExt.chroma_qp_offset_list_enabled_flag ? 1 : 0;
    if (ppsExt.chroma_qp_offset_list_enabled_flag)
    {
        thisExt.diffLwChromaQpAdjustmentDepth = ppsExt.diff_lw_chroma_qp_offset_depth;
        thisExt.chromaQpAdjustmentTableSize = ppsExt.chroma_qp_offset_list_len_minus1 + 1;
        for (UINT32 i = 0; ppsExt.chroma_qp_offset_list_len_minus1 >= i; ++i)
        {
            thisExt.cb_qp_adjustment[i] = ppsExt.cb_qp_offset_list[i];
            thisExt.cr_qp_adjustment[i] = ppsExt.cr_qp_offset_list[i];
        }
    }
    // the following two fields have default values according to the H.265
    // specification, so they are out of the
    // 'if (chroma_qp_offset_list_enabled_flag)' block
    thisExt.log2SaoOffsetScaleLuma = ppsExt.log2_sao_offset_scale_luma;
    thisExt.log2SaoOffsetScaleChroma = ppsExt.log2_sao_offset_scale_chroma;

    if (sps.scaling_list_enabled_flag)
    {
        const H265::ScalingListData *sd = pic.GetScalingListData();
        for (size_t matrixId = 0; 6 > matrixId; ++matrixId)
        {
            m_sm.m_scalingFactorDC16x16[matrixId] = sd->ScalingFactor(H265::SCALING_MTX_16x16, matrixId, 0, 0);
        }
        m_sm.m_scalingFactorDC32x32[0] = sd->ScalingFactor(H265::SCALING_MTX_32x32, 0, 0, 0);
        m_sm.m_scalingFactorDC32x32[1] = sd->ScalingFactor(H265::SCALING_MTX_32x32, 1, 0, 0);

        for (size_t matrixId = 0; 6 > matrixId; ++matrixId)
        {
            for (size_t x = 0; 4 > x; ++x)
            {
                for (size_t y = 0; 4 > y; ++y)
                {
                    m_sm.m_scalingFactor4x4[matrixId][x][y] =
                        sd->ScalingFactor(H265::SCALING_MTX_4x4, matrixId, x, y);
                }
            }
        }
        for (size_t matrixId = 0; 6 > matrixId; ++matrixId)
        {
            for (size_t x = 0; 8 > x; ++x)
            {
                for (size_t y = 0; 8 > y; ++y)
                {
                    m_sm.m_scalingFactor8x8[matrixId][x][y] =
                        sd->ScalingFactor(H265::SCALING_MTX_8x8, matrixId, x, y);
                }
            }
        }
        for (size_t matrixId = 0; 6 > matrixId; ++matrixId)
        {
            for (size_t x = 0; 8 > x; ++x)
            {
                for (size_t y = 0; 8 > y; ++y)
                {
                    m_sm.m_scalingFactor16x16[matrixId][x][y] =
                        sd->ScalingFactor(H265::SCALING_MTX_16x16, matrixId, 2 * x + 1, 2 * y + 1);
                }
            }
        }
        for (size_t matrixId = 0; 1 > matrixId; ++matrixId)
        {
            for (size_t x = 0; 8 > x; ++x)
            {
                for (size_t y = 0; 8 > y; ++y)
                {
                    m_sm.m_scalingFactor32x32[matrixId][x][y] =
                        sd->ScalingFactor(H265::SCALING_MTX_32x32, matrixId, 4 * x + 1, 4 * y + 1);
                }
            }
        }
    }

    fill(m_ti.begin(), m_ti.end(), 0);
    vector<UINT16>::iterator p = m_ti.begin();
    if (pps.tiles_enabled_flag)
    {
        if (pps.uniform_spacing_flag)
        {
            for (UINT32 i = 0; pps.num_tile_rows_minus1 >= i; ++i)
            {
                // See (6-4) of ITU-T H.265
                UINT32 h = ((i + 1) * sps.PicHeightInCtbsY) / (pps.num_tile_rows_minus1 + 1) -
                                 (i * sps.PicHeightInCtbsY) / (pps.num_tile_rows_minus1 + 1);
                for (UINT32 j = 0; pps.num_tile_columns_minus1 >= j; ++j)
                {
                    UINT32 w = ((j + 1) * sps.PicWidthInCtbsY) / (pps.num_tile_columns_minus1 + 1) -
                                     (j * sps.PicWidthInCtbsY) / (pps.num_tile_columns_minus1 + 1);
                    *p++ = w;
                    *p++ = h;
                }
            }
        }
        else
        {
            for (UINT32 i = 0; pps.num_tile_rows_minus1 >= i; ++i)
            {
                for (UINT32 j = 0; pps.num_tile_columns_minus1 >= j; ++j)
                {
                    *p++ = pps.column_width_minus1[j] + 1;
                    *p++ = pps.row_height_minus1[i] + 1;
                }
            }
        }

        // filling of this buffer is copied from hevc_asic.c
        UINT32 count = 0;
        p = m_ti.begin() + pxlColwOffset;
        for (UINT32 i = 0; pps.num_tile_columns_minus1 >= i; ++i)
        {
            count += m_ti[i];
            *p++ = count << (sps.CtbLog2SizeY - 4);
        }
        count = 0;
        for (UINT32 i = 0; pps.num_tile_rows_minus1 >= i; ++i)
        {
            count += m_ti[i + 1];
            *p++ = count << (sps.CtbLog2SizeY - 4);
        }
    }
    else
    {
        m_ti[0] = sps.PicWidthInCtbsY;
        m_ti[1] = sps.PicHeightInCtbsY;
    }
}

void ClC4B0Engine::H265Ctrl::UpdateDPB(INT32 lwrrPicPoc, const UINT08 l0[H265::maxNumRefPics],
                                          const UINT08 l1[H265::maxNumRefPics],
                                          const INT32 pocs[H265::maxMaxDpbSize], UINT16 ltMask,
                                          unsigned char numRefFrames)
{
    copy(&l0[0], &l0[0] + NUMELEMS(m_ps.initreflistidxl0), &m_ps.initreflistidxl0[0]);
    copy(&l1[0], &l1[0] + NUMELEMS(m_ps.initreflistidxl1), &m_ps.initreflistidxl1[0]);

    m_ps.longtermflag = ltMask;
    m_ps.num_ref_frames = numRefFrames;

    for (size_t i = 0; H265::maxNumRefPics > i; ++i)
    {
        m_ps.RefDiffPicOrderCnts[l0[i]] = H265::Clip3(-128, 127, lwrrPicPoc - pocs[l0[i]]);
        m_ps.RefDiffPicOrderCnts[l1[i]] = H265::Clip3(-128, 127, lwrrPicPoc - pocs[l1[i]]);
    }
}

void ClC4B0Engine::H265Ctrl::SetLwrrPicIdx(unsigned int lwrPicIdx)
{
    m_ps.lwrr_pic_idx = lwrPicIdx;
}

void ClC4B0Engine::H265Ctrl::SetPitch(const Surface2D &surfY, const Surface2D &surfUV,
                                         OutputSize outSize)
{
    m_ps.framestride[0] = surfY.GetPitch();
    m_ps.framestride[1] = surfUV.GetPitch();
    if (OutputSize::_16bits == outSize)
    {
        m_ps.framestride[0] /= 2;
        m_ps.framestride[1] /= 2;
    }
}

void ClC4B0Engine::H265Ctrl::SetColMvBufSize(unsigned int size)
{
    m_ps.colMvBuffersize = size / 256;
}

void ClC4B0Engine::H265Ctrl::SetSaoBufferOffset(unsigned int offset)
{
    m_ps.HevcSaoBufferOffset = offset;
}

void ClC4B0Engine::H265Ctrl::SetBsdCtrlOffset(unsigned int offset)
{
    m_ps.HevcBsdCtrlOffset = offset;
}

void ClC4B0Engine::H265Ctrl::SetFilterAboveBufferOffset(unsigned int offset)
{
    m_ps.v1.hevc_main10_444_ext.HevcFltAboveOffset = offset;
}

void ClC4B0Engine::H265Ctrl::SetSaoAboveBufferOffset(unsigned int offset)
{
    m_ps.v1.hevc_main10_444_ext.HevcSaoAboveOffset = offset;
}

void ClC4B0Engine::H265Ctrl::SetSlicesSize(unsigned int slicesSize)
{
    m_ps.stream_len = slicesSize;
}

unsigned int ClC4B0Engine::H265Ctrl::GetSlicesSize(void const *buf) const
{
    const lwdec_hevc_pic_s *ps = reinterpret_cast<const lwdec_hevc_pic_s *>(buf);

    return ps->stream_len;
}

string ClC4B0Engine::H265Ctrl::PicSetupToText(void const * buf) const
{
    using Utility::StrPrintf;

    const lwdec_hevc_pic_s *ps = reinterpret_cast<const lwdec_hevc_pic_s *>(buf);

    string psStr;
    psStr += StrPrintf(
        "lwdec_hevc_pic_s =\n{\n"
        "    wrapped_session_key[0] = 0x%08X\n"
        "    wrapped_session_key[1] = 0x%08X\n"
        "    wrapped_session_key[2] = 0x%08X\n"
        "    wrapped_session_key[3] = 0x%08X\n"
        "    wrapped_content_key[0] = 0x%08X\n"
        "    wrapped_content_key[1] = 0x%08X\n"
        "    wrapped_content_key[2] = 0x%08X\n"
        "    wrapped_content_key[3] = 0x%08X\n"
        "    initialization_vector[0] = 0x%08X\n"
        "    initialization_vector[1] = 0x%08X\n"
        "    initialization_vector[2] = 0x%08X\n"
        "    initialization_vector[3] = 0x%08X\n"
        "    tileformat = %u\n"
        "    sw_start_code_e = %u\n"
        "    disp_output_mode = %u\n"
        "    framestride[0] = %u\n"
        "    framestride[1] = %u\n"
        "    colMvBuffersize = %u\n"
        "    HevcSaoBufferOffset = %u\n"
        "    HevcBsdCtrlOffset = %u\n"
        "    pic_width_in_luma_samples = %u\n"
        "    pic_height_in_luma_samples = %u\n"
        "    chroma_format_idc = %u\n"
        "    bit_depth_luma = %u\n"
        "    bit_depth_chroma = %u\n"
        "    log2_min_luma_coding_block_size = %u\n"
        "    log2_max_luma_coding_block_size = %u\n"
        "    log2_min_transform_block_size = %u\n"
        "    log2_max_transform_block_size = %u\n"
        "    reserved3 = %u\n"
        "    max_transform_hierarchy_depth_inter = %u\n"
        "    max_transform_hierarchy_depth_intra = %u\n"
        "    scalingListEnable = %u\n"
        "    amp_enable_flag = %u\n"
        "    sample_adaptive_offset_enabled_flag = %u\n"
        "    pcm_enabled_flag = %u\n"
        "    pcm_sample_bit_depth_luma = %u\n"
        "    pcm_sample_bit_depth_chroma = %u\n"
        "    log2_min_pcm_luma_coding_block_size = %u\n"
        "    log2_max_pcm_luma_coding_block_size = %u\n"
        "    pcm_loop_filter_disabled_flag = %u\n"
        "    sps_temporal_mvp_enabled_flag = %u\n"
        "    strong_intra_smoothing_enabled_flag = %u\n"
        "    reserved4 = %u\n"
        "    dependent_slice_segments_enabled_flag = %u\n"
        "    output_flag_present_flag = %u\n"
        "    num_extra_slice_header_bits = %u\n"
        "    sign_data_hiding_enabled_flag = %u\n"
        "    cabac_init_present_flag = %u\n"
        "    num_ref_idx_l0_default_active = %u\n"
        "    num_ref_idx_l1_default_active = %u\n"
        "    init_qp = %u\n"
        "    constrained_intra_pred_flag = %u\n"
        "    transform_skip_enabled_flag = %u\n"
        "    lw_qp_delta_enabled_flag = %u\n"
        "    diff_lw_qp_delta_depth = %u\n"
        "    reserved5 = %u\n"
        "    pps_cb_qp_offset = %d\n"
        "    pps_cr_qp_offset = %d\n"
        "    pps_beta_offset = %d\n"
        "    pps_tc_offset = %d\n"
        "    pps_slice_chroma_qp_offsets_present_flag = %u\n"
        "    weighted_pred_flag = %u\n"
        "    weighted_bipred_flag = %u\n"
        "    transquant_bypass_enabled_flag = %u\n"
        "    tiles_enabled_flag = %u\n"
        "    entropy_coding_sync_enabled_flag = %u\n"
        "    num_tile_columns = %u\n"
        "    num_tile_rows = %u\n"
        "    loop_filter_across_tiles_enabled_flag = %u\n"
        "    loop_filter_across_slices_enabled_flag = %u\n"
        "    deblocking_filter_control_present_flag = %u\n"
        "    deblocking_filter_override_enabled_flag = %u\n"
        "    pps_deblocking_filter_disabled_flag = %u\n"
        "    lists_modification_present_flag = %u\n"
        "    log2_parallel_merge_level = %u\n"
        "    slice_segment_header_extension_present_flag = %u\n"
        "    reserved6 = %u\n"
        "    num_ref_frames = %u\n"
        "    reserved7 = %u\n"
        "    longtermflag = %u\n",
        ps->wrapped_session_key[0],
        ps->wrapped_session_key[1],
        ps->wrapped_session_key[2],
        ps->wrapped_session_key[3],
        ps->wrapped_content_key[0],
        ps->wrapped_content_key[1],
        ps->wrapped_content_key[2],
        ps->wrapped_content_key[3],
        ps->initialization_vector[0],
        ps->initialization_vector[1],
        ps->initialization_vector[2],
        ps->initialization_vector[3],
        static_cast<unsigned int>(ps->tileformat),
        static_cast<unsigned int>(ps->sw_start_code_e),
        static_cast<unsigned int>(ps->disp_output_mode),
        ps->framestride[0],
        ps->framestride[1],
        ps->colMvBuffersize,
        ps->HevcSaoBufferOffset,
        ps->HevcBsdCtrlOffset,
        static_cast<unsigned int>(ps->pic_width_in_luma_samples),
        static_cast<unsigned int>(ps->pic_height_in_luma_samples),
        ps->chroma_format_idc,
        ps->bit_depth_luma,
        ps->bit_depth_chroma,
        ps->log2_min_luma_coding_block_size,
        ps->log2_max_luma_coding_block_size,
        ps->log2_min_transform_block_size,
        ps->log2_max_transform_block_size,
        ps->reserved3,
        ps->max_transform_hierarchy_depth_inter,
        ps->max_transform_hierarchy_depth_intra,
        ps->scalingListEnable,
        ps->amp_enable_flag,
        ps->sample_adaptive_offset_enabled_flag,
        ps->pcm_enabled_flag,
        ps->pcm_sample_bit_depth_luma,
        ps->pcm_sample_bit_depth_chroma,
        ps->log2_min_pcm_luma_coding_block_size,
        ps->log2_max_pcm_luma_coding_block_size,
        ps->pcm_loop_filter_disabled_flag,
        ps->sps_temporal_mvp_enabled_flag,
        ps->strong_intra_smoothing_enabled_flag,
        ps->reserved4,
        ps->dependent_slice_segments_enabled_flag,
        ps->output_flag_present_flag,
        ps->num_extra_slice_header_bits,
        ps->sign_data_hiding_enabled_flag,
        ps->cabac_init_present_flag,
        ps->num_ref_idx_l0_default_active,
        ps->num_ref_idx_l1_default_active,
        ps->init_qp,
        ps->constrained_intra_pred_flag,
        ps->transform_skip_enabled_flag,
        ps->lw_qp_delta_enabled_flag,
        ps->diff_lw_qp_delta_depth,
        ps->reserved5,
        static_cast<int>(ps->pps_cb_qp_offset),
        static_cast<int>(ps->pps_cr_qp_offset),
        static_cast<int>(ps->pps_beta_offset),
        static_cast<int>(ps->pps_tc_offset),
        ps->pps_slice_chroma_qp_offsets_present_flag,
        ps->weighted_pred_flag,
        ps->weighted_bipred_flag,
        ps->transquant_bypass_enabled_flag,
        ps->tiles_enabled_flag,
        ps->entropy_coding_sync_enabled_flag,
        ps->num_tile_columns,
        ps->num_tile_rows,
        ps->loop_filter_across_tiles_enabled_flag,
        ps->loop_filter_across_slices_enabled_flag,
        ps->deblocking_filter_control_present_flag,
        ps->deblocking_filter_override_enabled_flag,
        ps->pps_deblocking_filter_disabled_flag,
        ps->lists_modification_present_flag,
        ps->log2_parallel_merge_level,
        ps->slice_segment_header_extension_present_flag,
        ps->reserved6,
        static_cast<unsigned int>(ps->num_ref_frames),
        static_cast<unsigned int>(ps->reserved7),
        static_cast<unsigned int>(ps->longtermflag)
    );
    psStr += "    initreflistidxl0 = {";
    for (size_t i = 0; NUMELEMS(ps->initreflistidxl0) > i; ++i)
    {
        if (0 == i)
        {
            psStr += StrPrintf("%u", static_cast<unsigned int>(ps->initreflistidxl0[i]));
        }
        else
        {
            psStr += StrPrintf(", %u", static_cast<unsigned int>(ps->initreflistidxl0[i]));
        }
    }
    psStr += "}\n";

    psStr += StrPrintf("    initreflistidxl1 = {");
    for (size_t i = 0; NUMELEMS(ps->initreflistidxl1) > i; ++i)
    {
        if (0 == i)
        {
            psStr += StrPrintf("%u", static_cast<unsigned int>(ps->initreflistidxl1[i]));
        }
        else
        {
            psStr += StrPrintf(", %u", static_cast<unsigned int>(ps->initreflistidxl1[i]));
        }
    }
    psStr += "}\n";

    psStr += "    RefDiffPicOrderCnts = {";
    for (size_t i = 0; NUMELEMS(ps->RefDiffPicOrderCnts) > i; ++i)
    {
        if (0 == i)
        {
            psStr += StrPrintf("%d", static_cast<int>(ps->RefDiffPicOrderCnts[i]));
        }
        else
        {
            psStr += StrPrintf(", %d", static_cast<int>(ps->RefDiffPicOrderCnts[i]));
        }
    }
    psStr += "}\n";

    psStr += StrPrintf(
        "    IDR_picture_flag = %u\n"
        "    RAP_picture_flag = %u\n"
        "    lwrr_pic_idx = %u\n"
        "    pattern_id = %u\n"
        "    sw_hdr_skip_length = %u\n"
        "    stream_len = %u\n"
        "    enable_encryption = %u\n"
        "    key_increment = %u\n"
        "    encryption_mode = %u\n"
        "    reserved1 = %u\n"
        "    ecdma_cfg =\n"
        "    {\n"
        "        ecdma_enable = %u\n"
        "        ecdma_blk_x_src = %u\n"
        "        ecdma_blk_y_src = %u\n"
        "        ecdma_blk_x_dst = %u\n"
        "        ecdma_blk_y_dst = %u\n"
        "        ref_pic_idx = %u\n"
        "        boundary0_top = %u\n"
        "        boundary0_bottom = %u\n"
        "        boundary1_left = %u\n"
        "        boundary1_right = %u\n"
        "        blk_copy_flag = %u\n"
        "        ctb_size = %u\n"
        "    }\n"
        "    separate_colour_plane_flag = %u\n"
        "    log2_max_pic_order_cnt_lsb_minus4 = %u\n"
        "    num_short_term_ref_pic_sets = %u\n"
        "    num_long_term_ref_pics_sps = %u\n"
        "    bBitParsingDisable = %u\n"
        "    reserved_dxva = %u\n"
        "    num_bits_short_term_ref_pics_in_slice = %u\n"
        "    v1 =\n"
        "    {\n"
        "        hevc_main10_444_ext =\n"
        "        {\n"
        "            transformSkipRotationEnableFlag = %u\n"
        "            transformSkipContextEnableFlag = %u\n"
        "            intraBlockCopyEnableFlag = %u\n"
        "            implicitRdpcmEnableFlag = %u\n"
        "            explicitRdpcmEnableFlag = %u\n"
        "            extendedPrecisionProcessingFlag = %u\n"
        "            intraSmoothingDisabledFlag = %u\n"
        "            highPrecisionOffsetsEnableFlag = %u\n"
        "            fastRiceAdaptationEnableFlag = %u\n"
        "            cabacBypassAlignmentEnableFlag = %u\n"
        "            log2MaxTransformSkipSize = %u\n"
        "            crossComponentPredictionEnableFlag = %u\n"
        "            chromaQpAdjustmentEnableFlag = %u\n"
        "            diffLwChromaQpAdjustmentDepth = %u\n"
        "            chromaQpAdjustmentTableSize = %u\n"
        ,
        static_cast<unsigned int>(ps->IDR_picture_flag),
        static_cast<unsigned int>(ps->RAP_picture_flag),
        static_cast<unsigned int>(ps->lwrr_pic_idx),
        static_cast<unsigned int>(ps->pattern_id),
        static_cast<unsigned int>(ps->sw_hdr_skip_length),
        ps->stream_len,
        ps->enable_encryption,
        ps->key_increment,
        ps->encryption_mode,
        ps->enable_tile_WAR,
        ps->ecdma_cfg.ecdma_enable,
        static_cast<unsigned int>(ps->ecdma_cfg.ecdma_blk_x_src),
        static_cast<unsigned int>(ps->ecdma_cfg.ecdma_blk_y_src),
        static_cast<unsigned int>(ps->ecdma_cfg.ecdma_blk_x_dst),
        static_cast<unsigned int>(ps->ecdma_cfg.ecdma_blk_y_dst),
        static_cast<unsigned int>(ps->ecdma_cfg.ref_pic_idx),
        static_cast<unsigned int>(ps->ecdma_cfg.boundary0_top),
        static_cast<unsigned int>(ps->ecdma_cfg.boundary0_bottom),
        static_cast<unsigned int>(ps->ecdma_cfg.boundary1_left),
        static_cast<unsigned int>(ps->ecdma_cfg.boundary1_right),
        static_cast<unsigned int>(ps->ecdma_cfg.blk_copy_flag),
        static_cast<unsigned int>(ps->ecdma_cfg.ctb_size),
        ps->separate_colour_plane_flag,
        ps->log2_max_pic_order_cnt_lsb_minus4,
        ps->num_short_term_ref_pic_sets,
        ps->num_long_term_ref_pics_sps,
        ps->bBitParsingDisable,
        ps->reserved_dxva,
        ps->num_bits_short_term_ref_pics_in_slice,
        ps->v1.hevc_main10_444_ext.transformSkipRotationEnableFlag,
        ps->v1.hevc_main10_444_ext.transformSkipContextEnableFlag,
        ps->v1.hevc_main10_444_ext.intraBlockCopyEnableFlag,
        ps->v1.hevc_main10_444_ext.implicitRdpcmEnableFlag,
        ps->v1.hevc_main10_444_ext.explicitRdpcmEnableFlag,
        ps->v1.hevc_main10_444_ext.extendedPrecisionProcessingFlag,
        ps->v1.hevc_main10_444_ext.intraSmoothingDisabledFlag,
        ps->v1.hevc_main10_444_ext.highPrecisionOffsetsEnableFlag,
        ps->v1.hevc_main10_444_ext.fastRiceAdaptationEnableFlag,
        ps->v1.hevc_main10_444_ext.cabacBypassAlignmentEnableFlag,
        ps->v1.hevc_main10_444_ext.log2MaxTransformSkipSize,
        ps->v1.hevc_main10_444_ext.crossComponentPredictionEnableFlag,
        ps->v1.hevc_main10_444_ext.chromaQpAdjustmentEnableFlag,
        ps->v1.hevc_main10_444_ext.diffLwChromaQpAdjustmentDepth,
        ps->v1.hevc_main10_444_ext.chromaQpAdjustmentTableSize
    );

    psStr += "            cb_qp_adjustment = {";
    for (UINT32 i = 0; NUMELEMS(ps->v1.hevc_main10_444_ext.cb_qp_adjustment) > i; ++i)
    {
        if (0 == i)
        {
            psStr += StrPrintf(
                "%d",
                static_cast<int>(ps->v1.hevc_main10_444_ext.cb_qp_adjustment[i])
            );
        }
        else
        {
            psStr += StrPrintf(
                ", %d",
                static_cast<int>(ps->v1.hevc_main10_444_ext.cb_qp_adjustment[i])
            );
        }
    }
    psStr += "}\n";
    psStr += "            cr_qp_adjustment = {";
    for (UINT32 i = 0; NUMELEMS(ps->v1.hevc_main10_444_ext.cr_qp_adjustment) > i; ++i)
    {
        if (0 == i)
        {
            psStr += StrPrintf(
                "%d",
                static_cast<int>(ps->v1.hevc_main10_444_ext.cr_qp_adjustment[i])
                );
        }
        else
        {
            psStr += StrPrintf(
                ", %d",
                static_cast<int>(ps->v1.hevc_main10_444_ext.cr_qp_adjustment[i])
                );
        }
    }
    psStr += "}\n";
    psStr += StrPrintf(
        "            log2SaoOffsetScaleLuma = %u\n"
        "            log2SaoOffsetScaleChroma = %u\n"
        "            HevcFltAboveOffset = %u\n"
        "            HevcSaoAboveOffset = %u\n"
        "        }\n"
        "        sw_skip_start_length = %u\n"
        "        external_ref_mem_dis = %u\n"
        "        error_recovery_start_pos = %u\n"
        "        error_external_mv_en = %u\n"
        "    }\n"
        "}\n"
        ,
        ps->v1.hevc_main10_444_ext.log2SaoOffsetScaleLuma,
        ps->v1.hevc_main10_444_ext.log2SaoOffsetScaleChroma,
        ps->v1.hevc_main10_444_ext.HevcFltAboveOffset,
        ps->v1.hevc_main10_444_ext.HevcSaoAboveOffset,
        ps->v1.sw_skip_start_length,
        ps->v1.external_ref_mem_dis,
        ps->v1.error_recovery_start_pos,
        ps->v1.error_external_mv_en
    );

    return psStr;
}

string ClC4B0Engine::H265Ctrl::ScalingMatricesToText(void const *buf) const
{
    using Utility::StrPrintf;

    const ScalingMatrices *sm = reinterpret_cast<const ScalingMatrices *>(buf);

    string smStr = "Scaling matrices =\n{\n";

    // 4x4
    for (size_t row = 0; 4 > row; ++row)
    {
        smStr += "    ";
        for (size_t matrixId = 0; 6 > matrixId; ++matrixId)
        {
            if (0 != matrixId) smStr += "  ";
            for (size_t col = 0; 4 > col; ++col)
            {
                smStr += StrPrintf(
                    "%4u",
                    static_cast<unsigned int>(sm->m_scalingFactor4x4[matrixId][col][row])
                );
            }
        }
        smStr += '\n';
    }

    // 8x8
    for (size_t row = 0; 8 > row; ++row)
    {
        smStr += "    ";
        for (size_t matrixId = 0; 6 > matrixId; ++matrixId)
        {
            if (0 != matrixId) smStr += "  ";
            for (size_t col = 0; 8 > col; ++col)
            {
                smStr += StrPrintf(
                    "%4u",
                    static_cast<unsigned int>(sm->m_scalingFactor8x8[matrixId][col][row])
                );
            }
        }
        smStr += '\n';
    }

    // 16x16
    for (size_t row = 0; 8 > row; ++row)
    {
        smStr += "    ";
        for (size_t matrixId = 0; 6 > matrixId; ++matrixId)
        {
            if (0 != matrixId) smStr += "  ";
            for (size_t col = 0; 8 > col; ++col)
            {
                if (0 == col && 0 == row)
                {
                    smStr += StrPrintf("%4u, %4u",
                        static_cast<unsigned int>(sm->m_scalingFactorDC16x16[matrixId]),
                        static_cast<unsigned int>(sm->m_scalingFactor16x16[matrixId][0][0])
                    );
                }
                else if (0 == col)
                {
                    smStr += StrPrintf(
                        "%10u",
                        static_cast<unsigned int>(sm->m_scalingFactor16x16[matrixId][0][row])
                    );
                }
                else
                {
                    smStr += StrPrintf(
                        "%4u",
                        static_cast<unsigned int>(sm->m_scalingFactor16x16[matrixId][col][row])
                    );
                }
            }
        }
        smStr += '\n';
    }

    // 32x32
    for (size_t row = 0; 8 > row; ++row)
    {
        smStr += "    ";
        for (size_t matrixId = 0; 2 > matrixId; ++matrixId)
        {
            if (0 != matrixId) smStr += "  ";
            for (size_t col = 0; 8 > col; ++col)
            {
                if (0 == col && 0 == row)
                {
                    smStr += StrPrintf("%4u, %4u",
                        static_cast<unsigned int>(sm->m_scalingFactorDC32x32[matrixId]),
                        static_cast<unsigned int>(sm->m_scalingFactor32x32[matrixId][0][0])
                    );
                }
                else if (0 == col)
                {
                    smStr += StrPrintf(
                        "%10u",
                        static_cast<unsigned int>(sm->m_scalingFactor32x32[matrixId][0][row])
                    );
                }
                else
                {
                    smStr += StrPrintf(
                        "%4u",
                        static_cast<unsigned int>(sm->m_scalingFactor32x32[matrixId][col][row])
                    );
                }
            }
        }
        smStr += '\n';
    }

    smStr += "}\n";

    return smStr;
}

string ClC4B0Engine::H265Ctrl::TileInfoToText
(
    void const * buf,
    const H265::PicParameterSet &pps
) const
{
    using Utility::StrPrintf;

    const TileInfoBuf::value_type * const bufStart =
        reinterpret_cast<const TileInfoBuf::value_type *>(buf);

    string tiStr = "Tiles info =\n{\n";

    const TileInfoBuf::value_type *p = bufStart;
    if (pps.tiles_enabled_flag)
    {
        for (UINT32 i = 0; pps.num_tile_rows_minus1 >= i; ++i)
        {
            tiStr += "    ";
            for (UINT32 j = 0; pps.num_tile_columns_minus1 >= j; ++j)
            {
                unsigned int w = *p++;
                unsigned int h = *p++;
                if (0 == j)
                {
                    tiStr += StrPrintf("{%3u,%3u}", w, h);
                }
                else
                {
                    tiStr += StrPrintf(" {%3u,%3u}", w, h);
                }
            }
            tiStr += '\n';
        }

        tiStr += "    Pixel colwersion =\n"
                 "    {\n"
                 "        width = {";
        p = bufStart + pxlColwOffset;
        for (UINT32 i = 0; pps.num_tile_columns_minus1 >= i; ++i)
        {
            if (0 == i)
            {
                tiStr += StrPrintf("%3u", static_cast<unsigned int>(*p++));
            }
            else
            {
                tiStr += StrPrintf(", %3u", static_cast<unsigned int>(*p++));
            }
        }
        tiStr += "}\n"
                 "        height = {";
        for (UINT32 i = 0; pps.num_tile_rows_minus1 >= i; ++i)
        {
            if (0 == i)
            {
                tiStr += StrPrintf("%3u", static_cast<unsigned int>(*p++));
            }
            else
            {
                tiStr += StrPrintf(", %3u", static_cast<unsigned int>(*p++));
            }
        }
    }
    else
    {
        tiStr += StrPrintf("    {%3u,%3u}\n", bufStart[0], bufStart[1]);
    }

    tiStr += "}\n";

    return tiStr;
}

ClC4B0Engine::H265Ctrl::ScalingMatrices::ScalingMatrices()
{
    memset(this, 0, sizeof(ScalingMatrices));
    memset(m_scalingFactorDC16x16, 16, sizeof(m_scalingFactorDC16x16));
    memset(m_scalingFactorDC32x32, 16, sizeof(m_scalingFactorDC32x32));

    memset(m_scalingFactor4x4, 16, sizeof(m_scalingFactor4x4));
    memset(m_scalingFactor8x8, 16, sizeof(m_scalingFactor8x8));
    memset(m_scalingFactor16x16, 16, sizeof(m_scalingFactor16x16));
    memset(m_scalingFactor32x32, 16, sizeof(m_scalingFactor32x32));
}
