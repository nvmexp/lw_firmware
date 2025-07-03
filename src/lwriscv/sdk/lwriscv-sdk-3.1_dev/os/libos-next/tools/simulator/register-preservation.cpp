#if 0
#include "../common/common.h"

#define U(x)   (x##u)
#define ULL(x) (x##ull)
#include "../../include/libos_syscalls.h"

#define ASSERT(x)                                                                                            \
    if (!(x))                                                                                                \
        test_failed(#x);
#define MASK(x) (1ULL << (x))

void validator_register_preservation::handler_ecall()
{
    taskState *task    = &task_state_map[gsp->xscratch];
    task->lwrrent_ioctl = gsp->regs[LIBOS_REG_IOCTL_A0];
}

static LwU64 clobbered_registers_across_ioctl(LwU64 ioctl)
{
    switch (ioctl)
    {
    // @see ports.md register plan + __asm__ statements in libos.h
    case LIBOS_SYSCALL_TASK_PORT_SEND_RECV_WAIT:
        return MASK(RISCV_A0) | MASK(RISCV_A1) | MASK(RISCV_A2) | MASK(RISCV_A3) | MASK(RISCV_A5) |
               MASK(RISCV_T0) | MASK(RISCV_T1) | MASK(RISCV_T2) | MASK(RISCV_T3);

    // see libos.h
    case LIBOS_SYSCALL_TASK_YIELD:
        return 0;

#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
    case LIBOS_SYSCALL_SWITCH_TO:
        return 0;
#endif

    case LIBOS_SYSCALL_DCACHE_ILWALIDATE:
        return 0;

    case LIBOS_SYSCALL_SHUTTLE_RESET:
        return MASK(RISCV_A0);

    case LIBOS_SYSCALL_TASK_DMA_COPY:
        return MASK(RISCV_A0); /* return code */

    case LIBOS_SYSCALL_TASK_DMA_FLUSH:
        return 0;

    default:
        assert(0 && "Unknown IOCTL");
        return 0;
    }
}

void validator_register_preservation::handler_mret()
{
    // for (int i = 0; i < trap_depth; i++)
    //    printf("   ");
    // printf("MRET: xscratch=%016llx\n", xscratch);
    switch (trap_depth)
    {
    case 2: // trap inside kernel
    {
        // ASSERT(kernel_mscratch == gsp->xscratch && "Kernel trap must return to same task");
        ASSERT(gsp->privilege == privilege_machine);

        // Kernel fault should either be a load/store associated with port copy
        ASSERT(
            trap_mcause[trap_depth - 1] == DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _SACC_FAULT) ||
            trap_mcause[trap_depth - 1] == DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _LACC_FAULT));

        // Code that generated the fault should have been pre-emptable
        // ASSERT(DRF_VAL(_RISCV, _CSR_MSTATUS, _MPIE, gsp->mstatus));

        // Interrupts should be disabled
        ASSERT(!DRF_VAL(_RISCV, _CSR_MSTATUS, _MIE, gsp->mstatus));

        --trap_depth;
    }

        /* Fallthrough as we must return to userspace */

    case 1: // trap inside user-space
    {
        taskState *task = &task_state_map[gsp->xscratch];

        // No register clobbering requirements for the initial mret
        if (task->first_mret)
        {
            task->first_mret = LW_FALSE;
            break;
        }

        // Interrupts should be disabled until we return to user-thread
        ASSERT(!DRF_VAL(_RISCV, _CSR_MSTATUS, _MIE, gsp->mstatus));

        LwBool success            = LW_TRUE;
        LwU64 clobbered_registers = clobbered_registers_across_ioctl(task->lwrrent_ioctl);

        for (int i = 0; i < 32; i++)
        {
            // Kernel traps only preserve T0-T3 and A0-A3 (since they are ecall clobbered)
            // As a result kernel traps only happen in direct response to an ecall
            if (!((1ULL << i) & clobbered_registers))
            {
                if (task->regs[i] != gsp->regs[i])
                {
                    printf(
                        "r%02d: xscratch=%08llx %016llx clobbered -> %016llx (current ioctl=%d)\n", i,
                        gsp->xscratch, task->regs[i], gsp->regs[i], task->lwrrent_ioctl);
                    success = LW_FALSE;
                }
            }
        }
        if (!success)
            test_failed("Test failed.  User trap registers not preserved.");
        task->lwrrent_ioctl = 0;
    }
    break;

    default:
        printf("trap-depth %d\n\n", trap_depth);
        ASSERT(0 && "Kernel trap/mret constraints violated");
    }
    --trap_depth;
}

void validator_register_preservation::handler_trap(LwU64 new_mcause, LwU64 new_mbadaddr)
{
    // We're expecting a single store fault while in kernel mode
    if (trap_depth == 0)
    {
        memcpy(&task_state_map[gsp->xscratch].regs, gsp->regs, sizeof(gsp->regs));
    }
    else if (trap_depth >= 1)
    {
        kernel_mscratch = gsp->xscratch;
        memcpy(&kernel_trap_registers, &gsp->regs, sizeof(gsp->regs));
    }
    trap_mcause[trap_depth] = new_mcause;
    ++trap_depth;
}
#endif