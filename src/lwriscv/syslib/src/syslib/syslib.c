/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    syslib.c
 * @brief   System library for userspace tasks.
 */

/* ------------------------ System Includes -------------------------------- */

/* ------------------------ LW Includes ------------------------------------ */
#include <flcnifcmn.h>
#include <engine.h>
#include <lwmisc.h>

#ifdef DMA_SUSPENSION
#include "lwos_dma.h"
#endif // DMA_SUSPENSION

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <sections.h>
#include <lwriscv/print.h>

/* ------------------------ Drivers Includes ------------------------------- */
#include <shlib/syscall.h>

/* ------------------------ Module Includes -------------------------------- */
#include "syslib/syslib.h"
#include "riscv_csr.h"

/*
 * Copy of memory map - keep that up to date so we don't have to do pricey
 * mpu lookups everytime we need translation.
 *
 * MK TODO: perhaps kernel should provide that map on some shared page?
 */

typedef struct
{
    LwUPtr physBase;       // start of defined range in aperture
    LwUPtr length;         // length of defined range in aperture
    SYS_MEM_ORIGIN origin; // enum value identifying aperture
    LwUPtr apertureBase;   // base addr of aperture
    const char *name;      // text string identifying aperture by name
} PHYS_MEMORY_RANGE;

extern const char __imem_physical_base[];
extern const char __dmem_physical_base[];
extern const char __emem_physical_base[];
extern const char __imem_physical_size[];
extern const char __dmem_physical_size[];
extern const char __emem_physical_size[];

sysSYSLIB_RODATA static const PHYS_MEMORY_RANGE riscvPhysMemoryMap[] = {
    {(LwUPtr)__imem_physical_base, (LwUPtr)__imem_physical_size, SYS_MEM_ITCM, LW_RISCV_AMAP_IMEM_START, "ITCM" },
    {(LwUPtr)__dmem_physical_base, (LwUPtr)__dmem_physical_size, SYS_MEM_DTCM, LW_RISCV_AMAP_DMEM_START, "DTCM" },
    {(LwUPtr)__emem_physical_base, (LwUPtr)__emem_physical_size, SYS_MEM_EMEM, LW_RISCV_AMAP_EMEM_START, "EMEM" },
#ifdef LW_RISCV_AMAP_PRIV_START
    {LW_RISCV_AMAP_PRIV_START, LW_RISCV_AMAP_PRIV_SIZE,          SYS_MEM_PRIV, LW_RISCV_AMAP_PRIV_START, "PRIV" },
#endif // LW_RISCV_AMAP_PRIV_START
    {0}, // sentinel
};

#ifdef DMA_REGION_CHECK
extern LwBool vApplicationIsDmaIdxPermitted(LwU8 dmaIdx);
#endif // DMA_REGION_CHECK

// MMINTS-TODO: put into a shared header
extern FLCN_STATUS dmaMemTransfer(void *pBuf, LwU64 memDescAddress, LwU32 memDescParams, LwU32 offset, LwU32 sizeBytes, LwBool bRead, LwBool bMarkSelwre);

/*!
 * @brief Shim function around @ref dmaMemTransfer used to DMA transfer data
 * between FB (SYSMEM not supported yet) and DMEM.
 */
sysSYSLIB_CODE FLCN_STATUS
sysDmaTransfer
(
    void               *pBuf,
    RM_FLCN_MEM_DESC   *pMemDesc,
    LwU32               offset,
    LwU32               sizeBytes,
    LwBool              bRead,
    LwBool              bMarkSelwre
#ifdef DMA_REGION_CHECK
    ,
    LwBool              bCheckRegion
#endif
)
{
    FLCN_STATUS status;

#ifdef DMA_REGION_CHECK
    LwU8 dmaIdx = (LwU8)DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, pMemDesc->params);

    if (bCheckRegion && !vApplicationIsDmaIdxPermitted(dmaIdx))
    {
        status = FLCN_ERR_DMA_UNEXPECTED_DMAIDX;
    }
    else
#endif // DMA_REGION_CHECK
    if (lwrtosIS_KERNEL())
    {
        status = dmaMemTransfer(pBuf, LwU64_ALIGN32_VAL(&pMemDesc->address), pMemDesc->params, offset, sizeBytes, bRead, bMarkSelwre);
    }
    else
    {
#ifdef DMA_SUSPENSION
        lwrtosTaskCriticalEnter();
        {
            lwosDmaLockAcquire();
            lwosDmaExitSuspension();
#endif // DMA_SUSPENSION

            status = syscall7(LW_SYSCALL_DMA_TRANSFER, (LwU64)pBuf, LwU64_ALIGN32_VAL(&pMemDesc->address), (LwU64)pMemDesc->params, offset, sizeBytes, bRead, bMarkSelwre);

#ifdef DMA_SUSPENSION
            lwosDmaLockRelease();
        }
        lwrtosTaskCriticalExit();
#endif // DMA_SUSPENSION
    }

    return status;
}

#ifdef LW_PRGNLCL_TFBIF_TRANSCFG
/*!
 * @brief Syscall to dmaConfigureFbif_NoContext. Configure the TFBIF aperture for CheetAh.
 */
sysSYSLIB_CODE FLCN_STATUS
sysConfigureTfbif
(
    LwU8 dmaIdx,
    LwU8 swid,
    LwBool vpr,
    LwU8 apertureId
)
{
    return syscall4(LW_SYSCALL_CONFIGURE_FBIF, dmaIdx, swid, vpr, apertureId);
}
#else
/*!
 * @brief Syscall to dmaConfigureFbif_NoContext. Configure the FBIF aperture for DGPU.
 */
sysSYSLIB_CODE FLCN_STATUS
sysConfigureFbif
(
    LwU8 dmaIdx,
    LwU32 transcfg,
    LwU8 regionid
)
{
    return syscall3(LW_SYSCALL_CONFIGURE_FBIF, dmaIdx, transcfg, regionid);
}
#endif

/*!
 * @brief Wrapper function around @ref sysDmaTransfer used to read a buffer of data
 *        from the given FB (SYSMEM not supported yet) address into DMEM.
 */
sysSYSLIB_CODE FLCN_STATUS
dmaRead_IMPL
(
    void               *pBuf,
    PRM_FLCN_MEM_DESC   pMemDesc,
    LwU32               offset,
    LwU32               numBytes
)
{
#ifndef DMA_REGION_CHECK
    return sysDmaTransfer(pBuf, pMemDesc, offset, numBytes, LW_TRUE, LW_FALSE);
#else // DMA_REGION_CHECK
    return sysDmaTransfer(pBuf, pMemDesc, offset, numBytes, LW_TRUE, LW_FALSE, LW_TRUE);
#endif // DMA_REGION_CHECK
}

/*!
 * @brief Wrapper function around @ref sysDmaTransfer used to write a buffer of data
 *        in DMEM to given FB (SYSMEM not supported yet) address.
 */
sysSYSLIB_CODE FLCN_STATUS
dmaWrite_IMPL
(
    void               *pBuf,
    PRM_FLCN_MEM_DESC   pMemDesc,
    LwU32               offset,
    LwU32               numBytes
)
{
#ifndef DMA_REGION_CHECK
    return sysDmaTransfer(pBuf, pMemDesc, offset, numBytes, LW_FALSE, LW_FALSE);
#else // DMA_REGION_CHECK
    return sysDmaTransfer(pBuf, pMemDesc, offset, numBytes, LW_FALSE, LW_FALSE, LW_TRUE);
#endif // DMA_REGION_CHECK
}

#ifdef DMA_REGION_CHECK
/*!
 * @brief   Wrapper function around @ref s_dmaTransfer used to read a buffer of
 *          data from the given FB / SYSMEM address (DMA address) into DMEM.
 *
 * @copydoc s_dmaTransfer
 */
FLCN_STATUS
dmaReadUnrestricted_IMPL
(
    void               *pBuf,
    RM_FLCN_MEM_DESC   *pMemDesc,
    LwU32               offset,
    LwU32               numBytes
)
{
    return sysDmaTransfer(pBuf, pMemDesc, offset, numBytes, LW_TRUE, LW_FALSE, LW_FALSE);
}

/*!
 * @brief   Wrapper function around @ref s_dmaTransfer used to write a buffer of
 *          data in DMEM to given FB / SYSMEM address (DMA address)
 *
 * @copydoc s_dmaTransfer
 */
FLCN_STATUS
dmaWriteUnrestricted_IMPL
(
    void               *pBuf,
    RM_FLCN_MEM_DESC   *pMemDesc,
    LwU32               offset,
    LwU32               numBytes
)
{
    return sysDmaTransfer(pBuf, pMemDesc, offset, numBytes, LW_FALSE, LW_FALSE, LW_FALSE);
}
#endif // DMA_REGION_CHECK

#ifndef PMU_RTOS
sysSYSLIB_CODE void
sysPrivilegeRaise(void)
{
    LwrtosTaskPrivate *pPvt = lwrtosTaskPrivateRegister;

    if (!pPvt->bIsPrivileged)
    {
        syscall0(LW_SYSCALL_SET_KERNEL);
        pPvt->bIsPrivileged = LW_TRUE;
    }
}
#endif // PMU_RTOS

sysSYSLIB_CODE void
sysPrivilegeLower(void)
{
    LwrtosTaskPrivate *pPvt = lwrtosTaskPrivateRegister;

    if (pPvt->bIsPrivileged && !pPvt->bIsKernel)
    {
        syscall0(LW_SYSCALL_SET_USER);
        pPvt->bIsPrivileged = LW_FALSE;
    }
}

sysSYSLIB_CODE void
sysPrivilegeRaiseIntrDisabled(void)
{
    LwrtosTaskPrivate *pPvt = lwrtosTaskPrivateRegister;

    if (!pPvt->bIsPrivileged)
    {
        syscall0(LW_SYSCALL_SET_KERNEL_INTR_DISABLED);
        pPvt->bIsPrivileged = LW_TRUE;
    }
}

/*!
 * @brief   Wrapper function to enter wfi (wait-for-interrupt) state.
 */
sysSYSLIB_CODE void
sysWfi(void)
{
    syscall0(LW_APP_SYSCALL_WFI);
}

// MK TODO: remap codes
sysSYSLIB_CODE FLCN_STATUS
sysCmdqRegisterNotifier(LwU64 cmdqIdx)
{
    return syscall1(LW_APP_SYSCALL_CMDQ_REGISTER_NOTIFIER, cmdqIdx);
}

sysSYSLIB_CODE FLCN_STATUS
sysCmdqUnregisterNotifier(LwU64 cmdqIdx)
{
    return syscall1(LW_APP_SYSCALL_CMDQ_UNREGISTER_NOTIFIER, cmdqIdx);
}

sysSYSLIB_CODE FLCN_STATUS
sysCmdqEnableNotifier(LwU64 cmdqIdx)
{
    return syscall1(LW_APP_SYSCALL_CMDQ_ENABLE_NOTIFIER, cmdqIdx);
}

sysSYSLIB_CODE FLCN_STATUS
sysCmdqDisableNotifier(LwU64 cmdqIdx)
{
    return syscall1(LW_APP_SYSCALL_CMDQ_DISABLE_NOTIFIER, cmdqIdx);
}

#if defined(GSP_RTOS) || defined(SEC2_RTOS) || defined(SOE_RTOS)

#if SCHEDULER_ENABLED
sysSYSLIB_CODE FLCN_STATUS
sysNotifyRegisterTaskNotifier(LwU64 groupMask, LwrtosQueueHandle queue,
                              void *pMsg, LwU32 msgSize)
{
    return syscall4(LW_APP_SYSCALL_NOTIFY_REGISTER_NOTIFIER, groupMask,
                    (LwU64)queue, (LwU64)pMsg, (LwU64)msgSize);
}

sysSYSLIB_CODE FLCN_STATUS sysNotifyUnregisterTaskNotifier(void)
{
    return syscall0(LW_APP_SYSCALL_NOTIFY_UNREGISTER_NOTIFIER);
}

sysSYSLIB_CODE FLCN_STATUS sysNotifyRequestGroup(LwU64 groupMask)
{
    return syscall1(LW_APP_SYSCALL_NOTIFY_REQUEST_GROUP, groupMask);
}

sysSYSLIB_CODE FLCN_STATUS sysNotifyReleaseGroup(LwU64 groupMask)
{
    return syscall1(LW_APP_SYSCALL_NOTIFY_RELEASE_GROUP, groupMask);
}

sysSYSLIB_CODE FLCN_STATUS sysNotifyProcessingDone(void)
{
    return syscall0(LW_APP_SYSCALL_NOTIFY_PROCESSING_DONE);
}

sysSYSLIB_CODE LwU64 sysNotifyQueryIrqs(LwU8 groupNo)
{
    return syscall1(LW_APP_SYSCALL_NOTIFY_QUERY_IRQ, groupNo);
}

sysSYSLIB_CODE LwU64 sysNotifyQueryGroups(void)
{
    return syscall0(LW_APP_SYSCALL_NOTIFY_QUERY_GROUPS);
}

sysSYSLIB_CODE FLCN_STATUS sysNotifyEnableIrq(LwU64 groupMask, LwU64 irqMask)
{
    return syscall2(LW_APP_SYSCALL_NOTIFY_ENABLE_IRQ, groupMask, irqMask);
}

sysSYSLIB_CODE FLCN_STATUS sysNotifyDisableIrq(LwU64 groupMask, LwU64 irqMask)
{
    return syscall2(LW_APP_SYSCALL_NOTIFY_DISABLE_IRQ, groupMask, irqMask);
}

sysSYSLIB_CODE FLCN_STATUS sysNotifyAckIrq(LwU64 groupMask, LwU64 irqMask)
{
    return syscall2(LW_APP_SYSCALL_NOTIFY_ACK_IRQ, groupMask, irqMask);
}
#endif

#endif

//
// Function to do non-posted (blocking) writes.
// Note: UPOSTIO is explicitly being set back to TRUE after doing the non-posted write
//       since posted writes are considered to be the default behaviour.
//
sysSYSLIB_CODE void
sysBar0WriteNonPosted(LwU32 addr, LwU32 value)
{
    // If csr starts using more than one bit, this needs to change to RMW
    csr_clear(LW_RISCV_CSR_CFG, DRF_DEF64(_RISCV, _CSR_CFG, _UPOSTIO, _TRUE));
    *(volatile LwU32 *)(enginePRIV_VA_BASE + addr) = value;
    csr_set(LW_RISCV_CSR_CFG, DRF_DEF64(_RISCV, _CSR_CFG, _UPOSTIO, _TRUE));
}

sysSHARED_CODE LwBool
shaInRange(LwU64 addr, LwU64 base, LwU64 size)
{
    if (!size)
    {
        return LW_FALSE;
    }
    if ((addr < base) || (addr >= (base + size)))
    {
        return LW_FALSE;
    }
    return LW_TRUE;
}

/*!
 * Checks if an addr is within the allowed boundaries for any one
 * of the available engine memory map apertures,
 * and callwlates the offset inside that aperture.
 *
 * @param [in]  pPtr     pointer to check
 * @param [out] pPhys    if not NULL, filled with:
 *                         phys offset inside the aperture if bOffset is LW_TRUE
 *                         or full phys address if bOffset is LW_FALSE
 * @param [out] pOrigin  if not NULL, filled with an enum value identifying the aperture
 * @param [in]  bOffset  determines how pPhys is filled (with offset or full addr)
 *
 * @return  FLCN_OK                    on success
 *          FLCN_ERR_OBJECT_NOT_FOUND  if a phys addr range couldn't be found for pPtr
 */
sysSYSLIB_CODE FLCN_STATUS
sysVirtToEngineOffset
(
    void *pPtr,
    LwU64 *pPhys,
    SYS_MEM_ORIGIN *pOrigin,
    LwBool bOffset
)
{
    LwU64 addr;
    FLCN_STATUS status;

    status = sysVirtToPhys(pPtr, &addr);
    if (status != FLCN_OK)
    {
        return status;
    }

    const PHYS_MEMORY_RANGE *pMap = riscvPhysMemoryMap;
    for (; pMap->name != NULL; pMap++)
    {
        if (shaInRange(addr, pMap->physBase, pMap->length))
        {
            break;
        }
    }

    if (pMap->name == NULL)
    {
        return FLCN_ERR_OBJECT_NOT_FOUND;
    }

    if (bOffset)
    {
        addr = addr - pMap->apertureBase;
    }
    if (pPhys != NULL)
    {
        *pPhys = addr;
    }
    if (pOrigin != NULL)
    {
        *pOrigin = pMap->origin;
    }
    return FLCN_OK;
}

sysSYSLIB_CODE FLCN_STATUS
sysVirtToPhys(const void *pVirt, LwU64 *pPhys)
{
    if (!pPhys)
    {
        return FLCN_ERROR;
    }

    *pPhys = syscall1(LW_SYSCALL_VIRT_TO_PHYS, (LwUPtr)pVirt);
    return *pPhys ? FLCN_OK : FLCN_ERROR;
}

sysSYSLIB_CODE void sysSendSwGen(LwU64 no)
{
    syscall1(LW_APP_SYSCALL_FIRE_SWGEN, no);
}

sysSYSLIB_CODE void
sysTaskExit(const char *pStr, FLCN_STATUS err)
{
    dbgPuts(LEVEL_CRIT, pStr);
    dbgPrintf(LEVEL_CRIT, "Task error code: %d\n", err);

    lwrtosLwrrentTaskPrioritySet(lwrtosTASK_PRIORITY_IDLE + 1);
    while (LW_TRUE)
    {
        lwrtosYield();
    }
}

sysSYSLIB_CODE void
sysShutdown(void)
{
    syscall0(LW_APP_SYSCALL_SHUTDOWN);
}

sysSYSLIB_CODE void
sysFbhubGated(LwBool bGated)
{
    if (bGated)
    {
        syscall0(LW_APP_SYSCALL_FBHUB_GATED);
    }
    else
    {
        syscall0(LW_APP_SYSCALL_FBHUB_UNGATED);
    }
}

sysSYSLIB_CODE FLCN_STATUS
sysOdpPinSection(LwU32 index)
{
    return syscall1(LW_APP_SYSCALL_ODP_PIN_SECTION, index);
}

sysSYSLIB_CODE FLCN_STATUS
sysOdpUnpinSection(LwU32 index)
{
    return syscall1(LW_APP_SYSCALL_ODP_UNPIN_SECTION, index);
}

#ifdef PMU_RTOS
sysSYSLIB_CODE FLCN_STATUS
sysPerfMemTuneControllerSetTimestamp(LwU64 timestampId)
{
    return syscall1(LW_APP_SYSCALL_SET_TIMESTAMP, timestampId);
}

sysSYSLIB_CODE FLCN_STATUS
sysPerfMemTuneControllerSetVbiosMaxMclkkhz(LwU64 mclkkhz)
{
    return syscall1(LW_APP_SYSCALL_SET_VBIOS_MCLK, mclkkhz);
}

sysSYSLIB_CODE LwU64
sysPerfMemTuneControllerGetVbiosMaxMclkkhz()
{
    return syscall0(LW_APP_SYSCALL_GET_VBIOS_MCLK);
}
#endif // PMU_RTOS
