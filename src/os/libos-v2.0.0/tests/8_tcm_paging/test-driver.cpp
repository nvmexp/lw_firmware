#include "../common/common.h"
#include "pageable_imem.h"

int main(int argc, const char *argv[])
{
    image firmware("firmware");

    static test_processor processor(
        &firmware, std::vector<std::string>{"LOGINIT", "LOGIMEM", "LOGDMEM"}, argc, argv);

    LwU64 pageable_text_start;
    if (!libosDebugResolveSymbolToVA(&firmware.resolver, "testFuncAsm", &pageable_text_start))
    {
        test_failed("cannot find testFuncAsm symbol");
        return 1;
    }

    LwU64 pageable_text_end          = pageable_text_start + (numPages * 4096);
    LwU32 pageable_instruction_count = 0;

    while (processor.cycle < 200000000)
    {
        LwU64 expected;
        LwU64 scratch;
        LwU64 pc = processor.pc;

        //
        // If pc falls into our pageable text section and is not due to the
        // initial imem page fault then check to confirm it's the expected offset.
        // There are two funnies in the code below to be wary of:
        //   - there are two instructions inserted at each offset in intrSeqOffsets[] (hence the /
        //   and % operations)
        //   - intrSeqOffsets[] values are in units of 32bits (so we need to multiply by 4 to get
        //   byte offset)
        //
        if (pc >= pageable_text_start && pc < pageable_text_end)
        {
            if (processor.translate_address_HAL(pc, scratch, LW_FALSE, LW_TRUE, LW_FALSE))
            {
                expected = instrSeqOffsets[pageable_instruction_count / 2];
                expected += (pageable_instruction_count % 2);
                expected *= 4;
                if ((pc - pageable_text_start) != expected)
                {
                    test_failed("Bad instruction offset");
                }
                pageable_instruction_count++;
            }
        }
        processor.step();
        processor.poll_logs();
    }
    printf("pageable_instruction_count = %d\n", pageable_instruction_count);

    return 0;
}
