/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"
#include "libos-config.h"
#include "lwmisc.h"
#include "lwtypes.h"
#include "libelf.h"
#include "libriscv.h"
#include "task.h"
#include "scheduler.h"
#include "diagnostics.h"
#include "libos.h"

void KernelMpuLoad()
{
  KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, LIBOS_MPU_MMIO_KERNEL);

  KernelCsrWrite(
      LW_RISCV_CSR_XMPUATTR,
      REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL));
#if LIBOS_LWRISCV < LIBOS_LWRISCV_2_0
  KernelCsrWrite(LW_RISCV_CSR_XMPUPA, LW_RISCV_AMAP_PRIV_START);
  KernelCsrWrite(LW_RISCV_CSR_XMPURNG, LIBOS_CONFIG_PRI_MAPPING_RANGE);
#else
  KernelCsrWrite(LW_RISCV_CSR_XMPUPA, LW_RISCV_AMAP_INTIO_START);
  KernelCsrWrite(LW_RISCV_CSR_XMPURNG, LW_RISCV_AMAP_INTIO_SIZE);
#endif
  KernelCsrWrite(LW_RISCV_CSR_XMPUVA, LIBOS_CONFIG_PRI_MAPPING_BASE | 1ULL /* VALID */);
}


void LIBOS_SECTION_TEXT_COLD __attribute__((used)) KernelSyscallBootstrapMmap(LwU64 index, LwU64 va, LwU64 pa, LwU64 size, LwU64 attributes)
{
    // Locked down to init task only
#if LIBOS_TINY
    if (KernelSchedulerGetTask() != &TaskInit)
#else
    if (KernelSchedulerGetTask() != TaskInit)
#endif    
      KernelPanic();

    KernelTrace("KERNEL: KernelSyscallBootstrapMmap()\n");

    KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, index);
    KernelCsrWrite(LW_RISCV_CSR_XMPURNG, size);
    KernelCsrWrite(LW_RISCV_CSR_XMPUPA, pa);
    KernelCsrWrite(LW_RISCV_CSR_XMPUATTR, attributes);
    KernelCsrWrite(LW_RISCV_CSR_XMPUVA, va /* valid bit will be hit when we context switch */);

    KernelSchedulerDirtyPagetables();
}
