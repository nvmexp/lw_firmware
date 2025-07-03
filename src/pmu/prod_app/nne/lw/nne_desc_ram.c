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
 * @file   nne_desc_ram.c
 * @brief  DLC descriptor RAM chip-agnostic interfaces.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "objnne.h"
#include "nne/nne_desc_ram.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc nneAllocDescriptorRam
 */
FLCN_STATUS
nneDescRamAlloc
(
    LwU32   size,
    LwU32   *pOffset,
    LwU8    *pNnetId
)
{
    FLCN_STATUS    status = FLCN_OK;

    // Sanity checking.
    if ((pOffset == NULL) ||
        (pNnetId == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescRamAlloc_exit;
    }

    if ((size == 0) ||
        !LW_IS_ALIGNED(size, sizeof(LwU32)))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescRamAlloc_exit;
    }

    if ((nneDescRamSizeGet_HAL() - nneDescRamHeaderSizeGet_HAL()) < size)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_NO_FREE_MEM;
        goto nneDescRamAlloc_exit;
    }

    //
    // For now, we always return the first valid offset (i.e. right after
    // the descriptor RAM header), and the 0'th nnet ID.
    //
    *pOffset = nneDescRamHeaderSizeGet_HAL();
    *pNnetId = 0;

nneDescRamAlloc_exit:
    return status;
}

/*!
 * @copydoc nneDescRamFree
 */
void
nneDescRamFree(LwU8 nnetId)
{
    // Do nothing for now.
    (void) nnetId;
    return;
}
