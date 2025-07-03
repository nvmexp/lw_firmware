/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "surffmt.h"
#include "surf2d.h"
#include "blocklin.h"
#include "core/include/utility.h"
#include "core/include/rc.h"
#include "lwos.h"
#include "gpu/include/gpudev.h"

// Only makes sense for textures, for normal surfaces it's always 1
UINT32 SurfaceFormat::GetWidthScale()
{
    switch (m_Surface->GetColorFormat())
    {
        case ColorUtils::ASTC_2D_12X10:
        case ColorUtils::ASTC_2D_12X12:
        case ColorUtils::ASTC_SRGB_2D_12X10:
        case ColorUtils::ASTC_SRGB_2D_12X12:
            return 12;
        case ColorUtils::ASTC_2D_10X5:
        case ColorUtils::ASTC_2D_10X6:
        case ColorUtils::ASTC_2D_10X8:
        case ColorUtils::ASTC_2D_10X10:
        case ColorUtils::ASTC_SRGB_2D_10X5:
        case ColorUtils::ASTC_SRGB_2D_10X6:
        case ColorUtils::ASTC_SRGB_2D_10X8:
        case ColorUtils::ASTC_SRGB_2D_10X10:
            return 10;
        case ColorUtils::ASTC_2D_8X5:
        case ColorUtils::ASTC_2D_8X6:
        case ColorUtils::ASTC_2D_8X8:
        case ColorUtils::ASTC_SRGB_2D_8X5:
        case ColorUtils::ASTC_SRGB_2D_8X6:
        case ColorUtils::ASTC_SRGB_2D_8X8:
            return 8;
        case ColorUtils::ASTC_2D_6X5:
        case ColorUtils::ASTC_2D_6X6:
        case ColorUtils::ASTC_SRGB_2D_6X5:
        case ColorUtils::ASTC_SRGB_2D_6X6:
            return 6;
        case ColorUtils::ASTC_2D_5X4:
        case ColorUtils::ASTC_2D_5X5:
        case ColorUtils::ASTC_SRGB_2D_5X4:
        case ColorUtils::ASTC_SRGB_2D_5X5:
            return 5;
        case ColorUtils::DXT1:
        case ColorUtils::DXT23:
        case ColorUtils::DXT45:
        case ColorUtils::DXN1:
        case ColorUtils::DXN2:
        case ColorUtils::BC6H_SF16:
        case ColorUtils::BC6H_UF16:
        case ColorUtils::BC7U:
        case ColorUtils::ASTC_2D_4X4:
        case ColorUtils::ASTC_SRGB_2D_4X4:
        case ColorUtils::ETC2_RGB:
        case ColorUtils::ETC2_RGB_PTA:
        case ColorUtils::ETC2_RGBA:
        case ColorUtils::EAC:
        case ColorUtils::EACX2:
            return 4;
        case ColorUtils::R1:
            // Fermi does not support this format, treat it like A8R8G8B8
            return 1;
        case ColorUtils::G8B8G8R8:
        case ColorUtils::B8G8R8G8:
            return 2;
        default:
            return 1;
    }
    return 1;
}

// Only makes sense for textures, for normal surfaces it's always 1
UINT32 SurfaceFormat::GetHeightScale()
{
    switch (m_Surface->GetColorFormat())
    {
        case ColorUtils::ASTC_2D_12X12:
        case ColorUtils::ASTC_SRGB_2D_12X12:
            return 12;
        case ColorUtils::ASTC_2D_10X10:
        case ColorUtils::ASTC_2D_12X10:
        case ColorUtils::ASTC_SRGB_2D_10X10:
        case ColorUtils::ASTC_SRGB_2D_12X10:
            return 10;
        case ColorUtils::ASTC_2D_8X8:
        case ColorUtils::ASTC_2D_10X8:
        case ColorUtils::ASTC_SRGB_2D_8X8:
        case ColorUtils::ASTC_SRGB_2D_10X8:
            return 8;
        case ColorUtils::ASTC_2D_6X6:
        case ColorUtils::ASTC_2D_8X6:
        case ColorUtils::ASTC_2D_10X6:
        case ColorUtils::ASTC_SRGB_2D_6X6:
        case ColorUtils::ASTC_SRGB_2D_8X6:
        case ColorUtils::ASTC_SRGB_2D_10X6:
            return 6;
        case ColorUtils::ASTC_2D_5X5:
        case ColorUtils::ASTC_2D_6X5:
        case ColorUtils::ASTC_2D_8X5:
        case ColorUtils::ASTC_2D_10X5:
        case ColorUtils::ASTC_SRGB_2D_5X5:
        case ColorUtils::ASTC_SRGB_2D_6X5:
        case ColorUtils::ASTC_SRGB_2D_8X5:
        case ColorUtils::ASTC_SRGB_2D_10X5:
            return 5;
        case ColorUtils::DXT1:
        case ColorUtils::DXT23:
        case ColorUtils::DXT45:
        case ColorUtils::DXN1:
        case ColorUtils::DXN2:
        case ColorUtils::BC6H_SF16:
        case ColorUtils::BC6H_UF16:
        case ColorUtils::BC7U:
        case ColorUtils::ASTC_2D_4X4:
        case ColorUtils::ASTC_2D_5X4:
        case ColorUtils::ASTC_SRGB_2D_4X4:
        case ColorUtils::ASTC_SRGB_2D_5X4:
        case ColorUtils::ETC2_RGB:
        case ColorUtils::ETC2_RGB_PTA:
        case ColorUtils::ETC2_RGBA:
        case ColorUtils::EAC:
        case ColorUtils::EACX2:
            return 4;
        case ColorUtils::R1:
            // Fermi does not support this format, treat it like A8R8G8B8
            return 1;
        default:
            return 1;
    }
    return 1;
}

// Only makes sense for textures, for normal surfaces it's always 1
UINT32 SurfaceFormat::GetTexelScale()
{
    switch (m_Surface->GetColorFormat())
    {
        case ColorUtils::R32_G32_B32:
            return 3;
        default:
            return 1;
    }
    return 1;
}

// Release the memory used for DoLayout() and UnDoLayout()
void SurfaceFormat::ReleaseBuffer()
{
    if (m_BufLayouted)
    {
        delete[] m_BufLayouted;
        m_BufLayouted = 0;
    }
    if (m_BufUnLayouted)
    {
        delete[] m_BufUnLayouted;
        m_BufUnLayouted = 0;
    }
}

// Release ownership of buffer which has done layout
UINT08* SurfaceFormat::ReleaseOwnership()
{
    MASSERT(m_BufLayouted != nullptr);
    UINT08* ptr = m_BufLayouted;
    m_BufLayouted = 0;
    return ptr;
}

// Callwlate the buffer size to hold the surface in naive pitch
UINT32 SurfaceFormat::GetUnLayoutSize()
{
    if (m_UnLayoutSize > 0)
    {
        // The size has been callwlated
        return m_UnLayoutSize;
    }

    const Surface *surface = m_Surface;

    const UINT32 width  = surface->GetWidth() + surface->GetExtraWidth();  // AA scaled
    const UINT32 height = surface->GetHeight() + surface->GetExtraHeight(); // AA scaled
    const UINT32 depth  = surface->GetDepth();
    const UINT32 bpp = surface->GetBytesPerPixel();
    const UINT32 array_size = surface->GetArraySize();
    const UINT32 mip_levels = surface->GetMipLevels();
    const UINT32 dimensions = surface->GetDimensions(); // Lwbemap has 6 faces
    const UINT32 texel_scale = GetTexelScale();

    UINT32 level = 0;
    while ((level < mip_levels) || level == 0)
    {
        UINT32 l_width = GetMipWidth(width, level);
        UINT32 l_height = GetMipHeight(height, level);
        UINT32 l_depth = GetMipDepth(depth, level);
        m_UnLayoutSize += bpp * texel_scale * l_width * l_height * l_depth;
        level++;
    }
    m_UnLayoutSize *= array_size * dimensions;
    Printf(Tee::PriLow, "%s: naive pitch size 0x%x\n",
        surface->GetName().c_str(), m_UnLayoutSize);

    return m_UnLayoutSize;
}

RC SurfaceFormat::SetLayoutedBuf(const UINT08* layoutedBuf, UINT32 size)
{
    if (GetSize() != size)
    {
        Printf(Tee::PriHigh, "%s: argument buffer size(0x%x) does not match "
            "the callwlated one(0x%llx) required by this function.\n",
            __FUNCTION__, size, GetSize());
        return RC::SOFTWARE_ERROR;
    }

    if (!m_BufLayouted)
    {
        m_BufLayouted = new UINT08[size];
        MASSERT(m_BufLayouted);
    }

    memcpy(m_BufLayouted, layoutedBuf, size);

    return OK;
}

SurfaceFormat*
    SurfaceFormat::CreateFormat(Surface *surf, GpuDevice *gpuDev)
{
    SurfaceFormat *obj = 0;
    switch (surf->GetLayout())
    {
        case Surface::Pitch:
            obj = new SurfaceFormatPitch(surf, gpuDev);
            break;
        case Surface::BlockLinear:
            obj = new SurfaceFormatBL(surf, gpuDev);
            break;
        default:
            return 0;
    }
    return obj;
}

SurfaceFormat* SurfaceFormat::CreateFormat
(
    Surface *surf,
    GpuDevice *gpuDev,
    const UINT08* layoutedBuf,
    UINT32 size
)
{
    SurfaceFormat* obj = SurfaceFormat::CreateFormat(surf, gpuDev);

    if (obj && size > 0)
    {
        if (obj->SetLayoutedBuf(layoutedBuf, size) != OK)
        {
            delete obj;
            return 0;
        }
    }

    return obj;
}

UINT32 SurfaceFormat::GetScaledWidth()
{
    return CEIL_DIV(m_Surface->GetWidth() + m_Surface->GetExtraWidth(),
        GetWidthScale());
}

UINT32 SurfaceFormat::GetScaledHeight()
{
    return CEIL_DIV(m_Surface->GetHeight() + m_Surface->GetExtraHeight(),
        GetHeightScale());
}

UINT32 SurfaceFormat::GetMipWidth(UINT32 baseWidth, UINT32 level) const
{
    if (m_Surface->IsHtex())
    {
        return max(CEIL_DIV(baseWidth, 1 << level), 2U);
    }
    else
    {
        return max(baseWidth >> level, 1U);
    }
}

UINT32 SurfaceFormat::GetMipHeight(UINT32 baseHeight, UINT32 level) const
{
    if (m_Surface->IsHtex())
    {
        return max(CEIL_DIV(baseHeight, 1 << level), 2U);
    }
    else
    {
        return max(baseHeight >> level, 1U);
    }
}

UINT32 SurfaceFormat::GetMipDepth(UINT32 baseDepth, UINT32 level) const
{
    if (m_Surface->IsHtex())
    {
        UINT32 minDepth = (baseDepth > 1) ? 2 : 1;
        return max(CEIL_DIV(baseDepth, 1 << level), minDepth);
    }
    else
    {
        return max(baseDepth >> level, 1U);
    }
}

SurfaceFormatPitch::SurfaceFormatPitch
(
    Surface *surf,
    GpuDevice *gpuDev
)
    : SurfaceFormat(surf, gpuDev)
{
    if (LWOS32_TYPE_TEXTURE == surf->GetType())
    {
        // Textures are forced to align the pitch size
        m_PitchAlignment = 0x20;
    }
    else
    {
        // Other buffers need align pitch to 64 bytes
        m_PitchAlignment = 0x40;
    }
}

SurfaceFormatPitch::~SurfaceFormatPitch()
{
    ReleaseBuffer();
}

UINT64 SurfaceFormatPitch::GetSize()
{
    // Callwlate the surface size based on the arguments from ALLOC_SURFACE
    // This is important even when FILE argument is used for the buffer, we
    // can't simply get the file size and allocate corresponding buffer,
    // because trace file comes from diverse platforms(FakeGL/ACEx/CTL etc.),
    // each trace generator may has its own policy to store surfaces, in some
    // cases, the trace file size conflicts with the actual size needed in MODS
    if (m_Size > 0)
    {
        // The size has been callwlated
        return m_Size;
    }

    const Surface *surface = m_Surface;

    const UINT32 width  = surface->GetWidth() + surface->GetExtraWidth();  // AA scaled
    const UINT32 height = surface->GetHeight() + surface->GetExtraHeight(); // AA scaled
    const UINT32 depth  = surface->GetDepth();
    const UINT32 bpp = surface->GetBytesPerPixel();
    const UINT64 array_pitch = surface->GetArrayPitch() > 0 ?
                               surface->GetArrayPitch() : 1;
    const UINT32 array_size = surface->GetArraySize();
    const UINT32 mip_levels = surface->GetMipLevels();
    const UINT32 dimensions = surface->GetDimensions(); // Lwbemap has 6 layers
    UINT64 pitch = surface->GetPitch();
    const UINT32 width_scale = GetWidthScale();
    const UINT32 height_scale = GetHeightScale();
    const UINT32 texel_scale = GetTexelScale();

    if (0 == width || 0 == height)
    {
        // For some cases, trace only wants to allocate a buffer in a specified
        // size, so we don't need to callwlate anything
        return 0;
    }
    if (0 == pitch)
    {
        // If pitch size is explicitly specified, use it, otherwise align it
        pitch = ALIGN_UP(CEIL_DIV((UINT64)width, width_scale) * bpp * texel_scale, m_PitchAlignment);
    }
    if (LWOS32_TYPE_TEXTURE == surface->GetType())
    {
        // Textures need 0x20 alignment, even it's explicitly specified
        pitch = ALIGN_UP(pitch, m_PitchAlignment);
    }

    UINT32 level  = 0;
    while ((level < mip_levels) || level == 0)
    {
        UINT64 l_width = GetMipWidth(width, level);
        UINT64 l_height = GetMipHeight(height, level);
        UINT64 l_depth = GetMipDepth(depth, level);

        // Here we assume pitch size doesn't change for all mip levels
        // This not defined yet, in near future RT can be mip-mapped like
        // textures, but we're not sure if it's compatible with PITCH
        m_Size  += max(CEIL_DIV(l_width, width_scale) * bpp * texel_scale, pitch) * CEIL_DIV(l_height, height_scale) * l_depth;
        level++;
    }
    m_Size = ALIGN_UP(m_Size, array_pitch);
    m_Size *= array_size * dimensions;
    Printf(Tee::PriLow, "%s: pitch, callwlated size 0x%llx\n",
        surface->GetName().c_str(), m_Size);

    return m_Size;
}

UINT08* SurfaceFormatPitch::DoLayout(UINT08 *src, UINT32 size)
{
    const Surface *surface = m_Surface;

    const UINT32 width  = surface->GetWidth() + surface->GetExtraWidth();  // AA scaled
    const UINT32 height = surface->GetHeight() + surface->GetExtraHeight(); // AA scaled
    const UINT32 depth  = surface->GetDepth();
    const UINT32 bpp = surface->GetBytesPerPixel();
    const UINT32 array_size = surface->GetArraySize();
    const UINT32 mip_levels = surface->GetMipLevels();
    const UINT32 dimensions = surface->GetDimensions(); // Lwbemap has 6 layers
    UINT32 pitch = surface->GetPitch();
    const UINT32 width_scale = GetWidthScale();
    const UINT32 height_scale = GetHeightScale();
    const UINT32 texel_scale = GetTexelScale();

    if (0 == pitch)
    {
        // If pitch size is explicitly specified, use it, otherwise align it
        pitch = ALIGN_UP(width * bpp * texel_scale, m_PitchAlignment);
    }
    if (LWOS32_TYPE_TEXTURE == surface->GetType())
    {
        // Textures need 0x20 alignment, even it's explicitly specified
        pitch = ALIGN_UP(pitch, m_PitchAlignment);
    }

    UINT32 offset = 0;
    UINT32 src_offset = 0;
    if (m_BufLayouted)
    {
        delete[] m_BufLayouted;
        m_BufLayouted = 0;
    }
    m_BufLayouted = new UINT08[GetSize()];
    memset(m_BufLayouted, 0, GetSize());

    for (UINT32 array = 0; array < array_size; array++)
    {
        for (UINT32 dim = 0; dim < dimensions; dim++)
        {
            for (UINT32 level = 0; level < mip_levels; level++)
            {
                UINT32 l_w = CEIL_DIV(GetMipWidth(width, level), width_scale);
                UINT32 l_h = CEIL_DIV(GetMipHeight(height, level), height_scale);
                UINT32 l_d = GetMipDepth(depth, level);
                UINT32 copy_byte_per_line = l_w * bpp * texel_scale;
                UINT32 l_p = max(copy_byte_per_line, pitch);
                for (UINT32 d = 0; d < l_d; d++)
                {
                    for (UINT32 h = 0; h < l_h; h++)
                    {
                        if ((src_offset + copy_byte_per_line) > size)
                        {
                            copy_byte_per_line = size - src_offset;
                        }
                        memcpy(m_BufLayouted + offset, &src[src_offset],
                            copy_byte_per_line);
                        offset += l_p;
                        src_offset += copy_byte_per_line;
                    }
                }
            }
        }
    }

    return m_BufLayouted;
}

// Copy data from a surface with aligned layout to a naive pitch buffer
UINT08* SurfaceFormatPitch::UnDoLayout()
{
    const Surface *surface = m_Surface;

    const UINT32 size = GetUnLayoutSize();
    const UINT32 width  = surface->GetWidth() + surface->GetExtraWidth();  // AA scaled
    const UINT32 height = surface->GetHeight() + surface->GetExtraHeight(); // AA scaled
    const UINT32 depth  = surface->GetDepth();
    const UINT32 bpp = surface->GetBytesPerPixel();
    const UINT32 array_size = surface->GetArraySize();
    const UINT32 mip_levels = surface->GetMipLevels();
    const UINT32 dimensions = surface->GetDimensions(); // Lwbemap has 6 layers
    UINT32 pitch = surface->GetPitch();
    const UINT32 texel_scale = GetTexelScale();

    if (!m_BufLayouted)
    {
        Printf(Tee::PriHigh, "%s: DoLayout() is not called ever\n", __FUNCTION__);
        return 0;
    }

    if (0 == pitch)
    {
        // If pitch size is explicitly specified, use it, otherwise align it
        pitch = ALIGN_UP(width * bpp * texel_scale, m_PitchAlignment);
    }
    if (LWOS32_TYPE_TEXTURE == surface->GetType())
    {
        // Textures need 0x20 alignment, even it's explicitly specified
        pitch = ALIGN_UP(pitch, m_PitchAlignment);
    }

    UINT32 naive_offset = 0;
    UINT32 layout_offset = 0;
    if (m_BufUnLayouted)
    {
        delete[] m_BufUnLayouted;
        m_BufUnLayouted = 0;
    }
    m_BufUnLayouted = new UINT08[size];
    memset(m_BufUnLayouted, 0, size);

    for (UINT32 array = 0; array < array_size; array++)
    {
        for (UINT32 dim = 0; dim < dimensions; dim++)
        {
            for (UINT32 level = 0; level < mip_levels; level++)
            {
                UINT32 l_w = GetMipWidth(width, level);
                UINT32 l_h = GetMipHeight(height, level);
                UINT32 l_d = GetMipDepth(depth, level);
                UINT32 copy_byte_per_line = l_w * bpp * texel_scale;
                UINT32 l_p = max(copy_byte_per_line, pitch);
                for (UINT32 d = 0; d < l_d; d++)
                {
                    for (UINT32 h = 0; h < l_h; h++)
                    {
                        if ((naive_offset + copy_byte_per_line) > size)
                        {
                            copy_byte_per_line = size - naive_offset;
                        }
                        memcpy(m_BufUnLayouted + naive_offset,
                               m_BufLayouted +layout_offset, copy_byte_per_line);
                        layout_offset += l_p;
                        naive_offset += copy_byte_per_line;
                    }
                }
            }
        }
    }

    return m_BufUnLayouted;
}

RC SurfaceFormatPitch::Offset
(
    UINT64 *offset,
    UINT32 x, UINT32 y, UINT32 z,
    UINT32 level, UINT32 dim, UINT32 layer
)
{
    // Return an offset to the allocation start address for a given pixel
    *offset = 0;
    const Surface * surface = m_Surface;
    const UINT32 width = surface->GetWidth() + surface->GetExtraWidth();
    const UINT32 height = surface->GetHeight() + surface->GetExtraHeight();
    const UINT32 depth = surface->GetDepth();
    const UINT32 bpp = surface->GetBytesPerPixel();
    const UINT32 array_size = max(surface->GetArraySize(), UINT32(1));
    const UINT32 dim_size = max(surface->GetDimensions(), UINT32(1));
    const UINT32 mip_levels = max(surface->GetMipLevels(), UINT32(1));
    const UINT64 size = GetSize();
    const UINT64 array_pitch = size / array_size;
    const UINT64 dim_pitch = array_pitch / dim_size;
    UINT32 pitch = surface->GetPitch();
    const UINT32 texel_scale = GetTexelScale();
    if (0 == pitch)
    {
        pitch = ALIGN_UP(width * bpp * texel_scale, m_PitchAlignment);
    }
    if (LWOS32_TYPE_TEXTURE == surface->GetType())
    {
        pitch = ALIGN_UP(pitch, m_PitchAlignment);
    }

    // Sanity Check
    if (x >= width || y >= height || z >= depth || level >= mip_levels
        || dim >= dim_size || layer >= array_size)
    {
        Printf(Tee::PriHigh, "Invalid arguments: x=%d y=%d z=%d level=%d "
            "dim=%d layer=%d\n", x, y, z, level, dim, layer);
        return RC::SOFTWARE_ERROR;
    }

    *offset += array_pitch * layer;  // Find the layer from an array
    *offset += dim_pitch * dim;      // Find the 2d surface from a lwbemap

    // Add offset for upper levels
    for (UINT32 l = 0; l < level; l++)
    {
        UINT32 l_w = GetMipWidth(width, l);
        UINT32 l_h = GetMipHeight(height, l);
        UINT32 l_d = GetMipDepth(depth, l);
        UINT32 l_p = max(l_w * bpp * texel_scale, pitch);

        *offset += l_p * l_h * l_d;
    }

    // Offset in current level
    UINT32 lwrr_w = GetMipWidth(width, level);
    UINT32 lwrr_h = GetMipHeight(height, level);
    UINT32 lwrr_p = max(lwrr_w * bpp * texel_scale, pitch);
    *offset += z * lwrr_p * lwrr_h;
    *offset += y * lwrr_p;
    *offset += x * bpp * texel_scale;

    Printf(Tee::PriLow, "Pixel offset in %s: x=%-8d y=%-8d z=%-8d "
        "level=%-3d dim=%d layer=%-2d offset=0x%llx\n",
        surface->GetName().c_str(), x, y, z, level, dim, layer, *offset);

    return OK;
}

RC SurfaceFormatPitch::SanityCheck()
{
    if (m_Surface == 0 ||
        m_Surface->GetBytesPerPixel() == 0 ||
        (m_Surface->GetType() == LWOS32_TYPE_TEXTURE ||
         m_Surface->GetBytesPerPixel() > 16))
    {
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

SurfaceFormatBL::SurfaceFormatBL
(
    Surface *surf,
    GpuDevice *gpuDev
)
    : SurfaceFormat(surf, gpuDev), m_BufLevelSize(0)
{
    if (LWOS32_TYPE_TEXTURE == surf->GetType())
    {
        // Textures need to shrunk to right block
        m_Shrunk = true;
    }
    else
    {
        m_Shrunk = false;
    }
}

SurfaceFormatBL::~SurfaceFormatBL()
{
    UINT32 blSize = (UINT32)m_BL.size();
    for (UINT32 i = 0; i < blSize; ++i)
    {
        delete m_BL[i];
    }
}

// Callwlate the tile size for a sparse texture.
// This is used to align miplevels and array elements.
UINT32 SurfaceFormatBL::SparseTileAlignment()
{
    UINT32 alignment = m_GpuDevice->GobSize() *
        m_Surface->GetTileWidthInGobs() *
        (1 << m_Surface->GetLogBlockHeight()) *
        (1 << m_Surface->GetLogBlockDepth());

    return alignment;
}

// Align the miplevel width/height/depth of a sparse texture if necessary.
void SurfaceFormatBL::AlignSparseMipLevel(UINT32* width, UINT32* height,
    UINT32* depth)
{
    // Only need to align a miplevel if the tile size is greater than
    // a single gob.
    if (m_Surface->GetTileWidthInGobs() > 1)
    {
        UINT32 tileWidth = (64 / m_Surface->GetBytesPerPixel()) *
            m_Surface->GetTileWidthInGobs();
        UINT32 tileHeight = 8 * (1 << m_Surface->GetLogBlockHeight());
        UINT32 tileDepth = (1 << m_Surface->GetLogBlockDepth());

        // Mip-level alignment only applies if the width/height/depth
        // of the mip-level are each at least the size of the
        // width/height/depth of the tile.
        if ((*width >= tileWidth) &&
            (*height >= tileHeight) &&
            (*depth >= tileDepth))
        {
            *width = ALIGN_UP(*width, tileWidth);
            *height = ALIGN_UP(*height, tileHeight);
            *depth = ALIGN_UP(*depth, tileDepth);
        }
    }
}

// Align the array element size of a sparse texture if necessary.
UINT64 SurfaceFormatBL::AlignSparseArrayElement(UINT64 oldSize)
{
    UINT64 newSize = oldSize;

    if (((m_Surface->GetArraySize() > 1) || (m_Surface->GetDimensions() > 1)) &&
        (m_Surface->GetTileWidthInGobs() > 1))
    {
        newSize = ALIGN_UP(newSize, SparseTileAlignment());
    }

    return newSize;
}

UINT64 SurfaceFormatBL::GetSize()
{
    // Callwlate the surface size based on the arguments from ALLOC_SURFACE
    // This is important even when FILE argument is used for the buffer, we
    // can't simply get the file size and allocate corresponding buffer,
    // because trace file comes from diverse platforms(FakeGL/ACEx/CTL etc.),
    // each trace generator may has its own policy to store surfaces, in some
    // cases, the trace file size conflicts with the actual size needed in MODS
    if (m_Size > 0)
    {
        // The size has been callwlated
        return m_Size;
    }

    const Surface *surface = m_Surface;

    const UINT32 width  = surface->GetWidth() + surface->GetExtraWidth();  // AA scaled
    const UINT32 height = surface->GetHeight() + surface->GetExtraHeight(); // AA scaled
    const UINT32 depth  = surface->GetDepth();
    const UINT32 bpp = surface->GetBytesPerPixel();
    const UINT32 array_size = surface->GetArraySize();
    UINT32 block_width  = 1 << surface->GetLogBlockWidth();
    UINT32 block_height = 1 << surface->GetLogBlockHeight();
    UINT32 block_depth  = 1 << surface->GetLogBlockDepth();
    const UINT32 mip_levels = surface->GetMipLevels();
    const UINT32 border = surface->GetBorder();
    const UINT32 dimensions = surface->GetDimensions();
    const UINT32 width_scale = GetWidthScale();
    const UINT32 height_scale = GetHeightScale();
    const UINT32 texel_scale = GetTexelScale();

    if (0 == width || 0 == height)
    {
        // For some cases, trace only wants to allocate a buffer in a specified
        // size, so we don't need to callwlate anything
        return 0;
    }

    const UINT32 gob_w_pixels = m_GpuDevice->GobWidth() / bpp;
    const UINT32 gob_h_pixels = m_GpuDevice->GobHeight();

    // Check the given block_w/h/d can be shunk for base mip level
    const UINT32 scaled_w = CEIL_DIV(width, width_scale);
    UINT32 block_w_pixels = block_width * gob_w_pixels;
    while (m_Shrunk
           && (block_width >> 1) * gob_w_pixels >=
              (border>0 ? (2*m_Border_w + scaled_w) : scaled_w)
           && (block_width >> 1) != 0)
    {
        block_width >>= 1;
        block_w_pixels = block_width * gob_w_pixels;
    }
    const UINT32 scaled_h = CEIL_DIV(height, height_scale);
    UINT32 block_h_pixels = block_height * gob_h_pixels;
    while (m_Shrunk
           && (block_height >> 1) * gob_h_pixels >=
              (border>1 ? (2*m_Border_h + scaled_h) : scaled_h)
           && (block_height >> 1) != 0)
    {
        block_height >>= 1;
        block_h_pixels = block_height * gob_h_pixels;
    }
    UINT32 block_d_pixels = block_depth;
    while (m_Shrunk
           && (block_depth >> 1) >= (border>2 ? (2*m_Border_d + depth) : depth)
           && (block_depth >> 1) != 0)
    {
        block_depth >>= 1;
        block_d_pixels = block_depth;
    }

    UINT32 shrunken_block_width = block_width;
    UINT32 shrunken_block_height= block_height;
    UINT32 shrunken_block_depth = block_depth;
    m_BL.clear();
    m_BufLevelSize = 0;
    for (UINT32 i = 0; i < mip_levels; ++i)
    {
        UINT32 miplevel_width  = CEIL_DIV(GetMipWidth(width, i), width_scale);
        UINT32 miplevel_height = CEIL_DIV(GetMipHeight(height, i), height_scale);
        UINT32 miplevel_depth  = GetMipDepth(depth, i);

        if (border)
            miplevel_width += 2 * m_Border_w;
        miplevel_width *= texel_scale;
        if (border > 1)
            miplevel_height += 2 * m_Border_h;
        if (border > 2)
            miplevel_depth += 2 * m_Border_d;

        // Check if block width/height/depth can be shrunk for each miplevel
        if (m_Shrunk)
        {
            while((shrunken_block_width>>1)*gob_w_pixels >= miplevel_width &&
                  (shrunken_block_width>>1) != 0)
                shrunken_block_width >>= 1;
            while((shrunken_block_height>>1)*gob_h_pixels >= miplevel_height &&
                  (shrunken_block_height>>1) != 0)
                shrunken_block_height >>= 1;
            while((shrunken_block_depth>>1) >= miplevel_depth &&
                  (shrunken_block_depth>>1) != 0)
                shrunken_block_depth >>= 1;
        }

        if (surface->GetIsSparse())
        {
            AlignSparseMipLevel(&miplevel_width, &miplevel_height,
                &miplevel_depth);
        }

        // creating BlockLinear object for every miplevel
        m_BL.push_back(new BlockLinear(miplevel_width,
                                       miplevel_height,
                                       miplevel_depth,
                                       log2(shrunken_block_width),
                                       log2(shrunken_block_height),
                                       log2(shrunken_block_depth),
                                       bpp,
                                       m_GpuDevice));

        m_BufLevelSize += m_BL.back()->Size();
    }
    UINT32 block_size = block_w_pixels * block_h_pixels * block_d_pixels * bpp;
    m_BufLevelSize = ALIGN_UP(m_BufLevelSize, block_size);

    if (surface->GetIsSparse())
    {
        m_BufLevelSize = AlignSparseArrayElement(m_BufLevelSize);
    }

    m_Size = array_size * dimensions * m_BufLevelSize;

    Printf(Tee::PriLow, "%s: blocklinear, callwlated size 0x%llx\n",
        surface->GetName().c_str(), m_Size);

    return m_Size;
}

UINT08* SurfaceFormatBL::DoLayout(UINT08 *src, UINT32 size)
{
    if (size < GetSize())
    {
        Printf(Tee::PriHigh, "SurfaceFormatBL::DoLayout: found 0x%xB, expected 0x%llxB\n",
            size, GetSize());
    }

    const Surface *surface = m_Surface;

    const UINT32 width  = surface->GetWidth() + surface->GetExtraWidth();  // AA scaled
    const UINT32 height = surface->GetHeight() + surface->GetExtraHeight(); // AA scaled
    const UINT32 depth  = surface->GetDepth();
    const UINT32 bpp = surface->GetBytesPerPixel();
    const UINT32 array_size = surface->GetArraySize();
    const UINT32 gob_width = m_GpuDevice->GobWidth();
    const UINT32 mip_levels = surface->GetMipLevels();
    const UINT32 border = surface->GetBorder();
    const UINT32 dimensions = surface->GetDimensions();
    const UINT32 width_scale = GetWidthScale();
    const UINT32 height_scale = GetHeightScale();
    const UINT32 texel_scale = GetTexelScale();

    if (m_BufLayouted)
    {
        delete[] m_BufLayouted;
        m_BufLayouted = 0;
    }
    m_BufLayouted = new UINT08[GetSize()];
    memset(m_BufLayouted, 0, GetSize());

    UINT64 offset = 0;
    UINT64 src_offset = 0;
    for (UINT32 array = 0; array < array_size; array++)
    {
        for (UINT32 dim = 0; dim < dimensions; dim++)
        {
            UINT64 mip_base = 0;
            for (UINT32 level = 0; level < mip_levels; level++)
            {
                UINT32 l_w = CEIL_DIV(GetMipWidth(width, level), width_scale);
                UINT32 l_h = CEIL_DIV(GetMipHeight(height, level), height_scale);
                UINT32 l_d = GetMipDepth(depth, level);
                l_w = border > 0 ? 2 * m_Border_w + l_w : l_w;
                l_h = border > 1 ? 2 * m_Border_h + l_h : l_h;
                l_d = border > 2 ? 2 * m_Border_d + l_d : l_d;
                UINT32 w_in_bytes = l_w * bpp * texel_scale;
                UINT32 stride = min(w_in_bytes, gob_width);
                UINT32 l_offset = 0;
                for (UINT32 d = 0; d < l_d; d++)
                {
                    for (UINT32 h = 0; h < l_h; h++)
                    {
                        UINT32 w = 0;
                        while (w < w_in_bytes)
                        {
                            UINT32 bytes_to_copy = min(stride, w_in_bytes - w);
                            memcpy(m_BufLayouted + offset + mip_base +
                                m_BL[level]->Address(l_offset),
                                src + src_offset + l_offset,
                                bytes_to_copy);
                            w += bytes_to_copy;
                            l_offset += bytes_to_copy;
                        }
                    }
                }
                // Base address of next miplevel
                mip_base += m_BL[level]->Size();
                src_offset += l_w * l_h * l_d * bpp * texel_scale;
            }
            offset += m_BufLevelSize;
        }
    }

    return m_BufLayouted;
}

// Save surface to a naive pitch buffer
UINT08* SurfaceFormatBL::UnDoLayout()
{
    const Surface *surface = m_Surface;

    const UINT32 size = GetUnLayoutSize();
    const UINT32 width  = surface->GetWidth() + surface->GetExtraWidth();  // AA scaled
    const UINT32 height = surface->GetHeight() + surface->GetExtraHeight(); // AA scaled
    const UINT32 depth  = surface->GetDepth();
    const UINT32 bpp = surface->GetBytesPerPixel();
    const UINT32 array_size = surface->GetArraySize();
    const UINT32 gob_width = m_GpuDevice->GobWidth();
    const UINT32 mip_levels = surface->GetMipLevels();
    const UINT32 border = surface->GetBorder();
    const UINT32 dimensions = surface->GetDimensions();
    const UINT32 width_scale = GetWidthScale();
    const UINT32 height_scale = GetHeightScale();
    const UINT32 texel_scale = GetTexelScale();

    if (!m_BufLayouted)
    {
        Printf(Tee::PriHigh, "%s: DoLayout() is not called ever\n", __FUNCTION__);
        return 0;
    }

    if (m_BufUnLayouted)
    {
        delete[] m_BufUnLayouted;
        m_BufUnLayouted = 0;
    }
    m_BufUnLayouted = new UINT08[size];
    memset(m_BufUnLayouted, 0, size);

    UINT64 layout_offset = 0;
    UINT64 naive_offset = 0;
    for (UINT32 array = 0; array < array_size; array++)
    {
        for (UINT32 dim = 0; dim < dimensions; dim++)
        {
            UINT64 mip_base = 0;
            for (UINT32 level = 0; level < mip_levels; level++)
            {
                UINT32 l_w = CEIL_DIV(GetMipWidth(width, level), width_scale);
                UINT32 l_h = CEIL_DIV(GetMipHeight(height, level), height_scale);
                UINT32 l_d = GetMipDepth(depth, level);
                l_w = border > 0 ? 2 * m_Border_w + l_w : l_w;
                l_h = border > 1 ? 2 * m_Border_h + l_h : l_h;
                l_d = border > 2 ? 2 * m_Border_d + l_d : l_d;
                UINT32 w_in_bytes = l_w * bpp * texel_scale;
                UINT32 stride = min(w_in_bytes, gob_width);
                UINT32 l_offset = 0;
                for (UINT32 d = 0; d < l_d; d++)
                {
                    for (UINT32 h = 0; h < l_h; h++)
                    {
                        UINT32 w = 0;
                        while (w < w_in_bytes)
                        {
                            UINT32 bytes_to_copy = min(stride, w_in_bytes - w);
                            memcpy(m_BufUnLayouted + naive_offset + l_offset,
                                   m_BufLayouted + layout_offset + mip_base
                                    + m_BL[level]->Address(l_offset),
                                   bytes_to_copy);
                            w += bytes_to_copy;
                            l_offset += bytes_to_copy;
                        }
                    }
                }
                // Base address of next miplevel
                mip_base += m_BL[level]->Size();
                naive_offset += l_w * l_h * l_d * bpp * texel_scale;
            }
            layout_offset += m_BufLevelSize;
        }
    }

    return m_BufUnLayouted;
}

RC SurfaceFormatBL::Offset
(
    UINT64 *offset,
    UINT32 x, UINT32 y, UINT32 z,
    UINT32 level, UINT32 dim, UINT32 layer
)
{
     // Return an offset to the allocation start address for a given pixel
    *offset = 0;
    const Surface * surface = m_Surface;
    const UINT32 width = surface->GetWidth() + surface->GetExtraWidth();
    const UINT32 height = surface->GetHeight() + surface->GetExtraHeight();
    const UINT32 depth = surface->GetDepth();
    const UINT32 array_size = max(surface->GetArraySize(), UINT32(1));
    const UINT32 dim_size = max(surface->GetDimensions(), UINT32(1));
    const UINT32 mip_levels = max(surface->GetMipLevels(), UINT32(1));
    const UINT64 size = GetSize();
    const UINT64 array_pitch = size / array_size;
    const UINT64 dim_pitch = array_pitch / dim_size;

    // Sanity Check
    if (x >= width || y >= height || z >= depth || level >= mip_levels
        || dim >= dim_size || layer >= array_size)
    {
        Printf(Tee::PriHigh, "Invalid arguments: x=%d y=%d z=%d level=%d "
            "dim=%d layer=%d\n", x, y, z, level, dim, layer);
        return RC::SOFTWARE_ERROR;
    }

    // Add offset for layer
    *offset += array_pitch * layer;
    // Add offset for faces in a lwbemap
    *offset += dim_pitch * dim;
    // Add offset for each mipmap level
    for (UINT32 l = 0; l < level; l++)
    {
        *offset += m_BL[l]->Size();
    }

    // Adjust position based on scaling.
    x /= GetWidthScale();
    y /= GetHeightScale();

    // Add offset in current mip level
    *offset += m_BL[level]->Address(x, y, z);

    Printf(Tee::PriLow, "Pixel offset in %s: x=%-8d y=%-8d z=%-8d "
        "level=%-3d dim=%d layer=%-2d offset=0x%llx\n",
        surface->GetName().c_str(), x, y, z, level, dim, layer, *offset);

    return OK;
}

RC SurfaceFormatBL::SanityCheck()
{
    // TBD
    MASSERT(0);
    return OK;
}
