/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "../../kernel/riscv-isa.h"
#include "../common/test-common.h"
#include "debug/elf.h"
#include "drivers/common/inc/hwref/turing/tu102/dev_gsp.h"
#include "drivers/common/inc/hwref/turing/tu102/dev_gsp_riscv_csr_64.h"
#include "drivers/common/inc/swref/turing/tu102/dev_riscv_csr_64_addendum.h"
#include "drivers/common/inc/hwref/turing/tu102/dev_pri_ringstation_sys.h"
#include "include/libos_init_daemon.h"
#include "kernel/libos_defines.h"
#include "libos.h"
#include "task_init.h"
#include "task_init_interface.h"

DEFINE_LOG_INIT()

#ifndef LW_ARRAY_ELEMENTS
#define LW_ARRAY_ELEMENTS(x) ((sizeof(x) / sizeof((x)[0])))
#endif

static const char *taskToString(LibosTaskId taskId)
{
    static LIBOS_SECTION_LOGGING const char INIT_str[]    = "TASK_INIT"; // 0
    static LIBOS_SECTION_LOGGING const char TEST_str[]    = "TASK_TEST"; // 1
    static LIBOS_SECTION_LOGGING const char Unknown_str[] = "Unknown";

    static const struct
    {
        LibosTaskId taskId;
        const char *desc;
    } taskTbl[] =
    {
        {TASK_INIT, INIT_str},
        {TASK_TEST, TEST_str}
    };

    LwU64 i;
    LwU64 n = LW_ARRAY_ELEMENTS(taskTbl);

    for (i = 0; i < n; i++)
    {
        if (taskTbl[i].taskId == taskId)
        {
            return taskTbl[i].desc;
        }
    }

    return Unknown_str;
}

static const char *xcauseToString(LwU64 xcause)
{
    static LIBOS_SECTION_LOGGING const char IAMA_str[] =     "IAMA - Instruction address misaligned";    // 0x00000000
    static LIBOS_SECTION_LOGGING const char IACC_str[] =     "IACC - Instruction access fault";          // 0x00000001
    static LIBOS_SECTION_LOGGING const char ILL_str[] =      "ILL - Illegal instruction";                // 0x00000002
    static LIBOS_SECTION_LOGGING const char BKPT_str[] =     "BKPT - Breakpoint";                        // 0x00000003
    static LIBOS_SECTION_LOGGING const char LAMA_str[] =     "LAMA - Load address misaligned";           // 0x00000004
    static LIBOS_SECTION_LOGGING const char LACC_str[] =     "LACC- Load access fault";                 // 0x00000005
    static LIBOS_SECTION_LOGGING const char SAMA_str[] =     "SAMA - Store address misaligned";          // 0x00000006
    static LIBOS_SECTION_LOGGING const char SACC_str[] =     "SACC - Store access fault";                // 0x00000007
    static LIBOS_SECTION_LOGGING const char UCALL_str[] =    "UCALL - Environment call from user-space"; // 0x00000008
    static LIBOS_SECTION_LOGGING const char MCALL_str[] =    "MCALL - Environment call from m-mode";     // 0x0000000b
    static LIBOS_SECTION_LOGGING const char IPAGE_str[] =    "IPAGE - Instruction page fault";           // 0x0000000c
    static LIBOS_SECTION_LOGGING const char LPAGE_str[] =    "LPAGE - Load page fault";                  // 0x0000000d
    static LIBOS_SECTION_LOGGING const char SPAGE_str[] =    "SPAGE - Store page fault";                 // 0x0000000f
    static LIBOS_SECTION_LOGGING const char FAKEEX_str[] =   "FAKEEX";                                   // 0x00000010
    static LIBOS_SECTION_LOGGING const char DBG_str[] =      "DBG";                                      // 0x00000011
    static LIBOS_SECTION_LOGGING const char ENTER_BR_str[] = "ENTER_BR_STATE";                           // 0x00000012
    static LIBOS_SECTION_LOGGING const char EXIT_BR_str[] =  "EXIT_BR_STATE";                            // 0x00000013
    static LIBOS_SECTION_LOGGING const char USS_str[] =      "USS";                                      // 0x0000001c
    static LIBOS_SECTION_LOGGING const char SSS_str[] =      "SSS";                                      // 0x0000001d

    static LIBOS_SECTION_LOGGING const char U_SWINT_str[] =  "U_SWINT - User software interrupt";        //  0x00000000
    static LIBOS_SECTION_LOGGING const char M_SWINT_str[] =  "M_SWINT - Machine software interrupt";     //  0x00000003
    static LIBOS_SECTION_LOGGING const char U_TINT_str[] =   "U_TINT - User timer interrupt";            //  0x00000004
    static LIBOS_SECTION_LOGGING const char M_TINT_str[] =   "M_TINT - Machine timer interrupt";         //  0x00000007
    static LIBOS_SECTION_LOGGING const char U_EINT_str[] =   "U_EINT - User external interrupt";         //  0x00000008
    static LIBOS_SECTION_LOGGING const char M_EINT_str[] =   "M_EINT - Machine external interrupt";      //  0x0000000b

    static LIBOS_SECTION_LOGGING const char Unknown_str[] =  "Unknown";

    static const struct
    {
        LwU8        excode;
        const char *desc;
    }
    excodeExceptionTbl[] =
    {
        { LW_RISCV_CSR_MCAUSE_EXCODE_IAMA,            IAMA_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_IACC_FAULT,      IACC_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_ILL,             ILL_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_BKPT,            BKPT_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_LAMA,            LAMA_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_LACC_FAULT,      LACC_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_SAMA,            SAMA_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_SACC_FAULT,      SACC_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_UCALL,           UCALL_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_MCALL,           MCALL_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_IPAGE_FAULT,     IPAGE_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_LPAGE_FAULT,     LPAGE_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_SPAGE_FAULT,     SPAGE_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_FAKEEX,          FAKEEX_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_DBG,             DBG_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_ENTER_BR_STATE,  ENTER_BR_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_EXIT_BR_STATE,   EXIT_BR_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_USS,             USS_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_SSS,             SSS_str }
    };

    static const struct
    {
        LwU8        excode;
        const char *desc;
    }
    excodeInterruptTbl[] =
    {
        { LW_RISCV_CSR_MCAUSE_EXCODE_U_SWINT,        U_SWINT_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_M_SWINT,        M_SWINT_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_U_TINT,         U_TINT_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_M_TINT,         M_TINT_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_U_EINT,         U_EINT_str },
        { LW_RISCV_CSR_MCAUSE_EXCODE_M_EINT,         M_EINT_str },
    };

    LwU64  i;
    LwU64  n;
    LwU64  excode = (LwU8)DRF_VAL64(_RISCV_CSR, _MCAUSE, _EXCODE, xcause);
    LwBool bInt   = FLD_TEST_DRF64(_RISCV_CSR, _MCAUSE, _INT, _INTERRUPT, xcause);

    if (bInt)
    {
        n = LW_ARRAY_ELEMENTS(excodeInterruptTbl);
        for (i = 0; i < n; i++)
        {
            if (excodeInterruptTbl[i].excode == excode)
            {
                return excodeInterruptTbl[i].desc;
            }
        }
    }
    else
    {
        n = LW_ARRAY_ELEMENTS(excodeExceptionTbl);
        for (i = 0; i < n; i++)
        {
            if (excodeExceptionTbl[i].excode == excode)
            {
                return excodeExceptionTbl[i].desc;
            }
        }
    }

    return Unknown_str;
}

static const char *privErrorToString(LwU32 privError)
{
    static LIBOS_SECTION_LOGGING const char HOST_FECS_ERR_str[]                                 = "HOST_FECS_ERR";                                 // 0xBAD00F
    static LIBOS_SECTION_LOGGING const char HOST_PRI_TIMEOUT_str[]                              = "HOST_PRI_TIMEOUT";                              // 0xBAD001
    static LIBOS_SECTION_LOGGING const char HOST_FB_ACK_TIMEOUT_str[]                           = "HOST_FB_ACK_TIMEOUT";                           // 0xBAD0B0
    static LIBOS_SECTION_LOGGING const char FECS_PRI_TIMEOUT_str[]                              = "FECS_PRI_TIMEOUT";                              // 0xBADF10
    static LIBOS_SECTION_LOGGING const char FECS_PRI_DECODE_str[]                               = "FECS_PRI_DECODE";                               // 0xBADF11
    static LIBOS_SECTION_LOGGING const char FECS_PRI_RESET_str[]                                = "FECS_PRI_RESET";                                // 0xBADF12
    static LIBOS_SECTION_LOGGING const char FECS_PRI_FLOORSWEEP_str[]                           = "FECS_PRI_FLOORSWEEP";                           // 0xBADF13
    static LIBOS_SECTION_LOGGING const char FECS_PRI_STUCK_ACK_str[]                            = "FECS_PRI_STUCK_ACK";                            // 0xBADF14
    static LIBOS_SECTION_LOGGING const char FECS_PRI_0_EXPECTED_ACK_str[]                       = "FECS_PRI_0_EXPECTED_ACK";                       // 0xBADF15
    static LIBOS_SECTION_LOGGING const char FECS_PRI_FENCE_ERROR_str[]                          = "FECS_PRI_FENCE_ERROR";                          // 0xBADF16
    static LIBOS_SECTION_LOGGING const char FECS_PRI_SUBID_ERROR_str[]                          = "FECS_PRI_SUBID_ERROR";                          // 0xBADF17
    static LIBOS_SECTION_LOGGING const char FECS_PRI_RDAT_WAIT_VIOLATION_str[]                  = "FECS_PRI_RDAT_WAIT_VIOLATION";                  // 0xBADF18
    static LIBOS_SECTION_LOGGING const char FECS_PRI_WRBE_str[]                                 = "FECS_PRI_WRBE";                                 // 0xBADF19
    static LIBOS_SECTION_LOGGING const char FECS_PRI_ORPHAN_str[]                               = "FECS_PRI_ORPHAN";                               // 0xBADF20
    static LIBOS_SECTION_LOGGING const char FECS_PRI_POWER_OK_TIMEOUT_str[]                     = "FECS_PRI_POWER_OK_TIMEOUT";                     // 0xBADF21
    static LIBOS_SECTION_LOGGING const char FECS_DEAD_RING_str[]                                = "FECS_DEAD_RING";                                // 0xBADF30
    static LIBOS_SECTION_LOGGING const char FECS_DEAD_RING_LOW_POWER_str[]                      = "FECS_DEAD_RING_LOW_POWER";                      // 0xBADF31
    static LIBOS_SECTION_LOGGING const char FECS_TRAP_str[]                                     = "FECS_TRAP";                                     // 0xBADF40
    static LIBOS_SECTION_LOGGING const char FECS_PRI_TARGET_MASK_VIOLATION_str[]                = "FECS_PRI_TARGET_MASK_VIOLATION";                // 0xBADF41
    static LIBOS_SECTION_LOGGING const char FECS_PRI_CLIENT_ERR_str[]                           = "FECS_PRI_CLIENT_ERR";                           // 0xBADF50
    static LIBOS_SECTION_LOGGING const char FECS_PRI_CLIENT_PRIV_LEVEL_VIOLATION_str[]          = "FECS_PRI_CLIENT_PRIV_LEVEL_VIOLATION";          // 0xBADF51
    static LIBOS_SECTION_LOGGING const char FECS_PRI_CLIENT_INDIRECT_PRIV_LEVEL_VIOLATION_str[] = "FECS_PRI_CLIENT_INDIRECT_PRIV_LEVEL_VIOLATION"; // 0xBADF52
    static LIBOS_SECTION_LOGGING const char FECS_LOCAL_PRIV_RING_ERR_str[]                      = "FECS_LOCAL_PRIV_RING_ERR";                      // 0xBADF53
    static LIBOS_SECTION_LOGGING const char FALCON_MEM_ACCESS_PRIV_LEVEL_VIOLATION_str[]        = "FALCON_MEM_ACCESS_PRIV_LEVEL_VIOLATION";        // 0xBADF54
    static LIBOS_SECTION_LOGGING const char FECS_PRI_ROUTE_ERR_str[]                            = "FECS_PRI_ROUTE_ERR";                            // 0xBADF55
    static LIBOS_SECTION_LOGGING const char LWSTOM_PRI_CLIENT_ERR_str[]                         = "LWSTOM_PRI_CLIENT_ERR";                         // 0xBADF56
    static LIBOS_SECTION_LOGGING const char FECS_PRI_CLIENT_SOURCE_ENABLE_VIOLATION_str[]       = "FECS_PRI_CLIENT_SOURCE_ENABLE_VIOLATION";       // 0xBADF57
    static LIBOS_SECTION_LOGGING const char Unknown_str[] = "";

    static const struct
    {
        LwU32 errorCode;
        const char *desc;
    }
    errorCodeTbl[] =
    {
        { LW_PPRIV_SYS_PRI_ERROR_CODE_HOST_FECS_ERR,                                 HOST_FECS_ERR_str                                 },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_HOST_PRI_TIMEOUT,                              HOST_PRI_TIMEOUT_str                              },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_HOST_FB_ACK_TIMEOUT,                           HOST_FB_ACK_TIMEOUT_str                           },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_TIMEOUT,                              FECS_PRI_TIMEOUT_str                              },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_DECODE,                               FECS_PRI_DECODE_str                               },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_RESET,                                FECS_PRI_RESET_str                                },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_FLOORSWEEP,                           FECS_PRI_FLOORSWEEP_str                           },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_STUCK_ACK,                            FECS_PRI_STUCK_ACK_str                            },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_0_EXPECTED_ACK,                       FECS_PRI_0_EXPECTED_ACK_str                       },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_FENCE_ERROR,                          FECS_PRI_FENCE_ERROR_str                          },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_SUBID_ERROR,                          FECS_PRI_SUBID_ERROR_str                          },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_RDAT_WAIT_VIOLATION,                  FECS_PRI_RDAT_WAIT_VIOLATION_str                  },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_WRBE,                                 FECS_PRI_WRBE_str                                 },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_ORPHAN,                               FECS_PRI_ORPHAN_str                               },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_POWER_OK_TIMEOUT,                     FECS_PRI_POWER_OK_TIMEOUT_str                     },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_DEAD_RING,                                FECS_DEAD_RING_str                                },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_DEAD_RING_LOW_POWER,                      FECS_DEAD_RING_LOW_POWER_str                      },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_TRAP,                                     FECS_TRAP_str                                     },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_TARGET_MASK_VIOLATION,                FECS_PRI_TARGET_MASK_VIOLATION_str                },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_CLIENT_ERR,                           FECS_PRI_CLIENT_ERR_str                           },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_CLIENT_PRIV_LEVEL_VIOLATION,          FECS_PRI_CLIENT_PRIV_LEVEL_VIOLATION_str          },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_CLIENT_INDIRECT_PRIV_LEVEL_VIOLATION, FECS_PRI_CLIENT_INDIRECT_PRIV_LEVEL_VIOLATION_str },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_LOCAL_PRIV_RING_ERR,                      FECS_LOCAL_PRIV_RING_ERR_str                      },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FALCON_MEM_ACCESS_PRIV_LEVEL_VIOLATION,        FALCON_MEM_ACCESS_PRIV_LEVEL_VIOLATION_str        },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_ROUTE_ERR,                            FECS_PRI_ROUTE_ERR_str                            },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_LWSTOM_PRI_CLIENT_ERR,                         LWSTOM_PRI_CLIENT_ERR_str                         },
        { LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_CLIENT_SOURCE_ENABLE_VIOLATION,       FECS_PRI_CLIENT_SOURCE_ENABLE_VIOLATION_str       }
    };

    LwU64 i;
    LwU64 n = LW_ARRAY_ELEMENTS(errorCodeTbl);
    LwU64 errorCode = DRF_VAL(_PPRIV_SYS, _PRI_ERROR, _CODE, privError);

    for (i = 0; i < n; i++)
    {
        if (errorCodeTbl[i].errorCode == errorCode)
        {
            return errorCodeTbl[i].desc;
        }
    }

    return Unknown_str;
}

struct stackframe
{
    LwU64 frame_pointer;
    LwU64 ra;
};

static LwBool libosTraceIsValidCode(LibosTaskId remote, LwU64 pc)
{
    LwU32 perms;
    if (libosMemoryQuery(remote, (void *)pc, &perms, 0, 0, 0, 0) != LIBOS_OK)
        return LW_FALSE;
    return perms & LIBOS_MEMORY_ACCESS_MASK_EXELWTE;
}

static void libosTracePrintEntry(LibosTaskId remote, LwU64 pc)
{
    if (libosTraceIsValidCode(remote, pc))
        LOG_INIT(LOG_LEVEL_INFO, " %016llx %a\n", pc, pc);
    else
        LOG_INIT(LOG_LEVEL_INFO, "   ??? %016llx\n", pc);
}

static LwU64 taskRead(LibosTaskId remote, LwU64 address)
{
    LwU64 local = 0;
    libosTaskMemoryRead(&local, remote, address, sizeof(LwU64));
    return local;
}

static __attribute__((no_sanitize_address)) LwBool
libosTraceIsInStack(void *stackBase, LwU64 stackSize, LibosTaskState *regs, LwU64 address, LwU64 size)
{
    if (address & 3)
        return LW_FALSE;

    address -= (LwU64)stackBase;
    address += size;

    if (address >= stackSize)
        return LW_FALSE;

    return LW_TRUE;
}

static LwU64 libosTraverseFrame(
    LibosTaskId remote, LibosTaskState *regs, LwU64 frame_top, LwU64 *ra, void *stackBase, LwU64 stackSize)
{
    LwU64 candidateFramePtr = frame_top, nextFramePtr = 0;
    *ra = 0;
    for (LwU32 i = 0; i < 2; i++)
    {
        candidateFramePtr -= sizeof(LwU64);
        if (libosTraceIsInStack(stackBase, stackSize, regs, candidateFramePtr, sizeof(LwU64)))
        {
            LwU64 value = taskRead(remote, candidateFramePtr);
            if (!nextFramePtr && nextFramePtr < frame_top && value >= (LwU64)stackBase &&
                value <= ((LwU64)stackBase + stackSize))
            {
                nextFramePtr = value;
            }

            if (!*ra && libosTraceIsValidCode(remote, value))
                *ra = value;
        }
    }
    return nextFramePtr;
}

static void dumpState(LibosTaskState *state)
{
    LwU64 *r = state->registers;
    LOG_INIT(LOG_LEVEL_INFO," xepc=%016llx\txbadaddr=%016llx\txcause=%016llx %s\n",
        state->xepc, state->xbadaddr, state->xcause, xcauseToString(state->xcause));
    LOG_INIT(LOG_LEVEL_INFO," x0=%016llx\t ra=%016llx\t sp=%016llx\t gp=%016llx\n", r[RISCV_X0], r[RISCV_RA], r[RISCV_SP],  r[RISCV_GP]);
    LOG_INIT(LOG_LEVEL_INFO," tp=%016llx\t t0=%016llx\t t1=%016llx\t t2=%016llx\n", r[RISCV_TP], r[RISCV_T0], r[RISCV_T1],  r[RISCV_T2]);
    LOG_INIT(LOG_LEVEL_INFO," s0=%016llx\t s1=%016llx\t a0=%016llx\t a1=%016llx\n", r[RISCV_S0], r[RISCV_S1], r[RISCV_A0],  r[RISCV_A1]);
    LOG_INIT(LOG_LEVEL_INFO," a2=%016llx\t a3=%016llx\t a4=%016llx\t a5=%016llx\n", r[RISCV_A2], r[RISCV_A3], r[RISCV_A4],  r[RISCV_A5]);
    LOG_INIT(LOG_LEVEL_INFO," a6=%016llx\t a7=%016llx\t s2=%016llx\t s3=%016llx\n", r[RISCV_A6], r[RISCV_A7], r[RISCV_S2],  r[RISCV_S3]);
    LOG_INIT(LOG_LEVEL_INFO," s4=%016llx\t s5=%016llx\t s6=%016llx\t s7=%016llx\n", r[RISCV_S4], r[RISCV_S5], r[RISCV_S6],  r[RISCV_S7]);
    LOG_INIT(LOG_LEVEL_INFO," s8=%016llx\t s9=%016llx\ts10=%016llx\ts11=%016llx\n", r[RISCV_S8], r[RISCV_S9], r[RISCV_S10], r[RISCV_S11]);
    LOG_INIT(LOG_LEVEL_INFO," t3=%016llx\t t4=%016llx\t t5=%016llx\t t6=%016llx\n", r[RISCV_T3], r[RISCV_T4], r[RISCV_T5],  r[RISCV_T6]);
}

static LibosStatus initTaskBacktrace(LibosTaskId remoteTask, LibosTaskState *state)
{
    // Validate we have access to this tasks stack
    LwU32 stackPerms;
    void *stackBase;
    LwU64 stackSize;
    if (libosMemoryQuery(
            remoteTask, (void *)state->registers[RISCV_SP], &stackPerms, &stackBase, &stackSize, 0, 0) !=
        LIBOS_OK)
    {
        LOG_INIT(LOG_LEVEL_INFO, "   Debugger was not granted permissions to stack\n");
        return LIBOS_ERROR_ILWALID_ACCESS;
    }

    LwU64 frame = state->registers[RISCV_S0], pc = state->xepc;
    LwU64 count = 0;

    // Top of stack
    libosTracePrintEntry(remoteTask, pc);

    // One down
    frame = libosTraverseFrame(remoteTask, state, frame, &pc, stackBase, stackSize);
    if (!pc)
        pc = state->registers[RISCV_RA];

    if (pc != state->xepc)
        libosTracePrintEntry(remoteTask, pc);

    while ((frame = libosTraverseFrame(remoteTask, state, frame, &pc, stackBase, stackSize)) && count < 256)
    {
        libosTracePrintEntry(remoteTask, pc);
        count++;
    }

    return LIBOS_OK;
}

static LibosStatus process_util_command(const task_init_util_payload* payload, LibosTaskId task_id)
{
    LibosTaskState state;
    LibosStatus    status;

    LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK,
            libosTaskStateQuery(task_id, &state));
    if ( status != LIBOS_OK)
        goto shuttleReset;

    switch(payload->command)
    {
        case TASK_INIT_UTIL_CMD_STATE:
            LOG_INIT(LOG_LEVEL_INFO, "State for task %d %s:\n", task_id, taskToString(task_id));
            dumpState(&state);
            goto portReply;

        case TASK_INIT_UTIL_CMD_STACK:
            LOG_INIT(LOG_LEVEL_INFO, "Callstack for task %d %s:\n", task_id, taskToString(task_id));
            LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK, initTaskBacktrace(task_id, &state));
            goto portReply;

        case TASK_INIT_UTIL_CMD_STATE_AND_STACK:
            LOG_INIT(LOG_LEVEL_INFO, "State and callstack for task %d %s:\n", task_id, taskToString(task_id));
            dumpState(&state);
            LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK, initTaskBacktrace(task_id, &state));
            goto portReply;

        case TASK_INIT_UTIL_CMD_SELWRITY_VIOLATION:
            if (payload->secArgs.type == TASK_INIT_UTIL_SELWRITY_VIOLATION_TYPE_STACK_CANARY_CORRUPTED)
            {
                LOG_INIT(LOG_LEVEL_INFO, "\t\t** STACK SMASHING DETECTED **\n In task %d %s.\n"
                                         "Warning Callstack may not be accurate.\n"
                                         "State and callstack:\n", task_id, taskToString(task_id));
                dumpState(&state);
                LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK, initTaskBacktrace(task_id, &state));
            }
            goto shuttleReset;

        default:
            LOG_INIT(LOG_LEVEL_INFO, "TASK_INIT_SHUTTLE_UTIL: Requesting unknown command %d for task %d %s\n",
                payload->command, task_id, taskToString(task_id));
            //We should not reply if the command is unknown
            goto shuttleReset;
    }
shuttleReset:
    // ShuttleReset is the safest way to handle unknown or undefined behavior
    LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK,
            libosShuttleReset(TASK_INIT_SHUTTLE_UTIL));
    return status;

portReply:
    LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK,
            libosPortReply(TASK_INIT_SHUTTLE_UTIL, 0, 0, 0));
    return status;
}

LwBool task_emulate_mmio_fault(LibosTaskId remoteTask, LibosTaskState *state)
{
    LwU32 aperture, accessPerms;

    if (state->xcause != DRF_DEF64(_RISCV_CSR, _MCAUSE, _EXCODE, _LACC_FAULT) &&
        state->xcause != DRF_DEF64(_RISCV_CSR, _MCAUSE, _EXCODE, _SACC_FAULT))
    {
        return LW_FALSE;
    }

    LOG_INIT(LOG_LEVEL_INFO, "task_emulate_mmio_fault: xbadaddr %016llx xcause %u %s.\n",
        state->xbadaddr, state->xcause, xcauseToString(state->xcause));

    if (LIBOS_OK != libosMemoryQuery(remoteTask, (void *)state->xbadaddr, 0, 0, 0, &aperture, 0))
    {
        return LW_FALSE;
    }

    LOG_INIT(LOG_LEVEL_INFO, "xbadaddr aperture : %08lx.\n", aperture);

    if (aperture != LIBOS_MEMORY_APERTURE_MMIO)
        return LW_FALSE;

    LOG_INIT(
        LOG_LEVEL_INFO,
        R"(
____  ____  _____     __  _____ ___ __  __ _____ ___  _   _ _____
|  _ \|  _ \|_ _\ \   / / |_   _|_ _|  \/  | ____/ _ \| | | |_   _|
| |_) | |_) || | \ \ / /    | |  | || |\/| |  _|| | | | | | | | |
|  __/|  _ < | |  \ V /     | |  | || |  | | |__| |_| | |_| | | |
|_|   |_| \_\___|  \_/      |_| |___|_|  |_|_____\___/ \___/  |_|
)");

    // Is the PC valid and readable? (We assume we have debug permissions to all tasks)
    if (LIBOS_OK != libosMemoryQuery(remoteTask, (void *)state->xepc, &accessPerms, 0, 0, 0, 0))
    {
        LOG_INIT(LOG_LEVEL_INFO, "Internal error, unable to lookup\n");
        return LW_FALSE;
    }

    LOG_INIT(LOG_LEVEL_INFO, "xepc perms : %08lx.\n", accessPerms);

    if (!(accessPerms & LIBOS_MEMORY_ACCESS_MASK_EXELWTE) ||
        !(accessPerms & LIBOS_MEMORY_ACCESS_MASK_READ))
    {
        LOG_INIT(LOG_LEVEL_INFO, "Internal error, code not exelwtable. (accessPerms = %08lx)\n", accessPerms);
        return LW_FALSE;
    }

    if ((state->xepc & 3) != 0)
    {
        LOG_INIT(LOG_LEVEL_INFO, "Internal error, PC not aligned (did we finally add compressed ISA?)\n");
        return LW_FALSE;
    }

    // Grab the opcode
    LwU32 opcode = 0;
    if (LIBOS_OK != libosTaskMemoryRead(&opcode, remoteTask, state->xepc, sizeof(LwU32)))
    {
        LOG_INIT(LOG_LEVEL_INFO, "Internal error, unable to read opcode\n");
        return LW_FALSE;
    }

    // Advance one instruction
    state->xepc += 4;

    if (state->xcause == DRF_DEF64(_RISCV_CSR, _MCAUSE, _EXCODE, _LACC_FAULT) &&
        (opcode & 0x7F) == RISCV_OPCODE_LD)
    {
        // We should set the read register to all FF's
        LwU64 rd = (opcode >> 7) & 31;
        LwU64 result = (state->lwriscv.priv_err_info != 0) ? state->lwriscv.priv_err_info : LW_U64_MAX;

        switch ((opcode >> 12) & 7)
        {
            // The following are all sign extended
            case 0: // LB
            case 1: // LH
            case 2: // LW
            case 3: // LD
                LOG_INIT(LOG_LEVEL_INFO, "Emulating faulting load: r%d set to 0x%llx, resuming.\n", rd, result);
                state->registers[rd] = result;
                break;

            // The following are zero extended
            case 4: // LBU
            {
                LwU8 res8 = (LwU8)(result & 0xFF);
                LOG_INIT(LOG_LEVEL_INFO, "Emulating faulting load: r%d set to 0x%x, resuming.\n", rd, res8);
                state->registers[rd] = res8;
                break;
            }
            case 5: // LHU
            {
                LwU16 res16 = (LwU16)(result & 0xFFFF);
                LOG_INIT(LOG_LEVEL_INFO, "Emulating faulting load: r%d set to 0x%x, resuming.\n", rd, res16);
                state->registers[rd] = res16;
                break;
            }
            case 6: // LWU
            {
                LwU32 res32 = (LwU32)(result & 0xFFFFFFFF);
                LOG_INIT(LOG_LEVEL_INFO, "Emulating faulting load: r%d set to 0x%x, resuming.\n", rd, res32);
                state->registers[rd] = res32;
                break;
            }
        }
    }
    else if (state->xcause == DRF_DEF64(_RISCV_CSR, _MCAUSE, _EXCODE, _SACC_FAULT) &&
             (opcode & 0x7F) == RISCV_OPCODE_SD)
    {
        LOG_INIT(LOG_LEVEL_INFO, "Skipping faulting store.\n");
    }
    else
    {
        LOG_INIT(LOG_LEVEL_INFO, "Critical error: Unknown instruction encoding / Instruction does not match fault kind.\n");
        return LW_FALSE;
    }

    return LW_TRUE;
}

void task_init_entry(LwU64 elfPhysicalBase)
{
    task_init_util_payload  payload;
    LwU64                   memErrCount       = 0;
    LwU64                   memErrLastAddress = 0xffffffffffffffff;
    LwU64                   memErrLastPC      = 0xffffffffffffffff;
    LwU64                   payload_size;
    LibosTaskId             remote_task_id    = 0;
    LibosLocalShuttleId     shuttle           = 0;
    LwBool                  bMemErrHubErr     = LW_FALSE;
    LibosStatus             status;

    // TASK_INIT_FB gives the init task read-only access to FB addresses (VA=PA)
    elf64_header *elf = (elf64_header *)elfPhysicalBase;

    // Locate the manifest for the init arguments (external memory regions)
    LwU64                                    memory_region_init_arguments_count = 0;
    libos_manifest_init_arg_memory_region_t *memory_region_init_arguments =
        lib_init_phdr_init_arg_memory_regions(elf, &memory_region_init_arguments_count);

    // Read the init message mailbox from RM
    LwU64                          mailbox0   = *(volatile LwU32 *)(TASK_INIT_PRIV + LW_PGSP_FALCON_MAILBOX0);
    LwU64                          mailbox1   = *(volatile LwU32 *)(TASK_INIT_PRIV + LW_PGSP_FALCON_MAILBOX1);
    LwU64                          mailbox    = (mailbox1 << 32) | mailbox0;
    LibosMemoryRegionInitArgument *init_args = (LibosMemoryRegionInitArgument *)mailbox;

    // Pre-map the init tasks log through the TASK_INIT_FB aperture
    lib_init_premap_log(init_args);

    // Size and allocate the pagetables
    lib_init_pagetable_initialize(elf, elfPhysicalBase);

    // Populate pagetable entries
    while (init_args && init_args->id8)
    {
        for (LwU64 i = 0u; i < memory_region_init_arguments_count; i++)
        {
            if (memory_region_init_arguments[i].id8 == init_args->id8)
            {
                LwU64 index = memory_region_init_arguments[i].index;
                lib_init_pagetable_map(elf, index, init_args->pa, init_args->size);
                break;
            }
        }

        ++init_args;
    }
    LOG_INIT(LOG_LEVEL_INFO, "External memory regions wired.\n");
    construct_log_init();

    // Setup port for crash
    LOG_AND_VALIDATE_EQUAL(
        LOG_INIT, status, LIBOS_OK,
        libosPortAsyncRecv(
            TASK_INIT_SHUTTLE_PANIC, // recvShuttle
            TASK_INIT_PANIC_PORT,    // recvPort
            0,                       // recvPayload
            0));                     // recvPayloadSize

    // Setup port for util functions.
    LOG_AND_VALIDATE_EQUAL(
        LOG_INIT, status, LIBOS_OK,
        libosPortAsyncRecv(
            TASK_INIT_SHUTTLE_UTIL, // recvShuttle
            TASK_INIT_UTIL_PORT,    // recvPort
            &payload,               // recvPayload
            sizeof payload));       // recvPayloadSize

    LOG_INIT(LOG_LEVEL_INFO, "Init spawning tasks ... \n");

    // Start the test task
    LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK,
        libosPortSyncSend(
            LIBOS_SHUTTLE_DEFAULT,    // sendShuttle
            TASK_TEST_PORT,           // sendPort
            0,                        // sendPayload
            0,                        // sendPayloadSize
            0,                        // completedSize
            LIBOS_TIMEOUT_INFINITE)); // timeout

    LOG_INIT(LOG_LEVEL_INFO, "Init entering daemon mode... \n");

    while (1)
    {
        //
        //  Wait for any shuttle to complete
        //     (TASK_INIT_SHUTTLE_UTIL and TASK_INIT_SHUTTLE_PANIC
        //      are always kept waiting on their respective ports)
        //
        LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK,
            libosPortWait(
                LIBOS_SHUTTLE_ANY,        // waitShuttle
                &shuttle,                 // completedShuttle
                &payload_size,            // completedSize
                &remote_task_id,          // completedRemoteTaskId
                LIBOS_TIMEOUT_INFINITE)); // timeoutNs

        LOG_INIT(
            LOG_LEVEL_INFO, "Init got shuttle: %u  remote_task_id: %u %s  payload_size %u\n", shuttle,
            remote_task_id, taskToString(remote_task_id), payload_size);

        switch(shuttle)
        {
            case TASK_INIT_SHUTTLE_UTIL:
                LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK,
                        process_util_command(&payload, remote_task_id));

                // Re-arm the shuttle for further messages
                LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK,
                        libosPortAsyncRecv(
                            TASK_INIT_SHUTTLE_UTIL,   // recvShuttle
                            TASK_INIT_UTIL_PORT,      // recvPort
                            &payload,                 // recvPayload
                            sizeof payload));         // recvPayloadSize
                break;

            case TASK_INIT_SHUTTLE_PANIC:
            {
                LibosTaskState state;
                LwU8 excode;
                LwBool bLaccFault;
                LwBool bReply = LW_FALSE;
                LwBool bCallStack = LW_TRUE;

                // Read the register state to determine fault-kind
                LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK,
                        libosTaskStateQuery(remote_task_id, &state ));

                LOG_INIT(LOG_LEVEL_INFO, "Task %u %s has trapped with xcause %016llx %s\n",
                    remote_task_id, taskToString(remote_task_id),
                    state.xcause, xcauseToString(state.xcause));

                excode = DRF_VAL64(_RISCV_CSR, _MCAUSE, _EXCODE, state.xcause);
                bLaccFault = (excode == LW_RISCV_CSR_MCAUSE_EXCODE_LACC_FAULT);

                if ((excode == LW_RISCV_CSR_MCAUSE_EXCODE_M_EINT) ||
                    (bLaccFault && (state.lwriscv.priv_err_info != 0)))
                {
                    if (state.xepc == *(LwU64 *)"SPURIOUS")
                    {
                        LOG_INIT(LOG_LEVEL_INFO, "Spurious interrupt! IRQSTAT=%08x.\n", state.xbadaddr);
                        bCallStack = LW_FALSE;
                    }
                    else if (state.xbadaddr == 0xbad0acc5)
                    {
                        if (!bMemErrHubErr)
                        {
                            LOG_INIT(LOG_LEVEL_INFO, "HUB MemErr: Expect MMU fault.\n");

                            bMemErrHubErr = LW_TRUE;
                            memErrCount = 0;
                        }
                        bReply = LW_TRUE;
                    }
                    else // this is a PRIV MemErr (write) or LACC fault (read)
                    {
                        static LIBOS_SECTION_LOGGING const char Read_str[] = "Read";
                        static LIBOS_SECTION_LOGGING const char Write_str[] = "Write";
                        LwU32 privError = state.lwriscv.priv_err_info;
                        LwU64 errorAddr = state.xbadaddr;

                        LOG_INIT(LOG_LEVEL_INFO, "PRIV %s Error: 0x%08x %s @ 0x%016x.\n",
                            bLaccFault ? Read_str : Write_str, privError,
                            privErrorToString(privError), errorAddr);

                        bReply = LW_TRUE;
                    }

                    if ((memErrLastPC == state.xepc) || (memErrLastAddress == state.xbadaddr))
                    {
                        memErrCount++;
                        goto skipPrint;
                    }

                    memErrCount = 0;
                    memErrLastPC = state.xepc;
                    memErrLastAddress = state.xbadaddr;
                    memErrCount++;
                }

                if (state.xepc != 0)
                {
                    dumpState(&state);

                    if (bCallStack)
                    {
                        LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK,
                            initTaskBacktrace(remote_task_id, &state));
                    }
                }
                else
                {
                    LOG_INIT(LOG_LEVEL_INFO, "Task %u %s has terminated normally.\n",
                        remote_task_id, taskToString(remote_task_id));
                }

skipPrint:
                if (state.xepc != 0 && task_emulate_mmio_fault(remote_task_id, &state))
                {
                    bReply = LW_TRUE;
                }
                else if (excode == LW_RISCV_CSR_MCAUSE_EXCODE_BKPT)
                {
                    // @note Advance xepc past the ebreak instruction
                    state.xepc += 4;
                    bReply = LW_TRUE;
                }

                if (bReply)
                {
                    // Reply to trap message with the new state for the task (xepc+registers)
                    LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK,
                            libosPortReply(
                                TASK_INIT_SHUTTLE_PANIC,
                                &state,
                                sizeof(state),
                                0));
                }

                // Re-arm the shuttle
                LOG_AND_VALIDATE_EQUAL(LOG_INIT, status, LIBOS_OK,
                        libosPortAsyncRecv(
                            TASK_INIT_SHUTTLE_PANIC, // recvShuttle
                            TASK_INIT_PANIC_PORT,    // recvPort
                            0,                       // recvPayload
                            0));                     // recvPayloadSize
                break;
            }

            default:
                LOG_INIT(LOG_LEVEL_INFO, "Unknown shuttle completion %u: remoteTaskId: %u %s completed.\n",
                    shuttle, remote_task_id, taskToString(remote_task_id));
        }
    }

    __asm(R"(
        li a0, 0xF00D0001
        csrw 0x780, a0
    )" ::);
}
