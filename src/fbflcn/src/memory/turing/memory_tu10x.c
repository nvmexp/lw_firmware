/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>
#include "fbflcn_helpers.h"
#include "fbflcn_defines.h"

// include manuals
#include "dev_fbfalcon_csb.h"
#include "dev_pri_ringstation_fbp.h"
#include "dev_fbpa.h"
#include "dev_pri_ringstation_fbp.h"
#include "dev_pwr_pri.h"
#include "dev_disp.h"
#include "dev_fuse.h"

#include <falcon-intrinsics.h>

#include "fbflcn_access.h"
#include "fbflcn_table.h"
#include "memory.h"
#include "osptimer.h"
#include "config/g_memory_hal.h"

#include "priv.h"

#define LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH        0:0
#define LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_READY  0x1
#define LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_WAIT   0x0

// ok to switch timeout should span over at least 1 vsync on even the slowest display, set this to 1/20 second for now
// See bug 2169913 for definition of timeout, lwrrently set at 45.478 us to accomodate a min 23Hz refresh rate
#define SEQ_VBLANK_TIMEOUT_NS ((45478) * 1000)

/*
 * memoryWaitForDisplayOkToSwitch_TU10X
 *
 * the ok to switch signal requires display to be out of reset, enabled and set-up properly
 * that step is not available for boot time training so we only wait for ok to switch
 * in the mclk switch binary
 */

GCC_ATTRIB_SECTION("memory", "memoryWaitForDisplayOkToSwitch_TU10X")
void
memoryWaitForDisplayOkToSwitch_TU10X
(
        void
)
{
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_CORE_RUNTIME))

    FLCN_TIMESTAMP  waitTimerNs = { 0 };
    osPTimerTimeNsLwrrentGet(&waitTimerNs);

    //
    // detect presence of display
    //
    LwBool displayIsPresent = ( REF_VAL(LW_FUSE_STATUS_OPT_DISPLAY_DATA, REG_RD32(BAR0, LW_FUSE_STATUS_OPT_DISPLAY)) == LW_FUSE_CTRL_OPT_DISPLAY_DATA_ENABLE);

    if (displayIsPresent)
    {
        //
        // In VGA mode we poll buffer loading in order to get a bigger time slow for the 1st mclk switch, this should fix tearing issues
        // on boot screen.  Bug 2323654
        //
        LwBool vgaOwner = (REF_VAL(LW_PDISP_FE_CORE_HEAD_STATE_SHOWING_ACTIVE, REG_RD32(BAR0, LW_PDISP_FE_CORE_HEAD_STATE(0))) == LW_PDISP_FE_CORE_HEAD_STATE_SHOWING_ACTIVE_VBIOS);
        if (vgaOwner)
        {

            LwBool timeout = LW_FALSE;

            // wait to make sure vga is alreday in fetch or about to get started
            LwU32 statusVga;
            do {
                statusVga = REF_VAL(LW_PDISP_IHUB_WINDOW_DEBUG_STATUS_CMVGASM, REG_RD32(BAR0, LW_PDISP_IHUB_WINDOW_DEBUG_STATUS(0)));
                timeout = (osPTimerTimeNsElapsedGet(&waitTimerNs) >= SEQ_VBLANK_TIMEOUT_NS);
            }
            while ((statusVga == LW_PDISP_IHUB_WINDOW_DEBUG_STATUS_CMVGASM_WAIT_FOR_RDOUT_DONE) && (!timeout));

            // make sure that all the requests for a VGA frame (Gfx or Text mode) have been sent out and no more requests pending inside isohub
            do {
                statusVga = REF_VAL(LW_PDISP_IHUB_WINDOW_DEBUG_STATUS_CMVGASM, REG_RD32(BAR0, LW_PDISP_IHUB_WINDOW_DEBUG_STATUS(0)));
                timeout = (osPTimerTimeNsElapsedGet(&waitTimerNs) >= SEQ_VBLANK_TIMEOUT_NS);
            }
            while ((statusVga != LW_PDISP_IHUB_WINDOW_DEBUG_STATUS_CMVGASM_WAIT_FOR_RDOUT_DONE) && (!timeout));

            // wait till there's no fetch request outstanding
            LwU32 outstandingReq;
            do {
                outstandingReq = REF_VAL(LW_PDISP_IHUB_COMMON_FB_BLOCKER_CTRL_OUTSTANDING_REQ, REG_RD32(BAR0, LW_PDISP_IHUB_COMMON_FB_BLOCKER_CTRL));
                timeout = (osPTimerTimeNsElapsedGet(&waitTimerNs) >= SEQ_VBLANK_TIMEOUT_NS);
            }
            while ((outstandingReq != 0) && (!timeout));

        }

        // in all other modes we wait for ok_to switch
        else
        {
            LwU32 mb0;
            LwU8 ok_to_switch = LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_WAIT;
            do {
                mb0 = REG_RD32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(0));
                ok_to_switch = REF_VAL(LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH,mb0);
            }
            while ( (ok_to_switch != LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_READY) &&
                    (osPTimerTimeNsElapsedGet(&waitTimerNs) < SEQ_VBLANK_TIMEOUT_NS) );

        }
    }

#endif

    // halt on timeout
    //   display team requested to continue with mclk switch after a tieout (bug 2169913)
    //   so we no longer halt here.

}

/*
 * stopFb
 *
 * legacy function directly accessing the gpio pins in pmu
 * this does not program the host registers to clean out traffic.
 *
 */
GCC_ATTRIB_SECTION("memory", "memoryStopFb_TU10X")
void
memoryStopFb_TU10X
(
    LwU8            waitForDisplay,
    PFLCN_TIMESTAMP pStartFbStopTimeNs
)
{
    if (waitForDisplay)
    {
        memoryWaitForDisplayOkToSwitch_HAL();
    }

    LwU32 cmd_stop_fb = 0x0;
    cmd_stop_fb = FLD_SET_DRF(_PPWR, _PMU_GPIO_OUTPUT, _SET_FB_STOP_REQ, _TRIGGER, cmd_stop_fb);

    REG_WR32_STALL(LOG, LW_PPWR_PMU_GPIO_OUTPUT_SET, cmd_stop_fb);

    LwU32 ack_stop_fb = 0;
    while (ack_stop_fb != LW_PPWR_PMU_GPIO_INPUT_FB_STOP_ACK_TRUE)
    {
        ack_stop_fb = REG_RD32(LOG, LW_PPWR_PMU_GPIO_INPUT);
        ack_stop_fb = REF_VAL(LW_PPWR_PMU_GPIO_INPUT_FB_STOP_ACK, ack_stop_fb);
    }


    // Record the time immediately after the FB stop is asserted
    osPTimerTimeNsLwrrentGet(pStartFbStopTimeNs);
}


GCC_ATTRIB_SECTION("memory", "memoryStartFb_TU10X")
LwU32
memoryStartFb_TU10X
(
    PFLCN_TIMESTAMP pStartFbStopTimeNs
)
{
    // Send a s/w refresh as a workaround for clk gating bug
    // This should keep the clocks running
    REG_WR32_STALL(LOG, LW_PFB_FBPA_REF, 0x00000001);

    LwU32        cmd_start_fb = 0x0;
    cmd_start_fb = FLD_SET_DRF(_PPWR, _PMU_GPIO_OUTPUT, _CLEAR_FB_STOP_REQ, _CLEAR, cmd_start_fb);
    REG_WR32_STALL(LOG, LW_PPWR_PMU_GPIO_OUTPUT_CLEAR, cmd_start_fb);

    LwU32 ack_start_fb = 0x01;
    while (ack_start_fb != LW_PPWR_PMU_GPIO_INPUT_FB_STOP_ACK_FALSE)
    {
        ack_start_fb = REG_RD32(LOG, LW_PPWR_PMU_GPIO_INPUT);
        ack_start_fb = REF_VAL(LW_PPWR_PMU_GPIO_INPUT_FB_STOP_ACK, ack_start_fb);
    }

    // Find out the time elapsed for which FB stop was asserted
    LwU32        fbStopTimeNs = 0;
    fbStopTimeNs = osPTimerTimeNsElapsedGet(pStartFbStopTimeNs);
    return fbStopTimeNs;
}

//
// getGPIOIndexForFBVDDQ
//   find the GPIO index for the FB VDDQ GPIO control. This index is can be read from the
//   PMCT Header as a copy of the gpio tables.
//
LwU8
memoryGetGpioIndexForFbvddq_TU10X
(
        void
)
{
    PerfMemClk11Header* pMCHp = gBiosTable.pMCHp;
    LwU8 perfFbVDDQIndex = 0;
    // check that the gpio addition is available in the perf mem clk table header
    if (pMCHp->HeaderSize >= SIZE_OF_PERF_MEM_CLK11HEADER_TU10X)
    {
        LwU32 perfMClkTableGPIOIndex = TABLE_VAL(pMCHp->PerfMClkTableGPIOIndex);
        perfFbVDDQIndex = (perfMClkTableGPIOIndex & 0x000000ff);
    }
    else
    {
        // hard coded value stayed as backup for turing, it will removed by the
        // next hal implementation
        perfFbVDDQIndex = 8;
    }
    return perfFbVDDQIndex;
}

//
// getGPIOIndexForVref
//   find the GPIO index for the VREF GPIO control. This index is can be read from the
//   PMCT Header as a copy of the gpio tables.
//
LwU8
memoryGetGpioIndexForVref_TU10X
(
        void
)
{
    PerfMemClk11Header* pMCHp = gBiosTable.pMCHp;
    LwU8 perfVrefIndex = 0;
    // check that the gpio addition is available in the perf mem clk table header
    if (pMCHp->HeaderSize >= SIZE_OF_PERF_MEM_CLK11HEADER_TU10X)
    {
        LwU32 perfMClkTableGPIOIndex = TABLE_VAL(pMCHp->PerfMClkTableGPIOIndex);
        perfVrefIndex = (perfMClkTableGPIOIndex & 0x0000ff00) >> 8;
    }
    else
    {
        // hard coded value stayed as backup for turing, it will removed by the
        // next hal implementation
        perfVrefIndex = 10;
    }
    return perfVrefIndex;
}
