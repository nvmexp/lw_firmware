/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
// Wrapper for the generated CMDMGMT RPC source file.
#if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC))
#include "rpcgen/g_pmurpc_cmdmgmt.c"
#endif
