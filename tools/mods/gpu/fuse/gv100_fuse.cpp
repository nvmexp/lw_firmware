/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gv100_fuse.h"
#include "gpu/include/gpusbdev.h"
#include "volta/gv100/dev_bus.h"
#include "volta/gv100/dev_pri_ringstation_sys.h"

//-----------------------------------------------------------------------------
GV100Fuse::GV100Fuse(Device* pSubdev) : GP10xFuse(pSubdev)
{
    GpuSubdevice *pGpuSubdev = (GpuSubdevice*)GetDevice();
    switch (pGpuSubdev->DeviceId())
    {
        case Gpu::GV100:
            m_GPUSuffix = "GV100";
            break;
        default:
            m_GPUSuffix = "UNKNOWN";
            break;
    }

    m_RwlVal = MODS_FUSE_DEBUGCTRL_RWL_ENABLE;

    const FuseUtil::FuseDefMap* pFuseDefMap = nullptr;
    const FuseUtil::SkuList* pSkuList = nullptr;
    const FuseUtil::MiscInfo* pMiscInfo = nullptr;
    const FuseDataSet* pFuseDataset = nullptr;

    RC rc = ParseXmlInt(&pFuseDefMap, &pSkuList, &pMiscInfo, &pFuseDataset);

    if (rc == OK)
    {
        m_FuseSpec.NumOfFuseReg = pMiscInfo->MacroInfo.FuseRows;

        MASSERT(pMiscInfo->MacroInfo.FuseRecordStart * pMiscInfo->MacroInfo.FuseCols
            == pMiscInfo->FuselessStart);
        m_FuseSpec.FuselessStart = pMiscInfo->FuselessStart;    //ramRepairFuseBlockBegin

        MASSERT(m_FuseSpec.NumOfFuseReg * pMiscInfo->MacroInfo.FuseCols - 1
            == pMiscInfo->FuselessEnd);
        m_FuseSpec.FuselessEnd = pMiscInfo->FuselessEnd - 32;    //ramRepairFuseBlockEnd - 32
    }
    else
    {
        Printf(Tee::PriLow, "Fuse file could not be parsed! RC: %d\n", static_cast<INT32>(rc));
        Printf(Tee::PriLow, "Defaulting Fuse Spec to 0s to ilwalidate Fuse object.\n");
        m_FuseSpec.NumOfFuseReg = 0;
        m_FuseSpec.FuselessStart = 0;
        m_FuseSpec.FuselessEnd = 0;
    }

    m_FuseSpec.NumKFuseWords = 256;

    // Update the IFF decode table for GV100 changes
    m_SetFieldSpaceName[IFF_SPACE(PPRIV_SYS_0)] = "LW_PTRIM_8";
    m_SetFieldSpaceAddr[IFF_SPACE(PPRIV_SYS_0)] = DRF_BASE(LW_PPRIV_SYS);
}
//-----------------------------------------------------------------------------
RC GV100Fuse::GetFuseWord(UINT32 fuseRow, FLOAT64 timeoutMs, UINT32* pRetData)
{
    return GM20xFuse::DirectFuseRead(fuseRow, timeoutMs, pRetData);
}

//-----------------------------------------------------------------------------
bool GV100Fuse::IsFusePrivSelwrityEnabled()
{
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    RegHal& regs = pSubdev->Regs();
    return regs.Test32(MODS_FUSE_OPT_PRIV_SEC_EN_DATA_YES) &&
              (regs.Test32(MODS_FUSE_WDATA_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_DISABLE) ||
               regs.Test32(MODS_FUSE_FUSECTRL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_DISABLE));
}

//-----------------------------------------------------------------------------
RC GV100Fuse::CheckFuseblowEnabled()
{
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    if (!pSubdev->Regs().Test32(MODS_FUSE_WDATA_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED) ||
        !pSubdev->Regs().Test32(MODS_FUSE_FUSECTRL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED))
    {
        Printf(Tee::PriError, "Secure PMU could not be loaded !\n");
        return RC::PMU_DEVICE_FAIL;
    }
    return OK;
}

