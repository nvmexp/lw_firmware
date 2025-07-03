/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _ACR_OBJSHA_H_
#define _ACR_OBJSHA_H_

/*!
 * @file    acr_objsha.h
 * @copydoc acr_objsha.h
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#ifdef ACR_BUILD_ONLY
#include "config/g_sha_hal.h"
#include "config/acr-config.h"
#else
#include "g_sha_hal.h"
#endif


/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
typedef struct
{
    SHA_HAL_IFACES  hal;
} OBJSHA, *POBJSHA;

extern POBJSHA pSha;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

void constructSha(void)
        GCC_ATTRIB_SECTION(".imem_acr", "constructSha");

#endif // _ACR_OBJSHA_H_

