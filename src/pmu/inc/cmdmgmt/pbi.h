/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2008-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PBI_H
#define PBI_H

/*!
 * @file    pbi.h
 * @copydoc pbi.c
 */

// #include "rmpbicmdif.h"

/* ------------------------ Function Prototypes ----------------------------- */
FLCN_STATUS pbiService(void)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "pbiService");

#endif // PBI_H

