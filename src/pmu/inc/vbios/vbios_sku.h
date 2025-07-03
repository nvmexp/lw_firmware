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
 * @brief   Data structure and interfaces for interacting with the VBIOS's SKU
 *          identifying information.
 */

#ifndef VBIOS_SKU_H
#define VBIOS_SKU_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Data that identifies the VBIOS's SKU
 */
typedef struct
{
    /*!
     * VBIOS version
     */
    LwU32 version;

    /*!
     * Build version of the VBIOS
     */
    LwU32 buildVersion;

    /*!
     * Board ID for the board.
     */
    LwU16 boardId;

    /*!
     * OEM version.
     */
    LwU8 oemVersion;

    /*!
     * LW Chip SKU Number
     */
    char chipSku[4];             

    /*!
     * LW Chip SKU Modifier
     */
    char chipSkuMod[2];

    /*!
     * LW Project (Board) Number
     */
    char project[5];

    /*!
     * LW Project (Board) SKU Number
     */
    char projectSku[5];          

    /*!
     * LW Project (Board) SKU Modifier
     */
    char projectSkuMod[2];       
} VBIOS_SKU;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   Constructs the SKU data for the board.
 *
 * @param[out]  pSku   Pointer to SKU structure to construct
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pSku argument is NULL
 * @return  TODO - others
 */
FLCN_STATUS vbiosSkuConstruct(VBIOS_SKU *pSku)
    GCC_ATTRIB_SECTION("imem_init", "vbiosSkuConstruct");

#endif // VBIOS_SKU_H
