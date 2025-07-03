/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef MAXWELL_TEX_H
#define MAXWELL_TEX_H

#include "tex.h"

class GpuSubdevice;

////////////////////////////////////////////////////////////////////////////////
//
// MaxwellTextureHeaderState is a struct for storing the information from the
// texture headers of a Maxwell trace that uses textures.  A texture trace has
// one texture header file, but that file can contain multiple texture headers.
// Each of those texture headers will be stored in a MaxwellTextureHeaderState
// struct.
//
// The MaxwellTextureHeader class and it's derived classes access the state
// information via multi-word macros, unlike the KeplerTextureHeader class
// which accesses the state information via DWORD-specific macros.
// This is why the MaxwellTextureHeaderState struct must be used rather than
// the TextureHeaderState struct which stores the information via DWORD.
//
struct MaxwellTextureHeaderState
{
    UINT32 m_Data[8];

    MaxwellTextureHeaderState(const TextureHeaderState* state)
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
// MaxwellTextureHeader is a class for processing texture information from a
// TextureHeaderState object.  The data in the TextureHeaderState object must
// be in one of the texture header formats used by the Maxwell graphics class.
//
// There are five different Maxwell texture header formats.  The
// MaxwellTextureHeader class is an abstract base class with five derived
// classes representing each of the five Maxwell texture header formats.
// Some information from the TextureHeaderState object exists in all five
// texture headers and is also in the exact same bit positions.  This common
// data is processed in the MaxwellTextureHeader base class by using the macros
// defined for the Blocklinear texture header.  Any information that does not
// exist in all five texture headers is processed in the respective derived
// classes.
//
class MaxwellTextureHeader : public TextureHeader
{
friend class TextureHeader;
public:
    virtual ~MaxwellTextureHeader();

    virtual UINT32 GetHeapAttr() const;
    virtual UINT32 GetHeapType() const;
    virtual UINT32 GetMultiSampleAttr() const = 0;
    virtual bool IsBlocklinear() const = 0;
    virtual void FixBlockSize() const = 0;
    virtual bool Compressible() const;
    virtual UINT32 BlockWidth() const = 0;
    virtual UINT32 BlockHeight() const = 0;
    virtual UINT32 BlockDepth() const = 0;
    virtual ColorUtils::Format GetColorFormat() const;

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

    static RC ModifySectorPromotion(MaxwellTextureHeaderState* state, L1_PROMOTION newPromotion);

protected:
    static TextureHeader* CreateTextureHeader(const TextureHeaderState* state,
        GpuSubdevice* pGpuSubdev);

    MaxwellTextureHeader(const TextureHeaderState* state, GpuSubdevice* gpuSubdevice);

    virtual bool VCAADepthBuffer() const;
    virtual bool AllSourceInB() const;
    virtual UINT32 TexelSize() const;
    virtual UINT32 TexelScale() const;
    virtual UINT32 TextureWidth() const = 0;
    virtual UINT32 TextureWidthScale() const;
    virtual UINT32 TextureHeight() const = 0;
    virtual UINT32 TextureHeightScale() const;
    virtual UINT32 TextureDepth() const = 0;
    virtual UINT32 ArraySize() const = 0;
    virtual UINT32 BorderWidth() const;
    virtual UINT32 BorderHeight() const;
    virtual UINT32 BorderDepth() const;
    virtual UINT32 BorderSize() const = 0;
    virtual UINT32 DimSize() const;
    virtual UINT32 TexturePitch() const = 0;
    virtual UINT32 MaxMipLevel() const = 0;
    virtual void ReplaceTextureAddress(UINT64 offset) = 0;
    virtual void RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice) = 0;
    virtual RC RelocAAMode(MdiagSurf* surface, bool centerSpoof) = 0;

    MaxwellTextureHeaderState* m_State;
};

class MaxwellBlocklinearTextureHeader : public MaxwellTextureHeader
{
friend class MaxwellTextureHeader;
public:
    virtual ~MaxwellBlocklinearTextureHeader() {};

    virtual UINT32 GetMultiSampleAttr() const;
    virtual bool IsBlocklinear() const { return true; }
    virtual void FixBlockSize() const;
    virtual UINT32 BlockWidth() const;
    virtual UINT32 BlockHeight() const;
    virtual UINT32 BlockDepth() const;

protected:
    MaxwellBlocklinearTextureHeader(const TextureHeaderState* state, GpuSubdevice* gpuSubdevice)
        : MaxwellTextureHeader(state, gpuSubdevice) {};

    virtual UINT32 TextureWidth() const;
    virtual UINT32 TextureHeight() const;
    virtual UINT32 TextureDepth() const;
    virtual UINT32 ArraySize() const;
    virtual UINT32 BorderSize() const;
    virtual UINT32 TexturePitch() const;
    virtual UINT32 MaxMipLevel() const;
    virtual void ReplaceTextureAddress(UINT64 offset);
    virtual void RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice);
    virtual RC RelocAAMode(MdiagSurf* surface, bool centerSpoof);
};

class MaxwellBlocklinearColorkeyTextureHeader : public MaxwellTextureHeader
{
friend class MaxwellTextureHeader;
public:
    virtual ~MaxwellBlocklinearColorkeyTextureHeader() {};

    virtual UINT32 GetMultiSampleAttr() const { return 0; }
    virtual bool IsBlocklinear() const { return true; }
    virtual void FixBlockSize() const;
    virtual UINT32 BlockWidth() const;
    virtual UINT32 BlockHeight() const;
    virtual UINT32 BlockDepth() const;

protected:
    MaxwellBlocklinearColorkeyTextureHeader(const TextureHeaderState* state, GpuSubdevice* gpuSubdevice)
        : MaxwellTextureHeader(state, gpuSubdevice) {};

    virtual UINT32 TextureWidth() const;
    virtual UINT32 TextureHeight() const;
    virtual UINT32 TextureDepth() const;
    virtual UINT32 ArraySize() const;
    virtual UINT32 BorderSize() const;
    virtual UINT32 TexturePitch() const;
    virtual UINT32 MaxMipLevel() const;
    virtual void ReplaceTextureAddress(UINT64 offset);
    virtual void RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice);
    virtual RC RelocAAMode(MdiagSurf* surface, bool centerSpoof);
};

class MaxwellOneDBufferTextureHeader : public MaxwellTextureHeader
{
friend class MaxwellTextureHeader;
public:
    virtual ~MaxwellOneDBufferTextureHeader() {};

    virtual UINT32 GetMultiSampleAttr() const { return 0; }
    virtual bool IsBlocklinear() const { return false; }
    virtual void FixBlockSize() const {}
    virtual UINT32 BlockWidth() const { return 0; }
    virtual UINT32 BlockHeight() const { return 0; }
    virtual UINT32 BlockDepth() const { return 0; }

protected:
    MaxwellOneDBufferTextureHeader(const TextureHeaderState* state, GpuSubdevice* gpuSubdevice)
        : MaxwellTextureHeader(state, gpuSubdevice) {};

    virtual UINT32 TextureWidth() const;
    virtual UINT32 TextureHeight() const { return 1; }
    virtual UINT32 TextureDepth() const { return 1; }
    virtual UINT32 ArraySize() const { return 1; }
    virtual UINT32 BorderSize() const { return 0; }
    virtual UINT32 TexturePitch() const;
    virtual UINT32 MaxMipLevel() const { return 0; }
    virtual void ReplaceTextureAddress(UINT64 offset);
    virtual void RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice);
    virtual RC RelocAAMode(MdiagSurf* surface, bool centerSpoof);
};

class MaxwellPitchTextureHeader : public MaxwellTextureHeader
{
friend class MaxwellTextureHeader;
public:
    virtual ~MaxwellPitchTextureHeader() {};

    virtual UINT32 GetMultiSampleAttr() const;
    virtual bool IsBlocklinear() const { return false; }
    virtual void FixBlockSize() const {}
    virtual UINT32 BlockWidth() const { return 0; }
    virtual UINT32 BlockHeight() const { return 0; }
    virtual UINT32 BlockDepth() const { return 0; }

protected:
    MaxwellPitchTextureHeader(const TextureHeaderState* state, GpuSubdevice* gpuSubdevice)
        : MaxwellTextureHeader(state, gpuSubdevice) {};

    virtual UINT32 TextureWidth() const;
    virtual UINT32 TextureHeight() const;
    virtual UINT32 TextureDepth() const;
    virtual UINT32 ArraySize() const;
    virtual UINT32 BorderSize() const;
    virtual UINT32 TexturePitch() const;
    virtual UINT32 MaxMipLevel() const;
    virtual void ReplaceTextureAddress(UINT64 offset);
    virtual void RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice);
    virtual RC RelocAAMode(MdiagSurf* surface, bool centerSpoof);
};

class MaxwellPitchColorkeyTextureHeader : public MaxwellTextureHeader
{
friend class MaxwellTextureHeader;
public:
    virtual ~MaxwellPitchColorkeyTextureHeader() {};

    virtual UINT32 GetMultiSampleAttr() const { return 0; }
    virtual bool IsBlocklinear() const { return false; }
    virtual void FixBlockSize() const {}
    virtual UINT32 BlockWidth() const { return 0; }
    virtual UINT32 BlockHeight() const { return 0; }
    virtual UINT32 BlockDepth() const { return 0; }

protected:
    MaxwellPitchColorkeyTextureHeader(const TextureHeaderState* state, GpuSubdevice* gpuSubdevice)
        : MaxwellTextureHeader(state, gpuSubdevice) {};

    virtual UINT32 TextureWidth() const;
    virtual UINT32 TextureHeight() const;
    virtual UINT32 TextureDepth() const;
    virtual UINT32 ArraySize() const;
    virtual UINT32 BorderSize() const;
    virtual UINT32 TexturePitch() const;
    virtual UINT32 MaxMipLevel() const;
    virtual void ReplaceTextureAddress(UINT64 offset);
    virtual void RelocLayout(MdiagSurf* surface, GpuDevice* gpuDevice);
    virtual RC RelocAAMode(MdiagSurf* surface, bool centerSpoof);
};

#endif
