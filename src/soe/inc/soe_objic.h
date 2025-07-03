/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_OBJIC_H
#define SOE_OBJIC_H

/*!
 * @file    soe_objic.h
 * @copydoc soe_objic.h
 */

/* ------------------------ System Includes -------------------------------- */
#include "rmsoecmdif.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_ic_hal.h"
#include "config/soe-config.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern IC_HAL_IFACES IcHal;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

void constructIc(void)
    GCC_ATTRIB_SECTION("imem_init", "constructIc");

#endif // SOE_OBJIC_H

