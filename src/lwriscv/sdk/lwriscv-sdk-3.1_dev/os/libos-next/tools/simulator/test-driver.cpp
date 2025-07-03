#include "common.h"
#include "device-virtual-serial.h"
#include "peregrine-headers.h"
#include "kernel/lwriscv-2.0/sbi.h" // for PMP_ACCESS_MODE_READ

int main(int argc, const char *argv[])
{
    // Handle arguments
    std::string imageName;
    std::vector<std::string> symbols;
    bool gdb = false, ddd = false, trace = false;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--gdb"))
            gdb = true;
        else if (!strcmp(argv[i], "--ddd"))
            ddd = true;
        else if (!strcmp(argv[i], "--trace"))
            trace = true;
        else
        {
            if (imageName.empty())
                imageName = argv[i];
            else
                symbols.push_back(argv[i]);
        }

    image firmware(imageName.c_str());
    for (int i = 0; i < symbols.size(); i++)
        firmware.load_symbols(symbols[i]);

    static test_processor processor(&firmware, argc, argv);

    if (gdb)
        processor.gdb = std::make_unique<riscv_gdb>(&processor, false);
    
    if (ddd)
        processor.gdb = std::make_unique<riscv_gdb>(&processor, true);

    if (trace)
        processor.trace = std::make_unique<riscv_tracer>(&processor);

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

    printf("Test terminated.\n");
    return 0;
}
