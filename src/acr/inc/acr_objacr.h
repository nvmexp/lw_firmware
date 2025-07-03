/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef ACR_OBJACR_H
#define ACR_OBJACR_H

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "config/g_acr_hal.h"
#include "config/acr-config.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
typedef struct
{
    ACR_HAL_IFACES  hal;
} OBJACR, *POBJACR;

POBJACR pAcr;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Misc macro definitions ------------------------- */

void constructAcr(void)
        GCC_ATTRIB_SECTION("imem_acr", "constructAcr");

#endif // ACR_OBJACR_H
