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
#ifndef _THERMOVERTLATCH_H_
#define _THERMOVERTLATCH_H_

// test legacy overt latch for GV100
#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"
#include "mdiag/IRegisterMap.h"

class ThermOvertLatch : public LWGpuSingleChannelTest
{

public:

    ThermOvertLatch(ArgReader *params);

    virtual ~ThermOvertLatch(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual INT32 Setup(void);

    // gives the test valid pointer access to the gpu resources after an init/reset
    virtual int SetupGpuPointers(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

    UINT32 address = 0;

protected:
    UINT32 CheckThermOvertLatch();
    UINT32 CheckOvertBeforeFuseSense();
    UINT32 gpu_initialize();
    void   enable_gr_pfifo_engine();
    UINT32 SavePciReg();
    UINT32 RestorePciReg();
private:
    IRegisterMap* m_regMap;
    UINT32 data;
        UINT32 m_callwlated_temp;
        UINT32 m_programmed_temp;
        UINT32 m_time_period;
        UINT32 m_overt_enable;
        UINT32 m_threshold_temp;
        UINT32 m_polling_period;
        UINT32 m_stable_period;
        UINT32 m_failsafe_reg;
        UINT32 m_latchovert_reg;
        UINT32 m_external_overt;
        UINT32 m_SCI_SHUTDOWN_LATCH;
    UINT32 m_pmc_device_enable;
    UINT32  m_pmc_device_elpg_enable;
    UINT32 m_pmc_enable;
    UINT32 m_pmc_elpg_enable;
    
    UINT32 cmd_status ;
    UINT32 rev_id ; 
    UINT32 cache_lat ; 
    UINT32 bar0addr ;
    UINT32 misc ; 
    UINT32 subsys_id ; 
    UINT32 bar1addr ; 
    UINT32 bar2addr ; 
    UINT32 bar3addr ; 
    UINT32 bar4addr ; 
    UINT32 bar5addr ;

    UINT32 pci_domain_num; 
    UINT32 pci_bus_num ;
    UINT32 pci_dev_num ;
    UINT32 pci_func_num ; 
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(therm_overt_latch, ThermOvertLatch, "GPU ThermOvertLatch Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &therm_overt_latch_testentry
#endif

#endif  // _LEGACYOVERTLATCH_H_

