/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_vprga10x.c
 * @brief  VPR HAL Functions for GA10X
 *
 * VPR HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "flcntypes.h"
#include "sec2sw.h"
#include "dev_disp.h"
#include "dev_disp_addendum.h"
#include "dev_fuse.h"
#include "dev_fb.h"
#include "dev_master.h"
/* ------------------------- System Includes -------------------------------- */
#include "csb.h"
#include "flcnretval.h"
#include "sec2_bar0_hs.h"
#include "dev_sec_pri.h"

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
vprCheckIfBuildIsSupported_GA10X(void)
{
    LwU32       chip;
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_GA102:
        case LW_PMC_BOOT_42_CHIP_ID_GA103:
        case LW_PMC_BOOT_42_CHIP_ID_GA104:
        case LW_PMC_BOOT_42_CHIP_ID_GA106:
        case LW_PMC_BOOT_42_CHIP_ID_GA107:
            flcnStatus = FLCN_OK;
            break;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * Check Display IP version
 */
FLCN_STATUS
vprIsDisplayIpVersionSupported_GA10X(void)
{
    LwU32       ipVersion;
    FLCN_STATUS flcnStatus = FLCN_ERR_VPR_APP_DISPLAY_VERSION_NOT_SUPPORTED;
    LwU32       majorVer;
    LwU32       minorVer;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_FE_IP_VER, &ipVersion));
    majorVer = DRF_VAL(_PDISP, _FE_IP_VER, _MAJOR, ipVersion);
    minorVer = DRF_VAL(_PDISP, _FE_IP_VER, _MINOR, ipVersion);

    if ((majorVer == LW_PDISP_FE_IP_VER_MAJOR_INIT) && (minorVer >= LW_PDISP_FE_IP_VER_MINOR_INIT))
    {
        flcnStatus = FLCN_OK;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief: Checks whether VPR app is running on Falcon core or not
 */
FLCN_STATUS
vprCheckIfRunningOnFalconCore_GA10X(void)
{
    FLCN_STATUS flcnStatus  = FLCN_ERR_VPR_APP_UNEXPECTEDLY_RUNNING_ON_RISCV;
    LwU32 riscvActiveStatus = 0;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(
    LW_PSEC_RISCV_CPUCTL, &riscvActiveStatus));

    if (!FLD_TEST_DRF(_PSEC, _RISCV_CPUCTL, _ACTIVE_STAT, _ACTIVE, riscvActiveStatus))
    {
        flcnStatus = FLCN_OK;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Set trust levels for all VPR clients
 */
FLCN_STATUS
vprSetupClientTrustLevel_GA10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERROR;
    LwU32 cyaLo            = 0;
    LwU32 cyaHi            = 0;

    //
    // Check if VPR is already enabled, if yes fail with error
    // Since we need to setup client trust levels before enabling VPR
    //

    CHECK_FLCN_STATUS(vprIsVprEnableInMmu_HAL()); 
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_HI, &cyaHi));
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_LO, &cyaLo));

    // Setup VPR CYA's
    //
    // VPR_AWARE list from CYA_LO : CE2 
    // GRAPHICS list from CYA_LO  : TEX, PE
    //
    cyaLo = (DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_DEFAULT, LW_PFB_PRI_MMU_VPR_CYA_LO_TRUST_DEFAULT_USE_CYA) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_CPU,     LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_HOST,    LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PERF,    LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PMU,     LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_CE2,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_SEC,     LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_FE,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PD,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_SCC,     LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_SKED,    LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_L1,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_TEX,     LW_PFB_PRI_MMU_VPR_CYA_GRAPHICS) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PE,      LW_PFB_PRI_MMU_VPR_CYA_GRAPHICS));


    //
    // VPR_AWARE list from CYA_HI : LWENC, LWDEC
    // GRAPHICS list from CYA_LO  : RASTER, GCC, PROP
    //
    cyaHi = (DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_RASTER,     LW_PFB_PRI_MMU_VPR_CYA_GRAPHICS) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_GCC,        LW_PFB_PRI_MMU_VPR_CYA_GRAPHICS) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_GPCCS,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_PROP,       LW_PFB_PRI_MMU_VPR_CYA_GRAPHICS) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_PROP_READ,  LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_PROP_WRITE, LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_DNISO,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_LWENC,      LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_LWDEC,      LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_GSP,        LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_LWENC1,     LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_OFA,        LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED));

    // 
    // *_VPR_CYA_LO_TRUST_DEFAULT bit is the knob to apply trust levels programmed in *_LO and *_HI registers.
    //  So *_LO register is programmed after *_HI.
    //
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_HI, cyaHi));
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_LO, cyaLo));

    flcnStatus = FLCN_OK;
ErrorExit:
    return flcnStatus;
}
