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
 * @file   sec2_vprgv100only.c
 * @brief  VPR HAL Functions for GV100 only chips
 *
 * VPR HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "flcntypes.h"
#include "sec2sw.h"
#include "dev_disp.h"
#include "dev_fb.h"
#include "dev_master.h"
#include "dev_gsp.h"
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
 * Verify is this build should be allowed to run on particular chip
 */
FLCN_STATUS
vprCheckIfBuildIsSupported_GV100(void)
{
    LwU32       chip;
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

    if (chip == LW_PMC_BOOT_42_CHIP_ID_GV100)
    {
        flcnStatus = FLCN_OK;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * Check Display IP version
 */
FLCN_STATUS
vprIsDisplayIpVersionSupported_GV100(void)
{
    LwU32       ipVersion;
    FLCN_STATUS flcnStatus = FLCN_ERR_VPR_APP_DISPLAY_VERSION_NOT_SUPPORTED;
    LwU32       majorVer;
    LwU32       minorVer;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_FE_IP_VER, &ipVersion));
    majorVer = DRF_VAL(_PDISP, _FE_IP_VER, _MAJOR, ipVersion);
    minorVer = DRF_VAL(_PDISP, _FE_IP_VER, _MINOR, ipVersion);

    if ((majorVer == 3) && (minorVer >= 1))
    {
        flcnStatus = FLCN_OK;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Set trust levels for all VPR clients
 *     TODO: Revisit the trust level to check if it can be restricted further
 */
FLCN_STATUS
vprSetupClientTrustLevel_GV100(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERROR;
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
    // VPR_AWARE list from CYA_HI : RASTER, GCC, PROP, all LWENCs, LWDEC, Audio falcon
    // Note that the value is changing here when compared to GP10X because DWBIF is not present on GV100_and_later
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
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_GSP,        LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_LWENC1,     LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_HI_TRUST_AFALCON,    LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_VPR_AWARE));

    CHECK_FLCN_STATUS(vprWriteVprWprData_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_HI, cyaHi));

    flcnStatus = FLCN_OK;
ErrorExit:
    return flcnStatus;
}


