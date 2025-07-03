/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _GOODN_RST_REINIT_H_
#define _GOODN_RST_REINIT_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"

class GoodnRstReInit : public LWGpuSingleChannelTest {
public:
    GoodnRstReInit(ArgReader *params);

    virtual ~GoodnRstReInit(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

protected:
    UINT32 goodnRstReInitTest();
private:
    bool check_results( UINT32 address, UINT32 expect_data);

    UINT32 m_initVal_tsup_max;
    UINT32 m_initVal_tsup_addr;
    UINT32 m_initVal_thp_addr;
    UINT32 m_initVal_thp_ps;
    UINT32 m_initVal_twidth_pgm2;
    UINT32 m_initVal_tsup_ps;
    UINT32 m_initVal_thp_csps;
    UINT32 m_initVal_fusetime_pgm1;
    UINT32 m_initVal_fusetime_pgm2;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(goodn_rst_reinit, GoodnRstReInit, "Gt21x+ Reset ReInit Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &goodn_rst_reinit_testentry
#endif

#endif  // _GOODN_RST_REINIT_H_
