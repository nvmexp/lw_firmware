#include "../common/common.h"

int main(int argc, const char *argv[])
{
    image firmware("firmware");

    static test_processor processor(&firmware, std::vector<std::string>{"LOGINIT", "LOGTEST"}, argc, argv);

    fprintf(stdout, "Loaded firmware at address %016llX\n", (unsigned long long)processor.elf_address);
    for (LwU32 steps = 0; steps < 4000000; steps++)
    {
        processor.step();
        processor.poll_logs();
    }

    return 0;
}
