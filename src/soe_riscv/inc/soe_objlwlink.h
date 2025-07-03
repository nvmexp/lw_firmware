/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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
/* ------------------------ Application Includes --------------------------- */
#include "config/g_lwlink_hal.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */

typedef struct
{
    LwU64 linksEnableMask;
}OBJLWLINK;

extern OBJLWLINK Lwlink;
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
sysKERNEL_CODE void lwlinkPreInit(void);

#endif // SOE_OBJLWLINK_H
