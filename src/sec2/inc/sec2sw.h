/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2SW_H
#define SEC2SW_H

/*!
 * @file sec2sw.h
 */

#include <falcon-intrinsics.h>
#include "flcntypes.h"
#include "sec2debug.h"
#include "lwmisc.h"
#include "csb.h"
#include "lwrtos.h"
#include "lwoslayer.h"
#include "dmemovl.h"
#include "lwoscmdmgmt.h"
#include "lwuproc.h"
#include "config/g_tasks.h"
#include "config/g_profile.h"
#include "config/sec2-config.h"
#include "sec2_csb.h"
#include "sec2_objsec2.h"

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
#define SEC2_CLRB(ptr, drf)                                                     \
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
#define SEC2_SETB(ptr, drf)                                                     \
            falc_setb_i((LwU32*)(ptr), DRF_SHIFT(drf))

/******************************************************************************/

/*!
 * Following macros default to their respective OS_*** equivalents.
 * We keep them forked to allow fast change of SEC2 implementations.
 */
#define SEC2_HALT()                     OS_HALT()
#define SEC2_HALT_COND(cond)                                                   \
    do                                                                         \
    {                                                                          \
        if (!(cond))                                                           \
        {                                                                      \
            SEC2_HALT();                                                       \
        }                                                                      \
    } while (LW_FALSE)

#define SEC2_BREAKPOINT()               OS_BREAKPOINT()

/******************************************************************************/

/*!
 * Utility macro to disable big hammer lockdown prior to jumping out of
 * heavy secure mode
 */
#define DISABLE_BIG_HAMMER_LOCKDOWN()                                           \
    do                                                                          \
    {                                                                           \
        clearSCPregisters();                                                    \
        REG_WR32_STALL(CSB, LW_CSEC_SCP_CTL_CFG,                                \
                        DRF_DEF(_CSEC, _SCP_CTL_CFG, _LOCKDOWN_SCP, _ENABLE) |  \
                        DRF_DEF(_CSEC, _SCP_CTL_CFG, _LOCKDOWN, _DISABLE));     \
    } while (LW_FALSE)

GCC_ATTRIB_ALWAYSINLINE()
static inline
void sec2WriteStatusToMailbox0HS(FLCN_STATUS status)
{
    CSB_REG_WR32_HS_ERRCHK(LW_CSEC_FALCON_MAILBOX0, status);
}

/*!
 * Utility macro to do all required operations just before jumping out of
 * heavy secure mode
 */
#define EXIT_HS()                                                                              \
    do                                                                                         \
    {                                                                                          \
        FLCN_STATUS exitStatus = FLCN_OK;                                                      \
        if ((exitStatus = sec2UpdateResetPrivLevelMasksHS_HAL(&sec2, LW_FALSE)) != FLCN_OK)    \
        {                                                                                      \
            sec2WriteStatusToMailbox0HS(exitStatus);                                           \
            SEC2_HALT();                                                                       \
        }                                                                                      \
        sec2ForceStartScpRNGHs_HAL();                                                          \
        DISABLE_BIG_HAMMER_LOCKDOWN();                                                         \
    } while (LW_FALSE)

#endif // SEC2SW_H

