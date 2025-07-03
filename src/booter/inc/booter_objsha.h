/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _BOOTER_OBJSHA_H_
#define _BOOTER_OBJSHA_H_

/*!
 * @file    booter_objsha.h
 * @copydoc booter_objsha.h
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "config/g_sha_hal.h"
#include "config/booter-config.h"


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
        GCC_ATTRIB_SECTION(".imem_booter", "constructSha");

#endif // _BOOTER_OBJSHA_H_

