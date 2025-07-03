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
#ifndef _OVERTPIN_H_
#define _OVERTPIN_H_

// test dedicated overtpin for GK110

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"
#include "mdiag/IRegisterMap.h"

class FailSafeOvertPin : public LWGpuSingleChannelTest
{

public:

    FailSafeOvertPin(ArgReader *params);

    virtual ~FailSafeOvertPin(void);

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
    UINT32 CheckXtalClockGate();
    UINT32 CheckPexReset();
    UINT32 CheckPexResetFusesSensedHighTemp();
    UINT32 CheckPexResetFusesSensedLowTemp();
    UINT32 CheckPexResetFusesNotSensed();
    UINT32 CheckFundamentalReset();
    UINT32 CheckFundamentalResetFusesSensedHighTemp();
    UINT32 CheckFundamentalResetFusesSensedLowTemp();
    UINT32 CheckFundamentalResetFusesNotSensed();
    UINT32 CheckSoftRest();
    UINT32 CheckSoftRestFusesSensedHighTemp();
    UINT32 CheckSoftRestFusesSensedLowTemp();
    UINT32 CheckHotRest();
    UINT32 CheckHotRestFusesSensedHighTemp();
    UINT32 CheckHotRestFusesSensedLowTemp();
    UINT32 gpu_initialize();
    UINT32 temperature2code_final(INT32 temperature);
    UINT32 temperature2code_nominal(INT32 temperature);
    void   enable_gr_pfifo_engine();
    void   SCI_initial_programming(UINT32 s);
    UINT32 SavePciReg(UINT32 gen);
    UINT32 RestorePciReg(UINT32 gen);
private:
    IRegisterMap* m_regMap;
    UINT32 temp126_rawcode;
    UINT32 temp94_rawcode;
    UINT32 temp94_final_rawcode;
    UINT32 data;
    UINT32 final_a;
    UINT32 final_b;
    UINT32 nominal_a;
    UINT32 nominal_b;
    UINT32 rdValue;
    UINT32 lw_pmc_enable;
    UINT32 lw_pmc_elpg_enable;
    UINT32 m_callwlated_temp;
    UINT32 m_ts_adc_raw_code;
    UINT32 m_time_period;
    UINT32 m_overt_enable;
    UINT32 m_threshold_temp;
    UINT32 m_polling_period;
    UINT32 m_stable_period;
    UINT32 m_stable_scale;
    UINT32 m_failsafe_reg;
    UINT32 m_latchovert_reg;
    UINT32 m_THERM_SHUTDOWN_LATCH_ENABLE;
    UINT32 m_thresh_time_scale;
    UINT32 m_A_nom;
    UINT32 m_B_nom;
    UINT32 m_temp_alert_hi;
    UINT32 m_temp_alert_override;
    UINT32 m_temp_alert_force;
    UINT32 m_fan_cfg1_hi;
    UINT32 m_fan_cfg1_update;
    UINT32 m_gc6_island;
    UINT32 m_xtal_core_clamp;
    UINT32 m_ts_clk;
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
//PCIE Gen4 Regs
    UINT32 devctrl  ;
    UINT32 misc_1   ;
    UINT32 irqSet   ;
    UINT32 vendid   ;
    UINT32 bar1lo   ;
    UINT32 bar1hi   ;
    UINT32 bar2lo   ;
    UINT32 bar2hi   ;

    UINT32 pci_domain_num; 
    UINT32 pci_bus_num ;
    UINT32 pci_dev_num ;
    UINT32 pci_func_num ; 


    

};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(failsafe_overt, FailSafeOvertPin, "GPU FailSafeOvertPin Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &failsafe_overt_testentry
#endif

#endif  // _OVERTPIN_H_

