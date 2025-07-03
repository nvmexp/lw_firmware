/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef SEC2_FMC_PARTITIONS_H
#define SEC2_FMC_PARTITIONS_H

#ifndef __ASSEMBLER__
#include <lwtypes.h>
#include <lwriscv/sbi.h>
#include <liblwriscv/print.h>
#endif // __ASSEMBLER__

// Mark symbol as visible from outside of partition
// For functions, this should be *only* partition entry point
// For other objects, they should be declared in shared section.
#define PARTITION_EXPORT                  __attribute__((visibility("default")))

#ifndef __ASSEMBLER__
#define PARTITION_XSTR(X) PARTITION_STR(X)
#define PARTITION_STR(X) # X
#define PARTITION_NAME_SELF PARTITION_XSTR(PARTITION_NAME)

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

    while (1); // MK TODO: to mute noreturn complaining
}

// FWD decl of asm partition switch code, you need to build partctxasm.h into your ucode to get it
// MK Note: C preprocessor...
#define PARTITION_SWITCH_TOK(X) PARTITION_SWITCH_TOK2(X)
#define PARTITION_SWITCH_TOK2(X) partition##X##SwitchInt
#define PARTITION_SWITCH_INT_NAME PARTITION_SWITCH_TOK(PARTITION_NAME)

extern LwU64 PARTITION_SWITCH_INT_NAME(LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4, LwU64 partition);
// This call may return. To use it you must use partctxasm.h to save/restore contexts
static inline LwU64
partitionSwitch(LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4, LwU64 partition)
{
    LwU64 ret;

    ret = PARTITION_SWITCH_INT_NAME(arg0, arg1, arg2, arg3, arg4, partition);

    return ret;
}

// Print helper with partition name built in
#define pPrintf(fmt, ...) printf(PARTITION_NAME_SELF ": " fmt, ##__VA_ARGS__)
#define dbgpPrintf(level, fmt, ...) dbgPrintf(level, PARTITION_NAME_SELF ": " fmt, ##__VA_ARGS__)
#endif // __ASSEMBLER__

#endif // SEC2_FMC_PARTITIONS_H
