/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_OBJGFE_H
#define SEC2_OBJGFE_H

/*!
 * @file sec2_objgfe.h
 */

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "gfe/sec2_gfe.h"
#include "tsec_drv.h"
#include "config/g_gfe_hal.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
extern GFE_HAL_IFACES GfeHal;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
void constructGfe(void) GCC_ATTRIB_SECTION("imem_init", "constructGfe");

#endif // SEC2_OBJGFE_H
