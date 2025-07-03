/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_cg_elcgad10b.c
 * @brief  ELCG related functions.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_therm.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objlpwr.h"
#include "pmu_objfifo.h"
#include "pmu_cg.h"
#include "config/g_lpwr_private.h"

//
// Compile time check to ensure LW_CPWR_PMU_GATE_CTRL__SIZE_1 is
// supported by SW framework
//
ct_assert(LW_CPWR_PMU_GATE_CTRL__SIZE_1 <= RM_PMU_LPWR_CG_ELCG_CTRL__COUNT);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_lpwrCgElcgStateChange_AD10B(LwU32 engHwId, LwBool bEnable);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief ELCG post initialization
 */
FLCN_STATUS
lpwrCgElcgPostInit_AD10B(void)
{
    Lpwr.grElcgDisableReasonMask = BIT(LPWR_CG_ELCG_CTRL_REASON_RM);

    return FLCN_OK;
}

/*!
 * @brief API to enable ELCG Ctrl using HW engine ID
 *
 * @param[in] engHwId   Engine ID
 * @param[in] reasonId  Enable reason ID
 *
 * @return  FLCN_OK on success
 */
FLCN_STATUS
lpwrCgElcgCtrlEnable_AD10B
(
    LwU32 engHwId,
    LwU32 reasonId
)
{
    LwU32       engHwIdGr = GET_FIFO_ENG(PMU_ENGINE_GR);
    FLCN_STATUS status    = FLCN_OK;

    if (engHwId == engHwIdGr)
    {
        Lpwr.grElcgDisableReasonMask &= ~BIT(reasonId);

        if (Lpwr.grElcgDisableReasonMask == 0)
        {
            status = s_lpwrCgElcgStateChange_AD10B(engHwId, LW_TRUE);
        }
    }
    else
    {
        status = s_lpwrCgElcgStateChange_AD10B(engHwId, LW_TRUE);
    }

    return status;
}

/*!
 * @brief API to Disable ELCG Ctrl using HW engine ID
 *
 * @param[in] engHwId   Engine ID
 * @param[in] reasonId  Disable reason ID
 *
 * @return  FLCN_OK on success
 */
FLCN_STATUS
lpwrCgElcgCtrlDisable_AD10B
(
    LwU32 engHwId,
    LwU32 reasonId
)
{
    LwU32       engHwIdGr = GET_FIFO_ENG(PMU_ENGINE_GR);
    FLCN_STATUS status    = FLCN_OK;

    if (engHwId == engHwIdGr)
    {
        if (Lpwr.grElcgDisableReasonMask == 0)
        {
            status = s_lpwrCgElcgStateChange_AD10B(engHwId, LW_FALSE);
        }

        Lpwr.grElcgDisableReasonMask |= LWBIT(reasonId);
    }
    else
    {
        status = s_lpwrCgElcgStateChange_AD10B(engHwId, LW_FALSE);
    }

    return status;
}

/*!
 * @brief ELCG Pre-initialization
 */
void
lpwrCgElcgPreInit_AD10B(void)
{
    LwU32 regVal;
    LwU32 engHwId;

    // Initialize all ELCG domains
    for (engHwId = 0; engHwId < LW_CPWR_PMU_GATE_CTRL__SIZE_1; engHwId++)
    {
        regVal = REG_RD32(CSB, LW_CPWR_PMU_GATE_CTRL(engHwId));

        // Initialize Idle threshold
        regVal = FLD_SET_DRF(_CPWR_PMU, _GATE_CTRL, _ENG_IDLE_FILT_EXP,
                             __PROD, regVal);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GATE_CTRL, _ENG_IDLE_FILT_MANT,
                             __PROD, regVal);

        // Initialize delays for CG
        regVal = FLD_SET_DRF(_CPWR_PMU, _GATE_CTRL, _ENG_DELAY_BEFORE,
                             __PROD, regVal);
        regVal = FLD_SET_DRF(_CPWR_PMU, _GATE_CTRL, _ENG_DELAY_AFTER,
                             __PROD, regVal);

        // Ensure that ELCG is by default disabled
        regVal = FLD_SET_DRF(_CPWR_PMU, _GATE_CTRL, _ENG_CLK,
                             _RUN, regVal);

        REG_WR32(CSB, LW_CPWR_PMU_GATE_CTRL(engHwId), regVal);
    }

    // Initialize FECS idle filter
    regVal = REG_RD32(CSB, LW_CPWR_PMU_FECS_IDLE_FILTER);
    regVal = FLD_SET_DRF(_CPWR_PMU, _FECS_IDLE_FILTER, _VALUE,
                         __PROD, regVal);
    REG_WR32(CSB, LW_CPWR_PMU_FECS_IDLE_FILTER, regVal);

    // Initialize HUBMMU idle filter
    regVal = REG_RD32(CSB, LW_CPWR_PMU_HUBMMU_IDLE_FILTER);
    regVal = FLD_SET_DRF(_CPWR_PMU, _HUBMMU_IDLE_FILTER, _VALUE,
                         __PROD, regVal);
    REG_WR32(CSB, LW_CPWR_PMU_HUBMMU_IDLE_FILTER, regVal);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief To enable/disable the ELCG.
 *
 * @param[in] engHwId     - ELCG Eng HW ID
 * @param[in] bEnable     - LW_TRUE  -> Enable ELCG
 *                          LW_FALSE -> Disable ELCG
 */
static FLCN_STATUS
s_lpwrCgElcgStateChange_AD10B
(
    LwU32  engHwId,
    LwBool bEnable
)
{
    FLCN_STATUS status= FLCN_OK;
    LwU32       gateCtrl;

    //
    // Ensure that HW ID requested by RM does not exceed
    // LW_CPWR_PMU_GATE_CTRL__SIZE_1
    //
    if (engHwId >= LW_CPWR_PMU_GATE_CTRL__SIZE_1)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_RPC_ILWALID_INPUT;

        goto lpwrCgElcgStateChange_AD10B_exit;
    }

    gateCtrl = REG_RD32(CSB, LW_CPWR_PMU_GATE_CTRL(engHwId));

    if (bEnable)
    {
        gateCtrl = FLD_SET_DRF(_CPWR_PMU, _GATE_CTRL, _ENG_CLK,
                                _AUTO, gateCtrl);
    }
    else
    {
        gateCtrl = FLD_SET_DRF(_CPWR_PMU, _GATE_CTRL, _ENG_CLK,
                                _RUN, gateCtrl);
    }

    // Program the clock gating.
    REG_WR32(CSB, LW_CPWR_PMU_GATE_CTRL(engHwId), gateCtrl);

lpwrCgElcgStateChange_AD10B_exit:

    return status;
}
