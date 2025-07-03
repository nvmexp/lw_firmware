/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "limerockhulkloader.h"
#include "device/interface/lwlink/lwlregs.h"
#include "gpu/utility/lwswitchfalcon.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_limerock_dev.h"

// HULK ucodes
#include "gpu/ucode/hulk_lr10.enc_dbg.h"
#include "gpu/ucode/hulk_lr10.enc_prod.h"

unique_ptr<FalconImpl> LimerockHulkLoader::GetFalcon() noexcept
{
    static constexpr LwSwitchFalcon::FalconType falconType = LwSwitchFalcon::FalconType::SOE;
    return make_unique<LwSwitchFalcon>(m_limerockDev, falconType);
}
void LimerockHulkLoader::GetHulkFalconBinaryArray(
    unsigned char const ** array, 
    size_t *arrayLength) noexcept
{
    if (m_limerockDev->GetInterface<LwLinkRegs>()->Regs().
        Test32(MODS_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS_DATA_YES))
    {
        *array = bin_hs_lr10_hulk_enc_prod_bin;
        *arrayLength = bin_hs_lr10_hulk_enc_prod_bin_len;
    }
    else
    {
        *array = bin_hs_lr10_hulk_enc_dbg_bin;
        *arrayLength = bin_hs_lr10_hulk_enc_dbg_bin_len;
    }
}
void LimerockHulkLoader::GetLoadStatus(UINT32* progress, UINT32* errCode) noexcept
{
    // Result stored in LW_LWLSAW_SCRATCH_WARM
    // 9:0    - Error Code
    // 15:12  - Progress
    // (See https://confluence.lwpu.com/display/GFWGA10X/Scratch+Registers)
    static constexpr UINT32 LW_PRIV_OFFSET_LWLSAW = 0x00028000;
    UINT32 returnInfo = 0;
    m_limerockDev->GetInterface<LwLinkRegs>()->RegRd(0,
        RegHalDomain::RAW,
        LW_PRIV_OFFSET_LWLSAW +
        m_limerockDev->GetInterface<LwLinkRegs>()->Regs().LookupAddress(MODS_LWLSAW_SCRATCH_WARM),
        &returnInfo);
    *errCode = returnInfo & 0x3FF;
    *progress = (returnInfo >> 12) & 0xF;
}