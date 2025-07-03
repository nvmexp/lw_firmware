#include "libos.h"
#include "libprintf.h"
#include "peregrine-headers.h"
#include "libos_xcsr.h"

LwU64 mmio = 0x400000;

void putc(char ch) {
  *(volatile LwU32 *)(mmio + LW_PRGNLCL_FALCON_DEBUGINFO - LW_RISCV_AMAP_INTIO_START) = ch;
  while (*(volatile LwU32 *)(mmio + LW_PRGNLCL_FALCON_DEBUGINFO - LW_RISCV_AMAP_INTIO_START))
    ;
}

__attribute__((used)) int main(LwU64 a0, LwU64 a1, LwU64 a2, LwU64 a3, LwU64 a4, LwU64 a5, LwU64 a6, LwU64 a7)
{
    // @todo: This should be passed through
    LibosPrintf("Worker task alive %lld %lld %lld %lld %lld %lld %lld %lld.\n", 
        a0, a1, a2, a3, a4, a5, a6, a7);


    LibosPrintf("Worker task exiting.\n");
    //*((volatile LwU64 *)0) = 0;

    return 0;
}




