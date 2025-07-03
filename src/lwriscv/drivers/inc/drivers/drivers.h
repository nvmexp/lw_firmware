/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DRIVERS_DRIVERS_H
#define DRIVERS_DRIVERS_H
/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <lwmisc.h>
#include <engine.h>
#include <flcnifcmn.h>
#include <lwriscv/print.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <sections.h>
#include <lwrtos.h>
struct xPortTaskControlBlock;

//
// All functions declared in this file are placed in kernel section.
// That means no task should call them directly, instead they should use
// syslib.
//
// This is "public" drivers interface (that is every kernel code may use it)
//

/* ------------------------ Init functions ---------------------------------- */
#if LWRISCV_DEBUG_PRINT_ENABLED
sysKERNEL_CODE FLCN_STATUS debugInit(void);
sysKERNEL_CODE void debugBufferWaitForEmpty(void);
#else
#define debugInit()  FLCN_OK
#define debugBufferWaitForEmpty()  do {/* NOP */} while (LW_FALSE)
#endif

sysKERNEL_CODE FLCN_STATUS dmaInit(LwU32 idx);
sysKERNEL_CODE FLCN_STATUS exceptionInit(void);

#if defined(GSP_RTOS) || defined(SEC2_RTOS) || defined(SOE_RTOS)
FLCN_STATUS irqInit(void);
#endif // GSP_RTOS

sysKERNEL_CODE FLCN_STATUS mmInit(LwUPtr fbPhysBase, LwUPtr elfPhysBase, LwU64 wprId);
sysKERNEL_CODE_CREATE FLCN_STATUS traceInit(void);
#if LWRISCV_CORE_DUMP
sysKERNEL_CODE_CREATE void coreDumpInit(LwU64 coreDumpPhysAddr, LwU32 coreDumpSize);
#else
static inline void coreDumpInit(LwU64 coreDumpPhysAddr, LwU32 coreDumpSize) {}
#endif

#if LWRISCV_SYMBOL_RESOLVER
sysKERNEL_CODE_CREATE void symbolInit(LwU64 elfBasePhys, LwU64 elfSize, LwU64 wprId);
#else
static inline void symbolInit(LwU64 elfBasePhys, LwU64 elfSize, LwU64 wprId) {}
#endif
sysKERNEL_CODE void tlsInit(void);

/* ------------------------ Register polling -------------------------------- */
/*!
 * An interface for polling on a register value.
 */
typedef struct
{
    /*! Reg addr */
    LwU32  addr;

    /*! AND-mask */
    LwU32  mask;

    /*! Value to compare */
    LwU32  val;

    /*!
     * If LW_TRUE, check if the masked register value
     * is equal to the passed value, otherwise check
     * if it's not equal.
     */
    LwBool pollForEq;
} LW_RISCV_REG_POLL;

/*!
 * A way of polling on a specific field in a
 * register until the register reports a specific value
 * or until a timeout oclwrs.
 * Today this is only used for DMA timeout tracking, so it lives in dma.c
 *
 * @return LW_TRUE  - if the specificied condition is met
 * @return LW_FALSE - otherwise
 */
LwBool csbPollFunc(void *pArgs);

/* ------------------------ DMA --------------------------------------------- */
extern LwU32 dmaIndex;

//
// MMINTS-TODO: re-evalute timeout to a more reasonable value.
// Very large timeout of 1s to be safe for now. In reality a more reasonable
// timeout might be something like 50ms - we sometimes did see >10ms
// latencies on Ampere.
//
#define LW_DMA_TIMEOUT_MS (1000U)
#define LW_DMA_TIMEOUT_US (LW_DMA_TIMEOUT_MS * 1000U)

/*!
 * @brief Wait for DMA _IDLE condition. If timeout is reached, halt.
 */
void dmaWaitForIdle(void);

/*!
 * @brief Wait for DMA _FULL_FALSE condition. If timeout is reached, halt.
 */
void dmaWaitForNonFull(void);

/*!
 * \brief DMA transfer between TCM VA and FB
 *
 * Limitations:
 * - This function can't access context in gp, and it can't
 *   cause pagefaults or any other traps!
 * - Source/dest must have 4-byte alignment
 *   256-byte alignment is optimal
 * - Size must have 4-byte granularity
 *   256-byte granularity is optimal
 */
FLCN_STATUS dmaMemTransfer(void  *pBuf,
                           LwU64  memDescAddress,
                           LwU32  memDescParams,
                           LwU32  offset,
                           LwU32  sizeBytes,
                           LwBool bRead,
                           LwBool bMarkSelwre);

#ifdef DMA_NACK_SUPPORTED
/*!
 * \brief Check errors on the last DMA transaction.
 *
 * If an error (NACK) is found, it is cleared and the error code is returned.
 */
FLCN_STATUS dmaNackCheckAndClear(void);
#endif // DMA_NACK_SUPPORTED

/* ------------------------ Debug ------------------------------------------- */
#if LWRISCV_SYMBOL_RESOLVER
FLCN_STATUS symResolveSection(LwUPtr addr, const char **ppName, LwU64 *pOffset);
FLCN_STATUS symResolveFunction(LwUPtr addr, const char **ppName, LwU64 *pOffset);
FLCN_STATUS symResolveData(LwUPtr addr, const char **ppName, LwU64 *pOffset);
// If function resolve fails, returns section name for given address
FLCN_STATUS symResolveFunctionSmart(LwUPtr addr, const char **ppName, LwU64 *pOffset);
// If data resolve fails, returns section name for given address
FLCN_STATUS symResolveDataSmart(LwUPtr addr, const char **ppName, LwU64 *pOffset);
#else
static inline FLCN_STATUS symResolveSection(LwUPtr addr, const char **ppName, LwU64 *pOffset) { return FLCN_OK; }
static inline FLCN_STATUS symResolveFunction(LwUPtr addr, const char **ppName, LwU64 *pOffset) { return FLCN_OK; }
static inline FLCN_STATUS symResolveData(LwUPtr addr, const char **ppName, LwU64 *pOffset) { return FLCN_OK; }
static inline FLCN_STATUS symResolveFunctionSmart(LwUPtr addr, const char **ppName, LwU64 *pOffset) { return FLCN_OK; }
static inline FLCN_STATUS symResolveDataSmart(LwUPtr addr, const char **ppName, LwU64 *pOffset) { return FLCN_OK; }
#endif

#if defined(GSP_RTOS) || defined(SEC2_RTOS) || defined(SOE_RTOS)
/* ------------------------ Command queues (GSP) ---------------------------- */
void        cmdqIrqHandler(void);
FLCN_STATUS cmdqRegisterNotifier(LwU64 queue);
FLCN_STATUS cmdqUnregisterNotifier(LwU64 queue);
FLCN_STATUS cmdqEnableNotifier(LwU64 queue);
FLCN_STATUS cmdqDisableNotifier(LwU64 queue);

/* ------------------------ Notifications Tree (GSP) ------------------------ */
#if SCHEDULER_ENABLED

void        notifyInit(LwU64 kernelReservedGroups);
void        notifyExit(void);
void        notifyIrqHandlerHook(void);

FLCN_STATUS notifyRegisterTaskNotifier(LwrtosTaskHandle task, LwU64 groupMask,
                                       LwrtosQueueHandle queue, void *pMsg,
                                       LwU64 msgSize);
FLCN_STATUS notifyUnregisterTaskNotifier(LwrtosTaskHandle task);
FLCN_STATUS notifyProcessingDone(LwrtosTaskHandle task);
FLCN_STATUS notifyRequestGroup(LwrtosTaskHandle task, LwU64 groupMask);
FLCN_STATUS notifyReleaseGroup(LwrtosTaskHandle task,LwU64 groupMask);
LwU64 notifyQueryIrqs(LwrtosTaskHandle task, LwU8 groupNo);
LwU64 notifyQueryGroups(LwrtosTaskHandle task);
FLCN_STATUS notifyEnableIrq(LwrtosTaskHandle task, LwU64 groupMask, LwU64 irqMask);
FLCN_STATUS notifyDisableIrq(LwrtosTaskHandle task, LwU64 groupMask, LwU64 irqMask);
FLCN_STATUS notifyAckIrq(LwrtosTaskHandle task, LwU64 groupMask, LwU64 irqMask);

#else

#define notifyInit(x) do { (void) x; } while(0)
#define notifyExit() do { } while (0)

#endif
#endif

/* ------------------------ Interrupts -------------------------------------- */
FLCN_STATUS irqFireSwGen(LwU8 no);
void irqHandleMemerr(void);
void irqHandleIopmpErr(void);
LwBool irqIsSwGenClear(void *pNo);
FLCN_STATUS irqWaitForSwGenClear(LwU8 no);

/* ------------------------ Trace buffer ------------------------------------ */
void traceDump(void);

/* ------------------------ Register dumps ---------------------------------- */
void regsDump(void);

/* ------------------------ Core dump --------------------------------------- */
#if LWRISCV_CORE_DUMP
void coreDumpRegisterTask(LwrtosTaskHandle handle, const char *name);
void coreDump(LwrtosTaskHandle handle, LwU64 cause);
#else
static inline void coreDumpRegisterTask(LwrtosTaskHandle handle, const char *name) {}
static inline void coreDump(LwrtosTaskHandle handle, LwU64 cause) {}
#endif
void kernelVerboseOops(struct xPortTaskControlBlock *pTCB);
void kernelVerboseCrash(LwU64 cause);

/* ------------------------ Shutdown handler -------------------------------- */
void shutdown(void);

/* ----------------------------- App hooks ---------------------------------- */
extern void appShutdown(void);
extern void appHalt(void);

/* ----------------------------- FBhub gating ------------------------------- */
/*!
 * @brief Block access to memory which uses FBhub (FB/SYSMEM)
 *
 * This flag is set when we enter a low power state in which Fbhub is gated.
 * It is queried by other RISC-V drivers before attempting to access FB/SYSMEM.
 */
extern LwBool bFbhubGated;

static LW_FORCEINLINE void
fbhubGatedSet(LwBool gated)
{
    bFbhubGated = gated;
}

static LW_FORCEINLINE LwBool
fbhubGatedGet(void)
{
    return bFbhubGated;
}

#endif // DRIVERS_DRIVERS_H
