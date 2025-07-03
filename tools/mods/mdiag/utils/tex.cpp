/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012,2015-2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "tex.h"
#include "kepler_tex.h"
#include "maxwell_tex.h"
#include "hopper_tex.h"
#include "mdiag/sysspec.h"
#include "lwos.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include <cstring> // memcpy

TextureHeader* TextureHeader::CreateTextureHeader
(
    const TextureHeaderState* state,
    GpuSubdevice* pGpuSubdev,
    HeaderFormat headerFormat
)
{
    TextureHeader *header = nullptr;

    switch (headerFormat)
    {
        case HeaderFormat::Kepler:
            header = new KeplerTextureHeader(state, pGpuSubdev);
            break;

        case HeaderFormat::Maxwell:
            header = MaxwellTextureHeader::CreateTextureHeader(state, pGpuSubdev);
            break;

        case HeaderFormat::Hopper:
            header = HopperTextureHeader::CreateTextureHeader(state, pGpuSubdev);
            break;

        default:
            MASSERT(!"Unrecognized texture header format");
    }

    MASSERT(header != nullptr);

    header->CreateLayout();

    return header;
}

UINT08* TextureHeader::TextureParamsBlockLinear::DoLayout(UINT08* src, UINT32 Size) const
{
    DebugPrintf("Using blocklinear layout for texture: (%d,%d,%d) gobs per block%s",
        block_width, block_height, block_depth,
        (mip_levels > 1)?" in base miplevel\n":"\n");

    UINT32 el_size = 0;

    for (UINT32 i = 0; i < mip_levels; ++i)
    {
        UINT32 mip_width  = GetMipWidth(i);
        UINT32 mip_height = GetMipHeight(i);
        UINT32 mip_depth  = GetMipDepth(i);

        el_size += mip_height * mip_width * mip_depth * texel_size * texel_scale;
    }

    if ( Size < array_size * el_size * dim_size)
    {
        ErrPrintf("Wrong texture size. %d bytes are expected, only %d bytes are found.\n",
                array_size * el_size * dim_size, Size);
        return 0;
    }

    UINT08 *buffer_to_load = new UINT08[m_miplevel_size*dim_size*array_size];
    if (!buffer_to_load)
        return 0;

    std::fill(buffer_to_load, buffer_to_load+m_miplevel_size*dim_size*array_size, 0);

    for (UINT32 a = 0, lin_base = 0, el_base = 0; a < array_size; ++a)
    {
        for (UINT32 dim = 0; dim < dim_size; ++dim)
        {
            for (UINT32 i = 0, mip_base = 0; i < mip_levels; ++i)
            {
                UINT32 mip_width  = GetMipWidth(i);
                UINT32 mip_height = GetMipHeight(i);
                UINT32 mip_depth  = GetMipDepth(i);
                UINT32 offset = 0;
                UINT32 width_in_bytes = mip_width * texel_size * texel_scale;
                UINT32 step_length = min(width_in_bytes, m_pGpuDevice->GobWidth());

                for (UINT32 d=0; d < mip_depth; ++d)
                {
                    for (UINT32 h=0; h < mip_height; ++h)
                    {
                        UINT32 w = 0;
                        while( w < width_in_bytes )
                        {
                            UINT32 bytes_to_copy = min(step_length, width_in_bytes-w);
                            memcpy( buffer_to_load + el_base + mip_base + m_bl[i]->Address(offset),
                                src + lin_base + offset, bytes_to_copy );
                            w += bytes_to_copy;
                            offset += bytes_to_copy;
                        }
                    }
                }
                //callwlate base address of the next miplevel
                lin_base += mip_width*mip_height*mip_depth*texel_size*texel_scale;
                mip_base += m_bl[i]->Size();
            }
            el_base += m_miplevel_size;
        }
    }

    return buffer_to_load;
}

bool TextureHeader::TextureParamsBlockLinear::PixelOffset(UINT32 array_index, UINT32 lwbemap_face, UINT32 miplevel,
                                                          UINT32 x, UINT32 y, UINT32 z, UINT32* pix_offset) const
{
    MASSERT(pix_offset);

    if (array_index >= array_size)
        return false;

    if (miplevel >= m_bl.size())
        return false;

    UINT32 offset = (dim_size * array_index + lwbemap_face) * m_miplevel_size;

    UINT32 mip_width = 0;
    UINT32 mip_height = 0;
    for (UINT32 i = 0; i <= miplevel; ++i)
    {
        mip_width  = GetMipWidth(i);
        mip_height = GetMipHeight(i);

        offset += m_bl[i]->Size();
    }

    offset -= m_bl[miplevel]->Size();
    offset += m_bl[miplevel]->Address(((z * mip_height + y) * mip_width + x) * texel_size);

    *pix_offset = offset;

    return true;
}

UINT32 TextureHeader::TextureParamsBlockLinear::GetMipWidth(UINT32 mipLevel) const
{
    UINT32 mipWidth = max(texture_width >> mipLevel, UINT32(1));
    mipWidth = (mipWidth + width_scale - 1) / width_scale;
    mipWidth = max(mipWidth, m_MinimumMipWidth);
    mipWidth += 2 * m_BorderWidth;

    return mipWidth;
}

UINT32 TextureHeader::TextureParamsBlockLinear::GetMipHeight(UINT32 mipLevel) const
{
    UINT32 mipHeight = max(texture_height >> mipLevel, UINT32(1));
    mipHeight = (mipHeight + height_scale - 1) / height_scale;
    mipHeight = max(mipHeight, m_MinimumMipHeight);
    mipHeight += 2 * m_BorderHeight;

    return mipHeight;
}

UINT32 TextureHeader::TextureParamsBlockLinear::GetMipDepth(UINT32 mipLevel) const
{
    UINT32 mipDepth = max(texture_depth >> mipLevel, UINT32(1));
    mipDepth += 2 * m_BorderDepth;

    return mipDepth;
}

UINT08* TextureHeader::TextureParamsPitchLinear::DoLayout(UINT08* src, UINT32 size) const
{
    InfoPrintf("Using pitchlinear layout for texture\n");

    UINT08* buffer_to_load = new UINT08[Size()];

    const UINT32 scaledHeight = ScaledHeight();
    const UINT32 in_width = ScaledWidth();
    const UINT32 out_width = ScaledPitch();

    for (UINT32 i = 0; i < scaledHeight; ++i)
        copy(src + i*in_width, src + (i+1)*in_width, buffer_to_load + i*out_width);

    return buffer_to_load;
}

bool TextureHeader::TextureParamsPitchLinear::PixelOffset(UINT32 array_index, UINT32 lwbemap_face, UINT32 miplevel,
                                                          UINT32 x, UINT32 y, UINT32 z, UINT32* offset) const
{
    MASSERT(offset);

    if ((array_index > 0) || (lwbemap_face > 0) || (miplevel > 0))
    {
        ErrPrintf("MODS does not support Array/Lwbmap/Miplevel for pitch texture format!\n");
        return false;
    }
    else if ((x >= texture_width) || (y >= texture_height) || ( z > 0))
    {
        ErrPrintf("Given location is out of current texture buffer!\n");
        return false;
    }

    // FIXME: compressed textures aren't necessarily illegal with pitch layout,
    // but MODS doesn't support it lwrrently.
    else if ((m_WidthScale > 1) || (m_HeightScale > 1))
    {
        ErrPrintf("MODS can't compute pixel offset of a compressed texture!\n");
        return false;
    }

    const UINT32 in_width = texture_width*texel_size*texel_scale;
    const UINT32 out_width = (max(texture_pitch, in_width) + pitch_alignment - 1) & ~(pitch_alignment - 1);

    *offset = out_width * y + texel_scale * x;
    return true;
}

TextureHeader::TextureHeader(GpuSubdevice* pGpuSubdev)
:   m_layout(0),
    m_pGpuSubdevice(pGpuSubdev)
{
}

void TextureHeader::CreateLayout()
{
    if (IsBlocklinear())
    {
        m_layout = new TextureParamsBlockLinear(
            BlockWidth(),
            BlockHeight(),
            BlockDepth(),
            TextureWidth(),
            TextureHeight(),
            TextureDepth(),
            TextureWidthScale(),
            TextureHeightScale(),
            MaxMipLevel() + 1,
            MinimumMipWidth(),
            MinimumMipHeight(),
            TexelSize(),
            TexelScale(),
            DimSize(),
            ArraySize(),
            BorderWidth(),
            BorderHeight(),
            BorderDepth(),
            m_pGpuSubdevice->GetParentDevice());
    }
    else
    {
        MASSERT(TextureDepth() == 1);
        MASSERT(MaxMipLevel() == 0);
        MASSERT(DimSize() == 1);
        MASSERT(ArraySize() == 1);

        m_layout = new TextureParamsPitchLinear(
            TextureWidth(),
            TextureHeight(),
            TextureWidthScale(),
            TextureHeightScale(),
            TexturePitch(),
            TexelSize(),
            TexelScale(),
            BorderWidth(),
            BorderHeight(),
            BorderDepth());
    }
}

UINT32 TextureHeader::MinimumMipWidth() const
{
    // There are no lwrrently supported texture formats that have a minimum
    // mip width greater than 1.  (There was, temporarily, which is why
    // this function exists.)  Should there once again be a format added
    // that does have a minimum mip width greater than one, this function
    // will need to be made virtual and implemented in KeplerTextureHeader
    // and MaxwellTextureHeader.
    return 1;
}

UINT32 TextureHeader::MinimumMipHeight() const
{
    // There are no lwrrently supported texture formats that have a minimum
    // mip heightgreater than 1.  (There was, temporarily, which is why
    // this function exists.)  Should there once again be a format added
    // that does have a minimum mip height greater than one, this function
    // will need to be made virtual and implemented in KeplerTextureHeader
    // and MaxwellTextureHeader.
    return 1;
}

TextureHeader::~TextureHeader()
{
    if (m_layout)
        delete m_layout;
}

UINT08* TextureHeader::DoLayout(UINT08* in, UINT32 size) const
{
    return m_layout->DoLayout(in, size);
}

bool TextureHeader::PixelOffset(UINT32 array_index, UINT32 lwbemap_face, UINT32 miplevel,
                                UINT32 x, UINT32 y, UINT32 z, UINT32* offset) const
{
    return m_layout->PixelOffset(array_index, lwbemap_face, miplevel, x, y, z, offset);
}

bool TextureHeader::SanityCheck() const
{
    return m_layout->SanityCheck();
}

UINT32 TextureHeader::Size() const
{
    return m_layout->Size();
}

void TextureHeader::BlockShrinkingSanityCheck(MdiagSurf* surface, GpuDevice* gpuDevice) const
{
    if (surface->GetWidth() <= (1u << surface->GetLogBlockWidth()) *
        gpuDevice->GobWidth() >> 1 &&
        surface->GetLogBlockWidth() != 0)
    {
        WarnPrintf("Surface %s is %d pixels wide, has block %d gobs wide and used as a texture\n",
            surface->GetName().c_str(), surface->GetWidth(),
            1 << surface->GetLogBlockWidth());
        WarnPrintf("Use -block_width to reduce block height\n");
    }

    if (surface->GetHeight() <= (1u << surface->GetLogBlockHeight()) *
        gpuDevice->GobHeight() >> 1 &&
        surface->GetLogBlockHeight() != 0)
    {
        WarnPrintf("Surface %s is %d pixels heigh, has block %d gobs heigh and used as a texture\n",
            surface->GetName().c_str(), surface->GetHeight(),
            1 << surface->GetLogBlockHeight());
        WarnPrintf("Use -block_height to reduce block height\n");
    }

    if (surface->GetDepth() <= (1u << surface->GetLogBlockDepth()) >> 1 &&
        surface->GetLogBlockDepth() != 0)
    {
        WarnPrintf("Surface %s is %d pixels depth, has block %d gobs depth and used as a texture\n",
            surface->GetName().c_str(), surface->GetDepth(),
            1 << surface->GetLogBlockDepth());
        WarnPrintf("Use -block_depth to reduce block depth\n");
    }
}

TextureHeader::TextureParamsBlockLinear::TextureParamsBlockLinear
(
    UINT32 bw,
    UINT32 bh,
    UINT32 bd,
    UINT32 tw,
    UINT32 th,
    UINT32 td,
    UINT32 ws,
    UINT32 hs,
    UINT32 ml,
    UINT32 minimumMipWidth,
    UINT32 minimumMipHeight,
    UINT32 tsize,
    UINT32 tscale,
    UINT32 ds,
    UINT32 as,
    UINT32 borderWidth,
    UINT32 borderHeight,
    UINT32 borderDepth,
    GpuDevice *pGpuDev
)
:   TextureParams(borderWidth, borderHeight, borderDepth),
    block_width(bw),
    block_height(bh),
    block_depth(bd),
    texture_width(tw),
    texture_height(th),
    texture_depth(td),
    width_scale(ws),
    height_scale(hs),
    mip_levels(ml),
    m_MinimumMipWidth(minimumMipWidth),
    m_MinimumMipHeight(minimumMipHeight),
    texel_size(tsize),
    texel_scale(tscale),
    dim_size(ds),
    array_size(as),
    m_miplevel_size(0),
    m_pGpuDevice(pGpuDev)
{
    UINT32 gob_width_texels    = m_pGpuDevice->GobWidth()/texel_size;
    UINT32 gob_height_texels   = m_pGpuDevice->GobHeight();

    // first we see if gived block width/height/depth can be shrunk for the base miplevel
    UINT32 tex_w_scaled = GetMipWidth(0);
    UINT32 block_width_texels  = block_width*gob_width_texels;
    while((block_width>>1)*gob_width_texels >= tex_w_scaled
            && (block_width>>1) != 0)
    {
        block_width >>= 1;
        block_width_texels  = block_width*gob_width_texels;
    }
    UINT32 tex_h_scaled = GetMipHeight(0);
    UINT32 block_height_texels = block_height*gob_height_texels;
    while((block_height>>1)*gob_height_texels >= tex_h_scaled
            && (block_height>>1) != 0)
    {
        block_height >>=1;
        block_height_texels = block_height*gob_height_texels;
    }
    UINT32 block_depth_texels = block_depth;
    while((block_depth>>1) >= (texture_depth + m_BorderDepth * 2)
            && (block_depth>>1) != 0)
    {
        block_depth >>=1;
        block_depth_texels = block_depth;
    }

    UINT32 shrunken_block_width = block_width;
    UINT32 shrunken_block_height= block_height;
    UINT32 shrunken_block_depth = block_depth;

    for (UINT32 i = 0; i < mip_levels; ++i)
    {
        UINT32 miplevel_width  = GetMipWidth(i) * texel_scale;
        UINT32 miplevel_height = GetMipHeight(i);
        UINT32 miplevel_depth  = GetMipDepth(i);

        // second we see if block width/height/depth can be shrunk for each miplevel
        while((shrunken_block_width>>1)*gob_width_texels >= miplevel_width &&
                (shrunken_block_width>>1) != 0)
            shrunken_block_width >>= 1;

        while((shrunken_block_height>>1)*gob_height_texels >= miplevel_height &&
                (shrunken_block_height>>1) != 0)
            shrunken_block_height >>= 1;

        while((shrunken_block_depth>>1) >= miplevel_depth &&
                (shrunken_block_depth>>1) != 0)
            shrunken_block_depth >>= 1;

        // creating BlockLinear object for every miplevel
        m_bl.push_back(new BlockLinear( miplevel_width,
                                        miplevel_height,
                                        miplevel_depth,
                                        log2(shrunken_block_width),
                                        log2(shrunken_block_height),
                                        log2(shrunken_block_depth),
                                        texel_size,
                                        m_pGpuDevice));

        m_miplevel_size += m_bl.back()->Size();
    }

    UINT32 block_size = block_width_texels*block_height_texels*block_depth_texels*texel_size;
    m_miplevel_size = ((m_miplevel_size + block_size - 1)/block_size)*block_size;
}

TextureHeader::TextureParamsBlockLinear::~TextureParamsBlockLinear()
{
    for (vector<BlockLinear*>::iterator i = m_bl.begin(); i != m_bl.end(); ++i)
        delete *i;
}

bool TextureHeader::TextureParamsBlockLinear::SanityCheck() const
{
    //dimensions should be 2^n
    if ((block_width & (block_width -1))  ||
        (block_height & (block_height-1)) ||
        (block_depth & (block_depth-1)))
        return false;

    if ((block_width  == 0) || (block_width  > 2)  ||
        (block_height == 0) || (block_height > 32) ||
        (block_depth  == 0) || (block_depth  > 32) )
        return false;

    if ((texel_size < 1) || (texel_size > 16))
        return false;

    return true;
}

TextureHeader::TextureParamsPitchLinear::TextureParamsPitchLinear
(
    UINT32 tw,
    UINT32 th,
    UINT32 widthScale,
    UINT32 heightScale,
    UINT32 p,
    UINT32 ts,
    UINT32 tsc,
    UINT32 borderWidth,
    UINT32 borderHeight,
    UINT32 borderDepth
)
:   TextureParams(borderWidth, borderHeight, borderDepth),
    texture_width(tw),
    texture_height(th),
    m_WidthScale(widthScale),
    m_HeightScale(heightScale),
    texture_pitch(p),
    texel_size(ts),
    texel_scale(tsc)
{
}

bool TextureHeader::TextureParamsPitchLinear::SanityCheck() const
{
    if ((texel_size < 1) || (texel_size > 16) || (texture_width == 0))
        return false;

    if ((m_WidthScale == 0) || (m_HeightScale == 0))
        return false;

    return true;
}

UINT32 TextureHeader::TextureParamsPitchLinear::Size() const
{
    return ScaledHeight() * ScaledPitch();
}

UINT32 TextureHeader::TextureParamsPitchLinear::ScaledWidth() const
{
    UINT32 scaledWidth = (texture_width + m_WidthScale - 1) / m_WidthScale;
    scaledWidth *= texel_size * texel_scale;
    return scaledWidth;
}

UINT32 TextureHeader::TextureParamsPitchLinear::ScaledHeight() const
{
    return (texture_height + m_HeightScale - 1) / m_HeightScale;
}

UINT32 TextureHeader::TextureParamsPitchLinear::ScaledPitch() const
{
    UINT32 scaledPitch = max(texture_pitch, ScaledWidth());
    scaledPitch = ((scaledPitch + pitch_alignment-1) & ~(pitch_alignment-1));
    return scaledPitch;
}

TextureHeader::L1_PROMOTION TextureHeader::GetOptimalSectorPromotion() const
{
    // read register to get dual channel configuration
    if (m_pGpuSubdevice->Is32bitChannel())
    {
        return L1_PROMOTION_NONE;
    }

    // Burst Length
    switch (m_pGpuSubdevice->GetFB()->GetBurstSize())
    {
        case 16:
            return L1_PROMOTION_NONE;
        case 32:
            break;
        case 0:
            DebugPrintf("TextureHeader::GetOptimalSectorPromotion: Burst length "
                        "is zero, skipping burst width check\n");
            break;
        default:
            MASSERT(0);
    }

    // Pitch texture
    if (!IsBlocklinear())
        return L1_PROMOTION_NONE;

    if(GetHeapType() == LWOS32_TYPE_DEPTH)
    {
        if (VCAADepthBuffer() && AllSourceInB())
        {
            // VCAA texture in VCAA mode
            return L1_PROMOTION_NONE;
        }

        return L1_PROMOTION_2V;
    }

    if(m_pGpuSubdevice->HasFeature(Device::GPUSUB_HAS_TEXTURE_COALESCER))
    {
        return L1_PROMOTION_NONE; //Let texture Coalescer do its work.
    }
    else
    {
        return L1_PROMOTION_2V;
    }
}

RC TextureHeader::ModifySectorPromotion
(
    UINT32* state,
    L1_PROMOTION newPromotion,
    HeaderFormat headerFormat
)
{
    RC rc;

    switch (headerFormat)
    {
        case HeaderFormat::Kepler:
            CHECK_RC(KeplerTextureHeader::ModifySectorPromotion((TextureHeaderState*) state, newPromotion));

        case HeaderFormat::Maxwell:
            CHECK_RC(MaxwellTextureHeader::ModifySectorPromotion((MaxwellTextureHeaderState*) state, newPromotion));
            break;

        case HeaderFormat::Hopper:
            CHECK_RC(HopperTextureHeader::ModifySectorPromotion((HopperTextureHeaderState*) state, newPromotion));
            break;

        default:
            MASSERT(!"Unrecognized texture header format");
            rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}
