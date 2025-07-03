#include "mdiag/tests/stdtest.h"
#include "core/include/rc.h"
#include "gc6plus_maxwell.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/utils/crc.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/utility.h"
#include "core/include/gpu.h"
#include "lwmisc.h"
#include "gpio_engine.h"
#include "gpu/include/gpusbdev.h"
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include "lw_ref_plus_dev_gc6_island.h"

//Includes for register address defines from //.../drivers/resman/kernel/inc/fermi/
#include "maxwell/gm107/dev_graphics_nobundle.h"
#include "maxwell/gm107/dev_pwr_pri.h"
#include "maxwell/gm107/dev_master.h"
#include "maxwell/gm107/dev_bus.h"
#include "maxwell/gm107/dev_pri_ringmaster.h"
#include "maxwell/gm107/dev_pri_ringstation_sys.h"
#include "maxwell/gm107/dev_xbar.h"
#include "maxwell/gm107/dev_top.h"
#include "maxwell/gm107/dev_fb.h"
#include "maxwell/gm107/dev_therm.h"
#include "maxwell/gm107/dev_ltc.h"
#include "maxwell/gm107/dev_fbpa.h"
#include "maxwell/gm107/dev_lw_xve.h"
#include "maxwell/gm107/dev_gc6_island.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
//#include "golden_gc5_exit.h"//Uncomment for new power sequencer flow
//#include "golden_gc6_exit.h"//Uncomment for new power sequencer flow
// #include "golden_gc6plus_seq_default.h"//Uncomment for new power sequencer flow

int GC6plus_Maxwell::gc6plus_StdWakeupTest(func_pointer extra_init, func_pointer main_body, func_pointer extra_checks){
    int extra_init_result=0, main_body_result=0, extra_checks_result=0;
    GpuSubdevice *pSubdev = this->lwgpu->GetGpuSubdevice();

    // Init functions
    this->gc6plus_init(pSubdev);

    DebugPrintf("Running extra_init\n");
    extra_init(this, pSubdev);
    DebugPrintf("extra_init_result\n %d", extra_init_result);

    // Print enabled wakeups
    this->printWakeEnables(pSubdev);

    DebugPrintf("Running main_body\n");
    main_body(this, pSubdev);
    DebugPrintf("main_body_result\n %d", main_body_result);

    DebugPrintf("Running extra_checks\n");
    extra_checks(this, pSubdev);
    DebugPrintf("extra_checks_result\n %d", extra_checks_result);

    //Cleanup functions (with priv_init)
    this->gc6plus_cleanup(pSubdev);
    return ((
                this->check_sw_intr_status(pSubdev) //Check Pending Wakeup Interrupts and GC5/GC6 Exit/Abort flags
             && this->checkSteadyState(pSubdev) //we are now in steady state
         && checkTimers(pSubdev) //Check GC5/GC6 Entry, Resident, and Exit Timers
             && this->checkDebugTag(pSubdev) //Check BSI2PMU PRIV Write
         ) ? 0: 1);
}

int func_pointer::operator() (LWGpuSingleChannelTest *,
                              GpuSubdevice *){
    return 1;
}

class c_program_timer_wakeup: public func_pointer{
    int wakeup_timer_us;
public:
    c_program_timer_wakeup(int timer_us): wakeup_timer_us(timer_us){}
    int operator ()(GC6plus_Maxwell *test_obj,
        GpuSubdevice *pSubdev){
        test_obj->configure_wakeup_timer(pSubdev, wakeup_timer_us);
        test_obj->wakeupTimerEvent(pSubdev, "enable");
        return 1;
    }
};

class main_body_delay: public func_pointer{
    int delay_us;
public:
    main_body_delay(int d): delay_us(d){ }
    int operator () (GC6plus_Maxwell *test_obj,
            GpuSubdevice *pSubdev){
            Platform::Delay(delay_us);
            return 1;
        }
};

// UINT32 GC6plus_Maxwell::gc6plus_wakeupTest_mod(){
//     int g_wakeup_timer_us = 20;
//     // GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

//     return this->gc6plus_StdWakeupTest(
//      c_program_timer_wakeup(g_wakeup_timer_us),
//      main_body_delay(20),
//      func_pointer());
// }

void GC6plus_Maxwell::EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value){
    if (Platform::GetSimulationMode() == Platform::RTL){
        DebugPrintf("B-EscapeWrite \n");
        Platform::EscapeWrite(Path, Index, Size, Value);
    }
    else{
        DebugPrintf("Unsupported Platform for EscapeWrite %s\n", Path);
    }
}

int GC6plus_Maxwell::EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value){
    if (Platform::GetSimulationMode() == Platform::RTL){
        DebugPrintf("B-EscapeRead \n");
        return Platform::EscapeRead(Path, Index, Size, Value);
    }
    else{
        DebugPrintf("Unsupported Platform for EscapeWrite %s\n", Path);
        return 0;
    }
}
//Uncomment for new power sequencer flow
// This routine takes an array of (wr_addr, wr_data) pairs and performs priv writes.  The number of addr/data pairs is indicated in count.
int GC6plus_Maxwell::writePrivRegs(const unsigned int (*priv_config)[2], unsigned int count, GpuSubdevice *pSubdev) {
    for (unsigned int index = 0; index < count; index++) {
        pSubdev->RegWr32(priv_config[index][0], priv_config[index][1]);
    }
    return (0);
}
//Configures the Power Sequencer for SCI
// void GC6plus_Maxwell::configurePowerSequencer(GpuSubdevice *pSubdev, ModeType mode){
//     if(mode == GC5 || mode == GC6) {
//         writePrivRegs(golden_gc6plus_seq_default_pwr_seq_cfg, sizeof (golden_gc6plus_seq_default_pwr_seq_cfg) / sizeof (* golden_gc6plus_seq_default_pwr_seq_cfg), pSubdev);
//     }
//     /*if(mode == GC5) {
//         writePrivRegs(golden_gc5_exit_pwr_seq_cfg, sizeof (golden_gc5_exit_pwr_seq_cfg) / sizeof (* golden_gc5_exit_pwr_seq_cfg), pSubdev);
//     } else if(mode == GC6) {
//         writePrivRegs(golden_gc6_exit_pwr_seq_cfg, sizeof (golden_gc6_exit_pwr_seq_cfg) / sizeof (* golden_gc6_exit_pwr_seq_cfg), pSubdev);
//     }*/ else if(mode == STEADY) {
//         InfoPrintf("Steady State mode: Skipping Power Sequencer Configuration\n");
//     } else{
//         ErrPrintf("Undefined mode: Skipping Power Sequencer Configuration\n");
//     }
// }

