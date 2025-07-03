/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp_cmds.c
 * @brief  Task that performs validation services related to HDCP
 */

/* ------------------------- System Includes -------------------------------- */
#include "hdcp_cmn.h"
#include "dev_disp.h"
/* ------------------------- Application Includes --------------------------- */
#include "lwoscmdmgmt.h"
#include "flcnifcmn.h"
#include "flcnifhdcp.h"
#include "lib_hdcp.h"
#include "lib_intfchdcp.h"
#include "lib_hdcpauth.h"
#include "hdcp_objhdcp.h"
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * @brief Wrapper define for FLD_SET_DRF.
 */

#define HDCP_SET_STATUS_SOR(f, c, v) \
    FLD_SET_DRF(_PDISP, _HDCP_STATUS_SOR, f, c, v)

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
static void _hdcpProcessCmdInit(HDCP_CONTEXT *pContext)
                GCC_ATTRIB_SECTION("imem_libHdcp", "_hdcpProcessCmdInit");
static void _hdcpProcessCmdValidateSrm(HDCP_CONTEXT *pContext)
                GCC_ATTRIB_SECTION("imem_libHdcp", "_hdcpProcessCmdValidateSrm");
static void _hdcpProcessCmdValidateKsv(HDCP_CONTEXT *pContext)
                GCC_ATTRIB_SECTION("imem_libHdcp", "_hdcpProcessCmdValidateKsv");
static void _hdcpProcessCmdTestSelwreStatus(HDCP_CONTEXT *pContext)
                GCC_ATTRIB_SECTION("imem_libHdcp", "_hdcpProcessCmdTestSelwreStatus");
static void _hdcpSendResponse(HDCP_CONTEXT *pContext, LwU8 eventType)
                GCC_ATTRIB_SECTION("imem_libHdcp", "_hdcpSendResponse");
static LwBool _hdcpCheckDmaIdx(PRM_FLCN_MEM_DESC pMemDesc)
                GCC_ATTRIB_SECTION("imem_libHdcp", "_hdcpCheckDmaIdx");
static void _hdcpSetDmaIndex(RM_FLCN_HDCP_CMD *pHdcpCmd)
                GCC_ATTRIB_SECTION("imem_libHdcp", "_hdcpSetDmaIndex");

static FLCN_STATUS _hdcpValidateRevocationList(PRM_FLCN_MEM_DESC, LwU32, PRM_FLCN_MEM_DESC, LwU32)
                GCC_ATTRIB_SECTION("imem_libHdcp", "_hdcpValidateRevocationList");
static FLCN_STATUS _hdcpGetRevoListSize(PRM_FLCN_MEM_DESC pSrm, LwU32 *pSrmListSize)
                GCC_ATTRIB_SECTION("imem_libHdcp", "_hdcpGetRevoListSize");
static void _hdcpCallwlateV(LwU64 M0, LwU16 bInfo, LwU32 numKsvs, PRM_FLCN_MEM_DESC pBKSVList, LwU8 numBksvs, LwU8 *pV)
                GCC_ATTRIB_SECTION("imem_libHdcp", "_hdcpCallwlateV");
static LwU32 _hdcpVCallback(LwU8 *pBuff, LwU32 offset, LwU32 size, void *pInfo)
                GCC_ATTRIB_USED()
                GCC_ATTRIB_SECTION("imem_libHdcp", "_hdcpVCallback");

//
// Interface functions are stubbed in case of HS build as interface functions are in LS code.
// HS Overlays are loaded initially before exelwting command.
//
#if UPROC_RISCV
extern void              cmdQueueSweep(PRM_FLCN_QUEUE_HDR pHdr);
#define _hdcpCmdQueueAck(pOriginalCmd, queueId) cmdQueueSweep(&(((RM_FLCN_CMD *)pOriginalCmd)->cmdGen.hdr))
#else
#define _hdcpCmdQueueAck(pOriginalCmd, queueId) osCmdmgmtCmdQSweep(&(((RM_FLCN_CMD *)pOriginalCmd)->cmdGen.hdr), (queueId))
#endif // UPROC_RISCV

static void
_hdcpSetDmaIndex(RM_FLCN_HDCP_CMD *pHdcpCmd)
{
    LwU32 *pParams;

    if (pHdcpCmd->cmdType == RM_FLCN_HDCP_CMD_ID_VALIDATE_SRM)
    {
        pParams = &(pHdcpCmd->valSrm.srm.params);
        *pParams = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, RM_DPU_DMAIDX_PHYS_SYS_COH, *pParams);
    }
    else if (pHdcpCmd->cmdType == RM_FLCN_HDCP_CMD_ID_VALIDATE_KSV)
    {
        pParams = &(pHdcpCmd->valKsv.srm.params);
        *pParams = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, RM_DPU_DMAIDX_PHYS_SYS_COH, *pParams);

        pParams = &(pHdcpCmd->valKsv.ksvList.params);
        *pParams = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, RM_DPU_DMAIDX_PHYS_SYS_COH, *pParams);

        pParams = &(pHdcpCmd->valKsv.vPrime.params);
        *pParams = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, RM_DPU_DMAIDX_PHYS_SYS_COH, *pParams);

    }
}

/*!
 * @brief Check dmaIdx from the memory descriptor for validity
 *
 * @param[in]   PRM_FLCN_MEM_DESC    Descriptor of the memory surface (FB or SYSMEM)
 */
static LwBool _hdcpCheckDmaIdx(PRM_FLCN_MEM_DESC pMemDesc)
{
    if (!DMA_ADDR_IS_ZERO(pMemDesc))
    {
        LwU8 dmaIdx = DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, pMemDesc->params);
        return vApplicationIsDmaIdxPermitted(dmaIdx);
    }
    return LW_TRUE;
}

/*!
 * @brief Send an Hdcp event to the RM.
 *
 * @param[in]   pContext    Current Hdcp context.
 * @param[in]   eventType   Type of event.
 */

static void
_hdcpSendResponse
(
    HDCP_CONTEXT   *pContext,
    LwU8            eventType
)
{
    RM_FLCN_QUEUE_HDR   hdr;
    RM_FLCN_HDCP_MSG    msg;

    msg.gen.msgType = eventType;
    msg.gen.status  = pContext->status;
    hdr.unitId      = RM_FLCN_UNIT_HDCP;
    hdr.ctrlFlags   = RM_FLCN_QUEUE_HDR_FLAGS_STATUS;
    hdr.seqNumId    = pContext->seqNumId;

    if (RM_FLCN_HDCP_CMD_ID_VALIDATE_KSV == eventType)
    {
        hdr.size        = RM_FLCN_MSG_SIZE(HDCP, VALIDATE_KSV);
        msg.ksv.head    = pContext->hdcpCmd.valKsv.head;
    }
    else
    {
        hdr.size        = RM_FLCN_MSG_SIZE(HDCP, GENERIC);
    }

#if UPROC_RISCV
    if (libIntfcHdcpSendResponse(hdr, msg) != FLCN_OK)
    {
        lwuc_halt();
    }
#else
    if (!osCmdmgmtRmQueuePostBlocking(&hdr, &msg))
    {
        lwuc_halt();
    }
#endif
}

/*!
 * @brief Process HDCP initialize command.
 *
 * Sets HDCP status bits 1, 3, and 13 to 0 for all SORs
 *
 * @param[in]   pContext    Current Hdcp context.
 */

static void
_hdcpProcessCmdInit
(
    HDCP_CONTEXT *pContext
)
{
    LwU32 i;
    LwU32 regVal = 0;

    CtlOptions = pContext->hdcpCmd.init.options;
    HdcpChipId = pContext->hdcpCmd.init.chipId;
    SorMask = pContext->hdcpCmd.init.sorMask;
    //
    //For sanity check of CtlOptions.
    //If we are in production mode CtlOptions should be 1
    //and for debug mode it should be 0 or 1.
    //
#ifdef HDCP1x_DEBUG_MODE
    if (CtlOptions > 1)
    {
        pContext->status = FLCN_ERR_ILWALID_ARGUMENT;
    }
#else
   if (CtlOptions != 0)
    {
        pContext->status = FLCN_ERR_ILWALID_ARGUMENT;
    }
#endif
    for (i = 0; i < HDCP_STATUS_SOR_MAX; i++)
    {
        if (SorMask & BIT(i))
        {
            regVal = HDCP_SET_STATUS_SOR(_REP, _ILWALID, regVal);
            regVal = HDCP_SET_STATUS_SOR(_UNPROT, _VALID, regVal);
            regVal = HDCP_SET_STATUS_SOR(_SCOPE, _FAILED, regVal);
            if (hdcpWriteStatus_HAL(&Hdcp, i, regVal) != HDCP_BUS_SUCCESS)
            {
                pContext->status = RM_FLCN_HDCP_MSG_STATUS_FAIL;
                return;
            }
        }
    }
    pContext->status = RM_FLCN_HDCP_MSG_STATUS_OK;
}


/*!
 * @brief Process HDCP SRM validation command.
 *
 * The HDCP SRM validation command validates an SRM for use with revocation.
 *
 * @param[in]   pContext    Current Hdcp context.
 */
static void
_hdcpProcessCmdValidateSrm
(
    HDCP_CONTEXT *pContext
)
{
    FLCN_STATUS status;
    RM_FLCN_HDCP_CMD_VALIDATE_SRM *pCmd = &pContext->hdcpCmd.valSrm;

    if ((_hdcpCheckDmaIdx(&pCmd->srm) == LW_FALSE)    ||
        (pCmd->srmListSize > SRM_LIST_MAX_SIZE))
    {
        pContext->status = RM_FLCN_HDCP_MSG_STATUS_ERR_SRM_FORMAT;
    }
    else
    {
        status = hdcpValidateSrm(&pCmd->srm, pCmd->srmListSize, CtlOptions);

        switch (status)
        {
            case FLCN_OK:
                pContext->status = RM_FLCN_HDCP_MSG_STATUS_OK;
                break;
            case FLCN_ERROR:
                pContext->status = RM_FLCN_HDCP_MSG_STATUS_ERR_SRM_ILWALID;
                break;
            case FLCN_ERR_ILWALID_ARGUMENT:
            default:
                pContext->status = RM_FLCN_HDCP_MSG_STATUS_ERR_SRM_FORMAT;
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
_hdcpProcessCmdValidateKsv
(
    HDCP_CONTEXT *pContext
)
{
    FLCN_STATUS status = FLCN_OK;
    RM_FLCN_HDCP_CMD_VALIDATE_KSV *pCmd = &pContext->hdcpCmd.valKsv;
    LwU32 regVal = 0;
    LwU8  v[LW_HDCPSRM_SIGNATURE_SIZE] = {0};
    LwU64 m0;

    //
    // Determine the number of downstream ksvs, excluding any repeater
    // bksvs prepended to the front of the list
    //
    LwU8 numBksvs = DRF_VAL(_RM_FLCN_HDCP, _VALIDATE_FLAGS, _NUM_BKSVS,
                       pCmd->flags);
    LwU32 numKsvs   = pCmd->ksvNumEntries - numBksvs;

    if ((pCmd->sorIndex >= HDCP_STATUS_SOR_MAX)             ||
        (pCmd->srmListSize > SRM_LIST_MAX_SIZE)             ||
        (pCmd->ksvNumEntries > LW_HDCP_SRM_MAX_ENTRIES)     ||
        (_hdcpCheckDmaIdx(&pCmd->ksvList) == LW_FALSE)      ||
        (_hdcpCheckDmaIdx(&pCmd->srm) == LW_FALSE)          ||
        (_hdcpCheckDmaIdx(&pCmd->vPrime) == LW_FALSE) )
    {
        pContext->status = RM_FLCN_HDCP_MSG_STATUS_ERR_SRM_FORMAT;
        return;
    }

    // mark as starting the operation in the status register
    regVal = HDCP_SET_STATUS_SOR(_REP, _ILWALID, regVal);
    regVal = HDCP_SET_STATUS_SOR(_UNPROT, _VALID, regVal);
    regVal = HDCP_SET_STATUS_SOR(_SCOPE, _FAILED, regVal);
    regVal = HDCP_SET_STATUS_SOR(_MASK, _NOT_READY, regVal);
    if (hdcpWriteStatus_HAL(&Hdcp, pCmd->sorIndex, regVal) != HDCP_BUS_SUCCESS)
    {
        pContext->status = RM_FLCN_HDCP_MSG_STATUS_FAIL;
        return;
    }

    pContext->status = RM_FLCN_HDCP_MSG_STATUS_OK;

    // verify the signature on the SRM
    if (!DMA_ADDR_IS_ZERO(&pCmd->srm))
    {
        status = hdcpValidateSrm(&pCmd->srm, pCmd->srmListSize, CtlOptions);
        switch (status)
        {
            case FLCN_OK:
                // signature OK
                regVal = HDCP_SET_STATUS_SOR(_UNPROT, _ILWALID, regVal);
                regVal = HDCP_SET_STATUS_SOR(_SCOPE, _FAILED, regVal);
                break;

            case FLCN_ERROR:
                // signature was bad
                pContext->status = RM_FLCN_HDCP_MSG_STATUS_ERR_SRM_ILWALID;
                status = FLCN_ERROR;
                break;

            case FLCN_ERR_ILWALID_ARGUMENT:
            default:
                // SRM failed some basic sanity tests
                pContext->status = RM_FLCN_HDCP_MSG_STATUS_ERR_SRM_FORMAT;
                status = FLCN_ERROR;
        }
    }

    // verify the KSV list
    if ((FLCN_OK == status))
    {
        if (!DMA_ADDR_IS_ZERO(&pCmd->ksvList) && !DMA_ADDR_IS_ZERO(&pCmd->vPrime))
        {
            if (HDCP_BUS_SUCCESS == hdcpReadM0_HAL(&Hdcp, &m0))
            {
                LwU8 i;

                _hdcpCallwlateV(m0, pCmd->BInfo, numKsvs,
                               &pCmd->ksvList, numBksvs, v);

                // zero out M0 to deter snoopers
                hdcpMemset(&m0, 0, sizeof(m0));

                status = hdcpDmaRead(HdcpPrimeValue, &pCmd->vPrime, 0,
                                     ALIGN_UP(LW_HDCPSRM_SIGNATURE_SIZE, 16));
                if (FLCN_OK != status)
                {
                    pContext->status = RM_FLCN_HDCP_MSG_STATUS_ERR_DMA_TRANSFER_ERROR;
                    return;
                }

                for (i = 0; i < LW_HDCPSRM_SIGNATURE_SIZE / sizeof(LwU32); i++)
                {
                    //
                    // V is stored as a big endian byte stream, while V' is
                    // stored in little endian 32-bit values. Need to make
                    // sure they are both in the same format.
                    //

                    LwU32 *p32 = (LwU32 *)HdcpPrimeValue + i;
                    *p32 = REVERSE_BYTE_ORDER(*p32);
                }

                if (0 == hdcpMemcmp(v, HdcpPrimeValue, LW_HDCPSRM_SIGNATURE_SIZE))
                {
                    // V/V' match
                    regVal = HDCP_SET_STATUS_SOR(_REP, _VALID, regVal);
                    regVal = HDCP_SET_STATUS_SOR(_UNPROT, _ILWALID, regVal);
                }
                else
                {
                    // V/V' mismatch. mark as an error
                    pContext->status = RM_FLCN_HDCP_MSG_STATUS_ERR_KSV_ILWALID;
                    status = FLCN_ERROR;
                }
            }
            else
            {
                // error oclwrred while reading M0. could not compute V
                pContext->status = RM_FLCN_HDCP_MSG_STATUS_FAIL;
                status = FLCN_ERROR;
            }
        }
    }

    // do revocation check
    if ((FLCN_OK == status) &&
        (!DMA_ADDR_IS_ZERO(&pCmd->srm)) && (!DMA_ADDR_IS_ZERO(&pCmd->ksvList)))
    {
        status = _hdcpValidateRevocationList(&pCmd->srm, pCmd->srmListSize,
                    &pCmd->ksvList, pCmd->ksvNumEntries);
        if (FLCN_OK == status)
        {
            // no KSV entries found in the SRM revocation list
            pContext->status = RM_FLCN_HDCP_MSG_STATUS_OK;
            regVal = HDCP_SET_STATUS_SOR(_UNPROT, _VALID, regVal);
            regVal = HDCP_SET_STATUS_SOR(_SCOPE, _PASSED, regVal);
        }
        else
        {
            // a revoked entry found
            pContext->status = RM_FLCN_HDCP_MSG_STATUS_ERR_SRM_REVOKED;
        }
    }

    regVal = HDCP_SET_STATUS_SOR(_MASK, _READY, regVal);
    if (hdcpWriteStatus_HAL(&Hdcp, pCmd->sorIndex, regVal) != HDCP_BUS_SUCCESS)
    {
        pContext->status = RM_FLCN_HDCP_MSG_STATUS_FAIL;
    }
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
_hdcpProcessCmdTestSelwreStatus
(
    HDCP_CONTEXT *pContext
)
{
    LwU8  status = RM_FLCN_HDCP_MSG_STATUS_OK;
    LwU32 i;
    LwU32 regSaveVal = 0;
    LwU32 regTestVal;
    LwU32 regReadVal = 0;
    HDCP_BUS_STATUS busStatus;

    for (i = 0; i < HDCP_STATUS_SOR_MAX; i++)
    {
        if (SorMask & BIT(i))
        {
            // Save off the current value
            busStatus = hdcpReadStatus_HAL(&Hdcp, i, &regSaveVal);
            if (busStatus != HDCP_BUS_SUCCESS)
            {
                status = (i << 4) | RM_FLCN_HDCP_MSG_STATUS_FAIL;
                break;
            }

            // Flip the repeater bit
            regTestVal = regSaveVal;
            if (FLD_TEST_DRF(_PDISP, _HDCP_STATUS_SOR, _REP, _VALID, regTestVal))
            {
                regTestVal = HDCP_SET_STATUS_SOR(_REP, _ILWALID, regTestVal);
            }
            else
            {
                regTestVal = HDCP_SET_STATUS_SOR(_REP, _VALID, regTestVal);
            }
            busStatus = hdcpWriteStatus_HAL(&Hdcp, i, regTestVal);
            if (busStatus != HDCP_BUS_SUCCESS)
            {
                status = (i << 4) | RM_FLCN_HDCP_MSG_STATUS_FAIL;
                break;
            }

            busStatus = hdcpReadStatus_HAL(&Hdcp, i, &regReadVal);
            if (busStatus != HDCP_BUS_SUCCESS)
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
                // failed. Error handling is intentionally left here as status is
                // already set to fail above
                //
                (void) hdcpWriteStatus_HAL(&Hdcp, i, regSaveVal);
                break;
            }
            busStatus = hdcpWriteStatus_HAL(&Hdcp, i, regSaveVal);
            if (busStatus != HDCP_BUS_SUCCESS)
            {
                status = (i << 4) | RM_FLCN_HDCP_MSG_STATUS_FAIL;
                break;
            }
        }
    }
    pContext->status = status;
}

#ifdef __COVERITY__
void TaintGlobal (RM_FLCN_HDCP_CMD *pHdcpCmd)
{
    __coverity_tainted_data_argument__(pHdcpCmd);
}

void SanitizeGlobal(RM_FLCN_HDCP_CMD *pHdcpCmd)
{
    __coverity_tainted_data_sanitize__(pHdcpCmd);
}
#endif
/*!
 * @brief Process a command.
 *
 * @param[in]   pContext    Current Hdcp context.
 * @param[in]   pCmd        Command from the RM.
 */
void
hdcpProcessCmd
(
    HDCP_CONTEXT     *pContext,
    RM_FLCN_CMD      *pCmd
)
{
    RM_FLCN_HDCP_CMD *pHdcpCmd = (RM_FLCN_HDCP_CMD *)(&pCmd->cmdGen.cmd);
    RM_FLCN_CMD *pOriginalCmd  = pContext->pOriginalCmd;

    // Over write  DMA index comming from RM
    _hdcpSetDmaIndex(pHdcpCmd);
#ifdef __COVERITY__
    TaintGlobal (pHdcpCmd);
#endif

    if (pHdcpCmd->cmdType == RM_FLCN_HDCP_CMD_ID_INIT)
    {
        // Save the INIT parameters in pContext and ack the command
        hdcpMemcpy(&pContext->hdcpCmd, &pHdcpCmd->init,
            RM_FLCN_CMD_BODY_SIZE(HDCP, INIT));
        _hdcpCmdQueueAck(pOriginalCmd, pContext->cmdQueueId);

        hdcpAttachAndLoadOverlay(HDCP_BUS);
        _hdcpProcessCmdInit(pContext);
        hdcpDetachOverlay(HDCP_BUS);
    }
    else if (pHdcpCmd->cmdType == RM_FLCN_HDCP_CMD_ID_VALIDATE_SRM)
    {
        // Save the VALIDATE_SRM parameters in pContext and ack the command
        hdcpMemcpy(&pContext->hdcpCmd, &pHdcpCmd->valSrm,
            RM_FLCN_CMD_BODY_SIZE(HDCP, VALIDATE_SRM));
        _hdcpCmdQueueAck(pOriginalCmd, pContext->cmdQueueId);
        _hdcpProcessCmdValidateSrm(pContext);
    }
    else if (pHdcpCmd->cmdType == RM_FLCN_HDCP_CMD_ID_VALIDATE_KSV)
    {
        // Save the VALIDATE_KSV parameters in pContext and ack the command
        hdcpMemcpy(&pContext->hdcpCmd, &pHdcpCmd->valKsv,
            RM_FLCN_CMD_BODY_SIZE(HDCP, VALIDATE_KSV));
        _hdcpCmdQueueAck(pOriginalCmd, pContext->cmdQueueId);

        hdcpAttachAndLoadOverlay(HDCP_BUS);
        _hdcpProcessCmdValidateKsv(pContext);
        hdcpDetachOverlay(HDCP_BUS);
    }
    else if (pHdcpCmd->cmdType == RM_FLCN_HDCP_CMD_ID_TEST_SELWRE_STATUS)
    {
        _hdcpCmdQueueAck(pOriginalCmd, pContext->cmdQueueId);

        hdcpAttachAndLoadOverlay(HDCP_BUS);
        _hdcpProcessCmdTestSelwreStatus(pContext);
        hdcpDetachOverlay(HDCP_BUS);
    }
    else
    {
        _hdcpCmdQueueAck(pOriginalCmd, pContext->cmdQueueId);
    }
#ifdef __COVERITY__
     SanitizeGlobal (pHdcpCmd);
#endif

    _hdcpSendResponse(pContext, pHdcpCmd->cmdType);
}

/*!
 * @brief Verifies whether an entry in the KSV list is on the SRM's revocation
 * list or not.
 *
 * Since the KSV list and SRM are located in system memory, the function will
 * read chunks of each, scanning each until it either finds a match or reaches
 * the end of both lists.
 *
 * @param pSrmList
 *      The SRM, stored in system memory.
 *
 * @param srmLength
 *      The size of the SRM in bytes.
 *
 * @param pKsvList
 *      The KSV list, stored in system memory.
 *
 * @param ksvListSize
 *      The number of KSV entries.
 *
 * @return If a match was not found, FLCN_OK is returned; otherwise, FLCN_ERROR
 *      is returned.
 */
static FLCN_STATUS
_hdcpValidateRevocationList
(
    PRM_FLCN_MEM_DESC   pSrmList,
    LwU32               srmLength,
    PRM_FLCN_MEM_DESC   pKsvList,
    LwU32               ksvListSize
)
{
    LwU8        srmChunk[CHUNK_SIZE] = {0};
    LwU8        ksvChunk[CHUNK_SIZE] = {0};
    LwU32       srmListSize;
    LwU32       ksvIterSize;
    LwU32       srmOffset;
    LwU32       ksvOffset;

    LwU8        numSrmChunks;
    LwU8        numKsvChunks;
    LwU8        srmChunkEntries;
    LwU8        ksvChunkEntries;
    LwU8        srmChunkOffset;
    LwU8        ksvChunkOffset;

    LwU8        srmChunkIterator;
    LwU8        ksvChunkIterator;
    LwU8        srmEntryIterator;
    LwU8        ksvEntryIterator;

    LwU8        i;
    LwU8        j;
    FLCN_STATUS status;

    status = _hdcpGetRevoListSize(pSrmList, &srmListSize);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // Determine the number of DMA reads we need to do to read in the full
    // SRM revocation and KSV list. Since we are limited in memory and these
    // lists may be very large, they will need to be read in as chunks. The
    // chunks are designed to read in 8 entries from the list at a time.
    //
    numSrmChunks = (srmListSize) ?
                   ((srmListSize - 1) / NUM_ENTRIES_PER_CHUNK) + 1 : 0;
    numKsvChunks = (ksvListSize) ?
                   ((ksvListSize - 1) / NUM_ENTRIES_PER_CHUNK) + 1 : 0;

    srmOffset = LW_HDCPSRM_DEVICE_KSV_LIST;

    for (srmChunkIterator = 0; srmChunkIterator < numSrmChunks;
         srmChunkIterator++)
    {
        //
        // For each SRM chunk, we need to iterate across the KSV list to see
        // if any of the KSV entries match the set of entries found in the SRM
        // chunk.
        //
        if (hdcpAuthDmaRead(srmChunk, pSrmList, srmOffset, CHUNK_SIZE) != CHUNK_SIZE)
        {
            return FLCN_ERR_DMA_GENERIC;
        }
        srmChunkEntries = (srmListSize < NUM_ENTRIES_PER_CHUNK) ?
                           srmListSize : NUM_ENTRIES_PER_CHUNK;

        ksvOffset = 0;
        ksvIterSize = ksvListSize;
        for (ksvChunkIterator = 0; ksvChunkIterator < numKsvChunks;
             ksvChunkIterator++)
        {
            if (hdcpAuthDmaRead(ksvChunk, pKsvList, ksvOffset, CHUNK_SIZE) != CHUNK_SIZE)
            {
                return FLCN_ERR_DMA_GENERIC;
            }
            ksvChunkEntries = (ksvIterSize < NUM_ENTRIES_PER_CHUNK) ?
                               ksvIterSize : NUM_ENTRIES_PER_CHUNK;

            //
            // Now that we have a chunk from both the SRM and KSV, verify
            // none of the entries in the chunks match. If a match is found,
            // it means that one of the devices in the KSV list has been
            // revoked.
            //
            for (srmEntryIterator = 0, srmChunkOffset = 0;
                 srmEntryIterator < srmChunkEntries;
                 srmEntryIterator++, srmChunkOffset += HDCP_RCV_BKSV_SIZE)
            {
                for (ksvEntryIterator = 0, ksvChunkOffset = 0;
                     ksvEntryIterator < ksvChunkEntries;
                     ksvEntryIterator++, ksvChunkOffset += HDCP_RCV_BKSV_SIZE)
                {
                    //
                    // The revoked devices in the SRM as stored as big endian
                    // values, whereas the KSV entries are stored as little
                    // endian values. Need to take this into consideration
                    // when performing the comparison.
                    //
                    for (i = 0, j = HDCP_RCV_BKSV_SIZE - 1;
                         i < HDCP_RCV_BKSV_SIZE;
                         i++, j--)
                    {

                        if (srmChunk[srmChunkOffset+j] !=
                            ksvChunk[ksvChunkOffset+i])
                        {
                            break;
                        }
                    }

                    if (i == HDCP_RCV_BKSV_SIZE)
                    {
                        //
                        // We've managed to compare HDCP_RCV_BKSV_SIZE bytes
                        // and the values are equal. That means we have a
                        // match and a revoked device. We can stop scanning at
                        // this point, since it doesn't matter how many
                        // additional devices have been revoked.
                        //
                        return FLCN_ERROR;
                    }
                }
            }

            ksvOffset   += CHUNK_SIZE;
            ksvIterSize -= ksvChunkEntries;
        }

        srmOffset   += CHUNK_SIZE;
        srmListSize -= srmChunkEntries;
    }

    return FLCN_OK;
}

/*!
 * Returns the number of devices in the revocation list
 *
 * @param[in]   srm
 *      The location in system memory of the SRM.
 * @param[out]   srmListsize
 *      The number of devices in the revocation list.
 *
 * @return FLCN_OK if no error
 *         else return FLCN_ERROR in case of error in DMA read
 */
static FLCN_STATUS
_hdcpGetRevoListSize
(
    PRM_FLCN_MEM_DESC pSrm,
    LwU32 *pSrmListSize
)
{
    LwU8 srmChunk[HDCP_DMA_READ_ALIGNMENT] = {0};

    if (hdcpAuthDmaRead(srmChunk, pSrm, 0, HDCP_DMA_READ_ALIGNMENT) != HDCP_DMA_READ_ALIGNMENT)
    {
        return FLCN_ERR_DMA_GENERIC;
    }

    //
    // LW_HDCPSRM_NUM_DEVICES shares the same byte as LW_HDCPSRM_RESERVED3.
    // Therefore, we want to remove the reserved bits from the value.
    //
    *pSrmListSize = srmChunk[LW_HDCPSRM_NUM_DEVICES] & ~LW_HDCPSRM_RESERVED3;
    return FLCN_OK;
}

/*!
 * @brief Copy callback function for SHA-1 when callwlating V.
 *
 * The value of V is callwlated by taking the SHA-1 hash of the KSV list,
 * B<sub>info</sub>, and M<sub>0</sub>. The KSV list is stored in system
 * memory, which requires DMA transfers to get the data. B<sub>info</sub> is
 * passed to the FLCN with the command, and M<sub>0</sub> is obtained using the
 * display private bus.
 *
 * @param[out]  pBuff
 *      A memory buffer local to the SHA-1 function.
 *
 * @param[in]   offset
 *      The offset within the source data to begin copying from.
 *
 * @param[in]   size
 *      The size, in bytes, to copy.
 *
 * @param[in]   pInfo
 *      Pointer to a structure that contains the information needed to perform
 *      a DMA transfer of the KSV list, B<sub>info</sub>, and M<sub>0</sub>.
 *
 * @return The number of bytes copied into the buffer.
 */
static LwU32
_hdcpVCallback
(
    LwU8   *pBuff,
    LwU32   offset,
    LwU32   size,
    void   *pInfo
)
{
    VCallbackInfo *pCb = (VCallbackInfo *) pInfo;

    LwU32 copySize;
    LwU32 startOffset;
    LwU32 endOffset;
    LwU32 totalSize = 0;

    LwU32 bufferOffset;  // offset within the destination buffer
    LwU32 i;

    bufferOffset = 0;
    while (size > 0)
    {
        //
        // Need to copy the KSV list into the buffer to pass back to the SHA-1
        // function. Need to skip over any BKsvs that are being stored at the
        // beginning of the KSV list, as they are not part of the list itself.
        // The KSV list is pCb->ksvListSize in size.
        //
        if (offset < (HDCP_V_KSV_LIST_OFFSET + HDCP_V_KSV_LIST_SIZE(pCb)))
        {
            copySize      = LW_MIN(size, HDCP_V_KSV_LIST_SIZE(pCb) - offset);
            bufferOffset += hdcpAuthDmaRead(pBuff, pCb->pKsvList,
                                            pCb->bksvListSize + offset, copySize);
        }
        //
        // The offset is within the Binfo, which is 2 bytes in size.
        //
        else if (offset < (HDCP_V_BINFO_OFFSET(pCb) + HDCP_V_BINFO_SIZE))
        {
            copySize    = LW_MIN(size, HDCP_V_BINFO_SIZE);
            startOffset = offset - HDCP_V_BINFO_OFFSET(pCb);
            endOffset   = LW_MIN(startOffset + copySize, HDCP_V_BINFO_SIZE);

            for (i = startOffset; i < endOffset; i++)
            {
                pBuff[bufferOffset] = (LwU8) ((pCb->bInfo >> (i << 3)) & 0xFF);
                bufferOffset++;
            }
        }
        //
        // Final field in the SHA-1 input stream is M0, which is 8 bytes in
        // size.
        //
        else if (offset < (HDCP_V_M0_OFFSET(pCb) + HDCP_V_M0_SIZE))
        {
            copySize    = LW_MIN(size, HDCP_V_M0_SIZE);
            startOffset = offset - HDCP_V_M0_OFFSET(pCb);
            endOffset   = LW_MIN(startOffset + copySize, HDCP_V_M0_SIZE);

            for (i = startOffset; i < endOffset; i++)
            {
                pBuff[bufferOffset] = (LwU8) ((pCb->m0 >> (i << 3)) & 0xFF);
                bufferOffset++;
            }
        }
        // Invalid offset
        else
        {
            break;
        }

        size      -= copySize;
        offset    += copySize;
        totalSize += copySize;
    }
    return totalSize;
}

/*!
 * @brief   Callwlates V as defined in the HDCP spec.
 *
 * @param[in]  M0
 *          M<sub>0</sub> as defined in the HDCP spec.
 *
 * @param[in]  bInfo
 *          Value of B<sub>Info</sub> as defined in the HDCP spec.
 *
 * @param[in]  numKSVs
 *          Number of KSVs in the KSV List.
 *
 * @param[in]  pBKSVList
 *          Pointer to the KSVList.
 *
 * @param[out]  pV
 *          Pointer to the return value of V.
 */
static void
_hdcpCallwlateV
(
    LwU64              M0,
    LwU16              bInfo,
    LwU32              numKsvs,
    PRM_FLCN_MEM_DESC  pBKSVList,
    LwU8               numBksvs,
    LwU8              *pV
)
{
    LwU32 size = 0;
    VCallbackInfo cbInfo;

    if ((!pV) || (numKsvs > LW_HDCP_SRM_MAX_ENTRIES))
    {
        return;
    }

    hdcpMemset(pV, 0, LW_HDCPSRM_SIGNATURE_SIZE);

    // 1. Copy BKSVList
    cbInfo.pKsvList    = pBKSVList;
    cbInfo.ksvListSize = numKsvs * HDCP_RCV_BKSV_SIZE;
    cbInfo.bksvListSize = numBksvs * HDCP_RCV_BKSV_SIZE;

    // 2. Copy BStatus
    cbInfo.bInfo = bInfo;

    // 3. Copy M0
    cbInfo.m0 = M0;

    // 4. Compute SHA-1: we let the callback function copy the appropriate
    // data into the SHA-1 function.
    size = cbInfo.ksvListSize + sizeof(LwU16) + sizeof(LwU64);

    hdcpAttachAndLoadOverlay(HDCP_SHA1);
    libSha1(pV, (void *) &cbInfo, size, (FlcnSha1CopyFunc (*))(_hdcpVCallback));
    hdcpDetachOverlay(HDCP_SHA1);
}

