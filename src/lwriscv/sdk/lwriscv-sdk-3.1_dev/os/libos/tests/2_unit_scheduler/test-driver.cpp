#include "../common/common.h"
#include "../common/device-virtual-serial.h"
#include "peregrine-headers.h"

int main(int argc, const char *argv[])
{
    image firmware("firmware");

    static test_processor processor(&firmware, argc, argv);
    serial_virtual serial(&processor, LW_PRGNLCL_FALCON_DEBUGINFO);

    while (!processor.stopped)
    {
        processor.step();
    }
    printf("Test terminated.\n");

    return 0;
}
