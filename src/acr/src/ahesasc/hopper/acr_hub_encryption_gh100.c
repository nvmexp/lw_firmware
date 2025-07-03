/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_hub_encryption_gh100.c 
 */
#include "acr.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"


/*!
 * @Brief: Hub encryption is part of HSHUB and from Hopper onwards HW is removing sec2hub_selwre interface,  hub2sec_irq0 and hub2sec_irq1
 *          and connections are moving to FSP
 *          SEC2 no longer could program or read Hub encryption status using SECHUB
 *
 * @return: ACR_OK if Hub encrytion is enabled
 */
ACR_STATUS
acrProgramHubEncryption_GH100(void)
{
    // TODO: Check for secure scratch to probe hub encryption
    return ACR_OK;
}
