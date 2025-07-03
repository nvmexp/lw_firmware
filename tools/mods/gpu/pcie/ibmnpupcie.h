/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file ibmnpupcie.h -- Concrete implemetation class for IBM NPU devices

#pragma once

#ifndef INCLUDED_IBMNPUPCIE_H
#define INCLUDED_IBMNPUPCIE_H

#include "gpu/pcie/pcieimpl.h"

//--------------------------------------------------------------------
//! \brief PCIE implementation used for IbmNpu devices
//!
class IbmNpuPcie : public PcieImpl
{
public:
    IbmNpuPcie();
    virtual ~IbmNpuPcie() {}

protected:
    bool DoAerEnabled() const override
        { return false; }
    UINT32 DoAspmCapability() override
        { return 0; }
    bool DoDidLinkEnterRecovery() override
        { return false; }
    void DoEnableAer(bool flag) override
        { return; }
    UINT16 DoGetAerOffset() const override
        { return _UINT16_MAX; }
    RC DoGetAspmCyaL1SubState(UINT32 *pState) override
        { return RC::UNSUPPORTED_FUNCTION; }
    UINT32 DoGetAspmCyaState() override
        { return _UINT32_MAX; }
    RC DoGetAspmEntryCounts(AspmEntryCounters *pCounts) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoGetAspmL1SSCapability(UINT32 *pCap) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoGetErrorCounts
    (
        PexErrorCounts            *pLocCounts,
        PexErrorCounts            *pHostCounts,
        PexDevice::PexCounterType  countType
    ) override;
    bool DoGetForceCountersToZero() const override
        { return false; }
    UINT32 DoGetL1SubstateOffset() override
        { return _UINT32_MAX; }
    RC DoGetPolledErrors
    (
        Pci::PcieErrorsMask* pGpuErrors,
        Pci::PcieErrorsMask *pHostErrors
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }
    bool DoHasAspmL1Substates() override
        { return false; }
    bool DoIsASLMCapable() override
        { return false; }
    RC DoResetAspmEntryCounters() override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoResetErrorCounters() override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetAspmState(UINT32 state) override
        { return RC::UNSUPPORTED_FUNCTION; }
    void DoSetForceCountersToZero(bool forceZero) override
        { return; }
    RC DoSetLinkSpeed(Pci::PcieLinkSpeed newSpeed) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetLinkWidth(UINT32 linkWidth) override
        { return RC::UNSUPPORTED_FUNCTION; }
    void DoSetUsePolledErrors(bool bUsePoll) override
        { return; }
    RC DoShutdown() override
        { return OK; }
    bool DoUsePolledErrors() const override
        { return false; }
};

#endif

