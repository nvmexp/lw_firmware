/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#if defined LWRISCV_IS_ENGINE_gsp
#include <dev_gsp.h>
#elif defined LWRISCV_IS_ENGINE_pmu
#include <dev_pwr_pri.h>
#elif defined LWRISCV_IS_ENGINE_sec
#include <dev_sec_pri.h>
#endif

#include <dev_top.h>

#include <stdint.h>
#include <lwmisc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <liblwriscv/shutdown.h>
#include <lwriscv/peregrine.h>

uint32_t dmemSize = 0;

static void init(void)
{
    // Turn on tracebuffer
    localWrite(LW_PRGNLCL_RISCV_TRACECTL,
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _MODE, _STACK) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _MMODE_ENABLE, _TRUE) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _SMODE_ENABLE, _TRUE) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _UMODE_ENABLE, _TRUE));

    dmemSize = localRead(LW_PRGNLCL_FALCON_HWCFG3);
    dmemSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, dmemSize) << 8;

    // configure print to the start of DMEM
    printInit((uint8_t*)LW_RISCV_AMAP_DMEM_START + dmemSize - 0x1000, 0x1000, 0, 0);

    // Configure FBIF mode
    localWrite(LW_PRGNLCL_FBIF_CTL,
                (DRF_DEF(_PRGNLCL_FBIF, _CTL, _ENABLE, _TRUE) |
                 DRF_DEF(_PRGNLCL_FBIF, _CTL, _ALLOW_PHYS_NO_CTX, _ALLOW)));
    localWrite(LW_PRGNLCL_FBIF_CTL2,
                LW_PRGNLCL_FBIF_CTL2_NACK_MODE_NACK_AS_ACK);

    // Unlock IMEM and DMEM access
    localWrite(LW_PRGNLCL_FALCON_LOCKPMB, 0x0);

    // Release priv lockdown
    localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,
               DRF_DEF(_PRGNLCL, _RISCV_BR_PRIV_LOCKDOWN, _LOCK, _UNLOCKED));
}

int main(void)
{
    init();

    uint32_t platform = priRead(LW_PTOP_PLATFORM);

    printf("Hello world. You are running on LW_PTOP_PLATFORM=%x, this binary was built on: %s. ", platform, __DATE__ " " __TIME__);

    // Example priv access
#if defined LWRISCV_IS_ENGINE_gsp
    priWrite(LW_PGSP_FALCON_MAILBOX0, 0xabcd1234);
#elif defined LWRISCV_IS_ENGINE_pmu
    priWrite(LW_PPWR_FALCON_MAILBOX0, 0xabcd1234);
#elif defined LWRISCV_IS_ENGINE_sec
    priWrite(LW_PSEC_FALCON_MAILBOX0, 0xabcd1234);
#endif

    riscvShutdown();
    return 0;
}
