#ifndef RISCV_COMMON
#define RISCV_COMMON

#include "../../debug/logdecode.h"
#include "../../include/libos_log.h"
#include "../../simulator/riscv-gdb.h"
#include "../../simulator/riscv-trace.h"
#include "../exelwtion-validation/register-preservation.h"
#include "../exelwtion-validation/stack-depth.h"
#include <deque>
#include <memory>
#include <string.h>
#include "libelf.h"
#include "libdwarf.h"

void test_failed(const char *reason);

struct test_processor : public gsp
{
    // Exelwtion validators
    // validator_register_preservation                   register_preservation;
    validator_stack_depth stack_depth;

    LibosDebugResolver resolver = {0};
    std::unique_ptr<riscv_tracer> trace;
    std::unique_ptr<riscv_gdb> gdb;
    std::unique_ptr<trivial_memory> memory_elf;

    test_processor(
        image *firmware, int argc,
        const char *argv[])
        : // register_preservation(this),
          gsp(firmware),
          stack_depth(this)
    {
      // Handle arguments
      for (int i = 1; i < argc; i++)
          if (!strcmp(argv[i], "--gdb"))
              gdb = std::make_unique<riscv_gdb>(this, false);
          else if (!strcmp(argv[i], "--ddd"))
              gdb = std::make_unique<riscv_gdb>(this, true);
#ifdef TRACE
          else if (!strcmp(argv[i], "--trace"))
              trace = std::make_unique<riscv_tracer>(this);
          else
              assert(0);
#endif

        LibosElf64Header * elf = (LibosElf64Header *)&firmware->elf[0];
        LibosElf64ProgramHeader * phdr = 0;
        while (phdr = LibosElfNextPhdr(elf, phdr))
            if (phdr->type == PT_LOAD) {
                printf("Loading ELF section VA=%p to PA=%p\n", phdr->vaddr, phdr->paddr);
                for (int i = 0; i < phdr->filesz; i++) {
                    lwstom_memory * m = memory_for_address(phdr->paddr + i);
                    m->write8(phdr->paddr + i, *((LwU8 *)elf + phdr->offset + i));
                }
            }

        assert(LibosElfGetBootEntry(elf, &resetVector));
        pc = resetVector;
        LibosDebugResolverConstruct(&resolver, elf);
    }
};

#endif
