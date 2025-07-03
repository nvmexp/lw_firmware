/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_vprtu10x.c
 * @brief  VPR HAL Functions for TU10X
 *
 * VPR HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "flcntypes.h"
#include "sec2sw.h"
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
 * Get LW_FUSE_OPT_SW_VPR_ENABLED value
 * According to Bug 200333624, spare_bit_13 have been replaced with dedicated fuse
 */
FLCN_STATUS
vprIsAllowedInSWFuse_TU10X(void)
{
    LwU32       spareBit;
    FLCN_STATUS flcnStatus = FLCN_ERR_VPR_APP_NOT_SUPPORTED_BY_SW;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_SW_VPR_ENABLED, &spareBit));
    if (FLD_TEST_DRF(_FUSE, _OPT_SW_VPR_ENABLED, _DATA, _YES, spareBit))
    {
        flcnStatus = FLCN_OK;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * Verify is this build should be allowed to run on particular chip
 */
FLCN_STATUS
vprCheckIfBuildIsSupported_TU10X(void)
{
    LwU32       chip;
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU102:
        case LW_PMC_BOOT_42_CHIP_ID_TU104:
        case LW_PMC_BOOT_42_CHIP_ID_TU106:
        case LW_PMC_BOOT_42_CHIP_ID_TU116:
        case LW_PMC_BOOT_42_CHIP_ID_TU117:
            flcnStatus = FLCN_OK;
            break;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Set trust levels for all VPR clients
 */
FLCN_STATUS
vprSetupClientTrustLevel_TU10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERROR;
    LwU32 cyaLo            = 0;
    LwU32 cyaHi            = 0;

    //
    // Check if VPR is already enabled, if yes fail with error
    // Since we need to setup client trust levels before enabling VPR
    //

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_LO, &cyaLo));
    if (FLD_TEST_DRF(_PFB, _PRI_MMU_VPR_CYA_LO, _IN_USE, _TRUE, cyaLo))
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
    cyaLo = (DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_DEFAULT, LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_CPU,     LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_HOST,    LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PERF,    LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PMU,     LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_CE2,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_SEC,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_FE,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PD,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_SCC,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_SKED,    LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_L1,      LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_TEX,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PE,      LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE));

    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_LO, cyaLo));

    //
    // VPR_AWARE list from CYA_HI : RASTER, GCC, PROP, all LWENCs, LWDEC
    // Note that the value is changing here when compared to GP10X because DWBIF is not present on GV100_and_later
    // TODO pjambhlekar: Revisit VPR trust levels and check if accesses can be restristred further
    //
    cyaHi = (DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_RASTER,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_GCC,        LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_GPCCS,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_PROP,       LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_PROP_READ,  LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_PROP_WRITE, LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_DNISO,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_LWENC,      LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_LWDEC,      LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_GSP,        LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_LWENC1,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE));

    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_HI, cyaHi));

    flcnStatus = FLCN_OK;
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief vprSetupVprRangeInMmu_TU10X: 
 *  Setup VPR in MMU
 *
 * @param [in] startAddrInBytes   Start address of VPR in bytes
 * @param [in] endAddrInBytes     End address of VPR in bytes
 *
 * @return FLCN_OK on success
 */
FLCN_STATUS
vprSetupVprRangeInMmu_TU10X
(
    LwU64 startAddrInBytes,
    LwU64 endAddrInBytes
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32 startAddr4KAligned = 0;
    LwU32 endAddr4KAligned   = 0;

    // Right shift to get address in 4K unit
    startAddr4KAligned = (LwU32)(startAddrInBytes >> LW_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT);
    endAddr4KAligned   = (LwU32)(endAddrInBytes >> LW_PFB_PRI_MMU_VPR_ADDR_HI_ALIGNMENT);

    // Set the start and end address of VPR
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_ADDR_LO, 
                                              DRF_NUM(_PFB, _PRI_MMU_VPR_ADDR_LO, _VAL, startAddr4KAligned)));
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_ADDR_HI, 
                                              DRF_NUM(_PFB, _PRI_MMU_VPR_ADDR_HI, _VAL, endAddr4KAligned)));

ErrorExit:
    return flcnStatus;
}


/*!
 * @brief vprReleaseMemlock_TU10X: 
 *  Release memory lock after scrubbing
 *
 * @return FLCN_OK on success
 */
FLCN_STATUS
vprReleaseMemlock_TU10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Release the Mem Lock
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_LOCK_ADDR_LO, 
                                              DRF_NUM(_PFB, _PRI_MMU_LOCK_ADDR_LO, _VAL, ILWALID_MEMLOCK_RANGE_ADDR_LO)));
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_LOCK_ADDR_HI, 
                                              DRF_NUM(_PFB, _PRI_MMU_LOCK_ADDR_HI, _VAL, ILWALID_MEMLOCK_RANGE_ADDR_HI)));

ErrorExit:
    return flcnStatus;
}


/*!
 * @brief vprEnableVprInMmu_TU10X: 
 *  Enable VPR in MMU
 *
 * @param [in] bEnableVpr   Updated value of the IN_USE bit of VPR_CYA_LO
 *
 * @return FLCN_OK on success
 */
FLCN_STATUS
vprEnableVprInMmu_TU10X
(
    LwBool bEnableVpr
)
{
    LwU32 cyaLo = 0;
    FLCN_STATUS flcnStatus = FLCN_OK;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_LO, &cyaLo));
    cyaLo = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR, _CYA_LO_IN_USE, bEnableVpr, cyaLo);
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_LO, cyaLo));

ErrorExit:
    return flcnStatus;
}


/*!
 * @brief vprSetMemLockRange_TU10X: 
 *  Validate and set mem lock range for write protection before scrubbing.
 *
 * @param [in] startAddrInBytes   Start address of MEM LOCK region(in bytes)
 * @param [in] endAddrInBytes     End address of MEM LOCK region(in bytes)
 *
 * @return FLCN_OK on success
 */
FLCN_STATUS
vprSetMemLockRange_TU10X
(
    LwU64   startAddrInBytes,
    LwU64   endAddrInBytes
)
{
    LwU32 memlockLwrrStartAddr4KAligned;
    LwU32 memlockLwrrEndAddr4KAligned;
    LwU32 startAddr4KAligned;
    LwU32 endAddr4KAligned;
    FLCN_STATUS flcnStatus = FLCN_OK;

    if (startAddrInBytes > endAddrInBytes)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    // Right shift to get address in 4K unit
    startAddr4KAligned = startAddrInBytes >> LW_PFB_PRI_MMU_LOCK_ADDR_LO_ALIGNMENT;
    endAddr4KAligned   = endAddrInBytes   >> LW_PFB_PRI_MMU_LOCK_ADDR_HI_ALIGNMENT;

    // Read the mem lock range values
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_LOCK_ADDR_LO, &memlockLwrrStartAddr4KAligned));
    memlockLwrrStartAddr4KAligned = REF_VAL(LW_PFB_PRI_MMU_LOCK_ADDR_LO_VAL, memlockLwrrStartAddr4KAligned);
    
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_LOCK_ADDR_HI, &memlockLwrrEndAddr4KAligned));
    memlockLwrrEndAddr4KAligned = REF_VAL(LW_PFB_PRI_MMU_LOCK_ADDR_HI_VAL, memlockLwrrEndAddr4KAligned);

    //
    // Here, it is checked whether the mem lock range is already in use.
    // Comparison is done between 4K aligned addresses to avoid compiler issue of comparing two 64 bit numbers.
    // Also since memlock range is 4k aligned, this is not an issue functionally.
    //
    if (memlockLwrrEndAddr4KAligned >= memlockLwrrStartAddr4KAligned)
    {
        flcnStatus =  FLCN_ERR_VPR_APP_MEMLOCK_ALREADY_SET;
        goto ErrorExit;
    }
    else
    {
        // Set the mem lock range
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_LOCK_ADDR_LO, 
                                                DRF_NUM(_PFB, _PRI_MMU_LOCK_ADDR_LO, _VAL, startAddr4KAligned)));
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_LOCK_ADDR_HI, 
                                                DRF_NUM(_PFB, _PRI_MMU_LOCK_ADDR_HI, _VAL, endAddr4KAligned)));
    }

ErrorExit:
    return flcnStatus;
}


/*!
 * @brief vprUpdateMiscSettingsIfEnabled_TU10X: 
 *  Miscellaneous settings based on if VPR is enabled or not.
 *
 * @param [in] bEnabled         Used to check and update VPR settings   
 *
 * @return FLCN_OK on success
 */
FLCN_STATUS
vprUpdateMiscSettingsIfEnabled_TU10X
(
    LwBool bEnabled
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Update DISP_CRC
    CHECK_FLCN_STATUS(vprUpdateDispCrc_HAL(&VprHal, bEnabled));

    if (bEnabled)
    {
        LwU32 protectedMode;

        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_MODE, &protectedMode));
        protectedMode = FLD_SET_DRF(_PFB, _PRI_MMU_VPR_MODE, _VPR_NON_COMPRESSIBLE, _TRUE, protectedMode);
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_MODE, protectedMode));
    }
    else
    {
        CHECK_FLCN_STATUS(vprClearDisplayBlankingPolicies_HAL(&VprHal));
    }

ErrorExit:
    return flcnStatus;
}


/*!
 * @brief vprGetLwrVprRangeInBytes_TU10X: 
 *  Get the current VPR Range
 *
 * @param [out] pVprStartAddrInBytes   Start address of VPR
 * @param [out] pVprEndAddrInBytes     End address of VPR
 *
 * @return FLCN_OK on success
 */
FLCN_STATUS
vprGetLwrVprRangeInBytes_TU10X
(
    LwU64 *pVprStartAddrInBytes,
    LwU64 *pVprEndAddrInBytes
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       regVal;

    if (!pVprStartAddrInBytes || !pVprEndAddrInBytes)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    // Read VPR Start Address and align to byte boundary
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_ADDR_LO, &regVal));
    regVal = DRF_VAL(_PFB, _PRI_MMU_VPR_ADDR_LO, _VAL, regVal);
    *pVprStartAddrInBytes = (LwU64)regVal << LW_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT;

    // Read VPR End Address and align to byte boundary
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_ADDR_HI, &regVal));
    regVal = DRF_VAL(_PFB, _PRI_MMU_VPR_ADDR_HI, _VAL, regVal);
    *pVprEndAddrInBytes  = (LwU64)regVal << LW_PFB_PRI_MMU_VPR_ADDR_HI_ALIGNMENT;
    *pVprEndAddrInBytes |= ((NUM_BYTES_IN_1_MB) - 1);

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief vprSetVprOffsetInBytes_TU10X: 
 *  Set the VPR offset in Bytes
 *
 * @param [out] pVprOffsetInBytes   
 * @param [in]  vprStartAddrIn4K     Start address of VPR (4k Aligned)
 *
 * @return FLCN_OK on success
 */
FLCN_STATUS
vprSetVprOffsetInBytes_TU10X
(
    LwU64 *pVprOffsetInBytes,
    LwU64 vprStartAddrIn4K
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    if (!pVprOffsetInBytes)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    *pVprOffsetInBytes = vprStartAddrIn4K << LW_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT;

ErrorExit:
    return flcnStatus;
}
