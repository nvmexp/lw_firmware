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
 * @copydoc vbios_dirt.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "vbios/vbios_dirt.h"
#include "vbios/vbios_dirt_parser.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Compile-Time Asserts ---------------------------- */
/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS
vbiosDirtConstruct
(
    VBIOS_DIRT *pDirt,
    VBIOS_DIRT_PARSER *pParser
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDirt != NULL) &&
        (pParser != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosDirtConstruct_exit);

    //
    // Ensure the structure is zero'd, ensuring that the pointers are
    // initialized to NULL values.
    //
    (void)memset(pDirt, 0x0, sizeof(*pDirt));

    // Use the parser to actually populate the structure
    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosDirtParserParse(pParser, pDirt),
        vbiosDirtConstruct_exit);

vbiosDirtConstruct_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
