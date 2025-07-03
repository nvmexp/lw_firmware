/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gv11bfs.h"
#include "gpu/reghal/reghal.h"

//------------------------------------------------------------------------------
GV11BFs::GV11BFs(GpuSubdevice *pSubdev):
    GV10xFs(pSubdev)
{
}

/* virtual */ UINT32 GV11BFs::GetEngineResetMask()
{
    RegHal &regs = m_pSub->Regs();
    // This mask was actually pulled from RM's bifGetValidEnginesToReset_GV11B
    return regs.LookupMask(MODS_PMC_ENABLE_PGRAPH) |
           regs.LookupMask(MODS_PMC_ENABLE_PMEDIA) |
           regs.LookupMask(MODS_PMC_ENABLE_CE0) |
           regs.LookupMask(MODS_PMC_ENABLE_CE1) |
           regs.LookupMask(MODS_PMC_ENABLE_CE2) |
           regs.LookupMask(MODS_PMC_ENABLE_PFIFO) |
           regs.LookupMask(MODS_PMC_ENABLE_PWR) |
           regs.LookupMask(MODS_PMC_ENABLE_PDISP) |
           regs.LookupMask(MODS_PMC_ENABLE_LWDEC) |
           regs.LookupMask(MODS_PMC_ENABLE_LWENC0) |
           regs.LookupMask(MODS_PMC_ENABLE_LWENC1) |
           regs.LookupMask(MODS_PMC_ENABLE_LWENC2) |
           regs.LookupMask(MODS_PMC_ENABLE_SEC);
}

//------------------------------------------------------------------------------
CREATE_GPU_FS_FUNCTION(GV11BFs);
