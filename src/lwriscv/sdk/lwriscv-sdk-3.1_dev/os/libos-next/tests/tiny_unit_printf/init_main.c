#define LIBOS_UNIT_TEST_BUDDY
#include "libos-config.h"
#include "kernel.h"
#include "libos.h"
#include "task.h"
#include "diagnostics.h"
#include "mm/buddy.h"
#include "lwriscv-2.0/sbi.h"
#include "libos_log.h"

LwU32 LIBOS_SECTION_IMEM_PINNED rand() {
    static LwU64 LIBOS_SECTION_DMEM_PINNED(seed) = 1;
    seed = seed * 6364136223846793005 + 1442695040888963407;
    return (LwU32)(seed >> 32);
}


Task LIBOS_SECTION_DMEM_PINNED(TaskInit);

void KernelSchedulerDirtyPagetables(void) {}
Task *      KernelSchedulerGetTask() { return &TaskInit; }


void write64(LwU64 value)
{
    KernelMmioWrite(LW_PRGNLCL_FALCON_DEBUGINFO, value >> 32);
    while (KernelMmioRead(LW_PRGNLCL_FALCON_DEBUGINFO))    
        ;
    KernelMmioWrite(LW_PRGNLCL_FALCON_DEBUGINFO, value & 0xFFFFFFFF);
    while (KernelMmioRead(LW_PRGNLCL_FALCON_DEBUGINFO))    
        ;  
}

void LibosLogTokens(const libosLogMetadata * metadata, const LibosPrintfArgument * token, LwU64 count)
{
    write64((LwU64)metadata | 0x8000000000000000ULL);         
    for (int i = 0; i < count; i++)
        write64(token[i].i);         
}

void KernelBootstrap();

// The test runs directly in supervisor mode
__attribute__((used)) LIBOS_NORETURN  void KernelInit()  
{
    KernelMpuLoad();

    LibosPrintf("Hello world from bare metal.\n");

    //LibosPrintf("%s\n", "Embedded string");
    LibosPrintf("%d\n", 1024);
    LibosPrintf("%d\n", -1024);
    LibosPrintf("%i\n", 1024);
    LibosPrintf("%i\n", -1024);
    LibosPrintf("%u\n", 1024u);
    LibosPrintf("%u\n", -1024u);
    LibosPrintf("%o\n", 0777u);
    LibosPrintf("%o\n", -0777u);
    LibosPrintf("%x\n", 0x1234abcdu);
    LibosPrintf("%x\n", -0x1234abcdu);
    LibosPrintf("%X\n", 0x1234abcdu);
    LibosPrintf("%X\n", -0x1234abcdu);
    LibosPrintf("%c\n", 'x');
    LibosPrintf("%%\n");

    LibosPrintf("%+d\n", 1024);
    LibosPrintf("%+d\n", -1024);

    LibosLog(LOG_LEVEL_WARNING, "Hello from binary logger %lld %d %d\n",-123456789123456789LL,2,3);
    
    // @todo: Re-enable this test case when we have a good method of forcing this function to exist
    //        at a fixed address
    //LibosLog(LOG_LEVEL_WARNING, "Function: %p\n", KernelBootstrap);

    SeparationKernelShutdown();
}

void putc(char ch) {
    KernelMmioWrite(LW_PRGNLCL_FALCON_DEBUGINFO, ch);
    while (KernelMmioRead(LW_PRGNLCL_FALCON_DEBUGINFO))    
        ;
}


