/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "trace_3d.h"
#include "teegroups.h"

#include "core/include/gpu.h"
#include "lwmisc.h"
#include "lwos.h"
#include "core/include/lwrm.h"

#include "gpu/utility/blocklin.h"

#include "class/cl7476os.h"

struct VP2TilingModeDescription
{
    enum LayoutType {
        LayoutNoChange,
        LayoutPitchLinear,
        Layout16x16,
        LayoutBlockLinear,
        LayoutBlockLinearTexture
    };

    char const *header_keyword;
    char const *cmdline_keyword;
    LayoutType  layout_type;
    UINT32      method_data_id;
};

VP2TilingModeDescription const TilingModeDesc[] =
{
    { "VP2_1D",                 "1D",    VP2TilingModeDescription::LayoutNoChange,           LW7476_SURFACE_TILE_MODE_LINEAR_1D    },
    { "VP2_PITCH_LINEAR_2D",    "PL",    VP2TilingModeDescription::LayoutPitchLinear,        LW7476_SURFACE_TILE_MODE_PITCH_LINEAR },
    { "VP2_DXVA",               "DXVA",  VP2TilingModeDescription::LayoutNoChange,           LW7476_SURFACE_TILE_MODE_DXVA         },
    { "VP2_16X16",              "16X16", VP2TilingModeDescription::Layout16x16,              LW7476_SURFACE_TILE_MODE_16x16_VP2    },
    { "VP2_BLOCK_LINEAR_T",     "BLT",   VP2TilingModeDescription::LayoutBlockLinearTexture, LW7476_SURFACE_TILE_MODE_BL_T         },
    { "VP2_BLOCK_LINEAR_NAIVE", "BLN",   VP2TilingModeDescription::LayoutBlockLinear,        LW7476_SURFACE_TILE_MODE_BL_N         }
};

struct VP2TilingParams
{
    bool   tiling_active;
    UINT32 tiling_mode_idx;
    UINT32 width;
    UINT32 height;
    UINT32 bpp;
    bool   big_endian;
    UINT32 pitch;
    UINT32 blockRows;

    enum ParametersSource
    {
        SourceDefault,
        SourceHeader,
        SourceCommandLine
    };
    ParametersSource source;

    bool   tile_mode_reloc_present;
    bool   pitch_reloc_present;
};

const UINT32 ILWALID_TILING_MODE_IDX = 0xFFFFFFFF;

const UINT32 NUM_VP2_TILING_CTXDMAS = 10;

static const char *VP2TileCmdLineArumentNames[NUM_VP2_TILING_CTXDMAS] =
{
    "-vp2tile0",
    "-vp2tile1",
    "-vp2tile2",
    "-vp2tile3",
    "-vp2tile4",
    "-vp2tile5",
    "-vp2tile6",
    "-vp2tile7",
    "-vp2tile8",
    "-vp2tile9"
};

#define MSGID() T3D_MSGID(Massage)

RC GenericTraceModule::SetVP2TilingHeapAttr(bool block_linear)
{
    UINT32 attr;

    if (block_linear)
    {
        attr = DRF_DEF(OS32, _ATTR, _DEPTH, _128) |
               DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR);
    }
    else
    { // Pitch linear
        attr = DRF_DEF(OS32, _ATTR, _DEPTH, _8) |
               DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH);
    }

    if (SetHeapAttr(attr))
    {
        ErrPrintf("Trace: only one VP2 Tiling Mode can be set at a time.\n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

bool FindVP2TilingModeIdxHeader(char const *header_mode_name, UINT32 *tiling_mode_idx)
{
    MASSERT(tiling_mode_idx != NULL);

    for (UINT32 mode_idx = 0;
         mode_idx < ( sizeof(TilingModeDesc)/sizeof(TilingModeDesc[0]) );
         mode_idx++)
    {
        if ( strcmp(header_mode_name, TilingModeDesc[mode_idx].header_keyword) == 0)
        {
            *tiling_mode_idx = mode_idx;
            return true;
        }
    }

    return false;
}

bool FindVP2TilingModeIdxCmdLine(char const *cmd_line_mode_name, UINT32 *tiling_mode_idx)
{
    MASSERT(tiling_mode_idx != NULL);

    for (UINT32 mode_idx = 0;
         mode_idx < ( sizeof(TilingModeDesc)/sizeof(TilingModeDesc[0]) );
         mode_idx++)
    {
        if ( stricmp(cmd_line_mode_name, TilingModeDesc[mode_idx].cmdline_keyword) == 0)
        {
            *tiling_mode_idx = mode_idx;
            return true;
        }
    }

    return false;
}

bool GenericTraceModule::IsVP2TilingPitchParameterNeeded(char const *mode_name)
{
    UINT32 mode_idx;
    if (!FindVP2TilingModeIdxHeader(mode_name, &mode_idx))
        return false;

    return (TilingModeDesc[mode_idx].layout_type ==
                VP2TilingModeDescription::LayoutPitchLinear);
}

RC GenericTraceModule::SetVP2TilingModeFromHeader(char const *mode_name, UINT32 width,
    UINT32 height, UINT32 bpp, char const *endian, UINT32 pitch, UINT32 blockRows)
{
    RC rc = OK;

    if (m_VP2TilingParams == NULL)
    {
        CHECK_RC(SetDefualtVP2TilingParams());
    }

    // Command line settings have highest priority, do not overwrite
    if (m_VP2TilingParams->source == VP2TilingParams::SourceCommandLine)
        return rc;

    m_VP2TilingParams->width  = width;
    m_VP2TilingParams->height = height;
    m_VP2TilingParams->bpp    = bpp;
    m_VP2TilingParams->pitch  = pitch;
    m_VP2TilingParams->blockRows = blockRows;

    m_VP2TilingParams->tiling_mode_idx = ILWALID_TILING_MODE_IDX;
    m_VP2TilingParams->tiling_active   = false;

    if (stricmp(endian, "LE") == 0)
        m_VP2TilingParams->big_endian = false;
    else
    if (stricmp(endian, "BE") == 0)
        m_VP2TilingParams->big_endian = true;
    else
    {
        ErrPrintf("Trace: VP2 Tiling - endian mode not recognized.\n");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 mode_idx;
    if (!FindVP2TilingModeIdxHeader(mode_name, &mode_idx))
    {
        ErrPrintf("Trace: VP2 Tiling mode \"%s\" not recognized.\n", mode_name);
        return RC::SOFTWARE_ERROR;
    }

    if ( (TilingModeDesc[mode_idx].layout_type ==
             VP2TilingModeDescription::LayoutBlockLinearTexture) &&
         (bpp == 0))
    {
        ErrPrintf("Trace: VP2 Tiling layout - bpp cannot be 0 for VP2_BLOCK_LINEAR_T\n");
        return RC::SOFTWARE_ERROR;
    }

    if ( (TilingModeDesc[mode_idx].layout_type ==
             VP2TilingModeDescription::LayoutPitchLinear) &&
         (pitch < bpp*width) )
    {
        ErrPrintf("Trace: VP2 Tiling - pitch is smaller then bpp*width.\n");
        return RC::SOFTWARE_ERROR;
    }

    m_VP2TilingParams->tiling_mode_idx = mode_idx;
    m_VP2TilingParams->tiling_active   = true;
    m_VP2TilingParams->source = VP2TilingParams::SourceHeader;

    return rc;
}

RC GenericTraceModule::ExelwteVP2TilingMode()
{
    RC rc = OK;

    if (!IsVP2TilingSupported())
        return OK;

    MASSERT(m_VP2TilingParams != NULL);
    UINT32 gpuSubdevIdx = 0;

    if (m_VP2TilingParams->source == VP2TilingParams::SourceDefault)
    {
        MASSERT(m_VP2TilingParams->tiling_mode_idx == 0); // We are prepared only for 1D as the default mode

        // Update default tiling parameters based on surface propeties:
        UINT32 bpp = m_SwapSize; // Guessing bpp is equal to SwapSize
        if (bpp == 0)
            bpp = 1; // SwapSize not provided, default to 1 bpp

        UINT32 m_Size = (UINT32)m_CachedSurface->GetSize(gpuSubdevIdx);
        m_VP2TilingParams->height        = 1;
        m_VP2TilingParams->width         = m_Size/bpp;
        m_VP2TilingParams->pitch         = m_Size;
        m_VP2TilingParams->big_endian    = false;
    }

    CHECK_RC(SetVP2TilingModeFromCmdLine());

    if (m_VP2TilingParams->tiling_active)
    {
        CHECK_RC(SetVP2TilingHeapAttr(
                    TilingModeDesc[m_VP2TilingParams->tiling_mode_idx].layout_type ==
                        VP2TilingModeDescription::LayoutBlockLinear ||
                    TilingModeDesc[m_VP2TilingParams->tiling_mode_idx].layout_type ==
                        VP2TilingModeDescription::LayoutBlockLinearTexture
                    )
                 );
    }

    return rc;
}

RC GenericTraceModule::DoVP2TilingLayoutBlockLinear()
{
    UINT32 blockRows = m_VP2TilingParams->blockRows;
    if (blockRows == 0) blockRows = 16;

    BlockLinear block_linear(m_VP2TilingParams->width,
                                  m_VP2TilingParams->height,
                                  0, log2(blockRows/GetGpuDev()->GobHeight()), 0,
                                  m_VP2TilingParams->bpp,
                                  m_Test->GetBoundGpuDevice());
    return DoVP2TilingLayoutBlockLinearHelper(&block_linear);
}

RC GenericTraceModule::DoVP2TilingLayoutBlockLinearTexture()
{
    UINT32 blockRows = m_VP2TilingParams->blockRows;
    if (blockRows == 0) blockRows = 16;

    if (m_VP2TilingParams->bpp == 0)
    {
        ErrPrintf("Trace: VP2 Tiling layout - bpp cannot be 0 for VP2_BLOCK_LINEAR_T\n");
        return RC::SOFTWARE_ERROR;
    }

    BlockLinearTexture block_linear(m_VP2TilingParams->width,
                                    m_VP2TilingParams->height,
                                    0, log2(blockRows/GetGpuDev()->GobHeight()), 0,
                                    m_VP2TilingParams->bpp,
                                    m_Test->GetBoundGpuDevice());
    return DoVP2TilingLayoutBlockLinearHelper(&block_linear);
}

RC GenericTraceModule::DoVP2TilingLayoutBlockLinearHelper(BlockLinear *pBlockLinear)
{
    MASSERT(pBlockLinear != 0);

    UINT32 width  = m_VP2TilingParams->width;
    UINT32 height = m_VP2TilingParams->height;
    UINT32 bpp    = m_VP2TilingParams->bpp;

    UINT32 colwerted_data_size = pBlockLinear->Size();
    MASSERT( colwerted_data_size >= (height*width*bpp) );

    UINT08 *colwerted_data = new UINT08[colwerted_data_size];

    if (colwerted_data == NULL)
    {
        ErrPrintf("Trace: VP2 Tiling layout - cannot allocate memory\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    UINT32 src_addr = 0;

    UINT32 gpuSubdevIdx = 0;
    UINT32* m_Data = m_CachedSurface->GetPtr(gpuSubdevIdx);
    MASSERT(m_Data != 0);

    for (UINT32 y = 0; y < height; y++)
    {
        for (UINT32 x = 0; x < width; x++)
        {
            UINT32 dst_addr = pBlockLinear->Address(x, y, 0);

            for (UINT32 byte_idx = 0; byte_idx < bpp; byte_idx++)
            {
                colwerted_data[dst_addr] = ((UINT08*)m_Data)[src_addr];

                dst_addr++;
                src_addr++;
            }

        }
    }

    m_CachedSurface->RenewAll(colwerted_data, colwerted_data_size);

    return OK;
}

RC GenericTraceModule::DoVP2TilingLayoutPitchLinear()
{
    UINT32 width     = m_VP2TilingParams->width;
    UINT32 height    = m_VP2TilingParams->height;
    UINT32 bpp       = m_VP2TilingParams->bpp;
    UINT32 dst_pitch = m_VP2TilingParams->pitch;

    MASSERT(dst_pitch >= width * bpp);

    UINT32 colwerted_data_size = height * dst_pitch;
    UINT08 *colwerted_data = new UINT08[colwerted_data_size];
    if (colwerted_data == NULL)
    {
        ErrPrintf("Trace: VP2 Tiling layout - cannot allocate memory\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    UINT32 gpuSubdevIdx = 0;
    UINT08* src = (UINT08*)m_CachedSurface->GetPtr(gpuSubdevIdx);
    MASSERT(src != 0);

    UINT32 src_pitch = bpp * width;

    for (UINT32 y = 0; y < height; y++)
        copy(src + y * src_pitch, src + (y+1) * src_pitch,
                colwerted_data + y * dst_pitch);

    m_CachedSurface->RenewAll(colwerted_data, colwerted_data_size);

    return OK;
}

RC GenericTraceModule::DoVP2TilingLayout16x16()
{
    UINT32 width  = m_VP2TilingParams->width;
    UINT32 height = m_VP2TilingParams->height;
    UINT32 bpp    = m_VP2TilingParams->bpp;

    UINT32 pitch16  = ((m_VP2TilingParams->width*bpp + 15)/16)*16;
    UINT32 height16 = ((m_VP2TilingParams->height    + 15)/16)*16;

    UINT32 colwerted_data_size = pitch16 * height16;
    UINT08 *colwerted_data = new UINT08[colwerted_data_size];
    if (colwerted_data == NULL)
    {
        ErrPrintf("Trace: VP2 Tiling layout - cannot allocate memory\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    UINT32 gpuSubdevIdx = 0;
    UINT08* src = (UINT08*)m_CachedSurface->GetPtr(gpuSubdevIdx);
    MASSERT(src != 0);

    UINT32 in_pitch = bpp * width;

    for (UINT32 y = 0; y < height; y++)
        for (UINT32 x = 0; x < width; x++)
        {
            UINT32 src_addr = bpp * x + in_pitch * y;

            // Formula taken from:
            //hw/vp2/vp2_tools/lib/vp2/scalarLib/driver/driverSurfaceImpl.cc
            UINT32 dst_addr =   ((y/16)*16*pitch16*bpp)
                              + (x/(16/bpp)*256)
                              + ((y%16)*16)
                              + ((x%(16/bpp))*bpp);

            for (UINT32 byte_idx = 0; byte_idx < bpp; byte_idx++)
            {
                colwerted_data[dst_addr] = src[src_addr];
                src_addr++;
                dst_addr++;
            }
        }

    m_CachedSurface->RenewAll(colwerted_data, colwerted_data_size);

    return OK;
}

RC GenericTraceModule::DoVP2TilingLayout()
{
    RC rc = OK;

    if ( (m_VP2TilingParams == NULL) ||
         (!m_VP2TilingParams->tiling_active) )
        return OK;

    if ( (m_VP2TilingParams->tiling_mode_idx == ILWALID_TILING_MODE_IDX) ||
         (m_VP2TilingParams->tiling_mode_idx >=
              ( sizeof(TilingModeDesc)/sizeof(TilingModeDesc[0]) ) ) )
    {
        ErrPrintf("Trace: VP2 Tiling Layout mode not recognized");
        return RC::SOFTWARE_ERROR;
    }

    switch (TilingModeDesc[m_VP2TilingParams->tiling_mode_idx].layout_type)
    {
        default:
            MASSERT(!"Layout type not implemented.");
        case VP2TilingModeDescription::LayoutNoChange:
            break;
        case VP2TilingModeDescription::LayoutPitchLinear:
            CHECK_RC(DoVP2TilingLayoutPitchLinear());
            break;
        case VP2TilingModeDescription::Layout16x16:
            CHECK_RC(DoVP2TilingLayout16x16());
            break;
        case VP2TilingModeDescription::LayoutBlockLinear:
            CHECK_RC(DoVP2TilingLayoutBlockLinear());
            break;
        case VP2TilingModeDescription::LayoutBlockLinearTexture:
            // VP2 is not supported => BLT != BLN.
            CHECK_RC(DoVP2TilingLayoutBlockLinearTexture());
            break;
    }

    return rc;
}

void CheckDmaBuffer::SetBufferSize(UINT64 size)
{
    BufferSize = size;
}

UINT64 CheckDmaBuffer::GetBufferSize() const
{
    return BufferSize;
}

void CheckDmaBuffer::SetUntiledSize(UINT64 size)
{
    UntiledSize = size;
}

UINT64 CheckDmaBuffer::GetUntiledSize() const
{
    bool tiling_active = false;
    if (pTilingParams != NULL)
        tiling_active = pTilingParams->tiling_active;

    if (pBlockLinear)
        tiling_active = true;

    // If tiling is active, and BufferSize is >0, UntiledSize must also be >0.
    // Otherwise, we lost a buffer entirely during untiling, which is a SW bug.
    MASSERT((!tiling_active) || (BufferSize == 0) || (UntiledSize > 0));

    return tiling_active ? UntiledSize : BufferSize;
}

RC SurfaceReader::DoVP2UntilingLayout(UINT08 **ppSurface, CheckDmaBuffer *pCheck)
{
    RC rc = OK;

    if ( (pCheck->pTilingParams == NULL) ||
         (!pCheck->pTilingParams->tiling_active) )
        return OK;

    UINT32 mode_idx = pCheck->pTilingParams->tiling_mode_idx;

    if ( (mode_idx == ILWALID_TILING_MODE_IDX) ||
         (mode_idx >= ( sizeof(TilingModeDesc)/sizeof(TilingModeDesc[0]) ) ) )
    {
        ErrPrintf("Trace: VP2 Untiling Layout mode not recognized");
        return RC::SOFTWARE_ERROR;
    }

    switch (TilingModeDesc[mode_idx].layout_type)
    {
        default:
            MASSERT(!"Layout type not implemented.");
        case VP2TilingModeDescription::LayoutNoChange:
            pCheck->SetUntiledSize(pCheck->GetBufferSize());
            break;
        case VP2TilingModeDescription::LayoutPitchLinear:
            CHECK_RC(DoVP2UntilingLayoutPitchLinear(ppSurface, pCheck));
            break;
        case VP2TilingModeDescription::Layout16x16:
            CHECK_RC(DoVP2UntilingLayout16x16(ppSurface, pCheck));
            break;
        case VP2TilingModeDescription::LayoutBlockLinear:
            CHECK_RC(DoVP2UntilingLayoutBlockLinear(ppSurface, pCheck));
        break;
        case VP2TilingModeDescription::LayoutBlockLinearTexture:
            // VP2 is not supported => BLT != BLN.
            CHECK_RC(DoVP2UntilingLayoutBlockLinearTexture(ppSurface, pCheck));

        break;
    }

    return rc;
}

RC SurfaceReader::DoVP2UntilingLayoutPitchLinear(UINT08 **ppSurface, CheckDmaBuffer *pCheck)
{
    UINT32 dst_width  = pCheck->pTilingParams->width;
    UINT32 dst_height = pCheck->pTilingParams->height;
    UINT32 bpp        = pCheck->pTilingParams->bpp;
    UINT32 src_pitch  = pCheck->pTilingParams->pitch;

    UINT32 dst_pitch = dst_width * bpp;

    MASSERT(src_pitch >= dst_pitch);

    UINT32 colwerted_data_size = dst_height * dst_pitch;
    UINT08 *colwerted_data = new UINT08[colwerted_data_size];
    if (colwerted_data == NULL)
    {
        ErrPrintf("Trace: VP2 Untiling layout - cannot allocate memory\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    UINT08 *src = *ppSurface;

    for (UINT32 y = 0; y < dst_height; y++)
        copy(src + y * src_pitch, src + y * src_pitch + dst_pitch,
             colwerted_data + y * dst_pitch);

    delete[] src;
    *ppSurface = colwerted_data;
    pCheck->SetUntiledSize(colwerted_data_size);

    return OK;
}

RC SurfaceReader::DoVP2UntilingLayout16x16(UINT08 **ppSurface, CheckDmaBuffer *pCheck)
{
    UINT32 dst_width  = pCheck->pTilingParams->width;
    UINT32 dst_height = pCheck->pTilingParams->height;
    UINT32 bpp        = pCheck->pTilingParams->bpp;

    UINT32 src_pitch16 = ((dst_width*bpp + 15)/16)*16;

    UINT32 dst_pitch = dst_width * bpp;

    MASSERT(src_pitch16 >= dst_pitch);

    UINT32 colwerted_data_size = dst_height * dst_pitch;
    UINT08 *colwerted_data = new UINT08[colwerted_data_size];
    if (colwerted_data == NULL)
    {
        ErrPrintf("Trace: VP2 Untiling layout - cannot allocate memory\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    UINT08 *src = *ppSurface;

    for (UINT32 y = 0; y < dst_height; y++)
        for (UINT32 x = 0; x < dst_width; x++)
        {
            UINT32 dst_addr = bpp * x + dst_pitch * y;

            // Formula taken from:
            //hw/vp2/vp2_tools/lib/vp2/scalarLib/driver/driverSurfaceImpl.cc
            UINT32 src_addr =   ((y/16)*16*src_pitch16*bpp)
                              + (x/(16/bpp)*256)
                              + ((y%16)*16)
                              + ((x%(16/bpp))*bpp);

            for (UINT32 byte_idx = 0; byte_idx < bpp; byte_idx++)
            {
                colwerted_data[dst_addr] = src[src_addr];
                src_addr++;
                dst_addr++;
            }
        }

    delete[] src;
    *ppSurface = colwerted_data;
    pCheck->SetUntiledSize(colwerted_data_size);

    return OK;
}

RC SurfaceReader::DoVP2UntilingLayoutBlockLinear(UINT08 **ppSurface,
                                                 CheckDmaBuffer *pCheck)
{
    UINT32 blockRows = pCheck->pTilingParams->blockRows;
    if (blockRows == 0) blockRows = 16;

    BlockLinear block_linear(pCheck->pTilingParams->width,
                                  pCheck->pTilingParams->height,
                                  0,
                                  log2(blockRows/m_lwgpu->GetGpuDevice()->GobHeight()),
                                  0,
                                  pCheck->pTilingParams->bpp,
                                  m_lwgpu->GetGpuDevice());
    return DoVP2UntilingLayoutBlockLinear(ppSurface, pCheck, &block_linear);
}

RC SurfaceReader::DoVP2UntilingLayoutBlockLinearTexture(UINT08 **ppSurface,
                                                   CheckDmaBuffer *pCheck)
{
    UINT32 blockRows = pCheck->pTilingParams->blockRows;
    if (blockRows == 0) blockRows = 16;

    BlockLinearTexture block_linear(pCheck->pTilingParams->width,
                                    pCheck->pTilingParams->height,
                                    0,
                                    log2(blockRows/m_lwgpu->GetGpuDevice()->GobHeight()),
                                    0,
                                    pCheck->pTilingParams->bpp,
                                    m_lwgpu->GetGpuDevice());
    return DoVP2UntilingLayoutBlockLinear(ppSurface, pCheck, &block_linear);
}

RC SurfaceReader::DoVP2UntilingLayoutBlockLinear(UINT08 **ppSurface,
                                            CheckDmaBuffer *pCheck,
                                            BlockLinear *pBlockLinear)
{
    MASSERT(pBlockLinear != 0);

    UINT32 dst_width  = pCheck->pTilingParams->width;
    UINT32 dst_height = pCheck->pTilingParams->height;
    UINT32 bpp        = pCheck->pTilingParams->bpp;

    UINT32 colwerted_data_size = dst_height*dst_width*bpp;
    MASSERT( colwerted_data_size <= pBlockLinear->Size() );

    UINT08 *colwerted_data = new UINT08[colwerted_data_size];

    if (colwerted_data == NULL)
    {
        ErrPrintf("Trace: VP2 Untiling layout - cannot allocate memory\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    UINT08 *src = *ppSurface;
    UINT32 dst_addr = 0;
    for (UINT32 y = 0; y < dst_height; y++)
    {
        for (UINT32 x = 0; x < dst_width; x++)
        {
            UINT32 src_addr = pBlockLinear->Address(x, y, 0);

            for (UINT32 byte_idx = 0; byte_idx < bpp; byte_idx++)
            {
                colwerted_data[dst_addr] = src[src_addr];

                dst_addr++;
                src_addr++;
            }
        }
    }

    delete[] src;
    *ppSurface = colwerted_data;
    pCheck->SetUntiledSize(colwerted_data_size);

    return OK;
}

bool SurfaceReader::IsVP2TilingActive(const VP2TilingParams *pTilingParams)
{
    if (pTilingParams == NULL)
        return false;

    return (pTilingParams->tiling_active);
}

void GenericTraceModule::VP2SupportCleanup()
{
    delete m_VP2TilingParams;
    m_VP2TilingParams = NULL;
}

bool GenericTraceModule::IsVP2TilingActive() const
{
    if (!IsVP2TilingSupported())
        return false;

    if (m_VP2TilingParams == NULL)
        return false;

    return (m_VP2TilingParams->tiling_active);
}

RC GenericTraceModule::GetVP2TilingModeID(UINT32 *tiling_mode_id) const
{
    MASSERT(tiling_mode_id != NULL);

    if (!IsVP2TilingActive())
        return RC::SOFTWARE_ERROR;

    UINT32 mode_idx = m_VP2TilingParams->tiling_mode_idx;

    if ( (mode_idx == ILWALID_TILING_MODE_IDX) ||
         (mode_idx >= ( sizeof(TilingModeDesc)/sizeof(TilingModeDesc[0]) ) ) )
    {
        MASSERT(!"Tiling mode idx is not in the valid range");
        return RC::SOFTWARE_ERROR;
    }

    *tiling_mode_id = TilingModeDesc[mode_idx].method_data_id;

    return OK;
}

RC GenericTraceModule::GetVP2PitchValue(UINT32 *pitch_value) const
{
    MASSERT(pitch_value != NULL);

    if (!IsVP2TilingActive())
        return RC::SOFTWARE_ERROR;

    *pitch_value = m_VP2TilingParams->pitch;

    return OK;
}

RC GenericTraceModule::GetVP2Width(UINT32* pWidth) const
{
    if(0 == pWidth)
        return RC::SOFTWARE_ERROR;

    *pWidth  = m_VP2TilingParams->width;

    return OK;
}

RC GenericTraceModule::GetVP2Height(UINT32* pHeight) const
{
    if(0 == pHeight)
        return RC::SOFTWARE_ERROR;

    *pHeight = m_VP2TilingParams->height;

    return OK;
}

RC GenericTraceModule::GetVP2BlockRows(UINT32* pBlockRows) const
{
    if (0 == pBlockRows)
        return RC::SOFTWARE_ERROR;
    
    if (m_VP2TilingParams != nullptr)
        *pBlockRows = m_VP2TilingParams->blockRows;
    else
        *pBlockRows = 0;

    return OK;
}

RC GenericTraceModule::SetDefualtVP2TilingParams()
{
    if (m_VP2TilingParams == NULL)
    {
        m_VP2TilingParams = new VP2TilingParams;
        if (m_VP2TilingParams == NULL)
            return RC::CANNOT_ALLOCATE_MEMORY;
    }

    m_VP2TilingParams->width  = 1;
    m_VP2TilingParams->height = 1;
    m_VP2TilingParams->bpp    = 8;
    m_VP2TilingParams->pitch  = 1;
    m_VP2TilingParams->blockRows = 0;

    m_VP2TilingParams->big_endian = false;

    m_VP2TilingParams->tiling_mode_idx = 0; // "VP2_1D"
    m_VP2TilingParams->tiling_active   = false;

    m_VP2TilingParams->source = VP2TilingParams::SourceDefault;

    m_VP2TilingParams->tile_mode_reloc_present = false;
    m_VP2TilingParams->pitch_reloc_present = false;

    return OK;
}

bool GenericTraceModule::IsVP2TilingSupported() const
{
    return ( (GpuTrace::FT_VP2_0 <= m_FileType) && (m_FileType <= GpuTrace::FT_VP2_9) );
}

bool GenericTraceModule::IsVP2SourceCmdLine() const
{
    if (!IsVP2TilingSupported())
        return false;

    if (m_VP2TilingParams == NULL)
        return false;

    return (m_VP2TilingParams->source == VP2TilingParams::SourceCommandLine);
}

bool GenericTraceModule::IsVP2LayoutPitchLinear() const
{
    if (!IsVP2TilingSupported())
        return false;

    if (m_VP2TilingParams == NULL)
        return false;

    return (TilingModeDesc[m_VP2TilingParams->tiling_mode_idx].layout_type ==
            VP2TilingModeDescription::LayoutPitchLinear);
}

RC GenericTraceModule::SetVP2TilingModeFromCmdLine()
{
    RC rc = OK;

    MASSERT( (GpuTrace::FT_VP2_0 <= m_FileType) && (m_FileType <= GpuTrace::FT_VP2_9) );

    UINT32 ctxdma_idx = m_FileType - GpuTrace::FT_VP2_0;

    if (params->ParamPresent(VP2TileCmdLineArumentNames[ctxdma_idx]))
    {
        const char *tile_mode_name =
            params->ParamStr(VP2TileCmdLineArumentNames[ctxdma_idx]);

        UINT32 cmd_mode_idx;

        if (!FindVP2TilingModeIdxCmdLine(tile_mode_name, &cmd_mode_idx))
        {
            ErrPrintf("Trace: unrecognized VP2 Tiling mode \"%s\" in \"%s\" option.\n",
                tile_mode_name, VP2TileCmdLineArumentNames[ctxdma_idx]);
            return RC::SOFTWARE_ERROR;
        }

        m_VP2TilingParams->tiling_mode_idx = cmd_mode_idx;
        m_VP2TilingParams->source = VP2TilingParams::SourceCommandLine;
    }

    return rc;
}

void GenericTraceModule::SetVP2TileModeRelocPresent()
{
    if (m_VP2TilingParams != NULL)
        m_VP2TilingParams->tile_mode_reloc_present = true;
}

void GenericTraceModule::SetVP2PitchRelocPresent()
{
    if (m_VP2TilingParams != NULL)
        m_VP2TilingParams->pitch_reloc_present = true;
}

bool GenericTraceModule::AreRequiredVP2TileModeRelocsPresent()
{
    if (!IsVP2TilingActive())
        return true;

    if (m_VP2TilingParams->source != VP2TilingParams::SourceCommandLine)
        return true;

    return m_VP2TilingParams->tile_mode_reloc_present;
}

// Assumption is made that only the pitch linear mode needs the pitch relocs
bool GenericTraceModule::AreRequiredVP2PitchRelocsNeeded()
{
    if (!IsVP2TilingActive())
        return true;

    if (m_VP2TilingParams->source != VP2TilingParams::SourceCommandLine)
        return true;

    if (TilingModeDesc[m_VP2TilingParams->tiling_mode_idx].layout_type !=
            VP2TilingModeDescription::LayoutPitchLinear)
        return true;

    return m_VP2TilingParams->pitch_reloc_present;
}
