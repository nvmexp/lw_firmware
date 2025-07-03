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
 * @file: booter_wpr1_tu10x.c
 */
//
// Includes
//
#include "booter.h"

BOOTER_STATUS
booterCheckIfWpr1IsTornDown_TU10X(void)
{
    LwU32 data = 0;
    LwU32 wpr1Start = 0;
    LwU32 wpr1End   = 0;
    
    //
    // Check WPR1 settings in MMU 
    //
    data = BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_LO);
    wpr1Start = DRF_VAL( _PFB, _PRI_MMU_WPR1_ADDR_LO, _VAL, data);
    wpr1Start = (wpr1Start << LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT);

    data = BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_HI);
    wpr1End = DRF_VAL( _PFB, _PRI_MMU_WPR1_ADDR_HI, _VAL, data);
    wpr1End = (wpr1End << LW_PFB_PRI_MMU_WPR1_ADDR_HI_ALIGNMENT);

    // 
    // If WPR1 start address is less than or equal to the WPR1 end address, 
    // it means that WPR1 is still active, and we should error out.
    //
    if (wpr1Start <= wpr1End)
    {
        return BOOTER_ERROR_NO_WPR;
    }

    data = BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ);
    data = DRF_VAL( _PFB, _PRI_MMU_WPR_ALLOW_READ, _WPR1, data);
    if (data != BOOTER_UNLOCK_READ_MASK)
    {
        return BOOTER_ERROR_NO_WPR;
    }

    data = BOOTER_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE);
    data = DRF_VAL( _PFB, _PRI_MMU_WPR_ALLOW_WRITE, _WPR1, data);
    if (data != BOOTER_UNLOCK_WRITE_MASK)
    {
        return BOOTER_ERROR_NO_WPR;
    }

    return BOOTER_OK;
}
