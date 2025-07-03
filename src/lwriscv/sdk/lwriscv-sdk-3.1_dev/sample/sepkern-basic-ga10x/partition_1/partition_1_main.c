#if defined SAMPLE_PROFILE_gsp
#include <dev_gsp.h>
#elif defined SAMPLE_PROFILE_pmu
#include <dev_pwr_pri.h>
#elif defined SAMPLE_PROFILE_sec
#include <dev_sec_pri.h>
#endif

#include <dev_top.h>

#include <lw_riscv_address_map.h>
#include <stdint.h>
#include <lwmisc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>

#define TIME_CSR LW_RISCV_CSR_HPMCOUNTER_TIME
#define INSTRET_CSR LW_RISCV_CSR_HPMCOUNTER_INSTRET
#define CYCLE_CSR LW_RISCV_CSR_HPMCOUNTER_CYCLE

/*
 * arg0-arg4 are parameters passed by partition 0
 */
void partition_1_main(uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    uint64_t loc_in_partition_0 = 0x183183;
    uint64_t switch_time = (csr_read(TIME_CSR) - arg0) * 32;
    uint64_t switch_instr = (csr_read(INSTRET_CSR) - arg1);
    uint64_t switch_cycle = (csr_read(CYCLE_CSR) - arg2);

    printf("\n***************************************************************************\n");
    printf("I'm in partition 1 now! \n");
    printf("Switch time: %ldns; \n",            switch_time);
    printf("Switch instruction count: %ld; \n", switch_instr);
    printf("Switch cycle count: %ld \n\n",      switch_cycle);

    // Example priv access
#if defined SAMPLE_PROFILE_gsp
    priWrite(LW_PGSP_FALCON_MAILBOX0, 0xabcd1234);
#elif defined SAMPLE_PROFILE_pmu
    priWrite(LW_PPWR_FALCON_MAILBOX0, 0xabcd1234);
#elif defined SAMPLE_PROFILE_sec
    priWrite(LW_PSEC_FALCON_MAILBOX0, 0xabcd1234);
#endif

    printf("Let's try to access some Partition 0 memory and write to 0x%lX\n", loc_in_partition_0);
    *(uint64_t*)(loc_in_partition_0) = 123;

    printf("This should be unreachable, as the write above should fault\n");

    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);
}

void partition_1_trap_handler(void)
{
    uint64_t scause = csr_read(LW_RISCV_CSR_SCAUSE);
    uint64_t stval = csr_read(LW_RISCV_CSR_STVAL);

    printf("\nIn Partition 1 trap handler. \n");
    if (DRF_VAL64(_RISCV_CSR, _SCAUSE, _INT, scause) == 0) // We are expecting an exception
    {
        if (DRF_VAL64(_RISCV_CSR, _SCAUSE, _EXCODE, scause) == LW_RISCV_CSR_SCAUSE_EXCODE_SACC_FAULT)
        {
            printf("Store access fault as expected. stval == 0x%lX\n", stval);
        }
        else
        {
            printf("This is unexpected!!!\n");
        }
    }
    else
    {
        printf("Somehow got an interrupt. This is unexpected!!!\n");
    }

    printf("Shutting down\n");

    localWrite(LW_PRGNLCL_RISCV_ICD_CMD, 0);

    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);

    return;
}
