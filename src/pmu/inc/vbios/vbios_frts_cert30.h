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
 * @brief   Interfaces for interacting with the CERT30-specific portions of
 *          Firmware Runtime Security.
 */

#ifndef VBIOS_FRTS_CERT30_H
#define VBIOS_FRTS_CERT30_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "cert30.h"
#include "ucode_interface.h"

/* ------------------------ Application Includes ---------------------------- */
#include "vbios/vbios_frts_config.h"
#include "vbios/vbios_dirt.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Maximum number of VDPA entries supported with CERT30.
 *
 * @todo    This value was determined experimentally on GA10X. An actual value
 *          needs to be provided by GFW.
 */
#define VBIOS_FRTS_CERT30_VDPA_ENTRIES_MAX \
    (125U)

/*!
 * Size of VDPA entries supported with CERT30.
 */
#define VBIOS_FRTS_CERT30_VDPA_ENTRIES_SIZE \
    sizeof(BCRT30_VDPA_ENTRY_INT_BLK_BASE_S)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   Verifies CERT30-specific properties of the FRTS data.
 *
 * @param[in]   pMetadata   FRTS data to verify
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pMetadata pointer was NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     Descriptor data does not match
 *                                          expectations for CERT30
 */
FLCN_STATUS vbiosFrtsCert30Verify(const VBIOS_FRTS_METADATA *pMetadata)
    GCC_ATTRIB_SECTION("imem_init", "vbiosFrtsCert30Verify");

/*!
 * @brief   Parses into a @ref VBIOS_DIRT structure using CERT30 structures.
 *
 * @param[out]  pDirt       Pointer to @ref VBIOS_DIRT structure to populate.
 * @param[in]   pMetadata   Pointer to structure containing FRTS metadata.
 * @param[in]   pAccess     Pointer to structure containing FRTS access data.
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Any pointer argument was NULL
 * @return  Others                          Errors propagated from callees.
 */
FLCN_STATUS vbiosFrtsCert30DirtParse(
    VBIOS_DIRT *pDirt,
    const VBIOS_FRTS_METADATA *pMetadata,
    VBIOS_FRTS_ACCESS *pAccess)
    GCC_ATTRIB_SECTION("imem_init", "vbiosFrtsCert30DirtParse");

#endif // VBIOS_FRTS_CERT30_H
