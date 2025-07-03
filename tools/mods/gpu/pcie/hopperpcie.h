/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "ga10xpcie.h"

class HopperPcie : public GA10xPcie
{
public:
    HopperPcie() = default;
    virtual ~HopperPcie() = default;

protected:
    bool   DoSupportsFomMode(Pci::FomMode mode) override;
    RC     DoGetAspmCyaL1SubState(UINT32 *pState) override;
    RC     DoGetAspmCyaL1SubStateProtected(UINT32 *pState) const override;
    RC     DoGetAspmCyaStateProtected(UINT32 *pState) const override;
    RC     DoGetAspmEntryCounts(Pcie::AspmEntryCounters *pCounts) override;
    RC     DoGetAspmL1SSCapability(UINT32 *pCap) override;
    UINT32 DoGetAspmState() override;
    RC     DoGetL1ClockPmCyaSubstate(UINT32 *pState) const override;
    RC     DoGetL1ClockPmCyaSubstateProtected(UINT32 *pState) const override;
    RC     DoGetLTREnabled(bool *pEnabled) override;
    UINT32 GetRegLanes(bool isTx) const override;
    RC     DoGetPcieDetectedLanes(UINT32 *pLanes) override;
    bool   DoAerEnabled() const override;
    void   DoEnableAer(bool flag) override;
    UINT16 DoGetAerOffset() const override;
    UINT32 DoGetL1SubstateOffset() override;
    bool DoHasL1ClockPmSubstates() const override
        { return true; }
    RC     DoSetResetCrsTimeout(UINT32 timeUs) override;
    RC     DoGetScpmStatus(UINT32 * pStatus) const override;
    string DoDecodeScpmStatus(UINT32 status) const override;
    RC     DoResetAspmEntryCounters() override;
    RC     DoSetAspmCyaState(UINT32 state) override;
    RC     DoSetAspmCyaL1SubState(UINT32 state) override;
    RC     DoSetL1ClockPmCyaSubstate(UINT32 state) override;
    // PcieUphy
    Version DoGetVersion() const override { return UphyIf::Version::V61; }
    RC     DoReadPadLaneReg(UINT32 linkId, UINT32 lane,
                            UINT16 addr, UINT16* pData) override;
    RC     DoWritePadLaneReg(UINT32 linkId, UINT32 lane,
                             UINT16 addr, UINT16 data) override;
    RC     DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16* pData) override;

    RC GetEomSettings(Pci::FomMode mode, EomSettings* pSettings) override;
};
