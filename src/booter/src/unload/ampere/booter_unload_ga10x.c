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
 * @file: booter_unload_ga10x.c
 */
//
// Includes
//
#include "booter.h"

BOOTER_STATUS
booterPreEngineResetSequenceBug200590866_GA10X(void)
{
    BOOTER_STATUS status = BOOTER_OK;
    LwU32  val = 0;
    LwBool bFalconSequence = LW_FALSE;

    // 
    // Skip for fmodel for Bug 2874425. Once fmodel runs are updated, we can revert this
    // It is ok to have such check, because this is to fix functional issue, for security issue we have GA10X ECO in place
    //
    if (FLD_TEST_DRF_DEF(BAR0, _PTOP, _PLATFORM, _TYPE, _FMODEL))
    {
        return BOOTER_OK;
    }

    // Program core select
    val = BOOTER_REG_RD32(BAR0, LW_PGSP_RISCV_BCR_CTRL);
    if (FLD_TEST_DRF(_PRISCV, _RISCV_BCR_CTRL, _VALID, _FALSE, val))
    {
        val = DRF_DEF(_PRISCV, _RISCV_BCR_CTRL, _CORE_SELECT, _FALCON);
        BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BCR_CTRL, val);
    }
    val = BOOTER_REG_RD32(BAR0, LW_PGSP_RISCV_BCR_CTRL);
    if (FLD_TEST_DRF(_PRISCV, _RISCV_BCR_CTRL, _CORE_SELECT, _FALCON, val))
    {
        bFalconSequence = LW_TRUE;
    }

    if(bFalconSequence)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterFalconPreResetSequence_HAL(pBooter));
    }
    else
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterRiscvPreResetSequence_HAL(pBooter));
    }

    return status;
}

