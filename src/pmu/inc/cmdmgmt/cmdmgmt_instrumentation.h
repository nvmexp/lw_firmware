/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_INSTRUMENTATION_H
#define CMDMGMT_INSTRUMENTATION_H

/*!
 * @file cmdmgmt_instrumentation.h
 *
 * Provides the definitions and interfaces for all items related to the
 * instrumenatation framework.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Includes --------------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS pmuInstrumentationBegin(void)
    GCC_ATTRIB_SECTION("imem_init", "pmuInstrumentationBegin");

#endif // CMDMGMT_INSTRUMENTATION_H

