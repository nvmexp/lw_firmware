/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdint.h>
#include <lwmisc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/libc.h>
#include <liblwriscv/print.h>
#include <liblwriscv/shutdown.h>
#include <lwriscv/csr.h>
#include <lwriscv/gcc_attrs.h>
#include <lwriscv/sbi.h>

#include <tegra_cdev.h>
#include <tegra_lwpka_gen.h>

#include <dev_fsp_pri.h>
#include <dev_top.h>
#include LWRISCV64_MANUAL_ADDRESS_MAP

#define PARTITION_MGMT_ID 1

GCC_ATTR_SECTION(".bss.shared") char buffer_boot[4096];

static void init(void)
{
    uint32_t dmemSize = 0;

    dmemSize = localRead(LW_PRGNLCL_FALCON_HWCFG3);
    dmemSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, dmemSize) << 8;

    // configure print to the start of DMEM
    printInit((LwU8*)LW_RISCV_AMAP_DMEM_START + dmemSize - 0x1000, 0x1000, 0, 0);

    // Unlock IMEM and DMEM access
    localWrite(LW_PRGNLCL_FALCON_LOCKPMB, 0x0);

    // Release priv lockdown
    localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,
               DRF_DEF(_PRGNLCL, _RISCV_BR_PRIV_LOCKDOWN, _LOCK, _UNLOCKED));
}

int partition_boot_main
(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5,
    uint64_t arg6,
    uint64_t arg7
)
{
    if (arg0 == 0xdeadbeefULL)
    {
        printf("Switched from mgmt partition, going back...\n");
        sbicall6(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FIRST,
            0xcafebabeULL, 5, 6, 7, 8, PARTITION_MGMT_ID);

        printf("This should be unreachable.\n");
        riscvShutdown();
    }

    uint64_t sbiVersion = arg0;
    uint64_t misa = arg1;
    uint64_t marchId = arg2;
    uint64_t mimpd = arg3;
    uint64_t mvendorid = arg4;
    uint64_t mfetchattr = arg5;
    uint64_t mldstattr = arg6;
    uint64_t zero = arg7;

    init();

    printf("Hello from Partition boot!\n");
    printf("Printing initial arguments set by SK: \n");
    
    printf("a0 - SBI Version: 0x%lX \n",  sbiVersion);
    printf("a1 - misa: 0x%lX \n",         misa);
    printf("a2 - marchid: 0x%lX \n",      marchId);
    printf("a3 - mimpd: 0x%lX \n",        mimpd);
    printf("a4 - mvendorid: 0x%lX \n",    mvendorid);
    printf("a5 - mfetchattr: 0x%lX \n",   mfetchattr);
    printf("a6 - mldstattr: 0x%lX \n",    mldstattr);
    printf("a7 - always zero: 0x%lX; \n", zero);

    engine_t *engine;
    status_t status;

    printf("Initializing crypto devices...");
    status = init_crypto_devices();
    if (status != NO_ERROR)
    {
        printf("fatal: init_crypto_devices: 0x%x\n", status);
        riscvShutdown();
    }
    printf("Done!\n");

    printf("Selecting engine...");
    status = ccc_select_engine((const engine_t **) &engine, ENGINE_CLASS_RSA, ENGINE_PKA);
    if (status != NO_ERROR)
    {
        printf("fatal: select_engine: 0x%x\n", status);
        riscvShutdown();
    }
    printf("Done!\n");

    printf("Locking engine...");
    status = lwpka_acquire_mutex(engine);
    if (status != NO_ERROR)
    {
        printf("fatal: lwpka_acquire_mutex: 0x%x\n", status);
        riscvShutdown();
    }
    printf("Done!\n");

    printf("Unlocking engine...");
    lwpka_release_mutex(engine);
    printf("Done!\n");

    printf("Writing to shared buffer...");
    buffer_boot[0] = 'A';
    buffer_boot[1] = 'l';
    buffer_boot[2] = 'o';
    buffer_boot[3] = 'h';
    buffer_boot[4] = 'a';
    buffer_boot[5] = '!';
    buffer_boot[6] = '\0';
    printf("Done!\n");

    printf("\nAnd now switch to partition mgmt\n");
    sbicall6(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FIRST,
        0xf007ba11ULL, 1, 2, 3, 4, PARTITION_MGMT_ID);

    printf("This should be unreachable.\n");
    riscvShutdown();

    return 0;
}

void partition_boot_trap_handler(void) 
{
    printf("\nIn partition boot trap handler. \n");
    printf("Something unexpected happened. Just shutdown\n");
    riscvShutdown();
}
