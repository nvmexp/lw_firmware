/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"

class LwLinkMulticast
{
public:
    virtual ~LwLinkMulticast() {}

    RC GetResidencyCounts(UINT32 linkId, UINT32 mcId, UINT64* pMcCount, UINT64* pRedCount)
        { return DoGetResidencyCounts(linkId, mcId, pMcCount, pRedCount); }
    RC GetResponseCheckCounts(UINT32 linkId, UINT64* pPassCount, UINT64* pFailCount)
        { return DoGetResponseCheckCounts(linkId, pPassCount, pFailCount); }
    RC ResetResponseCheckCounts(UINT32 linkId)
        { return DoResetResponseCheckCounts(linkId); }
    RC StartResidencyTimers(UINT32 linkId)
        { return DoStartResidencyTimers(linkId); }
    RC StopResidencyTimers(UINT32 linkId)
        { return DoStopResidencyTimers(linkId); }

private:
    virtual RC DoGetResidencyCounts(UINT32 linkId, UINT32 mcId, UINT64* pMcCount, UINT64* pRedCount) = 0;
    virtual RC DoGetResponseCheckCounts(UINT32 linkId, UINT64* pPassCount, UINT64* pFailCount) = 0;
    virtual RC DoResetResponseCheckCounts(UINT32 linkId) = 0;
    virtual RC DoStartResidencyTimers(UINT32 linkId) = 0;
    virtual RC DoStopResidencyTimers(UINT32 linkId) = 0;
};
