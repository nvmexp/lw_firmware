/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/lwrm.h"

#include "turinglwlink.h"

//------------------------------------------------------------------------------
RC TuringLwLink::DoInitialize(LibDevHandles handles)
{
    RC rc;
    CHECK_RC(VoltaLwLink::DoInitialize(handles));

    m_CachedErrorCounts.resize(GetMaxLinks());
    m_CachedLpEntryCounts.resize(GetMaxLinks(), 0);
    m_CachedLpExitCounts.resize(GetMaxLinks(), 0);
    return rc;
}

//------------------------------------------------------------------------------
RC TuringLwLink::DoClearHwErrorCounts(UINT32 linkId)
{
    MASSERT(linkId < m_CachedLpEntryCounts.size());

    RC rc;

    if (LwlPowerStateCountersAreSupported())
    {
        // Cache the LP entry/exit counts here since clearing the error counts will
        // also clear the LP entry/exit counts
        //
        // Note that it is not necessary to accumulate in this function (i.e. not +=)
        // because the current cached error counts will be added as part of the get
        UINT32 tmpCount;
        bool bOverflow;

        // Need to explicitly call the volta version here to get only the current HW value
        // and not the current HW value with the current cached values added to it
        CHECK_RC(VoltaLwLink::DoGetLPEntryOrExitCount(linkId, true, &tmpCount, &bOverflow));
        m_CachedLpEntryCounts[linkId] = tmpCount;

        CHECK_RC(VoltaLwLink::DoGetLPEntryOrExitCount(linkId, false, &tmpCount, &bOverflow));
        m_CachedLpExitCounts[linkId] = tmpCount;
    }

    CHECK_RC(VoltaLwLink::DoClearHwErrorCounts(linkId));
    m_CachedErrorCounts[linkId] = LwLink::ErrorCounts();
    return rc;
}

//------------------------------------------------------------------------------
RC TuringLwLink::DoGetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts)
{
    MASSERT(linkId < m_CachedErrorCounts.size());

    RC rc;
    CHECK_RC(VoltaLwLink::DoGetErrorCounts(linkId, pErrorCounts));
    *pErrorCounts += m_CachedErrorCounts[linkId];
    return rc;
}

//------------------------------------------------------------------------------
RC TuringLwLink::DoGetLPEntryOrExitCount
(
    UINT32 linkId,
    bool entry,
    UINT32 *pCount,
    bool *pbOverflow
)
{
    MASSERT(linkId < m_CachedLpEntryCounts.size());
    RC rc;
    CHECK_RC(VoltaLwLink::DoGetLPEntryOrExitCount(linkId, entry, pCount, pbOverflow));
    if (entry)
        *pCount += m_CachedLpEntryCounts[linkId];
    else
        *pCount += m_CachedLpExitCounts[linkId];
    return rc;
}

//------------------------------------------------------------------------------
RC TuringLwLink::DoClearLPCounts(UINT32 linkId)
{
    MASSERT(linkId < m_CachedLpEntryCounts.size());
    RC rc;

    // Cache the error counts here since clearing the LP counts counts will
    // also clear the error counts
    LwLink::ErrorCounts lwrErrorCounts;
    CHECK_RC(GetErrorCounts(linkId, &m_CachedErrorCounts[linkId]));

    // Note that it is not necessary to accumulate in this function (i.e. not +=)
    // because the current cached error counts will be added as part of the get
    m_CachedErrorCounts[linkId] = lwrErrorCounts;
    
    CHECK_RC(VoltaLwLink::DoClearLPCounts(linkId));

    // Clearing one clears both (mirrors RM implementation)
    m_CachedLpEntryCounts[linkId] = 0;
    m_CachedLpExitCounts[linkId] = 0;
    return rc;
}

//------------------------------------------------------------------------------
RC TuringLwLink::DoSleepStateLock()
{
    RC rc;

    LwRmPtr lwRm;
    LW2080_CTRL_CMD_LWLINK_LOCK_LINK_POWER_STATE_PARAMS lockParams = { LW_TRUE };
    CHECK_RC(lwRm->Control(lwRm->GetSubdeviceHandle(GetGpuSubdevice()),
                            LW2080_CTRL_CMD_LWLINK_LOCK_LINK_POWER_STATE,
                            &lockParams, sizeof(lockParams)));

    return rc;
}

//------------------------------------------------------------------------------
RC TuringLwLink::DoSleepStateUnlock()
{
    RC rc;

    LwRmPtr lwRm;
    LW2080_CTRL_CMD_LWLINK_LOCK_LINK_POWER_STATE_PARAMS lockParams = { LW_FALSE };
    CHECK_RC(lwRm->Control(lwRm->GetSubdeviceHandle(GetGpuSubdevice()),
                            LW2080_CTRL_CMD_LWLINK_LOCK_LINK_POWER_STATE,
                            &lockParams, sizeof(lockParams)));

    return rc;
}

//------------------------------------------------------------------------------
RC TuringLwLink::DoGetSleepState(UINT32 linkId, SleepState* state) const
{
    RC rc;

    LwRmPtr pLwRm;
    LW2080_CTRL_LWLINK_GET_POWER_STATE_PARAMS getStateParams = { linkId };
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetGpuSubdevice()),
                            LW2080_CTRL_CMD_LWLINK_GET_POWER_STATE,
                            &getStateParams, sizeof(getStateParams)));

    *state = static_cast<LwLinkSleepState::SleepState>(getStateParams.powerState);

    return rc;
}

//------------------------------------------------------------------------------
RC TuringLwLink::DoSetSleepState(UINT32 mask, SleepState state)
{
    RC rc;

    LwRmPtr pLwRm;
    LW2080_CTRL_LWLINK_SET_POWER_STATE_PARAMS setStateParams = { mask, state };
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetGpuSubdevice()),
                            LW2080_CTRL_CMD_LWLINK_SET_POWER_STATE,
                            &setStateParams, sizeof(setStateParams)));

    return rc;
}

