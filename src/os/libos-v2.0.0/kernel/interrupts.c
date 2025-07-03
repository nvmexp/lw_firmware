/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "interrupts.h"
#include "kernel.h"
#include <dev_falcon_v4.h>

LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void panic_kernel()
{
    kernel_report_interrupt(LIBOS_INTERRUPT_KERNEL_PANIC);

    while (LW_TRUE)
    {
    }
}

LIBOS_SECTION_IMEM_PINNED void kernel_report_interrupt(LwU32 interruptMask)
{
    falcon_write(LW_PFALCON_FALCON_MAILBOX0, falcon_read(LW_PFALCON_FALCON_MAILBOX0) | interruptMask);
}