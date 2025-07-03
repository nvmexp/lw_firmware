/*
    RISCV-64 prelink baking tool
        Accepts same inputs as LD
        Runs bake() function at compile time (all code must exist in .compile-time.* sections)
        Has ABI to malloc() memory from specific sections

    How it works
        - We link the ELF with the provided command line
        - We execute malloc() statements and provide donor memory from the end of the address space
        - We generate a .C file declaring the symbols we need
        - We link again
        - We run the script a final time and ensure the memory is available


*/
#include "hal.h"
#include "riscv-core.h"
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>
#include <string>
#include <algorithm>
#include <exception>
#include "libos-config.h"
#include "peregrine-headers.h"
#include "riscv-trace.h"

using namespace std;

void saveFile(const std::string &filename, const std::vector<LwU8> &content)
{
    std::ofstream f(filename, ios::binary);
    if (!f.is_open())
        throw runtime_error(strerror(errno) + (": " + filename));
    copy(content.begin(), content.end(), ostreambuf_iterator<char>(f));
}

struct baker : riscv_core
{
    virtual void soft_reset()
    {
        riscv_core::reset();
    }

    std::vector<std::shared_ptr<trivial_memory> > ownedMemory;
    baker(image * firmware) 
        :  riscv_core(firmware)
    {
        privilege = privilege_supervisor;

        for (LibosElf64ProgramHeader * phdr = LibosElfProgramHeaderNext(&firmware->elfImage, 0);
             phdr;
             phdr = LibosElfProgramHeaderNext(&firmware->elfImage, phdr)) 
        {
            LwU64 regionStart = phdr->vaddr &~ (TlbLineSize-1);
            LwU64 regionEnd = (phdr->vaddr + phdr->memsz + TlbLineSize - 1)&~ (TlbLineSize-1);
            auto n = std::make_shared<trivial_memory>('PHDR0'+memory_regions.size(),regionStart, regionEnd - regionStart);
            LwU8 * data = (LwU8 *)firmware->elfImage.elf + phdr->offset;
            for (uint8_t i = 0; i < phdr->filesz; i++) 
                n->write8(phdr->vaddr+i, data[i]);
            ownedMemory.push_back(n);
            memory_regions.push_back(&*n);
            printf(" mapped %llx-%llx\n", phdr->vaddr, phdr->vaddr+phdr->memsz);
        }

        //mmioBase              = 0;
        has_supervisor_mode   = true;
    }

    void save(const char * outputFile) {
        // Copy the phdrs into an array as we're going to be resizing the elf
        std::vector<LibosElf64ProgramHeader> phdrRegions;
        for (LibosElf64ProgramHeader * phdr = LibosElfProgramHeaderNext(&firmware->elfImage, 0);
             phdr;
             phdr = LibosElfProgramHeaderNext(&firmware->elfImage, phdr)) 
        {
            phdrRegions.push_back(*phdr);
        }

        LwU64 phdrIndex = 0;
        for (auto & i : phdrRegions)
        {
            if (i.flags & PF_W)
            {
                // Did we grow the section?
                LwU64 newExtent = 0;
                for (LwU64 j = 0; j < i.memsz; j++)
                    if (ownedMemory[phdrIndex]->read8(i.vaddr + j))
                        newExtent = j + 1;

                printf("Committing %llx-%llx\n", i.vaddr, i.vaddr+newExtent);

                if (!LibosElfCommitData(&firmware->elfImage, i.vaddr, newExtent)) {
                    printf("Unable to grow phdr\n.");
                    exit(2);
                }

                // Writeback the data
                LwU8 * bytes = (LwU8*)firmware->elfImage.map(&firmware->elfImage, i.offset, i.memsz);
                for (uint64_t va = i.vaddr; va < i.vaddr + i.memsz; va++) 
                    bytes[va-i.vaddr] = ownedMemory[phdrIndex]->read8(va);
            }
            phdrIndex++;
        }        

        saveFile(outputFile, std::vector<LwU8>((LwU8*)firmware->elfImage.elf, (LwU8*)firmware->elfImage.elf+firmware->elfImage.size));
    }

    virtual LwU64 allocateAddress(LwU64 size) {

        std::vector<std::pair<uint64_t, uint64_t> > sorted_regions;
        for (auto & i : memory_regions) {
            trivial_memory * tm = (trivial_memory *)i;
            sorted_regions.push_back(std::pair<uint64_t, uint64_t>(tm->base, tm->base+tm->limit));
        }

        std::sort(sorted_regions.begin(), sorted_regions.end());

        // Don't allow allocations near null
        LwU64 trial = 0x100000;
        for (auto & i : sorted_regions)
        {
            if (trial < i.first && (i.first - trial) >= size)
                return trial;
            trial = i.second;
        }

        if ((-1ULL - trial) < size)
            throw std::runtime_error("Failed to allocate memory");
        
        return trial;
    }

    virtual LwBool translate_address(LwU64 va, LwU64 & pa, TranslationKind & mask) {
        pa = va;
        mask = (TranslationKind)(
               translateSupervisorLoad | 
               translateSupervisorStore |
               translateSupervisorExelwte);
        return LW_TRUE;
    }

    // Disable all CSRs
    virtual LwBool csr_validate_access(LwU32 addr) { return LW_FALSE; }

    void pmp_assert_partition_has_access(LwU64 physical, LwU64 size, TranslationKind kind) {}
    void update_interrupts() {}
};

void riscv_core::sbi()
{
    switch(regs[RISCV_A0])
    {
        // Terminate
        case 0: 
            shutdown = true;
            break;

    }
    return;
}

void help()
{
    printf("    \n\n");
    printf("usage: bake5 [input-elf] -o [output-elf] {--trace}\n");
    printf("options: \n");
    printf("  -h      : Print this help screen.\n");
}

int compile(const char * input_file, const char *output_file, bool trace)
{
    auto riscvImage = image((std::string)input_file);
    baker core(&riscvImage);

    // Create the core tracer if the user specified it
    std::shared_ptr<riscv_tracer> tracer;
    if (trace)
        tracer = std::make_unique<riscv_tracer>(&core);
    
    // Set the entry point
    if (!LibosDebugResolveSymbolToVA(&riscvImage.resolver, "bake", &core.pc)) {
        printf("You must have a bake() function to execute.\n");
        exit(1);
    }

    // Allocate a stack
    LwU64 stackBase = core.allocateAddress(65536);
    auto stackMemory = std::make_shared<trivial_memory>('STAC', stackBase, 65536);
    core.memory_regions.push_back(&*stackMemory);
    core.regs[RISCV_SP] = stackBase + 65536;

    while (!core.stopped && !core.shutdown)
    {
        core.step();
    }    

    if (core.stopped && !core.shutdown) {
        printf("Re-run with --trace to debug (or bug us to add support for our gdb stub).");
        return 1;
    }

    // Start the writeback process
    core.save(output_file);

    return 0;
}

int main(int argc, const char *argv[])
{
    const char * input_file = 0, *output_file = 0;
    bool trace = false;
    for (uint32_t i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-h")==0) {
                help();
                exit(1);
            } else if (strcmp(argv[i], "-o")==0) {
                output_file = argv[++i];
            }
            else if (strcmp(argv[i], "--trace")==0) {
                trace = true;
                ++i;
            }
            else
            {
                fprintf(stderr, "%s: Unknown option '%s'\n", argv[0], argv[i]);
                help();
                exit(1);
            }
        }
        else {
            if (input_file) {
                fprintf(stderr, "%s: Too many input files '%s'\n", argv[0], argv[i]);
                help();
                exit(1);
            }
            input_file = argv[i];
        }
    }

    if (!input_file) {
        fprintf(stderr, "%s: No input ELF\n", argv[0]);
        help();
        exit(1);
    }

    if (!output_file) {
        fprintf(stderr, "%s: No output ELF\n", argv[0]);
        help();
        exit(1);
    }

    return compile(input_file, output_file, trace);
}

