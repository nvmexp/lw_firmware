#include "../common/common.h"
#include "drivers/common/inc/hwref/ampere/ga102/dev_gsp_riscv_csr_64.h"
#include "test-registers.h"
// TODO: main and test_failed should be in common test code
int main(int argc, const char *argv[])
{
    image firmware("firmware");

    static test_processor processor(
        &firmware, std::vector<std::string>{"LOGINIT", "LOG"}, argc, argv);

    struct test_handler
    {
        LwU32 wfi_armed = 0;
        std::function<void(LwU64 addr, LwU32 value)> priv_write_raw;

        void handler_wfi() 
        {
            processor.irqSet |= wfi_armed;              
            wfi_armed = 0;
        }

        void priv_write(LwU64 offset, LwU32 value) 
        {
            if (offset == LW_PTEST_RAISE_INTERRUPT_ON_NEXT_WFI) {
                wfi_armed = value;
            }
            else if (offset == LW_PTEST_RAISE_INTERRUPT) {
                processor.irqSet |= value;              
            }
            else
                priv_write_raw(offset, value);
        }
    };

    test_handler callback;

    processor.event_wfi.connect(&test_handler::handler_wfi, &callback);
    callback.priv_write_raw = processor.priv_write_HAL;
    processor.priv_write_HAL = bind_hal(&test_handler::priv_write, &callback);

    fprintf(stdout, "Loaded firmware at address %016llX\n", (unsigned long long)processor.elf_address);
    while (processor.cycle < 100000)
    {
        processor.step();
        processor.poll_logs();
    }

    return 0;
}
