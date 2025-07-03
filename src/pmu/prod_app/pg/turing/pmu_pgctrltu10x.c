/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pgctrltu10x.c
 * @brief  PMU PG Controller Operation (Turing)
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_falcon_csb.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_objpmu.h"
#include "pmu_objlpwr.h"
#include "dbgprintf.h"

#include "config/g_pg_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */
static void   s_pgCtrlIdleCacheUpdateIdleFlip_TU10X(LwU32 ctrlId);
static void   s_pgCtrlIdleCacheUpdateIdleSnap_TU10X(LwU32 ctrlId);
static LwBool s_pgCtrlIsSwClientWakeUp_TU10X(LwU32 ctrlId);

//
// Move pgCtrlIdleFlipped_HAL to resident section if IDLE_FLIPPED_RESET WAR is
// enabled.
//
#if (PMUCFG_FEATURE_ENABLED(PMU_PG_IDLE_FLIPPED_RESET))
LwBool pgCtrlIdleFlipped_TU10X(LwU32 ctrlId)
       GCC_ATTRIB_SECTION("imem_resident", "pgCtrlIdleFlipped_TU10X");
#endif

/* ------------------------ Public Functions ------------------------------- */
/*!
 * @brief Initialize PgCtrl
 *
 * @param[in]  ctrlId   PG controller id
 */
void
pgCtrlInit_TU10X
(
    LwU32  ctrlId
)
{
    LwU32 data;
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId);

    data = REG_RD32(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx));

    //
    // Set State Machine type
    //
    // We support two types of SM from Turing and onward chips
    // 1) LPWR_ENG: Newly added on Turing
    // 2) PG_ENG: Legacy SM
    // Configure engine type based on SW settings
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_ENG) &&
        LPWR_ARCH_IS_SF_SUPPORTED(LPWR_ENG))
    {
        data = FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _ENG_TYPE, _LPWR, data);
    }
    else
    {
        data = FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _ENG_TYPE, _PG, data);
    }

    // Enable HW FSM
    data = FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _ENG, _ENABLE, data);

    REG_WR32(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx), data);
}

/*!
 * @brief General PG routine - do soft reset on the elpg controller
 *
 * @param[in]  ctrlId   PG controller ID
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
pgCtrlSoftReset_TU10X(LwU32 ctrlId)
{
    LwU32 data;
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId);

    // Issue soft reset
    data = REG_RD32(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx));
    data = FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _SOFT_RST, _TRUE, data);
    REG_WR32(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx), data);

    // Soft reset bit is not cleared by HW. Issue update to clear SW reset bit.
    data = FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _SOFT_RST, _FALSE, data);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx), data);

    //
    // Restore setting of config ready bit.
    //
    // Soft_rst resets the config ready setting. We need to restore cfg_rdy
    // setting after completing soft reset. Config ready setting can be updated
    // after completion of soft reset i.e after setting _SOFT_RST to _FALSE.
    // We cannot clear soft reset and restore config ready setting in same
    // register write. Thus, issue one more write to update config ready.
    //
    REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx), data);

    return FLCN_OK;
}

/*!
 * @brief Manage SM Actions
 *
 * @param[in]  pgId         PG controller ID
 * @param[in]  actionId     PG event.
 */
void
pgCtrlSmAction_TU10X
(
    LwU32 pgId,
    LwU32 actionId
)
{
    LwU32 hwEngIdx = PG_GET_ENG_IDX(pgId);
    LwU32 data;

    data = REG_RD32(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx));

    switch (actionId)
    {
        case PMU_PG_SM_ACTION_PG_OFF_START:
        {
            //
            // Activate/Deactivate PPU threshold
            //
            // Setting _REQSRC to _HOST activates PPU threshold for given exit.
            // Setting _REQSRC to _RM de-activates PPU threshold for given exit.
            //
            data = pgCtrlIsPpuThresholdRequired(pgId) ?
                   FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _PGOFF_REQSRC, _HOST, data) :
                   FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _PGOFF_REQSRC, _RM, data);

            // Initiate HW SM wake-up
            data = FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _PGOFF_ENG, _START, data);
            break;
        }
        case PMU_PG_SM_ACTION_PG_OFF_DONE:
        {
            data = FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _PGOFF_DONE, _VALID, data);
            break;
        }
        default:
        {
            PMU_HALT();
            break;
        }
    }

    REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx), data);
}

/*!
 * @breif Checks the state of IDLE_FLIPPED bit
 *
 * @param[in]   ctrlId      PG controller ID
 *
 * @return   LW_TRUE  if the PgCtrl is not idle.
 * @return   LW_FALSE otherwise
 */
LwBool
pgCtrlIdleFlipped_TU10X(LwU32 ctrlId)
{
    return FLD_TEST_DRF(_CPWR, _PMU_PG_STAT, _IDLE_FLIPPED, _ASSERTED,
                     REG_RD32(CSB, LW_CPWR_PMU_PG_STAT(PG_GET_ENG_IDX(ctrlId))));
}

/*!
 * @brief Configures idle counter mask.
 *
 * It sets Idle Mask for Supplemental Idle Counter.
 *
 * @param[in]  ctrlId   PG controller ID
 */
void
pgCtrlIdleMaskSet_TU10X
(
    LwU32 ctrlId,
    LwU32 *pIdleMask
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    REG_WR32(CSB, LW_CPWR_PMU_PG_IDLE_MASK(pPgState->hwEngIdx),
                  pIdleMask[0]);
    REG_WR32(CSB, LW_CPWR_PMU_PG_IDLE_MASK_1(pPgState->hwEngIdx),
                  pIdleMask[1]);
    REG_WR32(CSB, LW_CPWR_PMU_PG_IDLE_MASK_2(pPgState->hwEngIdx),
                  pIdleMask[2]);
}

/*!
 * @brief Configures idle counter mode.
 *
 * This function configures operation mode of Supplemental Idle Counter to
 * PG_SUPP_IDLE_COUNTER_MODE_xyz.
 * Refer @PG_SUPP_IDLE_COUNTER_MODE_xyz for further details.
 *
 * @param[in]  ctrlId           PG controller ID
 * @param[in]  operationMode    Define by PG_SUPP_IDLE_COUNTER_MODE_xyz
 */
void
pgCtrlConfigCntrMode_TU10X
(
    LwU32 ctrlId,
    LwU32 operationMode
)
{
    LwU32 data;
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId);

    data = REG_RD32(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx));

    switch (operationMode)
    {
        case PG_SUPP_IDLE_COUNTER_MODE_FORCE_IDLE:
        {
            data = FLD_SET_DRF(_CPWR, _PMU_PG_CTRL,
                               _IDLE_MASK_VALUE, _ALWAYS, data);
            break;
        }
        case PG_SUPP_IDLE_COUNTER_MODE_FORCE_BUSY:
        {
            data = FLD_SET_DRF(_CPWR, _PMU_PG_CTRL,
                               _IDLE_MASK_VALUE, _NEVER, data);
            break;
        }
        case PG_SUPP_IDLE_COUNTER_MODE_AUTO_IDLE:
        {
            data = FLD_SET_DRF(_CPWR, _PMU_PG_CTRL,
                               _IDLE_MASK_VALUE, _IDLE, data);
            break;
        }
        default:
        {
            PMU_HALT();
        }
    }

    REG_WR32(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx), data);
}

/*!
 * @brief General PG routine - Set/Unset CFG_RDY
 *
 * Set PG's CFG_RDY bit. When it's not set, PG interrupts are not
 * generated. It must be cleared when programming idle thresholds, etc.
 *
 * @param[in]  ctrlId   PG controller ID
 * @param[in]  bSet     True to set, False to clear
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
pgCtrlSetCfgRdy_TU10X
(
    LwU32  ctrlId,
    LwBool bSet
)
{
    LwU32 data;
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId);

    data = REG_RD32(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx));

    data = bSet ? FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _CFG_RDY, _TRUE, data):
                  FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _CFG_RDY, _FALSE, data);

    REG_WR32(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx), data);

    return FLCN_OK;
}

/*!
 * @brief Processes Idle Snap interrupt
 *
 * This function reads the idle status registers corresponding to idle snap
 * and checks reason for idle snap.
 *
 * @param[in]   pgCtrlId        PG controller id
 *
 * @return  PG_IDLE_SNAP_REASON_ERR_IDLE_FLIP   For idle snap because of
 *                                              idle flip
 *          PG_IDLE_SNAP_REASON_WAKE            For valid wake-up
 *          PG_IDLE_SNAP_REASON_ERR_DEFAULT     Otherwise
 */
LwU32
pgCtrlIdleSnapReasonMaskGet_TU10X
(
    LwU32 pgCtrlId
)
{
    LwU32 snapReason = PG_IDLE_SNAP_REASON__INIT;

    //
    // Idle snap from SW client: Valid wake-up
    //
    // SW client can trigger the wake-up by generating IDLE_SNAP. This is valid
    // type of wake-up. We must check for SW Client wakeup first during idle snap
    // interrupt processing. There might be the cases when idle flipped is also
    // asserted with sw Wakeup. In those cases we will miss the sw wakeup and
    // just disable the pgctrl due to idle flipped.
    //
    if (s_pgCtrlIsSwClientWakeUp_TU10X(pgCtrlId))
    {
        return PG_IDLE_SNAP_REASON_WAKEUP_SW_CLIENT;
    }

    // Idle snap because of AUTO WKAEUP TIMER: Valid wake-up
    if ((PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG)) &&
        (pgCtrlId == RM_PMU_LPWR_CTRL_ID_GR_RG))
    {
        if (pgCtrlIsAutoWakeupAsserted_HAL(&Pg, pgCtrlId))
        {
            return PG_IDLE_SNAP_REASON_WAKEUP_TIMER;
        }
    }

    //
    // Idle snap because of IDLE_FLIPPED: Error
    //
    // Idle flipped after point of no return can cause the idle snap. This
    // type of idle snap is not expected in production. Do consider it as
    // an error.
    //
    if (pgCtrlIdleFlipped_HAL(&Pg, pgCtrlId))
    {
        s_pgCtrlIdleCacheUpdateIdleFlip_TU10X(pgCtrlId);

        snapReason = PG_IDLE_SNAP_REASON_ERR_IDLE_FLIP_POWERING_DOWN;
    }
    //
    // Default idle snap: Error
    //
    // Idle snap can be generated when HW SM input fluctuated to BUSY in PWROFF
    // state. This type of idle snap is not expected in production. Do consider
    // it as error.
    //
    else
    {
        s_pgCtrlIdleCacheUpdateIdleSnap_TU10X(pgCtrlId);

        snapReason = PG_IDLE_SNAP_REASON_ERR_IDLE_FLIP_PWR_OFF;
    }

    //
    // WAR200428328: Ignore unbind step in power gating
    //
    // WAR disables unbind step in power gating. As a consequence of that GR-FE
    // can toggle idleness whenever there is cache eviction by ilwalidate
    // request. This can lead to idle-snap. Thus, covert such idle-snap into
    // WAKE.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_GR_UNBIND_IGNORE_WAR200428328) &&
        lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_GR, pgCtrlId))
    {
        OBJPGSTATE *pGrState = GET_OBJPGSTATE(pgCtrlId);

        if (!LPWR_CTRL_IS_SF_SUPPORTED(pGrState, GR, UNBIND))
        {
            if (FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _GR_FE, _BUSY,
                            pGrState->idleStatusCache[0]))
            {
                snapReason = PG_IDLE_SNAP_REASON_WAKEUP_IDLE_FLIP_PWR_OFF;
            }
        }
    }

    //
    // For EI, GR-Passive, MS-Passive and EI-Passive FSMs, idle snap is a valid wakeup
    // in all the scenario. Thus colwert idle snap of these features to wakeup event.
    //
    if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_GR_PASSIVE)) &&
        (pgCtrlId == RM_PMU_LPWR_CTRL_ID_GR_PASSIVE))
    {
        snapReason = PG_IDLE_SNAP_REASON_WAKEUP_IDLE_FLIP_PWR_OFF;
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI)) &&
             (pgCtrlId == RM_PMU_LPWR_CTRL_ID_EI))
    {
        snapReason = PG_IDLE_SNAP_REASON_WAKEUP_IDLE_FLIP_PWR_OFF;
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_PASSIVE)) &&
             (pgCtrlId == RM_PMU_LPWR_CTRL_ID_MS_PASSIVE))
    {
        snapReason = PG_IDLE_SNAP_REASON_WAKEUP_IDLE_FLIP_PWR_OFF;
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI_PASSIVE)) &&
             (pgCtrlId == RM_PMU_LPWR_CTRL_ID_EI_PASSIVE))
    {
        snapReason = PG_IDLE_SNAP_REASON_WAKEUP_IDLE_FLIP_PWR_OFF;
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_DFPR)) &&
             (pgCtrlId == RM_PMU_LPWR_CTRL_ID_DFPR))
    {
        snapReason = PG_IDLE_SNAP_REASON_WAKEUP_IDLE_FLIP_PWR_OFF;
    }

    return snapReason;
}

/*!
 * @brief Check for pgctrl subunit Idleness
 *
 * @return LW_TRUE  If all Subunits are Idle
 *         LW_FALSE If all subunits are Busy
 */
LwBool
pgCtrlIsSubunitsIdle_TU10X
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
    regVal[2] = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS_1);

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
 * @brief Disallow PgCtrl through HW mechanism (SW_CLIENT_2)
 *
 * NOTE - This is a non-blocking API, so the use-case should be
 *        restricted to cases where PgCtrl is in dis-engaged state
 *
 * NOTE - We don't use read/modify/write approach here to avoid
 *        toggling controls for other PgCtrls
 *
 * @param[in]   pgCtrlId        PG controller id
 *
 */
void
pgCtrlHwDisallowHal_TU10X
(
    LwU32 ctrlId
)
{
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId); 
    LwU32 regVal;

    // SET the ENG_BUSY bit
    regVal = FLD_IDX_SET_DRF(_CPWR, _PMU_PG_SW_CLIENT, _ENG_BUSY_SET, hwEngIdx,
                             _TRIGGER, 0);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_SW_CLIENT_2, regVal);
}

/*!
 * @brief Allow PgCtrl through HW mechanism (SW_CLIENT_2)
 *
 * NOTE - This is a non-blocking API
 *
 * NOTE - We don't use read/modify/write approach here to avoid
 *        toggling controls for other PgCtrls
 *
 * @param[in]   pgCtrlId        PG controller id
 *
 */
void
pgCtrlHwAllowHal_TU10X
(
    LwU32 ctrlId
)
{
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId); 
    LwU32 regVal;

    // CLEAR the ENG_BUSY bit
    regVal = FLD_IDX_SET_DRF(_CPWR, _PMU_PG_SW_CLIENT, _ENG_BUSY_CLR, hwEngIdx,
                             _TRIGGER, 0);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_SW_CLIENT_2, regVal);
}

/* ------------------------- Private Functions ----------------------------- */
/*!
 * @brief Update idle signal cache for idle flip
 *
 * @param[in]   ctrlId          PG controller id
 */
static void
s_pgCtrlIdleCacheUpdateIdleFlip_TU10X
(
    LwU32 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       val;

    // Set IDLE flip IDX to current engine
    val = REG_RD32(CSB, LW_CPWR_PMU_PG_MISC);
    val = FLD_SET_DRF_NUM(_CPWR, _PMU_PG_MISC,
                          _IDLE_FLIP_STATUS_IDX, pPgState->hwEngIdx, val);
    REG_WR32(CSB, LW_CPWR_PMU_PG_MISC, val);

    pPgState->idleStatusCache[0] = REG_RD32(CSB, LW_CPWR_PMU_PG_IDLE_FLIP_STATUS);
    pPgState->idleStatusCache[1] = REG_RD32(CSB, LW_CPWR_PMU_PG_IDLE_FLIP_STATUS_1);
    pPgState->idleStatusCache[2] = REG_RD32(CSB, LW_CPWR_PMU_PG_IDLE_FLIP_STATUS_2);
}

/*!
 * @brief Update idle signal cache for idle snap
 *
 * @param[in]   ctrlId          PG controller id
 */
static void
s_pgCtrlIdleCacheUpdateIdleSnap_TU10X
(
    LwU32 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       val;

    // Set IDX to current engine and enable SNAP_READ
    val = REG_RD32(CSB, LW_CPWR_PMU_PG_MISC);
    val = FLD_SET_DRF_NUM(_CPWR, _PMU_PG_MISC,
                          _IDLE_SNAP_IDX, pPgState->hwEngIdx, val);
    val = FLD_SET_DRF(_CPWR, _PMU_PG_MISC, _IDLE_SNAP_READ, _ENABLE, val);
    REG_WR32(CSB, LW_CPWR_PMU_PG_MISC, val);

    // Cache IDLE_STATUS 0/1/2
    pPgState->idleStatusCache[0] = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS);
    pPgState->idleStatusCache[1] = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS_1);
    pPgState->idleStatusCache[2] = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS_2);

    // Disable SNAP_READ
    val = FLD_SET_DRF(_CPWR, _PMU_PG_MISC, _IDLE_SNAP_READ, _DISABLE, val);
    REG_WR32(CSB, LW_CPWR_PMU_PG_MISC, val);
}

/*!
 * @brief Check if wake-up is caused by SW client
 *
 * @param[in]   ctrlId          PG controller id
 *
 * @return      LW_TRUE         Wake-up caused by SW client
 *              LW_FALSE        Otherwise
 */
static LwBool
s_pgCtrlIsSwClientWakeUp_TU10X
(
    LwU32 ctrlId
)
{
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId); 
    LwU32 val;

    val = REG_RD32(CSB, LW_CPWR_PMU_PG_SW_CLIENT_STATUS);

    return FLD_IDX_TEST_DRF(_CPWR, _PMU_PG_SW_CLIENT_STATUS,
                            _ST_ENG, hwEngIdx, _BUSY, val);
}
