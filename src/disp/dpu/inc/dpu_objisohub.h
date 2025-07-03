/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPU_OBJISOHUB_H
#define DPU_OBJISOHUB_H

/*!
 * @file    dpu_objisohub.h
 * @copydoc dpu_objisohub.c
 */

/* ------------------------ System includes -------------------------------- */
#include "rmdpucmdif.h"

/* ------------------------ Application includes --------------------------- */
#include "config/g_isohub_hal.h"
#include "config/g_dpu-config_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
extern ISOHUB_HAL_IFACES IsohubHal;

/* ------------------------ Static variables ------------------------------- */

/* ------------------------ Function Prototypes ---------------------------- */
void constructIsohub(void)
    GCC_ATTRIB_SECTION("imem_init", "constructIsohub");

#endif // DPU_OBJISOHUB_H
