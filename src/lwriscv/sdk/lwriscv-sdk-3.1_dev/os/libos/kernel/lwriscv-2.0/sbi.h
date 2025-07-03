#include <lwtypes.h>

void __attribute__((noreturn)) KernelSbiPartitionSwitch
(
    LwU64 param1,
    LwU64 param2,
    LwU64 param3,
    LwU64 param4,
    LwU64 param5,
    LwU64 partition_id
);
void KernelSbiTimerSet(LwU64 mtimecmp);
void __attribute__((noreturn)) KernelSbiShutdown();
