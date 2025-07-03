/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef __riscvdemo__TESTHARNESS_H
#define __riscvdemo__TESTHARNESS_H
#include <lwtypes.h>
#include <liblwriscv/io.h>
#include <lwriscv/peregrine.h>
#include LWRISCV64_MANUAL_ADDRESS_MAP

#define PET_WATCHDOG(x) localWrite(LW_PRGNLCL_FALCON_WDTMRVAL, (x))

#define FAIL_MEOW_PRINT ({ \
printf("If you can see this, the RISCV core failed stress test.\n" \
        "  |\\      _,,,--,,_ _,)  \n" \
        " / x`.-'`DEAD-CAT;-;;'    To get rid of this, FIX your hardware (or test)!\n" \
        "< x <  ) )-,_ ) /\\       \n" \
        " `--''(_/--' (_/-'       \n"); \
})

#define FAIL_MEOW(x) do { \
        printf((x)); \
        FAIL_MEOW_PRINT; \
        ICD_HALT \
    } while (0)

#define IMEM_START ((void*)LW_RISCV_AMAP_IMEM_START)
#define DMEM_START ((void*)LW_RISCV_AMAP_DMEM_START)

#define MEMRW_GADGET_CTRL LW_PRGNLCL_FALCON_MAILBOX0
#define MEMRW_GADGET_STATUS LW_PRGNLCL_FALCON_MAILBOX1

// Default watchdog timer is 1 second (or more precisely, 1.024 seconds)
//#define WATCHDOG_TIMER_TIMEOUT_1S 1000000
//#define WATCHDOG_TIMER_TIMEOUT_SOME_SECONDS 1340049
//#define WATCHDOG_TIMER_TIMEOUT_SOME_SECONDS  2340049
#define WATCHDOG_TIMER_TIMEOUT_SOME_SECONDS 10000000

#endif
