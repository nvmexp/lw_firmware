#define LIBOS_UNIT_TEST_BUDDY

#include "kernel.h"
#include "libos.h"
#include "task.h"
#include "diagnostics.h"
#include "libbit.h"
#include "lwriscv-2.0/sbi.h"
#include "libos_log.h"

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

__attribute__((aligned(8))) LwU8 src[64], dest[64], dest2[64];

void * memcpy(void *destination, const void *source, LwU64 n);

// The test runs directly in supervisor mode
__attribute__((used)) LIBOS_NORETURN  void KernelInit()  
{
    KernelMpuLoad();

    LibosPrintf("Hello world from bare metal.\n");
    
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 16; j++)
            for (int n = 0; n < 32; n++)
            {
                for (int x = 0; x < 64; x++)
                    dest[x] = 0x80 + x;
                for (int x = 0; x < 64; x++)
                    src[x] = 0x40 + x;
                memcpy(&dest[i], &src[j], n);

                for (int x = 0; x < 64; x++)
                    dest2[x] = 0x80 + x;
                for (int x = 0; x < n; x++)
                    dest2[x + i] = src[x+j];

                for (int x = 0; x < 64; x++)
                    KernelPanilwnless(dest2[x] == dest[x]);
                    
            }


    SeparationKernelShutdown();
}

void putc(char ch) {
    KernelMmioWrite(LW_PRGNLCL_FALCON_DEBUGINFO, ch);
    while (KernelMmioRead(LW_PRGNLCL_FALCON_DEBUGINFO))    
        ;
}


