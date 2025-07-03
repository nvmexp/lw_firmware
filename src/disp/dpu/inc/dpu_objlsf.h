/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPU_OBJLSF_H
#define DPU_OBJLSF_H

/*!
 * @file    dpu_objlsf.h
 * @copydoc dpu_objlsf.c
 */

/* ------------------------ System includes -------------------------------- */
#include "lwostimer.h"

/* ------------------------ Application includes --------------------------- */
#include "lwdpu.h"
#include "config/g_lsf_hal.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
extern LSF_HAL_IFACES LsfHal;

/* ------------------------ Static variables ------------------------------- */

/* ------------------------ Function Prototypes ---------------------------- */
void constructLsf(void) GCC_ATTRIB_SECTION("imem_init", "constructLsf");
FLCN_STATUS lsfInitLightSelwreFalcon(void) GCC_ATTRIB_SECTION("imem_init", "lsfInitLightSelwreFalcon");

#endif // DPU_OBJLSF_H
