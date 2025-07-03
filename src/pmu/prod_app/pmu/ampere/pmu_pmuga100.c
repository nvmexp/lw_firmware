/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_pmuga100.c
 * @brief   PMU HAL functions for GA100
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "pmu_bar0.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"

#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Bind PMU context
 */
FLCN_STATUS
pmuCtxBind_GA100
(
    LwU32 ctxBindAddr
)
{
     LwU32 data;

     data = REG_RD32(FECS, LW_PPWR_FALCON_DEBUG1);
     data = FLD_SET_DRF_NUM(_PPWR_FALCON, _DEBUG1, _CTXSW_MODE, 1, data);
     data = FLD_SET_DRF(_PPWR_FALCON, _DEBUG1, _CTXSW_MODE1, _BYPASS_IDLE_CHECKS, data);
     REG_WR32(FECS, LW_PPWR_FALCON_DEBUG1, data);

     data = REG_RD32(FECS, LW_PPWR_FALCON_ITFEN);
     data = FLD_SET_DRF(_PPWR_FALCON, _ITFEN, _CTXEN, _ENABLE, data);
     REG_WR32(FECS, LW_PPWR_FALCON_ITFEN, data);

     REG_WR32(FECS, LW_PPWR_PMU_NEW_INSTBLK, ctxBindAddr);

     do
     {
         data = REG_RD32(FECS, LW_PPWR_FALCON_RSTAT3);

         switch (DRF_VAL(_PPWR_FALCON, _RSTAT3, _CTXSW_STATE, data))
         {
             case LW_PPWR_FALCON_ICD_RDATA_RSTAT3_CTXSW_STATE_SM_SAVE:
                 // Should never hit this case since PMU is coming out of reset.
                 PMU_HALT();
                 break;
             case LW_PPWR_FALCON_ICD_RDATA_RSTAT3_CTXSW_STATE_SM_RESET:
                 // Acknowledge restore of new context.
                 REG_WR32(FECS, LW_PPWR_FALCON_CTXACK,
                     DRF_NUM(_PPWR_FALCON, _CTXACK, _REST_ACK, 1));
                 break;
             default:
                 // NOOP for all other states.
                 break;
         }

    }
    while (FLD_TEST_DRF_NUM(_PPWR_FALCON, _RSTAT3, _CTXSW_IDLE, 0x0, data));

    REG_WR32(FECS, LW_PPWR_FALCON_IRQSCLR,
        DRF_NUM(_PPWR_FALCON, _IRQSCLR, _CTXSW, 1));

    return FLCN_OK;
}
