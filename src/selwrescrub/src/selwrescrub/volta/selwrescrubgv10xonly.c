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
 * @file: selwrescrubgv10xonly.c
 */
/* ------------------------- System Includes -------------------------------- */

#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "selwrescrub.h"
#include "selwrescrubreg.h"
#include "dev_fuse.h"
#include "dev_master.h"
#include "dev_fb.h"
#include "dev_sec_pri.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

SELWRESCRUB_RC
selwrescrubProgramDecodeTrapsForFECS_GV10X(void)
{

    // Lower the read protection on LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK
    FALC_REG_WR32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK,
                            DRF_DEF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL0, _ENABLE)   |  /* read protection all levels enabled */
                            DRF_DEF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL1, _ENABLE)   |
                            DRF_DEF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL2, _ENABLE)   |
                            DRF_DEF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, _READ_VIOLATION, _REPORT_ERROR)     |

                            DRF_DEF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE) |  /* write protection all levels disabled */
                            DRF_DEF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE) |
                            DRF_DEF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE) |
                            DRF_DEF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK, _WRITE_VIOLATION, _REPORT_ERROR));
    //
    // Escalate FECS to level 3 for writing to LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15(0).
    //

    // Write the register offset to TRAP17_MATCH
    FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP17_MATCH, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15(0));

    //
    // Use SUBID = 0xF to match all SUBIDs from 0 through 0xF which can be used by pri sequencer.
    // The pri_sequencer (FECS as well as GPCCS) can both use SUBIDs 0x0 through 0xf and no one else can use these SUBIDs.
    // However, FECS pri_sequencer is connected as an input to the SYS DECODE_TRAPs, and GPCCS pri_sequencer is connected
    // as an input to the GPC DECODE_TRAPs.  GPCCS can not generate transactions that would go through the SYS DECODE_TRAPs.
    // So, SUBID = 0xF in *SYS*_DECODE_TRAP means that this trap will match only FECS pri sequencer
    //
    REG_WR_DRF_NUM(BAR0, _PPRIV, _SYS_PRI_DECODE_TRAP17_MASK, _SUBID, 0xF);

    // The action for this decode trap is to set the priv level of the transaction.
    REG_WR_DRF_DEF(BAR0, _PPRIV, _SYS_PRI_DECODE_TRAP17_ACTION, _SET_PRIV_LEVEL, _ENABLE);

    // Ignore the trap for reads since we will be lowering the PLM to allow reads to all priv levels
    REG_WR_DRF_DEF(BAR0, _PPRIV, _SYS_PRI_DECODE_TRAP17_MATCH_CFG, _IGNORE_READ, _IGNORED);

    FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP17_PRIV_LEVEL_MASK,
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP17_PRIV_LEVEL_MASK, _CONTROL_READ_PROTECTION_LEVEL0, _ENABLE)   |  /* read protection all levels enabled */
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP17_PRIV_LEVEL_MASK, _CONTROL_READ_PROTECTION_LEVEL1, _ENABLE)   |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP17_PRIV_LEVEL_MASK, _CONTROL_READ_PROTECTION_LEVEL2, _ENABLE)   |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP17_PRIV_LEVEL_MASK, _CONTROL_READ_VIOLATION, _REPORT_ERROR)     |

                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP17_PRIV_LEVEL_MASK, _CONTROL_WRITE_PROTECTION_LEVEL0, _DISABLE) |  /* write protection all levels disabled */
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP17_PRIV_LEVEL_MASK, _CONTROL_WRITE_PROTECTION_LEVEL1, _DISABLE) |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP17_PRIV_LEVEL_MASK, _CONTROL_WRITE_PROTECTION_LEVEL2, _DISABLE) |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP17_PRIV_LEVEL_MASK, _CONTROL_WRITE_VIOLATION, _REPORT_ERROR)    |

                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP17_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL0, _DISABLE)         |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP17_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL1, _DISABLE)         |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP17_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL2, _ENABLE)          |  /* trap application only level2 enabled */
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP17_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL3, _DISABLE));

    // Set target PLM, escalate to Level 3.
    REG_WR_DRF_DEF(BAR0, _PPRIV, _SYS_PRI_DECODE_TRAP17_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3);

    // Call the GP10X version to also install traps for VPR registers for FECS
    return selwrescrubProgramDecodeTrapsForFECS_GP10X();
}

/*
 * @brief: Get SW ucode version, the define comes from selwrescrub-profiles.lwmk -> make-profile.lwmk
 */
LwU32
selwrescrubGetSwUcodeVersion_GV10X(LwU32 *pVersionSw)
{
    //
    // We are expecting here that the SEC2_MUTEX_VPR_SCRATCH mutex has been acquired by the caller
    // of this function so we can directly go and do RMW on LW_PGC6_BSI_VPR_SELWRE_SCRATCH_* registers.
    //
    // Setup a ct_assert to warn in advance when we're close to running out #bits for scrubber version in the secure scratch
    //
    // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
    // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
    //
    // Never use "SELWRESCRUB_GV10X_UCODE_BUILD_VERSION" directly, always use the function selwrescrubGetSwUcodeVersion() to get this
    // The define is used directly here in special case where we need compile time assert instead of fetching this at run time
    //
    CT_ASSERT(SELWRESCRUB_GV10X_UCODE_BUILD_VERSION + 3 < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_SCRUBBER_BINARY_VERSION));
    //
    // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
    // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
    //

    if (!pVersionSw)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    *pVersionSw = SELWRESCRUB_GV10X_UCODE_BUILD_VERSION;

    return SELWRESCRUB_RC_OK;
}

/*!
* @brief selwrescrubGetLwrMemlockRangeInBytes_GV10X:
*        Get the current Mem Lock range
*
* @param[out]  pMemlockStartAddrInBytes   Start Address of Mem Lock
* @param[out]  pMemlockEndAddrInBytes     End Address of Mem Lock
*
*/
SELWRESCRUB_RC
selwrescrubGetLwrMemlockRangeInBytes_GV10X
(
    LwU64 *pMemlockStartAddrInBytes,
    LwU64 *pMemlockEndAddrInBytes
)
{
    SELWRESCRUB_RC status = SELWRESCRUB_RC_OK;

    if (!pMemlockStartAddrInBytes || !pMemlockEndAddrInBytes)
    {
        return SELWRESCRUB_RC_ILWALID_ARGUMENT;
    }

    // Read the Start Address of Mem Lock
    *pMemlockStartAddrInBytes   = ((LwU64)selwrescrubReadWprInfo_HAL(pSelwrescrub, LW_PFB_PRI_MMU_WPR_INFO_INDEX_LOCK_ADDR_LO));
    *pMemlockStartAddrInBytes <<= LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;

    // Read the End Address of Mem Lock
    *pMemlockEndAddrInBytes   = ((LwU64)selwrescrubReadWprInfo_HAL(pSelwrescrub, LW_PFB_PRI_MMU_WPR_INFO_INDEX_LOCK_ADDR_HI));
    *pMemlockEndAddrInBytes <<= LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;
    *pMemlockEndAddrInBytes  |= ((NUM_BYTES_IN_4_KB) - 1);

    return status;
}

/*!
* @brief selwrescrubGetHwScrubberRangeInBytes_GV10X:
*        Get the current HW Scrubber range
*
* @param[out]  pHwScrubberStartAddressInBytes   Start Address of HW Scrubber
* @param[out]  pHwScrubberEndAddressInBytes     End Address of HW Scrubber
*
*/
SELWRESCRUB_RC
selwrescrubGetHwScrubberRangeInBytes_GV10X
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
    *pHwScrubberStartAddressInBytes = (LwU64)regVal << LW_PFB_PRI_MMU_VPR_INFO_ADDR_ALIGNMENT;

    // Read End address of HW Scrubber
    regVal = FALC_REG_RD32(BAR0, LW_PFB_NISO_SCRUB_LAST_ADDR_STATUS);
    *pHwScrubberEndAddressInBytes = (LwU64)regVal << LW_PFB_PRI_MMU_VPR_INFO_ADDR_ALIGNMENT;
    *pHwScrubberEndAddressInBytes|= ((NUM_BYTES_IN_4_KB) - 1);

    return status;
}


