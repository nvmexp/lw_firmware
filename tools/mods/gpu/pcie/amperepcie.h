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

#include "turingpcie.h"

class AmperePcie : public TuringPcie
{
public:
    AmperePcie() = default;
    virtual ~AmperePcie() = default;

protected:
    bool DoSupportsFomMode(Pci::FomMode mode) override;
    RC DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs) override;
    RC DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData) override;

    // PcieUphy
    Version DoGetVersion() const override { return UphyIf::Version::V50; }

    virtual RC GetEomSettings(Pci::FomMode mode, EomSettings* pSettings);
};
