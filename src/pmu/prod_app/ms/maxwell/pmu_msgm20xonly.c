/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msgm20xonly.c
 * @brief  HAL-interface for the GM20X Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_pri.h"
#include "dev_pwr_csb.h"
#include "dev_master.h"
#include "dev_fb.h"
#include "dev_fuse.h"
#include "dev_top.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objms.h"
#include "pmu_objpmu.h"
#include "pmu_objfifo.h"
#include "pmu_objfuse.h"

#include "dbgprintf.h"
#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
/* ------------------------ Public Functions  ------------------------------- */

/*!
 * @brief Initializes the idle mask for MS engine.
 */
void
msMscgIdleMaskInit_GM20X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU32       im0      = 0;
    LwU32       im1      = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        im0 = (FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR, _ENABLED, im0)     |
                FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED, im0) |
                FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED, im0) |
                FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_BE, _ENABLED, im0));

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _GR_INTR_NOSTALL,
                            _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE0_INTR_NOSTALL,
                          _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE1_INTR_NOSTALL,
                          _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE2_INTR_NOSTALL,
                            _ENABLED, im1);
    }
    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWDEC, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _VD_LWDEC_INTR_NOSTALL,
                          _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _VD_LWENC0_INTR_NOSTALL,
                          _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC1, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _VD_LWENC1_INTR_NOSTALL,
                          _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _SEC, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _SEC_INTR_NOSTALL,
                          _ENABLED, im1);
    }

    im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _MS_NISO_EX_ISO, _ENABLED, im1);

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
}

/*!
 * @brief       Extract Active FBIOs for Mixed-Mode memory configuration.
 *
 * @details     Per-partition registers (LW_PFB_FBPA_<num>) space gets handled
 *              differently from their corresponding broadcast registers. Per-
 *              partition registers get mapped to the corresponding physical FBP
 *              and FBIO. Thus, we must check "Floor Sweeping" status of FBP and
 *              FBIO before accessing these registers.
 *
 * @see         bug 653484
 * @see         https://wiki.lwpu.com/fermi/index.php/Fermi_Floorsweeping#FB_to_FBIO_connections
 */
void msInitActiveFbios_GM20X(void)
{
    LwU32     fbioFloorsweptMask;
    LwU32     fbioValidMask;
    LwU32     val;
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    val = REG_RD32(BAR0, LW_PFB_FBHUB_NUM_ACTIVE_LTCS);
    pSwAsr->bMixedMemDensity = FLD_TEST_DRF(_PFB, _FBHUB_NUM_ACTIVE_LTCS,
                                            _MIXED_MEM_DENSITY, _ON, val);

    val = fuseRead(LW_FUSE_STATUS_OPT_FBIO);
    fbioFloorsweptMask =
        DRF_VAL(_FUSE, _STATUS_OPT_FBIO, _DATA, val);

    fbioValidMask =
        BIT(REG_RD32(BAR0, LW_PTOP_SCAL_NUM_FBPAS)) - 1;

    pSwAsr->regs.mmActiveFBIOs =
        (~fbioFloorsweptMask) & fbioValidMask;
}

/*!
 * Check if host interrupt is pending
 *
 * @return   LW_TRUE  if a host interrupt is pending
 * @return   LW_FALSE otherwise
 */
LwBool
msIsHostIntrPending_GM20X(void)
{
    // for GM20X LW_PMC_INTR__SIZE_1 == 3
    if (REG_FLD_IDX_TEST_DRF_DEF(BAR0, _PMC, _INTR, 0, _PFIFO, _PENDING) ||
        REG_FLD_IDX_TEST_DRF_DEF(BAR0, _PMC, _INTR, 1, _PFIFO, _PENDING) ||
        REG_FLD_IDX_TEST_DRF_DEF(BAR0, _PMC, _INTR, 2, _PFIFO, _PENDING))
    {
        return LW_TRUE;
    }
    return LW_FALSE;
}

/*!
 * @brief Check if any non-stalling interrupt is pending on each engine.
 *
 * @return   LW_TRUE  if a interrupt is pending on any engine
 * @return   LW_FALSE otherwise
 */
LwBool
msIsEngineIntrPending_GM20X(void)
{
    LwBool      bIntrPending = LW_FALSE;
    LwU32       val          = 0;

    val = REG_RD32(BAR0, LW_PMC_INTR(1));
    bIntrPending = (DRF_VAL(_PMC, _INTR, _PGRAPH, val) ||
                    DRF_VAL(_PMC, _INTR, _LWDEC,  val) ||
                    DRF_VAL(_PMC, _INTR, _SEC,    val) ||
                    DRF_VAL(_PMC, _INTR, _CE0,    val) ||
                    DRF_VAL(_PMC, _INTR, _CE1,    val) ||
                    DRF_VAL(_PMC, _INTR, _CE2   , val) ||
                    DRF_VAL(_PMC, _INTR, _LWENC,  val) ||
                    DRF_VAL(_PMC, _INTR, _LWENC1, val));

    return bIntrPending;
}
