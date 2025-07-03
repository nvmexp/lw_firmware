/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "mdiag/tests/stdtest.h"

#include <math.h>

#include "verif2d.h"

#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/include/floorsweepimpl.h"
#include "gpu/utility/scopereg.h"

#include "class/cl9097.h" // FERMI_A
#include "ctrl/ctrl0041.h"

#include "mdiag/utils/raster_types.h"
#include "gpu/utility/blocklin.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "lwmisc.h"
#include "core/include/utility.h"
#include "core/include/chiplibtracecapture.h"

#include "fermi/gf100/dev_bus.h"
#include "fermi/gf100/dev_ram.h"

#include "mdiag/resource/lwgpu/dmard.h"

#define FERMI_DEFAULT_PITCH_ALIGNMENT 128  // Fermi: texture pitch 32B aligned(need to specify it when creating surfaces); default is 128B aligned

class GpuDevice; // Used only as a pointer in this file

map<string,V2dSurface::ClearPattern> *V2dSurface::m_clearPatternMap = NULL;

static void remap(map<const string, LW50Raster*> &rasters,
                  const string &to, const string &from) {

    LW50Raster* r = rasters[from];
    if (!r)
        ErrPrintf("undefined LW50Raster string: %s", from.c_str());
    else
        rasters[to] = r;
}

static map<const string, LW50Raster*> V2dCreateColorMap() {

    map<const string, LW50Raster*> rasters = CreateColorMap();

    // create some equivalences for the purpose of dumping .tga files:
    remap(rasters, "Y8",  "R8");
    remap(rasters, "AY8", "R8");
    remap(rasters, "Y16", "R16");
    remap(rasters, "Y32", "RU32");

    // TODO: change this from R8 to R1, once Konstantin implements R1
    remap(rasters, "Y1_8X8", "R8");

    // save these as A* images, so glivd will display the X channel
    remap(rasters, "O1R5G5B5", "A1R5G5B5");
    remap(rasters, "Z1R5G5B5", "A1R5G5B5");
    remap(rasters, "O8R8G8B8", "A8R8G8B8");
    remap(rasters, "Z8R8G8B8", "A8R8G8B8");
    remap(rasters, "X1R5G5B5_O1R5G5B5", "A1R5G5B5");
    remap(rasters, "X1R5G5B5_Z1R5G5B5", "A1R5G5B5");
    remap(rasters, "X8R8G8B8_O8R8G8B8", "A8R8G8B8");
    remap(rasters, "X8R8G8B8_Z8R8G8B8", "A8R8G8B8");
    remap(rasters, "X8B8G8R8_O8B8G8R8", "A8B8G8R8");
    remap(rasters, "X8B8G8R8_Z8B8G8R8", "A8B8G8R8");

    return rasters;
}

// v2d script blocklinear surface:
V2dSurface::V2dSurface(Verif2d *v2d,
                       UINT32 width, UINT32 height, UINT32 depth,
                       string format, Flags flags,
                       UINT64 offset, INT64 limit,
                       const string &where, ArgReader *pParams,
                       UINT32 log2blockHeight, UINT32 log2blockDepth,
                       Label label, UINT32 subdev)
{
    m_v2d = v2d;
    m_Log2blockHeight = log2blockHeight;
    m_Log2blockDepth = log2blockDepth;
    m_colorFormat = m_v2d->GetColorFormatFromString( format );
    m_bytesPerPixel = ColorFormatToSize( m_colorFormat );

    GpuDevice *pGpuDev = m_v2d->GetLWGpu()->GetGpuDevice();
    m_subdev = pGpuDev->FixSubdeviceInst(subdev);
    m_label = label;

    if (pParams->ParamPresent("-optimal_block_size") > 0)
    {
        if (m_label == NO_LABEL)
        {
            UINT32 block_size[3] = {1, 1, 1};
            // Bug 427305, it's legal to get 0 fb for UMA chips, so skip checking
            //MASSERT(pGpuDev->GetSubdevice(m_subdev)->GetFsImpl()->FbMask() != Gpu::IlwalidFbMask);
            BlockLinear::OptimalBlockSize2D(block_size, m_bytesPerPixel, height,
                                            depth, 9,
                                            Utility::CountBits(pGpuDev->GetSubdevice(m_subdev)->GetFsImpl()->FbMask()),
                                             pGpuDev);
            m_Log2blockHeight = log2(block_size[1]);
            m_Log2blockDepth = log2(block_size[2]);
        }
        else
            DebugPrintf("labeled surface ignors -optimal_block_size\n");
    }

    if (pParams->ParamPresent("-block_height") > 0)
    {
        // The -block_height argument only affects surfaces without a label.
        // See lwbug 387379
        if (m_label == NO_LABEL)
        {
            UINT32 bh = pParams->ParamUnsigned("-block_height", 1 << m_Log2blockHeight);
            m_Log2blockHeight = log2(bh);
        }
    }

    if (pParams->ParamPresent("-block_height_a") > 0)
    {
        if (m_label == LABEL_A)
        {
            UINT32 bh = pParams->ParamUnsigned("-block_height_a", 1 << m_Log2blockHeight);
            m_Log2blockHeight = log2(bh);
        }
        else if (m_label == NO_LABEL)
        {
            V2D_THROW("all surfaces must have label parameters when -block_height_a is specified");
        }
    }

    if (pParams->ParamPresent("-block_height_b") > 0)
    {
        if (m_label == LABEL_B)
        {
          UINT32 bh = pParams->ParamUnsigned("-block_height_b", 1 << m_Log2blockHeight);
            m_Log2blockHeight = log2(bh);
        }
        else if (m_label == NO_LABEL)
        {
            V2D_THROW("all surfaces must have label parameters when -block_height_b is specified");
        }
    }

    m_gpu = m_v2d->GetGpu();
    m_ch = m_v2d->GetChannel();
    m_width = width;
    m_height = height;
    m_depth = depth;
    m_colorFormatString = format;
    m_pitch = m_width * m_bytesPerPixel;
    m_preExisting = false;
    m_pParams = pParams;
    m_ctxDmaHandle = 0;
    m_mappingAddress = 0;

    UINT32 pitchMultiple = m_gpu->GetPitchMultiple();
    if ( (m_pitch % pitchMultiple ) != 0 )
    {
        m_pitch += pitchMultiple;
        m_pitch &= ~(pitchMultiple - 1);
    }

    m_blocklinear_map = new BlockLinear(m_pitch/m_bytesPerPixel, height, depth,
                                        0, m_Log2blockHeight, m_Log2blockDepth,
                                        m_bytesPerPixel,
                                        pGpuDev);

    m_preExisting = false;

    //block linear surface should be alligned to the whole number of blocks
    UINT32 blhl = pGpuDev->GobHeight() *
        (1 << m_Log2blockHeight);
    UINT32 bldl = 1 << m_Log2blockDepth;
    m_sizeInBytes = UINT64(m_pitch) * ((m_height + blhl - 1)/blhl)*blhl
                                    * ((m_depth  + bldl - 1)/bldl)*bldl;

    m_tiled = (flags & TILED) != 0;
    m_swizzled = (flags & SWIZZLED) != 0;
    MASSERT(!m_tiled);
    MASSERT(!m_swizzled);
    m_compressed = (flags & COMPRESSED) != 0;

    // tiling and swizzling not compatible with blocklinear:
    assert(!m_tiled);
    assert(!m_swizzled);

    m_where = where;
    m_offset = offset;
    m_limit = limit;
    m_status = NOT_READY;
    m_mapped = false;

    m_cacheMode = false;
}

// v2d script pitch surface:
V2dSurface::V2dSurface(Verif2d *v2d,
                       UINT32 width, UINT32 height,
                       string format, Flags flags,
                       UINT64 offset, INT64 limit, UINT32 pitch,
                       const string &where, ArgReader *pParams,
                       UINT32 subdev)
{
    m_blocklinear_map = NULL;
    m_Log2blockHeight = 0;
    m_Log2blockDepth = 0;
    m_label = NO_LABEL;

    m_v2d = v2d;
    m_gpu = m_v2d->GetGpu();
    m_ch = m_v2d->GetChannel();
    m_width = width;
    m_height = height;
    m_depth = 1;
    m_colorFormatString = format;
    m_colorFormat = m_v2d->GetColorFormatFromString( format );
    m_bytesPerPixel = ColorFormatToSize( m_colorFormat );
    m_preExisting = false;
    m_pParams = pParams;
    m_ctxDmaHandle = 0;
    m_mappingAddress = 0;

    UINT32 pitchMultiple;

    if ( m_v2d->m_classId != V2dClassObj::_FERMI_TWOD_A) {
        if ( pitch != 0 )
            m_pitch = pitch;
        else
            m_pitch = m_width * m_bytesPerPixel;

        pitchMultiple = m_gpu->GetPitchMultiple();

        if ( (m_pitch % pitchMultiple ) != 0 )
        {
            m_pitch += pitchMultiple;
            m_pitch &= ~(pitchMultiple - 1);
        }
    }
    else {

    // For Fermi 2D and mem2mem, pitch texture aligned to 32B while color target aligned to 128B
    // Mods/v2d support it by setting default pitch multiple as 128B, but not changing 32B-aligned pitch if specified by v2d script
    // so if SW wants 32B-aligned pitch texture, it must explicitly specify it in 2d script

        if ( pitch != 0 ) {  // pitch must be multiples of 32B or 128B, enforced by class method check
            m_pitch = pitch;
        }
        else {  // if pitch size not specified, get width and round it up to multiples of 128Byte
            m_pitch = m_width * m_bytesPerPixel;

            pitchMultiple = FERMI_DEFAULT_PITCH_ALIGNMENT;

            if ( (m_pitch % pitchMultiple ) != 0 )
            {
                m_pitch += pitchMultiple;
                m_pitch &= ~(pitchMultiple - 1);
            }
        }
        InfoPrintf("m_pitch=%d\n", m_pitch);
    }

    m_sizeInBytes = (UINT64)m_pitch * m_height;
    m_tiled = (flags & TILED) != 0;
    m_swizzled = (flags & SWIZZLED) != 0;
    MASSERT(!m_tiled);
    MASSERT(!m_swizzled);
    m_compressed = false;
    m_where = where;
    m_offset = offset;
    m_limit = limit;
    m_status = NOT_READY;
    m_mapped = false;

    m_subdev = m_v2d->GetLWGpu()->GetGpuDevice()->FixSubdeviceInst(subdev);

    m_cacheMode = false;
}

//
// create a V2dSurface for a surface which has already been created
// used by tc5d
V2dSurface::V2dSurface(Verif2d *v2d,
                       UINT32 width, UINT32 height, UINT32 depth,
                       string format, Flags flags,
                       UINT64 offset, UINT32 pitch,
                       uintptr_t baseAddr, UINT32 handle, bool preExisting,
                       const string &where, ArgReader *pParams,
                       UINT32 log2blockHeight, UINT32 log2blockDepth,
                       UINT32 subdev)
{
    m_v2d = v2d;
    m_Log2blockHeight = log2blockHeight;
    m_Log2blockDepth = log2blockDepth;
    m_colorFormat = m_v2d->GetColorFormatFromString( format );
    m_bytesPerPixel = ColorFormatToSize( m_colorFormat );

    GpuDevice *pGpuDev = m_v2d->GetLWGpu()->GetGpuDevice();
    m_subdev = pGpuDev->FixSubdeviceInst(subdev);
    m_label = NO_LABEL;

    if (pParams->ParamPresent("-optimal_block_size") > 0)
    {
        if (m_label == NO_LABEL)
        {
            UINT32 block_size[3] = {1, 1, 1};
            // Bug 427305, it's legal to get 0 fb for UMA chips, so skip checking
            //MASSERT(pGpuDev->GetSubdevice(m_subdev)->GetFsImpl()->FbMask() != Gpu::IlwalidFbMask);
            BlockLinear::OptimalBlockSize2D(block_size, m_bytesPerPixel, height,
                                            depth, 9,
                                            Utility::CountBits(pGpuDev->GetSubdevice(m_subdev)->GetFsImpl()->FbMask()),
                                            pGpuDev);
            m_Log2blockHeight = log2(block_size[1]);
            m_Log2blockDepth = log2(block_size[2]);
        }
        else
            DebugPrintf("labeled surface ignors -optimal_block_size\n");
    }

    if (pParams->ParamPresent("-block_height") > 0)
    {
        UINT32 bh = pParams->ParamUnsigned("-block_height", 1 << m_Log2blockHeight);
        m_Log2blockHeight = log2(bh);
    }

    m_gpu = m_v2d->GetGpu();
    m_ch = m_v2d->GetChannel();

    m_mappingAddress = reinterpret_cast<void*>(baseAddr);
    m_ctxDmaHandle = handle;
    m_dmaBuffer.SetLocation(Memory::Fb);

    m_width = width;
    m_height = height;
    m_depth = 1;
    m_colorFormatString = format;
    m_preExisting = preExisting;
    m_pParams = pParams;

    m_pitch = pitch;

    m_sizeInBytes = (UINT64)m_pitch * m_height;
    m_tiled = (flags & TILED) != 0;
    m_swizzled = (flags & SWIZZLED) != 0;
    MASSERT(!m_tiled);
    MASSERT(!m_swizzled);
    m_compressed = (flags & COMPRESSED) != 0;
    m_where = where;
    m_offset = offset;
    m_limit = -1;
    m_status = NOT_READY;

    // Note: We do this to avoid somebody calling MapBuffer() while
    // baseAddr was initially 0, which lwrrently is the case in
    // Trace5dTest::Rulw2dTest(). This is because the size of
    // m_dmaBuffer for a pre-existing surface is 0 and if we find
    // ourselves calling Map() we end up in a very uncomfortable situation.
    m_mapped = preExisting;

    m_blocklinear_map = new BlockLinear( m_pitch/m_bytesPerPixel,
                                         height,
                                         depth,
                                         0,
                                         m_Log2blockHeight,
                                         m_Log2blockDepth,
                                         m_bytesPerPixel,
                                         pGpuDev);

    m_cacheMode = false;
}

void V2dSurface::InitClearPatternMaps( void )
{
    // initialize static data member map:

    if ( m_clearPatternMap == NULL )
    {
        map<string,ClearPattern> *C = m_clearPatternMap =
            new map<string,ClearPattern>;

        (*C)[ "XY_COORD_GRID"          ] = V2dSurface::XY_COORD_GRID;
        (*C)[ "CONST"                  ] = V2dSurface::CONST;
        (*C)[ "GRAY_RAMP_HORZ"         ] = V2dSurface::GRAY_RAMP_HORZ;
        (*C)[ "GRAY_RAMP_VERT"         ] = V2dSurface::GRAY_RAMP_VERT;
        (*C)[ "GRAY_COARSE_RAMP_VERT"  ] = V2dSurface::GRAY_COARSE_RAMP_VERT;
        (*C)[ "GRAY_RAMP_CHECKER"      ] = V2dSurface::GRAY_RAMP_CHECKER;
        (*C)[ "RGB"                    ] = V2dSurface::RGB;
        (*C)[ "XY_ALL_COLORS"          ] = V2dSurface::XY_ALL_COLORS;
        (*C)[ "XY_ALL_COLORS2"         ] = V2dSurface::XY_ALL_COLORS2;
        (*C)[ "CORNER_DIFFERENTIAL"    ] = V2dSurface::CORNER_DIFFERENTIAL;
        (*C)[ "HORZ_DIFFERENTIAL"      ] = V2dSurface::HORZ_DIFFERENTIAL;
        (*C)[ "FONT_TEST_PATTERN"      ] = V2dSurface::FONT_TEST_PATTERN;

        (*C)[ "COLOR_RAMP_HORZ"        ] = V2dSurface::COLOR_RAMP_HORZ;
        (*C)[ "COLOR_RAMP_VERT"        ] = V2dSurface::COLOR_RAMP_VERT;
        (*C)[ "YUV_TEST_PATTERN1"      ] = V2dSurface::YUV_TEST_PATTERN1;
        (*C)[ "YUV_TEST_PATTERN2"      ] = V2dSurface::YUV_TEST_PATTERN2;
        (*C)[ "RANDOM"                 ] = V2dSurface::RANDOM;
        (*C)[ "BORDER"                 ] = V2dSurface::BORDER;
        (*C)[ "GPU"                    ] = V2dSurface::GPU;
        (*C)[ "ALPHA_CONST"            ] = V2dSurface::ALPHA_CONST;
        (*C)[ "ALPHA_RAMP_HORZ"        ] = V2dSurface::ALPHA_RAMP_HORZ;
        (*C)[ "ALPHA_RAMP_VERT"        ] = V2dSurface::ALPHA_RAMP_VERT;
        (*C)[ "ALPHA_RANDOM"           ] = V2dSurface::ALPHA_RANDOM;
        (*C)[ "ALPHA_BORDER"           ] = V2dSurface::ALPHA_BORDER;
    }
}

int V2dSurface::Granularity()
{
    if (m_lwrrRaster == NULL)
        V2D_THROW("V2dSurface::Granularity(): m_lwrrRaster is NULL");

    int g = m_lwrrRaster->Granularity();

    if (m_colorFormatString == "Y1_8X8")
        g = 1;

    return g;
}

V2dSurface::~V2dSurface()
{
    if (m_status == READY)
    {
        if (m_dmaBuffer.GetSize() != 0)
        {
            if (m_mapped && !m_preExisting)
            {
                m_dmaBuffer.Unmap();
            }

            m_dmaBuffer.Free();
        }
    }

    delete m_blocklinear_map;
}

int V2dSurface::ColorFormatToSize( ColorFormat format )
{
    int size;

    switch (static_cast<int>(format))
    {
     case A8R8G8B8:
     case A8RL8GL8BL8:
     case A8BL8GL8RL8:
     case X8RL8GL8BL8:
     case X8BL8GL8RL8:
     case Y32:
     case A8B8G8R8:
     case X8R8G8B8_Z8R8G8B8:
     case X8R8G8B8_O8R8G8B8:
     case X8B8G8R8_Z8B8G8R8:
     case X8B8G8R8_O8B8G8R8:
     case X1A7R8G8B8_Z1A7R8G8B8:
     case X1A7R8G8B8_O1A7R8G8B8:
     case A8CR8CB8Y8:
     case A2R10G10B10:
     case A2B10G10R10:
        size = 4;
        break;

     case Y16:
     case R5G6B5:
     case A1R5G5B5:
     case X1R5G5B5_Z1R5G5B5:
     case X1R5G5B5_O1R5G5B5:
     case CR8YB8CB8YA8: // 4 bytes per 2 pixels
     case YB8CR8YA8CB8: // 4 bytes per 2 pixels
     case A4CR6YB6A4CB6YA6: // 4 bytes per 2 pixels
        size = 2;
        break;

    case Y8:
    case AY8:
        size = 1;
        break;

    case Y1_8X8:
        size = 8;
        break;

    case LW9097_SET_COLOR_TARGET_FORMAT_V_RF32_GF32_BF32_AF32:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RS32_GS32_BS32_AS32:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RU32_GU32_BU32_AU32:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RF32_GF32_BF32_X32:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RS32_GS32_BS32_X32:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RU32_GU32_BU32_X32:

        size = 16;
        break;

    case LW9097_SET_COLOR_TARGET_FORMAT_V_R16_G16_B16_A16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RN16_GN16_BN16_AN16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RS16_GS16_BS16_AS16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RU16_GU16_BU16_AU16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RF16_GF16_BF16_AF16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RF32_GF32:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RS32_GS32:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RU32_GU32:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RF16_GF16_BF16_X16:

        size = 8;
        break;

    case LW9097_SET_COLOR_TARGET_FORMAT_V_A8R8G8B8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_A8RL8GL8BL8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_A2B10G10R10:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_AU2BU10GU10RU10:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_A8B8G8R8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_A8BL8GL8RL8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_AN8BN8GN8RN8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_AS8BS8GS8RS8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_AU8BU8GU8RU8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_R16_G16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RN16_GN16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RS16_GS16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RU16_GU16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RF16_GF16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_A2R10G10B10:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_BF10GF11RF11:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RS32:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RU32:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RF32:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_X8R8G8B8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_X8RL8GL8BL8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_X8B8G8R8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_X8BL8GL8RL8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_R32:

        size = 4;
        break;

    case LW9097_SET_COLOR_TARGET_FORMAT_V_R5G6B5:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_A1R5G5B5:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_G8R8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_GN8RN8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_GS8RS8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_GU8RU8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_R16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RN16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RS16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RU16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RF16:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_X1R5G5B5:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_Z1R5G5B5:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_O1R5G5B5:

        size = 2;
        break;

    case LW9097_SET_COLOR_TARGET_FORMAT_V_R8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RN8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RS8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_RU8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_A8:

        size = 1;
        break;

    case LW9097_SET_COLOR_TARGET_FORMAT_V_Z8R8G8B8:
    case LW9097_SET_COLOR_TARGET_FORMAT_V_O8R8G8B8:

        size = 4;
        break;

    case (LW9097_SET_ZT_FORMAT_V_Z16 + 0x100):

        size = 2;
        break;

    case (LW9097_SET_ZT_FORMAT_V_Z24S8 + 0x100):
    case (LW9097_SET_ZT_FORMAT_V_X8Z24 + 0x100):
    case (LW9097_SET_ZT_FORMAT_V_S8Z24 + 0x100):
    case (LW9097_SET_ZT_FORMAT_V_V8Z24 + 0x100):
    case (LW9097_SET_ZT_FORMAT_V_ZF32  + 0x100):

        size = 4;
        break;

    case (LW9097_SET_ZT_FORMAT_V_ZF32_X24S8 + 0x100):
    case (LW9097_SET_ZT_FORMAT_V_X8Z24_X16V8S8 + 0x100):
    case (LW9097_SET_ZT_FORMAT_V_ZF32_X16V8X8 + 0x100):
    case (LW9097_SET_ZT_FORMAT_V_ZF32_X16V8S8 + 0x100):

        size = 8;
        break;

    case LW9097_SET_COLOR_TARGET_FORMAT_V_DISABLED:
        V2D_THROW("V2dSurface: format disabled!\n");
        break;

    default:
        V2D_THROW( "V2dSurface::ColorFormatToSize() unknown format: "
                   << format );
    }

    return size;
}

void V2dSurface::Init( void )
{
    if (m_colorFormatString.size() == 0)
        V2D_THROW( "surface created without color format string" );

    LwRm* pLwRm = m_ch->GetLwRmPtr();
    m_rasters = V2dCreateColorMap();
    m_lwrrRaster = m_rasters[m_colorFormatString];

    InitClearPatternMaps();

    if ( m_status == READY )
        V2D_THROW( "surface is already ready for use!" );

    _DMA_TARGET target;
    if ( ! m_preExisting )
    {
        if ( m_where == "fb" )
            target = _DMA_TARGET_VIDEO;
        else if ( m_where == "vid" )
            target = _DMA_TARGET_VIDEO;
        else if ( m_where == "pci" )
            target = _DMA_TARGET_COHERENT;
        else if ( m_where == "coh" )
            target = _DMA_TARGET_COHERENT;
        else if ( m_where == "agp" )
            target = _DMA_TARGET_NONCOHERENT;
        else if ( m_where == "ncoh" )
            target = _DMA_TARGET_NONCOHERENT;
        else
            V2D_THROW( "unrecognized \"m_where\" parameter: \"" <<
                       m_where << '"' );

        if (m_pParams->ParamPresent("-loc") > 0)
            target = (_DMA_TARGET)m_pParams->ParamUnsigned("-loc");

        m_bl_attr = 0;
        if (m_blocklinear_map != NULL)
        {
            // surfaces with small heights are assumed to have blocks
            // no larger than necessary.  rather than silently adjust
            // the blockHeight, v2d prohibits these cases.
            unsigned int bh = (4 << m_Log2blockHeight);
            if (bh > 4 && (m_height*2 <= bh))
                V2D_THROW("illegal block height (" << (1<<m_Log2blockHeight) <<
                          ") for surface of height " << m_height);

            m_bl_attr |= DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR);
            switch (m_bytesPerPixel)
            {
                case  1: m_bl_attr |= DRF_DEF(OS32, _ATTR, _DEPTH,   _8); break;
                case  2: m_bl_attr |= DRF_DEF(OS32, _ATTR, _DEPTH,  _16); break;
                case  3: m_bl_attr |= DRF_DEF(OS32, _ATTR, _DEPTH,  _24); break;
                case  4: m_bl_attr |= DRF_DEF(OS32, _ATTR, _DEPTH,  _32); break;
                case  8: m_bl_attr |= DRF_DEF(OS32, _ATTR, _DEPTH,  _64); break;
                case 16: m_bl_attr |= DRF_DEF(OS32, _ATTR, _DEPTH, _128); break;
                default:
                    V2D_THROW( "surface creation failed: blocklinear does not support bpp=" << m_bytesPerPixel );
                    break;
            }
        }

        _PAGE_LAYOUT layout = GetPageLayoutFromParams(m_pParams, (char *)NULL);

        GpuDevice *pGpuDev = m_v2d->GetLWGpu()->GetGpuDevice();
        MASSERT(pGpuDev);
        UINT64 mask = pGpuDev->GobSize() - 1;
        m_sizeInBytes = (m_sizeInBytes + mask) & ~mask;

        m_dmaBuffer.SetAlignment(mask+1);
        m_dmaBuffer.SetArrayPitch(m_sizeInBytes);
        m_dmaBuffer.SetColorFormat(ColorUtils::Y8);
        m_dmaBuffer.SetForceSizeAlloc(true);
        m_dmaBuffer.SetLocation(TargetToLocation(target));
        SetPageLayout(m_dmaBuffer, layout);

        m_dmaBuffer.ConfigFromAttr(m_bl_attr);

        if (m_pParams->ParamPresent("-zbc_mode"))
        {
            if (m_pParams->ParamUnsigned("-zbc_mode") > 0)
            {
                m_dmaBuffer.SetZbcMode(Surface2D::ZbcOn);
            }
            else
            {
                m_dmaBuffer.SetZbcMode(Surface2D::ZbcOff);
            }
        }

        if (m_pParams->ParamPresent("-gpu_cache_mode"))
        {
            if (m_pParams->ParamUnsigned("-gpu_cache_mode") > 0)
            {
                m_dmaBuffer.SetGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else
            {
                m_dmaBuffer.SetGpuCacheMode(Surface2D::GpuCacheOff);
            }
        }

        if (m_pParams->ParamPresent("-gpu_p2p_cache_mode"))
        {
            if (m_pParams->ParamUnsigned("-gpu_p2p_cache_mode") > 0)
            {
                m_dmaBuffer.SetP2PGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else
            {
                m_dmaBuffer.SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
            }
        }

        if (m_pParams->ParamPresent("-split_gpu_cache_mode"))
        {
            if (m_pParams->ParamUnsigned("-split_gpu_cache_mode") > 0)
            {
                m_dmaBuffer.SetSplitGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else
            {
                m_dmaBuffer.SetSplitGpuCacheMode(Surface2D::GpuCacheOff);
            }
        }

        if (m_compressed || m_pParams->ParamPresent("-compress"))
        {
            m_dmaBuffer.SetCompressed(true);
            m_dmaBuffer.SetCompressedFlag(LWOS32_ATTR_COMPR_REQUIRED);
        }

        if (m_limit > -1)
            m_dmaBuffer.SetLimit(m_limit);

        if (m_dmaBuffer.Alloc(m_v2d->GetLWGpu()->GetGpuDevice(), pLwRm) != OK)
        {
            m_status = ERROR;
            V2D_THROW( "dma buffer allocation failed: width=" << m_width << "height=" << m_height <<
                       "bpp=" << m_bytesPerPixel << "tiled=" << m_tiled <<
                       "m_swizzled=" << m_swizzled );
        }
        PrintDmaBufferParams(m_dmaBuffer);
        if (m_ch && (m_dmaBuffer.BindGpuChannel(m_ch->ChannelHandle()) != OK))
        {
            m_status = ERROR;
            V2D_THROW("Unable to bind DmaBuffer to channel");
        }
        m_ctxDmaHandle = m_dmaBuffer.GetCtxDmaHandle();
        m_offset = m_dmaBuffer.GetCtxDmaOffsetGpu() + m_dmaBuffer.GetExtraAllocSize();
    }

    char bl_config[32];
    if (m_blocklinear_map)
        sprintf(bl_config, " (%dx%dx%d)", 1 << m_blocklinear_map->Log2BlockWidthGob(),
                                          1 << m_blocklinear_map->Log2BlockHeightGob(),
                                          1 << m_blocklinear_map->Log2BlockDepthGob());

    char aperture[32];
    switch (m_dmaBuffer.GetLocation())
    {
        case Memory::Fb:
            sprintf(aperture, "video");
            break;
        case Memory::Coherent:
            sprintf(aperture, "coherent");
            break;
        case Memory::NonCoherent:
            sprintf(aperture, "non-coherent");
            break;
        default:
            V2D_THROW("Unknown aperture\n");
    }

    string pa;
    if (m_dmaBuffer.GetLocation() == Memory::Fb)
    {
#ifdef LW_VERIF_FEATURES
        if (m_dmaBuffer.GetMemHandle())
        {
            LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
            params.memOffset = 0;

            RC rc = pLwRm->Control( m_dmaBuffer.GetMemHandle(), LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                    &params, sizeof(params));
            if (rc != OK)
            {
                ErrPrintf("%s\n", rc.Message());
                MASSERT(0);
            }
            pa = Utility::StrPrintf("0x%llx", params.memOffset);
        }
        else
#endif
            pa = "N/A";
    }
    else
    {
        pa = "{bug 397074}";
    }

    InfoPrintf( "V2dSurface::Init: created surface: width=%d, height=%d, bpp=%d, baseAddr=%p"
                ", VA/Offset=0x%llx"
                ", PhysAddr=%s, sizeInBytes=%lld, tiled=%d, swizzled=%d, where=%s, memorylayout=%s%s"
                ", format=%s, aperture=%s\n",
                m_width, m_height, m_bytesPerPixel, m_mappingAddress,
                m_dmaBuffer.GetCtxDmaOffsetGpu()+m_dmaBuffer.GetExtraAllocSize(),
                pa.c_str(),
                m_sizeInBytes,
                m_tiled, m_swizzled, m_where.c_str(),
                m_blocklinear_map ? "BLOCKLINEAR" : "PITCH",
                m_blocklinear_map ? bl_config : "",
                m_colorFormatString.c_str(), aperture);

    m_v2d->AddHandleObjectPairSurf(m_preExisting ? m_ctxDmaHandle :
                                                   m_dmaBuffer.GetMemHandle(), this);

    m_status = READY;
}

void V2dSurface::MapBuffer()
{
    if (m_mapped)
        return;

    InfoPrintf("memory mapping v2d surface, %dx%d\n",
               m_width, m_height);

    if (m_dmaBuffer.Map() != OK)
    {
        m_status = ERROR;
        V2D_THROW("dma buffer mapping failed: width=" << m_width <<
                  "height=" << m_height << "bpp=" << m_bytesPerPixel <<
                  "tiled=" << m_tiled << "m_swizzled=" << m_swizzled);
    }
    m_mappingAddress = m_dmaBuffer.GetAddress();

    m_mapped = true;
};

void V2dSurface::SaveToTGAfile( string fileName, UINT32 layer )
{
    m_v2d->WaitForIdle();

    if (!m_lwrrRaster) {
        ErrPrintf("V2dSurface::SaveToTGAfile: format not supported: %s\n",
                  m_colorFormatString.c_str());
        return;
    }
    if (layer > m_depth)
        V2D_THROW( "V2dSurface::SaveToTGAfile: reqest to dump layer " << layer <<
                " when the surface has only " << m_depth << " layers");

    vector<UINT08> pitchBuffer;
    UINT32 pitch;
    ReadSurfaceToPitch(pitchBuffer, pitch);

    m_v2d->m_buf->SetConfigBuffer(new TeslaBufferConfig);
    InfoPrintf("V2dSurface::SaveToTGAfile: saving blocklinear surface\n");

    if (m_colorFormat == Y1_8X8) {
        // when we save Y1_8X8, the buffer is organized as
        // 64-bit 8x8 blocks, and we save it as 8-bit data
        // so, we divide pitch by 8 and multiply height by 8
        // mods will rearrange bytes to linearize the data,
        // and glivd will expand each byte into 8 pixels

        vector<UINT08> pitchBuffer2(m_width * m_height * 64);
        for (unsigned int i=0; i<m_width*m_height*64; i++)
            pitchBuffer2[i] = 0x80;

        for (unsigned int jj=0; jj<m_height*8; jj++)
            for (unsigned int ii=0; ii<m_width*8; ii++) {
                int b = (jj/8)*m_pitch + (ii/8)*8;
                b += jj%8;

                if (pitchBuffer[b] & (1 << (ii%8)))
                    pitchBuffer2[jj*m_width*8+ii] = 0xff;
                else
                    pitchBuffer2[jj*m_width*8+ii] = 0x00;
            }

        m_v2d->m_buf->SaveBufTGA( fileName.c_str(),
                m_width*8, m_height*8, (uintptr_t) &pitchBuffer2[0], m_width*8,
                1,
                CTRaster(RASTER_LW50, m_lwrrRaster),
                m_swizzled);
    }
    else
    {
        m_v2d->m_buf->SaveBufTGA( fileName.c_str(),
                m_width, m_height, (uintptr_t) &pitchBuffer[0],
                pitch, m_bytesPerPixel,
                CTRaster(RASTER_LW50, m_lwrrRaster),
                m_swizzled);
    }
}

void V2dSurface::LoadFromTGAfile( string fileName )
{
    // if into layer 0, if depth>1

    unsigned int i, j;
    unsigned char ch;
    ifstream iFile( fileName.c_str(), ios::in );

    if ( !iFile )
    {
        V2D_THROW( "V2dSurface::LoadFromTGAfile() unable to open file "
                   << fileName << endl
                   << "Failed reason: "
                   << Utility::InterpretFileError().Message()) ;
    }

    // surfaces are only memory mapped when necessary
    // since this function calls SetPixel(), it is necessary now
    MapBuffer();

    unsigned int id_length = iFile.get();
    int has_color_map = iFile.get();
    int image_type_code = iFile.get();

    for ( i = 0; i < 5; i++ )
        ch = iFile.get();

    ch = iFile.get();
    int origin_x = iFile.get() * 256 + ch;
    ch = iFile.get();
    int origin_y = iFile.get() * 256 + ch;
    ch = iFile.get();
    unsigned int width = iFile.get() * 256 + ch;
    ch = iFile.get();
    unsigned int height = iFile.get() * 256 + ch;
    int bpp = iFile.get();
    int image_descriptor_byte = iFile.get();

    int attr_bits = image_descriptor_byte & 0x0f;
    int screen_origin = (image_descriptor_byte & 0x20) >> 5;
    int interleaving = (image_descriptor_byte & 0xc0) >> 6;

    for ( i = 0; i < id_length; i++ )
        ch = iFile.get();

    if ( has_color_map )
        V2D_THROW( "V2dSurface::LoadFromTGAfile(): color maps not supported" );
    if ( origin_x != 0 )
        V2D_THROW( "V2dSurface::LoadFromTGAfile(): origin_x != 0" );
    if ( origin_y != 0 )
        V2D_THROW( "V2dSurface::LoadFromTGAfile(): origin_y != 0" );
    if ( bpp != 24 && bpp != 32 )
        V2D_THROW( "V2dSurface::LoadFromTGAfile(): bpp == " << bpp );
    if ( !((bpp == 24 && attr_bits == 0) ||
           (bpp == 32 && attr_bits == 0) ||
           (bpp == 32 && attr_bits == 8)))
        V2D_THROW( "V2dSurface::LoadFromTGAfile(): bpp == " << bpp <<
                   " and attr_bits == " << attr_bits );
    if ( interleaving != 0 )
        V2D_THROW( "V2dSurface::LoadFromTGAfile(): interleaving == "
                   << interleaving );

    if ( image_type_code == 2 )
    {
        for ( j = 0; j < height; j++ )
            for ( i = 0; i < width; i++ )
            {
                unsigned int b = iFile.get();
                unsigned int g = iFile.get();
                unsigned int r = iFile.get();
                unsigned int a = (bpp == 32) ? iFile.get() : 0;
                if ( i < m_width && j < m_height )
                {
                    if ( screen_origin )
                        SetPixel8888( i, j, 0, a, r, g, b );
                    else
                        SetPixel8888( i, height-j-1, 0, a, r, g, b );
                }
            }
    }
    else if ( image_type_code == 10 )
    {
        int remaining=0;
        unsigned int r=0, g=0, b=0, a=0, rle=0;
        for ( j = 0; j < height; j++ )
            for ( i = 0; i < width; i++ )
            {
                if (remaining == 0)
                {
                    int header = iFile.get();
                    rle = (header > 127);
                    remaining = (header & 0x7f) + 1;
                    if ( rle )
                    {
                        b = iFile.get();
                        g = iFile.get();
                        r = iFile.get();
                        a = (bpp == 32) ? iFile.get() : 0;
                    }
                }
                if ( !rle )
                {
                    b = iFile.get();
                    g = iFile.get();
                    r = iFile.get();
                    a = (bpp == 32) ? iFile.get() : 0;
                }
                if ( i < m_width && j < m_height )
                {
                    if ( screen_origin )
                        SetPixel8888( i, j, 0, a, r, g, b );
                    else
                        SetPixel8888( i, height-j-1, 0, a, r, g, b );
                }
                remaining--;
            }
    }
    else
        V2D_THROW( "V2dSurface::LoadFromTGAfile(): image_type_code == "
                   << image_type_code );

    iFile.close();
}

void V2dSurface::FlushCache()
{
    MASSERT(m_cacheBuf.size() > 0);
    UINT32 size = LwU64_LO32(m_dmaBuffer.GetAllocSize());

    //Colwert naive bl to xbar raw
    if(m_dmaBuffer.MemAccessNeedsColwersion(m_pParams, MdiagSurf::WrSurf))
    {
        GpuDevice *pGpuDev = m_v2d->GetLWGpu()->GetGpuDevice();
        MASSERT(pGpuDev);
        PixelFormatTransform::ColwertBetweenRawAndNaiveBlSurf(&m_cacheBuf[0], size,
                                                              PixelFormatTransform::NAIVE_BL,
                                                              PixelFormatTransform::XBAR_RAW, pGpuDev);
        DebugPrintf("%s Colwert v2d surface from naive bl to xbar_raw bl.\n", __FUNCTION__);
    }

    Platform::VirtualWr(m_mappingAddress, &m_cacheBuf[0], size);
    DebugPrintf("Cache buffer flushed to surface %s\n",
        m_dmaBuffer.GetName().c_str());
}

void V2dSurface::InitCache(ClearPattern& p)
{
    UINT32 size = LwU64_LO32(m_dmaBuffer.GetAllocSize());
    m_cacheBuf.resize(size);

    switch (p)
    {
        case ALPHA_CONST:
        case ALPHA_RAMP_HORZ:
        case ALPHA_RAMP_VERT:
        case ALPHA_RANDOM:
        case ALPHA_BORDER:
            // Alpha operation depends on the existing data in fb so we have to
            // load data from real surface to cache buffer
            Platform::VirtualRd(m_mappingAddress, &m_cacheBuf[0], size);
            if(m_dmaBuffer.MemAccessNeedsColwersion(m_pParams, MdiagSurf::WrSurf))
            {
                GpuDevice *pGpuDev = m_v2d->GetLWGpu()->GetGpuDevice();
                MASSERT(pGpuDev);
                PixelFormatTransform::ColwertBetweenRawAndNaiveBlSurf(&m_cacheBuf[0], size,
                                                                      PixelFormatTransform::XBAR_RAW,
                                                                      PixelFormatTransform::NAIVE_BL, pGpuDev);
                DebugPrintf("%s Colwert v2d surface from naive bl to xbar_raw bl.\n", __FUNCTION__);
            }
            break;
        default:
            fill(m_cacheBuf.begin(), m_cacheBuf.end(), 0);
    }
}

void V2dSurface::Clear( string clearPattern, vector<UINT32> clearData )
{
    if ( m_clearPatternMap->find( clearPattern) == m_clearPatternMap->end() )
        V2D_THROW( "illegal surface clear pattern: " << clearPattern);

    // surfaces are only memory mapped when necessary
    // since this function calls SetPixel(), it is necessary now
    MapBuffer();

    unsigned int x, y, z;
    ClearPattern p = (*m_clearPatternMap)[clearPattern];

    UINT32 data0 = (clearData.size() >= 1) ? clearData[0] : 0;
    UINT32 data1 = (clearData.size() >= 2) ? clearData[1] : 0;
    UINT32 data2 = (clearData.size() >= 3) ? clearData[2] : 0;
    UINT32 data3 = (clearData.size() >= 4) ? clearData[3] : 0;

    // In some cases, MODS accesses fb in an unexpected granularity, which might
    // lead unexpected error and the frequent memory access is very inefficient.
    // As we've dislwssed in bug 723990, we need a cache mechanism.
    // In cache mode, all the pixel accesses are targeted to a cache buffer, and
    // once all pixels are done, we flush the cache to real buffer.
    m_cacheMode = true;
    InitCache(p);

    switch( p )
    {
        case XY_COORD_GRID:
            Clear_XY_Coord_Grid();
            break;
        case CORNER_DIFFERENTIAL:
            Clear_Differential(true,(UINT08)data0,(UINT08)data1,(UINT08)data2 ,(UINT08) data3);
            break;
        case HORZ_DIFFERENTIAL:
            Clear_Differential(false,(UINT08)data0,(UINT08)data1,(UINT08)data2 ,(UINT08) data3);
            break;
        case FONT_TEST_PATTERN:
            Clear_Font_Test_Pattern(data0);
            break;
        case XY_ALL_COLORS:
            Clear_XY_All_Colors(data0);
            break;
        case XY_ALL_COLORS2:
            Clear_XY_All_Colors2(data0);
            break;
        case CONST:
            for ( z = 0; z < m_depth; z++ )
                for ( y = 0; y < m_height; y++ )
                    for ( x = 0; x < m_width; x++ )
                    {
                        switch (m_bytesPerPixel)
                        {
                            case 16:
                                SetPixel_16byte( x, y, z, data0, data1, data2, data3);
                                break;
                            case 8:
                                SetPixel_8byte( x, y, z, data0, data1, 4);
                                break;
                            default:
                                SetPixel8888( x, y, z, ( data0 >> 24 ) & 0xff,
                                                       ( data0 >> 16 ) & 0xff,
                                                       ( data0 >>  8 ) & 0xff,
                                                       ( data0       ) & 0xff );
                        }
                    }
            break;

        case GRAY_RAMP_HORZ:
            if(data0==0)
                 data0=1;//set default scale = 1
            Clear_Gray_Ramp( false, true, false,false,data0 );
            break;

        case GRAY_RAMP_VERT:
            if(data0==0)
                 data0=1;//set default scale = 1
            Clear_Gray_Ramp( true, true, false,false,data0 );
            break;

        case GRAY_COARSE_RAMP_VERT:
            Clear_Gray_Ramp( true, true, false, true );
            break;

        case GRAY_RAMP_CHECKER:
            Clear_Gray_Ramp_Checker( true, false );
            break;

        case RGB:
            Clear_RGB();
            break;

        case COLOR_RAMP_HORZ:
            Clear_Color_Ramp(false);
            break;

        case COLOR_RAMP_VERT:
            Clear_Color_Ramp(true);
            break;

        case YUV_TEST_PATTERN1:
            Clear_YUV_Test_Pattern( 1 );
            break;

        case YUV_TEST_PATTERN2:
            Clear_YUV_Test_Pattern( 2 );
            break;

        case RANDOM:
            for ( z = 0; z < m_depth; z++ )
              for ( y = 0; y < m_height; y++ )
                for ( x = 0; x < m_width; x++ )
                {
                    // machine/compiler dependent, parameters could be evaluated
                    // in any order :(
                    // TODO: determine what order they are being evaluated
                    // in, and then move Random() calls outside, preserving
                    // that order
                    SetPixel8888( x, y, z, m_v2d->Random() & 0xff,
                                           m_v2d->Random() & 0xff,
                                           m_v2d->Random() & 0xff,
                                           m_v2d->Random() & 0xff );
                }
            break;

        case BORDER:
            for ( z = 0; z < m_depth; z++ )
            {
              for ( x = 0; x < m_width; x++ )
                for ( y = 0; y < m_height; y += (m_height-1) )
                    SetPixel8888( x, y, z, ( data0 >> 24 ) & 0xff,
                                           ( data0 >> 16 ) & 0xff,
                                           ( data0 >>  8 ) & 0xff,
                                           ( data0       ) & 0xff );
              for ( x = 0; x < m_width; x += (m_width-1) )
                for ( y = 0; y < m_height; y++ )
                    SetPixel8888( x, y, z, ( data0 >> 24 ) & 0xff,
                                           ( data0 >> 16 ) & 0xff,
                                           ( data0 >>  8 ) & 0xff,
                                           ( data0       ) & 0xff );
            }
            break;

        case ALPHA_CONST:
            for ( z = 0; z < m_depth; z++ )
              for ( y = 0; y < m_height; y++ )
                for ( x = 0; x < m_width; x++ )
                    SetPixelAlpha( x, y, z, data0&0xff );
            break;

        case ALPHA_RAMP_HORZ:
            if(data0==0)
                 data0=1;//set default scale = 1
            Clear_Gray_Ramp( false, false, true,data0 );
            break;

        case ALPHA_RAMP_VERT:
            if(data0==0)
                 data0=1;//set default scale = 1

            Clear_Gray_Ramp( true, false, true,data0 );
            break;

        case ALPHA_RANDOM:
            for ( z = 0; z < m_depth; z++ )
              for ( y = 0; y < m_height; y++ )
                for ( x = 0; x < m_width; x++ )
                    SetPixelAlpha( x, y, z, m_v2d->Random() & 0xff );
            break;

        case ALPHA_BORDER:
            for ( z = 0; z < m_depth; z++ )
            {
              for ( y = 0; y < m_height; y += (m_height-1) )
                for ( x = 0; x < m_width; x++ )
                    SetPixelAlpha( x, y, z, data0&0xff );
              for ( y = 0; y < m_height; y++ )
                for ( x = 0; x < m_width; x += (m_width-1) )
                    SetPixelAlpha( x, y, z, data0&0xff );
            }
            break;

     case V2dSurface::GPU:
        for ( z = 0; z < m_depth; z++ )
          for ( y = 0; y < m_height; y++ )
            for ( x = 0; x < m_width; x++ )
            {
                // now that shadow surfaces are gone, this
                // clear pattern doesn't have to do anything
                //
                //data0 = GetPixel( x, y, z );
                //SetPixel( x, y, z, data0 );
            }
        break;

        default:
            V2D_THROW( "unknown surface clear pattern type: " << p );
    }

    // In cached write mode, all write operations are aclwmulated in m_cacheBuf,
    // we need to flush the buffer to fb
    if (m_cacheMode)
    {
        FlushCache();
        // Cache mode is disabled by default, each time we enable cache mode we
        // should make the access atomic and set cache mode disabled
        m_cacheMode = false;
    }
}

void V2dSurface::Clear_XY_Coord_Grid( void )
{
    UINT32 x, y, z;

    for ( z = 0; z < m_depth; z++ )
      for ( y = 0; y < m_height; y++ )
        for ( x = 0; x < m_width; x++ )
        {
            switch ( m_bytesPerPixel )
            {
             // it may be tempting to incorporate z into the formulas
             // below, but that would change existing CRCs
             case 4:
                SetPixel( x, y, z, (x << 16) | (y & 0xFFFF ) );
                break;
             case 2:
                SetPixel( x, y, z, ((x & 0xFF) << 8) | (y & 0xFF) );
                break;
             case 1:
                SetPixel( x, y, z, ((x & 0xF) << 4) | (y & 0xF) );
                break;
            }
        }
}

void V2dSurface::Clear_Differential(bool corner, UINT08 da, UINT08 dr,UINT08 dg, UINT08 db )
{
    UINT32 x, y,z;
    UINT08 a,r,g,b;

    a = 0x00; r = 0x00; g = 0x00; b = 0x00;

    if(corner)
         {
         if(m_width<16 || m_height <16)
              {
              V2D_THROW( "To use CORNER_DIFFERENTIAL surface clearing mode, surface size must be at least 16x16 current size is "<<m_width<<"x"<<m_height );
              }

         for ( z = 0; z < m_depth; z++ )
           for ( y = 0; y < 16; y++ )
             for ( x = 0; x < 16; x++ )
             {
                SetPixel8888( x, y, z, a,r,g,b);
                a += da; r += dr; g += dg; b += db;
             }
         }
     else
         {
         for ( z = 0; z < m_depth; z++ )
           for ( y = 0; y < m_height; y++ )
             for ( x = 0; x < m_width; x++ )
             {
                SetPixel8888( x, y, z, a,r,g,b);
                a += da; r += dr; g += dg; b += db;
             }
         }
}

void V2dSurface::Clear_Font_Test_Pattern( UINT32 size )
{
    UINT32 x, y, z, i, j;

    int cx = m_width * 4;
    int cy = m_height * 4;

    for ( z = 0; z < m_depth; z++ )
        for ( y = 0; y < m_height; y++ )
            for ( x = 0; x < m_width; x++ )
            {
                UINT32 data0 = 0;
                UINT32 data1 = 0;

                for ( j = y*8; j < y*8+8; j++)
                    for ( i = x*8; i < x*8+8; i++)
                    {
                        float dx = float(int(i) - cx);
                        float dy = float(int(j) - cy);
                        float r = sqrt(dx*dx + dy*dy) / size;
                        if ((r - int(r)) >= 0.5)
                        {
                            if (j < y*8+4)
                                data0 |= (1 << (i-x*8 + (j-y*8)*8));
                            else
                                data1 |= (1 << (i-x*8 + (j-y*8-4)*8));
                        }
                    }

                SetPixel_8byte( x, y, z, data0, data1, 1 );
            }
}

void V2dSurface::Clear_XY_All_Colors( UINT32 size )
{
    UINT32 x, y, z;
    UINT08 a,r,g,b;

    UINT32 scale = 0xff/size;

    for ( z = 0; z < m_depth; z++ )
      for ( y = 0; y < m_height; y++ )
        for ( x = 0; x < m_width; x++ )
        {
           a = (((y/size) %size)*scale)&0xff;
           r = ((x%size)*scale)&0xff;
           g = ((y%size)*scale)&0xff;
           b = (((x/size) % size)*scale)&0xff;

           SetPixel8888( x, y, z, a,r,g,b);
        }
}

void V2dSurface::Clear_XY_All_Colors2( UINT32 size )
{
    UINT32 x, y, z;
    UINT08 a,r,g,b;

    UINT32 scale = 0xff/(size-1);

    for ( z = 0; z < m_depth; z++ )
      for ( y = 0; y < m_height; y++ )
        for ( x = 0; x < m_width; x++ )
        {
           a = (((y/size) % size)*scale) & 0xff;
           r = (((x/size) % size)*scale) & 0xff;
           g = ((y%size)*scale) & 0xff;
           b = ((x%size)*scale) & 0xff;

           SetPixel8888( x, y, z, a,r,g,b);
        }
}

void V2dSurface::Clear_Gray_Ramp( bool vert, bool color, bool alpha, bool coarse, UINT32 scale )
{
    UINT32 x, y, z;

    for ( z = 0; z < m_depth; z++ )
      for ( y = 0; y < m_height; y++ )
        for ( x = 0; x < m_width; x++ )
        {
            int g = vert ? ( y & 0xff ) : ( x & 0xff );
            g = g*scale;
            if( coarse )
                g = g & 0xF0;
            if ( color )
            {
                if(m_colorFormat == Y32)
                     SetPixel8888( x, y, z, g, g, g, g );
                else
                     SetPixel8888( x, y, z, 0xff, g, g, g );
            }
            if ( alpha )
                SetPixelAlpha( x, y, z, g );
        }
}

void V2dSurface::Clear_Color_Ramp( bool vert )
{
    UINT32 x,y,z;
    UINT08 a,r,g,b;
    int i;
    UINT32 color[16]={0x00ff0000,0x0000ff00,0x000000ff,0x00000000,0x00ffffff,0x7fff0000,0x7f00ff00,0x7f0000ff,0x7f000000,0x7fffffff,0xffff0000,0xff00ff00,0xff0000ff,0xff000000,0xffffffff,0xff00ffff} ;

    for ( z = 0; z < m_depth; z++ )
      for ( y = 0; y < m_height; y++ )
        for ( x = 0; x < m_width; x++ )
        {
            i = vert ? ( y & 0xf ) : ( x & 0xf );

            a = color[i]>>24 & 0xff;
            r = color[i]>>16 & 0xff;
            g = color[i]>>8  & 0xff;
            b = color[i]     & 0xff;

            SetPixel8888( x, y, z, a,r,g,b);

        }
}
void V2dSurface::Clear_Gray_Ramp_Checker( bool color, bool alpha )
{
    UINT32 x, y, z;
    unsigned int g;

    for ( z = 0; z < m_depth; z++ )
      for ( y = 0; y < m_height; y++ )
        for ( x = 0; x < m_width; x++ )
        {
            if ( ((x>>4)+(y>>4)) % 2)
                g = (x & 0xf);
            else
                g = (y & 0xf);

            switch ( m_colorFormat )
            {
                case AY8:
                case Y8:
                    SetPixel( x, y, z, (g * 0x11) );
                    break;
                case Y16:
                    SetPixel( x, y, z, (g * 0x1111) );
                    break;
                case Y32:
                    SetPixel( x, y, z, (g * 0x11111111) );
                    break;
                default:
                    if ( color )
                        SetPixel8888( x, y, z, 0xff, g*0x11, g*0x11, g*0x11 );
                    if ( alpha )
                        SetPixelAlpha( x, y, z, g*0x11 );
            }
        }
}

void V2dSurface::Clear_RGB( void )
{
    UINT32 x,y,z;
    unsigned int temp,c1,c2,c3;

    c1 = 0xff;
    c2 = 0x00;
    c3 = 0x00;

    for ( z = 0; z < m_depth; z++ )
      for ( y = 0; y < m_height; y++ )
        for ( x = 0; x < m_width; x++ )
        {

            SetPixel8888( x, y, z, 0xff, c1, c2, c3 );

            //rotate color values
            temp    = c3    ;
            c3      = c2    ;
            c2      = c1    ;
            c1      = temp  ;

        }

}

void V2dSurface::Clear_YUV_Test_Pattern( int pattern )
{
    // patterns are intended to fill a 512x256 surface

    UINT32 x, y, z;

    for ( z = 0; z < m_depth; z++ )
      for ( y = 0; y < m_height; y++ )
        for ( x = 0; x < m_width; x+=2 )
        {
            int cr, cb, ya, yb;
            switch ( pattern )
            {
                case 1:
                    // cr and cb 2d ramp
                    // should see green  in top-left
                    //            red    in top-right
                    //            blue   in bottom-left
                    //            purple in bottom-right
                    // checkerboard with: constant ya,yb
                    //                    alternating light/dark ya,yb
                    cr = (x/2) & 0xff;
                    cb = y & 0xff;
                    if (((x/16)+(y/16))%2)
                    {
                        ya = 0x80;
                        yb = 0x80;
                    }
                    else
                    {
                        ya = 0x40;
                        yb = 0xC0;
                    }
                    break;
                case 2:
                    // ya,yb ramp 0-255 then 255-0, testing all values
                    // in each of ya and yb
                    // top 8 lines have constant cr,cb (should see gray ramp)
                    // rest of lines have random cr,cb
                    if (y < 8)
                    {
                        cr = 0x80;
                        cb = 0x80;
                    }
                    else
                    {
                        cr = m_v2d->Random() & 0xff;
                        cb = m_v2d->Random() & 0xff;
                    }
                    if (x <= 0xff)
                    {
                        ya = x & 0xff;
                        yb = (x+1) & 0xff;
                    }
                    else
                    {
                        ya = 0xff - (x & 0xff);
                        yb = 0xff - ((x+1) & 0xff);
                    }
                    break;
                default:
                    V2D_THROW( "unknown pattern " << pattern <<
                               " in Clear_YUV_Test_Pattern()" );
            }

            switch ( m_colorFormat )
            {
                case CR8YB8CB8YA8:
                    SetPixel8888( x, y, z, cr, yb, cb, ya );
                    break;
                case YB8CR8YA8CB8:
                    SetPixel8888( x, y, z, yb, cr, ya, cb );
                    break;
                default:
                    V2D_THROW( " in Clear_YUV_Test_Pattern() " <<
                               " requres CR8YB8CB8YA8 or YB8CR8YA8CB8" );
            }
        }
}

void V2dSurface::SetPixel( UINT32 x, UINT32 y, UINT32 z, UINT32 data )
{
    UINT32 offset;

    offset = (z * m_pitch*m_height) + (y * m_pitch) + (x * m_bytesPerPixel);

    if (m_blocklinear_map)
        offset = m_blocklinear_map->Address(offset);

    uintptr_t addr = (m_cacheMode ? (uintptr_t)&m_cacheBuf[0] :
                                    (uintptr_t)m_mappingAddress)
                     + offset;

    switch ( m_bytesPerPixel )
    {
     case 1:
        MEM_WR08( addr, (UINT08) (data & 0xFF) );
        break;
     case 2:
        MEM_WR16( addr, (UINT16) (data & 0xFFFF) );
        break;
     case 4:
        MEM_WR32( addr, data );
        break;
     default:
        InfoPrintf("set shadow pixel %d %d\n", x, y);

        V2D_THROW( "unknown/illegal m_bytesPerPixel: " << m_bytesPerPixel );
    }
}

UINT64 V2dSurface::GetAddress( UINT32 x, UINT32 y, UINT32 z )
{
    UINT64 offset = (z * m_pitch*m_height) + (y * m_pitch) + (x * GetBpp());
    if (m_blocklinear_map)
        offset = m_blocklinear_map->Address(offset) + GetOffset();
    return offset;
}

void V2dSurface::SetPixel_8byte( UINT32 x, UINT32 y, UINT32 z,
                                 UINT32 data0, UINT32 data1, int granularity)
{
    if ( m_bytesPerPixel != 8 )
        V2D_THROW( "m_bytesPerPixel != 8" );

    UINT32 offset = (z * m_pitch*m_height) + (y * m_pitch) + (x * 8);
    if (m_blocklinear_map)
        offset = m_blocklinear_map->Address(offset);

    uintptr_t addr = (m_cacheMode ? (uintptr_t)&m_cacheBuf[0] :
                                    (uintptr_t)m_mappingAddress)
                     + offset;

    switch (granularity) {
        case 1:
            MEM_WR08(addr+0, (data0 >>  0) & 0xff);
            MEM_WR08(addr+1, (data0 >>  8) & 0xff);
            MEM_WR08(addr+2, (data0 >> 16) & 0xff);
            MEM_WR08(addr+3, (data0 >> 24) & 0xff);
            MEM_WR08(addr+4, (data1 >>  0) & 0xff);
            MEM_WR08(addr+5, (data1 >>  8) & 0xff);
            MEM_WR08(addr+6, (data1 >> 16) & 0xff);
            MEM_WR08(addr+7, (data1 >> 24) & 0xff);
            break;
        case 4:
            MEM_WR32(addr+0, data0);
            MEM_WR32(addr+4, data1);
            break;
        default:
            V2D_THROW( "unknown granularity " << granularity <<
                       " in SetPixel_8byte" );
    }
}

void V2dSurface::SetPixel_16byte( UINT32 x, UINT32 y, UINT32 z,
                                  UINT32 data0, UINT32 data1,
                                  UINT32 data2, UINT32 data3 )
{
    if ( m_bytesPerPixel != 16 )
        V2D_THROW( "m_bytesPerPixel != 16" );

    UINT32 offset = (z * m_pitch*m_height) + (y * m_pitch) + (x * 16);
    if (m_blocklinear_map)
        offset = m_blocklinear_map->Address(offset);

    uintptr_t addr = (m_cacheMode ? (uintptr_t)&m_cacheBuf[0] :
                                    (uintptr_t)m_mappingAddress)
                     + offset;

    MEM_WR32( addr+ 0, data0 );
    MEM_WR32( addr+ 4, data1 );
    MEM_WR32( addr+ 8, data2 );
    MEM_WR32( addr+12, data3 );
}

static unsigned int colwert_integer(unsigned int v, int bits_in, int bits_out) {

// colwert between different integer precisions, by bit replication and/or
// truncation:

    unsigned int v0 = v;
    int bits_in0 = bits_in;

    while (bits_in < bits_out) {
        v = (v << bits_in0) | v0;
        bits_in += bits_in0;
    }

    if (bits_in > bits_out)
        v = v >> (bits_in - bits_out);

    return v;
}

void V2dSurface::SetPixel8888( UINT32 x, UINT32 y, UINT32 z,
                               UINT08 a, UINT08 r, UINT08 g, UINT08 b )
{
    switch ( static_cast<int>(m_colorFormat) )
    {
        case Y8:
        case AY8:
            SetPixel( x, y, z, b );
            break;

        case LW9097_SET_COLOR_TARGET_FORMAT_V_R8:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_RN8:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_RS8:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_RU8:
            SetPixel( x, y, z, r );
            break;

        case LW9097_SET_COLOR_TARGET_FORMAT_V_A8:
            SetPixel( x, y, z, a );
            break;

        case A1R5G5B5:
        case X1R5G5B5_Z1R5G5B5:
        case X1R5G5B5_O1R5G5B5:
            SetPixel( x, y, z, ((a>>7)<<15) | ((r>>3)<<10) |
                                     ((g>>3)<< 5) | ((b>>3)<< 0) );
            break;

        case R5G6B5:
            SetPixel( x, y, z, ((r>>3)<<11) | ((g>>2)<<5) | ((b>>3)<<0) );
            break;

        case Y16:
            SetPixel( x, y, z, (g<<8) | (b<<0) );
            break;

        case LW9097_SET_COLOR_TARGET_FORMAT_V_R16_G16:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_RN16_GN16:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_RS16_GS16:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_RU16_GU16:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_RF16_GF16:
            SetPixel( x, y, z, (r<<16) | (g<<0) );
            break;
 
        case LW9097_SET_COLOR_TARGET_FORMAT_V_BF10GF11RF11:
            SetPixel( x, y, z,
                     (colwert_integer(b, 8, 10) << 22) |
                     (colwert_integer(g, 8, 11) << 11) |
                     (colwert_integer(r, 8, 11) <<  0) );
            break;

        case LW9097_SET_COLOR_TARGET_FORMAT_V_RS32:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_RU32:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_RF32:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_R32:
            SetPixel( x, y, z, r);
            break;

        case X8R8G8B8_Z8R8G8B8:
        case X8R8G8B8_O8R8G8B8:
        case X1A7R8G8B8_Z1A7R8G8B8:
        case X1A7R8G8B8_O1A7R8G8B8:
        case A8RL8GL8BL8:
        case X8RL8GL8BL8:
        case A8R8G8B8:
        case Y32:
        case A8CR8CB8Y8:
            SetPixel( x, y, z, (a<<24) | (r<<16) | (g<<8) | (b<<0) );
            break;

        case A8BL8GL8RL8:
        case X8BL8GL8RL8:
        case X8B8G8R8_Z8B8G8R8:
        case X8B8G8R8_O8B8G8R8:
        case A8B8G8R8:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_AN8BN8GN8RN8:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_AS8BS8GS8RS8:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_AU8BU8GU8RU8:
            SetPixel( x, y, z, (a<<24) | (b<<16) | (g<<8) | (r<<0) );
            break;

        case A2R10G10B10:
             SetPixel( x, y, z,
                     (colwert_integer(a, 8, 2 ) << 30) |
                     (colwert_integer(r, 8, 10) << 20) |
                     (colwert_integer(g, 8, 10) << 10) |
                     (colwert_integer(b, 8, 10) <<  0) );
             break;
        case A2B10G10R10:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_AU2BU10GU10RU10:
             SetPixel( x, y, z,
                     (colwert_integer(a, 8, 2 ) << 30) |
                     (colwert_integer(b, 8, 10) << 20) |
                     (colwert_integer(g, 8, 10) << 10) |
                     (colwert_integer(r, 8, 10) <<  0) );
             break;

        case CR8YB8CB8YA8:
        case YB8CR8YA8CB8:
        case A4CR6YB6A4CB6YA6:
            {
                // deal with 2-pixel-per-4 byte formats:
                // TODO: something better
                UINT32 color = (a<<24) | (r<<16) | (g<<8) | (b<<0);
                SetPixel( (x&0xfffffffe)+0, y, z, (color & 0xffff) );
                SetPixel( (x&0xfffffffe)+1, y, z, (color >> 16) );
            }
            break;

        case LW9097_SET_COLOR_TARGET_FORMAT_V_G8R8:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_GN8RN8:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_GS8RS8:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_GU8RU8:
            {
                SetPixel( (x&0xfffffffe)+0, y, z, (g<<8) | (r<<0) );
                SetPixel( (x&0xfffffffe)+1, y, z, (g<<8) | (r<<0) );
            }
            break;

        case LW9097_SET_COLOR_TARGET_FORMAT_V_R16:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_RN16:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_RS16:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_RU16:
        case LW9097_SET_COLOR_TARGET_FORMAT_V_RF16:
            {
                SetPixel( (x&0xfffffffe)+0, y, z, r);
                SetPixel( (x&0xfffffffe)+1, y, z, r);
            }
            break;

        case (LW9097_SET_ZT_FORMAT_V_Z16 + 0x100):
        case (LW9097_SET_ZT_FORMAT_V_Z24S8 + 0x100):
        case (LW9097_SET_ZT_FORMAT_V_X8Z24 + 0x100):
        case (LW9097_SET_ZT_FORMAT_V_S8Z24 + 0x100):
        case (LW9097_SET_ZT_FORMAT_V_V8Z24 + 0x100):
        case (LW9097_SET_ZT_FORMAT_V_ZF32  + 0x100):
            V2D_THROW( "invalid surface clear pattern for format ZT_*" );

        case Y1_8X8:
            V2D_THROW( "invalid surface clear pattern for format Y1_8X8" );

        default:
            V2D_THROW( "unknown m_colorFormat in V2dSurface::SetPixel8888(): " << m_colorFormat );
    }
}

void V2dSurface::SetPixelAlpha( UINT32 x, UINT32 y, UINT32 z, UINT08 a )
{
    UINT32 dst;

    switch ( m_colorFormat )
    {
        case AY8:
        case Y8:
        case Y16:
        case Y32:
        case X1R5G5B5_Z1R5G5B5:
        case X1R5G5B5_O1R5G5B5:
        case R5G6B5:
        case X8R8G8B8_Z8R8G8B8:
        case X8R8G8B8_O8R8G8B8:
        case X8B8G8R8_Z8B8G8R8:
        case X8B8G8R8_O8B8G8R8:
        case X8RL8GL8BL8:
        case X8BL8GL8RL8:
        case CR8YB8CB8YA8:
        case YB8CR8YA8CB8:
            break;

        case X1A7R8G8B8_Z1A7R8G8B8:
        case X1A7R8G8B8_O1A7R8G8B8:
            dst = GetPixel(x, y, z );
            SetPixel( x, y, z, ( dst & 0x80ffffff ) | ( (a >> 1) << 24 ) );
            break;

        case A1R5G5B5:
            dst = GetPixel(x, y, z );
            SetPixel( x, y, z, ( dst & 0x7fff ) | ( ((a>>7) << 15 ) ));
            break;

        case A8R8G8B8:
        case A8B8G8R8:
        case A8RL8GL8BL8:
        case A8BL8GL8RL8:
        case A8CR8CB8Y8:
            dst = GetPixel( x, y, z );
            SetPixel( x, y, z, ( dst & 0x00ffffff ) | (a << 24) );
            break;

        case A2R10G10B10:
        case A2B10G10R10:
            dst = GetPixel( x, y, z );
            SetPixel( x, y, z, ( dst & 0x3fffffff ) | ((a>>6) << 30) );
            break;

        case A4CR6YB6A4CB6YA6:
            // deal with 2-pixel-per-4 byte format:
            // TODO: something better
            dst = GetPixel( (x&0xfffffffe)+1, y, z );
            SetPixel( (x&0xfffffffe)+1, y, z, ( dst & 0x0fff ) |
                                                    ( (a >> 4) << 12) );
            break;

        default:
            V2D_THROW( "unknown m_colorFormat in V2dSurface::SetPixelAlpha(): " << m_colorFormat );
    }
}

UINT32 V2dSurface::GetPixel( UINT32 x, UINT32 y, UINT32 z )
{
    UINT32 offset, data;

    offset = (z * m_pitch*m_height) + (y * m_pitch) + (x * m_bytesPerPixel);

    // TODO: test this GetPixel() with blocklinear surface
    if (m_blocklinear_map)
        offset = m_blocklinear_map->Address(offset);

    // We should be very carefully to enable cache mode, if you're going to do
    // a clear, you should either make sure there's no dependency to the real
    // surface data or read the surface data to cache buffer before you do any
    // reading operation. All alpha clear operations depend on existing buffer,
    // for such cases, we load the buffer from fb to cache buffer at first
    uintptr_t addr = (m_cacheMode ? (uintptr_t)&m_cacheBuf[0] :
                                    (uintptr_t)m_mappingAddress)
                     + offset;

    switch ( m_bytesPerPixel )
    {
     case 1:
        data = (UINT32) MEM_RD08( addr );
        break;
     case 2:
        data = (UINT32) MEM_RD16( addr );
        break;
     case 4:
        data = MEM_RD32( addr );
        break;
     default:
        V2D_THROW( "unknown/illegal m_bytesPerPixel: " << m_bytesPerPixel );
    }

    return data;
}

UINT32 V2dSurface::CalcCrlwsingGpu( void )
{
    RC rc;
    Tasker::MutexHolder* mutexHolder = 0;

    bool crcNewChannel = (m_pParams->ParamPresent("-crc_new_channel") ||
                     m_ch->GetModsChannel()->GetRunlistChannelWrapper());

    if (crcNewChannel)
    {
        mutexHolder = new Tasker::MutexHolder(m_v2d->GetLWGpu()->GetCrcChannelMutex());
    }

    UINT32 hSrcCtxDma = m_dmaBuffer.GetCtxDmaHandle();
    UINT64 SrcOffset = m_dmaBuffer.GetCtxDmaOffsetGpu() + m_dmaBuffer.GetExtraAllocSize();

    int subch = 0;

    // create the copier.
    DmaReader* dmaReader = m_v2d->GetDmaReader();

    // this lets the current object know that it is no longer set, which
    // causes it to call SetObject the next time it tries to write a
    // method:
    if (!crcNewChannel)
    {
        m_v2d->Verif2d::LoseLastSubchAllocObjSubchannel();
    }

    // Yes, v2d doesn't use surf2d attributes correctly ...
    UINT32 savePitch = m_dmaBuffer.GetPitch();
    UINT32 saveWidth = m_dmaBuffer.GetWidth();
    UINT32 saveHeight = m_dmaBuffer.GetHeight();
    UINT32 saveDepth = m_dmaBuffer.GetDepth();
    UINT32 saveBytesPerPixel = m_dmaBuffer.GetBytesPerPixel();
    ColorUtils::Format saveColorFormat = m_dmaBuffer.GetColorFormat();
    UINT32 saveLogBlockHeight = m_dmaBuffer.GetLogBlockHeight();
    UINT32 saveLogBlockDepth = m_dmaBuffer.GetLogBlockDepth();
    m_dmaBuffer.SetPitch(m_pitch);
    m_dmaBuffer.SetWidth(m_width);
    m_dmaBuffer.SetHeight(m_height);
    m_dmaBuffer.SetDepth(m_depth);
    m_dmaBuffer.SetBytesPerPixel(m_bytesPerPixel);
    m_dmaBuffer.SetColorFormat(ColorUtils::LWFMT_NONE);
    m_dmaBuffer.SetLogBlockHeight(m_Log2blockHeight);
    m_dmaBuffer.SetLogBlockDepth(m_Log2blockDepth);

    rc = dmaReader->CopySurface(&m_dmaBuffer,
        GetPageLayoutFromParams(m_pParams, NULL), subch/*gpusubdevice*/);
    if(rc != OK)
    {
        ErrPrintf("Could not copy %s buffer: %s\n",
            m_dmaBuffer.GetName().c_str(), rc.Message());
    }
    hSrcCtxDma = dmaReader->GetCopyHandle();
    SrcOffset = dmaReader->GetCopyBuffer();

    UINT32 width  = m_dmaBuffer.GetWidth();
    UINT32 height = m_dmaBuffer.GetHeight();
    UINT32 size = width*m_dmaBuffer.GetBytesPerPixel()*height;
    size = size*m_dmaBuffer.GetDepth()*m_dmaBuffer.GetArraySize();
    if (size == 0)
    {
        // No width and height, so not a graphics surface
        size = static_cast<UINT32>(m_dmaBuffer.GetSize());
    }
    if (size & 0x3)
    {
        ErrPrintf("Could not CRC %s buffer: HW CRC supported only for "
            "multiple-of-four sizes.\n", m_dmaBuffer.GetName().c_str());
        return 0;
    }

    DmaReader* gpuCrcCallwlator = m_v2d->GetGpuCrcCallwlator();
    rc = gpuCrcCallwlator->ReadLine(
                hSrcCtxDma, SrcOffset, 0,
                1, size, subch/*gpuSubdevice*/, m_dmaBuffer.GetSurf2D());
    if(rc != OK)
    {
        ErrPrintf("Could not read back %s buffer: %s\n",
            m_dmaBuffer.GetName().c_str(), rc.Message());
    }

    // CYA - restore bogus settings.
    m_dmaBuffer.SetPitch(savePitch);
    m_dmaBuffer.SetWidth(saveWidth);
    m_dmaBuffer.SetHeight(saveHeight);
    m_dmaBuffer.SetDepth(saveDepth);
    m_dmaBuffer.SetBytesPerPixel(saveBytesPerPixel);
    m_dmaBuffer.SetColorFormat(saveColorFormat);
    m_dmaBuffer.SetLogBlockHeight(saveLogBlockHeight);
    m_dmaBuffer.SetLogBlockDepth(saveLogBlockDepth);

    if (crcNewChannel)
    {
        delete mutexHolder;
    }

    DebugPrintf("GPU-callwlated CRC is at address 0x%x\n",
        gpuCrcCallwlator->GetBuffer());

    UINT32 crc;
    Platform::VirtualRd((void*)gpuCrcCallwlator->GetBuffer(), &crc, 4);

    return crc;
}

UINT32 V2dSurface::CalcCRC( void )
{
    m_v2d->WaitForIdle();

    if (m_v2d->GetDoGpuCrcCalc()) {
        return CalcCrlwsingGpu();
    }
    LwRm* pLwRm = m_ch->GetLwRmPtr();

    // TODO: allow selection of layer, if depth > 1

    // endianness: BufCrcDim32() appears to handle endian swapping,
    // so there's no need to swap bytes before calling it.

    bool readRawImage = false;

    if (m_pParams->ParamPresent("-RawImageMode") > 0)
    {
        if ((_RAWMODE)m_pParams->ParamUnsigned("-RawImageMode") == RAWON)
        {
            readRawImage = true;
        }
    }

    if (readRawImage && m_dmaBuffer.GetCompressed())
    {
        pLwRm->VidHeapReleaseCompression(m_dmaBuffer.GetMemHandle(),
            m_v2d->GetLWGpu()->GetGpuDevice());

        if (m_dmaBuffer.GetSplit())
        {
            pLwRm->VidHeapReleaseCompression(m_dmaBuffer.GetSplitMemHandle(),
                m_v2d->GetLWGpu()->GetGpuDevice());
        }
    }

    vector<UINT08> pitchBuffer;
    UINT32 pitch;
    ReadSurfaceToPitch(pitchBuffer, pitch, true);

    // Dump raw memory
    if (m_pParams->ParamPresent("-backdoor_crc") > 0)
    {
        DumpRawSurface();
    }

    if (readRawImage && m_dmaBuffer.GetCompressed())
    {
        pLwRm->VidHeapReacquireCompression(m_dmaBuffer.GetMemHandle(),
            m_v2d->GetLWGpu()->GetGpuDevice());

        if (m_dmaBuffer.GetSplit())
        {
            pLwRm->VidHeapReacquireCompression(m_dmaBuffer.GetSplitMemHandle(), m_v2d->GetLWGpu()->GetGpuDevice());
        }
    }

    UINT32 crc = BufCrcDim32((uintptr_t) &pitchBuffer[0], pitch,
                             m_width, m_height,
                             m_bytesPerPixel);
    return crc;
}

uintptr_t V2dSurface::CEReadBeforeCalcCRC( UINT08* pDst )
{
    RC rc;
    Tasker::MutexHolder* mutexHolder = 0;

    bool crcNewChannel = (m_pParams->ParamPresent("-crc_new_channel") ||
                          m_ch->GetModsChannel()->GetRunlistChannelWrapper());

    if (crcNewChannel)
    {
        mutexHolder = new Tasker::MutexHolder(m_v2d->GetLWGpu()->GetCrcChannelMutex());
    }

    int subch = 0;

    // TODO: create the reader.
    DmaReader* dmaReader = m_v2d->GetDmaReader();

    // this lets the current object know that it is no longer set, which
    // causes it to call SetObject the next time it tries to write a
    // method:
    if (!crcNewChannel)
    {
        m_v2d->Verif2d::LoseLastSubchAllocObjSubchannel();
    }

    if (m_blocklinear_map && dmaReader->NoPitchCopyOfBlocklinear())
    {
        // Unfortunately the V2dSurface object and the corresponding
        // MdiagSurf object are not consistent with each other.  The
        // DmaReader::CopySurface function uses MdiagSurf data for the copy,
        // but the V2dSurface object has the correct data.  It's unclear why
        // the MdiagSurf data does not match, so this code plays it safe and
        // temporarily changes the relevant MdiagSurf data so that the data
        // will be correct for the copy, but will be restored to the old values
        // in case later code relies on the MdiagSurf data being different
        // from the V2dSurface data.

        UINT32 savePitch = m_dmaBuffer.GetPitch();
        UINT32 saveWidth = m_dmaBuffer.GetWidth();
        UINT32 saveHeight = m_dmaBuffer.GetHeight();
        UINT32 saveDepth = m_dmaBuffer.GetDepth();
        UINT32 saveBytesPerPixel = m_dmaBuffer.GetBytesPerPixel();
        ColorUtils::Format saveColorFormat = m_dmaBuffer.GetColorFormat();
        UINT32 saveLogBlockHeight = m_dmaBuffer.GetLogBlockHeight();
        UINT32 saveLogBlockDepth = m_dmaBuffer.GetLogBlockDepth();
        m_dmaBuffer.SetPitch(m_width * m_bytesPerPixel);
        m_dmaBuffer.SetWidth(m_width);
        m_dmaBuffer.SetHeight(m_height);
        m_dmaBuffer.SetDepth(m_depth);
        m_dmaBuffer.SetBytesPerPixel(m_bytesPerPixel);
        m_dmaBuffer.SetColorFormat(ColorUtils::LWFMT_NONE);
        m_dmaBuffer.SetLogBlockHeight(m_Log2blockHeight);
        m_dmaBuffer.SetLogBlockDepth(m_Log2blockDepth);

        rc = dmaReader->CopySurface(&m_dmaBuffer,
            GetPageLayoutFromParams(m_pParams, NULL), subch/*gpusubdevice*/);

        if (rc != OK)
        {
            ErrPrintf("Could not copy %s buffer: %s\n",
                m_dmaBuffer.GetName().c_str(), rc.Message());
        }

        UINT32 width  = m_dmaBuffer.GetWidth();
        UINT32 height = m_dmaBuffer.GetHeight();
        UINT32 size = width * m_dmaBuffer.GetBytesPerPixel() * height;
        size *= m_dmaBuffer.GetDepth() * m_dmaBuffer.GetArraySize();
        MdiagSurf *copySurface = dmaReader->GetCopySurface();
        CHECK_RC(copySurface->Map());
        Platform::VirtualRd(copySurface->GetAddress(), pDst, size);

        if (rc != OK)
        {
            ErrPrintf("Could not read back %s buffer: %s\n",
                m_dmaBuffer.GetName().c_str(), rc.Message());
        }

        copySurface->Unmap();

        // CYA - restore bogus settings.
        m_dmaBuffer.SetPitch(savePitch);
        m_dmaBuffer.SetWidth(saveWidth);
        m_dmaBuffer.SetHeight(saveHeight);
        m_dmaBuffer.SetDepth(saveDepth);
        m_dmaBuffer.SetBytesPerPixel(saveBytesPerPixel);
        m_dmaBuffer.SetColorFormat(saveColorFormat);
        m_dmaBuffer.SetLogBlockHeight(saveLogBlockHeight);
        m_dmaBuffer.SetLogBlockDepth(saveLogBlockDepth);
    }
    else
    {
        for (UINT64 off=0; off<m_sizeInBytes; off+=(1<<18))
        {
            // copy in 256k chunks (1M chunk does not work for tesla)
            //
            // with a little more work, the blockliner to pitch colwersion
            // (if needed) could be done in this step by mem2mem.
            //
            UINT64 length = m_sizeInBytes - off;
            if (length > (1<<18))
                length = 1<<18;

            CHECK_RC(dmaReader->ReadLine(
                     m_ctxDmaHandle, m_dmaBuffer.GetCtxDmaOffsetGpu() + off, 0,
                     1, length, subch/*m_v2d->GetLWGpu()->GetGpuSubdevice()*/, m_dmaBuffer.GetSurf2D()));

            Platform::VirtualRd((void *)dmaReader->GetBuffer(), // Source
                                    &pDst[off], // Destination
                                    length);
        }
    }

    if (crcNewChannel)
    {
        delete mutexHolder;
    }

    return (uintptr_t)pDst;
}

void V2dSurface::AddToTable(BuffInfo* buffInfo)
{
    string name = "NO_LABEL";
    if (m_label == LABEL_A)
    {
        name = "LABEL_A";
    }
    else if (m_label == LABEL_B)
    {
        name = "LABEL_B";
    }
    buffInfo->SetDmaBuff(name, m_dmaBuffer);
    buffInfo->Print(nullptr, m_v2d->GetLWGpu()->GetGpuDevice());
}

extern RC FlushEverything(LWGpuResource *pGpuRes, UINT32 subdev, LwRm* pLwR);
void V2dSurface::DumpRawSurface()
{
    bool chiplibDumpDisabled = false;
    LWGpuResource *pGpuRes = m_v2d->GetLWGpu();
    LwRm* pLwRm = m_ch->GetLwRmPtr();
    FlushEverything(pGpuRes, m_subdev, pLwRm);

    // Disable the chiplib trace dump as we don't want the mem dump to be part of the capture.
    if (ChiplibTraceDumper::GetPtr()->IsChiplibTraceDumpEnabled())
    {
        ChiplibTraceDumper::GetPtr()->DisableChiplibTraceDump();
        chiplibDumpDisabled = true;
    }

    LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
    params.memOffset = 0;

    pLwRm->Control( m_dmaBuffer.GetMemHandle(), LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
            &params, sizeof(params));

    UINT32 size = (m_blocklinear_map == NULL ?
            m_pitch*m_height : m_blocklinear_map->Size());

    string filename = m_colorFormatString + ".chiplib_dump.raw";
    FILE *dumpFp;
    Utility::OpenFile(filename.c_str(), &dumpFp, "w");

    UINT64 readAddr = params.memOffset;
    UINT32 bytesRemaining = size;
    const UINT32 BAR0_WINDOW_SIZE = 0x100000;

    while (bytesRemaining > 0)
    {
        auto scopedBar0window =
            pGpuRes->GetGpuSubdevice(m_subdev)->ProgramScopedPraminWindow(readAddr, m_dmaBuffer.GetLocation());

        UINT32 bar0base = static_cast<UINT32>(readAddr & 0x000000000000ffffLL);
        UINT32 bytesToRead = min(bytesRemaining, BAR0_WINDOW_SIZE);

        for (UINT32 i = 0; i < bytesToRead; i += 4)
        {
            UINT32 data = pGpuRes->RegRd32(DEVICE_BASE(LW_PRAMIN) + bar0base + i, m_subdev);

            fprintf(dumpFp, "%02x\n", data & 0xff);
            fprintf(dumpFp, "%02x\n", (data >> 8) & 0xff);
            fprintf(dumpFp, "%02x\n", (data >> 16) & 0xff);
            fprintf(dumpFp, "%02x\n", (data >> 24) & 0xff);
        }

        bytesRemaining -= bytesToRead;
        readAddr += bytesToRead;
    }

    fclose(dumpFp);

    // Restore the chiplib trace dump.
    if (chiplibDumpDisabled)
    {
        ChiplibTraceDumper::GetPtr()->EnableChiplibTraceDump();

        ChiplibOpBlock *block = ChiplibTraceDumper::GetPtr()->GetLwrrentChiplibOpBlock();
        string replayFilename = m_colorFormatString + ".chiplib_replay.raw";
        block->AddRawMemDumpOp("FB", params.memOffset, size, replayFilename);
    }
}

void V2dSurface::ReadSurfaceToNaiveBL(UINT08 *pData)
{
    GpuDevice *pGpuDev = m_v2d->GetLWGpu()->GetGpuDevice();
    MASSERT(pGpuDev);

    Platform::VirtualRd(m_mappingAddress, pData, m_sizeInBytes);

    PixelFormatTransform::ColwertBetweenRawAndNaiveBlSurf(pData,
            m_sizeInBytes,
            PixelFormatTransform::XBAR_RAW,
            PixelFormatTransform::NAIVE_BL,
            pGpuDev);
    DebugPrintf("%s Colwert v2d surface from xbar raw to naive bl.\n", __FUNCTION__);
}

void V2dSurface::ReadSurfaceToPitch(vector<UINT08>& pitchBuffer, UINT32& pitch, bool chiplibTrace)
{
    // surfaces are only memory mapped when necessary
    // since this function reads memory directly, it is necessary now
    MapBuffer();

    uintptr_t baseAddr = (uintptr_t)m_mappingAddress;

    bool bUseCEDmaReader = m_v2d->GetDoDmaCheckCE() || m_dmaBuffer.UseCEForMemAccess(m_pParams, MdiagSurf::RdSurf);

    vector<UINT08> pDst(m_sizeInBytes);
    bool blocklinearToPitch = false;
    pitch = GetPitch();
    if (bUseCEDmaReader)
    {
        baseAddr = CEReadBeforeCalcCRC(&pDst[0]);
        blocklinearToPitch = (m_blocklinear_map != nullptr) &&
            m_v2d->GetDmaReader()->NoPitchCopyOfBlocklinear();

        if (blocklinearToPitch)
        {
            // The pitch value for a blocklinear surface will account for
            // block width alignment.  However, after a blocklinear-to-pitch
            // CE copy, the data will be in pitch format and the pitch value
            // should not be aligned to the block width when reading
            // the pitch copy.
            pitch = m_width * m_bytesPerPixel;
        }
    }
    else if (m_v2d->GetDoDmaCheck())
    {
        MASSERT(!"old mem2mem-based -dmacheck not supported any more, newer chips don't support mem2mem");
    }
    else 
    {
        if (m_dmaBuffer.MemAccessNeedsColwersion(m_pParams, MdiagSurf::RdSurf))
        {
           ReadSurfaceToNaiveBL(&pDst[0]);
           baseAddr = (uintptr_t)(&pDst[0]);
        }
    }

    // Surface information to dump chiplib trace; No rect here
    ChiplibOpScope::Crc_Surf_Read_Info surfInfo =
    {
        m_dmaBuffer.GetName(), ColorUtils::FormatToString(m_dmaBuffer.GetColorFormat()),
        pitch, m_dmaBuffer.GetWidth(), m_dmaBuffer.GetHeight(),
        m_dmaBuffer.GetBytesPerPixel(), m_dmaBuffer.GetDepth(),m_dmaBuffer.GetArraySize(),
        0, 0, m_dmaBuffer.GetWidth(), m_dmaBuffer.GetHeight(), 0, 0
    };

    pitchBuffer.resize(pitch * m_height);
    if (m_blocklinear_map && !blocklinearToPitch)
    {
        // if surface is blocklinear, copy to a pitch buffer for callwlating CRC
        // TODO: What if depth>1?
        //       Right now, this just checks the first layer.
        if (chiplibTrace)
        {
            ChiplibOpScope newScope("crc_surf_read", NON_IRQ, ChiplibOpScope::SCOPE_CRC_SURF_READ, &surfInfo);
        }

        InfoPrintf("copying blocklinear surface to pitch\n");
        m_v2d->m_buf->CopyBlockLinearBuf(baseAddr, 0, (UINT32*) &pitchBuffer[0], m_blocklinear_map->Size(),
                0, 0, 0,
                m_width, m_height, 1, 1, 0,
                m_blocklinear_map,
                pitch);
    }
    else
    {
        if (chiplibTrace)
        {
            ChiplibOpScope newScope("crc_surf_read", NON_IRQ, ChiplibOpScope::SCOPE_CRC_SURF_READ, &surfInfo);
        }

        InfoPrintf("copying pitch surface directly\n");
        // use fast routine to fetch memory data
        Platform::VirtualRd((void*)baseAddr, &pitchBuffer[0], pitch * m_height);
    }
}
