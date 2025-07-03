/*
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_sec2gp10x.c
 * @brief  SEC2 HAL Functions for GP10X
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "dev_sec_csb.h"
#include "sec2_bar0.h"
#include "sec2_hs.h"
#include "sec2_objsec2.h"
#include "sec2_hostif.h"
#include "sec2_csb.h"

#include "dev_master.h"
#include "dev_fuse.h"
#include "mscg_wl_bl_init.h"

/* ------------------------- Application Includes --------------------------- */
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
 * @brief Early initialization for GP10X chips.
 *
 * Any general initialization code not specific to particular engines should be
 * done here. This function must be called prior to starting the scheduler.
 */
FLCN_STATUS
sec2PreInit_GP10X(void)
{
    LwU32 val;

    // At the start of binary, set CPUCTL_ALIAS bit to FALSE so that external entity cannot restart
    // the falcon from any abrupt point.
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE);

    //
    // The below bit controls the fbif NACK behavior. When fbif receives a
    // NACK, it will consume and complete this request anyway. It will send
    // write data to FB anyway, and return garbade data (0xdead5ee0) to Falcon
    // for a read request. It will also continue to serve further DMA requests
    // after a NACKed DMA. If this bit is left at its init value, fbif will
    // continue to function in legacy mode.
    //
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FBIF_CTL2, _NACK_MODE, _NACK_AS_ACK);

    val = REG_RD32(CSB, LW_CSEC_FALCON_DEBUG1);

    //
    // Ask HW to bypass engine idle checks in the ctxsw FSM. As we're running
    // an RTOS, we will likely never be idle unless we're exelwting the IDLE
    // task. See bug 200139194 for more details.
    //
    val = FLD_SET_DRF_NUM(_CSEC, _FALCON_DEBUG1, _CTXSW_MODE1, 0x1, val);

    // Set the TRACEPC format to compressed to aid debugging
    val = FLD_SET_DRF(_CSEC, _FALCON_DEBUG1, _TRACE_FORMAT, _COMPRESSED, val);

    REG_WR32(CSB, LW_CSEC_FALCON_DEBUG1, val);

#ifdef DMEM_VA_SUPPORTED
    //
    // Initialize DMEM_VA_BOUND. This can be done as soon as the initialization
    // arguments are cached. RM copies those to the top of DMEM, and once we
    // have cached them into our local structure, all of the memory above
    // DMEM_VA_BOUND can be used as the swappable region.
    //
    REG_FLD_WR_DRF_NUM(CSB, _CSEC, _FALCON_DMVACTL, _BOUND,
               PROFILE_DMEM_VA_BOUND);

    // Enable SP to take values above 64kB
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_DMCYA, _LDSTVA_DIS, _FALSE);
#endif

    // Raise the privlevel mask for secure reset and other registers
    sec2RaisePrivLevelMasks_HAL();

    return sec2PreInit_GM20X();
}

/*!
 * @brief Initialize the register used by falc_debug.
 */
void
sec2FalcDebugInit_GP10X(void)
{
    falc_debug_init(LW_CSEC_MAILBOX(RM_SEC2_MAILBOX_ID_FALC_DEBUG));
}

/*!
 * @brief Enable the EMEM aperture to map EMEM to the top of DMEM VA space
 */
void
sec2EnableEmemAperture_GP10X(void)
{
    //
    // keep timeout at default (256 cycles) and enable DMEM Aperature
    // total timeout in cycles := (DMEMAPERT.time_out + 1) * 2^(DMEMAPERT.time_unit + 8)
    //
    LwU32 apertVal = DRF_DEF(_CSEC, _FALCON_DMEMAPERT, _ENABLE, _TRUE);
    REG_WR32(CSB, LW_CSEC_FALCON_DMEMAPERT, apertVal);
}

/*!
 * @brief Program the SW override bit in the host idle signal to idle
 *
 * SEC2 will report idle to host unless there is a method in our FIFO or a
 * ctxsw request pending as long as the bit field remains at this value.
 */
void
sec2HostIdleProgramIdle_GP10X(void)
{
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_IDLESTATE, _ENGINE_BUSY_CYA, _SW_IDLE);
}

/*!
 * @brief Program the SW override bit in the host idle signal to busy
 *
 * SEC2 will report busy to host as long as the bit field remains at this
 * value.
 */
void
sec2HostIdleProgramBusy_GP10X(void)
{
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_IDLESTATE, _ENGINE_BUSY_CYA, _SW_BUSY);
}

/*!
 * @brief Returns status of DMA NACK bit, clearing bit if set
 *
 * The DMA NACK bit is set by hardware whenever the feature is enabled and
 * a virtual DMA fault has oclwred.
 *
 * @returns  LW_TRUE  if DMA NACK was received
 *           LW_FALSE if DMA NACK was not received
 */
LwBool
sec2DmaNackCheckAndClear_GP10X(void)
{
    LwU32 val = REG_RD32(CSB, LW_CSEC_FALCON_DMAINFO_CTL);

    if (FLD_TEST_DRF(_CSEC, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION,
                     _TRUE, val))
    {
        val = FLD_SET_DRF(_CSEC, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION,
                          _CLR, val);
        REG_WR32(CSB, LW_CSEC_FALCON_DMAINFO_CTL, val);
        return LW_TRUE;
    }
    else
    {
        return LW_FALSE;
    }
}

/*!
 * @brief Enables or disables the mode that decides whether HW will send an ack
 * to host for ctxsw request
 */
void
sec2SetHostAckMode_GP10X
(
    LwBool bEnableNack
)
{
    if (bEnableNack)
    {
        REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_ITFEN, _CTXSW_NACK, _TRUE);
    }
    else
    {
        REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_ITFEN, _CTXSW_NACK, _FALSE);
    }
}

/*
 * @brief Raise the privlevel mask for secure reset and other registers
 * so that RM doesn't interfere with SEC2 operation until they are lowered
 * again.
 */
void
sec2RaisePrivLevelMasks_GP10X(void)
{
    LwBool bLSMode = LW_FALSE;
    LwBool bHSMode = LW_FALSE;

    _sec2IsLsOrHsModeSet_GP10X(&bLSMode, &bHSMode);

    if (bLSMode || bHSMode)
    {
        //
        // Only raise priv level masks when we're in booted in LS or HS mode,
        // else we'll take away our own ability to reset the masks when we
        // unload.
        //
        REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK,
                   _WRITE_PROTECTION_LEVEL0, _DISABLE);
        REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_ENGCTL_PRIV_LEVEL_MASK,
                   _WRITE_PROTECTION_LEVEL0, _DISABLE);
    }
}

/*!
 * @brief Lower the privlevel mask for secure reset and other registers
 * so that RM can write into them
 */
void
sec2LowerPrivLevelMasks_GP10X(void)
{
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK,
               _WRITE_PROTECTION_LEVEL0, _ENABLE);
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_ENGCTL_PRIV_LEVEL_MASK,
               _WRITE_PROTECTION_LEVEL0, _ENABLE);
}

/*!
 * @brief Process HOST internal methods unrelated to SEC2 applications or OS.
 *
 * @param [in] mthdId   Method ID
 * @param [in] mthdData Method Data
 *
 * @return FLCN_OK if method handled else FLCN_ERR_NOT_SUPPORTED
 */

//
//TODO-gdhanuskodi: Use ref2h to fetch host_internal ref file into hwref
//
#define LW_PMETHOD_SET_CHANNEL_INFO                       0x00003f00
FLCN_STATUS
sec2ProcessHostInternalMethods_GP10X
(
    LwU16 mthdId,
    LwU32 mthdData
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (SEC2_METHOD_ID_TO_METHOD(mthdId))
    {
        case LW_PMETHOD_SET_CHANNEL_INFO:
            //
            // Bug: 1723537 requires ucode to NOP this method
            // Consider handled.
            //
            break;
        default:
            status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief Program and enable the watchdog timer
 *
 * @param[in]  val   Watchdog timer value in cycles
 */
void
sec2WdTmrEnable_GP10X
(
    LwU32 val
)
{
    // Program the timer value
    REG_FLD_WR_DRF_NUM(CSB, _CSEC, _FALCON_WDTMRVAL, _VAL, val);

    // Then enable
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_WDTMRCTL, _WDTMREN, _ENABLE);
}

/*!
 * @brief Disable the watchdog timer
 */
void
sec2WdTmrDisable_GP10X(void)
{
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FALCON_WDTMRCTL, _WDTMREN, _DISABLE);
}

/*!
 * @brief Get the HW fuse version
 */
FLCN_STATUS
sec2GetHWFuseVersionHS_GP10X
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_BAR0_PRIV_READ_ERROR;
    LwU32 bit0             = 0;
    LwU32 bit1             = 0;
    LwU32 bit2             = 0;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_SPARE_BIT_0, &bit0));
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_SPARE_BIT_1, &bit1));
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_SPARE_BIT_2, &bit2));

    bit0 = bit0 & 1;
    bit1 = bit1 & 1;
    bit2 = bit2 & 1;

    *pFuseVersion = bit2 << 2 | bit1 << 1 | bit0;
    flcnStatus = FLCN_OK;
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Get the HW fuse version, this is LS function
 */
FLCN_STATUS
sec2GetHWFuseVersion_GP10X
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_BAR0_PRIV_READ_ERROR;
    LwU32 bit0             = 0;
    LwU32 bit1             = 0;
    LwU32 bit2             = 0;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_FUSE_SPARE_BIT_0, &bit0));
    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_FUSE_SPARE_BIT_1, &bit1));
    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_FUSE_SPARE_BIT_2, &bit2));

    bit0 = bit0 & 1;
    bit1 = bit1 & 1;
    bit2 = bit2 & 1;

    *pFuseVersion = bit2 << 2 | bit1 << 1 | bit0;
    flcnStatus = FLCN_OK;
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Get the SW fuse version
 */
#define SEC2_GP104_UCODE_VERSION    (0x4)
FLCN_STATUS
sec2GetSWFuseVersionHS_GP10X
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_BAR0_PRIV_READ_ERROR;
    LwU32 boot42           = 0;
    LwU32 chip             = 0;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &boot42));
    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, boot42);

    if (chip == LW_PMC_BOOT_42_CHIP_ID_GP102 ||
        chip == LW_PMC_BOOT_42_CHIP_ID_GP104 ||
        chip == LW_PMC_BOOT_42_CHIP_ID_GP106 ||
        chip == LW_PMC_BOOT_42_CHIP_ID_GP107 ||
        chip == LW_PMC_BOOT_42_CHIP_ID_GP108)
    {
        *pFuseVersion = SEC2_GP104_UCODE_VERSION;
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
sec2EnforceAllowedChipsForPlayreadyHS_GP10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);
    if ((chip == LW_PMC_BOOT_42_CHIP_ID_GP102) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_GP104) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_GP106) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_GP107) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_GP108))
    {
        flcnStatus = FLCN_OK;
    }
    else
    {
        flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Ensure the ucode will not be run on dev version boards
 */
FLCN_STATUS
sec2DisallowDevVersionHS_GP10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_BAR0_PRIV_READ_ERROR;
    LwU32 bit14            = 0;
    LwU32 bit15            = 0;
    LwU32 bit16            = 0;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_SPARE_BIT_14, &bit14));
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_SPARE_BIT_15, &bit15));
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_SPARE_BIT_16, &bit16));

    bit14 = bit14 & 1;
    bit15 = bit15 & 1;
    bit16 = bit16 & 1;

    LwU32 devVersion = bit16 << 2 | bit15 << 1 | bit14;

    if (devVersion == 0)
    {
        return FLCN_ERR_HS_DEV_VERSION_ON_PROD;
    }

    flcnStatus = FLCN_OK;
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
sec2EnsureConsistentFalconsProdModeHS_GP10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_BAR0_PRIV_READ_ERROR;
    LwU32 data32           = 0;
    LwBool bSec2ProdMode   = !OS_SEC_FALC_IS_DBG_MODE();
    LwBool bPmuProdMode    = LW_FALSE;
    LwBool bLwdecProdMode  = LW_FALSE;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_SELWRE_PMU_DEBUG_DIS, &data32));
    bPmuProdMode = FLD_TEST_DRF(_FUSE, _OPT_SELWRE_PMU_DEBUG_DIS,   _DATA, _YES, data32);

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_SELWRE_LWDEC_DEBUG_DIS, &data32));
    bLwdecProdMode = FLD_TEST_DRF(_FUSE, _OPT_SELWRE_LWDEC_DEBUG_DIS, _DATA, _YES, data32);

    if (!(bSec2ProdMode && bPmuProdMode && bLwdecProdMode) &&
        !(!bSec2ProdMode && !bPmuProdMode && !bLwdecProdMode))
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
 * @returns  FLCN_OK
 */
FLCN_STATUS
sec2EnsureUcodeRunningOverSec2HS_GP10X(void)
{
    // LW_CSEC_FALCON_ENGID is not implemented in GP10X, so always return OK
    return FLCN_OK;
}

/*!
 * This is the common checker function which needs to be called (or indirectly
 * called) at the beginning of every HS overlay in order to make sure SEC2 is
 * running in a secure enough environment.  The expected design for each HS
 * overlay is to call one of the following as the first thing from the HS entry
 * point.
 * 1. sec2HsPreCheckCommon_GP10X directly
 * 2. A fn named <functionality>HsPreCheck_GP10X which calls
 *    sec2HsPreCheckCommon_GP10X in addition to performing its own additional
 *    checks.
 *
 * Case#2 is expected to be required in most cases given that the common checker
 * is not enforcing chip ID check which is a very common check and requried as
 * per the following wiki.
 * https://wiki.lwpu.com/gpuhwmaxwell/index.php/MaxwellSW/Resman/Security#Guidelines_for_HS_binary
 *
 * See prHsPreCheck_GP10X for an example of case #2.
 *
 * @param[in]  bSkipDmemLv2Chk   Flag indicates whether we want to skip the lv2
 *                               PLM check for some special condition (e.g.
 *                               LASSAHS mode in PR task)
 *
 * @return FLCN_OK if SEC2 is already running at proper configuration
 */
FLCN_STATUS
sec2HsPreCheckCommon_GP10X
(
    LwBool bSkipDmemLv2Chk
)
{
    FLCN_STATUS flcnStatus         = FLCN_ERR_ILLEGAL_OPERATION;
    LwBool      bSec2ProdMode      = LW_FALSE;
    LwU32       data32             = 0;
    LwBool      bLsMode            = LW_FALSE;

    // HS Entry step of forgetting signatures
    falc_scp_forget_sig();

    clearSCPregisters();

    // Ensure that CSB bus pri error handling is enabled
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CSEC_FALCON_CSBERRSTAT, &data32));
    data32 = FLD_SET_DRF(_CSEC, _FALCON_CSBERRSTAT, _ENABLE, _TRUE, data32);
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CSEC_FALCON_CSBERRSTAT, data32));

    // Write CMEMBASE_VAL = 0, to make sure ldd/std instructions are not routed to CSB.
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CSEC_FALCON_CMEMBASE,
                      FLD_SET_DRF(_CSEC, _FALCON_CMEMBASE, _VAL, _INIT, 0)));

    // Ensure that CPUCTL_ALIAS is FALSE so that sec2 cannot be restarted
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CSEC_FALCON_CPUCTL, &data32));
    if (!FLD_TEST_DRF(_CSEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, data32))
    {
        flcnStatus = FLCN_ERR_HS_CHK_CPUCTL_ALIAS_FALSE;
        goto ErrorExit;
    }

    // Raise Reset Protection to lvl3 for HS only. We will lower this back to LS settings during HS Exit.
    CHECK_FLCN_STATUS(sec2UpdateResetPrivLevelMasksHS_HAL(&Sec2, LW_TRUE));

    //
    // Ensure that binary is not revoked. This must be the first step in
    // sec2HsPreCheckCommon_HAL. The only check or register access that is
    // permitted before this step is read of BOOT_42. CSB reads are also
    // discouraged and must be carefully reviewed before addition.
    //
    CHECK_FLCN_STATUS(sec2CheckFuseRevocationHS());

    // Ensure this ucode is running on SEC2 engine (This check only works for GV100+)
    CHECK_FLCN_STATUS(sec2EnsureUcodeRunningOverSec2HS_HAL(&Sec2));
    CHECK_FLCN_STATUS(_sec2CheckPmbPLM_GP10X(bSkipDmemLv2Chk));
    // Ensure that we are in LSMODE
    _sec2IsLsOrHsModeSet_GP10X(&bLsMode, NULL);
    if (!bLsMode)
    {
        flcnStatus = FLCN_ERR_HS_CHK_NOT_IN_LSMODE;
        goto ErrorExit;
    }

    // Ensure that UCODE_LEVEL is 2
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CSEC_FALCON_SCTL, &data32));
    if (DRF_VAL(_CSEC, _FALCON_SCTL, _UCODE_LEVEL,  data32) != 2)
    {
        flcnStatus = FLCN_ERR_HS_CHK_ILWALID_LS_PRIV_LEVEL;
        goto ErrorExit;
    }

    // Ensure that REGIONCFG is set to correct WPR region
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CSEC_FBIF_REGIONCFG, &data32));
    if (DRF_IDX_VAL(_CSEC, _FBIF_REGIONCFG, _T, RM_SEC2_DMAIDX_UCODE, data32)
        != LSF_WPR_EXPECTED_REGION_ID)
    {
        flcnStatus = FLCN_ERR_HS_CHK_ILWALID_REGIONCFG;
        goto ErrorExit;;
    }

    bSec2ProdMode = !OS_SEC_FALC_IS_DBG_MODE();

    // Ensure that priv sec is enabled on prod board
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_PRIV_SEC_EN, &data32));
    if (bSec2ProdMode && FLD_TEST_DRF(_FUSE_OPT, _PRIV_SEC_EN, _DATA, _NO, data32))
    {
        flcnStatus = FLCN_ERR_HS_CHK_PRIV_SEC_DISABLED_ON_PROD;
        goto ErrorExit;
    }

    //
    // From hopper onwards, connection of SEC2 to FBHUB is getting removed and FSP will be 
    // connected to FBHUB. It means SEC2 could not access FBHUB interface of Hub encryption
    // POR of sec2 is to move to RISCV in Hopper so this code path is also no longer valid for SOL
    // Adding a WAR to disable hub encryption check
    //
#ifndef WAR_DISABLE_HUB_ENC_CHECK_BUG_3126208

    LwBool      bEncryptionEnabled = LW_FALSE;

    // Ensure that Hub Encryption is enabled on WPR1
    if ((flcnStatus = _sec2HubIsAcrEncryptionEnabled_GP10X(LSF_WPR_EXPECTED_REGION_ID, &bEncryptionEnabled)) != FLCN_OK)
    {
        goto ErrorExit;
    }

    if (!bEncryptionEnabled)
    {
        flcnStatus = FLCN_ERR_HS_CHK_HUB_ENCRPTION_DISABLED;
        goto ErrorExit;
    }
#endif // WAR_DISABLE_HUB_ENC_CHECK_BUG_3126208

    // Ensure that falcons are in consistent debug/prod mode
    if ((flcnStatus = sec2EnsureConsistentFalconsProdModeHS_HAL(&Sec2)) != FLCN_OK)
    {
        goto ErrorExit;
    }

    //
    // The mutex mapping can be accessed by HS code but now it's stored on
    // global variable which is not protected against non-HS code. So, as a WAR,
    // here we are initializing it at every HS entry to ensure HS code is using
    // valid mapping data until we have a way to protect the data via dmlvl.
    //
    sec2MutexEstablishMappingHS_HAL(&Sec2);

    flcnStatus = FLCN_OK;

ErrorExit:
    return flcnStatus;
}

/*!
 * Re-configure the RNG CTRL registers (before exiting HS) to ensure RNG can
 * still work at LS
 */
void
sec2ForceStartScpRNGHs_GP10X(void)
{
    LwU32 val;

    // First disable RNG to ensure we are not programming it while it's running
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _SCP_CTL1, _RNG_EN, _DISABLED);

    // set RNG parameter control registers and enable RNG
    REG_FLD_WR_DRF_NUM_STALL(CSB, _CSEC, _SCP_RNDCTL0, _HOLDOFF_INIT_LOWER, 0x7fff);

    REG_FLD_WR_DRF_NUM_STALL(CSB, _CSEC, _SCP_RNDCTL1, _HOLDOFF_INTRA_MASK, 0x03ff);

    val = REG_RD32_STALL(CSB, LW_CSEC_SCP_RNDCTL11);

    val = FLD_SET_DRF_NUM(_CSEC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_A, 0x000f, val);
    val = FLD_SET_DRF_NUM(_CSEC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_B, 0x000f, val);

    REG_WR32_STALL(CSB, LW_CSEC_SCP_RNDCTL11, val);

    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSEC, _SCP_CTL1, _RNG_EN, _ENABLED);
}

/*!
 * @brief: Program Bar0 timeout value
 */
void
sec2ProgramBar0Timeout_GP10X(void)
{
    REG_WR32(CSB, LW_CSEC_BAR0_TMOUT,
             DRF_NUM(_CSEC, _BAR0_TMOUT, _CYCLES, 0x1000));
}

#if (SEC2CFG_FEATURE_ENABLED(SEC2JOB_MSCG_TEST))
#if (SEC2CFG_FEATURE_ENABLED(SEC2JOB_MSCG_FBBLOCKER_TEST))
/*!
 * @brief: Issue FB access when SEC2 is in MSCG
 *
 * @param[in] offset:    FB offset (low 32 bits)
 * @param[in] offsetH:   FB offset (upper 32 bits)
 * @param[in] fbOp:      ISSUE_READ: issue read
 *                       ISSUE_WRITE: Issue write
 * @return:
 *     ISSUE_READ:  FLCN_OK if FB region specified contains test pattern
 *                  FLCN_ERR_DMA_GENERIC if pattern inaccessible or wrong.
 *     ISSUE_WRITE: FLCN_OK if write of test pattern to FB region succeeds
 *                  FLCN_ERR_DMA_GENERIC if dma write to FB region fails.
 */
FLCN_STATUS
sec2MscgIssueFbAccess_GP10X
(
    LwU32 offset,
    LwU32 offsetH,
    LwU8  fbOp
)
{
    LwU8             buf[MSCG_ISSUE_FB_SIZE+DMA_MIN_READ_ALIGNMENT];
    LwU8             *pBuf = (LwU8 *)LW_ALIGN_UP((LwUPtr)buf, DMA_MIN_READ_ALIGNMENT);
    LwU32            index;
    RM_FLCN_MEM_DESC memDesc;

    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     RM_SEC2_DMAIDX_PHYS_VID_FN0 , 0);
    memDesc.address.lo = offset;
    memDesc.address.hi = offsetH;

    // To reduce stack pressure, we wait for MSCG to engage before calling this function.

    if (fbOp == MSCG_ISSUE_FB_ACCESS_WRITE)
    {
        for (index = 0; index < MSCG_ISSUE_FB_SIZE; index++)
        {
            pBuf[index] = (LwU8) index;
        }
        if (dmaWrite(pBuf, &memDesc, 0, MSCG_ISSUE_FB_SIZE) != FLCN_OK)
        {
             return FLCN_ERR_DMA_GENERIC;
        }
    }
    else
    {
        if (dmaRead(pBuf, &memDesc, 0, MSCG_ISSUE_FB_SIZE) != FLCN_OK)
        {
            return FLCN_ERR_DMA_GENERIC;
        }
        else
        {
            for (index = 0; index < MSCG_ISSUE_FB_SIZE; index++)
            {
                if (pBuf[index] != (LwU8) index)
                {
                    return FLCN_ERR_DMA_GENERIC;
                }
            }
        }
    }

    return FLCN_OK;
}
#endif // (SEC2CFG_FEATURE_ENABLED(SEC2JOB_MSCG_FBBLOCKER_TEST))

/*
 * @brief Waits for MSCG to engage.
 *
 * Checks if MSCG is engaged or not. If it's not engaged then it keeps looping
 * till MSCG gets engaged.
 *
 * We delay 30us * MSCG_POLL_DELAY_TICKS; as defined above, this is 1.92ms
 *
 * @returns    FLCN_OK if MSCG is entered, FLCN_ERROR if MSCG entry timed out,
 *             and whatever *_REG_RD32_ERRCHK returns if we can't read register,
 *             usually FLCN_ERR_BAR0_PRIV_READ_ERROR/FLCN_ERR_CSB_PRIV_READ_ERROR
 * Stubbed out for GP10x until Lpwr team implements this function.
 */
FLCN_STATUS
sec2WaitMscgEngaged_GP10X(void)
{
    //Pascal MSCG is very different from Turing MSCG. Need input from Lpwr on how to
    //test whether we are in MSCG for Pascal.
    return FLCN_OK;
}
#endif // (SEC2CFG_FEATURE_ENABLED(SEC2JOB_MSCG_TEST))


/*!
 * Issue a membar.sys
 */
FLCN_STATUS
sec2FbifFlush_GP10X(void)
{
    REG_FLD_WR_DRF_DEF(CSB, _CSEC, _FBIF_CTL, _FLUSH, _SET);

    //
    // Poll for the flush to finish. HW says 1 ms is a reasonable value to use
    // It is the same as our PCIe timeout in LW_PBUS_FB_TIMEOUT.
    //
    if (!SEC2_REG32_POLL_US(LW_CSEC_FBIF_CTL,
                            DRF_SHIFTMASK(LW_CSEC_FBIF_CTL_FLUSH),
                            LW_CSEC_FBIF_CTL_FLUSH_CLEAR,
                            1000, // 1 ms
                            SEC2_REG_POLL_MODE_CSB_EQ))
    {
        //
        // Flush didn't finish. There is really no good action we can take here
        // We'll return an error and let the caller decide what to do.
        //
        return FLCN_ERR_TIMEOUT;
    }
    return FLCN_OK;
}
