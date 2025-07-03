#include <falcon-intrinsics.h>
#include <falc_debug.h>
#include <falc_trace.h>

#include "fbflcn_dummy_mclk_switch.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_table.h"
#include "fbflcn_access.h"
#include "fbflcn_mutex.h"
#include "priv.h"
#include "osptimer.h"

#include "memory.h"
#include "fbflcn_objfbflcn.h"
#include "config/g_memory_hal.h"

#include "dev_fbfalcon_csb.h"

#include "dev_top.h"

extern LwU8 gPlatformType;



GCC_ATTRIB_SECTION("memory", "doMclkSwitchPrimary")
LwU32
doDummyMclkSwitch
(
        LwU16 targetFreqMHz
)
{
    LwU32 fbStopTimeNs = 0;
    FLCN_TIMESTAMP  startFbStopTimeNs = { 0 };

    memoryWaitForDisplayOkToSwitchVal_HAL(&Fbflcn,LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_WAIT);

    // simulate a delay for 250us as the max exelwtion time for a regular mclk switch
    // in emulation the timing normal is slower so the timer is scaled to account for that
    LwU32 fakeSwitchTime = 250;
    if (gPlatformType == LW_PTOP_PLATFORM_TYPE_EMU) {
        fakeSwitchTime = 13;
    }

    // In case of fmodel and emulation we just do a dummy mclk switch with fb_stop / fb_start but no
    // content
    memoryStopFb_HAL(&Fbflcn, 1, &startFbStopTimeNs);
    OS_PTIMER_SPIN_WAIT_US(fakeSwitchTime);
    fbStopTimeNs = memoryStartFb_HAL(&Fbflcn, &startFbStopTimeNs);
    return fbStopTimeNs;
}
