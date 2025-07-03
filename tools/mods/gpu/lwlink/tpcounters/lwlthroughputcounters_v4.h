/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "lwlthroughputcounters_v3.h"

class LwLink;

class LwLinkThroughputCountersV4 : public LwLinkThroughputCountersV3
{
public:
    LwLinkThroughputCountersV4() = default;
    virtual ~LwLinkThroughputCountersV4() { }

private:
    RC WriteThroughputClearRegs(UINT32 linkId, UINT32 counterMask) override;
    RC WriteThroughputStartRegs(UINT32 linkId, UINT32 counterMask, bool bStart) override;
    RC CaptureCounter(LwLinkThroughputCount::CounterId counterId, UINT32 linkId) override;
};
