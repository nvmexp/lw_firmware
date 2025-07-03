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

//! \file gpupcie.h -- Pcie interface definition

#pragma once

#ifndef INCLUDED_PCIE_H
#define INCLUDED_PCIE_H

#include "core/include/device.h"
#include "core/include/pci.h"
#include "core/include/rc.h"
#include "core/include/types.h"
#include "gpu/utility/pcie/pexdev.h"
#include <vector>

typedef struct PciDevInfo_t PciDevInfo;

//--------------------------------------------------------------------
//! \brief Pure virtual interface describing PCIE interfaces specific to a device
//!
class Pcie
{
public:
    INTERFACE_HEADER(Pcie);

    enum L1ClockPmSubstates
    {
         L1_CLK_PM_DISABLED   = 0
        ,L1_CLK_PM_PLLPD      = (1 << 0)
        ,L1_CLK_PM_CPM        = (1 << 1)
        ,L1_CLK_PM_PLLPD_CPM  = (L1_CLK_PM_PLLPD | L1_CLK_PM_CPM)
        ,L1_CLK_PM_UNKNOWN    = (1 << 2)
    };

    virtual ~Pcie() {};

    // General PCIE Functions
    UINT32 DeviceId() const
        { return DoDeviceId(); }
    UINT32 DomainNumber() const
        { return DoDomainNumber(); }
    UINT32 BusNumber() const
        { return DoBusNumber(); }
    UINT32 DeviceNumber() const
        { return DoDeviceNumber(); }
    UINT32 FunctionNumber() const
        { return DoFunctionNumber(); }
    UINT32 SubsystemDeviceId()
        { return DoSubsystemDeviceId(); }
    UINT32 SubsystemVendorId()
        { return DoSubsystemVendorId(); }

    // PCIE Link speed and width functions
    bool IsASLMCapable()
        { return DoIsASLMCapable(); }
    Pci::PcieLinkSpeed GetLinkSpeed(Pci::PcieLinkSpeed expectedSpeed)
        { return DoGetLinkSpeed(expectedSpeed); }
    UINT32 GetLinkWidth()
        { return DoGetLinkWidth(); }
    RC GetLinkWidths(UINT32 *pGpuWidth, UINT32 *pHostWidth)
        { return DoGetLinkWidths(pGpuWidth, pHostWidth); }
    Pci::PcieLinkSpeed LinkSpeedCapability() const
        { return DoLinkSpeedCapability(); }
    UINT32 LinkWidthCapability()
        { return DoLinkWidthCapability(); }
    RC SetLinkSpeed(Pci::PcieLinkSpeed newSpeed)
        { return DoSetLinkSpeed(newSpeed); }
    RC SetLinkWidth(UINT32 linkWidth)
        { return DoSetLinkWidth(linkWidth); }

    // PCIE Error counting functions
    bool AerEnabled() const
        { return DoAerEnabled(); }
    void EnableAer(bool flag)
        { return DoEnableAer(flag); }
    UINT16 GetAerOffset() const
        { return DoGetAerOffset(); }
    RC GetAerLog(vector<PexDevice::AERLogEntry>* pLogEntries) const
        { return DoGetAerLog(pLogEntries); }
    RC ClearAerLog()
        { return DoClearAerLog(); }
    RC GetErrorCounts
    (
        PexErrorCounts            *pLocCounts,
        PexErrorCounts            *pHostCounts,
        PexDevice::PexCounterType  countType
    ) { return DoGetErrorCounts(pLocCounts, pHostCounts, countType); }
    RC GetPolledErrors
    (
        Pci::PcieErrorsMask* pDevErrors,
        Pci::PcieErrorsMask *pHostErrors
    ) { return DoGetPolledErrors(pDevErrors, pHostErrors); }
    bool GetForceCountersToZero() const
        { return DoGetForceCountersToZero(); }
    void SetForceCountersToZero(bool forceZero)
        { return DoSetForceCountersToZero(forceZero); }
    void SetUsePolledErrors(bool bUsePoll)
        { return DoSetUsePolledErrors(bUsePoll); }
    RC ResetErrorCounters()
        { return DoResetErrorCounters(); }
    bool UsePolledErrors() const
        { return DoUsePolledErrors(); }

    // PCIE ASPM functions
    // Note: Setting changing ASPM L1 state requires both the upstream
    // and downstream device to be done at the same time.
    struct AspmEntryCounters
    {
        UINT32 XmitL0SEntry;        //!< Number of XMIT L0S Entries
        UINT32 UpstreamL0SEntry;    //!< Number of upstream L0S Entries
        UINT32 L1Entry;             //!< Number of L1 Entries
        UINT32 L1PEntry;            //!< Number of L1P Entries
        UINT32 DeepL1Entry;         //!< Number of DeepL1 Entries
        UINT32 L1_1_Entry;          //!< Number of L1_1 substate Entries
        UINT32 L1_2_Entry;          //!< Number of L1_2 substate Entries
        UINT32 L1_2_Abort;
        UINT32 L1SS_DeepL1Timouts;
        UINT32 L1_ShortDuration;
        UINT32 L1ClockPmPllPdEntry;
        UINT32 L1ClockPmCpmEntry;
    };
    UINT32 AspmCapability()
        { return DoAspmCapability(); }
    RC EnableAspmDeepL1(bool bEnable)
        { return DoEnableAspmDeepL1(bEnable); }
    RC EnableL1ClockPm(bool bEnabled)
        { return DoEnableL1ClockPm(bEnabled); }
    RC GetAspmCyaL1SubState(UINT32 *pState)
        { return DoGetAspmCyaL1SubState(pState); }
    RC GetAspmCyaL1SubStateProtected(UINT32 *pState) const
        { return DoGetAspmCyaL1SubStateProtected(pState); }
    UINT32 GetAspmCyaState()
        { return DoGetAspmCyaState(); }
    RC GetAspmCyaStateProtected(UINT32 *pState) const
        { return DoGetAspmCyaStateProtected(pState); }
    RC GetAspmDeepL1Enabled(bool *pbEnabled)
        { return DoGetAspmDeepL1Enabled(pbEnabled); }
    RC GetAspmEntryCounts(AspmEntryCounters *pCounts)
        { return DoGetAspmEntryCounts(pCounts); }
    RC GetAspmL1SSCapability(UINT32 *pCap)
        { return DoGetAspmL1SSCapability(pCap); }
    RC GetAspmL1ssEnabled(UINT32 *pState)
        { return DoGetAspmL1ssEnabled(pState); }
    UINT32 GetAspmState()
        { return DoGetAspmState(); }
    RC GetL1ClockPmCyaSubstate(UINT32 *pSubState) const
        { return DoGetL1ClockPmCyaSubstate(pSubState); }
    RC GetL1ClockPmCyaSubstateProtected(UINT32 *pSubState) const
        { return DoGetL1ClockPmCyaSubstateProtected(pSubState); }
    UINT32 GetL1SubstateOffset()
        { return DoGetL1SubstateOffset(); }
    RC GetLTREnabled(bool *pEnabled)
        { return DoGetLTREnabled(pEnabled); }
    bool HasAspmL1Substates()
        { return DoHasAspmL1Substates(); }
    RC HasAspmDeepL1(bool *pbHasDeepL1)
        { return DoHasAspmDeepL1(pbHasDeepL1); }
    RC HasL1ClockPm(bool *pbHasL1ClockPm) const
        { return DoHasL1ClockPm(pbHasL1ClockPm); }
    bool HasL1ClockPmSubstates() const
        { return DoHasL1ClockPmSubstates(); }
    RC IsL1ClockPmEnabled(bool *pbEnabled) const
        { return DoIsL1ClockPmEnabled(pbEnabled); }
    RC ResetAspmEntryCounters()
        { return DoResetAspmEntryCounters(); }
    RC SetAspmState(UINT32 state)
        { return DoSetAspmState(state); }
    RC SetAspmCyaState(UINT32 state)
        { return DoSetAspmCyaState(state); }
    RC SetAspmCyaL1SubState(UINT32 state)
        { return DoSetAspmCyaL1SubState(state); }
    RC SetL1ClockPmCyaSubstate(UINT32 state)
        { return DoSetL1ClockPmCyaSubstate(state); }

    // NOTE : DO NOT ADD "SET" FUNCTIONS FOR ASPM L1 SUBSTATES
    // They cannot be arbitrarily changed like ASPM states.
    // See PexDevMgr::SetAspmL1SS for details

    // PCIE Functions associated with the device upstream of the GPU
    RC GetUpStreamInfo(PexDevice **pPexDev)
        { return DoGetUpStreamInfo(pPexDev); }
    RC GetUpStreamInfo(PexDevice **pPexDev, UINT32* pPort)
        { return DoGetUpStreamInfo(pPexDev, pPort); }
    Pci::PcieLinkSpeed GetUpstreamLinkSpeedCap()
        { return DoGetUpstreamLinkSpeedCap(); }
    UINT32 GetUpstreamLinkWidthCap()
        { return DoGetUpstreamLinkWidthCap(); }
    RC SetUpStreamDevice(PexDevice* pPexDev, UINT32 index)
        { return DoSetUpStreamDevice(pPexDev, index); }

    // Miscellaneous PCIE functions
    RC GetPcieLanes(UINT32 *pRxLanes, UINT32 *pTxLanes)
        { return DoGetPcieLanes(pRxLanes, pTxLanes); }
    RC GetPcieDetectedLanes(UINT32 *pLanes)
        { return DoGetPcieDetectedLanes(pLanes); }
    UINT32 AdNormal()
        { return DoAdNormal(); }
    bool DidLinkEnterRecovery()
        { return DoDidLinkEnterRecovery(); }
    RC EnableAtomics(bool *bAtomicsEnabled, UINT32 *pAtomicTypes)
        { return DoEnableAtomics(bAtomicsEnabled, pAtomicTypes); }
    RC Initialize(Device::LibDeviceHandle libHandle)
        { return DoInitialize(libHandle); }
    RC Initialize()
        { return DoInitialize(Device::ILWALID_LIB_HANDLE); }
    bool SupportsFom() const
        { return DoSupportsFom(); }
    bool SupportsFomMode(Pci::FomMode mode)
        { return DoSupportsFomMode(mode); }
    RC GetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs)
        { return DoGetEomStatus(mode, status, timeoutMs); }
    RC Shutdown()
        { return DoShutdown(); }
    RC SetResetCrsTimeout(UINT32 timeUs)
        { return DoSetResetCrsTimeout(timeUs); }
    RC GetScpmStatus(UINT32 *pStatus)
        { return DoGetScpmStatus(pStatus); }
    string DecodeScpmStatus(UINT32 status)
        { return DoDecodeScpmStatus(status); }

    static const char * L1ClockPMSubstateToString(L1ClockPmSubstates state);

protected:
    virtual UINT32 DoAdNormal() = 0;
    virtual bool DoAerEnabled() const = 0;
    virtual UINT32 DoAspmCapability() = 0;
    virtual UINT32 DoBusNumber() const = 0;
    virtual RC DoClearAerLog() = 0;
    virtual UINT32 DoDeviceId() const = 0;
    virtual UINT32 DoDeviceNumber() const = 0;
    virtual bool DoDidLinkEnterRecovery() = 0;
    virtual RC DoEnableAtomics(bool *bAtomicsEnabled, UINT32 *pAtomicTypes) = 0;
    virtual UINT32 DoDomainNumber() const = 0;
    virtual void DoEnableAer(bool flag) = 0;
    virtual RC DoEnableAspmDeepL1(bool bEnable) = 0;
    virtual RC DoEnableL1ClockPm(bool bEnabled) = 0;
    virtual UINT32 DoFunctionNumber() const = 0;
    virtual RC DoGetAerLog(vector<PexDevice::AERLogEntry>* pLogEntries) const = 0;
    virtual UINT16 DoGetAerOffset() const = 0;
    virtual RC DoGetAspmCyaL1SubState(UINT32 *pState) = 0;
    virtual RC DoGetAspmCyaL1SubStateProtected(UINT32 *pState) const = 0;
    virtual UINT32 DoGetAspmCyaState() = 0;
    virtual RC DoGetAspmCyaStateProtected(UINT32 *pState) const = 0;
    virtual RC DoGetAspmDeepL1Enabled(bool *pbEnabled) = 0;
    virtual RC DoGetAspmEntryCounts(AspmEntryCounters *pCounts) = 0;
    virtual RC DoGetAspmL1SSCapability(UINT32 *pCap) = 0;
    virtual RC DoGetAspmL1ssEnabled(UINT32 *pState) = 0;
    virtual UINT32 DoGetAspmState() = 0;
    virtual RC DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs) = 0;
    virtual RC DoGetErrorCounts
    (
        PexErrorCounts            *pLocCounts,
        PexErrorCounts            *pHostCounts,
        PexDevice::PexCounterType  countType
    ) = 0;
    virtual bool DoGetForceCountersToZero() const = 0;
    virtual RC DoGetL1ClockPmCyaSubstate(UINT32 *pState) const = 0;
    virtual RC DoGetL1ClockPmCyaSubstateProtected(UINT32 *pState) const = 0;
    virtual UINT32 DoGetL1SubstateOffset() = 0;
    virtual Pci::PcieLinkSpeed DoGetLinkSpeed(Pci::PcieLinkSpeed expectedSpeed) = 0;
    virtual UINT32 DoGetLinkWidth() = 0;
    virtual RC DoGetLinkWidths(UINT32 *pGpuWidth, UINT32 *pHostWidth) = 0;
    virtual RC DoGetLTREnabled(bool *pEnabled) = 0;
    virtual RC DoGetPolledErrors
    (
        Pci::PcieErrorsMask* pGpuErrors,
        Pci::PcieErrorsMask *pHostErrors
    ) = 0;
    virtual RC DoGetUpStreamInfo(PexDevice **pPexDev, UINT32* pPort) = 0;
    virtual RC DoGetUpStreamInfo(PexDevice **pPexDev) = 0;
    virtual Pci::PcieLinkSpeed DoGetUpstreamLinkSpeedCap() = 0;
    virtual UINT32 DoGetUpstreamLinkWidthCap() = 0;
    virtual RC DoHasAspmDeepL1(bool *pbHasDeepL1) = 0;
    virtual bool DoHasAspmL1Substates() = 0;
    virtual RC DoHasL1ClockPm(bool *pbHasL1ClockPm) const = 0;
    virtual bool DoHasL1ClockPmSubstates() const = 0;
    virtual RC DoInitialize
    (
        Device::LibDeviceHandle libHandle
    ) = 0;
    virtual bool DoIsASLMCapable() = 0;
    virtual RC DoIsL1ClockPmEnabled(bool *pbEnabled) const = 0;
    virtual Pci::PcieLinkSpeed DoLinkSpeedCapability() const = 0;
    virtual UINT32 DoLinkWidthCapability() = 0;
    virtual RC DoResetAspmEntryCounters() = 0;
    virtual RC DoResetErrorCounters() = 0;
    virtual RC DoSetAspmState(UINT32 state) = 0;
    virtual RC DoSetAspmCyaState(UINT32 state) = 0;
    virtual RC DoSetAspmCyaL1SubState(UINT32 state) = 0;
    virtual void DoSetForceCountersToZero(bool forceZero) = 0;
    virtual RC DoSetL1ClockPmCyaSubstate(UINT32 state) = 0;
    virtual RC DoSetLinkSpeed(Pci::PcieLinkSpeed newSpeed) = 0;
    virtual RC DoSetLinkWidth(UINT32 linkWidth) = 0;
    virtual RC DoGetPcieLanes(UINT32 *pRxLanes, UINT32 *pTxLanes) = 0;
    virtual RC DoGetPcieDetectedLanes(UINT32 *pLanes) = 0;
    virtual RC DoSetResetCrsTimeout(UINT32 timeUs) = 0;
    virtual RC DoSetUpStreamDevice(PexDevice* pPexDev, UINT32 index) = 0;
    virtual void DoSetUsePolledErrors(bool bUsePoll) = 0;
    virtual RC DoShutdown() = 0;
    virtual UINT32 DoSubsystemDeviceId() = 0;
    virtual UINT32 DoSubsystemVendorId() = 0;
    virtual bool DoSupportsFom() const = 0;
    virtual bool DoSupportsFomMode(Pci::FomMode mode) = 0;
    virtual bool DoUsePolledErrors() const = 0;
    virtual RC DoGetScpmStatus(UINT32 *pStatus) const = 0;
    virtual string DoDecodeScpmStatus(UINT32 status) const = 0;
};

#endif

