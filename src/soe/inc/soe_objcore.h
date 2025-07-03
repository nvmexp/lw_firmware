/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
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
LwrtosQueueHandle Disp2QCoreThd;
OS_TIMER          CoreOsTimer;
extern LwrtosTaskHandle OsTaskCore;

/*!
 * Enumeration of CORE task's OS_TIMER callback entries
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

/*!
 * CORE Object definition
 */
typedef struct
{
    CORE_HAL_IFACES  hal;
} OBJCORE;

/* ------------------------ External Definitions --------------------------- */
extern OBJCORE Core;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS constructCore(void) GCC_ATTRIB_SECTION("imem_core", "constructCore");
void corePreInit(void) GCC_ATTRIB_SECTION("imem_core", "corePreInit");

#endif // SOE_OBJCORE_H
