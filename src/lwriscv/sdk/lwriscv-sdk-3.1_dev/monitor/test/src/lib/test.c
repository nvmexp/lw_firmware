#include "test.h"
#include <stdarg.h>
#include <lwmisc.h>
#include <lwriscv/sbi.h>
#include <lwriscv/csr.h>
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>

#define SBI_EXTENSION_TEST 0x74657374

uint32_t num_tests;

void TEST(int32_t extension, int32_t funcid, uint64_t num_args, ...)
{
    num_tests++;

    va_list ptr;
    va_start(ptr, num_args);

    uint64_t args[6];

    for (uint64_t i = 0; i < num_args; i++)
    {
        args[i] = va_arg(ptr, uint64_t);
    }
    va_end(ptr);

    switch (extension)
    {
        case SBI_EXTENSION_SET_TIMER:
            test_set_timer(funcid, num_args, args);
            break;
        case SBI_EXTENSION_SHUTDOWN:
            test_shutdown(funcid);
            break;
        case SBI_EXTENSION_LWIDIA:
            switch (funcid)
            {
                case 0:
                    test_switch_to(funcid, num_args, args);
                    break;
                case 1:
                    test_release_priv_lockdown(funcid);
                    break;
                case 2:
                    test_update_tracectl(funcid, num_args, args);
                    break;
                case 3:
                    test_fbif_transcfg(funcid, num_args, args);
                    break;
                case 4:
                    test_fbif_regioncfg(funcid, num_args, args);
                    break;
                case 5:
                    test_tfbif_transcfg(funcid, num_args, args);
                    break;
                case 6:
                    test_tfbif_regioncfg(funcid, num_args, args);
                    break;
            }
            break;
        default:
            test_ilwalid_extension(extension, funcid, num_args, args);
            break;
    };
}

void TEST_INIT(const char* test)
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

    printf("TEST_START: %s\n", test);

    num_tests = 0;
}

void TEST_END(void)
{
    printf("\nTEST_END: %d tests\n", num_tests);
}

int test_get_mcsr(uint32_t csr_addr, uint64_t* value)
{
    SBI_RETURN_VALUE rc = sbicall1(SBI_EXTENSION_TEST, 0, csr_addr);

    if (rc.error != SBI_SUCCESS) return -1;

    *value = (uint64_t)(rc.value);
    return 0;
}
