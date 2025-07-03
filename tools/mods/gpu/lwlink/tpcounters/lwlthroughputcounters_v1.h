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

#include "lwlthroughputcountersimpl.h"

class LwLink;

class LwLinkThroughputCountersV1 : public LwLinkThroughputCountersImpl
{
public:
    LwLinkThroughputCountersV1() = default;
    virtual ~LwLinkThroughputCountersV1() { }
private:

    // Throughput counter helper functions
    bool DoIsCounterSupported(LwLinkThroughputCount::CounterId cid) const override;
    RC ReadThroughputCounts
    (
        UINT32 linkId
       ,UINT32 counterMask
       ,vector<LwLinkThroughputCount> *pCounts
    ) override;
    RC WriteThroughputClearRegs(UINT32 linkId, UINT32 counterMask) override;
    RC WriteThroughputStartRegs(UINT32 linkId, UINT32 counterMask, bool bStart) override;
    RC WriteThroughputSetupRegs
    (
        UINT32 linkId,
        const vector<LwLinkThroughputCount::Config> &configs
    ) override;
};
