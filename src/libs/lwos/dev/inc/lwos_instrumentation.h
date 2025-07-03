/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWOS_INSTRUMENTATION_H
#define LWOS_INSTRUMENTATION_H

/*!
 * @file    lwos_instrumentation.h
 * @copydoc lwos_instrumentation.c
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwrtos.h"
#include "lwtypes.h"
#include "lwuproc.h"
#include "ctrl/ctrl2080/ctrl2080flcn.h"

/* ------------------------ Defines ----------------------------------------- */
#ifdef INSTRUMENTATION
#define LWOS_INSTR_IS_ENABLED() LW_TRUE
#else
#define LWOS_INSTR_IS_ENABLED() LW_FALSE
#endif

#ifdef INSTRUMENTATION_PROTECT
#define LWOS_INSTR_IS_PROTECTED() LW_TRUE
#else
#define LWOS_INSTR_IS_PROTECTED() LW_FALSE
#endif

#ifdef INSTRUMENTATION_PTIMER
#define LWOS_INSTR_USE_PTIMER() LW_TRUE
#else
#define LWOS_INSTR_USE_PTIMER() LW_FALSE
#endif

/* ------------------------ Public Variables -------------------------------- */
extern LwU8 LwosInstrumentationEventMask[];

/* ------------------------ Function Prototypes ----------------------------- */

//
// These functions control the exelwtion of instrumentation, and only need to
// be run when enabling/disabling it (often in cmdmgmt), so they can go in the
// imem_instCtrl overlay.
//
FLCN_STATUS lwosInstrumentationBegin(RM_FLCN_MEM_DESC *queueMem)
    GCC_ATTRIB_SECTION("imem_instInit", "lwosInstrumentationBegin");
FLCN_STATUS lwosInstrumentationEnd(void)
    GCC_ATTRIB_SECTION("imem_instInit", "lwosInstrumentationEnd");

//
// These functions must be resident, so we put them in a separate overlay to
// the other instrumentation code.
//
FLCN_STATUS lwosInstrumentationLog(LwU8 eventType, LwU8 trackingId)
    GCC_ATTRIB_SECTION("imem_instRuntime", "lwosInstrumentationLog");
FLCN_STATUS lwosInstrumentationLogFromISR(LwU8 eventType, LwU8 trackingId)
    GCC_ATTRIB_SECTION("imem_instRuntime", "lwosInstrumentationLogFromISR");

FLCN_STATUS lwosInstrumentationRecalibrate(void)
    GCC_ATTRIB_SECTION("imem_instCtrl", "lwosInstrumentationRecalibrate");

//
// Flushing code can be used in different places depending on the application.
// However, it will be used in the idle task (which has no overlays) so it must
// be kept resident (so it can go in instRuntime).
//
FLCN_STATUS lwosInstrumentationFlush(void)
    GCC_ATTRIB_SECTION("imem_instRuntime", "lwosInstrumentationFlush");

/* ------------------------ Defines ----------------------------------------- */

//
// Wrapper defines for the instrumentation functions. When instrumentation is
// disabled, the defines are NOPs/trivial
//
#ifdef INSTRUMENTATION
/*!
 * @brief      Wrapper for @ref lwosInstrumentationLog
 */
#define LWOS_INSTRUMENTATION_LOG(_eventType, _trackingId)                      \
    lwosInstrumentationLog((_eventType), (_trackingId))

/*!
 * @brief      Wrapper for @ref lwosInstrumentationLog that uses lwr task ID as
 *             trackingId. Only usable if osTaskIdGet header is included.
 */
#define LWOS_INSTRUMENTATION_LOG_LWR_TASK(_eventType)                          \
    lwosInstrumentationLog((_eventType), osTaskIdGet())

/*!
 * @brief      Wrapper for @ref lwosInstrumentationLogFromIsr
 */
#define LWOS_INSTRUMENTATION_LOG_FROM_ISR(_eventType, _trackingId)             \
    lwosInstrumentationLogFromISR((_eventType), (_trackingId))

/*!
 * @brief      Wrapper for @ref lwosInstrumentationLogFromIsr that uses lwr task
 *             ID as trackingId. Only usable if osTaskIdGet header is included.
 */
#define LWOS_INSTRUMENTATION_LOG_LWR_TASK_FROM_ISR(_eventType)                 \
    lwosInstrumentationLogFromISR((_eventType), osTaskIdGet())

/*!
 * @brief      Wrapper for @ref lwosInstrumentationFlush
 */
#define LWOS_INSTRUMENTATION_FLUSH()                                           \
    lwosInstrumentationFlush()

/*!
 * @brief      Wrapper for @ref lwosInstrumentationBegin
 */
#define LWOS_INSTRUMENTATION_BEGIN(_memDesc)                                   \
    lwosInstrumentationBegin(_memDesc)

/*!
 * @brief      Wrapper for @ref lwosInstrumentationEnd
 */
#define LWOS_INSTRUMENTATION_END()                                             \
    lwosInstrumentationEnd()

/*!
 * @brief      Wrapper for @ref lwosInstrumentationRecalibrate
 */
#define LWOS_INSTRUMENTATION_RECALIBRATE()                                     \
    lwosInstrumentationRecalibrate()

#else /* INSTRUMENTATION */

#define LWOS_INSTRUMENTATION_LOG(_eventType, _trackingId)                      \
    lwosVarNOP((_eventType), (_trackingId))

#define LWOS_INSTRUMENTATION_LOG_LWR_TASK(_eventType)                          \
    lwosVarNOP(_eventType)

#define LWOS_INSTRUMENTATION_LOG_FROM_ISR(_eventType, _trackingId)             \
    lwosVarNOP((_eventType), (_trackingId))

#define LWOS_INSTRUMENTATION_LOG_LWR_TASK_FROM_ISR(_eventType)                 \
    lwosVarNOP(_eventType)

#define LWOS_INSTRUMENTATION_FLUSH()                                           \
    lwosNOP()

#define LWOS_INSTRUMENTATION_BEGIN(_memDesc)                                   \
    lwosVarNOP(_memDesc)

#define LWOS_INSTRUMENTATION_END()                                             \
    lwosNOP()

/*!
 * @brief      Trivial definition of lwosInstrumentationRecalibrate, needed
 *             since return value is used even when INSTRUMENTATION is disabled
 */
#define LWOS_INSTRUMENTATION_RECALIBRATE()                                     \
    (FLCN_OK)

#endif /* INSTRUMENTATION */

#endif /* LWOS_INSTRUMENTATION_H */
