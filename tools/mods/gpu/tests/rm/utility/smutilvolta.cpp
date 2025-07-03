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

#include "volta/gv100/dev_graphics_nobundle.h"
#include "smutil.h"

RC SMUtil::ReadWarpGlobalMaskValuesVolta
(
    GpuSubdevice *pSubdev,
    LwRm::Handle hCh,
    UINT32 *warpValue,
    UINT32 *globalValue,
    LwBool isCtxsw
)
{
    const RegHal &regs = pSubdev->Regs();
    // Read the current register value
    if (isCtxsw)
    {
        pSubdev->CtxRegRd32(hCh, regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_WARP_ESR_REPORT_MASK), warpValue);
        pSubdev->CtxRegRd32(hCh, regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_GLOBAL_ESR_REPORT_MASK), globalValue);
    }
    else
    {
        *warpValue = pSubdev->RegRd32(regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_WARP_ESR_REPORT_MASK));
        *globalValue = pSubdev->RegRd32(regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_GLOBAL_ESR_REPORT_MASK));
    }

    return OK;
}

RC SMUtil::WriteWarpGlobalMaskValuesVolta
(
    GpuSubdevice *pSubdev,
    LwRm::Handle hCh,
    UINT32 warpValue,
    UINT32 globalValue,
    LwBool isCtxsw
)
{
    const RegHal &regs = pSubdev->Regs();
    // Read the current register value
    if (isCtxsw)
    {
        pSubdev->CtxRegWr32(hCh,  regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_WARP_ESR_REPORT_MASK), warpValue);
        pSubdev->CtxRegWr32(hCh,  regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_GLOBAL_ESR_REPORT_MASK), globalValue);
    }
    else
    {
        pSubdev->RegWr32(regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_WARP_ESR_REPORT_MASK), warpValue);
        pSubdev->RegWr32(regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_GLOBAL_ESR_REPORT_MASK), globalValue);
    }

    return OK;
}
