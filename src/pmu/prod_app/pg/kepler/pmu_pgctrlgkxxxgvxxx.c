/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pgctrlgkxxxgvxxx.c
 * @brief  PMU PG Controller Operation for kepler to volta GPUs
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
//
// Move pgCtrlIdleFlipped_HAL to resident section if IDLE_FLIPPED_RESET WAR is
// enabled.
//
#if (PMUCFG_FEATURE_ENABLED(PMU_PG_IDLE_FLIPPED_RESET))
LwBool pgCtrlIdleFlipped_GMXXX(LwU32 ctrlId)
       GCC_ATTRIB_SECTION("imem_resident", "pgCtrlIdleFlipped_GMXXX");
#endif

/* ------------------------ Public Functions ------------------------------- */
/*!
 * @brief Initialize PgCtrl
 *
 * @param[in]  ctrlId   PG controller id
 */
void
pgCtrlInit_GMXXX
(
    LwU32  ctrlId
)
{
    LwU32 data;
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId);

    // Enable HW FSM
    data = REG_RD32(CSB, LW_CPWR_PMU_PG_CTRL);
    data = FLD_IDX_SET_DRF(_CPWR, _PMU_PG_CTRL, _ENG, hwEngIdx, _ENABLE, data);
    REG_WR32(CSB, LW_CPWR_PMU_PG_CTRL, data);
}

/*!
 * @brief General PG routine - do soft reset on the elpg controller
 *
 * @param[in]  ctrlId   PG controller ID
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
pgCtrlSoftReset_GMXXX(LwU32 ctrlId)
{
    LwU32 data;
    LwU32 cfgRdy;
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId);

    cfgRdy = REG_RD32(CSB, LW_CPWR_PMU_PG_CFG_RDY);

    data = REG_RD32(CSB, LW_CPWR_PMU_PG_SOFT_RST);
    data = FLD_IDX_SET_DRF(_CPWR,
            _PMU_PG_SOFT_RST, _ENG, hwEngIdx, _TRUE, data);
    REG_WR32(CSB, LW_CPWR_PMU_PG_SOFT_RST, data);

    data = FLD_IDX_SET_DRF(_CPWR,
            _PMU_PG_SOFT_RST, _ENG, hwEngIdx, _FALSE, data);
    REG_WR32(CSB, LW_CPWR_PMU_PG_SOFT_RST, data);
    REG_RD32(CSB, LW_CPWR_PMU_PG_SOFT_RST); // flush

    REG_WR32(CSB, LW_CPWR_PMU_PG_CFG_RDY, cfgRdy);

    return FLCN_OK;
}

/*!
 * @brief Manage SM Actions
 *
 * @param[in]  ctrlId       PG controller ID
 * @param[in]  actionId     PG event.
 */
void
pgCtrlSmAction_GMXXX
(
    LwU32 ctrlId,
    LwU32 actionId
)
{
    LwU32 ctrlIdLocal;
    LwU32 pgOff;
    LwU32 hwEngIdx;

    switch (actionId)
    {
        case PMU_PG_SM_ACTION_PG_OFF_START:
        {
            //
            // Write PG_OFF
            //
            // LW_CPWR_PMU_PG_PGOFF needs to be written in a read-modify-write
            // fashion so that there are no side-effects on the engine that is
            // not be purposely affected However, there is a caveat that the
            // read-modify-write only applies to the REQSRC field. If Eng0 is
            // lwrrently in the power-up process when an attempt to power-up
            // Eng1 (and vice-versa), it is important to write a 0 into the
            // _ENG0 field, not the current value (which will be 1).
            // The safe procedure is to always write a 0 to the _ENGx field of
            // the non-affected engine, but, maintain the value of _REQSRC
            //
            pgOff = REG_RD32(CSB, LW_CPWR_PMU_PG_PGOFF);

            FOR_EACH_INDEX_IN_MASK(32, ctrlIdLocal, Pg.supportedMask)
            {
                hwEngIdx = PG_GET_ENG_IDX(ctrlIdLocal);

                if (ctrlIdLocal == ctrlId)
                {
                    pgOff = FLD_IDX_SET_DRF(_CPWR, _PMU_PG_PGOFF, _ENG,
                            hwEngIdx, _START, pgOff);
                    pgOff = FLD_IDX_SET_DRF(_CPWR, _PMU_PG_PGOFF, _REQSRC,
                            hwEngIdx, _HOST, pgOff);
                }
                else
                {
                    pgOff = FLD_IDX_SET_DRF(_CPWR, _PMU_PG_PGOFF, _ENG,
                            hwEngIdx, _DONE,  pgOff);
                }
            }
            FOR_EACH_INDEX_IN_MASK_END;

            REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_PGOFF, pgOff);
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
 * @brief Disable power gating delays
 *
 * EPE delay: Engine Power Enable delay. Its time needed for power enable
 *            propagation.
 * Clamp delay: Its time needed for clamp enable signal propagation.
 *
 * EPE and Clamp delays are needed for "power gating". If PgCtrl is just clock
 * gating we can safely set them to 0 to reduce the entry/exit time.
 *
 * @param[in]   ctrlId      PG controller ID
 */
void
pgCtrlPgDelaysDisable_GMXXX(LwU32 ctrlId)
{
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId);

    REG_WR32(CSB, LW_CPWR_PMU_PG_EPE_DELAY(hwEngIdx), 0);
    REG_WR32(CSB, LW_CPWR_PMU_PG_CLAMP_DELAY(hwEngIdx), 0);
}

/*!
 * @breif Checks the state of IDLE_FLIPPED bit
 *
 * @param[in]   ctrlId      PG controller ID
 *
 * @return   LW_TRUE  if the MS is not idle.
 * @return   LW_FALSE otherwise
 */
LwBool
pgCtrlIdleFlipped_GMXXX(LwU32 ctrlId)
{
    LwBool bIdleFlipped = LW_TRUE;

#ifdef LW_CPWR_PMU_PG_STAT_IDLE_FLIPPED
    bIdleFlipped = FLD_TEST_DRF(_CPWR_PMU, _PG_STAT, _IDLE_FLIPPED, _ASSERTED,
                     REG_RD32(CSB, LW_CPWR_PMU_PG_STAT(PG_GET_ENG_IDX(ctrlId))));
#endif

    return bIdleFlipped;
}

/*!
 * @brief Configures idle counter mask.
 *
 * It sets Idle Mask for Supplemental Idle Counter.
 *
 * @param[in]  ctrlId   PG controller ID
 */
void
pgCtrlIdleMaskSet_GMXXX
(
    LwU32 ctrlId,
    LwU32 *pIdleMask
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    REG_WR32(CSB, LW_CPWR_PMU_IDLE_MASK_SUPP(pPgState->idleSuppIdx),
                  pIdleMask[0]);
    REG_WR32(CSB, LW_CPWR_PMU_IDLE_MASK_1_SUPP(pPgState->idleSuppIdx),
                  pIdleMask[1]);
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
pgCtrlConfigCntrMode_GMXXX
(
    LwU32 ctrlId,
    LwU32 operationMode
)
{
    LwU32 idleSuppIdx = GET_OBJPGSTATE(ctrlId)->idleSuppIdx;

    switch (operationMode)
    {
        case PG_SUPP_IDLE_COUNTER_MODE_FORCE_IDLE:
        {
            REG_WR32(CSB, LW_CPWR_PMU_IDLE_CTRL_SUPP(idleSuppIdx),
                          DRF_DEF(_CPWR_PMU, _IDLE_CTRL_SUPP, _VALUE, _ALWAYS));
            break;
        }
        case PG_SUPP_IDLE_COUNTER_MODE_FORCE_BUSY:
        {
            REG_WR32(CSB, LW_CPWR_PMU_IDLE_CTRL_SUPP(idleSuppIdx),
                          DRF_DEF(_CPWR_PMU, _IDLE_CTRL_SUPP, _VALUE, _NEVER));
            break;
        }
        case PG_SUPP_IDLE_COUNTER_MODE_AUTO_IDLE:
        {
            REG_WR32(CSB, LW_CPWR_PMU_IDLE_CTRL_SUPP(idleSuppIdx),
                          DRF_DEF(_CPWR_PMU, _IDLE_CTRL_SUPP, _VALUE, _IDLE));
            break;
        }
        default:
        {
            PMU_HALT();
        }
    }
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
pgCtrlSetCfgRdy_GMXXX
(
    LwU32  ctrlId,
    LwBool bSet
)
{
    LwU32 data;
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId);

    data = REG_RD32(CSB, LW_CPWR_PMU_PG_CFG_RDY);

    data = bSet ?
        FLD_IDX_SET_DRF(_CPWR, _PMU_PG_CFG_RDY, _ENG, hwEngIdx, _TRUE, data):
        FLD_IDX_SET_DRF(_CPWR, _PMU_PG_CFG_RDY, _ENG, hwEngIdx, _FALSE,data);
    REG_WR32(CSB, LW_CPWR_PMU_PG_CFG_RDY, data);

    return FLCN_OK;
}
