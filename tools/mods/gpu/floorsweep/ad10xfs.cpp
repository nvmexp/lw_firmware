/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ad10xfs.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/reghal/reghal.h"
#include "core/include/gpu.h"
#include "core/include/platform.h"

//------------------------------------------------------------------------------
AD10xFs::AD10xFs(GpuSubdevice *pSubdev):
    GA10xFs(pSubdev)
{
}

//------------------------------------------------------------------------------
RC AD10xFs::InitializePriRing_FirstStage(bool bInSetup)
{
    RC rc;
    RegHal &regs = m_pSub->Regs();

    if (regs.HasRWAccess(MODS_PMC_ELPG_ENABLE))
    {
        UINT32 regVal = regs.Read32(MODS_PMC_ELPG_ENABLE);
        regs.SetField(&regVal, MODS_PMC_ELPG_ENABLE_XBAR_DISABLED);
        regs.SetField(&regVal, MODS_PMC_ELPG_ENABLE_L2_DISABLED);
        regs.SetField(&regVal, MODS_PMC_ELPG_ENABLE_HUB_DISABLED);
        regs.Write32(MODS_PMC_ELPG_ENABLE, regVal);
    }
    CHECK_RC(GA10xFs::InitializePriRing_FirstStage(bInSetup));

    return rc;
}

RC AD10xFs::InitializePriRing()
{
    RC rc;
    RegHal &regs = m_pSub->Regs();

    CHECK_RC(GA10xFs::InitializePriRing());
    if (regs.HasRWAccess(MODS_PMC_ELPG_ENABLE))
    {
        UINT32 regVal = regs.Read32(MODS_PMC_ELPG_ENABLE);
        regs.SetField(&regVal, MODS_PMC_ELPG_ENABLE_XBAR_ENABLED);
        regs.SetField(&regVal, MODS_PMC_ELPG_ENABLE_L2_ENABLED);
        regs.SetField(&regVal, MODS_PMC_ELPG_ENABLE_HUB_ENABLED);
        regs.Write32(MODS_PMC_ELPG_ENABLE, regVal);
    }

    return rc;
}

//------------------------------------------------------------------------------
CREATE_GPU_FS_FUNCTION(AD10xFs);
