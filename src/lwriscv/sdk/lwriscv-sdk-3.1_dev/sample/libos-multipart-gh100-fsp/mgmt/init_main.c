/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdint.h>
#include <kernel/task.h>
#include <kernel/port.h>
#include <libos.h>
#include <libos_log.h>
#include <libos_xcsr.h>
#include <libriscv.h>
#include <peregrine-headers.h>

#include "ada_tasks.h"

int main(
  // Params passed from SK or other partition
  LwU64 param1,
  LwU64 param2,
  LwU64 param3,
  LwU64 param4,
  LwU64 param5
);
int worker();

__attribute__((used,section(".init.data")))   LwU64 init_stack[64];
__attribute__((used,section(".worker.data"))) LwU64 worker_stack[128];

// Declare your MPU indices
enum {
    LIBOS_MPU_TEST_PRI = LIBOS_MPU_USER_INDEX,
    LIBOS_MPU_DATA_KERNEL,
    LIBOS_MPU_DATA_WORKER,
    LIBOS_MPU_PRINT_BUFFER,
    LIBOS_MPU_TEXT_SHARED,
    LIBOS_MPU_DATA_SHARED
};

LibosPortNameExtern(taskWorkerPort);
LibosTaskNameExtern(taskInit);
LibosTaskNameExtern(taskWorker);

// Init task
LibosManifestTask(taskInit, 0, main, init_stack, 
  LIBOS_MPU_CODE, 
  LIBOS_MPU_DATA_INIT_AND_KERNEL, 
  LIBOS_MPU_DATA_WORKER, 
  LIBOS_MPU_MMIO_KERNEL, 
  LIBOS_MPU_TEST_PRI,
  LIBOS_MPU_PRINT_BUFFER,
  LIBOS_MPU_TEXT_SHARED,
  LIBOS_MPU_DATA_SHARED);

// Worker task
LibosManifestTaskWaiting(taskWorker, 3, taskWorkerPort, worker, worker_stack, 
  LIBOS_MPU_CODE, 
  LIBOS_MPU_DATA_KERNEL,  
  LIBOS_MPU_DATA_WORKER, 
  LIBOS_MPU_MMIO_KERNEL, 
  LIBOS_MPU_TEST_PRI,
  LIBOS_MPU_PRINT_BUFFER,
  LIBOS_MPU_TEXT_SHARED,
  LIBOS_MPU_DATA_SHARED)

// Port between init and worker tasks
LibosManifestPort(taskWorkerPort);
LibosManifestGrantSend(taskWorkerPort, taskInit);
LibosManifestGrantWait(taskWorkerPort, taskWorker);

// User-space daemon
LibosManifestPort(libosDaemonPort);
LibosManifestGrantWait(libosDaemonPort, taskInit);
LibosManifestGrantSend(libosDaemonPort, taskWorker);

// From liblwriscv
int putchar(int c);

void putc(int ch)
{
  putchar(ch);
}

void LibosInitHookMpu()
{
  /*
    [0] LIBOS_MPU_CODE                   - mapping of all code
    [1] LIBOS_MPU_DATA_INIT_AND_KERNEL   - kernel mapping for init task
    [2] LIBOS_MPU_MMIO_KERNEL            - kernel pri mapping
    [3] LIBOS_MPU_TEST_PRI               - test app MMIO mapping
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
          REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL) |
          REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL) |
          REF_NUM64(LW_RISCV_CSR_XMPUATTR_UW, 1ULL) |
          REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1ULL));          

  // Create a mmio mapping
  LibosBootstrapMmap(
    LIBOS_MPU_TEST_PRI,
    LW_RISCV_AMAP_EXTIO1_START,
    LW_RISCV_AMAP_EXTIO1_START,
    LW_RISCV_AMAP_EXTIO1_END - LW_RISCV_AMAP_EXTIO1_START,
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_UW, 1ULL) |
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1ULL)
  );

  LibosBootstrapMmap(
    LIBOS_MPU_PRINT_BUFFER,
    LW_RISCV_AMAP_DMEM_START + 0x27000,
    LW_RISCV_AMAP_DMEM_START + 0x27000,
    0x1000,
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_UW, 1ULL) |
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1ULL)
  );

  extern char shared_text[], shared_text_size[];
  LibosBootstrapMmap(
    LIBOS_MPU_TEXT_SHARED,
    (LwU64)&shared_text,
    (LwU64)&shared_text,
    (LwU64)&shared_text_size,
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_UX, 1ULL) |
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1ULL)
  );

  extern char shared_data[], shared_data_size[];
  LibosBootstrapMmap(
    LIBOS_MPU_DATA_SHARED,
    (LwU64)&shared_data,
    (LwU64)&shared_data,
    (LwU64)&shared_data_size,
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_UW, 1ULL) |
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1ULL)
  );
}

__attribute__((section(".bss.shared"))) char buffer_mgmt_1[1024];
__attribute__((section(".bss.shared"))) char buffer_mgmt_2[4096];

int main(
  // Params passed from SK or other partition
  LwU64 param1,
  LwU64 param2,
  LwU64 param3,
  LwU64 param4,
  LwU64 param5
)
{
  LibosInitHookMpu();
  LibosPrintf("Params passed from SK or other partition: 0x%llX, 0x%llX, 0x%llX, 0x%llX, 0x%llX\n",
    param1, param2, param3, param4, param5);
  LibosPrintf("Shared buffer from boot partition: %s\n", buffer_mgmt_1);
  int status;
  if ((status = adaInitTask(ID(taskWorkerPort))) != 0)
    return status;
  return LibosInitDaemon();
}

int worker() {
  LibosPrintf("Worker task started...\n");
  LibosPrintf("Switching to Partition boot...\n");
  LwU64 inArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT] = {0xdeadbeefULL, 0, 0, 0, 0};
  LwU64 outArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT];
  LibosPartitionSwitch(0, inArgs, outArgs);
  LibosPrintf("Return values: 0x%llX, 0x%llX, 0x%llX, 0x%llX, 0x%llX\n",
    outArgs[0], outArgs[1], outArgs[2], outArgs[3], outArgs[4]);

  return adaWorkerTask();
}
