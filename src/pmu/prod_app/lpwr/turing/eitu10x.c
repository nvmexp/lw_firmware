/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   eitu10x.c
 * @brief  Chip specific functions for EI features.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objei.h"
#include "pmu_objfifo.h"
#include "config/g_ei_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Function to set Idle mask for EI - Engine Idle FSM
 */
void
eiIdleMaskInit_TU10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_EI);
    LwU32       im0      = 0;
    LwU32       im1      = 0;
    LwU32       im2      = 0;

    if (pPgState == NULL)
    {
        PMU_BREAKPOINT();
        return;
    }

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE3))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_3, _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE4))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWDEC, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC1))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC1, _ENABLED, im2);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC2))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC2, _ENABLED, im2);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC1, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _SEC, _ENABLED, im0);
    }

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
    pPgState->idleMask[2] = im2;

    // Update all the Idle signal mask with default values
    Ei.idleMaskNonGr[0] =  pPgState->idleMask[0];
    Ei.idleMaskNonGr[1] =  pPgState->idleMask[1];
    Ei.idleMaskNonGr[2] =  pPgState->idleMask[2];

    // Override the Non Gr Idle signals
    Ei.idleMaskNonGr[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR, _DISABLED,
                                      Ei.idleMaskNonGr[0]);
    Ei.idleMaskNonGr[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _DISABLED,
                                      Ei.idleMaskNonGr[0]);
    Ei.idleMaskNonGr[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _DISABLED,
                                      Ei.idleMaskNonGr[0]);
}
