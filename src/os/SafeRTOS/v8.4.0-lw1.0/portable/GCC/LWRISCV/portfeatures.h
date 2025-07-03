/*
    Copyright (C)2006 onward WITTENSTEIN aerospace & simulation limited. All rights reserved.

    This file is part of the SafeRTOS product, see projdefs.h for version number
    information.

    SafeRTOS is distributed exclusively by WITTENSTEIN high integrity systems,
    and is subject to the terms of the License granted to your organization,
    including its warranties and limitations on use and distribution. It cannot be
    copied or reproduced in any way except as permitted by the License.

    Licenses authorize use by processor, compiler, business unit, and product.

    WITTENSTEIN high integrity systems is a trading name of WITTENSTEIN
    aerospace & simulation ltd, Registered Office: Brown's Court, Long Ashton
    Business Park, Yanley Lane, Long Ashton, Bristol, BS41 9LB, UK.
    Tel: +44 (0) 1275 395 600, fax: +44 (0) 1275 393 630.
    E-mail: info@HighIntegritySystems.com

    http://www.HighIntegritySystems.com
*/

/*-----------------------------------------------------------------------------
 * Features specific to this product variant.
 *---------------------------------------------------------------------------*/

#ifndef PORT_FEATURES_H
#define PORT_FEATURES_H

/*-----------------------------------------------------------------------------
 * Declare Include Files
 *---------------------------------------------------------------------------*/

/* Include sections macros. */
#include "sections.h"

/* Include the Run-Time Statistics functionality. */
#include "runtimestats.h"

/* The list module definitions are needed by several kernel modules. */
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
 * Structure definitions.
 *---------------------------------------------------------------------------*/

/* User-friendly indices to context uxGpr array. */
enum LWRISCV_GPR
{
    LWRISCV_GPR_RA = 0,
    LWRISCV_GPR_SP, LWRISCV_GPR_GP, LWRISCV_GPR_TP,
    LWRISCV_GPR_T0, LWRISCV_GPR_T1, LWRISCV_GPR_T2, LWRISCV_GPR_S0,
    LWRISCV_GPR_S1, LWRISCV_GPR_A0, LWRISCV_GPR_A1,
    LWRISCV_GPR_A2, LWRISCV_GPR_A3, LWRISCV_GPR_A4, LWRISCV_GPR_A5,
    LWRISCV_GPR_A6, LWRISCV_GPR_A7, LWRISCV_GPR_S2, LWRISCV_GPR_S3,
    LWRISCV_GPR_S4, LWRISCV_GPR_S5, LWRISCV_GPR_S6, LWRISCV_GPR_S7,
    LWRISCV_GPR_S8, LWRISCV_GPR_S9, LWRISCV_GPR_S10, LWRISCV_GPR_S11,
    LWRISCV_GPR_T3, LWRISCV_GPR_T4, LWRISCV_GPR_T5, LWRISCV_GPR_T6,
};

#if ( configLW_FPU_SUPPORTED == 1U )
/* User-friendly indices to context uxFpGpr array. */
enum LWRISCV_FP_REGS
{
    LWRISCV_FREG_T0, LWRISCV_FREG_T1, LWRISCV_FREG_T2, LWRISCV_FREG_T3,
    LWRISCV_FREG_T4, LWRISCV_FREG_T5, LWRISCV_FREG_T6, LWRISCV_FREG_T7,
    LWRISCV_FREG_S0, LWRISCV_FREG_S1,
    LWRISCV_FREG_A0, LWRISCV_FREG_A1,
    LWRISCV_FREG_A2, LWRISCV_FREG_A3, LWRISCV_FREG_A4, LWRISCV_FREG_A5,
    LWRISCV_FREG_A6, LWRISCV_FREG_A7,
    LWRISCV_FREG_S2, LWRISCV_FREG_S3, LWRISCV_FREG_S4, LWRISCV_FREG_S5,
    LWRISCV_FREG_S6, LWRISCV_FREG_S7, LWRISCV_FREG_S8, LWRISCV_FREG_S9,
    LWRISCV_FREG_S10, LWRISCV_FREG_S11,
    LWRISCV_FREG_T8, LWRISCV_FREG_T9, LWRISCV_FREG_T10, LWRISCV_FREG_T11,
};
#endif /* ( configLW_FPU_SUPPORTED == 1U ) */

#define TASK_CONTEXT_FLAG_SWITCH_MASK    0x1
#define TASK_CONTEXT_FLAG_SWITCH_FULL    0x1
#define TASK_CONTEXT_FLAG_SWITCH_PART    0x0

/* Task Context - stores everything that is needed for per-task context.
 *
 * Structure size: 36 words == 288 bytes
 *
 * WARNING: update portasmmacro.h whenever you change xPortTaskContext or TCB.
 * TODO: we need something like asm-offsets.h for that and TCB. */
typedef struct
{
    portUnsignedBaseType uxGpr[31];                 /* We skip x0 since it is always zero. */
    portUnsignedBaseType uxPc;
    portUnsignedBaseType uxSstatus;                 /* Only *PIE and MPP are used. */
    portUnsignedBaseType uxFlags;                   /* Only one flag for now. */
    portUnsignedBaseType uxSie;
} xPortTaskContext;

#if ( configLW_FPU_SUPPORTED == 1U )
typedef struct
{
    portFloat32Type      uxFpGpr[32];               /* FPU GPRs */
    portUnsignedBaseType uxFpStatus;                /* FCSR */
} xPortFpuTaskContext;
#endif /* ( configLW_FPU_SUPPORTED == 1U ) */

/*---------------------------------------------------------------------------*/

/* Task control block. Each task has its own task control block.
 * The TCB is defined here so that it is accessible to the portable layer
 * and can colweniently contain the port specific elements.
 *
 * Fields xCtx, uxTopOfStackMirror and hFPUCtxOffset are used by the context
 * switching assembly. Moving any of these fields requires changes to the assembly.
 */
typedef struct xPortTaskControlBlock
{
    xPortTaskContext                xCtx;                     /* Task CPU Context. THIS MUST BE THE FIRST MEMBER OF THE STRUCT. */
    portUnsignedBaseType            uxTopOfStackMirror;       /* The uxTopOfStackMirror will be set to the bitwise ilwerse (XOR) of pxTopOfStack. */

#if ( configLW_FPU_SUPPORTED == 1U )
    portInt16Type                   hFPUCtxOffset;            /* Offset to FPU context relative to TCB or 0 if task doesn't use FPU */
    portInt16Type                   hFPUCtxOffsetMirror;      /* The hFPUCtxOffsetMirror will be set to the bitwise ilwerse (XOR) of hFPUCtxOffset. */
#endif /* ( configLW_FPU_SUPPORTED == 1U ) */

    portInt8Type                   *pcStackLastAddress;       /* The address of the end of the task stack. This byte should not be accessed. */
    portUnsignedBaseType            uxStackSizeBytes;         /* The number of bytes allocated for the task stack. */

    xListItem                       xStateListItem;           /* List item used to place the TCB in ready and blocked queues. */
    xListItem                       xEventListItem;           /* List item used to place the TCB in event lists. */

#if ( configINCLUDE_RUN_TIME_STATS == 1U )
    xRTS                            xRunTimeStats;            /* The run-time statistics data. */
#endif /* ( configINCLUDE_RUN_TIME_STATS == 1U ) */

    volatile portUnsignedBaseType   uxNotifiedValue;          /* The Notification value. */
    volatile portBaseType           xNotifyState;             /* Notify state of the task. */
    void                           *pvObject;                 /* Points to the instance of the C++ object that tracks this structure. */

#ifdef TASK_RESTART
    pdTASK_CODE                     pvTaskCode;               /* Points to the first instruction of the task. */
    portUInt8Type                   ucDefaultPriority;        /* The starting priority of the task. */
#endif /* TASK_RESTART */

    portUnsignedBaseType            uxPriority;               /* The priority of the task where 0 is the lowest priority. */

#if ( configINCLUDE_MUTEXES == 1U )
    portUnsignedBaseType            uxBasePriority;           /* The priority last assigned to the task - used by the priority inheritance mechanism. */
    xList                           xMutexesHeldList;         /* A list used to manage held mutexes. */
#endif /* ( configINCLUDE_MUTEXES == 1U ) */

#if ( configINCLUDE_EVENT_POLL == 1U )
    xList                           xEventPollObjectsList;    /* The list of event poll objects owned by this task. */
#endif /* ( configINCLUDE_EVENT_POLL == 1U ) */

    /* VG-TODO variables used only by WHIS - we don't care about them and would be best if they didn't exist */
    portStackType                  *pxStackInUseMarker;       /* Contains a fixed code to indicate that this task stack is in use. */
    portUnsignedBaseType            uxStackLimitMirror;       /* The uxStackLimitMirror will be set to the bitwise ilwerse (XOR) of pxStackLimit. */
    portStackType                  *pxStackLimit;             /* Points to the limit of the stack that still leaves enough space for the context that is saved by the kernel.
                                                               * The stack cannot legitimately grow past this address otherwise a context switch will cause a corruption. */

    void                           *pxMpuInfo;                /* The MPU configuration for this task. */

    /* SAFERTOSTRACE TCBTASKNUMBER */

} xTCB;
/*---------------------------------------------------------------------------*/

/* The structure supplied to xTaskCreate(). */
typedef struct xTASK_PARAMETERS
{
    pdTASK_CODE             pvTaskCode;
    xTCB                   *pxTCB;
    portInt8Type           *pcStackBuffer;
    portUnsignedBaseType    uxStackDepthBytes;
    void                   *pvParameters;
    void                   *pvObject;
    portUInt8Type           uxPriority;
    portUnsignedBaseType    uxTlsSize;
    portUnsignedBaseType    uxTlsAlignment;

#if ( configLW_FPU_SUPPORTED == 1U )
    portInt16Type           hFPUCtxOffset;
#endif /* ( configLW_FPU_SUPPORTED == 1U ) */

    void                   *pxMpuInfo;
} xTaskParameters;
/*---------------------------------------------------------------------------*/

/* The structure supplied to xTaskInitializeScheduler().
 * This is actually never used, so it is defined as opaque type. */
typedef struct PORT_INIT_PARAMETERS xPORT_INIT_PARAMETERS;

/*-----------------------------------------------------------------------------
 * LWPU CODE BELOW.
 *---------------------------------------------------------------------------*/

#ifndef SAFE_RTOS_BUILD
#error This header file must not be included outside SafeRTOS build.
#endif

#ifdef __cplusplus
}
#endif

#endif /* PORT_FEATURES_H */
