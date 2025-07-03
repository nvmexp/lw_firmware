/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_verify_chip_ga100.c
 * @brief  The code in this file is used to verify that the profile is running
 *         on the appropriate chip.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_gpuarch.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Verifies that the microcode is running on an expected GPU
 *
 *
 * @note    This function exelwtes a @ref PMU_HALT, which is normally strongly
 *          discouraged. The rationale for this is that failing the chip-check
 *          means we are in a completely undefined state, and it might not be
 *          safe to execute any more code. Therefore, the function exelwtes a
 *          @ref PMU_HALT to avoid exposing any more issues.
 *
 * @return  FLCN_OK                 Profile is running on an expected GPU
 * @return  FLCN_ERR_ILWALID_STATE  Profile is running on an unexpected GPU
 */
FLCN_STATUS
pmuPreInitVerifyChip_GA10X(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (!IsGPU(_GA102) &&
        !IsGPU(_GA103) &&
        !IsGPU(_GA104) &&
        !IsGPU(_GA106) &&
        !IsGPU(_GA107))
    {
        PMU_HALT();
        status = FLCN_ERR_ILWALID_STATE;
        goto pmuPreInitVerifyChip_GA10X_exit;
    }

pmuPreInitVerifyChip_GA10X_exit:
    return status;
}
