/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file voltalwlink.h -- Concrete implementation for volta version of lwlink

#pragma once

#include "device/interface/lwlink/lwlinkuphy.h"
#include "device/interface/lwlink/lwlpowerstatecounters.h"
#include "pascallwlink.h"

#include <vector>
#include <map>

//--------------------------------------------------------------------
//! \brief Pascal implementation used for GP100+
//!
class VoltaLwLink
  : public PascalLwLink
  , public LwLinkPowerStateCounters
  , public LwLinkUphy
{
protected:
    VoltaLwLink() {}
    virtual ~VoltaLwLink() {}

    RC DoDetectTopology() override;
    RC DoGetEomStatus
    (
        FomMode mode
       ,UINT32 link
       ,UINT32 numErrors
       ,UINT32 numBlocks
       ,FLOAT64 timeoutMs
       ,vector<INT32>* status
    ) override;
    RC DoGetErrorFlags(ErrorFlags * pErrorFlags) override;
    RC DoInitializePostDiscovery() override;
    bool DoIsLinkAcCoupled(UINT32 linkId) const override;
    UINT32 DoGetMaxLinkGroups() const override { return (GetMaxLinks() + 1) / 2; } // 2 links per group

    enum ErrFlagType
    {
        TLCRX0,
        TLCRX1,
        TLCRXSYS0,
        TLCTX0,
        TLCTX1,
        TLCTXSYS0,
        MIFRX0,
        MIFTX0,
        LWLIPT,
        LWLIPT_LNK
    };
    typedef vector<pair<ModsGpuRegField, string>> ErrorFlagData;
    typedef map<ErrFlagType, ErrorFlagData> ErrorFlagDefinition;
    RC GetErrorFlagsInternal
    (
        const ErrorFlagDefinition &flags,
        ErrorFlags * pErrorFlags
    );

    bool LwlPowerStateCountersAreSupported() const override { return true; }

    virtual RC GetEomConfigValue
    (
        FomMode mode,
        UINT32 numErrors,
        UINT32 numBlocks,
        UINT32 *pConfigVal
    ) const;

    // LwLinkPowerStateCounters
    RC DoClearLPCounts(UINT32 linkId) override;
    RC DoGetLPEntryOrExitCount
    (
        UINT32 linkId,
        bool entry,
        UINT32 *pCount,
        bool *pbOverflow
    ) override;
private: // Interface Implementations
    // LwLinkPowerState
    bool LwlPowerStateIsSupported() const override { return true; }
    RC DoGetLinkPowerStateStatus
    (
        UINT32 linkId,
        LwLinkPowerState::LinkPowerStateStatus *pLinkPowerStatus
    ) const override;
    RC DoRequestPowerState
    (
        UINT32 linkId
      , LwLinkPowerState::SubLinkPowerState state
      , bool bHwControlled
    ) override;
    UINT32 DoGetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const override;

    // LwLinkUphy
    UINT32 DoGetActiveLaneMask(RegBlock regBlock) const override;
    UINT32 DoGetMaxLanes(RegBlock regBlock) const override;
    UINT32 DoGetMaxRegBlocks(RegBlock regBlock) const override;
    bool DoGetRegLogEnabled() const override { return m_bUphyRegLogEnabled; }
    UINT64 DoGetRegLogRegBlockMask(RegBlock regBlock) override;
    Version DoGetVersion() const override { return UphyIf::Version::V30; }
    RC DoIsRegBlockActive(RegBlock regBlock, UINT32 blockIdx, bool *pbActive) override;
    bool DoIsUphySupported() const override { return true; }
    RC DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData) override;
    RC DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data) override;
    RC DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData) override;
    RC DoSetRegLogLinkMask(UINT64 linkMask) override;
    void DoSetRegLogEnabled(bool bEnabled) override { m_bUphyRegLogEnabled = bEnabled; }

private:
    UINT32 GetDomainBase(RegHalDomain domain, UINT32 linkId) override;
    virtual bool HasLPCountOverflowed(bool bEntry, UINT32 lpCount) const;

    UINT64 m_UphyRegsLinkMask      = 0ULL;
    UINT32 m_UphyRegsIoctlMask     = ~0U;
    bool   m_bUphyRegLogEnabled    = false;
    static const ErrorFlagDefinition s_ErrorFlags;
};
