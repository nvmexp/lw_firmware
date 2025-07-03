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
#ifndef _SMARTFANSCI_H_
#define _SMARTFANSCI_H_

// test the smartfan behavior of GPIO[16]

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"

class SmartFanSCI : public LWGpuSingleChannelTest
{

public:
    void DelayNs(UINT32);
    void EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value);
    int EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value);
    SmartFanSCI(ArgReader *params);

    virtual ~SmartFanSCI(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual INT32 Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

    INT32 check_smartfan_support();
    INT32 check_smartfan_straps(UINT32 exp_strap);
    INT32 get_scale_factor(UINT32 *factor);
    INT32 get_boot_period_hi_clks(UINT32 strap, UINT32 *period_clks,
        UINT32 *hi_clks, UINT32 fan_index);
    INT32 get_period_hi_clks(UINT32 period, UINT32 duty_cycle,
        UINT32 *period_clks, UINT32 *hi_clks);
    void trigger_smartfan_monitor(UINT32 period, UINT32 fan_index);
    INT32 check_smartfan_period_dutycycle(UINT32 period, UINT32 duty_cycle,
            UINT32 fan_index);
    INT32 set_smartfan_period_hi(UINT32 period_clks, UINT32 hi_clks,
            UINT32 fan_index);
    INT32 check_smartfan_actual_pwm_regs(UINT32 period_clks, UINT32 hi_clks, 
            UINT32 fan_index);

protected:
    UINT32 smartfanTest();
    UINT32 thermAlertForceTest();
    UINT32 thermAlertTest();
    UINT32 do_update(UINT32 fan_index);
    int over_temp_alert;
    int over_temp_alert_force;
private:
    IRegisterMap* m_regMap;
    UINT32 m_ErrCount;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(smartfan_sci, SmartFanSCI, "GPU SmartFanSCI Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &smartfan_sci_testentry
#endif

#endif  // _SMARTFANSCI_H_
