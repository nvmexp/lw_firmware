/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pgctrlgkxxx.c
 * @brief  PMU PG Controller Operation (Kepler)
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_falcon_csb.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objpmu.h"
#include "dbgprintf.h"

#include "config/g_pg_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */
/* ------------------------ Private Prototypes ----------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * @brief Clear PG interrupts
 *
 * @param[in]  ctrlId   PG controller ID
 * @param[in]  eventId  PG event  (we may consider changing this to bitmask)
 */
void
pgCtrlInterruptClear_GMXXX
(
    LwU32 ctrlId,
    LwU32 eventId
)
{
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId);

    switch (eventId)
    {
        case PMU_PG_EVENT_PG_ON:
        {
            PMU_HALT_COND(REG_RD32(CSB, LW_CPWR_PMU_PG_INTRSTAT(hwEngIdx)) &
                 DRF_DEF(_CPWR, _PMU_PG_INTRSTAT, _PG_ON, _CLEAR));

            REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_INTRSTAT(hwEngIdx),
                DRF_DEF(_CPWR, _PMU_PG_INTRSTAT, _PG_ON, _CLEAR));
            break;
        }
        case PMU_PG_EVENT_PG_ON_DONE:
        {
            PMU_HALT_COND(REG_RD32(CSB, LW_CPWR_PMU_PG_INTRSTAT(hwEngIdx)) &
                 DRF_DEF(_CPWR, _PMU_PG_INTRSTAT, _PG_ON_DONE, _CLEAR));

            REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_INTRSTAT(hwEngIdx),
                DRF_DEF(_CPWR, _PMU_PG_INTRSTAT, _PG_ON_DONE, _CLEAR));
            break;
        }
        case PMU_PG_EVENT_ENG_RST:
        {
            REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_INTRSTAT(hwEngIdx),
                DRF_DEF(_CPWR, _PMU_PG_INTRSTAT, _ENG_RST, _CLEAR));
            break;
        }
        case PMU_PG_EVENT_CTX_RESTORE:
        {
            REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_INTRSTAT(hwEngIdx),
                DRF_DEF(_CPWR, _PMU_PG_INTRSTAT, _CTX_RESTORE, _CLEAR));
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }
}

/*!
 * @brief Enable PG interrupts
 *
 * @param[in]  ctrlId   PG controller ID
 * @param[in]  bEnable  Enable elpg interrupts if set to TRUE.
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
pgCtrlInterruptsEnable_GMXXX
(
    LwU32  ctrlId,
    LwBool bEnable
)
{
    FLCN_STATUS status   = FLCN_OK;
    LwU32       hwEngIdx = PG_GET_ENG_IDX(ctrlId);
    LwU32       data;

    // Enable/disable the PG_ENG SM interrupts
    data = REG_RD32(CSB, LW_CPWR_PMU_PG_INTREN(hwEngIdx));

    if (bEnable)
    {
        data = FLD_SET_DRF(_CPWR, _PMU_PG_INTREN, _CFG_ERR,     _ENABLE, data);
        data = FLD_SET_DRF(_CPWR, _PMU_PG_INTREN, _PG_ON,       _ENABLE, data);
        data = FLD_SET_DRF(_CPWR, _PMU_PG_INTREN, _PG_ON_DONE,  _ENABLE, data);
        data = FLD_SET_DRF(_CPWR, _PMU_PG_INTREN, _CTX_RESTORE, _ENABLE, data);
        data = FLD_SET_DRF(_CPWR, _PMU_PG_INTREN, _ENG_RST,     _ENABLE, data);
    }
    else
    {
        data = FLD_SET_DRF(_CPWR, _PMU_PG_INTREN, _CFG_ERR,     _DISABLE, data);
        data = FLD_SET_DRF(_CPWR, _PMU_PG_INTREN, _PG_ON,       _DISABLE, data);
        data = FLD_SET_DRF(_CPWR, _PMU_PG_INTREN, _PG_ON_DONE,  _DISABLE, data);
        data = FLD_SET_DRF(_CPWR, _PMU_PG_INTREN, _CTX_RESTORE, _DISABLE, data);
        data = FLD_SET_DRF(_CPWR, _PMU_PG_INTREN, _ENG_RST,     _DISABLE, data);
    }

    REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_INTREN(hwEngIdx), data);

    // If PG_ON slipped in while disabling PG_ENG interrupts, we need to reset it.
    if (!bEnable)
    {
        data = REG_RD32(CSB, LW_CPWR_PMU_PG_INTRSTAT(hwEngIdx));

        //
        // 872594: Do not assume that only one bit of INTRSTAT will be set when
        // an interrupt is raised. Due to race condition between the ISR and
        // LPWR task, its possible that we read the INTRSTAT register here
        // before the ISR has chance to service the interrupt and clear bit 0
        // or INTRSTAT_INTR bit. Best way is to check pending interrupt bit
        // explicitly.
        //
        // Its not expected to have CFG_ERR, PG_ON_DONE, ENG_RST and CTX_RESTORE
        // pending at this point. Assert breakpoint if any of these interruprs
        // are pending.
        //
        if (FLD_TEST_DRF(_CPWR, _PMU_PG_INTRSTAT, _CFG_ERR,     _SET, data) ||
            FLD_TEST_DRF(_CPWR, _PMU_PG_INTRSTAT, _PG_ON_DONE,  _SET, data) ||
            FLD_TEST_DRF(_CPWR, _PMU_PG_INTRSTAT, _ENG_RST,     _SET, data) ||
            FLD_TEST_DRF(_CPWR, _PMU_PG_INTRSTAT, _CTX_RESTORE, _SET, data))
        {
            PMU_BREAKPOINT();
        }
        else
        {
            pgCtrlSoftReset_HAL(&Pg, ctrlId);
        }
    }

    pgCtrlSetCfgRdy_HAL(&Pg, ctrlId, bEnable);

    return status;
}

/*!
 * @brief Set Idle and PPU thresholds
 *
 * @param[in]   ctrlId              PG controller ID
 */
void
pgCtrlIdleThresholdsSet_GMXXX
(
    LwU32 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    // Set idle threshold if its greater than zero
    if (pPgState->thresholdCycles.idle > 0)
    {
        REG_WR32(CSB, LW_CPWR_PMU_PG_IDLEFILTH(pPgState->hwEngIdx),
                      pPgState->thresholdCycles.idle);
    }

    // Set PPU threshold if its greater than zero
    if (pPgState->thresholdCycles.ppu > 0)
    {
        REG_WR32(CSB, LW_CPWR_PMU_PG_PPUIDLEFILTH(pPgState->hwEngIdx),
                      pPgState->thresholdCycles.ppu);
    }
}

/*!
 * @brief return a pgctrl's on/off power state.
 *
 * An internal interface to get the current power state of a PG controller
 * denoted by pgId. This interface simply returns the current power state of
 * the PG controller, thus it should be used carefully if it's used to
 * determine the 'safe' time to access the engine associated with the given
 * controller by making sure the interrupts are disabled and no transitions of
 * power gating states can occur in the PMU.
 *
 * @param[in]  ctrlId  PG controller ID
 *
 * @return LW_TRUE
 *     if the given pgctrl is powered on and not in the middle of power gating
 *     operations.
 *
 * @return LW_FALSE
 *     if the given pgctrl is powered off or in the middle of power gating
 *     operations.
 */
LwBool
pgCtrlIsPoweredOn_GMXXX(LwU32 ctrlId)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_PMU_ONLY) &&
        pgIsCtrlSupported(ctrlId))
    {
        return PG_IS_FULL_POWER(ctrlId);
    }
    else
    {
        return LW_TRUE;
    }
}

/*!
 * @brief Check for pgctrl subunit Idleness
 *
 * @return LW_TRUE  If all Subunits are Idle
 *         LW_FALSE If all subunits are Busy
 */
LwBool
pgCtrlIsSubunitsIdle_GMXXX
(
    LwU32 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       regVal[RM_PMU_PG_IDLE_BANK_SIZE];
    LwU32       i;

    // Read all the signal status registers
    regVal[0] = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS);
    regVal[1] = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS_1);
    regVal[2] = LW_U32_MAX;

    for (i = 0; i < RM_PMU_PG_IDLE_BANK_SIZE; i++)
    {
        // If any of the enabled IDLE Signal is reporting busy, return
        if ((pPgState->idleMask[i] & regVal[i]) != pPgState->idleMask[i])
        {
            return LW_FALSE;
        }
    }

    return LW_TRUE;
}

/*!
 * @brief Initialize the Min/Max idle threshold for LPWR CTRLs
 *
 * @param[in]  ctrlId   PG controller id
 *
 */
void
pgCtrlMinMaxIdleThresholdInit_GMXXX
(
    LwU32  ctrlId
)
{
    OBJPGSTATE *pPgState  = GET_OBJPGSTATE(ctrlId);
    LwU32       pwrClkMHz = PMU_GET_PWRCLK_HZ() / (1000000);

    // colwert the minimum idle threshold time in terms of power clock cycles
    pPgState->thresholdCycles.minIdle = (pwrClkMHz * RM_PMU_PG_CTRL_IDLE_THRESHOLD_MINIMUM_US);

    // colwert the maximum idle threshold time in terms of power clock cycles
    pPgState->thresholdCycles.maxIdle = (pwrClkMHz * RM_PMU_PG_CTRL_IDLE_THRESHOLD_MAXIMUM_US);
}
