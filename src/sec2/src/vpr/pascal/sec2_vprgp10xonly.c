 /* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_vprgp10xonly.c
 * @brief  VPR HAL Functions for GP10X only
 *
 * VPR HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "flcntypes.h"
#include "sec2sw.h"
#include "lwosselwreovly.h"
#include "dev_disp.h"
#include "dev_disp_falcon.h"
#include "dev_fb.h"
#include "dev_therm.h"
#include "sec2_bar0_hs.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objvpr.h"
#include "config/g_vpr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * Check Display IP version. we only support GP10x_and_later for now
 */
FLCN_STATUS
vprIsDisplayIpVersionSupported_GP10X(void)
{
    LwU32       ipVersion;
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       majorVer;
    LwU32       minorVer;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_DSI_IP_VER, &ipVersion));
    majorVer = DRF_VAL(_PDISP, _DSI_IP_VER, _MAJOR, ipVersion);
    minorVer = DRF_VAL(_PDISP, _DSI_IP_VER, _MINOR, ipVersion);

    if ((majorVer == 2) && (minorVer >= 8))
    {
        flcnStatus = FLCN_OK;
    }
    else
    {
        flcnStatus = FLCN_ERR_VPR_APP_DISPLAY_VERSION_NOT_SUPPORTED;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief: Update display upstream CRC if VPR is enabled
 */
FLCN_STATUS
vprUpdateDispCrc_GP10X(LwBool bEnable)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       reg        = 0;

    if (bEnable)
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_UPSTREAM_CRC, &reg));
        reg = FLD_SET_DRF( _PDISP, _UPSTREAM_CRC, _ACCESS, _DISABLE, reg);
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PDISP_UPSTREAM_CRC, reg));
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Clear display blanking policies.
 */
FLCN_STATUS
vprClearDisplayBlankingPolicies_GP10X(void)
{
    // TODO first call into DPU and unlock type1

    LwU32       lwrHwPolicy;
    FLCN_STATUS flcnStatus = FLCN_OK;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL, &lwrHwPolicy));

    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_NO_HDCP,   _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP1X,    _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22,    _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_INTERNAL,  _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_ANALOG,    _DISABLE, lwrHwPolicy);
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_SLI,       _ENABLE,  lwrHwPolicy); // SLI  banking is always enabled, we do the same in VPR app.
    lwrHwPolicy = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_RWPR,             _ENABLE,  lwrHwPolicy); // RWPR banking is always enabled, we do the same in VPR app.

    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL, lwrHwPolicy));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Check if DISP falcon is in LS mode
 */
FLCN_STATUS
vprCheckIfDispFalconIsInLSMode_GP10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       flcnSctl    = DRF_DEF(_PDISP, _FALCON_SCTL, _LSMODE, _FALSE);

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_FALCON_SCTL, &flcnSctl));
    if (!FLD_TEST_DRF(_PDISP, _FALCON_SCTL, _LSMODE, _TRUE, flcnSctl))
    {
        flcnStatus = FLCN_ERR_VPR_APP_DISP_FALCON_IS_NOT_IN_LS_MODE;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Set trust levels for all VPR clients
 */
FLCN_STATUS
vprSetupClientTrustLevel_GP10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32 cyaLo = 0;
    LwU32 cyaHi = 0;

    //
    // Check if VPR is already enabled, if yes fail with error
    // Since we need to setup client trust levels before enabling VPR
    //
    CHECK_FLCN_STATUS(vprReadVprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_INFO_INDEX_CYA_LO, &cyaLo));
    if (FLD_TEST_DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_IN_USE, LW_TRUE, cyaLo))
    {
        return FLCN_ERR_VPR_APP_VPR_IS_ALREADY_ENABLED;
    }

    //
    // Setup VPR CYA's
    // To program CYA register, we can't do R-M-W, as there is no way to read the build in settings
    // RTL built in settings for all fields are provided by James Deming
    // Apart from these built in settings, we are only updating PD/SKED in CYA_LO and FECS/GPCCS in CYA_HI
    //

    // VPR_AWARE list from CYA_LO : all CE engines, SEC2, SCC, L1, TEX, PE
    cyaLo = (DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_DEFAULT, LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_CPU,     LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_HOST,    LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_PERF,    LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_PMU,     LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_CE2,     LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_SEC,     LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_FE,      LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_PD,      LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_SCC,     LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_SKED,    LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_L1,      LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_TEX,     LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_PE,      LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE));

    CHECK_FLCN_STATUS(vprWriteVprWprData_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_LO, cyaLo));

    //
    // VPR_AWARE list from CYA_HI : RASTER, GCC, PROP, all LWENCs, LWDEC, Audio falcon, DWBIF
    // Note that LWENC and LWENC1 are not aliases in the HW register
    //
    cyaHi = (DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_RASTER,     LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_GCC,        LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_GPCCS,      LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_PROP,       LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_PROP_READ,  LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_PROP_WRITE, LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_DNISO,      LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_LWENC,      LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_LWDEC,      LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_MSPPP,      LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_LWENC1,     LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_AFALCON,    LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_DFALCON,    LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_DWBIF,      LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE));

    CHECK_FLCN_STATUS(vprWriteVprWprData_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_HI, cyaHi));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Check if THERM and MMU PLM are at level3
 *        Since these two PLMs are variable on various GP10X boards based on fuses/IFFs,
 *        We need to ensure that they are level3 protected
 */
FLCN_STATUS
vprCheckConcernedPLM_GP10X(void)
{
    LwU32       privLevelMask;
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Check if THERM PLM is level3 protected
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_THERM_SELWRE_WR_SCRATCH_PRIV_LEVEL_MASK, &privLevelMask));
    if (!FLD_TEST_DRF(_THERM, _SELWRE_WR_SCRATCH_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_DISABLED, privLevelMask))
    {
        flcnStatus = FLCN_ERR_INSUFFICIENT_PMB_PLM_PROTECTION;
        goto ErrorExit;
    }

    //
    // Check if MMU PLM is level3 protected
    // Skipe this check on debug boards
    //
    if (!(OS_SEC_FALC_IS_DBG_MODE()))
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_PRIV_LEVEL_MASK, &privLevelMask));

        if (!FLD_TEST_DRF(_PFB, _PRI_MMU_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_DISABLED, privLevelMask))
        {
            flcnStatus = FLCN_ERR_INSUFFICIENT_PMB_PLM_PROTECTION;
            goto ErrorExit;
        }
    }

ErrorExit:
    return flcnStatus;
}

