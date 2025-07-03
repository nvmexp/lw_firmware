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
 * @brief   An addendum to the @ref data_id_reference_table.h file to provide
 *          additional common definitions.
 */

#ifndef DATA_ID_REFERENCE_TABLE_ADDENDUM_H
#define DATA_ID_REFERENCE_TABLE_ADDENDUM_H

/* ------------------------ System Includes --------------------------------- */
#include "data_id_reference_table.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Represents a "DIRT ID" for identifying a VBIOS table.
 */
typedef LwU8 LW_GFW_DIRT;

/*!
 * The number of @ref LW_GFW_DIRT table IDs
 */
#define LW_GFW_DIRT__COUNT (LW_GFW_DIRT_MINION_UCODE + 1U)

#endif // DATA_ID_REFERENCE_TABLE_ADDENDUM_H
