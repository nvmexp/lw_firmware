/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file ibmnpulwlink_p9p.h -- Concrete implemetation for ibmnpu P9 prime version of lwlink

#pragma once

#include "gpu/lwlink/ibmnpulwlink.h"
#include "device/interface/lwlink/lwlpowerstatecounters.h"
#include "device/interface/lwlink/lwlpowerstate.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_libif.h"
#include <map>

//--------------------------------------------------------------------
//! \brief IBM NPU implementation used for P9'+
//!
class IbmP9PLwLink : public IbmNpuLwLink
{
protected:
    IbmP9PLwLink() = default;
    virtual ~IbmP9PLwLink() {}

    RC DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable) override;
    bool DoHasNonPostedWrites() const override { return false; }
    RC DoPerLaneErrorCountsEnabled(UINT32 linkId, bool *pbPerLaneEnabled) override;

    bool DomainRequiresByteSwap(RegHalDomain domain) const override;
private: //Interface Implementations
    // LwLinkPowerState
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

    // LwLinkPowerStateCounters
    RC DoClearLPCounts(UINT32 linkId) override;
    RC DoGetLPEntryOrExitCount
    (
        UINT32 linkId,
        bool entry,
        UINT32 *pCount,
        bool *pbOverflow
    ) override;

private:
    RC SavePerLaneCrcSetting();
    UINT32 m_SavedPerLaneEnable   = ~0U; //!< Saved value for per lane counter enable
};
