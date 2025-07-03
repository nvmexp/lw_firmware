/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/platform.h"
#include "blocklin.h"
#include "core/include/massert.h"
#include "gpu/include/gpudev.h"
#include "core/include/utility.h"
#include <math.h>

// -----------------------------------------------------------------------------
// Utility functions
// -----------------------------------------------------------------------------

inline UINT32 Exp2(UINT32 x)
{
    return (1 << x);
}

inline UINT32 Mask(UINT32 x)
{
    return Exp2(x) - 1;
}

inline UINT32 Clamp(UINT32 a, UINT32 val, UINT32 b)
{
    if (val <= a)
        return a;
    if (val >= b)
        return b;
    return val;
}

#ifdef DEBUG
static bool IsPowerOf2(UINT32 value)
{
    return (value & (value - 1)) == 0;
}
#endif

UINT32 log2(UINT32 x)
{
    MASSERT(IsPowerOf2(x));

    if (x == 0)
        return 0;

    int n(1);
    if ((x & 0x0000FFFF) == 0) {n +=16; x = x >>16;}
    if ((x & 0x000000FF) == 0) {n += 8; x = x >> 8;}
    if ((x & 0x0000000F) == 0) {n += 4; x = x >> 4;}
    if ((x & 0x00000003) == 0) {n += 2; x = x >> 2;}

    return (n - (x&1));
}

static float flog2(UINT32 x) {
    static const double ONE_OVER_LOG_2 = 1.4426950408889634073599246810019;
    return (float)(log(double(x)) * ONE_OVER_LOG_2);
}

inline UINT64 Linearmap(UINT64 x, UINT64 y, UINT64 z, UINT64 w, UINT64 h)
{
    return (z*h + y)*w + x;
}

XYZ unlinearmap(UINT64 addr, UINT32 width, UINT32 height)
{
    XYZ ret;
    ret.x = addr % width;
    UINT64 y64 = height ? (((addr - ret.x)/ width) % height) : ((addr - ret.x)/ width);
    ret.y = UNSIGNED_CAST(UINT32, y64);
    UINT64 z64 = height ? (((addr - ret.x)/ width - ret.y)/ height) : 0;
    ret.z = UNSIGNED_CAST(UINT32, z64);
    return ret;
}

// =============================================================================
// Block Linear
// =============================================================================

BlockLinear::BlockLinear(UINT32 display_width, UINT32 display_height,
                         UINT32 display_depth,
                         UINT32 LogBlockWidth, UINT32 LogBlockHeight,
                         UINT32 LogBlockDepth, UINT32 bpp, GpuDevice *pGpuDev) :
    m_pGpuDev(pGpuDev),
    m_Bpp(bpp),
    m_GobSize(pGpuDev->GobSize()),
    m_GobWidth(pGpuDev->GobWidth()),
    m_GobHeight(pGpuDev->GobHeight()),
    m_Log2GobWidthTexel(log2(pGpuDev->GobWidth()/bpp)),
    m_Log2GobHeightTexel(log2(pGpuDev->GobHeight())),
    m_Log2BlockWidthGob(LogBlockWidth),
    m_Log2BlockHeightGob(LogBlockHeight),
    m_Log2BlockDepthGob(LogBlockDepth),
    m_BlockWidthTexel(1 << (m_Log2GobWidthTexel+m_Log2BlockWidthGob)),
    m_BlockHeightTexel(1 << (m_Log2GobHeightTexel+m_Log2BlockHeightGob)),
    m_BlockDepthTexel(1 << (m_Log2BlockDepthGob)),
    m_ImageWidthBlock((display_width+m_BlockWidthTexel-1)/m_BlockWidthTexel),
    m_ImageHeightBlock((display_height+m_BlockHeightTexel-1)/m_BlockHeightTexel),
    m_ImageDepthBlock((display_depth+m_BlockDepthTexel-1)/m_BlockDepthTexel),
    m_ImageWidthTexels(display_width),
    m_ImageHeightTexels(display_height)
{
    MASSERT(pGpuDev != NULL);
    MASSERT(IsPowerOf2(m_GobSize));
    MASSERT(IsPowerOf2(m_GobWidth));
    MASSERT(IsPowerOf2(m_GobHeight));
}

BlockLinear::BlockLinear(UINT32 display_width, UINT32 display_height,
                         UINT32 LogBlockWidth, UINT32 LogBlockHeight,
                         UINT32 LogBlockDepth, UINT32 bpp, GpuDevice *pGpuDev) :
    m_pGpuDev(pGpuDev),
    m_Bpp(bpp),
    m_GobSize(pGpuDev->GobSize()),
    m_GobWidth(pGpuDev->GobWidth()),
    m_GobHeight(pGpuDev->GobHeight()),
    m_Log2GobWidthTexel(log2(pGpuDev->GobWidth()/bpp)),
    m_Log2GobHeightTexel(log2(pGpuDev->GobHeight())),
    m_Log2BlockWidthGob(LogBlockWidth),
    m_Log2BlockHeightGob(LogBlockHeight),
    m_Log2BlockDepthGob(LogBlockDepth),
    m_BlockWidthTexel(1 << (m_Log2GobWidthTexel+m_Log2BlockWidthGob)),
    m_BlockHeightTexel(1 << (m_Log2GobHeightTexel+m_Log2BlockHeightGob)),
    m_BlockDepthTexel(1 << (m_Log2BlockDepthGob)),
    m_ImageWidthBlock((display_width+m_BlockWidthTexel-1)/m_BlockWidthTexel),
    m_ImageHeightBlock((display_height+m_BlockHeightTexel-1)/m_BlockHeightTexel),
    m_ImageDepthBlock(1),
    m_ImageWidthTexels(display_width),
    m_ImageHeightTexels(display_height)
{
    MASSERT(pGpuDev != NULL);
    MASSERT(IsPowerOf2(m_GobSize));
    MASSERT(IsPowerOf2(m_GobWidth));
    MASSERT(IsPowerOf2(m_GobHeight));
}

// -----------------------------------------------------------------------------
void BlockLinear::Create(unique_ptr<BlockLinear>* ppBLAddressing,
                       UINT32 w, UINT32 h, UINT32 d,
                       UINT32 LogBlockWidth, UINT32 LogBlockHeight,
                       UINT32 LogBlockDepth, UINT32 bpp, GpuDevice *pGpuDev,
                       BlockLinearLayout bll)
{
    if (Platform::UsesLwgpuDriver())
    {
        ppBLAddressing->reset(new BlockLinearDirectTegra16BX2Raw(
            w, h, LogBlockWidth, LogBlockHeight, LogBlockDepth,
            bpp, pGpuDev));
    }
    else
    {
        if (bll == BlockLinearLayout::XBAR_RAW)
        {
            ppBLAddressing->reset(new BlockLinearDirectTegra16BX2Raw(
                w, h, LogBlockWidth, LogBlockHeight, LogBlockDepth,
                bpp, pGpuDev));
        }
        else
        {
            ppBLAddressing->reset(new BlockLinear(
                w, h, d, LogBlockWidth, LogBlockHeight, LogBlockDepth,
                bpp, pGpuDev));
        }
    }
}

// -----------------------------------------------------------------------------
void BlockLinear::SetLog2BlockHeightGob(UINT32 GobHeight)
{
    m_Log2GobHeightTexel = log2(GobHeight);
}

// -----------------------------------------------------------------------------

UINT64 BlockLinear::GobAddress(UINT32 X, UINT32 Y, UINT32 Z) const
{
    // Compute coordinates of the gob storing the texel
    //  X_gob = X DIV m_GobWidthTexel = X DIV 64
    UINT32 X_gob = X >> m_Log2GobWidthTexel;
    //  Y_gob = Y DIV m_GobHeightTexel = Y DIV 4
    UINT32 Y_gob = Y >> m_Log2GobHeightTexel;
    //  Z_gob = Z
    UINT32 Z_gob = Z;

    // Compute coordinates of the block containing the gob
    //  X_block = X_gob DIV 1 = X DIV 64
    UINT32 X_block = X_gob >> m_Log2BlockWidthGob;
    //  Y_block = Y_gob DIV 4 = Y DIV 16
    UINT32 Y_block = Y_gob >> m_Log2BlockHeightGob;
    //  Z_block = Z_gob DIV 1 = Z
    UINT32 Z_block = Z_gob >> m_Log2BlockDepthGob;

    // Rearrange the block in a linear array in the frame
    //  Seq_block = X_block + Y_block * m_ImageWidthBlock * m_ImageHeightBlock
    //            = X DIV 64 + (Y DIV 16) * S
    // There is some padding:
    //  S (frame stride in macroblocks) = m_ImageWidthBlock * m_ImageHeightBlock
    UINT64 Seq_block = Linearmap(X_block, Y_block, Z_block,
                                 m_ImageWidthBlock, m_ImageHeightBlock);

    // Compute coordinates of the gob in the block
    // X_in_block_gob = X_gob MOD 1 = 0
    UINT32 X_in_block_gob = X_gob & Mask(m_Log2BlockWidthGob);
    // Y_in_block_gob = X_gob MOD 4 = (X DIV 64) MOD 4
    UINT32 Y_in_block_gob = Y_gob & Mask(m_Log2BlockHeightGob);
    UINT32 Z_in_block_gob = Z_gob & Mask(m_Log2BlockDepthGob);

    // Rearrange the gobs in a linear array in the block
    //  Seq_gob = X_in_block_gob +
    //            Y_in_block_gob * BlockWidthGob * BlockHeightGob
    //          = ((X DIV 64) MOD 4) * 1 * 4
    //          = ((X DIV 64) MOD 4) * 4
    UINT64 Seq_gob = Linearmap(X_in_block_gob,
                               Y_in_block_gob,
                               Z_in_block_gob,
                               Exp2(m_Log2BlockWidthGob),
                               Exp2(m_Log2BlockHeightGob));

    // Compute the address of the gob in the tiled frame
    //  = Seq_block * (number of gobs per block) + Seq_gob
    return
        //  (X DIV 64 + (Y DIV 16)*S) * 4 +
        (Seq_block << (m_Log2BlockWidthGob+ m_Log2BlockHeightGob+
                       m_Log2BlockDepthGob)) +
        //  ((X DIV 64) MOD 64) * 4
        Seq_gob;
}

UINT64 BlockLinear::GobAddress(UINT32 X, UINT32 Y, UINT32 Z,
                               UINT32 xfactor, UINT32 yfactor)
{
    UINT64 gobAddr = 0;

    m_ImageWidthBlock  *= xfactor;
    m_ImageHeightBlock *= yfactor;

    gobAddr = GobAddress(X, Y, Z);

    m_ImageWidthBlock  /= xfactor;
    m_ImageHeightBlock /= yfactor;

    return gobAddr;
}

// -----------------------------------------------------------------------------

UINT64 BlockLinear::Address(UINT32 v) const
{
    XYZ lwr = unlinearmap(v/m_Bpp, m_ImageWidthTexels, m_ImageHeightTexels);
    return (GobAddress(lwr.x, lwr.y, lwr.z) * m_GobSize) +
        OffsetInGob(v);
}

UINT64 BlockLinear::Address(UINT32 x, UINT32 y, UINT32 z) const
{
    // from //hw/gpuvideo/doc/msdec/uarch/msppp_tf_uarch.doc
    //
    // The address can be callwlated as:
    // addr = base + (Y DIV 16) * 1024 * (S DIV 4)+
    //  (Y MOD 16) * 64 +
    //  (X DIV 64) * 1024 +
    //  (X MOD 64)
    //
    // Tiling address of the first texel in the gob holding the texel (x,y,z)
    UINT64 address = GobAddress(x, y, z) * m_GobSize;
    // X MOD 64
    address += (x*m_Bpp) & (m_GobWidth - 1);
    // (Y MOD 4) * 64
    address += (y & (m_GobHeight - 1)) * m_GobWidth;
    return address;
}

UINT64 BlockLinear::Address(UINT32 x, UINT32 y, UINT32 z,
                                 UINT32 xfactor, UINT32 yfactor,
                                 UINT32 dx, UINT32 dy)
{
    UINT64 address = 0;
    UINT32 bX, bY;

    bX = x*xfactor+dx;
    bY = y*yfactor+dy;

    address = GobAddress(bX, bY, z, xfactor, yfactor) * m_GobSize;
    address += (bX*m_Bpp) & (m_GobWidth - 1);
    address += (bY & (m_GobHeight - 1)) * m_GobWidth;
    return address;
}

// -----------------------------------------------------------------------------
UINT32 BlockLinear::OffsetInGob(UINT32 v) const
{
    XYZ lwr = unlinearmap(v/m_Bpp, m_ImageWidthTexels, m_ImageHeightTexels);
    return (((lwr.y & Mask(m_Log2GobHeightTexel)) << m_Log2GobWidthTexel) +
                        (lwr.x & Mask(m_Log2GobWidthTexel)))*m_Bpp +
                        v%m_Bpp;
}

bool BlockLinear::OptimalBlockSize3D(UINT32 block_size[3], UINT32 bpp,
                                   UINT32 height, UINT32 depth,
                                   UINT32 dram_col_bits, UINT32 num_partitions,
                                   GpuDevice *pGpuDev)
{
    const UINT32 gobSize = pGpuDev->GobSize();
    // Fermi might need to re-implement this
    // Well... Fermi didn't re-implement this, neither any other chip so far.
    MASSERT(gobSize == 256);

    MASSERT(depth != 0);

    // gob aspect ratio
    UINT32 gob_width  = pGpuDev->GobWidth()/bpp;
    UINT32 gob_height = pGpuDev->GobHeight();
    UINT32 gob_depth  = 1;

    UINT32 gob_aspect_ratio = gob_width / gob_height;

    // compute the superpage size
    UINT32 superpage_size = (1 << (dram_col_bits + 3)) * num_partitions;
    UINT32 superpage_size_in_gobs = superpage_size / gobSize;

    block_size[0] = 1; // gob width is always 1

    // We want the superblocks to be as square as possible - that improves 2D efficiency
    block_size[1] = 0;
    if (num_partitions == 0)
    {
        // Bug 427305, work around for UMA chips which have 0 fb partition
        block_size[1] = 16;
    }
    else
    {
        if (depth == 1)
        {
            float log_page_side = flog2(superpage_size_in_gobs * gob_aspect_ratio);
            block_size[1] = 1 << UINT32((log_page_side + 1.01f) / 2.0f);
        }
        else
        {
            float log_page_side = flog2(superpage_size_in_gobs * gob_aspect_ratio
                    / gob_height);
            block_size[1] = 1 << UINT32((log_page_side + 2.01f) / 3.0f);
        }

        // block shrinkage
        while (block_size[1] * gob_height >= height * 3 / 2)
        {
            block_size[1] >>= 1;
        }
        block_size[1] = Clamp(1, block_size[1], 32);
    }

    block_size[2] = 0;
    if (depth > 1)
    {
        float log_page_side = flog2(superpage_size_in_gobs * gob_aspect_ratio
                / gob_height / block_size[1]);
        block_size[2]  = gob_height * 1 << UINT32((log_page_side + 0.99f) / 2.0f);
    }
    // block shrinkage
    while (block_size[2] * gob_depth >= depth * 3 / 2)
    {
        block_size[2] >>= 1;
    }
    block_size[2] = Clamp(1, block_size[2], 32);

    return true;
}

bool BlockLinear::OptimalBlockSize2D(UINT32 block_size[3], UINT32 bpp,
                                   UINT32 height, UINT32 depth,
                                   UINT32 dram_col_bits, UINT32 num_partitions,
                                   GpuDevice *pGpuDev)
{
    const UINT32 gobSize = pGpuDev->GobSize();
    // Fermi might need to re-implement this
    // Well... Fermi didn't re-implement this, neither any other chip so far.
    MASSERT(gobSize == 256);

    MASSERT(depth != 0);

    // gob aspect ratio
    UINT32 gob_width  = pGpuDev->GobWidth()/bpp;
    UINT32 gob_height = pGpuDev->GobHeight();
    UINT32 gob_depth  = 1;

    UINT32 gob_aspect_ratio = gob_width / gob_height;

    // compute the superpage size
    UINT32 superpage_size = (1 << (dram_col_bits + 3)) * num_partitions;
    UINT32 superpage_size_in_gobs = superpage_size / gobSize;

    block_size[0] = 1; // gob width is always 1

    // Bug 387379, requested to be hardcode
    switch (num_partitions)
    {
    case 1:
        block_size[1] = 2;
        break;
    case 2:
        block_size[1] = 4;
        break;
    case 3:
    case 4:
        block_size[1] = 8;
        break;
    case 5:
    case 6:
    case 7:
    case 8:
        block_size[1] = 16;
        break;
    default:
        block_size[1] = 16;
    }

    block_size[2] = 0;
    if (depth > 1)
    {
        float log_page_side = flog2(superpage_size_in_gobs * gob_aspect_ratio
                / gob_height / block_size[1]);
        block_size[2]  = gob_height * 1 << UINT32((log_page_side + 0.99f) / 2.0f);
    }
    // block shrinkage
    while (block_size[2] * gob_depth >= depth * 3 / 2)
    {
        block_size[2] >>= 1;
    }
    block_size[2] = Clamp(1, block_size[2], 32);

    return true;
}

BlockLinear::BlockLinearLayout BlockLinear::SelectBlockLinearLayout
(
    INT32 pteKind,
    GpuDevice *pGpuDev
)
{
    // BlockLinear helper for translating GOB addresses
    BlockLinearLayout bll = BlockLinear::NAIVE_BL;
    if (pGpuDev->GetSubdevice(0)->HasFeature(Device::GPUSUB_SUPPORTS_GENERIC_MEMORY) &&
        (pGpuDev->GetSubdevice(0)->GetKindTraits(pteKind) &
        (GpuSubdevice::Generic_Memory)))
    {
        bll = BlockLinear::XBAR_RAW;
    }

    return bll;
}

// =============================================================================
// Block Linear Direct CheetAh 16BX2
// =============================================================================

BlockLinearDirectTegra16BX2::BlockLinearDirectTegra16BX2
(
    UINT32 w,
    UINT32 h,
    UINT32 LogBlockWidth,
    UINT32 LogBlockHeight,
    UINT32 LogBlockDepth,
    UINT32 bpp,
    GpuDevice *pGpuDev
) : BlockLinear(w, h, LogBlockWidth, LogBlockHeight,
                LogBlockDepth, bpp, pGpuDev)
{
}

UINT64 BlockLinearDirectTegra16BX2::Address(UINT32 v) const
{
    XYZ lwr = unlinearmap(v/m_Bpp, m_ImageWidthTexels, m_ImageHeightTexels);
    return (GobAddress(lwr.x, lwr.y, lwr.z) * m_GobSize) +
            OffsetInGob((lwr.x & Mask(m_Log2GobWidthTexel)) * m_Bpp,
                        lwr.y & Mask(m_Log2GobHeightTexel)) + v % m_Bpp;
}

UINT64 BlockLinearDirectTegra16BX2::Address(UINT32 x, UINT32 y, UINT32 z) const
{
    // from //hw/gpuvideo/doc/msdec/uarch/msppp_tf_uarch.doc
    //
    // The address can be callwlated as:
    // addr = base + (Y DIV 16) * 1024 * (S DIV 4)+
    //  (Y MOD 16) * 64 +
    //  (X DIV 64) * 1024 +
    //  (X MOD 64)
    //
    // Tiling address of the first texel in the gob holding the texel (x,y,z)
    UINT64 address = GobAddress(x, y, z) * m_GobSize;
    address += OffsetInGob((x*m_Bpp) & (m_GobWidth - 1),
                           y & (m_GobHeight - 1));
    return address;
}

UINT64 BlockLinearDirectTegra16BX2::Address(UINT32 x, UINT32 y, UINT32 z,
                                 UINT32 xfactor, UINT32 yfactor,
                                 UINT32 dx, UINT32 dy)
{
    UINT64 Address = 0;
    UINT32 bX, bY;

    bX = x*xfactor+dx;
    bY = y*yfactor+dy;

    Address= GobAddress(bX, bY, z, xfactor, yfactor) * m_GobSize;
    Address += OffsetInGob((bX*m_Bpp) & (m_GobWidth - 1),
                           bY & (m_GobHeight - 1));
    return Address;
}

// -----------------------------------------------------------------------------
UINT32 BlockLinearDirectTegra16BX2::OffsetInGob(UINT32 v) const
{
    XYZ lwr = unlinearmap(v/m_Bpp, m_ImageWidthTexels, m_ImageHeightTexels);
    return OffsetInGob((lwr.x & Mask(m_Log2GobWidthTexel)) * m_Bpp,
                        lwr.y & Mask(m_Log2GobHeightTexel)) + v % m_Bpp;
}

//! -----------------------------------------------------------------------------
//! Callwlate the byte offset in the GOB from a x,y location
//!
//! \param x : Horizontal offset in BYTES for the location being accessed
//! \param y : Vertical offset in lines for the location being accessed
//!
//! \return The actual byte offset within the GOB for the specified location
UINT32 BlockLinearDirectTegra16BX2::OffsetInGob(UINT32 x, UINT32 y) const
{
    // Swizzle the bytes based on the CheetAh sector ordering for 16BX2 block
    // linear.  Taken directly from the "T40 Pixel Memory Format GFD.doc"
    // section 3.1.2.1
    return ((x%64)/32)*256 + ((y%8)/2)*64 + ((x%32)/16)*32 + (y%2)*16 + (x%16);
}

// =============================================================================
// Block Linear Texture
// =============================================================================

BlockLinearTexture::BlockLinearTexture
(
UINT32 display_width, UINT32 display_height,
UINT32 display_depth,
UINT32 LogBlockWidth, UINT32 LogBlockHeight,
UINT32 LogBlockDepth, UINT32 bpp, GpuDevice *pGpuDev
) :
BlockLinear(display_width, display_height,
            display_depth,
            LogBlockWidth, LogBlockHeight,
            LogBlockDepth, bpp, pGpuDev)
{
}

BlockLinearTexture::BlockLinearTexture
(
UINT32 display_width, UINT32 display_height,
UINT32 LogBlockWidth, UINT32 LogBlockHeight,
UINT32 LogBlockDepth, UINT32 bpp, GpuDevice *pGpuDev
) :
BlockLinear(display_width, display_height,
            LogBlockWidth, LogBlockHeight,
            LogBlockDepth, bpp, pGpuDev)
{
}

// -----------------------------------------------------------------------------

UINT64 BlockLinearTexture::Address(UINT32 v) const
{
    XYZ lwr = unlinearmap(v/m_Bpp, m_ImageWidthTexels, m_ImageHeightTexels);
    return (GobAddress(lwr.x, lwr.y, lwr.z) * m_GobSize) +
        OffsetInGob(v);
}

UINT64 BlockLinearTexture::Address(UINT32 x, UINT32 y, UINT32 z) const
{
    const UINT32 gobWord = m_pGpuDev->GobWord();
    // Tiling address of the first texel in the gob holding the texel (x,y,z)
    UINT64 address = GobAddress(x, y, z) * m_GobSize;
    // ( ((X MOD 64) DIV 16) * 64 + X MOD 16
    // (Y MOD 4)
    address += (((x*m_Bpp) & (m_GobWidth - 1)) >>
                log2(gobWord)) * m_GobWidth;
    address += (x*m_Bpp) & (gobWord - 1);
    // Before Fermi this is equivalent to:
    // address += (((x*m_Bpp) & (m_GobWidth - 1)) >> 4) << 6;
    // address += (x*m_Bpp) & 0xF;
    // (Y MOD 4)
    address += (y & (m_GobHeight - 1)) * gobWord;
    // Before Fermi this is equivalent to:
    // address += (y & (m_GobHeight - 1)) << 4;
    return address;
}

UINT64 BlockLinearTexture::Address(UINT32 x, UINT32 y, UINT32 z,
                                   UINT32 xfactor, UINT32 yfactor,
                                   UINT32 dx, UINT32 dy)
{
    UINT64 Address = 0;
    UINT32 bX, bY;

    bX = x*xfactor+dx;
    bY = y*yfactor+dy;

    Address = GobAddress(bX, bY, z, xfactor, yfactor) * m_GobSize;
    Address += (bX*m_Bpp) & (m_GobWidth - 1);
    Address += (bY & (m_GobHeight - 1)) * m_GobWidth;
    return Address;
}

// -----------------------------------------------------------------------------
UINT32 BlockLinearTexture::OffsetInGob(UINT32 v) const
{
    XYZ lwr = unlinearmap(v/m_Bpp, m_ImageWidthTexels, m_ImageHeightTexels);
    return (((lwr.y & Mask(m_Log2GobHeightTexel)) << m_Log2GobWidthTexel) +
                        (lwr.x & Mask(m_Log2GobWidthTexel)))*m_Bpp +
                        v%m_Bpp;
}

// =============================================================================
// Block Linear Direct CheetAh 16BX2Raw
// =============================================================================

BlockLinearDirectTegra16BX2Raw::BlockLinearDirectTegra16BX2Raw
(
    UINT32 w,
    UINT32 h,
    UINT32 LogBlockWidth,
    UINT32 LogBlockHeight,
    UINT32 LogBlockDepth,
    UINT32 bpp,
    GpuDevice *pGpuDev
) : BlockLinear(w, h, LogBlockWidth, LogBlockHeight,
                LogBlockDepth, bpp, pGpuDev)
{
}

UINT64 BlockLinearDirectTegra16BX2Raw::Address(UINT32 v) const
{
    XYZ lwr = unlinearmap(v/m_Bpp, m_ImageWidthTexels, m_ImageHeightTexels);
    return (GobAddress(lwr.x, lwr.y, lwr.z) * m_GobSize) +
            OffsetInGob((lwr.x & Mask(m_Log2GobWidthTexel)) * m_Bpp,
                        lwr.y & Mask(m_Log2GobHeightTexel)) + v % m_Bpp;
}

UINT64 BlockLinearDirectTegra16BX2Raw::Address(UINT32 x, UINT32 y, UINT32 z) const
{
    // from //hw/gpuvideo/doc/msdec/uarch/msppp_tf_uarch.doc
    //
    // The address can be callwlated as:
    // addr = base + (Y DIV 16) * 1024 * (S DIV 4)+
    //  (Y MOD 16) * 64 +
    //  (X DIV 64) * 1024 +
    //  (X MOD 64)
    //
    // Tiling address of the first texel in the gob holding the texel (x,y,z)
    //
    UINT64 address = GobAddress(x, y, z) * m_GobSize;
    address += OffsetInGob((x*m_Bpp) & (m_GobWidth - 1),
                           y & (m_GobHeight - 1));
    return address;
}

UINT64 BlockLinearDirectTegra16BX2Raw::Address(UINT32 x, UINT32 y, UINT32 z,
                                 UINT32 xfactor, UINT32 yfactor,
                                 UINT32 dx, UINT32 dy)
{
    UINT64 address = 0;
    UINT32 bX, bY;

    bX = x*xfactor+dx;
    bY = y*yfactor+dy;
    address = GobAddress(bX, bY, z, xfactor, yfactor) * m_GobSize;
    address += OffsetInGob((bX*m_Bpp) & (m_GobWidth - 1),
                           bY & (m_GobHeight - 1));
    return address;
}

// -----------------------------------------------------------------------------
UINT32 BlockLinearDirectTegra16BX2Raw::OffsetInGob(UINT32 v) const
{
    XYZ lwr = unlinearmap(v/m_Bpp, m_ImageWidthTexels, m_ImageHeightTexels);
    return OffsetInGob((lwr.x & Mask(m_Log2GobWidthTexel)) * m_Bpp,
                        lwr.y & Mask(m_Log2GobHeightTexel)) + v % m_Bpp;
}

//! -----------------------------------------------------------------------------
//! Callwlate the byte offset in the GOB from a x,y location
//!
//! \param x : Horizontal offset in BYTES for the location being accessed
//! \param y : Vertical offset in lines for the location being accessed
//!
//! \return The actual byte offset within the GOB for the specified location
UINT32 BlockLinearDirectTegra16BX2Raw::OffsetInGob(UINT32 x, UINT32 y) const
{
    // Swizzle the bytes based on the CheetAh sector ordering for 16BX2 block
    // linear.  Taken directly from the "T40 Pixel Memory Format GFD.doc"
    // section 3.1.2.1
    return ((x%64)/32)*256 + ((y%8)/4)*128 + ((x%32)/16)*64 + (y%4)*16 + (x%16);
}
