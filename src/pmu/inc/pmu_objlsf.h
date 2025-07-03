/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJLSF_H
#define PMU_OBJLSF_H

#include "g_pmu_objlsf.h"

#ifndef G_PMU_OBJLSF_H
#define G_PMU_OBJLSF_H

/*!
 * @file    pmu_objlsf.h
 * @copydoc pmu_objlsf.c
 */

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "pmusw.h"
#include "rmbsiscratch.h"
#include "config/g_lsf_hal.h"

/* ------------------------ Types definitions ------------------------------ */
/*!
 * LSF object Definition
 */
typedef struct
{
    // BSI RAM scratch structure
    RM_PMU_BSI_RAM_SELWRE_SCRATCH_DATA brssData;
} OBJLSF;

extern OBJLSF Lsf;

/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS lsfInitLightSelwreFalcon(void)
    GCC_ATTRIB_SECTION("imem_init", "lsfInitLightSelwreFalcon");

#endif // G_PMU_OBJLSF_H
#endif // PMU_OBJLSF_H
