#include "../common/common.h"
#include "../common/device-virtual-serial.h"
#include "peregrine-headers.h"
#include "kernel/lwriscv-2.0/sbi.h"

int main(int argc, const char *argv[])
{
    image firmware("firmware");
    firmware.load_symbols("kernel.elf");
    firmware.load_symbols("init.elf");

    static test_processor processor(&firmware, argc, argv);
    serial_virtual serial(&processor, LW_PRGNLCL_FALCON_DEBUGINFO, &firmware);

    // Create PMP entry for init partition
    processor.partitionIndex = 0;
    processor.partition[0].pmp[0].begin = 0;
    processor.partition[0].pmp[0].end = 0xFFFFFFFFFFFFFFFFULL;
    processor.partition[0].pmp[0].pmpAccess = PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE;

    while (!processor.shutdown)
    {
        processor.step();
    }

    return 0;
}
