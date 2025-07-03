/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "voltapcie.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "mods_reg_hal.h"

// TODO: These should be defined in some external header file
// See Bug 1808284
// Field values taken from comment at:
// https://lwbugswb.lwpu.com/LwBugs5/SWBug.aspx?bugid=1808284&cmtNo=64
// Register addresses take from comment at:
// https://lwbugswb.lwpu.com/LwBugs5/SWBug.aspx?bugid=1880995&cmtNo=13
#define LW_EOM_CTRL1              0xAB
#define LW_EOM_CTRL1_TYPE         1:0
#define LW_EOM_CTRL1_TYPE_ODD     0x1
#define LW_EOM_CTRL1_TYPE_EVEN    0x2
#define LW_EOM_CTRL1_TYPE_ODDEVEN 0x3
#define LW_EOM_CTRL2              0xAF
#define LW_EOM_CTRL2_NERRS        3:0
#define LW_EOM_CTRL2_NBLKS        7:4
#define LW_EOM_CTRL2_MODE         11:8
#define LW_EOM_CTRL2_MODE_X       0x5
#define LW_EOM_CTRL2_MODE_XL      0x0
#define LW_EOM_CTRL2_MODE_XH      0x1
#define LW_EOM_CTRL2_MODE_Y       0xb
#define LW_EOM_CTRL2_MODE_YL      0x6
#define LW_EOM_CTRL2_MODE_YH      0x7

RC VoltaPcie::DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs)
{
    // PHY registers are not implemented in C models, return static non-zero data
    // in that case so the test can actually be run to allow checking of remaining flow
    if ((Platform::GetSimulationMode() == Platform::Fmodel) ||
        (Platform::GetSimulationMode() == Platform::Amodel))
    {
        fill(status->begin(), status->end(), 1);
        return OK;
    }

    MASSERT(status);
    MASSERT(status->size());
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    const UINT32 numLanes = static_cast<UINT32>(status->size());
    const UINT32 rxLaneMask = GetRegLanesRx();
    MASSERT(rxLaneMask);
    const UINT32 firstLane = (rxLaneMask) ? static_cast<UINT32>(Utility::BitScanForward(rxLaneMask)) : 0;
    const UINT32 laneMask = regs.LookupFieldValueMask(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT);

    UINT32 modeVal = 0x0;
    switch (mode)
    {
        case Pci::EYEO_X:
            modeVal = LW_EOM_CTRL2_MODE_X;
            break;
        case Pci::EYEO_Y:
            modeVal = LW_EOM_CTRL2_MODE_Y;
            break;
        default:
            Printf(Tee::PriError, "Invalid Mode = %u\n", mode);
            return RC::ILWALID_ARGUMENT;
    }

    regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    UINT32 ctl8 = regs.Read32(MODS_XP_PEX_PAD_CTL_8);
    regs.SetField(&ctl8, MODS_XP_PEX_PAD_CTL_8_CFG_WDATA, DRF_NUM(_EOM, _CTRL2, _MODE, modeVal) |
                                                          DRF_NUM(_EOM, _CTRL2, _NBLKS, 0xd) |
                                                          DRF_NUM(_EOM, _CTRL2, _NERRS, 0x7));
    regs.SetField(&ctl8, MODS_XP_PEX_PAD_CTL_8_CFG_ADDR, LW_EOM_CTRL2);
    regs.SetField(&ctl8, MODS_XP_PEX_PAD_CTL_8_CFG_WDS, 0x1);
    regs.Write32(MODS_XP_PEX_PAD_CTL_8, ctl8);

    regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    regs.Write32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_EN_DISABLE);

    UINT32 doneStatus = 0x0;
    auto pollEomDone = [&]()->bool
        {
            return regs.Test32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_DONE, doneStatus);
        };

    for (UINT32 i = 0; i < numLanes; i++)
    {
        regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (laneMask << (firstLane + i)) & laneMask);
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
        regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (laneMask << (firstLane + i)) & laneMask);
        CHECK_RC(Tasker::PollHw(timeoutMs, pollEomDone, __FUNCTION__));
    }

    regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    regs.Write32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_EN_DISABLE);
    regs.Write32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_OVRD_DISABLE);

    for (UINT32 i = 0; i < numLanes; i++)
    {
        regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (laneMask << (firstLane + i)) & laneMask);
        status->at(i) = regs.Read32(MODS_XP_PEX_PAD_CTL_5_RX_EOM_STATUS);
    }

    return OK;
}
