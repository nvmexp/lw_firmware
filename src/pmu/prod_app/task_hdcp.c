/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_hdcp.c
 * @brief  Task that performs validation services related to HDCP
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "dbgprintf.h"
#include "pmu_objpmu.h"
#include "pmu_oslayer.h"
#include "hdcp/pmu_auth.h"
#include "hdcp/pmu_pvtbus.h"
#include "hdcp/pmu_upstream.h"
#include "hdcp/hdcp_types.h"
#include "lwos_dma.h"
#include "lwuproc.h"

#include "g_pmurpc.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief Wrapper define for FLD_SET_DRF.
 */

#define HDCP_SET_STATUS_SOR(f, c, v) \
    FLD_SET_DRF(_PDISP, _PRIVATE_HDCP_STATUS_SOR, f, c, v)

/*!
 * @brief Reverses the byte order of a 32-bit value.
 */
#define REVERSE_BYTE_ORDER(a) \
    (((a) >> 24) | ((a) << 24) | (((a) >> 8) & 0xFF00) | \
    (((a) << 8) & 0xFF0000))

/* ------------------------- Prototypes ------------------------------------- */
lwrtosTASK_FUNCTION(task_hdcp, pvParameters);

/* ------------------------- Global Variables ------------------------------- */
/*
 * Purposefully not dolwmented to doxygen.
 */
LwU32 HdcpChipId;

/*!
 *  Mask of existing SORs
 */
LwU8 SorMask;

/*!
 * This will be used to control optional behavior, such as using test vectors
 * instead of production values for verification.
 */
LwU32 CtlOptions = 0;

/* ------------------------- Static Variables ------------------------------- */
static LwU8 HdcpPrimeValue[ALIGN_UP(LW_HDCPSRM_SIGNATURE_SIZE, 16)] GCC_ATTRIB_ALIGN(16);

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Process HDCP initialize command.
 *
 * Sets HDCP status bits 1, 3, and 13 to 0 for all SORs
 *
 * @param[in]   pContext    Current Hdcp context.
 */
static void
s_hdcpProcessCmdInit
(
    RM_FLCN_HDCP_CMD   *pHdcpCmd,
    RM_FLCN_HDCP_MSG   *pHdcpMsg
)
{
    LwU32 i;
    LwU32 regVal;

    CtlOptions = pHdcpCmd->init.options;
    HdcpChipId = pHdcpCmd->init.chipId;
    SorMask    = pHdcpCmd->init.sorMask;

    for (i = 0; i < LW_PDISP_PRIVATE_HDCP_STATUS_SOR_MAX; i++)
    {
        if (SorMask & BIT(i))
        {
            hdcpReadStatus(i, &regVal);
            regVal = HDCP_SET_STATUS_SOR(_REP, _ILWALID, regVal);
            regVal = HDCP_SET_STATUS_SOR(_UNPROT, _VALID, regVal);
            regVal = HDCP_SET_STATUS_SOR(_SCOPE, _FAILED, regVal);
            hdcpWriteStatus(i, regVal);
        }
    }
    pHdcpMsg->gen.status = RM_FLCN_HDCP_MSG_STATUS_OK;
}

/*!
 * @brief Process HDCP set flags command.
 *
 * Sets the new control flags
 *
 * @param[in]   pContext    Current Hdcp context.
 */
static void
s_hdcpProcessCmdSetOptions
(
    RM_FLCN_HDCP_CMD   *pHdcpCmd,
    RM_FLCN_HDCP_MSG   *pHdcpMsg
)
{
    CtlOptions = pHdcpCmd->setOptions.options;

    pHdcpMsg->gen.status = RM_FLCN_HDCP_MSG_STATUS_OK;
}

/*!
 * @brief Process HDCP SRM validation command.
 *
 * The HDCP SRM validation command validates an SRM for use with revocation.
 *
 * @param[in]   pContext    Current Hdcp context.
 */
static void
s_hdcpProcessCmdValidateSrm
(
    RM_FLCN_HDCP_CMD   *pHdcpCmd,
    RM_FLCN_HDCP_MSG   *pHdcpMsg
)
{
    FLCN_STATUS status;
    RM_FLCN_HDCP_CMD_VALIDATE_SRM *pCmd = &(pHdcpCmd->valSrm);

    if (DMA_ADDR_IS_ZERO(&pCmd->srm))
    {
        // SRM is "null", mark as bad
        pHdcpMsg->gen.status = RM_FLCN_HDCP_MSG_STATUS_ERR_SRM_FORMAT;
    }
    else
    {
        status = hdcpValidateSrm(&pCmd->srm, pCmd->srmListSize);
        switch (status)
        {
            case FLCN_OK:
                pHdcpMsg->gen.status = RM_FLCN_HDCP_MSG_STATUS_OK;
                break;
            case FLCN_ERROR:
                pHdcpMsg->gen.status = RM_FLCN_HDCP_MSG_STATUS_ERR_SRM_ILWALID;
                break;
            case FLCN_ERR_ILWALID_ARGUMENT:
            default:
                pHdcpMsg->gen.status = RM_FLCN_HDCP_MSG_STATUS_ERR_SRM_FORMAT;
                break;
        }
    }
}

/*!
 * @brief Process HDCP KSV validation command.
 *
 * The HDCP validation command validates a KSV list and performs a revocation
 * check of the KSV list against the SRM.
 *
 * @param[in]   pContext    Current Hdcp context.
 */

static void
s_hdcpProcessCmdValidateKsv
(
    RM_FLCN_HDCP_CMD   *pHdcpCmd,
    RM_FLCN_HDCP_MSG   *pHdcpMsg
)
{
    FLCN_STATUS status = FLCN_OK;
    RM_FLCN_HDCP_CMD_VALIDATE_KSV *pCmd = &(pHdcpCmd->valKsv);
    LwU8  v[LW_HDCPSRM_SIGNATURE_SIZE];
    LwU64 m0;
    LwU32 regVal;

    LwU32 numKsvs;
    LwU8  numBksvs;

    // mark as starting the operation in the status register
    hdcpReadStatus(pCmd->sorIndex, &regVal);
    regVal = HDCP_SET_STATUS_SOR(_REP, _ILWALID, regVal);
    regVal = HDCP_SET_STATUS_SOR(_UNPROT, _VALID, regVal);
    regVal = HDCP_SET_STATUS_SOR(_SCOPE, _FAILED, regVal);
    regVal = HDCP_SET_STATUS_SOR(_MASK, _NOT_READY, regVal);
    hdcpWriteStatus(pCmd->sorIndex, regVal);

    pHdcpMsg->gen.status = RM_FLCN_HDCP_MSG_STATUS_OK;

    //
    // Determine the number of downstream ksvs, excluding any repeater
    // bksvs prepended to the front of the list
    //
    numBksvs = DRF_VAL(_RM_FLCN_HDCP, _VALIDATE_FLAGS, _NUM_BKSVS,
                       pCmd->flags);
    numKsvs  = pCmd->ksvNumEntries - numBksvs;

    // verify the signature on the SRM
    if (!DMA_ADDR_IS_ZERO(&pCmd->srm))
    {
        status = hdcpValidateSrm(&pCmd->srm, pCmd->srmListSize);
        switch (status)
        {
            case FLCN_OK:
                // signature OK
                regVal = HDCP_SET_STATUS_SOR(_UNPROT, _ILWALID, regVal);
                regVal = HDCP_SET_STATUS_SOR(_SCOPE, _FAILED, regVal);
                break;

            case FLCN_ERROR:
                // signature was bad
                pHdcpMsg->gen.status = RM_FLCN_HDCP_MSG_STATUS_ERR_SRM_ILWALID;
                status = FLCN_ERROR;
                break;

            case FLCN_ERR_ILWALID_ARGUMENT:
            default:
                // SRM failed some basic sanity tests
                pHdcpMsg->gen.status = RM_FLCN_HDCP_MSG_STATUS_ERR_SRM_FORMAT;
                status = FLCN_ERROR;
        }
    }

    // verify the KSV list
    if (status == FLCN_OK)
    {
        if (!DMA_ADDR_IS_ZERO(&pCmd->ksvList) && !DMA_ADDR_IS_ZERO(&pCmd->vPrime))
        {
            if (BUS_SUCCESS == hdcpReadM0(&m0))
            {
                LwU8 i;

                hdcpCallwlateV(m0, pCmd->BInfo, numKsvs,
                               &pCmd->ksvList, numBksvs, v);

                // zero out M0 to deter snoopers
                memset(&m0, 0, sizeof(m0));

                (void)dmaRead(HdcpPrimeValue, &pCmd->vPrime, 0,
                              ALIGN_UP(LW_HDCPSRM_SIGNATURE_SIZE, 16));

                for (i = 0; i < LW_HDCPSRM_SIGNATURE_SIZE / sizeof(LwU32); i++)
                {
                    //
                    // V is stored as a big endian byte stream, while V' is
                    // stored in little endian 32-bit values. Need to make
                    // sure they are both in the same format.
                    //

                    LwU32 *p32 = (LwU32*)HdcpPrimeValue + i;
                    *p32 = REVERSE_BYTE_ORDER(*p32);
                }

                if (memcmp(v, HdcpPrimeValue, LW_HDCPSRM_SIGNATURE_SIZE) == 0)
                {
                    // V/V' match
                    regVal = HDCP_SET_STATUS_SOR(_REP, _VALID, regVal);
                    regVal = HDCP_SET_STATUS_SOR(_UNPROT, _ILWALID, regVal);
                }
                else
                {
                    // V/V' mismatch. mark as an error
                    pHdcpMsg->gen.status = RM_FLCN_HDCP_MSG_STATUS_ERR_KSV_ILWALID;
                    status = FLCN_ERROR;
                }
            }
            else
            {
                // error oclwrred while reading M0. could not compute V
                pHdcpMsg->gen.status = RM_FLCN_HDCP_MSG_STATUS_FAIL;
                status = FLCN_ERROR;
            }
        }
    }

    // do revocation check
    if ((status == FLCN_OK) &&
        (!DMA_ADDR_IS_ZERO(&pCmd->srm)) && (!DMA_ADDR_IS_ZERO(&pCmd->ksvList)))
    {
        status = hdcpValidateRevocationList(&pCmd->srm, pCmd->srmListSize,
                    &pCmd->ksvList, pCmd->ksvNumEntries);
        if (status == FLCN_OK)
        {
            // no KSV entries found in the SRM revocation list
            pHdcpMsg->gen.status = RM_FLCN_HDCP_MSG_STATUS_OK;
            regVal = HDCP_SET_STATUS_SOR(_UNPROT, _VALID, regVal);
            regVal = HDCP_SET_STATUS_SOR(_SCOPE, _PASSED, regVal);
        }
        else
        {
            // a revoked entry found
            pHdcpMsg->gen.status = RM_FLCN_HDCP_MSG_STATUS_ERR_SRM_REVOKED;
        }
    }

    regVal = HDCP_SET_STATUS_SOR(_MASK, _READY, regVal);
    hdcpWriteStatus(pCmd->sorIndex, regVal);
}

/*!
 * @brief Validates our access to the Secure Status registers.
 *
 * Sets HDCP status bits to a known configuration, reads them back, checks
 * to see if the bits are set as expected, and restores the original values.
 * A status message is sent back to indicate success or failure.
 *
 * @param[in]   pContext    Current Hdcp context.
 */
static void
s_hdcpProcessCmdTestSelwreStatus
(
    RM_FLCN_HDCP_CMD   *pHdcpCmd,
    RM_FLCN_HDCP_MSG   *pHdcpMsg
)
{
    LwU8  status = RM_FLCN_HDCP_MSG_STATUS_OK;
    LwU32 i;
    LwU32 regSaveVal;
    LwU32 regTestVal;
    LwU32 regReadVal;
    BUS_STATUS busStatus;

    for (i = 0; i < LW_PDISP_PRIVATE_HDCP_STATUS_SOR_MAX; i++)
    {
        if (SorMask & BIT(i))
        {
            // Save off the current value
            busStatus = hdcpReadStatus(i, &regSaveVal);
            if (busStatus != BUS_SUCCESS)
            {
                status = (i << 4) | RM_FLCN_HDCP_MSG_STATUS_FAIL;
                break;
            }

            // Flip the repeater bit
            regTestVal = regSaveVal;
            if (FLD_TEST_DRF(_PDISP, _PRIVATE_HDCP_STATUS_SOR, _REP, _VALID, regTestVal))
            {
                regTestVal = HDCP_SET_STATUS_SOR(_REP, _ILWALID, regTestVal);
            }
            else
            {
                regTestVal = HDCP_SET_STATUS_SOR(_REP, _VALID, regTestVal);
            }
            busStatus = hdcpWriteStatus(i, regTestVal);
            if (busStatus != BUS_SUCCESS)
            {
                status = (i << 4) | RM_FLCN_HDCP_MSG_STATUS_FAIL;
                break;
            }

            busStatus = hdcpReadStatus(i, &regReadVal);
            if (busStatus != BUS_SUCCESS)
            {
                status = (i << 4) | RM_FLCN_HDCP_MSG_STATUS_FAIL;
                break;
            }

            // If we don't get a match, bail out early and indicate a failure
            if (regTestVal != regReadVal)
            {
                // Set the status to FAIL with the SOR index in the upper nibble
                status = (i << 4) | RM_FLCN_HDCP_MSG_STATUS_FAIL;

                //
                // Try to write the saved value back in case it was the read that
                // failed.
                //
                hdcpWriteStatus(i, regSaveVal);
                break;
            }
            (void)hdcpWriteStatus(i, regSaveVal);
        }
    }
    pHdcpMsg->gen.status = status;
}

/*!
 * @brief Command handler to read SPRIME. Does sanity check
 *        on SPRIME to make sure INTPNL bit matches that of SOR
 *        protocol. If not, it corrupts the SPRIME.
 *
 * @param[in]   pContext    Current Hdcp context.
 */
static void
s_hdcpProcessCmdReadSprime
(
    RM_FLCN_HDCP_CMD   *pHdcpCmd,
    RM_FLCN_HDCP_MSG   *pHdcpMsg
)
{
    LwU8                         cmodeIdx  = 0;
    LwU8                         panelType = 0;
    RM_FLCN_HDCP_MSG_READ_SPRIME *pMsg     = &(pHdcpMsg->readSprime);

    if (hdcpReadSprime(pMsg->sprime, &cmodeIdx, &panelType) == FLCN_OK)
    {
        pMsg->status = RM_FLCN_HDCP_MSG_STATUS_OK;
        hdcpSprimeSanityCheck(pMsg->sprime, cmodeIdx, panelType);
    }
    else
    {
        pMsg->status = RM_FLCN_HDCP_MSG_STATUS_ERR_SPRIME_ILWALID;
    }
}

/*!
 * @brief   Execute generic LEGACY_CMD RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_HDCP_LEGACY_CMD
 */
FLCN_STATUS
pmuRpcHdcpLegacyCmd
(
    RM_PMU_RPC_STRUCT_HDCP_LEGACY_CMD *pParams
)
{
    RM_FLCN_HDCP_CMD   *pHdcpCmd = &(pParams->request);
    RM_FLCN_HDCP_MSG   *pHdcpMsg = &(pParams->response);
    FLCN_STATUS         status   = FLCN_OK;

    switch (pHdcpCmd->cmdType)
    {
        case RM_FLCN_HDCP_CMD_ID_INIT:
        {
            s_hdcpProcessCmdInit(pHdcpCmd, pHdcpMsg);
            break;
        }
        case RM_FLCN_HDCP_CMD_ID_SET_OPTIONS:
        {
            s_hdcpProcessCmdSetOptions(pHdcpCmd, pHdcpMsg);
            break;
        }
        case RM_FLCN_HDCP_CMD_ID_VALIDATE_SRM:
        {
            s_hdcpProcessCmdValidateSrm(pHdcpCmd, pHdcpMsg);
            break;
        }
        case RM_FLCN_HDCP_CMD_ID_VALIDATE_KSV:
        {
            s_hdcpProcessCmdValidateKsv(pHdcpCmd, pHdcpMsg);

            pHdcpMsg->ksv.head = pHdcpCmd->valKsv.head;
            break;
        }
        case RM_FLCN_HDCP_CMD_ID_TEST_SELWRE_STATUS:
        {
            s_hdcpProcessCmdTestSelwreStatus(pHdcpCmd, pHdcpMsg);
            break;
        }
        case RM_FLCN_HDCP_CMD_ID_READ_SPRIME:
        {
            s_hdcpProcessCmdReadSprime(pHdcpCmd, pHdcpMsg);
            break;
        }
        default:
        {
            status = FLCN_ERR_QUEUE_TASK_ILWALID_CMD_TYPE;
            break;
        }
    }

    return status;
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Initialize the HDCP Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
hdcpPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;

    OSTASK_CREATE(status, PMU, HDCP);
    if (status != FLCN_OK)
    {
        goto hdcpPreInitTask_exit;
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, HDCP), 2, sizeof(DISP2UNIT_RM_RPC));
    if (status != FLCN_OK)
    {
        goto hdcpPreInitTask_exit;
    }

hdcpPreInitTask_exit:
    return status;
}

/*!
 * @brief Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(task_hdcp, pvParameters)
{
    DISP2UNIT_RM_RPC disp2hdcp;

    OSTASK_ATTACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libLw64));
    OSTASK_ATTACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libLw64Umul));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(sha1));

    // Process commands sent from the RM to the HDCP task.
    LWOS_TASK_LOOP_NO_CALLBACKS(LWOS_QUEUE(PMU, HDCP), &disp2hdcp, status, LWOS_TASK_CMD_BUFFER_ILWALID)
    {
        status = FLCN_ERR_QUEUE_TASK_ILWALID_UNIT_ID;

        if (disp2hdcp.hdr.unitId == RM_PMU_UNIT_HDCP)
        {
            status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

            if (disp2hdcp.hdr.eventType == DISP2UNIT_EVT_RM_RPC)
            {
                status = pmuRpcProcessUnitHdcp(&disp2hdcp);
            }
        }
    }
    LWOS_TASK_LOOP_END(status);
}
