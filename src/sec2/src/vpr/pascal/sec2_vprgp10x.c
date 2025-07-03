/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwmisc.h"
#include "flcntypes.h"
#include "sec2sw.h"
#include "main.h"
#include "lw_mutex.h"
#include "lib_mutex.h"
#include "lwosselwreovly.h"
#include "vpr/sec2_vpr.h"
#include "sec2_objvpr.h"
#include "config/g_vpr_private.h"
#include "dev_fuse.h"
#include "sec2_objsec2.h"
#include "dev_fb.h"
#include "dev_ltc.h"
#include "dev_pri_ringmaster.h"
#include "dev_master.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_pwr_pri.h"
#include "dev_sec_addendum.h"
#include "sec2_bar0_hs.h"
#include "sec2_csb.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

//
// This struct will help keep the VPR call stateful in order to maintain the 
// Raise/Lower PLM operations.
//
VPR_PRIV_MASKS_RESTORE privMask     GCC_ATTRIB_SECTION("dmem_vpr", "_privMask")  = {0};

/*!
 * @brief Scrubs specified memory region
 */
FLCN_STATUS
vprScrubRegion_GP10X
(
    LwU64 startAddrInBytes,
    LwU64 endAddrInBytes
)
{
    LwU32       startAddrReadBack;
    LwU32       endAddrReadBack;
    LwU32       scrubStatus;
    LwU32       startAddr4KAligned;
    LwU32       endAddr4KAligned;
    LwU32       vprScrubToken      = LW_PFB_NISO_SCRUB_PATTERN_FIELD_ALL_ZEROS;
    FLCN_STATUS flcnStatus         = FLCN_OK;

    // HW Scrubber accepts input addresses in 4K
    startAddr4KAligned = startAddrInBytes >> SHIFT_4KB;
    endAddr4KAligned   = endAddrInBytes   >> SHIFT_4KB;

    //
    // If start/endAddrInBytes needs to be used for any reason beyond this point, then shift start/endAddr4KAligned
    // and not use the input params directly. The reason is that caller may set one of the first 12 bits that
    // may lead to some problems. For all practical purposes we just want to treat those bits as 0s
    //
    if (startAddr4KAligned > endAddr4KAligned)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    //
    // TODO: From Turing onwards use a mutex before using HW scrubber, Bug 200342775
    // Until Volta, we had limited usecases of HW scrubber, but later it is better to use mutex
    //

    // Wait for any ongoing scrub to complete.
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_NISO_SCRUB_STATUS, &scrubStatus));
    while (!FLD_TEST_DRF(_PFB, _NISO_SCRUB_STATUS, _FLAG, _DONE, scrubStatus))
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_NISO_SCRUB_STATUS, &scrubStatus));
    }

    // Program the start addr, end addr and the pattern into the scrubber
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_NISO_SCRUB_START_ADDR, startAddr4KAligned));
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_NISO_SCRUB_END_ADDR, endAddr4KAligned));
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_NISO_SCRUB_PATTERN, vprScrubToken));

    // Trigger the scrub
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_NISO_SCRUB_TRIGGER, LW_PFB_NISO_SCRUB_TRIGGER_FLAG_ENABLED));

    // Wait for the scrubber to complete
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_NISO_SCRUB_STATUS, &scrubStatus));
    while (!FLD_TEST_DRF(_PFB, _NISO_SCRUB_STATUS, _FLAG, _DONE, scrubStatus))
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_NISO_SCRUB_STATUS, &scrubStatus));
    }

    // Match the addresses and return success only if they match with what we wanted.
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_NISO_SCRUB_START_ADDR_STATUS, &startAddrReadBack));
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_NISO_SCRUB_LAST_ADDR_STATUS, &endAddrReadBack));
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_NISO_SCRUB_STATUS, &scrubStatus));
    if ((startAddr4KAligned == startAddrReadBack) &&
        (endAddr4KAligned   == endAddrReadBack)   &&
        (FLD_TEST_DRF(_PFB, _NISO_SCRUB_STATUS, _FLAG, _DONE, scrubStatus)))
    {
        flcnStatus = FLCN_OK;
    }
    else
    {
        flcnStatus = FLCN_ERR_VPR_APP_SCRUB_VERIF_FAILED;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Get opt_vpr_enabled fuse value
 */
FLCN_STATUS
vprIsAllowedInHWFuse_GP10X(void)
{
    LwU32       vprEnable;
    FLCN_STATUS flcnStatus = FLCN_OK;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_VPR_ENABLED, &vprEnable));

    if (FLD_TEST_DRF(_FUSE, _OPT_VPR_ENABLED, _DATA, _YES, vprEnable))
    {
        flcnStatus = FLCN_OK;
    }
    else
    {
        flcnStatus = FLCN_ERR_VPR_APP_NOT_SUPPORTED_BY_HW;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 *  Get SPARE_BIT_13 value
 */
FLCN_STATUS
vprIsAllowedInSWFuse_GP10X(void)
{
    LwU32       chip;
    LwU32       spareBit;
    FLCN_STATUS flcnStatus = FLCN_OK;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

    if ((chip == LW_PMC_BOOT_42_CHIP_ID_GP107) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_GP108))
    {
        flcnStatus = vprIsAllowedInSWFuse_GP107();
        // Stop here and return flcnStatus
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_SPARE_BIT_13, &spareBit));
    if (FLD_TEST_DRF(_FUSE, _SPARE_BIT_13, _DATA, _ENABLE, spareBit))
    {
        flcnStatus = FLCN_OK;
    }
    else
    {
        flcnStatus = FLCN_ERR_VPR_APP_NOT_SUPPORTED_BY_SW;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * Check if display is present/enabled via fuse
 */
FLCN_STATUS
vprIsDisplayPresent_GP10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       fuseValue;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_STATUS_OPT_DISPLAY, &fuseValue));

    if (!FLD_TEST_DRF( _FUSE, _STATUS_OPT_DISPLAY, _DATA, _ENABLE, fuseValue))
    {
        flcnStatus = FLCN_ERR_VPR_APP_DISPLAY_NOT_PRESENT;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Get VPR max address from secure scratch register
 */
FLCN_STATUS
vprGetMaxEndAddress_GP10X(LwU64 *pVprMaxEndAddr)
{
    LwU64 vprMinStartAddr  = 0;
    LwU64 vprMaxSize       = 0;
    LwU64 vprMaxEndAddr    = 0;
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Check the out parameter for NULL
    if (pVprMaxEndAddr == NULL)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(vprGetMinStartAddress_HAL(&VprHal, &vprMinStartAddr));
    CHECK_FLCN_STATUS(vprGetMaxSize_HAL(&VprHal, &vprMaxSize));
    vprMaxEndAddr = vprMinStartAddr + (vprMaxSize - 1);

    // Update return val
    *pVprMaxEndAddr = vprMaxEndAddr;

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Get VPR min address from secure scratch register
 */
FLCN_STATUS
vprGetMinStartAddress_GP10X(LwU64 *pVprMinAddr)
{
    LwU64       vprMinAddr = 0;
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Check the out parameter for NULL
    if (pVprMinAddr == NULL)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15, (LwU32*)&vprMinAddr));
    vprMinAddr = DRF_VAL( _PGC6, _BSI_VPR_SELWRE_SCRATCH_15, _VPR_MAX_RANGE_START_ADDR_MB_ALIGNED, vprMinAddr);
    vprMinAddr = vprMinAddr << VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT;

    // Update return val
    *pVprMinAddr = vprMinAddr;

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Get VPR min address from secure scratch register
 */
FLCN_STATUS
vprGetMaxSize_GP10X(LwU64 *pVprMaxSize)
{
    LwU64       vprMaxSize = 0;
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Check the out parameter for NULL
    if (pVprMaxSize == NULL)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_13, (LwU32*)&vprMaxSize));
    vprMaxSize = DRF_VAL(_PGC6, _BSI_VPR_SELWRE_SCRATCH_13, _MAX_VPR_SIZE_MB, vprMaxSize);
    vprMaxSize = vprMaxSize << VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT;

    // Update return val
    *pVprMaxSize = vprMaxSize;

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Get VPR min address from secure scratch register
 */
FLCN_STATUS
vprGetLwrrentSize_GP10X(LwU64 *pVprLwrrentSizeInBytes)
{
    LwU64       vprLwrrentSizeInBytes   = 0;
    LwU64       vprLwrrStartAddrInBytes = 0;
    LwU64       vprLwrrEndAddrInBytes   = 0;
    FLCN_STATUS flcnStatus              = FLCN_OK;

    // Check the out parameter for NULL
    if (pVprLwrrentSizeInBytes == NULL)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    // Get the Start and End address of VPR
    CHECK_FLCN_STATUS(vprGetLwrVprRangeInBytes_HAL(&VprHal, &vprLwrrStartAddrInBytes, &vprLwrrEndAddrInBytes));

    if (vprLwrrEndAddrInBytes >= vprLwrrStartAddrInBytes)
    {
        vprLwrrentSizeInBytes = vprLwrrEndAddrInBytes - vprLwrrStartAddrInBytes + 1;
    }
    else
    {
        vprLwrrentSizeInBytes = 0;
    }

    // Update return val
    *pVprLwrrentSizeInBytes = vprLwrrentSizeInBytes;

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Raise/Lower few priv level masks before/after setting vpr region
 *
 * This is required because setting up of VPR in MMU and BSI secure scratch
 * registers is not atomic. Therefore we need to disable sec2 reset and gc6 trigger
 * by raising corresponding priv level masks so that no one else can interfere
 */
FLCN_STATUS
vprRaisePrivLevelMasks_GP10X
(
    VPR_PRIV_MASKS_RESTORE *pPrivMask,
    LwBool bLock
)
{
    LwU32       regSec2FlcnResetMask = 0;
#if (!SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
    LwU32       regGc6BsiCtrlMask    = 0;
    LwU32       regGc6SciMastMask    = 0;
#endif
    FLCN_STATUS flcnStatus           = FLCN_ERROR;

    if (pPrivMask == NULL)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    if (bLock)
    {
        //
        // Using only one boolean seems sufficient here as we set it to true at the end after raising all 3 masks
        // and we check this boolean before lowering these masks. Granularity will help if there is an issue while
        // modifying any of these masks i.e. functional failure, but we wont have any security issue considering our checks in place
        //
        if (pPrivMask->areMasksRaised == LW_TRUE)
        {
            return FLCN_ERR_VPR_APP_PLM_PROTECTION_ALREADY_RAISED;
        }

        // Raise sec2 falcon reset mask
        CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK, &regSec2FlcnResetMask));
        pPrivMask->sec2FlcnResetMask = (LwU8)(DRF_VAL( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, regSec2FlcnResetMask));
        regSec2FlcnResetMask         = FLD_SET_DRF( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, regSec2FlcnResetMask);
        regSec2FlcnResetMask         = FLD_SET_DRF( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, regSec2FlcnResetMask);
        regSec2FlcnResetMask         = FLD_SET_DRF( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, regSec2FlcnResetMask);
        CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK, regSec2FlcnResetMask));

#if (!SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
        // Raise gc6 bsi ctrl mask
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_CTRL_PRIV_LEVEL_MASK, &regGc6BsiCtrlMask));
        pPrivMask->gc6BsiCtrlMask = (LwU8)(DRF_VAL(_PGC6, _BSI_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION, regGc6BsiCtrlMask));
        regGc6BsiCtrlMask = FLD_SET_DRF(_PGC6, _BSI_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, regGc6BsiCtrlMask);
        regGc6BsiCtrlMask = FLD_SET_DRF(_PGC6, _BSI_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, regGc6BsiCtrlMask);
        regGc6BsiCtrlMask = FLD_SET_DRF(_PGC6, _BSI_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, regGc6BsiCtrlMask);
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PGC6_BSI_CTRL_PRIV_LEVEL_MASK, regGc6BsiCtrlMask));

        // Raise gc6 sci mast mask
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_SCI_MAST_FSM_PRIV_LEVEL_MASK, &regGc6SciMastMask));
        pPrivMask->gc6SciMastMask = (LwU8)(DRF_VAL(_PGC6, _SCI_MAST_FSM_PRIV_LEVEL_MASK, _WRITE_PROTECTION, regGc6SciMastMask));
        regGc6SciMastMask = FLD_SET_DRF(_PGC6, _SCI_MAST_FSM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, regGc6SciMastMask);
        regGc6SciMastMask = FLD_SET_DRF(_PGC6, _SCI_MAST_FSM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, regGc6SciMastMask);
        regGc6SciMastMask = FLD_SET_DRF(_PGC6, _SCI_MAST_FSM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, regGc6SciMastMask);
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PGC6_SCI_MAST_FSM_PRIV_LEVEL_MASK, regGc6SciMastMask));
#endif

        pPrivMask->areMasksRaised = LW_TRUE;
    }
    else
    {
        if (pPrivMask->areMasksRaised == LW_FALSE)
        {
            return FLCN_ERR_VPR_APP_PLM_PROTECTION_NOT_RAISED;
        }

        // Restore sec2 falcon reset mask
        CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK, &regSec2FlcnResetMask));
        regSec2FlcnResetMask = FLD_SET_DRF_NUM( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, pPrivMask->sec2FlcnResetMask, regSec2FlcnResetMask);
        CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK, regSec2FlcnResetMask));

#if (!SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
        // Restore gc6 bsi ctrl mask
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_CTRL_PRIV_LEVEL_MASK, &regGc6BsiCtrlMask));
        regGc6BsiCtrlMask = FLD_SET_DRF_NUM(_PGC6, _BSI_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION, pPrivMask->gc6BsiCtrlMask, regGc6BsiCtrlMask);
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PGC6_BSI_CTRL_PRIV_LEVEL_MASK, regGc6BsiCtrlMask));

        // Restore gc6 sci mast mask
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_SCI_MAST_FSM_PRIV_LEVEL_MASK, &regGc6SciMastMask));
        regGc6SciMastMask = FLD_SET_DRF_NUM(_PGC6, _SCI_MAST_FSM_PRIV_LEVEL_MASK, _WRITE_PROTECTION, pPrivMask->gc6SciMastMask, regGc6SciMastMask);
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PGC6_SCI_MAST_FSM_PRIV_LEVEL_MASK, regGc6SciMastMask));
#endif
        pPrivMask->areMasksRaised = LW_FALSE;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Program VPR region as per the request
 */
FLCN_STATUS
vprProgramRegion_GP10X
(
    LwU64 startAddrIn4K,
    LwU64 sizeInMB
)
{
    FLCN_STATUS            flcnStatus              = FLCN_OK;
    LwU64                  vprEndAddrInBytes       = 0;
    LwU64                  vprLwrrStartAddrInBytes = 0;
    LwU64                  vprLwrrEndAddrInBytes   = 0;
    LwU64                  vprLwrrentSize          = 0;
    LwU64                  vprOffsetInBytes        = 0;
#ifdef SELWRITY_ENGINE
    FLCN_STATUS            tmpStatus               = FLCN_OK;
    LwU8                   mutexToken              = LW_MUTEX_OWNER_ID_ILWALID;
#endif

    CHECK_FLCN_STATUS(vprSetVprOffsetInBytes_HAL(&VprHal, &vprOffsetInBytes, startAddrIn4K));
    LwU64 vprSizeInBytes   = sizeInMB << SHIFT_1MB;

#ifdef SELWRITY_ENGINE
    // Acquire mutex for accessing MMU register VPR_WPR_WRITE for VPR programming
    CHECK_FLCN_STATUS(mutexAcquire_hs(LW_MUTEX_ID_MMU_VPR_WPR_WRITE, (LwU32)VPR_MUTEX_ACQUIRE_TIMEOUT_NS, &mutexToken));
#endif

    // Check if request is valid
    CHECK_FLCN_STATUS(vprValidateRequest_HAL(&VprHal, vprOffsetInBytes, vprSizeInBytes));

    vprEndAddrInBytes = vprOffsetInBytes + vprSizeInBytes - 1;
    // Get the Start and End address of VPR
    CHECK_FLCN_STATUS(vprGetLwrVprRangeInBytes_HAL(&VprHal, &vprLwrrStartAddrInBytes, &vprLwrrEndAddrInBytes));

    // Allocate VPR
    if (vprSizeInBytes > 0)
    {
        // Resize VPR, not the first request
        CHECK_FLCN_STATUS(vprGetLwrrentSize_HAL(&VprHal, &vprLwrrentSize));
        if (vprLwrrentSize != 0)
        {
            // Disjoint request
            if ((vprOffsetInBytes > vprLwrrEndAddrInBytes) ||
                (vprEndAddrInBytes < vprLwrrStartAddrInBytes))
            {
                // Scrub current disjoint VPR
                CHECK_FLCN_STATUS(vprLockAndScrub_HAL(&VprHal, vprLwrrStartAddrInBytes, vprLwrrEndAddrInBytes));

                // Setup new disjoint request
                CHECK_FLCN_STATUS(vprEnableVprInMmu_HAL(&VprHal, LW_FALSE));
                CHECK_FLCN_STATUS(vprSetupVprRangeInMmu_HAL(&VprHal, vprOffsetInBytes, vprEndAddrInBytes));
                CHECK_FLCN_STATUS(vprEnableVprInMmu_HAL(&VprHal, LW_TRUE));
                CHECK_FLCN_STATUS(vprSelwreScratchUpdateRange_HAL(&VprHal, vprOffsetInBytes, vprEndAddrInBytes));
                CHECK_FLCN_STATUS(vprReleaseMemlock_HAL(&VprHal));
            }
            else    // VPR joint or clash or overlap
            {
                // Check start address
                if (vprOffsetInBytes > vprLwrrStartAddrInBytes)
                {
                    // Scrub left side
                    CHECK_FLCN_STATUS(vprLockAndScrub_HAL(&VprHal, vprLwrrStartAddrInBytes, vprOffsetInBytes - 1));
                }
                else
                {
                    //nothing to scrub
                }

                // setup new start addr
                CHECK_FLCN_STATUS(vprSetupVprRangeInMmu_HAL(&VprHal, vprOffsetInBytes, vprLwrrEndAddrInBytes));
                CHECK_FLCN_STATUS(vprSelwreScratchUpdateRange_HAL(&VprHal, vprOffsetInBytes, vprLwrrEndAddrInBytes));
                CHECK_FLCN_STATUS(vprReleaseMemlock_HAL(&VprHal));

                // Check End address
                if (vprEndAddrInBytes < vprLwrrEndAddrInBytes)
                {
                    // Scrub right side
                    CHECK_FLCN_STATUS(vprLockAndScrub_HAL(&VprHal, vprEndAddrInBytes + 1, vprLwrrEndAddrInBytes));
                }
                else
                {
                    //nothing to scrub
                }

                // setup new end addr
                CHECK_FLCN_STATUS(vprSetupVprRangeInMmu_HAL(&VprHal, vprOffsetInBytes, vprEndAddrInBytes));
                CHECK_FLCN_STATUS(vprSelwreScratchUpdateRange_HAL(&VprHal, vprOffsetInBytes, vprEndAddrInBytes));
                CHECK_FLCN_STATUS(vprReleaseMemlock_HAL(&VprHal));
            }
        }
        else    // first VPR
        {

            //
            // Raise required PLMs to make vpr programming atomic. These PLMS need to be raised only
            // when current VPR size is 0 and we have a VPR grow request.
            //
            CHECK_FLCN_STATUS(vprRaisePrivLevelMasks_HAL(&VprHal, &privMask, LW_TRUE));
            
            CHECK_FLCN_STATUS(vprUpdateMiscSettingsIfEnabled_HAL(&VprHal, LW_TRUE));
            CHECK_FLCN_STATUS(vprSetupVprRangeInMmu_HAL(&VprHal, vprOffsetInBytes, vprEndAddrInBytes));
            CHECK_FLCN_STATUS(vprSetupClientTrustLevel_HAL(&VprHal));
            CHECK_FLCN_STATUS(vprEnableVprInMmu_HAL(&VprHal, LW_TRUE));
            CHECK_FLCN_STATUS(vprSelwreScratchUpdateRange_HAL(&VprHal, vprOffsetInBytes, vprEndAddrInBytes));
        }
    }
    else    // destroy VPR
    {
        CHECK_FLCN_STATUS(vprGetLwrrentSize_HAL(&VprHal, &vprLwrrentSize));
        if (vprLwrrentSize != 0) // Scrub if present
        {
            // Scrub current vpr
            CHECK_FLCN_STATUS(vprLockAndScrub_HAL(&VprHal, vprLwrrStartAddrInBytes, vprLwrrEndAddrInBytes));

            // Unset vpr by writing invalid address
            CHECK_FLCN_STATUS(vprEnableVprInMmu_HAL(&VprHal, LW_FALSE));
            CHECK_FLCN_STATUS(vprSetupVprRangeInMmu_HAL(&VprHal, ILWALID_VPR_ADDR_LO_IN_BYTES, ILWALID_VPR_ADDR_HI_IN_BYTES));
            CHECK_FLCN_STATUS(vprSelwreScratchUpdateRange_HAL(&VprHal, ILWALID_VPR_ADDR_LO_IN_BYTES, ILWALID_VPR_ADDR_HI_IN_BYTES));
            CHECK_FLCN_STATUS(vprReleaseMemlock_HAL(&VprHal));

#if (!SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
            //
            // When VPR size becomes zero i.e. completely released, HDCP22 type1 lock needs to be released/unlocked
            //
            CHECK_FLCN_STATUS(vprReleaseHdcpType1Lock_HAL(&VprHal));
#endif // SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM)
        }
        else
        {
            // nothing to scrub
        }
        CHECK_FLCN_STATUS(vprUpdateMiscSettingsIfEnabled_HAL(&VprHal, LW_FALSE));
        
        // Downgrade earlier raised PLMs
        CHECK_FLCN_STATUS(vprRaisePrivLevelMasks_HAL(&VprHal, &privMask, LW_FALSE));
    }

ErrorExit:
    //  Don't overwrite an earlier failure, after ErrorExit
#ifdef SELWRITY_ENGINE
    // Release mutex after accessing MMU register VPR_WPR_WRITE for VPR programming
    if (mutexToken != LW_MUTEX_OWNER_ID_ILWALID)
    {
       tmpStatus = mutexRelease_hs(LW_MUTEX_ID_MMU_VPR_WPR_WRITE, mutexToken);
       if (flcnStatus == FLCN_OK)
        {
            flcnStatus = tmpStatus;
        }
    }
#endif

    return flcnStatus;
}

/*!
 * @brief Release HDCP22 type1 lock when VPR is freed
 */
FLCN_STATUS
vprReleaseHdcpType1Lock_GP10X(void)
{
    OS_SEC_HS_LIB_VALIDATE(libVprPolicyHs, HASH_SAVING_DISABLE);

    return sec2HdcpType1LockHs_HAL(&Sec2, LW_FALSE);
}

/*!
 * @brief Validate VPR request
 */
FLCN_STATUS
vprValidateRequest_GP10X
(
    LwU64 vprOffsetInBytes,
    LwU64 vprSizeInBytes
)
{
    FLCN_STATUS flcnStatus             = FLCN_OK;
    LwU64       vprLwrrentSizeInBytes  = 0;
    LwU64       vprMinStartAddrInBytes = 0;
    LwU64       vprMaxEndAddrInBytes   = 0;
    LwU64       vprEndAddrInBytes      = 0;

    // Check if start address is MB aligned as we expect VPR requests from KMD in MBs
    if (!VAL_IS_ALIGNED(vprOffsetInBytes, NUM_BYTES_IN_1_MB))
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    // Check if start address is less than minimum allowed start address
    CHECK_FLCN_STATUS(vprGetMinStartAddress_HAL(&VprHal, &vprMinStartAddrInBytes));
    if (vprOffsetInBytes < vprMinStartAddrInBytes)
    {
        flcnStatus = FLCN_ERR_VPR_APP_ILWALID_REQUEST_START_ADDR;
        goto ErrorExit;
    }

    // VPR grow call
    if (vprSizeInBytes > 0)
    {
        // Check if size is MB aligned as we expect VPR requests from KMD in MBs
        if (!VAL_IS_ALIGNED(vprSizeInBytes, NUM_BYTES_IN_1_MB))
        {
            flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
            goto ErrorExit;
        }

        // If this calc with -1 needs to be pulled out, take care of underflow
        vprEndAddrInBytes = vprOffsetInBytes + vprSizeInBytes - 1;

        // Check if end address is within limits
        CHECK_FLCN_STATUS(vprGetMaxEndAddress_HAL(&VprHal, &vprMaxEndAddrInBytes));
        if (vprEndAddrInBytes > vprMaxEndAddrInBytes)
        {
            flcnStatus = FLCN_ERR_VPR_APP_ILWALID_REQUEST_END_ADDR;
            goto ErrorExit;
        }

        // Check CBC and VPR address clash
        CHECK_FLCN_STATUS(vprCheckIfClashWithCbcRange_HAL(&VprHal));

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
        // Check clash with FB holes on GA100
        CHECK_FLCN_STATUS(vprCheckVprRangeWithRowRemapperReserveFb_HAL(&VprHal, vprOffsetInBytes, vprEndAddrInBytes));
#endif
    }

    CHECK_FLCN_STATUS(vprGetLwrrentSize_HAL(&VprHal, &vprLwrrentSizeInBytes));
    if ((vprSizeInBytes == 0) && (vprLwrrentSizeInBytes == 0))
    {
        flcnStatus = FLCN_ERR_VPR_APP_NOTHING_TO_DO;
        goto ErrorExit;
    }

ErrorExit:
    return flcnStatus;
}

/*
 * Update the vpr range in secure scratch register
 */
FLCN_STATUS
vprSelwreScratchUpdateRange_GP10X
(
    LwU64 vprOffsetInBytes,
    LwU64 vprEndAddrInBytes
)
{
    FLCN_STATUS flcnStatus     = FLCN_OK;
    LwU64       vprSizeInBytes = 0;

    if (vprOffsetInBytes <= vprEndAddrInBytes)
    {
        vprSizeInBytes = vprEndAddrInBytes - vprOffsetInBytes + 1;
    }
    else
    {
        // init value of vprSizeInBytes is already 0
    }

    LwU32       vprOffsetMBAligned = vprOffsetInBytes >> VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT;
    LwU32       vprSizeMBAligned   = vprSizeInBytes >> VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT;
    LwU32       reg                = 0;
#ifdef SELWRITY_ENGINE
    LwU8        mutexToken         = LW_MUTEX_OWNER_ID_ILWALID;
    FLCN_STATUS tmpStatus      = FLCN_OK;

    // Acquire mutex for accesses to secure scratch registers
    CHECK_FLCN_STATUS(mutexAcquire_hs(LW_MUTEX_ID_VPR_SCRATCH, (LwU32)VPR_MUTEX_ACQUIRE_TIMEOUT_NS, &mutexToken));
#endif

    // Patch current vpr start address
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14, &reg));
    reg = FLD_SET_DRF_NUM( _PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _LWRRENT_VPR_RANGE_START_ADDR_MB_ALIGNED, vprOffsetMBAligned, reg);
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14, reg));

    // Patch current vpr size
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_13, &reg));
    reg = FLD_SET_DRF_NUM( _PGC6, _BSI_VPR_SELWRE_SCRATCH_13, _LWRRENT_VPR_SIZE_MB, vprSizeMBAligned, reg);
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_13, reg));

ErrorExit:
    //  Don't overwrite an earlier failure, after ErrorExit
#ifdef SELWRITY_ENGINE
    // Release mutex after accesses to secure scratch registers
    if (mutexToken != LW_MUTEX_OWNER_ID_ILWALID)
    {
        tmpStatus = mutexRelease_hs(LW_MUTEX_ID_VPR_SCRATCH, mutexToken);
        if (flcnStatus == FLCN_OK)
        {
            flcnStatus = tmpStatus;
        }
    }
#endif

    return flcnStatus;
}

/*!
 * @brief: Write protect region using memlock and scrub
 */
FLCN_STATUS
vprLockAndScrub_GP10X
(
    LwU64 scrubStartAddrInBytes,
    LwU64 scrubEndAddrInBytes
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Set mem lock
    CHECK_FLCN_STATUS(vprSetMemLockRange_HAL(&VprHal, scrubStartAddrInBytes, scrubEndAddrInBytes));

    // Scrub region
    CHECK_FLCN_STATUS(vprScrubRegion_HAL(&VprHal, scrubStartAddrInBytes, scrubEndAddrInBytes));

ErrorExit:
    return flcnStatus;
}

/*!
 * Verify is this build should be allowed to run on particular chip
 */
FLCN_STATUS
vprCheckIfBuildIsSupported_GP10X(void)
{
    LwU32       chip;
    FLCN_STATUS flcnStatus = FLCN_OK;

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
 * @brief Write Error Status to falcon mailbox
 */
FLCN_STATUS
vprWriteStatusToFalconMailbox_GP10X
(
    FLCN_STATUS errorCode
)
{
    return CSB_REG_WR32_HS_ERRCHK(LW_CSEC_FALCON_MAILBOX0, errorCode);
}

/*!
 * @brief Check if previous command had failed
 *        This is required to allow/disallow new command and return error
 */
FLCN_STATUS
vprCheckPreviousCommandStatus_GP10X(void)
{
    FLCN_STATUS flcnStatus   = FLCN_OK;
    LwU32       vprScratch13 = 0;

    //
    // MAX_VPR_SIZE is set by vbios only once during boot time only on VPR supported chips,
    // and then this is cross-checked wherever we have VPR related programming
    // It is sort of SW knob to know if VPR is supported or not
    //
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_13, &vprScratch13));

    if (FLD_TEST_DRF_NUM(_PGC6, _BSI_VPR_SELWRE_SCRATCH_13, _MAX_VPR_SIZE_MB, 0, vprScratch13))
    {
        flcnStatus = FLCN_ERR_VPR_APP_PREVIOUS_CMD_FAILED_AS_MAX_VPR_IS_0;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief To block new VPR requests set MAX_VPR_SIZE to 0
 */
FLCN_STATUS
vprBlockNewRequests_GP10X(void)
{
    FLCN_STATUS flcnStatus   = FLCN_OK;
    LwU32       vprScratch13 = 0;

#ifdef SELWRITY_ENGINE
    LwU8        mutexToken   = LW_MUTEX_OWNER_ID_ILWALID;
    FLCN_STATUS tmpStatus    = FLCN_OK;

    //
    // Like explained in vprCheckPreviousCommandStatus_GP10X, MAX_VPR_SIZE informs us if VPR is supported or not,
    // changing its value to zero will eventually make VPR unsupported
    //

    // Acquire mutex for accesses to secure scratch registers
    CHECK_FLCN_STATUS(mutexAcquire_hs(LW_MUTEX_ID_VPR_SCRATCH, (LwU32)VPR_MUTEX_ACQUIRE_TIMEOUT_NS, &mutexToken));
#endif

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_13, &vprScratch13));
    vprScratch13 = FLD_SET_DRF_NUM(_PGC6, _BSI_VPR_SELWRE_SCRATCH_13, _MAX_VPR_SIZE_MB, 0, vprScratch13);
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_13, vprScratch13));

ErrorExit:
    //  Don't overwrite an earlier failure, after ErrorExit
#ifdef SELWRITY_ENGINE
    // Release mutex after accesses to secure scratch registers
    if (mutexToken != LW_MUTEX_OWNER_ID_ILWALID)
    {
        tmpStatus = mutexRelease_hs(LW_MUTEX_ID_VPR_SCRATCH, mutexToken);
        if (flcnStatus == FLCN_OK)
        {
            flcnStatus = tmpStatus;
        }
    }
#endif
    return flcnStatus;
}

/*!
 * @brief Check handoff from scrubber binary
 */
FLCN_STATUS
vprCheckScrubberHandoff_GP10X(void)
{
    FLCN_STATUS flcnStatus   = FLCN_OK;
    LwU32       vprScratch15 = 0;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15, &vprScratch15));
    if (!FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_15, _VPR_HANDOFF, _VALUE_SCRUBBER_BIN_DONE, vprScratch15))
    {
        flcnStatus = FLCN_ERR_VPR_APP_UNEXPECTED_VPR_HANDOFF_FROM_SCRUBBER;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Check if DPU is in LS mode
 */
FLCN_STATUS
vprCheckIfDpuIsInLSMode_GP10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    /*
    LwU32       dpuSctl    = DRF_DEF(_PDISP, _FALCON_SCTL, _LSMODE, _FALSE);

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_FALCON_SCTL, &dpuSctl));
    if (!FLD_TEST_DRF(_PDISP, _FALCON_SCTL, _LSMODE, _TRUE, dpuSctl))
    {
        flcnStatus = FLCN_ERR_VPR_APP_DISP_FALCON_IS_NOT_IN_LS_MODE;
    }

ErrorExit:
    */
    return flcnStatus;
}
