#include "../kernel/riscv-riscv-core.h"
#include "sdk/lwpu/inc/lwmisc.h"
#include "sdk/lwpu/inc/lwtypes.h"
#include <assert.h>
#include <drivers/common/inc/hwref/turing/tu102/dev_gsp_riscv_csr_64.h>
#include <drivers/common/inc/swref/turing/tu102/dev_riscv_csr_64_addendum.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory.h>
#include <sdk/lwpu/inc/lwtypes.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "../debug/elf.h"
extern "C" {
#include "../debug/lines.h"
}
#include "riscv-core.h-core.h"

int main(int argc, const char *argv[])
{
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.size() != 3)
    {
        fprintf(stderr, "riscv_core: image image.elf address\n");
        return 2;
    }

    std::vector<char> buffer;
    {
        std::ifstream filestream(args[0], std::ios::binary);
        if (filestream.fail())
        {
            fprintf(stderr, "Unable to open input file %s\n", args[0]);
            return 1;
        }

        filestream >> std::noskipws;
        std::copy(
            std::istreambuf_iterator<char>(filestream), std::istreambuf_iterator<char>(),
            std::back_inserter(buffer));
    }

    std::vector<char> elf;
    {
        std::ifstream filestream(args[1], std::ios::binary);
        if (filestream.fail())
        {
            fprintf(stderr, "Unable to open input file %s\n", args[1]);
            return 1;
        }

        filestream >> std::noskipws;
        std::copy(
            std::istreambuf_iterator<char>(filestream), std::istreambuf_iterator<char>(),
            std::back_inserter(elf));
    }

    static gsp processor;
    char *     end  = 0;
    LwU64      addr = strtoull((char *)args[2].c_str(), &end, 0);
    processor.pc    = addr;

    swap(processor.elf, elf);
    libosDebugResolverConstruct(&processor.resolver, (elf64_header *)(&processor.elf[0]));
    processor.loaded_address = addr;
    memcpy(&processor.memory[addr], &buffer[0], buffer.size());
    printf("riscv_core initialized with 1GB of RAM\n", args[0].c_str(), (unsigned long long)addr);
    printf("Loaded firmware %s at address %016llX\n", args[0].c_str(), (unsigned long long)addr);
    while (LW_TRUE)
    {
        processor.step();
    }
}
