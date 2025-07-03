/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: selwrescrubgm206.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_fb.h"
#include "dev_disp.h"
#include "dev_lwdec_csb.h"
#include "dev_master.h"
#include "dev_fuse.h"
#include "selwrescrub.h"
#include "selwrescrubreg.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void selwrescrubBar0WaitIdle_GM206(void) ATTR_OVLY(SELWRE_OVL);
static LwU32 _selwrescrubReadVprInfo(LwU32) ATTR_OVLY(SELWRE_OVL);
static LwU32 _selwrescrubReadWprInfo(LwU32) ATTR_OVLY(SELWRE_OVL);
static void _selwrescrubWriteVprWprData(LwU32, LwU32) ATTR_OVLY(SELWRE_OVL);
static void _selwrescrubSetVprState(LwBool) ATTR_OVLY(SELWRE_OVL);
static LwBool  _selwrescrubGetVprState(void) ATTR_OVLY(SELWRE_OVL);
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Wait for BAR0 to become idle
 */
static void selwrescrubBar0WaitIdle_GM206(void)
{
    LwU32 csr_data   = 0;
    LwU32 lwr_status = LW_CLWDEC_BAR0_CSR_STATUS_BUSY;

    //wait for the previous transaction to complete
    while(lwr_status == LW_CLWDEC_BAR0_CSR_STATUS_BUSY)
    {
        csr_data = falc_ldx_i((LwU32*)LW_CLWDEC_BAR0_CSR, 0);
        lwr_status = DRF_VAL(_CLWDEC, _BAR0_CSR, _STATUS, csr_data);
    }
}

/*!
 * @brief Reads BAR0
 */
LwU32 selwrescrubReadBar0_GM206(LwU32 addr)
{
    LwU32 val32;
    LwU32 read_trig_value = DRF_DEF(_CLWDEC, _BAR0_CSR, _CMD, _READ) |
                            DRF_DEF(_CLWDEC, _BAR0_CSR, _TRIG, _TRUE) |
                            DRF_NUM(_CLWDEC, _BAR0_CSR, _BYTE_ENABLE, 0xF);

    falc_stx((LwU32 *)LW_CLWDEC_BAR0_TMOUT, (1<<27));
    falc_stx((LwU32*)LW_CLWDEC_BAR0_ADDR, addr);
    falc_stx((LwU32*)LW_CLWDEC_BAR0_CSR, read_trig_value);
    selwrescrubBar0WaitIdle_GM206();//  Bar0WaitIdle();
    val32 = falc_ldx_i((LwU32*)LW_CLWDEC_BAR0_DATA, 0);
    return val32;
}

/*!
 * @brief Writes BAR0
 */
LwU32 selwrescrubWriteBar0_GM206(LwU32 addr, LwU32 data)
{
    LwU32 val32;
    LwU32 write_trig_value = DRF_DEF(_CLWDEC, _BAR0_CSR, _CMD, _WRITE) |
                             DRF_DEF(_CLWDEC, _BAR0_CSR, _TRIG, _TRUE) |
                             DRF_NUM(_CLWDEC, _BAR0_CSR, _BYTE_ENABLE, 0xF);

    falc_stx((LwU32 *)LW_CLWDEC_BAR0_TMOUT, (1<<27));
    falc_stx((LwU32*)LW_CLWDEC_BAR0_ADDR, addr);
    FALC_CSB_REG_WR32(LW_CLWDEC_BAR0_DATA, data);
    falc_stx((LwU32*)LW_CLWDEC_BAR0_CSR, write_trig_value);
    selwrescrubBar0WaitIdle_GM206();//  Bar0WaitIdle();
    val32 = falc_ldx_i((LwU32*)LW_CLWDEC_BAR0_DATA, 0);
    return val32;
}

/*!
 * @brief: Programs CTX DMA
 *
 * @param[in] target    : FB or sysmem
 * @param[in] addr      : address space: virtual or physical
 * @param[in] ctxDma    : context DMA 
 */
SELWRESCRUB_RC
selwrescrubProgramCtxDma_GM206
(
    ADDR_TARGET target,
    ADDR_SPACE addr,
    LwU32 ctxDma
)
{
    LwU32 data = 0;

    switch(target)
    {
        case ADDR_TARGET_FB:
            data = FLD_SET_DRF(_CLWDEC, _FBIF_TRANSCFG, _TARGET, _LOCAL_FB, data);
        break;
        default:
            //
            // Writing to sysmem is not supported here on purpose, it's not needed.
            // We also don't want to add some code that could be utilized unknowingly.
            //
            return SELWRESCRUB_RC_ILWALID_ARGUMENT;
        break;
    }

    switch(addr)
    {
        case ADDR_SPACE_PHYSICAL:
            data = FLD_SET_DRF(_CLWDEC, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, data);
        break;
        default:
            //
            // Virtual address space is not supported here on purpose, it's not needed.
            // We also don't want to add some code that could be utilized unknowingly.
            //
            return SELWRESCRUB_RC_ILWALID_ARGUMENT;
        break;
    }
    // All is good, program the TRANSCFG now.
    FALC_REG_WR32(CSB, LW_CLWDEC_FBIF_TRANSCFG(ctxDma), data);

    return SELWRESCRUB_RC_OK;
}

/*!
 * @brief: WPR info can be read by querying WPR_INFO registers in MMU
 */
static LwU32
_selwrescrubReadWprInfo
(
    LwU32 index
)
{
    LwU32 cmd = 0;
    while(LW_TRUE)
    {
        cmd = DRF_NUM(_PFB, _PRI_MMU_WPR_INFO, _INDEX, index);
        FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_INFO, cmd);

        cmd = FALC_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_INFO);
        //
        // Ensure that WPR info that we read has correct index. This is because
        // hacker can keep polling this register to yield invalid expected
        // value or RM also uses this register and can auto increment
        //
        if (FLD_TEST_DRF_NUM(_PFB, _PRI_MMU_WPR_INFO, _INDEX, index, cmd))
        {
            LwU32 val = DRF_VAL(_PFB, _PRI_MMU_WPR_INFO, _DATA, cmd);
            
            if (index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_ALLOW_READ || 
                index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_ALLOW_WRITE)
            {
                // Basically left shift by 4 bits and return value of read/write mask.
                return DRF_NUM(_PFB, _PRI_MMU_WPR_INFO, _DATA, val);
            }
            else
            {
                return val;
            }
        }
    }

    // Code will not come here
    selwrescrubCleanup();
    falc_halt();
    return 0;
}


/*!
 * @brief: VPR info can be read by querying VPR_INFO registers in MMU
 */
static LwU32
_selwrescrubReadVprInfo
(
    LwU32 index
)
{
    LwU32 cmd = 0;
    while(LW_TRUE)
    {
        cmd = DRF_NUM(_PFB, _PRI_MMU_VPR_INFO, _INDEX,index);
        FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_INFO, cmd);

        cmd = FALC_REG_RD32(BAR0, LW_PFB_PRI_MMU_VPR_INFO);

        //
        // Ensure that VPR info that we read has correct index. This is because
        // hacker can keep polling this reigster to yield invalid expected
        // value or RM also uses this register and can autoincreament
        //
        if (FLD_TEST_DRF_NUM(_PFB, _PRI_MMU_VPR_INFO, _INDEX, index, cmd))
        {
            return DRF_VAL(_PFB, _PRI_MMU_VPR_INFO, _DATA, cmd);
        }
    }

    // Code will not come here
    selwrescrubCleanup();
    falc_halt();
    return 0;
}

/*!
 * @brief: Update VPR/WPR information in MMU
 *
 * This function takes a VPR or WPR index and data pair, and programs that
 * information in MMU. All VPR & WPR indexes listed for the register
 * LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX* are supported.
 */
static void
_selwrescrubWriteVprWprData
(
    LwU32 index,
    LwU32 data
)
{
    LwU32 cmd = 0;

    cmd = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _INDEX, index, 0 );
    cmd = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _DATA, data, cmd);

    FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_WPR_WRITE, cmd);
}

/*!
 * @brief: Set/Unset VPR_IN_USE bit which enables VPR range check in MMU
 */
static void _selwrescrubSetVprState
(
    LwBool bEnableVpr
)
{
    LwU32 vprCmd = 0;
    LwU32 cyaLo;

    //
    // Unselwre entity may keep polling the VPR_INFO register. that will give invalid
    // CYA_LO info. since VPR_INFO_INDEX gets increamented after read, we need
    // to ensure that value we are reading is correct.
    //
    do
    {
        // Query VPR CYA_LO info
        vprCmd = DRF_DEF(_PFB, _PRI_MMU_VPR_INFO, _INDEX, _CYA_LO);
        FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_INFO, vprCmd);

        // Read back the register
        vprCmd = FALC_REG_RD32(BAR0, LW_PFB_PRI_MMU_VPR_INFO);

        // Clear IN_USE_BIT
        vprCmd = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_IN_USE, 0, vprCmd );

        // GET CYA_LO
        cyaLo = DRF_VAL(_PFB, _PRI_MMU_VPR, _INFO_DATA, vprCmd);
    }while( !FLD_TEST_DRF(_PFB, _PRI_MMU_VPR_INFO, _INDEX, _CYA_LO, vprCmd));


    // Write back VPR value
    FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_WPR_WRITE,
         DRF_DEF(_PFB, _PRI_MMU_VPR_WPR_WRITE, _INDEX, _VPR_CYA_LO)   |
         DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_IN_USE, bEnableVpr) |
         DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _DATA, cyaLo));

}

/*!
 * @brief: Returns VPR state in MMU
 */
static LwBool
_selwrescrubGetVprState(void)
{
    LwU32 vprCmd = 0;

    //
    // Unselwre Entity may keep polling the VPR_INFO register. that will give invalid
    // CYA_LO info. since VPR_INFO_INDEX gets increamented after read, we need
    // to ensure that value we are reading is correct.
    //
    do
    {
        // Query VPR CYA_LO info
        vprCmd = DRF_DEF(_PFB, _PRI_MMU_VPR_INFO, _INDEX, _CYA_LO);
        FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_INFO, vprCmd);

        // Read back the register
        vprCmd = FALC_REG_RD32(BAR0, LW_PFB_PRI_MMU_VPR_INFO);

    }while( !FLD_TEST_DRF(_PFB, _PRI_MMU_VPR_INFO, _INDEX, _CYA_LO, vprCmd));

    return DRF_VAL(_PFB, _PRI_MMU_VPR_INFO, _CYA_LO_IN_USE, vprCmd);
}
/*!
 * @brief: Query VPR information in MMU
 */
void
selwrescrubGetSetVprMmuSettings_GM206
(
    PVPR_ACCESS_CMD pVprCmd
)
{
    switch(pVprCmd->cmd)
    {
        case VPR_SETTINGS_GET:
            pVprCmd->vprRangeStart = ((LwU64)_selwrescrubReadVprInfo(LW_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_LO))<<LW_PFB_PRI_MMU_VPR_INFO_ADDR_ALIGNMENT;
            pVprCmd->vprRangeEnd   = ((LwU64)_selwrescrubReadVprInfo(LW_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_HI))<<LW_PFB_PRI_MMU_VPR_INFO_ADDR_ALIGNMENT;
            pVprCmd->vprRangeEnd  |= ((VPR_END_ADDR_ALIGNMENT) - 1);
            pVprCmd->bVprEnabled   = _selwrescrubGetVprState();
        break;

        case VPR_SETTINGS_SET:
            _selwrescrubWriteVprWprData(LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_LO,
                    (LwU32)(pVprCmd->vprRangeStart>>LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT));
            _selwrescrubWriteVprWprData(LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_HI,
                    (LwU32)(pVprCmd->vprRangeEnd>>LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT));
            _selwrescrubSetVprState(pVprCmd->bVprEnabled);
        break;
    }
}

/*!
 * @brief: reports error code in mailbox 0 and additional information in
 *          mailbox1
 */
void
selwrescrubReportStatus
(
    LwU32 mailbox0,
    LwU32 mailbox1
)
{
    FALC_REG_WR32(CSB, LW_CLWDEC_FALCON_MAILBOX0, mailbox0);

    // Do not update mailbox1 if it is 0. it carries additional error information
    if (mailbox1)
    {
        FALC_REG_WR32(CSB, LW_CLWDEC_FALCON_MAILBOX1, mailbox1);
    }
}

/*!
 * @brief Cleanup the falcon state before exiting.
 */
void selwrescrubCleanup(void)
{
    // De-assert big hammer
    FALC_REG_WR32_STALL(CSB, LW_CLWDEC_SCP_CTL_CFG, 0x0);
}

LwBool
selwrescrubIsDebugModeEnabled_GM200(void)
{
    LwU32 scpCtl = FALC_REG_RD32(CSB, LW_CLWDEC_CTL_STAT);
    return !FLD_TEST_DRF(_CLWDEC, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, scpCtl);
}
#define SELWRESCRUB_UCODE_VERSION_GM206     (0)
#define SELWRESCRUB_UCODE_VERSION_DEFAULT   (0)
/*!
 * @brief: revoke scrub binary
 */
void
selwrescrubRevokeHsBin_GM206(void)
{
    LwU32  bit0 = FALC_REG_RD32(BAR0, LW_FUSE_SPARE_BIT_5) & 0x1;
    LwU32  bit1 = FALC_REG_RD32(BAR0, LW_FUSE_SPARE_BIT_6) & 0x1;
    LwU32  fuseVersionHW = (bit1 << 1) | (bit0);
    LwU32  ucodeVersion = 0;
    LwBool bUnsupportedChip = LW_FALSE;

    LwU32 chipId  = FALC_REG_RD32(BAR0, LW_PMC_BOOT_42);
    chipId        = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chipId);

    switch(chipId)
    {
        case LW_PMC_BOOT_42_CHIP_ID_GM206:
            ucodeVersion = SELWRESCRUB_UCODE_VERSION_GM206;
        break;
        default:
            if (selwrescrubIsDebugModeEnabled_GM200())
            {
                ucodeVersion = SELWRESCRUB_UCODE_VERSION_DEFAULT;
            }
            else
            {
                bUnsupportedChip = LW_TRUE;
            }
    }
    if (bUnsupportedChip  || 
            ucodeVersion < fuseVersionHW)
    {
        selwrescrubReportStatus(SELWRESCRUB_RC_BIN_REVOKED, 0);
        selwrescrubCleanup();
        falc_halt();
    }
}


/*!
 * @brief: Set VPR display blanking policy
 */
void
selwrescrubSetVprBlankingPolicy_GM206(LwU32 policy)
{
    FALC_REG_WR32(BAR0, LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL, policy);
}


/*!
 * @brief: Lock range with WPR2
 */
SELWRESCRUB_RC
selwrescrubMakeRangeWriteProtected_GM206
(
    LwU64   startAddress,
    LwU64   endAddress,
    LwBool  bLockOperation
)
{
    LwU32  finalStartAddr4KAligned  = 0;
    LwU32  finalEndAddr4KAligned    = 0;
    LwU32  wprWriteMask             = 0;
    LwU32  wprReadMask              = 0;
    LwU32  regionCfg                = 0;
    // TODO when using this code, please move these move these globals to stack for better protection.
    static LwU64    origStartAddrWPR2, origEndAddrWPR2;
    static LwU32    origReadMaskWPR2, origWriteMaskWPR2;
    static LwBool   bOrigWPR2SettingsValid = LW_FALSE;

    if (bLockOperation)
    {
        // Save original WPR2 settings.
        origStartAddrWPR2       = ((LwU64)_selwrescrubReadWprInfo(LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR2_ADDR_LO)) << LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT; 
        origEndAddrWPR2         = ((LwU64)_selwrescrubReadWprInfo(LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR2_ADDR_HI)) << LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT; 
        origReadMaskWPR2        = _selwrescrubReadWprInfo(LW_PFB_PRI_MMU_WPR_INFO_INDEX_ALLOW_READ);
        origWriteMaskWPR2       = _selwrescrubReadWprInfo(LW_PFB_PRI_MMU_WPR_INFO_INDEX_ALLOW_WRITE);
        bOrigWPR2SettingsValid  = LW_TRUE;

        // WPR has no valid or IN_USE bit like VPR. If the start addr is <= end addr, that WPR is active.
        if (origStartAddrWPR2 <= origEndAddrWPR2)
        {
            //
            // If there is a valid WPR2 already set, lets take a union of current settings and what we want to set.
            // No SW code should use WPR2. It's reserved for the purpose of VPR scrubbing. But, if someone used it
            // mistakenly, we don't want to take away protection from it when we use WPR2.
            // This also means that, if the current WPR2 range is not right next to our range, we will still apply
            // union of the two ranges, that means the middle part which doesn't need WPR protection will still get
            // it inadverently. We want to avoid a security bug, hence this behavior. 
            //
            startAddress    = LW_MIN(startAddress, origStartAddrWPR2);
            endAddress      = LW_MAX(endAddress, origEndAddrWPR2);
        }

        finalStartAddr4KAligned = (LwU32)(startAddress >> LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT);
        finalEndAddr4KAligned   = (LwU32)(endAddress   >> LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT);

        // For lock operation, update ALLOW_WRITE of WPR2 so level3 client can write to the region. We are level3! 
        wprWriteMask = origWriteMaskWPR2;
        wprWriteMask = FLD_SET_DRF_NUM (_PFB, _PRI_MMU_VPR_WPR_WRITE, _ALLOW_WRITE_WPR2_SELWRE3, 1, wprWriteMask);        
        wprWriteMask = FLD_SET_DRF_NUM (_PFB, _PRI_MMU_VPR_WPR_WRITE, _ALLOW_WRITE_WPR2_SELWRE2, 0, wprWriteMask);        
        wprWriteMask = FLD_SET_DRF_NUM (_PFB, _PRI_MMU_VPR_WPR_WRITE, _ALLOW_WRITE_WPR2_SELWRE1, 0, wprWriteMask);        
        wprWriteMask = FLD_SET_DRF_NUM (_PFB, _PRI_MMU_VPR_WPR_WRITE, _ALLOW_WRITE_WPR2_SELWRE0, 0, wprWriteMask);        

        // For lock operation, update ALLOW_READ of WPR2 so only level3 client can read the region. We are level3! 
        wprReadMask = origReadMaskWPR2;
        wprReadMask = FLD_SET_DRF_NUM (_PFB, _PRI_MMU_VPR_WPR_WRITE, _ALLOW_READ_WPR2_SELWRE3, 1, wprReadMask);        
        wprReadMask = FLD_SET_DRF_NUM (_PFB, _PRI_MMU_VPR_WPR_WRITE, _ALLOW_READ_WPR2_SELWRE2, 0, wprReadMask);        
        wprReadMask = FLD_SET_DRF_NUM (_PFB, _PRI_MMU_VPR_WPR_WRITE, _ALLOW_READ_WPR2_SELWRE1, 0, wprReadMask);        
        wprReadMask = FLD_SET_DRF_NUM (_PFB, _PRI_MMU_VPR_WPR_WRITE, _ALLOW_READ_WPR2_SELWRE0, 0, wprReadMask);

        // For lock operation, we will use REGIONCFG to indicate we will be writing to WPR2.
        regionCfg = FLD_SET_DRF_NUM(_CLWDEC, _FBIF_REGIONCFG, _T0, REGIONCFG_WPR2, 0);
    }
    else
    {
        if (!bOrigWPR2SettingsValid)
        {
            //
            // First Lock should've been requested, so we would've saved original settings.
            // If original settings are not saved & valid, we can't restore them. Return error.
            //
            return SELWRESCRUB_RC_ILWALID_STATE;
        }

        // If unlock is requested, restore original settings that were saved during lock.
        finalStartAddr4KAligned = (LwU32)(origStartAddrWPR2 >> LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT);
        finalEndAddr4KAligned   = (LwU32)(origEndAddrWPR2   >> LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT);
        wprWriteMask            = origWriteMaskWPR2;
        wprReadMask             = origReadMaskWPR2;

        // For unlock operation, we will reset REGIONCFG to zero.
        regionCfg = FLD_SET_DRF_NUM(_CLWDEC, _FBIF_REGIONCFG, _T0, 0, 0);
    }

    LwBool done = LW_FALSE;
    while (!done)
    {
        // Write start address to WPR2_ADDR_LO
        _selwrescrubWriteVprWprData(LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR2_ADDR_LO, finalStartAddr4KAligned);

        // Write end address to WPR2_ADDR_HI
        _selwrescrubWriteVprWprData(LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR2_ADDR_HI, finalEndAddr4KAligned);

        // update ALLOW_WRITE of WPR2. Do not use _selwrescrubWriteVprWprData function, Index has to be merged directly.
        LwU32 data = FLD_SET_DRF(_PFB, _PRI_MMU_VPR_WPR_WRITE, _INDEX, _WPR_ALLOW_WRITE, wprWriteMask);
        FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_WPR_WRITE, data);

        // update ALLOW_READ of WPR2. Do not use _selwrescrubWriteVprWprData function, Index has to be merged directly
        data = FLD_SET_DRF(_PFB, _PRI_MMU_VPR_WPR_WRITE, _INDEX, _WPR_ALLOW_READ, wprReadMask);
        FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_WPR_WRITE, data);

        // Update REGIONCFG to indicate we will be writing to WPR2.
        FALC_REG_WR32(CSB, LW_CLWDEC_FBIF_REGIONCFG, regionCfg);

        // Now verify that the register writes have actually taken effect.
        LwU32 programmedStartAddr       = _selwrescrubReadWprInfo(LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR2_ADDR_LO);
        LwU32 programmedEndAddr         = _selwrescrubReadWprInfo(LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR2_ADDR_HI);
        LwU32 programmedWprWriteMask    = _selwrescrubReadWprInfo(LW_PFB_PRI_MMU_WPR_INFO_INDEX_ALLOW_WRITE);
        LwU32 programmedWprReadMask     = _selwrescrubReadWprInfo(LW_PFB_PRI_MMU_WPR_INFO_INDEX_ALLOW_READ);
        LwU32 programmedRegionCfg       = FALC_REG_RD32(CSB, LW_CLWDEC_FBIF_REGIONCFG);
        
        if ((programmedStartAddr    == (finalStartAddr4KAligned & MASK_MAX_4KB_WPR_ADDR_MMU & MASK_4KB_ADDR_TO_128KB_ADDR)) && 
            (programmedEndAddr      == (finalEndAddr4KAligned   & MASK_MAX_4KB_WPR_ADDR_MMU & MASK_4KB_ADDR_TO_128KB_ADDR)) &&
            (programmedWprWriteMask == wprWriteMask) &&
            (programmedWprReadMask  == wprReadMask)  &&
            (programmedRegionCfg    == regionCfg))
        {
            done = LW_TRUE;
        }
    }

    return SELWRESCRUB_RC_OK;
}


/*!
 * @brief: Update falcon REGIONCFG and sticky bit to be able to read from VPR.
 */
void
selwrescrubProgramFalconForVPRDataRead_GM206
(
    LwBool  bEnableVprReads
)
{
    LwU32 data = 0;
    if (bEnableVprReads)
    {
        // Program REGIONCFG, so we can read from VPR.
        LwU32 data = FLD_SET_DRF_NUM(_CLWDEC, _FBIF_REGIONCFG, _T0, REGIONCFG_VPR, 0);
        FALC_REG_WR32(CSB, LW_CLWDEC_FBIF_REGIONCFG, data);

        // Program Sticky bit, so writes from this binary can never go outside VPR.
        data = FLD_SET_DRF (_CLWDEC, _FALCON_PRIVSTATE, _VPR_FORCE_STICKY, _ENABLE, 0);
        FALC_REG_WR32(CSB, LW_CLWDEC_FALCON_PRIVSTATE, data);
    }
    else
    {
        // For disabling reads, reset REGIONCFG and Sticky bit
        FALC_REG_WR32(CSB, LW_CLWDEC_FBIF_REGIONCFG, 0);
        data = FLD_SET_DRF (_CLWDEC, _FALCON_PRIVSTATE, _VPR_FORCE_STICKY, _DISABLE, 0);
        FALC_REG_WR32(CSB, LW_CLWDEC_FALCON_PRIVSTATE, data);
    }
}


/*!
 * @brief: Issue a FB flush and wait for it to complete.
 */
SELWRESCRUB_RC 
selwrescrubIssueFlushAndWaitForCompletion_GM206(void)
{
    LwU32 data32                = 0;
    LwU32 flushWaitMaxNs        = 10 * 1000;    // 10us total
    volatile LwU32 timerStartNs = 0;
    LwU32 timerLwrrentNs        = 0;

    data32 = FALC_REG_RD32(CSB, LW_CLWDEC_FBIF_CTL);

    data32 = FLD_SET_DRF(_CLWDEC, _FBIF_CTL, _FLUSH,  _SET , data32);
    data32 = FLD_SET_DRF(_CLWDEC, _FBIF_CTL, _ENABLE, _TRUE, data32);

    FALC_REG_WR32(CSB, LW_CLWDEC_FBIF_CTL, data32);

    timerStartNs = FALC_REG_RD32(CSB, LW_CLWDEC_FALCON_PTIMER0);

    do
    {
        data32 = FALC_REG_RD32(CSB, LW_CLWDEC_FBIF_CTL);

        //
        // HW should take a max of 3.8 uSec to clear up flush bit. However, we shall wait for
        // a max of 10uSec to make sure we have waited enough for the bit to be cleared up.
        //
        timerLwrrentNs = FALC_REG_RD32(CSB, LW_CLWDEC_FALCON_PTIMER0);

        // Account for roll-over of timer0. If current < start, the timer has rolled over.
        if (timerLwrrentNs < timerStartNs)
        {
            // timer0 is 32bit, can go max upto 0xFFFFFFFF. Compute the total time spent.
            if ((0xFFFFFFFF - timerStartNs) + timerLwrrentNs > flushWaitMaxNs)
            {
                return SELWRESCRUB_RC_FB_FLUSH_TIMEOUT;
            }
        }
        else
        {
            // timer0 has not rolled over yet.
            if ((timerLwrrentNs - timerStartNs) > flushWaitMaxNs)
            {
                return SELWRESCRUB_RC_FB_FLUSH_TIMEOUT;
            }
        }

    } while(!FLD_TEST_DRF(_CLWDEC, _FBIF_CTL, _FLUSH, _CLEAR, data32));

    return SELWRESCRUB_RC_OK;
}
