#define LIBOS_UNIT_TEST_BUDDY

#include "kernel.h"
#include "libos.h"
#include "task.h"
#include "diagnostics.h"
#include "libbit.h"
#include "lwriscv-2.0/sbi.h"
#include "libos_log.h"

LwU32  erandom32()
{
    static LwU32 state[16] = { 94, 15, 96, 76, 86, 179, 18, 91, 204, 20,238,247,231,217,208,189 };
    static LwU32 index = 0;
    LwU32 a, b, c, d;

    a = state[index];
    c = state[(index+13)&15];
    b = a^c^(a<<16)^(c<<15);
    c = state[(index+9)&15];
    c ^= (c>>11);
    a = state[index] = b^c;
    d = a^((a<<5)&0xDA442D24UL);
    index = (index + 15)&15;
    a = state[index];
    state[index] = a^b^d^(a<<2)^(b<<18)^(c<<28);
    return state[index];
}

LwU64 erandom64() {
    return erandom32() | ((erandom32()*1ULL)<<32);
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

// The test runs directly in supervisor mode
__attribute__((used)) LIBOS_NORETURN  void KernelInit()  
{
    KernelMpuLoad();

    LibosPrintf("Hello world from bare metal.\n");

    // LibosLog2GivenPow2
    for (int i = 0; i < 64; i++)
        KernelPanilwnless(LibosLog2GivenPow2(1ULL << i) == i);

    // libosNextPower2 & libosLog2
    for (int i = 0; i < 64; i++) {
        KernelPanilwnless(LibosBitRoundUpPower2(1ULL << i) == (1ULL << i));
        KernelPanilwnless(LibosBitHighest(1ULL << i) == (1ULL << i));
    }

    // Test all functions across sums of powers of 2
    for (int i = 0; i < 64; i++) 
        for (int j = 0; j < i-1; j++) {
            LwU64 v = (1ULL << i);
            
            KernelPanilwnless(LibosBitRoundUpPower2(v - (1ULL << j)) == v);
            KernelPanilwnless(LibosBitHighest(v + (1ULL << j)) == v);
            KernelPanilwnless(LibosBitLowest(v + (1ULL << j)) == (1ULL << j));
            KernelPanilwnless(LibosBitLog2Ceil(v + (1ULL << j)) == (i+1));
            KernelPanilwnless(LibosBitLog2Floor(v + (1ULL << j)) == i);
        }

    for (int i = 0; i < 10000; i++) 
    {
        LwU64 rnd = erandom64();

        if  (rnd > 0x8000000000000000ULL) {
            KernelPanilwnless(LibosBitRoundUpPower2(rnd) == 0);
            KernelPanilwnless(LibosBitLog2Ceil(rnd) == 64);
        }
        else 
        {
            KernelPanilwnless(LibosBitRoundUpPower2(rnd) == (1ULL << LibosBitLog2Ceil(rnd)));
            KernelPanilwnless((LibosBitLog2Ceil(rnd) - LibosBitLog2Floor(rnd)) <= 1);
            KernelPanilwnless(rnd <= LibosBitRoundUpPower2(rnd));
            KernelPanilwnless(rnd > LibosBitRoundUpPower2(rnd)/2);
        }
    }

    SeparationKernelShutdown();
}

void putc(char ch) {
    KernelMmioWrite(LW_PRGNLCL_FALCON_DEBUGINFO, ch);
    while (KernelMmioRead(LW_PRGNLCL_FALCON_DEBUGINFO))    
        ;
}


