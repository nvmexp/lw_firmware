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

//! \file lwlinkimpl.h -- Common LwLink implementation definition

#pragma once

#include "device/interface/lwlink.h"
#include "device/interface/lwlink/lwlpowerstate.h"
#include "device/interface/lwlink/lwlregs.h"
#include "gpu/reghal/lwlinkreghal.h"

//--------------------------------------------------------------------
//! \brief Common interface describing LwLink interfaces for all LwLink devices
//!
class LwLinkImpl :
    public LwLink
   ,public LwLinkPowerState
   ,public LwLinkRegs
{
protected:
    LwLinkImpl() = default;
    virtual ~LwLinkImpl() {}

    bool DoAreLanesReversed(UINT32 linkId) const override;
    RC DoAssertBufferReady(UINT32 linkId, bool ready) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoClearErrorCounts(UINT32 linkId)
        { return DoClearHwErrorCounts(linkId); }
    RC DoConfigurePorts(UINT32 topologyId) override;
    RC DoDumpRegs(UINT32 linkId) const override;
    RC DoDumpAllRegs(UINT32 linkId) override;
    FLOAT64 DoGetDefaultErrorThreshold
    (
        ErrorCounts::ErrId errId,
        bool bRateErrors
    ) const;
    UINT32 DoGetFabricRegionBits() const override { return 0; }
    RC DoGetFomStatus(vector<PerLaneFomData> * pFomData) override;
    UINT32 DoGetLineRateMbps(UINT32 linkId) const override;
    UINT32 DoGetLinkClockRateMhz(UINT32 linkId) const override;
    string DoGetLinkGroupName() const override { return "IOCTRL"; }
    UINT32 DoGetLinkDataRateKiBps(UINT32 linkId) const override;
    UINT32 DoGetLinksPerGroup() const override { return DoGetMaxLinks(); }
    UINT32 DoGetLinkVersion(UINT32 linkId) const override;
    UINT32 DoGetMaxLinkDataRateKiBps(UINT32 linkId) const override;
    UINT32 DoGetMaxLinkGroups() const override { return 1; }
    UINT64 DoGetMaxLinkMask() const override { return (1ULL << GetMaxLinks()) - 1;}
    RC DoGetRemoteEndpoint(UINT32 locLinkId, EndpointInfo *pRemoteEndpoint) const override;
    UINT32 DoGetSublinkWidth(UINT32 linkId) const override;
    UINT32 DoGetMaxPerLinkPerDirBwKiBps(UINT32 linkRateMHz) const override;
    UINT32 DoGetMaxTotalBwKiBps(UINT32 linkRateMHz) const override;
    UINT64 DoGetNeaLoopbackLinkMask() override
        { return m_NeaLoopbackLinkMask; }
    UINT64 DoGetNedLoopbackLinkMask() override
        { return m_NedLoopbackLinkMask; }
    RC DoGetPciInfo
    (
        UINT32  linkId,
        UINT32 *pDomain,
        UINT32 *pBus,
        UINT32 *pDev,
        UINT32 *pFunc
    ) override;
    UINT32 DoGetRefClkSpeedMhz(UINT32 linkId) const override;
    UINT32 DoGetRxdetLaneMask(UINT32 linkId) const override;
    SystemLineRate DoGetSystemLineRate(UINT32 linkId) const override
        { return SystemLineRate::GBPS_UNKNOWN; }
    RC DoGetXbarBandwidth
    (
        bool bWriting,
        bool bReading,
        bool bWrittenTo,
        bool bReadFrom,
        UINT32 *pXbarBwKiBps
    ) override { return RC::UNSUPPORTED_FUNCTION; }
    bool DoHasCollapsedResponses() const override { return false; }
    RC DoInitialize(LibDevHandles handles) override;
    RC DoInitializePostDiscovery() override;
    bool DoIsInitialized() const override { return m_Initialized; }
    bool DoIsLinkAcCoupled(UINT32 linkId) const override { return false; }
    bool DoIsLinkActive(UINT32 linkId) const override;
    bool DoIsLinkPam4(UINT32 linkId) const override { return false; }
    bool DoIsLinkValid(UINT32 linkId) const override;
    RC DoIsPerLaneEccEnabled(UINT32 linkId, bool *pbPerLaneEccEnabled) const override;
    bool DoIsPostDiscovery() const override { return m_PostDiscovery; }
    RegHal& DoRegs() override { return m_LwLinkRegs; }
    const RegHal& DoRegs() const override { return m_LwLinkRegs; }
    RC DoReleaseCounters() override { return RC::OK; }
    RC DoReserveCounters() override { return RC::OK; }
    RC DoResetTopology() override;
    RC DoSetCompressedResponses(bool bEnable) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoSetDbiEnable(bool bEnable) override
        { return RC::UNSUPPORTED_FUNCTION; }
    void DoSetEndpointInfo(const vector<EndpointInfo>& endpointInfo) override
        { m_EndpointInfo = endpointInfo; }
    RC DoSetLinkState(UINT64 linkMask, SubLinkState state) override;
    void DoSetNeaLoopbackLinkMask(UINT64 linkMask) override
        { m_NeaLoopbackLinkMask = linkMask; }
    RC DoSetNearEndLoopbackMode(UINT64 linkMask, NearEndLoopMode loopMode)
        { return RC::UNSUPPORTED_FUNCTION; }
    void DoSetNedLoopbackLinkMask(UINT64 linkMask) override
        { m_NedLoopbackLinkMask = linkMask; }
    RC DoSetSystemParameter(UINT64 linkMask, SystemParameter parm, SystemParamValue val) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoShutdown();
    bool DoSupportsErrorCaching() const
        { return false; }
    RC DoWaitForLinkInit() override;
    RC DoWaitForIdle(FLOAT64 timeoutMs) override
        { return RC::OK; }
    RC DoGetLineCode(UINT32 linkId, SystemLineCode *pLineCode) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoGetBlockCodeMode(UINT32 linkId, SystemBlockCodeMode *pBlockCodeMode) override
        { return RC::UNSUPPORTED_FUNCTION; }

protected:
    virtual RC CheckLinkMask(UINT64 linkMask, const char *pFunc) const;
    virtual RC ChangeLinkState
    (
        SubLinkState fromState
       ,SubLinkState toState
       ,const EndpointInfo &locEp
       ,const EndpointInfo &remEp
    );
    virtual RC ChangeLinkState
    (
        SubLinkState fromState,
        SubLinkState toState,
        const vector<pair<EndpointInfo, EndpointInfo>>& epPairs
    );
    UINT32 CallwlateLinkBwKiBps(UINT32 lineRateMbps, UINT32 sublinkWidth) const;
    virtual ErrorCounts::ErrId ErrorCounterBitToId(UINT32 counterBit);
    virtual UINT32 GetErrorCounterControlMask(bool bIncludePerLane);

    // Need to use template because lwlink_link_state cannot be forward declared
    template <typename RmLinkState>
    RC ValidateLinkState(LwLink::SubLinkState state, const RmLinkState &linkState);

    static ModsGpuRegValue SystemLineRateToRegVal(SystemLineRate lineRate);
    static ModsGpuRegValue SystemLineCodeToRegVal(SystemLineCode lineCode);
    static ModsGpuRegValue SystemTxTrainAlgToRegVal(SystemTxTrainAlg txTrainAlg);
    static ModsGpuRegValue SystemBlockCodeToRegVal(SystemBlockCodeMode blockCode);
    static ModsGpuRegValue SystemRefClockModeToRegVal(SystemRefClockMode refCLockMode);
    static ModsGpuRegValue SystemChannelTypeToRegVal(SystemChannelType channelType);

private:
    static string ClaimStateToString(ClaimState state);

    LwLinkRegHal         m_LwLinkRegs;
    bool                 m_Initialized = false;     //!< True if the lwlink was initialized
    bool                 m_PostDiscovery = false;
    vector<LinkInfo>     m_LinkInfo;                //!< Info for each link on the device
    vector<EndpointInfo> m_EndpointInfo;            //!< Endpoint info for each link on the
                                                    //!< device
    UINT64               m_NeaLoopbackLinkMask = 0; //!< Mask that tells which link shoud be
                                                    //!< set to NEA Loopback mode
    UINT64               m_NedLoopbackLinkMask = 0; //!< Mask that tells which link shoud be
                                                    //!< set to NED Loopback mode

    // LwLinkPowerState interface
    bool LwlPowerStateIsSupported() const override
        { return false; }
    RC DoGetLinkPowerStateStatus
    (
        UINT32 linkId,
        LinkPowerStateStatus *pLinkPowerStatus
    ) const override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC DoRequestPowerState(
        UINT32 linkId
      , SubLinkPowerState state
      , bool bHwControlled
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }
    UINT32 DoGetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const override
        { MASSERT(!"Power state not supported"); return 0; }
    bool DoSupportsLpHwToggle() const override
        { return false; }
    RC DoStartPowerStateToggle(UINT32 linkId, UINT32 countIn, UINT32 countOut) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC   DoCheckPowerStateAvailable
    (
        LwLinkPowerState::SubLinkPowerState sls,
        bool bHwControl
    ) override;

    RC DoClaimPowerState(ClaimState state);
    RC DoReleasePowerState();

    RC DoRegRd(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 *pData) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoRegWr(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) override
        { return RC::UNSUPPORTED_FUNCTION; }
    RC DoRegWrBroadcast(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data) override
        { return RC::UNSUPPORTED_FUNCTION; }

    Tasker::Mutex m_PowerStateClaimMutex   = Tasker::NoMutex();
    ClaimState    m_ClaimedPowerState      = ClaimState::NONE;
    UINT32        m_PowerStateRefCount     = 0;
    vector<LinkPowerStateStatus> m_OriginalPowerStates;

protected:
    virtual TestDevice* GetDevice() = 0;
    virtual const TestDevice* GetDevice() const = 0;

};
