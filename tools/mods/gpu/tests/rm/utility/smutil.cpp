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
#include "mods_reg_hal.h"

#include "fermi/gf100/dev_graphics_nobundle.h"
#include "smutil.h"

SMUtil::SMUtil()
{
}

SMUtil::~SMUtil()
{
}

RC SMUtil::ReadWarpGlobalMaskValues
(
    GpuSubdevice *pSubdev,
    LwRm::Handle hCh,
    UINT32 *warpValue,
    UINT32 *globalValue,
    LwBool isCtxsw
)
{
    const RegHal &regs = pSubdev->Regs();
    if((pSubdev->GetParentDevice())->GetFamily() >= GpuDevice::Volta)
    {
        return ReadWarpGlobalMaskValuesVolta(pSubdev, hCh, warpValue, globalValue, isCtxsw);
    }

    // Read the current register value
    if (isCtxsw)
    {
        pSubdev->CtxRegRd32(hCh, regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SM_HWW_WARP_ESR_REPORT_MASK), warpValue);
        pSubdev->CtxRegRd32(hCh, regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SM_HWW_GLOBAL_ESR_REPORT_MASK), globalValue);
    }
    else
    {
        *warpValue = pSubdev->RegRd32(regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SM_HWW_WARP_ESR_REPORT_MASK));
        *globalValue = pSubdev->RegRd32(regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SM_HWW_GLOBAL_ESR_REPORT_MASK));
    }

    return OK;
}

RC SMUtil::WriteWarpGlobalMaskValues
(
    GpuSubdevice *pSubdev,
    LwRm::Handle hCh,
    UINT32 warpValue,
    UINT32 globalValue,
    LwBool isCtxsw
)
{
    const RegHal &regs  = pSubdev->Regs();
    if((pSubdev->GetParentDevice())->GetFamily() >= GpuDevice::Volta)
    {
        return WriteWarpGlobalMaskValuesVolta(pSubdev, hCh, warpValue, globalValue, isCtxsw);
    }

    // Read the current register value
    if (isCtxsw)
    {
        pSubdev->CtxRegWr32(hCh, regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SM_HWW_WARP_ESR_REPORT_MASK), warpValue);
        pSubdev->CtxRegWr32(hCh, regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SM_HWW_GLOBAL_ESR_REPORT_MASK), globalValue);
    }
    else
    {
        pSubdev->RegWr32(regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SM_HWW_WARP_ESR_REPORT_MASK), warpValue);
        pSubdev->RegWr32(regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SM_HWW_GLOBAL_ESR_REPORT_MASK), globalValue);
    }

    return OK;
}
