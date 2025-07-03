/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @copydoc vbios_sku.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objvbios.h"
#include "vbios/vbios_sku.h"
#include "vbios/vbios_bit.h"
#include "vbios/vbios_image.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Compile-Time Asserts ---------------------------- */
/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS
vbiosSkuConstruct
(
    VBIOS_SKU *pSku
)
{
    FLCN_STATUS status;
    const VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2 *pInternalUseTable;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSku != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosSkuConstruct_exit);

    (void)memset(pSku, 0, sizeof(*pSku));

    //
    // Get the pointer to the table data, and if it's not available, just bail
    // out without an error. Error out on all other errors.
    //
    // Note that we use the earliest table size with the data we need
    //
    status = vbiosImageDataGet(
        &Vbios, 
        LW_GFW_DIRT_BIT_INTERNAL_USE_ONLY,
        (const void **)&pInternalUseTable,
        0U,
        VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_SIZE_66);
    if (status == FLCN_ERR_NOT_SUPPORTED)
    {
        status = FLCN_OK;
        goto vbiosSkuConstruct_exit;
    }
    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        vbiosSkuConstruct_exit);

    pSku->version = pInternalUseTable->Version;
    pSku->buildVersion = pInternalUseTable->P4MagicNumber;
    pSku->boardId = pInternalUseTable->BoardId;
    pSku->oemVersion = pInternalUseTable->OemVersion;
    (void)memcpy(
        pSku->chipSku,
        pInternalUseTable->sLWChipSKU,
        sizeof(pInternalUseTable->sLWChipSKU));
    (void)memcpy(
        pSku->chipSkuMod,
        &pInternalUseTable->sLWChipSKUMod,
        sizeof(pInternalUseTable->sLWChipSKUMod));
    (void)memcpy(
        pSku->project,
        pInternalUseTable->sLWProject,
        sizeof(pInternalUseTable->sLWProject));
    (void)memcpy(
        pSku->projectSku,
        pInternalUseTable->sLWProjectSKU,
        sizeof(pInternalUseTable->sLWProjectSKU));
    (void)memcpy(
        pSku->projectSkuMod,
        &pInternalUseTable->sLWProjectSKUMod,
        sizeof(pInternalUseTable->sLWProjectSKUMod));

vbiosSkuConstruct_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
