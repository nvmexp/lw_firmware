#include "libriscv.h"
#include <lwtypes.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    LwU32 * buffer;

    //
    //  Read the input file
    //
    FILE * f = fopen("test.bin", "rb");
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    rewind(f);

    buffer = (LwU32 *)malloc(fsize);
    if (fread(buffer, 1, fsize, f) != fsize) {
        fprintf(stderr, "Error reading test.bin\n");
        exit(1);
    }
    fclose(f);

    LwU64 pc = 0;
    LibosRiscvInstruction i = {0};
    while (pc < fsize) {
        LwU64 nextpc = libosRiscvDecode(*(LwU32*)((LwU8 *)buffer + pc), pc, &i);
        libosRiscvPrint(pc, &i);
        assert(nextpc);
        pc = nextpc;
    }

    return 0;
}