/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOESW_H
#define SOESW_H

/*!
 * @file soesw.h
 */

#include <falcon-intrinsics.h>
#include "flcntypes.h"
#include "soedebug.h"
#include "lwmisc.h"
#include "csb.h"
#include "lwrtos.h"
#include "lwostimer.h"
#include "lwoslayer.h"
#include "dmemovl.h"
#include "lwoscmdmgmt.h"
#include "lwuproc.h"
#include "config/g_tasks.h"
#include "config/g_profile.h"
#include "config/soe-config.h"

/* ------------------------ External definitions --------------------------- */

/******************************************************************************/

/*!
 * Clear a single bit in the value pointed to by 'ptr'.  The bit cleared is
 * specified by drf-value.  DO NOT specify DRF values representing fields
 * larger than 1-bit.  Doing so will only clear the lowest bit in the range.
 *
 * @param[in]  ptr  Pointer to the value to modify
 * @param[in]  drf  The 1-bit field to clear
 */
#define SOE_CLRB(ptr, drf)                                                     \
            falc_clrb_i((LwU32*)(ptr), DRF_SHIFT(drf))

/*!
 * Sets a single bit in the value pointed to by 'ptr'.  The bit set is
 * specified specified by drf-value.  DO NOT specify DRF values representing
 * fields larger than 1-bit.  Doing so will only set the lowest bit in the
 * range.
 *
 * @param[in]  ptr  Pointer to the value to modify
 * @param[in]  drf  The 1-bit field to set
 */
#define SOE_SETB(ptr, drf)                                                     \
            falc_setb_i((LwU32*)(ptr), DRF_SHIFT(drf))

/******************************************************************************/

/*!
 * Following macros default to their respective OS_*** equivalents.
 * We keep them forked to allow fast change of SOE implementations.
 */
#define SOE_HALT()                     OS_HALT()
#define SOE_HALT_COND(cond)                                                    \
    do                                                                         \
    {                                                                          \
        if (!(cond))                                                           \
        {                                                                      \
            SOE_HALT();                                                        \
        }                                                                      \
    } while (LW_FALSE)

#define SOE_BREAKPOINT()               OS_BREAKPOINT()

/*!
 * Macro for aligning a pointer up to requested granularity
 */
#define SOE_ALIGN_UP_PTR(ptr, gran)                                            \
    ((void *)(LW_ALIGN_UP(((LwUPtr)(ptr)), (gran))))

/******************************************************************************/

#endif // SOESW_H

