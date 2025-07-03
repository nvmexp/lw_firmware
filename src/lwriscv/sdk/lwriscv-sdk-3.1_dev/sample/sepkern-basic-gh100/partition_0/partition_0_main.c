#include <lw_riscv_address_map.h>
#include <stdint.h>
#include <lwmisc.h>
#include <liblwriscv/libc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>

#define TIME_CSR LW_RISCV_CSR_HPMCOUNTER_TIME
#define INSTRET_CSR LW_RISCV_CSR_HPMCOUNTER_INSTRET
#define CYCLE_CSR LW_RISCV_CSR_HPMCOUNTER_CYCLE

#define PARTITION_1_ID 1

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

void mysleep(unsigned uSec)
{
    uint64_t r = csr_read(TIME_CSR) + uSec * 1000 ;
    uint64_t a = 0;

    while (a < r)
    {
        a = csr_read(TIME_CSR);
    }
}

int partition_0_main(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6, uint64_t arg7)
{
    uint64_t sbiVersion = arg0;
    uint64_t misa = arg1;
    uint64_t marchId = arg2;
    uint64_t mimpd = arg3;
    uint64_t mvendorid = arg4;
    uint64_t mfetchattr = arg5;
    uint64_t mldstattr = arg6;
    uint64_t zero = arg7;

    init();

    printf("Hello from Partition 0!\n");
    printf("Printing initial arguments set by SK: \n");

    printf("a0 - SBI Version: 0x%lX \n",  sbiVersion);
    printf("a1 - misa: 0x%lX \n",         misa);
    printf("a2 - marchid: 0x%lX \n",      marchId);
    printf("a3 - mimpd: 0x%lX \n",        mimpd);
    printf("a4 - mvendorid: 0x%lX \n",    mvendorid);
    printf("a5 - mfetchattr: 0x%lX \n",   mfetchattr);
    printf("a6 - mldstattr: 0x%lX \n",    mldstattr);
    printf("a7 - always zero: 0x%lX; \n", zero);

    // Calling Set Timer SBI to write into mtimecmp
    csr_set(LW_RISCV_CSR_SIE, DRF_NUM64(_RISCV, _CSR_SIE, _STIE, 1));
    csr_set(LW_RISCV_CSR_SSTATUS, DRF_DEF64(_RISCV, _CSR_SSTATUS, _SIE, _ENABLE));
    sbicall1(SBI_EXTENSION_SET_TIMER, 0, csr_read( TIME_CSR ) + 5 );
    mysleep(10);

    printf("This should be unreachable, enter ICD to debug\n");
    localWrite(LW_PRGNLCL_RISCV_ICD_CMD, 0);

    printf("Now just shutdown\n");
    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);

    return 0;
}

void partition_0_trap_handler(void)
{
    uint64_t scause = csr_read(LW_RISCV_CSR_SCAUSE);

    printf("\nIn partition 0 trap handler. \n");

    if (DRF_VAL64(_RISCV_CSR, _SCAUSE, _INT, scause))
    {
        if (DRF_VAL64(_RISCV_CSR, _SCAUSE, _EXCODE, scause) == LW_RISCV_CSR_SCAUSE_EXCODE_S_TINT)
        {
            printf("Timer interrupt delivered\n");

            // Just to clear SIP call Set Timer SBI with very large value
            sbicall1(SBI_EXTENSION_SET_TIMER, 0, LW_U64_MAX );

            printf("\nAnd now switch to partition 1\n");

            /*
             * The following 'switch_to' SBI call allows up to 5 opaque parameters to be passed using a0-a4 argument registers
             * a5 argument register is used for partition ID we're switching to
             */
            sbicall6(SBI_EXTENSION_LWIDIA,
                     SBI_LWFUNC_FIRST,
                     csr_read(TIME_CSR),
                     csr_read(INSTRET_CSR),
                     csr_read(CYCLE_CSR),
                     0,
                     0,
                     PARTITION_1_ID);

            return;
        }
    }

    printf("Something unexpected happened.\n");

    printf("Enter ICD first.\n");
    localWrite(LW_PRGNLCL_RISCV_ICD_CMD, 0);

    printf("Just shutdown\n");
    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);

    return;
}
