/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"

class LwLinkRecal
{
public:
    virtual ~LwLinkRecal() {}

    UINT32 GetRecalStartTimeUs(UINT32 linkId) const
    {
        return DoGetRecalStartTimeUs(linkId);
    }

    UINT32 GetRecalWakeTimeUs(UINT32 linkId) const
    {
        return DoGetRecalWakeTimeUs(linkId);
    }

    UINT32 GetMinRecalTimeUs(UINT32 linkId) const
    {
        return DoGetMinRecalTimeUs(linkId);
    }

private:
    virtual UINT32 DoGetRecalStartTimeUs(UINT32 linkId) const = 0;
    virtual UINT32 DoGetRecalWakeTimeUs(UINT32 linkId) const = 0;
    virtual UINT32 DoGetMinRecalTimeUs(UINT32 linkId) const = 0;
};
