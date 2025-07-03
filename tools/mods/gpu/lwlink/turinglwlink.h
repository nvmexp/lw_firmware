/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "device/interface/lwlink/lwlsleepstate.h"

#include "voltalwlink.h"

class TuringLwLink :
    public VoltaLwLink
  , public LwLinkSleepState
{
protected:
    TuringLwLink() = default;

    RC DoGetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts) override;
    RC DoInitialize(LibDevHandles handles) override;
private:
    RC DoClearHwErrorCounts(UINT32 linkId) override;
    FLOAT64 DoGetMaxObservableBwPct(LwLink::TransferType tt) override { return 1.0; }

    RC DoClearLPCounts(UINT32 linkId) override;
    RC DoGetLPEntryOrExitCount
    (
        UINT32 linkId,
        bool entry,
        UINT32 *pCount,
        bool *pbOverflow
    ) override;


    bool DoIsSleepStateSupported() const { return true; }
    RC DoSleepStateLock() override;
    RC DoSleepStateUnlock() override;
    RC DoGetSleepState(UINT32 linkId, SleepState *state) const override;
    RC DoSetSleepState(UINT32 mask, SleepState state) override;

    Version DoGetVersion() const override { return UphyIf::Version::V31; }

    // LP counts and error counts on Turing are always cleared together due to
    // RM/Minion implementation.  It is necessary to cache the values to avoid
    // losing counts since MODS relies on being able to clear them separately
    vector<LwLink::ErrorCounts> m_CachedErrorCounts;
    vector<UINT32> m_CachedLpEntryCounts;
    vector<UINT32> m_CachedLpExitCounts;
};
