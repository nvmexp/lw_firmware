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

#include "amperepcie.h"

class GA10xPcie : public AmperePcie
{
public:
    GA10xPcie() = default;
    virtual ~GA10xPcie() = default;
    RC DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs) override;
protected:
    // PcieUphy
    Version DoGetVersion() const override { return UphyIf::Version::V32; }
    RC GetEomSettings(Pci::FomMode mode, EomSettings* pSettings) override;
};

