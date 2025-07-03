/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @brief   Data structures for interacting with the VBIOS Firmware Runtime
 *          Security model.
 */

#ifndef VBIOS_FRTS_H
#define VBIOS_FRTS_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "vbios/vbios_dirt.h"
#include "vbios/vbios_frts_access.h"
#include "vbios/vbios_frts_config.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * The alignment of the "sub-sections" in the FRTS region.
 */
#define VBIOS_FRTS_SUB_REGION_ALIGNMENT \
    (256U)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Structure describing various information about the Firmware Runtime Security
 * data.
 */
typedef struct
{
    /*!
     * The configuration settings for Firmware Runtime Security.
     */
    VBIOS_FRTS_CONFIG config;

    /*!
     * All of the metadata describing locations of data in FRTS region.
     */
    VBIOS_FRTS_METADATA metadata;

    /*!
     * Data needed to access FRTS.
     */
    VBIOS_FRTS_ACCESS access;
} VBIOS_FRTS;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   Verifies sanity of access to FRTS data and constructs any necessary
 *          data associated with FRTS.
 *
 * @param[in,out]   pFrts   Pointer to FRTS structure
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Any pointer argument is NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     FRTS was not in the expected format.
 * @return  Others                          Errors propagated from callees.
 */
FLCN_STATUS vbiosFrtsConstructAndVerify(VBIOS_FRTS *pFrts)
    GCC_ATTRIB_SECTION("imem_init", "vbiosFrtsConstructAndVerify");

/*!
 * @brief   Parses from FRTS into a @ref VBIOS_DIRT data structure, setting up
 *          its mappings.
 *
 * @param[in]   pFrts   Pointer to FRTS structure to use for parsing
 * @param[out]  pDirt   Pointer to structure to initialize
 * 
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     Internal state is inconsistent.
 * @return  Others                          Errors propagated from callees.
 */
FLCN_STATUS vbiosFrtsDirtParse(VBIOS_FRTS *pFrts, VBIOS_DIRT *pDirt)
    GCC_ATTRIB_SECTION("imem_init", "vbiosFrtsDirtParse");

#endif // VBIOS_FRTS_H
