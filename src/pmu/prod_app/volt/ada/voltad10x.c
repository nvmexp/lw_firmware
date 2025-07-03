/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    voltad10x.c
 * @brief   PMU HAL functions related to voltage devices for AD10x+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_chiplet_pwr.h"
#include "dev_pwr_csb.h"
#include "dev_fuse.h"
#include "dev_top.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "volt/voltdev.h"
#include "pmu_objfuse.h"

#include "config/g_volt_private.h"

#define LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_DBG_0(i)   (LW_PCHIPLET_PWR_GPC0_LWVDD_RAM_ASSIST_DBG_0 + (i) * 0x100)

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief  Returns the status of RAM Assist per GPC
 *
 * @param[in]     railType                  Volt Rail type
 * @param[out]    pRamAssistEngagedMask      Mask Indicating which ADCs have RAM Assist Enabled
 *
 * @return FLCN_OK            Ram Assist Status Mask is set successfully.
 */
FLCN_STATUS
voltRailRamAssistEngagedMaskGet_AD10X
(
    LwU8   railType,
    LwU32 *pRamAssistEngagedMask
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       regVal;
    LwU32       gpcMask;
    LwU32       maxGpcCount;
    LwU8        gpcIdx;

    *pRamAssistEngagedMask = 0;

    switch (railType)
    {
        case LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC:
        {
            // Read max GPC count
            maxGpcCount = DRF_VAL(_PTOP, _SCAL_NUM_GPCS, _VALUE,
                            REG_RD32(BAR0, LW_PTOP_SCAL_NUM_GPCS));
        
            //
            // Read GPC mask from fuse.
            // In case LW_FUSE_STATUS_OPT_GPC_IDX_ENABLE is zero below becomes
            // GPC floorswept mask else it is GPC mask.
            //
            gpcMask = fuseRead(LW_FUSE_STATUS_OPT_GPC);
        
            // Ilwert the mask value if 0 is enabled.
#if (LW_FUSE_STATUS_OPT_GPC_IDX_ENABLE == 0)
            gpcMask = ~gpcMask;
#endif
        
            // Trim the mask based on max supported GPCs.
            gpcMask = gpcMask & (LWBIT(maxGpcCount) - 1);

            FOR_EACH_INDEX_IN_MASK(32, gpcIdx, gpcMask)
            {
                regVal = REG_RD32(FECS,
                            LW_PCHIPLET_PWR_GPC_LWVDD_RAM_ASSIST_DBG_0(gpcIdx));
    
                if (FLD_TEST_DRF(_PCHIPLET, _PWR_GPC0_LWVDD_RAM_ASSIST_DBG_0,
                                 _ASSIST_STAT, _ENGAGED, regVal))
                {
                    *pRamAssistEngagedMask |= LWBIT(gpcIdx);
                }
            }
            FOR_EACH_INDEX_IN_MASK_END;
            break;
        }
    
        case LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD:
        {
            regVal = REG_RD32(CSB, LW_CPWR_THERM_MSVDD_RAM_ASSIST_DBG_0);
    
            // 
            // Mask value 0x1 indicates ram assist is enabled 
            // for all controller SRAM cells with MSVDD as source
            //
            if (FLD_TEST_DRF(_CPWR_THERM, _MSVDD_RAM_ASSIST_DBG_0,
                             _ASSIST_STAT, _ENGAGED, regVal))
            {
                *pRamAssistEngagedMask = LWBIT(0);
            }
            break;
        }
    }
    return status;
}
