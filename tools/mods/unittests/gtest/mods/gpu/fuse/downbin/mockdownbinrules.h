/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/fuse/downbin/downbinrules.h"

//------------------------------------------------------------------------------
class MockDownbinRule : public DownbinRule
{
public:
    MockDownbinRule()
    {
        m_CallCount = 0;
    }

    RC Execute()
    {
        m_CallCount++;
        return RC::OK;
    }

    UINT32 GetCallCount()
    {
        return m_CallCount;
    }
    void SetCallCount(UINT32 count)
    {
        m_CallCount = count;
    }

private:
    UINT32 m_CallCount;
};
