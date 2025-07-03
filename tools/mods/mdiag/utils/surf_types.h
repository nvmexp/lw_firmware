/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _SURF_TYPES_H
#define _SURF_TYPES_H

#include "raster_types.h"
#include "mdiagsurf.h"

#define _ILWALID_COLOR_FORMAT   0
#define _ILWALID_ZETA_FORMAT    0

#define MAX_RENDER_TARGETS   8
#define TESLA_RENDER_TARGETS 8

// These enums match Lwrie, although it would be good to remove the assumptions
// that they do.
// Reuse the empty slots (0x1 and 0x2) for AAMODE_???_D3D mode for now.
// We probably need to reorganize them to comply with current tesla2 order (and
// remove unused entries).
enum AAmodetype
{
    AAMODE_1X1            = 0x0,
    AAMODE_2X1_D3D        = 0x1,
    AAMODE_4X2_D3D        = 0x2,
    AAMODE_2X1            = 0x3,
    AAMODE_2X2_REGGRID    = 0x4,
    AAMODE_2X2            = 0x5,
    AAMODE_4X2            = 0x6,
    AAMODE_2X2_VC_4       = 0x7,
    AAMODE_2X2_VC_12      = 0x8,
    AAMODE_4X2_VC_8       = 0x9,
    AAMODE_4X2_VC_24      = 0xa,
    AAMODE_4X4            = 0xb,
    FORCE_DWORD_AA_MODE   = 0xffffffff
};

const UINT32 AAwidthScale[]  = { 1, 2, 4, 2, 2, 2, 4, 2,  2,  4,  4,  4 };
const UINT32 AAheightScale[] = { 1, 1, 2, 1, 2, 2, 2, 2,  2,  2,  2,  4 };
const UINT32 AAsamples[]     = { 1, 2, 8, 2, 4, 4, 8, 8, 16, 16, 32, 16 };

enum _SCREEN_SIZE
{
    SCRNSIZE_80x60 =        0,
    SCRNSIZE_160x120 =      1,
    SCRNSIZE_320x240 =      2,
    SCRNSIZE_640x480 =      3,
    SCRNSIZE_800x600 =      4,
    SCRNSIZE_1024x768 =     5,
    SCRNSIZE_1280x1024 =    6,
    SCRNSIZE_1600x1200 =    7,
    SCRNSIZE_2048x1536 =    8
};

enum _PAGE_LAYOUT
{
    PAGE_PHYSICAL     = 0, // Physical context DMA
    PAGE_VIRTUAL      = 1, // Virtual context DMA with default page size
    PAGE_VIRTUAL_4KB  = 2, // Virtual context DMA with page size forced to 4KB
    PAGE_VIRTUAL_64KB = 3, // Virtual context DMA with page size forced to 64KB
    PAGE_VIRTUAL_2MB  = 4, // Virtual context DMA with page size forced to 2MB
    PAGE_VIRTUAL_512MB = 5, // Virtual context DMA with page size forced to 512MB
};

enum _RAWMODE
{
    RAWSTART        = 0,
    RAWOFF          = 0,
    RAWON           = 1,
    RAWEND          = 1 // this must be the last entry, matching the last defined valid mode
};

enum _DMA_TARGET
{
    _DMA_TARGET_VIDEO        = 0,
    _DMA_TARGET_COHERENT     = 2,
    _DMA_TARGET_NONCOHERENT  = 3,
    _DMA_TARGET_P2P  =         4
};

enum _SCALE_LMEM_SIZE
{
    SCALE_LMEM_SIZE_START      = 0,
    SCALE_LMEM_SIZE_PER_WARP   = 0,
    SCALE_LMEM_SIZE_PER_TPC     = 1,
    SCALE_LMEM_SIZE_NONE       = 2,
    SCALE_LMEM_SIZE_END        = 2,
};

enum RAW_MEMORY_DUMP_MODE
{
    DUMP_RAW_VIA_BAR1_PHYS = 0,
    DUMP_RAW_VIA_PRAMIN = 1,
    DUMP_RAW_VIA_PTE_CHANGE = 2,
    DUMP_RAW_MEM_OP_ONLY = 3,
    DUMP_RAW_VIA_PHYS_READ = 4,
    DUMP_RAW_CRC = 5
};

class ArgReader;

extern const char *DmaTargetName[];

extern AAmodetype GetAAModeFromSurface(MdiagSurf *surface);
extern void       SetSurfaceAAMode(MdiagSurf *surface, AAmodetype mode);
extern bool       GetAAModeFromName(const char* name, Surface2D::AAMode* aamode);
extern AAmodetype GetTirAAMode(const ArgReader *params, AAmodetype aaMode);
extern UINT32 GetProgrammableSampleData(MdiagSurf *surface, UINT32 index,
    const ArgReader *params);
extern UINT32 GetProgrammableSampleDataFloat(MdiagSurf *surface, UINT32 index,
    const ArgReader *params);

#endif
