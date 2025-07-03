/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_SUPERSURFACE_H
#define CMDMGMT_SUPERSURFACE_H

/*!
 * @file cmdmgmt_supersurface.h
 *
 * Provides the definitions and interfaces for all items related to the
 * super-surface management.
 */

#include "g_cmdmgmt_supersurface.h"

#ifndef G_CMDMGMT_SUPERSURFACE
#define G_CMDMGMT_SUPERSURFACE

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Includes --------------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
mockable FLCN_STATUS cmdmgmtSuperSurfaceMemberDescriptorsInit(void)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "cmdmgmtSuperSurfaceMemberDescriptorsInit");

#endif // G_CMDMGMT_SUPERSURFACE
#endif // CMDMGMT_SUPERSURFACE_H

