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
 * @file   pmu_msad10x.c
 * @brief  HAL-interface for the GA10X Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_disp.h"
#include "dev_hshub_SW.h"
#include "mscg_wl_bl_init.h"
#include "dev_sec_pri.h"
#include "dev_gsp.h"
#include "dev_pwr_pri.h"
#include "dev_pwr_csb.h"
#include "dev_smcarb.h"
#include "dev_fbpa.h"
#include "dev_ltc.h"
#include "dev_perf.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objfifo.h"
#include "pmu_objic.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"

#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Static Functions -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
/* ------------------------ Public Functions  ------------------------------- */

/*!
 * @brief Initializes the idle mask for MS engine.
 */
void
msMscgIdleMaskInit_AD10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU32       im[RM_PMU_PG_IDLE_BANK_SIZE] = { 0 };

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        im[0] = (FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR, _ENABLED, im[0])     |
                 FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED, im[0]) |
                 FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED, im[0]));

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_GR, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE0, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE1, im);

    }

    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE2, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE3))
    {
        im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_3, _ENABLED, im[1]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE3, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE4))
    {
        im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im[1]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE4,im);
    }

    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWDEC, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_BSP, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC1))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC1, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_DEC1, im);
    }

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    if (FIFO_IS_ENGINE_PRESENT(DEC2))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC2, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_DEC2, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC3))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC3, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_DEC3, im);
    }
#endif

    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_ENC0, im);
    }

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    if (FIFO_IS_ENGINE_PRESENT(ENC1))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC1, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_ENC1, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC2))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC2, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_ENC2, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(OFA0))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_OFA, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_OFA0, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(JPG0))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWJPG0, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_JPG0, im);
    }
#endif

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _SEC, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_SEC2, im);

        if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, SEC2))
        {
            im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _SEC_FBIF,
                                _ENABLED, im[1]);
            im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _SEC_PRIV_BLOCKER_MS,
                                _ENABLED, im[2]);

        }
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, GSP))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_FB, _ENABLED, im[2]);
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_PRIV_BLOCKER_MS,
                            _ENABLED, im[2]);
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, HSHUB))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HSHUB_NISO, _ENABLED, im[0]);
    }

    // From TU10X, FBHUB_ALL only represents the NISO traffic.
    im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _FBHUB_ALL, _ENABLED, im[1]);

    // Add the XVE Bar Blocker MS Idle signal
    im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _XVE_BAR_BLK_MS, _ENABLED, im[2]);

    pPgState->idleMask[0] = im[0];
    pPgState->idleMask[1] = im[1];
    pPgState->idleMask[2] = im[2];
}

/*!
 * @brief Initializes the idle mask for MS passive.
 */
void
msPassiveIdleMaskInit_AD10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_PASSIVE);
    LwU32       im[RM_PMU_PG_IDLE_BANK_SIZE] = { 0 };

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        im[0] = (FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR, _ENABLED, im[0])     |
                 FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED, im[0]) |
                 FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED, im[0]));

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_GR, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE0, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE1, im);

    }

    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE2, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE3))
    {
        im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_3, _ENABLED, im[1]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE3, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE4))
    {
        im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im[1]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE4,im);
    }

    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWDEC, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_BSP, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC1))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC1, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_DEC1, im);
    }

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    if (FIFO_IS_ENGINE_PRESENT(DEC2))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC2, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_DEC2, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC3))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC3, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_DEC3, im);
    }
#endif

    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_ENC0, im);
    }

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    if (FIFO_IS_ENGINE_PRESENT(ENC1))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC1, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_ENC1, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC2))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC2, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_ENC2, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(OFA0))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_OFA, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_OFA0, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(JPG0))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWJPG0, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_JPG0, im);
    }
#endif

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _SEC, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_SEC2, im);

        if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, SEC2))
        {
            im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _SEC_FBIF,
                                _ENABLED, im[1]);
            im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _SEC_PRIV_BLOCKER_MS,
                                _ENABLED, im[2]);

        }
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, GSP))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_FB, _ENABLED, im[2]);
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_PRIV_BLOCKER_MS,
                            _ENABLED, im[2]);
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, HSHUB))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HSHUB_NISO, _ENABLED, im[0]);
    }

    // FBHUB_ALL only represents the NISO traffic.
    im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _FBHUB_ALL, _ENABLED, im[1]);

    // Add the XVE Bar Blocker MS Idle signal
    im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _XVE_BAR_BLK_MS, _ENABLED, im[2]);

    pPgState->idleMask[0] = im[0];
    pPgState->idleMask[1] = im[1];
    pPgState->idleMask[2] = im[2];
}

/*!
 * @brief Initializes the idle mask for DIFR SW_ASR FSM.
 */
void
msDifrIdleMaskInit_AD10X
(
    LwU8 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       im[RM_PMU_PG_IDLE_BANK_SIZE] = { 0 };

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        im[0] = (FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR, _ENABLED, im[0])     |
                 FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED, im[0]) |
                 FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED, im[0]));

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_GR, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE0, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE1, im);

    }

    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE2, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE3))
    {
        im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_3, _ENABLED, im[1]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE3, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE4))
    {
        im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im[1]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_CE4,im);
    }

    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWDEC, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_BSP, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC1))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC1, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_DEC1, im);
    }

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    if (FIFO_IS_ENGINE_PRESENT(DEC2))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC2, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_DEC2, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(DEC3))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWDEC3, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_DEC3, im);
    }
#endif

    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_ENC0, im);
    }

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    if (FIFO_IS_ENGINE_PRESENT(ENC1))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC1, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_ENC1, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(ENC2))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC2, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_ENC2, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(OFA0))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_OFA, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_OFA0, im);
    }

    if (FIFO_IS_ENGINE_PRESENT(JPG0))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _VD_LWJPG0, _ENABLED, im[2]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_JPG0, im);
    }
#endif

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _SEC, _ENABLED, im[0]);

        // Init the Esched Idle signal for the engine
        lpwrEngineToEschedIdleMask_HAL(&Lpwr, PMU_ENGINE_SEC2, im);

        if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, SEC2))
        {
            im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _SEC_FBIF,
                                _ENABLED, im[1]);
            im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _SEC_PRIV_BLOCKER_MS,
                                _ENABLED, im[2]);

        }
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, GSP))
    {
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_FB, _ENABLED, im[2]);
        im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_PRIV_BLOCKER_MS,
                            _ENABLED, im[2]);
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, HSHUB))
    {
        im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HSHUB_NISO, _ENABLED, im[0]);
    }

    // From TU10X, FBHUB_ALL only represents the NISO traffic.
    im[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _FBHUB_ALL, _ENABLED, im[1]);

    // Add the XVE Bar Blocker MS Idle signal
    im[2] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _XVE_BAR_BLK_MS, _ENABLED, im[2]);

    //
    // Add FBPA Idle signal.
    // To get this signal working, following bit must be enabled:
    // LW_PFB_FBPA_DEBUG_0_FB_IDLE_NOREF_ENABLED. Bug: 200667694
    //
    im[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _FB_PA, _ENABLED, im[0]);

    pPgState->idleMask[0] = im[0];
    pPgState->idleMask[1] = im[1];
    pPgState->idleMask[2] = im[2];
}

/*!
 * @brief Configure the FBPA idle signal to provide only
 * L2 <---> FB Traffic and does not include internal DRAM
 * refersh traffic.
 */
void msFbpaIdleConfig_AD10X(void)
{
    LwU32 regVal = 0;

    regVal = REG_RD32(FECS, LW_PFB_FBPA_DEBUG_0);

    regVal = FLD_SET_DRF(_PFB, _FBPA_DEBUG_0, _FB_IDLE_NOREF, _ENABLED, regVal);
    REG_WR32(FECS, LW_PFB_FBPA_DEBUG_0, regVal);
}

/* ------------------------- Private Functions ------------------------------ */
