#include "libos.h"
#include "peregrine-headers.h"
#include "libos_xcsr.h"

int main();
int worker();

__attribute__((used,section(".init.data")))   LwU64 init_stack[64];
__attribute__((used,section(".worker.data"))) LwU64 worker_stack[128];

// Declare your MPU indices
enum {
    LIBOS_MPU_TEST_PRGNLCL = LIBOS_MPU_USER_INDEX,
    LIBOS_MPU_DATA_KERNEL,
    LIBOS_MPU_DATA_WORKER
};

LibosPortNameExtern(taskWorkerPort);
LibosTaskNameExtern(taskInit);
LibosTaskNameExtern(taskWorker);

// Init task
LibosManifestTask(taskInit, 0, main, init_stack, 
  LIBOS_MPU_CODE, 
  LIBOS_MPU_DATA_INIT_AND_KERNEL, 
  LIBOS_MPU_MMIO_KERNEL, 
  LIBOS_MPU_TEST_PRGNLCL);

// Worker task
LibosManifestTaskWaiting(taskWorker, 3, taskWorkerPort, worker, worker_stack, 
  LIBOS_MPU_CODE, 
  LIBOS_MPU_DATA_KERNEL,  
  LIBOS_MPU_DATA_WORKER, 
  LIBOS_MPU_MMIO_KERNEL, 
  LIBOS_MPU_TEST_PRGNLCL)

// User-space daemon
LibosManifestPort(libosDaemonPort);
LibosManifestGrantWait(libosDaemonPort, taskInit);
LibosManifestGrantSend(libosDaemonPort, taskWorker);

// Port for task traps
LibosManifestPort(panicPort);
LibosManifestGrantAll(panicPort, taskInit);

// Port between init and worker tasks
LibosManifestPort(taskWorkerPort);
LibosManifestGrantSend(taskWorkerPort, taskInit);
LibosManifestGrantWait(taskWorkerPort, taskWorker);

void putc(char ch) {
  *(volatile LwU32 *)(LW_PRGNLCL_FALCON_DEBUGINFO) = ch;
  while (*(volatile LwU32 *)(LW_PRGNLCL_FALCON_DEBUGINFO))
    ;
}

void LibosInitHookMpu()
{
  /*
    [0] LIBOS_MPU_CODE                   - mapping of all code
    [1] LIBOS_MPU_DATA_INIT_AND_KERNEL   - kernel mapping for init task
    [2] LIBOS_MPU_MMIO_KERNEL            - kernel pri mapping
    [3] LIBOS_MPU_TEST_PRGNLCL           - test app MMIO mapping
    [4] LIBOS_MPU_DATA_KERNEL            - kernel mapping for use in not init tasks
    [5] LIBOS_MPU_DATA_WORKER            - data section for worker task
  */
 
  // Create a mapping of the kernel for the other user-space tasks
  extern char taskinit_and_kernel_data[], taskinit_and_kernel_data_size[];
  LibosBootstrapMmap(
    LIBOS_MPU_DATA_KERNEL,
    (LwU64)&taskinit_and_kernel_data,
    (LwU64)&taskinit_and_kernel_data,
    (LwU64)&taskinit_and_kernel_data_size,
          REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL) |
          REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL));

  // Create a data mapping for the worker task
  extern char taskworker_data[], taskworker_data_size[];
  LibosBootstrapMmap(
    LIBOS_MPU_DATA_WORKER,
    (LwU64)&taskworker_data,
    (LwU64)&taskworker_data,
    (LwU64)&taskworker_data_size,
          REF_NUM64(LW_RISCV_CSR_XMPUATTR_UW, 1ULL) |
          REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1ULL));          

  // Create a mmio mapping
  LibosBootstrapMmap(
    LIBOS_MPU_TEST_PRGNLCL,
    LW_RISCV_AMAP_INTIO_START,
    LW_RISCV_AMAP_INTIO_START,
    LW_RISCV_AMAP_INTIO_END - LW_RISCV_AMAP_INTIO_START,
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_UW, 1ULL) |
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1ULL)
  );
}

int main()
{
  LibosInitHookMpu();
  LibosPortSyncSend(ID(taskWorkerPort), 0, 0, 0, 0);
  return LibosInitDaemon();
}

void fillMem16(void *_buf, LwBool hello)
{
  LwU64 *buf = _buf;
  buf[0] = buf[1] = 0ULL;
  if (hello)
  {
    char *buf = _buf;
    buf[0] = 'h'; buf[1] = 'e'; buf[2]  = 'l'; buf[3]  = 'l';
    buf[4] = 'o'; buf[5] = ','; buf[6]  = ' '; buf[7]  = 'w';
    buf[8] = 'o'; buf[9] = 'r'; buf[10] = 'l'; buf[11] = 'd';
  }
}

int worker()
{
  LibosStatus status;
  char gdmaDst[16], gdmaSrc[16];

  fillMem16(gdmaDst, LW_FALSE);
  fillMem16(gdmaSrc, LW_TRUE);

  LibosPrintf("Before GDMA: \"%s\" != \"%s\"\n", gdmaDst, gdmaSrc);

  status = LibosGdmaTransfer((LwU64) gdmaDst, (LwU64) gdmaSrc, sizeof(gdmaDst), 0ULL);
  if (status != LibosOk)
  {
    LibosPrintf("LibosGdmaTransfer failed: %d\n", status);
    goto shutdown;
  }

  status = LibosGdmaFlush();
  if (status != LibosOk)
  {
    LibosPrintf("LibosGdmaFlush failed: %d\n", status);
    goto shutdown;
  }

  LibosPrintf("After GDMA:  \"%s\" == \"%s\"\n", gdmaDst, gdmaSrc);

shutdown:
  LibosProcessorShutdown();
  return 0;
}

void LibosInitHookTaskTrap(LibosTaskState * taskState, LibosShuttleId taskInitShuttleTrap)
{
  LibosPrintf("Child task has trapped or exited Task=%d pc=%p.\n", taskState->id, taskState->xepc);

  LibosProcessorShutdown();
}