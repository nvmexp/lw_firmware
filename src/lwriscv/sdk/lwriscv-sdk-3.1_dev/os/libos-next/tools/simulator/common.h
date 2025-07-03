#ifndef RISCV_COMMON
#define RISCV_COMMON
#include "libos-config.h"
#include "riscv-gdb.h"
#include "riscv-trace.h"
#include "register-preservation.h"
#include "stack-depth.h"
#include <deque>
#include <memory>
#include <string.h>
extern "C"{
#include "libelf.h"
#include "libdwarf.h"
}
#include "libos_partition.h"

void test_failed(const char *reason);

struct test_processor : public gsp
{
    // Exelwtion validators
    // validator_register_preservation                   register_preservation;
    validator_stack_depth stack_depth;

    LibosElfImage elfImage;

    std::unique_ptr<riscv_tracer> trace;
    std::unique_ptr<riscv_gdb> gdb;
    std::unique_ptr<trivial_memory> memory_elf;

    test_processor(
        image *firmware, 
        int argc,
        const char *argv[])
        : // register_preservation(this),
          gsp(firmware),
          stack_depth(this)
    {
      setvbuf(stdout, NULL, _IONBF, 0);


#if LIBOS_TINY
        LibosElf64Header * elf = (LibosElf64Header *)&firmware->elf[0];
        if (LibosOk != LibosElfImageConstruct(&elfImage, elf, firmware->elf.size()))
        {
            fprintf(stderr, "firmware.elf: Not a LIBOS ELF. \n");
            throw std::exception();
        }
        LibosElf64ProgramHeader * phdr = 0;

        while (phdr = LibosElfProgramHeaderNext(&elfImage, phdr))
            if (phdr->type == PT_LOAD) {
                for (int i = 0; i < phdr->filesz; i++) {
                    lwstom_memory * m = memory_for_address(phdr->paddr + i);
                    m->write8(phdr->paddr + i, *((LwU8 *)elf + phdr->offset + i));
                }
            }

        assert(LibosTinyElfGetBootEntry(&elfImage, &partition[0].entry));
#else
        printf("Loading boot image.\n");
        LwU64 paddr = LW_RISCV_AMAP_FBGPA_START;
        partition[0].entry = paddr;

        for (int i = 0; i < firmware->elf.size(); i++) {
            lwstom_memory * m = memory_for_address(paddr + i);
            assert(m);
            m->write8(paddr + i, firmware->elf[i]);
        }
#endif

        pc = partition[0].entry;
    }
};

#endif
