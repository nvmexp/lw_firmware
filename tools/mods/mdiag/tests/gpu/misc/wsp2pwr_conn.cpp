/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2009, 2020-2021 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first
#include "mdiag/tests/stdtest.h"

#include "wsp2pwr_conn.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "sim/IChip.h"

#define  LW_PPWR_PMU_DEBUG__2        0x0010a5c8

// define which tests are run
static int wsp2pwr_conn_basic=0;

extern const ParamDecl wsp2pwr_conn_params[] =
{
    SIMPLE_PARAM("-wsp2pwr_conn_basic", "sanity test"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

// constructor
wsp2pwr_conn::wsp2pwr_conn(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-wsp2pwr_conn_basic"))
        wsp2pwr_conn_basic = 1;
}

// destructor
wsp2pwr_conn::~wsp2pwr_conn(void)
{
}

// Factory
STD_TEST_FACTORY(wsp2pwr_conn,wsp2pwr_conn)

// setup method
int wsp2pwr_conn::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();
    ch = lwgpu->CreateChannel();
    m_arch = lwgpu->GetArchitecture();
    if(!ch)
    {
        ErrPrintf("wsp2pwr_conn::Setup failed to create channel\n");
        return 0;
    }
    getStateReport()->init("wsp2pwr_conn");
    getStateReport()->enable();

    return 1;
}

//CleanUp
void wsp2pwr_conn::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }
    if (lwgpu)
    {
        DebugPrintf("Gpio::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void wsp2pwr_conn::DelayNs(UINT32 Lwclks)
{
  // Lwrrently this function is in terms of .2nS / lwclk. This is incorrectly done
  // in RTL. This should be a close approx of 1 nanosecond.
  Platform::EscapeWrite("CLOCK_WAIT",IChip::ECLOCK_LWCLK,0,Lwclks * 5);
}

// run - main routine
void wsp2pwr_conn::Run(void)
{

    //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("FLAG : wsp2pwr_conn starting ....\n");

    if(wsp2pwr_conn_basic)
    {
        if (wsp2pwr_conn_basicTest())
        {
        SetStatus(TEST_FAILED);
        ErrPrintf("wsp2pwr_conn::wsp2pwr_conn_basicTest failed\n");
        return;
        }

    InfoPrintf("wsp2pwr_conn_basicTest() Done !\n");
    } //endif_wsp2pwr_conn_basicTest

    InfoPrintf("wsp2pwr_conn test complete\n");
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();

    ch->WaitForIdle();
}//end_Run

int wsp2pwr_conn::wsp2pwr_conn_basicTest()
{
    int errCnt = 0;
    InfoPrintf("wsp2pwr_conn test starts...\n");

//    if (lwgpu->SetRegFldDef("LW_PMC_ENABLE","_PDISP","_ENABLED")) {
//        ErrPrintf("Could not be updated LW_PMC_ENABLE_PDISP to value _ENABLED\n");
//    }

    //host2pmu_fb_blocker_intr, gpio1, bit 15
//    if(lwgpu->SetRegFldDef("LW_PFIFO_FB_IFACE" , "_INTR_OVERRIDE_VAL", "_INIT")) ErrPrintf("Can not update LW_PFIFO_FB_IFACE\n");
//    if(check_gpio_connection( 15 , false, "host2pmu_fb_blocker_intr")) errCnt++;
//
//    if(lwgpu->SetRegFldDef("LW_PFIFO_FB_IFACE" , "_INTR_OVERRIDE_VAL", "_FORCE_HIGH")) ErrPrintf("Can not update LW_PFIFO_FB_IFACE\n");
//    if(check_gpio_connection( 15 , true, "host2pmu_fb_blocker_intr")) errCnt++;
//
//    if(lwgpu->SetRegFldDef("LW_PFIFO_FB_IFACE" , "_INTR_OVERRIDE_VAL", "_INIT")) ErrPrintf("Can not update LW_PFIFO_FB_IFACE\n");
//    if(check_gpio_connection( 15 , false, "host2pmu_fb_blocker_intr")) errCnt++;

    //xve2pmu_bar_blocker_intr, gpio1, bit 16
    //if(lwgpu->SetRegFldDef("LW_XVE_BAR_BLOCKER" , "_INTR_OVERRIDE_VAL", "_NOOP")) ErrPrintf("Can not update LW_XVE_BAR_BLOCKER\n");
    //if(check_gpio_connection( 16 , false, "xve2pmu_bar_blocker_intr")) errCnt++;

    //if(lwgpu->SetRegFldDef("LW_XVE_BAR_BLOCKER" , "_INTR_OVERRIDE_VAL", "_ASSERT")) ErrPrintf("Can not update LW_XVE_BAR_BLOCKER\n");
    //if(check_gpio_connection( 16 , true, "xve2pmu_bar_blocker_intr")) errCnt++;

    //if(lwgpu->SetRegFldDef("LW_XVE_BAR_BLOCKER" , "_INTR_OVERRIDE_VAL", "_NOOP")) ErrPrintf("Can not update LW_XVE_BAR_BLOCKER\n");
    //if(check_gpio_connection( 16 , false, "xve2pmu_bar_blocker_intr")) errCnt++;
//below connections were moved to check_connect.cpp
    //perf2pmu_fb_blocker_intr, gpio1, bit 17
//    if(lwgpu->SetRegFldDef("LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL" , "_INTR_OVERRIDE_VAL", "_FALSE")) ErrPrintf("Can not update LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL\n");
//    if(check_gpio_connection( 17 , false, "perf2pmu_fb_blocker_intr")) errCnt++;
//
//    if(lwgpu->SetRegFldDef("LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL" , "_INTR_OVERRIDE_VAL", "_TRUE")) ErrPrintf("Can not update LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL\n");
//    if(check_gpio_connection( 17 , true, "perf2pmu_fb_blocker_intr")) errCnt++;
//
//    if(lwgpu->SetRegFldDef("LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL" , "_INTR_OVERRIDE_VAL", "_FALSE")) ErrPrintf("Can not update LW_PERF_PMASYS_SYS_FB_BLOCKER_CTRL\n");
//    if(check_gpio_connection( 17 , false, "perf2pmu_fb_blocker_intr")) errCnt++;
//    //hub2pmu_idle, idle1, bit 2
//    set_idle_mask(2);
//    Platform::EscapeWrite("drv_hub2pmu_idle", 0, 1, 0);
//    if(check_idle_connection(false, "hub2pmu_idle")) errCnt++;
//
//    Platform::EscapeWrite("drv_hub2pmu_idle", 0, 1, 1);
//    if(check_idle_connection(true, "hub2pmu_idle")) errCnt++;
//
//    Platform::EscapeWrite("drv_hub2pmu_idle", 0, 1, 0);
//    if(check_idle_connection(false, "hub2pmu_idle")) errCnt++;
//
//    //hub2pmu_all_but_iso_idle, idle1, bit 3
//    set_idle_mask(3);
//    Platform::EscapeWrite("drv_hub2pmu_all_but_iso_idle", 0, 1, 0);
//    if(check_idle_connection(false, "hub2pmu_all_but_iso_idle")) errCnt++;
//
//    Platform::EscapeWrite("drv_hub2pmu_all_but_iso_idle", 0, 1, 1);
//    if(check_idle_connection(true, "hub2pmu_all_but_iso_idle")) errCnt++;
//
//    Platform::EscapeWrite("drv_hub2pmu_all_but_iso_idle", 0, 1, 0);
//    if(check_idle_connection(false, "hub2pmu_all_but_iso_idle")) errCnt++;
//
//    //hubmmu2pmu_idle, idle1, bit 4
//    set_idle_mask(4);
//    Platform::EscapeWrite("drv_hubmmu2pmu_idle", 0, 1, 0);
//    if(check_idle_connection(false, "hubmmu2pmu_idle")) errCnt++;
//
//    Platform::EscapeWrite("drv_hubmmu2pmu_idle", 0, 1, 1);
//    if(check_idle_connection(true, "hubmmu2pmu_idle")) errCnt++;
//
//    Platform::EscapeWrite("drv_hubmmu2pmu_idle", 0, 1, 0);
//    if(check_idle_connection(false, "hubmmu2pmu_idle")) errCnt++;
//    InfoPrintf("wsp2pwr_conn test ends...\n");

    if(errCnt > 0) return 1;

    return 0;
}

void wsp2pwr_conn::set_idle_mask(int val)
{
    int data ;
    InfoPrintf("set_idle_mask, IDLE_MASK = 0x%x, IDLE_MASK_1 = 0x%x\n", val);
    lwgpu->RegWr32 (0x0010a504, 0x0); //LW_PPWR_PMU_IDLE_MASK(0)
    data = lwgpu->RegRd32(0x0010a504);
    if(data != 0) ErrPrintf("Can not update 0x0010a504\n");

    lwgpu->RegWr32 (0x0010aa34, (1 << val)); //LW_PPWR_PMU_IDLE_MASK_1(0)
    data = lwgpu->RegRd32(0x0010aa34);
    if(data != (1 << val)) ErrPrintf("Can not update 0x0010aa34\n");
}

int wsp2pwr_conn::check_idle_connection( bool exp, const char * msg)
{
    UINT32 data;
    wsp2pwr_conn::DelayNs(1000);
    Platform::EscapeRead("idle0_status", 0, 1, &data);
    InfoPrintf("data = %x \n", data);
    if(exp == true) {
        if(data == 1) {
            InfoPrintf("check %s passed, exp is true\n", msg);
        }
        else {
            ErrPrintf("check %s fail, exp is true, but it is not\n", msg);
            return 1;
        }
    }
    else {
        if(data == 0) {
            InfoPrintf("check %s passed, exp is false\n", msg);
        }
        else {
            ErrPrintf("check %s fail, exp is false, but it is not\n", msg);
            return 1;
        }
    }

    return 0;
}

int wsp2pwr_conn::check_gpio_connection(int val, bool exp, const char * msg)
{
    UINT32 data;
    wsp2pwr_conn::DelayNs(1000);
    lwgpu->GetRegFldNum("LW_PPWR_PMU_GPIO_1_INPUT", "", &data);
    if(exp == true) {
        if(data & (1 << val)) {
            InfoPrintf("check %s passed, exp is true\n", msg);
        }
        else {
            ErrPrintf("check %s fail, exp is true, but it is not\n", msg);
            return 1;
        }
    }
    else {
        if((data & (1 << val)) == 0) {
            InfoPrintf("check %s passed, exp is false\n", msg);
        }
        else {
            ErrPrintf("check %s fail, exp is false, but it is not\n", msg);
            return 1;
        }
    }

    return 0;
}

int wsp2pwr_conn::check_mspg_ok(bool exp)
{
    UINT32 data;
    Platform::EscapeRead("ihub2pmu_mspg_ok", 0, 1, &data);
    if(exp == true) {
        if(data == 1) {
            InfoPrintf("check ihub2pmu_mspg_ok passed, exp is true\n");
        }
        else {
            ErrPrintf("check ihub2pmu_mspg_ok fail, exp is true, but it is not\n");
            return 1;
        }
    }
    else {
        if(data == 0) {
            InfoPrintf("check ihub2pmu_mspg_ok passed, exp is false\n");
        }
        else {
            ErrPrintf("check ihub2pmu_mspg_ok fail, exp is false, but it is not\n");
            return 1;
        }
    }

    return 0;

}
