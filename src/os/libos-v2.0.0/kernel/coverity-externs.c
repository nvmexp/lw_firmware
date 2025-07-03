/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"
#include "memory.h"
#include "paging.h"
#include "task.h"

#ifdef __COVERITY__

libos_task_t task_idle_instance, task_init_instance;
LwU8 notaddr_task_by_id_count[];
libos_task_t *task_by_id[];
LwU64 code_paged_begin[], code_paged_end[];
LwU8 notaddr_memory_region_init_arguments_count[];

libos_manifest_init_arg_memory_region_t memory_region_init_arguments[];

#endif
