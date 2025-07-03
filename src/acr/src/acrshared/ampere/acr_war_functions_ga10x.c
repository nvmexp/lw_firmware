/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_war_functions_ga10x.c  
 */
#include "acr.h"
#include "dev_timer.h"
#include "dev_gc6_island.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"
#include "dev_gc6_island_addendum.h"

/*
 * @brief: Upgrade PLM of PTIMER register during ACR load
 */
ACR_STATUS
acrProtectHostTimerRegisters_GA10X(LwBool bProtect)
{
    if (bProtect)
    {
        LwU32 hostPtimerPlm = ACR_REG_RD32(BAR0, LW_PTIMER_TIME_PRIV_LEVEL_MASK);
        LwU32 sciPtimerPlm  = ACR_REG_RD32(BAR0, LW_PGC6_SCI_SEC_TIMER_PRIV_LEVEL_MASK);
        LwU32 pTimer0, pTimer1;

        if (FLD_TEST_DRF(_PTIMER, _TIME_PRIV_LEVEL_MASK, _WRITE_PROTECTION,
                _ONLY_LEVEL3_ENABLED, hostPtimerPlm))
        {
            // Early return if PTIMER is already protected. i.e. only level 3 writable
            return ACR_OK;
        }

        // During ACR load set HOST PTIMER plm so only level 3 falcon can access.
        hostPtimerPlm = FLD_SET_DRF(_PTIMER, _TIME_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, hostPtimerPlm);
        hostPtimerPlm = FLD_SET_DRF(_PTIMER, _TIME_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, hostPtimerPlm);
        hostPtimerPlm = FLD_SET_DRF(_PTIMER, _TIME_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, hostPtimerPlm);
        hostPtimerPlm = FLD_SET_DRF(_PTIMER, _TIME_PRIV_LEVEL_MASK, _WRITE_VIOLATION, _REPORT_ERROR, hostPtimerPlm);

        // During ACR load set SCI SEC TIMER plm so only level 3 falcon can access.
        sciPtimerPlm = FLD_SET_DRF(_PGC6, _SCI_SEC_TIMER_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, sciPtimerPlm);
        sciPtimerPlm = FLD_SET_DRF(_PGC6, _SCI_SEC_TIMER_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, sciPtimerPlm);
        sciPtimerPlm = FLD_SET_DRF(_PGC6, _SCI_SEC_TIMER_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, sciPtimerPlm);
        sciPtimerPlm = FLD_SET_DRF(_PGC6, _SCI_SEC_TIMER_PRIV_LEVEL_MASK, _WRITE_VIOLATION, _REPORT_ERROR, sciPtimerPlm);

        ACR_REG_WR32(BAR0, LW_PTIMER_TIME_PRIV_LEVEL_MASK, hostPtimerPlm);
        ACR_REG_WR32(BAR0, LW_PGC6_SCI_SEC_TIMER_PRIV_LEVEL_MASK, sciPtimerPlm);

        pTimer0 = ACR_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);
        pTimer1 = ACR_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER1);

        //
        // Field of _NSEC of SCI SEC timer 0 and host ptimer 0 are with the same
        // width and offset except first 5 bits are wired to 0 in host ptimer
        // So we have to set _UPDATE to _TRIGGER to update the SCI SEC timer
        // #define LW_PGC6_SCI_SEC_TIMER_TIME_0_UPDATE                             0:0 /* RWIVF */
        // #define LW_PGC6_SCI_SEC_TIMER_TIME_0_UPDATE_DONE                 0x00000000 /* R-I-V */
        // #define LW_PGC6_SCI_SEC_TIMER_TIME_0_UPDATE_NOOP                 0x00000000 /* -W--V */
        // #define LW_PGC6_SCI_SEC_TIMER_TIME_0_UPDATE_TRIGGER              0x00000001 /* -W--V */
        //
        pTimer0 = FLD_SET_DRF(_PGC6, _SCI_SEC_TIMER_TIME_0, _UPDATE, _TRIGGER, pTimer0);

        //
        // SCI_TIMER_TIME_1 has to occur first, see dev_gc6_island.ref
        // Also the write has to be atomic
        //
        ACR_REG_WR32(BAR0, LW_PGC6_SCI_SEC_TIMER_TIME_1, pTimer1);
        ACR_REG_WR32(BAR0, LW_PGC6_SCI_SEC_TIMER_TIME_0, pTimer0);
    }
    else
    {
        LwU32 hostPtimerPlm = ACR_REG_RD32(BAR0, LW_PTIMER_TIME_PRIV_LEVEL_MASK);

        //
        // During ACR unload set host ptimer plm to default value.
        // Keep SCI sec timer as protected across GC6
        //
        hostPtimerPlm = FLD_SET_DRF(_PTIMER, _TIME_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _DEFAULT_PRIV_LEVEL, hostPtimerPlm);
        hostPtimerPlm = FLD_SET_DRF(_PTIMER, _TIME_PRIV_LEVEL_MASK, _WRITE_VIOLATION, _REPORT_ERROR, hostPtimerPlm);
        ACR_REG_WR32(BAR0, LW_PTIMER_TIME_PRIV_LEVEL_MASK, hostPtimerPlm);
    }

    return ACR_OK;
}

ACR_STATUS
acrGetUsableFbSizeInMB_GA10X(LwU64 *pUsableFbSizeMB)
{
    LwU32 data = 0;
    
    if (pUsableFbSizeMB == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    
    // Get Usable FB  size 
    data = ACR_REG_RD32( BAR0, LW_USABLE_FB_SIZE_IN_MB);
    data = DRF_VAL(_USABLE, _FB_SIZE_IN_MB, _VALUE, data);

    *pUsableFbSizeMB = (LwU64)data;
    return ACR_OK;
}

