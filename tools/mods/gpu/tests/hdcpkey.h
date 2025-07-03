/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef DIAG_MODS_GPU_TESTS_HDCPKEY_H_
#define DIAG_MODS_GPU_TESTS_HDCPKEY_H_

#include "gputest.h"
#include <memory>

class TestDevice;
typedef shared_ptr<TestDevice> TestDevicePtr;

class HdcpKey : public GpuTest
{
public:
    using GpuTest::BindTestDevice;

    enum LfsrInitMode
    {
        NoInit,
        InitA,
        InitB
    };

    HdcpKey();
    virtual ~HdcpKey();
    RC Run();
    bool IsSupported();

    SETGET_PROP(MaxKpToTest, UINT32);
    SETGET_PROP(DisplayToTest, UINT32);
    SETGET_PROP(SkipHDCPUpstreamErrors, bool);

private:
    UINT32 m_MaxKpToTest;
    UINT32 m_DisplayToTest;
    bool   m_SkipHDCPUpstreamErrors = false;
};

#endif /* DIAG_MODS_GPU_TESTS_HDCPKEY_H_ */
