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
 * @file   sec2ad10x.c
 * @brief  SEC2 HAL Functions for AD10X
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_hs.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define SEC2_AD10X_DEFAULT_UCODE_VERSION    (0x0)
#define SEC2_AD102_UCODE_VERSION            (0x1)

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */

/*
 * @brief Get the SW fuse version
 */
FLCN_STATUS
sec2GetSWFuseVersionHS_AD10X
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;

    if (OS_SEC_FALC_IS_DBG_MODE())
    {
        *pFuseVersion = SEC2_AD10X_DEFAULT_UCODE_VERSION;
        return FLCN_OK;
    }
    else
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));

        chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

        switch(chip)
        {
            case LW_PMC_BOOT_42_CHIP_ID_AD102:
            case LW_PMC_BOOT_42_CHIP_ID_AD103:
            case LW_PMC_BOOT_42_CHIP_ID_AD104:
            case LW_PMC_BOOT_42_CHIP_ID_AD106:
            case LW_PMC_BOOT_42_CHIP_ID_AD107:
            {
                *pFuseVersion = SEC2_AD102_UCODE_VERSION;
                return FLCN_OK;
            }
            default:
            {
                return FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
            }
        }
    }

ErrorExit:
    return flcnStatus;
}

/*
 * Check if Chip is Supported - LS
 */
FLCN_STATUS
sec2CheckIfChipIsSupportedLS_AD10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_PMC_BOOT_42, &chip));

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_AD102:
        case LW_PMC_BOOT_42_CHIP_ID_AD103:
        case LW_PMC_BOOT_42_CHIP_ID_AD104:
        case LW_PMC_BOOT_42_CHIP_ID_AD106:
        case LW_PMC_BOOT_42_CHIP_ID_AD107:
        {
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

