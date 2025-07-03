/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_DETACH_H
#define CMDMGMT_DETACH_H

/*!
 * @file cmdmgmt_detach.h
 *
 * Provides the definitions and interfaces for PMU detach command
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_oslayer.h"

/* ------------------------- Includes --------------------------------------- */
#include "cmdmgmt/cmdmgmt.h"

/* ------------------------- External Definitions --------------------------- */

/* ------------------------- Type Definitions ------------------------------- */

/* ------------------------- Macros and Defines ----------------------------- */

/* ------------------------- Prototypes ------------------------------------- */
void cmdmgmtDetachDeferredProcessing(void)
#ifdef ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_resident", "cmdmgmtDetachDeferredProcessing");
#else // ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_cmdmgmtRpc", "cmdmgmtDetachDeferredProcessing");
#endif // ON_DEMAND_PAGING_BLK

/* ------------------------- Global Variables ------------------------------ */

#endif // CMDMGMT_DETACH_H
