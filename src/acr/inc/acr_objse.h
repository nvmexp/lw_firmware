/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _ACR_OBJSE_H_
#define _ACR_OBJSE_H_

/*!
 * @file    acr_objse.h
 * @copydoc acr_objse.h
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */

#ifdef ACR_BUILD_ONLY
#include "config/g_se_hal.h"
#include "config/acr-config.h"
#else
#include "g_se_hal.h"
#endif

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
typedef struct
{
    SE_HAL_IFACES  hal;
} OBJSE, *POBJSE;

extern POBJSE pSe;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

void constructSe(void)
        GCC_ATTRIB_SECTION(".imem_acr", "constructSe");

#endif // _ACR_OBJSE_H_

