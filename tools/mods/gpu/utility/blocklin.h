/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007, 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef BLOCKLINEAR_H
#define BLOCKLINEAR_H

#ifndef INCLUDED_STL_MEMORY
#include <memory>
#define INCLUDED_STL_MEMORY
#endif

struct XYZ
{
    UINT32 x, y, z;
};

// For fermi, gob size information handled by GpuDevice
class GpuDevice;

// exported functions
XYZ unlinearmap(UINT64 addr, UINT32 w, UINT32 h = 0);
UINT32 log2(UINT32 x);

// =============================================================================
// Block Linear
// =============================================================================

class BlockLinear
{
public:
    //For block linear layout spec
    //Spec: //hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_Color_Z_Packing.docx
    //Spec: //hw/doc/gpu/turing/turing/design/Functional_Descriptions/physical-to-raw.vsd
    enum BlockLinearLayout
    {
        NAIVE_BL,
        XBAR_RAW,
        FB_RAW
    };

    BlockLinear(UINT32 w, UINT32 h, UINT32 d,
                UINT32 LogBlockWidth, UINT32 LogBlockHeight,
                UINT32 LogBlockDepth, UINT32 bpp, GpuDevice *pGpuDev);

    BlockLinear(UINT32 w, UINT32 h,
                UINT32 LogBlockWidth, UINT32 LogBlockHeight,
                UINT32 LogBlockDepth, UINT32 bpp, GpuDevice *pGpuDev);

    virtual ~BlockLinear() {}

    static void Create(unique_ptr<BlockLinear>* ppBLAddressing,
                       UINT32 w, UINT32 h, UINT32 d,
                       UINT32 LogBlockWidth, UINT32 LogBlockHeight,
                       UINT32 LogBlockDepth, UINT32 bpp, GpuDevice *pGpuDev,
                       BlockLinearLayout bll = BlockLinearLayout::NAIVE_BL);

    virtual UINT64 Address(UINT32 a) const;
    virtual UINT64 Address(UINT32 x, UINT32 y, UINT32 z) const;
    virtual UINT64 Address(UINT32 x, UINT32 y, UINT32 z,
                           UINT32 xfactor, UINT32 yfactor,
                           UINT32 dx, UINT32 dy);
    UINT64 GobAddress(UINT32 x, UINT32 y, UINT32 z) const;
    UINT64 GobAddress(UINT32 x, UINT32 y, UINT32 z,
                      UINT32 xfactor, UINT32 yfactor);
    UINT64 Width() const { return m_ImageWidthBlock*m_BlockWidthTexel; }
    UINT64 Height() const { return m_ImageHeightBlock*m_BlockHeightTexel; }
    UINT64 Depth() const { return m_ImageDepthBlock*m_BlockDepthTexel; }
    UINT64 Size() const { return Width() * Height() * Depth() * m_Bpp; }
    UINT32 BytesPerPixel() const { return m_Bpp; }
    UINT32 Log2BlockWidthGob() const { return m_Log2BlockWidthGob; }
    UINT32 Log2BlockHeightGob() const { return m_Log2BlockHeightGob; }
    UINT32 Log2BlockDepthGob() const { return m_Log2BlockDepthGob; }
    void   SetLog2BlockHeightGob(UINT32 GobHeight);

    virtual UINT32 OffsetInGob(UINT32 offset) const;

    static bool OptimalBlockSize3D(UINT32 gob_size[3], UINT32 bpp, UINT32 height,
                           UINT32 depth, UINT32 dram_col_bits, UINT32 num_part,
                           GpuDevice *);

    static bool OptimalBlockSize2D(UINT32 gob_size[3], UINT32 bpp, UINT32 height,
                           UINT32 depth, UINT32 dram_col_bits, UINT32 num_part,
                           GpuDevice *);

    static BlockLinearLayout SelectBlockLinearLayout
    (
        INT32 pteKind,
        GpuDevice *pGpuDev
    );

protected:
    // Used for accessing GobSize
    GpuDevice *m_pGpuDev;

    //! Byte per pixels
    UINT32 m_Bpp;
    //! GOB size in bytes
    UINT32 m_GobSize;
    //! GOB width in bytes
    UINT32 m_GobWidth;
    //! GOB height in pixels
    UINT32 m_GobHeight;
    //! log2 of number of Texels in a Gob horizontally
    UINT32 m_Log2GobWidthTexel;
    //! log2 of number of Texels in a Gob vertically
    UINT32 m_Log2GobHeightTexel;
    //! log2 of number of Gobs in a Block horizontally
    UINT32 m_Log2BlockWidthGob;
    //! log2 of number of Gobs in a Block vertically
    UINT32 m_Log2BlockHeightGob;
    UINT32 m_Log2BlockDepthGob;

    UINT32 m_BlockWidthTexel;
    UINT32 m_BlockHeightTexel;
    UINT32 m_BlockDepthTexel;

    UINT32 m_ImageWidthBlock;
    UINT32 m_ImageHeightBlock;
    UINT32 m_ImageDepthBlock;

    UINT32 m_ImageWidthTexels;
    UINT32 m_ImageHeightTexels;
};

// =============================================================================
// BlockLinearDirectTegra16BX2
// =============================================================================

class BlockLinearDirectTegra16BX2 : public BlockLinear
{
public:
    BlockLinearDirectTegra16BX2
    (
        UINT32 w,
        UINT32 h,
        UINT32 LogBlockWidth,
        UINT32 LogBlockHeight,
        UINT32 LogBlockDepth,
        UINT32 bpp,
        GpuDevice *pGpuDev
    );
    virtual ~BlockLinearDirectTegra16BX2() {}

    virtual UINT64 Address(UINT32 a) const;
    virtual UINT64 Address(UINT32 x, UINT32 y, UINT32 z) const;
    virtual UINT64 Address(UINT32 x, UINT32 y, UINT32 z,
                           UINT32 xfactor, UINT32 yfactor,
                           UINT32 dx, UINT32 dy);
    virtual UINT32 OffsetInGob(UINT32 offset) const;
private:
    UINT32 OffsetInGob(UINT32 x, UINT32 y) const;
};

// =============================================================================
// Block Linear Texture
// =============================================================================

class BlockLinearTexture : public BlockLinear
{
public:
    BlockLinearTexture(UINT32 w, UINT32 h, UINT32 d,
                       UINT32 LogBlockWidth, UINT32 LogBlockHeight,
                       UINT32 LogBlockDepth, UINT32 bpp, GpuDevice *pGpuDev);

    BlockLinearTexture(UINT32 w, UINT32 h,
                       UINT32 LogBlockWidth, UINT32 LogBlockHeight,
                       UINT32 LogBlockDepth, UINT32 bpp, GpuDevice *pGpuDev);

    ~BlockLinearTexture() {}

    UINT64 Address(UINT32 a) const;
    UINT64 Address(UINT32 x, UINT32 y, UINT32 z) const;
    UINT64 Address(UINT32 x, UINT32 y, UINT32 z, UINT32 xfactor,
                   UINT32 yfactor, UINT32 dx, UINT32 dy);
    UINT32 OffsetInGob(UINT32 offset) const;
};

// =============================================================================
// BlockLinearDirectTegra16BX2Raw
// =============================================================================

class BlockLinearDirectTegra16BX2Raw : public BlockLinear
{
public:
    BlockLinearDirectTegra16BX2Raw
    (
        UINT32 w
        ,UINT32 h
        ,UINT32 LogBlockWidth
        ,UINT32 LogBlockHeight
        ,UINT32 LogBlockDepth
        ,UINT32 bpp
        ,GpuDevice *pGpuDev
    );
    virtual ~BlockLinearDirectTegra16BX2Raw() {}

    UINT64 Address(UINT32 a) const override;
    UINT64 Address
    (
        UINT32 x
        ,UINT32 y
        ,UINT32 z
    ) const override;
    UINT64 Address
    (
        UINT32 x
        ,UINT32 y
        ,UINT32 z
        ,UINT32 xfactor
        ,UINT32 yfactor
        ,UINT32 dx
        ,UINT32 dy
    ) override;
    UINT32 OffsetInGob(UINT32 offset) const override;
private:
    UINT32 OffsetInGob
    (
        UINT32 x
        ,UINT32 y
    ) const;
};

#endif /* BLOCKLINEAR_H */
