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
 * @file: selwrescrubgp10xonly.c
 */
/* ------------------------- System Includes -------------------------------- */

#include "hwproject.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "selwrescrub.h"
#include "selwrescrubreg.h"
#include "dev_fuse.h"
#include "dev_master.h"
#include "dev_fb.h"
#include "dev_sec_pri.h"
#include "dev_graphics_nobundle.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

/*
 * @brief: updates mutex protection level to 3
 */
SELWRESCRUB_RC
selwrescrubProgramDecodeTrapToUpgradeSec2MutexToLevel3_GP10X(void)
{
    //
    // TRAP to drop writes by level0/1/2 to SEC2 mutex registers
    // so only level3 is allowed to access SEC2 mutex registers
    //

    //
    // The action for this decode trap is to force error return. Until GP10X, writing ACTION causes TRAP_APPLICATION
    // to automatically get set to trap the priv level that is writing this register which means if level 3 is writing
    // this register it will trap even level 3 but we may not want level 3 to be trapped. So, let's write this register first
    //
    REG_WR_DRF_DEF(BAR0, _PPRIV, _SYS_PRI_DECODE_TRAP18_ACTION, _FORCE_ERROR_RETURN, _ENABLE );

    FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP18_PRIV_LEVEL_MASK,
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_PRIV_LEVEL_MASK, _CONTROL_READ_PROTECTION_LEVEL0, _ENABLE)   |  /* read protection all levels enabled */
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_PRIV_LEVEL_MASK, _CONTROL_READ_PROTECTION_LEVEL1, _ENABLE)   |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_PRIV_LEVEL_MASK, _CONTROL_READ_PROTECTION_LEVEL2, _ENABLE)   |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_PRIV_LEVEL_MASK, _CONTROL_READ_VIOLATION, _REPORT_ERROR)     |

                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_PRIV_LEVEL_MASK, _CONTROL_WRITE_PROTECTION_LEVEL0, _DISABLE) |  /* write protection all levels disabled */
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_PRIV_LEVEL_MASK, _CONTROL_WRITE_PROTECTION_LEVEL1, _DISABLE) |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_PRIV_LEVEL_MASK, _CONTROL_WRITE_PROTECTION_LEVEL2, _DISABLE) |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_PRIV_LEVEL_MASK, _CONTROL_WRITE_VIOLATION, _REPORT_ERROR)    |

                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL0, _ENABLE)          |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL1, _ENABLE)          |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL2, _ENABLE)          |  /* trap application level 0 - 2 */
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_PRIV_LEVEL_MASK, _TRAP_APPLICATION_LEVEL3, _DISABLE));

    // Trapping both reads and writes to LW_PSEC_MUTEX_ID/LW_PSEC_MUTEX_ID/LW_PSEC_MUTEX(i) registers from level 0/1/2
    FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP18_MATCH_CFG,
                    DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_MATCH_CFG, _COMPARE_CTL,  _COMPARED) |
                    DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_MATCH_CFG, _IGNORE_READ,  _MATCHED)  |
                    DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP18_MATCH_CFG, _IGNORE_WRITE, _MATCHED));

    // Write the register offset of the first register in the range to be protected to TRAP18_MATCH, i.e. "LW_PSEC_MUTEX_ID".
    FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP18_MATCH, LW_PSEC_MUTEX_ID);

    //
    // Use SUBID = 0x3F to match all SUBIDs.
    // for ADDR, use 0x8C to match addresses until and including 0xXXXXXX8C,
    // 0x8C is the last address to be protected (0x87A48 to 0x87A8C)
    //
    FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP18_MASK,
                    DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP18_MASK, _SUBID, 0x3F) |
                    DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP18_MASK, _ADDR,  0x8C));

    return SELWRESCRUB_RC_OK;
}

/*
 * @brief: Get SW ucode version, the define comes from selwrescrub-profiles.lwmk -> make-profile.lwmk
 */
LwU32
selwrescrubGetSwUcodeVersion_GP10X(LwU32 *pVersionSw)
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
    // Never use "SELWRESCRUB_GP10X_UCODE_BUILD_VERSION" directly, always use the function selwrescrubGetSwUcodeVersion() to get this
    // The define is used directly here in special case where we need compile time assert instead of fetching this at run time
    //
    CT_ASSERT(SELWRESCRUB_GP10X_UCODE_BUILD_VERSION + 3 < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_SCRUBBER_BINARY_VERSION));
    //
    // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
    // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
    //

    if (!pVersionSw)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    *pVersionSw = SELWRESCRUB_GP10X_UCODE_BUILD_VERSION;

    return SELWRESCRUB_RC_OK;
}
