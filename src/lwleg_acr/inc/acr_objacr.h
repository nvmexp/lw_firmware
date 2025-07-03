/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef TEGRA_ACR_OBJACR_H
#define TEGRA_ACR_OBJACR_H

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

extern OBJACR Acr;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Misc macro definitions ------------------------- */

#endif // TEGRA_ACR_OBJACR_H
