/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <algorithm>
#include <map>
#include <string>

#include <boost/container/vector.hpp>

#if defined(ALIGN_UP)
#    undef ALIGN_UP
#endif
#include "class/a0b0_new_drv.h"
#if defined(ALIGN_UP)
#    undef ALIGN_UP
#endif

#include "core/include/channel.h"
#include "ctr64encryptor.h"
#include "core/include/massert.h"
#include "core/include/lwrm.h"
#include "core/include/golden.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/platform.h"
#include "core/include/rc.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surffill.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include "core/include/utility.h"

#include "h264parser.h"

#include "clb0b0engine.h"

using Utility::AlignUp;
using Utility::StrPrintf;
using Platform::VirtualRd;

class ClB0B0Engine::H264Ctrl
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
    void UpdateDPB(const H264::Picture &pic, size_t whereIdx, H264::DPB::const_iterator fromWhat,
                   unsigned int lwdecRefIdx, MemLayout memLayout);
    void SetOutputMemoryLayout(MemLayout memLayout);
    void SetHistBufferSize(unsigned int size);
    void EnableHistogram(short startX, short startY, short endX, short endY);
    void SetPitchOffset(const Surface2D &surfY, const Surface2D &surfUV);

    void UpdateFromPicture(const H264::Picture &pic);
    void EnableEncryption(unsigned int keyIncrement, const UINT32 sessionKey[4],
                          const UINT32 contentKey[4], const UINT32 initVector[4]);
    string PicSetupToText(void const *buf) const;

private:
    lwdec_h264_pic_s m_ps;
    vector<UINT32>   m_sliceOffsets;
};

ClB0B0Engine::ClB0B0Engine(Channel *lwCh, LwRm::Handle hLwdec, Tee::Priority printPri)
  : m_lwdecHandle(hLwdec)
  , m_printPri(printPri)
  , m_lwCh(lwCh)
{
    MASSERT(m_lwCh);

    memset(m_wrappedSessionKey, 0, sizeof(m_wrappedSessionKey));
    memset(m_wrappedContentKey, 0, sizeof(m_wrappedContentKey));
    memset(m_contentKey, 0, sizeof(m_contentKey));
    memset(m_initVector, 0, sizeof(m_initVector));
}

ClB0B0Engine::~ClB0B0Engine()
{}

template <class H264Layouter>
void ClB0B0Engine::BuildH264Layout(H264Layouter *layouter)
{
    MASSERT(layouter);

    H264::Parser::pic_iterator picIt;
    H264CtrlCont::iterator ctrlIt;

    // Layout slices and control structures
    for (ctrlIt = m_h264Ctrl.begin(), picIt = m_h264parser->pic_begin();
         m_h264parser->pic_end() != picIt; ++picIt, ++ctrlIt)
    {
        layouter->AddPicture(picIt->slices_begin(), picIt->slices_end());
        if (m_encryption)
        {
            ctrlIt->EnableEncryption(
                m_keyIncrement,
                GetWrappedSessionKey(),
                GetWrappedContentKey(),
                layouter->GetInitializatiolwector()
            );
        }
        layouter->AddPicSetup(ctrlIt->GetPicSetup(), ctrlIt->GetPicSetupSize());
        layouter->AddSliceOffsets(ctrlIt->GetSliceOffsets(), ctrlIt->GetSliceOffsetsSize());
    }

    layouter->AddHistBuffer(m_h264HistBufSize);
    layouter->AddColocBuffer(m_h264ColocBufSize);
    layouter->AddMBHistBuffer(m_h264MbHistBufSize);
}

UINT32 ClB0B0Engine::CalcH264LayoutSize()
{
    if (!m_encryption)
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

RC ClB0B0Engine::InitH264(GpuDevice *dev, GpuSubdevice *subDev, LwRm::Handle chHandle)
{
    RC rc;

    m_h264Ctrl.clear();

    m_initedTo = h264;
    UpdateControlStructsFromPics();

    // Sizes of the buffers below are taken from
    // //hw/gpu_ip/unit/msdec/5.0_merge/cmod/h264/drv and
    // //sw/dev/gpu_drv/chips_a/drivers/video/decode/src. There is no
    // documentation, explaining these values.

    // From CLWDECH264Decoder::Initialize in
    // //sw/dev/gpu_drv/chips_a/drivers/video/decode/src/lwLWDEC3H264.cpp.
    m_h264HistBufSize = 3 * 256 * m_h264parser->GetWidthInMb();

    // From tvecdrvgen_c::coloc_init_sequence in
    // //hw/gpu_ip/unit/msdec/5.0_merge/cmod/h264/drv/tvecdrvgen.cpp.
    unsigned int colocBufSizeInMb;
    unsigned int widthInMb  = m_h264parser->GetWidthInMb();
    unsigned int heightInMb = m_h264parser->GetHeightInMb();
    colocBufSizeInMb = widthInMb * AlignUp<2>(heightInMb);
    colocBufSizeInMb = AlignUp<4>(colocBufSizeInMb);
    m_h264ColocBufSize = static_cast<UINT32>(
        64 * colocBufSizeInMb * (m_h264parser->GetMaxDpbSize() + 1)
    );
    m_h264ColocBufSize = AlignUp<256>(m_h264ColocBufSize);

    // From //hw/lwgpu_gmlit1/class/mfs/class/lwdec/gmlit1_lwdec_decode.mfs.
    m_h264MbHistBufSize = AlignUp<256>(widthInMb * 104);

    m_inSurf.SetColorFormat(ColorUtils::Y8);
    m_inSurf.SetWidth(CalcH264LayoutSize());
    m_inSurf.SetHeight(1);
    m_inSurf.SetProtect(Memory::ReadWrite);
    m_inSurf.SetLayout(Surface2D::Pitch);
    m_inSurf.SetDisplayable(false);
    m_inSurf.SetType(LWOS32_TYPE_IMAGE);
    m_inSurf.SetLocation(Memory::Optimal);
    CHECK_RC(m_inSurf.Alloc(dev));
    CHECK_RC(m_inSurf.BindGpuChannel(chHandle));
    CHECK_RC(m_inSurf.Fill(0, subDev->GetSubdeviceInst()));

    return rc;
}

RC ClB0B0Engine::DoInitFromH264File(const char *fileName, const TarFile *tarFile, GpuDevice *dev,
                                    GpuSubdevice *subDev, LwRm::Handle chHandle)
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
        m_inStreamFileName.assign(&fileName[0], dotPos - 1);
    }
    else
    {
        m_inStreamFileName = fileName;
    }

    m_h264parser = make_shared<H264::Parser>();

    CHECK_RC(m_h264parser->ParseStream(stream.begin(), stream.end()));
    CHECK_RC(InitH264(dev, subDev, chHandle));

    return rc;
}

RC ClB0B0Engine::DoInitFromH265File(const char *fileName, const TarFile *tarFile, GpuDevice *dev,
                                    GpuSubdevice *subDev, LwRm::Handle chHandle)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC ClB0B0Engine::DoInitFromAnother
(
    GpuDevice *dev,
    GpuSubdevice *subDev,
    LwRm::Handle chHandle,
    Engine &another
)
{
    RC rc;

    auto &ae = dynamic_cast<ClB0B0Engine &>(another);
    m_inStreamFileName = ae.GetInStreamFileName();
    m_h264parser = ae.GetH264Parser();

    if (m_h264parser)
    {
        CHECK_RC(InitH264(dev, subDev, chHandle));
    }

    return rc;
}

RC ClB0B0Engine::DoInitOutput(GpuDevice * dev, GpuSubdevice * subDev, LwRm::Handle chHandle,
                              FLOAT64 skTimeout)
{
    RC rc;

    UINT32 numOutSurf = 0;
    unsigned int width = 0;
    unsigned int height = 0;

    if (h264 != m_initedTo)
    {
        return rc;
    }

    numOutSurf = static_cast<UINT32>(m_h264parser->GetMaxDpbSize() + 1);
    width = m_h264parser->GetWidth();
    height = m_h264parser->GetHeight();

    m_outputSize = OutputSize::_8bits;

    CHECK_RC(InitOutSurfaces(width, height, numOutSurf, dev, subDev, chHandle));

    m_histogramSurf.SetColorFormat(ColorUtils::Y8);
    m_histogramSurf.SetWidth(maxHistBins * sizeof(UINT32));
    m_histogramSurf.SetHeight(1);
    m_histogramSurf.SetProtect(Memory::ReadWrite);
    m_histogramSurf.SetLayout(Surface2D::Pitch);
    m_histogramSurf.SetDisplayable(false);
    m_histogramSurf.SetType(LWOS32_TYPE_IMAGE);
    CHECK_RC(m_histogramSurf.Alloc(dev));
    CHECK_RC(m_histogramSurf.Fill(0, subDev->GetSubdeviceInst()));
    CHECK_RC(m_histogramSurf.BindGpuChannel(chHandle));

    H264CtrlCont::iterator ctrlIt;
    for (ctrlIt = m_h264Ctrl.begin(); m_h264Ctrl.end() != ctrlIt; ++ctrlIt)
    {
        ctrlIt->SetPitchOffset(m_outSurfsY.front(), m_outSurfsUV.front());
    }
    BuildH264Picsetup();
    if (m_encryption)
    {
        // arbitrary numbers, Pi for the content key and Euler's e for the init vector
        static const UINT32 contentKey[] = { 0x54442d18, 0x400921fb, 0x54442d18, 0x400921fb };
        static const UINT32 initVector[] = { 0x8b145769, 0x4005bf0a, 0x8b145769, 0x4005bf0a };

        SetContentKey(contentKey);
        SetInitVector(initVector);

        CHECK_RC(SetupContentKey(subDev, skTimeout));
        m_h264EncMemLayout.SetContentKey(GetContentKey());
        m_h264EncMemLayout.SetInitializatiolwector(GetInitVector());
    }

    Surface2D::MappingSaver oldMapping(m_inSurf);
    if (m_inSurf.IsMapped() || OK == (rc = m_inSurf.Map()))
    {
        UINT08 *surfBegin = static_cast<UINT08*>(m_inSurf.GetAddress());
        if (m_encryption)
        {
            m_h264EncMemLayout.SetStartAddress(surfBegin);
            m_h264EncMemLayout.SetSliceAlign(GetH264EncSlicesAlignment());
            m_h264EncMemLayout.SetControlAlign(GetControlStructsAlignment());
            BuildH264Layout(&m_h264EncMemLayout);
        }
        else
        {
            m_h264MemLayout.SetStartAddress(surfBegin);
            m_h264MemLayout.SetSliceAlign(GetH264SlicesAlignment());
            m_h264MemLayout.SetControlAlign(GetControlStructsAlignment());
            BuildH264Layout(&m_h264MemLayout);
        }
    }

    m_lwrPic = 0;
    m_goldOutSurfLoopCnt = 0;
    m_goldHistSurfLoopCnt = 0;
    m_outFramesCount = 0;

    return rc;
}

RC ClB0B0Engine::DoUpdateInDataGoldens(GpuGoldenSurfaces * goldSurfs, Goldelwalues * goldValues)
{
    MASSERT(goldSurfs);
    MASSERT(goldValues);

    RC rc;

    if (m_encryption)
    {
        // We cannot check the consistency of the encrypted data, because
        // the input data is encrypted with the current DH key. The DH key is
        // different every time.
       return rc;
    }

    if (0 == goldSurfs->NumSurfaces())
    {
        goldSurfs->AttachSurface(&m_inSurf, "A", 0);
    }
    else
    {
        goldSurfs->ReplaceSurface(0, &m_inSurf);
    }
    CHECK_RC(goldValues->SetSurfaces(goldSurfs));
    CHECK_RC(goldValues->OverrideSuffix(m_inStreamGoldSuffix + "-indata"));
    goldValues->SetLoop(0);

    CHECK_RC(goldValues->Run());

    return rc;
}

RC ClB0B0Engine::DoUpdateOutPicsGoldens(GpuGoldenSurfaces *goldSurfs, Goldelwalues *goldValues)
{
    MASSERT(goldSurfs);
    MASSERT(goldValues);

    RC rc;

    if (h264 != m_initedTo)
    {
        return rc;
    }

    const OutSurfIDsCont *outPics = nullptr;
    if (GetNumPics() > m_lwrPic)
    {
        const H264::Picture &pic = *(m_h264parser->pic_begin() + m_lwrPic);
        size_t lwrPicIdx = pic.GetPicIdx();
        outPics = &m_outputPics[lwrPicIdx];
    }
    else
    {
        outPics = &m_lastOutPics;
    }

    for (OutSurfIDsCont::const_iterator it = outPics->begin(); outPics->end() != it; ++it)
    {
        if (0 == goldSurfs->NumSurfaces())
        {
            goldSurfs->AttachSurface(&m_outSurfsY[*it], "A", 0);
            goldSurfs->AttachSurface(&m_outSurfsUV[*it], "A", 0);
        }
        else
        {
            goldSurfs->ReplaceSurface(0, &m_outSurfsY[*it]);
            goldSurfs->ReplaceSurface(1, &m_outSurfsUV[*it]);
        }
        CHECK_RC(goldValues->SetSurfaces(goldSurfs));
        CHECK_RC(goldValues->OverrideSuffix(m_inStreamGoldSuffix));
        goldValues->SetLoop(m_goldOutSurfLoopCnt++);

        CHECK_RC(goldValues->Run());
    }

    return rc;
}

RC ClB0B0Engine::DoUpdateHistGoldens(GpuGoldenSurfaces * goldSurfs, Goldelwalues * goldValues)
{
    MASSERT(goldSurfs);
    MASSERT(goldValues);

    RC rc;

    if (h264 != m_initedTo)
    {
        return rc;
    }

    if (0 == goldSurfs->NumSurfaces())
    {
        goldSurfs->AttachSurface(&m_histogramSurf, "A", 0);
    }
    else
    {
        goldSurfs->ReplaceSurface(0, &m_histogramSurf);
    }
    CHECK_RC(goldValues->SetSurfaces(goldSurfs));
    CHECK_RC(goldValues->OverrideSuffix(m_inStreamGoldSuffix + "-hist"));
    goldValues->SetLoop(m_goldHistSurfLoopCnt++);

    CHECK_RC(goldValues->Run());

    return rc;
}

RC ClB0B0Engine::DoSaveFrames(const string &nameTemplate, UINT32 instNum)
{
    RC rc;

    if (h264 != m_initedTo)
    {
        return rc;
    }

    const OutSurfIDsCont *outPics = nullptr;
    if (GetNumPics() > m_lwrPic)
    {
        const H264::Picture &pic = *(m_h264parser->pic_begin() + m_lwrPic);
        size_t lwrPicIdx = pic.GetPicIdx();
        outPics = &m_outputPics[lwrPicIdx];
    }
    else
    {
        outPics = &m_lastOutPics;
    }

    for (OutSurfIDsCont::const_iterator it = outPics->begin(); outPics->end() != it; ++it)
    {
        string fileName =
            m_inStreamFileName +
            "-Y-" +
            StrPrintf("%03u", m_outFramesCount) +
            ".png";
        CHECK_RC(m_outSurfsY[*it].WritePng(fileName.c_str()));
        fileName =
            m_inStreamFileName +
            "-UV-" +
            StrPrintf("%03u", m_outFramesCount) +
            ".png";
        CHECK_RC(m_outSurfsUV[*it].WritePng(fileName.c_str()));
        ++m_outFramesCount;
    }

    return rc;
}

RC ClB0B0Engine::DoFree()
{
    RC rc;

    m_inSurf.Free();
    OutSurfCont::iterator surfIt;
    for (surfIt = m_outSurfsY.begin(); m_outSurfsY.end() != surfIt; ++surfIt)
    {
        surfIt->Free();
    }
    m_outSurfsY.clear();

    for (surfIt = m_outSurfsUV.begin(); m_outSurfsUV.end() != surfIt; ++surfIt)
    {
        surfIt->Free();
    }
    m_outSurfsUV.clear();

    if (h264 == m_initedTo)
    {
        m_histogramSurf.Free();
    }

    m_h264MemLayout.SetStartAddress(0);
    m_h264EncMemLayout.SetStartAddress(0);

    m_h264parser.reset();

    m_h264Ctrl.clear();

    return rc;
}

size_t ClB0B0Engine::DoGetNumPics() const
{
    return m_h264parser->GetNumFrames();
}

RC ClB0B0Engine::DoInitChannel()
{
    RC rc;
    CHECK_RC(m_lwCh->SetObject(0, m_lwdecHandle));
    return rc;
}

RC ClB0B0Engine::DoSetupContentKey(GpuSubdevice *subDev, FLOAT64 skTimeout)
{
    return Engine::DoSetupContentKey(subDev, skTimeout);
}

const UINT32* ClB0B0Engine::DoGetWrappedSessionKey() const
{
    return m_wrappedSessionKey;
}

void ClB0B0Engine::DoSetWrappedSessionKey(const UINT32 wsk[AES::DW])
{
    memcpy(m_wrappedSessionKey, wsk, sizeof(m_wrappedSessionKey));
}

const UINT32* ClB0B0Engine::DoGetWrappedContentKey() const
{
    return m_wrappedContentKey;
}

void ClB0B0Engine::DoSetWrappedContentKey(const UINT32 wck[AES::DW])
{
    memcpy(m_wrappedContentKey, wck, sizeof(m_wrappedContentKey));
}

const UINT32* ClB0B0Engine::DoGetContentKey() const
{
    return m_contentKey;
}

void ClB0B0Engine::DoSetContentKey(const UINT32 ck[AES::DW])
{
    memcpy(m_contentKey, ck, sizeof(m_contentKey));
}

const UINT32* ClB0B0Engine::DoGetInitVector() const
{
    return m_initVector;
}

void ClB0B0Engine::DoSetInitVector(const UINT32 iv[AES::DW])
{
    memcpy(m_initVector, iv, sizeof(m_initVector));
}

UINT08 ClB0B0Engine::DoGetKeyIncrement() const
{
    return m_keyIncrement;
}

void ClB0B0Engine::DoSetKeyIncrement(UINT08 ki)
{
    m_keyIncrement = ki;
}

RC ClB0B0Engine::DoDecodeOneFrame(FLOAT64 timeOut)
{
    RC rc;

    if (GetNumPics() <= m_lwrPic)
    {
        return rc;
    }

    if (h264 != m_initedTo)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(CreateH264PushBuffer());

    CHECK_RC(Execute(false, notifyOnEnd, true));
    CHECK_RC(Flush());
    CHECK_RC(m_lwCh->WaitIdle(timeOut));

    ++m_lwrPic;

    return rc;
}

template <class Layouter>
RC ClB0B0Engine::PrintH264InputData(const Layouter &layouter)
{
    RC rc;

    const H264::Picture &pic = *(m_h264parser->pic_begin() + m_lwrPic);
    const UINT32 numSlices = static_cast<UINT32>(pic.GetNumSlices());
    const H264Ctrl &ctrl = m_h264Ctrl[m_lwrPic];
    const UINT32 psOffs = *(layouter.pic_set_begin() + m_lwrPic);
    const UINT32 sliceOffsetsOffs = *(layouter.sliceofs_begin() + m_lwrPic);

    Printf(m_printPri, "Control structures and input data for picture %u\n",
           static_cast<unsigned int>(m_lwrPic));

    Surface2D::MappingSaver oldMapping(m_inSurf);
    if (m_inSurf.IsMapped() || OK == (rc = m_inSurf.Map()))
    {
        UINT08* inAddr = static_cast<UINT08*>(m_inSurf.GetAddress());

        vector<UINT08> ps(ctrl.GetPicSetupSize());
        vector<UINT08> sliceOffsets(ctrl.GetSliceOffsetsSize());

        VirtualRd(inAddr + psOffs, &ps[0], static_cast<UINT32>(ps.size()));
        VirtualRd(inAddr + sliceOffsetsOffs, &sliceOffsets[0], static_cast<UINT32>(sliceOffsets.size()));

        string psStr = ctrl.PicSetupToText(&ps[0]);
        string sliceOffsetsStr = ctrl.SliceOffsetsToText(&sliceOffsets[0], numSlices);

        Printf(m_printPri, "%s", psStr.c_str());
        Printf(m_printPri, "%s", sliceOffsetsStr.c_str());

        string slicesDataStr;
        vector<UINT08> slicesData(ctrl.GetSlicesSize(&ps[0]));
        size_t slicesDataStart = *(layouter.frames_begin() + m_lwrPic);
        VirtualRd(inAddr + slicesDataStart, &slicesData[0],
            static_cast<UINT32>(slicesData.size()));

        slicesDataStr += StrPrintf("Slices data for picture %u = \n{\n", m_lwrPic);

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
        Printf(m_printPri, "%s", slicesDataStr.c_str());
    }

    return rc;
}

RC ClB0B0Engine::DoPrintInputData()
{
    RC rc;

    if (h264 != m_initedTo)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (m_encryption)
    {
        CHECK_RC(PrintH264InputData(m_h264EncMemLayout));
    }
    else
    {
        CHECK_RC(PrintH264InputData(m_h264MemLayout));
    }

    return rc;
}

RC ClB0B0Engine::DoPrintHistogram()
{
    RC rc;

    if (h264 != m_initedTo)
    {
        return rc;
    }

    Surface2D::MappingSaver oldMapping(m_histogramSurf);
    if (m_histogramSurf.IsMapped() || OK == (rc = m_histogramSurf.Map()))
    {
        vector<UINT32> histData(maxHistBins);
        Platform::VirtualRd(
            m_histogramSurf.GetAddress(),
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
        Printf(m_printPri, "Picture histogram = {\n%s}\n", histStr.c_str());
    }

    return rc;
}

LWDEC::Engine* ClB0B0Engine::DoClone() const
{
    return new ClB0B0Engine(*this);
}

RC ClB0B0Engine::SetApplicationID(ApplicationId id)
{
    RC rc;

    Printf(m_printPri, "SetApplicationID, id = %u\n", static_cast<unsigned int>(id));

    CHECK_RC(m_lwCh->Write(
        0, LWB0B0_SET_APPLICATION_ID,
        DRF_NUM(B0B0, _SET_APPLICATION_ID, _ID, static_cast<unsigned int>(id))));

    return rc;
}

RC ClB0B0Engine::SetControlParams(CodecType codecType, bool gpTimerOn, bool retErr,
                                  bool errConcealOn, unsigned int errorFrmIdx)
{
    RC rc;

    Printf(m_printPri,
        "SetControlParams, codecType = %u, "
        "gpTimerOn = %u, "
        "retErr = %u, "
        "errConcealOn = %u, "
        "errorFrmIdx = %u\n",
        static_cast<unsigned int>(codecType),
        gpTimerOn ? 1 : 0,
        retErr ? 1 : 0,
        errConcealOn ? 1 : 0,
        errorFrmIdx
    );

    UINT32 controlParams;

    controlParams = DRF_NUM(B0B0, _SET_CONTROL_PARAMS, _CODEC_TYPE, codecType);
    controlParams |= DRF_NUM(B0B0, _SET_CONTROL_PARAMS, _GPTIMER_ON, gpTimerOn);
    controlParams |= DRF_NUM(B0B0, _SET_CONTROL_PARAMS, _RET_ERROR, retErr);
    controlParams |= DRF_NUM(B0B0, _SET_CONTROL_PARAMS, _ERR_CONCEAL_ON, errConcealOn);
    controlParams |= DRF_NUM(B0B0, _SET_CONTROL_PARAMS, _ERROR_FRM_IDX, errorFrmIdx);

    CHECK_RC(m_lwCh->Write(0, LWB0B0_SET_CONTROL_PARAMS, controlParams));

    return rc;
}

RC ClB0B0Engine::SetInBufBaseOffset(const Surface2D& surf, UINT32 offs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + offs) >> 8);
    Printf(m_printPri, "SetInBufBaseOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_lwCh->WriteWithSurface(0, LWB0B0_SET_IN_BUF_BASE_OFFSET, surf, offs, 8));

    return rc;
}

RC ClB0B0Engine::SetPictureIndex(UINT32 idx)
{
    RC rc;

    Printf(m_printPri, "SetPictureIndex, idx = 0x%08x\n", idx);

    CHECK_RC(m_lwCh->Write(
        0, LWB0B0_SET_PICTURE_INDEX,
        DRF_NUM(B0B0, _SET_PICTURE_INDEX, _INDEX, idx)));

    return rc;
}

RC ClB0B0Engine::SetDrvPicSetupOffset(const Surface2D& surf, UINT32 ofs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + ofs) >> 8);
    Printf(m_printPri, "SetDrvPicSetupOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_lwCh->WriteWithSurface(0, LWB0B0_SET_DRV_PIC_SETUP_OFFSET, surf, ofs, 8));

    return rc;
}

RC ClB0B0Engine::SetHistoryOffset(const Surface2D& surf, UINT32 ofs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + ofs) >> 8);
    Printf(m_printPri, "SetHistoryOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_lwCh->WriteWithSurface(0, LWB0B0_SET_HISTORY_OFFSET, surf, ofs, 8));

    return rc;
}

RC ClB0B0Engine::SetColocDataOffset(const Surface2D& surf, UINT32 ofs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + ofs) >> 8);
    Printf(m_printPri, "SetColocDataOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_lwCh->WriteWithSurface(0, LWB0B0_SET_COLOC_DATA_OFFSET, surf, ofs, 8));

    return rc;
}

RC ClB0B0Engine::SetHistogramOffset(const Surface2D& surf)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>(surf.GetCtxDmaOffsetGpu() >> 8);
    Printf(m_printPri, "SetHistogramOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_lwCh->WriteWithSurface(0, LWB0B0_SET_HISTOGRAM_OFFSET, surf, 0, 8));

    return rc;
}

RC ClB0B0Engine::H264SetMBHistBufOffset(const Surface2D& surf, UINT32 ofs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + ofs) >> 8);
    Printf(m_printPri, "H264SetMBHistBufOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_lwCh->WriteWithSurface(0, LWB0B0_H264_SET_MBHIST_BUF_OFFSET, surf, ofs, 8));

    return rc;
}

RC ClB0B0Engine::SetPictureLumaOffset(size_t picIdx, const Surface2D& surf)
{
    static const UINT32 method[] =
    {
        LWB0B0_SET_PICTURE_LUMA_OFFSET0
      , LWB0B0_SET_PICTURE_LUMA_OFFSET1
      , LWB0B0_SET_PICTURE_LUMA_OFFSET2
      , LWB0B0_SET_PICTURE_LUMA_OFFSET3
      , LWB0B0_SET_PICTURE_LUMA_OFFSET4
      , LWB0B0_SET_PICTURE_LUMA_OFFSET5
      , LWB0B0_SET_PICTURE_LUMA_OFFSET6
      , LWB0B0_SET_PICTURE_LUMA_OFFSET7
      , LWB0B0_SET_PICTURE_LUMA_OFFSET8
      , LWB0B0_SET_PICTURE_LUMA_OFFSET9
      , LWB0B0_SET_PICTURE_LUMA_OFFSET10
      , LWB0B0_SET_PICTURE_LUMA_OFFSET11
      , LWB0B0_SET_PICTURE_LUMA_OFFSET12
      , LWB0B0_SET_PICTURE_LUMA_OFFSET13
      , LWB0B0_SET_PICTURE_LUMA_OFFSET14
      , LWB0B0_SET_PICTURE_LUMA_OFFSET15
      , LWB0B0_SET_PICTURE_LUMA_OFFSET16
    };

    if (picIdx >= NUMELEMS(method))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    const UINT32 data = static_cast<UINT32>(surf.GetCtxDmaOffsetGpu() >> 8);
    Printf(m_printPri,
        "SetPictureLumaOffset, picIdx = %u, offset = 0x%08x\n",
        static_cast<unsigned int>(picIdx),
        data
    );

    return m_lwCh->WriteWithSurface(0, method[picIdx], surf, 0, 8);
}

RC ClB0B0Engine::SetPictureChromaOffset(size_t picIdx, const Surface2D& surf)
{
    static const UINT32 method[] =
    {
        LWB0B0_SET_PICTURE_CHROMA_OFFSET0
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET1
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET2
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET3
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET4
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET5
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET6
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET7
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET8
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET9
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET10
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET11
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET12
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET13
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET14
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET15
      , LWB0B0_SET_PICTURE_CHROMA_OFFSET16
    };

    if (picIdx >= NUMELEMS(method))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    const UINT32 data = static_cast<UINT32>(surf.GetCtxDmaOffsetGpu() >> 8);
    Printf(m_printPri,
        "SetPictureChromaOffset, picIdx = %u, offset = 0x%08x\n",
        static_cast<unsigned int>(picIdx),
        data
    );

    return m_lwCh->WriteWithSurface(0, method[picIdx], surf, 0, 8);
}

RC ClB0B0Engine::SetSliceOffsetsBufOffset(const Surface2D& surf, UINT32 ofs)
{
    RC rc;

    const UINT32 data = static_cast<UINT32>((surf.GetCtxDmaOffsetGpu() + ofs) >> 8);
    Printf(m_printPri, "SetSliceOffsetsBufOffset, offset = 0x%08x\n", data);

    CHECK_RC(m_lwCh->WriteWithSurface(0, LWB0B0_SET_SLICE_OFFSETS_BUF_OFFSET, surf, ofs, 8));

    return rc;
}

RC ClB0B0Engine::Execute(bool notify, NotifyOn notifyOn, bool awaken)
{
    RC rc;

    Printf(m_printPri,
        "Execute, notify = %u, "
        "notifyOn = %u, "
        "awaken = %u\n",
        notify ? 1 : 0,
        static_cast<unsigned int>(notifyOn),
        awaken ? 1 : 0
    );

    CHECK_RC(m_lwCh->Write(
        0, LWB0B0_EXELWTE,
        (notify ?
            DRF_DEF(B0B0, _EXELWTE, _NOTIFY, _ENABLE) :
            DRF_DEF(B0B0, _EXELWTE, _NOTIFY, _DISABLE))
        |
        (awaken ?
            DRF_DEF(B0B0, _EXELWTE, _AWAKEN, _ENABLE) :
            DRF_DEF(B0B0, _EXELWTE, _AWAKEN, _DISABLE))
        |
        (notifyOnBeg == notifyOn ?
            DRF_DEF(B0B0, _EXELWTE, _NOTIFY_ON, _BEGIN) :
            DRF_DEF(B0B0, _EXELWTE, _NOTIFY_ON, _END))
    ));

    return rc;
}

RC ClB0B0Engine::Flush()
{
    RC rc;

    CHECK_RC(m_lwCh->Flush());

    return rc;
}

void ClB0B0Engine::UpdateControlStructsFromPics()
{
    if (h264 != m_initedTo)
    {
        MASSERT(false);
    }

    m_h264Ctrl.resize(GetNumPics());
    H264::Parser::pic_const_iterator it;
    size_t picNum;
    for (picNum = 0, it = m_h264parser->pic_begin(); m_h264parser->pic_end() != it; ++it, ++picNum)
    {
        m_h264Ctrl[picNum].UpdateFromPicture(*it);
    }
}

RC ClB0B0Engine::InitOutSurfaces(UINT32 width, UINT32 height, UINT32 numPics, GpuDevice *dev,
                                 GpuSubdevice *subDev, LwRm::Handle chHandle)
{
    RC rc;

    m_outSurfsY.resize(numPics);
    m_outSurfsUV.resize(numPics);
    OutSurfCont::iterator outSurfIt;
    for (outSurfIt = m_outSurfsY.begin(); m_outSurfsY.end() != outSurfIt; ++outSurfIt)
    {
        outSurfIt->SetWidth(width);
        outSurfIt->SetHeight(height);
        outSurfIt->SetColorFormat(m_outputSize == OutputSize::_16bits ?
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

    for (outSurfIt = m_outSurfsUV.begin(); m_outSurfsUV.end() != outSurfIt; ++outSurfIt)
    {
        outSurfIt->SetWidth(width);
        outSurfIt->SetHeight(height / 2);
        outSurfIt->SetColorFormat(m_outputSize == OutputSize::_16bits ?
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

void ClB0B0Engine::BuildH264Picsetup()
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

    size_t outSurfsSize = m_outSurfsY.size();
    for (size_t i = 0; outSurfsSize > i; ++i)
    {
        surfIndicesPool.insert(i);
    }

    size_t maxDPBSize = m_h264Ctrl.front().GetMaxDPBSize();
    for (size_t i = 0; maxDPBSize > i; ++i)
    {
        lwdecDPBIndicesPool.insert(i);
    }

    H264::Parser::pic_iterator picIt;
    H264CtrlCont::iterator ctrlIt;
    for (ctrlIt = m_h264Ctrl.begin(), picIt = m_h264parser->pic_begin();
         m_h264parser->pic_end() != picIt; ++picIt, ++ctrlIt)
    {
        size_t lwrPicIdx = picIt->GetPicIdx();
        const H264::DPB &dpb = picIt->GetDPB();

        // memorize what surfaces can be output
        OutSurfIDsCont &outPics = m_outputPics[lwrPicIdx];
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
        ctrlIt->SetHistBufferSize(m_h264HistBufSize / 256);
        ctrlIt->EnableHistogram(0, 0, m_h264parser->GetWidth() - 1, m_h264parser->GetHeight() - 1);
        ctrlIt->SetSlicesSize(static_cast<unsigned int>(picIt->GetTotalNalusSize()));
        ctrlIt->SetSlicesCount(static_cast<unsigned int>(picIt->GetNumSlices()));
        ctrlIt->SetMBHistBufSize(static_cast<unsigned int>(m_h264MbHistBufSize));
    }
    const H264::DPB &dpb = m_h264parser->pic_back().GetDPB();
    m_lastOutPics.clear();
    // add pictures remained in the last DPB
    for (H264::DPB::const_iterator it = dpb.begin(); dpb.end() != it; ++it)
    {
        if (!it->IsOutput())
        {
            m_lastOutPics.push_back(it->GetPicIdx());
        }
    }
    // add the last picture itself
    m_lastOutPics.push_back(m_h264parser->pic_back().GetPicIdx());

    // sort the remaining output pictures according to the display order
    sort(m_lastOutPics.begin(), m_lastOutPics.end(), CompPOC(*m_h264parser));
    // map H.264 parser indices to output surface indices
    for (OutSurfIDsCont::iterator it = m_lastOutPics.begin(); m_lastOutPics.end() != it; ++it)
    {
        *it = mapToLWDECIdx[*it];
    }

    // print out the result
    size_t list0MaxSize = 0;
    size_t list1MaxSize = 0;
    size_t outPicsMax = 0;
    for (picIt = m_h264parser->pic_begin(); m_h264parser->pic_end() != picIt; ++picIt)
    {
        size_t list0Size = picIt->slices_front().l0_size();
        size_t list1Size = picIt->slices_front().l1_size();

        list0MaxSize = max(list0MaxSize, list0Size);
        list1MaxSize = max(list1MaxSize, list1Size);

        const OutSurfIDsCont &outPics = m_outputPics[picIt->GetPicIdx()];
        outPicsMax = max(outPicsMax, outPics.size());
    }
    string picListStr = "       # POC type|";
    picListStr += StrPrintf(
        StrPrintf("%%%ds", static_cast<int>(list0MaxSize * 4)).c_str(),
        "l0");
    picListStr += '|';
    picListStr += StrPrintf(
        StrPrintf("%%%ds", static_cast<int>(list1MaxSize * 4)).c_str(),
        "l1");
    picListStr += "| idx col|";
    if (0 < outPicsMax)
    {
        picListStr += StrPrintf(
            StrPrintf("%%-%ds", static_cast<int>(maxDPBSize * 6)).c_str(),
            " DPB indices and colocation indices");
        picListStr += "| Output";
    }
    else
    {
        picListStr += " DPB indices and colocation indices";
    }
    picListStr += '\n';
    size_t count = 0;
    for (picIt = m_h264parser->pic_begin(); m_h264parser->pic_end() != picIt; ++picIt, ++count)
    {
        const H264::DPB &dpb = picIt->GetDPB();
        H264::Slice &frontSlice = picIt->slices_front();
        const H264Ctrl &ctrl = m_h264Ctrl[count];

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
            const OutSurfIDsCont &outPics = m_outputPics[picIt->GetPicIdx()];
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
        m_printPri,
        "Reference lists, DPB and output order of to be decoded pictures:\n%s",
        picListStr.c_str());
    picListStr = "";
    if (!m_lastOutPics.empty())
    {
        OutSurfIDsCont::const_iterator it;
        for (it = m_lastOutPics.begin(); m_lastOutPics.end() != it; ++it)
        {
            picListStr += StrPrintf("%3d", static_cast<int>(*it));
        }
    }
    else
    {
        picListStr = "none";
    }
    Printf(m_printPri, "Remaining output pictures: %s\n", picListStr.c_str());
}

RC ClB0B0Engine::CreateH264PushBuffer()
{
    RC rc;

    UINT32 frameOffset;
    UINT32 sliceOffsetsOffset;
    UINT32 ps;
    UINT32 colocBuf;
    UINT32 histBuf;
    UINT32 mbHistBuf;

    if (m_encryption)
    {
        frameOffset = *(m_h264EncMemLayout.frames_begin() + m_lwrPic);
        sliceOffsetsOffset = *(m_h264EncMemLayout.sliceofs_begin() + m_lwrPic);
        ps = *(m_h264EncMemLayout.pic_set_begin() + m_lwrPic);
        colocBuf = m_h264EncMemLayout.GetColocBufOffset();
        histBuf = m_h264EncMemLayout.GetHistBufOffset();
        mbHistBuf = m_h264EncMemLayout.GetMBHistBufOffset();
    }
    else
    {
        frameOffset = *(m_h264MemLayout.frames_begin() + m_lwrPic);
        sliceOffsetsOffset = *(m_h264MemLayout.sliceofs_begin() + m_lwrPic);
        ps = *(m_h264MemLayout.pic_set_begin() + m_lwrPic);
        colocBuf = m_h264MemLayout.GetColocBufOffset();
        histBuf = m_h264MemLayout.GetHistBufOffset();
        mbHistBuf = m_h264MemLayout.GetMBHistBufOffset();
    }

    CHECK_RC(SetApplicationID(APPLICATION_ID_H264));
    CHECK_RC(SetControlParams(CODEC_TYPE_H264, false, false, false, 0));
    CHECK_RC(SetPictureIndex(m_lwrPic));
    CHECK_RC(SetInBufBaseOffset(m_inSurf, frameOffset));
    CHECK_RC(SetDrvPicSetupOffset(m_inSurf, ps));
    CHECK_RC(SetColocDataOffset(m_inSurf, colocBuf));
    CHECK_RC(SetHistoryOffset(m_inSurf, histBuf));
    CHECK_RC(H264SetMBHistBufOffset(m_inSurf, mbHistBuf));
    CHECK_RC(SetSliceOffsetsBufOffset(m_inSurf, sliceOffsetsOffset));
    OutSurfCont::const_iterator osIt;
    size_t surfCount = 0;
    for (osIt = m_outSurfsY.begin(); m_outSurfsY.end() != osIt; ++osIt)
    {
        CHECK_RC(SetPictureLumaOffset(surfCount++, *osIt));
    }
    surfCount = 0;
    for (osIt = m_outSurfsUV.begin(); m_outSurfsUV.end() != osIt; ++osIt)
    {
        CHECK_RC(SetPictureChromaOffset(surfCount++, *osIt));
    }
    CHECK_RC(SetHistogramOffset(m_histogramSurf));

    return rc;
}

ClB0B0Engine::H264Ctrl::H264Ctrl()
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

void ClB0B0Engine::H264Ctrl::UpdateFromPicture(const H264::Picture &pic)
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

void ClB0B0Engine::H264Ctrl::EnableEncryption(unsigned int keyIncrement, const UINT32 sessionKey[4],
                                              const UINT32 contentKey[4], const UINT32 initVector[4])
{
    m_ps.encryption_params.enable_encryption = 1;
    m_ps.encryption_params.key_increment = keyIncrement;
    m_ps.encryption_params.encryption_mode = 0;
    memcpy(&m_ps.encryption_params.wrapped_session_key[0], &sessionKey[0], 16);
    memcpy(&m_ps.encryption_params.wrapped_content_key[0], &contentKey[0], 16);
    memcpy(&m_ps.encryption_params.initialization_vector[0], &initVector[0], 16);
}

void const * ClB0B0Engine::H264Ctrl::GetPicSetup() const
{
    return &m_ps;
}

size_t ClB0B0Engine::H264Ctrl::GetPicSetupSize() const
{
    return sizeof(m_ps);
}

size_t ClB0B0Engine::H264Ctrl::GetMaxDPBSize() const
{
    return NUMELEMS(m_ps.dpb);
}

unsigned int ClB0B0Engine::H264Ctrl::GetLwrrPicIdx() const
{
    return m_ps.LwrrPicIdx;
}

unsigned int ClB0B0Engine::H264Ctrl::GetLwrrColIdx() const
{
    return m_ps.LwrrColIdx;
}

unsigned int ClB0B0Engine::H264Ctrl::GetDPBPicIdx(size_t idx) const
{
    return m_ps.dpb[idx].index;
}

unsigned int ClB0B0Engine::H264Ctrl::GetDPBColIdx(size_t idx) const
{
    return m_ps.dpb[idx].col_idx;
}

bool ClB0B0Engine::H264Ctrl::IsDPBEntryUsed(size_t idx) const
{
    return 0 != m_ps.dpb[idx].state;
}

void ClB0B0Engine::H264Ctrl::SetLwrrPicIdx(unsigned int val)
{
    m_ps.LwrrPicIdx = val;
    m_ps.LwrrColIdx = val;
}

void ClB0B0Engine::H264Ctrl::UpdateDPB(const H264::Picture &pic, size_t whereIdx,
                                       H264::DPB::const_iterator fromWhat, unsigned int lwdecRefIdx,
                                       MemLayout memLayout)
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

void ClB0B0Engine::H264Ctrl::SetOutputMemoryLayout(MemLayout memLayout)
{
    m_ps.output_memory_layout = MemLayout::LW12 == memLayout ? 0 : 1;
}

void ClB0B0Engine::H264Ctrl::SetHistBufferSize(unsigned int size)
{
    m_ps.HistBufferSize = size;
}

void ClB0B0Engine::H264Ctrl::EnableHistogram(short startX, short startY, short endX, short endY)
{
    m_ps.displayPara.enableHistogram = 1;
    m_ps.displayPara.HistogramStartX = startX;
    m_ps.displayPara.HistogramStartY = startY;
    m_ps.displayPara.HistogramEndX = endX;
    m_ps.displayPara.HistogramEndY = endY;
}

void ClB0B0Engine::H264Ctrl::SetPitchOffset(const Surface2D &surfY, const Surface2D &surfUV)
{
    m_ps.pitch_luma = surfY.GetPitch();
    m_ps.pitch_chroma = surfUV.GetPitch();
    m_ps.luma_bot_offset = 0;
    m_ps.chroma_bot_offset = 0;
    m_ps.luma_frame_offset = m_ps.luma_top_offset = 0;
    m_ps.chroma_frame_offset = m_ps.chroma_top_offset = 0;
}

void ClB0B0Engine::H264Ctrl::SetSlicesSize(unsigned int slicesSize)
{
    m_ps.stream_len = slicesSize;
}

unsigned int ClB0B0Engine::H264Ctrl::GetSlicesSize(void const *buf) const
{
    const lwdec_h264_pic_s *ps = reinterpret_cast<const lwdec_h264_pic_s *>(buf);

    return ps->stream_len;
}

void ClB0B0Engine::H264Ctrl::SetSlicesCount(unsigned int slicesCount)
{
    m_ps.slice_count = slicesCount;
}

void ClB0B0Engine::H264Ctrl::SetMBHistBufSize(unsigned int mbHistBufSize)
{
    m_ps.mbhist_buffer_size = mbHistBufSize;
}

void const * ClB0B0Engine::H264Ctrl::GetSliceOffsets() const
{
    return &m_sliceOffsets[0];
}

size_t ClB0B0Engine::H264Ctrl::GetSliceOffsetsSize() const
{
    return sizeof(m_sliceOffsets[0]) * m_sliceOffsets.size();
}

string ClB0B0Engine::H264Ctrl::PicSetupToText(void const *buf) const
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

std::string ClB0B0Engine::H264Ctrl::SliceOffsetsToText(void const *buf, size_t numSlices) const
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

