/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SOE_OBJLWLINK_H
#define SOE_OBJLWLINK_H

/*!
 * @file    soe_objlwlink.h 
 * @copydoc soe_objlwlink.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "dev_soe_ip_addendum.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ Application Includes --------------------------- */
#include "soe_lwlink.h"
#include "config/g_lwlink_hal.h"

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern LWLINK_HAL_IFACES LwlinkHal;

typedef struct
{
    LwU64 linksEnableMask;
}OBJLWLINK;

extern OBJLWLINK Lwlink;
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
void constructLwlink(void) GCC_ATTRIB_SECTION("imem_init", "constructLwlink");
void lwlinkPreInit(void)   GCC_ATTRIB_SECTION("imem_init", "lwlinkPreInit");

#endif // SOE_OBJLWLINK_H
