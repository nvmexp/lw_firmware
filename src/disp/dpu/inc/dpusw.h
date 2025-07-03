/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPUSW_H
#define DPUSW_H

#include "lwostrace.h"
#include "lwdpu.h"
#include "csb.h"
#include "lwrtos.h"
#include "lwoslayer.h"
#include "lwoscmdmgmt.h"
#include "osptimer.h"
#include "lib_lw64.h"
#include "config/g_profile.h"
#include "config/dpu-config.h"

/******************************************************************************/

//
// DBG_PRINTF
//
// Properly define the DBG_PRINTF macro based on whether or not debug
// print statement are enabled.  When not enabled, DBG_PRINTF should
// evaluate to nothing.
//
#if (DPUCFG_FEATURE_ENABLED(DBG_PRINTF_OS_TRACE_REG))
    #define  DBG_PRINTF(x)             osTraceReg x
#elif (DPUCFG_FEATURE_ENABLED(DBG_PRINTF_OS_TRACE_FB))
    #define  DBG_PRINTF(x)             osTraceFb x
#else
    #define  DBG_PRINTF(x)
#endif

//
// DBG_PRINTF_ISR
//
#if (DPUCFG_FEATURE_ENABLED(DBG_PRINTF_OS_TRACE_REG))
    #define  DBG_PRINTF_ISR(x)         osTraceReg x
#else
    #define  DBG_PRINTF_ISR(x)
#endif

/*!
 * The following is a series of defines (one for each OS task) that
 * allow debug print statements to be compiled in/out on a per-task
 * basis.  When zero, debug print statements for that task are
 * compiled out.  These have no effect when DEBUG is not defined.
 */
#define  DBG_PRINTF_ENABLED_CMDMGMT    0

/*!
 * Properly define all DBG_PRINTF_<task> defines based on whether or
 * not debug print-statements are enabled for that task.
 */
#if (DBG_PRINTF_ENABLED_CMDMGMT)
#define  DBG_PRINTF_CMDMGMT(x)         DBG_PRINTF(x)
#else
#define  DBG_PRINTF_CMDMGMT(x)
#endif

/******************************************************************************/

/*!
 * Following macros default to their respective OS_*** equivalents.
 * We keep them forked to allow fast change of DPU implementations.
 */
#define DPU_HALT()                      OS_HALT()
#define DPU_HALT_COND(cond)                                                    \
    do                                                                         \
    {                                                                          \
        if (!(cond))                                                           \
        {                                                                      \
            DPU_HALT();                                                        \
        }                                                                      \
    } while (LW_FALSE)

#define DPU_BREAKPOINT()                OS_BREAKPOINT()

#define LW_ARRAY_ELEMENTS(x)   ((sizeof(x)/sizeof((x)[0])))

/* ------------------------ External definitions --------------------------- */
#ifdef IS_SSP_ENABLED
// Variable that stores DPU canary for SSP for both HS and LS
extern void * __stack_chk_guard;
#endif
/* ------------------------ Private Functions ------------------------------- */
#ifdef IS_SSP_ENABLED
/*
 * @brief Set canary value, this function is used in DPU LS as well as HS code.
 *
 * @param[in]   canaryValue     New value for canary
 */
GCC_ATTRIB_ALWAYSINLINE()
inline static void 
_dpuSetStackCanary(void* pCanaryValue)
{
    __stack_chk_guard = pCanaryValue;
}
#endif

#endif // DPUSW_H

