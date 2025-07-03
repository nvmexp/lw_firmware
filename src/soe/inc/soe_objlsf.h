/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_OBJLSF_H
#define SOE_OBJLSF_H

/*!
 * @file    soe_objlsf.h 
 * @copydoc soe_objlsf.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "config/g_lsf_hal.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern LSF_HAL_IFACES LsfHal;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
void constructLsf(void) GCC_ATTRIB_SECTION("imem_init", "constructLsf");
void lsfPreInit(void)   GCC_ATTRIB_SECTION("imem_init", "lsfPreInit");

#endif // SOE_OBJLSF_H
