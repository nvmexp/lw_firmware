/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _VIDPWM_H_
#define _VIDPWM_H_

// test the connection of pwr2pmgr_vid_pwm between LW_pwr and pmgr

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"

class VidPwm : public LWGpuSingleChannelTest
{

public:

    VidPwm(ArgReader *params);

    virtual ~VidPwm(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

    int route_vid_pwm_to_gpio(UINT32 gpio);
    int cfg_vid_pwm_value(UINT32 data);
    int check_gpio_value(UINT32 gpio, UINT32 data);
protected:
    UINT32 vidpwmTest();
private:
    IRegisterMap* m_regMap;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(vid_pwm, VidPwm, "GPU VID PWM Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &vid_pwm_testentry
#endif

#endif  // _VIDPWM_H_
