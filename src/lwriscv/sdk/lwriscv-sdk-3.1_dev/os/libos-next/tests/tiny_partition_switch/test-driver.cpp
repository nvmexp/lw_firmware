#include "../common/common.h"
#include "../common/device-virtual-serial.h"
#include "peregrine-headers.h"

int main(int argc, const char *argv[])
{
    image firmware("firmware.elf");

    static test_processor processor(&firmware, argc, argv);
    serial_virtual serial(&processor, LW_PRGNLCL_FALCON_DEBUGINFO, &firmware);

    while (!processor.shutdown)
    {
        processor.step();
    }

    return 0;
}
