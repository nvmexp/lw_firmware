/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>

#include "config/fbfalcon-config.h"

#include "dev_fbfalcon_csb.h"
//#include "dev_fbfalcon_pri.h"
#include "fbflcn_defines.h"
#include "fbflcn_interrupts.h"

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_CMDQUEUE_SUPPORT))
#include "fbflcn_queues.h"
#endif  // FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_CMDQUEUE_SUPPORT)

#include "fbflcn_timer.h"
#include "fbflcn_helpers.h"
#include "priv.h"

#include "fbflcn_objfbflcn.h"
#include "config/g_fbfalcon_private.h"

#include "lwuproc.h"

#if (!FBFALCONCFG_FEATURE_ENABLED(PAFALCON_INTERRUPT))
volatile LwU8 gTimerEnabled = 0;
#endif


/*
 * IV0_routine
 * Interrupt handler entry point for IV0
 */

GCC_ATTRIB_NO_STACK_PROTECT() GCC_ATTRIB_SECTION("resident", "interrupt")
void
IV0_routine
(
        void
)
{  //__attribute__ "naked";
    asm volatile ( "push a9; push a10; push a11; push a12; push a13; push a14; push a15;");
    asm volatile ( "rspr a10 CSW; push a10;");
    asm volatile ( "call _IV0_routine_entryC;");
    asm volatile ( "pop a10; wspr CSW a10;");
    asm volatile ( "pop a15; pop a14; pop a13; pop a12; pop a11; pop a10; pop a9;");
    asm volatile ( "reti; ");
}


/*
 * IV1_routine
 * Interrupt handler entry point for IV1
 */

GCC_ATTRIB_NO_STACK_PROTECT() GCC_ATTRIB_SECTION("resident", "interrupt")
void
IV1_routine
(
        void
)
{  //__attribute__ "naked";
    asm volatile ( "push a9; push a10; push a11; push a12; push a13; push a14; push a15;");
    asm volatile ( "rspr a10 CSW; push a10;");
    asm volatile ( "call _IV1_routine_entryC;");
    asm volatile ( "pop a10; wspr CSW a10;");
    asm volatile ( "pop a15; pop a14; pop a13; pop a12; pop a11; pop a10; pop a9;");
    asm volatile ( "reti; ");
}


/*
 * EV_routine
 * Error handler entry point
 */

GCC_ATTRIB_NO_STACK_PROTECT() GCC_ATTRIB_SECTION("resident", "interrupt")
void
EV_routine
(
        void
)
{  //__attribute__ "naked";
    asm volatile (" sclrb 24;");
    asm volatile ( "push a9; push a10; push a11; push a12; push a13; push a14; push a15;");
    asm volatile ( "rspr a10 CSW; push a10;");
    asm volatile ( "call _EV_routine_entryC;");
    asm volatile ( "pop a10; wspr CSW a10;");
    asm volatile ( "pop a15; pop a14; pop a13; pop a12; pop a11; pop a10; pop a9;");
    asm volatile ( "reti; ");
}


/*
 * FalconInterruptHandler(void)
 * Main Falcon interrupt handler, takes care of interrupt controll registers,
 */

GCC_ATTRIB_SECTION("resident", "interrupt")
void
IV0_routine_entryC
(
        void
)
{
    LwU32 status = REG_RD32(CSB, LW_CFBFALCON_FALCON_IRQSTAT);
    LwU32 status_ext = REF_VAL(LW_CFBFALCON_FALCON_IRQSTAT_EXT,status);

    LwU8 processed = 0;

    //*
    //* EXTERNAL INTERRUPTS
    //*  decodes the external hw wired interrupts to the fbalcon core
    //*
    if (status_ext != 0)
    {


#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_CMDQUEUE_SUPPORT))
        // EXTERNAL_INTERRUPTS
        // priority = 2, process cmd queue interrupts
        if (REF_VAL(HW_INTERRUPT_BIT_CMD_TRIGGER,status) > 0)
        {
            fbflcnQInterruptHandler();
            processed = 1;
        }
#endif  // FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_CMDQUEUE_SUPPORT)


    }  // end of external interrupts

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_INTERRUPT))
    void paflcnInterruptHandler(void);
    paflcnInterruptHandler();   // defined in paflcn/main.c
#else
    if ((status_ext != 0) && (REF_VAL(HW_INTERRUPT_BIT_TIMER_TRIGGER,status) > 0))
    {
        // execute the timer interrupt handler
        fbflcnTimerInterruptHandler();
        processed = 1;

        if (gTimerEnabled != 0)
        {
            // restart the timer with the next shot.
            LwU32 ctrl;
            ctrl = REG_RD32(CSB, LW_CFBFALCON_TIMER_CTRL);
            ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_GATE,_STOP, ctrl);
            REG_WR32_STALL(CSB, LW_CFBFALCON_TIMER_CTRL,ctrl);
            ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_GATE,_RUN, ctrl);
            REG_WR32_STALL(CSB, LW_CFBFALCON_TIMER_CTRL,ctrl);
        }
        // disable irtrstat
        REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_IRQSCLR, FLD_SET_DRF(_CFBFALCON,_FALCON_IRQSCLR,_EXT_EXTIRQ4,_SET,0x0));
    }
#endif

    if (processed == 0) {
#if (FBFALCONCFG_FEATURE_ENABLED(DEBUG_OUTPUT))
        FW_MBOX_WR32(13, status);
#endif
        fbfalconIllegalInterrupt_HAL();
    }
}

GCC_ATTRIB_SECTION("resident", "interrupt")
void
IV1_routine_entryC
(
        void
)
{
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    FBFLCN_HALT(FBFLCN_ERROR_CODE_UNRESOLVED_INTERRUPT);
#endif
}

//
// a miss in imem due to improper loading will trigger the error interrupt handler
//
GCC_ATTRIB_SECTION("resident", "interrupt")
void
EV_routine_entryC
(
        void
)
{
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    FBFLCN_HALT(FBFLCN_ERROR_CODE_UNRESOLVED_INTERRUPT);
#endif
}

void
FalconInterruptSetup
(
        void
)
{

#if (!FBFALCONCFG_FEATURE_ENABLED(PAFALCON_INTERRUPT))
    FalconInterruptDisablePeriodicTimer();
#endif

    falc_wspr(IV0, IV0_routine);
    falc_wspr(IV1, IV1_routine);
    falc_wspr(EV, EV_routine);
    falc_lwst_ssetb(0);

    // clear intr status
    REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_IRQSCLR,0xFFFFFFFF);

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_INTERRUPT))
    // set PA interrupt destination to falcon[ exterr(irq0) ]
    REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_IRQDEST,0x00000000 |
//            DRF_DEF(_CFBFALCON,_FALCON_IRQDEST,_HOST_HALT,_FALCON) |
//            DRF_DEF(_CFBFALCON,_FALCON_IRQDEST,_HOST_SWGEN0,_FALCON) |
//            DRF_DEF(_CFBFALCON,_FALCON_IRQDEST,_HOST_SWGEN1,_FALCON) |
            DRF_DEF(_CFBFALCON,_FALCON_IRQDEST,_HOST_EXTERR,_FALCON)
    );
#else
    // set interrupt destination w/ HALT & SWGEN interrupts going to host
    REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_IRQDEST,0x00000000 |
            DRF_DEF(_CFBFALCON,_FALCON_IRQDEST,_HOST_HALT,_HOST) |
            DRF_DEF(_CFBFALCON,_FALCON_IRQDEST,_HOST_SWGEN0,_HOST) |
            DRF_DEF(_CFBFALCON,_FALCON_IRQDEST,_HOST_SWGEN1,_HOST) |
            DRF_DEF(_CFBFALCON,_FALCON_IRQDEST,_HOST_EXTERR,_HOST)
    );
#endif

    // set swgen and halt as edge triggered interrupt
    REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_IRQMODE,0xFFFFFFFF &
            ~DRF_DEF(_CFBFALCON,_FALCON_IRQMODE,_LVL_SWGEN0,_TRUE) &
            ~DRF_DEF(_CFBFALCON,_FALCON_IRQMODE,_LVL_SWGEN1,_TRUE) &
            ~DRF_DEF(_CFBFALCON,_FALCON_IRQMODE,_LVL_HALT,_TRUE) &
            ~DRF_DEF(_CFBFALCON,_FALCON_IRQMODE,_LVL_EXT_EXTIRQ4,_TRUE) &
            ~DRF_DEF(_CFBFALCON,_FALCON_IRQMODE,_LVL_EXTERR,_TRUE)
    );

    // set irqmask
    //  - enable all interrupts
    //  - disable exterr

#if (!FBFALCONCFG_FEATURE_ENABLED(PAFALCON_INTERRUPT))
    // enable interrupts
    REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_IRQMSET,0xFFFFFFFF);


    // disable exterr interrupt
    //
    // issues with enabling the interrupt is that the priv request inside
    // the interrupt handler falsify the results for the inline priv checks
    // to the point that priv retries become impossible due to race conditions
    // of that code vs the interrupt code.
    //
    // there's a bug with irqmask not working properly at suspend resume,
    // deploying the same WAR
    // https://lwbugswb.lwpu.com/LwBugs5/HWBug.aspx?bugid=200272812&cmtNo=

    while (FLD_TEST_DRF(_CFBFALCON, _FALCON_IRQMASK, _EXTERR, _ENABLE,
            REG_RD32(CSB, LW_CFBFALCON_FALCON_IRQMASK)))
    {
        REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_IRQMCLR, 0x00000000 |
                DRF_DEF(_CFBFALCON,_FALCON_IRQMCLR,_EXTERR,_SET)
        );
    }
#else
    // enable interrupts
    REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_IRQMSET,DRF_DEF(_CFBFALCON,_FALCON_IRQMSET,_EXTERR,_SET));
#endif

    // enable interrupts in CSW
    falc_lwst_ssetb(16);
    falc_lwst_ssetb(17);
    falc_lwst_ssetb(18);

#if (!FBFALCONCFG_FEATURE_ENABLED(PAFALCON_INTERRUPT))
    // setup msg queue interrrupt as SWGEN1 target to host 
    // redirect halt interrupt to host as well
    REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_IRQDEST, 0x00000000
            | DRF_DEF(_CFBFALCON, _FALCON_IRQDEST_HOST, _SWGEN0, _HOST)
            | DRF_DEF(_CFBFALCON, _FALCON_IRQDEST_HOST, _SWGEN1, _HOST)
            | DRF_DEF(_CFBFALCON, _FALCON_IRQDEST_HOST, _HALT, _HOST)
            | DRF_DEF(_CFBFALCON, _FALCON_IRQDEST_HOST, _EXTERR, _HOST)
    );
#endif

    // clear the ok_to_switch bit
    REG_WR32_STALL(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX_CLEAR(0),0x1);
    // clear interrupts stats
    REG_WR32_STALL(CSB, LW_CFBFALCON_MAILBOX0_INTRSTAT, 0xFFFFFFFF);
    // set interrupt enables for ok_to_switch
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    REG_WR32_STALL(CSB, LW_CFBFALCON_MAILBOX0_INTREN, 0x1);
#endif
}


void
maskHaltInterrupt
(
        void
)
{
    while (FLD_TEST_DRF(_CFBFALCON, _FALCON_IRQMASK, _HALT, _ENABLE,
            REG_RD32(CSB, LW_CFBFALCON_FALCON_IRQMASK)))
    {
        REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_IRQMCLR, 0x00000000 |
                DRF_DEF(_CFBFALCON,_FALCON_IRQMCLR,_HALT,_SET)
        );
    }
}

#if (!FBFALCONCFG_FEATURE_ENABLED(PAFALCON_INTERRUPT))

void
FalconInterruptEnablePeriodicTimer
(
        void
)
{
    gTimerEnabled = 1;
}


void
FalconInterruptDisablePeriodicTimer
(
        void
)
{
    gTimerEnabled = 0;
}
#endif
