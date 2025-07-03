
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_sect210.c
 */
#include "acr.h"
#include "dev_master.h"
#include "acr_objacr.h"

/*!
 * Checks if build is supported. For CHEETAH, return OK
 */
ACR_STATUS
acrCheckIfBuildIsSupported_T210(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if (chip == LW_PMC_BOOT_42_CHIP_ID_GM20B)
    {
        return ACR_OK;
    }
    return  ACR_ERROR_ILWALID_CHIP_ID;
}

/*!
 * @brief Check fuse and Halt if doesnt match
 */
ACR_STATUS
acrCheckFuseRevocation_T210(void)
{
    return ACR_OK;
}

