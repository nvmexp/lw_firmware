/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_dispgp10x.c
 * @brief  HAL-interface for the GP10x Display Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_disp.h"
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_disp.h"  /// XXXEB RECHECK LIST HERE
#include "pmu_objseq.h"
#include "pmu_objvbios.h"
#include "lw_ref_dev_sr.h"
#include "pmu_i2capi.h"
#include "task_i2c.h"
#include "dev_pmgr.h"
#include "dev_trim.h"
#include "main.h"
#include "config/g_disp_private.h"
#include "pmu_objdisp.h"
#include "displayport/dpcd.h"

/* ------------------------- Type Definitions ------------------------------- */
typedef enum
{
    padlink_A = 0,
    padlink_B = 1,
    padlink_C = 2,
    padlink_D = 3,
    padlink_E = 4,
    padlink_F = 5,
    padlink_G = 6,
    padlink_ilwalid
} LW_DISP_DFPPADLINK;

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
#if PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING)
static LwBool s_dispVgaCrRegIndexIsValid(LwU8 idx);
#endif // PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING)

/* ---------------------------Private functions ----------------------------- */
/*!
 * @brief       read a VGA register
 *
 * @param[in]   index    VGA register index
 *
 * @return      register value
 */
LwU8
dispReadVgaReg_GP10X
(
    LwU8 index
)
{
    LwU32 nAddr;
    LwU32 nShift;
    LwU32 nData;

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (!s_dispVgaCrRegIndexIsValid(index))
        {
            PMU_BREAKPOINT();
            return 0U;
        }
    }

    nAddr = (LW_PDISP_VGA_CR_REG0 + index) & ~0x3;
    nShift = (index & 0x3) * 8;
    nData = REG_RD32(BAR0, nAddr);
    nData >>= nShift;
    return (LwU8)nData;
}

/*!
 * @brief  write a VGA register
 *
 * @param[in]   index             VGA registerindex
 * @param[in]   value             value
 *
 * @return void
 */
void
dispWriteVgaReg_GP10X
(
    LwU8 index,
    LwU8 data
)
{
    LwU32 nAddr;
    LwU32 nShift;
    LwU32 nMask;
    LwU32 nDataOld;
    LwU32 dataNew;

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (!s_dispVgaCrRegIndexIsValid(index))
        {
            PMU_BREAKPOINT();
            return;
        }
    }

    nAddr = (LW_PDISP_VGA_CR_REG0 + index) & ~0x3;
    nShift = (index & 0x3) * 8;
    nMask = 0xFF << nShift;
    nDataOld = REG_RD32(BAR0, nAddr);
    nDataOld &= ~nMask;
    dataNew = ((LwU32)data) << nShift;
    dataNew |= nDataOld;
    PMU_BAR0_WR32_SAFE(nAddr, dataNew);
}

/*!
 * @brief       Reads DP aux register
 *
 * @param[in]   port         DP port
 * @param[in]   reg          Register offset
 * @param[in]   bWrite       Read (0) or write (1)
 * @param[out]  pData        Data being passed or returned
 *
 * @return      FLCN_OK if completed
 */
FLCN_STATUS
dispReadOrWriteRegViaAux_GP10X
(
    LwU8    port,
    LwU32   reg,
    LwBool  bWrite,
    LwU8   *pData
)
{
    I2C_INT_MESSAGE    msg;
    I2C_COMMAND        command;    // Command to submit to the server.
    FLCN_STATUS        status;

    command.hdr.eventType = I2C_EVENT_TYPE_DPAUX;
    command.dpaux.port = port;
    command.dpaux.cmd = bWrite ? LW_PMGR_DP_AUXCTL_CMD_AUXWR : LW_PMGR_DP_AUXCTL_CMD_AUXRD;
    command.dpaux.addr = reg;
    command.dpaux.size = 1;
    command.dpaux.pData = (LwU8 *)pData;
    command.dpaux.hQueue = DpAuxAckQueue;

    status = lwrtosQueueIdSend(LWOS_QUEUE_ID(PMU, I2C),
                               &command, sizeof(command), DP_AUX_WAIT_US);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Now wait for the AUX task to acknowledge completion of the
    // request by waiting for a response in the shared AUX queue.
    //
    if (FLCN_OK != lwrtosQueueReceive(DpAuxAckQueue, &msg, sizeof(msg), lwrtosMAX_DELAY))
    {
        return FLCN_ERR_TIMEOUT;
    }

    if (msg.errorCode != FLCN_OK)
    {
        return FLCN_ERR_AUX_ERROR;
    }

    return FLCN_OK;
}

/*!
 * @brief     Disable sparse self refresh
 *
 * @param[in] port  DP port
 *
 * @return    FLCN_STATUS FLCN_OK if exit sparse is completed
 */
FLCN_STATUS
dispPsrDisableSparseSr_GP10X
(
    LwU8 port
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 CtrlReg = 0;
    LwU32 StatReg = 0;

    // Check if SR is enabled.
    status = dispReadOrWriteRegViaAux_HAL(&Disp, port, LW_SR_STATUS, LW_FALSE, (LwU8 *)&StatReg);
    if (status != FLCN_OK)
    {
        return status;
    }

    if (FLD_TEST_DRF(_SR, _STATUS, _SR_STATE, _OFFLINE, StatReg))
    {
        // Nothing to do
        return FLCN_OK;
    }

    // For LWSR panels we use framelock method and resync delay as 1 frame.
    CtrlReg = 0;
    CtrlReg = FLD_SET_DRF(_SR, _RESYNC_CTL, _METHOD, _FL, CtrlReg);
    CtrlReg = FLD_SET_DRF_NUM(_SR, _RESYNC_CTL, _DELAY, 1, CtrlReg);
    status = dispReadOrWriteRegViaAux_HAL(&Disp, port, LW_SR_RESYNC_CTL, LW_TRUE, (LwU8 *)&CtrlReg);

    // Enable the sideband select
    CtrlReg = 0;
    CtrlReg = FLD_SET_DRF(_SR, _SRC_CTL1, _SIDEBAND_EXIT_SEL, _YES, CtrlReg);
    CtrlReg = FLD_SET_DRF(_SR, _SRC_CTL1, _SIDEBAND_EXIT_MASK, _ENABLE, CtrlReg);
    status = dispReadOrWriteRegViaAux_HAL(&Disp, port, LW_SR_SRC_CTL1, LW_TRUE, (LwU8 *)&CtrlReg);

    // Write the State control registers
    CtrlReg = 0;
    CtrlReg = FLD_SET_DRF(_SR, _SRC_CTL0, _SR_ENABLE_CTL, _ENABLED, CtrlReg);
    CtrlReg = FLD_SET_DRF(_SR, _SRC_CTL0, _SR_ENABLE_MASK, _ENABLE, CtrlReg);

    // Set the SR exit request (SEXR and SEXM)
    CtrlReg = FLD_SET_DRF(_SR, _SRC_CTL0, _SR_EXIT_REQ, _YES, CtrlReg);
    CtrlReg = FLD_SET_DRF(_SR, _SRC_CTL0, _SR_EXIT_MASK, _ENABLE, CtrlReg);
    status = dispReadOrWriteRegViaAux_HAL(&Disp, port, LW_SR_SRC_CTL0, LW_TRUE, (LwU8 *)&CtrlReg);

    return status;
}

/*!
 * @brief     Restore SOR power settings
 *
 * @param[in] padlink
 *
 * @return    FLCN_STATUS
 */
FLCN_STATUS
dispRestoreSorPwrSettings_GP10X(void)
{
    LwU32 regVal;

    regVal = REG_RD32(BAR0, LW_PDISP_SOR_PWR(Disp.pPmsBsodCtx->head.orIndex));
    regVal = FLD_SET_DRF(_PDISP, _SOR_PWR, _NORMAL_STATE, _PU, regVal);
    regVal = FLD_SET_DRF(_PDISP, _SOR_PWR, _NORMAL_START, _NORMAL, regVal);
    regVal = FLD_SET_DRF(_PDISP, _SOR_PWR, _SAFE_STATE, _PD, regVal);
    regVal = FLD_SET_DRF(_PDISP, _SOR_PWR, _SAFE_START, _NORMAL, regVal);
    regVal = FLD_SET_DRF(_PDISP, _SOR_PWR, _SETTING_NEW, _TRIGGER, regVal);
    PMU_BAR0_WR32_SAFE(LW_PDISP_SOR_PWR(Disp.pPmsBsodCtx->head.orIndex), regVal);

    // Poll for LW_PDISP_SOR_PWR_SETTING_NEW to be _DONE
    if (!PMU_REG32_POLL_US(USE_FECS(LW_PDISP_SOR_PWR(Disp.pPmsBsodCtx->head.orIndex)),
        DRF_SHIFTMASK(LW_PDISP_SOR_PWR_SETTING_NEW),
        LW_PDISP_SOR_PWR_SETTING_NEW_DONE,
        LW_DISP_CNTL_TRIGGER_UDPATE_DONE_TIME_US,
        PMU_REG_POLL_MODE_BAR0_EQ))
    {
        return FLCN_ERR_TIMEOUT;
    }

    return FLCN_OK;
}

/*!
 * @brief  restore the DPCD link settings
 *
 * @return FLCN_OK if success.
 */
FLCN_STATUS
dispSetDpcdLinkSettings_GP10X(void)
{
    LwU8            dpAuxData;
    LwBool          bSupportEnhancedFraming = LW_FALSE;
    FLCN_STATUS     status;

    status = dispReadOrWriteRegViaAux_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum, LW_DPCD_LINK_BANDWIDTH_SET, LW_FALSE, &dpAuxData);
    if (status != FLCN_OK)
    {
        return status;
    }
    dpAuxData = (LwU8)FLD_SET_DRF_NUM(_DPCD, _LINK_BANDWIDTH_SET, _VAL, Disp.pPmsBsodCtx->dpInfo.linkSpeedMul, dpAuxData);

    status = dispReadOrWriteRegViaAux_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum, LW_DPCD_LINK_BANDWIDTH_SET, LW_TRUE, &dpAuxData);
    if (status != FLCN_OK)
    {
        return status;
    }

    status = dispReadOrWriteRegViaAux_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum, LW_DPCD_MAX_LANE_COUNT, LW_FALSE, &dpAuxData);
    if (status != FLCN_OK)
    {
        return status;
    }
    if (FLD_TEST_DRF(_DPCD, _MAX_LANE_COUNT, _ENHANCED_FRAMING, _YES, dpAuxData))
    {
        bSupportEnhancedFraming = LW_TRUE;
    }

    status = dispReadOrWriteRegViaAux_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum, LW_DPCD_LANE_COUNT_SET, LW_FALSE, &dpAuxData);
    if (status != FLCN_OK)
    {
        return status;
    }
    if (bSupportEnhancedFraming)
    {
        dpAuxData = FLD_SET_DRF(_DPCD, _LANE_COUNT_SET, _ENHANCEDFRAMING, _TRUE, dpAuxData);
    }
    else
    {
        dpAuxData = FLD_SET_DRF(_DPCD, _LANE_COUNT_SET, _ENHANCEDFRAMING, _FALSE, dpAuxData);
    }

    dpAuxData = (LwU8)FLD_SET_DRF_NUM(_DPCD, _LANE_COUNT_SET, _LANE, Disp.pPmsBsodCtx->dpInfo.laneCount, dpAuxData);
    status = dispReadOrWriteRegViaAux_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum, LW_DPCD_LANE_COUNT_SET, LW_TRUE, &dpAuxData);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Make sure sink device is capable to do NLT for the given link setting
    status = dispReadOrWriteRegViaAux_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum, LW_SR_NLT_CAPABILITIES0, LW_FALSE, &dpAuxData);
    if (status != FLCN_OK)
    {
        return status;
    }

    if (FLD_TEST_DRF(_SR, _NLT_CAPABILITIES0, _NLT_CAPABLE_CONFIG, _NO, dpAuxData))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
    }
    return status;
}

/*!
 * @brief      Returns a pair padlink
 *
 * @param[in]  padlink
 *
 * @return     LwU32 the paired padlink
 */
LwU32
dispGetPairPadLink_GP10X
(
    LwU32 padlink
)
{
    if ((padlink == padlink_G) || (padlink == padlink_ilwalid))
    {
        return padlink;
    }
    else if ((padlink == padlink_A) || (padlink == padlink_C) || (padlink == padlink_E))
    {
        return (padlink + 1);
    }
    else
    {
        return (padlink - 1);
    }
}

/*!
 * @brief     enable or disable the idle pattern
 *
 * @param[in] bEnable    enable or disable
 *
 * @return    void
 */
void
dispForceIdlePattern_GP10X
(
    LwBool bEnable
)
{
    LwU32 linkCtl;
    linkCtl = REG_RD32(BAR0, LW_PDISP_SOR_DP_LINKCTL(Disp.pPmsBsodCtx->head.orIndex, Disp.pPmsBsodCtx->dpInfo.linkIndex));

    if (bEnable)
    {
        linkCtl = FLD_SET_DRF(_PDISP, _SOR_DP_LINKCTL, _FORCE_IDLEPTTRN, _YES, linkCtl);
    }
    else
    {
        linkCtl = FLD_SET_DRF(_PDISP, _SOR_DP_LINKCTL, _FORCE_IDLEPTTRN, _NO, linkCtl);
    }

    PMU_BAR0_WR32_SAFE(LW_PDISP_SOR_DP_LINKCTL(Disp.pPmsBsodCtx->head.orIndex, Disp.pPmsBsodCtx->dpInfo.linkIndex), linkCtl);
}


/*!
 * @brief  Set display owner to driver
 *
 * @return FLCN_STATUS FLCN_OK if display owner set to driver
 */
FLCN_STATUS
dispSetDisplayOwner_GP10X(void)
{
    PMU_BAR0_WR32_SAFE(LW_PDISP_VGA_CR_REG58, LW_PDISP_VGA_CR_REG58_SET_DISP_OWNER_DRIVER);

    if (!PMU_REG32_POLL_NS(LW_PDISP_VGA_CR_REG58,
        DRF_SHIFTMASK(LW_PDISP_VGA_CR_REG58_DISP_OWNER),
        DRF_DEF(_PDISP, _VGA_CR_REG58, _DISP_OWNER, _DRIVER),
        LW_DISP_PMU_REG_POLL_PMS_TIMEOUT, PMU_REG_POLL_MODE_BAR0_EQ))
    {
        PMU_HALT();
        return FLCN_ERR_TIMEOUT;
    }

    return FLCN_OK;
}

#if PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING)
/*!
 * @brief  check the VGA register index
 *
 * @return LwBool
 */
static LwBool
s_dispVgaCrRegIndexIsValid
(
    LwU8    idx
)
{
    //
    // Refer to vgaIsCrIndexValid_v02_01
    //
    // CR4C to CR4E (LW_PDISP_VGA_CR_REG19)    are undefined in VGA space.
    // CR5C to CR5E (LW_PDISP_VGA_CR_REG23)    are undefined in VGA space.
    // CR6C to CR6E (LW_PDISP_VGA_CR_REG27)    are undefined in VGA space.
    // CRA4 to CRA7 (LW_PDISP_VGA_CR_REG41)    are undefined in VGA space.
    // CRAB (LW_PDISP_VGA_CR_REG42[31:24])      is undefined in VGA space.
    // CRAC to CRBF (LW_PDISP_VGA_CR_REG43-47) are undefined in VGA space.
    // CRC0 to CRDF (LW_PDISP_VGA_CR_REG48-55) are defined in PMGR. CRCA to CRD0 are defined in dev_disp for vbios.
    // CRE0 to CRE7 (LW_PDISP_VGA_CR_REG56-57) are undefined in VGA space.
    // CRF0 to CRFF (LW_PDISP_VGA_CR_REG60-63) are implemented in host and we
    //              don't have a priv reg mapping for those. F0 is RMA which we
    //              don't care about and F1 through FF are reserved.
    //
    // Note:
    // CR80 to CR9F (LW_PDISP_VGA_CR_REG32-39) are direct scratch regs mapped
    //              via LW_PDISP_VGA_CR_REG_SCRATCH which use same reg offsets
    //              as LW_PDISP_VGA_CR_REG32-39. So, LW_PDISP_VGA_CR_REG32-39
    //              aren't actually missing.
    //
    if (((idx >= 0x4C) && (idx <= 0x4E)) || // LW_PDISP_VGA_CR_REG19
        ((idx >= 0x5C) && (idx <= 0x5E)) || // LW_PDISP_VGA_CR_REG23
        ((idx >= 0x6C) && (idx <= 0x6E)) || // LW_PDISP_VGA_CR_REG27
        ((idx >= 0xA4) && (idx <= 0xA7)) || // LW_PDISP_VGA_CR_REG41
        ((idx >= 0xAB) && (idx <= 0xBF)) || // LW_PDISP_VGA_CR_REG42 - 47
        ((idx >= 0xE0) && (idx <= 0xE7)) || // LW_PDISP_VGA_CR_REG56 - 57
        (idx >= 0xF1))                      // All behind LW_PDISP_VGA_CR_REG60 are reserved
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING)

