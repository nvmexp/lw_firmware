/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dispflcntu116.c
 * @brief  GSP(TU116/TU117) Specific Hal Functions
 *
 * GSP HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "dev_master.h"
/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "gsp_bar0_hs.h"
#include "lwosselwreovly.h"

/* ------------------------- Type Definitions ------------------------------- */
#define GSP_TU116_UCODE_VERSION    (0x0)

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

/*
 * @brief Make sure the chip is allowed to do HDCP
 */
void
dpuEnforceAllowedChipsForHdcpHS_TU116(void)
{
    LwU32 chip;
    LwU32 data32 = 0;

    if (FLCN_OK != EXT_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &data32))
    {
        DPU_HALT();
    }

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, data32);

    if ((chip != LW_PMC_BOOT_42_CHIP_ID_TU116) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_TU117))
    {
        DPU_HALT();
    }
}

/*!
 * @brief Get the SW Ucode version
 */
FLCN_STATUS
dpuGetUcodeSWVersionHS_TU116
(
    LwU32* pUcodeVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       chip;
    LwU32       data32;

    if (pUcodeVersion == NULL)
    {
        flcnStatus = FLCN_ERR_HS_CHK_ILWALID_INPUT;
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &data32));

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, data32);

    if (chip == LW_PMC_BOOT_42_CHIP_ID_TU116 ||
        chip == LW_PMC_BOOT_42_CHIP_ID_TU117)
    {
        *pUcodeVersion = GSP_TU116_UCODE_VERSION;
    }
    else
    {
        if (OS_SEC_FALC_IS_DBG_MODE())
        {
            *pUcodeVersion = GSP_TU116_UCODE_VERSION;
        }
        else
        {
            flcnStatus = FLCN_ERR_HS_PROD_MODE_NOT_YET_SUPPORTED;
        }
    }

ErrorExit:
    return flcnStatus;
}
