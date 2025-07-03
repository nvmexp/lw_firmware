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
#ifndef _THERMAL_SHUTDOWN_LATCH_H_
#define _THERMAL_SHUTDOWN_LATCH_H_

// test the thermal shutdown latch behavior in GPIO[8]

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"

class ThermalShutdownLatch : public LWGpuSingleChannelTest
{

public:

    ThermalShutdownLatch(ArgReader *params);

    virtual ~ThermalShutdownLatch(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

    int check_therm_shutdown_support();
    int config_therm_shutdown_fuse(bool value);
    int config_therm_shutdown_priv_enable_disable(bool enable, bool disable);
    int config_overtemp_on_boot_fuse(bool value);
    int config_en_sw_override_fuse(bool value);
    int check_therm_shutdown_state(bool value);
    int clear_therm_shutdown_latch();

protected:
    UINT32 thermal_shutdown_latchTest();
private:
    IRegisterMap* m_regMap;
    UINT32 m_arch;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(thermal_shutdown_latch, ThermalShutdownLatch, "GPU Thermal Shutdown Latch Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &thermal_shutdown_latch_testentry
#endif

#endif  // _THERMAL_SHUTDOWN_LATCH_H_
