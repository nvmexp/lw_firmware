/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2DEBUG_H
#define SEC2DEBUG_H

/*!
 * @file sec2debug.h
 */

#include <falcon-intrinsics.h>
#include <falc_debug.h>
#include <falc_trace.h>
#include "flcntypes.h"
#include "lwmisc.h"
#include "lwoslayer.h"
#include "config/g_tasks.h"
#include "config/sec2-config.h"
#include "sec2/sec2mailboxid.h"

/******************************************************************************/

//
// DBG_PRINTF
//
// Properly define the DBG_PRINTF macro based on whether or not debug
// print statement are enabled.  When not enabled, DBG_PRINTF should
// evaluate to nothing.
//
#if (SEC2CFG_FEATURE_ENABLED(DBG_PRINTF_FALCDEBUG))
    #define  DBG_PRINTF(x)             falc_debug x
#else
    #define  DBG_PRINTF(x)
#endif

//
// DBG_PRINTF_ISR
//
#if (SEC2CFG_FEATURE_ENABLED(DBG_PRINTF_FALCDEBUG))
    #define  DBG_PRINTF_ISR(x)         falc_debug x
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
#define  DBG_PRINTF_ENABLED_CHNMGMT    0

/*!
 * Properly define all DBG_PRINTF_<task> defines based on whether or
 * not debug print-statements are enabled for that task.
 */
#if (DBG_PRINTF_ENABLED_CMDMGMT)
#define  DBG_PRINTF_CMDMGMT(x)         DBG_PRINTF(x)
#else
#define  DBG_PRINTF_CMDMGMT(x)
#endif

#if (DBG_PRINTF_ENABLED_CHNMGMT)
#define  DBG_PRINTF_CHNMGMT(x)         DBG_PRINTF(x)
#else
#define  DBG_PRINTF_CHNMGMT(x)
#endif

#if defined(MCHECK)
#define MCHECK_REG(x)   (x)
#else 
#define MCHECK_REG(x)
#endif

#endif // SEC2DEBUG_H
