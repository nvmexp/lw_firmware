#pragma once
#include <lwtypes.h>

typedef void __attribute__((noreturn)) (*SwitchStackCallback)(LwU64 a0, LwU64 a1, LwU64 a2, LwU64 a3, LwU64 a4, LwU64 a5, LwU64 a6);

void LIBOS_NORETURN KernelSchedulerSwitchToStack
(
    LwU64 a0,
    LwU64 a1,
    LwU64 a2,
    LwU64 a3,
    LwU64 a4,
    LwU64 a5,
    LwU64 a6,
    SwitchStackCallback cb
);