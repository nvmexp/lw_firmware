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
 * @file   sec2_apmencryptdma.c
 * @brief  Encryption/decryption functions for APM methods
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objapm.h"

/* ------------------------- Function Definitions --------------------------- */
/**
 * @brief The function fetches the offset of RTS in the FB.
 * 
 * @param  pOutRts[out] The pointer to the LwU64 where the RTS offset is output.
 *
 * @return FLCN_STATUS  FLCN_OK if successful, relevant error otherwise.
 */
FLCN_STATUS apmGetRtsOffset(LwU64 *pOutRts)
{
    return apmGetRtsFbOffset_HAL(&ApmHal, pOutRts);
}
