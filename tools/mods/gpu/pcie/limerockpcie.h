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

#pragma once

#include "lwswitchpcie.h"

class LimerockPcie : public LwSwitchPcie
{
public:
    LimerockPcie() = default;
    virtual ~LimerockPcie() = default;

protected:
    bool DoSupportsFomMode(Pci::FomMode mode) override;
    RC DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs) override;
    RC DoSetLinkSpeed(Pci::PcieLinkSpeed newSpeed) override;

    // PcieUphy
    Version DoGetVersion() const override { return UphyIf::Version::V50; }
    bool DoIsUphySupported() const override { return true; }
    RC DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData) override;

    virtual RC GetEomSettings(Pci::FomMode mode, EomSettings* pSettings);
};
