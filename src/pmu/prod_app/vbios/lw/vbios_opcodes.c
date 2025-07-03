/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   vbios_opcodes.c
 * @brief  VBIOS opcodes functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_bar0.h"
#include "pmu_objvbios.h"
#include "pmu_objseq.h"
#include "pmu_objdisp.h"
#include "vbios/vbios_opcodes.h"
#include "dev_disp.h"
#include "displayport/dpcd.h"
#include "dev_master.h"

#include "config/g_vbios_private.h"
#include "config/g_disp_private.h"

/* ------------------------- Macro Definitions ------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief      INIT_NOT - Perform NOT on the condition flag.
 *
 * @param[in]  pCtx   devinit engine context
 * @param[in]  pData  header
 *
 * @return     FLSC_STATUS
 */
FLCN_STATUS
vbiosOpcode_NOT
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    pCtx->bCondition = !pCtx->bCondition;
    return FLCN_OK;
}


/*!
 * @brief    INIT_GENERIC_CONDITION - Restrict further processing based on a generic
 *                                    condition with a condition ID.
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_GENERIC_CONDITION
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_GENERIC_CONDITION_HDR *pHdr = pData;
    FLCN_STATUS status = FLCN_OK;

    switch (pHdr->condition)
    {
        case  CONDITION_ID_ASSR_SUPPORT:
        {
            LwU8        configCap;
            // Read the config cap.
            status = dispReadOrWriteRegViaAux_HAL(&Disp, pCtx->dpPortNum, LW_DPCD_EDP_CONFIG_CAP, LW_FALSE, &configCap);
            if (status != FLCN_OK)
            {
                return status;
            }
            pCtx->bCondition = FLD_TEST_DRF(_DPCD, _EDP_CONFIG_CAP,
                                             _ALTERNATE_SCRAMBLER_RESET, _YES, configCap);
            break;
        }
        case CONDITION_ID_USE_EDP:
        {
            pCtx->bCondition = (pCtx->dpPortNum != 0xFF);
            break;
        }
        case CONDITION_ID_NO_PANEL_SEQ_DELAYS:
        {
            //
            // Need to check GC6 exiting and LWSR panel.
            // Remove SOR sequencer delays only when exiting GC6 on LWSR systems.
            //
            pCtx->bCondition = LW_TRUE;
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
        }
    }
    return status;
}


/*!
 * @brief    INIT_CRTC - Read/Modify/Write a CRTC register
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_CRTC
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_CRTC_HDR *pHdr = pData;
    LwU8           data;

    if (pCtx->bCondition)
    {
        data = dispReadVgaReg_HAL(&Disp, (LwU8)pHdr->index);
        data &= pHdr->mask;
        data |= pHdr->data;
        dispWriteVgaReg_HAL(&Disp, (LwU8)pHdr->index, data);
    }
    return FLCN_OK;
}


/*!
 * @brief    INIT_POLL_LW - Polls a privileged register until a masked value
 *                          compares correctly.
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_POLL_LW
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_POLL_LW_HDR *pHdr       = pData;
    LwS32             timeout_us = pHdr->timeout * 100000;
    FLCN_STATUS       status     = FLCN_OK;
    LwBool            bResult    = LW_FALSE;

    while (timeout_us > 0)
    {
        status = vbiosIedCheckCondition(pCtx, (LwU8)pHdr->condition, &bResult);
        if (status != FLCN_OK)
        {
            return status;
        }
        if (bResult)
        {
            return FLCN_OK;
        }

        // Spin-wait between condition checks.
        OS_PTIMER_SPIN_WAIT_NS(POLL_INTERVAL_US * 1000);

        timeout_us -= POLL_INTERVAL_US;
    }

    pCtx->bCondition = LW_FALSE;
    return FLCN_OK;
}


/*!
 * @brief    INIT_SUB_DIRECT - Execute another script (specified with a direct offset)
 *                              and then return to process the current script.
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_SUB_DIRECT
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_SUB_DIRECT_HDR *pHdr = pData;
    VBIOS_IED_CONTEXT    devEngCtxSub;

    if (pCtx->bCondition)
    {
        devEngCtxSub = *pCtx;
        devEngCtxSub.nOffsetLwrrent = pHdr->offset;
        devEngCtxSub.bCondition = LW_TRUE;
        vbiosIedExelwteScriptSub(&devEngCtxSub);
    }
    return FLCN_OK;
}

/*!
 * @brief    INIT_REG_ARRAY - Modify set of register.
 *           Write a sequential block of the 32-bit LW registers with constants.
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_REG_ARRAY
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_REG_ARRAY_HDR *pHdr   = pData;
    FLCN_STATUS         status = FLCN_OK;
    LwU32               addr;
    LwU32               i;
    LwU32               pData32[VBIOS_MAX_BLOCK_SIZE_REG_ARRAY];

    if (pCtx->bCondition)
    {
        addr = vbiosOpcodePatchAddr_HAL(&Vbios, pCtx, pHdr->addr);

        if (pHdr->count > VBIOS_MAX_BLOCK_SIZE_REG_ARRAY)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto vbiosOpcode_REG_ARRAY_exit;
        }

        status = vbiosIedFetchBlock(pCtx, (void *)pData32, (pHdr->count * sizeof(pData32[0])));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto vbiosOpcode_REG_ARRAY_exit;
        }

        for (i = 0; i < pHdr->count; i++)
        {
            PMU_BAR0_WR32_SAFE(addr, pData32[i]);
            addr += 4;
        }
    }

vbiosOpcode_REG_ARRAY_exit:
    return status;
}

/*!
 * @brief    INIT_LW_COPY - Perform a copy from a privileged register to another
 *                          privileged register.
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_LW_COPY
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_LW_COPY_HDR *pHdr = pData;
    LwU32             nData, nDestData;
    LwU32             addr;

    if (pCtx->bCondition)
    {
        addr = vbiosOpcodePatchAddr_HAL(&Vbios, pCtx, pHdr->addr);
        nData = REG_RD32(BAR0, addr);
        if (pHdr->shift > 0)
        {
            nData = nData >> pHdr->shift;
        }
        else
        {
            nData = nData << -pHdr->shift;
        }
        nData &= pHdr->andMask;
        nData ^= pHdr->xorMask;
        addr = vbiosOpcodePatchAddr_HAL(&Vbios, pCtx, pHdr->destAddr);
        nDestData = REG_RD32(BAR0, addr);
        nDestData &= pHdr->destAndMask;
        nDestData |= nData;
        PMU_BAR0_WR32_SAFE(addr, nDestData);
    }
    return FLCN_OK;
}


/*!
 * @brief    INIT_LW_REG - Modify a register.
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_LW_REG
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_LW_REG_HDR *pHdr = pData;
    LwU32            val;
    LwU32            addr;

    if (pCtx->bCondition)
    {
        addr = vbiosOpcodePatchAddr_HAL(&Vbios, pCtx, pHdr->addr);
        val = REG_RD32(BAR0, addr);
        val &= pHdr->mask;
        val |= pHdr->data;
        PMU_BAR0_WR32_SAFE(addr, val);
    }
    return FLCN_OK;
}


/*!
 * @brief    INIT_RESETBITS_LW_REG - Modify a register.
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_RESETBITS_LW_REG
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_RESETBITS_LW_REG_HDR *pHdr = pData;
    LwU32                      val, addr;

    if (pCtx->bCondition)
    {
        addr = vbiosOpcodePatchAddr_HAL(&Vbios, pCtx, pHdr->addr);
        val = REG_RD32(BAR0, addr);
        val &= ~(pHdr->data);
        PMU_BAR0_WR32_SAFE(addr, val);
    }
    return FLCN_OK;
}


/*!
 * @brief    INIT_SETBITS_LW_REG - Modify a register.
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_SETBITS_LW_REG
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_SETBITS_LW_REG_HDR *pHdr = pData;
    LwU32                   val, addr;

    if (pCtx->bCondition)
    {
        addr = vbiosOpcodePatchAddr_HAL(&Vbios, pCtx, pHdr->addr);
        val = REG_RD32(BAR0, addr);
        val |= pHdr->data;
        PMU_BAR0_WR32_SAFE(addr, val);
    }
    return FLCN_OK;
}


/*!
 * @brief    INIT_DPCD_REG - Modify a DPAUX register.
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_DPCD_REG
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_DPCD_REG_HDR *pHdr     = pData;
    FLCN_STATUS        status   = FLCN_OK;
    LwU8               data, andMask, orMask;

    if (pCtx->bCondition)
    {
        while (pHdr->count-- > 0)
        {
            // Read the DPCD data.
            status = dispReadOrWriteRegViaAux_HAL(&Disp, pCtx->dpPortNum, pHdr->startReg, LW_FALSE, &data);
            if (status != FLCN_OK)
            {
                return status;
            }

            // Get the AND mask.
            vbiosIedFetch(pCtx, &andMask);

            // Get the OR mask.
            vbiosIedFetch(pCtx, &orMask);


            data &= andMask;
            data |= orMask;

            // Write the DPCD data.
            status = dispReadOrWriteRegViaAux_HAL(&Disp, pCtx->dpPortNum, pHdr->startReg, LW_TRUE, &data);
            if (status != FLCN_OK)
            {
                return status;
            }
            pHdr->startReg++;
        }
    }
    else
    {
        // Advance IP
        pCtx->nOffsetLwrrent += pHdr->count * 2;
    }
    return FLCN_OK;
}


/*!
 * @brief    INIT_DONE - End of current script.
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_DONE
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    pCtx->bCompleted = LW_TRUE;
    return FLCN_OK;
}



/*!
 * @brief    INIT_RESUME - Used to end a block of script that was restricted due to
 *                         a condition.
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_RESUME
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    pCtx->bCondition = LW_TRUE;
    return FLCN_OK;
}


/*!
 * @brief    INIT_TIME - Delay a period of time in microsecond units.
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_TIME
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_TIME_HDR *pHdr = pData;

    if (pCtx->bCondition)
    {
        OS_PTIMER_SPIN_WAIT_US((LwU32)pHdr->delayUs);
    }
    return FLCN_OK;
}


/*!
 * @brief    INIT_CONDITION - Restrict further processing based on a register condition.
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_CONDITION
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_CONDITION_HDR *pHdr = pData;
    FLCN_STATUS         status;
    LwBool              bResult;

    status = vbiosIedCheckCondition(pCtx, (LwU8)pHdr->condition, &bResult);
    if (status != FLCN_OK)
    {
        return status;
    }
    if (!bResult)
    {
        pCtx->bCondition = LW_FALSE;
    }
    return FLCN_OK;
}


/*!
 * @brief    INIT_ZM_REG - Write a new value to a privileged register (zero mask).
 *
 * @copydoc  vbiosOpcode_NOT
 */
FLCN_STATUS
vbiosOpcode_ZM_REG
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_ZM_REG_HDR *pHdr = pData;
    LwU32            addr;

    if (pCtx->bCondition)
    {
        addr = vbiosOpcodePatchAddr_HAL(&Vbios, pCtx, pHdr->addr);
        PMU_BAR0_WR32_SAFE(addr, pHdr->data);
    }
    return FLCN_OK;
}


/*!
* @brief    INIT_TIME_MSEC - delay a period of time in millisecond units.
*
* @copydoc  vbiosOpcode_NOT
*/
FLCN_STATUS
vbiosOpcode_TIME_MSEC
(
    VBIOS_IED_CONTEXT *pCtx,
    void              *pData
)
{
    INIT_TIME_MSEC_HDR *pHdr = pData;

    if (pCtx->bCondition)
    {
        OS_PTIMER_SPIN_WAIT_US(pHdr->delayMs * 1000);
    }
    return FLCN_OK;
}
