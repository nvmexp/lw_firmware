/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPU_OBJIC_H
#define DPU_OBJIC_H

/*!
 * @file    dpu_objic.h
 * @copydoc dpu_objic.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "lwrtos.h"
/* ------------------------ Application Includes --------------------------- */
#include "lwdpu.h"
#include "config/g_ic_hal.h"

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

#endif // DPU_OBJIC_H

