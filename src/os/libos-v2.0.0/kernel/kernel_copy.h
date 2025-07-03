/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#pragma once
#include "kernel.h"
#include "task.h"

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN LIBOS_NAKED void
kernel_port_copy_and_continue(LwU64 destination, LwU64 source, LwU64 size);

void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN
kernel_handle_copy_fault(LwU64 a0, LwU64 a1, LwU64 a2, LwU64 a3, libos_task_t *copy_context);
