/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

// Source for SSP is added unconditionally, so need to have this ifdef check
#ifdef IS_SSP_ENABLED

#include <lwrtos.h>
#include <lwtypes.h>
#include <riscv_csr.h>
#include <sections.h>

// TODO(suppal) : Remove when we get Security Engine (SE) hooked up to get random number
#define SSP_MAGIC_NUMBER 0xD3F8EBA644B92C14

sysKERNEL_DATA void *__kernel_stack_chk_guard;
sysKERNEL_DATA void *__task_stack_chk_guard;
sysSHARED_DATA void *__stack_chk_guard;

// Local implementation of SSP handler
__attribute__ ((noreturn))
void __stack_chk_fail(void)
{
    // Intentional invalid instruction
    asm volatile(".word 0");

    // Indicates to GCC that this function will not return
    __builtin_unreachable();
}

//
// Generate random number from time
// TODO(suppal) : Use Security Engine (SE) to get random number when available for GSP
//
static LwU64 get64BitRandomNumber(void)
{
    LwU64 randomNumber  = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
    LwU64 lowerBits     = randomNumber & 0xFFFF;

    randomNumber = ((randomNumber ^ (lowerBits<<48)) | (randomNumber ^ (lowerBits<<32)) |
                        (randomNumber ^ (lowerBits<<16)) | lowerBits);

    return randomNumber;
}

LwUPtr getTimeBasedRandomCanary(void)
{
    static LwBool bXorMagicNum = LW_FALSE;

    LwUPtr result = ((LwUPtr)(RANDOM_CANARY ^ get64BitRandomNumber()));

    //
    // Need to XOR with SSP_MAGIC_NUMBER because get64BitRandomNumber
    // is not truly random
    //
    if (bXorMagicNum)
    {
        result ^= SSP_MAGIC_NUMBER;
    }

    bXorMagicNum = !bXorMagicNum;

    return result;
}

// Setup canary for SSP
// Not used right now, but might be used by GSP in the future
__attribute__((__optimize__("no-stack-protector")))
void __canary_setup(void)
{
    //
    // We use separate canary values for kernel and tasks, since it
    // would be more secure from an adversarial point of view
    //
    __kernel_stack_chk_guard = (void *)(getTimeBasedRandomCanary());
    __task_stack_chk_guard   = (void *)(getTimeBasedRandomCanary());
    __stack_chk_guard        = __kernel_stack_chk_guard;
}

#endif
