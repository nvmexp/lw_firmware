/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_grga10x_only.c
 * @brief  HAL-interface for the GA10X Graphics Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_graphics_nobundle.h"
#include "dev_pri_ringstation_control_sys.h"
#include "dev_pwr_csb.h"
#include "dev_pmgr.h"
#include "dev_top.h"
#include "dev_gc6_island.h"
#include "dev_fuse.h"
#include "dev_sec_pri.h"
#include "dev_sec_pri_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_bar0.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_objfifo.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"
#include "pmu_bar0.h"
#include "pmu/pmumailboxid.h"
#include "config/g_gr_private.h"

/* ------------------------ Private Prototypes ----------------------------- */
static void  s_grRgGpccsUcodeLoad_GA10X(void);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Macros to define GPIO related timeouts
 *
 * TODO : Remove these defines once production API is in place
 */
#define GPIO_MUTEX_ACQUIRE_TIMEOUT_US     200000
#define GPIO_CNTL_TRIGGER_UDPATE_TIME_US  50

/*!
 * @breif Timeout for GPCCS Bootstrapping
 */
#define GR_GPCCS_BOOTSTRAP_TIMEOUT_US_GA10X          10000

/*!
 * @brief Macro to define PGOOD related timeouts
 */
#define PGOOD_TRIGGER_TIMEOUT_US     2000

/*!
 * @brief Initializes the idle mask for GR.
 */
void
grInitSetIdleMasks_GA10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    LwU32       im0      = 0;
    LwU32       im1      = 0;
    LwU32       im2      = 0;

    im0 = DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR,     _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED);

    if (FIFO_IS_ENGINE_PRESENT(CE0) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE1) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE2) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE3) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE3))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_3, _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE4) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE4))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _SEC_PRIV_BLOCKER_GR,
                          _ENABLED, im2);
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, GR, GSP))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_PRIV_BLOCKER_GR,
                          _ENABLED, im2);
    }

    // Add XVE BAR Blocker Idle Signal
    im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _XVE_BAR_BLK_GR, _ENABLED, im2);

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
    pPgState->idleMask[2] = im2;
}

/*!
 * @brief Initializes the idle mask for GR-RG.
 */
void
grRgInitSetIdleMasks_GA10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
    LwU32       im0      = 0;
    LwU32       im1      = 0;
    LwU32       im2      = 0;

    im0 = DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR,     _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED);

    if (FIFO_IS_ENGINE_PRESENT(CE0) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE1) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE2) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE3) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE3))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_3, _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE4) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE4))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _SEC, _ENABLED, im0);
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _SEC_PRIV_BLOCKER_GR,
                          _ENABLED, im2);
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, GR, GSP))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_PRIV_BLOCKER_GR,
                          _ENABLED, im2);
    }

    // Add XVE BAR Blocker Idle Signal
    im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _XVE_BAR_BLK_GR, _ENABLED, im2);

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
    pPgState->idleMask[2] = im2;
}

/*!
 * @brief Initializes the idle mask for GR.
 */
void
grPassiveIdleMasksInit_GA10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PASSIVE);
    LwU32       im0      = 0;
    LwU32       im1      = 0;
    LwU32       im2      = 0;

    im0 = DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR,     _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED);

    if (FIFO_IS_ENGINE_PRESENT(CE0) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE1) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE2) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im0);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE3) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE3))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_3, _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE4) &&
        fifoIsGrCeEngine(PMU_ENGINE_CE4))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE_4, _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _SEC_PRIV_BLOCKER_GR,
                          _ENABLED, im2);
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, GR, GSP))
    {
        im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _GSP_PRIV_BLOCKER_GR,
                          _ENABLED, im2);
    }

    // Add XVE BAR Blocker Idle Signal
    im2 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_2, _XVE_BAR_BLK_GR, _ENABLED, im2);

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
    pPgState->idleMask[2] = im2;
}

/*!
 * @brief Configure entry conditions for GR-RG
 *
 * There are 2 pre-conditions for GR-RG -
 *  1. Lpwr Ctrl EI 
 *  2. TS_PD_READY
 *
 * Assumption : Here we are assuming EI is having the HW idx less than 4.
 *              Thats why PG_ON_TRIGGER_MASK is configured.
 */
void
grRgEntryConditionConfig_GA10X(void)
{
    OBJPGSTATE *pPgState         = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
    LwU32       val;
    LwU8        childFsmHwCtrlId = PG_GET_ENG_IDX(RM_PMU_LPWR_CTRL_ID_GR_RG);

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI) &&
        LPWR_ARCH_IS_SF_SUPPORTED(EI_COUPLED_GR_RG))
    {
        lpwrFsmDependencyInit_HAL(&Lpwr, PG_ENG_IDX_ENG_EI, childFsmHwCtrlId);
    }

    if (LPWR_CTRL_IS_SF_SUPPORTED(pPgState, GR, RG_TS_PD))
    {
        val = REG_RD32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK_2(childFsmHwCtrlId));

        // Set TS_PD_READY as a pre-condition
        val = FLD_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK_2,
                            _TS_PD_READY_AND, _ENABLE, val);

        REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK_2(childFsmHwCtrlId), val);
    }
}

/*!
 * Asserts GPC reset
 *
 * @param[in] bAssert   True to assert, False otherwise
 */
void
grAssertGPCReset_GA10X(LwBool bAssert)
{
    LwU32 gpcReset;

    gpcReset = REG_RD32(BAR0, LW_PGRAPH_PRI_GPCS_GPCCS_CTXSW_RESET_CTL);

    if (bAssert)
    {
        // Assert GPC Engine and Context Reset
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_RESET_CTL,
                               _GPC_ENGINE_RESET, _ENABLED, gpcReset);
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_RESET_CTL,
                               _GPC_CONTEXT_RESET, _ENABLED, gpcReset);
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_RESET_CTL,
                               _ZLWLL_RT_RESET, _ENABLED, gpcReset);
    }
    else
    {
        // De-assert GPC Engine and Context Reset
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_RESET_CTL,
                               _GPC_ENGINE_RESET, _DISABLED, gpcReset);
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_RESET_CTL,
                               _GPC_CONTEXT_RESET, _DISABLED, gpcReset);
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_RESET_CTL,
                               _ZLWLL_RT_RESET, _DISABLED, gpcReset);
    }

    REG_WR32(BAR0, LW_PGRAPH_PRI_GPCS_GPCCS_CTXSW_RESET_CTL, gpcReset);

    // Force read for flush
    REG_RD32(BAR0, LW_PGRAPH_PRI_GPCS_GPCCS_CTXSW_RESET_CTL);
}

/*!
 * Asserts FECS reset
 *
 * @param[in] bAssert   True to assert, False otherwise
 */
void
grAssertFECSReset_GA10X(LwBool bAssert)
{
    LwU32 fecsReset;

    fecsReset = REG_RD32(BAR0, LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL);

    if (bAssert)
    {
        // Assert FECS Engine and Context Reset
        fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                              _SYS_ENGINE_RESET, _ENABLED, fecsReset);
        fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                              _SYS_CONTEXT_RESET, _ENABLED, fecsReset);
    }
    else
    {
        // De-assert FECS Engine and Context Reset
        fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                              _SYS_CONTEXT_RESET, _DISABLED, fecsReset);
        fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                              _SYS_ENGINE_RESET, _DISABLED, fecsReset);
    }

    REG_WR32(BAR0, LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL, fecsReset);

    // Force read for flush
    REG_RD32(BAR0, LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL);
}

/*!
 * @brief  Interface to block the propagation of reset signal to external units
 *
 * Graphics engine is scattered in GPC, FBP and SYS partitions. Asserting GPC reset
 * reset can propagate this signal to FBP and SYS portion of graphics engine.
 * This resets all registers in Graphics-FPB and Graphics-SYS. This disables 
 * BLCG and SLCG which will lead to power loss.
 * To avoid this we need to assert blocker/clamp to restrict the reset to
 * internal units only.
 *
 * @param[in] bAssert   boolean to specify block or unblock
 */
void
grRgExtResetBlock_GA10X(LwBool bBlock)
{
    LwU32 regVal;

    regVal = REG_RD32(CSB, LW_CPWR_PMU_GPCRG_CONTROL);

    if (bBlock)
    {
        // Assert clamp to block reset for external units
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL,
                             _RG_BLOCK_EXT_RESET, _ENABLE, regVal);
    }
    else
    {
        // De-assert clamp to block reset for external units
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL,
                             _RG_BLOCK_EXT_RESET, _DISABLE, regVal);
    }

    REG_WR32_STALL(CSB, LW_CPWR_PMU_GPCRG_CONTROL, regVal);
}

/*!
 * @brief Interface to assert/de-assert full GPC reset
 *
 * We have independent reset controls for various GPC sub-units.
 * Following is the sequence to assert and de-assert the GPC
 * reset (control register - LW_CPWR_PMU_GPCRG_CONTROL)
 *
 * ASSERT reset -
 *   1. Set XV2ALL, HOST2XBAR and HOST2PERFMON reset bits to ENABLE
 *   2. Wait for 4 us
 *   3. Set XV2CLK and CYA_XV2CLK reset bits to ENABLE
 *   4. Wait for 4 us
 *   5. Set RESET2CLK_DIRECT reset bit to ENABLE
 *   6. Wait for 4 us
 *
 * DEASSERT reset -
 *   1. Set RESET2CLK_DIRECT reset bit to DISABLE
 *   2. Wait for 4 us
 *   3. Set XV2CLK and CYA_XV2CLK reset bits to DISABLE
 *   4. Wait for 4 us
 *   5. Set XV2ALL, HOST2XBAR and HOST2PERFMON reset bits to DISABLE 
 *   6. Wait for 4 us
 *
 * NOTE : 
 * After de-assertion, we need to bring the GPC out of reset 
 * as per the new SMC reset scheme. This is because when GPC-RG
 * is engaged, GPC reset control register also goes down and
 * its init state is reset. So after power up, its still in 
 * reset state.
 * This ilwolves the following steps -
 *   1. Copy the PRIV settings by rebroadcasting the virtual
 *      wire information
 *   2. Program GPC Engine reset control to bring GPC out of
 *      reset
 *
 * @param[in] bAssert   boolean to specify assert or de-assert
 */
void
grRgGpcResetAssert_GA10X(LwBool bAssert)
{
    LwU32 regVal;

    if (bAssert)
    {
        // Set XV2ALL, HOST2XBAR and HOST2PERFMON reset bits to ENABLE
        regVal = REG_RD32(CSB, LW_CPWR_PMU_GPCRG_CONTROL);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL_RESET_OVERRIDE,
                             _XV2ALL_RESET, _ENABLE, regVal);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL_RESET_OVERRIDE,
                             _HOST2XBAR_RESET, _ENABLE, regVal);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL_RESET_OVERRIDE,
                             _HOST2PERFMON_RESET, _ENABLE, regVal);
        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPCRG_CONTROL, regVal);
        OS_PTIMER_SPIN_WAIT_NS(4000);

        // Set XV2CLK and CYA_XV2CLK reset bits to ENABLE
        regVal = REG_RD32(CSB, LW_CPWR_PMU_GPCRG_CONTROL);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL_RESET_OVERRIDE,
                             _XV2CLK_RESET, _ENABLE, regVal);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL_RESET_OVERRIDE,
                             _CYA_XV2CLK_RESET, _ENABLE, regVal);
        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPCRG_CONTROL, regVal);
        OS_PTIMER_SPIN_WAIT_NS(4000);

        // Set RESET2CLK_DIRECT reset bit to ENABLE
        regVal = REG_RD32(CSB, LW_CPWR_PMU_GPCRG_CONTROL);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL_RESET_OVERRIDE,
                             _RESET2CLK_DIRECT_RESET, _ENABLE, regVal);
        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPCRG_CONTROL, regVal);
        OS_PTIMER_SPIN_WAIT_NS(4000);
    }
    else
    {
        // Set RESET2CLK_DIRECT reset bit to DISABLE
        regVal = REG_RD32(CSB, LW_CPWR_PMU_GPCRG_CONTROL);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL_RESET_OVERRIDE,
                             _RESET2CLK_DIRECT_RESET, _DISABLE, regVal);
        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPCRG_CONTROL, regVal);
        OS_PTIMER_SPIN_WAIT_NS(4000);

        // Set XV2CLK and CYA_XV2CLK reset bits to DISABLE
        regVal = REG_RD32(CSB, LW_CPWR_PMU_GPCRG_CONTROL);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL_RESET_OVERRIDE,
                             _XV2CLK_RESET, _DISABLE, regVal);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL_RESET_OVERRIDE,
                             _CYA_XV2CLK_RESET, _DISABLE, regVal);
        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPCRG_CONTROL, regVal);
        OS_PTIMER_SPIN_WAIT_NS(4000);

        // Set XV2ALL, HOST2XBAR and HOST2PERFMON reset bits to DISABLE
        regVal = REG_RD32(CSB, LW_CPWR_PMU_GPCRG_CONTROL);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL_RESET_OVERRIDE,
                             _XV2ALL_RESET, _DISABLE, regVal);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL_RESET_OVERRIDE,
                             _HOST2XBAR_RESET, _DISABLE, regVal);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL_RESET_OVERRIDE,
                             _HOST2PERFMON_RESET, _DISABLE, regVal);
        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPCRG_CONTROL, regVal);
        OS_PTIMER_SPIN_WAIT_NS(4000);
    }
}

/*!
 * @brief Program SMC's PRI GPCCS_ENGINE_RESET_CTL
 *
 *  This brings the GPC out of reset as per the new SMC reset scheme
 */
void
grRgGpcResetControl_GA10X(void)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    LwU32 regVal;

    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_GPCCS_ENGINE_RESET_CTL);

    regVal = FLD_SET_DRF(_PGRAPH_PRI_GPCS_GPCCS, _ENGINE_RESET_CTL,
                        _GPC_ENGINE_RESET, _DISABLED, regVal);

    REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_GPCCS_ENGINE_RESET_CTL , regVal);

    // Read back the register to flush the write
    REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_GPCCS_ENGINE_RESET_CTL);
#endif  // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
}

/*!
 * @brief Assert/De-assert GR-RG engine reset
 *
 * This includes the following -
 *  FECS reset
 *  BECS reset
 *
 * Note - GPC reset is done separately for GPC-RG
 *
 * @param[in] bAssert   True to assert, False otherwise
 */
void
grRgEngineResetAssert_GA10X(LwBool bAssert)
{
    if (bAssert)
    {
        grAssertBECSReset_HAL(&Gr, LW_TRUE);
        grAssertFECSReset_HAL(&Gr, LW_TRUE);
    }
    else
    {
        grAssertFECSReset_HAL(&Gr, LW_FALSE);
        grAssertBECSReset_HAL(&Gr, LW_FALSE);
    }
}

/*!
 * @brief Interface to assert/de-assert GPC rail-gating clamp
 *
 * This does the following -
 *    -Disables/Enables power clamp
 *    -Disables/Enables clock clamp
 *    -Assert/De-assert reset for SPLIT-FIFO
 *
 * @param[in] bAssert   boolean to specify assert or de-assert
 */
void
grRgClampAssert_GA10X(LwBool bAssert)
{
    LwU32 regVal;

    regVal = REG_RD32(CSB, LW_CPWR_PMU_GPCRG_CONTROL);

    if (bAssert)
    {
        // Assert GPC rail-gating clamp
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL,
                             _RG_CLAMP, _ENABLE, regVal);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL,
                             _RG_CLK_CLAMP, _ENABLE, regVal);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL,
                             _RG_RESET_INT_CLAMP, _ENABLE, regVal);
    }
    else
    {
        // De-assert GPC rail-gating clamp
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL,
                             _RG_CLAMP, _DISABLE, regVal);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL,
                             _RG_CLK_CLAMP, _DISABLE, regVal);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_CONTROL,
                             _RG_RESET_INT_CLAMP, _DISABLE, regVal);
    }

    REG_WR32_STALL(CSB, LW_CPWR_PMU_GPCRG_CONTROL, regVal);
}

/*!
 * Override the GR-RG PRIV access Error Handling Support based on PLM
 */
void
grRgPrivErrHandlingSupportOverride_GA10X(void)
{
    OBJPGSTATE *pGrState = NULL;
    LwU32       regVal   = 0;

    //
    // Read the PLM for this register. If PLM is set for Level 3 write access only,
    // then remove the support for PRIV_RING Error handling
    //
    regVal = REG_RD32(CSB, LW_CPWR_PMU_MEMSYS_GPC_PRI_ERR_PRIV_LEVEL_MASK);
    if (FLD_TEST_DRF(_CPWR, _PMU_MEMSYS_GPC_PRI_ERR_PRIV_LEVEL_MASK, _WRITE_PROTECTION,
                     _LEVEL3_ENABLED_FUSE1, regVal))
    {
        pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
        // Since Level 3 PLM is set, remove the PRIV_RING Error support
        pGrState->supportMask = LPWR_SF_MASK_CLEAR(GR, PRIV_RING, pGrState->supportMask);
    }
}

/*!
 * @brief Interface to Activate/Deactivate PRI error mechanism for GPC access
 *
 * @param[in] Activate   boolean to specify activation or deactivation
 */
void
grRgPriErrHandlingActivate_GA10X(LwBool bActivate)
{
    LwU32 regVal;

    regVal = REG_RD32(CSB, LW_CPWR_PMU_GPCRG_GPC_PRI_ERR);

    if (bActivate)
    {
        // Enable PRI error mechanism for GPC access
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_GPC_PRI_ERR,
                             _SW, _EN, regVal);
    }
    else
    {
        // Disable PRI error mechanism for GPC access
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPCRG_GPC_PRI_ERR,
                             _SW, _DIS, regVal);
    }

    REG_WR32_STALL(CSB, LW_CPWR_PMU_GPCRG_GPC_PRI_ERR, regVal);
}

/*!
 * @brief Bootstrap GPCCS falcon
 *
 * GPCCS bootstrapping includes following steps -
 *   1. Load GPCCS ucode
 *   2. Mark the context as not required
 *   3. Start the GPCCS falcon
 *   4. Wait for GPCCS init to complete
 *
 * NOTE : As part of handling the INIT_COMPELTE method (step 4),
 *        FECS does all the required processing on its side.
 *        Any data required during this processing is cached by FECS
 *        during INIT time.
 */
void
grRgGpccsBootstrap_GA10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
    LwU32       regVal   = 0;

    // S1 - Load GPCCS ucode
    s_grRgGpccsUcodeLoad_GA10X();

    // S2 - Mark the context as not required
    regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_DMACTL);
    regVal = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_FALCON_DMACTL, _REQUIRE_CTX,
                         _FALSE, regVal);
    REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_DMACTL, regVal);

    //
    // S3 - Start the GPCCS falcon
    // In secure setup i.e. where sec2-rtos is available to boot GPCCS in LS mode,
    // we need to program CPUCTL_ALIAS to trigger GPCCS since CPUCTL is blocked (Bug 3107441)
    // and CPUCTL_ALIAS is enabled as part of GPCCS's LS setup.
    // In non-secure setup, we need to program CPUCTL directly
    //
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, GR, RG_GPCCS_BS_PMU))
    {
        REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_CPUCTL_ALIAS,
            DRF_DEF(_PGRAPH, _PRI_GPCS_GPCCS_FALCON_CPUCTL_ALIAS, _STARTCPU, _TRUE));
    }
    else
    {
        regVal = REG_RD32(FECS, LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_CPUCTL);
        regVal = FLD_SET_DRF(_PGRAPH_PRI_GPCS, _GPCCS_FALCON_CPUCTL, _STARTCPU,
                             _TRUE, regVal);
        REG_WR32(FECS, LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_CPUCTL, regVal);
    }

    //
    // S4 - Wait for GPCCS init to complete
    //
    // Method data = 0x1
    // Poll value  = method id (_CHECK_GPCCS_INIT_COMPLETE)
    //
    grSubmitMethod_HAL(&Gr, LW_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_CHECK_GPCCS_INIT_COMPLETE,
                       0x1, LW_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_CHECK_GPCCS_INIT_COMPLETE);
}

/*!
 * @brief Interface to gate/ungate LWVDD rail
 *
 * TODO : This is a temporary API for Arch testing. It will be replaced by
 *        the production level API from Volt team.
 *        Here, we are using GPIO 29 as per GPIO assignement in GA102 VBIOS
 *        table
 *
 * @param[in] bGate     Whether to gate or ungate the rail
 */
FLCN_STATUS
grRgRailGate_GA10X
(
    LwBool bGate
)
{
    FLCN_STATUS status;
    LwU8        token;
    LwU32       regVal;

    // Acquire the GPIO mutex
    status = mutexAcquire(LW_MUTEX_ID_GPIO,
                          (200000 * 1000),
                          &token);
    if (status != FLCN_OK)
    {
        PMU_HALT();
    }

    // Assert/de-assert the GPIO 29 (active high)
    regVal = REG_RD32(FECS, LW_GPIO_OUTPUT_CNTL(29));
    if (bGate)
    {
        regVal = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _IO_OUTPUT, _0, regVal);
        regVal = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _IO_OUT_EN, _YES, regVal);
    }
    else
    {
        regVal = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _IO_OUTPUT, _1, regVal);
        regVal = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _IO_OUT_EN, _YES, regVal);
    }
    REG_WR32(FECS, LW_GPIO_OUTPUT_CNTL(29), regVal);

    // Trigger the GPIO update
    REG_FLD_WR_DRF_DEF(FECS, _PMGR, _GPIO_OUTPUT_CNTL_TRIGGER, _UPDATE, _TRIGGER);

    // Poll for roughly 50us for the trigger to get done.
    if (!PMU_REG32_POLL_US(
         USE_FECS(LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER),
         DRF_SHIFTMASK(LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE),
         LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_DONE,
         50,
         PMU_REG_POLL_MODE_BAR0_EQ))
    {
        PMU_HALT();
    }

    // Release the GPIO mutex
    mutexRelease(LW_MUTEX_ID_GPIO, token);

    //
    // While ungating the rail, ensure that PGOOD is asserted within 2 ms.
    // Note - This check is enabled only on RTL for verification purpose.
    //
    regVal = REG_RD32(BAR0, LW_PTOP_PLATFORM);
    if (FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _RTL, regVal))
    {
        if (!bGate)
        {
            if (!PMU_REG32_POLL_US(
                 USE_FECS(LW_PGC6_SCI_PGOOD),
                 DRF_SHIFTMASK(LW_PGC6_SCI_PGOOD_STATUS),
                 LW_PGC6_SCI_PGOOD_STATUS_TRUE,
                 PGOOD_TRIGGER_TIMEOUT_US,
                 PMU_REG_POLL_MODE_BAR0_EQ))
            {
                PMU_HALT();
            }
        }
        else
        {
            if (!PMU_REG32_POLL_US(
                 USE_FECS(LW_PGC6_SCI_PGOOD),
                 DRF_SHIFTMASK(LW_PGC6_SCI_PGOOD_STATUS),
                 LW_PGC6_SCI_PGOOD_STATUS_FALSE,
                 PGOOD_TRIGGER_TIMEOUT_US,
                 PMU_REG_POLL_MODE_BAR0_EQ))
            {
                PMU_HALT();
            }
        }
    }

    return status;
}

/*!
 * @brief Interface to poll for RAM repair completion
 *
 * RAM repair is triggered by HW. SW just polls for its completion
 * (i.e. Poll on LW_FUSE_RESHIFT_GPC_RAIL_STATUS to reach _DONE)
 */
void
grRamRepairCompletionPoll_GA10X(void)
{
    // Poll on LW_FUSE_RESHIFT_GPC_RAIL_STATUS to reach _DONE
    if (!PMU_REG32_POLL_US(
         USE_FECS(LW_FUSE_RESHIFT_GPC_RAIL),
         DRF_SHIFTMASK(LW_FUSE_RESHIFT_GPC_RAIL_STATUS),
         DRF_DEF(_FUSE, _RESHIFT_GPC_RAIL, _STATUS, _DONE),
         GR_RAM_REPAIR_TIMEOUT_US,
         PMU_REG_POLL_MODE_BAR0_EQ))
    {
        PMU_HALT();
    }
}

/*!
 * @brief Load GPCCS ucode
 *
 * Use SEC2 or RM path based on Priv Sec is enabled or not
 */
static void
s_grRgGpccsUcodeLoad_GA10X(void)
{
    PMU_RM_RPC_STRUCT_LPWR_GR_RG_GPCCS_BOOTSTRAP rpc;
    OBJPGSTATE    *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
    LwU32          regVal   = 0;
    FLCN_STATUS    status;

    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, GR, RG_GPCCS_BS_PMU))
    {
        // Set the GPCCS bootstrapping state as NOT_STARTED
        regVal = REG_RD32(FECS, LW_PSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO);
        regVal = FLD_SET_DRF(_PSEC_GPC_RG, _GPCCS_BOOTSTRAP_INFO,
                             _STATE, _NOT_STARTED, regVal);
        REG_WR32_STALL(FECS, LW_PSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO, regVal);

        // Send a 'command queue interrupt' to SEC2 for bootstrapping GPCCS
        REG_WR32_STALL(FECS, LW_PSEC_QUEUE_HEAD(LW_PSEC_QUEUE_HEAD__SIZE_1 - 1), 0x1);

        // Wait till the GPCCS bootstrapping state is DONE
        if (!PMU_REG32_POLL_US(
                USE_FECS(LW_PSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO),
                DRF_SHIFTMASK(LW_PSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO_STATE),
                DRF_DEF(_PSEC_GPC_RG, _GPCCS_BOOTSTRAP_INFO, _STATE, _DONE),
                GR_GPCCS_BOOTSTRAP_TIMEOUT_US_GA10X,
                PMU_REG_POLL_MODE_BAR0_EQ))
        {
            PMU_BREAKPOINT();
        }
    }
    else
    {
        memset(&rpc, 0x00, sizeof(rpc));

        // Set the GPCCS bootstrapping state as NOT_STARTED
        REG_WR32_STALL(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_LPWR_GR_RG),
                       RM_PMU_GPCCS_BOOTSTRAP_NOT_STARTED);

        // Send the RPC to RM
        PMU_RM_RPC_EXELWTE_BLOCKING(status, LPWR, GR_RG_GPCCS_BOOTSTRAP, &rpc);

        // Wait till the GPCCS bootstrapping state is DONE
        do
        {
            regVal = REG_RD32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_LPWR_GR_RG));
        } while (regVal != RM_PMU_GPCCS_BOOTSTRAP_DONE);
    }
}
