/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOOTER_OBJBOOTER_H
#define BOOTER_OBJBOOTER_H

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "config/g_booter_hal.h"
#include "config/booter-config.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
typedef struct
{
    BOOTER_HAL_IFACES  hal;
} OBJBOOTER, *POBJBOOTER;

POBJBOOTER pBooter;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Misc macro definitions ------------------------- */

void constructBooter(void)
        GCC_ATTRIB_SECTION("imem_booter", "constructBooter");

#endif // BOOTER_OBJBOOTER_H
