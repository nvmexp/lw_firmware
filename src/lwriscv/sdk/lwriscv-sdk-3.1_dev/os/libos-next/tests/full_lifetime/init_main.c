#include "libos.h"
#include "libprintf.h"
#include "peregrine-headers.h"
#include "libos_xcsr.h"
#include "libriscv.h"
#include "libos_ipc.h"

int main(LwU8 selfPartitionId);

LwU64 mmio = 0x400000;

void putc(char ch) {
  *(volatile LwU32 *)(mmio + LW_PRGNLCL_FALCON_DEBUGINFO - LW_RISCV_AMAP_INTIO_START) = ch;
  while (*(volatile LwU32 *)(mmio + LW_PRGNLCL_FALCON_DEBUGINFO - LW_RISCV_AMAP_INTIO_START))
    ;
}


__attribute__((used)) int main(LwU8 selfPartitionId)
{
    LibosRegionHandle region;
    LibosAddressSpaceMapPhysical(
        LibosDefaultAddressSpace, 
        mmio, 
        LW_RISCV_AMAP_INTIO_START, 
        LW_RISCV_AMAP_INTIO_END - LW_RISCV_AMAP_INTIO_START, 
        LibosMemoryReadable | LibosMemoryWriteable,
        &region
    );

    LibosPrintf("Allocating and releasing an address space\n");

    // Create address space for worker task
    LibosAddressSpaceHandle workerAsid = 0;
    LibosAddressSpaceCreate(&workerAsid);
    LibosHandleClose(workerAsid);

    LibosPrintf("Allocating address space and region.\n");
    LibosAddressSpaceCreate(&workerAsid);
    LwU64 virtualAddress = 0;
    LibosAssert(LibosOk == LibosMemoryReserve(workerAsid, &virtualAddress, 0x1000, &region));
    LibosPrintf("Returned address %lld.\n", virtualAddress);

    LibosPrintf("Releasing address space.\n");
    LibosHandleClose(workerAsid);

    LibosPrintf("Releasing region.\n");
    LibosHandleClose(region);

    LibosProcessorShutdown();
    return 0;
}




