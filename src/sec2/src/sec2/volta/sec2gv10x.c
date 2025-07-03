/*
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2gv10x.c
 * @brief  SEC2 HAL Functions for GV10X and later chips
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */
/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_bar0_hs.h"
#include "sec2_csb.h"
#include "lwosselwreovly.h"

#include "dev_sec_csb.h"
#include "dev_se_seb.h"
#include "dev_master.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_bar0_hs.h"

#include "config/g_sec2_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Early initialization for GV10X chips.
 *
 * Any general initialization code not specific to particular engines should be
 * done here. This function must be called prior to starting the scheduler.
 */
FLCN_STATUS
sec2PreInit_GV10X(void)
{
    // DBGCTL is not accessible on GA10X with boot from HS fusing
#ifndef BOOT_FROM_HS_BUILD
    // Set up TRACEPC in stack mode
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_DBGCTL, _TRACE_MODE, _STACK);
#endif // ifndef BOOT_FROM_HS_BUILD

    return sec2PreInit_GP10X();
}


/*!
 * @brief   Program Bar0 timeout value greater than Host timeout.
 * @see     http://lwbugs/2073014/277
 * @see     http://lwbugs/2073014/286
 *
 * @details We want a timeout of about 40ms, which is 68728522 LWDCLK cycles
 *          at 1717MHz.   From the bug comment:
 *          40000000 / 0.582 = 68728522 ~= 40000000 *1.717
 */
void
sec2ProgramBar0Timeout_GV10X(void)
{
    const LwU32 bar0_timeout_min = 68728522;
    REG_WR32(CSB, LW_CSEC_BAR0_TMOUT,
             DRF_NUM(_CSEC, _BAR0_TMOUT, _CYCLES, bar0_timeout_min));
}


FLCN_STATUS
sec2CheckSelwreBusAddRangeHs_GV10X(LwU32  addr)
{
    FLCN_STATUS retVal = FLCN_ERROR;

    if( addr <= LW_SSE_SE_SWITCH_DISP_ADDR_END && addr >= LW_SSE_SE_SWITCH_DISP_ADDR_START)
    {
        retVal = FLCN_OK;
    }

    return retVal;
}


/*
 * @brief Get the SW fuse version
 */
#define SEC2_GV10X_UCODE_VERSION    (0x0)
#define SEC2_GV100_UCODE_VERSION    (0x3)
FLCN_STATUS
sec2GetSWFuseVersionHS_GV10X
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_BAR0_PRIV_READ_ERROR;
    LwU32 chip             = 0;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

    if (chip == LW_PMC_BOOT_42_CHIP_ID_GV100)
    {
        *pFuseVersion = SEC2_GV100_UCODE_VERSION;
    }
    else
    {
        if (OS_SEC_FALC_IS_DBG_MODE())
        {
            *pFuseVersion = SEC2_GV10X_UCODE_VERSION;
        }
        else
        {
            return FLCN_ERR_HS_PROD_MODE_NOT_YET_SUPPORTED;
        }
    }

    flcnStatus = FLCN_OK;
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Make sure the chip is allowed to do Playready
 *
 * Note that this function should never be prod signed for a chip that doesn't
 * have a silicon.
 *
 * @returns  FLCN_OK if chip is allowedto do Playready
 *           FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED otherwise
 */
FLCN_STATUS
sec2EnforceAllowedChipsForPlayreadyHS_GV10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);
    if (chip == LW_PMC_BOOT_42_CHIP_ID_GV100)
    {
        return FLCN_OK;
    }
    CHECK_FLCN_STATUS(FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED);

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Ensure that falcons are in consistent debug/prod mode
 *
 * @returns  FLCN_OK if falcons are in consistent mode
 *           FLCN_ERR_HS_CHK_INCONSISTENT_PROD_MODES otherwise
 */
FLCN_STATUS
sec2EnsureConsistentFalconsProdModeHS_GV10X(void)
{
    FLCN_STATUS flcnStatus  = FLCN_ERR_BAR0_PRIV_READ_ERROR;
    LwBool bSec2ProdMode    = !OS_SEC_FALC_IS_DBG_MODE();
    LwBool bPmuProdMode     = LW_FALSE;
    LwBool bLwdecProdMode   = LW_FALSE;
    LwBool bGspLiteProdMode = LW_FALSE;
    LwU32 data32            = 0;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_SELWRE_PMU_DEBUG_DIS, &data32));
    bPmuProdMode     = FLD_TEST_DRF(_FUSE, _OPT_SELWRE_PMU_DEBUG_DIS,   _DATA, _YES, data32);

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_SELWRE_LWDEC_DEBUG_DIS, &data32));
    bLwdecProdMode   = FLD_TEST_DRF(_FUSE, _OPT_SELWRE_LWDEC_DEBUG_DIS, _DATA, _YES, data32);

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_SELWRE_GSP_DEBUG_DIS, &data32));
    bGspLiteProdMode = FLD_TEST_DRF(_FUSE, _OPT_SELWRE_GSP_DEBUG_DIS, _DATA, _YES, data32);

    if (!(bSec2ProdMode && bPmuProdMode && bLwdecProdMode && bGspLiteProdMode) &&
        !(!bSec2ProdMode && !bPmuProdMode && !bLwdecProdMode && !bGspLiteProdMode))
    {
        return FLCN_ERR_HS_CHK_INCONSISTENT_PROD_MODES;
    }

    flcnStatus = FLCN_OK;
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Ensure the ucode is running at expected falcon (i.e. SEC2)
 *
 * @returns  FLCN_OK if the ucode is running at SEC2
 *           FLCN_ERR_HS_CHK_ENGID_MISMATCH otherwise
 */
FLCN_STATUS
sec2EnsureUcodeRunningOverSec2HS_GV10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_ENGID_MISMATCH;
    LwU32       engId      = 0;

    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CSEC_FALCON_ENGID, &engId));

    if (!FLD_TEST_DRF_NUM(_CSEC, _FALCON_ENGID, _INSTID, 0, engId) ||
        !FLD_TEST_DRF(_CSEC, _FALCON_ENGID, _FAMILYID, _SEC, engId))
    {
        CHECK_FLCN_STATUS(FLCN_ERR_HS_CHK_ENGID_MISMATCH);
    }

ErrorExit:
    return flcnStatus;
}

