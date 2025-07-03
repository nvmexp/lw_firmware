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

class Channel;
class TegraVicHelper;

//--------------------------------------------------------------------
//! \brief Test for VIC engine on cheetah
//!
//! VIC (VIdeo Compositor) takes a series of video streams, called
//! "slots", and combines them into a single stream.  Each slot can be
//! configured with a different pixel format, denoising, motion
//! combination, deinterlacing, etc.
//!
//! The VIC engine handles each frame independantly.  For each frame,
//! SW must write methods to pass the following information to the VIC
//! engine:
//! - At least one image for each slot, containing the current field
//!   in that slot.  Some algorithms also require the surrounding
//!   fields and/or other image buffers, for deinterlacing and such.
//!   Each image consists of 1-3 surfaces containing the
//!   luma/chroma/rgb/whatever components.
//! - A surface containing a "config struct", that has most of the
//!   information VIC needs to composit the frame.
//! - The surface(s) in which the dst image will be written into.
//!   This contains one frame of the output stream.
//! For more details, see:
//! https://p4viewer.lwpu.com/get///hw/ar/doc/vic/vic3.0/arch/VIC_IAS.doc
//! https://p4viewer.lwpu.com/get///hw/ar/doc/vic/vic3.0/www/class/VIC.VIC.html
//! https://p4viewer.lwpu.com/get///hw/ar/doc/vic/vic4.0/arch/VIC4_IAS.docx
//! https://p4viewer.lwpu.com/get///hw/ar/doc/vic/vic4.0/class/www/class/VIC.VIC.html
//! https://p4viewer.lwpu.com/get///hw/doc/mmplex/vic/4.2/class/www//class/VIC.VIC.html
//!
//! This test runs a loop in which each loop composits a single random
//! frame.  The different VIC versions (VIC3 and VIC4) mostly work the
//! same, but the VIC methods and config structs are radically
//! different.  So this test offloads most of the work into a
//! TegraVicHelper class, which is subclassed for each VIC version.
//!
class TegraVic : public GpuTest
{
public:
    enum Version
    {
        VERSION_VIC3,
        VERSION_VIC4,
        VERSION_VIC41,
        VERSION_VIC42,
        NUM_VERSIONS
    };

    TegraVic();
    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC SetDefaultsForPicker(UINT32 idx);
    virtual void PrintJsProperties(Tee::Priority pri);
    static const char *GetVersionName(Version version);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(DumpFiles, UINT32);
    SETGET_PROP(EnableFlipX, UINT08);
    SETGET_PROP(EnableFlipY, UINT08);
    SETGET_PROP(EnableCadenceDetection, UINT08);
    SETGET_PROP(EnableNoiseReduction, UINT08);
    SETGET_PROP(FrameFormat, UINT32);
    SETGET_PROP(InputPixelFormat, UINT32);
    SETGET_PROP(OutputPixelFormat, UINT32);
    SETGET_PROP(InputBlockKind, UINT08);
    SETGET_PROP(OutputBlockKind, UINT08);
    SETGET_PROP(DeInterlaceMode, UINT08);
    SETGET_PROP_LWSTOM(BackCompatibleMode, string);

private:
    UINT32 GetFirstSupportedClass();
    RC ValidateTestArgs();
    static RC ValidateBlockKind(const UINT08 blockKind);
    static RC ValidatePixelFormat(const UINT32 pixelFormat);
    friend class TegraVicHelper;

    static const UINT32 s_SupportedClasses[];
    unique_ptr<TegraVicHelper> m_pHelper;
    bool m_KeepRunning = false;
    enum DumpFilesValues {DUMP_TEXT = 0x1, DUMP_BINARY = 0x2};
    UINT32 m_DumpFiles;
    bool m_IsBackCompatible;
    Version m_FirstBackCompatibleVersion;
    Version m_LastBackCompatibleVersion;
    UINT08 m_EnableFlipX = _UINT08_MAX;
    UINT08 m_EnableFlipY = _UINT08_MAX;
    UINT08 m_EnableCadenceDetection = _UINT08_MAX;
    UINT08 m_EnableNoiseReduction = _UINT08_MAX;
    UINT32 m_FrameFormat = _UINT32_MAX;
    UINT32 m_InputPixelFormat = _UINT32_MAX;
    UINT32 m_OutputPixelFormat = _UINT32_MAX;
    UINT08 m_InputBlockKind = _UINT08_MAX;
    UINT08 m_OutputBlockKind = _UINT08_MAX;
    UINT08 m_DeInterlaceMode = _UINT08_MAX;
};

//--------------------------------------------------------------------
//! \brief Helper class for TegraVic test
//!
//! This class does most of the work of the TegraVic test.  It has subclasses
//! for the different VIC versions (e.g. VIC3 & VIC4).  For the most
//! part, the subclasses only deal with the version-specific config
//! structs and channel methods.  Almost everything else, such as
//! choosing the loop parameters via fancyPicker, is common to all VIC
//! versions and handled by the base class.
//!
//! The base class frequently uses Vic3 enums, since almost all of
//! them are common to all VIC versions.
//!
//! Notes:
//!
//! The VIC documentation uses the term "surface" to refer to the
//! combined luma/chroma planes that make up an image, but in MODS,
//! such a "surface" may consist of up to three Surface2D surfaces.
//! To minimize confusion, this class uses the term "Image" to refer
//! to the combined luma/chroma planes.
//!
//! Another source of confusion is that VIC optionally transposes the
//! dst image, flipping it along the diagonal and swapping the width &
//! height.  The m_Transpose fancyPick controls this behavior.  VIC
//! usually pretends that we're writing to an untransposed image that
//! gets flipped at the last second, and this class follows that
//! convention.  So unless otherwise specified, all rectangle
//! coordinates and width/height dimensions for the dst image apply
//! *before* the optional transpose.
//!
class TegraVicHelper
{
public:
    TegraVicHelper(TegraVic *pTest);
    virtual ~TegraVicHelper() {}
    RC Setup();
    RC Run();
    RC Cleanup();

protected:
    //! This struct contains enough information (width, height, etc)
    //! to allocate the surface planes of an image
    struct ImageDimensions
    {
        UINT32 width;        //!< Width of the image, in pixels
        UINT32 fieldHeight;  //!< Height of one field of the image
        UINT32 frameHeight;  //!< Height of one frame;
                             //!< same as fieldHeight if progressive
        Vic3::PIXELFORMAT pixelFormat;
        Vic3::BLK_KIND blockKind;
        UINT32 widthGranularity;      //!<width must be a multiple of this
        UINT32 frameHeightGranularity;//!<frameHeight must be a multiple of this
        UINT32 lumaPitch;
        UINT32 lumaWidth;
        UINT32 lumaHeight;
        UINT32 chromaPitch;
        UINT32 chromaWidth;
        UINT32 chromaHeight;
        Vic3::ChromaLoc chromaLocHoriz;
        Vic3::ChromaLoc chromaLocVert;
        ColorUtils::Format lumaFormat;
        ColorUtils::Format chromaUFormat;
        ColorUtils::Format chromaVFormat;
        bool hasAlpha;  //!< True if one of the surfaces has an alpha component
    };

    //! This struct contains the Surface2D surfaces that make up an image
    struct Image
    {
        ~Image() { Free(); }
        RC Fill(UINT32 value);
        RC FillRandom(UINT32 seed);
        RC Write(Channel *pCh, UINT32 lumaMethod,
                 UINT32 chromaUMethod, UINT32 chromaVMethod) const;
        void Free();
        Surface2D luma;
        Surface2D chromaU;
        Surface2D chromaV;
    };

    //! This struct contains all the images for one slot
    struct SrcImagesForSlot
    {
        ~SrcImagesForSlot();
        ImageDimensions dimensions;
        typedef map<UINT32, Image*> ImageMap;
        ImageMap images;          //!< Map of Vic*::SurfaceIndex enum to image
    };
    typedef vector<SrcImagesForSlot> SrcImages;

    //! Rectangle of pixels in an image.  The bounds are inclusive.
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

    //! All values chosen by fancyPickers for one slot.
    struct SlotPicks
    {
        bool IsProgressive() const;
        bool IsInterlaced() const { return !IsProgressive(); }
        bool IsFrameBased() const;
        bool IsFieldBased() const { return !IsFrameBased(); }
        bool HasMotionMap() const;
        Vic3::DXVAHD_FRAME_FORMAT frameFormat;
        Vic3::DXVAHD_DEINTERLACE_MODE_PRIVATE deinterlaceMode;
        Vic3::PIXELFORMAT pixelFormat;
        Vic3::BLK_KIND blockKind;
        UINT32 imageWidth;
        UINT32 imageHeight;
        bool deNoise;
        bool cadenceDetect;
        Rect srcRect;
        Rect dstRect;
        UINT32 lightLevel;
        UINT32 deGammaMode;
        UINT32 gamutMatrix[3][4];
    };

    // Methods, dolwmented in tegravic.cpp
    //
    virtual TegraVic::Version GetVersion() const = 0;
    static UINT32 GetMaxSlotsForVersion(TegraVic::Version version);
    UINT32 GetMaxSlots() const
        { return GetMaxSlotsForVersion(GetVersion()); }
    UINT32 GetEffMaxSlots() const;
    static ImageDimensions GetImageDimensions(
            UINT32 width, UINT32 frameHeight, bool isInterlaced,
            Vic3::PIXELFORMAT pixelFormat, Vic3::BLK_KIND blockKind);
    static UINT32 IntToFxp(UINT32 num) { return num << 16; }
    static UINT32 FxpToIntFloor(UINT32 fxp) { return fxp >> 16; }
    static UINT32 FxpToIntCeil(UINT32 fxp) { return (fxp + 0xffff) >> 16; }
    static void FprintfForCheckConfig(FILE*, const char *format, ...);
    static void PrintfForCheckConfig(const char *format, ...);
    RC AllocDataSurface(Surface2D *pSurface,
                        const void *pData, size_t size) const;
    RC AllocDataSurface(Surface2D *pSurface, const string &filename,
                        vector<UINT08> *pBuffer) const;
    RC AllocImage(Image *pImage, const ImageDimensions &dims) const;
    RC PickLoopParams();
    RC ApplyConstraints();
    size_t GetNumSlots() const { return m_SlotPicks.size(); }
    RC AllocSrcImages(SrcImages *pSrcImages);
    RC AllocDstImage(Image *pDstImage);
    virtual RC FillConfigStruct(
            vector<UINT08> *pCfgBuffer,
            const SrcImages &srcImages) const = 0;
    virtual bool CheckConfigStruct(const void *pConfigStruct) const = 0;
    virtual RC DumpConfigStruct(FILE *pFile,
                                const void *pConfigStruct) const = 0;
    RC DumpSrcFilesIfNeeded(const vector<UINT08> &cfgBuffer,
                            const SrcImages &srcImages) const;
    virtual RC WriteMethods(const void *pConfigStruct,
                            const SrcImages &srcImages,
                            const Image &dstImage) = 0;
    RC DumpDstFilesIfNeeded(const Image &dstImage) const;
    static RC DumpSurfaceFile(const string &filename, const Surface2D &surface);
    static RC PrintVicEngineState(Tee::Priority pri);

    // Fields derived from TegraVic wrapper object
    //
    TegraVic *const m_pTest;
    LwRm *const m_pLwRm;
    GpuDevice *const m_pGpuDevice;
    GpuSubdevice *const m_pGpuSubdevice;
    GpuTestConfiguration *const m_pTestConfig;
    Goldelwalues *const m_pGoldelwalues;
    FancyPicker::FpContext *const m_pFpCtx;
    const bool m_IsBackCompatible;
    const TegraVic::Version m_FirstBackCompatibleVersion;
    const TegraVic::Version m_LastBackCompatibleVersion;
    const Tee::Priority m_Pri;

    // Channels & surfaces
    //
    Channel *m_pCh;
    TegraGoldenSurfaces m_GoldenSurfaces;

    // Values chosen by fancypicker
    //
    Vic3::PIXELFORMAT m_DstPixelFormat;
    Vic3::BLK_KIND m_DstBlockKind;
    bool m_FlipX;
    bool m_FlipY;
    bool m_Transpose;
    UINT32 m_DstImageWidth;
    UINT32 m_DstImageHeight;
    UINT32 m_PercentSlots;
    vector<SlotPicks> m_SlotPicks;

    // Dimensions of the dst image.
    //
    // Unlike all the other dimensions in this class, this one
    // contains the dimensions *after* the optional transpose
    // specified by m_Transpose.  That's because the rules for padding
    // the width/height of pitch-linear/block-linear data apply to the
    // actual surface dimensions, not the imaginary untransposed image
    // that the rest of VIC pretends we're we're writing.
    //
    ImageDimensions m_TrDstDimensions;

    // Misc
    //
    static const UINT64 ALL_ONES;     //!< Fills a ConfigStruct bitfield with 1
    static const UINT64 IGNORED;      //!< Indicates a ConfigStruct bitfield
                                      //!< that's zeroed-out merely because
                                      //!< we're not doing anything with it yet.
private:
    enum PickOption
    {
        random,
        enable,
        disable
    };

    static RC PickBoolParams
    (
        const UINT32 pickerParam,
        PickOption option,
        FancyPickerArray &fpk,
        bool *pParam
    );
};

//--------------------------------------------------------------------
//! \brief Subclass of TegraVicHelper for VIC4
//!
class TegraVic4Helper : public TegraVicHelper
{
public:
    TegraVic4Helper(TegraVic *pTest) : TegraVicHelper(pTest) {}
protected:
    virtual TegraVic::Version GetVersion() const
        { return TegraVic::VERSION_VIC4; }
    virtual RC FillConfigStruct(vector<UINT08> *pCfgBuffer,
                                const SrcImages &srcImages) const;
    virtual bool CheckConfigStruct(const void *pConfigStruct) const;
    virtual RC DumpConfigStruct(FILE *pFile, const void *pConfigStruct) const;
    virtual RC WriteMethods(const void *pConfigStruct,
                            const SrcImages &srcImages,
                            const Image &dstImage);
    virtual size_t GetConfigSize() const { return sizeof(Vic4::ConfigStruct); }
    virtual UINT32 GetLumaMethod(UINT32 slotIdx, UINT32 surfaceIdx) const;
    virtual UINT32 GetChromaUMethod(UINT32 slotIdx, UINT32 surfaceIdx) const;
    virtual UINT32 GetChromaVMethod(UINT32 slotIdx, UINT32 surfaceIdx) const;
private:
    void FillSlotStruct(Vic4::ConfigStruct *pCfg,
                        const SrcImages &srcImages,
                        UINT32 slotIdx)const;
    static void DumpMatrixStruct(FILE *pFile, const string &prefix,
                                 const Vic4::MatrixStruct &matrix);
    static void DumpSlotStruct(FILE *pFile, const string &prefix,
                               const Vic4::SlotStruct &slotStruct);
};

//--------------------------------------------------------------------
//! \brief Subclass of TegraVicHelper for VIC4.1
//!
class TegraVic41Helper : public TegraVic4Helper
{
public:
    TegraVic41Helper(TegraVic *pTest) : TegraVic4Helper(pTest) {}
protected:
    virtual TegraVic::Version GetVersion() const
        { return TegraVic::VERSION_VIC41; }
    virtual bool CheckConfigStruct(const void *pConfigStruct) const;
    virtual size_t GetConfigSize() const { return sizeof(Vic41::ConfigStruct); }
    virtual UINT32 GetLumaMethod(UINT32 slotIdx, UINT32 surfaceIdx) const;
    virtual UINT32 GetChromaUMethod(UINT32 slotIdx, UINT32 surfaceIdx) const;
    virtual UINT32 GetChromaVMethod(UINT32 slotIdx, UINT32 surfaceIdx) const;
};

//--------------------------------------------------------------------
//! \brief Subclass of TegraVicHelper for VIC4.2
//!
class TegraVic42Helper : public TegraVic41Helper
{
public:
    TegraVic42Helper(TegraVic *pTest) : TegraVic41Helper(pTest) {}
protected:
    virtual TegraVic::Version GetVersion() const
        { return TegraVic::VERSION_VIC42; }
    virtual bool CheckConfigStruct(const void *pConfigStruct) const;
    virtual size_t GetConfigSize() const { return sizeof(Vic42::ConfigStruct); }
    virtual UINT32 GetLumaMethod(UINT32 slotIdx, UINT32 surfaceIdx) const;
    virtual UINT32 GetChromaUMethod(UINT32 slotIdx, UINT32 surfaceIdx) const;
    virtual UINT32 GetChromaVMethod(UINT32 slotIdx, UINT32 surfaceIdx) const;
};
