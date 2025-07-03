/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_cg.c
 * @brief  LPWR Clock Gating related functions.
 */

/*!
 * GR-ELCG <-> OSM CG WAR bugs: 916757, 1512510, 200093466, 1603090, 1512407.
 *
 * There are couple of HW issues with ELCG. Summarizing all -
 * Issue 1) ELCG HW SM needs to be disabled before gating GPC/LTC clocks from
 * OSM. Otherwise ELCG HW SM goes in hung state.
 * There are again 3 different reasons of this issue:
 * - On GK208_and_later, the GPC, FBP and SYS shares the same ELCG control.
 *   So wake up sysclk from ELCG also requires the ACK from GPC/FBP which
 *   require the gpcclk_noeg/ltcclk_noeg to be ON. During MSCG/GC5 we turn
 *   off gpc/ltc clocks at the root of the tree.
 *   Solution: Disable ELCG clock gating before gating GPC and LTC clock from
 *   OSM
 * - We see similar hard if ELCG SM is active and ELCG clock gating is disabled.
 *   Solution: Disable complete ELCG SM before gating GPC and LTC clock from
 *   OSM.
 * - Changing state of THERM_GATE_CTRL when GPC/LTC are gated from OSM causes
 *   hard hang.
 *   Solution: Delay the update of THERM_GATE_CTRL until we ungate GPC/LTC
 *   clock from OSM.
 *
 * Issue 2) Disabling ELCG SM also disables engine hold-off SM.
 * - Both of these HW state machines are coupled with each other. So SW can't
 *   engage Hold-off if ELCG SM is disabled.
 * - This issue has been fixed on GP10X by decoupling ELCG and hold-off SM on
 *   GP10X. Reference bug 1598901.
 * - NOTE: If Holdoff is engaged by host and then PMU asserted THERM_ENG_HOLDOFF
 *   to keep holdoff engaged then disabling ELCG SM doesn't result in releasing
 *   holdoff.
 *
 * Issue 3) Only PRIV access can wake ELCG SM. CSB access don't wakes ELCG.
 * - To avoid race explained in bug 1603090 SW should always use priv access
 *   to disable ELCG clock gating and ELCG SM on GP100. We will still use
 *   CSB on GM20X as that's the way we have qualified GM20X. Also bug
 *   1603090 needs HW WAR which only present on GP100.
 * - This issue has been fixed on GP10X. Reference bug 1598901.
 *
 * There is difference in disabling ELCG clock gating and disabling complete
 * ELCG SM. Disabling ELCG SM means whole ELCG logic is disabled in HW. On
 * other side disabling ELCG clock gating only make sure that ELCG SM will not
 * gate engine clock but ELCG SM is still active and can result in issue1.
 *
 * Sequence to disable ELCG clock gating -
 * - Set LW_CPWR_THERM_GATE_CTRL_ENG_CLK is set to _RUN.
 *
 * Sequence to disable ELCG SM -
 * - Set LW_CPWR_THERM_GATE_CTRL_ENG_CLK is set to _RUN.
 * - Set LW_CPWR_THERM_GATE_CTRL_ENG_PWR is set to _ON.
 *
 * MSCG engages hold-off. MSCG also gates GPC/LTC clocks from OSM. So we came
 * up with this sequence that can provide SW WAR for all HW issues.
 *
 * MSCG entry:
 * 1) ELCG SM is enabled and thus holdoff SM is also active.
 * 2) PG_ON interrupt for MSCG came
 * 3) MSCG engaged hold-off.
 * 4) PMU completes rest of blocker sequence of MSCG and puts FB in self-refresh.
 * 5) Disable ELCG SM.
 *    - For GM20X: Using CSB write to disable ELCG SM. This is way we have
 *                 qualified GM20X chips.
 *    - For GP100: Disable ELCG SM using priv write so that ELCG SM gets
 *                 disabled immediately.
 * 6) Gate GPC and LTC clocks. Clock gating happens through PRIV write. PRIV
 *    write will first wake ELCG if its engaged (for GM20X due to issue #3) and
 *    will lock ELCG SM to disabled state then register access will proceed
 *    further for gating clocks.
 *
 * MSCG exit:
 * 1) Ungate GPC and LTC clock from OSM
 * 2) Enable ELCG HW SM
 *    - For GM20X: Using CSB write to enable ELCG SM. This is way we have
 *      qualified GM20X chips.
 *    - For GP100: Use PRIV write to enable ELCG SM.
 * 3) Proceed further with rest of MSCG exit sequence.
 */

/*!
 * PMU ELCG SW WAR:
 *
 * RM cannot take decision about when it should send CMD_ID_ELCG_INIT_AND_ENABLE
 * command CMD_ID_ELCG_STATE_CHANGE. RM always sends CMD_ID_ELCG_INIT_AND_ENABLE
 * command every time enabling/disabling ELCG.
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objlpwr.h"
#include "pmu_objfifo.h"

#include "config/g_pg_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief API to enable ELCG Ctrl using PMU engine ID
 *
 * @param[in] pmuEngId  PMU SW Engine ID
 * @param[in] reasonId  Enable reason ID
 *
 * @return  FLCN_OK on success
 */
FLCN_STATUS
lpwrCgElcgEnable
(
    LwU32 pmuEngId,
    LwU32 reasonId
)
{
    FLCN_STATUS status;

    if (pmuEngId < PMU_ENGINE_ID__COUNT)
    {
        status = lpwrCgElcgCtrlEnable_HAL(&Lpwr, GET_FIFO_ENG(pmuEngId), reasonId);
    }
    else
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief API to disable ELCG Ctrl using PMU engine ID
 *
 * @param[in] pmuEngId  PMU SW Engine ID
 * @param[in] reasonId  Enable reason ID
 *
 * @return  FLCN_OK on success
 */
FLCN_STATUS
lpwrCgElcgDisable
(
    LwU32 pmuEngId,
    LwU32 reasonId
)
{
    FLCN_STATUS status;

    if (pmuEngId < PMU_ENGINE_ID__COUNT)
    {
        status = lpwrCgElcgCtrlDisable_HAL(&Lpwr, GET_FIFO_ENG(pmuEngId), reasonId);
    }
    else
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief LPWR clock gating pre init.
 */
void
lpwrCgPreInit(void)
{
    // Disable the FBPA SLCG.
    lpwrCgFbpaSlcgDisable_HAL(&Lpwr);

    // Initialize ELCG
    lpwrCgElcgPreInit_HAL(&Lpwr);

    // Enable clock gating for LPWR ELPG Sequencer
    lpwrCgElpgSequencerEnable_HAL(&Lpwr);
}

/*!
 * @brief LPWR clock gating post init
 *
 * @return  FLCN_OK     On success.
 */
FLCN_STATUS
lpwrCgPostInit(void)
{
    return lpwrCgElcgPostInit_HAL(&Lpwr);
}

/*!
 * @brief LPWR clock gating post init.
 *
 * @param[in] - bIstCgSupport - LW_TRUE  -> Gate the IST clock.
 *                            - LW_FALSE -> Ungate the IST clock.
 *
 * @return  FLCN_OK     On success.
 */
FLCN_STATUS
lpwrCgPostInitRpcHandler(LwBool bIstCgSupport)
{
    FLCN_STATUS status;

    status = lpwrCgFrameworkPostInit_HAL();
    if (status != FLCN_OK)
    {
        goto lpwrCgPostInit_exit;
    }

    // Gate IST clock gating.
    status = lpwrCgIstPostInit_HAL(&lpwr, bIstCgSupport);

lpwrCgPostInit_exit:

    return status;
}
/* ------------------------- Private Functions ------------------------------ */
