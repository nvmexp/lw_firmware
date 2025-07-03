/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2009, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _WSP2PWR_CONN_H_
#define _WSP2PWR_CONNWSP2PWR_CONN_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"

class wsp2pwr_conn : public LWGpuSingleChannelTest {
public:
    wsp2pwr_conn(ArgReader *params);

    virtual ~wsp2pwr_conn(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

    virtual void DelayNs(UINT32);

protected:
    int wsp2pwr_conn_basicTest();
    void set_idle_mask(int val);
    int  check_idle_connection( bool exp, const char * msg);
    int  check_gpio_connection(int val, bool exp, const char * msg);
    int  check_mspg_ok(bool exp);

private:
    UINT32 m_arch;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(wsp2pwr_conn, wsp2pwr_conn, "wsp2pwr_conn  Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &wsp2pwr_conn_testentry
#endif

#endif  // _WSP2PWR_CONN_H_
