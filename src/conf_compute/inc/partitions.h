/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef CC_PARTITIONS_H
#define CC_PARTITIONS_H

#ifndef __ASSEMBLER__
#include <lwtypes.h>
#include <lwriscv/sbi.h>
#include <liblwriscv/print.h>
#include <liblwriscv/ssp.h>
#include "misc.h"
#endif // __ASSEMBLER__

// GSP CC Partition IDs
#include "gsp/gspifccpart.h"

// Mark symbol as visible from outside of partition
// For functions, this should be *only* partition entry point
// For other objects, they should be declared in shared section.
#define PARTITION_EXPORT                  __attribute__((visibility("default")))

#define PARTITION_STACK_VA_START    0xF000000000000000

#ifndef __ASSEMBLER__
#define PARTITION_XSTR(X) PARTITION_STR(X)
#define PARTITION_STR(X) # X
#define PARTITION_NAME_SELF PARTITION_XSTR(PARTITION_NAME)

static inline void partitionSwitchNoreturn(LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 partition_origin, LwU64 partition) __attribute__((noreturn));
static inline void partitionSwitchNoreturn(LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 partition_origin, LwU64 partition)
{
    sbicall6(SBI_EXTENSION_LWIDIA,
             SBI_LWFUNC_FIRST,
             arg0,
             arg1,
             arg2,
             arg3,
             partition_origin,
             partition);

    // We should never reach this point. If we do, that's fatal.
    ccPanic();
}


//#define XTOK(X) TOK(X)
//#define TOK(X) ## X

// FWD decl of asm partition switch code, you need to build partctxasm.h into your ucode to get it
// MK Note: C preprocessor...
#define PARTITION_SWITCH_TOK(X) PARTITION_SWITCH_TOK2(X)
#define PARTITION_SWITCH_TOK2(X) partition##X##SwitchInt
#define PARTITION_SWITCH_INT_NAME PARTITION_SWITCH_TOK(PARTITION_NAME)

extern LwU64 PARTITION_SWITCH_INT_NAME(LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 partition_origin, LwU64 partition);
// This call may return. To use it you must use partctxasm.h to save/restore contexts
static inline LwU64
partitionSwitch(LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 partition_origin, LwU64 partition)
{
    LwU64 ret;
    LwU64 canary;

    canary = sspGetCanary();
    sspSetCanary(0);
    ret = PARTITION_SWITCH_INT_NAME(arg0, arg1, arg2, arg3, partition_origin, partition);
    sspSetCanary(canary);

    return ret;
}

// Print helper with partition name built in
#if CC_PERMIT_DEBUG
#define pPrintf(fmt, ...) printf(PARTITION_NAME_SELF ": " fmt, ##__VA_ARGS__)
#define dbgpPrintf(level, fmt, ...) dbgPrintf(level, PARTITION_NAME_SELF ": " fmt, ##__VA_ARGS__)
#else // CC_PERMIT_DEBUG
#define pPrintf(fmt, ...)
#define dbgpPrintf(level, fmt, ...)
#endif // CC_PERMIT_DEBUG

/* Separate print, for rm-proxy only. */
#define rmPrintf(fmt, ...) printf(PARTITION_NAME_SELF ": " fmt, ##__VA_ARGS__)
#define dbgRmPrintf(level, fmt, ...) dbgPrintf(level, PARTITION_NAME_SELF ": " fmt, ##__VA_ARGS__)

#endif // __ASSEMBLER__

#endif // CC_PARTITIONS_H
