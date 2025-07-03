/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_wpr2_tu10x.c
 */
//
// Includes
//
#include "booter.h"

#define FRTS_SIZE_ALIGNMENT      0xC

static void s_booterLockWpr2ToPl3Only(void) ATTR_OVLY(OVL_NAME);

static void
s_booterLockWpr2ToPl3Only(void)
{
    LwU32 data;
    
    data = BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_READ, _MIS_MATCH0, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_READ, _MIS_MATCH1, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_READ, _MIS_MATCH2, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_READ, _MIS_MATCH3, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_READ, _WPR2_SELWRE0, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_READ, _WPR2_SELWRE1, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_READ, _WPR2_SELWRE2, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_READ, _WPR2_SELWRE3, _YES, data);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ, data);

    data = BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_WRITE, _MIS_MATCH0, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_WRITE, _MIS_MATCH1, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_WRITE, _MIS_MATCH2, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_WRITE, _MIS_MATCH3, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_WRITE, _WPR2_SELWRE0, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_WRITE, _WPR2_SELWRE1, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_WRITE, _WPR2_SELWRE2, _NO, data);
    data = FLD_SET_DRF(_PFB, _PRI_MMU_WPR_ALLOW_WRITE, _WPR2_SELWRE3, _YES, data);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE, data);
}

/*!
 * @brief Checks if FRTS is already set up by FWSEC.
 * @return BOOTER_OK if FRTS is up
 * @return BOOTER_BIN_NOT_SUPPORTED if FRTS doesn't exist for this chip
 */
BOOTER_STATUS
booterCheckIfFrtsIsSetUp_TU10X(LwU64 *pActualFrtsStart, LwU64 *pActualFrtsEnd)
{
    // Re-use ACR readiness check here
    LwU32 regFrtsStatus = BOOTER_REG_RD32(BAR0, LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2);
    if (!FLD_TEST_DRF( _PGC6, _AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2, _ACR_READY, _YES, regFrtsStatus))
    {
        // FWSEC has NOT set up FRTS data in WPR2
        return BOOTER_ERROR_FRTS_INCOMPLETE_OR_MISSING;
    }

    // Sanity check to make sure that size of WPR2 == size of FRTS
    LwU64 wpr2Start = DRF_VAL( _PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO));
    wpr2Start = (wpr2Start << LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT);

    LwU64 wpr2End   = DRF_VAL( _PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI));
    wpr2End = (wpr2End << LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT) + BOOTER_WPR_ALIGNMENT;

    if (wpr2End < wpr2Start)
    {
        // WPR2 is not up!
        return BOOTER_ERROR_FRTS_INCOMPLETE_OR_MISSING;
    }

    LwU64 frtsSize  = LW_PGC6_AON_FRTS_INPUT_WPR_SIZE_SELWRE_SCRATCH_GROUP_03_0_WPR_SIZE_1MB_IN_4K;
    frtsSize = (frtsSize << FRTS_SIZE_ALIGNMENT);

    if ((wpr2End - wpr2Start) != frtsSize)
    {
        // This is unexpected, FWSEC says FRTS is ready but size doesn't match our expectations...
        return BOOTER_ERROR_FRTS_INCOMPLETE_OR_MISSING;
    }

    *pActualFrtsStart = wpr2Start;
    *pActualFrtsEnd = wpr2End;

    // FWSEC has set up FRTS data in WPR2 and the size matches our expectations
    return BOOTER_OK;
}

/*!
 * @brief Extends WPR2 since FRTS is already set up by FWSEC in WPR2.
 */
BOOTER_STATUS
booterExtendWpr2_TU10X(GspFwWprMeta *pWprMeta)
{
    LwU64 gspFwFbStartVal       = (pWprMeta->gspFwWprStart >> LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT);
    LwU32 newWpr2Start          = LwU64_LO32(gspFwFbStartVal);
    LwU32 lwrWpr2Start;

    //
    // FRTS has already brought WPR2 up, with PL0 read/write denied.
    //
    // Here, we lock it down further as we know downstream of Booter, the only
    // consumer of FRTS data (in WPR2) is AHESASC (which copies it to WPR1 but
    // sets up its own sub-WPR to do so and is PL3 itself).
    //
    // So lock it down to PL3 only. We will setup sub-WPRs for Booters
    // and GSP-RM to use.
    //
    s_booterLockWpr2ToPl3Only();

    // Note: FRTS resides at the end of WPR2, so we extend the start
    lwrWpr2Start = BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO);
    lwrWpr2Start = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, newWpr2Start, lwrWpr2Start);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO, lwrWpr2Start);

    return BOOTER_OK;
}

/*!
 * @brief Sets up WPR2 for GSP-RM. FRTS is NOT set up by FWSEC in WPR2.
 */
BOOTER_STATUS
booterSetUpWpr2_TU10X(GspFwWprMeta *pWprMeta)
{
    LwU64 gspFwFbStartVal = (pWprMeta->gspFwWprStart >> LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT);
    LwU32 newWpr2Start    = LwU64_LO32(gspFwFbStartVal);

    LwU64 gspFwFbEndVal   = ((pWprMeta->gspFwWprEnd-1) >> LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT);
    LwU32 newWpr2End      = LwU64_LO32(gspFwFbEndVal);

    LwU32 lwrWpr2End = BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI);
    LwU32 lwrWpr2Start = BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO);

    // Ensure WPR2 is not already up (it shouldn't be if there was no FRTS)
    // Note: not <= as GA100 has an HW bug where WPRs are initialized incorrectly
    if (lwrWpr2Start < lwrWpr2End)
    {
        return BOOTER_ERROR_NO_WPR;
    }

    // Initialize WPR2_ADDR_HI to 0 first before doing anything to ensure no WPR glitching
    lwrWpr2End = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, 0, lwrWpr2End);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI, lwrWpr2End);

    // Then set WPR2_ADDR_LO to what we want (this ensures WPR2 will be down with HI < LO)
    lwrWpr2Start = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, newWpr2Start, lwrWpr2Start);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO, lwrWpr2Start);

    //
    // With WPR2 addrs lwrrently invalid, lock it down to PL3 only.
    // We will setup sub-WPRs for Booters and GSP-RM to use.
    //
    s_booterLockWpr2ToPl3Only();

    // Finally set WPR2_ADDR_HI to what we want
    lwrWpr2End = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, newWpr2End, lwrWpr2End);
    BOOTER_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI, lwrWpr2End);

    return BOOTER_OK;
}


