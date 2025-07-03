/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GPUTEST_H
#include "gputest.h"
#endif
#ifndef INCLUDED_SURF2D_H
#include "gpu/utility/surf2d.h"
#endif
#ifndef INCLUDED_TEGRA_GOLD_SURF_H
#include "cheetah/include/tegragoldsurf.h"
#endif
#ifndef INCLUDED_VIC_API_H
#include "cheetah/dev/vic/vic_api.h"
#endif
#include <cstdio>

namespace
{
    // These are copied from //hw/gpu_ip/unit/vic/4.2/cmod/geotran/geoDriver.cpp
    const UINT32 TNR3LEFTCOLTILESIZE = 14*256;
    const UINT32 TNR3ABOVEROWTILESIZE = 2*64;
    const UINT32 LW_VIC_TNR3_BF_RANGE_LUMA_TABLE_SIZE = 64;
    const UINT32 LW_VIC_TNR3_BF_RANGE_CHROMA_TABLE_SIZE = 64;
};

class Channel;
class VicLdcTnrHelper;

//!--------------------------------------------------------------------
//! \brief Test for VIC 4.2 engine on cheetah
//!
//! Leagacy VIC (VIdeo Compositor) takes a series of video streams, called
//! "slots", and combines them into a single stream.  Each slot can be
//! configured with a different pixel format, denoising, motion
//! combination, deinterlacing, etc.
//!
//! In VIC 4.2 engine two new features have been added which deviates from the
//! legacy video compositor.
//! The features are Lens Distortion Correction (LDC) and Temporal Noise Reduction 3
//! These two features has its own constraints like it doesn't support RGB, it supports
//! only one slot, the configuration struct is different from the legacy vic,
//! HW has a different pipeline.
//!
//! For more details, see:
//! IAS:  //hw/doc/mmplex/vic/4.2/arch/VIC4.2_IAS.docx
//! VAD: //hw/doc/mmplex/vic/4.2/verif/VIC t194 Verification Architecture Document.docx
//! XML:  /home/scratch.dechang_gpu/VIC/vic_xml_image/video/vic4p2/xml/
//! https://p4viewer.lwpu.com/get///hw/doc/mmplex/vic/4.2/class/www//class/VIC.VIC.html
//! TegraVic test
//! Most of the code is similar to one in //hw/gpu_ip/unit/vic/4.2/cmod/geotran/geoDriver.cpp
//!
class VicLdcTnr : public GpuTest
{
public:
    enum Version
    {
        VERSION_VIC42,
        NUM_VERSIONS
    };

    VicLdcTnr();
    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC SetDefaultsForPicker(UINT32 idx);
    virtual void PrintJsProperties(Tee::Priority pri);
    static const char *GetVersionName(Version version);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(DumpFiles, UINT32);
    SETGET_PROP(TestSampleTNR3Image, bool);
    SETGET_PROP(TestSampleLDCImage, bool);

private:
    UINT32 GetFirstSupportedClass();
    friend class VicLdcTnrHelper;

    static const UINT32 s_SupportedClasses[];
    unique_ptr<VicLdcTnrHelper> m_pHelper;
    bool m_KeepRunning = false;
    enum DumpFilesValues
    {
        DUMP_TEXT = 0x1
        ,DUMP_BINARY = 0x2
        ,DUMP_PNG = 0x4
    };
    UINT32 m_DumpFiles;
    bool m_TestSampleTNR3Image;
    bool m_TestSampleLDCImage;
};

//!--------------------------------------------------------------------
//! \brief Helper class for VicLDCTNR test
//!
//! This class does most of the work of the VicLDCTNR test. It can have subclasses
//! for the different VIC versions (e.g. Vic42). For the most
//! part, the subclasses only deal with the version-specific config
//! structs and channel methods.  Almost everything else, such as
//! choosing the loop parameters via fancyPicker, is common to all VIC
//! versions and handled by the base class.
//!
//!
class VicLdcTnrHelper
{
public:
    VicLdcTnrHelper(VicLdcTnr *pTest);
    virtual ~VicLdcTnrHelper() {}
    RC Setup();
    RC Run();
    RC Cleanup();

protected:
    // This struct contains enough information (width, height, etc)
    // to allocate the surface planes of an image
    struct ImageDimensions
    {
        UINT32 width;        // Width of the image, in pixels
        UINT32 fieldHeight;  // Height of one field of the image
        UINT32 frameHeight;  // Height of one frame;
        Vic42::PIXELFORMAT pixelFormat;
        Vic42::BLK_KIND blockKind;
        UINT32 widthGranularity;      //!<width must be a multiple of this
        UINT32 frameHeightGranularity;//!<frameHeight must be a multiple of this
        UINT32 lumaPitch;
        UINT32 lumaWidth;
        UINT32 lumaHeight;
        UINT32 chromaPitch;
        UINT32 chromaWidth;
        UINT32 chromaHeight;
        ColorUtils::Format lumaFormat;
        ColorUtils::Format chromaUFormat;
        ColorUtils::Format chromaVFormat;
    };

    // This struct contains the Surface2D surfaces that make up an image
    struct Image
    {
        RC Fill(UINT32 value);
        RC FillRandom(UINT32 seed);
        RC FillFromFile(const string &fileName);
        RC Write(Channel *pCh, UINT32 lumaMethod,
                 UINT32 chromaUMethod, UINT32 chromaVMethod) const;
        Surface2D* pLuma;
        Surface2D* pChromaU;
        Surface2D* pChromaV;
    };

    // This struct contains all the input images
    struct SrcImages
    {
        ImageDimensions dimensions;
        typedef map<UINT32, unique_ptr<Image>> ImageMap;
        ImageMap images;          // Map of Vic42::SurfaceIndex enum to image
    };

    // This struct contains the output Image
    struct DstImage
    {
        ImageDimensions dimensions;
        unique_ptr<Image> image;
    };

    // Rectangle of pixels in an image. The bounds are inclusive.
    struct Rect
    {
        void SetWidth(UINT32 newWidth, UINT32 imageWidth);
        void SetHeight(UINT32 newHeight, UINT32 imageHeight);
        UINT32 GetWidth() const { return right - left + 1; }
        UINT32 GetHeight() const { return bottom - top + 1; }
        UINT32 left;
        UINT32 right;
        UINT32 top;
        UINT32 bottom;
    };

    // This struct contains the values for TNR3 input
    struct TNR3Params
    {
        UINT08 tnr3En;
        UINT08 betaBlendingEn;
        UINT08 alphaBlendingEn;
        UINT08 alphaSmoothEn;
        UINT08 tempAlphaRestrictEn;
        UINT08 alphaClipEn;
        UINT08 bFRangeEn;
        UINT08 bFDomainEn;
        UINT32 SADMultiplier;
        UINT32 SADWeightLuma;
        UINT32 TempAlphaRestrictIncCap;
        UINT32 AlphaScaleIIR;
        UINT32 AlphaClipMaxLuma;
        UINT32 AlphaClipMinLuma;
        UINT32 AlphaClipMaxChroma;
        UINT32 AlphaClipMinChroma;
        UINT32 BetaCalcMaxBeta;
        UINT32 BetaCalcMinBeta;
        UINT32 BetaCalcBetaX1;
        UINT32 BetaCalcBetaX2;
        UINT32 rangeSigmaLuma;
        UINT32 rangeSigmaChroma;
        UINT32 domainSigmaLuma;
        UINT32 domainSigmaChroma;
    };

    // This struct contains the values for LDC input
    struct LDCParams
    {
        UINT08 geoTranEn;
        UINT32 sparseWarpMapWidth;
        UINT32 sparseWarpMapHeight;
        UINT32 sparseWarpMapStride;
    };

    // All values chosen by fancyPickers for input.
    struct InputParams
    {
        bool IsProgressive() const;
        bool IsInterlaced() const { return !IsProgressive(); }
        bool IsFrameBased() const;
        bool IsFieldBased() const { return !IsFrameBased(); }
        Vic42::DXVAHD_FRAME_FORMAT frameFormat;
        Vic42::PIXELFORMAT pixelFormat;
        Vic42::FILTER_TYPE pixelFilterType;
        Vic42::BLK_KIND blockKind;
        UINT32 imageWidth;
        UINT32 imageHeight;
        Rect srcRect;
        TNR3Params tnr3Params;
        LDCParams ldcParams;
    };

    // All values chosen by fancypickers for output.
    struct OutputParams
    {
        Vic42::PIXELFORMAT pixelFormat;
        Vic42::BLK_KIND blockKind;
        UINT32 imageWidth;
        UINT32 imageHeight;
        Rect dstRect;
    };

    enum VicSurfaces
    {
        lwrrentFieldLuma    = 0
        ,lwrrentFieldChroma = 1
        ,bottomFieldLuma    = 2   // If the frame is LW24, the VIC app uses an additional surface
                                  // to keep noise reduced field of the current picture
        ,bottomFieldChroma  = 3
        ,maxSurfaces        = 4
    };

    virtual VicLdcTnr::Version GetVersion() const = 0;
    static ImageDimensions GetImageDimensions
    (
        UINT32 width
        ,UINT32 frameHeight
        ,bool isInterlaced
        ,Vic42::PIXELFORMAT pixelFormat
        ,Vic42::BLK_KIND blockKind
    );
    RC AllocDataSurface
    (
        Surface2D *pSurface
        ,const void *pData
        ,size_t size
        ,bool isFillRandom
    ) const;
    RC AllocDataSurface
    (
        Surface2D *pSurface
        ,const string &filename
        ,vector<UINT08> *pBuffer
    ) const;
    RC AllocDataSurface
    (
        Surface2D *pSurface
        ,size_t size
        ,const string &filename
    ) const;
    RC FillImageParams
    (
        unique_ptr<Image> &pImage
        ,const ImageDimensions &dims
    ) const;
    RC PickLoopParams();
    virtual RC ApplyConstraints();
    virtual RC FillSrcImages(SrcImages *pSrcImages, DstImage *pDstImage);
    virtual RC FillDstImage(DstImage *pDstImage);
    virtual RC FillConfigStruct
    (
        vector<UINT08> *pCfgBuffer
        ,const SrcImages &srcImages
        ,const DstImage &dstImage
    ) const = 0;
    virtual bool CheckConfigStruct(const void *pConfigStruct) const = 0;
    virtual RC DumpConfigStruct
    (
        FILE *pFile
        ,const void *pConfigStruct
    ) const = 0;
    RC DumpSrcFilesIfNeeded
    (
        const vector<UINT08> &cfgBuffer
        ,const SrcImages &srcImages
    ) const;
    virtual RC WriteMethods
    (
        const void *pConfigStruct
        ,const SrcImages &srcImages
        ,const unique_ptr<Image> &dstImage
    ) = 0;
    RC DumpDstFilesIfNeeded(unique_ptr<Image> &dstImage) const;
    static RC DumpSurfaceFile(const string &filename, const Surface2D *surface);
    static RC PrintVicEngineState(Tee::Priority pri);
    RC DumpPng(const string &fileName, Surface2D *surface) const;

    unsigned int F32ToU32(float in_f32) const;
    void PrintConfigValues(vector<UINT08> *pCfgBuffer)const;

    RC CalcBufferPadding
    (
        VicLdcTnrHelper::ImageDimensions* dims
        ,UINT32 width
        ,UINT32 height
        ,Vic42::PIXELFORMAT pixelFormat
        ,bool isLW24
        ,Vic42::BLK_KIND blockKind
        ,UINT32 blkHeight
        ,UINT32 orgCacheWidth
        ,bool isInterlaced
        ,UINT32 align_height
    );
    RC GetFetchParams
    (
        Vic42::BLK_KIND blk_kind
        ,UINT32 blk_height
        ,UINT32 *fetch_width
        ,UINT32 *fetch_height
    );
    RC GetSurfParams
    (
        Vic42::PIXELFORMAT f
        ,bool isLW24
        ,UINT32 *loop_count
        ,UINT32 *x_shift
        ,UINT32 *dx
        ,UINT32 *dy
    ) const;

    //! Fields derived from VicLDCTNR wrapper object
    VicLdcTnr *const m_pTest;
    LwRm *const m_pLwRm;
    GpuDevice *const m_pGpuDevice;
    GpuSubdevice *const m_pGpuSubdevice;
    GpuTestConfiguration *const m_pTestConfig;
    Goldelwalues *const m_pGoldelwalues;
    FancyPicker::FpContext *const m_pFpCtx;
    const Tee::Priority m_Pri;

    //! Channels & surfaces
    Channel *m_pCh;
    TegraGoldenSurfaces m_GoldenSurfaces;

    //! Values chosen by fancypicker
    InputParams m_InputParams;
    OutputParams m_OutputParams;

    Surface2D m_CfgSurface;
    Surface2D m_CrcStruct;
    Surface2D m_Tnr3NeighBuffer;
    Surface2D m_Tnr3LwrrAlphaBuffer;
    Surface2D m_Tnr3PreAlphaBuffer;
    Surface2D m_LwrrentImageLuma;
    Surface2D m_LwrrentImageChromaU;
    Surface2D m_LwrrentImageChromaV;
    Surface2D m_PreviousImageLuma;
    Surface2D m_PreviousImageChromaU;
    Surface2D m_PreviousImageChromaV;
    Surface2D m_DestImageLuma;
    Surface2D m_DestImageChromaU;
    Surface2D m_DestImageChromaV;

    //! Misc
    static const UINT64 ALL_ONES;     //!< Fills a ConfigStruct bitfield with 1
    static const UINT64 IGNORED;      //!< Indicates a ConfigStruct bitfield
                                      //!< that's zeroed-out merely because
                                      //!< we're not doing anything with it yet.
    union m_u32_f32;
    static const UINT32 MAXPLANES;
};

//!--------------------------------------------------------------------
//! \brief Subclass of VicLDCTNRHelper for VIC4.2
//!
class VicLdcTnr42Helper : public VicLdcTnrHelper
{
public:
    VicLdcTnr42Helper(VicLdcTnr *pTest) : VicLdcTnrHelper(pTest) {}
protected:
    virtual VicLdcTnr::Version GetVersion() const
    {
        return VicLdcTnr::VERSION_VIC42;
    }
    virtual bool CheckConfigStruct(const void *pConfigStruct) const;
    virtual size_t GetConfigSize() const { return sizeof(Vic42::ConfigStruct); }
    virtual RC DumpConfigStruct(FILE *pFile, const void *pConfigStruct) const;
    virtual UINT32 GetLumaMethod(UINT32 slotIdx, UINT32 surfaceIdx) const;
    virtual UINT32 GetChromaUMethod(UINT32 slotIdx, UINT32 surfaceIdx) const;
    virtual UINT32 GetChromaVMethod(UINT32 slotIdx, UINT32 surfaceIdx) const;
    virtual RC FillConfigStruct
    (
        vector<UINT08> *pCfgBuffer
        ,const SrcImages &srcImages
        ,const DstImage &dstImage
    ) const;
    virtual RC WriteMethods
    (
        const void *pConfigStruct
        ,const SrcImages &srcImages
        ,const unique_ptr<Image> &dstImage
    );
    int GetBetaCalcStepBeta(int c, int clip_s, int clip_e) const;
    RC GetSurfaceParamsVicPipe
    (
        Vic42::PIXELFORMAT PixelFormat
        ,UINT32 *numPlanes
        ,UINT32 *numComp
    ) const;
};
