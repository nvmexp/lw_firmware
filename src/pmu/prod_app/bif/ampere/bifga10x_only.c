/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   bifga10x_only.c
 * @brief  Contains HAL implementations for GA10x(GA102 and later) only.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_objpmu.h"

#include "dev_lw_xp.h"
#include "dev_lw_xp_addendum.h"
#include "dev_top.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "config/g_bif_private.h"
#include "pmu_objbif.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

static LwBool s_bifEomDoneStatus_GA10X(void *pArgs)
    GCC_ATTRIB_SECTION("imem_bif", "s_bifEomDoneStatus_GA10X");

/*!
 * @brief Run EOM sequence and fill the output buffer with EOM status values
 *
 * @Note: This sequence is same as sequence in TuringPcie::DoGetEomStatus in MODS
 *
 * @param[in]   eomMode      Mode on EOM
 * @param[in]   eomNblks     Number of blocks
 * @param[in]   eomNerrs     Number of errors
 * @param[in]   eomBerEyeSel BER eye select
 * @param[in]   eomPamEyeSel PAM eye select
 * @param[in]   laneMask     Lanemask specifying which lanes needs to be selected
 *                           for running EOM sequence
 * @params[out] pEomStatus   Pointer to output buffer
 *
 * @return FLCN_OK  If EOM sequence is run correctly
 */
FLCN_STATUS
bifGetEomStatus_GA10X
(
    LwU8   eomMode,
    LwU8   eomNblks,
    LwU8   eomNerrs,
    LwU8   eomBerEyeSel,
    LwU8   eomPamEyeSel,
    LwU32  laneMask,
    LwU16  *pEomStatus
)
{
    LwU8 laneNum;
    LwU32 regVal;
    LwBool bEomDoneStatus;
    LwU8 numRequestedLanes = 0U;
    LwU8 firstLaneFromLsb = LW_U8_MAX;
    FLCN_STATUS status = FLCN_OK;

    if (laneMask == 0U)
    {
        // Nothing to do
        goto bifGetEomStatus_GA10X_exit;
    }

    for (laneNum = 0; laneNum < BIF_MAX_PCIE_LANES; laneNum++)
    {
        if ((BIT(laneNum) & laneMask) != 0U)
        {
            if (firstLaneFromLsb == LW_U8_MAX)
            {
                firstLaneFromLsb = laneNum;
            }
            numRequestedLanes++;
        }
    }

    if (numRequestedLanes > BIF_MAX_PCIE_LANES ||
        (firstLaneFromLsb + numRequestedLanes) > BIF_MAX_PCIE_LANES)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto bifGetEomStatus_GA10X_exit;
    }

    // Set EOM params for requested lanes
    bifDoLaneSelect_HAL(&Bif, laneMask);
    bifSetEomParameters_HAL(&Bif, eomMode, eomNblks, eomNerrs,
                            eomBerEyeSel, eomPamEyeSel);

    // Disable EOM for requested lanes.
    bifDoLaneSelect_HAL(&Bif, laneMask);
    regVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_5);
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_5, _RX_EOM_EN,
                         _DISABLE, regVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_5, regVal);

    // Poll for EOM done status to be FALSE for all requested lanes
    bEomDoneStatus = LW_FALSE;
    status = bifPollEomDoneStatus_HAL(&Bif, numRequestedLanes, firstLaneFromLsb,
                bEomDoneStatus, LW_UPHY_EOM_DONE_FALSE_TIMEOUT_NS);
    if (status != FLCN_OK)
    {
        goto bifGetEomStatus_GA10X_exit;
    }

    // Enable EOM for requested lanes.
    bifDoLaneSelect_HAL(&Bif, laneMask);
    regVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_5);
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_5, _RX_EOM_OVRD, _ENABLE, regVal);
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_5, _RX_EOM_EN, _ENABLE, regVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_5, regVal);

    bEomDoneStatus = LW_TRUE;
    status = bifPollEomDoneStatus_HAL(&Bif, numRequestedLanes, firstLaneFromLsb,
                bEomDoneStatus, LW_UPHY_EOM_DONE_TRUE_TIMEOUT_NS);
    if (status != FLCN_OK)
    {
        goto bifGetEomStatus_GA10X_exit;
    }

    // Disable EOM for requested lanes
    bifDoLaneSelect_HAL(&Bif, laneMask);
    regVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_5);
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_5, _RX_EOM_EN, _DISABLE, regVal);
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_5, _RX_EOM_OVRD, _DISABLE, regVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_5, regVal);

    // Fill up EOM status array starting from index 0 always.
    for (laneNum = firstLaneFromLsb;
         laneNum < (firstLaneFromLsb + numRequestedLanes); laneNum++)
    {
        bifDoLaneSelect_HAL(&Bif, BIT(laneNum));
        pEomStatus[laneNum] = DRF_VAL(_XP, _PEX_PAD_CTL_5, _RX_EOM_STATUS,
                                      REG_RD32(FECS, LW_XP_PEX_PAD_CTL_5));
    }

bifGetEomStatus_GA10X_exit:
    return status;
}

/*!
 * @brief Select requsted PCIe lanes.
 *
 * @param[in] laneMask  Bit mask representing requested lanes. If 0th lane needs
 *                      to be selected, 0th bit should be set in the laneMask.
 *                      If 0th and 1st lane need to be selected, 0th and 1st
 *                      bits need to be set and so on.
 *
 * @return FLCN_OK
 */
FLCN_STATUS
bifDoLaneSelect_GA10X
(
    LwU32 laneMask
)
{
    LwU32 regVal;
    regVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_SEL);
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_SEL, _LANE_SELECT,
                             laneMask, regVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_SEL, regVal);
    return FLCN_OK;
}

/*!
 * @brief Poll EOM done status for requested lanes
 *
 * @param[in] numRequestedLanes  Number of requested lanes for which EOM done
 *                               status needs to be polled
 * @param[in]  firstLaneFromLsb  First lane number from LSB for which EOM done
 *                               status needs to be polled
 * @param[in]  bEomDoneStatus    Poll for EOM done status to be set to this value
 * @param[in]  timeoutNs         Timeout in nanoseconds after which this should
 *                               break out of polling loop
 *
 * @return FLCN_OK  If all requested lanes have expected EOM status
 */
FLCN_STATUS
bifPollEomDoneStatus_GA10X
(
    LwU8   numRequestedLanes,
    LwU8   firstLaneFromLsb,
    LwBool bEomDoneStatus,
    LwU32  timeoutNs
)
{
    LwU8 laneNum;
    LwBool bIsFmodel;
    FLCN_STATUS status = FLCN_OK;

    bIsFmodel = FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _FMODEL,
                             REG_RD32(BAR0, LW_PTOP_PLATFORM));

    // Poll for EOM done status to be set to FALSE
    for (laneNum = 0; laneNum < numRequestedLanes; laneNum++)
    {
        // Select one lane at a time here
        bifDoLaneSelect_HAL(&Bif, BIT(firstLaneFromLsb + laneNum));

        // Skip following polling on FMODEL
        if (!bIsFmodel)
        {
            if (!OS_PTIMER_SPIN_WAIT_NS_COND(s_bifEomDoneStatus_GA10X,
                    &bEomDoneStatus, timeoutNs))
            {
                //
                // TODO: Use mailbox register that would be used across
                // the PEX SW in PMU for indicating failure points
                //
                #if (PMU_PROFILE_GA10X_RISCV) // NJ??
                {
                    dbgPrintf(LEVEL_ERROR,
                              "Timed out waiting for EOM status to be set to %x\n",
                              bEomDoneStatus);
                }
                #endif
                PMU_BREAKPOINT();
                status = FLCN_ERR_TIMEOUT;
                goto bifPollEomDoneStatus_GA10X_exit;
            }
        }
    }

bifPollEomDoneStatus_GA10X_exit:
    return status;
}

/*!
 * @brief OS_PTIMER_COND_FUNC to check if EOM done status is set as expected
 *
 * @param[in] pArgs  Pointer to boolean indicating expected EOM done status
 *
 * @return  LW_TRUE  If EOM done status matches expected EOM done status
 */
static LwBool
s_bifEomDoneStatus_GA10X
(
    void *pArgs
)
{
    LwU32 regVal;
    const LwBool *pEomDoneStatus = pArgs;
    regVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_5);
    return (DRF_VAL(_XP, _PEX_PAD_CTL_5, _RX_EOM_DONE, regVal)) ==
           (*pEomDoneStatus);
}
