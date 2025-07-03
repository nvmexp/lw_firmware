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
 * @copydoc vbios_frts_cert30.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "cert30.h"
#include "ucode_interface.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objvbios.h"
#include "vbios/vbios_frts_access.h"
#include "vbios/vbios_frts_cert30.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Compile-Time Asserts ---------------------------- */
/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS
vbiosFrtsCert30Verify
(
    const VBIOS_FRTS_METADATA *pMetadata
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pMetadata != NULL),
        FLCN_ERR_ILWALID_STATE,
        vbiosFrtsCert30Verify_exit);

    // Ensure that the VDPA entry size matches the structure we expect.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pMetadata->vdpaEntrySize == VBIOS_FRTS_CERT30_VDPA_ENTRIES_SIZE) &&
        (pMetadata->vdpaEntryCount <= VBIOS_FRTS_CERT30_VDPA_ENTRIES_MAX),
        FLCN_ERR_ILWALID_STATE,
        vbiosFrtsCert30Verify_exit);

vbiosFrtsCert30Verify_exit:
    return status;
}

FLCN_STATUS
vbiosFrtsCert30DirtParse
(
    VBIOS_DIRT *pDirt,
    const VBIOS_FRTS_METADATA *pMetadata,
    VBIOS_FRTS_ACCESS *pAccess
)
{
    FLCN_STATUS status;
    const BCRT30_VDPA_ENTRY_INT_BLK_BASE_S *pVdpaEntries;
    LwU32 i;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDirt != NULL) &&
        (pMetadata != NULL) &&
        (pAccess != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsCert30DirtParse_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosFrtsAccessVdpaEntriesGet(
            pAccess, (const void **)&pVdpaEntries),
        vbiosFrtsCert30DirtParse_exit);

    for (i = 0U; i < pMetadata->vdpaEntryCount; i++)
    {
        const BCRT30_VDPA_ENTRY_INT_BLK_BASE_S *const pVdpaEntry =
            &pVdpaEntries[i];

        //
        // Only entries with type TYPE_MAJOR _INT_BLK and _TYPE_MINOR _NORMAL
        // correspond to VBIOS tables.
        //
        if (FLD_TEST_REF(
                LW_BCRT30_VDPA_ENTRY_TYPE_MAJOR,
                _INT_BLK,
                pVdpaEntry->type) &&
            FLD_TEST_REF(
                LW_BCRT30_VDPA_ENTRY_TYPE_MINOR,
                _NORMAL,
                pVdpaEntry->type))
        {
            //
            // Extract the DIRT, and if it's a table that we can recognize, add
            // its data to the lookup table.
            //
            const LW_GFW_DIRT dirtId =
                REF_VAL(LW_BCRT30_VDPA_ENTRY_TYPE_DIRT_ID, pVdpaEntry->type);

            if (dirtId < LW_ARRAY_ELEMENTS(pDirt->tables))
            {
                VBIOS_DIRT_TABLE *const pDirtTable = &pDirt->tables[dirtId];

                // Set the size and retrieve the table pointer
                pDirtTable->size = REF_VAL(
                    LW_BCRT30_VDPA_ENTRY_INT_BLK_SIZE_SIZE,
                    pVdpaEntry->size);
                PMU_ASSERT_OK_OR_GOTO(status,
                    vbiosFrtsAccessTablePrepareAndGet(
                        pAccess,
                        pMetadata,
                        dirtId,
                        pVdpaEntry->offset_start,
                        pDirtTable->size,
                        &pDirtTable->pTable),
                    vbiosFrtsCert30DirtParse_exit);
            }
        }
    }

vbiosFrtsCert30DirtParse_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
