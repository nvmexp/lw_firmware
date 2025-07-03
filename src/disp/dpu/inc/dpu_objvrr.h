/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPU_OBJVRR_H
#define DPU_OBJVRR_H

/*!
 * @file    dpu_objvrr.h
 * @copydoc dpu_objvrr.c
 */

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "dpu_vrr.h"
#include "config/g_vrr_hal.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
extern VRR_HAL_IFACES VrrHal;

/* ------------------------ Static variables ------------------------------- */

/* ------------------------ Function Prototypes ---------------------------- */
void constructVrr(void) GCC_ATTRIB_SECTION("imem_init", "constructVrr");

#endif // DPU_OBJVRR_H
