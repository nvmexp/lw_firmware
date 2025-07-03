/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _FB_UTIL_H_
#define _FB_UTIL_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"
#include "pwr_macro.h"

class FB_UTIL_Test: public LWGpuSingleChannelTest
{
public:
    FB_UTIL_Test(ArgReader *params);
    virtual ~FB_UTIL_Test(void);

    static Test *Factory(ArgDatabase *args);

    virtual int Setup(void);

    virtual void Run(void);

    virtual void CleanUp(void);

private:
    UINT32 m_arch;
    int fb_util_test(float);// returns error count
    Macro macros;

    int errCnt;

};

#ifdef MAKE_TEST_LIST
// This macro is defined in ../../test.h
CREATE_TEST_LIST_ENTRY(fb_util, FB_UTIL_Test, "Testing FB utilization reporting feature");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &fb_util_testentry
#endif

#endif //_FB_UTIL_H_
