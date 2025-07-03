#pragma once
#include <lwtypes.h>
#include "libos_status.h"

void __attribute__((noreturn)) SeparationKernelPartitionSwitch
(
    LwU64 param1,
    LwU64 param2,
    LwU64 param3,
    LwU64 param4,
    LwU64 param5,
    LwU64 partition_id
);

LibosStatus SeparationKernelPartitionCall
(
    LwU64 partition_id,
    LwU64 arguments[5]
);

void SeparationKernelEntrypointSet(LwU64 targetPartitionId, LwU64 entryPointAddress);
void SeparationKernelTimerSet(LwU64 mtimecmp);

#define PMP_ACCESS_MODE_EXELWTE 4
#define PMP_ACCESS_MODE_WRITE   2
#define PMP_ACCESS_MODE_READ    1

void SeparationKernelPmpMap(
    LwU64         localPmpIndex,
    LwU64         targetPartitionId,
    LwU64         targetPmpEntryIndex,
    LwU64         pmpAccessMode,
    LwU64         address,
    LwU64         size);

void __attribute__((noreturn)) SeparationKernelShutdown();

inline LwU64 SeparationKernelEncodeNapot(LwU64 base, LwU64 size)
{
    return (base | ((size - 1) >> 1)) >> 2;
}

extern volatile LwBool StartInContinuation;
void SeparationKernelPartitionContinuation(
    LwU64 a0,
    LwU64 a1,
    LwU64 a2,
    LwU64 a3,
    LwU64 a4,
    LwU64 a5,
    LwU64 a6,
    LwU64 a7
);
