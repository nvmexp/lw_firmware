/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#if defined SAMPLE_PROFILE_gsp
#include <dev_gsp.h>
#elif defined SAMPLE_PROFILE_pmu
#include <dev_pwr_pri.h>
#elif defined SAMPLE_PROFILE_minion
#include <dev_minion_ip.h>
#elif defined SAMPLE_PROFILE_sec
#include <dev_sec_pri.h>
#elif defined SAMPLE_PROFILE_fsp
#include <dev_fsp_pri.h>
#endif

#include <dev_top.h>

#include <stdint.h>
#include <lwmisc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <liblwriscv/shutdown.h>
#include <lwriscv/peregrine.h>

#include "tegra_cdev.h"
#include "rsa3kpss.h"
#include "ecdsa.h"
#include "gdma_test.h"

static void init(void)
{
    uint32_t dmemSize = 0;

    // Turn on tracebuffer
    localWrite(LW_PRGNLCL_RISCV_TRACECTL,
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _MODE, _STACK) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _MMODE_ENABLE, _TRUE) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _SMODE_ENABLE, _TRUE) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _UMODE_ENABLE, _TRUE));

#if !defined SAMPLE_PROFILE_fsp
    dmemSize = localRead(LW_PRGNLCL_FALCON_HWCFG3);
    dmemSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, dmemSize) << 8;
#else
    // TODO: Remove WAR when hardware bug 3072648 is fixed
    dmemSize = LWRISCV_DMEM_SIZE;
#endif
    // configure print to the start of DMEM
    printInit((uint8_t*)LW_RISCV_AMAP_DMEM_START + dmemSize - 0x1000, 0x1000, 0, 0);

#if ! defined SAMPLE_PROFILE_minion
    // Configure FBIF mode
    localWrite(LW_PRGNLCL_FBIF_CTL,
                (DRF_DEF(_PRGNLCL_FBIF, _CTL, _ENABLE, _TRUE) |
                 DRF_DEF(_PRGNLCL_FBIF, _CTL, _ALLOW_PHYS_NO_CTX, _ALLOW)));
    localWrite(LW_PRGNLCL_FBIF_CTL2,
                LW_PRGNLCL_FBIF_CTL2_NACK_MODE_NACK_AS_ACK);
#endif

    // Unlock IMEM and DMEM access
    localWrite(LW_PRGNLCL_FALCON_LOCKPMB, 0x0);

    // Release priv lockdown
    localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,
               DRF_DEF(_PRGNLCL, _RISCV_BR_PRIV_LOCKDOWN, _LOCK, _UNLOCKED));

    tegra_init_crypto_devices();
}

int main(void)
{
    uint32_t mailbox0_value = 0xabcd1234;
    init();

    printf("\n***** RSA3KPSS SIGNATURE VERIFICATION *****\n");
    if (rsa3kPssVerify() != 0) {
        mailbox0_value = 0xffffffff;
    }

    printf("\n\n***** ECDSA SIGNATURE GENERATION *****\n");
    if (verifyEcdsaP256Signing() != 0) {
        mailbox0_value = 0xfffffffe;
    };

    printf("\n\n***** GDMA test *****\n");
    if (gdma_test() != 0) {
        mailbox0_value = 0xfffffffd;
    }

    uint32_t platform = priRead(LW_PTOP_PLATFORM);

    printf("\nHello world. You are running on LW_PTOP_PLATFORM=%x, this binary was built on: %s.\n", platform, __DATE__ " " __TIME__);

#if defined SAMPLE_PROFILE_gsp
    priWrite(LW_PGSP_FALCON_MAILBOX0, mailbox0_value);
#elif defined SAMPLE_PROFILE_pmu
    priWrite(LW_PPWR_FALCON_MAILBOX0, mailbox0_value);
#elif defined SAMPLE_PROFILE_sec
    priWrite(LW_PSEC_FALCON_MAILBOX0, mailbox0_value);
#elif defined SAMPLE_PROFILE_fsp
    priWrite(LW_PFSP_FALCON_MAILBOX0, mailbox0_value);
#endif
    while(1);
    return 0;
}
