/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_TASK_SYNC_H
#define LWOS_TASK_SYNC_H

/*!
 * @file    lwos_task_sync.h
 * @copydoc lwos_task_sync.c
 */

/* ------------------------ System includes --------------------------------- */
#include "lwuproc.h"

/* ------------------------ Application includes ---------------------------- */
/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
#ifdef TASK_SYNC

FLCN_STATUS lwosTaskSyncInitialize(void)
    GCC_ATTRIB_SECTION("imem_init", "lwosTaskSyncInitialize");

FLCN_STATUS lwosTaskCreated(void)
    GCC_ATTRIB_SECTION("imem_init", "lwosTaskCreated");

void lwosTaskInitDone(void)
    GCC_ATTRIB_SECTION("imem_resident", "lwosTaskInitDone");

FLCN_STATUS lwosTaskInitWaitAll(void)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "lwosTaskInitWaitAll");

#else // TASK_SYNC

#define lwosTaskSyncInitialize()        (FLCN_OK)
#define lwosTaskCreated()               (FLCN_OK)
#define lwosTaskInitDone()              lwosNOP()
#define lwosTaskInitWaitAll()           (FLCN_OK)

#endif // TASK_SYNC

#endif // LWOS_TASK_SYNC_H
