/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

 /*!
  * @copydoc vbios_image.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objvbios.h"
#include "vbios/vbios_image.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Compile-Time Asserts ---------------------------- */
/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS
vbiosImageDataGet
(
    const OBJVBIOS *pVbios,
    LW_GFW_DIRT dirtId,
    const void **ppData,
    LwLength offset,
    LwLength size
)
{
    FLCN_STATUS status;
    const VBIOS_DIRT *pDirt;

    //
    // Ensure we've got no NULL pointers, the table ID is valid, and that the
    // offset and size don't overflow
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pVbios != NULL) &&
        (dirtId < LW_GFW_DIRT__COUNT) &&
        (ppData != NULL) &&
        ((offset + size) >= offset),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosImageDataGet_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosDirtGetConst(pVbios, &pDirt),
        vbiosImageDataGet_exit);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDirt != NULL),
        FLCN_ERR_ILWALID_STATE,
        vbiosImageDataGet_exit);

    //
    // If the table pointer is NULL, this table is not available, so return not
    // supported.
    //
    // Note that this should not be considered an error here, because there may
    // be valid cases where we need to check if a table is supported or not.
    //
    if (pDirt->tables[dirtId].pTable == NULL)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto vbiosImageDataGet_exit;
    }

    // Ensure that no part of the requested data lies beyond the table's limits.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((offset + size) <= pDirt->tables[dirtId].size),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosImageDataGet_exit);

    *ppData =
        (const void *)(((const LwU8 *)pDirt->tables[dirtId].pTable) + offset);

vbiosImageDataGet_exit:
    return status;
}

/*-------------------------- Private Functions --------------------------------*/
