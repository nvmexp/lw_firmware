/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   nne_weight_ram.c
 * @brief  DLC weight RAM interface.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "objnne.h"
#include "nne/nne_weight_ram.h"
#include "task_nne.h"
#include "main.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * @brief Offset into the weight RAM that has been allocated for non-inference
 *        specific allocations. Should be reset whenever a new descriptor is loaded
 *        by using @ref nneWeightRamReset.
 */
static LwU32   nonInferenceWeightOffset
    GCC_ATTRIB_SECTION("dmem_nne", "nonInferenceWeightOffset");

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc nneWeightRamAlloc
 */
FLCN_STATUS
nneWeightRamAlloc
(
    LwU32    size,
    LwU32   *pOffset
)
{
    FLCN_STATUS  status = FLCN_OK;

    // Sanity checking.
    if (pOffset == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneWeightRamAlloc_exit;
    }

    if ((nneWeightRamSizeGet_HAL() < size) ||
        ((nneWeightRamSizeGet_HAL() - size) < nonInferenceWeightOffset))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_NO_FREE_MEM;
        goto nneWeightRamAlloc_exit;
    }

    *pOffset                  = nonInferenceWeightOffset;
    nonInferenceWeightOffset += size;

nneWeightRamAlloc_exit:
    return status;
}

/*!
 * @copydoc nneWeightRamReset
 */
FLCN_STATUS
nneWeightRamReset(void)
{
    nonInferenceWeightOffset = 0;
    return FLCN_OK;
}

/* ------------------------ Private Functions ------------------------------- */
