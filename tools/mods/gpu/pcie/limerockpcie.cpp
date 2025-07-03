/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "limerockpcie.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "ctrl_dev_internal_lwswitch.h"
#include "device/interface/lwlink/lwlregs.h"
#include "gpu/include/gpusbdev.h"
#include "mods_reg_hal.h"

//-----------------------------------------------------------------------------
/* virtual */ bool LimerockPcie::DoSupportsFomMode(Pci::FomMode mode)
{
    return (mode == Pci::EYEO_Y);
}

//------------------------------------------------------------------------------
RC LimerockPcie::DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs)
{
    if (!SupportsFomMode(mode))
    {
        Printf(Tee::PriError, "Unsupported FOM mode : %d\n", mode);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    MASSERT(status);
    MASSERT(status->size());
    RC rc;
    const UINT32 numLanes = static_cast<UINT32>(status->size());
    const UINT32 rxLaneMask = GetRegLanesRx();
    MASSERT(rxLaneMask);
    const UINT32 firstLane = (rxLaneMask) ? static_cast<UINT32>(Utility::BitScanForward(rxLaneMask)) : 0;

    LWSWITCH_PEX_GET_EOM_STATUS_PARAMS eomStatus = { };
    EomSettings settings;
    CHECK_RC(GetEomSettings(mode, &settings));
    eomStatus.mode  = settings.mode;
    eomStatus.nblks = settings.numBlocks;
    eomStatus.nerrs = settings.numErrors;
    eomStatus.berEyeSel = settings.berEyeSel;
    eomStatus.laneMask = rxLaneMask;
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
         LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_PEX_GET_EOM_STATUS,
         &eomStatus,
         sizeof(eomStatus)));

    for (UINT32 lwrLane = firstLane; lwrLane < (firstLane + numLanes); lwrLane++)
    {
        status->at(lwrLane - firstLane) = eomStatus.eomStatus[lwrLane];
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LimerockPcie::DoSetLinkSpeed(Pci::PcieLinkSpeed newSpeed)
{
    RC rc;
    LWSWITCH_SET_PCIE_LINK_SPEED_PARAMS setLinkSpeedParams = { };
    switch (newSpeed)
    {
        case Pci::Speed2500MBPS:
            setLinkSpeedParams.linkSpeed = LWSWITCH_BIF_LINK_SPEED_GEN1PCIE;
            break;
        case Pci::Speed5000MBPS:
            setLinkSpeedParams.linkSpeed = LWSWITCH_BIF_LINK_SPEED_GEN2PCIE;
            break;
        case Pci::Speed8000MBPS:
            setLinkSpeedParams.linkSpeed = LWSWITCH_BIF_LINK_SPEED_GEN3PCIE;
            break;
        case Pci::Speed16000MBPS:
            setLinkSpeedParams.linkSpeed = LWSWITCH_BIF_LINK_SPEED_GEN4PCIE;
            break;
        case Pci::Speed32000MBPS:
            setLinkSpeedParams.linkSpeed = LWSWITCH_BIF_LINK_SPEED_GEN5PCIE;
            break;
        default:
            Printf(Tee::PriError, "Invalid PCIE Link Speed = %d\n", static_cast<UINT32>(newSpeed));
            return RC::ILWALID_ARGUMENT;
    }

    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
         LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_SET_PCIE_LINK_SPEED,
         &setLinkSpeedParams,
         sizeof(setLinkSpeedParams)));
    return rc;
}

//------------------------------------------------------------------------------
RC LimerockPcie::DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData)
{
    RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    const UINT32 laneMask = regs.LookupFieldValueMask(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT);
    if (!((1 << lane) & laneMask))
    {
        Printf(Tee::PriError, "Invalid lane %u specified\n", lane);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    RC rc;
    LWSWITCH_GET_PEX_UPHY_DLN_CFG_SPACE_PARAMS getDlnRegParams = { };
    getDlnRegParams.regAddress = addr;
    getDlnRegParams.laneSelectMask = 1U << lane;
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
         LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_PEX_GET_UPHY_DLN_CFG_SPACE,
         &getDlnRegParams,
         sizeof(getDlnRegParams)));
    *pData = getDlnRegParams.regValue;
    return rc;
}


//------------------------------------------------------------------------------
RC LimerockPcie::GetEomSettings(Pci::FomMode mode, EomSettings* pSettings)
{
    if (mode != Pci::EYEO_Y)
    {
        Printf(Tee::PriError, "Invalid or unsupported FOM mode : %d\n", mode);
        return RC::ILWALID_ARGUMENT;
    }

    constexpr UINT32 eomModeY = 2;
    pSettings->mode      = eomModeY;
    pSettings->numBlocks = 0x8;
    pSettings->numErrors = 0x4;
    return RC::OK;
}
