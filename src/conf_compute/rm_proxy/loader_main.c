/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <dev_gsp.h>

#include <lwmisc.h>
#include <lwtypes.h>
#include <riscvifriscv.h>

#include <lwriscv/csr.h>
#include <lwriscv/fence.h>
#include <lwriscv/gcc_attrs.h>
#include <lwriscv/peregrine.h>
#include <lwriscv/sbi.h>

#include <liblwriscv/io.h>
#include <liblwriscv/libc.h>
#include <liblwriscv/mpu.h>
#include <liblwriscv/print.h>
#include <liblwriscv/shutdown.h>

#include "partitions.h"
#include "cmdmgmt.h"
#include "rm_proxy.h"
#include "rpc.h"

#define MPU_ATTR_RWX    REF_NUM64(LW_RISCV_CSR_SMPUATTR_SR, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_SW, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_SX, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UR, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UW, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UX, 1ULL)

#define MPU_ATTR_RX     REF_NUM64(LW_RISCV_CSR_SMPUATTR_SR, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_SX, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UR, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UX, 1ULL)

#define MPU_ATTR_RW     REF_NUM64(LW_RISCV_CSR_SMPUATTR_SR, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_SW, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UR, 1ULL) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_UW, 1ULL)

#define MPU_ATTR_CACHE  REF_NUM64(LW_RISCV_CSR_SMPUATTR_CACHEABLE, 1u) | \
                        REF_NUM64(LW_RISCV_CSR_SMPUATTR_COHERENT, 1u)

#define MPU_ATTR_WPR2   REF_NUM64(LW_RISCV_CSR_SMPUATTR_WPR, 2u)

extern LwU8 __proxy_entry_start[];
extern LwU8 __proxy_entry_end[];
extern LwU8 __imem_rm_start[];
extern LwU8 __dmem_print_start[];
extern LwU8 __dmem_print_size[];

extern LwU8 _partition_rm_stack_bottom[];
extern LwU8 _partition_rm_stack_top[];


GCC_ATTR_NO_SSP void loader_init_rm_proxy(CONF_COMPUTE_PARTITION_REQUEST *pRequest,
                                          CONF_COMPUTE_PARTITION_REPLY *pReply)
{
    // Setup canary (for loader)
    if (sspGenerateAndSetCanaryWithInit() != LWRV_OK)
    {
        riscvPanic();
    }

    // (Re-)initialize print
    printInitEx((LwU8*)__dmem_print_start,
                (LwU16)((LwUPtr)__dmem_print_size & 0xFFFF),
                LW_PGSP_QUEUE_HEAD(RM_RISCV_DEBUG_BUFFER_QUEUE),
                LW_PGSP_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE), 1);

    rmPrintf("GSP-RM-Proxy Loader.\n");

    rmPrintf("Copy GSP-RM-Proxy entry point code to IMEM.\n");
    // Copy entry point code
    memcpy((void*)__imem_rm_start, (void*)__proxy_entry_start,
           (LwU64)__proxy_entry_end - (LwU64)__proxy_entry_start);

    // Wire RPC
    rmPrintf("Wire RPC.\n");
    rpcInit(pRequest, pReply);

    // Setup MPU (map stack)
    {
        MpuContext ctx;
        MpuHandle handle;

        rmPrintf("Setup stack.\n");
        mpuInit(&ctx);
        mpuReserveEntry(&ctx, 0, &handle);

        mpuWriteEntry(&ctx, handle,
                      PARTITION_STACK_VA_START - (LwUPtr)(_partition_rm_stack_top - _partition_rm_stack_bottom),
                      (LwUPtr)_partition_rm_stack_bottom,
                      (LwUPtr)(_partition_rm_stack_top - _partition_rm_stack_bottom),
                      MPU_ATTR_RW);
        mpuEnableEntry(&ctx, handle);
    }

    rmPrintf("Start partition code.\n");
}
