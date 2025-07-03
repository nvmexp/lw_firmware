/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "limerockpcie.h"

class LagunaPcie : public LimerockPcie
{
public:
    LagunaPcie() = default;
    virtual ~LagunaPcie() = default;

protected:
    RC DoGetAspmCyaL1SubState(UINT32 *pAspmCya) override;
    RC DoGetAspmCyaL1SubStateProtected(UINT32 *pState) const override;
    UINT32 DoGetAspmCyaState() override;
    RC DoGetAspmCyaStateProtected(UINT32 *pState) const override;
    RC DoGetAspmEntryCounts(AspmEntryCounters *pCounts) override;
    RC DoGetL1ClockPmCyaSubstate(UINT32 *pState) const override;
    RC DoGetL1ClockPmCyaSubstateProtected(UINT32 *pState) const override;
    bool DoHasL1ClockPmSubstates() const override
        { return true; }
    RC DoResetAspmEntryCounters() override;
    RC DoSetAspmCyaState(UINT32 state) override;
    RC DoSetAspmCyaL1SubState(UINT32 state) override;
    RC DoSetL1ClockPmCyaSubstate(UINT32 state) override;
    bool DoSupportsFomMode(Pci::FomMode mode) override;
    UINT32 GetRegLanes(bool isTx) const override;

    // PcieUphy
    Version DoGetVersion() const override { return UphyIf::Version::V61; }
    RC DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData) override;
    RC DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data) override;
    RC DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData) override;

    RC GetEomSettings(Pci::FomMode mode, EomSettings* pSettings) override;
};
