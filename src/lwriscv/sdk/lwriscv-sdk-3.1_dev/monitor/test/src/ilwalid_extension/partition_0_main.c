#include <lw_riscv_address_map.h>
#include <stdint.h>
#include <lwmisc.h>
#include <liblwriscv/libc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <test.h>

#define TEST_MAX_EXTENSION 0x10
#define TEST_MAX_FUNCID 0x10

int partition_0_main(void)
{
    TEST_INIT("SBI_ILWALID_EXTENSION");

    // pick some invalid ones to test, covering all combo will take too long
    for (int32_t extension = 0; extension < TEST_MAX_EXTENSION; extension++)
    {
        // skip valid extensions
        if ((extension == SBI_EXTENSION_SHUTDOWN) ||
            (extension == SBI_EXTENSION_SET_TIMER) ||
            (extension == SBI_EXTENSION_LWIDIA))
        {
            continue;
        }

        for (int32_t funcid = 0; funcid < TEST_MAX_FUNCID; funcid++)
        {
            TEST(extension, funcid, 0);
        }
    }

    TEST_END();

    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);

    return 0;
}

void partition_0_trap_handler(void)
{
    uint64_t scause = csr_read(LW_RISCV_CSR_SCAUSE);
    uint64_t svalue = csr_read(LW_RISCV_CSR_STVAL);
    TEST_FAILED("Trap handler. cause: 0x%lx, value: 0x%lx\n", scause, svalue);
    return;
}
