/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lwlinktu10x.c
 * @brief  Contains LWLINK routines specific to TU10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_bar0.h"

#include "dev_lwltlc.h"
#include "dev_lwltlc_ip.h"
#include "dev_pwr_csb.h"
#include "dev_hshub.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_bif_private.h"
#include "pmu_objbif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

#if defined(LW_PLWLTLC_TX6)
// When more links become available, we need to add them to this.
# error "More TLC registers than expected."
#elif defined(LW_PLWLTLC_TX5)
# define BIF_LWLINK_LINK_COUNT 6
#elif defined(LW_PLWLTLC_TX4)
# define BIF_LWLINK_LINK_COUNT 5
#elif defined(LW_PLWLTLC_TX3)
# define BIF_LWLINK_LINK_COUNT 4
#elif defined(LW_PLWLTLC_TX2)
# define BIF_LWLINK_LINK_COUNT 3
#elif defined(LW_PLWLTLC_TX1)
# define BIF_LWLINK_LINK_COUNT 2
#elif defined(LW_PLWLTLC_TX0)
# define BIF_LWLINK_LINK_COUNT 1
#else
// If there's no lwlink, this file shouldn't have been compiled in the first place.
# error "No LWLINK TLC registers?"
#endif

#define TLC_REG_ORIGIN DRF_BASE(LW_PLWLTLC_TX0)

#if BIF_LWLINK_LINK_COUNT > 1
# define TLC_REG_STRIDE (DRF_BASE(LW_PLWLTLC_TX1) - TLC_REG_ORIGIN)
#else
# define TLC_REG_STRIDE 0
#endif

#define TLC_REG_BASE(linkIdx)                   (TLC_REG_ORIGIN + TLC_REG_STRIDE * linkIdx)
#define TLC_REG_WR32(linkIdx, offset, value)    REG_WR32(BAR0, TLC_REG_BASE(linkIdx) + (offset), (value))
#define TLC_REG_RD32(linkIdx, offset)           REG_RD32(BAR0, TLC_REG_BASE(linkIdx) + (offset))

#define BIF_LWLINK_LPWR_LINK_ENABLED(i) (Bif.lwlinkLpwrEnableMask & BIT(i))

#define BIF_LWLINK_LINK_MASK (BIT(BIF_LWLINK_LINK_COUNT) - 1)

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

// TODO: VBIOS access. All access is commented out, and obvious enough, for when it gets done.

/*!
 * @brief Enable and configure Lpwr settings on links.
 *
 * @param[in]   changeMask  Bitmask of links to change enabled state.
 * @param[in]   newLinks    New bitmask of enabled links (masked against changeMask).
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
FLCN_STATUS
bifLwlinkLpwrLinkEnable_TU10X
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
        goto bifLwlinkLpwrLinkEnable_TU10X_end;
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
        goto bifLwlinkLpwrLinkEnable_TU10X_end;
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

bifLwlinkLpwrLinkEnable_TU10X_end:
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
bifLwlinkTlcLpwrSet_TU10X
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
        goto _bifLwlinkTlcLpwrSet_TU10X_end;
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
_bifLwlinkTlcLpwrSet_TU10X_end:
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
bifLwlinkTlcLpwrLinkInit_TU10X
(
    LwU32 linkIdx
)
{
    FLCN_STATUS returnStatus = FLCN_OK;

    if (linkIdx >= BIF_LWLINK_LINK_COUNT)
    {
        returnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto _bifLwlinkTlcLpwrLinkInit_TU10X_end;
    }
    if (!BIF_LWLINK_LPWR_LINK_ENABLED(linkIdx))
    {
        returnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto _bifLwlinkTlcLpwrLinkInit_TU10X_end;
    }

    // If the changing is not suspended, write out the register values.
    if (!Bif.bLwlinkSuspend)
    {
        // Power Management threshold settings
        TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_PWRM_IC_LIMIT,
                     DRF_DEF(_LWLTLC, _TX_PWRM_IC_LIMIT, _LIMIT, __PROD));

        TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_PWRM_IC_LIMIT,
                     DRF_DEF(_LWLTLC, _RX_PWRM_IC_LIMIT, _LIMIT, __PROD));

        TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_PWRM_IC_INC,
                     DRF_DEF(_LWLTLC, _TX_PWRM_IC_INC, _FBINC, __PROD) |
                     DRF_DEF(_LWLTLC, _TX_PWRM_IC_INC, _LPINC, __PROD));

        TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_PWRM_IC_INC,
                     DRF_DEF(_LWLTLC, _RX_PWRM_IC_INC, _FBINC, __PROD) |
                     DRF_DEF(_LWLTLC, _RX_PWRM_IC_INC, _LPINC, __PROD));

        TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_PWRM_IC_DEC,
                     DRF_DEF(_LWLTLC, _TX_PWRM_IC_DEC, _FBDEC, __PROD) |
                     DRF_DEF(_LWLTLC, _TX_PWRM_IC_DEC, _LPDEC, __PROD));

        TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_PWRM_IC_DEC,
                     DRF_DEF(_LWLTLC, _RX_PWRM_IC_DEC, _FBDEC, __PROD) |
                     DRF_DEF(_LWLTLC, _RX_PWRM_IC_DEC, _LPDEC, __PROD));

        TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_PWRM_IC_LP_ENTER_THRESHOLD,
                     DRF_DEF(_LWLTLC, _TX_PWRM_IC_LP_ENTER_THRESHOLD, _THRESHOLD, __PROD));

        TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_PWRM_IC_LP_ENTER_THRESHOLD,
                     DRF_DEF(_LWLTLC, _RX_PWRM_IC_LP_ENTER_THRESHOLD, _THRESHOLD, __PROD));

        // These _PROD values are not defined on some chips, therefore should not be written for them.
#ifdef LW_LWLTLC_TX_PWRM_IC_LP_EXIT_THRESHOLD_THRESHOLD__PROD
        TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_PWRM_IC_LP_EXIT_THRESHOLD,
                     DRF_DEF(_LWLTLC, _TX_PWRM_IC_LP_EXIT_THRESHOLD, _THRESHOLD, __PROD));
#endif

#ifdef LW_LWLTLC_RX_PWRM_IC_LP_EXIT_THRESHOLD_THRESHOLD__PROD
        TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_PWRM_IC_LP_EXIT_THRESHOLD,
                     DRF_DEF(_LWLTLC, _RX_PWRM_IC_LP_EXIT_THRESHOLD, _THRESHOLD, __PROD));
#endif

        // Count start must be at the end.

        TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_PWRM_IC_SW_CTRL,
                     FLD_SET_DRF(_LWLTLC, _TX_PWRM_IC_SW_CTRL, _COUNTSTART, __PROD,
                                 TLC_REG_RD32(linkIdx, LW_LWLTLC_TX_PWRM_IC_SW_CTRL)));

        TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_PWRM_IC_SW_CTRL,
                     FLD_SET_DRF(_LWLTLC, _RX_PWRM_IC_SW_CTRL, _COUNTSTART, __PROD,
                                 TLC_REG_RD32(linkIdx, LW_LWLTLC_RX_PWRM_IC_SW_CTRL)));

        returnStatus = bifLwlinkPwrMgmtToggle_TU10X(linkIdx, Bif.lwlinkLpwrSetMask & BIT(linkIdx));
    }

_bifLwlinkTlcLpwrLinkInit_TU10X_end:
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
bifLwlinkPwrMgmtToggle_TU10X
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
        goto _bifLwlinkPwrMgmtToggle_TU10X_end;
    }
    if (!BIF_LWLINK_LPWR_LINK_ENABLED(linkIdx))
    {
        returnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto _bifLwlinkPwrMgmtToggle_TU10X_end;
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

        tempRegVal = TLC_REG_RD32(linkIdx, LW_LWLTLC_TX_PWRM_IC_SW_CTRL);
        tempRegVal = FLD_SET_DRF_NUM(_LWLTLC, _TX_PWRM_IC_SW_CTRL, _SOFTWAREDESIRED, softwareDesired, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_LWLTLC, _TX_PWRM_IC_SW_CTRL, _HARDWAREDISABLE, hardwareDisable, tempRegVal);
        TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_PWRM_IC_SW_CTRL, tempRegVal);

        tempRegVal = TLC_REG_RD32(linkIdx, LW_LWLTLC_RX_PWRM_IC_SW_CTRL);
        tempRegVal = FLD_SET_DRF_NUM(_LWLTLC, _RX_PWRM_IC_SW_CTRL, _SOFTWAREDESIRED, softwareDesired, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_LWLTLC, _RX_PWRM_IC_SW_CTRL, _HARDWAREDISABLE, hardwareDisable, tempRegVal);
        TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_PWRM_IC_SW_CTRL, tempRegVal);
    }

_bifLwlinkPwrMgmtToggle_TU10X_end:
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
bifLwlinkLpwrSuspend_TU10X
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
bifLwlinkTlcLpwrSetOne_TU10X
(
    LwU32 linkIdx
)
{
    // IC Limit
    LwU32 icLimit = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].idleCounterSaturationLimit;

    TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_PWRM_IC_LIMIT,
                 DRF_NUM(_LWLTLC, _TX_PWRM_IC_LIMIT, _LIMIT, icLimit));

    TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_PWRM_IC_LIMIT,
                 DRF_NUM(_LWLTLC, _RX_PWRM_IC_LIMIT, _LIMIT, icLimit));

    //IC Inc
    LwU32 fbIcInc = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].fbIdleCounterIncrement;
    LwU32 lpIcInc = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].lpIdleCounterIncrement;

    TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_PWRM_IC_INC,
                 DRF_NUM(_LWLTLC, _TX_PWRM_IC_INC, _FBINC, fbIcInc) |
                 DRF_NUM(_LWLTLC, _TX_PWRM_IC_INC, _LPINC, lpIcInc));

    TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_PWRM_IC_INC,
                 DRF_NUM(_LWLTLC, _RX_PWRM_IC_INC, _FBINC, fbIcInc) |
                 DRF_NUM(_LWLTLC, _RX_PWRM_IC_INC, _LPINC, lpIcInc));

    //IC Dec
    LwU32 fbIcDec = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].fbIdleCounterDecrement;
    LwU32 lpIcDec = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].lpIdleCounterDecrement;

    TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_PWRM_IC_DEC,
                 DRF_NUM(_LWLTLC, _TX_PWRM_IC_DEC, _FBDEC, fbIcDec) |
                 DRF_NUM(_LWLTLC, _TX_PWRM_IC_DEC, _LPDEC, lpIcDec));

    TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_PWRM_IC_DEC,
                 DRF_NUM(_LWLTLC, _RX_PWRM_IC_DEC, _FBDEC, fbIcDec) |
                 DRF_NUM(_LWLTLC, _RX_PWRM_IC_DEC, _LPDEC, lpIcDec));

    //IC Enter Threshold
    LwU32 lpEntryThreshold = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].lpEntryThreshold;

    TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_PWRM_IC_LP_ENTER_THRESHOLD,
                 DRF_NUM(_LWLTLC, _TX_PWRM_IC_LP_ENTER_THRESHOLD, _THRESHOLD, lpEntryThreshold));

    TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_PWRM_IC_LP_ENTER_THRESHOLD,
                 DRF_NUM(_LWLTLC, _RX_PWRM_IC_LP_ENTER_THRESHOLD, _THRESHOLD, lpEntryThreshold));

    //IC Exit Threshold
    LwU32 lpExitThreshold = 0;//pLpwr->vbiosData.lwlink.entry[lwlinkIdx].lpExitThreshold;

    TLC_REG_WR32(linkIdx, LW_LWLTLC_TX_PWRM_IC_LP_EXIT_THRESHOLD,
                 DRF_NUM(_LWLTLC, _TX_PWRM_IC_LP_EXIT_THRESHOLD, _THRESHOLD, lpExitThreshold));

    TLC_REG_WR32(linkIdx, LW_LWLTLC_RX_PWRM_IC_LP_EXIT_THRESHOLD,
                 DRF_NUM(_LWLTLC, _RX_PWRM_IC_LP_EXIT_THRESHOLD, _THRESHOLD, lpExitThreshold));

    return bifLwlinkPwrMgmtToggle_TU10X(linkIdx, Bif.lwlinkLpwrSetMask & BIT(linkIdx));
}

/*!
 * @brief Update the HSHUB config registers
 *
 * @param[in]   config0    HSHUB config0 register
 * @param[in]   config1    HSHUB config1 register
 * @param[in]   config2    HSHUB config2 register
 * @param[in]   config6    HSHUB config6 register
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
FLCN_STATUS
bifLwlinkHshubUpdate_TU10X
(
    LwU32 config0,
    LwU32 config1,
    LwU32 config2,
    LwU32 config6
)
{
    // Get the current mask of peers on PCIe
    LwU32 lwrPeerPcieMask = DRF_VAL(_PFB_HSHUB, _CONFIG0, _PEER_PCIE_MASK,
                                    BAR0_REG_RD32(LW_PFB_HSHUB_CONFIG0));

    // Get the updated mask of peers on PCIe
    LwU32 newPeerPcieMask = DRF_VAL(_PFB_HSHUB, _CONFIG0, _PEER_PCIE_MASK,
                                    config0);

    //
    // While switching HSHUB, the config registers should be updated
    // together in order to avoid any race with the flushes
    //
    appTaskCriticalEnter();
    {
        // Update HSHUB config6 register
        BAR0_REG_WR32(LW_PFB_HSHUB_CONFIG6, config6);

        //
        // If a peer is being switched from LWLink to PCIe, update
        // config0 after updating config1 and config2 registers
        //
        if ((lwrPeerPcieMask & newPeerPcieMask) != newPeerPcieMask)
        {
            BAR0_REG_WR32(LW_PFB_HSHUB_CONFIG1, config1);
            BAR0_REG_WR32(LW_PFB_HSHUB_CONFIG2, config2);
            BAR0_REG_WR32(LW_PFB_HSHUB_CONFIG0, config0);
        }
        else // Update config0 before config1 and config2
        {
            BAR0_REG_WR32(LW_PFB_HSHUB_CONFIG0, config0);
            BAR0_REG_WR32(LW_PFB_HSHUB_CONFIG1, config1);
            BAR0_REG_WR32(LW_PFB_HSHUB_CONFIG2, config2);
        }
    }
    appTaskCriticalExit();

    return FLCN_OK;
}
