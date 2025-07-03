/*! _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_cmds.c
 * @brief  This task implements HDCP 2.2 state mechine
 *         Here is SW IAS describes flow
 *         //sw/docs/resman/chips/Maxwell/gm20x/Display/HDCP-2-2_SW_IAS.doc
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "lwoscmdmgmt.h"
#include "hdcp22wired_cmn.h"
#include "displayport.h"
#include "hdcp22wired_ake.h"
#include "hdcp22wired_akeh.h"
#include "hdcp22wired_lc.h"
#include "hdcp22wired_repeater.h"
#include "hdcp22wired_ske.h"
#include "hdcp22wired_srm.h"
#include "hdcp22wired_timer.h"
#include "seapi.h"
#include "mem_hs.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define _hdcp22wiredStartTimer(timeoutUs)   hdcp22wiredStartTimer((timeoutUs));
#define _hdcp22wiredStopTimer()             hdcp22wiredStopTimer()

#define GET_ACTIVE_SESSION_INDEX(pActiveSession)                            \
    ((LwUPtr)pActiveSession - (LwUPtr)&activeSessions[0])/(sizeof(HDCP22_ACTIVE_SESSION))

/* ------------------------- Function Prototypes ---------------------------- */
static FLCN_STATUS  _hdcp22GetRxStatusForReAuth(HDCP22_SESSION *, LwU16 *)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22GetRxStatusForReAuth");
static void         _hdcp22SessionInit(HDCP22_SESSION *)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22SessionInit");
static FLCN_STATUS _hdcp22wiredStartSessionWithType1LockCheck(HDCP22_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22wiredStartSessionWithType1LockCheck");
static FLCN_STATUS  _hdcp22SessionStartAuth(HDCP22_SESSION *)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22SessionStartAuth");
static FLCN_STATUS  _hdcp22HandleSessionAkeInit(HDCP22_SESSION *)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22HandleSessionAkeInit");
static void         _hdcp22StateReset(HDCP22_SESSION *, LwBool bCleanActiveSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22StateReset");
static FLCN_STATUS  _hdcp22SaveSession(HDCP22_SESSION *)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22SaveSession");
static FLCN_STATUS  _hdcp22RetrieveSession(HDCP22_SESSION *, HDCP22_ACTIVE_SESSION **)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22RetrieveSession");
static LwBool       _hdcp22ValidHdmiReAuthRequest(LwU16 i2cPort)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22ValidHdmiReAuthRequest");
static FLCN_STATUS  _hdcp22CheckHdcp22Version(HDCP22_SESSION *)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22CheckHdcp22Version");
static FLCN_STATUS  _hdcp22StartRepeaterAuth(HDCP22_SESSION  *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22StartRepeaterAuth");
static LwBool       _hdcp22IsCertRxReady(HDCP22_SESSION *pSession, LwU16 rxStatus)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22IsCertRxReady");
static LwBool       _hdcp22IsHprimeReady(HDCP22_SESSION *pSession, LwU16 rxStatus)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22IsHprimeReady");
static LwBool       _hdcp22IsPairingAvailable(HDCP22_SESSION *pSession, LwU16 rxStatus)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22IsPairingAvailable");
static LwBool       _hdcp22IsLprimeReady(HDCP22_SESSION *pSession, LwU16 rxStatus)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22IsLprimeReady");
static LwBool       _hdcp22IsRepeaterReady(HDCP22_SESSION *pSession, LwU16 rxStatus)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22IsRepeaterReady");
static LwBool       _hdcp22IsMprimeReady(HDCP22_SESSION *pSession, LwU16 rxStatus)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22IsMprimeReady");
static FLCN_STATUS  _hdcp22SrmCheckRxIds(HDCP22_SESSION *pSession, LwU8 *pKsvList, LwU16 noOfDevices)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22SrmCheckRxIds");
static FLCN_STATUS  _hdcp22DpWriteStreamType(HDCP22_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22DpWriteStreamType");
static LwBool       _hdcp22CheckDmaIdx(PRM_FLCN_MEM_DESC pMemDesc)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22CheckDmaIdx");
static LwBool       _hdcp22IsStreamTypeChngRequest(RM_FLCN_HDCP22_CMD *pHdcp22Cmd, HDCP22_SESSION  *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22IsStreamTypeChngRequest");
static FLCN_STATUS  _hdcp22HandleStreamTypeChngReq(RM_FLCN_HDCP22_CMD *pHdcp22Cmd, void  *pCmd, HDCP22_SESSION  *pSession, HDCP22_ACTIVE_SESSION *pActiveSession, LwU8 cmdQueueId, LwU8 seqNumId)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22HandleStreamTypeChngReq");
static inline void _swapEndianness(void *pOutData, const void *pInData, const LwU32 size)   // Must be inline for LS/HS 2 version funcs.
    GCC_ATTRIB_ALWAYSINLINE();

/* ------------------------- Global Variables ------------------------------- */
HDCP22_SESSION          lwrrentSession
    GCC_ATTRIB_ALIGN(sizeof(LwU32));
HDCP22_ACTIVE_SESSION   activeSessions[HDCP22_ACTIVE_SESSIONS_MAX]
    GCC_ATTRIB_ALIGN(sizeof(LwU32));
LwU32                   gActiveSessionsHashVal[HDCP22_ACTIVE_SESSIONS_MAX][HDCP22_SIZE_INTEGRITY_HASH_U32];

/*!
 * globals related to sec2 type enforcement (lock/unlock) request
 */
LwBool gBType1LockState = 0;
LwBool gBBackGrndReAuthIsInProgress = LW_FALSE;
LwU8   gBgLwrrentReAuthSessionIndex = 0;

/*!
 * gBInitError variable is used to catch register error
 * at this task initialization
 * when a command is received from RM in hdcp22wiredProcessCmd
 * then this variable will be checked if any error oclwred.
 */
LwBool gBInitError = LW_FALSE;

/*!
 * This flag indicated I2C/AUX overlay is loaded at that point or not
 */
static LwBool gBIsAuxI2cLoaded = LW_FALSE;

/*!
 * This index indicated authentication stage attached overlay that need to
 * be detached when detecting reAuth request.
 */
static HDCP_OVERLAY_ID gLastAttachedOverlay = HDCP_OVERLAYID_MAX;

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief Check dmaIdx from the memory descriptor for validity
 *
 * @param[in]   PRM_FLCN_MEM_DESC    Descriptor of the memory surface
 *                                   (FB or SYSMEM)
 */
static LwBool
_hdcp22CheckDmaIdx
(
    PRM_FLCN_MEM_DESC pMemDesc
)
{
    if (!DMA_ADDR_IS_ZERO(pMemDesc))
    {
        LwU8 dmaIdx = DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                              pMemDesc->params);
        return vApplicationIsDmaIdxPermitted(dmaIdx);
    }

    return LW_TRUE;
}

/*
 * @brief: This function validates receiver id list with SRM revoked receiver Ids
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @param[in]  pKsvList         Pointer to the KSV list.
 * @param[in]  noOfDevices      No of devices in KSV list.
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
*/
static FLCN_STATUS
_hdcp22SrmCheckRxIds
(
    HDCP22_SESSION *pSession,
    LwU8 *pKsvList,
    LwU16 noOfDevices
)
{
    FLCN_STATUS status = FLCN_OK;

    if (!DMA_ADDR_IS_ZERO(&pSession->srmDma))
    {
        hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_FALSE);

        hdcpAttachAndLoadOverlay(HDCP22WIRED_CERTRX_SRM);
        status = hdcp22RevocationCheck(pKsvList, noOfDevices, &pSession->srmDma);
        hdcpDetachOverlay(HDCP22WIRED_CERTRX_SRM);

        hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_TRUE);
    }

    //
    // DD doesn't always populate SRM Buffer so if SRM Buffer is NULL
    // return OK.
    //
    return status;
}

/*!
 *  @brief This function Reads hdcp22Bcaps for HDCP22Version. DCP issued
 *  clarification that if DUT doesn't read HDCP22Version before authentication
 *  treats as error instead warning.
 *
 *
 *  @param[in]     pSession      pointer to active HDCP22 session.
 *  @returns       FLCN_STATUS   FLCN_OK succeed and hdcp22,
 *                               FLCN_ERR_NOT_SUPPORTED if not hdcp22,
 *                               error code otherwise.
 */
static FLCN_STATUS
_hdcp22CheckHdcp22Version
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;

    if (!pSession->bIsAux)
    {
        LwU8 version = 0;

        status = hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
                                                      pSession->priDDCPort,  // HDMI always on primary port.
                                             LW_HDMI_HDCP22_VERSION,
                                             LW_HDMI_HDCP22_VERSION_SIZE,
                                             (LwU8 *)&version,
                                             LW_TRUE);

        if (!FLD_TEST_DRF(_HDMI_HDCP22, _VERSION, _CAPABLE, _YES, version))
        {
            status = FLCN_ERR_NOT_SUPPORTED;
        }
    }

    return status;
}

/*!
 *  @brief Send DP SST streamType value message.
 *  @param[in]  pSession          pointer to the current session
 *  @returns    FLCN_STATUS       returns the status of transaction
 */
static FLCN_STATUS
_hdcp22DpWriteStreamType
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pSession->bIsAux && !pSession->sesVariablesIr.bRepeater)
    {
        //
        // Write StreamType to DP non-repeater device, and type1Locked case
        // is already processed.
        //
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_STREAM_TYPE,
            HDCP22_SIZE_RX_STREAM_TYPE,
            &pSession->sesVariablesRptr.streamIdType[0].streamType,
            LW_FALSE));
    }
    else
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
    }

label_return:
    return status;
}

/*!
 *  @brief This function tells if CertRx is ready per Rx status
 *  @param[in]  pSession     Pointer to active HDCP22 session
 *  @param[in]  rxStatus     Received RxSatus register value
 *  @returns    LwBool       LW_TRUE on ready
 */
static LwBool
_hdcp22IsCertRxReady
(
    HDCP22_SESSION  *pSession,
    LwU16            rxStatus
)
{
    if (pSession->bIsAux)
    {
        //
        // There's no state register to check, assume DP CertRx is ready with
        // suciffient polling time.
        //
        return LW_TRUE;
    }
    else
    {
        if (FLD_TEST_DRF_NUM(_RCV_HDCP22, _RX_STATUS, _MESSAGE_SIZE,
                HDCP22_SIZE_MSG_SEND_CERT, rxStatus))
        {
            return LW_TRUE;
        }
    }

    return LW_FALSE;
}

/*!
 *  @brief This function tells if Hprime is ready per Rx status
 *  @param[in]  pSession     Pointer to active HDCP22 session
 *  @param[in]  rxStatus     Received RxSatus register value
 *  @returns    LwBool       LW_TRUE on ready
 */
static LwBool
_hdcp22IsHprimeReady
(
    HDCP22_SESSION  *pSession,
    LwU16            rxStatus
)
{
    if (pSession->bIsAux)
    {
        if (FLD_TEST_DRF(_DPCD, _HDCP22_RX_STATUS, _HPRIME_AVAILABLE, _YES,
                rxStatus))
        {
            return LW_TRUE;
        }
    }
    else
    {
        if (FLD_TEST_DRF_NUM(_RCV_HDCP22, _RX_STATUS, _MESSAGE_SIZE,
                HDCP22_SIZE_MSG_SEND_HPRIME, rxStatus))
        {
            return LW_TRUE;
        }
    }

    return LW_FALSE;
}

/*!
 *  @brief This function tells if Pairing Info is ready per Rx status
 *  @param[in]  pSession     Pointer to active HDCP22 session.
 *  @param[in]  rxStatus     Received RxSatus register value
 *  @returns    LwBool       LW_TRUE on ready.
 */
static LwBool
_hdcp22IsPairingAvailable
(
    HDCP22_SESSION  *pSession,
    LwU16            rxStatus
)
{
    if (pSession->bIsAux)
    {
        if (FLD_TEST_DRF(_DPCD, _HDCP22_RX_STATUS, _PAIRING_AVAILABLE, _YES,
                rxStatus))
        {
            return LW_TRUE;
        }
    }
    else
    {
        if (FLD_TEST_DRF_NUM(_RCV_HDCP22, _RX_STATUS, _MESSAGE_SIZE,
                HDCP22_SIZE_MSG_PAIRING_INFO, rxStatus))
        {
            return LW_TRUE;
        }
    }

    return LW_FALSE;
}


/*!
 *  @brief This function tells if Lprime ready state per Rx status
 *  @param[in]  pSession     Pointer to active HDCP22 session
 *  @param[in]  rxStatus     Received RxSatus register value
 *  @returns    LwBool       LW_TRUE on ready
 */
static LwBool
_hdcp22IsLprimeReady
(
    HDCP22_SESSION  *pSession,
    LwU16            rxStatus
)
{
    if (pSession->bIsAux)
    {
        //
        // There's no state register to check, assume DP Lprime is ready with
        // suciffient polling time.
        //
        return LW_TRUE;
    }
    else
    {
        if (FLD_TEST_DRF_NUM(_RCV_HDCP22, _RX_STATUS, _MESSAGE_SIZE,
                HDCP22_SIZE_MSG_SEND_LPRIME, rxStatus))
        {
            return LW_TRUE;
        }
    }

    return LW_FALSE;
}

/*!
 *  @brief This function tells if Mprime is ready per Rx status.
 *  @param[in]  pSession     Pointer to active HDCP22 session
 *  @param[in]  rxStatus     Received RxSatus register value
 *  @returns    LwBool       LW_TRUE on ready
 */
static LwBool
_hdcp22IsMprimeReady
(
    HDCP22_SESSION  *pSession,
    LwU16           rxStatus
)
{
    if (pSession->bIsAux)
    {
        //
        // There's no state register to check, assume DP Mprime is ready with
        // suciffient polling time.
        //
        return LW_TRUE;
    }
    else
    {
        if (FLD_TEST_DRF_NUM(_RCV_HDCP22, _RX_STATUS, _MESSAGE_SIZE,
                HDCP22_SIZE_MSG_RPT_STR_READY, rxStatus))
        {
            return LW_TRUE;
        }
    }

    return LW_FALSE;
}

/*!
 *  @brief Handles AKE INIT stage of HDCP22
 *  @param[in]  pSession        Pointer to active HDCP22 session.
 *  @returns    FLCN_STATUS     FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22HandleSessionAkeInit
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pSession->bIsAux)
    {
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RTX,
            HDCP22_SIZE_R_TX,
            (LwU8*)&pSession->sesVariablesAke.rTx[0],
            LW_FALSE));

        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_TX_CAPS,
            HDCP22_SIZE_TX_CAPS,
            (LwU8*)&pSession->txCaps[0],
            LW_FALSE));
    }
    else
    {
        LwU8 msgBuffer[HDCP22_SIZE_MSG_AKE_INIT];

        hdcpmemset(msgBuffer, 0, HDCP22_SIZE_MSG_AKE_INIT);

        //
        // Prepare AKE_INIT message
        msgBuffer[0] = HDCP22_MSG_ID_AKE_INIT;
        hdcpmemcpy(&msgBuffer[HDCP22_SIZE_MSG_ID], &pSession->sesVariablesAke.rTx[0], HDCP22_SIZE_R_TX);
        hdcpmemcpy(&msgBuffer[HDCP22_SIZE_MSG_ID + HDCP22_SIZE_R_TX], &pSession->txCaps[0], HDCP22_SIZE_TX_CAPS);

        CHECK_STATUS(hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
            pSession->priDDCPort,  // HDMI always on primary port.
            LW_HDMI_SCDC_WRITE_OFFSET,
            HDCP22_SIZE_MSG_AKE_INIT,
            msgBuffer,
            LW_FALSE));
    }

    // Reset this Seq Number everytime AKE INIT is sent
    pSession->sesVariablesRptr.seqNumM = 0;

label_return:
    return status;
}

/*!
 *  @brief This function Reads RX Status on HDMI and check whether
 *         its valid ReAuth Request
 *  @param[in]     pSession     pointer to active HDCP22 session.
 *  @returns       LwBool       LW_TRUE if ReAuth request.
 *                              Lw_FALSE if no Re Auth request.
 */
static LwBool
_hdcp22ValidHdmiReAuthRequest
(
    LwU16 i2cPort
)
{
    LwU16 rxStatus = 0;
    FLCN_STATUS status = FLCN_OK;

    status =  hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
                                                   i2cPort, // HDMI always on primary port.
                                                   LW_RCV_HDCP22_RX_STATUS,
                                                   LW_RCV_HDCP22_RX_STATUS_SIZE,
                                                   (LwU8 *)&rxStatus,
                                                   LW_TRUE);

    // If the I2C read itself is failing we need to disable Enc.
    if (status != FLCN_OK)
    {
        return LW_TRUE;
    }
    return FLD_TEST_DRF(_RCV_HDCP22_RX_STATUS, _REAUTH, _REQUEST, _YES, rxStatus);
}

/*!
 *  @brief This function Reads RX Status on sink and check whether
 *         there is re-auth Request.
 *         Function returns FLCN_ERR_HDCP22_ABORT_AUTHENTICATION only in
 *         case of Re-Auth request on which the Authentication is aborted.
 *         Function returns FLCN_OK even for I2C/AUX errors as we
 *         dont want to abort Authentication in this case
 *
 *  @param[in]     pSession      pointer to active HDCP22 session.
 *  @param[out]    pRecvMsgSize  Size of the Rx message.
 *  @returns       FLCN_STATUS   FLCN_ERROR if ReAuth request.
 *                               FLCN_OK otherwise.
 */
static FLCN_STATUS
_hdcp22GetRxStatusForReAuth
(
    HDCP22_SESSION *pSession,
    LwU16          *pRxStatus
)
{
    LwU16       rxStatus = 0;
    FLCN_STATUS status   = FLCN_OK;

    // Check if got unplug from RM and abort active authentication immediately.
    if (pSession->bHandleHpdFromRM)
    {
        // If got HPd to abort, need to reset the WAR flag that no more retry from RM.
        pSession->bApplyHwDrmType1LockedWar = LW_FALSE;

        pSession->bHandleHpdFromRM = LW_FALSE;
        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_HPD;
        status = FLCN_ERR_HPD_UNPLUG;
        goto label_return;
    }

    if (!pSession->bIsAux)
    {
        status = hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
                                                      pSession->priDDCPort,  // HDMI always on primary port.
                                                      LW_RCV_HDCP22_RX_STATUS,
                                                      LW_RCV_HDCP22_RX_STATUS_SIZE,
                                                      (LwU8 *)&rxStatus,
                                                      LW_TRUE);
        if (status != FLCN_OK)
        {
            status = FLCN_OK;
            goto label_return;
        }

        if (FLD_TEST_DRF(_RCV_HDCP22, _RX_STATUS, _REAUTH_REQUEST, _YES,
                         rxStatus))
        {
            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_REAUTH_REQ;
            status = FLCN_ERR_HDCP22_ABORT_AUTHENTICATION;
            goto label_return;
        }
    }
    else
    {
        status = hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
                     HDCP22_AUX_PORT(pSession),
                     LW_DPCD_HDCP22_RX_STATUS,
                     LW_DPCD_HDCP22_RX_STATUS_SIZE,
                     (LwU8*)&rxStatus,
                     LW_TRUE);
        if (status != FLCN_OK)
        {
            if (FLCN_ERR_HPD_UNPLUG == status)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_HPD;
            }
            else
            {
                status = FLCN_OK;
            }
            goto label_return;
        }

        if (FLD_TEST_DRF(_DPCD, _HDCP22_RX_STATUS, _LINK_INTEGRITY_FAILURE,
                _YES, rxStatus))
        {
            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_REAUTH_REQ;
            status = FLCN_ERR_HDCP22_ABORT_AUTHENTICATION;
        }
        else if (FLD_TEST_DRF(_DPCD, _HDCP22_RX_STATUS, _REAUTH_REQUEST,
                              _YES, rxStatus) ||
                 pSession->bReAuthNotified)
        {
            if (pSession->bReAuthNotified)
            {
                //
                // Bug 2860192: With Synaptics MST hub, uproc event handling is
                // using polling thus cannot tell RM reAuth or rxStatus READY
                // notification comes earlier. In the case, prefer to treat
                // READY comes earlier then complete whole RxIdList stage that
                // will clear pending READY bit.
                //
                if (FLD_TEST_DRF(_DPCD, _HDCP22_RX_STATUS, _READY, _YES, rxStatus) ||
                    (pSession->stage == HDCP22_AUTH_STAGE_WAIT_TO_RXID_ACK))
                {
                    goto label_return;
                }

                pSession->bReAuthNotified = LW_FALSE;
            }

            //
            // DP dongle always throw reAuth CP_IRQ before L_HPD, and need to
            // wait a while to see if abort the reAuthentication due to
            // following L-HPD.
            //
            if (HDCP22_AUTH_STAGE_WAIT_FOR_REAUTH != pSession->stage)
            {
                pSession->stage = HDCP22_AUTH_STAGE_WAIT_FOR_REAUTH;
                pSession->hdcp22TimerCount = 0;
                status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_HPD);
                if (status != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                    goto label_return;
                }

                //
                // Override status to and abort authentication at
                // HDCP22_AUTH_STAGE_WAIT_FOR_REAUTH stage.
                //
                status = FLCN_ERR_HDCP22_DELAY_ABORT_AUTHENTICATION;
            }
        }
    }

label_return:

    if ((status != FLCN_OK) &&
        (gLastAttachedOverlay < HDCP_OVERLAYID_MAX))
    {
        hdcp22AttachDetachOverlay(gLastAttachedOverlay, LW_FALSE);
    }

    *pRxStatus = rxStatus;
    return status;
}

/*!
 *  @brief      This function saves the session variables of current active
 *              session.
 *  @param[in]  pSession     Pointer to active HDCP22 session.
 *  @returns    FLCN_STATUS     FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22SaveSession
(
    HDCP22_SESSION *pSession
)
{
    LwU8                  i;
    HDCP22_ACTIVE_SESSION *pActiveSession = NULL;
    FLCN_STATUS           status          = FLCN_OK;
    LwBool                bEncActive      = LW_FALSE;

    // Find previous active session.
    for (i = 0; i < HDCP22_ACTIVE_SESSIONS_MAX; i++)
    {
        pActiveSession = &activeSessions[i];

        //
        // DPAux, HDMI may share same hybrid physical port thus need to check
        // protocol also.
        //
        if ((pActiveSession->priDDCPort == pSession->priDDCPort) &&
            (pActiveSession->sorProtocol == pSession->sorProtocol))
        {
            break;
        }
    }

    // If not found, find nonactive session.
    if (i == HDCP22_ACTIVE_SESSIONS_MAX)
    {
        for (i = 0; i < HDCP22_ACTIVE_SESSIONS_MAX; i++)
        {
            pActiveSession = &activeSessions[i];
            if (pActiveSession->bActive)
            {
                CHECK_STATUS(hdcp22wiredEncryptionActive_HAL(&Hdcp22wired, pActiveSession->sorNum, &bEncActive));
                if (bEncActive)
                {
                    continue;
                }
            }
            break;
        }
    }

    // Save session state variables.
    pActiveSession->bActive = LW_TRUE;
    pActiveSession->sorNum = pSession->sorNum;
    pActiveSession->sorProtocol = pSession->sorProtocol;
    pActiveSession->priDDCPort = pSession->priDDCPort;
    pActiveSession->secDDCPort = pSession->secDDCPort;
    pActiveSession->seqNumV = pSession->seqNumV;
    pActiveSession->numStreams = pSession->sesVariablesRptr.numStreams;
    pActiveSession->bEnforceType0Hdcp1xDS = pSession->sesVariablesRptr.bEnforceType0Hdcp1xDS;
    hdcpmemcpy(pActiveSession->streamIdType, pSession->sesVariablesRptr.streamIdType,
               sizeof(HDCP22_STREAM) * pSession->sesVariablesRptr.numStreams);

    pActiveSession->srmDma = pSession->srmDma;
    pActiveSession->bIsAux = pSession->bIsAux;
    pActiveSession->bIsRepeater = pSession->sesVariablesIr.bRepeater;
    pActiveSession->bTypeChangeRequired = pSession->sesVariablesIr.bTypeChangeRequired;

    status = hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                        (LwU32*)pActiveSession,
                                                        &gActiveSessionsHashVal[GET_ACTIVE_SESSION_INDEX(pActiveSession)][0],
                                                        sizeof(HDCP22_ACTIVE_SESSION),
                                                        LW_TRUE);

label_return:
    return status;
}

/*!
 *  @brief      This function finds the matching session and retrieves the
 *              session from saved ones.
 *  @param[in]  pSession  Pointer to active HDCP22 session.
 *  @param[out] pActiveSession  Pointer to fill active session if it is found.
                                Sets NULL if active session is not found or incase of any failure.
 *  @returns    FLCN_STATUS     FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22RetrieveSession
(
    HDCP22_SESSION *pSession,
    HDCP22_ACTIVE_SESSION **pActiveSession
)
{
    LwU8        i;
    LwBool      bEncActive = LW_FALSE;
    FLCN_STATUS status     = FLCN_OK;

    for (i = 0; i < HDCP22_ACTIVE_SESSIONS_MAX; i++)
    {
        (*pActiveSession) = &activeSessions[i];

        //
        // DPAux, HDMI may share same hybrid physical port thus need to check
        // protocol also.
        //
        if (((*pActiveSession)->priDDCPort == pSession->priDDCPort) &&
            ((*pActiveSession)->sorProtocol == pSession->sorProtocol))
        {
            if ((*pActiveSession)->bActive)
            {
                // Check active activeSession integrity.
                status = hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                                    (LwU32*)*pActiveSession,
                                                                    &gActiveSessionsHashVal[i][0],
                                                                    sizeof(HDCP22_ACTIVE_SESSION),
                                                                    LW_FALSE);
                if (status != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_INTEGRITY_CHECK_FAILURE;
                    goto label_return;
                }

                CHECK_STATUS(hdcp22wiredEncryptionActive_HAL(&Hdcp22wired, (*pActiveSession)->sorNum, &bEncActive));

                if (bEncActive)
                {
                    pSession->sesVariablesRptr.numStreams = (*pActiveSession)->numStreams;
                    pSession->sesVariablesRptr.bEnforceType0Hdcp1xDS = (*pActiveSession)->bEnforceType0Hdcp1xDS;
                    hdcpmemcpy(pSession->sesVariablesRptr.streamIdType, (*pActiveSession)->streamIdType,
                        sizeof(HDCP22_STREAM) * (*pActiveSession)->numStreams);
                    pSession->seqNumV = (*pActiveSession)->seqNumV;
                    pSession->bIsAux = (*pActiveSession)->bIsAux;
                    pSession->sesVariablesIr.bRepeater = (*pActiveSession)->bIsRepeater;
                    pSession->sorProtocol = (*pActiveSession)->sorProtocol;
                    pSession->sorNum = (*pActiveSession)->sorNum;
                    pSession->sesVariablesIr.bTypeChangeRequired = (*pActiveSession)->bTypeChangeRequired;

                    status = hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                                        (LwU32*)&pSession->sesVariablesIr,
                                                                        pSession->integrityHashVal,
                                                                        sizeof(HDCP22_SESSION_VARS_IR),
                                                                        LW_TRUE);
                    if (FLCN_OK != status)
                    {
                        *pActiveSession = NULL;
                    }
                    return status;
                }
            }
        }
    }

label_return:
    *pActiveSession = NULL;
    return status;
}

/*!
 *  @brief Initialize the HDCP22 state with Defaults
 *  @param[in]  pSession     Pointer to active HDCP22 session.
 */
static void
_hdcp22SessionInit
(
    HDCP22_SESSION *pSession
)
{
    LwBool bApplyHwDrmType1LockedWar = pSession->bApplyHwDrmType1LockedWar;

    hdcpmemset(pSession, 0, sizeof(HDCP22_SESSION));
    pSession->bIsStateMachineActive = LW_TRUE;
    pSession->hdcpVersion  = HDCP22_VERSION_22;

    // Don't reset the WAR flag.
    pSession->bApplyHwDrmType1LockedWar = bApplyHwDrmType1LockedWar;
}

/*!
 *  @brief Init session secrets with type1Lock check.
 *  @param[in]  pSession     Pointer to active HDCP22 session.
 */
static FLCN_STATUS
_hdcp22wiredStartSessionWithType1LockCheck
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;

    CHECK_STATUS(hdcp22wiredUpdateType1LockState(pSession));
    CHECK_STATUS(hdcp22wiredStartSession(pSession));

label_return:
    return status;
}

/*!
 *  @brief Restarts the Session by sending AKE_INIT.
 *  @param[in]  pSession     Pointer to active HDCP22 session.
 */
static FLCN_STATUS
_hdcp22SessionStartAuth
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;

    // Reset the counters
    pSession->sesVariablesRptr.seqNumV = 0;
    pSession->sesVariablesRptr.seqNumM = 0;
    pSession->hdcp22TimerCount = 0;
    pSession->seqNumV = 0;
    pSession->bHandleHpdFromRM = LW_FALSE;

    status = _hdcp22CheckHdcp22Version(pSession);
    if (status != FLCN_OK)
    {
        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_NOT_SUPPORTED;
        return status;
    }

    //
    // Init internal session secrets with type1Lock check. The function's caller
    // already supports retry to all failures thus no extra retry here.
    //
    status = _hdcp22wiredStartSessionWithType1LockCheck(pSession);
    if (status != FLCN_OK)
    {
        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_SESSION;
        return status;
    }

    // Send AKE INIT
    status = _hdcp22HandleSessionAkeInit(pSession);

    // Start timer for next stage if succeed to send AKE_INIT.
    if (status == FLCN_OK)
    {
        pSession->stage = HDCP22_AUTH_STAGE_WAIT_FOR_RX_CERT;
        status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_RXCERT(pSession->sorProtocol));
    }
    else
    {
        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_AKE_INIT;

        // Ignore return code not override original error status.
        (void)hdcp22wiredEndSession();
    }

    return status;
}

/*!
 *  @brief Resets the HDCP State and Detaches attached i2c, aux overlays
 *  @param[in]  pSession            Pointer to active HDCP22 session.
 *  @param[in]  bCleanActiveSession Flag to see if wants to clean activeSession.
 *  @returns    void
 */
static void
_hdcp22StateReset
(
    HDCP22_SESSION *pSession,
    LwBool          bCleanActiveSession
)
{
    if (bCleanActiveSession)
    {
        HDCP22_ACTIVE_SESSION *pActiveSession = NULL;
        LwU8 i;

        // Find previous active session.
        for (i = 0; i < HDCP22_ACTIVE_SESSIONS_MAX; i++)
        {
            pActiveSession = &activeSessions[i];
            if (pActiveSession->priDDCPort == pSession->priDDCPort)
            {
                break;
            }
        }

        if ((pActiveSession) &&
            (pActiveSession->priDDCPort == pSession->priDDCPort))
        {
            //
            // Update actionSessions storage's integrity hash for later check
            // without check because caller won't use that to override actual
            // return status.
            //
            (void)hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                            (LwU32*)pActiveSession,
                                                            &gActiveSessionsHashVal[GET_ACTIVE_SESSION_INDEX(pActiveSession)][0],
                                                            sizeof(HDCP22_ACTIVE_SESSION),
                                                            LW_TRUE);
        }
    }

    hdcpmemset(pSession, 0, sizeof(HDCP22_SESSION));
}

/*!
 * @brief Start repeater authentication stage
 * @param[in]  pSession     Pointer to active HDCP22 session.
 * @return     FLCN_STATUS  Returns FLCN_OK if succeed else error code.
 */
static FLCN_STATUS
_hdcp22StartRepeaterAuth
(
    HDCP22_SESSION  *pSession
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       pollRxIdListInterval = HDCP22_TIMER_POLL_INTERVAL_RXID_LIST;

    hdcp22AttachDetachOverlay(HDCP22WIRED_REPEATER, LW_TRUE);

    // CPIRQ of RxIdList READY notification no needs to wait more.
    if (pSession->bReadKsvList)
    {
        pollRxIdListInterval = HDCP22_TIMER_POLL_INTERVAL;
    }

    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_RPTR_STARTED;
    pSession->stage = HDCP22_AUTH_STAGE_WAIT_TO_START_RPTR;
    pSession->hdcp22TimerCount = 0;
    status = _hdcp22wiredStartTimer(pollRxIdListInterval);

    return status;
}

/*!
 * @brief This function checks whether HDCP2.2 enable request has any type change whether
 *        Stream Ids are not expected to change. If they are chnaged it is considered as
 *        improper request and return LW_FALSE
 *
 * @param[in]  pHdcp22Cmd  HDCP2.2 command
 * @param[in]  pSession    Pointer to HDCP22 active session
 * @return     LwBool      return LW_TRUE if stream types are changed
 *                         return LW_FALSE if stream types are not changed
 */
static LwBool
_hdcp22IsStreamTypeChngRequest
(
    RM_FLCN_HDCP22_CMD  *pHdcp22Cmd,
    HDCP22_SESSION      *pSession
)
{
    LwU8 i;
    LwU8 j;
    LwU8 count;

    if (pSession->bIsAux || pSession->sesVariablesIr.bRepeater)
    {
        // Type Change is needed, then we need to renegotiate irrespective saved type value.
        if (pSession->sesVariablesIr.bTypeChangeRequired)
        {
            return LW_TRUE;
        }

        for (i = 0; i < pHdcp22Cmd->cmdHdcp22Enable.numStreams; i++)
        {
            count = 0;
            j = i;

            while (count < pHdcp22Cmd->cmdHdcp22Enable.numStreams)
            {
                if (pSession->sesVariablesRptr.streamIdType[i].streamId ==
                        pHdcp22Cmd->cmdHdcp22Enable.streamIdType[j].streamId)
                {
                    if (pSession->sesVariablesRptr.streamIdType[i].streamType !=
                            pHdcp22Cmd->cmdHdcp22Enable.streamIdType[j].streamType)
                    {
                       return LW_TRUE;
                    }

                    break;
                }

                count++;
                j++;

                if (j == pHdcp22Cmd->cmdHdcp22Enable.numStreams)
                {
                    j = 0;
                }
            }

            if (count == pHdcp22Cmd->cmdHdcp22Enable.numStreams)
            {
                return LW_FALSE;
            }
        }
    }
    else
    {
        if (pSession->sesVariablesRptr.streamIdType[0].streamType !=
                pHdcp22Cmd->cmdHdcp22Enable.streamIdType[0].streamType)
        {
            return LW_TRUE;
        }
    }

    return LW_FALSE;
}

/*!
 * @brief This function Handles Stream type Change request. It reauthenticates HDCP2.2
 *        with modified stream types. If SEC2 type 1 enforcement is enabled, then modified
 *        stream types are saved and applied after SEC2 type 1 enforcement is disabled.
 *
 * @param[in]  pHdcp22Cmd     HDCP2.2 command
 * @param[in] pCmd            FLCN command
 * @param[in]  pSession       Pointer to HDCP22 active session
 * @param[in]  pActiveSession Pointer to HDCP22 saved session
 * @param[in]  pCmd           Pointer to HDCP22 saved session
 * @param[in]  cmdQueueId     cmdQueue id
 * @param[in]  seqNumId       seqNum id
 * @return     FLCN_STATUS    returns FLCN_OK if request is completely handled
 *                            returns FLCN_ERR_MORE_PROCESSING_REQUIRED if request needs to
 *                                    be completed further
 *                            returns ERROR in case of any other error
 */
static FLCN_STATUS
_hdcp22HandleStreamTypeChngReq
(
    RM_FLCN_HDCP22_CMD    *pHdcp22Cmd,
    void                  *pCmd,
    HDCP22_SESSION        *pSession,
    HDCP22_ACTIVE_SESSION *pActiveSession,
    LwU8                   cmdQueueId,
    LwU8                   seqNumId
)
{
    FLCN_STATUS status = FLCN_OK;

    // Save the modified Stream Id Type values in to active session
    if (pActiveSession->sorProtocol == RM_FLCN_HDCP22_SOR_HDMI)
    {
        pActiveSession->streamIdType[0] = pHdcp22Cmd->cmdHdcp22Enable.streamIdType[0];
        pActiveSession->numStreams = 0x1;
    }
    else
    {
        LwU8 i;

        pActiveSession->numStreams = pHdcp22Cmd->cmdHdcp22Enable.numStreams;

        for (i = 0; i < pHdcp22Cmd->cmdHdcp22Enable.numStreams; i++)
        {
            pActiveSession->streamIdType[i] = pHdcp22Cmd->cmdHdcp22Enable.streamIdType[i];
        }
    }

    pActiveSession->bEnforceType0Hdcp1xDS = pHdcp22Cmd->cmdHdcp22Enable.bEnforceType0Hdcp1xDS;

    status = hdcp22wiredUpdateTypeChangeRequired(pHdcp22Cmd, pSession,
                                                 pActiveSession,
                                                 cmdQueueId, seqNumId,
                                                 &gActiveSessionsHashVal[GET_ACTIVE_SESSION_INDEX(pActiveSession)][0]);

   return status;
}

/*!
 * @brief Swaps the Endianess of any given Array
 * @param[in]  pInData Input array to be swapped for endiness
 * @param[out] pOutData output array after swapping,
 * @param[in]  size  array size.
 */
static inline void
_swapEndianness
(
    void         *pOutData,
    const void   *pInData,
    const LwU32   size
)
{
    LwU32 i;

    for (i = 0; i < size / 2; i++)
    {
        LwU8 b1 = ((LwU8*)pInData)[i];
        ((LwU8*)pOutData)[i] = ((LwU8*)pInData)[size - 1 - i];
        ((LwU8*)pOutData)[size - 1 - i] = b1;
    }
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Swaps the Endianess of any given Array
 * @param[in]  pInData Input array to be swapped for endiness
 * @param[out] pOutData output array after swapping,
 * @param[in]  size  array size.
 */
void
swapEndianness
(
    void         *pOutData,
    const void   *pInData,
    const LwU32   size
)
{
    _swapEndianness(pOutData, pInData, size);
}

#if defined(GSPLITE_RTOS)
void
swapEndiannessHs
(
    void         *pOutData,
    const void   *pInData,
    const LwU32   size
)
{
    _swapEndianness(pOutData, pInData, size);
}
#endif

/*!
 * @brief This function attaches or detaches aux/i2c ovelrays based on sor protocol.
 * @param[in]  sorProtocol Sor protocol DP/HDMI
 * @param[out] bAttach if LW_TRUE then specified overlay attached
 *                     if LW_FALSE then specified overlay detached.
 */
void
hdcp22ConfigAuxI2cOverlays
(
    RM_FLCN_HDCP22_SOR_PROTOCOL sorProtocol,
    LwBool                      bAttach
)
{
    switch(sorProtocol)
    {
        case RM_FLCN_HDCP22_SOR_DP:
        case RM_FLCN_HDCP22_SOR_DUAL_DP:
            if(bAttach)
            {
                hdcpAttachAndLoadOverlay(HDCP22WIRED_DPAUX);
                gBIsAuxI2cLoaded = LW_TRUE;
            }
            else
            {
                hdcpDetachOverlay(HDCP22WIRED_DPAUX);
                gBIsAuxI2cLoaded = LW_FALSE;
            }
            break;

        case RM_FLCN_HDCP22_SOR_HDMI:
            if(bAttach)
            {
                hdcpAttachAndLoadOverlay(HDCP22WIRED_I2C);
                gBIsAuxI2cLoaded = LW_TRUE;
            }
            else
            {
                hdcpDetachOverlay(HDCP22WIRED_I2C);
                gBIsAuxI2cLoaded = LW_FALSE;
            }
            break;

        default:
           {
                //
                // Error handling is already done for this parameter.
                // So, it should not reach here in any case.
                // Added default to resolve coverity issue
                //
                lwuc_halt();
           }
           break;
   }
}

/*!
 *  @brief This function tells if repeater is ready for downstream receiver
 *         IDs per Rx status
 *  @param[in]  pSession     Pointer to active HDCP22 session
 *  @param[in]  rxStatus     Received RxSatus register value
 *  @returns    LwBool       LW_TRUE on ready
 */
static LwBool
_hdcp22IsRepeaterReady
(
    HDCP22_SESSION  *pSession,
    LwU16            rxStatus
)
{
    if (pSession->bIsAux)
    {
        if ((FLD_TEST_DRF(_DPCD, _HDCP22_RX_STATUS, _READY, _YES, rxStatus)) ||
            (pSession->bReadyNotified))
        {
            pSession->bReadyNotified = LW_FALSE;
            return LW_TRUE;
        }
    }
    else
    {
        if (DRF_VAL(_RCV_HDCP22_RX_STATUS, _MESSAGE, _SIZE, rxStatus) >=
                HDCP22_SIZE_MSG_RPT_RX_IDS_MIN)
        {
            return LW_TRUE;
        }
    }

    return LW_FALSE;
}

/*!
 * @brief This function gets triggered with timer events,  Timer events or callbacks
 *        will occur at programmed timer intervals,  HDCP22 authentication state mechine
 *        uses these timer events to handle the transition from one state to other state.
 *
 * @param[in] pSession      Pointer to HDCP22 active session.
 * @param[in] timerEvtType  Timer event type ( legal or illegal ).
 */
FLCN_STATUS
hdcp22wiredHandleTimerEvent
(
    HDCP22_SESSION         *pSession,
    HDCP22_TIMER_EVT_TYPE   timerEvtType
)
{
    FLCN_STATUS         status      = FLCN_OK;
    LwU16               rxStatus    = 0;
    // Stack variable alignment declaration is not guaranteed, use aligned pointer alternatively.
    LwU8                certBuf[sizeof(HDCP22_CERTIFICATE) + HDCP22_U32_ALIGN_INC_SIZE];
    HDCP22_CERTIFICATE *pRecvCert   = (HDCP22_CERTIFICATE *)LW_ALIGN_UP((LwUPtr)certBuf, sizeof(LwU32));
    LwBool              bEncActive  = LW_FALSE;

    if (HDCP22_TIMER_ILLEGAL_EVT == timerEvtType)
    {
        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_ILLEGAL_TIMEREVENT;
        status = FLCN_ERR_TIMEOUT;
        goto label_return;
    }

    // Return errors only as re-authentication is required.
    status = _hdcp22GetRxStatusForReAuth(pSession, &rxStatus);
    if (FLCN_OK != status)
    {
        //
        // For DP reAuth CP_IRQ, move to HDCP22_AUTH_STAGE_WAIT_FOR_REAUTH
        // stage and see if L-HPD coming to abort reAuthentication.
        //
        if (FLCN_ERR_HDCP22_DELAY_ABORT_AUTHENTICATION == status)
        {
            status = FLCN_OK;
        }
        goto label_return;
    }

    // Check sanity of state variables that integrity required.
    status = hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                        (LwU32*)&pSession->sesVariablesIr,
                                                        pSession->integrityHashVal,
                                                        sizeof(HDCP22_SESSION_VARS_IR),
                                                        LW_FALSE);
    if (FLCN_OK != status)
    {
        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_INTEGRITY_CHECK_FAILURE;
        goto label_return;
    }

    switch (pSession->stage)
    {
        case HDCP22_AUTH_STAGE_WAIT_FOR_RX_CERT:
        {
            if (!_hdcp22IsCertRxReady(pSession, rxStatus))
            {
                pSession->hdcp22TimerCount++;
                if(pSession->hdcp22TimerCount >= HDCP22_TIMEOUT_COUNT_RXCERT(pSession->sorProtocol))
                {
                    status = FLCN_ERR_TIMEOUT;
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_TIMEOUT_CERT_RX;
                }
                else
                {
                    status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_RXCERT(pSession->sorProtocol));
                    if (status != FLCN_OK)
                    {
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                    }
                }
                break;
            }

            hdcp22AttachDetachOverlay(HDCP22WIRED_CERTRX_SRM, LW_TRUE);
            status = hdcp22HandleVerifyCertRx(pSession, pRecvCert);
            hdcp22AttachDetachOverlay(HDCP22WIRED_CERTRX_SRM, LW_FALSE);

            if (status != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_CERT_RX;
                break;
            }

            pSession->hdcp22TimerCount = 0;
            if (pSession->bIsAux)
            {
                pSession->stage = HDCP22_AUTH_STAGE_WAIT_TO_MASTER_KEY_EXCHANGE;
                pSession->hdcp22TimerCount = 0;
                status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_MASTER_KEY);
                if (status != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                }

                break;
            }
            else
            {
                // Fall through without delay for HDMI case.
            }
        }

        case HDCP22_AUTH_STAGE_WAIT_TO_MASTER_KEY_EXCHANGE:
        {
            //
            // Verify CertRx takes too much time and possible doesn't detect L-HPD.
            // We rely on RM to forward bHpdFromRm cmd to abort authenticaton for
            // the situation.
            //
            pSession->hdcp22TimerCount++;
            if(pSession->hdcp22TimerCount < HDCP22_TIMEOUT_COUNT_MASTER_KEY(pSession->sorProtocol))
            {
                status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_MASTER_KEY);
                if (status != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                }

                break;
            }

            hdcp22AttachDetachOverlay(HDCP22WIRED_AKE, LW_TRUE);
            status = hdcp22HandleMasterKeyExchange(pSession, &pRecvCert->modulus[0]);
            hdcp22AttachDetachOverlay(HDCP22WIRED_AKE, LW_FALSE);

            if (status != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_MASTER_KEY_EXCHANGE;
                break;
            }

            pSession->stage = HDCP22_AUTH_STAGE_WAIT_FOR_H_PRIME;
            pSession->hdcp22TimerCount = 0;
            status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_HPRIME);
            if (status != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
            }

            break;
        }

        case HDCP22_AUTH_STAGE_WAIT_FOR_H_PRIME:
        {
            if (!_hdcp22IsHprimeReady(pSession, rxStatus))
            {
                pSession->hdcp22TimerCount++;
                if(pSession->hdcp22TimerCount >= HDCP22_TIMEOUT_COUNT_HPRIME)
                {
                    status = FLCN_ERR_TIMEOUT;
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_TIMEOUT_H_PRIME;
                }
                else
                {
                    status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_HPRIME);
                    if (status != FLCN_OK)
                    {
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                    }
                }
                break;
            }

            hdcp22AttachDetachOverlay(HDCP22WIRED_AKEH, LW_TRUE);
            status = hdcp22HandleVerifyH(pSession);
            hdcp22AttachDetachOverlay(HDCP22WIRED_AKEH, LW_FALSE);

            if (status != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_TIMEOUT_H_PRIME;
                break;
            }

            hdcp22AttachDetachOverlay(HDCP22WIRED_LC, LW_TRUE);
            pSession->stage = HDCP22_AUTH_STAGE_WAIT_FOR_PAIRING;
            pSession->hdcp22TimerCount = 0;
            status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_PAIRING);
            if (status != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
            }

            break;
        }
        case HDCP22_AUTH_STAGE_WAIT_FOR_PAIRING:
        {
            if (!_hdcp22IsPairingAvailable(pSession, rxStatus))
            {
                pSession->hdcp22TimerCount++;
                if(pSession->hdcp22TimerCount >= HDCP22_TIMEOUT_COUNT_PAIRING)
                {
                    status = FLCN_ERR_TIMEOUT;
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_TIMEOUT_PAIRING;
                    hdcp22AttachDetachOverlay(HDCP22WIRED_LC, LW_FALSE);
                }
                else
                {
                    status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_PAIRING);
                    if (status != FLCN_OK)
                    {
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                    }
                }
                break;
            }

            status = hdcp22HandlePairing(pSession);
            if (status != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_PAIRING;
                hdcp22AttachDetachOverlay(HDCP22WIRED_LC, LW_FALSE);
                break;
            }

            pSession->stage = HDCP22_AUTH_STAGE_WAIT_FOR_LC_INIT;
            pSession->hdcp22TimerCount = 0;
            pSession->sesVariablesIr.hdcp22LcRetryCount = 0;
            status = hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                                (LwU32*)&pSession->sesVariablesIr,
                                                                pSession->integrityHashVal,
                                                                sizeof(HDCP22_SESSION_VARS_IR),
                                                                LW_TRUE);
            if (FLCN_OK != status)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_INTEGRITY_UPDATE_FAILURE;
                hdcp22AttachDetachOverlay(HDCP22WIRED_LC, LW_FALSE);
                break;
            }
        }
        // Deliberately removed break so that it can go to  next stage

        case HDCP22_AUTH_STAGE_WAIT_FOR_LC_INIT:
        {
            status =  hdcp22HandleLcInit(pSession);
            if (status == FLCN_OK)
            {
                pSession->stage = HDCP22_AUTH_STAGE_WAIT_FOR_LPRIME;
                pSession->hdcp22TimerCount = 0;
                status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_LPRIME(pSession->sorProtocol));
                if (status != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                }
                break;
            }
            else
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_LC_INIT;

                // Fall through to next stage with retry error handling.
            }
        }

        case HDCP22_AUTH_STAGE_WAIT_FOR_LPRIME:
        {
            if (status == FLCN_OK && !_hdcp22IsLprimeReady(pSession, rxStatus))
            {
                pSession->hdcp22TimerCount++;
                if(pSession->hdcp22TimerCount >= HDCP22_TIMEOUT_COUNT_LPRIME(pSession->sorProtocol))
                {
                    status = FLCN_ERR_TIMEOUT;
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_TIMEOUT_L_PRIME;
                    // Don't break Since LC needs to be re tried
                }
                else
                {
                    status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_LPRIME(pSession->sorProtocol));
                    if (status != FLCN_OK)
                    {
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                    }
                    break;
                }
            }

            if (status == FLCN_OK)
            {
                status =  hdcp22HandleVerifyL(pSession);
                if (status != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_L_PRIME;
                }
            }

            if (status != FLCN_OK)
            {
                if(pSession->sesVariablesIr.hdcp22LcRetryCount < HDCP22_LC_RETRIES)
                {
                    // Retry for LC init
                    pSession->sesVariablesIr.hdcp22LcRetryCount++;
                    status = hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                                   (LwU32*)&pSession->sesVariablesIr,
                                                                   pSession->integrityHashVal,
                                                                   sizeof(HDCP22_SESSION_VARS_IR),
                                                                   LW_TRUE);
                    if (status == FLCN_OK)
                    {
                        pSession->stage = HDCP22_AUTH_STAGE_WAIT_FOR_LC_INIT;
                        pSession->hdcp22TimerCount = 0;
#ifdef HDCP22_LC_RETRY_WAR_ENABLED
                        if (pSession->sesVariablesIr.hdcp22LcRetryCount == 1)
                        {
                            //
                            // Bug200635910: GA10X found Simplay 1A05 failed because source sends retry
                            // LC_INIT but TE expects AKE_INIT after asserting HPD.
                            // Turing pass because retry LC_INIT hit i2c LW_PMGR_I2C_CNTL_STATUS_NO_ACK
                            // error while Ampere succeed to send that.
                            // WAR to increase retry interval long enough that allows HPD comes to abort
                            // active authentication.
                            // There's no spec to define retry LC_INIT interval and we even don't support
                            // LC_INIT retry before bug 200301993 Simplay 1A13-T1 intermittent failure.
                            //
                            status = _hdcp22wiredStartTimer(HDCP22_TIMER_INTERVAL_LC_RETRY_WAR(pSession->sorProtocol));
                        }
                        else
                        {
                            status = _hdcp22wiredStartTimer(HDCP22_TIMER_STAGE_DELAY);
                        }
#else
                        status = _hdcp22wiredStartTimer(HDCP22_TIMER_STAGE_DELAY);
#endif
                        if (status != FLCN_OK)
                        {
                            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                        }
                    }
                    else
                    {
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_INTEGRITY_CHECK_FAILURE;
                    }
                }
                else
                {
                    hdcp22AttachDetachOverlay(HDCP22WIRED_LC, LW_FALSE);
                }
                break;
            }

            hdcp22AttachDetachOverlay(HDCP22WIRED_LC, LW_FALSE);

            pSession->stage = HDCP22_AUTH_STAGE_WAIT_FOR_SKE;
            status = _hdcp22wiredStartTimer(HDCP22_TIMER_STAGE_DELAY);
            if (status != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
            }

            break;
        }

        case HDCP22_AUTH_STAGE_WAIT_FOR_SKE:
        {
            hdcp22AttachDetachOverlay(HDCP22WIRED_SKE, LW_TRUE);
            status = hdcp22HandleGenerateSkeInit(pSession);
            hdcp22AttachDetachOverlay(HDCP22WIRED_SKE, LW_FALSE);

            if (status != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_SKE_INIT;
                break;
            }

            if (pSession->sesVariablesIr.bRepeater)
            {
                // If the Downstream device is a repeater, start Repeater authentication.
                status = _hdcp22StartRepeaterAuth(pSession);
                if (status != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_SKE_INIT;
                }
                break;
            }
            else
            {
                if (pSession->bIsAux)
                {
                    //
                    // DP non-repeater device needs to have streamType information.
                    // Errata to HDCP on DisplayPort Spec 2.2 July 3, 2014.
                    //
                    status = _hdcp22DpWriteStreamType(pSession);
                    if (status != FLCN_OK)
                    {
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_SET_STREAM_TYPE;
                        break;
                    }
                }

                pSession->stage = HDCP22_AUTH_STAGE_WAIT_TO_ENABLE_ENC;

                if (!pSession->bApplyHwDrmType1LockedWar)
                {
                    status = _hdcp22wiredStartTimer(HDCP22_TIMEROUT_ENC_ENABLE);
                    if (status != FLCN_OK)
                    {
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                    }
                    break;
                }

                //
                // HWDRM type1locked WAR continue to fall through and enable
                // encryption immediately or sink cipher got updated eKs to
                // trigger reAuth again.
                //
            }
        }

        case HDCP22_AUTH_STAGE_WAIT_TO_ENABLE_ENC:
        {
            LwBool bSor8LaneDp = LW_FALSE;

            status = hdcp22wiredControlEncryption(ENC_ENABLE,
                                                  pSession->sorNum,
                                                  pSession->linkIndex,
                                                  pSession->bApplyHwDrmType1LockedWar,
                                                  LW_FALSE,
                                                  0,
                                                  pSession->bIsAux);
            if (status != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_EN_ENC;
                break;
            }

            // Authentication is Successful
            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ENC_ENABLED;

            status = hdcp22wiredIsSorDrivesEightLaneDp_HAL(&Hdcp22wired, pSession->sorNum, &bSor8LaneDp);
            if (status != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
                break;
            }

            // Check if the Secondary Link is present
            if (bSor8LaneDp && (pSession->linkIndex == HDCP22_LINK_PRIMARY))
            {
                pSession->linkIndex = HDCP22_LINK_SECONDARY;
                pSession->priDDCPort = pSession->secDDCPort;

                // Start Auth on Secondary Link
                status = _hdcp22SessionStartAuth(pSession);

                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_INIT_SECONDARY_LINK;
            }
            break;
        }

        // Repeater Authentication stages
        case HDCP22_AUTH_STAGE_WAIT_TO_START_RPTR:
        {
            LwU8 vBuffer[HDCP22_KSV_LIST_SIZE_MAX + HDCP22_SIZE_RX_INFO + HDCP22_SIZE_SEQ_NUM_V];

            if (!_hdcp22IsRepeaterReady(pSession, rxStatus))
            {
                pSession->hdcp22TimerCount++;
                if(pSession->hdcp22TimerCount >= HDCP22_TIMEOUT_COUNT_RXID_LIST)
                {
                    status = FLCN_ERR_TIMEOUT;
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_TIMEOUT_RXID_LIST;
                    hdcp22AttachDetachOverlay(HDCP22WIRED_REPEATER, LW_FALSE);
                }
                else
                {
                    status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_RXID_LIST);
                    if (status != FLCN_OK)
                    {
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                    }
                }
                break;
            }
            // Fall through to next stage that read RxIdList and verify.

        case HDCP22_AUTH_STAGE_WAIT_TO_READ_RXID:
            // Read KsvList, RxInfo, seqNumV from sink which are not confidential to protect.
            status = hdcp22HandleReadReceiverIdList(pSession, vBuffer, sizeof(vBuffer));
            if (status == FLCN_OK)
            {
                //
                // Unload the repeater code while doing SRM check of RXIDS as
                // code size was exceeding with both repeater and SRM overlays
                // loaded at same time resulting in DPU halt.
                //
                hdcpDetachOverlay(HDCP22WIRED_REPEATER);
                status = _hdcp22SrmCheckRxIds(pSession,
                                              vBuffer,
                                              pSession->sesVariablesRptr.numDevices);
                hdcpAttachAndLoadOverlay(HDCP22WIRED_REPEATER);
            }

            if (status != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_RPTR_INIT;
                hdcp22AttachDetachOverlay(HDCP22WIRED_REPEATER, LW_FALSE);

                // Ignore return status to keep original failure.
                (void)_hdcp22wiredStopTimer();
                break;
            }

            if (!pSession->bReadKsvList)
            {
                //
                // Start timer to send RXID ACK only after some delay.
                // This is needed to handle compliace HPD handle issue. BUG 1769263
                //
                pSession->stage = HDCP22_AUTH_STAGE_WAIT_TO_RXID_ACK;
                status = _hdcp22wiredStartTimer(HDCP22_TIMER_RXID_ACK_DELAY);
                if (status != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                }

                hdcp22AttachDetachOverlay(HDCP22WIRED_REPEATER, LW_FALSE);
                break;
            }
        }
        // Deliberately avoided break as next stage is HDCP22_AUTH_STAGE_WAIT_TO_RXID_ACK

        case HDCP22_AUTH_STAGE_WAIT_TO_RXID_ACK:
        {
            if (!pSession->bReadKsvList)
            {
                hdcp22AttachDetachOverlay(HDCP22WIRED_REPEATER, LW_TRUE);
            }
            status = hdcp22WriteReceiverIdAck(pSession);
            if (status != FLCN_OK)
            {
                hdcp22AttachDetachOverlay(HDCP22WIRED_REPEATER, LW_FALSE);
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_RPTR_INIT;
                break;
            }

            //
            // HDMI Tx, A8:A5 is allowed and we can skip A9 if RepeaterAuth_Stream_Manage
            // was previously transmitted for current session.
            // 1B-09 compliance test saw intermittent failure with A9 substate, and skip A9
            // for HDMI.
            //
            // DisplayPort Tx, A8:A5 is not allowed and we are expected to resend
            // RepeaterAuth_Stream_Manage for each CP_IRQ 'Ready' notification.
            //
            if ((pSession->bReadKsvList) &&
                (pSession->sorProtocol == RM_FLCN_HDCP22_SOR_HDMI))
            {
                // Done with the Repeater RxIDs read and ACK.
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_RPTR_DONE;
                hdcp22AttachDetachOverlay(HDCP22WIRED_REPEATER, LW_FALSE);
                break;
            }

            pSession->stage = HDCP22_AUTH_STAGE_WAIT_FOR_STREAM_MANAGE;
            pSession->hdcp22MRetryCount = 0;
        }
        // Deliberately avoided break as next stage is WAIT_FOR_STREAM_MANAGE

        case HDCP22_AUTH_STAGE_WAIT_FOR_STREAM_MANAGE:
        {
            // When M' verify failed, should transit to A5 that can detect READY notification
            if (pSession->hdcp22MRetryCount &&
                _hdcp22IsRepeaterReady(pSession, rxStatus))
            {
                pSession->stage = HDCP22_AUTH_STAGE_WAIT_TO_READ_RXID;
                status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL);
                if (status != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                }
            }
            else
            {
                status = hdcp22WriteStreamManageMsg(pSession);
                if (status != FLCN_OK)
                {
                    hdcp22AttachDetachOverlay(HDCP22WIRED_REPEATER, LW_FALSE);
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_RPTR_STREAM_MNT;
                    break;
                }

                pSession->stage = HDCP22_AUTH_STAGE_WAIT_FOR_MPRIME;
                pSession->hdcp22TimerCount = 0;
                status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_MPRIME(pSession->sorProtocol));
                if (status != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                }
            }
            break;
        }

        case HDCP22_AUTH_STAGE_WAIT_FOR_MPRIME:
        {
            if (!_hdcp22IsMprimeReady(pSession, rxStatus))
            {
                pSession->hdcp22TimerCount++;
                if(pSession->hdcp22TimerCount >= HDCP22_TIMEOUT_COUNT_MPRIME(pSession->sorProtocol))
                {
                    status = FLCN_ERR_TIMEOUT;
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_TIMEOUT_MPRIME;
                    // Don't break since MPrime needs to be re tried
                }
                else
                {
                    status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_MPRIME(pSession->sorProtocol));
                    if (status != FLCN_OK)
                    {
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                    }
                    break;
                }
            }

            if (status == FLCN_OK)
            {
                status = hdcp22HandleVerifyM(pSession);
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_RPTR_MPRIME;
            }

            if (status != FLCN_OK)
            {
                if(pSession->hdcp22MRetryCount < HDCP22_M_RETRIES)
                {
                    // M' verification failed, retry to send again. Modify status as OK to continue.
                    status = FLCN_OK;
                    pSession->hdcp22MRetryCount++;
                    pSession->stage = HDCP22_AUTH_STAGE_WAIT_FOR_STREAM_MANAGE;
                    status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL);
                    if (status != FLCN_OK)
                    {
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                    }
                    break;
                }
                else
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_RPTR_MPRIME;
                    status = FLCN_ERR_TIMEOUT;
                    // Fall through to end of the case then detach overlay.
                }
            }
            else
            {
                // To repeater case, enable encryption control after streamType forwarded.
                status = hdcp22wiredControlEncryption(ENC_ENABLE,
                                                      pSession->sorNum,
                                                      pSession->linkIndex,
                                                      pSession->bApplyHwDrmType1LockedWar,
                                                      LW_FALSE,
                                                      0,
                                                      pSession->bIsAux);
                if (status == FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_RPTR_DONE;
                }
                else
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_EN_ENC;
                }

                // Fall through to end of the case then detach overlay.
            }

            hdcp22AttachDetachOverlay(HDCP22WIRED_REPEATER, LW_FALSE);
            break;
        }

        case HDCP22_AUTH_STAGE_WAIT_FOR_REAUTH:
        {
            //
            // DP sink throw reauth CP_IRQ before L-HPD, and wait a while
            // to confirm no L-HPD before reAuthentication else abort.
            //
            pSession->hdcp22TimerCount++;
            if(pSession->hdcp22TimerCount >= HDCP22_TIMEOUT_COUNT_HPD)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_REAUTH_REQ;
                status = FLCN_ERR_HDCP22_ABORT_AUTHENTICATION;
            }
            else
            {
                status = _hdcp22wiredStartTimer(HDCP22_TIMER_POLL_INTERVAL_HPD);
                if (status != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER;
                }
            }
            break;
        }

        case HDCP22_AUTH_STAGE_NULL:
        {
            // Error stage.
            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_NULL;
            status = FLCN_ERROR;
            break;
        }
    }

label_return:

    if (((status == FLCN_OK) && (RM_FLCN_HDCP22_STATUS_ENC_ENABLED == pSession->msgStatus)
            && (pSession->bIsStateMachineActive) && !pSession->sesVariablesIr.bRepeater) ||
        ((status == FLCN_OK) && (RM_FLCN_HDCP22_STATUS_RPTR_DONE == pSession->msgStatus)
            && (pSession->bIsStateMachineActive)) ||
        (status != FLCN_OK))
    {
        // Both authenticated or authentication failed cases can reset internal secrets.
        (void)hdcp22wiredEndSession();

        // If Repeater validation or SRM ksv validation fails,
        // disable encryption and return error status.
        if ((status != FLCN_OK))
        {
            if (gLastAttachedOverlay < HDCP_OVERLAYID_MAX)
            {
                hdcp22AttachDetachOverlay(gLastAttachedOverlay, LW_FALSE);
            }

            if ((hdcp22wiredEncryptionActive_HAL(&Hdcp22wired, pSession->sorNum, &bEncActive)) != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
                goto label_return2;
            }

            // HWDRM WAR doesn't allow to disable encryption even fail at reAuthentication.
            if (bEncActive && !pSession->bApplyHwDrmType1LockedWar)
            {
                if ((hdcp22wiredControlEncryption(ENC_DISABLE,
                                                  pSession->sorNum,
                                                  pSession->linkIndex,
                                                  pSession->bApplyHwDrmType1LockedWar,
                                                  LW_TRUE,
                                                  0,
                                                  pSession->bIsAux)) != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
                    goto label_return2;
                }
            }
        }

        if ((!pSession->bReadKsvList))
        {
            if ((hdcp22wiredEncryptionActive_HAL(&Hdcp22wired, pSession->sorNum, &bEncActive)) != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
                goto label_return2;
            }

            if (bEncActive)
            {
                if ((_hdcp22SaveSession(pSession)) != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
                    goto label_return2;
                }
            }
        }

        if (status != FLCN_OK)
        {
            if (pSession->bApplyHwDrmType1LockedWar)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_HWDRM_WAR_AUTH_FAILURE;
            }
            else
            {
                //
                // If detect unplug, abort authentication and response to RM
                // immediately.
                //
                if ((status != FLCN_ERR_HPD_UNPLUG) &&
#ifdef HDCP22_WAR_3051763_ENABLED
                    //
                    // Bug 3051763: QD980 1A06 setStreamType, rxStatus probe got transaction error when
                    // TE deasserting L-HPD instead of HPD_STATUS_UNPLUG. DP cannot retry authentication
                    // before link trainning completed thus return to dplib without retry here.
                    // If retry in uproc, TE won't notify H' available to timeout and test shows noStoredKm
                    // failure result.
                    //
                    !((pSession->sorProtocol != RM_FLCN_HDCP22_SOR_HDMI) &&
                      (status == FLCN_ERR_AUX_ERROR) &&
                        // TODO: remove below special case that aux error case not retrying in uproc as common case.
                        ((pSession->msgStatus == RM_FLCN_HDCP22_STATUS_ERROR_SET_STREAM_TYPE) ||
                         (pSession->stage == HDCP22_AUTH_STAGE_WAIT_TO_ENABLE_ENC))) &&
#endif
                    (pSession->sesVariablesIr.hdcp22authRetryCount < HDCP22_AKE_INIT_RETRY_MAX))
                {
                    pSession->sesVariablesIr.hdcp22authRetryCount++;

                    if (hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                                   (LwU32*)&pSession->sesVariablesIr,
                                                                   pSession->integrityHashVal,
                                                                   sizeof(HDCP22_SESSION_VARS_IR),
                                                                   LW_TRUE) == FLCN_OK)
                    {
                        //
                        // If failed during authentication to retart, clear pending reAuth notification.
                        // Other cases fall through to cleared at sessionReset.
                        //
                        pSession->bReAuthNotified = LW_FALSE;

                        if (_hdcp22SessionStartAuth(pSession) == FLCN_OK)
                        {
                            // Continue to next authentication stage and timeout handling.
                            return FLCN_OK;
                        }
                    }

                    // If error, fall through to clean up and reponse to RM.
                }
            }
        }

        // No matter authentication succeed or not, need to clear the special WAR flag.
        pSession->bApplyHwDrmType1LockedWar = LW_FALSE;

label_return2:

        hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_FALSE);

        if (!pSession->sesVariablesIr.bSec2ReAuthTypeEnforceReq)
        {
            hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, ENABLE_HDCP22),
                                     pSession->cmdQueueId, pSession->cmdSeqNumId);
        }

        //
        // We need to keep activeSession for authenticated case and can clean
        // up at error cases.
        //
        if (status != FLCN_OK)
        {
            _hdcp22StateReset(pSession, LW_TRUE);
        }
        else
        {
            _hdcp22StateReset(pSession, LW_FALSE);
        }

        if (gBBackGrndReAuthIsInProgress)
        {
            LwBool bRegisterError = LW_FALSE;
            status = hdcp22ProcessBackgroundReAuth();
            if (status != FLCN_OK)
            {
                    bRegisterError = LW_TRUE;
            }

            hdcp22wiredBackgroundReAuthResponse(pSession,
                                                gBBackGrndReAuthIsInProgress,
                                                bRegisterError);
        }
    }

    return status;
}

#ifdef __COVERITY__
void
TaintGlobal
(
    RM_FLCN_HDCP22_CMD *pHdcp22wired
)
{
    __coverity_tainted_data_argument__(pHdcp22wired);
}

void
SanitizeGlobal
(
    RM_FLCN_HDCP22_CMD *pHdcp22wired
)
{
    __coverity_tainted_data_sanitize__(pHdcp22wired);
}
#endif

/*!
 *  @brief Handles the Commands from RM
 *  @param[in]  pSession     Pointer to HDCP22 active session.
 *  @param[in]  pCmd         Pointer to RM FLCN command.
 *  @param[in]  seqNumId     seqNum Id, needed to send the response
 *  @param[in]  cmdQueueId   Command Queue identifier
 */
FLCN_STATUS
hdcp22wiredProcessCmd
(
    HDCP22_SESSION     *pSession,
    RM_FLCN_CMD        *pCmd,
    LwU8                seqNumId,
    LwU8                cmdQueueId
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        cmdType;
    LwU32       i;
    LwU8        maxNoOfSors = 0;
    LwU8        maxNoOfHeads = 0;

    RM_FLCN_HDCP22_CMD *pHdcp22wired = (RM_FLCN_HDCP22_CMD *)(&pCmd->cmdGen.cmd);

    // Over write  DMA index comming from RM
    hdcp22wiredSetDmaIndex(pHdcp22wired);

    cmdType = pHdcp22wired->cmdType;

#ifdef __COVERITY__
    TaintGlobal (pHdcp22wired);
#endif

    status = hdcp22wiredGetMaxNoOfSorsHeads_HAL(&Hdcp22wired, &maxNoOfSors, &maxNoOfHeads);
    if (status != FLCN_OK)
    {
        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
        hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, ENABLE_HDCP22),
                                cmdQueueId, seqNumId);
        return status;
    }

    pSession->maxNoOfSors = maxNoOfSors;
    pSession->maxNoOfHeads = maxNoOfHeads;

    switch (cmdType)
    {
        case RM_FLCN_CMD_TYPE(HDCP22, ENABLE_HDCP22):
        {
            LwBool bRxIDMsgPending = LW_FALSE;
            LwBool bRxRestartRequest = LW_FALSE;
            LwBool bStopProcessingCmd = LW_FALSE;
            HDCP22_ACTIVE_SESSION *pActiveSession = NULL;
            LwBool bEncActive = LW_FALSE;

            if (gBInitError)
            {
                status = FLCN_ERROR;
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
                hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, ENABLE_HDCP22),
                                         cmdQueueId, seqNumId);
                return status;
            }

            if ( (pHdcp22wired->cmdHdcp22Enable.sorNum >= maxNoOfSors)                      ||
                 // TODO: Validate this differently
                 (pHdcp22wired->cmdHdcp22Enable.sorProtocol >= RM_FLCN_HDCP22_MAX_PROTOCOL) ||
#ifdef HDCP22_SUPPORT_MST
                 (pHdcp22wired->cmdHdcp22Enable.numStreams > HDCP22_NUM_STREAMS_MAX)        ||
#else
                 (pHdcp22wired->cmdHdcp22Enable.numStreams != 1)                            ||  // HDMI, SST only supports single stream.
#endif
                 (pHdcp22wired->cmdHdcp22Enable.srmListSize > SRM_LIST_MAX_SIZE)            ||
                 (_hdcp22CheckDmaIdx(&pHdcp22wired->cmdHdcp22Enable.srm) == LW_FALSE) )
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                pSession->msgStatus =  RM_FLCN_HDCP22_STATUS_ILWALID_ARGUMENT;
                hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, ENABLE_HDCP22),
                                         cmdQueueId, seqNumId);
                return status;
            }

            if (pSession->bIsStateMachineActive)
            {
                //
                // If state machine is active, we should not process the request
                // further. Terminate the request with proper notification to active
                // authentication then send the response back.
                //
                // TODO: add sor/link check and only abort same sink's active authentication.
                //
                bStopProcessingCmd = LW_TRUE;
            }
            else if (pHdcp22wired->cmdHdcp22Enable.bHpdFromRM)
            {
                //
                // If sink raise HPD, according to CTS, source should abort
                // active authentication then trigger AKE_INI again. Bus
                // HPD interrupt handler enqueue cmd with hpd + reAuth and can
                // be processed below to check encryption state.
                //
                // DP would trigger reauthentication from dplib after modeset
                // thus simply abort active authentication here.
                //
                if ((pSession->sorProtocol == RM_FLCN_HDCP22_SOR_HDMI) &&
                    (pHdcp22wired->cmdHdcp22Enable.bRxRestartRequest))
                {
                    bStopProcessingCmd = LW_FALSE;
                }
                else
                {
                    bStopProcessingCmd = LW_TRUE;
                }
            }

#ifdef __COVERITY__
    SanitizeGlobal (pHdcp22wired);
#endif

            if (pHdcp22wired->cmdHdcp22Enable.bHpdFromRM && pSession->bIsStateMachineActive)
            {
                //
                // Bug # 1769263: If the HPD comes from Sink, Authentication should be stopped at once.
                // HDMI abort authentication after master key exchange is specifically for compliance,
                // while DP bug#1847394 should abort any authentication immediately.
                //
                // TODO: add sor/link check and only abort same sink's active authentication.
                //
                if (pSession->bIsAux || pSession->stage > HDCP22_AUTH_STAGE_WAIT_FOR_H_PRIME)
                {
                    pSession->bHandleHpdFromRM = LW_TRUE;
                }
                else
                {
                    pSession->bHandleHpdFromRM = LW_FALSE;
                }
            }

            bRxIDMsgPending = pHdcp22wired->cmdHdcp22Enable.bRxIDMsgPending;
            bRxRestartRequest = pHdcp22wired->cmdHdcp22Enable.bRxRestartRequest;

            if (!bStopProcessingCmd)
            {
#ifdef GSPLITE_RTOS
    #ifdef HDCP22_FORCE_TYPE1_ONLY
                // Force to change input type as type1 directly.
                hdcp22wiredForceTypeOne(&pHdcp22wired->cmdHdcp22Enable);
    #endif // HDCP22_FORCE_TYPE1_ONLY

                status = hdcp22wiredIsType1LockActive(&gBType1LockState);
                if (status != FLCN_OK)
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
                    hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, ENABLE_HDCP22),
                         cmdQueueId, seqNumId);
                    return status;
                }
#endif

                hdcp22AttachDetachOverlay(HDCP22WIRED_TIMER, LW_TRUE);
                hdcp22ConfigAuxI2cOverlays((RM_FLCN_HDCP22_SOR_PROTOCOL)pHdcp22wired->cmdHdcp22Enable.sorProtocol,
                             LW_TRUE);

                if (pHdcp22wired->cmdHdcp22Enable.sorProtocol == RM_FLCN_HDCP22_SOR_HDMI)
                {
                   bRxRestartRequest = bRxRestartRequest ||
                                       _hdcp22ValidHdmiReAuthRequest(pHdcp22wired->cmdHdcp22Enable.ddcPortPrimary);
                }

                status = hdcp22wiredEncryptionActive_HAL(&Hdcp22wired, pHdcp22wired->cmdHdcp22Enable.sorNum, &bEncActive);
                if(status != FLCN_OK)
                {
                    hdcp22AttachDetachOverlay(HDCP22WIRED_TIMER, LW_FALSE);
                    hdcp22ConfigAuxI2cOverlays((RM_FLCN_HDCP22_SOR_PROTOCOL)pHdcp22wired->cmdHdcp22Enable.sorProtocol,
                                               LW_FALSE);
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
                    hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, ENABLE_HDCP22),
                         cmdQueueId, seqNumId);
                    return status;
                }

               //
               // TODO vijkumar : Consider DP reauth, Since DP is IRQ based FLCN task need to catch
               // reauthentication CP IRQ and execute reauthentication request.
               //
                if (bEncActive)
                {
                    // Get previous active Session's numStreams information.
                    pSession->priDDCPort = pHdcp22wired->cmdHdcp22Enable.ddcPortPrimary;
                    pSession->sorProtocol = (RM_FLCN_HDCP22_SOR_PROTOCOL)pHdcp22wired->cmdHdcp22Enable.sorProtocol;
                    status = _hdcp22RetrieveSession(pSession, &pActiveSession);

                    if (status != FLCN_OK)
                    {
                        hdcp22AttachDetachOverlay(HDCP22WIRED_TIMER, LW_FALSE);
                        hdcp22ConfigAuxI2cOverlays((RM_FLCN_HDCP22_SOR_PROTOCOL)pHdcp22wired->cmdHdcp22Enable.sorProtocol,
                                                   LW_FALSE);
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
                        hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, ENABLE_HDCP22),
                             cmdQueueId, seqNumId);
                        return status;
                    }

                    //
                    // HW bug 2036231 that crypt keeps to be active while encryption
                    // disabled when SOR lane count as 0. The fix is RM send
                    // enable cmd with check AUTODIS_STATE field when enable
                    // flush mode. Falcon would disable crypt if see AUTODIS_STATE
                    // is _DISABLE_LC_0.
                    //
                    if (((pHdcp22wired->cmdHdcp22Enable.sorProtocol == RM_FLCN_HDCP22_SOR_DP) ||
                            (pHdcp22wired->cmdHdcp22Enable.sorProtocol == RM_FLCN_HDCP22_SOR_DUAL_DP)) &&
                        (pHdcp22wired->cmdHdcp22Enable.bCheckAutoDisableState))
                    {
                        LwBool bDisabledWithLaneCount0 = LW_FALSE;

                        status = hdcp22wiredIsAutoDisabledWithLaneCount0_HAL(&Hdcp22wired,
                                                                             pHdcp22wired->cmdHdcp22Enable.sorNum,
                                                                             &bDisabledWithLaneCount0);
                        if ((status == FLCN_OK) && bDisabledWithLaneCount0)
                        {
                            hdcp22AttachDetachOverlay(HDCP22WIRED_TIMER, LW_FALSE);
                            hdcp22ConfigAuxI2cOverlays((RM_FLCN_HDCP22_SOR_PROTOCOL)pHdcp22wired->cmdHdcp22Enable.sorProtocol,
                                                       LW_FALSE);

                            //
                            // As the lane count is set to 0, the HDCP block will not see any SR
                            // and hence the CRYPT_STATUS will continue to show ACTIVE. This is
                            // because it transitions only on SR (Scrambler Reset). We should skip
                            // checking the HDCP22_STATUS for CRYPT_STATUS here.
                            //
                            // Once flush mode exist happens, the lane count will be set to the
                            // new value. At this point SR will start getting sent out on the link.
                            // As SR will be the first valid symbol, it will first reset the HDCP22
                            // STATUS register to DISABLE.
                            //
                            status = hdcp22wiredControlEncryption(ENC_DISABLE,
                                                                  pSession->sorNum,
                                                                  pSession->linkIndex,
                                                                  LW_FALSE,
                                                                  LW_FALSE,
                                                                  0,
                                                                  pSession->bIsAux);
                            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_DISABLE_WITH_LANECNT0;
                            hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, ENABLE_HDCP22),
                                                     cmdQueueId, seqNumId);
                            return status;
                        }
                    }

                    //
                    // If encryption enable request comes again on a encryption enabled sor and
                    // If there is no pending restart request or numStreams are the identical to
                    // previous enable action then just terminate the command.
                    //
                    if (bRxRestartRequest ||
                        bRxIDMsgPending ||
                        ((pHdcp22wired->cmdHdcp22Enable.sorProtocol != RM_FLCN_HDCP22_SOR_HDMI) &&
                         (pHdcp22wired->cmdHdcp22Enable.numStreams != pSession->sesVariablesRptr.numStreams)))
                    {
                        status = hdcp22wiredSendEncryptionEnabledResponse(pHdcp22wired,
                                                                          pSession,
                                                                          cmdQueueId,
                                                                          seqNumId,
                                                                          bRxRestartRequest);
                        if (status == FLCN_ERR_MORE_PROCESSING_REQUIRED)
                        {
                            // Fall through and override status as good.
                            status = FLCN_OK;
                        }
                        else
                        {
                            // Purposely return here directly that avoiding extra AKE_INIT retry.
                            return status;
                        }
                    }
                    else if (_hdcp22IsStreamTypeChngRequest(pHdcp22wired, pSession))
                    {
                        hdcp22AttachDetachOverlay(HDCP22WIRED_TIMER, LW_FALSE);
                        hdcp22ConfigAuxI2cOverlays((RM_FLCN_HDCP22_SOR_PROTOCOL)pHdcp22wired->cmdHdcp22Enable.sorProtocol,
                                                   LW_FALSE);
                        status = _hdcp22HandleStreamTypeChngReq(pHdcp22wired, pCmd, pSession,
                                     pActiveSession, cmdQueueId, seqNumId);

                        if (status != FLCN_ERR_MORE_PROCESSING_REQUIRED)
                        {
                            pSession->msgStatus =(status == FLCN_OK) ?
                                    RM_FLCN_HDCP22_STATUS_ENC_ENABLED :
                                    RM_FLCN_HDCP22_STATUS_ERROR_INIT_SESSION_FAILED;

                            hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, ENABLE_HDCP22),
                                                     cmdQueueId, seqNumId);
                         }
                         return status;
                    }
                    else
                    {
                        hdcp22AttachDetachOverlay(HDCP22WIRED_TIMER, LW_FALSE);
                        hdcp22ConfigAuxI2cOverlays((RM_FLCN_HDCP22_SOR_PROTOCOL)pHdcp22wired->cmdHdcp22Enable.sorProtocol,
                                                   LW_FALSE);
                        bStopProcessingCmd = LW_TRUE;
                    }
                }
            }

            if (bStopProcessingCmd)
            {
                if ((pSession->sorProtocol == RM_FLCN_HDCP22_SOR_DP) ||
                    (pSession->sorProtocol == RM_FLCN_HDCP22_SOR_DUAL_DP))
                {
                    //
                    // Bug 2053730: Unigraf clear READY bit right after CP_IRQ read and
                    // it's too soon to complete authentication. Unfortunately, competitors
                    // intel, AMD both support the case and better to WAR that for real
                    // products.
                    //
                    if ((bRxIDMsgPending) &&
                        (pSession->stage == HDCP22_AUTH_STAGE_WAIT_TO_START_RPTR))
                    {
                        pSession->bReadyNotified = LW_TRUE;
                    }

                    // Bug 200441153: Unigraf 1A07 clear REAUTH right after CP_IRQ read.
                    if (bRxRestartRequest)
                    {
                        pSession->bReAuthNotified = LW_TRUE;
                    }
                }

                pSession->msgStatus = pHdcp22wired->cmdHdcp22Enable.bHpdFromRM ?
                                          RM_FLCN_HDCP22_STATUS_ERROR_HPD :
                                          (pSession->bIsStateMachineActive ?
                                              RM_FLCN_HDCP22_STATUS_ERROR_FLCN_BUSY :
                                              RM_FLCN_HDCP22_STATUS_ERROR_ENC_ACTIVE);
                hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, ENABLE_HDCP22),
                                         cmdQueueId, seqNumId);
                return status;
            }

            // Initialize the session
            _hdcp22SessionInit(pSession);

            pSession->cmdQueueId  = cmdQueueId;
            pSession->cmdSeqNumId = seqNumId;

            pSession->sorNum      = pHdcp22wired->cmdHdcp22Enable.sorNum;
            pSession->sorProtocol = (RM_FLCN_HDCP22_SOR_PROTOCOL)pHdcp22wired->cmdHdcp22Enable.sorProtocol;
            pSession->priDDCPort  = pHdcp22wired->cmdHdcp22Enable.ddcPortPrimary;
            pSession->secDDCPort  = pHdcp22wired->cmdHdcp22Enable.ddcPortSecondary;
            pSession->bReadKsvList = LW_FALSE;

            if (pSession->sorProtocol == RM_FLCN_HDCP22_SOR_HDMI)
            {
                // For HDMI case, only one stream with id as 0 and streamType information from RM.
                pSession->sesVariablesRptr.numStreams = 0x1;
                pSession->sesVariablesRptr.streamIdType[0].streamId = 0;
                pSession->sesVariablesRptr.streamIdType[0].streamType =
                    pHdcp22wired->cmdHdcp22Enable.streamIdType[0].streamType;
            }
            else
            {
                pSession->sesVariablesRptr.numStreams = pHdcp22wired->cmdHdcp22Enable.numStreams;

                for (i = 0; i < pHdcp22wired->cmdHdcp22Enable.numStreams; i++)
                {
                    pSession->sesVariablesRptr.streamIdType[i] = pHdcp22wired->cmdHdcp22Enable.streamIdType[i];
                }

                for (i = 0; i < HDCP22_NUM_DP_TYPE_MASK; i++)
                {
                    pSession->sesVariablesRptr.dpTypeMask[i] = pHdcp22wired->cmdHdcp22Enable.dpTypeMask[i];
                }
            }

            // If set, this will enforce type0 in the repeater Authentication when there is HDCP1X device downstream.
            pSession->sesVariablesRptr.bEnforceType0Hdcp1xDS = pHdcp22wired->cmdHdcp22Enable.bEnforceType0Hdcp1xDS;
            pSession->srmDma = pHdcp22wired->cmdHdcp22Enable.srm;

            if (pSession->sorProtocol == RM_FLCN_HDCP22_SOR_DP || pSession->sorProtocol == RM_FLCN_HDCP22_SOR_DUAL_DP)
            {
                pSession->bIsAux = LW_TRUE;
            }
            else if (pSession->sorProtocol == RM_FLCN_HDCP22_SOR_HDMI)
            {
                pSession->bIsAux = LW_FALSE;
            }

            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_NULL;

            pSession->hdcp22TimerCount = 0;
            if (bRxIDMsgPending && (!bRxRestartRequest))
            {
                pSession->bReadKsvList = LW_TRUE;

                // Get the Session which is already Active
                status = _hdcp22RetrieveSession(pSession, &pActiveSession);
                if (status != FLCN_OK)
                {
                    break;
                }

                // Before Repeater Auth after encryption is enabled need to call start session.
                status = _hdcp22wiredStartSessionWithType1LockCheck(pSession);
                if (status != FLCN_OK)
                {
                    // Retry if type1Lock transition change.
                    status = _hdcp22wiredStartSessionWithType1LockCheck(pSession);

                    if (status != FLCN_OK)
                    {
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_START_SESSION;
                        return status;
                    }
                }

                // Start Repeater authentication
                status = _hdcp22StartRepeaterAuth(pSession);
                if (status != FLCN_OK)
                {
                    break;
                }
            }
            else
            {
                // Start the Authentication
                status = _hdcp22SessionStartAuth(pSession);

                if ((status != FLCN_OK) && (!pSession->bApplyHwDrmType1LockedWar))
                {
                    // HWDRM WAR doesn't allow AKE_INIT retry.
                    while ((status != FLCN_OK) &&
                           (pSession->sesVariablesIr.hdcp22authRetryCount < HDCP22_AKE_INIT_RETRY_MAX))
                    {
                        pSession->sesVariablesIr.hdcp22authRetryCount++;
                        status = hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                                            (LwU32*)&pSession->sesVariablesIr,
                                                                            pSession->integrityHashVal,
                                                                            sizeof(HDCP22_SESSION_VARS_IR),
                                                                            LW_TRUE);
                        if (FLCN_OK != status)
                        {
                            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_INTEGRITY_UPDATE_FAILURE;
                            break;
                        }

                        status = _hdcp22SessionStartAuth(pSession);
                    }
                }
            }

            break;
        }

#ifdef GSPLITE_RTOS
        case RM_FLCN_CMD_TYPE(HDCP22, FLUSH_TYPE):
        {
            status = hdcp22wiredProcessFlushType(pSession, cmdQueueId,
                                                 seqNumId);
            return status;
        }
#endif

        case RM_FLCN_CMD_TYPE(HDCP22, MONITOR_OFF):
        {
            LwBool  bSorPowerDown = LW_FALSE;

            if (pHdcp22wired->cmdHdcp22MonitorOff.sorNum >= maxNoOfSors)
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ILWALID_ARGUMENT;
                hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, MONITOR_OFF),
                                    cmdQueueId, seqNumId);
                break;
            }

#ifdef __COVERITY__
    SanitizeGlobal (pHdcp22wired);
#endif

            status = hdcp22wiredIsSorPoweredDown_HAL(&Hdcp22wired,
                pHdcp22wired->cmdHdcp22MonitorOff.sorNum,
                pHdcp22wired->cmdHdcp22MonitorOff.dfpSublinkMask, &bSorPowerDown);

            if (status != FLCN_OK)
            {
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
                hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, MONITOR_OFF),
                                    cmdQueueId, seqNumId);
            }
            else
            {
                if (bSorPowerDown)
                {
                    // Disable Encryption.
                    status = hdcp22wiredControlEncryption(ENC_DISABLE,
                                                          pSession->sorNum,
                                                          pSession->linkIndex,
                                                          LW_FALSE, LW_TRUE,
                                                          0,
                                                          pSession->bIsAux);
                    if (status != FLCN_OK)
                    {
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
                        hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, MONITOR_OFF),
                                            cmdQueueId, seqNumId);
                    }
                    else
                    {
                        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_MONITOR_OFF_SUCCESS;
                        hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, MONITOR_OFF),
                                            cmdQueueId, seqNumId);
                    }
                }
                else
                {
                    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_MONITOR_OFF_SUCCESS;
                    hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, MONITOR_OFF),
                                             cmdQueueId, seqNumId);
                }
            }
            break;
        }

        case RM_FLCN_CMD_TYPE(HDCP22, VALIDATE_SRM2):
        {
            FLCN_STATUS         status = FLCN_OK;
            LwBool              bNeedToLoadBackAuxI2c = LW_FALSE;
            RM_FLCN_HDCP22_CMD_VALIDATE_SRM2 validateSrm2;

            if ((pHdcp22wired->cmdValidateSrm2.srmListSize > SRM_LIST_MAX_SIZE) ||
                (_hdcp22CheckDmaIdx(&pHdcp22wired->cmdValidateSrm2.srm) == LW_FALSE) )
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
            }
            else
            {

#ifdef __COVERITY__
    SanitizeGlobal (pHdcp22wired);
#endif

                hdcpmemcpy(&validateSrm2,
                           &pHdcp22wired->cmdValidateSrm2,
                           sizeof(RM_FLCN_HDCP22_CMD_VALIDATE_SRM2));

                if (!DMA_ADDR_IS_ZERO(&validateSrm2.srm))
                {
                    if (gBIsAuxI2cLoaded)
                    {
                        hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_FALSE);
                        bNeedToLoadBackAuxI2c = LW_TRUE;
                    }

                    hdcp22AttachDetachOverlay(HDCP22WIRED_CERTRX_SRM, LW_TRUE);
                    status = hdcp22RevocationCheck(NULL, 0, &validateSrm2.srm);
                    hdcp22AttachDetachOverlay(HDCP22WIRED_CERTRX_SRM, LW_FALSE);

                    if (bNeedToLoadBackAuxI2c)
                    {
                        hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_TRUE);
                    }
                }
                else
                {
                    status = FLCN_ERROR;
                }
            }

            pSession->msgStatus = (status != FLCN_OK) ?
                                    RM_FLCN_HDCP22_STATUS_ERROR_ILWALID_SRM :
                                    RM_FLCN_HDCP22_STATUS_VALID_SRM;
            hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, VALIDATE_SRM2),
                                     cmdQueueId, seqNumId);

            return status;
        }

#ifndef GSPLITE_RTOS
        case RM_FLCN_CMD_TYPE(HDCP22, TEST_SE):
        {
            status = hdcp22wiredProcessTestSE(pSession, cmdQueueId, seqNumId);
            return status;
        }
#endif // GSPLITE_RTOS

#ifdef HDCP22_SUPPORT_MST
        case RM_FLCN_CMD_TYPE(HDCP22, WRITE_DP_ECF):
        {
            //
            // TODO: As of now DP ECF is placed under dpaux overlay, will need 
            // to move it to libhdcp22wired or new overlay after optimizing the 
            // overlay combinations of Pascal
            //
            hdcpAttachAndLoadOverlay(HDCP22WIRED_DPAUX);
            status = hdcp22wiredProcessWriteDpECF(pSession, pHdcp22wired,
                                                  seqNumId, cmdQueueId);
            hdcpDetachOverlay(HDCP22WIRED_DPAUX);
            return status;
        }
#endif // HDCP22_SUPPORT_MST

        default:
        {
            break;
        }
    }

    // Incase of FLCN_ERROR, Send Message back to RM.
    if ((cmdType == RM_FLCN_CMD_TYPE(HDCP22, ENABLE_HDCP22)) &&
        (FLCN_OK != status))
    {
        // Don't override the special return code to HWDRM WAR.
        if (pSession->bApplyHwDrmType1LockedWar)
        {
            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_HWDRM_WAR_AUTH_FAILURE;
            pSession->bApplyHwDrmType1LockedWar = LW_FALSE;
        }
        else
        {
            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_INIT_SESSION_FAILED;
        }

        // Ignore return status to keep original failure.
        (void)_hdcp22wiredStopTimer();

        hdcp22AttachDetachOverlay(HDCP22WIRED_TIMER, LW_FALSE);
        hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_FALSE);

        hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, ENABLE_HDCP22),
                                 cmdQueueId, seqNumId);

        _hdcp22StateReset(pSession, LW_FALSE);
    }
    return status;
}

/*!
 *  @brief This function handles SOR interrupt request.
 *  It disables hdcp if sor is powered down.
 *  @param[in]  sorNumber       SOR number
 *  @returns    FLCN_STATUS     FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredHandleSorInterrupt
(
    LwU8 sorNumber
)
{
    FLCN_STATUS status = FLCN_OK;
    LwBool bSorPowerDown;

    CHECK_STATUS(hdcp22wiredIsSorPoweredDown_HAL(&Hdcp22wired, sorNumber, 1,
                                                 &bSorPowerDown));

    if (bSorPowerDown)
    {
        // Disable Encryption.
        CHECK_STATUS(hdcp22wiredControlEncryption(ENC_DISABLE,
                                                  sorNumber,
                                                  0,
                                                  LW_FALSE,
                                                  LW_TRUE,
                                                  0,
                                                  LW_FALSE));
    }

label_return:
        return status;
}

/*!
 *  @brief This function handles Stream Id Type lock and unlock request
 *         This function will be initially ilwoked by sec2 request
 *         It iterates through all the active repeaters and DP devices on which encryption is enabled.
 *         If Type has to be changed, it re-authenticates the repeaters with appropriate type
 *         Once authentication is triggered, it can not process other sors's re-authentication.
 *         So, it increments the global gBgLwrrentReAuthSessionIndex and comes out of the loop.
 *         Once lwrent authentication request is completed, it ilwokes this function again.
 *         Then it starts the loop with saved gBgLwrrentReAuthSessionIndex.
 *         Once all the active repeaters and DP devices are re-authenticated with appropriate type, it sends resposne to SEC2.
 *         If it finds state machine is active, it comes out of the loop and this function will be ilwoked again
 *         after completion of ongoing request.
 *  @returns    FLCN_STATUS     FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22ProcessBackgroundReAuth(void)
{
    FLCN_STATUS status = FLCN_OK;
    HDCP22_ACTIVE_SESSION *pActiveSession;
    LwBool bEncActive = LW_FALSE;

    //
    // Iterarte through all active sessions to authenticate with appropriate stream type
    // If state machine is active processing some other request then
    // current background process is deferred till the end of ongoing process
    //
    while ((!lwrrentSession.bIsStateMachineActive) &&
           (gBgLwrrentReAuthSessionIndex < HDCP22_ACTIVE_SESSIONS_MAX))
    {
        pActiveSession = &activeSessions[gBgLwrrentReAuthSessionIndex];

        CHECK_STATUS(hdcp22wiredEncryptionActive_HAL(&Hdcp22wired, pActiveSession->sorNum, &bEncActive));
        CHECK_STATUS(hdcp22wiredIsType1LockActive(&gBType1LockState));

        //
        // Change the Stream id type only if session is active, it should be
        // a repeater or DP device , encryption should be enabled, if type
        // change is required or if authentication is pending and if Type1 Lock
        // is disabled. If Tyep1 lock is active, reauth is aborted.
        //
        if ((pActiveSession->bActive) &&
            (bEncActive) &&
            (pActiveSession->bIsAux || pActiveSession->bIsRepeater) &&
            (pActiveSession->bTypeChangeRequired) &&
            (!gBType1LockState))
        {
            // Re-Authenticate to change the Stream Id Type
            status = hdcp22ProcessReAuth(&lwrrentSession, pActiveSession, LW_TRUE, 0, 0);
            if (status == FLCN_ERR_ILWALID_STATE)
            {
                break;
            }

            pActiveSession->bTypeChangeRequired = LW_FALSE;

            // Update activeSession storage integrity hash value for later check.
            status = hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                                (LwU32*)pActiveSession,
                                                                &gActiveSessionsHashVal[GET_ACTIVE_SESSION_INDEX(pActiveSession)][0],
                                                                sizeof(HDCP22_ACTIVE_SESSION),
                                                                LW_TRUE);
            if (FLCN_OK != status)
            {
                break;
            }
        }
        gBgLwrrentReAuthSessionIndex++;
    }

    //
    // Clear the response flag of sec2, after itterating through all active sessions and
    // after completion of current re-auth request
    //
    if ((!lwrrentSession.bIsStateMachineActive) &&
        (gBgLwrrentReAuthSessionIndex == HDCP22_ACTIVE_SESSIONS_MAX))
    {
        gBBackGrndReAuthIsInProgress = LW_FALSE;
        gBgLwrrentReAuthSessionIndex = 0;
    }

label_return:
    return status;
}

/*!
 *  @brief This function handles Stream Id Type lock and unlock request
 *  @param[in]  pSession                    pointer to the current session
 *  @param[in]  pActiveSession              pointer to the active session of the sor
 *  @param[in]  bIsSec2ReAuthTypeEnForceReq boolean to specify whether request from
 *                                          SEC2 type enforcement.
 *  @param[in]  cmdQueueId                  Command Queue Id, needed to send the response
 *  @param[in]  seqNumId                    seqNumId Id, needed to send the response
 *  @returns    FLCN_STATUS                 returns the status of reauthentication process
 */
FLCN_STATUS
hdcp22ProcessReAuth
(
    HDCP22_SESSION         *pSession,
    HDCP22_ACTIVE_SESSION  *pActiveSession,
    LwBool                  bIsSec2ReAuthTypeEnForceReq,
    LwU8                    cmdQueueId,
    LwU8                    seqNumId
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       i;
    LwBool      bIsHdcp22Disabled = LW_FALSE;

    //
    // If state machine is active processing someother request,
    // we should wait for the process to complete
    //
    if (pSession->bIsStateMachineActive)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    // Initialize the session
    _hdcp22SessionInit(pSession);

    hdcp22ConfigAuxI2cOverlays(pActiveSession->sorProtocol, LW_TRUE);

    pSession->cmdQueueId  = cmdQueueId;
    pSession->cmdSeqNumId = seqNumId;

    pSession->sorNum      = pActiveSession->sorNum;
    pSession->sorProtocol = pActiveSession->sorProtocol;
    pSession->priDDCPort  = pActiveSession->priDDCPort;
    pSession->secDDCPort  = pActiveSession->secDDCPort;
    pSession->bReadKsvList = LW_FALSE;

    // Get Sec2 lock state
    pSession->sesVariablesIr.bSec2ReAuthTypeEnforceReq = bIsSec2ReAuthTypeEnForceReq;
    pSession->sesVariablesIr.bIsStreamTypeLocked = gBType1LockState;

    status = hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                        (LwU32*)&pSession->sesVariablesIr,
                                                        pSession->integrityHashVal,
                                                        sizeof(HDCP22_SESSION_VARS_IR),
                                                        LW_TRUE);
    if (FLCN_OK != status)
    {
        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_INTEGRITY_UPDATE_FAILURE;
        hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_FALSE);
        return status;
    }

    if (pSession->sorProtocol == RM_FLCN_HDCP22_SOR_HDMI)
    {
        pSession->sesVariablesRptr.numStreams = 0x1;
        pSession->sesVariablesRptr.streamIdType[0] = pActiveSession->streamIdType[0];
    }
    else
    {
        pSession->sesVariablesRptr.numStreams = pActiveSession->numStreams;

        for (i = 0; i < pActiveSession->numStreams; i++)
        {
            pSession->sesVariablesRptr.streamIdType[i] = pActiveSession->streamIdType[i];
        }
    }

    // If set, this will enforce Type0 in the repeater Authentication when there is HDCP1X device downstream.
    pSession->sesVariablesRptr.bEnforceType0Hdcp1xDS = pActiveSession->bEnforceType0Hdcp1xDS;

    pSession->srmDma = pActiveSession->srmDma;

    if (pSession->sorProtocol == RM_FLCN_HDCP22_SOR_DP || pSession->sorProtocol == RM_FLCN_HDCP22_SOR_DUAL_DP)
    {
        pSession->bIsAux = LW_TRUE;
    }
    else if (pSession->sorProtocol == RM_FLCN_HDCP22_SOR_HDMI)
    {
        pSession->bIsAux = LW_FALSE;
    }

    pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_NULL;

    // Disable Encryption
    CHECK_STATUS(hdcp22wiredControlEncryption(ENC_DISABLE,
                                              pSession->sorNum,
                                              pSession->linkIndex,
                                              LW_FALSE,
                                              LW_TRUE,
                                              0,
                                              pSession->bIsAux));

    bIsHdcp22Disabled = LW_TRUE;

    pSession->hdcp22TimerCount = 0;

    //
    // Start the Authentication process again
    // during the authentication process stream id is set based on Sec2 Lock Ref Count
    //
    status = _hdcp22SessionStartAuth(pSession);

label_return:
    if (FLCN_OK != status)
    {
        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_INIT_SESSION_FAILED;

        // Ignore return status to keep original failure.
        (void)_hdcp22wiredStopTimer();

        hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_FALSE);

        _hdcp22StateReset(pSession, bIsHdcp22Disabled);
    }

    return status;
}

/*!
 * @brief This function attaches or detaches ovelrays.
 *
 * @param[in]  overlayId    Overlay index
 * @param[in]  bAttach      if LW_TRUE then specified overlay attached
 *                          if LW_FALSE then specified overlay detached.
 *
 * TODO: bug 200197739 that review libhdcp22wired should implement to
 * attach/detach overlay in dynamic way comparing to existing version.
 */
void
hdcp22AttachDetachOverlay
(
    HDCP_OVERLAY_ID ovlIdx,
    LwBool          bAttach
)
{
    if (bAttach)
    {
        hdcpAttachAndLoadOverlay(ovlIdx);
        gLastAttachedOverlay = ovlIdx;
    }
    else
    {
        hdcpDetachOverlay(ovlIdx);
        gLastAttachedOverlay = HDCP_OVERLAYID_MAX;
    }
}

