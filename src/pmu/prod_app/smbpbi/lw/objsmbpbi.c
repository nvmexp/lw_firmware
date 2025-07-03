/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objsmbpbi.c
 * @brief  Container-object for the SMBus PostBox Interface routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objsmbpbi.h"
#include "pmu_objhal.h"

/* ------------------------- Global Variables ------------------------------- */
#ifdef DMEM_VA_SUPPORTED
OBJSMBPBI Smbpbi GCC_ATTRIB_SECTION("dmem_smbpbi", "Smbpbi") = {{{{0}}}};
#else
OBJSMBPBI Smbpbi = {{{{0}}}};
#endif
OBJSMBPBI_RESIDENT SmbpbiResident = {0};

/* ------------------------- Public Functions ------------------------------- */

FLCN_STATUS
constructSmbpbi(void)
{
    // placeholder
    return FLCN_OK;
}

