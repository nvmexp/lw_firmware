/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once

#include "device/interface/lwlink/lwlthroughputcounters.h"

class TestDevice;

class LwLinkThroughputCountersImpl : public LwLinkThroughputCounters
{
public:
    LwLinkThroughputCountersImpl() = default;
    virtual ~LwLinkThroughputCountersImpl() { }

protected:
    bool DoIsSupported() const override;
    RC DoCheckLinkMask(UINT64 linkMask, const char * reason) const override;

protected:
    virtual TestDevice* GetDevice() = 0;
    virtual const TestDevice* GetDevice() const = 0;
};

