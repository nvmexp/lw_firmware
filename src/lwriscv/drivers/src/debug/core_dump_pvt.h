/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef DRIVERS_CORE_DUMP_PRIV_H
#define DRIVERS_CORE_DUMP_PRIV_H

// All offsets and sizes assume 64b system.
ct_assert(__riscv_xlen == 64);

// Number of pages allocated for the core dump buffer
#define CORE_DUMP_PAGE_COUNT 2

#define MAX_DEBUGGED_TASK_COUNT     10
#define MAX_DEBUGGED_TASK_NAME_SIZE 16

// These values are from GDB:
// bfd/elfnn-riscv.c
#define PRSTATUS_SIZE               376
#define PRSTATUS_OFFSET_PR_LWRSIG   12
#define PRSTATUS_OFFSET_PR_PID      32
#define PRSTATUS_OFFSET_PR_REG      112
#define ELF_GREGSET_T_SIZE          256
#define PRPSINFO_SIZE               136
#define PRPSINFO_OFFSET_PR_PID      24
#define PRPSINFO_OFFSET_PR_FNAME    40
#define PRPSINFO_OFFSET_PR_PSARGS   56
#define PRPSINFO_PR_FNAME_LEN       16
#define PRPSINFO_PR_PSARGS_LEN      80
// gdb/dcache.c
#define DCACHE_DEFAULT_LINE_SIZE    64

// These values are from Linux headers:
#define NT_PRSTATUS 1
#define NT_PRPSINFO 3

// These structs are based on the ELF Note format and the above values.
// We write the note data with fixed offsets to match how GDB reads them out,
// and GDB reads them out that way to avoid dependencies on the Linux headers
// which define the struct layout.
// See bfd/elfnn-riscv.c: riscv_elf_grok_prstatus() and riscv_elf_grok_psinfo()
typedef struct {
    LwU32 namesz;               // 5
    LwU32 descsz;               // PRPSINFO_SIZE
    LwU32 type;                 // NT_PRPSINFO
    LwU8 name[8];               // "CORE"
    LwU8 desc[PRPSINFO_SIZE];
} Elf_Nprpsinfo;

typedef struct {
    LwU32 namesz;               // 5
    LwU32 descsz;               // PRSTATUS_SIZE
    LwU32 type;                 // NT_PRSTATUS
    LwU8 name[8];               // "CORE"
    LwU8 desc[PRSTATUS_SIZE];
} Elf_Nprstatus;

#endif // DRIVERS_CORE_DUMP_PRIV_H
