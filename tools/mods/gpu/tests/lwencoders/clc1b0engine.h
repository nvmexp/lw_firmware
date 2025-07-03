/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <map>
#include <memory>
#include <string>

#include <boost/container/vector.hpp>

#include "core/include/channel.h"
#include "ctr64encryptor.h"
#include "core/include/lwrm.h"
#include "core/include/golden.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/rc.h"
#include "gpu/utility/surf2d.h"
#include "core/include/tee.h"
#include "core/include/types.h"

#include "class/clc1b0.h"

#include "offsetsstorage.h"
#include "h264layouter.h"
#include "h265layouter.h"

#include "ienginewrapper.h"

class ClC1B0Engine
  : public LWDEC::Engine
  , public LWDEC::Saver
{
public:
    typedef tuple<
        Channel *
      , LwRm::Handle
      , Tee::Priority
      > ConstructorArgs;

    ClC1B0Engine() = delete;
    ClC1B0Engine(Channel *lwCh, LwRm::Handle hLwdec, Tee::Priority printPri);

    ~ClC1B0Engine();

private:
    enum SupportedCodec { h264, h265 };

    class H264Ctrl;
    class H265Ctrl;

    typedef vector<size_t>    OutSurfIDsCont;
    typedef vector<Surface2D> OutSurfCont;

    //! A map between H.264 parser picture indices and output surfaces. These
    //! pictures can be output before decoding the picture which index is the
    //! key of the map. Using this map provides the correct display order.
    typedef map<size_t, OutSurfIDsCont> OutputPicsMap;

    typedef H264Layouter<MemoryWriter, OffsetsStorage, NilEncryptor>   H264MemLayouter;
    typedef H264Layouter<MemoryWriter, OffsetsStorage, CTR64Encryptor> H264EncMemLayouter;
    typedef H265Layouter<MemoryWriter, OffsetsStorage> H265MemLayouter;

    // use boost's vector for type erasure
    typedef boost::container::vector<H264Ctrl> H264CtrlCont;
    typedef boost::container::vector<H265Ctrl> H265CtrlCont;

    static const size_t maxHistBins = 256;

    template <class H264Layouter>
    void BuildH264Layout(H264Layouter *layouter);

    template <class H265Layouter>
    void BuildH265Layout(H265Layouter *layouter);

    UINT32 CalcH264LayoutSize();
    UINT32 CalcH265LayoutSize();

    UINT32 GetH264SlicesAlignment() const
    {
        return 0x100;
    }

    UINT32 GetH264EncSlicesAlignment() const
    {
        return 0x100;
    }

    UINT32 GetH265SlicesAlignment() const
    {
        return 0x100;
    }

    UINT32 GetControlStructsAlignment() const
    {
        return 0x100;
    }

    auto GetH264Parser()
    {
        return m_h264parser;
    }

    auto GetH265Parser()
    {
        return m_h265parser;
    }

    auto GetInStreamFileName() const
    {
        return m_inStreamFileName;
    }

    RC InitH264(GpuDevice *dev, GpuSubdevice *subDev, LwRm::Handle chHandle);
    RC InitH265(GpuDevice *dev, GpuSubdevice *subDev, LwRm::Handle chHandle);

    RC DoInitFromH264File(const char *fileName, const TarFile *tarFile, GpuDevice *dev,
                          GpuSubdevice *subDev, LwRm::Handle chHandle) override;
    RC DoInitFromH265File(const char *fileName, const TarFile *tarFile, GpuDevice *dev,
                          GpuSubdevice *subDev, LwRm::Handle chHandle) override;
    RC DoInitFromAnother(GpuDevice *dev, GpuSubdevice *subDev, LwRm::Handle chHandle,
                         LWDEC::Engine &another) override;
    RC DoInitOutput(GpuDevice * dev, GpuSubdevice * subDev, LwRm::Handle chHandle,
                    FLOAT64 skTimeout) override;
    RC DoUpdateInDataGoldens(GpuGoldenSurfaces * goldSurfs, Goldelwalues * goldValues) override;
    RC DoUpdateOutPicsGoldens(GpuGoldenSurfaces *goldSurfs, Goldelwalues *goldValues) override;
    RC DoUpdateHistGoldens(GpuGoldenSurfaces * goldSurfs, Goldelwalues * goldValues) override;
    RC DoSaveFrames(const string &nameTemplate, UINT32 instNum) override;
    RC DoFree() override;
    size_t DoGetNumPics() const override;
    RC DoSetupContentKey(GpuSubdevice *subDev, FLOAT64 skTimeout) override;
    const UINT32* DoGetWrappedSessionKey() const override;
    const UINT32* DoGetWrappedContentKey() const override;
    const UINT32* DoGetContentKey() const override;
    const UINT32* DoGetInitVector() const override;
    RC            DoInitChannel() override;
    bool          DoGetEncryption() const override { return m_encryption; }
    void          DoSetEncryption(bool encrypt) override { m_encryption = encrypt; }
    RC DoDecodeOneFrame(FLOAT64 timeOut) override;
    void DoSetPrintPri(Tee::Priority val) override { m_printPri = val; }
    RC DoPrintInputData() override;
    RC DoPrintHistogram() override;
    string DoGetStreamGoldSuffix() const override { return m_inStreamGoldSuffix; }
    void DoSetStreamGoldSuffix(string val) override { m_inStreamGoldSuffix = val; }

    void DoSetWrappedSessionKey(const UINT32 sk[AES::DW]) override;
    void DoSetWrappedContentKey(const UINT32 wck[AES::DW]) override;
    void DoSetContentKey(const UINT32 ck[AES::DW]) override;
    void DoSetInitVector(const UINT32 iv[AES::DW]) override;
    UINT08 DoGetKeyIncrement() const override;
    void DoSetKeyIncrement(UINT08 val) override;

    Engine* DoClone() const override;

    enum ApplicationId
    {
        APPLICATION_ID_MPEG12       = LWC1B0_SET_APPLICATION_ID_ID_MPEG12,
        APPLICATION_ID_VC1          = LWC1B0_SET_APPLICATION_ID_ID_VC1,
        APPLICATION_ID_H264         = LWC1B0_SET_APPLICATION_ID_ID_H264,
        APPLICATION_ID_MPEG4        = LWC1B0_SET_APPLICATION_ID_ID_MPEG4,
        APPLICATION_ID_VP8          = LWC1B0_SET_APPLICATION_ID_ID_VP8,
        APPLICATION_ID_CTR64        = LWC1B0_SET_APPLICATION_ID_ID_CTR64,
        APPLICATION_ID_HEVC         = LWC1B0_SET_APPLICATION_ID_ID_HEVC,
        APPLICATION_ID_VP9          = LWC1B0_SET_APPLICATION_ID_ID_VP9,
        APPLICATION_ID_PASS1        = LWC1B0_SET_APPLICATION_ID_ID_PASS1,
        APPLICATION_ID_HEVC_PARSER  = LWC1B0_SET_APPLICATION_ID_ID_HEVC_PARSER
    };

    enum NotifyOn { notifyOnBeg, notifyOnEnd };

    enum CodecType
    {
        CODEC_TYPE_MPEG1 = LWC1B0_SET_CONTROL_PARAMS_CODEC_TYPE_MPEG1,
        CODEC_TYPE_MPEG2 = LWC1B0_SET_CONTROL_PARAMS_CODEC_TYPE_MPEG2,
        CODEC_TYPE_VC1   = LWC1B0_SET_CONTROL_PARAMS_CODEC_TYPE_VC1,
        CODEC_TYPE_H264  = LWC1B0_SET_CONTROL_PARAMS_CODEC_TYPE_H264,
        CODEC_TYPE_MPEG4 = LWC1B0_SET_CONTROL_PARAMS_CODEC_TYPE_MPEG4,
        CODEC_TYPE_DIVX3 = LWC1B0_SET_CONTROL_PARAMS_CODEC_TYPE_DIVX3,
        CODEC_TYPE_VP8   = LWC1B0_SET_CONTROL_PARAMS_CODEC_TYPE_VP8,
        CODEC_TYPE_HEVC  = LWC1B0_SET_CONTROL_PARAMS_CODEC_TYPE_HEVC,
        CODEC_TYPE_VP9   = LWC1B0_SET_CONTROL_PARAMS_CODEC_TYPE_VP9
    };

    RC SetApplicationID(ApplicationId id);
    RC SetControlParams(CodecType codecType, bool gpTimerOn, bool retErr, bool errConcealOn,
                        unsigned int errorFrmIdx, bool mbTimerOn);
    RC SetInBufBaseOffset(const Surface2D& surf, UINT32 offs);
    RC SetPictureIndex(UINT32 idx);
    RC SetDrvPicSetupOffset(const Surface2D& surf, UINT32 ofs);
    RC SetHistoryOffset(const Surface2D& surf, UINT32 ofs);
    RC SetColocDataOffset(const Surface2D& surf, UINT32 ofs);
    RC HEVCSetScalingListOffset(const Surface2D& surf, UINT32 ofs);
    RC HEVCSetFilterBufferOffset(const Surface2D& surf, UINT32 ofs);
    RC HEVCSetTileSizesOffset(const Surface2D& surf, UINT32 ofs);
    RC SetHistogramOffset(const Surface2D& surf);
    RC H264SetMBHistBufOffset(const Surface2D& surf, UINT32 ofs);
    RC SetPictureLumaOffset(size_t picIdx, const Surface2D& surf);
    RC SetPictureChromaOffset(size_t picIdx, const Surface2D& surf);
    RC SetSliceOffsetsBufOffset(const Surface2D& surf, UINT32 ofs);
    RC SemaphoreAB(const Surface2D& surf);
    RC SemaphoreC(UINT32 payload);
    RC Execute(bool notify, NotifyOn notifyOn, bool awaken);
    RC Flush();

    void UpdateControlStructsFromPics();
    RC   InitOutSurfaces(UINT32 width, UINT32 height, UINT32 numPics, GpuDevice *dev,
                         GpuSubdevice *subDev, LwRm::Handle chHandle);
    void BuildH264Picsetup();
    void BuildH265Picsetup();

    template <class Layouter>
    RC PrintH264InputData(const Layouter &layouter);
    RC PrintH265InputData(const H265MemLayouter &layouter);

    RC CreateH264PushBuffer();
    RC CreateH265PushBuffer();

    string                        m_inStreamFileName;
    string                        m_inStreamGoldSuffix;

    shared_ptr<H264::Parser>      m_h264parser;
    shared_ptr<H265::Parser>      m_h265parser;

    unsigned int                  m_lwrPic = 0;
    OutputPicsMap                 m_outputPics;
    OutSurfIDsCont                m_lastOutPics;

    H264MemLayouter               m_h264MemLayout;
    H264EncMemLayouter            m_h264EncMemLayout;
    H265MemLayouter               m_h265MemLayout;

    UINT32                        m_goldOutSurfLoopCnt = 0;
    UINT32                        m_goldHistSurfLoopCnt = 0;
    UINT32                        m_outFramesCount = 0;

    Surface2D                     m_inSurf;
    Surface2D                     m_histogramSurf;
    OutSurfCont                   m_outSurfsY;
    OutSurfCont                   m_outSurfsUV;

    H264CtrlCont                  m_h264Ctrl;
    H265CtrlCont                  m_h265Ctrl;

    UINT32                        m_h264HistBufSize = 0;
    UINT32                        m_h264ColocBufSize = 0;
    UINT32                        m_h264MbHistBufSize = 0;
    UINT32                        m_h265ColMvBufSize = 0;
    UINT32                        m_totFilterBufferSize = 0;
    UINT32                        m_saoBufferOffset = 0;
    UINT32                        m_bsdBufferOffset = 0;
    UINT32                        m_filterAboveBufferOffset = 0;
    UINT32                        m_saoAboveBufferOffset = 0;

    LwRm::Handle                  m_lwdecHandle;

    bool                          m_encryption = false;
    OutputSize                    m_outputSize = OutputSize::_8bits;
    Tee::Priority                 m_printPri;

    SupportedCodec                m_initedTo = h264;

    UINT32                        m_wrappedSessionKey[AES::DW];
    UINT32                        m_wrappedContentKey[AES::DW];
    UINT32                        m_contentKey[AES::DW];
    UINT32                        m_initVector[AES::DW];
    UINT08                        m_keyIncrement = 1;

    Channel                      *m_lwCh;
};
