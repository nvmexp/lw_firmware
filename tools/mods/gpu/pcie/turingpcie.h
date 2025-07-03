/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "voltapcie.h"
#include "pciemargin.h"

class TuringPcie : public VoltaPcie
                 , public PcieMargin
{
public:
    TuringPcie() = default;
    virtual ~TuringPcie() {}

protected:
    RC DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs) override;

private:
    // PcieMargin Functions
    RC DoClearMarginErrorLog(UINT32 lane) override;
    RC DoDisableMargining() override;
    RC DoEnableMargining() override;
    RC DoGetMarginCtrlCaps(UINT32 lane, CtrlCaps* pCaps) override;
    RC DoGetMarginMaxTimingOffset(UINT32 lane, UINT32* pOffset) override;
    RC DoGetMarginMaxVoltageOffset(UINT32 lane, UINT32* pOffset) override;
    RC DoGetMarginNumTimingSteps(UINT32 lane, UINT32* pSteps) override;
    RC DoGetMarginNumVoltageSteps(UINT32 lane, UINT32* pSteps) override;
    RC DoSetMarginErrorCountLimit(UINT32 lane, UINT32 limit) override;
    RC DoSetMarginNormalSettings(UINT32 lane) override;
    RC DoSetMarginTimingOffset(UINT32 lane, TimingMarginDir dir, UINT32 offset, FLOAT64 timeoutMs, bool* pVerified) override;
    RC DoSetMargilwoltageOffset(UINT32 lane, VoltageMarginDir dir, UINT32 offset, FLOAT64 timeoutMs, bool* pVerified) override;

    // PcieUphy
    Version DoGetVersion() const override { return UphyIf::Version::V31; }

private:
    RC RunMarginCommand(UINT32 lane, UINT32 control, UINT32* pReturn);
    RC RunMarginNoCmd(UINT32 lane);
    RC VerifyMarginOffset(UINT32 lane, FLOAT64 marginTimeMs, bool* pVerified);
};
