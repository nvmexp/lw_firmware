/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dispflcngv10x.c
 * @brief  GSPLITE (GV10X) Hal Functions
 *
 * GSPLite HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "dispflcn_regmacros.h"
#include "gsp_hs.h"
#include "gsp_bar0_hs.h"
#include "gsp_prierr.h"
#include "dev_fuse.h"
#include "dev_disp_seb.h"
#include "dev_fuse_addendum.h"
#include "dev_gsp_csb_addendum.h"
#include "dev_gc6_island_addendum.h"

/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "dpu_objdpu.h"
#include "config/g_dpu_private.h"
#include "hdcp22wired_selwreaction.h"
#include "lwosselwreovly.h"
#include "seapi.h"
/* ------------------------- Type Definitions ------------------------------- */
#define PDI_BASED_PRODSIGNING                           (0)
#define GSP_GV10X_UCODE_VERSION                         (0x3)

#if PDI_BASED_PRODSIGNING
#define MAX_HDCP_INTERNAL_BOARDS                        (4)
#endif

#define LW_PGSP_SCP_RNDCTL0_INITIAL_VALUE               (0)
#define LW_PGSP_SCP_RNDCTL1_INITIAL_HOLDOFF_MASK        (0x03FF)
#define LW_PGSP_SCP_RNDCTL11_INITIAL_RING_LENGTH_VALUE  (0x000F)
/* ------------------------- External Definitions --------------------------- */
#ifdef DMEM_VA_SUPPORTED
extern LwUPtr   _dmem_ovl_start_address[];
extern LwU16    _dmem_ovl_size_max[];
#endif // DMEM_VA_SUPPORTED

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/*!
* @brief Early initialization for GSP-Lite (GV10X)
*
* Any general initialization code not specific to particular engines should be
* done here. This function must be called prior to starting the scheduler.
*/
FLCN_STATUS
dpuInit_GV10X(void)
{
    LwU32 fuseValue;

    // TODO: Implement GSP-Lite specific ACR code

    // At the start of binary, set CPUCTL_ALIAS bit to FALSE so that external entity cannot restart
    // the falcon from any abrupt point.
    DFLCN_REG_WR_DEF(CPUCTL, _ALIAS_EN, _FALSE);

#ifdef DMEM_VA_SUPPORTED
    //
    // Initialize DMEM_VA_BOUND. This can be done as soon as the initialization
    // arguments are cached. RM copies those to the top of DMEM, and once we
    // have cached them into our local structure, all of the memory above
    // DMEM_VA_BOUND can be used as the swappable region.
    //
    DFLCN_REG_WR_NUM(DMVACTL, _BOUND, PROFILE_DMEM_VA_BOUND);

    // Enable SP to take values above 64kB
    DFLCN_REG_WR_DEF(DMCYA, _LDSTVA_DIS, _FALSE);
#endif

    dpuBar0TimeoutInit_HAL();

    // Check if the GPU is Display Capable or Not.
    fuseValue = EXT_REG_RD32(LW_FUSE_STATUS_OPT_DISPLAY);
    if (!FLD_TEST_DRF( _FUSE, _STATUS_OPT_DISPLAY, _DATA, _ENABLE, fuseValue))
    {
        DPU_HALT();
    }

    //
    // After we are done with GV10X specific stuff, call into the GM20X version dpuInit_v02_05
    // to do common initialization which is common between GV10X and GP10X.
    //
    return dpuInit_v02_05();
}

/*
 * @brief Program timeout value for BAR0 transactions
 */
void
dpuBar0TimeoutInit_GV10X(void)
{
    //
    // Initialize the timeout value within which the host has to ack a read or
    // write transaction.
    //
    // TODO (nkuo): Due to bug 1758045, we need to use a super large timeout to
    // suppress some of the timeout failures.  Will need to restore this value
    // once the issue is resolved.
    // (Ray Ngai found that 0x4000 was not sufficient with TOT, so 0xFFFFFF
    // is picked up for now)
    //
    // BUG 2073014: increase timeout to 40ms.
    // The value is callwlated from a maximum LWDCLK of 1717MHz
    // 40 / 1000 / (1/LWDCLK_MAX) = 68728522
    //
    DENG_REG_WR32(BAR0_TMOUT,
             DENG_DRF_NUM(BAR0_TMOUT, _CYCLES, 68728522));
}

/*!
 * @brief Enable the EMEM aperture to map EMEM to the top of DMEM VA space
 */
void
dpuEnableEmemAperture_GV10X(void)
{
    //
    // keep timeout at default (256 cycles) and enable DMEM Aperature
    // total timeout in cycles := (DMEMAPERT.time_out + 1) * 2^(DMEMAPERT.time_unit + 8)
    //
    LwU32 apertVal = DFLCN_DRF_DEF(DMEMAPERT, _ENABLE, _TRUE);
    DFLCN_REG_WR32(DMEMAPERT, apertVal);
}

/*!
 * Writes a 32-bit value to the given BAR0 address.  This is a nonposted write
 * (will wait for transaction to complete).  It is safe to call it repeatedly
 * and in any combination with other BAR0MASTER functions.
 *
 * @param[in]  addr  The BAR0 address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 */
FLCN_STATUS
dpuBar0RegWr32NonPosted_GV10X
(
    LwU32  addr,
    LwU32  data
)
{
    FLCN_STATUS status = FLCN_OK;

    lwrtosENTER_CRITICAL();
#ifdef UPROC_RISCV
    *(volatile LwU32*)(LWRISCV_PRIV_VA_BASE + addr) = data;
#else
    {
        status = _dpuBar0ErrorChkRegWr32NonPosted_GV10X(addr, data);
    }
#endif
    lwrtosEXIT_CRITICAL();

    return status;
}

/*!
 * Reads the given BAR0 address. The read transaction is nonposted (will wait
 * for transaction to complete). It is safe to call it repeatedly and in any
 * combination with other BAR0MASTER functions.
 *
 * @param[in]  addr  The BAR0 address to read
 *
 * @return The 32-bit value of the requested BAR0 address
 */
LwU32
dpuBar0RegRd32_GV10X
(
    LwU32  addr
)
{
    LwU32 val = 0;

    lwrtosENTER_CRITICAL();
#ifdef UPROC_RISCV
    val = *(volatile LwU32*)(LWRISCV_PRIV_VA_BASE + addr);
#else
    //TODO: return the error value to caller function
    {
        _dpuBar0ErrorChkRegRd32_GV10X(addr, &val);
    }
#endif
    lwrtosEXIT_CRITICAL();

    return val;
}

/*!
 * Writes a 32-bit value to the given BAR0 address.
 * TODO: change to use HW reg access library and remove the interface func.
 *
 * @param[in]  addr  The BAR0 address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 */
FLCN_STATUS
libInterfaceRegWr32ErrChk
(
    LwU32  addr,
    LwU32  data
)
{
    return dpuBar0RegWr32NonPosted_HAL(&Dpu.hal, addr, data);
}

/*!
 * Writes a 32-bit value to the given BAR0 address in HS.
 * TODO: change to use HW reg access library and remove the interface func.
 *
 * @param[in]  addr  The BAR0 address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 */
FLCN_STATUS
libInterfaceRegWr32ErrChkHs
(
    LwU32  addr,
    LwU32  data
)
{
    return EXT_REG_WR32_HS_ERRCHK(addr, data);
}

/*!
 * Reads the given BAR0 address. The read transaction is nonposted.
 * TODO: change to use HW reg access library and remove the interface func.
 *
 * @param[in]  addr  The BAR0 address to read
 *
 * @return The 32-bit value of the requested BAR0 address
 */
LwU32
libInterfaceRegRd32
(
    LwU32  addr
)
{
    return dpuBar0RegRd32_HAL(&Dpu.hal, addr);
}

/*!
 * Reads the given BAR0 address. The read transaction is nonposted.
 *
 * Checks the return read value for priv errors and returns a status.
 * It is up the the calling apps to decide how to handle a failing status.
 * TODO: change to use HW reg access library and remove the interface func.
 *
 * @param[in]  addr   The BAR0 address to read
 * @param[out] pVal   The 32-bit value of the requested BAR0 address
 *
 * @return The status of the read operation.
 */
FLCN_STATUS
libInterfaceRegRd32ErrChk
(
    LwU32   addr,
    LwU32  *pVal
)
{
    return dpuBar0ErrChkRegRd32_HAL(&Dpu.hal, addr, pVal);
}

/*!
 * Reads the given BAR0 address. The read transaction is nonposted in HS.
 *
 * Checks the return read value for priv errors and returns a status.
 * It is up the the calling apps to decide how to handle a failing status.
 * TODO: change to use HW reg access library and remove the interface func.
 *
 * @param[in]  addr   The BAR0 address to read
 * @param[out] pVal   The 32-bit value of the requested BAR0 address
 *
 * @return The status of the read operation.
 */
FLCN_STATUS
libInterfaceRegRd32ErrChkHs
(
    LwU32   addr,
    LwU32  *pVal
)
{
    return EXT_REG_RD32_HS_ERRCHK(addr, pVal);
}

/*
 * @brief Make sure the chip is allowed to do HDCP
 */
void
dpuEnforceAllowedChipsForHdcpHS_GV10X(void)
{
    LwU32 chip;
    LwU32 data32 = 0;

    if (FLCN_OK != EXT_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &data32))
    {
        DPU_HALT();
    }

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, data32);

    if (chip != LW_PMC_BOOT_42_CHIP_ID_GV100)
    {
        DPU_HALT();
    }
}

/*!
 * @brief Get the HW fuse version
 */
FLCN_STATUS
dpuGetHWFuseVersionHS_GV10X(LwU32* pFuseVersion)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       data32;

    if (pFuseVersion == NULL)
    {
        flcnStatus = FLCN_ERR_HS_CHK_ILWALID_INPUT;
        goto ErrorExit;
    }
    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_FUSE_UCODE_GSP_REV, &data32));
    *pFuseVersion = DRF_VAL(_FUSE, _OPT_FUSE_UCODE_GSP_REV, _DATA, data32);

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Get the SW Ucode version
 */
FLCN_STATUS
dpuGetUcodeSWVersionHS_GV10X(LwU32* pUcodeVersion)
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

    if (chip == LW_PMC_BOOT_42_CHIP_ID_GV100)
    {
        *pUcodeVersion = GSP_GV10X_UCODE_VERSION;
    }
    else
    {
        if (OS_SEC_FALC_IS_DBG_MODE())
        {
            *pUcodeVersion = GSP_GV10X_UCODE_VERSION;
        }
        else
        {
            flcnStatus = FLCN_ERR_HS_PROD_MODE_NOT_YET_SUPPORTED;
        }
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Ensure that HW fuse version matches SW ucode version
 *
 * @return FLCN_OK  if HW fuse version and SW ucode version matches
 *         FLCN_ERR_HS_CHK_UCODE_REVOKED  if mismatch
 */
FLCN_STATUS
dpuCheckFuseRevocationHS_GV10X(void)
{
    // TODO: Define a compile time variable to specify this via make arguments
    LwU32       fuseVersionHW = 0;
    LwU32       ucodeVersionSW = 0;
    FLCN_STATUS status        = FLCN_OK;

    // Read the version number from FUSE
    status = dpuGetHWFuseVersionHS_HAL(&Dpu.hal, &fuseVersionHW);
    if (status != FLCN_OK)
    {
        return status;
    }

    status = dpuGetUcodeSWVersionHS_HAL(&Dpu.hal, &ucodeVersionSW);
    if (status != FLCN_OK)
    {
        return status;
    }

    if (ucodeVersionSW < fuseVersionHW)
    {
        return FLCN_ERR_HS_CHK_UCODE_REVOKED;
    }
    return FLCN_OK;
}

/*!
 * @brief  Write GSPLite/GSP ucode version in to secure scratch register
 *
 * @return FLCN_OK  if succesfully GSP ucode version is updated
 */
FLCN_STATUS
dpuWriteDpuBilwersionToBsiSelwreScratchHS_GV10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERROR;
    LwU32       lwrrentUcodeVersion = 0;
    LwU32       val;
    LwU32       gspUcodeVersionInScratch;
    LwU32       plmVal = 0;
    LwU32       versionToWriteToScratch;
    LwBool      bPlmNeedsUpdate = LW_FALSE;

    // Get current PLM Mask and allow level0 read protection
    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00_PRIV_LEVEL_MASK, &plmVal));

    val = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_04_00_PRIV_LEVEL_MASK, _READ_PROTECTION, plmVal);
    if (val != LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED)
    {
        plmVal = FLD_SET_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_04_00_PRIV_LEVEL_MASK, _READ_PROTECTION, _ALL_LEVELS_ENABLED, plmVal);
        bPlmNeedsUpdate = LW_TRUE;
    }

    val =  DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_04_00_PRIV_LEVEL_MASK, _READ_VIOLATION, plmVal);
    if (val != LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00_PRIV_LEVEL_MASK_READ_VIOLATION_REPORT_ERROR)
    {
        plmVal = FLD_SET_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_04_00_PRIV_LEVEL_MASK, _READ_VIOLATION, _REPORT_ERROR, plmVal);
        bPlmNeedsUpdate = LW_TRUE;
    }

    if (bPlmNeedsUpdate)
    {
        CHECK_FLCN_STATUS(EXT_REG_WR32_HS_ERRCHK(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00_PRIV_LEVEL_MASK, plmVal));
    }
    val = 0;

    // Get the current version of GSPLite/GSP ucode
    CHECK_FLCN_STATUS(dpuGetUcodeSWVersionHS_HAL(&Dpu.hal, &lwrrentUcodeVersion));
    // Get the version stored in the secure scratch
    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00, &val));
    gspUcodeVersionInScratch = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_04_00, _GSP_UCODE_VERSION, val);

    //
    // Across multiple driver runs, that could have different versions of GSP ucode, we will maintain the lowest version seen so far, in secure scratch
    // This logic has been borrowed from scrubber binary from function selwrescrubWriteScrubberBilwersionToBsiSelwreScratch_GP10X
    //
    if ((gspUcodeVersionInScratch == 0) ||
        (lwrrentUcodeVersion < gspUcodeVersionInScratch))
    {
        versionToWriteToScratch = lwrrentUcodeVersion;
    }
    else
    {
        versionToWriteToScratch = gspUcodeVersionInScratch;
    }

    // if an update has to be done, then only do it
    if (versionToWriteToScratch != gspUcodeVersionInScratch)
    {
        val = FLD_SET_DRF_NUM(_PGC6, _AON_SELWRE_SCRATCH_GROUP_04_00, _GSP_UCODE_VERSION, versionToWriteToScratch, val);
        CHECK_FLCN_STATUS(EXT_REG_WR32_HS_ERRCHK(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00, val));

        val = 0;
        // Now read back the version and verify it was written correctly.
        CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00, &val));
        gspUcodeVersionInScratch = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_04_00, _GSP_UCODE_VERSION, val);
        if (versionToWriteToScratch != gspUcodeVersionInScratch)
        {
            flcnStatus = FLCN_ERR_ILWALID_STATE;
            goto ErrorExit;
        }
    }

    return FLCN_OK;
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief  Update the Reset PLM's so that NS/LS entity cannot reset GSP/GSPLite while in HS
 *         Note: Not updating ENG_CTL_PLM here as ENGCTL is in AllowedList when PRIV had been
 *               locked-down to enable stall engine when there is some error in secure mode.
 *
 * @params[in]  bIsRaise   If it is raise or not.
 * @returns     FLCN_OK    FLCN_OK on successfull exelwtion
 */
FLCN_STATUS
dpuUpdateResetPrivLevelMaskHS_GV10X(LwBool bIsRaise)
{
    FLCN_STATUS flcnStatus   = FLCN_ERROR;
    LwU32       resetPlmVal  = 0;
    LwU32       data32       = 0;

    // Read the existing priv level masks
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CGSP_FALCON_RESET_PRIV_LEVEL_MASK, &resetPlmVal));

    if (bIsRaise)
    {
        // Lock falcon reset for level 0, 1, 2
        resetPlmVal = FLD_SET_DRF(_CGSP, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, resetPlmVal);
        resetPlmVal = FLD_SET_DRF(_CGSP, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, resetPlmVal);
        resetPlmVal = FLD_SET_DRF(_CGSP, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, resetPlmVal);
    }
    else
    {
        // Downgrade the access level/restore it to previous level while HS exit
        resetPlmVal = FLD_SET_DRF(_CGSP, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, resetPlmVal);
        resetPlmVal = FLD_SET_DRF(_CGSP, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE, resetPlmVal);

        //
        // If ucode is not authorized to run at LS mode, enable for LEVEL0
        // For example: DFPGA, Non-ACR case
        //
        CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CGSP_FALCON_SCTL, &data32));

        if (FLD_TEST_DRF(_CGSP, _FALCON_SCTL, _LSMODE, _TRUE, data32))
        {
            resetPlmVal = FLD_SET_DRF(_CGSP, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, resetPlmVal);
        }
        else
        {
            if (!OS_SEC_FALC_IS_DBG_MODE())
            {
                return FLCN_ERR_HS_UPDATE_RESET_PLM_ERROR;
            }
            else
            {
                resetPlmVal = FLD_SET_DRF(_CGSP, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, resetPlmVal);
            }
        }
    }

    // Write and update the register with new PLM
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CGSP_FALCON_RESET_PRIV_LEVEL_MASK, resetPlmVal));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief This is the HDCP common checker function which needs to be called (or indirectly
 *        called) at the beginning of common HS Entry of HDCP function.
 *
 * @params[in]  pArg         pointer to secure action argument
 * @return      FLCN_OK      If all security checks passes
 *                           Appropriate error status on failure.
 */
FLCN_STATUS
dpuHsHdcpPreCheck_GV10X
(
    void   *pArg
)
{
    FLCN_STATUS         flcnStatus         = FLCN_ERR_ILLEGAL_OPERATION;
    LwU32               data32             = 0xbadf;
    LwBool              bIsDispEngEnabled  = LW_FALSE;
    LwBool              bIsHdcpEnabled     = LW_FALSE;
    LwU8               *pArgMemCheckStart  = NULL;
    LwU8               *pArgMemCheckEnd    = NULL;
#if PDI_BASED_PRODSIGNING
    LwU32               pdiVal_0          = 0xbadf;
    LwU32               pdiVal_1          = 0xbadf;
    LwU8                index             = 0;
    LwBool              bValidBoard = LW_FALSE;
    LwU32               HDCP_VERIF_BOARD_PDI_0[MAX_HDCP_INTERNAL_BOARDS] = {0, 0, 0, 0};
    LwU32               HDCP_VERIF_BOARD_PDI_1[MAX_HDCP_INTERNAL_BOARDS] = {0, 0, 0, 0};
#endif

    // Check  HDCP_EN fuse, if it is disabled do not continue
    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_HDCP_EN, &data32));
    bIsHdcpEnabled = FLD_TEST_DRF(_FUSE, _OPT_HDCP_EN, _DATA, _YES, data32);
    if (!bIsHdcpEnabled)
    {
        flcnStatus = FLCN_ERR_HS_CHK_HDCP_DISABLED;
        goto ErrorExit;
    }

    // Check if display engine is enabled or not, if disabled do not continue
    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_STATUS_OPT_DISPLAY, &data32));
    bIsDispEngEnabled = FLD_TEST_DRF(_FUSE, _STATUS_OPT_DISPLAY, _DATA, _ENABLE, data32);
    if(!bIsDispEngEnabled)
    {
        flcnStatus = FLCN_ERR_HS_CHK_DISP_ENG_DISABLED;
        goto ErrorExit;
    }

    //
    // Check if MST PLMS Security Init is done
    // This will ensure MST register's Souce id mask is set to only GSP and CPU
    // During ECF programing it will be ensured that CPU access will be disabled
    // by disabling L0 write access to these registers
    //
    CHECK_FLCN_STATUS(dpuIsMstPlmsSelwrityInitDoneHS_HAL(&Dpu.hal));

    // Check if chip is allowed chip to run HDCP HS code
    dpuEnforceAllowedChipsForHdcpHS_HAL(&Dpu.hal);

#if PDI_BASED_PRODSIGNING
    // Check for specific board, only for validation
    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_PRIV_PDI_0, &pdiVal_0));

    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_PRIV_PDI_1, &pdiVal_1));

    bValidBoard = LW_FALSE;

    if(pdiVal_0 == 0 && pdiVal_1 == 0)
    {
        // PDI can't be entirely 0. That indicates unfused/improperly fused board.
        // Reject to run on this board.
        flcnStatus = FLCN_ERR_HS_CHK_IMPROPERLY_FUSED_BOARD;
        goto ErrorExit;
    }

    //Continuing with other PDI checks..
    for (index = 0; index < MAX_HDCP_INTERNAL_BOARDS; index++)
    {
        // For empty slot indicated by both PDIs as 0, we skip that entry.
        if ((HDCP_VERIF_BOARD_PDI_0[index] == 0) &&
            (HDCP_VERIF_BOARD_PDI_1[index] == 0))
        {
            continue;
        }

        if ((HDCP_VERIF_BOARD_PDI_0[index] == pdiVal_0) &&
            (HDCP_VERIF_BOARD_PDI_1[index] == pdiVal_1))
        {
            bValidBoard = LW_TRUE;
            break;
        }
    }

    if (!bValidBoard)
    {
        flcnStatus = FLCN_ERR_HS_CHK_BOARD_MISMATCH;
        goto ErrorExit;
    }
#endif

    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_INTERNAL_SKU, &data32));
#if PDI_BASED_PRODSIGNING
    // Ensure that SKU is internal, for internal prod signing
    if (!(FLD_TEST_DRF(_FUSE, _OPT_INTERNAL_SKU, _DATA, _ENABLE, data32)))
    {
        flcnStatus = FLCN_ERR_HS_OPT_INTERNAL_SKU_CHECK_FAILED;
        goto ErrorExit;
    }
#endif
    // Only early bringup board (fuseVersion 0) with
    // early bringup prod signed SW allows OPT_INTERNAL
    if ((FLD_TEST_DRF(_FUSE, _OPT_INTERNAL_SKU, _DATA, _ENABLE, data32)))
    {
        LwU32 fuseVersionHW  = 0;
        LwU32 ucodeVersionSW = 0;
        CHECK_FLCN_STATUS(dpuGetHWFuseVersionHS_HAL(&Dpu.hal, &fuseVersionHW));
        CHECK_FLCN_STATUS(dpuGetUcodeSWVersionHS_HAL(&Dpu.hal, &ucodeVersionSW));

        if (fuseVersionHW > 1 || ucodeVersionSW > 1)  // @vyadav: allowing version 1 to work with OPT_INTERNAL boards, signoff build to be added here.
        {
            flcnStatus = FLCN_ERR_HS_OPT_INTERNAL_SKU_CHECK_FAILED;
            goto ErrorExit;
        }
    }

   if(pArg == NULL)
    {
        flcnStatus = FLCN_ERR_HS_CHK_ILWALID_INPUT;
        goto ErrorExit;
    }

    if (
#ifdef DMEM_VA_SUPPORTED
        // Check if argument pointer comes from hdcp task stack overlay.
        !((((LwUPtr)pArg >= _dmem_ovl_start_address[OVL_INDEX_DMEM(hdcp22wiredHs)]) &&
            ((LwUPtr)pArg < (_dmem_ovl_start_address[OVL_INDEX_DMEM(hdcp22wiredHs)] +
                             _dmem_ovl_size_max[OVL_INDEX_DMEM(hdcp22wiredHs)]))) ||
          (((LwUPtr)pArg >= _dmem_ovl_start_address[OVL_INDEX_DMEM(hdcpHs)]) &&
            ((LwUPtr)pArg < (_dmem_ovl_start_address[OVL_INDEX_DMEM(hdcpHs)] +
                             _dmem_ovl_size_max[OVL_INDEX_DMEM(hdcpHs)])))) ||
#endif
        ((((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_REG_ACCESS) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_SCP_CALLWLATE_HASH) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_SCP_GEN_DKEY) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_START_SESSION) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_VERIFY_CERTIFICATE) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_KMKD_GEN) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_HPRIME_VALIDATION) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_LPRIME_VALIDATION) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_EKS_GEN) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_CONTROL_ENCRYPTION) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_VPRIME_VALIDATION) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_MPRIME_VALIDATION) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_WRITE_DP_ECF) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_END_SESSION) &&
         (((SELWRE_ACTION_ARG *)pArg)->actionType != SELWRE_ACTION_SRM_REVOCATION)))
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    // Check input argument structure that should be zero besides action struct.
    pArgMemCheckEnd = (LwU8*)&((SELWRE_ACTION_ARG *)pArg)->actionType;
    switch (((SELWRE_ACTION_ARG *)pArg)->actionType)
    {
    case SELWRE_ACTION_REG_ACCESS:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_HDCP_ARG *)pArg)->regAccessArg) + sizeof(SELWRE_ACTION_REG_ACCESS_ARG);
        pArgMemCheckEnd = (LwU8*)&((SELWRE_ACTION_HDCP_ARG *)pArg)->actionType;
        break;
    case SELWRE_ACTION_SCP_CALLWLATE_HASH:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_SCP_CALLWLATE_HASH_ARG);
        break;
    case SELWRE_ACTION_SCP_GEN_DKEY:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_SCP_GEN_DKEY_ARG);
        break;

        // new actions for LS/HS reorg.
    case SELWRE_ACTION_START_SESSION:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_START_SESSION_ARG);
        break;
    case SELWRE_ACTION_VERIFY_CERTIFICATE:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_VERIFY_CERTIFICATE_ARG);
        break;
    case SELWRE_ACTION_KMKD_GEN:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_KMKD_GEN_ARG);
        break;
    case SELWRE_ACTION_HPRIME_VALIDATION:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_HPRIME_VALIDATION_ARG);
        break;
    case SELWRE_ACTION_LPRIME_VALIDATION:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_LPRIME_VALIDATION_ARG);
        break;
    case SELWRE_ACTION_EKS_GEN:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_EKS_GEN_ARG);
        break;
    case SELWRE_ACTION_CONTROL_ENCRYPTION:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_CONTROL_ENCRYPTION_ARG);
        break;
    case SELWRE_ACTION_VPRIME_VALIDATION:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_VPRIME_VALIDATION_ARG);
        break;
    case SELWRE_ACTION_MPRIME_VALIDATION:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_MPRIME_VALIDATION_ARG);
        break;
    case SELWRE_ACTION_WRITE_DP_ECF:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_WRITE_DP_ECF_ARG);
        break;
    case SELWRE_ACTION_END_SESSION:
        pArgMemCheckStart = (LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action;
        break;
    case SELWRE_ACTION_SRM_REVOCATION:
        pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_SRM_REVOCATION_ARG);
        break;

    case SELWRE_ACTION_ILWALID:
    default:
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
        break;
    }

    // Check if input argumented tampered.
    while (pArgMemCheckStart < pArgMemCheckEnd)
    {
        if (*pArgMemCheckStart != 0x00)
        {
            flcnStatus = FLCN_ERR_HS_SELWRE_ACTION_ARG_CHECK_FAILED;
            goto ErrorExit;
        }
        pArgMemCheckStart++;
    }

    if (((SELWRE_ACTION_ARG *)pArg)->actionType == SELWRE_ACTION_REG_ACCESS)
    {
        PSELWRE_ACTION_REG_ACCESS_ARG pRegAccessArg = &((SELWRE_ACTION_ARG *)pArg)->action.regAccessArg;
        LwU32                         addr          = pRegAccessArg->addr;

        // Check address range and only allow addresses related to HDCP
        if ((addr > LW_SSE_SE_SWITCH_DISP_ADDR_END) ||
            (addr < LW_SSE_SE_SWITCH_DISP_ADDR_START))
        {
            flcnStatus = FLCN_ERR_HS_CHK_ILWALID_INPUT;
            goto ErrorExit;
        }
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief This is the common checker function which needs to be called at every HS
 * entry in order to make sure GSP/GSPLITE is running in a secure enough environment.
 * HS entry checks and sequence is captured in
 * https://confluence.lwpu.com/display/GFS/HS+Entry+and+Exit+Security+Checks
 *
 * @return FLCN_OK  If all HS common pre check passes
 *                  Appropriate error status on failure.
 */
FLCN_STATUS
dpuHsPreCheckCommon_GV10X(void)
{
    FLCN_STATUS flcnStatus    = FLCN_ERR_ILLEGAL_OPERATION;
    LwBool      bGspProdMode  = !OS_SEC_FALC_IS_DBG_MODE();
    LwBool      bPmuProdMode;
    LwBool      bSec2ProdMode;
    LwBool      bLwdecProdMode;
    LwU32       data32 = 0;
    LwBool      bLsMode = 0;

    // HS Entry step of forgetting signatures
    falc_scp_forget_sig();

    // Clear SCP registers
    _clearSCPregisters();

    //**************NO CSB ACCESS BEFORE THIS POINT *********************************

    // Ensure that CSB bus error handling is enabled
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CGSP_FALCON_CSBERRSTAT, &data32));

    data32 = FLD_SET_DRF(_CGSP, _FALCON_CSBERRSTAT, _ENABLE, _TRUE, data32);
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CGSP_FALCON_CSBERRSTAT, data32));

    // Write CMEMBASE = 0, to make sure ldd/std instructions are not routed to CSB.
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CGSP_FALCON_CMEMBASE, 0));

    // Ensure that CPUCTL_ALIAS is FALSE so that gsplite/gsp cannot be restarted
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CGSP_FALCON_CPUCTL, &data32));
    if (!FLD_TEST_DRF(_CGSP, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, data32))
    {
        flcnStatus = FLCN_ERR_HS_CHK_CPUCTL_ALIAS_FALSE;
        goto ErrorExit;
    }

    //******************* NO HALT BEFORE THIS POINT ********************************

    //
    // Raise Reset Protection to Level3 i.e to HS only.
    // We lower this back to LS settings during HS Exit.
    //
    if ((flcnStatus = dpuUpdateResetPrivLevelMaskHS_HAL(&Dpu.hal, LW_TRUE)) != FLCN_OK)
    {
        flcnStatus = FLCN_ERR_HS_UPDATE_RESET_PLM_ERROR;
        goto ErrorExit;
    }

    // Ensure that binary is not revoked
    if ((flcnStatus = dpuCheckFuseRevocationHS_HAL(&Dpu.hal)) != FLCN_OK)
    {
        goto ErrorExit;
    }

#if BOOT_FROM_HS
    // Check whether Bootstrap and FWSEC have run successfully to ensure Chain of Trust.
    if ((flcnStatus = dpuCheckChainOfTrustHS_HAL(&Dpu.hal)) != FLCN_OK)
    {
        goto ErrorExit;
    }
#endif

    // Check if current falcon is GSP/GSPLite
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CGSP_FALCON_ENGID, &data32));
    if (LW_CGSP_FALCON_ENGID_FAMILYID_GSP != DRF_VAL(_CGSP, _FALCON_ENGID, _FAMILYID, data32))
    {
        REG_WR32(CSB, LW_CGSP_FALCON_MAILBOX0, FLCN_ERR_HS_CHK_ENGID_MISMATCH);
        DPU_HALT();
    }

    // Write GSP Ucode vserion to Secure Scratch Register
    CHECK_FLCN_STATUS(dpuWriteDpuBilwersionToBsiSelwreScratchHS_HAL(&Dpu.hal));

    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CGSP_SELWRE_SCRATCH_0, &data32));
    // If the cached flcnStatus is valid, then use cached data directly to avoid unnecessary BAR0 access
    if (!FLD_TEST_DRF(_CGSP, _SELWRE_SCRATCH_0, _FLCN_PROD_MODE_CACHE, _VALID, data32))
    {
        CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_SELWRE_PMU_DEBUG_DIS, &data32));
        bPmuProdMode   = FLD_TEST_DRF(_FUSE, _OPT_SELWRE_PMU_DEBUG_DIS, _DATA, _YES, data32);
        CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS, &data32));
        bSec2ProdMode  = FLD_TEST_DRF(_FUSE, _OPT_SELWRE_SECENGINE_DEBUG_DIS, _DATA, _YES, data32);
        CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_SELWRE_LWDEC_DEBUG_DIS, &data32));
        bLwdecProdMode = FLD_TEST_DRF(_FUSE, _OPT_SELWRE_LWDEC_DEBUG_DIS, _DATA, _YES, data32);

        data32 &= ~(DRF_SHIFTMASK(LW_CGSP_SELWRE_SCRATCH_0_PMU_PROD_MODE)   |
                    DRF_SHIFTMASK(LW_CGSP_SELWRE_SCRATCH_0_SEC2_PROD_MODE) |
                    DRF_SHIFTMASK(LW_CGSP_SELWRE_SCRATCH_0_LWDEC_PROD_MODE) |
                    DRF_SHIFTMASK(LW_CGSP_SELWRE_SCRATCH_0_FLCN_PROD_MODE_CACHE));

        data32 |= (DRF_NUM(_CGSP, _SELWRE_SCRATCH_0, _PMU_PROD_MODE,         bPmuProdMode)   |
                   DRF_NUM(_CGSP, _SELWRE_SCRATCH_0, _SEC2_PROD_MODE,        bSec2ProdMode)  |
                   DRF_NUM(_CGSP, _SELWRE_SCRATCH_0, _LWDEC_PROD_MODE,       bLwdecProdMode) |
                   DRF_DEF(_CGSP, _SELWRE_SCRATCH_0, _FLCN_PROD_MODE_CACHE,  _VALID));

        CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CGSP_SELWRE_SCRATCH_0, data32));
    }
    else
    {
        bPmuProdMode   = FLD_TEST_DRF(_CGSP, _SELWRE_SCRATCH_0, _PMU_PROD_MODE,   _ENABLED, data32);
        bSec2ProdMode   = FLD_TEST_DRF(_CGSP, _SELWRE_SCRATCH_0, _SEC2_PROD_MODE,  _ENABLED, data32);
        bLwdecProdMode = FLD_TEST_DRF(_CGSP, _SELWRE_SCRATCH_0, _LWDEC_PROD_MODE, _ENABLED, data32);
    }

    if ((flcnStatus = _dpuCheckPmbPLM_GV10X()) != FLCN_OK)
    {
        goto ErrorExit;
    }

    // Ensure that we are in LSMODE
    _dpuIsLsOrHsModeSet_GV10X(&bLsMode, NULL);
    if (!bLsMode)
    {
        flcnStatus = FLCN_ERR_HS_CHK_NOT_IN_LSMODE;
        goto ErrorExit;
    }

    // Ensure that we are in LSMODE. Ensure that UCODE_LEVEL is 2
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CGSP_FALCON_SCTL, &data32));
    if (DRF_VAL(_CGSP, _FALCON_SCTL, _UCODE_LEVEL,  data32) != 2)
    {
        flcnStatus = FLCN_ERR_HS_CHK_ILWALID_LS_PRIV_LEVEL;
        goto ErrorExit;
    }

    // Ensure that REGIONCFG is set to correct WPR region
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CGSP_FBIF_REGIONCFG, &data32));
    if (DRF_IDX_VAL(_CGSP, _FBIF_REGIONCFG, _T, RM_DPU_DMAIDX_UCODE, data32)
        != LSF_WPR_EXPECTED_REGION_ID)
    {
        flcnStatus = FLCN_ERR_HS_CHK_ILWALID_REGIONCFG;
        goto ErrorExit;
    }

    // Ensure that priv sec is enabled on prod board
    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_PRIV_SEC_EN, &data32));
    if (bGspProdMode && FLD_TEST_DRF(_FUSE_OPT, _PRIV_SEC_EN, _DATA, _NO, data32))
    {
        REG_WR32(CSB, LW_CGSP_FALCON_MAILBOX0, FLCN_ERR_HS_CHK_PRIV_SEC_DISABLED_ON_PROD);
        DPU_HALT();
    }

    //
    // Ensure that devid override is disabled on prod boards except on
    // internal SKU. On internal sku allow to run HDCP without checking
    // opt_devid_sw_override_dis. Requirement due to bug 2254851
    //
    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_INTERNAL_SKU, &data32));
    if ((FLD_TEST_DRF(_FUSE, _OPT_INTERNAL_SKU, _DATA, _DISABLE, data32)))
    {
        CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_DEVID_SW_OVERRIDE_DIS, &data32));
        if (bGspProdMode && FLD_TEST_DRF(_FUSE, _OPT_DEVID_SW_OVERRIDE_DIS, _DATA, _NO, data32))
        {
            REG_WR32(CSB, LW_CGSP_FALCON_MAILBOX0, FLCN_ERR_HS_CHK_DEVID_OVERRIDE_ENABLED_ON_PROD);
            DPU_HALT();
        }
    }

    // HUB is not accessible from GSP/GSPLite
    // TODO: Ensure that Hub Encryption is enabled on WPR1

    // Ensure that falcons are in consistent debug/prod mode
    if (!(bGspProdMode && bSec2ProdMode && bPmuProdMode && bLwdecProdMode) &&
        !(!bGspProdMode && !bSec2ProdMode && !bPmuProdMode && !bLwdecProdMode))
    {
        REG_WR32(CSB, LW_CGSP_FALCON_MAILBOX0, FLCN_ERR_HS_CHK_INCONSISTENT_PROD_MODES);
        DPU_HALT();
    }

    flcnStatus = FLCN_OK;

ErrorExit:
    return flcnStatus;
}

/*!
 * Re-configure the RNG CTRL registers (before exiting HS) to ensure RNG can
 * still work at LS
 *
 * @return FLCN_OK.
 */
FLCN_STATUS
dpuForceStartScpRNGHs_GV10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       val;

    // First disable RNG to ensure we are not programming it while it's running
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(DFLCN_DRF(SCP_CTL1), &val));
    val = DFLCN_FLD_SET_DRF(SCP_CTL1, _RNG_EN, _DISABLED, val);
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(DFLCN_DRF(SCP_CTL1), val));

    // Set RNG parameter control registers and enable RNG

    //
    // Set delay (in number of lwclk cycles) of the production of the first
    // random number.
    //
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(DFLCN_DRF(SCP_RNDCTL0),
                                             LW_PGSP_SCP_RNDCTL0_INITIAL_VALUE));

    //
    // Reset the initial holdoff period used to enforce a free run period
    // of the RNG prior to the production of the first random number.
    //
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(DFLCN_DRF(SCP_RNDCTL1), &val));
    val = DFLCN_FLD_SET_DRF_NUM(SCP_RNDCTL1, _HOLDOFF_INTRA_MASK,
                                LW_PGSP_SCP_RNDCTL1_INITIAL_HOLDOFF_MASK, val);
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(DFLCN_DRF(SCP_RNDCTL1), val));

    // Set the ring length.
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(DFLCN_DRF(SCP_RNDCTL11), &val));

    val = DFLCN_FLD_SET_DRF_NUM(SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_A,
                                LW_PGSP_SCP_RNDCTL11_INITIAL_RING_LENGTH_VALUE,
                                val);
    val = DFLCN_FLD_SET_DRF_NUM(SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_B,
                                LW_PGSP_SCP_RNDCTL11_INITIAL_RING_LENGTH_VALUE,
                                val);
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(DFLCN_DRF(SCP_RNDCTL11), val));

    // Enable RNG.
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(DFLCN_DRF(SCP_CTL1), &val));
    val = DFLCN_FLD_SET_DRF(SCP_CTL1, _RNG_EN, _ENABLED, val);
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(DFLCN_DRF(SCP_CTL1), val));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief       This function gets a random number
 * @param[out]  pDest     Address in which random number will be written
 * @param[in]   size      size of random number in dwords
 *
 * @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
dpuGetRandNumber_GV10X
(
    LwU32  *pDest,
    LwS32   size
)
{
#if ((DPU_PROFILE_ad10x) || (DPU_PROFILE_ad10x_boot_from_hs))
    // Using a default fixed Random number till we have LibCCC RNG ready to use
    // Todo: Move to LibCCC LWRNG. Tracking Bug No: 200762634
     *pDest = 15;
#else
    if (SE_OK != seTrueRandomGetNumber(pDest, size))
    {
        return FLCN_ERR_HS_GEN_RANDOM;
    }
#endif

    return FLCN_OK;
}

/*!
 * Library interface func to read the given CSB address. 
 * The read transaction is nonposted.
 * 
 * @param[in]  addr   The CSB address to read
 * @param[out] pData  The 32-bit value of the requested address
 *
 * @return The status of the read operation.
 */
FLCN_STATUS
libInterfaceCsbRegRd32ErrChk
(
    LwU32   addr,
    LwU32  *pData
)
{
    return CSB_REG_RD32_ERRCHK(addr, pData);
}

/*!
 * Library interface func to write a 32-bit value to the given CSB address.
 *
 * @param[in]  addr  The CSB address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 */
FLCN_STATUS
libInterfaceCsbRegWr32ErrChk
(
    LwU32  addr,
    LwU32  data
)
{
    return CSB_REG_WR32_ERRCHK(addr, data);
}

/*!
 * Library interface func to read the given CSB address. 
 * The read transaction is nonposted.
 * 
 * @param[in]  addr   The CSB address to read
 * @param[out] pData  The 32-bit value of the requested address
 *
 * @return The status of the read operation.
 */
FLCN_STATUS
libInterfaceCsbRegRd32ErrChkHs
(
    LwU32   addr,
    LwU32  *pData
)
{
    return CSB_REG_RD32_HS_ERRCHK(addr, pData);
}

/*!
 * Library interface func to write a 32-bit value to the given CSB address.
 *
 * @param[in]  addr  The CSB address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 */
FLCN_STATUS
libInterfaceCsbRegWr32ErrChkHs
(
    LwU32  addr,
    LwU32  data
)
{
    return CSB_REG_WR32_HS_ERRCHK(addr, data);
}

