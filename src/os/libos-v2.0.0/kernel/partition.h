/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#pragma once

#include "kernel.h"

#if LIBOS_CONFIG_PARTITION

LIBOS_SECTION_TEXT_COLD void kernel_partition_save_bootstrap_tcm(const libos_bootstrap_args_t *);
LIBOS_SECTION_TEXT_COLD void kernel_partition_restore_bootstrap_tcm(void);
LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void kernel_syscall_partition_switch(
    LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 from_partition, LwU64 to_partition);

#endif
