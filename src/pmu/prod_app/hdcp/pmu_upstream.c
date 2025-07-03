/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_upstream.c
 * @brief   Supplies HDCP SPRIME read and sanity check functions.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_bar0.h"
#include "hdcp/pmu_upstream.h"
#include "dev_disp.h"
#include "class/cl907d.h"

/* -------------------------- Function Prototypes ---------------------------- */
static LwU8  hdcpFindSorPanelType(LwU32);

/*!
 * @brief Check if SOR is driving internal or external panel
 *
 * @param[in]  sorIndex  SOR Index
 *
 * @return HDCP_SOR_PANEL_INTERNAL if SOR is driving an internal display
 *         HDCP_SOR_PANEL_EXTERNAL if SOR is driving an external display
 *         HDCP_SOR_PANEL_NONE     if SOR is not attached to Head.
 */

static LwU8
hdcpFindSorPanelType
(
    LwU32 sorIndex
)
{
    LwU32    regVal    = 0;
    LwU32    sorCtrl   = 0;
    LwU8     panelType = HDCP_SOR_PANEL_TYPE_EXTERNAL;

    sorCtrl  = REG_RD32(BAR0, LW_UDISP_DSI_CHN_ARMED_BASEADR
               (LW_PDISP_907D_CHN_CORE)+ LW907D_SOR_SET_CONTROL(sorIndex));

    if (FLD_TEST_DRF(907D, _SOR_SET_CONTROL, _OWNER_MASK, _NONE, sorCtrl))
    {
        panelType = HDCP_SOR_PANEL_TYPE_NONE;
    }
    else
    {
        if (FLD_TEST_DRF(907D, _SOR_SET_CONTROL, _PROTOCOL, _LVDS_LWSTOM, sorCtrl))
        {
            panelType = HDCP_SOR_PANEL_TYPE_INTERNAL;
        }
        else if (FLD_TEST_DRF(907D, _SOR_SET_CONTROL, _PROTOCOL, _DP_A, sorCtrl))
        {
            regVal = REG_RD32(BAR0, LW_PDISP_SOR_DP_SPARE0(sorIndex));
            if (FLD_TEST_DRF(_PDISP, _SOR_DP_SPARE0, _PANEL, _INTERNAL, regVal))
            {
                panelType = HDCP_SOR_PANEL_TYPE_INTERNAL;
            }
        }
        else if (FLD_TEST_DRF(907D, _SOR_SET_CONTROL, _PROTOCOL, _DP_B, sorCtrl))
        {
            regVal = REG_RD32(BAR0, LW_PDISP_SOR_DP_SPARE1(sorIndex));
            if (FLD_TEST_DRF(_PDISP, _SOR_DP_SPARE1, _PANEL, _INTERNAL, regVal))
            {
                panelType = HDCP_SOR_PANEL_TYPE_INTERNAL;
            }            
        }
    }

    return panelType;
}

/*!
 * @brief Reads Sprime value
 * 
 * @param[out]  pSprime     Sprime output
 * @param[out]  pCmodeIdx   Cmode Idx as in Sprime
 * @param[out]  pPanelType  Panel type (int/ext) as in Sprime
 * 
 * @return  FLCN_OK   If read is successful
 *          FLCN_ERROR If read is not successful
 */

FLCN_STATUS
hdcpReadSprime
(
    LwU8  *pSprime,
    LwU8  *pCmodeIdx,
    LwU8  *pPanelType
)
{
    FLCN_STATUS status = FLCN_ERROR;
    LwU32      data   = 0;

    data = REG_RD32(BAR0, LW_PDISP_UPSTREAM_HDCP_CTRL);

    // Check if SPRIME_VALID is set
    if (FLD_TEST_DRF(_PDISP, _UPSTREAM_HDCP_CTRL, _SPRIME, _VALID, data))
    {
        status = FLCN_OK;

        data  = REG_RD32(BAR0, LW_PDISP_UPSTREAM_HDCP_SPRIME_LSB1);
        memcpy(&(pSprime[0]), &data, 4);

        data  = REG_RD32(BAR0, LW_PDISP_UPSTREAM_HDCP_SPRIME_LSB2);
        memcpy(&(pSprime[4]), &data, 4);
        *pCmodeIdx = DRF_VAL(_PDISP, _UPSTREAM_HDCP_SPRIME_LSB2, 
                                            _STATUS_CMODE_IDX, data);

        data    = REG_RD32(BAR0, LW_PDISP_UPSTREAM_HDCP_SPRIME_MSB);
        pSprime[8] = (LwU8)(data & 0xFF);
        *pPanelType = DRF_VAL(_PDISP, _UPSTREAM_HDCP_SPRIME_MSB, 
                                               _STATUS_INTPNL, data);
    }

    return status;
}

/*
 * @brief Sprime sanity check. At present checks only if SPRIME internal bit
 *        matches HW state. Corrupts SPRIME if sanity check fails
 * 
 * @param[in,out]  pSprime    Holds Sprime to be checked
 * @param[in]      cmodeIdx   Cmode Index got while reading Sprime
 * @param[in]      panelType  Panel type in SPRIME MSB reg
 * 
 * @return FLCN_OK If Sprime sanity check has passed
 *         FLCN_ERROR If Sprime sanity check has failed
 */

FLCN_STATUS
hdcpSprimeSanityCheck
(
    LwU8   *pSprime,
    LwU8   cmodeIdx,
    LwU8   panelType
)
{
    FLCN_STATUS status     = FLCN_OK;
    LwU32      sorIndex   = 0;
    LwU32      sorPnlType = 0;
    LwBool     bIsIntPnl  = LW_FALSE;

    // Get SorIndex from Cmode Index
    sorIndex = cmodeIdx - LW_PDISP_UPSTREAM_HDCP_CMODE_INDEX_SOR0;

    // Check if status is for SOR
    if (sorIndex < LW_PDISP_UPSTREAM_HDCP_CMODE_INDEX_SOR__SIZE_1)
    {
        // Is internal panel?
        bIsIntPnl = (panelType == LW_PDISP_UPSTREAM_HDCP_SPRIME_MSB_STATUS_INTPNL_ACTV);

        //
        // Find SOR panel type and compare with SPRIME panel type
        // If SOR panel type and panel type from SPRIME doesnt match, something has gone wrong
        // and we should corrupt the SPRIME to stop 'protected content' playback
        //
        sorPnlType = hdcpFindSorPanelType(sorIndex);

        if ( ((sorPnlType == HDCP_SOR_PANEL_TYPE_INTERNAL) && (!bIsIntPnl)) ||
             ((sorPnlType == HDCP_SOR_PANEL_TYPE_EXTERNAL) && (bIsIntPnl)))
        {
            if (bIsIntPnl)
            {
                pSprime[8] = FLD_SET_DRF(_PDISP, _UPSTREAM_HDCP_SPRIME_MSB, 
                                    _STATUS_INTPNL, _INACTV, pSprime[8]);
            }
            // Should corrupted byte be random?
            pSprime[3] = ~pSprime[3];
            status = FLCN_ERROR;            
        }
    }

    return status;
}
