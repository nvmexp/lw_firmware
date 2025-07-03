/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _PWR_TSENSE_H_
#define _PWR_TSENSE_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"
#include "pwr_macro.h"

class TSENSETest: public LWGpuSingleChannelTest
{
public:
    TSENSETest(ArgReader *params);
    virtual ~TSENSETest(void);

    static Test *Factory(ArgDatabase *args);

    virtual int Setup(void);

    virtual void Run(void);

    virtual void CleanUp(void);

private:
    UINT32 m_arch;
    int tsense_direct_test();// returns error count
    Macro macros;

    UINT32 m_OptTsCalAvg;
    UINT32 m_OptTsTc;
    int m_OptIntTsA;
    int m_OptIntTsB;
    bool m_SkipLwlink;
    int errCnt;

};

#ifdef MAKE_TEST_LIST
// This macro is defined in ../../test.h
CREATE_TEST_LIST_ENTRY(tsense, TSENSETest, "Test TSENSE logic");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &tsense_testentry
#endif

#endif //_PWR_TSENSE_H_
