/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_VOLTAPCIE_H
#define INCLUDED_VOLTAPCIE_H

#include "pascalpcie.h"

class VoltaPcie : public PascalPcie
{
public:
    VoltaPcie() = default;
    virtual ~VoltaPcie() {}

protected:
    RC DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs) override;

    // PcieUphy
    Version DoGetVersion() const override { return UphyIf::Version::V30; }
    bool DoIsUphySupported() const override { return true; }
};

#endif
