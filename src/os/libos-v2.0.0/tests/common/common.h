#ifndef RISCV_COMMON
#define RISCV_COMMON

#include "../../debug/logdecode.h"
#include "../../include/libos_log.h"
#include "../../kernel/interrupts.h"
#include "../../simulator/riscv-gdb.h"
#include "../../simulator/riscv-trace.h"
#include "../exelwtion-validation/register-preservation.h"
#include "../exelwtion-validation/stack-depth.h"
#include "include/libos_init_args.h"
#include <deque>
#include <memory>
extern "C" {
#include "../../debug/lines.h"
}

void test_failed(const char *reason);

struct test_processor : public gsp
{
    // FB allocation
    LwU64 heap_pointer = 0;

    // Exelwtion validators
    // validator_register_preservation                   register_preservation;
    validator_stack_depth stack_depth;

    libosDebugResolver resolver;
    std::unique_ptr<riscv_tracer> trace;
    std::unique_ptr<riscv_gdb> gdb;
    std::unique_ptr<trivial_memory> memory_elf, memory_heap;
    std::vector<std::unique_ptr<trivial_memory>> log_buffers;
    LIBOS_LOG_DECODE log_decoder;

    // Init
    trivial_memory init_buffer;
    LibosMemoryRegionInitArgument *tail;

    void mark_end() { tail->id8 = 0; }

    void program_mailbox()
    {
        // Point hardware at the init buffer
        mailbox0 = init_buffer.base & 0xFFFFFFFF;
        mailbox1 = init_buffer.base >> 32;
    }

    void append(const char *id8, LwU64 pa, LwU64 size)
    {
        LwU64 id = 0, c;
        while ((c = (LwU8)*id8++))
            id = (id << 8) | c;
        tail->id8  = id;
        tail->pa   = pa;
        tail->size = size;
        ++tail;
        mark_end();
    }

    LwU64 allocate_framebuffer(LwU64 size)
    {
        LwU64 start = heap_pointer;
        heap_pointer += size;
        heap_pointer = (heap_pointer + 4095) & ~4095;
        return start + gsp::fb_for_chip(chip);
    }

    chip_t detect_chip(int argc, const char ** argv)
    {
        chip_t chip = tu11x;

        // Handle arguments
        for (int i = 1; i < argc; i++)
            if (!strcmp(argv[i], "--arch=ga100"))
                chip = ga100;
            else if (!strcmp(argv[i], "--arch=tu11x"))
                chip = tu11x;
            else if (!strcmp(argv[i], "--arch=ga102"))
                chip = ga102;

        return chip;
    }

    test_processor(
        image *firmware, const std::vector<std::string> &log_buffer_names, int argc,
        const char *argv[])
        : // register_preservation(this),
          gsp(detect_chip(argc, argv), firmware),
          stack_depth(this),
          init_buffer('FB', allocate_framebuffer(4096), std::vector<LwU8>(4096, 0))
    {
        // Detect chip and wire HALs
        chip_t chip = tu11x;

        // Handle arguments
        for (int i = 1; i < argc; i++)
            if (!strcmp(argv[i], "--gdb"))
                gdb = std::make_unique<riscv_gdb>(this, false);
            else if (!strcmp(argv[i], "--ddd"))
                gdb = std::make_unique<riscv_gdb>(this, true);
            else if (!strcmp(argv[i], "--arch=ga100") ||
                     !strcmp(argv[i], "--arch=tu11x") ||
                     !strcmp(argv[i], "--arch=ga102"))
                /* Handled in detect_chip */;
#ifdef TRACE
            else if (!strcmp(argv[i], "--trace"))
                trace = std::make_unique<riscv_tracer>(this);
            else
                assert(0);
#endif

        // Init message construction
        memory_regions.push_back(&init_buffer);
        tail = (LibosMemoryRegionInitArgument *)&init_buffer.block[0];
        mark_end();

        LwU64 block = allocate_framebuffer(firmware->elf.size() + firmware->additional_heap);

        // Initialize the heap
        memory_heap =
            std::make_unique<trivial_memory>('FB', block, std::vector<LwU8>(firmware->additional_heap, 0));
        memory_regions.push_back(&*memory_heap);

        // Load the ELF
        set_elf_address(block + firmware->additional_heap);
        memory_elf = std::make_unique<trivial_memory>('FB', elf_address, firmware->elf);
        memory_regions.push_back(&*memory_elf);
        // Write the heap header right before the ELF header
        memory_heap->write64(
            memory_heap->base + firmware->additional_heap - 16, 0x51CB7F1D /* LIBOS_MAGIC */);
        memory_heap->write64(memory_heap->base + firmware->additional_heap - 8, firmware->additional_heap);

        // Set the bootvector
        pc = resetVector = memory_elf->base + firmware->entry_offset;

        libosDebugResolverConstruct(&resolver, (elf64_header *)&firmware->elf[0]);


        libosLogCreate(&log_decoder);

        // Register the log buffers
        for (auto &name : log_buffer_names)
        {
            auto memory = std::unique_ptr<trivial_memory>(new trivial_memory(
                'SYS', // place log buffers in sysmem
                this->allocate_framebuffer(4096), std::vector<LwU8>(4096, 0)));
            append(name.c_str(), memory->base, memory->block.size());
            program_mailbox();
            memory_regions.push_back(&*memory);
            libosLogAddLog(&log_decoder, &memory->block[0], memory->block.size(), 0, name.c_str());
            log_buffers.push_back(std::move(memory));
        }

        libosLogInit(&log_decoder, (elf64_header *)&firmware->elf[0]);
    }

    ~test_processor() { libosLogDestroy(&log_decoder); }
    void poll_logs() { libosExtractLogs(&log_decoder, LW_FALSE); }
};

#endif
