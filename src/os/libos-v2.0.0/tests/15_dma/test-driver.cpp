#include "../../include/libos_init_daemon.h"
#include "../../include/libos_log.h"
#include "../common/common.h"
#include <deque>

#include "../../debug/logdecode.h"

// TODO: main and test_failed should be in common test code
int main(int argc, const char *argv[])
{
    image firmware("firmware");

    static test_processor processor(&firmware, std::vector<std::string>{"LOGTEST"}, argc, argv);

    // Place 4kb of SYSMEM at the start of the SYSMEM base
    auto memory = std::unique_ptr<trivial_memory>(new trivial_memory(
        'SYS',                 // place log buffers in sysmem
        0x8180000000000000ULL, // must match scratchbuffer
        std::vector<LwU8>(65536, 0)));

    processor.memory_regions.push_back(&*memory);

    // Pass this memory in
    processor.append("SYS", memory->base, memory->block.size());
    processor.program_mailbox();

    while (processor.cycle < 100000000)
    {
        processor.step();
        processor.poll_logs();
    }

    return 0;
}
