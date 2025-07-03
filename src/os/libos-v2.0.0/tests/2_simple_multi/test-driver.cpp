#include "../common/common.h"

// TODO: main and test_failed should be in common test code
int main(int argc, const char *argv[])
{
    image firmware("firmware");

    static test_processor processor(
        &firmware, std::vector<std::string>{"LOGINIT", "LOG1A", "LOG1B", "LOG2A", "LOG2B"}, argc, argv);

    fprintf(stdout, "Loaded firmware at address %016llX\n", (unsigned long long)processor.elf_address);
    while (processor.cycle < 4000000)
    {
        processor.step();
        processor.poll_logs();
    }

    return 0;
}
