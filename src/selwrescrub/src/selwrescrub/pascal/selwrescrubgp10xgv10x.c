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
 * @file: selwrescrubgp10xgv10x.c
 */
/* ------------------------- System Includes -------------------------------- */

#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "selwrescrub.h"
#include "selwrescrubreg.h"
#include "dev_fb.h"
#include "dev_graphics_nobundle.h"

/*!
 * @brief: WPR info can be read by querying WPR_INFO registers in MMU
 */
LwU32
selwrescrubReadWprInfo_GP10X
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

            if (index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_ALLOW_READ   ||
                index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_ALLOW_WRITE  ||
                index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_VPR_PROTECTED_MODE)
            {
                // Basically left shift by 4 bits and return value of read/write mask.
                return DRF_NUM(_PFB, _PRI_MMU_WPR_INFO, _DATA, val);
            }
            else if (index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR1_ADDR_LO ||
                     index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR1_ADDR_HI ||
                     index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR2_ADDR_LO ||
                     index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR2_ADDR_HI ||
                     index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_LOCK_ADDR_LO ||
                     index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_LOCK_ADDR_HI)
            {
                return val;
            }
            else
            {
                //
                // If we are here, then the coder needs to be made aware of the fact that
                // the index they are trying to use should be added to one of the if/else
                // conditions above, otherwise this function does not support that index.
                // Error report and halt so this is brought to immediate attention.
                //
                selwrescrubReportStatus_HAL(pSelwrescrub,SELWRESCRUB_RC_ILWALID_INDEX, __LINE__);
                falc_halt();
            }
        }
    }

    // Code will not come here
    falc_halt();
    return 0;
}


/*!
 * @brief: VPR info can be read by querying VPR_INFO registers in MMU
 */
LwU32
selwrescrubReadVprInfo_GP10X
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
            LwU32 val = DRF_VAL(_PFB, _PRI_MMU_VPR_INFO, _DATA, cmd);

            if (index == LW_PFB_PRI_MMU_VPR_INFO_INDEX_CYA_LO ||
                index == LW_PFB_PRI_MMU_VPR_INFO_INDEX_CYA_HI)
            {
                // Basically left shift by 4 bits and return value of read/write mask.
                return DRF_NUM(_PFB, _PRI_MMU_VPR_INFO, _DATA, val);
            }
            else if (index == LW_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_LO ||
                     index == LW_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_HI)
            {
                 return val;
            }
            else
            {
                //
                // If we are here, then the coder needs to be made aware of the fact that
                // the index they are trying to use should be added to one of the if/else
                // conditions above, otherwise this function does not support that index.L
                // Error report and halt so this is brought to immediate attention.
                //
                selwrescrubReportStatus_HAL(pSelwrescrub, SELWRESCRUB_RC_ILWALID_INDEX, __LINE__);
                falc_halt();
            }
        }
    }

    // Code will not come here
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
void
selwrescrubWriteVprWprData_GP10X
(
    LwU32 index,
    LwU32 data
)
{
    LwU32 cmd = 0;

    if (index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_LO      ||
        index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_HI      ||
        index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR_ALLOW_READ  ||
        index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR_ALLOW_WRITE ||
        index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_PROTECTED_MODE)
    {
        cmd = data;
    }
    else if (index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_LO  ||
             index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_HI  ||
             index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR1_ADDR_LO ||
             index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR1_ADDR_HI ||
             index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR2_ADDR_LO ||
             index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR2_ADDR_HI ||
             index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_LOCK_ADDR_LO ||
             index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_LOCK_ADDR_HI)
    {
        cmd = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _DATA, data, cmd);
    }
    else
    {
        //
        // If we are here, then the coder needs to be made aware of the fact that
        // the index they are trying to use should be added to one of the if/else
        // conditions above, otherwise this function does not support that index.
        // Error report and halt so this is brought to immediate attention.
        //
        selwrescrubReportStatus_HAL(pSelwrescrub, SELWRESCRUB_RC_ILWALID_INDEX, __LINE__);
        falc_halt();
    }

    cmd = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _INDEX, index, cmd);

    FALC_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_WPR_WRITE, cmd);
}

/*!
 * @brief: Set/Unset VPR_IN_USE bit which enables VPR range check in MMU
 */
void selwrescrubSetVprState_GP10X
(
    LwBool bEnableVpr
)
{
    LwU32 cyaLo = selwrescrubReadVprInfo_HAL(pSelwrescrub,LW_PFB_PRI_MMU_VPR_INFO_INDEX_CYA_LO);
    cyaLo = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_IN_USE, !!bEnableVpr, cyaLo);
    selwrescrubWriteVprWprData_HAL(pSelwrescrub, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_LO, cyaLo);
}

/*!
 * @brief: Returns VPR state in MMU
 */
LwBool
selwrescrubGetVprState_GP10X(void)
{
    return !!DRF_VAL(_PFB, _PRI_MMU_VPR_INFO, _CYA_LO_IN_USE, selwrescrubReadVprInfo_HAL(pSelwrescrub,LW_PFB_PRI_MMU_VPR_INFO_INDEX_CYA_LO));
}
/*!
 * @brief: Query VPR information in MMU
 */
void
selwrescrubGetSetVprMmuSettings_GP10X
(
    PVPR_ACCESS_CMD pVprCmd
)
{
    switch(pVprCmd->cmd)
    {
        case VPR_SETTINGS_GET:
            pVprCmd->vprRangeStartInBytes = ((LwU64)selwrescrubReadVprInfo_HAL(pSelwrescrub,LW_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_LO))<<LW_PFB_PRI_MMU_VPR_INFO_ADDR_ALIGNMENT;
            pVprCmd->vprRangeEndInBytes   = ((LwU64)selwrescrubReadVprInfo_HAL(pSelwrescrub, LW_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_HI))<<LW_PFB_PRI_MMU_VPR_INFO_ADDR_ALIGNMENT;
            pVprCmd->vprRangeEndInBytes  |= ((NUM_BYTES_IN_1_MB) - 1);
            pVprCmd->bVprEnabled   = selwrescrubGetVprState_HAL();
        break;

        case VPR_SETTINGS_SET:
            selwrescrubWriteVprWprData_HAL(pSelwrescrub, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_LO,
                    (LwU32)(pVprCmd->vprRangeStartInBytes>>LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT));
            selwrescrubWriteVprWprData_HAL(pSelwrescrub, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_HI,
                    (LwU32)(pVprCmd->vprRangeEndInBytes>>LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT));
            selwrescrubSetVprState_HAL(pSelwrescrub, pVprCmd->bVprEnabled);
        break;
    }
}

/*!
 * @brief: Check if the VPR range is write-locked with appropriate protection - WPR2/memlock range.
 */
SELWRESCRUB_RC
selwrescrubValidateWriteProtectionIsSetOlwPRRange_GP10X
(
    LwU64   vprStartAddressInBytes,
    LwU64   vprEndAddressInBytes,
    LwBool *pIsWpr2ProtectionAppliedToVPR,
    LwBool *pIsMemLockRangeProtectionAppliedToVPR
)
{
    if (pIsWpr2ProtectionAppliedToVPR           == NULL ||
        pIsMemLockRangeProtectionAppliedToVPR   == NULL)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    LwU32   wprWriteMask            = selwrescrubReadWprInfo_HAL(pSelwrescrub, LW_PFB_PRI_MMU_WPR_INFO_INDEX_ALLOW_WRITE);
    LwU64   wpr2StartAddressInBytes = ((LwU64)selwrescrubReadWprInfo_HAL(pSelwrescrub, LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR2_ADDR_LO)) << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;
    LwU64   wpr2EndAddressInBytes   = ((LwU64)selwrescrubReadWprInfo_HAL(pSelwrescrub, LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR2_ADDR_HI)) << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;
    wpr2EndAddressInBytes  |= ((NUM_BYTES_IN_128_KB) - 1);

    //
    // Check WPR2 protection. WPR2 addresses should match exactly to VPR addresses.
    // WPR2 write protection should not be set for any type of clients
    //
    if (vprStartAddressInBytes == wpr2StartAddressInBytes &&
        vprEndAddressInBytes   == wpr2EndAddressInBytes   &&
        FLD_TEST_DRF(_PFB, _PRI_MMU_VPR_WPR_WRITE, _ALLOW_WRITE_WPR2_SELWRE3, _NO, wprWriteMask) &&
        FLD_TEST_DRF(_PFB, _PRI_MMU_VPR_WPR_WRITE, _ALLOW_WRITE_WPR2_SELWRE2, _NO, wprWriteMask) &&
        FLD_TEST_DRF(_PFB, _PRI_MMU_VPR_WPR_WRITE, _ALLOW_WRITE_WPR2_SELWRE1, _NO, wprWriteMask) &&
        FLD_TEST_DRF(_PFB, _PRI_MMU_VPR_WPR_WRITE, _ALLOW_WRITE_WPR2_SELWRE0, _NO, wprWriteMask))
    {
        *pIsWpr2ProtectionAppliedToVPR = LW_TRUE;
    }
    else
    {
        *pIsWpr2ProtectionAppliedToVPR = LW_FALSE;
    }

    LwU64   memlockStartAddressInBytes = ((LwU64)selwrescrubReadWprInfo_HAL(pSelwrescrub, LW_PFB_PRI_MMU_WPR_INFO_INDEX_LOCK_ADDR_LO)) << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;
    LwU64   memlockEndAddressInBytes   = ((LwU64)selwrescrubReadWprInfo_HAL(pSelwrescrub, LW_PFB_PRI_MMU_WPR_INFO_INDEX_LOCK_ADDR_HI)) << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;
    memlockEndAddressInBytes |= ((NUM_BYTES_IN_4_KB) - 1);

    if (vprStartAddressInBytes == memlockStartAddressInBytes &&
        vprEndAddressInBytes   == memlockEndAddressInBytes)
    {
        *pIsMemLockRangeProtectionAppliedToVPR = LW_TRUE;
    }
    else
    {
        *pIsMemLockRangeProtectionAppliedToVPR = LW_FALSE;
    }

    if (!*pIsMemLockRangeProtectionAppliedToVPR && !*pIsWpr2ProtectionAppliedToVPR)
    {
        //
        // If none of the write protection mechanism was found correctly applied, that is an security issue.
        // We should return error so caller bails early without releasing VPR range.
        //
        return SELWRESCRUB_RC_NO_WRITE_PROTECTION_FOUND_ON_VPR;
    }

    return SELWRESCRUB_RC_OK;
}

/*!
 * @brief: Remove write protection as requested - WPR2/memlock range.
 */
SELWRESCRUB_RC
selwrescrubRemoveWriteProtection_GP10X
(
    LwBool  bRemoveWpr2WriteProtection,
    LwBool  bRemoveMemLockRangeProtection
)
{
    if (bRemoveWpr2WriteProtection)
    {
        selwrescrubWriteVprWprData_HAL(pSelwrescrub, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR2_ADDR_LO, ILWALID_WPR_ADDR_LO);
        selwrescrubWriteVprWprData_HAL(pSelwrescrub, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR2_ADDR_HI, ILWALID_WPR_ADDR_HI);
    }

    if (bRemoveMemLockRangeProtection)
    {
        selwrescrubWriteVprWprData_HAL(pSelwrescrub, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_LOCK_ADDR_LO, ILWALID_MEMLOCK_RANGE_ADDR_LO);
        selwrescrubWriteVprWprData_HAL(pSelwrescrub, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_LOCK_ADDR_HI, ILWALID_MEMLOCK_RANGE_ADDR_HI);
    }

    return SELWRESCRUB_RC_OK;
}


/*!
* @brief: Validate VPR has been scrubbed by HW scrubber. Scrub was started by devinit.
*/
SELWRESCRUB_RC
selwrescrubValidateVprHasBeenScrubbed_GP10X
(
    LwU64 vprStartAddressInBytes,
    LwU64 vprEndAddressInBytes
)
{
    LwU32   vprStartAddr4KAligned  = (LwU32)(vprStartAddressInBytes >> LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT);
    LwU32   vprEndAddr4KAligned    = (LwU32)(vprEndAddressInBytes   >> LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT);
    LwBool  bScrubberDone       = LW_FALSE;

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
        // If start address itself does not contain VPR region, scrubber has been not been correctly started to cover VPR range. Return error.
        return SELWRESCRUB_RC_HW_SCRUBBER_ILWALID_STATE;
    }

    return SELWRESCRUB_RC_OK;
}

SELWRESCRUB_RC
selwrescrubProgramDecodeTrapsForFECS_GP10X(void)
{

    //
    // TRAP #1 to escalate FECS to level 3 for reading or writing to LW_PGRAPH_PRI_GPCS_MMU_VPR_WPR_WRITE.
    // Escalation for read is not necessary since we don't need read protection.
    //

    // Write the register offset to TRAP10_MATCH
    FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP10_MATCH, LW_PGRAPH_PRI_GPCS_MMU_VPR_WPR_WRITE);

    //
    // Use SUBID = 0xF to match all SUBIDs from 0 through 0xF which can be used by pri sequencer.
    // The pri_sequencer (FECS as well as GPCCS) can both use SUBIDs 0x0 through 0xf and no one else can use these SUBIDs.
    // However, FECS pri_sequencer is connected as an input to the SYS DECODE_TRAPs, and GPCCS pri_sequencer is connected
    // as an input to the GPC DECODE_TRAPs.  GPCCS can not generate transactions that would go through the SYS DECODE_TRAPs.
    // So, SUBID = 0xF in *SYS*_DECODE_TRAP means that this trap will match only FECS pri sequencer
    //
    REG_WR_DRF_NUM(BAR0, _PPRIV, _SYS_PRI_DECODE_TRAP10_MASK, _SUBID, 0xF);

    // The action for this decode trap is to set the priv level of the transaction.
    REG_WR_DRF_DEF(BAR0, _PPRIV, _SYS_PRI_DECODE_TRAP10_ACTION, _SET_PRIV_LEVEL, _ENABLE);

    FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP10_PRIV_LEVEL_MASK,
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP10_PRIV_LEVEL_MASK, _CONTROL_READ_PROTECTION_LEVEL0, _ENABLE)   | /* read protection all levels enabled */
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP10_PRIV_LEVEL_MASK, _CONTROL_READ_PROTECTION_LEVEL1, _ENABLE)   |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP10_PRIV_LEVEL_MASK, _CONTROL_READ_PROTECTION_LEVEL2, _ENABLE)   |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP10_PRIV_LEVEL_MASK, _CONTROL_READ_VIOLATION, _REPORT_ERROR)     |

                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP10_PRIV_LEVEL_MASK, _CONTROL_WRITE_PROTECTION_LEVEL0, _DISABLE) | /* write protection all levels disabled */
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP10_PRIV_LEVEL_MASK, _CONTROL_WRITE_PROTECTION_LEVEL1, _DISABLE) |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP10_PRIV_LEVEL_MASK, _CONTROL_WRITE_PROTECTION_LEVEL2, _DISABLE) |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP10_PRIV_LEVEL_MASK, _CONTROL_WRITE_VIOLATION, _REPORT_ERROR)    |

                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP10_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL0, _DISABLE)         |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP10_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL1, _DISABLE)         |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP10_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL2, _ENABLE)          | /* trap application only level2 enabled */
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP10_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL3, _DISABLE));

    // Set target PLM, escalate to Level 3.
    LwU32 targetPLM = 0x3;
    REG_WR_DRF_NUM(BAR0, _PPRIV, _SYS_PRI_DECODE_TRAP10_DATA1, _PRIV_LEVEL, targetPLM);


    //
    // TRAP #2 to escalate FECS to level 3 for reading or writing to LW_PFB_PRI_MMU_VPR_WPR_WRITE.
    // Escalation for read is not necessary since we don't need read protection.
    //
    // Write the register offset to TRAP11_MATCH
    FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP11_MATCH, LW_PFB_PRI_MMU_VPR_WPR_WRITE);

    //
    // Use SUBID = 0xF to match all SUBIDs from 0 through 0xF which can be used by pri sequencer.
    // The pri_sequencer (FECS as well as GPCCS) can both use SUBIDs 0x0 through 0xf and no one else can use these SUBIDs.
    // However, FECS pri_sequencer is connected as an input to the SYS DECODE_TRAPs, and GPCCS pri_sequencer is connected
    // as an input to the GPC DECODE_TRAPs.  GPCCS can not generate transactions that would go through the SYS DECODE_TRAPs.
    // So, SUBID = 0xF in *SYS*_DECODE_TRAP means that this trap will match only FECS pri sequencer
    //
    REG_WR_DRF_NUM(BAR0, _PPRIV, _SYS_PRI_DECODE_TRAP11_MASK, _SUBID, 0xF);

    // The action for this decode trap is to set the priv level of the transaction.
    REG_WR_DRF_DEF(BAR0, _PPRIV, _SYS_PRI_DECODE_TRAP11_ACTION, _SET_PRIV_LEVEL, _ENABLE);

    FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP11_PRIV_LEVEL_MASK,
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP11_PRIV_LEVEL_MASK, _CONTROL_READ_PROTECTION_LEVEL0, _ENABLE)   | /* read protection all levels enabled */
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP11_PRIV_LEVEL_MASK, _CONTROL_READ_PROTECTION_LEVEL1, _ENABLE)   |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP11_PRIV_LEVEL_MASK, _CONTROL_READ_PROTECTION_LEVEL2, _ENABLE)   |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP11_PRIV_LEVEL_MASK, _CONTROL_READ_VIOLATION, _REPORT_ERROR)     |

                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP11_PRIV_LEVEL_MASK, _CONTROL_WRITE_PROTECTION_LEVEL0, _DISABLE) | /* write protection all levels disabled */
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP11_PRIV_LEVEL_MASK, _CONTROL_WRITE_PROTECTION_LEVEL1, _DISABLE) |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP11_PRIV_LEVEL_MASK, _CONTROL_WRITE_PROTECTION_LEVEL2, _DISABLE) |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP11_PRIV_LEVEL_MASK, _CONTROL_WRITE_VIOLATION, _REPORT_ERROR)    |

                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP11_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL0, _DISABLE)         |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP11_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL1, _DISABLE)         |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP11_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL2, _ENABLE)          | /* trap application only level2 enabled */
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP11_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL3, _DISABLE));

    // Set target PLM, escalate to Level 3.
    REG_WR_DRF_NUM(BAR0, _PPRIV, _SYS_PRI_DECODE_TRAP11_DATA1, _PRIV_LEVEL, targetPLM);


    //
    // No security related need to read back these registers and verifying they got written properly.
    // There are essentially lowering security so FECS can write to the PFB and PGRAPH versions of VPR_WPR_WRITE register.
    // If due to any hacking attempt these writes are dropped, that would mean FECS won't be able to write to these registers to
    // clear protected mode. That will leave the chip in a locked state and would be a DOS attack on the hacker's own machine.
    // So we should be fine here without reading these back and verifying.
    //

    return SELWRESCRUB_RC_OK;
}

