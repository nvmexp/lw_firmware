/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012,2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file socgpupcie.h -- Concrete implemetation classes for SocGpuPcie

#pragma once

#ifndef INCLUDED_SOCPCIE_H
#define INCLUDED_SOCPCIE_H

#include "fermipcie.h"

//--------------------------------------------------------------------
//! \brief PCIE implementation used for SOC GPUs.  Maintains much of the
//!        functionality from Fermi GPUs, however other functionality is
//!        stubbed since SoC devices are not really PCIE devices
//!
class SocPcie : public FermiPcie
{
public:
    SocPcie() = default;
    virtual ~SocPcie() {}

protected:
    UINT32 DoAdNormal() override;
    bool DoAerEnabled() const override;
    UINT32 DoAspmCapability() override;
    void DoEnableAer(bool flag) override;
    RC DoGetErrorCounts
    (
        PexErrorCounts            *pLocCounts,
        PexErrorCounts            *pHostCounts,
        PexDevice::PexCounterType  CountType
    ) override;
    RC DoResetErrorCounters() override;
    RC DoSetAspmState(UINT32 State) override;
    UINT32 DoSubsystemDeviceId() override;
    UINT32 DoSubsystemVendorId() override;

protected:
    RC DoGetErrorCounters(PexErrorCounts *pCounts) override;
};

#endif

