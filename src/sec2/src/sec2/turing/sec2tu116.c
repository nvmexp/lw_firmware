/*
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2tu116.c
 * @brief  SEC2 HAL Functions for TU116
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "dev_ram.h"
#include "lwosselwreovly.h"
#include "dev_master.h"
#include "sec2_hs.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "sec2_bar0_hs.h"
#include "config/g_sec2_private.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */

/*
 * @brief Get the SW fuse version
 */
#define SEC2_TU116_UCODE_VERSION    (0x2)
FLCN_STATUS
sec2GetSWFuseVersionHS_TU116
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip       = 0;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU116:
        case LW_PMC_BOOT_42_CHIP_ID_TU117:
        {
            *pFuseVersion = SEC2_TU116_UCODE_VERSION;
            return FLCN_OK;
        }
        default:
        {
            return FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
        }
    }

ErrorExit:
    return flcnStatus;
}

