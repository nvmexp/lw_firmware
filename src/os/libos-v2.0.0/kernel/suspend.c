/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "suspend.h"
#include "gdma.h"
#include "interrupts.h"
#include "mmu.h"
#include "profiler.h"
#include "task.h"

LIBOS_SECTION_TEXT_COLD void kernel_prepare_for_suspend(void)
{
    kernel_profiler_save_counters();

    resume_task = self;

    kernel_mmu_prepare_for_processor_suspend();

    fence_rw_rw();

    kernel_report_interrupt(LIBOS_INTERRUPT_PROCESSOR_SUSPENDED);
}
