#include "../../debug/logdecode.h"
#include "../../include/libos_init_daemon.h"
#include "../../include/libos_log.h"
#include "../../kernel/interrupts.h"
#include "../common/common.h"
#include <deque>

int main(int argc, const char *argv[])
{
    image firmware("firmware");

    static test_processor processor(&firmware, std::vector<std::string>{"LOGINIT", "LOG1A"}, argc, argv);

    // Wait for processor suspend
    for (int i = 0; i < 4000000; i++)
    {
        if (processor.mailbox0 & LIBOS_INTERRUPT_PROCESSOR_SUSPENDED)
            break;
        processor.step();
        processor.poll_logs();
    }
    assert(processor.mailbox0 & LIBOS_INTERRUPT_PROCESSOR_SUSPENDED);

    assert(processor.privilege == privilege_machine ||
           processor.privilege == privilege_supervisor);

    fprintf(stdout, " Mailbox0 shows LIBOS has entered a suspended state.\n");
    fprintf(stdout, " <Resetting Processor> \n");

    processor.reset();
    for (int i = 0; i < 4000000; i++)
    {
        processor.step();
        processor.poll_logs();
    }

    return 0;
}
