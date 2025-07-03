/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SELWRESCRUB_OBJACR_H
#define SELWRESCRUB_OBJACR_H

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "config/g_selwrescrub_hal.h"
#include "config/selwrescrub-config.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
typedef struct
{
    SELWRESCRUB_HAL_IFACES  hal;
} OBJSELWRESCRUB, *POBJSELWRESCRUB;

POBJSELWRESCRUB pSelwrescrub;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Misc macro definitions ------------------------- */

void constructSelwrescrub(void)
        GCC_ATTRIB_SECTION("imem_selwrescrub", "constructSelwrescrub");

#endif // SELWRESCRUB_OBJACR_H
