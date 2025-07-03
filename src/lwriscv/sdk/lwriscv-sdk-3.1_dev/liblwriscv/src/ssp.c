/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdint.h>
#include <stdbool.h>
#include <lwriscv/gcc_attrs.h>
#include <liblwriscv/shutdown.h>
#include <liblwriscv/ssp.h>
#if !LWRISCV_FEATURE_SSP_FORCE_SW_CANARY
#include <liblwriscv/scp.h>
#endif // LWRISCV_FEATURE_SSP_FORCE_SW_CANARY

#define CHECK_ERROR_OR_GOTO(X, Y)   \
({                                  \
    ret = X;                        \
    if (ret != LWRV_OK)             \
    {                               \
       goto Y;                      \
    }                               \
})

/*
 * For baremetal image, it can stay as is.
 * For multi-partition images, it likely nees to be written in some shared data
 * segment and then stored/restored on partition entry/exit
 */
GCC_ATTR_SECTION(".data.liblwriscv.__stack_chk_guard")
uintptr_t __stack_chk_guard  = 0;

#if !LWRISCV_FEATURE_SSP_FORCE_SW_CANARY
GCC_ATTR_SECTION(".data.liblwriscv.ssp_scpdma_buf")
GCC_ATTR_ALIGNED(SCP_RAND_SIZE) static uintptr_t ssp_scpdma_buf[SCP_RAND_SIZE / sizeof(uint64_t)] = {0,};
#endif

GCC_ATTR_NAKED
GCC_ATTR_NORETURN
GCC_ATTR_NO_SSP
void __stack_chk_fail(void)
{
#if LWRISCV_FEATURE_SSP_ENABLE_FAIL_HOOK
    __asm__("jal sspCheckFailHook");
#endif // LWRISCV_FEATURE_SSP_ENABLE_FAIL_HOOK
    __asm__("j riscvPanic");
}

GCC_ATTR_NO_SSP
void sspSetCanary(uintptr_t canary)
{
    __stack_chk_guard = canary;
}

uintptr_t sspGetCanary(void)
{
    return __stack_chk_guard;
}

#if !LWRISCV_FEATURE_SSP_FORCE_SW_CANARY
GCC_ATTR_NO_SSP
LWRV_STATUS sspGenerateAndSetCanaryWithInit(void)
{
    LWRV_STATUS ret = LWRV_OK;

    CHECK_ERROR_OR_GOTO(scpInit(SCP_INIT_FLAG_DEFAULT), out);

    CHECK_ERROR_OR_GOTO(scpConfigureRand(SCP_RAND_CONFIG_DEFAULT), out);

    CHECK_ERROR_OR_GOTO(scpStartRand(), out);

    CHECK_ERROR_OR_GOTO(sspGenerateAndSetCanary(), out);

    ret = scpShutdown();

out:
    return ret;

}

GCC_ATTR_NO_SSP
LWRV_STATUS sspGenerateAndSetCanary(void)
{
    LWRV_STATUS ret = LWRV_OK;
    uint32_t attempts=10; // Try 10 times to get proper number

    ssp_scpdma_buf[0] = 0;

    while (attempts--)
    {
        CHECK_ERROR_OR_GOTO(scpGetRand((uintptr_t)&ssp_scpdma_buf, sizeof(ssp_scpdma_buf), 0), out);

        if (ssp_scpdma_buf[0] != 0)
        {
            sspSetCanary(ssp_scpdma_buf[0]);
            goto out;
        }
    }

    ret = LWRV_ERR_TIMEOUT;

out:
    // Wipe buffer to avoid canary leak
    ssp_scpdma_buf[0] = 0;
    return ret;
}
#endif // LWRISCV_FEATURE_SSP_FORCE_SW_CANARY
