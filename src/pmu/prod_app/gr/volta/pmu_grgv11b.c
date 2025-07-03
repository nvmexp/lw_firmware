
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_grgv11b.c
 * @brief  HAL-interface for the GV11B Graphics Engine.
 */
/* ------------------------- System Includes -------------------------------- */

#include "pmusw.h"
/* ------------------------- Application Includes --------------------------- */

#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_objlpwr.h"

#include "dev_ltc.h"
#include "dev_pwr_csb.h"
#include "dev_graphics_nobundle.h"
#include "dev_graphics_nobundle_addendum.h"

#include "config/g_gr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */

/*! 
 * Max Poll time can be 40 us according to callwlations i.e. 
 * (NUM_ACTIVE_GROUPS * 8(zones present in every partition) * (ZONE_DELAY) * PMU_Utilsclk_cycle)
 * (64 * 8 * 15(max as it is 4 bit value) * (4.9ns)
 */

#define GR_UNIFIED_SEQ_POLL_TIME_US_GV11B    40

/* ------------------------ Private Prototypes ----------------------------- */

static void s_grPgEngageSram_GV11B(void);
static void s_grPgDisengageSram_GV11B(void);
static void s_grPgEngageLogic_GV11B(void);
static void s_grPgDisengageLogic_GV11B(void);
static void s_grPgEngageL2Rppg_GV11B(void);
static void s_grPgDisengageL2Rppg_GV11B(void);
/* ------------------------ Public Functions ------------------------------- */

/*!
 * @brief Power-gate GR engine.
 */
void
grHwPgEngage_GV11B(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_SRAM))
    {
        s_grPgEngageSram_GV11B();
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_LOGIC))
    {
        s_grPgEngageLogic_GV11B();
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_L2RPPG))
    {
        s_grPgEngageL2Rppg_GV11B();
    }
}

/*!
 * @brief Power-ungate GR engine.
 */
void
grHwPgDisengage_GV11B(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_L2RPPG))
    {
        s_grPgDisengageL2Rppg_GV11B();
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_LOGIC))
    {
        s_grPgDisengageLogic_GV11B();
    }

    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG_SRAM))
    {
        s_grPgDisengageSram_GV11B();
    }
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Power-gate GR engine's SRAM.
 */
void
s_grPgEngageSram_GV11B(void)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_RAM_CTRL);
    val = FLD_SET_DRF(_CPWR_PMU, _RAM_CTRL, _GR, _ENTER_SD, val);
    REG_WR32(CSB, LW_CPWR_PMU_RAM_CTRL, val);

    if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                        DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_GR),
                        DRF_DEF(_CPWR, _PMU_RAM_STATUS, _GR, _SD),
                        GR_UNIFIED_SEQ_POLL_TIME_US_GV11B,
                        PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_HALT();
    }
}

/*!
 * @brief Power-ungate GR engine's SRAM.
 */
void
s_grPgDisengageSram_GV11B(void)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_RAM_CTRL);
    val = FLD_SET_DRF(_CPWR_PMU, _RAM_CTRL, _GR, _EXIT, val);
    REG_WR32(CSB, LW_CPWR_PMU_RAM_CTRL, val);

    if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                        DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_GR),
                        DRF_DEF(_CPWR, _PMU_RAM_STATUS, _GR, _POWER_ON),
                        GR_UNIFIED_SEQ_POLL_TIME_US_GV11B,
                        PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_HALT();
    }
}

/*!
 * @brief Power-gate GR engine's LOGIC.
 */
void
s_grPgEngageLogic_GV11B(void)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_LOGIC_CTRL);
    val = FLD_SET_DRF(_CPWR_PMU, _LOGIC_CTRL, _GR, _ENTER_PG, val);
    REG_WR32(CSB, LW_CPWR_PMU_LOGIC_CTRL, val);

    if (!PMU_REG32_POLL_US(LW_CPWR_PMU_LOGIC_STATUS,
                        DRF_SHIFTMASK(LW_CPWR_PMU_LOGIC_STATUS_GR),
                        DRF_DEF(_CPWR, _PMU_LOGIC_STATUS, _GR, _PG),
                        GR_UNIFIED_SEQ_POLL_TIME_US_GV11B,
                        PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_HALT();
    }
}

/*!
 * @brief Power-ungate GR engine's LOGIC.
 */
void
s_grPgDisengageLogic_GV11B(void)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_LOGIC_CTRL);
    val = FLD_SET_DRF(_CPWR_PMU, _LOGIC_CTRL, _GR, _EXIT, val);
    REG_WR32(CSB, LW_CPWR_PMU_LOGIC_CTRL, val);

    if (!PMU_REG32_POLL_US(LW_CPWR_PMU_LOGIC_STATUS,
                        DRF_SHIFTMASK(LW_CPWR_PMU_LOGIC_STATUS_GR),
                        DRF_DEF(_CPWR, _PMU_LOGIC_STATUS, _GR, _POWER_ON),
                        GR_UNIFIED_SEQ_POLL_TIME_US_GV11B,
                        PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_HALT();
    }
}

/*!
 * @brief Power-gate GR engine's L2 NGR.
 */
void
s_grPgEngageL2Rppg_GV11B(void)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_RAM_CTRL);
    val = FLD_SET_DRF(_CPWR_PMU, _RAM_CTRL, _NGR, _ENTER_RPPG, val);
    REG_WR32(CSB, LW_CPWR_PMU_RAM_CTRL, val);

    if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                        DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_NGR),
                        DRF_DEF(_CPWR, _PMU_RAM_STATUS, _NGR, _RPPG),
                        GR_UNIFIED_SEQ_POLL_TIME_US_GV11B,
                        PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_HALT();
    }
}

/*!
 * @brief Power-ungate GR engine's L2 NGR.
 */
void
s_grPgDisengageL2Rppg_GV11B(void)
{
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_RAM_CTRL);
    val = FLD_SET_DRF(_CPWR_PMU, _RAM_CTRL, _NGR, _EXIT, val);
    REG_WR32(CSB, LW_CPWR_PMU_RAM_CTRL, val);

    if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                        DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_NGR),
                        DRF_DEF(_CPWR, _PMU_RAM_STATUS, _NGR, _POWER_ON),
                        GR_UNIFIED_SEQ_POLL_TIME_US_GV11B,
                        PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_HALT();
    }
}

/*!
 * Submits a FECS method
 *
 * @param[in] method       A method to be sent to FECS.
 * @param[in] methodData   Data to be sent along with the method
 * @param[in] pollValue    value to poll on MALBOX(0) for method completion
 */
FLCN_STATUS
grSubmitMethod_GV11B
(
    LwU32 method,
    LwU32 methodData,
    LwU32 pollValue
)
{
    // Clear MAILBOX(0) which is used to poll for completion.
    REG_WR32(FECS, LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0), 0);

    // Set the method data
    REG_WR32(FECS, LW_PGRAPH_PRI_FECS_METHOD_DATA, methodData);

    // Submit Method
    REG_WR32(BAR0, LW_PGRAPH_PRI_FECS_METHOD_PUSH,
        DRF_NUM(_PGRAPH, _PRI_FECS_METHOD_PUSH, _ADR, method));

    //
    // wait until we get a result. This ilwolves polling the MAILBOX(0)
    // register till it sets bit 0
    //
    if (!PMU_REG32_POLL_US(
            USE_FECS(LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0)),
            LW_U32_MAX, pollValue, GR_FECS_SUBMIT_METHOD_TIMEOUT_US,
            PMU_REG_POLL_MODE_BAR0_EQ))
    {
        PMU_BREAKPOINT();
    }

    return FLCN_OK;
}

