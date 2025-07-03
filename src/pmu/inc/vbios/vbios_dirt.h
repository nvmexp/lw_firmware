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
 * @brief   Data structure for interacting with the VBIOS via the Data ID
 *          Reference Table IDs.
 */

#ifndef VBIOS_DIRT_H
#define VBIOS_DIRT_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "data_id_reference_table.h"
#include "data_id_reference_table_addendum.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
struct VBIOS_DIRT_PARSER;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Represents the configuration needed to access FRTS, including the memory
 * aperture settings.
 */
typedef struct
{
    /*!
     * Pointer to the table in memory
     */
    const void *pTable;

    /*!
     * Size of the table.
     */
    LwLength size;
} VBIOS_DIRT_TABLE;

typedef struct
{
    /*!
     * Lookup table from @ref LW_GFW_DIRT to a corresponding pointer to a table
     * and size of the table.
     */
    VBIOS_DIRT_TABLE tables[LW_GFW_DIRT__COUNT];
} VBIOS_DIRT;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   Constructs a @ref VBIOS_DIRT data structure, primarily initializing
 *          it with mapping data for looking up tables.
 *
 * @param[in]   pDirt   Pointer to structure to initialize
 * @param[in]   pParser Pointer to structure to used to parse the dirt mappings
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Any pointer argument is NULL
 * @return  Others                          Errors propagated from callees.
 */
FLCN_STATUS vbiosDirtConstruct(VBIOS_DIRT *pDirt, struct VBIOS_DIRT_PARSER *pParser)
    GCC_ATTRIB_SECTION("imem_init", "vbiosDirtConstruct");

#endif // VBIOS_DIRT_H
