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

    processor.reset(false /* do not clear TCM */, true /* do clear MPU */);
    printf("Restarting CPU...\n");

    while (!processor.stopped)
    {
        processor.step();
    }

    return 0;
}
