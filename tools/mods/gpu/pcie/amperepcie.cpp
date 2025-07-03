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

#include "amperepcie.h"
#include "core/include/lwrm.h"
#include "ctrl/ctrl2080.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "mods_reg_hal.h"

//-----------------------------------------------------------------------------
/* virtual */ bool AmperePcie::DoSupportsFomMode(Pci::FomMode mode)
{
    return (mode == Pci::EYEO_Y);
}

//-----------------------------------------------------------------------------
RC AmperePcie::DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs)
{
    if (!SupportsFomMode(mode))
    {
        Printf(Tee::PriError, "Unsupported FOM mode : %d\n", mode);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    MASSERT(status);
    MASSERT(status->size());
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    const UINT32 numLanes = static_cast<UINT32>(status->size());
    const UINT32 rxLaneMask = GetRegLanesRx();
    MASSERT(rxLaneMask);
    const UINT32 firstLane = (rxLaneMask) ? static_cast<UINT32>(Utility::BitScanForward(rxLaneMask)) : 0;

    EomSettings settings;
    CHECK_RC(GetEomSettings(mode, &settings));
    LW2080_CTRL_CMD_BUS_SET_EOM_PARAMETERS_PARAMS eomParams =
    { 
        settings.mode,
        settings.numBlocks,
        settings.numErrors
    };
    regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        GetSubdevice(),
        LW2080_CTRL_CMD_BUS_SET_EOM_PARAMETERS,
        &eomParams,
        sizeof(eomParams)
    ));

    regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    regs.Write32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_EN_DISABLE);

    UINT32 doneStatus = 0x0;
    auto pollEomDone = [&]()->bool
        {
            return regs.Test32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_DONE, doneStatus);
        };
    for (UINT32 i = 0; i < numLanes; i++)
    {
        regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (0x1 << (firstLane + i)));
        if (Platform::GetSimulationMode() != Platform::Fmodel) // Skip for sanity runs
        {
            CHECK_RC(Tasker::PollHw(timeoutMs, pollEomDone, __FUNCTION__));
        }
    }

    regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    regs.Write32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_OVRD_ENABLE);
    regs.Write32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_EN_ENABLE);

    doneStatus = 0x1;
    for (UINT32 i = 0; i < numLanes; i++)
    {
        regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (0x1 << (firstLane + i)));
        CHECK_RC(Tasker::PollHw(timeoutMs, pollEomDone, __FUNCTION__));
    }

    regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    regs.Write32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_EN_DISABLE);
    regs.Write32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_OVRD_DISABLE);

    for (UINT32 i = 0; i < numLanes; i++)
    {
        regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (0x1 << (firstLane + i)));
        status->at(i) = regs.Read32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_STATUS);
    }
    return RC::OK;
}

RC AmperePcie::DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData)
{
    RegHal &regs =  GetSubdevice()->Regs();
    const UINT32 laneMask = regs.LookupFieldValueMask(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT);
    if (!((1 << lane) & laneMask))
    {
        Printf(Tee::PriError, "Invalid lane %u specified\n", lane);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    RC rc;
    LW2080_CTRL_CMD_BUS_GET_UPHY_DLN_CFG_SPACE_PARAMS getDlnRegParams = { addr, (1U << lane), 0 };
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        GetSubdevice(),
        LW2080_CTRL_CMD_BUS_GET_UPHY_DLN_CFG_SPACE,
        &getDlnRegParams,
        sizeof(getDlnRegParams)
    ));
    *pData = getDlnRegParams.regValue;
    return rc;
}

RC AmperePcie::GetEomSettings(Pci::FomMode mode, EomSettings* pSettings)
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
