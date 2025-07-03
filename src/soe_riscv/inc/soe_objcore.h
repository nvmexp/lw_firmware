/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_OBJCORE_H
#define SOE_OBJCORE_H

/*!
 * @file soe_objcore.h
 * @brief  SOE CORE Library HAL header file.
 */


/* ------------------------ System Includes -------------------------------- */
#include "lwuproc.h"
#include "soesw.h"

/* ------------------------ Application Includes --------------------------- */
#include "unit_dispatch.h"
#include "config/g_core_hal.h"
/* ------------------------- Global Variables ------------------------------- */

/*!
 * Enumeration of CORE task's callback entries
 */
typedef enum
{
    /*!
     * Entry to poll on SOE reset PLMs
     */
    CORE_OS_TIMER_ENTRY_SOE_RESET,

    /*!
     * Must always be the last entry.
     */
    CORE_OS_TIMER_ENTRY_NUM_ENTRIES
} CORE_OS_TIMER_ENTRIES;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

#endif // SOE_OBJCORE_H
