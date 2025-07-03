/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SYSLIB_H
#define SYSLIB_H

/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <flcnifcmn.h>
#include <lwrtos.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <shared.h>

/*
 * This is system library wrapper for tasks. This code is available to every task
 * (user and machine). Should not be called from kernel code.
 * Also, must not have global variables of any kind (it will be shared RO/RX only).
 */

typedef enum
{
    SYS_MEM_ITCM,
    SYS_MEM_DTCM,
    SYS_MEM_EMEM,
    SYS_MEM_FB,
    SYS_MEM_SYSMEM,
    SYS_MEM_PRIV
} SYS_MEM_ORIGIN;

#include "memops.h"

void sysPrivilegeRaise(void);
void sysPrivilegeLower(void);

void sysPrivilegeRaiseIntrDisabled(void);

void sysWfi(void);

#ifndef KERNEL_SOURCE_FILE

FLCN_STATUS sysCmdqRegisterNotifier(LwU64 cmdqIdx);
FLCN_STATUS sysCmdqUnregisterNotifier(LwU64 cmdqIdx);
FLCN_STATUS sysCmdqEnableNotifier(LwU64 cmdqIdx);
FLCN_STATUS sysCmdqDisableNotifier(LwU64 cmdqIdx);

FLCN_STATUS sysNotifyRegisterTaskNotifier(LwU64 groupMask,
                                          LwrtosQueueHandle queue, void *pMsg,
                                          LwU32 msgSize);
FLCN_STATUS sysNotifyUnregisterTaskNotifier(void);
FLCN_STATUS sysNotifyRequestGroup(LwU64 groupMask);
FLCN_STATUS sysNotifyReleaseGroup(LwU64 groupMask);
FLCN_STATUS sysNotifyProcessingDone(void);
LwU64 sysNotifyQueryIrqs(LwU8 groupNo);
LwU64 sysNotifyQueryGroups(void);
FLCN_STATUS sysNotifyEnableIrq(LwU64 groupMask, LwU64 irqMask);
FLCN_STATUS sysNotifyDisableIrq(LwU64 groupMask, LwU64 irqMask);
FLCN_STATUS sysNotifyAckIrq(LwU64 groupMask, LwU64 irqMask);

void  sysBar0WriteNonPosted(LwU32 addr, LwU32 value);
FLCN_STATUS sysVirtToEngineOffset(void *pPtr, LwU64 *pPhys, SYS_MEM_ORIGIN *pOrigin, LwBool bOffset);
FLCN_STATUS sysVirtToPhys(const void *pVirt, LwU64 *pPhys);

FLCN_STATUS sysDmaTransfer(void *pBuf, PRM_FLCN_MEM_DESC pMemDesc, LwU32 offset, LwU32 sizeBytes, LwBool bRead, LwBool bMarkSelwre
#ifdef DMA_REGION_CHECK
    ,
    LwBool              bCheckRegion
#endif
);

#ifdef LW_PRGNLCL_TFBIF_TRANSCFG
// CheetAh TFBIF configuration
FLCN_STATUS sysConfigureTfbif(LwU8 dmaIdx, LwU8 swid, LwBool vpr, LwU8 apertureId);
#else
// Dgpu FBIF configuration
FLCN_STATUS sysConfigureFbif(LwU8 dmaIdx, LwU32 transcfg, LwU8 regionid);
#endif

void sysSendSwGen(LwU64 no);

// "Panic" code for task
void sysTaskExit(const char *pStr, FLCN_STATUS err);

// shut down engine
void sysShutdown(void);

// Initialize heap (if non-standard initial size is needed)
// Can be called once, before any allocation
FLCN_STATUS mallocInit(size_t size);

// Works like standard C functions
void *malloc(size_t len);
void free(void *ptr);
void *aligned_alloc(size_t alignment, size_t size);
#endif // ifndef KERNEL_SOURCE_FILE

void sysFbhubGated(LwBool bGated);

FLCN_STATUS sysOdpPinSection(LwU32 index);

FLCN_STATUS sysOdpUnpinSection(LwU32 index);

//
// Code shared between userspace and kernel, placed in IMEM
// Must not have data or rodata sections.
//
LwBool shaInRange(LwU64 addr, LwU64 base, LwU64 size);

#ifdef PMU_RTOS
//! User-mode wrapper to update memTuneControllerTimeStamp to the current timer value.
FLCN_STATUS sysPerfMemTuneControllerSetTimestamp(LwU64 timestampId);

//! User-mode wrapper to update memTuneControllerVbiosMaxMclkkHz to the VBIOS defined value.
FLCN_STATUS sysPerfMemTuneControllerSetVbiosMaxMclkkhz(LwU64 mclkkhz);

//! User-mode wrapper to query memTuneControllerVbiosMaxMclkkHz as defined in VBIOS.
LwU64       sysPerfMemTuneControllerGetVbiosMaxMclkkhz(void);
#endif // PMU_RTOS

#endif // SYSLIB_H

