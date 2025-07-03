/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msgp10x.c
 * @brief  HAL-interface for the GP10X Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "hwproject.h"
#include "dev_master.h"
#include "dev_bus.h"
#include "dev_timer.h"
#include "dev_pbdma.h"
#include "dev_top.h"
#include "dev_disp.h"
#include "dev_perf.h"
#include "dev_hdacodec.h"
#include "dev_pwr_pri.h"
#include "dev_fuse.h"
#include "dev_pmgr.h"
#include "mscg_wl_bl_init.h"
#include "dev_sec_pri.h"
#include "dev_pwr_csb.h"
#include "dev_top.h"
#include "dev_trim.h"
#include "dev_sec_pri.h"
#include "dev_fbpa.h"
#include "dev_therm.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objms.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"
#include "pmu_objfifo.h"
#include "pmu_objic.h"
#include "pmu_bar0.h"
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
 * @brief Initializes the Holdoff mask for MS engine.
 */
void
msHoldoffMaskInit_GP10X(void)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU32       hm       = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_GR));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE0));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE1));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE2));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE3))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE3));
    }
    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_BSP));
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_ENC0));
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC1))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_ENC1));
    }
    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_SEC2));
    }

    pMsState->holdoffMask = hm;
}

/*!
 * @brief Disable (mask) the SEC2 PRIV blocker interrupt to PMU
 */
void
msSec2PrivBlockerIntrDisable_GP10X(void)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    if (!LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, SEC2))
    {
        // Do nothing if SEC2 RTOS isn't supported
        return;
    }

    REG_FLD_WR_DRF_DEF(CSB, _CPWR_PMU, _GPIO_1_INTR_RISING_EN,
               _SEC_PRIV_BLOCKER, _DISABLED);
}

/*!
 * @brief Enable (unmask) the SEC2 PRIV blocker interrupt to PMU
 */
void
msSec2PrivBlockerIntrEnable_GP10X(void)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    if (!LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, SEC2))
    {
        // Do nothing if SEC2 RTOS isn't supported
        return;
    }

    REG_FLD_WR_DRF_DEF(CSB, _CPWR_PMU, _GPIO_1_INTR_RISING_EN,
               _SEC_PRIV_BLOCKER, _ENABLED);
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
void msInitActiveFbios_GP10X(void)
{
    LwU32     fbioFloorsweptMask;
    LwU32     fbioValidMask;
    LwU32     val;
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    val = fuseRead(LW_FUSE_STATUS_OPT_FBIO);

    fbioFloorsweptMask =
        DRF_VAL(_FUSE, _STATUS_OPT_FBIO, _DATA, val);

    fbioValidMask =
        BIT(REG_RD32(BAR0, LW_PTOP_SCAL_NUM_FBPAS)) - 1;

    pSwAsr->regs.mmActiveFBIOs =
        (~fbioFloorsweptMask) & fbioValidMask;
}

/*!
 * @brief Initialize FB parameters
 */
void
msInitFbParams_GP10X(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     val;

    val = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST);

    //
    // On Pascal we support both GDDR5 and GDDR5x memory types. Therefor setting
    // this varible if memory type is either GDDR5 or GDDR5x.
    //
    // Also we have same SW ASR sequence for both GDDR5 and GDDR5X.
    //
    pSwAsr->bIsGDDRx = (FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _DDR_MODE,
                                     _GDDR5, val) ||
                        FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _DDR_MODE,
                                     _GDDR5X, val));

    pSwAsr->bIsGDDR3MirrorCmdMapping =
        (FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _CMD_MAPPING,
                      _GDDR3_GT215_COMP_MIRR, val )             |
         FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _CMD_MAPPING,
                      _GDDR3_GT215_COMP_MIRR1, val));

    if (PMUCFG_FEATURE_ENABLED(PMU_MS_HALF_FBPA))
    {
        // Determine if Half-FBPA is enabled on the system
        msInitHalfFbpaMask_HAL();
    }
}

/*!
 * @brief Initialize Half FBPA Mask
 */
void msInitHalfFbpaMask_GP10X(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    // Determine if Half-FBPA is enabled on the system
    pSwAsr->halfFbpaMask = REG_RD32(FECS, LW_FUSE_STATUS_OPT_HALF_FBPA);
}

/*!
 * @brief Check for SEC2 wake-ups
 *
 * As a part of WAR 200089154, SEC2 sets _IDLE_STATUS_1_SW1 to _BUSY to
 * wake MSCG.
 *
 * @return LW_TRUE      SEC2 wakeup pending
 *         LW_FALSE     Otherwise
 */
LwBool
msSec2WakeupPending_GP10X(void)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS_1);

    if (FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS_1, _SW1, _BUSY, val))
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}

/*!
 * @brief Disengage the SEC2 priv and FB blockers
 */
void
msSec2PrivFbBlockersDisengage_GP10X(void)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU32       val;

    if (!LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, SEC2))
    {
        // If SEC2 RTOS isn't supported, do nothing.
        return;
    }

    val = REG_RD32(FECS, LW_PSEC_FALCON_ENGCTL);

    // Note that this will also clear STALLREQ_MODE
    val = FLD_SET_DRF(_PSEC, _FALCON_ENGCTL, _CLR_STALLREQ,  _TRUE, val);

    REG_WR32(FECS, LW_PSEC_FALCON_ENGCTL, val);

    //
    // Explicitly enable the SEC_PRIV_BLOCKER GPIO_1_INTR interrupt after
    // disengaging the blocker. This function is called during aborts and
    // if the abort point that called this function was before we enabled
    // SEC_PRIV_BLOCKER GPIO_1 then we would want to explicitly enable it
    //
    REG_FLD_WR_DRF_DEF(CSB, _CPWR_PMU, _GPIO_1_INTR_RISING_EN,
               _SEC_PRIV_BLOCKER, _ENABLED);

    // Disengage the Hold off as well
    lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_SEC2_BLK_WAR,
                           LPWR_HOLDOFF_MASK_NONE);
}
