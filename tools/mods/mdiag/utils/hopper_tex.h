/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef HOPPER_TEX_H
#define HOPPER_TEX_H

#include "tex.h"

class GpuSubdevice;

////////////////////////////////////////////////////////////////////////////////
//
// HopperTextureHeaderState is a struct for storing the information from the
// texture headers of a Hopper trace that uses textures.  A texture trace has
// one texture header surface, but that surface can contain multiple texture
// headers.  Each of those texture headers will be stored in a
// HopperTextureHeaderState struct.
//
// The HopperTextureHeader class and its derived classes access the state
// information via multi-word macros, unlike the KeplerTextureHeader class
// which accesses the state information via DWORD-specific macros.
// This is why the HopperTextureHeaderState struct must be used rather than
// the TextureHeaderState struct which stores the information via DWORD.
//
struct HopperTextureHeaderState
{
    UINT32 m_Data[8];

    HopperTextureHeaderState(const TextureHeaderState* state)
    {
        m_Data[0] = state->DWORD0;
        m_Data[1] = state->DWORD1;
        m_Data[2] = state->DWORD2;
        m_Data[3] = state->DWORD3;
        m_Data[4] = state->DWORD4;
        m_Data[5] = state->DWORD5;
        m_Data[6] = state->DWORD6;
        m_Data[7] = state->DWORD7;
    }
};

////////////////////////////////////////////////////////////////////////////////
//
// HopperTextureHeader is a class for processing texture information from a
// TextureHeaderState object whose data is in the Hopper texture header format.
//
// The Hopper texture header format has six different versions, each
// representing a different kind of texture.  Each of the versions has a
// different data layout, reflecting the different needs of each version.
// For example, only the blocklinear texture header versions have a field
// for GOBS_PER_BLOCK_WIDTH because that value does not apply to pitch or
// 1D textures.  Because a texture header needs to be read differently for each
// version, there are six classes to represent the six different texture header
// versions.  The HopperTextureHeader class is an abstract class that implements
// the functionality common between each texture header version and also defines
// the functions that each texture header version class must implement.  When a
// texture header object is created, it is allocated as one of the six texture
// header version classes.  However, the six classes representing the different
// texture header versions are not directly derived from HopperTextureHeader
// class.  While very few fields in a texture header are identical across all
// versions, most fields are shared by at least two different texture header
// versions.  In order to prevent duplication, code to read fields that are
// common to multiple texture header versions are implemented in classes that
// represent common groups.  These implementation classes inherit from
// HopperTextureHeader, and in turn the six texture header version classes
// inherit from a combination of the implementation classes.  For example,
// the HopperBlocklinearTextureHeader header version class inherits from
// HopperTwoDTextureImpl, HopperBlocklinearTextureImpl, and
// HopperMultiSampleTextureImpl.
//
class HopperTextureHeader : public TextureHeader
{
public:
    static TextureHeader* CreateTextureHeader(const TextureHeaderState* state,
        GpuSubdevice* pGpuSubdev);

    HopperTextureHeader(const TextureHeaderState* state, GpuSubdevice* gpuSubdevice);
    virtual ~HopperTextureHeader();

    virtual UINT32 GetHeapAttr() const;
    virtual bool Compressible() const;

    virtual RC RelocTexture
    (
        MdiagSurf *surface,
        UINT64 offset,
        LW50Raster* raster,
        TextureMode textureMode,
        GpuDevice* gpuDevice,
        bool useOffset,
        bool useSurfaceAttr,
        bool optimalPromotion,
        bool centerSpoof,
        TextureHeaderState *newState
    );

    static RC ModifySectorPromotion(HopperTextureHeaderState* state,
        L1_PROMOTION newPromotion);

protected:
    bool VCAADepthBuffer() const override { return false; };
    bool AllSourceInB() const override;
    UINT32 BorderWidth() const override { return 0; }
    UINT32 BorderHeight() const override { return 0; }
    UINT32 BorderDepth() const override { return 0; }

    virtual UINT32 GetAttrDepth() const = 0;
    virtual bool IsBlocklinear() const = 0;
    virtual bool IsImageCompressible() const = 0;
    virtual UINT32 GetMultiSampleAttr() const = 0;
    virtual UINT32 GetMultiSampleWidthScale() const = 0;
    virtual UINT32 GetMultiSampleHeightScale() const = 0;

    virtual void RelocAddress(UINT64 offset) = 0;
    virtual void RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice) = 0;
    virtual RC RelocAAMode(MdiagSurf* surface, bool centerSpoof) = 0;

    HopperTextureHeaderState* m_State;
};

///////////////////////////////////////////////////////////////////////////////
//
// The following classes represent the implementation classes used to compose
// a texture header version class.  They are all virtually inherited from
// HopperTextureHeader so that the texture header version classes can inherit
// multiple implementation classes without duplicating HopperTextureHeader data
// or causing a multiple inheritance diamond problem.
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// The HopperTwoDTextureImpl class implements texture header processing
// that is common to two-dimensional texture header versions.
//
class HopperTwoDTextureImpl : virtual public HopperTextureHeader
{
public:
    HopperTwoDTextureImpl() : HopperTextureHeader(nullptr, nullptr) {}

    UINT32 GetAttrDepth() const override;
    UINT32 GetHeapType() const override;
    bool IsImageCompressible() const override;
    UINT32 TexelSize() const override;
    UINT32 TexelScale() const override;
    UINT32 TextureWidthScale() const override;
    UINT32 TextureHeightScale() const override;
    UINT32 DimSize() const override;
    ColorUtils::Format GetColorFormat() const override;
    UINT32 TextureWidth() const override;
    UINT32 TextureHeight() const override;
    UINT32 MaxMipLevel() const override;

    static RC ModifySectorPromotion(HopperTextureHeaderState* state,
        L1_PROMOTION newPromotion);
};

///////////////////////////////////////////////////////////////////////////////
//
// The HopperOneDTextureImpl class implements texture header processing
// that is common to one-dimensional texture header versions.
//
class HopperOneDTextureImpl : virtual public HopperTextureHeader
{
public:
    HopperOneDTextureImpl() : HopperTextureHeader(nullptr, nullptr) {}

    UINT32 GetAttrDepth() const override;
    bool IsBlocklinear() const override { return false; }
    UINT32 GetHeapType() const override { return LWOS32_TYPE_IMAGE; }
    bool IsImageCompressible() const override;
    UINT32 TexelSize() const override;
    UINT32 TexelScale() const override;
    UINT32 TextureWidthScale() const override;
    UINT32 TextureHeightScale() const override { return 1; }
    UINT32 DimSize() const override { return 1; }
    ColorUtils::Format GetColorFormat() const override;
    UINT32 BlockWidth() const override { return 0; }
    UINT32 BlockHeight() const override { return 0; }
    UINT32 BlockDepth() const override { return 0; }
    void FixBlockSize() const override {};
    UINT32 TextureWidth() const override;
    UINT32 TextureHeight() const override { return 1; }
    UINT32 TextureDepth() const override { return 1; }
    UINT32 ArraySize() const override { return 1; }
    UINT32 TexturePitch() const override { return 0; }
    UINT32 MaxMipLevel() const override { return 0; }
    void RelocAddress(UINT64 address) override;
    void RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice) override;

    static RC ModifySectorPromotion(HopperTextureHeaderState* state,
        L1_PROMOTION newPromotion);
};

///////////////////////////////////////////////////////////////////////////////
//
// The HopperBlocklinearTextureImpl class implements texture header processing
// that is common to blocklinear texture header versions.
//
class HopperBlocklinearTextureImpl : virtual public HopperTextureHeader
{
public:
    HopperBlocklinearTextureImpl() : HopperTextureHeader(nullptr, nullptr) {}

    bool IsBlocklinear() const override { return true; }
    UINT32 BlockWidth() const override;
    UINT32 BlockHeight() const override;
    UINT32 BlockDepth() const override;
    void FixBlockSize() const override;
    UINT32 TextureDepth() const override;
    UINT32 ArraySize() const override;
    UINT32 TexturePitch() const override { return 0; }
    void RelocAddress(UINT64 address) override;
    void RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice) override;
};

///////////////////////////////////////////////////////////////////////////////
//
// The HopperPitchTextureImpl class implements texture header processing
// that is common to pitch texture header versions.
//
class HopperPitchTextureImpl : virtual public HopperTextureHeader
{
public:
    HopperPitchTextureImpl() : HopperTextureHeader(nullptr, nullptr) {}

    bool IsBlocklinear() const override { return false; }
    UINT32 BlockWidth() const override { return 0; }
    UINT32 BlockHeight() const override { return 0; }
    UINT32 BlockDepth() const override { return 0; }
    void FixBlockSize() const override {};
    UINT32 TextureDepth() const override { return 1; }
    UINT32 ArraySize() const override { return 1; }
    UINT32 TexturePitch() const override;
    void RelocAddress(UINT64 address) override;
    void RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice) override;
};

///////////////////////////////////////////////////////////////////////////////
//
// The HopperMultiSampleTextureImpl class implements texture header processing
// that is common to texture header versions that support anti-aliasing.
//
class HopperMultiSampleTextureImpl : virtual public HopperTextureHeader
{
public:
    HopperMultiSampleTextureImpl() : HopperTextureHeader(nullptr, nullptr) {}

    UINT32 GetMultiSampleAttr() const override;
    UINT32 GetMultiSampleWidthScale() const override;
    UINT32 GetMultiSampleHeightScale() const override;
    RC RelocAAMode(MdiagSurf* surface, bool centerSpoof) override;
};

///////////////////////////////////////////////////////////////////////////////
//
// The HopperSingleSampleTextureImpl class implements texture header processing
// that is common to texture header versions that do not support anti-aliasing.
//
class HopperSingleSampleTextureImpl : virtual public HopperTextureHeader
{
public:
    HopperSingleSampleTextureImpl() : HopperTextureHeader(nullptr, nullptr) {}

    UINT32 GetMultiSampleAttr() const override { return 0; }
    UINT32 GetMultiSampleWidthScale() const override { return 1; }
    UINT32 GetMultiSampleHeightScale() const override { return 1; }
    RC RelocAAMode(MdiagSurf* surface, bool centerSpoof) override;
};

///////////////////////////////////////////////////////////////////////////////
//
// The following classes represent the six different texture header versions
// of the Hopper texture header format.  Each class is composed of multiple
// implementation classes from above.

class HopperBlocklinearTextureHeader :
    public HopperTwoDTextureImpl,
    public HopperBlocklinearTextureImpl,
    public HopperMultiSampleTextureImpl
{
public:
    HopperBlocklinearTextureHeader
    (
        const TextureHeaderState* state,
        GpuSubdevice* gpuSubdevice
    ) : HopperTextureHeader(state, gpuSubdevice) {}

    virtual ~HopperBlocklinearTextureHeader() {};
};

class HopperBlocklinearColorkeyTextureHeader :
    public HopperTwoDTextureImpl,
    public HopperBlocklinearTextureImpl,
    public HopperSingleSampleTextureImpl
{
public:
    HopperBlocklinearColorkeyTextureHeader
    (
        const TextureHeaderState* state,
        GpuSubdevice* gpuSubdevice
    ) : HopperTextureHeader(state, gpuSubdevice) {}

    virtual ~HopperBlocklinearColorkeyTextureHeader() {};
};

class HopperOneDRawTypedTextureHeader :
    public HopperOneDTextureImpl,
    public HopperSingleSampleTextureImpl
{
public:
    HopperOneDRawTypedTextureHeader
    (
        const TextureHeaderState* state,
        GpuSubdevice* gpuSubdevice
    ) : HopperTextureHeader(state, gpuSubdevice) {}

    virtual ~HopperOneDRawTypedTextureHeader() {};

protected:
    virtual bool AllSourceInB() const { return false; }
};

class HopperOneDStructBufTextureHeader :
    public HopperOneDTextureImpl,
    public HopperSingleSampleTextureImpl
{
public:
    HopperOneDStructBufTextureHeader
    (
        const TextureHeaderState* state,
        GpuSubdevice* gpuSubdevice
    ) : HopperTextureHeader(state, gpuSubdevice) {}

    virtual ~HopperOneDStructBufTextureHeader() {};
};

class HopperPitchTextureHeader :
    public HopperTwoDTextureImpl,
    public HopperPitchTextureImpl,
    public HopperMultiSampleTextureImpl
{
public:
    HopperPitchTextureHeader
    (
        const TextureHeaderState* state,
        GpuSubdevice* gpuSubdevice
    ) : HopperTextureHeader(state, gpuSubdevice) {}

    virtual ~HopperPitchTextureHeader() {};
};

class HopperPitchColorkeyTextureHeader :
    public HopperTwoDTextureImpl,
    public HopperPitchTextureImpl,
    public HopperSingleSampleTextureImpl
{
public:
    HopperPitchColorkeyTextureHeader
    (
        const TextureHeaderState* state,
        GpuSubdevice* gpuSubdevice
    ) : HopperTextureHeader(state, gpuSubdevice) {}

    virtual ~HopperPitchColorkeyTextureHeader() {};
};

#endif
