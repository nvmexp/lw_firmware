/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   ssp.c
 * @brief  SSP functions.
 */

#include <riscv_csr.h>
#include <lwtypes.h>
#include <debug.h>

/*! The canary initialization functions have been borrowed from
 *  lwriscv/shlib/src/ssp/ssp.c
 */
#define SSP_MAGIC_NUMBER 0xD3F8EBA644B92C14

void * __stack_chk_guard __attribute__((section(".data.__stack_chk_guard"))) ;

__attribute__ ((noreturn))
void __stack_chk_fail(void)
{
    dbgExit(BOOTLOADER_ERROR_STACK_CORRUPTION);
}

static LwU64 get64BitRandomNumber(void)
{
    LwU64 randomNumber  = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
    LwU64 lowerBits     = randomNumber & 0xFFFF;

    randomNumber ^= ((lowerBits<<48) | (lowerBits<<32) |
                     (lowerBits<<16));

    return randomNumber;
}

static LwUPtr getTimeBasedRandomCanary(void)
{
    LwUPtr result = ((LwUPtr)get64BitRandomNumber());

    //
    // Need to XOR with SSP_MAGIC_NUMBER because get64BitRandomNumber
    // is not truly random
    //
    result ^= SSP_MAGIC_NUMBER;

    return result;
}

/*!
 Setup canary : Initializes the stack canary to a random value generated using
 ptimers
 */
__attribute__((__optimize__("no-stack-protector")))
void sspCanarySetup(void)
{
    __stack_chk_guard = (void *)getTimeBasedRandomCanary();
}
