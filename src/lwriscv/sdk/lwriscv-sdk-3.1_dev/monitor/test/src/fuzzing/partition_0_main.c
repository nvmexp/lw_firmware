#include <lw_riscv_address_map.h>
#include <stdint.h>
#include <lwmisc.h>
#include <liblwriscv/libc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <test.h>
#include <switch_to_param.h>

#define PARTITION_1_ID 1

static int entry = 0;
static int index = 0;

#define RAND_MODULAR 4294967295 // 0xFFFFFFFFUL
uint64_t SEED = 0xA5A51897ULL; // any non-zero number would do

// posix rand48()
static uint64_t _rand(void)
{
    SEED = 25214903917 * SEED + 11;
    return SEED % RAND_MODULAR;
}

static uint64_t random(uint64_t min, uint64_t max)
{
    return min + _rand() / (RAND_MODULAR / (max - min + 1) + 1);
}

int partition_0_main(void)
{
    if (entry == 0)
    {
        TEST_INIT("FUZZING");
        entry++;
    }
    else
    {
        SWITCHED(0);
    }

    uint64_t args[6];
    for (; index < 10; index++)
    {
        uint64_t num_args = random(0, 6);
        // extension do not exceed 0x8000_0000_0000_0000
        // as there's a hack existing in SK for PMU to hijack the U mode ecall handler in M mode
        int32_t ext = (int32_t)random(0, 0x7FFFFFFFULL);
        int32_t func = (int32_t)random(0, 0x7FFFFFFFULL);

        printf("fuzzy input: ext: 0x%x, funcid: 0x%x, num_args: %ld", ext, func, num_args);

        if (num_args != 0)
        {
            printf("\n");
            for (uint64_t i = 0; i < num_args; i++)
            {
                args[i] = random(0, 0x7FFFFFFFULL);
                printf("args[%ld]: 0x%lx ", i, args[i]);
            }
        }

        if ((ext == SBI_EXTENSION_SHUTDOWN) && (func = 0))
        {
            printf("skipping shutdown\n");
            --index;
            continue;
        }

        TEST(ext, func, num_args, args[0], args[1], args[2], args[3], args[4], args[5]);
    }

    TEST_END();

    // test shutdown at the end
    TEST(SBI_EXTENSION_SHUTDOWN, 0, 0);

    return 0;
}

void partition_0_trap_handler(void)
{
    uint64_t scause = csr_read(LW_RISCV_CSR_SCAUSE);
    uint64_t svalue = csr_read(LW_RISCV_CSR_STVAL);
    TEST_FAILED("Partition 0 Trap handler. cause: 0x%lx, value: 0x%lx\n", scause, svalue);
    return;
}
