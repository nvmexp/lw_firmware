#include "../common/common.h"

struct test_processor_log_tester : public test_processor
{
    test_processor_log_tester(image *firmware, int argc, const char *argv[])
        : test_processor(firmware, std::vector<std::string>{"LOGINIT"}, argc, argv)
    {
    }

    void poll_logs_and_skip_some()
    {
        static int skip        = 0;
        static LwU64 shadowPut = 0;

        LwU64 lwrrentPut = *log_decoder.log[0].physicLogBuffer;
        if (lwrrentPut != shadowPut)
        {
            shadowPut = lwrrentPut;
            skip--;
        }

        if (skip > 0)
            return;

        skip = rand() % 512;

        libosExtractLogs(&log_decoder, LW_FALSE);
    }
};

// TODO: main and test_failed should be in common test code
int main(int argc, const char *argv[])
{
    image firmware("firmware");

    static test_processor_log_tester processor(&firmware, argc, argv);

    while (processor.cycle < 4000000)
    {
        processor.step();
        processor.poll_logs_and_skip_some();
    }

    return 0;
}
