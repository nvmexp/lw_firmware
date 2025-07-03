/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "turingpcie.h"
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
// Turing update at:
// https://lwbugswb.lwpu.com/LwBugs5/SWBug.aspx?bugid=2175925&cmtNo=10
#define LW_EOM_CTRL1              0xAB
#define LW_EOM_CTRL1_TYPE         1:0
#define LW_EOM_CTRL1_TYPE_ODD     0x1
#define LW_EOM_CTRL1_TYPE_EVEN    0x2
#define LW_EOM_CTRL1_TYPE_ODDEVEN 0x3
#define LW_EOM_CTRL2              0xB3
#define LW_EOM_CTRL2_NERRS        3:0
#define LW_EOM_CTRL2_NBLKS        7:4
#define LW_EOM_CTRL2_MODE         11:8
#define LW_EOM_CTRL2_MODE_X       0x5
#define LW_EOM_CTRL2_MODE_XL      0x0
#define LW_EOM_CTRL2_MODE_XH      0x1
#define LW_EOM_CTRL2_MODE_Y       0xb
#define LW_EOM_CTRL2_MODE_YL      0x6
#define LW_EOM_CTRL2_MODE_YH      0x7

// Definitions come from PCIE Spec 4.0
#define MARGIN_RCVR_BROADCAST 0x0
#define MARGIN_RCVR_RXA       0x1 // Downstream Port Receiver
#define MARGIN_RCVR_RXB       0x2
#define MARGIN_RCVR_RXC       0x3
#define MARGIN_RCVR_RXD       0x4
#define MARGIN_RCVR_RXE       0x5
#define MARGIN_RCVR_RXF       0x6 // Upstream Port Receiver

#define MARGIN_TYPE_REPORT       0x1
#define MARGIN_TYPE_SET          0x2
#define MARGIN_TYPE_STEP_TIMING  0x3
#define MARGIN_TYPE_STEP_VOLTAGE 0x4
#define MARGIN_TYPE_NOCMD        0x7

#define MARGIN_PAYLOAD_NOCMD              0x9C
#define MARGIN_PAYLOAD_CONTROL_CAPS       0x88
#define MARGIN_PAYLOAD_NUM_VOLTAGE_STEPS  0x89
#define MARGIN_PAYLOAD_NUM_TIMING_STEPS   0x8A
#define MARGIN_PAYLOAD_MAX_TIMING_OFFSET  0x8B
#define MARGIN_PAYLOAD_MAX_VOLTAGE_OFFSET 0x8C
#define MARGIN_PAYLOAD_MAX_LANES          0x90
#define MARGIN_PAYLOAD_SET_NORMAL         0x0F
#define MARGIN_PAYLOAD_CLEAR_ERR_LOG      0x55

//-----------------------------------------------------------------------------
RC TuringPcie::DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs)
{
    MASSERT(status);
    MASSERT(status->size());
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    const UINT32 numLanes = static_cast<UINT32>(status->size());
    const UINT32 rxLaneMask = GetRegLanesRx();
    MASSERT(rxLaneMask);
    const UINT32 firstLane = (rxLaneMask) ? static_cast<UINT32>(Utility::BitScanForward(rxLaneMask)) : 0;

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
                                                          DRF_NUM(_EOM, _CTRL2, _NBLKS, 0xa) |
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

    return OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::DoClearMarginErrorLog(UINT32 lane)
{
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    UINT32 retVal = 0;
    UINT32 ctrl = regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_RECEIVER_NUMBER, MARGIN_RCVR_BROADCAST) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_TYPE, MARGIN_TYPE_SET) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_PAYLOAD, MARGIN_PAYLOAD_CLEAR_ERR_LOG);
    CHECK_RC(RunMarginNoCmd(lane));
    CHECK_RC(RunMarginCommand(lane, ctrl, &retVal));
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::DoDisableMargining()
{
    RegHal& regs = GetSubdevice()->Regs();
    regs.Write32(MODS_PPWR_PMU_GPIO_2_INTR_HIGH_EN_XVE_MARGINING_INTR_DISABLED);
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::DoEnableMargining()
{
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();

    regs.Write32(MODS_PPWR_PMU_GPIO_2_INTR_HIGH_EN_XVE_MARGINING_INTR_ENABLED);

    UINT32 marginPortCapsStatus = regs.Read32(MODS_EP_PCFG_GPU_LANE_MARGINING_CAP_STATUS_REGISTER);

    if (!regs.TestField(marginPortCapsStatus, MODS_EP_PCFG_GPU_LANE_MARGINING_CAP_STATUS_REGISTER_MARGINING_READY, 1) ||
        (regs.TestField(marginPortCapsStatus, MODS_EP_PCFG_GPU_LANE_MARGINING_CAP_STATUS_REGISTER_MARGINING_USES_DRIVER_SOFTWARE, 1) &&
         !regs.TestField(marginPortCapsStatus, MODS_EP_PCFG_GPU_LANE_MARGINING_CAP_STATUS_REGISTER_MARGINING_SOFTWARE_READY, 1)))
    {
        Printf(Tee::PriError, "Margining not ready!\n");
        return RC::HW_STATUS_ERROR;
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::DoGetMarginCtrlCaps(UINT32 lane, CtrlCaps* pCaps)
{
    MASSERT(pCaps);
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    UINT32 retVal = 0;
    UINT32 ctrl = regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_RECEIVER_NUMBER, MARGIN_RCVR_RXF) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_TYPE, MARGIN_TYPE_REPORT) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_PAYLOAD, MARGIN_PAYLOAD_CONTROL_CAPS);
    CHECK_RC(RunMarginNoCmd(lane));
    CHECK_RC(RunMarginCommand(lane, ctrl, &retVal));

#define LW_MARGIN_PAYLOAD_IND_ERROR_SAMPLER     4:4
#define LW_MARGIN_PAYLOAD_IND_LEFT_RIGHT_TIMING 2:2
#define LW_MARGIN_PAYLOAD_IND_UP_DOWN_VOLTAGE   1:1
#define LW_MARGIN_PAYLOAD_VOLTAGE_SUPPORTED     0:0
    pCaps->bIndErrorSampler = DRF_VAL(_MARGIN, _PAYLOAD, _IND_ERROR_SAMPLER, retVal);
    pCaps->bIndLeftRightTiming = DRF_VAL(_MARGIN, _PAYLOAD, _IND_LEFT_RIGHT_TIMING, retVal);
    pCaps->bIndUpDowlwoltage = DRF_VAL(_MARGIN, _PAYLOAD, _IND_UP_DOWN_VOLTAGE, retVal);
    pCaps->bVoltageSupported = DRF_VAL(_MARGIN, _PAYLOAD, _VOLTAGE_SUPPORTED, retVal);
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::DoGetMarginMaxTimingOffset(UINT32 lane, UINT32* pOffset)
{
    MASSERT(pOffset);
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    UINT32 retVal = 0;
    UINT32 ctrl = regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_RECEIVER_NUMBER, MARGIN_RCVR_RXF) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_TYPE, MARGIN_TYPE_REPORT) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_PAYLOAD, MARGIN_PAYLOAD_MAX_TIMING_OFFSET);
    CHECK_RC(RunMarginNoCmd(lane));
    CHECK_RC(RunMarginCommand(lane, ctrl, &retVal));

#define LW_MARGIN_PAYLOAD_MAX_TIMING_OFFSET 6:0
    *pOffset = DRF_VAL(_MARGIN, _PAYLOAD, _MAX_TIMING_OFFSET, retVal);;
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::DoGetMarginMaxVoltageOffset(UINT32 lane, UINT32* pOffset)
{
    MASSERT(pOffset);
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    UINT32 retVal = 0;
    UINT32 ctrl = regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_RECEIVER_NUMBER, MARGIN_RCVR_RXF) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_TYPE, MARGIN_TYPE_REPORT) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_PAYLOAD, MARGIN_PAYLOAD_MAX_VOLTAGE_OFFSET);
    CHECK_RC(RunMarginNoCmd(lane));
    CHECK_RC(RunMarginCommand(lane, ctrl, &retVal));

#define LW_MARGIN_PAYLOAD_MAX_VOLT_OFFSET 6:0
    *pOffset = DRF_VAL(_MARGIN, _PAYLOAD, _MAX_VOLT_OFFSET, retVal);;
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::DoGetMarginNumTimingSteps(UINT32 lane, UINT32* pSteps)
{
    MASSERT(pSteps);
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    UINT32 retVal = 0;
    UINT32 ctrl = regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_RECEIVER_NUMBER, MARGIN_RCVR_RXF) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_TYPE, MARGIN_TYPE_REPORT) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_PAYLOAD, MARGIN_PAYLOAD_NUM_TIMING_STEPS);
    CHECK_RC(RunMarginNoCmd(lane));
    CHECK_RC(RunMarginCommand(lane, ctrl, &retVal));

#define LW_MARGIN_PAYLOAD_NUM_TIMING_STEPS 5:0
    *pSteps = DRF_VAL(_MARGIN, _PAYLOAD, _NUM_TIMING_STEPS, retVal);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::DoGetMarginNumVoltageSteps(UINT32 lane, UINT32* pSteps)
{
    MASSERT(pSteps);
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    UINT32 retVal = 0;
    UINT32 ctrl = regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_RECEIVER_NUMBER, MARGIN_RCVR_RXF) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_TYPE, MARGIN_TYPE_REPORT) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_PAYLOAD, MARGIN_PAYLOAD_NUM_VOLTAGE_STEPS);
    CHECK_RC(RunMarginNoCmd(lane));
    CHECK_RC(RunMarginCommand(lane, ctrl, &retVal));

#define LW_MARGIN_PAYLOAD_NUM_VOLT_STEPS 6:0
    *pSteps = DRF_VAL(_MARGIN, _PAYLOAD, _NUM_VOLT_STEPS, retVal);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::DoSetMarginErrorCountLimit(UINT32 lane, UINT32 limit)
{
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    UINT32 retVal = 0;
#define LW_MARGIN_PAYLOAD_ERROR_UPPER       7:6
#define LW_MARGIN_PAYLOAD_ERROR_COUNT_LIMIT 5:0
    UINT32 payload = DRF_NUM(_MARGIN, _PAYLOAD, _ERROR_UPPER, 0x3) | // Hard-coded value from spec
                     DRF_NUM(_MARGIN, _PAYLOAD, _ERROR_COUNT_LIMIT, limit);
    UINT32 ctrl = regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_RECEIVER_NUMBER, MARGIN_RCVR_RXF) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_TYPE, MARGIN_TYPE_SET) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_PAYLOAD, payload);
    CHECK_RC(RunMarginNoCmd(lane));
    CHECK_RC(RunMarginCommand(lane, ctrl, &retVal));

    UINT32 statusLimit = DRF_VAL(_MARGIN, _PAYLOAD, _ERROR_COUNT_LIMIT, retVal);
    if (limit != statusLimit)
    {
        Printf(Tee::PriError, "Cannot set Error Count Limit = %d\n", limit);
        return RC::HW_STATUS_ERROR;
    }
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::DoSetMarginNormalSettings(UINT32 lane)
{
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    UINT32 retVal = 0;
    UINT32 ctrl = regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_RECEIVER_NUMBER, MARGIN_RCVR_BROADCAST) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_TYPE, MARGIN_TYPE_SET) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_PAYLOAD, MARGIN_PAYLOAD_SET_NORMAL);
    CHECK_RC(RunMarginNoCmd(lane));
    CHECK_RC(RunMarginCommand(lane, ctrl, &retVal));
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::DoSetMarginTimingOffset
(
    UINT32 lane,
    TimingMarginDir dir,
    UINT32 offset,
    FLOAT64 timeoutMs,
    bool* pVerified
)
{
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    UINT32 retVal = 0;
#define LW_MARGIN_PAYLOAD_DIRECTION_TIMING 6:6
#define LW_MARGIN_PAYLOAD_OFFSET           5:0
    UINT32 payload = ((dir != TMD_UNIDIR) ? DRF_NUM(_MARGIN, _PAYLOAD, _DIRECTION_TIMING, dir) : 0x0) |
                     DRF_NUM(_MARGIN, _PAYLOAD, _OFFSET, offset);
    UINT32 ctrl = regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_RECEIVER_NUMBER, MARGIN_RCVR_RXF) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_TYPE, MARGIN_TYPE_STEP_TIMING) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_PAYLOAD, payload);
    CHECK_RC(RunMarginNoCmd(lane));
    CHECK_RC(RunMarginCommand(lane, ctrl, &retVal));

    CHECK_RC(VerifyMarginOffset(lane, timeoutMs, pVerified));

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::DoSetMargilwoltageOffset
(
    UINT32 lane,
    VoltageMarginDir dir,
    UINT32 offset,
    FLOAT64 timeoutMs,
    bool* pVerified
)
{
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    UINT32 retVal = 0;
#define LW_MARGIN_PAYLOAD_DIRECTION_VOLTAGE 7:7
#define LW_MARGIN_PAYLOAD_OFFSET            5:0
    UINT32 payload = ((dir != VMD_UNIDIR) ? DRF_NUM(_MARGIN, _PAYLOAD, _DIRECTION_VOLTAGE, dir) : 0x0) |
                     DRF_NUM(_MARGIN, _PAYLOAD, _OFFSET, offset);
    UINT32 ctrl = regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_RECEIVER_NUMBER, MARGIN_RCVR_RXF) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_TYPE, MARGIN_TYPE_STEP_VOLTAGE) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_PAYLOAD, payload);
    CHECK_RC(RunMarginNoCmd(lane));
    CHECK_RC(RunMarginCommand(lane, ctrl, &retVal));

    CHECK_RC(VerifyMarginOffset(lane, timeoutMs, pVerified));

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::RunMarginCommand(UINT32 lane, UINT32 control, UINT32* pReturn)
{
    MASSERT(pReturn);
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();

    static const FLOAT64 marginCmdTimeoutMs = 10.0; // Defined in the spec
    UINT32 retries = 50;

    UINT32 status = 0;
    do
    {
        regs.Write32(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER, lane, control);
        rc = Tasker::PollHw(marginCmdTimeoutMs, [&]() -> bool
        {
            status = regs.Read32(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER, lane);
            return (regs.GetField(status, MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_RECEIVER_NUMBER) == regs.GetField(status, MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_RECEIVER_NUMBER_STATUS) &&
                    regs.GetField(status, MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_TYPE) == regs.GetField(status, MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_TYPE_STATUS));
        }, __FUNCTION__);

        if (rc == OK)
            break;

        if ((rc == RC::TIMEOUT_ERROR) && (retries > 1))
        {
            rc.Clear();
            Printf(Tee::PriLow,
                   "Retrying lane %u control 0x%x, retries remaining %u (%d)\n",
                   lane, control, retries, GetLinkSpeed(Pci::SpeedUnknown));

            Tasker::Sleep(50);
        }
        if (rc != RC::OK)
        {
            Printf(Tee::PriError,
                   "PCIE margining command failed on lane %u: control 0x%x, margining lane value "
                   "0x%x\n",
                   lane, control, regs.Read32(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER, lane));
            return rc;
        }
        retries--;
    } while (retries > 0);

    *pReturn = regs.GetField(status, MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_PAYLOAD_STATUS);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TuringPcie::RunMarginNoCmd(UINT32 lane)
{
    RegHal& regs = GetSubdevice()->Regs();
    UINT32 retVal = 0;
    UINT32 ctrl = regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_RECEIVER_NUMBER, MARGIN_RCVR_BROADCAST) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_TYPE, MARGIN_TYPE_NOCMD) |
                  regs.SetField(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_PAYLOAD, MARGIN_PAYLOAD_NOCMD);
    return RunMarginCommand(lane, ctrl, &retVal);
}

//-----------------------------------------------------------------------------
RC TuringPcie::VerifyMarginOffset(UINT32 lane, FLOAT64 marginTimeMs, bool* pVerified)
{
    MASSERT(pVerified);
    RC rc;
    RegHal& regs = GetSubdevice()->Regs();
    *pVerified = false;
#define LW_MARGIN_PAYLOAD_EXE_STATUS             7:6
#define LW_MARGIN_PAYLOAD_EXE_STATUS_ERROR_LIMIT 0x0
#define LW_MARGIN_PAYLOAD_EXE_STATUS_SETUP       0x1
#define LW_MARGIN_PAYLOAD_EXE_STATUS_MARGINING   0x2
#define LW_MARGIN_PAYLOAD_EXE_STATUS_NAK         0x3
#define LW_MARGIN_PAYLOAD_ERROR_COUNT            5:0
    StickyRC marginRC;
    bool setup = true;
    auto MarginingDone = [&]() -> bool
    {
        UINT32 payload = regs.Read32(MODS_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER_MARGIN_PAYLOAD_STATUS, lane);

        if (FLD_TEST_DRF(_MARGIN, _PAYLOAD, _EXE_STATUS, _NAK, payload)
            || FLD_TEST_DRF(_MARGIN, _PAYLOAD, _EXE_STATUS, _ERROR_LIMIT, payload))
        {
            *pVerified = false;
            return true;
        }
        else if (setup && FLD_TEST_DRF(_MARGIN, _PAYLOAD, _EXE_STATUS, _MARGINING, payload))
        {
            // Setup is done
            return true;
        }
        return false;
    };

    static const FLOAT64 setupTimeoutMs = 200.0; // Defined in the spec
    CHECK_RC(Tasker::PollHw(setupTimeoutMs, MarginingDone, __FUNCTION__));
    CHECK_RC(marginRC);

    setup = false;
    *pVerified = true;
    rc = Tasker::PollHw(marginTimeMs, MarginingDone, __FUNCTION__);
    if (rc == RC::TIMEOUT_ERROR)
    {
        // Timeout actually means it reached the end of the specified time without failing.
        rc.Clear();
    }
    CHECK_RC(rc);
    CHECK_RC(marginRC);

    CHECK_RC(RunMarginNoCmd(lane));

    return RC::OK;
}
