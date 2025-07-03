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
 * @file   soe_lsflr10.c
 * @brief  Light Secure Falcon (LSF) LR10 HAL Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "csb.h"
#include "dev_soe_csb.h"
#include "dev_fuse.h"
#include "soe_bar0.h"

/* ------------------------ Application includes --------------------------- */

#include "soesw.h"
#include "rmlsfm.h"
#include "main.h"
#include "soe_objlsf.h"

#include "config/g_lsf_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */

/*!
 @brief: Program MTHDCTX PRIV level mask
 */
void
lsfSetupMthdctxPrivMask_LR10(void)
{
    LwU32 data32 = REG_RD32(CSB, LW_CSOE_FALCON_SCTL);

    // Check if UCODE is running in LS mode
    if (FLD_TEST_DRF(_CSOE, _FALCON_SCTL, _LSMODE, _TRUE, data32))
    {
        data32 = REG_RD32(CSB, LW_CSOE_FALCON_MTHDCTX_PRIV_LEVEL_MASK);

        data32 = FLD_SET_DRF(_CSOE, _FALCON_MTHDCTX_PRIV_LEVEL_MASK,
                             _READ_PROTECTION, _ALL_LEVELS_ENABLED, data32);
        data32 = FLD_SET_DRF(_CSOE, _FALCON_MTHDCTX_PRIV_LEVEL_MASK,
                             _WRITE_PROTECTION_LEVEL0, _DISABLE, data32);

        REG_WR32(CSB, LW_CSOE_FALCON_MTHDCTX_PRIV_LEVEL_MASK, data32);
    }
}

/*!
 * @brief Check if the Falcon is running in secure mode.
 * The Caller of this function should take necessary action based on the returned value.
 * 
 * @return LW_TRUE: if Falcon is running at secure level 2.
 *         LW_FALSE: otherwise.
 */
LwBool
lsfVerifyFalconSelwreRunMode_LR10(void)
{
    LwU32 data32;

    // Verify that we are in LSMODE
    data32 = REG_RD32(CSB, LW_CSOE_FALCON_SCTL);
    if (FLD_TEST_DRF(_CSOE, _FALCON_SCTL, _LSMODE, _TRUE, data32))
    {
        // Check if the LS is at secure level 2 (configured in HS mode).
        if (DRF_VAL(_CSOE, _FALCON_SCTL, _LSMODE_LEVEL,  data32) == 2)
        {
            return LW_TRUE;
        }
    }

    // Falcon (SOE) IS NOT secure.
    return LW_FALSE;
}

/*!
 * Checks whether an access using the specified dmaIdx is permitted.
 *
 * @param[in]  dmaIdx    DMA index to check
 *
 * @return     LwBool    LW_TRUE  if the access is permitted
 *                       LW_FALSE if the access is not permitted
 */
LwBool
lsfIsDmaIdxPermitted_LR10
(
    LwU8 dmaIdx
)
{
    return LW_TRUE;
    // TODO_Apoorvg: Confirm this and update later
    // LwU32 val = REG_RD32(CSB, LW_CSOE_FBIF_REGIONCFG);
    // return (DRF_IDX_VAL(_CSOE, _FBIF_REGIONCFG, _T, dmaIdx, val) == LSF_UNPROTECTED_REGION_ID);
}

/*!
 * @brief Initialize aperture settings (protected TRANSCFG regs)
 */
void
lsfInitApertureSettings_LR10(void)
{
    //
    // The following setting updates only take affect if SOE is in LSMODE &&
    // UCODE_LEVEL > 0
    //

   // COHERENT_SYSMEM aperture for SPA.
    REG_WR32(CSB, LW_CSOE_FBIF_TRANSCFG(SOE_DMAIDX_PHYS_SYS_COH_FN0),
             (DRF_DEF(_CSOE, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL)        |
              DRF_DEF(_CSOE, _FBIF_TRANSCFG, _TARGET,         _COHERENT_SYSMEM) |
              DRF_DEF(_CSOE, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _BAR2_FN0)));

    // NONCOHERENT_SYSMEM aperture for SPA.
    REG_WR32(CSB, LW_CSOE_FBIF_TRANSCFG(SOE_DMAIDX_PHYS_SYS_NCOH_FN0),
             (DRF_DEF(_CSOE, _FBIF_TRANSCFG, _MEM_TYPE,       _PHYSICAL)           |
              DRF_DEF(_CSOE, _FBIF_TRANSCFG, _TARGET,         _NONCOHERENT_SYSMEM) |
              DRF_DEF(_CSOE, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _BAR2_FN0)));
}

FLCN_STATUS
lsfVerifyPrivSecEnabled_LR10(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Halt SOE if PRIV_SEC is disabled on production boards
    if (SOECFG_FEATURE_ENABLED(SOE_PRIV_SEC_HALT_IF_DISABLED))
    {
        if (FLD_TEST_DRF(_CSOE, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, REG_RD32(CSB, LW_CSOE_SCP_CTL_STAT)))
        {
            if(FLD_TEST_DRF(_FUSE, _OPT_PRIV_SEC_EN, _DATA, _NO, REG_RD32(BAR0, LW_FUSE_OPT_PRIV_SEC_EN)))
            {
                SOE_HALT();
                status = FLCN_ERR_PRIV_SEC_VIOLATION;
            }
        }
    }

    return status;
}
