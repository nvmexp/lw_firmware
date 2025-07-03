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

__attribute__((used)) int main(LwU64 messageId, LwU64 handleCount, LibosPortHandle sendPort, LwU64 sendPortGrant, LibosPortHandle recvPort, LwU64 recvPortGrant, LwU64 a6, LwU64 a7)
{
    LibosPrintf("Worker task alive %lld %lld %d %lld %d %lld %lld %lld.\n", 
        messageId, handleCount, sendPort, sendPortGrant, recvPort, recvPortGrant, a6, a7);

    LibosPortSyncRecv(recvPort, 0, 0, 0, LibosTimeoutInfinite, 0);

    LibosPrintf("Worker task exiting.\n");

    return 0;
}




