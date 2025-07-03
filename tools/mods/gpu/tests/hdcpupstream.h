/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gputest.h"

class HDCPUpstream : public GpuTest
{
public:
    enum LfsrInitMode
    {
        NoInit,
        InitA,
        InitB
    };

    HDCPUpstream();
    virtual ~HDCPUpstream();
    RC Setup() override;
    RC Run() override;
    bool IsSupported() override;

private:
    RC CheckKsvs
    (
        const map<string, LwU64>& keysToTest,
        UINT32 index
    );

    // For displayless upstream test, this will always be 1
    UINT32 m_ApsToTest = 1;
};

