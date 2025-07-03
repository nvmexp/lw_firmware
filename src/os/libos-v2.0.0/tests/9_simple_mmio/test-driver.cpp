#include "../common/common.h"

int main(int argc, const char *argv[])
{
    image firmware("firmware");

    static test_processor processor(&firmware, std::vector<std::string>{"LOGINIT", "LOGTEST"}, argc, argv);

    while (processor.cycle < 1000000)
    {
        processor.step();
        processor.poll_logs();
    }

    return 0;
}
