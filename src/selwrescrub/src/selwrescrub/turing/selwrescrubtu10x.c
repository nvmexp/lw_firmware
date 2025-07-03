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
 * @file: selwrescrubtu10x.c
 */
/* ------------------------- System Includes -------------------------------- */
#include "selwrescrub.h"
#include "selwrescrubreg.h"
#include "dev_fuse.h"
#include "dev_master.h"
#include "dev_fb.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "config/g_selwrescrub_private.h"
#include "dev_lwdec_csb.h"
#include "dev_se_seb.h"
#include "dev_bus.h"
#include "falcon.h"

/*
 * @brief: Check if UCODE is running on valid chip
 */
LwBool
selwrescrubIsChipSupported_TU10X(void)
{
    LwBool bSupportedChip = LW_FALSE;

    LwU32 chipId  = FALC_REG_RD32(BAR0, LW_PMC_BOOT_42);
    chipId        = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chipId);

    switch (chipId)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU102:
        case LW_PMC_BOOT_42_CHIP_ID_TU104:
        case LW_PMC_BOOT_42_CHIP_ID_TU106:
            bSupportedChip = LW_TRUE;
        break;
    }

    return bSupportedChip;
}

/*
 * @brief: Returns supported binary version blowned in fuses.
 */
SELWRESCRUB_RC
selwrescrubGetHwFuseVersion_TU10X(LwU32 *pFuseVersionHw)
{
    if (!pFuseVersionHw)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    *pFuseVersionHw  = FALC_REG_RD32(BAR0, LW_FUSE_OPT_FUSE_UCODE_SCRUBBER_BIN_REV);

    return SELWRESCRUB_RC_OK;
}

/*!
 * @brief: Returns the CBC adjustment size required to callwlate the range locked by Vbios
 *         Refer Bug 2194638 - on SKUs having greater CBC size we need to adjustment of 768MB otherwise 256MB
 *         In short, expand unlocked upper memory region to account for large CBC sizes on Turing GPUs with > 12GB FB
 *
 * @param[in] totalFbSizeInMB: Total FB size in MB
 *
 * @return CBC adjustment size
 */
SELWRESCRUB_RC
selwrescrubGetFbSyncScrubSizeAtFbEndInBytes_TU10X
(
    LwU64 totalFbSizeInMB,
    LwU64 *pFbSyncScrubSizeInBytes
)
{
    if(pFbSyncScrubSizeInBytes == NULL)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    if (totalFbSizeInMB > MIN_FB_SIZE_FOR_LARGE_SYNC_SCRUB_CBC)
    {
        *pFbSyncScrubSizeInBytes = NUM_BYTES_IN_768_MB;
    }
    else
    {
        *pFbSyncScrubSizeInBytes = NUM_BYTES_IN_256_MB;
    }

    return SELWRESCRUB_RC_OK;
}

/*!
* @brief: selwrescrubGetSetVprMmuSettings_TU10X 
*  Get or Set the VPR Settings
*
* @param: [in] pVprCmd   VPR Access structure to GET/SET VPR State and Range.
*
*/
void
selwrescrubGetSetVprMmuSettings_TU10X
(
    PVPR_ACCESS_CMD pVprCmd
)
{
    LwU32 regVal = 0;

    if (pVprCmd != NULL)
    {
        switch(pVprCmd->cmd)
        {
            case VPR_SETTINGS_GET:
                // Read VPR Start Address and align to byte boundary
                regVal = REF_VAL(LW_PFB_PRI_MMU_VPR_ADDR_LO_VAL, FALC_REG_RD32(BAR0, LW_PFB_PRI_MMU_VPR_ADDR_LO));
                pVprCmd->vprRangeStartInBytes = (LwU64)regVal << LW_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT;

                // Read VPR End Address and align to byte boundary
                regVal = REF_VAL(LW_PFB_PRI_MMU_VPR_ADDR_HI_VAL, FALC_REG_RD32(BAR0, LW_PFB_PRI_MMU_VPR_ADDR_HI));
                pVprCmd->vprRangeEndInBytes  = (LwU64)regVal << LW_PFB_PRI_MMU_VPR_ADDR_HI_ALIGNMENT;
                pVprCmd->vprRangeEndInBytes |= ((NUM_BYTES_IN_1_MB) - 1);

                pVprCmd->bVprEnabled   = selwrescrubGetVprState_HAL();
            break;

            case VPR_SETTINGS_SET:
                // Set the start address of VPR
                FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_ADDR_LO, 
                              REF_NUM(LW_PFB_PRI_MMU_VPR_ADDR_LO_VAL, 
                              (LwU32)(pVprCmd->vprRangeStartInBytes >> LW_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT)));

                // Set the end address of VPR
                FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_ADDR_HI, 
                              REF_NUM(LW_PFB_PRI_MMU_VPR_ADDR_HI_VAL, 
                              (LwU32)(pVprCmd->vprRangeEndInBytes >> LW_PFB_PRI_MMU_VPR_ADDR_HI_ALIGNMENT)));

                selwrescrubSetVprState_HAL(pSelwrescrub, pVprCmd->bVprEnabled);
            break;
        }
    }
}

/*!
* @brief: selwrescrubGetVprState_TU10X 
*  Returns VPR state in MMU
*/
LwBool
selwrescrubGetVprState_TU10X(void)
{
    return !!REF_VAL(LW_PFB_PRI_MMU_VPR_CYA_LO_IN_USE, FALC_REG_RD32(BAR0, LW_PFB_PRI_MMU_VPR_CYA_LO));
}

/*!
 * @brief: selwrescrubRemoveWriteProtection_TU10X
 * Remove write protection as requested - WPR2/memlock range.
 *
 * @param [in]  bRemoveWpr2WriteProtection     WPR2 Write Protection permission 
 * @param [in]  bRemoveMemLockRangeProtection  MEM Lock Write Protection permission
 *
 * @return SELWRESCRUB_RC_OK on SUCCESS
 */
SELWRESCRUB_RC
selwrescrubRemoveWriteProtection_TU10X
(
    LwBool  bRemoveWpr2WriteProtection,
    LwBool  bRemoveMemLockRangeProtection
)
{
    if (bRemoveWpr2WriteProtection)
    {
        // Ilwalidate start Address of WPR2
        FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO, 
                      REF_NUM(LW_PFB_PRI_MMU_WPR2_ADDR_LO_VAL, ILWALID_WPR_ADDR_LO));

        // Ilwalidate end address of WPR2
        FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI, 
                      REF_NUM(LW_PFB_PRI_MMU_WPR2_ADDR_HI_VAL, ILWALID_WPR_ADDR_HI));
    }

    if (bRemoveMemLockRangeProtection)
    {
        // Ilwalidate start Address of Mem Lock
        FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_LOCK_ADDR_LO, 
                      REF_NUM(LW_PFB_PRI_MMU_LOCK_ADDR_LO_VAL, ILWALID_MEMLOCK_RANGE_ADDR_LO));

        // Ilwalidate end Address of Mem Lock
        FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_LOCK_ADDR_HI, 
                      REF_NUM(LW_PFB_PRI_MMU_LOCK_ADDR_HI_VAL, ILWALID_MEMLOCK_RANGE_ADDR_HI));
    }

    return SELWRESCRUB_RC_OK;
}


/*!
* @brief selwrescrubGetLwrMemlockRangeInBytes_TU10X:
*        Get the current Mem Lock range
*
* @param[out]  pMemlockStartAddrInBytes   Start Address of Mem Lock
* @param[out]  pMemlockEndAddrInBytes     End Address of Mem Lock
*
*/
SELWRESCRUB_RC
selwrescrubGetLwrMemlockRangeInBytes_TU10X
(
    LwU64 *pMemlockStartAddrInBytes,
    LwU64 *pMemlockEndAddrInBytes
)
{
    LwU32 regVal = 0;
    SELWRESCRUB_RC status = SELWRESCRUB_RC_OK;

    if (!pMemlockStartAddrInBytes || !pMemlockEndAddrInBytes)
    {
        return SELWRESCRUB_RC_ILWALID_ARGUMENT;
    }

    // Read the Start Address of Mem Lock
    regVal = REF_VAL(LW_PFB_PRI_MMU_LOCK_ADDR_LO_VAL, FALC_REG_RD32(BAR0, LW_PFB_PRI_MMU_LOCK_ADDR_LO));
    *pMemlockStartAddrInBytes = (LwU64)regVal << LW_PFB_PRI_MMU_LOCK_ADDR_LO_ALIGNMENT;

    // Read the End Address of Mem Lock
    regVal = REF_VAL(LW_PFB_PRI_MMU_LOCK_ADDR_HI_VAL, FALC_REG_RD32(BAR0, LW_PFB_PRI_MMU_LOCK_ADDR_HI));
    *pMemlockEndAddrInBytes  = (LwU64)regVal << LW_PFB_PRI_MMU_LOCK_ADDR_HI_ALIGNMENT;
    *pMemlockEndAddrInBytes |= ((NUM_BYTES_IN_4_KB) - 1);

    return status;
}

/*!
* @brief selwrescrubGetHwScrubberRangeInBytes_TU10X:
*        Get the current HW Scrubber range
*
* @param[out]  pHwScrubberStartAddressInBytes   Start Address of HW Scrubber
* @param[out]  pHwScrubberEndAddressInBytes     End Address of HW Scrubber
*
*/
SELWRESCRUB_RC
selwrescrubGetHwScrubberRangeInBytes_TU10X
(
    LwU64 *pHwScrubberStartAddressInBytes,
    LwU64 *pHwScrubberEndAddressInBytes
)
{
    LwU32 regVal = 0;
    SELWRESCRUB_RC status = SELWRESCRUB_RC_OK;

    if (!pHwScrubberStartAddressInBytes || !pHwScrubberEndAddressInBytes)
    {
        return SELWRESCRUB_RC_ILWALID_ARGUMENT;
    }

    // Read start address of HW Scrubber
    regVal = FALC_REG_RD32(BAR0, LW_PFB_NISO_SCRUB_START_ADDR_STATUS);
    *pHwScrubberStartAddressInBytes = (LwU64)regVal << LW_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT;

    // Read End address of HW Scrubber
    regVal = FALC_REG_RD32(BAR0, LW_PFB_NISO_SCRUB_LAST_ADDR_STATUS);
    *pHwScrubberEndAddressInBytes = (LwU64)regVal << LW_PFB_PRI_MMU_VPR_ADDR_HI_ALIGNMENT;
    *pHwScrubberEndAddressInBytes|= ((NUM_BYTES_IN_4_KB) - 1);

    return status;
}

/*!
* @brief: selwrescrubSetVprState_TU10X
*  Set/Unset VPR_IN_USE bit which enables VPR range check in MMU.
*
* @param [in] bEnableVpr   Updated value of the IN_USE bit of VPR_CYA_LO
*
*/
void selwrescrubSetVprState_TU10X
(
    LwBool bEnableVpr
)
{
    LwU32 cyaLo;

    cyaLo = FALC_REG_RD32(BAR0, LW_PFB_PRI_MMU_VPR_CYA_LO);
    cyaLo = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR, _CYA_LO_IN_USE, bEnableVpr, cyaLo);
    FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_CYA_LO, cyaLo);
}

/*!
* @brief: selwrescrubValidateVprHasBeenScrubbed_TU10X
* Validate VPR has been scrubbed by HW scrubber. Scrub was started by devinit.
*
* @param [in] vprStartAddressInBytes   Start Address of VPR 
* @param [in] vprEndAddressInBytes     End Address of VPR
*
* @return SELWRESCRUB_RC_OK on SUCCESS
*/
SELWRESCRUB_RC
selwrescrubValidateVprHasBeenScrubbed_TU10X
(
    LwU64 vprStartAddressInBytes,
    LwU64 vprEndAddressInBytes
)
{
    LwU32   vprStartAddr4KAligned  = (LwU32)(vprStartAddressInBytes >> LW_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT);
    LwU32   vprEndAddr4KAligned    = (LwU32)(vprEndAddressInBytes   >> LW_PFB_PRI_MMU_VPR_ADDR_HI_ALIGNMENT);
    LwBool  bScrubberDone          = LW_FALSE;

    // Let's wait for the HW scrubber to finish.
    do
    {
        bScrubberDone = FLD_TEST_DRF(_PFB, _NISO_SCRUB_STATUS, _FLAG, _DONE, FALC_REG_RD32(BAR0, LW_PFB_NISO_SCRUB_STATUS));
    } while (!bScrubberDone);

    // Try to see if VPR range falls within the scrubbed range
    if (vprStartAddr4KAligned >= FALC_REG_RD32(BAR0, LW_PFB_NISO_SCRUB_START_ADDR_STATUS))
    {
        if (vprEndAddr4KAligned <= FALC_REG_RD32(BAR0, LW_PFB_NISO_SCRUB_LAST_ADDR_STATUS))
        {
            return SELWRESCRUB_RC_OK;
        }
        else
        {
            return SELWRESCRUB_RC_ENTIRE_VPR_RANGE_NOT_SCRUBBED_BY_HW_SCRUBBER;
        }
    }
    else
    {
        //
        // If start address itself does not contain VPR region, scrubber has been not been 
        // correctly started to cover VPR range. Return error.
        //
        return SELWRESCRUB_RC_HW_SCRUBBER_ILWALID_STATE;
    }

    return SELWRESCRUB_RC_OK;
}

/*!
 * @brief Function that checks if priv sec is enabled on prod board
 */
SELWRESCRUB_RC
selwrescrubCheckIfPrivSecEnabledOnProd_TU10X(void)
{

    if ( FLD_TEST_DRF( _CLWDEC, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, FALC_REG_RD32( CSB, LW_CLWDEC_SCP_CTL_STAT ) ) )
    {
        if ( FLD_TEST_DRF( _FUSE, _OPT_PRIV_SEC_EN, _DATA, _NO, FALC_REG_RD32( BAR0, LW_FUSE_OPT_PRIV_SEC_EN ) ) )
        {
            // Error: Priv sec is not enabled
            return SELWRESCRUB_RC_ERROR_PRIV_SEC_NOT_ENABLED;
        }
    }
    return  SELWRESCRUB_RC_OK;
}

/*!
 * @brief: Set timeout value for BAR0 transactions
 */
void selwrescrubSetBar0Timeout_TU10X(void)
{
    FALC_REG_WR32_STALL(CSB, LW_CLWDEC_BAR0_TMOUT,
        DRF_DEF(_CLWDEC, _BAR0_TMOUT, _CYCLES, __PROD));
}

/*
 * This funtion will ilwalidate the bubbles (blocks not of SELWRESCRUB HS, caused if SELWRESCRUB blocks are not loaded contiguously in IMEM)
 */
void
selwrescrubValidateBlocks_TU10X(void)
{
    LwU32 start        = (SELWRESCRUB_PC_TRIM((LwU32)_selwrescrub_resident_code_start));
    LwU32 end          = (SELWRESCRUB_PC_TRIM((LwU32)_selwrescrub_resident_code_end));

    LwU32 tmp          = 0;
    LwU32 imemBlks     = 0;
    LwU32 blockInfo    = 0;
    LwU32 lwrrAddr     = 0;

    //
    // Loop through all IMEM blocks and ilwalidate those that doesnt fall within
    // selwrescrub_resident_code_start and selwrescrub_resident_code_end
    //

    // Read the total number of IMEM blocks
    tmp = FALC_REG_RD32(CSB, LW_CLWDEC_FALCON_HWCFG);
    imemBlks = DRF_VAL(_CLWDEC_FALCON, _HWCFG, _IMEM_SIZE, tmp);

    for (tmp = 0; tmp < imemBlks; tmp++)
    {
        falc_imblk(&blockInfo, tmp);

        if (!(IMBLK_IS_ILWALID(blockInfo)))
        {
            lwrrAddr = IMBLK_TAG_ADDR(blockInfo);

            // Ilwalidate the block if it is not a part of SELWRESCRUB binary
            if (lwrrAddr < start || lwrrAddr >= end)
            {
                falc_imilw(tmp);
            }
        }
    }
}
