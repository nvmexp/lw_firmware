/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _XBASHONOVERT_H_
#define _XBASHONOVERT_H_

// test xbashing on overt for GV100
#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"
#include "mdiag/IRegisterMap.h"

class XbashOnOvert : public LWGpuSingleChannelTest
{

public:

    XbashOnOvert(ArgReader *params);

    virtual ~XbashOnOvert(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual INT32 Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // gives the test valid pointer access to the gpu resources after an init/reset
    virtual int SetupGpuPointers(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

protected:
    UINT32 CheckXbashOnOvert();
    UINT32 temperature2code_final(UINT32 temperature);
    void enable_gr_pfifo_engine();
    void SCI_initial_programming_xbash();
    UINT32 gpu_initialize();
private:
    IRegisterMap* m_regMap;
    UINT32 data;
    UINT32 m_external_overt;
    UINT32 m_stable_scale;
    UINT32 m_time_period;
    UINT32 m_thresh_time_scale;
    UINT32 m_programmed_temp;
    UINT32 m_polling_period;
    UINT32 final_a;
    UINT32 final_b;
    UINT32 temp126_rawcode;
    UINT32 temp94_rawcode;
    UINT32 m_temp_alert_hi;
    UINT32 m_temp_alert_override;
    UINT32 m_temp_alert_force;
    UINT32 m_fan_cfg1_hi;
    UINT32 m_fan_cfg1_update;
    UINT32 m_gc6_island;
    UINT32 m_ts_clk;

    UINT32 m_pmc_device_enable;
    UINT32  m_pmc_device_elpg_enable;
    UINT32 m_pmc_enable;
    UINT32 m_pmc_elpg_enable;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(xbashlwvdd_overt, XbashOnOvert, "GPU XbashOnOvert Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &xbashlwvdd_overt_testentry
#endif

#endif  // _XBASHONOVERT_H_
