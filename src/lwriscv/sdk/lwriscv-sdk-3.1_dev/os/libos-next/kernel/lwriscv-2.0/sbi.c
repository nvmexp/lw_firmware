#include "libos-config.h"
#include "peregrine-headers.h"
#include "sbi.h"

void SeparationKernelTimerSet(LwU64 mtimecmp)
{
    register LwU64 a7 __asm__("a7") = 0;
    register LwU64 a6 __asm__("a6") = 0;
    register LwU64 a0 __asm__("a0") = mtimecmp;

    // Caution! This SBI is non-compliant and clears GP and TP
    //          This works out as TP is guaranteed to be 0 while in the kernel.
    //          We don't lwrrently use GP in the kernel.

    __asm__ __volatile__("ecall"
        : "+r"(a0), "+r"(a7), "+r"(a6)
        :
        : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "a1", "a2", "a3", "a4", "a5", "ra");

    if (a0 != 0)
        SeparationKernelShutdown();
}

void SeparationKernelEntrypointSet(LwU64 targetPartitionId, LwU64 entryPointAddress)
{
    register LwU64 a7 __asm__("a7") = 0x90001EB;
    register LwU64 a6  __asm__("a6") = 1;
    register LwU64 a0 __asm__("a0") = targetPartitionId;
    register LwU64 a1 __asm__("a1") = entryPointAddress;

    __asm__ __volatile__("ecall"
        : "+r"(a0), "+r"(a1), "+r"(a7), "+r"(a6)
        : 
        : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "a2", "a3", "a4", "a5", "ra");

    if (a0 != 0)
        SeparationKernelShutdown();
}

void SeparationKernelPmpMap(
    LwU64         localPmpIndex,
    LwU64         targetPartitionId,
    LwU64         targetPmpEntryIndex,
    LwU64         pmpAccessMode,
    LwU64         address,
    LwU64         size)
{
    register LwU64 a7 __asm__("a7") = 0x90001EB;
    register LwU64 a6 __asm__("a6") = 3 /* @bug: We need assigned values from SK */;
    register LwU64 a0 __asm__("a0") = targetPartitionId;
    register LwU64 a1 __asm__("a1") = localPmpIndex;
    register LwU64 a2 __asm__("a2") = targetPmpEntryIndex;
    register LwU64 a3 __asm__("a3") = pmpAccessMode;
    register LwU64 a4 __asm__("a4") = address;
    register LwU64 a5 __asm__("a5") = size;

    __asm__ __volatile__("ecall"
        : "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3), "+r"(a4), "+r"(a5), "+r"(a7), "+r"(a6)
        : 
        : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "ra");

    if (a0 != 0)
        SeparationKernelShutdown();
}


__attribute__((noreturn,used)) void SeparationKernelShutdown()
{
    register LwU64 sbi_extension_id __asm__("a7") = 8;
    register LwU64 sbi_function_id  __asm__("a6") = 0;
    __asm__ __volatile__("ecall"  :: "r"(sbi_extension_id), "r"(sbi_function_id));    
    __builtin_unreachable();
}

/**
 * @brief This function performs an SBI call to the separation kernel to request
 * a partition switch. The interface is taken from
 * https://p4viewer/get///sw/lwriscv/docs/monitor/build/html5/sk_sadd.html#_switch_to.
 *
 * @param[in]   param1          The 1st param to be passed to the partition.
 * @param[in]   param2          The 2nd param to be passed to the partition.
 * @param[in]   param3          The 3rd param to be passed to the partition.
 * @param[in]   param4          The 4th param to be passed to the partition.
 * @param[in]   param5          The 5th param to be passed to the partition.
 * @param[in]   partition_id    The ID of the partition to switch to.
 *
 * @return This function does not return.
 */
__attribute__((noreturn)) void SeparationKernelPartitionSwitch
(
    LwU64 param1,
    LwU64 param2,
    LwU64 param3,
    LwU64 param4,
    LwU64 param5,
    LwU64 partition_id
)
{
    register LwU64 a0 __asm__("a0") = param1;
    register LwU64 a1 __asm__("a1") = param2;
    register LwU64 a2 __asm__("a2") = param3;
    register LwU64 a3 __asm__("a3") = param4;
    register LwU64 a4 __asm__("a4") = param5;
    register LwU64 a5 __asm__("a5") = partition_id;
    register LwU64 a6 __asm__("a6") = 0;
    register LwU64 a7 __asm__("a7") = 0x090001EB;
    __asm__ __volatile__(
        "ecall"
        :: "r"(a0), "r"(a1), "r"(a2), "r"(a3),
           "r"(a4), "r"(a5), "r"(a6), "r"(a7)
    );    
    __builtin_unreachable();
}

static __attribute__((used)) LwU64 separationCallRegisters[32];
__attribute__((used)) volatile LwBool StartInContinuation = LW_FALSE;
__attribute__((used)) void SeparationKernelPartitionContinuation();

__attribute__((used)) LibosStatus SeparationKernelPartitionCall
(
    LwU64 partition_id,
    LwU64 arguments[5]
)
{
    register LwU64 a0 __asm__("a0") = arguments[0];
    register LwU64 a1 __asm__("a1") = arguments[1];
    register LwU64 a2 __asm__("a2") = arguments[2];
    register LwU64 a3 __asm__("a3") = arguments[3];
    register LwU64 a4 __asm__("a4") = arguments[4];
    register LwU64 a5 __asm__("a5") = partition_id;
    register LwU64 a6 __asm__("a6") = 0;
    register LwU64 a7 __asm__("a7") = 0x090001EB;
    
    StartInContinuation = LW_TRUE;
    
    __asm__ __volatile__(
        "la       gp, separationCallRegisters;"       
        "sd       s0, 8*8(gp);"
        "sd       s1, 9*8(gp);"
        "sd       s2, 18*8(gp);"
        "sd       s3, 19*8(gp);"
        "sd       s4, 20*8(gp);"
        "sd       s5, 21*8(gp);"
        "sd       s6, 22*8(gp);"
        "sd       s7, 23*8(gp);"
        "sd       s8, 24*8(gp);"
        "sd       s9, 25*8(gp);"
        "sd       s10, 26*8(gp);"
        "sd       s11, 27*8(gp);"
        "sd       ra, 1*8(gp);"
        "sd       sp, 2*8(gp);"
        "ecall;"
        ".global SeparationKernelPartitionContinuation;"
        /* Continuation function must be called by start.s when StartInContinuation is true */
        "SeparationKernelPartitionContinuation: ;"
        "la       gp, separationCallRegisters;"
        "ld       s0, 8*8(gp);"
        "ld       s1, 9*8(gp);"
        "ld       s2, 18*8(gp);"
        "ld       s3, 19*8(gp);"
        "ld       s4, 20*8(gp);"
        "ld       s5, 21*8(gp);"
        "ld       s6, 22*8(gp);"
        "ld       s7, 23*8(gp);"
        "ld       s8, 24*8(gp);"
        "ld       s9, 25*8(gp);"
        "ld       s10, 26*8(gp);"
        "ld       s11, 27*8(gp);"
        "ld       ra, 1*8(gp);"
        "ld       sp, 2*8(gp);"

        : "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3), "+r"(a4), "+r"(a5), "+r"(a6), "+r"(a7)
        : 
        : "t0", "t1", "t2", "t3", "t4", "t5", "t6",  "ra", "memory");
    
    // Ensure we came back from the partition in question
    // before pulling any data
    StartInContinuation = LW_FALSE;
    if (a5 != partition_id) 
        return LibosErrorSpoofed;
    
    arguments[0] = a0;
    arguments[1] = a1;
    arguments[2] = a2;
    arguments[3] = a3;
    arguments[4] = a4;
    return LibosOk;
}
