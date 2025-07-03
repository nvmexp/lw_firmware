/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_sig_verif_ga10x.c
 */

//
// Includes
//
#include "booter.h"
#include "booter_revocation.h"

/*!
 * Read revocation information from LW_PGC6_AON_SELWRE_SCRATCH_GROUP_25(i)
 * if available.
 */
BOOTER_STATUS
booterReadRevocationRegFromGroup25_GA10X(LwU32 i, LwU32 *pVal)
{
    if (pVal == NULL)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    // only i = 0, 1, 2, 3 are reserved for GSP-RM revocation
    if (i >= 4)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    *pVal = BOOTER_REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_25(i));
    return BOOTER_OK;
}
