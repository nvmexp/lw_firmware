#pragma once
#include "libbootfs.h"
struct AddressSpace;

typedef struct {
    LibosBootfsRecord * elf;
} Elf;

LibosStatus KernelElfOpen(const char * elfName, Elf * * outElf);
LibosStatus KernelElfMap(struct AddressSpace * asid, LibosBootfsRecord * elfRecord);
void KernelInitElfLoader();