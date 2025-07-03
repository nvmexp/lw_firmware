/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * \file  surffill.h
 * \brief Utilities for filling surfaces.
 *
 * EXAMPLES:
 *     // Fill a surface with an 8-bit value:
 *     CHECK_RC(SurfaceUtils::FillSurface08(pSurface, 0xff));
 *
 *     // Fill a surface with a 32-bit little-endian value:
 *     CHECK_RC(SurfaceUtils::FillSurface32(pSurface, 0x12345678));
 *
 *     // Fill several surfaces, in a way that will minimize the
 *     // setup & wait times:
 *     OptimalSurfaceFiller filler(pGpuDevice);
 *     filler.SetAutoWait(false);
 *     CHECK_RC(filler.Fill08(pSurface1, 0x00));
 *     CHECK_RC(filler.Fill32(pSurface2, 0x11223344));
 *     ...
 *     CHECK_RC(filler.Wait());
 *
 * CLASSES:
 *     This file has an ISurfaceFiller base class with four subclasses:
 *     - ThreeDSurfaceFiller:  Uses the 3D engine, with ZBC if possible.
 *     - CopyEngineSurfaceFiller: Uses the copy engine to fill the surface.
 *     - MappingSurfaceFiller: Uses memory-mapping to fill the surface.
 *     - OptimalSurfaceFiller: Uses one of the other three subclasses
 *         to fill the surface, choosing whichever one is supported on
 *         the current platform and likely to be fastest.  This is
 *         usually the preferred API.
 *
 * NOTES:
 *     - On SLI, these utilities do a broadcast-write unless you
 *       specify a target subdevice.
 *     - For compatibility with MappingSurfaceReader/Writer, these
 *       utilities don't fill fill any Hidden or Extra data at the
 *       start of the Surface2D (see surf2d.cpp).  It just fills
 *       the ArrayPitch*ArraySize region.
 *     - If you specify an offset & size, these utilities fill the
 *       surface in memory order, even if the surface is block-linear.
 *       The 3D subclass forces fills to be GOB-aligned to simplify
 *       this requirement.
 */

#pragma once

#ifndef INCLUDED_SURFFILL_H
#define INCLUDED_SURFFILL_H

#include <memory>
#include "core/include/display.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/include/dmawrap.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/nfysema.h"
#include "gpu/tests/gputestc.h"
#include "gpu/tests/gputest.h"
#include "gpu/utility/gpuutils.h"
#include "surfrdwr.h"

class Channel;
class GpuDevice;
class GpuSubdevice;
class Surface2D;

/**
 *  \addtogroup SurfaceUtils
 *  @{
 */
namespace SurfaceUtils
{
/**
 *  \addtogroup FramePatterns
 *  @{
 */
namespace FramePatterns
{
/*
     *
     * Elements of Raster:
     * The basic entity is a pixel. The pixel is of type ColorFormat. Sizeof pixel = B.p.p(ColorFormat)
     * A Texture is a 2-d Chunk of pixels. sizeof Texture = (TexWd * TexHt pixels) * Bp.p.(pixel).
     * A Raster (or Frame) is 2d tiled Textures of same size.
     * The size of Raster = (WdInTextures * HtInTextures) * sizeof(Texture)
     * Each . below is a pixel
     * _________________________________
     * ---------------------------------
     * ||.....|.....|.....|.....|.....||
     * ||.T00.|.T01.|.T02.|.T03.|.T04.||
     * ||.....|.....|.....|.....|.....||
     * ---------------------------------
     * ||.....|.....|.....|.....|.....||
     * ||.T10.|.T11.|.T12.|.T13.|.T14.||
     * ||.....|.....|.....|.....|.....||
     * ---------------------------------   <--- 1 Plane (eg A8R8G8B8)
     * ||.....|.....|.....|.....|.....||
     * ||.T20.|.T21.|.T22.|.T23.|.T24.||
     * ||.....|.....|.....|.....|.....||
     * ---------------------------------
     *  There are some colour formats which are associated with multiple Surfaces(Planes).
     *  In such cases we have multiple planes of Raster Frames. eg Y-UV semiplanar has 2 planes.
     *  Each . below is a texture.
     *      ----.----
     *      |...V...|
     *  ----.----...|    <-- 2 Planes (eg SemiPlanar formats)
     *  |.......|...|
     *  |...Y...|----
     *  |.......|
     *  ----.----
     *
     */
    using Byte = UINT08;
    using Bpp = UINT32;
    /**
     * \enum SupportedBpp
     * \brief Set of Bytes per pixel values for all kind of colourformats
     */
    enum SupportedBpp
    {
        BPP_1  = 1 << 0,
        BPP_2  = 1 << 1,
        BPP_4  = 1 << 2,
        BPP_8  = 1 << 3,
        BPP_16 = 1 << 4,
    };

    /**
     * \enum Pattern
     * \brief Kind of patterns are used in LwDisplay test
     *
     * The patterns are classified by method each pixel is generated. The random
     * values depend upon the seed in the LWDisplay::TestConfiguration
     *
     */
    enum Pattern
    {
        RAND_FILL,
        RAND_DSC_FILL,
        MAX_COUNT
    };
    struct Pattype {};
    struct PatRand:Pattype{};
    struct PatRandDSC:Pattype{};
    struct PatUnsupported:Pattype{};

    /**
     * \struct PatternType
     *
     * \brief Determine the kind of Pattern we want to use based on the 'Pattern' enum
     *
     * Returns equivalent struct typename for Function overloading.
     *
     */
    template <INT32>
    struct PatternType
    {
        using type = PatUnsupported;
    };
    template <>
    struct PatternType<Pattern::RAND_FILL>
    {
        using type = PatRand;
    };
    template <>
    struct PatternType<Pattern::RAND_DSC_FILL>
    {
        using type = PatRandDSC;
    };
} // namespace FramePatterns
/** @}*/

enum class FillMethod
{
    // When FillMethod is Any, GpuFill is automatically selected for larger surfaces
    // and CpuFill is used for smaller surfaces.
    Any,
    Gpufill,
    Cpufill
};

RC FillSurface08(Surface2D *pSurface, UINT08 fillValue,
        const GpuSubdevice *pGpuSubdevice = nullptr);
RC FillSurface32(Surface2D *pSurface, UINT32 fillValue,
        const GpuSubdevice *pGpuSubdevice = nullptr);
RC FillSurface(Surface2D *pSurface, UINT64 fillValue, UINT32 bpp,
        const GpuSubdevice *pGpuSubdevice = nullptr,
        FillMethod fillMethod = FillMethod::Any);

//----------------------------------------------------------------
//! \brief Abstract base class for filling surfaces
//!
class ISurfaceFiller
{
public:
    explicit ISurfaceFiller(GpuDevice *pGpuDevice);
    virtual ~ISurfaceFiller() {}
    GpuDevice *GetGpuDevice() const { return m_pGpuDevice; }
    RC SetTargetSubdevice(const GpuSubdevice *pTargetSubdev);
    const GpuSubdevice *GetTargetSubdevice() const
    { return m_pTargetSubdevice; }
    void SetAutoWait(bool autoWait) { m_AutoWait = autoWait; }
    bool GetAutoWait() const { return m_AutoWait; }
    RC Fill(Surface2D *pSurface, UINT64 fillValue, UINT32 bpp);
    RC Fill08(Surface2D *pSurface, UINT08 fillValue)
    { return Fill(pSurface, fillValue, 8); }
    RC Fill32(Surface2D *pSurface, UINT32 fillValue)
    { return Fill(pSurface, fillValue, 32); }
    virtual RC FillRange(Surface2D *pSurface, UINT64 fillValue, UINT32 bpp,
            UINT64 offset, UINT64 size) = 0;
    virtual RC Wait() = 0;
    virtual RC Cleanup() = 0;
protected:
    static bool IsSupported(const Surface2D &surface, UINT64 fillValue,
            UINT32 bpp, UINT64 offset, UINT64 size);
    static RC ResizeFillValue(const Surface2D &surface,
            UINT64 *pFillValue, UINT32 *pBpp);
    RC SetSubdeviceMask(Channel *pCh, bool *pDidSetMask);
    static RC ClearSubdeviceMask(Channel *pCh);
    RC WriteSemaphore
    (
        NotifySemaphore *pSemaphore,
        Channel         *pCh,
        UINT32           subch,
        UINT32           methodA
    );
    RC WaitSemaphore(NotifySemaphore *pSemaphore, Channel *pCh,
            FLOAT64 timeoutMs);
private:
    ISurfaceFiller(const ISurfaceFiller&);            // Non-copyable
    ISurfaceFiller& operator=(const ISurfaceFiller&); // Non-copyable
    GpuDevice *const    m_pGpuDevice;
    const GpuSubdevice *m_pTargetSubdevice;
    bool                m_AutoWait; // If true, Fill*() waits to finish
};

//----------------------------------------------------------------
//! \brief Subclass that uses memory-mapping to fill surfaces
//!
//! This is the slowest technique for large surfaces, but it's
//! almost guaranteed to be supported.  And it has minimal
//! overhead, so it can be fast for small surfaces.
//!
class MappingSurfaceFiller: public ISurfaceFiller
{
public:
    explicit MappingSurfaceFiller(GpuDevice *pGpuDevice)
        : ISurfaceFiller(pGpuDevice) {}
    static bool IsSupported(const Surface2D &surface, UINT64 fillValue,
            UINT32 bpp, UINT64 offset, UINT64 size);
    virtual RC FillRange(Surface2D *pSurface, UINT64 fillValue, UINT32 bpp,
            UINT64 offset, UINT64 size);
    virtual RC Wait() { return OK; }
    virtual RC Cleanup() { return OK; }
};

//----------------------------------------------------------------
//! \brief Subclass that uses the copy engine to fill surfaces
//!
//! This is fairly fast for large surfaces, and supported on
//! almost all GPUs.  It uses a REMAP_CONST feature of the copy
//! engine, which allows us to fill the destination surface
//! without creating a source surface.
//!
class CopyEngineSurfaceFiller : public ISurfaceFiller
{
public:
    enum { ANY_ENGINE = 0xffffffff };
    explicit CopyEngineSurfaceFiller(GpuDevice *pGpuDevice,
            UINT32 engineNum = ANY_ENGINE);
    virtual ~CopyEngineSurfaceFiller() { Cleanup(); }
    static bool IsSupported(const Surface2D &surface, UINT64 fillValue,
            UINT32 bpp, UINT64 offset, UINT64 size);
    RC Alloc();
    virtual RC FillRange(Surface2D *pSurface, UINT64 fillValue, UINT32 bpp,
            UINT64 offset, UINT64 size);
    virtual RC Wait();
    virtual RC Cleanup();
private:
    const UINT32         m_EngineNum;
    GpuTestConfiguration m_TestConfig;
    Channel             *m_pCh;
    LwRm::Handle         m_hCh;
    LwRm::Handle         m_hVASpace;
    SurfaceUtils::VASpaceMapping m_Mapping;
    DmaCopyAlloc         m_DmaCopyAlloc;
    NotifySemaphore      m_Semaphore;
};

//----------------------------------------------------------------
//! \brief Subclass that uses a 3D clear to fill surfaces
//!
//! This is fastest for large surfaces.  But it's not supported on
//! all GPUs, and there are some fairly draconian restrictions about
//! which surfaces it can be used on.
//!
//! This class uses ZBC (zero-bandwidth clear) if possible.  ZBC
//! only works on compressed FB surfaces.  In a compressed
//! surface, the PTE (page table entry) contains a pointer to an
//! array of "comp bits" in a separate region of memory.  There
//! are 1-4 comp bits (depending on PTE kind) for each ROP tile
//! (256 bytes, or 1/2 GOB) in the page, as well as a page-wide
//! index into a global 15-element ZBC table of clear colors.  If
//! the comp bits indicate "ZBC", then the GPU treats the ROP tile
//! as if it were filled with the page's ZBC clear color.  See
//! //hw/doc/gpu/fermi/fermi/design/Functional_Descriptions/
//! FermiFBCompression_FD.doc for more details.
//!
class ThreeDSurfaceFiller : public ISurfaceFiller
{
public:
    explicit ThreeDSurfaceFiller(GpuDevice *pGpuDevice);
    virtual ~ThreeDSurfaceFiller() { Cleanup(); }
    static bool IsSupported(const Surface2D &surface, UINT64 fillValue,
            UINT32 bpp, UINT64 offset, UINT64 size);
    static bool SupportsZbc(const Surface2D &surface, UINT64 fillValue,
            UINT32 bpp, UINT64 offset, UINT64 size);
    RC Alloc();
    virtual RC FillRange(Surface2D *pSurface, UINT64 fillValue, UINT32 bpp,
            UINT64 offset, UINT64 size);
    virtual RC Wait();
    virtual RC Cleanup();
private:
    GpuTestConfiguration m_TestConfig;
    Channel             *m_pCh;
    LwRm::Handle         m_hCh;
    LwRm::Handle         m_hVASpace;
    SurfaceUtils::VASpaceMapping m_Mapping;
    ThreeDAlloc          m_ThreeDAlloc;
    NotifySemaphore      m_Semaphore;
};

//----------------------------------------------------------------
//! \brief Subclass that chooses the fastest way to fill surfaces
//!
//! This subclass contains instances of the other three
//! subclasses, and uses whichever one is supported on the current
//! GPU and seems fastest in the long run.
//!
class OptimalSurfaceFiller : public ISurfaceFiller
{
public:
    explicit OptimalSurfaceFiller(GpuDevice *pGpuDevice)
        : ISurfaceFiller(pGpuDevice) {}
    static bool IsSupported(const Surface2D &surface, UINT64 fillValue,
            UINT32 bpp, UINT64 offset, UINT64 size);
    virtual RC FillRange(Surface2D *pSurface, UINT64 fillValue, UINT32 bpp,
            UINT64 offset, UINT64 size);
    virtual RC Wait();
    virtual RC Cleanup();
private:
    unique_ptr<MappingSurfaceFiller>    m_pMappingFiller;
    unique_ptr<CopyEngineSurfaceFiller> m_pCopyEngineFiller;
    unique_ptr<ThreeDSurfaceFiller>     m_pThreeDFiller;
};

//----------------------------------------------------------------
/*
 * \class SurfaceFill
 *
 * \brief Baseclass that abstracts the difference of DMA vs Non-DMA
 *
 * This class contains an array of concrete Image(surfaceStorage) objects
 * which can be used for scanning out as an input to Display::DisplayImage.
 * SurfaceFill class can handle filling up of Image objects with needed data pattern.
 */

class SurfaceFill
{
public:
    typedef UINT32 SurfaceId;

    explicit SurfaceFill(bool useMemoryFillFns);
    virtual ~SurfaceFill() = default;

    /**
     * \brief Initialize PatternInfo map. Store the test config for Fpicker context
     * that can be used for Random fill seed. Also setup valid GpuDev ptr for
     * later fill operations
     */
    virtual RC Setup(GpuTest *gpuTest, GpuTestConfiguration *testConfig);
    virtual RC Setup(GpuSubdevice *pGpuSubDev, GpuTestConfiguration *testConfig);
    virtual RC Cleanup() = 0;

    virtual bool IsSupportedImageFile(const string& fileName);
    virtual bool IsSupportedImage(const string& imageName);

    virtual RC PrepareSurface
    (
         SurfaceId surfaceId,
         const Display::Mode& resolutionMode,
         ColorUtils::Format forcedColorFormat = ColorUtils::LWFMT_NONE
    ) = 0;
    virtual RC PrepareSurface
    (
         SurfaceId surfaceId,
         const Display::Mode& resolutionMode,
         UINT32 surfFieldType,
         ColorUtils::Format forcedColorFormat = ColorUtils::LWFMT_NONE
    ) = 0;
    virtual RC PrepareSurface
    (
         SurfaceId surfaceId,
         const Display::Mode& resolutionMode,
         ColorUtils::Format forcedColorFormat,
         ColorUtils::YCrCbType colorTypeYCrCb,
         Surface2D::Layout layout,
         UINT32 logBlockWidth,
         UINT32 logBlockHeight,
         UINT32 surfFieldType,
         Surface2D::InputRange inputRange,
         unique_ptr<Surface2D> pRefSysmemSurface
    )  = 0;
    virtual RC Reserve(const vector<SurfaceId> &surfaceIds) = 0;
    virtual RC Insert(const SurfaceId surfaceId, unique_ptr<Surface2D> pSurf) = 0;
    virtual RC Insert
    (
         const SurfaceId surfaceId,
         unique_ptr<Surface2D> pFbSurf,
         shared_ptr<Surface2D> pSysSurf
    ) = 0;
    virtual void Erase(const SurfaceId surfaceId) = 0;

    // Actually fill data in the surface corresponding to surfaceId
    // stored in SurfaceMap of concrete type DMAkind or NonDMAkind.
    // This function has to be called the test.
    RC FillPattern(SurfaceId surfaceId, const string& image);
    RC FillPattern(SurfaceId surfaceId, const FramePatterns::Pattern pattern);
    RC FillPattern(SurfaceId surfaceId, const vector<UINT32> & patternData);
    RC FillPatternRect
    (
         SurfaceId surfaceId,
         const char *PatternType,
         const char *PatternStyle,
         const char *PatternDir
    );

    // This function can be use to specify custom operations like rbg flipping or
    // any specific operation that involve filling a new surface using data from a
    // previously filled one
    //! \param srcSurfaceId: surfaceid of the source surface to be used as refernce for
    //!                      fill operation.
    //! \param dstSufaceId: surfaceid of the destination surface where filled pixels
    // !                    would be copied.
    //! \param pFillOperation: type of fill operation being performed specified as a string
    //
    RC FillPatternLwstom
    (
        SurfaceId srcSurfaceId,
        SurfaceId dstSurfaceId,
        const char *pFillOperation
    );

    virtual RC CopySurface(SurfaceId surfaceId) = 0;

    virtual Surface2D* GetSurface(SurfaceId surfaceId) = 0;
    virtual Surface2D* GetImage(SurfaceId surfaceId) = 0;
    virtual Surface2D* operator[](SurfaceId surfaceId) = 0;

    virtual RC FreeAllSurfaces() = 0;
    SETGET_PROP(Layout, Surface2D::Layout);
    SETGET_PROP(LogBlockWidth, UINT32);
    SETGET_PROP(LogBlockHeight, UINT32);
    SETGET_PROP(YCrCbType, ColorUtils::YCrCbType);
    SETGET_PROP(InputRange, Surface2D::InputRange);
    SETGET_PROP(Type, UINT32);
    SETGET_PROP(PlanesFormat, ColorUtils::Format);

protected:
    GpuDevice *m_pGpuDev = nullptr;
    UINT32 m_GpuSubDevInstance = Gpu::UNSPECIFIED_SUBDEV;
    GpuTestConfiguration *m_pTstCfg = nullptr;

    struct PatternInfo
    {
    private:
        string type;
        string style;
        string direction;
    public:
        PatternInfo(const string& type, const string& style, const string& direction):
            type(type), style(style), direction(direction)
        {

        }
        PatternInfo() = default;
        const char* GetType() { return type.c_str(); }
        const char* GetStyle() { return style.c_str(); }
        const char* GetDirection() { return direction.c_str(); }
    };
    RC AllocSurface
    (
        const Display::Mode& resolutionMode,
        Memory::Location location,
        ColorUtils::Format colorFormat,
        bool isDisplayable,
        unique_ptr<Surface2D> *ppSurface
    );
    //map<string, PatternInfo> m_PatternInfoMap;

private:
    RC InitPatternInfoMap();
    RC FillSurfaceRandom(SurfaceId surfaceId, FramePatterns::PatRand* pPatKind);
    RC FillSurfaceRandom(SurfaceId surfaceId, FramePatterns::PatRandDSC* pPatKind);
    // If true then use the Memory::Fill* functions for filling Images otherwise
    // use Surface2D::Fill functions
    bool m_UseMemoryFillFns = false;
    // Store the Pattern styles that are supported in Surface2D PRESETS
    // key is PatternName[ "horiz_bars", "vert_bars", "horiz_stripes", ...,
    // ..., "random" ]. The value stores corresponding PatternStyle or seed.
    map<string, PatternInfo> m_PatternInfoMap;
    Surface2D::Layout m_Layout = Surface2D::Pitch;
    UINT32 m_LogBlockWidth = 0;
    UINT32 m_LogBlockHeight = 4;
    UINT32 m_Type = LWOS32_TYPE_IMAGE;
    ColorUtils::YCrCbType m_YCrCbType = ColorUtils::CCIR601;
    Surface2D::InputRange m_InputRange = Surface2D::BYPASS;
    ColorUtils::Format m_PlanesFormat = ColorUtils::LWFMT_NONE;
};

//----------------------------------------------------------------
/*
 * \class SurfaceFillDMA
 *
 * \brief Setup concrete SurfaceFill object with DMA copy implementation
 *        of Fill functions.
 * Only one instance is needed for one test and Gpu pair to manage all of
 * the filled surfaces
 *
 */
class SurfaceFillDMA : public SurfaceFill
{
public:
    explicit SurfaceFillDMA(bool useMemoryFillFns);

    RC Setup(GpuTest *gpuTest, GpuTestConfiguration *testConfig) override;
    RC Cleanup() override;

    RC PrepareSurface
    (
         SurfaceId surfaceId,
         const Display::Mode& resolutionMode,
         ColorUtils::Format forcedColorFormat = ColorUtils::LWFMT_NONE
    ) override;
    RC PrepareSurface
    (
         SurfaceId surfaceId,
         const Display::Mode& resolutionMode,
         UINT32 surfFieldType,
         ColorUtils::Format forcedColorFormat = ColorUtils::LWFMT_NONE
    ) override;
    RC PrepareSurface
    (
         SurfaceId surfaceId,
         const Display::Mode& resolutionMode,
         ColorUtils::Format forcedColorFormat,
         ColorUtils::YCrCbType colorTypeYCrCb,
         Surface2D::Layout layout,
         UINT32 logBlockWidth,
         UINT32 logBlockHeight,
         UINT32 surfFieldType,
         Surface2D::InputRange inputRange,
         unique_ptr<Surface2D> pRefSysmemSurf
    ) override;
    RC Reserve(const vector<SurfaceId> &surfaceIds) override;
    RC Insert(const SurfaceId surfaceId, unique_ptr<Surface2D> pSurf) override;
    RC Insert
    (
         const SurfaceId surfaceId,
         unique_ptr<Surface2D> pFbSurf,
         shared_ptr<Surface2D> pSysSurf
    ) override;
    void Erase(const SurfaceId surfaceId) override;
    RC CopySurface(SurfaceId surfaceId) override;

    Surface2D* GetSurface(SurfaceId surfaceId) override;
    Surface2D* GetImage(SurfaceId surfaceId) override;
    Surface2D* operator[](SurfaceId surfaceId) override;

    RC FreeAllSurfaces() override;
private:
    DmaWrapper m_DmaWrap;

    // Key: Reference Surf2d object. Generally it's in Sys mem, containing the
    //      the source fill.
    // Data: Pointer to Dest surf2d. Generally in FB which is to be used by the
    //       test for scan-out.
    typedef pair<std::shared_ptr<Surface2D>, std::unique_ptr<Surface2D>> SurfaceDmaPair;
    // Key: Unique SurfaceId provided the test.
    // Data: SurfaceDmaPair
    map<UINT32, SurfaceDmaPair> m_SurfaceMap;
};

//----------------------------------------------------------------
/*
 * \class SurfaceFillFB
 *
 * \brief Setup concrete SurfaceFill object with CPU write implementation
 *
 * Only one instance is needed for one test and Gpu pair to manage all of
 * the filled surface. Fmodel does not support DMA copy so we can use this
 * implementation.
 *
 */
class SurfaceFillFB : public SurfaceFill
{
public:
    explicit SurfaceFillFB(bool useMemoryFillFns);

    RC Cleanup() override;

    RC PrepareSurface
        (
         SurfaceId surfaceId,
         const Display::Mode& resolutionMode,
         ColorUtils::Format forcedColorFormat = ColorUtils::LWFMT_NONE
        ) override;
    RC PrepareSurface
        (
         SurfaceId surfaceId,
         const Display::Mode& resolutionMode,
         UINT32 surfFieldType,
         ColorUtils::Format forcedColorFormat = ColorUtils::LWFMT_NONE
        ) override;
    RC PrepareSurface
        (
         SurfaceId surfaceId,
         const Display::Mode& resolutionMode,
         ColorUtils::Format forcedColorFormat,
         ColorUtils::YCrCbType colorTypeYCrCb,
         Surface2D::Layout layout,
         UINT32 logBlockWidth,
         UINT32 logBlockHeight,
         UINT32 surfFieldType,
         Surface2D::InputRange inputRange,
         unique_ptr<Surface2D> pRefSysmemSurf
        ) override;
    RC Reserve(const vector<SurfaceId> &surfaceIds) override;
    RC Insert(const SurfaceId surfaceId, unique_ptr<Surface2D> Fb) override;
    RC Insert
        (
         const SurfaceId surfaceId,
         unique_ptr<Surface2D> pFbSurf,
         shared_ptr<Surface2D> pSysSurf
        ) override;
    void Erase(const SurfaceId surfaceId) override;

    RC CopySurface(SurfaceId surfaceId) override;

    Surface2D* GetSurface(SurfaceId surfaceId) override;
    Surface2D* GetImage(SurfaceId surfaceId) override;
    Surface2D* operator[](SurfaceId surfaceId) override;

    RC FreeAllSurfaces() override;
private:
    // Key: Unique surface Id provided by the test for each of the used surface
    // Data: Surf2D ptr for filling and re-use
    map<UINT32, unique_ptr<Surface2D>> m_SurfaceMap;
};

struct SurfaceParams
{
    UINT32 imageWidth;
    UINT32 imageHeight;
    UINT32 colorDepthBpp;
    bool bFillFBWithColor;
    UINT32 fbColor;
    Surface2D::Layout layout;
    Surface2D::InputRange inputRange;
    ColorUtils::Format colorFormat;
    string imageFileName;

    SurfaceParams();
    void Reset();
};

class SurfaceHelper
{
public:
    static RC SetupSurface
    (
     const GpuSubdevice *gpuSubdevice,
     const SurfaceParams *pSurfParams,
     Surface2D *pChnImage,
     Display::ChannelID chId
    );

    static ColorUtils::Format LwrsColorDepthToFormat(UINT32 colorDepth);
};


struct Packed
{
    Surface2D *pWindowImage;
};

struct PackedStereo
{
    Surface2D *pLeftWindowImage;
    Surface2D *pRightWindowImage;
};

struct SemiPlanar
{
    Surface2D *pYImage;
    Surface2D *pUVImage;
    ColorUtils::Format colorFormat;
};

struct SemiPlanarStereo
{
    Surface2D *pYLeftImage;
    Surface2D *pUVLeftImage;
    Surface2D *pYRightImage;
    Surface2D *pUVRightImage;
    ColorUtils::Format colorFormat;
};

struct Planar
{
    Surface2D *pYImage;
    Surface2D *pUImage;
    Surface2D *pVImage;
    ColorUtils::Format colorFormat;
};

struct PlanarStereo
{
    Surface2D *pYLeftImage;
    Surface2D *pULeftImage;
    Surface2D *pVLeftImage;
    Surface2D *pYRightImage;
    Surface2D *pURightImage;
    Surface2D *pVRightImage;
    ColorUtils::Format colorFormat;
};

enum StorageFormat
{
    PACKED,
    PACKED_STEREO,
    PACKED_DID,         // DisplayImageDescription
    PACKED_DID_STEREO,
    SEMIPLANAR,
    SEMIPLANAR_STEREO,
    PLANAR,
    PLANAR_STEREO,
};

/**
 *\class SurfaceStorage
 *
 * \brief Wrapper over Display Scanout specific Image formats
 * For general ColourFormats : Packed
 * For Stereo mode store Lt and Rt image: Packed[2] or PackedStereo
 * For SemiPlanar colourFormats : SemiPlanar (separate Surface2D for Y and for UV planes)
 * For Planar colourFormats : Planar (separate for Y, U and V planes)
 * For directly scanning out 2D array of pixels: DisplayImageDescription
 *
 */
struct SurfaceStorage
{
    StorageFormat storageFormat;
    union
    {
        Packed pkd;
        PackedStereo pkdStereo;
        SemiPlanar semiPlanar;
        SemiPlanarStereo semiPlanarStereo;
        Planar planar;
        PlanarStereo planarStereo;
        GpuUtility::DisplayImageDescription imageDescription;
    };
    SurfaceStorage();
    void Reset();
    bool IsDirty() const;
    void FillPackedImage(Surface2D* pSurf);
    void FillSemiPlanarImage(ColorUtils::Format, Surface2D* pSurfY, Surface2D* pSurfUV);
    void FillStereoImageDescription
    (
        Surface2D* pSurf,
        Surface2D* pSurfRight,
        UINT32 width,
        UINT32 height
    );
    UINT32 GetWidth() const;
    UINT32 GetHeight() const;
    ColorUtils::Format GetColorFormat() const;
    Surface2D::Layout GetLayout() const;
    UINT32 GetBlockWidth(UINT32 planeIdx = 0) const;
    UINT32 GetLogBlockHeight() const;
    UINT32 GetPitch(UINT32 planeIdx = 0) const;
    Surface2D::InputRange GetInputRange() const;
    RC GetSurfaces(Surfaces *pSurfs) const;
};
} // namespace SurfaceUtils
/** @}*/
#endif // INCLUDED_SURFFILL_H
