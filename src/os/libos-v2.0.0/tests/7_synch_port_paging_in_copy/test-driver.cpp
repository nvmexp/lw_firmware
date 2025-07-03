#include "../common/common.h"

struct test_processor_paging_in_copy : public test_processor
{
    LwBool seen_fault_in_copy = LW_FALSE;

    test_processor_paging_in_copy(
        image *firmware, const std::vector<std::string> &log_names, int argc, const char *argv[])
        : test_processor(firmware, log_names, argc, argv)
    {
        event_mret.connect(&test_processor_paging_in_copy::handler_mret, this);
        event_ecall.connect(&test_processor_paging_in_copy::handler_ecall, this);
    }

    LwBool in_ecall = LW_FALSE;
    void   handler_ecall() { in_ecall = LW_TRUE; }

    void handler_mret() { in_ecall = LW_FALSE; }

    virtual void trap(LwU64 new_mcause, LwU64 new_mbadaddr)
    {
        if (privilege == privilege_machine && in_ecall)
            printf("    Page fault in kernel\n");
        test_processor::trap(new_mcause, new_mbadaddr);
    }
};

// TODO: main and test_failed should be in common test code
int main(int argc, const char *argv[])
{
    image firmware("firmware");

    static test_processor_paging_in_copy processor(
        &firmware, std::vector<std::string>{"LOGINIT", "LOGTEST"}, argc, argv);

    while (processor.cycle < 1000000)
    {
        processor.step();
        processor.poll_logs();
    }
    return 0;
}
