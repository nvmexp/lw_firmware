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
 * @file   dispflcnad10x.c
 * @brief  GSP(AD10X) Specific Hal Functions
 *
 * GSP HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "dev_master.h"
#include "dispflcn_regmacros.h"

// TODO: Needed to be removed on using ADA manuals in ADA GSP bfhs/nbfhs profile
#define LW_PMC_BOOT_42_CHIP_ID_AD102F  0x0000019F
#define LW_PMC_BOOT_42_CHIP_ID_AD102   0x00000192 
#define LW_PMC_BOOT_42_CHIP_ID_AD103   0x00000193 
#define LW_PMC_BOOT_42_CHIP_ID_AD104   0x00000194 
#define LW_PMC_BOOT_42_CHIP_ID_AD106   0x00000196 
#define LW_PMC_BOOT_42_CHIP_ID_AD107   0x00000197 

/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "gsp_bar0_hs.h"
#include "lwosselwreovly.h"

/* ------------------------- Type Definitions ------------------------------- */
#define GSP_AD10X_DEFAULT_UCODE_VERSION    (0x0)
#define GSP_AD10X_UCODE_VERSION            (0x0)

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static inline void _hdcpCheckAllowedChip(LwU32 data32)
    GCC_ATTRIB_ALWAYSINLINE();

/* ------------------------- Implementation --------------------------------- */

/*!
 * This function is used to check if the chip on which code is running is correct.
 *
 * @param[in]  data32  LW_PMC_BOOT_42 read register value.
 */
static inline void 
_hdcpCheckAllowedChip
(
    LwU32 data32
)
{
    LwU32 chip;
    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, data32);

    if ((chip != LW_PMC_BOOT_42_CHIP_ID_AD102) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_AD103) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_AD104) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_AD106) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_AD107) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_AD102F))
    {
        DPU_HALT();
    }
}


/*
 * @brief HS function to make sure the chip is allowed to do HDCP
 */
void
dpuEnforceAllowedChipsForHdcpHS_AD10X(void)
{
    LwU32 data32 = 0;

    if (FLCN_OK != EXT_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &data32))
    {
        DPU_HALT();
    }

    _hdcpCheckAllowedChip(data32);
}

/*
 * @brief LS Function to make sure the chip is allowed to do HDCP 
 */
void
dpuEnforceAllowedChipsForHdcp_AD10X(void)
{
    LwU32 data32 = 0;

    data32 = EXT_REG_RD32(LW_PMC_BOOT_42);

    _hdcpCheckAllowedChip(data32);
}


/*!
 * @brief  HS function to get the SW Ucode version
 *
 * @params[out] pUcodeVersion  Pointer to sw fuse version. 
 * @returns     FLCN_STATUS    FLCN_OK on successfull exelwtion
 *                             Appropriate error status on failure.
 */
FLCN_STATUS
dpuGetUcodeSWVersionHS_AD10X
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

    if ((chip == LW_PMC_BOOT_42_CHIP_ID_AD102) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_AD103) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_AD104) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_AD106) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_AD107))
    {
        *pUcodeVersion = GSP_AD10X_UCODE_VERSION;
    }
    else
    {
        if (OS_SEC_FALC_IS_DBG_MODE())
        {
            *pUcodeVersion = GSP_AD10X_DEFAULT_UCODE_VERSION;
        }
        else
        {
            flcnStatus = FLCN_ERR_HS_PROD_MODE_NOT_YET_SUPPORTED;
        }
    }

ErrorExit:
    return flcnStatus;
}

