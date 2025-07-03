/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PORT_SYSCALL_H
#define PORT_SYSCALL_H

#ifndef __ASSEMBLER__
# include <lwtypes.h>

/* List of syscalls required by RISC-V Port.
 * More syscalls may be added by application, but those are required by port layer. */

/* TODO: Hack to allow M-mode to redirect syscalls back to S-mode */
# define LW_SYSCALL_FLAG_SMODE              (1ULL << 63)
#else // __ASSEMBLER__
/* in ASM this is redefined to 0 to use li, we use 1<<63 separately manually */
# define LW_SYSCALL_FLAG_SMODE              (0)
#endif // __ASSEMBLER__

/* First syscall, below are SBI calls */
#define LW_SYSCALL_FIRST                    (0x10 | LW_SYSCALL_FLAG_SMODE)
#define LW_SYSCALL_YIELD                    (LW_SYSCALL_FIRST + 0x00)
#define LW_SYSCALL_TASK_EXIT                (LW_SYSCALL_FIRST + 0x01)
#define LW_SYSCALL_SET_KERNEL               (LW_SYSCALL_FIRST + 0x03)
#define LW_SYSCALL_SET_USER                 (LW_SYSCALL_FIRST + 0x04)
#define LW_SYSCALL_SET_KERNEL_INTR_DISABLED (LW_SYSCALL_FIRST + 0x05)
#define LW_SYSCALL_INTR_DISABLE             (LW_SYSCALL_FIRST + 0x06)
#define LW_SYSCALL_INTR_ENABLE              (LW_SYSCALL_FIRST + 0x07)
#define LW_SYSCALL_VIRT_TO_PHYS             (LW_SYSCALL_FIRST + 0x09)
#define LW_SYSCALL_DMA_TRANSFER             (LW_SYSCALL_FIRST + 0x0A)
#define LW_SYSCALL_CONFIGURE_FBIF           (LW_SYSCALL_FIRST + 0x0B)

/* That is number at which application should start syscalls */
#define LW_SYSCALL_APPLICATION_START        (LW_SYSCALL_FIRST + 0x10)

#ifndef __ASSEMBLER__
/* Wrappers for syscalls, used by everyone */

/* Explanation:
 * For now we force inline of syscall code because it may be called within
 * kernel context.
 * We have to notify compiler that all caller-saved registers may be cleared.
 * That way, syscall handling code don't have to save them on context switch,
 * because compiler will save (only used) registers and treat syscall like
 * ordinary function call.
 * It should give us performance imporovement (at least some registers will not
 * be saved). */

#define SYSCALL __attribute__( ( always_inline ) ) inline

#define CLOBBER_TEMP "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory"

SYSCALL LwU64 syscall0( LwU64 num )
{
    register LwU64 a7 __asm__( "a7" ) = num;
    register LwU64 a0 __asm__( "a0" );
    __asm__ volatile ("ecall" : "+r"( a0 ), "+r"( a7 ) :: "ra", CLOBBER_TEMP, "a1", "a2", "a3", "a4", "a5", "a6");
    return a0;
}

SYSCALL LwU64 syscall1( LwU64 num, LwU64 arg0 )
{
    register LwU64 a7 __asm__( "a7" ) = num;
    register LwU64 a0 __asm__( "a0" ) = arg0;
    __asm__ volatile ("ecall" : "+r"( a0 ), "+r"( a7 ) :: "ra", CLOBBER_TEMP, "a1", "a2", "a3", "a4", "a5", "a6" );
    return a0;
}

SYSCALL LwU64 syscall2( LwU64 num, LwU64 arg0, LwU64 arg1 )
{
    register LwU64 a7 __asm__( "a7" ) = num;
    register LwU64 a0 __asm__( "a0" ) = arg0;
    register LwU64 a1 __asm__( "a1" ) = arg1;
    __asm__ volatile ("ecall" : "+r"( a0 ), "+r"( a7 ), "+r"( a1 ) :: "ra", CLOBBER_TEMP, "a2", "a3", "a4", "a5", "a6");
    return a0;
}

SYSCALL LwU64 syscall3( LwU64 num, LwU64 arg0, LwU64 arg1, LwU64 arg2 )
{
    register LwU64 a7 __asm__( "a7" ) = num;
    register LwU64 a0 __asm__( "a0" ) = arg0;
    register LwU64 a1 __asm__( "a1" ) = arg1;
    register LwU64 a2 __asm__( "a2" ) = arg2;
    __asm__ volatile ("ecall" : "+r"( a0 ), "+r"( a7 ), "+r"( a1 ), "+r"( a2 ) :: "ra", CLOBBER_TEMP, "a3", "a4", "a5", "a6");
    return a0;
}

SYSCALL LwU64 syscall4( LwU64 num, LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3 )
{
    register LwU64 a7 __asm__( "a7" ) = num;
    register LwU64 a0 __asm__( "a0" ) = arg0;
    register LwU64 a1 __asm__( "a1" ) = arg1;
    register LwU64 a2 __asm__( "a2" ) = arg2;
    register LwU64 a3 __asm__( "a3" ) = arg3;
    __asm__ volatile ("ecall" : "+r"( a0 ), "+r"( a7 ), "+r"( a1 ), "+r"( a2 ), "+r"( a3 ) :: "ra", CLOBBER_TEMP, "a4", "a5", "a6");
    return a0;
}

SYSCALL LwU64 syscall5( LwU64 num, LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4 )
{
    register LwU64 a7 __asm__( "a7" ) = num;
    register LwU64 a0 __asm__( "a0" ) = arg0;
    register LwU64 a1 __asm__( "a1" ) = arg1;
    register LwU64 a2 __asm__( "a2" ) = arg2;
    register LwU64 a3 __asm__( "a3" ) = arg3;
    register LwU64 a4 __asm__( "a4" ) = arg4;
    __asm__ volatile ("ecall" : "+r"( a0 ), "+r"( a7 ), "+r"( a1 ), "+r"( a2 ), "+r"( a3 ), "+r"( a4 ) :: "ra", CLOBBER_TEMP, "a5", "a6");

    return a0;
}

SYSCALL LwU64 syscall6( LwU64 num, LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4, LwU64 arg5 )
{
    register LwU64 a7 __asm__( "a7" ) = num;
    register LwU64 a0 __asm__( "a0" ) = arg0;
    register LwU64 a1 __asm__( "a1" ) = arg1;
    register LwU64 a2 __asm__( "a2" ) = arg2;
    register LwU64 a3 __asm__( "a3" ) = arg3;
    register LwU64 a4 __asm__( "a4" ) = arg4;
    register LwU64 a5 __asm__( "a5" ) = arg5;
    __asm__ volatile ("ecall" : "+r"( a0 ), "+r"( a7 ), "+r"( a1 ), "+r"( a2 ), "+r"( a3 ), "+r"( a4 ), "+r"( a5 ) :: "ra", CLOBBER_TEMP, "a6");

    return a0;
}

SYSCALL LwU64 syscall7( LwU64 num, LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4, LwU64 arg5, LwU64 arg6 )
{
    register LwU64 a7 __asm__( "a7") = num;
    register LwU64 a0 __asm__( "a0") = arg0;
    register LwU64 a1 __asm__( "a1") = arg1;
    register LwU64 a2 __asm__( "a2") = arg2;
    register LwU64 a3 __asm__( "a3") = arg3;
    register LwU64 a4 __asm__( "a4") = arg4;
    register LwU64 a5 __asm__( "a5") = arg5;
    register LwU64 a6 __asm__( "a6") = arg6;
    __asm__ volatile ( "ecall" : "+r"( a0 ), "+r"( a7 ), "+r"( a1 ), "+r"( a2 ), "+r"( a3 ), "+r"( a4 ), "+r"( a5 ), "+r"( a6 ) :: "ra", CLOBBER_TEMP );

    return a0;
}

#undef SYSCALL
#undef CLOBBER_TEMP

#endif // __ASSEMBLER__

#endif /* PORT_SYSCALL_H */
