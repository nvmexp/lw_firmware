#include <dev_top.h>

#include <lw_riscv_address_map.h>
#include <stdint.h>
#include <lwmisc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <test.h>
#include <switch_to_param.h>

/*
 * arg0-arg4 are parameters passed by partition 0
 */
int partition_1_main(void)
{

    SWITCHED(1);

    sbicall6(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FIRST, 0, 0, 0, 0, 0, 0);

    return 0;
}

void partition_1_trap_handler(void)
{
    uint64_t scause = csr_read(LW_RISCV_CSR_SCAUSE);
    uint64_t svalue = csr_read(LW_RISCV_CSR_STVAL);
    TEST_FAILED("Partition 1 Trap handler. cause: 0x%lx, value: 0x%lx\n", scause, svalue);
    return;
}
