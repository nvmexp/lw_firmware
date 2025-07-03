/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef PMU_FMC_PARTITIONS_H
#define PMU_FMC_PARTITIONS_H

#include <lwtypes.h>
#include <sbilib/sbilib.h>

#define PARTITION_ID_RTOS 0

#ifdef PMU_TEGRA_ACR_PARTITION
// This file defines PARTITION_ID_TEGRA_ACR==1
# include "acr/task_acr.h"
#endif // PMU_TEGRA_ACR_PARTITION

static inline void partitionSwitchNoreturn(LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4, LwU64 partition) __attribute__((noreturn));
static inline void partitionSwitchNoreturn(LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4, LwU64 partition)
{
    sbicall6(SBI_EXTENSION_LWIDIA,
             SBI_LWFUNC_FIRST,
             arg0,
             arg1,
             arg2,
             arg3,
             arg4,
             partition);

    __builtin_unreachable();
}

#endif // PMU_FMC_PARTITIONS_H
