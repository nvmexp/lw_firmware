/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file gpupcie.h -- Concrete implemetation classes for Pcie
// PCIE implemetation for various Device types

#pragma once

#include "core/include/device.h"
#include "device/interface/pcie.h"
#include "gpu/reghal/reghal.h"
#include "mods_reg_hal.h"

//--------------------------------------------------------------------
//! \brief Common PCIE implementation used for various devices
//!
class PcieImpl : public Pcie
{
public:
    PcieImpl() = default;
    virtual ~PcieImpl() {}

protected:
    UINT32 DoAdNormal() override;
    UINT32 DoBusNumber() const override;
    RC DoClearAerLog() override;
    UINT32 DoDeviceId() const override;
    UINT32 DoDeviceNumber() const override;
    UINT32 DoDomainNumber() const override;
    RC DoEnableAspmDeepL1(bool bEnable) override;
    RC DoEnableAtomics(bool *bAtomicsEnabled, UINT32 *pAtomicTypes) override;
    RC DoEnableL1ClockPm(bool bEnable) override;
    UINT32 DoFunctionNumber() const override;
    RC DoGetAerLog(vector<PexDevice::AERLogEntry>* pLogEntry) const override;
    RC DoGetAspmCyaL1SubStateProtected(UINT32 *pState) const override;
    RC DoGetAspmCyaStateProtected(UINT32 *pState) const override;
    RC DoGetAspmDeepL1Enabled(bool *pbEnabled) override;
    RC DoGetAspmL1ssEnabled(UINT32 *pState) override;
    UINT32 DoGetAspmState() override;
    Pci::PcieLinkSpeed DoGetLinkSpeed(Pci::PcieLinkSpeed expectedSpeed) override;
    UINT32 DoGetLinkWidth() override;
    RC DoGetL1ClockPmCyaSubstate(UINT32 *pState) const override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoGetL1ClockPmCyaSubstateProtected(UINT32 *pState) const override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoGetLinkWidths(UINT32 *pGpuWidth, UINT32 *pHostWidth) override;
    RC DoGetPcieLanes(UINT32 *pRxLanes, UINT32 *pTxLanes) override;
    RC DoGetPcieDetectedLanes(UINT32 *pLanes) override;
    RC DoGetLTREnabled(bool *pEnabled) override;
    RC DoGetUpStreamInfo(PexDevice **pPexDev, UINT32* pPort) override;
    RC DoGetUpStreamInfo(PexDevice **pPexDev) override;
    Pci::PcieLinkSpeed DoGetUpstreamLinkSpeedCap() override;
    UINT32 DoGetUpstreamLinkWidthCap() override;
    RC DoHasAspmDeepL1(bool *pbHasDeepL1) override;
    RC DoHasL1ClockPm(bool *pbHasL1ClockPm) const override;
    bool DoHasL1ClockPmSubstates() const override
        { return false; }
    RC DoInitialize(Device::LibDeviceHandle libHandle) override;
    RC DoIsL1ClockPmEnabled(bool *pbEnabled) const override;
    Pci::PcieLinkSpeed DoLinkSpeedCapability() const override;
    UINT32 DoLinkWidthCapability() override;
    RC DoSetAspmCyaState(UINT32 state) override;
    RC DoSetAspmCyaL1SubState(UINT32 state) override;
    RC DoSetL1ClockPmCyaSubstate(UINT32 state)override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetResetCrsTimeout(UINT32 timeUs) override;
    RC DoSetUpStreamDevice(PexDevice* pPexDev, UINT32 index) override;
    UINT32 DoSubsystemDeviceId() override;
    UINT32 DoSubsystemVendorId() override;
    bool DoSupportsFom() const override { return false; }
    bool DoSupportsFomMode(Pci::FomMode mode) override { return false; }
    RC DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoGetScpmStatus(UINT32 *pStatus) const override
        { return RC::UNSUPPORTED_FUNCTION; }
    string DoDecodeScpmStatus(UINT32 status) const override
        { return ""; }

protected:
    struct EomSettings
    {
        UINT08 mode      = 0;
        UINT08 numBlocks = 0;
        UINT08 numErrors = 0;
        UINT08 berEyeSel = 0;
    };

    RC GetDevicePtr(TestDevicePtr *ppTestDevice);
    RC GetUpstreamErrorCounts(PexErrorCounts *pCounts, PexDevice::PexCounterType type);
    RC ReadPci(UINT32 offset, UINT16* pVal);
    RC ReadPciCap(UINT32 offset, UINT16* pVal) const;
    RC ReadPciCap32(UINT32 offset, UINT32* pVal) const;
    RC WritePciCap(UINT32 offset, UINT16 val);
    RC UpdateAerLog();
    UINT32 GetRegLanesTx() const { return GetRegLanes(true); }
    UINT32 GetRegLanesRx() const { return GetRegLanes(false); }
    virtual UINT32 GetRegLanes(bool isTx) const;

    RC GetAtomicTypesSupported(UINT32 * pAtomicTypes);
private:
    RC ClearAerErrorStatus() const;

    //! Structure describing the Upstream PEX device information for the GPU
    struct UpStreamInfo
    {
        PexDevice* pPexDev = nullptr;    //!< Upstream PEX device
        UINT32 portIndex   = 0xFFFFFFFF; //!< this is the downstream port index on the Upstream device
    };
    UpStreamInfo m_UpStreamInfo;  //!< PEX information for the Bridge/Rootpoort

    deque<PexDevice::AERLogEntry> m_AerLog;

protected:
    virtual TestDevice* GetDevice() = 0;
    virtual const TestDevice* GetDevice() const = 0;
};

