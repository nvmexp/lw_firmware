/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef RMT_DMEMVA_H
#define RMT_DMEMVA_H

#include "gpu/tests/rmtest.h"
#include "lwtypes.h"

// #define USE_FAST_REBUILD

struct TestCtl
{
    bool bRunOnSec;
    bool bRunOnPmu;
    bool bRunOnLwdec;
    bool bRunOnGsp;
};

class DmvaTest : public RmTest
{
public:
    DmvaTest();
    virtual ~DmvaTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    LwU32 memrd32(LwU32 *addr);
    void memwr32(LwU32 *addr, LwU32 data);

    SETGET_PROP(bDmvaTestSec, bool);
    SETGET_PROP(bDmvaTestPmu, bool);
    SETGET_PROP(bDmvaTestLwdec, bool);
    SETGET_PROP(bDmvaTestGsp, bool);

private:
    bool m_bDmvaTestSec;
    bool m_bDmvaTestPmu;
    bool m_bDmvaTestLwdec;
    bool m_bDmvaTestGsp;
    GpuPtr m_pGpu;
};

RC runTestSuite(DmvaTest *pDmvaTest, TestCtl testCtl);

#endif
