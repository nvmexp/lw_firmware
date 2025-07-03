/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_lsfgp100_only.c
 * @brief  Light Secure Falcon (LSF) GP100 Hal Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"

/* ------------------------ Application includes --------------------------- */
#include "pmu_objlsf.h"

#include "config/g_lsf_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */


/*!
 * @brief Do not copy BRSS struct to pointer
 *
 * @param[in] pBrssData  RM_PMU_BSI_RAM_SELWRE_SCRATCH_DATA pointer
 */
void
lsfCopyBrssData_GP100
(
    RM_PMU_BSI_RAM_SELWRE_SCRATCH_DATA *pBrssData
)
{
    // Make BRSS data as invalid
    pBrssData->bIsValid = LW_FALSE;
}

