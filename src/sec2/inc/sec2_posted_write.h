/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_POSTED_WRITE_H
#define SEC2_POSTED_WRITE_H

/*!
 * @file sec2_posted_write.h
 */

/* ------------------------ System Includes -------------------------------- */

/* ------------------------ Application includes --------------------------- */
#include "acr.h"
#include "config/g_sec2_hal.h"

/* ------------------------ Macros and Defines ----------------------------- */
// Used to distinguish between different PRI hubs
#define SYS0_PRI_HUB_ID 0
#define SYSB_PRI_HUB_ID 1
#define SYSC_PRI_HUB_ID 2
#define GPC_PRI_HUB_ID  3
#define FBP_PRI_HUB_ID  4

// Used to distinguish between enable / disable of SEC2 precedence
#define SEC2_ENABLE_PRECEDENCE     LW_TRUE
#define SEC2_DISABLE_PRECEDENCE    LW_FALSE

// 
// NOTE:: POSTED_WRITE_INIT and POSTED_WRITE_END have to be called in same function
//        lwrrently to avoid misuse / incorrect use of the posted write feature.
//

// Should be called before first posted write operation is performed
#define POSTED_WRITE_INIT()                                                         \
    {                                                                               \
        do                                                                          \
        {                                                                           \
            CHECK_FLCN_STATUS(sec2CheckInterruptsDisabled_HAL(&Acr));               \
            CHECK_FLCN_STATUS(sec2ToggleSec2PrivErrorPrecedence_HAL(&Acr));         \
        } while (LW_FALSE)

// Should be called after last posted write operation is performed
#define POSTED_WRITE_END(reg)                                                       \
        do                                                                          \
        {                                                                           \
            CHECK_FLCN_STATUS(sec2CheckInterruptsDisabled_HAL(&Acr));               \
            CHECK_FLCN_STATUS(sec2FlushPriTransactions_HAL(&Acr));                  \
            CHECK_FLCN_STATUS(sec2CheckNonBlockingWritePriErrors_HAL(&Acr,reg));    \
        } while (LW_FALSE);                                                         \
    }

#endif // SEC2_POSTED_WRITE_H
