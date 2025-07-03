/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef THERM_DETACH_H 
#define THERM_DETACH_H

/*!
 * @file therm_detach.h
 *
 * Provides the definitions and interfaces for THERM detach command
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
void thermDetachProcess(void)
    GCC_ATTRIB_NOINLINE();

/* ------------------------- Global Variables ------------------------------ */

#endif // THERM_DETACH_H
