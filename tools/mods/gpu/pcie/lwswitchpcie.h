/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file lwswitchpcie.h -- Concrete implemetation class for LwSwitch devices

#pragma once

#include "core/include/device.h"
#include "device/interface/pcie/pcieuphy.h"
#include "gpu/pcie/pcieimpl.h"

class LwLinkDev;

//--------------------------------------------------------------------
//! \brief PCIE implementation used for LwSwitch devices
//!
class LwSwitchPcie
  : public PcieImpl
  , public PcieUphy
{
public:
    LwSwitchPcie() = default;
    virtual ~LwSwitchPcie() {}

protected:
    bool DoAerEnabled() const override;
    UINT32 DoAspmCapability() override;
    bool DoDidLinkEnterRecovery() override
        { return false; }
    void DoEnableAer(bool flag) override;
    UINT16 DoGetAerOffset() const override
        { return _UINT16_MAX; }
    RC DoGetAspmCyaL1SubState(UINT32 *pState) override;
    UINT32 DoGetAspmCyaState() override;
    RC DoGetAspmDeepL1Enabled(bool *pbEnabled) override;
    RC DoGetAspmEntryCounts(AspmEntryCounters *pCounts) override;
    RC DoGetAspmL1SSCapability(UINT32 *pCap) override;
    bool DoSupportsFom() const override { return true; }
    bool DoSupportsFomMode(Pci::FomMode mode) override;
    RC DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs) override;
    RC DoGetErrorCounts
    (
        PexErrorCounts            *pLocCounts,
        PexErrorCounts            *pHostCounts,
        PexDevice::PexCounterType  countType
    ) override;
    bool DoGetForceCountersToZero() const override
        { return false; }
    UINT32 DoGetL1SubstateOffset() override;
    Pci::PcieLinkSpeed DoGetLinkSpeed(Pci::PcieLinkSpeed expectedSpeed) override;
    RC DoGetPolledErrors
    (
        Pci::PcieErrorsMask* pGpuErrors,
        Pci::PcieErrorsMask *pHostErrors
    ) override;
    bool DoHasAspmL1Substates() override
        { return true; }
    RC DoHasAspmDeepL1(bool *pbHasDeepL1);
    RC DoInitialize
    (
        Device::LibDeviceHandle libHandle
    ) override;
    bool DoIsASLMCapable() override
        { return false; }
    RC DoResetAspmEntryCounters() override;
    RC DoResetErrorCounters() override;
    RC DoSetAspmState(UINT32 state) override;
    void DoSetForceCountersToZero(bool forceZero) override
        { return; }
    RC DoSetLinkSpeed(Pci::PcieLinkSpeed newSpeed) override;
    RC DoSetLinkWidth(UINT32 linkWidth) override
        { return RC::UNSUPPORTED_FUNCTION; }
    void DoSetUsePolledErrors(bool bUsePoll) override
        { return; }
    RC DoShutdown() override;
    bool DoUsePolledErrors() const override
        { return false; }

    UINT32 GetRegLanes(bool isTx) const override;

    // PcieUphy
    UINT32 DoGetActiveLaneMask(RegBlock regBlock) const override;
    UINT32 DoGetMaxRegBlocks(RegBlock regBlock) const override { return 1; }
    UINT32 DoGetMaxLanes(RegBlock regBlock) const override;
    bool DoGetRegLogEnabled() const override { return m_bUphyRegLogEnabled; }
    UINT64 DoGetRegLogRegBlockMask(RegBlock regBlock) override { return 1ULL; }
    Version DoGetVersion() const override { return UphyIf::Version::V30; }
    RC DoIsRegBlockActive(RegBlock regBlock, UINT32 blockIdx, bool *pbActive) override
        { MASSERT(pbActive); *pbActive = true; return RC::OK; }
    bool DoIsUphySupported() const override { return false; }
    RC DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData) override;
    RC DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data) override;
    RC DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData) override;
    RC DoSetRegLogLinkMask(UINT64 linkMask) override { return RC::OK; }
    void DoSetRegLogEnabled(bool bEnabled) override { m_bUphyRegLogEnabled = bEnabled; }

    Device::LibDeviceHandle GetLibHandle() { return m_LibHandle; }

private:
    RC GetHostErrorCounts(PexErrorCounts* pCounts, PexDevice::PexCounterType countType);
    RC GetLocalErrorCounts(PexErrorCounts* pCounts);

    Device::LibDeviceHandle m_LibHandle = Device::ILWALID_LIB_HANDLE;

    bool m_bRestoreAer = false;
    bool m_bOrigAerEnabled = false;
    bool m_bUphyRegLogEnabled = false;
};

