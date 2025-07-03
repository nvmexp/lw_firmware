#include "kernel.h"

LIBOS_SECTION_IMEM_PINNED void KernelSbiTimerSet(LwU64 mtimecmp)
{
    register LwU64 sbi_extension_id __asm__("a7") = 0;
    register LwU64 sbi_function_id  __asm__("a6") = 0;
    register LwU64 sbi_mtimecmp_or_return_value __asm__("a0") = mtimecmp;

    // Caution! This SBI is non-compliant and clears GP and TP
    //          This works out as TP is guaranteed to be 0 while in the kernel.
    //          We don't lwrrently use GP in the kernel.

    __asm__ __volatile__("ecall"
        : "+r"(sbi_mtimecmp_or_return_value), "+r"(sbi_extension_id), "+r"(sbi_function_id)
        :
        : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "a1", "a2", "a3", "a4", "a5", "ra");

    if (sbi_mtimecmp_or_return_value != 0)
        KernelPanic();
}

LIBOS_SECTION_TEXT_COLD LIBOS_NORETURN void KernelSbiShutdown()
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
LIBOS_SECTION_TEXT_COLD LIBOS_NORETURN void KernelSbiPartitionSwitch
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
