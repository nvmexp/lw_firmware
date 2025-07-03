/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef TEGRA_ACR_OBJACRLIB_H
#define TEGRA_ACR_OBJACRLIB_H

/*!
 * @file    acr_objacrlib.h
 * @copydoc acr_objacrlib.h
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_hal.h"
#include "config/acr-config.h"
#else
#include "g_acrlib_hal.h"
#endif

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
typedef struct
{
    ACRLIB_HAL_IFACES  hal;
} OBJACRLIB, *POBJACRLIB;

extern OBJACRLIB Acrlib;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

#endif // TEGRA_ACR_OBJACRLIB_H

