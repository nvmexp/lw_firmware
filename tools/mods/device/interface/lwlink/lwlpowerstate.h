/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <string>
#include <tuple>

#include "core/include/rc.h"
#include "core/include/types.h"
#include "core/include/tasker.h"

#include "device/interface/lwlink.h"

class LwLinkPowerState
{
public:
    enum SubLinkPowerState
    {
        SLS_PWRM_FB
      , SLS_PWRM_LP
      , SLS_PWRM_ILWALID
    };

    struct LinkPowerStateStatus
    {
        bool              rxHwControlledPowerState   = false;
        SubLinkPowerState rxSubLinkLwrrentPowerState = SLS_PWRM_ILWALID;
        SubLinkPowerState rxSubLinkConfigPowerState  = SLS_PWRM_ILWALID;
        bool              txHwControlledPowerState   = false;
        SubLinkPowerState txSubLinkLwrrentPowerState = SLS_PWRM_ILWALID;
        SubLinkPowerState txSubLinkConfigPowerState  = SLS_PWRM_ILWALID;
        LinkPowerStateStatus() = default;
    };

    enum class ClaimState
    {
        FULL_BANDWIDTH
       ,SINGLE_LANE
       ,HW_CONTROL
       ,ALL_STATES
       ,NONE
    };

    virtual ~LwLinkPowerState() {};
    // For GPUs we do double virtual dispatch, i.e. we have a GPU object with a
    // virtual interface, which aggregates another "implementation" object, with
    // its own virtual interface. A correct aggregation would forward interface
    // queries from the outer object to the inner. Unfortunately we don't have
    // this mechanism, therefore just querying for an interface via dynamic_cast
    // is not enough to determine if the interface is supported. We need this
    // additional function that will forward the question to the aggregated
    // "implementation" object.
    bool IsSupported() const
    {
        return LwlPowerStateIsSupported();
    }

    RC GetLinkPowerStateStatus(UINT32 linkId, LinkPowerStateStatus *pLinkPowerStatus) const
    {
        return DoGetLinkPowerStateStatus(linkId, pLinkPowerStatus);
    }

    //------------------------------------------------------------------------------
    //! \brief Requests the DL to switch to either lower power (LP) or full
    //! bandwidth (FB) modes
    //!
    //! \param linkId         : Link id to enable or disable hardware desired LP.
    //! \param state          : Indicates either LP or FB state to switch to.
    //! \param bHwControlled  : Enable or disable HW control
    //!
    //! \return OK if successful, not OK otherwise
    RC RequestPowerState(UINT32 linkId, SubLinkPowerState state, bool bHwControlled)
    {
        return DoRequestPowerState(linkId, state, bHwControlled);
    }

    UINT32 GetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const
    {
        return DoGetLowPowerEntryTimeMs(linkId, dir);
    }

    bool SupportsLpHwToggle() const
    {
        return DoSupportsLpHwToggle();
    }

    RC StartPowerStateToggle(UINT32 linkId, UINT32 countIn, UINT32 countOut)
    {
        return DoStartPowerStateToggle(linkId, countIn, countOut);
    }

    class Owner
    {
        public:
            Owner() = default;
            Owner(LwLinkPowerState * pPowerState) : m_pLwLinkPowerState(pPowerState) { }
            ~Owner() { Release(); }
            RC Claim(ClaimState state);
            RC Release();
        private:
            LwLinkPowerState * m_pLwLinkPowerState = nullptr;
            bool m_bClaimed = false;
    };

    RC CheckPowerStateAvailable
    (
        LwLinkPowerState::SubLinkPowerState sls,
        bool bHwControl
    )
    {
        return DoCheckPowerStateAvailable(sls, bHwControl);
    }

    static string SubLinkPowerStateToString(SubLinkPowerState state);
    static string ClaimStateToString(ClaimState state);
private:
    virtual bool LwlPowerStateIsSupported() const = 0;

    virtual RC DoGetLinkPowerStateStatus
    (
        UINT32 linkId
      , LinkPowerStateStatus *pLinkPowerStatus
    ) const = 0;

    virtual RC DoRequestPowerState
    (
        UINT32 linkId
      , SubLinkPowerState state
      , bool bHwControlled
    ) = 0;
    virtual UINT32 DoGetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const = 0;

    virtual bool DoSupportsLpHwToggle() const = 0;
    virtual RC DoStartPowerStateToggle(UINT32 linkId, UINT32 countIn, UINT32 countOut) = 0;

    virtual RC   DoCheckPowerStateAvailable
    (
        LwLinkPowerState::SubLinkPowerState sls,
        bool bHwControl
    ) = 0;

    // Claim and release purposefully do not have public interfaces to force usage of the
    // owner class
    RC ClaimPowerState(ClaimState state)
        { return DoClaimPowerState(state); }
    RC ReleasePowerState()
        { return DoReleasePowerState(); }
    virtual RC DoClaimPowerState(ClaimState state) = 0;
    virtual RC DoReleasePowerState() = 0;
};
