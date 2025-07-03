/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJLWLINK_H
#define PMU_OBJLWLINK_H

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "flcntypes.h"
#include "unit_api.h"

/* ------------------------ Forward Declartion ----------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "config/g_lwlink_hal.h"
#include "main.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/*!
 * LWLINK object Definition
 */
typedef struct
{
    LwU8    dummy;    // unused -- for compilation purposes only
} OBJLWLINK;

/* ------------------------ External definitions --------------------------- */
extern OBJLWLINK Lwlink;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
void constructLwlink(void)
    // Called only at init time -> init overlay.
    GCC_ATTRIB_SECTION("imem_init", "constructLwlink");
void lwlinkPreInit(void)
    // Called only at init time -> init overlay.
    GCC_ATTRIB_SECTION("imem_init", "lwlinkPreInit");

#endif // PMU_OBJLWLINK_H
