#include "../common/common.h"
#include "../common/device-virtual-serial.h"
#include "peregrine-headers.h"

int main(int argc, const char *argv[])
{
    image firmware("firmware");

    static test_processor processor(&firmware, argc, argv);
    serial_virtual serial(&processor, LW_PRGNLCL_FALCON_DEBUGINFO);

    struct test_handler
    {
        LwU32 wfi_armed = 0;
        std::function<void(LwU64 addr, LwU32 value)> priv_write_raw;

        void handler_wfi() 
        {
            if (wfi_armed)
                processor.irqSet |= 1;              
            wfi_armed = 0;
        }

        void priv_write(LwU64 offset, LwU32 value) 
        {
            if (offset == LW_PRGNLCL_FALCON_DEBUGINFO && value == 0xBADF00D)  {
                wfi_armed = 1;
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
    while (!processor.stopped)
    {
        processor.step();
    }

    return 0;
}
