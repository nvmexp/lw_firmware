/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012,2014-2018,2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file gpupcieimpl.h -- Concrete implemetation classes for GpuPcie
// Gpu subdevice PCIE implemetation for various GPU types

#pragma once

#ifndef INCLUDED_FERMIPCIE_H
#define INCLUDED_FERMIPCIE_H

#include "gpu/pcie/pcieimpl.h"

//--------------------------------------------------------------------
//! \brief GPU PCIE implementation used for G80+
//!
class FermiPcie : public PcieImpl
{
public:
    FermiPcie();
    virtual ~FermiPcie() {}

protected:
    bool DoAerEnabled() const override;
    UINT32 DoAspmCapability() override;
    UINT32 DoDeviceId() const override;
    bool DoDidLinkEnterRecovery() override;
    void DoEnableAer(bool flag) override;
    RC DoEnableAspmDeepL1(bool bEnable) override;
    UINT16 DoGetAerOffset() const override;
    RC DoGetAspmCyaL1SubState(UINT32 *pState) override;
    UINT32 DoGetAspmCyaState() override;
    RC DoGetAspmDeepL1Enabled(bool *pbEnabled) override;
    RC DoGetAspmEntryCounts(AspmEntryCounters *pCounts) override;
    RC DoGetAspmL1SSCapability(UINT32 *pCap) override;
    RC DoGetAspmL1ssEnabled(UINT32 *pState) override;
    UINT32 DoGetAspmState() override;
    RC DoGetErrorCounts
    (
        PexErrorCounts            *pLocCounts,
        PexErrorCounts            *pHostCounts,
        PexDevice::PexCounterType  countType
    ) override;
    bool DoGetForceCountersToZero() const override
        { return m_ForceCountersToZero; }
    UINT32 DoGetL1SubstateOffset() override
        { return 0; }
    Pci::PcieLinkSpeed DoGetLinkSpeed(Pci::PcieLinkSpeed expectedSpeed) override;
    UINT32 DoGetLinkWidth() override;
    RC DoGetLTREnabled(bool *pEnabled) override;
    RC DoGetPolledErrors
    (
        Pci::PcieErrorsMask* pGpuErrors,
        Pci::PcieErrorsMask *pHostErrors
    ) override;
    Pci::PcieLinkSpeed DoGetUpstreamLinkSpeedCap() override;
    UINT32 DoGetUpstreamLinkWidthCap() override;
    RC DoHasAspmDeepL1(bool *pbHasDeepL1) override;
    bool DoHasAspmL1Substates() override;
    bool DoIsASLMCapable() override;
    Pci::PcieLinkSpeed DoLinkSpeedCapability() const override;
    UINT32 DoLinkWidthCapability() override;
    RC DoResetAspmEntryCounters() override;
    RC DoResetErrorCounters() override;
    RC DoSetAspmState(UINT32 State) override;
    void DoSetForceCountersToZero(bool forceZero) override;
    RC DoSetLinkSpeed(Pci::PcieLinkSpeed newSpeed) override;
    RC DoSetLinkWidth(UINT32 linkWidth) override;
    void DoSetUsePolledErrors(bool bUsePoll) override;
    RC DoShutdown() override;
    UINT32 DoSubsystemDeviceId() override;
    UINT32 DoSubsystemVendorId() override;
    bool DoUsePolledErrors() const override
        { return m_UsePolledErrors; }

protected:
    GpuSubdevice *GetSubdevice();
    const GpuSubdevice *GetSubdevice() const;
    virtual RC DoGetErrorCounters(PexErrorCounts *pCounts);

private:
    RC GetGpuPolledErrorCounts(PexErrorCounts * pCounts);
    RC LoadPcieInfo();

    bool   m_UsePolledErrors;     //!< true to poll PCIE status flags for the
                                  //!< GPU rather than using hardware counters
    bool   m_PciInfoLoaded;       //!< true if PCI info has been loaded from RM
    bool   m_ForceCountersToZero;

    //! PCI information about the GPU retrieved from RM
    LW2080_CTRL_BUS_GET_PCI_INFO_PARAMS m_PciInfo;

    bool m_bRestorePexAer;
    bool m_bPexAerEnabled;
};

#endif

