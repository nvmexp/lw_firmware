/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_OBJAPM_H
#define SEC2_OBJAPM_H

/*!
 * @file sec2_objapm.h
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "apm/sec2_apm.h"
#include "config/g_apm_hal.h"
#include "apm/apm_keys.h"

/* ------------------------ External Definitions --------------------------- */
extern APM_HAL_IFACES ApmHal;

/* ------------------------ Static Variables ------------------------------- */
APM_HAL_IFACES ApmHal;

/* ------------------------ Function Prototypes ---------------------------- */
/**
 * @brief The function fetches the offset of RTS in the FB.
 * 
 * @param  pOutRts[out] The pointer to the LwU64 where the RTS offset is output.
 *
 * @return FLCN_STATUS  FLCN_OK if successful, relevant error otherwise.
 */
FLCN_STATUS apmGetRtsOffset(LwU64 *pOutRts)
    GCC_ATTRIB_SECTION("imem_apm", "apmGetRtsOffset");

#endif // SEC2_OBJAPM_H
