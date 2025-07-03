#include "libos.h"
#include "peregrine-headers.h"
#include "libos_xcsr.h"

int main();
int worker();

__attribute__((used,section(".init.data")))   LwU64 init_stack[128];
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

// Port between init and worker tasks
LibosManifestPort(taskWorkerPort);
LibosManifestGrantSend(taskWorkerPort, taskInit);
LibosManifestGrantWait(taskWorkerPort, taskWorker);

// Port for task traps
LibosManifestPort(panicPort);
LibosManifestGrantAll(panicPort, taskInit);

// Port for interrupt
LibosManifestPort(interruptPort);
LibosManifestGrantAll(interruptPort, taskWorker);

// Worker task has a kernel timer
LibosManifestTimer(libosWorkerTimer)
LibosManifestGrantTimerAll(libosWorkerTimer, taskWorker);

// Create a shuttle for the timer recv
LibosShuttleNameDecl(libosWorkerTimerShuttle)
LibosManifestShuttle(taskWorker, libosWorkerTimerShuttle);

// Create a lock
LibosManifestLock(printLock);
LibosManifestGrantLockAll(printLock, taskWorker)
LibosManifestGrantLockAll(printLock, taskInit)

void putc(char ch) {
  LibosLockSyncAcquire(ID(printLock), LibosTimeoutInfinite);
  *(volatile LwU32 *)(LW_PRGNLCL_FALCON_DEBUGINFO) = ch;
  while (*(volatile LwU32 *)(LW_PRGNLCL_FALCON_DEBUGINFO))
    ;
  LibosLockRelease(ID(printLock));    
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

  // Wakeup the worker task
  LibosPortSyncSend(ID(taskWorkerPort), 0, 0, 0, 0);
  LibosPrintf("Init task exiting.\n");
  return LibosInitDaemon();
}


// callwl a^n%mod
LwU32 power(LwU32 a, LwU64 n, LwU32 mod)
{
  LwU64 power = a;
  LwU64 result = 1;

  while (n)
  {
    if (n & 1)
      result = (result * power) % mod;
    power = (power * power) % mod;
    n >>= 1;
  }
  return result;
}
 
// n−1 = 2^s * d with d odd by factoring powers of 2 from n−1
_Bool witness(LwU32 n, LwU32 s, LwU32 d, LwU32 a)
{
  LwU64 x = power(a, d, n);
  LwU64 y = 0;

  while (s) {
    y = (x * x) % n;
    if (y == 1 && x != 1 && x != n-1)
      return 0;
    x = y;
    --s;
  }
  if (y != 1)
    return 0;
  return 1;
}
 
/*
 * if n < 1,373,653, it is enough to test a = 2 and 3;
 * if n < 9,080,191, it is enough to test a = 31 and 73;
 * if n < 4,759,123,141, it is enough to test a = 2, 7, and 61;
 */       
 
_Bool is_prime_mr(LwU32 n)
{
    if (((!(n & 1)) && n != 2 ) || (n < 2) || (n % 3 == 0 && n != 3))
      return LW_FALSE;
    if (n <= 3)
      return 1;
 
    LwU32 d = n / 2;
    LwU32 s = 1;
    while (!(d & 1)) {
      d /= 2;
      ++s;
    }
 
    if (n < 1373653)
      return witness(n, s, d, 2) && witness(n, s, d, 3);
    if (n < 9080191)
      return witness(n, s, d, 31) && witness(n, s, d, 73);
    return witness(n, s, d, 2) && witness(n, s, d, 7) && witness(n, s, d, 61);
}

void test_prime(LwU64 p) {
  if (is_prime_mr(p))
    LibosPrintf("%lld is prime.\n", p);
  else 
    LibosPrintf("%lld is composite.\n", p);
}

int worker() {
  LibosShuttleId completedShuttle;

  LibosPrintf("Worker task started...\n");
  test_prime(3239479627);
  test_prime(4228961479);
  test_prime(56431ULL*51449);
  test_prime(55501ULL*53783);
  test_prime(7);
  test_prime(3*7);
  
  // Issue a receive on the timer
  LibosPortAsyncRecv(ID(libosWorkerTimerShuttle), ID(libosWorkerTimer), 0, 0);
  LibosPrintf("Timer recv initiated.\n");

  // Set the timer
  LibosTimerSet(ID(libosWorkerTimer), LibosTimeNs() + 1000);
  LibosPrintf("Timer set.\n");
  
  // Wait until the timer fires
  LibosPortWait(ID(shuttleAny), &completedShuttle, 0, 0, LibosTimeoutInfinite);
  LibosPrintf("Timer wait completed.\n");

  LibosAssert(completedShuttle == ID(libosWorkerTimerShuttle));

  // Wait for interrupt
  if (LibosOk != LibosInterruptAsyncRecv(ID(shuttleSyncRecv), ID(interruptPort), 1))
    LibosAssert(0 && "LibosInterruptAsyncRecv failed");

  // Ensure the interrupt isn't pending
  LibosAssert(LibosErrorTimeout == LibosPortWait(ID(shuttleSyncRecv), 0, 0, 0, 0));

  // Set the interrupt to fire on next WFI
  *(volatile LwU32 *)(LW_PRGNLCL_FALCON_DEBUGINFO) = 0xBADF00D;

  // Wait for the interrupt
  LibosAssert(LibosOk == LibosPortWait(ID(shuttleSyncRecv), 0, 0, 0, LibosTimeoutInfinite));

  // Force a trap
  LibosBreakpoint();

  return 0;
}

void LibosInitHookTaskTrap(LibosTaskState * taskState, LibosShuttleId taskInitShuttleTrap)
{
  LibosPrintf("Child task has trapped or exited Task=%d pc=%p.\n", taskState->id, taskState->xepc);

  LibosProcessorShutdown();
}