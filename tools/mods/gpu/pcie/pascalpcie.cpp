/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "pascalpcie.h"
#include "core/include/pci.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "mods_reg_hal.h"

namespace
{
    bool PollEomDone(void *args)
    {
        GpuSubdevice* pGpuSubdev = static_cast<GpuSubdevice*>(args);
        return !!pGpuSubdev->Regs().Read32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_DONE);
    }
}

//------------------------------------------------------------------------------
bool PascalPcie::DoSupportsFomMode(Pci::FomMode mode)
{
    switch (mode)
    {
        case Pci::EYEO_X:
        case Pci::EYEO_Y:
            return true;
        default:
            break;
    }
    return false;
}

//------------------------------------------------------------------------------
RC PascalPcie::DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs)
{
    MASSERT(status);
    MASSERT(status->size());
    RC rc;
    GpuSubdevice* pGpuSubdev = GetSubdevice();
    const UINT32 rxLaneMask = GetRegLanesRx();
    MASSERT(rxLaneMask);
    const UINT32 firstLane = (rxLaneMask) ? static_cast<UINT32>(Utility::BitScanForward(rxLaneMask)) : 0;
    const UINT32 laneMask = pGpuSubdev->Regs().LookupFieldValueMask(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT);

    pGpuSubdev->Regs().Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    UINT32 ctl8 = pGpuSubdev->Regs().Read32(MODS_XP_PEX_PAD_CTL_8);
    const UINT32 errorBits = 0xe;
    // log scale, numBits=2^(errorBits+9). Higher number means lower bit error rate. BER = (numErrors/numbits)
    const UINT32 numErrors = 0x7;
    // target is numErrors/(numErrors*A). Higher number gives more confidence of meeting 1/A target
    const UINT32 wdata = (errorBits << 4) | numErrors;
    if (mode == Pci::EYEO_X)
    {
        pGpuSubdev->Regs().SetField(&ctl8, MODS_XP_PEX_PAD_CTL_8_CFG_WDATA, 0x500 | wdata);
    }
    else if (mode == Pci::EYEO_Y)
    {
        pGpuSubdev->Regs().SetField(&ctl8, MODS_XP_PEX_PAD_CTL_8_CFG_WDATA, 0xb00 | wdata);
    }
    else
    {
        Printf(Tee::PriError, "Invalid Mode = %u\n", mode);
        return RC::SOFTWARE_ERROR;
    }
    pGpuSubdev->Regs().SetField(&ctl8, MODS_XP_PEX_PAD_CTL_8_CFG_ADDR, 0x67);
    pGpuSubdev->Regs().SetField(&ctl8, MODS_XP_PEX_PAD_CTL_8_CFG_WDS, 0x1);
    pGpuSubdev->Regs().Write32(MODS_XP_PEX_PAD_CTL_8, ctl8);

    pGpuSubdev->Regs().Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    pGpuSubdev->Regs().Write32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_EN_DISABLE);
    pGpuSubdev->Regs().Write32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_OVRD_ENABLE);
    pGpuSubdev->Regs().Write32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_EN_ENABLE);

    for (UINT32 i = 0; i < status->size(); i++)
    {
        pGpuSubdev->Regs().Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (laneMask << (firstLane + i)) & laneMask);
        CHECK_RC(POLLWRAP_HW(&PollEomDone, (void*)pGpuSubdev, timeoutMs));
    }

    pGpuSubdev->Regs().Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    pGpuSubdev->Regs().Write32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_EN_DISABLE);
    pGpuSubdev->Regs().Write32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_OVRD_DISABLE);

    for (UINT32 i = 0; i < status->size(); i++)
    {
        pGpuSubdev->Regs().Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (laneMask << (firstLane + i)) & laneMask);
        status->at(i) = pGpuSubdev->Regs().Read32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_STATUS);
    }

    return OK;
}

//------------------------------------------------------------------------------
UINT32 PascalPcie::DoGetActiveLaneMask(RegBlock regBlock) const
{
    if (regBlock == RegBlock::CLN)
        return 1;

    return GetRegLanesRx();
}

UINT32 PascalPcie::DoGetMaxLanes(RegBlock regBlock) const 
{
    if (regBlock == RegBlock::CLN)
        return 1;

    return 16;
}

RC PascalPcie::DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData)
{
    RegHal &regs =  GetSubdevice()->Regs();
    const UINT32 laneMask = regs.LookupFieldValueMask(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT);
    if (!((1 << lane) & laneMask))
    {
        Printf(Tee::PriError, "Invalid lane %u specified\n", lane);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    UINT32 padCtl8 = regs.Read32(MODS_XP_PEX_PAD_CTL_8);
    regs.SetField(&padCtl8, MODS_XP_PEX_PAD_CTL_8_CFG_ADDR, addr);
    regs.SetField(&padCtl8, MODS_XP_PEX_PAD_CTL_8_CFG_RDS, 1);
    regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (1 << lane));
    regs.Write32(MODS_XP_PEX_PAD_CTL_8, padCtl8);
    // Later GPUs (e.g. Ampere) have priv protected access with a decode trap because there
    // is no priv protection register, check if it can be successfully written by reading
    // back the address that was just written
    if (!regs.Test32(MODS_XP_PEX_PAD_CTL_8_CFG_ADDR, addr))
    {
        Printf(Tee::PriWarn,
               "PCIE UPHY PAD registers may not be read due to priv protection\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }
    *pData = static_cast<UINT16>(regs.Read32(MODS_XP_PEX_PAD_CTL_9_CFG_RDATA));
    return RC::OK;
}

RC PascalPcie::DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data)
{
    RegHal &regs =  GetSubdevice()->Regs();
    const UINT32 laneMask = regs.LookupFieldValueMask(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT);
    if (!((1 << lane) & laneMask))
    {
        Printf(Tee::PriError, "Invalid lane %u specified\n", lane);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    UINT32 padCtl8 = regs.Read32(MODS_XP_PEX_PAD_CTL_8);
    regs.SetField(&padCtl8, MODS_XP_PEX_PAD_CTL_8_CFG_WDATA, data);
    regs.SetField(&padCtl8, MODS_XP_PEX_PAD_CTL_8_CFG_ADDR, addr);
    regs.SetField(&padCtl8, MODS_XP_PEX_PAD_CTL_8_CFG_WDS, 1);
    regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (1 << lane));
    regs.Write32(MODS_XP_PEX_PAD_CTL_8, padCtl8);
    // Later GPUs (e.g. Ampere) have priv protected access with a decode trap because there
    // is no priv protection register, check if it can be successfully written by reading
    // back the address that was just written
    if (!regs.Test32(MODS_XP_PEX_PAD_CTL_8_CFG_ADDR, addr))
    {
        Printf(Tee::PriWarn,
               "PCIE UPHY PAD registers may not be written due to priv protection\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }
    return RC::OK;
}

RC PascalPcie::DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData)
{
    RegHal &regs = GetSubdevice()->Regs();
    UINT32 pllCtl5 = regs.Read32(MODS_XP_PEX_PLL_CTL5);
    regs.SetField(&pllCtl5, MODS_XP_PEX_PLL_CTL5_CFG_ADDR, addr);
    regs.SetField(&pllCtl5, MODS_XP_PEX_PLL_CTL5_CFG_RDS_PENDING);
    regs.Write32(MODS_XP_PEX_PLL_CTL5, pllCtl5);
    // Later GPUs (e.g. Ampere) have priv protected access with a decode trap because there
    // is no priv protection register, check if it can be successfully written by reading
    // back the address that was just written
    if (!regs.Test32(MODS_XP_PEX_PLL_CTL5_CFG_ADDR, addr))
    {
        Printf(Tee::PriWarn,
               "PCIE UPHY PLL registers may not be read due to priv protection\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }
    *pData = static_cast<UINT16>(regs.Read32(MODS_XP_PEX_PLL_CTL6_CFG_RDATA));
    return RC::OK;
}
