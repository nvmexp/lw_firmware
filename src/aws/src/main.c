 /* _LWRM_COPYRIGHT_BEGIN_
 *  
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 * 
 * _LWRM_COPYRIGHT_END_
 * */
#include <falcon-intrinsics.h>
#include "lwmisc.h"

/*
 * This code is expected to be loaded into every falcon core and ensure MAILBOX0 register (or the one in csbAddr) gets updated with csbData.
 * RISCV cores with its own cache can run without TCM and just randomizing TCM writes will not be enough to ensure core is not being used.
 * The pythong script loading this ucode is expected to patch csbAddr and csbData. MAILBOX0 is preferred for csbAddr.
 * Main function is aligned to 0x100 to avoid loading _start which needs SP to be updated. Without _start, stack is not used and script gets
 * more DMEM range for its randomization logic.
 */

LwU32          csbAddr = 0xdead1234;
volatile LwU32 csbData = 0xbeef5678;
#define STOPCODE 0xdead9999
#define HALTCODE 0xdead8888

int __attribute__((aligned(256))) main(void)
{
    falc_wspr(CSW, 0x30001); // Enables bit 0 and IE flags. This is required for wait instruction to stop and wait for intr/startcpu.
    while (1)
    {
       if (csbData == STOPCODE)
       {
           falc_wait(0);
           continue;
       }
       else if (csbData == HALTCODE)
       {
           falc_halt();
       }
       falc_stxb((LwU32 *)csbAddr, csbData);
    }
    return 0;
}
