/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lwlinkga100.c
 * @brief  Contains LWLINK routines specific to GA100.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_bar0.h"

#include "dev_lwltlc_ip.h"
#include "dev_pwr_csb.h"
#include "ioctrl_discovery.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_bif_private.h"
#include "pmu_objbif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */

/*!
 * Macro to get total number of Lwlinks Supported in the platform
 */
#if defined(LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLTLC_MULTI)
#define LWLINK_COUNT_IOCTRL_0 4
#else
#define LWLINK_COUNT_IOCTRL_0 0
#endif

#if defined(LW_DISCOVERY_IOCTRL_UNICAST_1_SW_DEVICE_BASE_LWLTLC_MULTI)
#define LWLINK_COUNT_IOCTRL_1 4
#else
#define LWLINK_COUNT_IOCTRL_1 0
#endif

#if defined(LW_DISCOVERY_IOCTRL_UNICAST_2_SW_DEVICE_BASE_LWLTLC_MULTI)
#define LWLINK_COUNT_IOCTRL_2 4
#else
#define LWLINK_COUNT_IOCTRL_2 0
#endif

#define LWLINK_COUNT_IOCTRL (LWLINK_COUNT_IOCTRL_0 + LWLINK_COUNT_IOCTRL_1 + LWLINK_COUNT_IOCTRL_2)
LwU32 baseOffsetTl[LWLINK_COUNT_IOCTRL] GCC_ATTRIB_SECTION("dmem_bif", "baseOffsetTl");
/* ------------------------- Macros and Defines ----------------------------- */

#define BIF_LWLINK_LINK_COUNT                   LWLINK_COUNT_IOCTRL
#define TLC_REG_BASE(linkIdx)                   (baseOffsetTl[linkIdx])
#define TLC_REG_WR32(linkIdx, offset, value)    REG_WR32(BAR0, TLC_REG_BASE(linkIdx) + (offset), (value))
#define TLC_REG_RD32(linkIdx, offset)           REG_RD32(BAR0, TLC_REG_BASE(linkIdx) + (offset))

#define BIF_LWLINK_LPWR_LINK_ENABLED(i) (Bif.lwlinkLpwrEnableMask & BIT(i))

#define BIF_LWLINK_LINK_MASK (BIT(BIF_LWLINK_LINK_COUNT) - 1)

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

// TODO: VBIOS access. All access is commented out, and obvious enough, for when it gets done.

/*!
 * @brief Enable and configure  Lpwr settings on links.
 *
 * @param[in]   changeMask  Bitmask of links to change enabled state.
 * @param[in]   newLinks    New bitmask of enabled links (masked against changeMask).
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
FLCN_STATUS
bifLwlinkLpwrLinkEnable_GA100
(
    LwU32 changeMask,
    LwU32 newLinks
)
{
    FLCN_STATUS returnStatus = FLCN_OK;
    FLCN_STATUS status = FLCN_OK;
    LwU32 i;

    newLinks &= changeMask;

    if ((newLinks & ~BIF_LWLINK_LINK_MASK) != 0)
    {
        // New mask has bits for links that aren't possible.
        returnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto bifLwlinkLpwrLinkEnable_GA100_end;
    }

    // TODO: Verify that the newly enabled links are actually present, so that we won't choke.

    // Enable the links
    Bif.lwlinkLpwrEnableMask = (Bif.lwlinkLpwrEnableMask & ~changeMask) | newLinks;

    // Apply the lpwr settings on the newly enabled links
    Bif.lwlinkLpwrSetMask = (Bif.lwlinkLpwrSetMask & ~changeMask) | newLinks;

    // Links are only being disabled, we're done.
    if (newLinks == 0)
    {
        returnStatus = FLCN_OK;
        goto bifLwlinkLpwrLinkEnable_GA100_end;
    }

    for (i = 0; i < BIF_LWLINK_LINK_COUNT; i++)
    {
        if (Bif.lwlinkVbiosIdx == BIF_LWLINK_VBIOS_INDEX_UNSET)
        {
            // Use defaults if LPWR setting hasn't been sent.
            status = bifLwlinkTlcLpwrLinkInit_HAL(&Bif, i);
        }
        else
        {
            // Otherwise, apply the setting.
            status = bifLwlinkTlcLpwrSetOne_HAL(&Bif, i);
        }

        if (status != FLCN_OK)
        {
            returnStatus = FLCN_ERROR;
        }
    }

bifLwlinkLpwrLinkEnable_GA100_end:
    return returnStatus;
}

/*!
 * @brief Set LWLINK TLC low power settings.
 *
 * @param[in]   vbiosIdx    VBIOS table index for the new setting.
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
FLCN_STATUS
bifLwlinkTlcLpwrSet_GA100
(
    LwU32 vbiosIdx
)
{
    FLCN_STATUS returnStatus = FLCN_OK;
    FLCN_STATUS status = FLCN_OK;
    LwU32 i;

    // TODO: Validate index.

    Bif.lwlinkVbiosIdx = vbiosIdx;

    // If the changing is suspended, all we do is save the VBIOS table index.
    if (Bif.bLwlinkSuspend)
    {
        goto _bifLwlinkTlcLpwrSet_GA100_end;
    }

    if (vbiosIdx == BIF_LWLINK_VBIOS_INDEX_UNSET)
    {
        Bif.bLwlinkGlobalLpEnable = LW_TRUE;
    }
    else
    {
        Bif.bLwlinkGlobalLpEnable = LW_FALSE;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].bLpEnable;
    }

    for (i = 0; i < BIF_LWLINK_LINK_COUNT; i++)
    {
        // Do not try to change settings on links that are not configured.
        if (!BIF_LWLINK_LPWR_LINK_ENABLED(i))
        {
            continue;
        }

        // Set to _PROD values if the VBIOS table index hasn't been set.
        if (vbiosIdx == BIF_LWLINK_VBIOS_INDEX_UNSET)
        {
            status = bifLwlinkTlcLpwrLinkInit_HAL(&Bif, i);
        }
        else
        {
            status = bifLwlinkTlcLpwrSetOne_HAL(&Bif, i);
        }

        if (status != FLCN_OK)
        {
            returnStatus = FLCN_ERROR;
        }
    }
_bifLwlinkTlcLpwrSet_GA100_end:
    return returnStatus;
}

/*!
 * @brief Initialize a newly setup link with default values.
 *
 * @param[in]   linkIdx     Link index to initialize.
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
FLCN_STATUS
bifLwlinkTlcLpwrLinkInit_GA100
(
    LwU32 linkIdx
)
{
    FLCN_STATUS returnStatus = FLCN_OK;

    if (linkIdx >= BIF_LWLINK_LINK_COUNT)
    {
        returnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto _bifLwlinkTlcLpwrLinkInit_GA100_end;
    }
    if (!BIF_LWLINK_LPWR_LINK_ENABLED(linkIdx))
    {
        returnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto _bifLwlinkTlcLpwrLinkInit_GA100_end;
    }

    // If the changing is not suspended, write out the register values.
    if (!Bif.bLwlinkSuspend)
    {
        returnStatus = bifLwlinkPwrMgmtToggle_GA100(linkIdx, Bif.lwlinkLpwrSetMask & BIT(linkIdx));
    }

_bifLwlinkTlcLpwrLinkInit_GA100_end:
    return returnStatus;
}

/*!
 * @brief Toggle whether 1/8th mode may be set.
 *
 * @param[in]   linkIdx     Link index to toggle.
 * @param[in]   bEnable     If true, 1/8th mode support will be enabled.
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
FLCN_STATUS
bifLwlinkPwrMgmtToggle_GA100
(
    LwU32 linkIdx,
    LwBool bEnable
)
{
    FLCN_STATUS returnStatus = FLCN_OK;
    LwU32 tempRegVal;

    if (linkIdx >= BIF_LWLINK_LINK_COUNT)
    {
        returnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto _bifLwlinkPwrMgmtToggle_GA100_end;
    }
    if (!BIF_LWLINK_LPWR_LINK_ENABLED(linkIdx))
    {
        returnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto _bifLwlinkPwrMgmtToggle_GA100_end;
    }

    if (bEnable)
    {
        Bif.lwlinkLpwrSetMask |= BIT(linkIdx);
    }
    else
    {
        Bif.lwlinkLpwrSetMask &= ~BIT(linkIdx);
    }

    // If the changing is not suspended, write the registers.
    if (!Bif.bLwlinkSuspend)
    {
        //
        // If the VBIOS entry says LP entry is enabled, _and_ the parameter is true, then enable it.
        // Otherwise, it will be forced off.
        //
        LwU8 softwareDesired = (bEnable && Bif.bLwlinkGlobalLpEnable) ? 0x1 : 0x0;
        LwU8 hardwareDisable = (bEnable && Bif.bLwlinkGlobalLpEnable) ? 0x0 : 0x1;

        tempRegVal = TLC_REG_RD32(linkIdx, LW_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL);
        tempRegVal = FLD_SET_DRF_NUM(_LWLTLC, _TX_LNK_PWRM_IC_SW_CTRL, _SOFTWAREDESIRED, softwareDesired, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_LWLTLC, _TX_LNK_PWRM_IC_SW_CTRL, _HARDWAREDISABLE, hardwareDisable, tempRegVal);
        TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL, tempRegVal);

        tempRegVal = TLC_REG_RD32(linkIdx, LW_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL);
        tempRegVal = FLD_SET_DRF_NUM(_LWLTLC, _RX_LNK_PWRM_IC_SW_CTRL, _SOFTWAREDESIRED, softwareDesired, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_LWLTLC, _RX_LNK_PWRM_IC_SW_CTRL, _HARDWAREDISABLE, hardwareDisable, tempRegVal);
        TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL, tempRegVal);
    }

_bifLwlinkPwrMgmtToggle_GA100_end:
    return returnStatus;
}

/*!
 * @brief Suspend (or resume) PMU controlling LWLINK 1/8th mode.
 *
 * When suspended, PMU will continue to accept commands and cache the state.
 * When resumed, the new state will be applied immediately.
 *
 * @param[in]   bSuspend    New control state.
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
FLCN_STATUS
bifLwlinkLpwrSuspend_GA100
(
    LwBool bSuspend
)
{
    Bif.bLwlinkSuspend = bSuspend;

    // If resuming, set the TLC LPWR state.
    if (!bSuspend)
    {
        return bifLwlinkTlcLpwrSet_HAL(&Bif, Bif.lwlinkVbiosIdx);
    }
    return FLCN_OK;
}

/*!
 * @brief Set LWLINK TLC low power settings for one link.
 *
 * @param[in]   linkIdx     Link index to configure.
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
FLCN_STATUS
bifLwlinkTlcLpwrSetOne_GA100
(
    LwU32 linkIdx
)
{
    // IC Limit
    LwU32 icLimit = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].idleCounterSaturationLimit;

    TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_LNK_PWRM_IC_LIMIT,
                 DRF_NUM(_LWLTLC, _TX_LNK_PWRM_IC_LIMIT, _LIMIT, icLimit));

    TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_LNK_PWRM_IC_LIMIT,
                 DRF_NUM(_LWLTLC, _RX_LNK_PWRM_IC_LIMIT, _LIMIT, icLimit));

    //IC Inc
    LwU32 fbIcInc = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].fbIdleCounterIncrement;
    LwU32 lpIcInc = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].lpIdleCounterIncrement;

    TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_LNK_PWRM_IC_INC,
                 DRF_NUM(_LWLTLC, _TX_LNK_PWRM_IC_INC, _FBINC, fbIcInc) |
                 DRF_NUM(_LWLTLC, _TX_LNK_PWRM_IC_INC, _LPINC, lpIcInc));

    TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_LNK_PWRM_IC_INC,
                 DRF_NUM(_LWLTLC, _RX_LNK_PWRM_IC_INC, _FBINC, fbIcInc) |
                 DRF_NUM(_LWLTLC, _RX_LNK_PWRM_IC_INC, _LPINC, lpIcInc));

    //IC Dec
    LwU32 fbIcDec = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].fbIdleCounterDecrement;
    LwU32 lpIcDec = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].lpIdleCounterDecrement;

    TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_LNK_PWRM_IC_DEC,
                 DRF_NUM(_LWLTLC, _TX_LNK_PWRM_IC_DEC, _FBDEC, fbIcDec) |
                 DRF_NUM(_LWLTLC, _TX_LNK_PWRM_IC_DEC, _LPDEC, lpIcDec));

    TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_LNK_PWRM_IC_DEC,
                 DRF_NUM(_LWLTLC, _RX_LNK_PWRM_IC_DEC, _FBDEC, fbIcDec) |
                 DRF_NUM(_LWLTLC, _RX_LNK_PWRM_IC_DEC, _LPDEC, lpIcDec));

    //IC Enter Threshold
    LwU32 lpEntryThreshold = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].lpEntryThreshold;

    TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_LNK_PWRM_IC_LP_ENTER_THRESHOLD,
                 DRF_NUM(_LWLTLC, _TX_LNK_PWRM_IC_LP_ENTER_THRESHOLD, _THRESHOLD, lpEntryThreshold));

    TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_LNK_PWRM_IC_LP_ENTER_THRESHOLD,
                 DRF_NUM(_LWLTLC, _RX_LNK_PWRM_IC_LP_ENTER_THRESHOLD, _THRESHOLD, lpEntryThreshold));

    //IC Exit Threshold
    LwU32 lpExitThreshold = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].lpExitThreshold;

    TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_LNK_PWRM_IC_LP_EXIT_THRESHOLD,
                 DRF_NUM(_LWLTLC, _TX_LNK_PWRM_IC_LP_EXIT_THRESHOLD, _THRESHOLD, lpExitThreshold));

    TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_LNK_PWRM_IC_LP_EXIT_THRESHOLD,
                 DRF_NUM(_LWLTLC, _RX_LNK_PWRM_IC_LP_EXIT_THRESHOLD, _THRESHOLD, lpExitThreshold));

    return bifLwlinkPwrMgmtToggle_GA100(linkIdx, Bif.lwlinkLpwrSetMask & BIT(linkIdx));
}

/*!
 * @brief Update TLC base addresses
 */

FLCN_STATUS
bifUpdateTlcBaseOffsets_GA100(void)
{
    LwU32 offset = 0;

    if (LWLINK_COUNT_IOCTRL == 0)
    {
        return FLCN_OK;
    }

#if defined LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLTLC_MULTI
    baseOffsetTl[offset + 0] = LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLTLC_0;
    baseOffsetTl[offset + 1] = LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLTLC_1;
    baseOffsetTl[offset + 2] = LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLTLC_2;
    baseOffsetTl[offset + 3] = LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLTLC_3;
    offset += 4;
#endif
#if defined LW_DISCOVERY_IOCTRL_UNICAST_1_SW_DEVICE_BASE_LWLTLC_MULTI
    baseOffsetTl[offset + 0] = LW_DISCOVERY_IOCTRL_UNICAST_1_SW_DEVICE_BASE_LWLTLC_0;
    baseOffsetTl[offset + 1] = LW_DISCOVERY_IOCTRL_UNICAST_1_SW_DEVICE_BASE_LWLTLC_1;
    baseOffsetTl[offset + 2] = LW_DISCOVERY_IOCTRL_UNICAST_1_SW_DEVICE_BASE_LWLTLC_2;
    baseOffsetTl[offset + 3] = LW_DISCOVERY_IOCTRL_UNICAST_1_SW_DEVICE_BASE_LWLTLC_3;
    offset += 4;
#endif
#if defined LW_DISCOVERY_IOCTRL_UNICAST_2_SW_DEVICE_BASE_LWLTLC_MULTI
    baseOffsetTl[offset + 0] = LW_DISCOVERY_IOCTRL_UNICAST_2_SW_DEVICE_BASE_LWLTLC_0;
    baseOffsetTl[offset + 1] = LW_DISCOVERY_IOCTRL_UNICAST_2_SW_DEVICE_BASE_LWLTLC_1;
    baseOffsetTl[offset + 2] = LW_DISCOVERY_IOCTRL_UNICAST_2_SW_DEVICE_BASE_LWLTLC_2;
    baseOffsetTl[offset + 3] = LW_DISCOVERY_IOCTRL_UNICAST_2_SW_DEVICE_BASE_LWLTLC_3;
    offset += 4;
#endif
    return FLCN_OK;
}
