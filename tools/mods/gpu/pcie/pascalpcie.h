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

#pragma once

#include "device/interface/pcie/pcieuphy.h"
#include "maxwellpcie.h"

class PascalPcie
  : public MaxwellPcie
  , public PcieUphy
{
public:
    PascalPcie() = default;
    virtual ~PascalPcie() {}

protected:
    bool DoSupportsFom() const override { return true; }
    bool DoSupportsFomMode(Pci::FomMode mode) override;
    RC DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs) override;

    // PcieUphy
    UINT32 DoGetActiveLaneMask(RegBlock regBlock) const override;
    UINT32 DoGetMaxRegBlocks(RegBlock regBlock) const override { return 1; }
    UINT32 DoGetMaxLanes(RegBlock regBlock) const override;
    bool DoGetRegLogEnabled() const override { return m_bUphyRegLogEnabled; }
    UINT64 DoGetRegLogRegBlockMask(RegBlock regBlock) override { return 1ULL; }
    Version DoGetVersion() const override { return UphyIf::Version::UNKNOWN; }
    RC DoIsRegBlockActive(RegBlock regBlock, UINT32 blockIdx, bool *pbActive) override
        { MASSERT(pbActive); *pbActive = true; return RC::OK; }
    bool DoIsUphySupported() const override { return false; }
    RC DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData) override;
    RC DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data) override;
    RC DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData) override;
    RC DoSetRegLogLinkMask(UINT64 linkMask) override { return RC::OK; }
    void DoSetRegLogEnabled(bool bEnabled) override { m_bUphyRegLogEnabled = bEnabled; }

private:
    bool m_bUphyRegLogEnabled = false;
};

