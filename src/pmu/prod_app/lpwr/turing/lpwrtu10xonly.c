/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 - 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwrtu10xonly.c
 * @brief  PMU Lowpower Object
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objfifo.h"
#include "pmu_objlpwr.h"

#include "config/g_lpwr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------  Public Functions  ------------------------------ */

/*!
 * @brief Function to return the GPU Engine's Idle Mask
 *
 * @param[in] pIdleMask Pointer to the array for idleMask
 *
 * This function will return the various GPU Engine's Idle
 * mask. We can use these idle mask to check if corresponding
 * GPU engine's are idle or not.
 */
void
lpwrGpuIdleMaskGet_TU10X
(
    LwU32 *pIdleMask
)
{
    LwU32 im0 = 0;
    LwU32 im1 = 0;
    LwU32 im2 = 0;

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

    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
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

    pIdleMask[0] = im0;
    pIdleMask[1] = im1;
    pIdleMask[2] = im2;
}
/* ------------------------ Private Functions  ------------------------------ */
