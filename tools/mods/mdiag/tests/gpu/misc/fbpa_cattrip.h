/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018, 2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _FBPA_CATTRIP_H_
#define _FBPA_CATTRIP_H_

#include "mdiag/tests/gpu/lwgpu_single.h"
#include "pwr_macro.h"
class FbpaCattripTest: public LWGpuSingleChannelTest
{
public:
        FbpaCattripTest(ArgReader *params);
        virtual int Setup(void);
        virtual void Run(void);
        virtual void CleanUp(void);
        static Test *Factory(ArgDatabase *args);

private:
        UINT32 m_arch;
        Macro macros;
        int errCnt;
        int test_connection;

        int TestConnection();
};

#ifdef MAKE_TEST_LIST
// This macro is defined in ../../test.h
CREATE_TEST_LIST_ENTRY(fbpa_cattrip, FbpaCattripTest, "Verify that fbpa cattrip signals are merged to overt. (Bug 200033051)");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &fbpa_cattrip_testentry
#endif

#endif //_FBPA_CATTRIP_H_

