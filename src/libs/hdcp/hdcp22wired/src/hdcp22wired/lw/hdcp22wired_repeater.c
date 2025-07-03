/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    hdcp22wired_repeater.c
 * @brief   Library to implement repeater functions for HDCP 2.2.
            Sends Stream_Manage message to Sink.
            Read KSV list from RX.
            Checks if the Stream_Manage is sent correctly to Rx.
 */

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_repeater.h"
#include "lib_hdcpauth.h"
#include "hdcp22wired_srm.h"
#include "sha256.h"

/* ------------------------ External definations----------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Private Functions ------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*
 * @brief: This function handles and reads receiver Id list
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @param[in]  pVBuffer         Pointer to vBuffer array.
 * @param[in]  vBufferSize      vBuffer array size.
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22HandleReadReceiverIdList
(
    HDCP22_SESSION *pSession,
    LwU8           *pVBuffer,
    LwU32           vBufferSize
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        sizeKSVList = 0;
    LwBool      bEnforceType0Hdcp1xDS;

    hdcpmemset(pVBuffer, 0, vBufferSize);

    if (pSession->bIsAux)
    {
        // Read RxInfo
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_RX_INFO,
            HDCP22_SIZE_RX_INFO,
            &pVBuffer[HDCP22_KSV_LIST_SIZE_MAX],
            LW_TRUE));

        // Swap the RxInfo's endiness to correct byte order for reference
        swapEndianness(&pSession->sesVariablesRptr.rxInfo,
            &pVBuffer[HDCP22_KSV_LIST_SIZE_MAX], HDCP22_SIZE_RX_INFO);

        pSession->sesVariablesRptr.numDevices = DRF_VAL(_RCV_HDCP22_RX_INFO,
            _DEVICE, _COUNT, pSession->sesVariablesRptr.rxInfo);

        // Read Seq Num V
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_SEQ_NUM_V,
            HDCP22_SIZE_SEQ_NUM_V,
            (LwU8 *)(&pSession->sesVariablesRptr.seqNumV),
            LW_TRUE));

        // Read V PRIME
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_VPRIME,
            HDCP22_SIZE_V_PRIME,
            pSession->sesVariablesRptr.Vprime,
            LW_TRUE));

        sizeKSVList = pSession->sesVariablesRptr.numDevices * HDCP22_SIZE_RECV_ID_8;

        // Copy KSV List
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_ID_LIST,
            sizeKSVList,
            &pVBuffer[0],
            LW_TRUE));

        // Copy RxInfo
        hdcpmemcpy(&pVBuffer[sizeKSVList], &pVBuffer[HDCP22_KSV_LIST_SIZE_MAX],
            HDCP22_SIZE_RX_INFO);

        // Copy seqNumV
        hdcpmemcpy(&pVBuffer[sizeKSVList + HDCP22_SIZE_RX_INFO],
            &pSession->sesVariablesRptr.seqNumV, HDCP22_SIZE_SEQ_NUM_V);
    }
    else
    {
        LwU8 msgBuffer[HDCP22_SIZE_MSG_RPT_RX_IDS];

        hdcpmemset(msgBuffer, 0, HDCP22_SIZE_MSG_RPT_RX_IDS);

        CHECK_STATUS(hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
            pSession->priDDCPort,  // HDMI always on primary port.
            LW_HDMI_SCDC_READ_OFFSET,
            HDCP22_SIZE_MSG_RPT_RX_IDS,
            msgBuffer,
            LW_TRUE));

        if (msgBuffer[0] != HDCP22_MSG_ID_RPT_RXIDS)
        {
            return FLCN_ERR_HDCP22_ILWALID_RXIDLIST;
        }

        // Copy the RXInfo
        hdcpmemcpy(&pSession->sesVariablesRptr.rxInfo, &msgBuffer[HDCP22_SIZE_MSG_ID], HDCP22_SIZE_RX_INFO);

        // swap the endiness to correct the Byte order
        swapEndianness(&pSession->sesVariablesRptr.rxInfo, &pSession->sesVariablesRptr.rxInfo, HDCP22_SIZE_RX_INFO);

        // Read the Number of Devices Downstream
        pSession->sesVariablesRptr.numDevices = DRF_VAL(_RCV_HDCP22_RX_INFO, _DEVICE, _COUNT,
                                                        pSession->sesVariablesRptr.rxInfo);

        // Copy the Seq Number of V
        hdcpmemcpy(&pSession->sesVariablesRptr.seqNumV, &msgBuffer[HDCP22_SIZE_MSG_ID +
                                         HDCP22_SIZE_RX_INFO], HDCP22_SIZE_SEQ_NUM_V);

        // Copy Vprime from Sink
        hdcpmemcpy(&pSession->sesVariablesRptr.Vprime, &msgBuffer[HDCP22_SIZE_MSG_ID +
                HDCP22_SIZE_RX_INFO + HDCP22_SIZE_SEQ_NUM_V], HDCP22_SIZE_V_PRIME);

        // Prepare msg for V' calulwlation
        // {v' = ReceiverIdList || RxInfo || SeqV, Kd  }
        sizeKSVList = pSession->sesVariablesRptr.numDevices * HDCP22_SIZE_RECV_ID_8;

        // Copy the KSV list
        hdcpmemcpy(&pVBuffer[0], &msgBuffer[HDCP22_SIZE_MSG_ID + HDCP22_SIZE_RX_INFO +
                   HDCP22_SIZE_SEQ_NUM_V + HDCP22_SIZE_V_PRIME], sizeKSVList);

        // Copy RxInfo
        hdcpmemcpy(&pVBuffer[sizeKSVList], &msgBuffer[HDCP22_SIZE_MSG_ID], HDCP22_SIZE_RX_INFO);

        // Copy seqNumV
        hdcpmemcpy(&pVBuffer[sizeKSVList + HDCP22_SIZE_RX_INFO], &msgBuffer[HDCP22_SIZE_MSG_ID +
                                         HDCP22_SIZE_RX_INFO], HDCP22_SIZE_SEQ_NUM_V);

    }

    pSession->seqNumV++;

    //
    // Verify the KSV list message is transmitted correctly and also update vlsb
    // at ReceiverIdListAck message.
    //
    // Here, we foward inout flag if enforce type0 with HDCP1X DS and HS would 
    // check and update the flag.
    //
    bEnforceType0Hdcp1xDS = pSession->sesVariablesRptr.bEnforceType0Hdcp1xDS;

    hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_FALSE);
    status = hdcp22wiredVprimeValidation(pSession->sesVariablesRptr.numDevices,
                                         pVBuffer, 
                                         pSession->sesVariablesRptr.Vprime,
                                         pSession->sesVariablesRptr.Vlsb,
                                         &bEnforceType0Hdcp1xDS);
    hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_TRUE);
    if (status != FLCN_OK)
    {
        goto label_return;
    }

    if ((DRF_VAL(_RCV_HDCP22_RX_INFO, _MAX_DEVS, _EXCEEDED,
                 pSession->sesVariablesRptr.rxInfo)) ||
        (DRF_VAL(_RCV_HDCP22_RX_INFO, _MAX_CASCADE, _EXCEEDED,
                 pSession->sesVariablesRptr.rxInfo)))
    {
        // Max number of Down Stream/Cascaded devices exceeded
        return FLCN_ERR_HDCP22_ILWALID_RXIDLIST;
    }

    // Check the initial sequence number and Roll over of sequence number
    if (((pSession->seqNumV ==1) && pSession->sesVariablesRptr.seqNumV) ||
        ((pSession->seqNumV !=1) && (!pSession->sesVariablesRptr.seqNumV)))
    {
        return FLCN_ERR_HDCP22_SEQ_ROLLOVER;
    }

    //
    // If HS found HDCP1.x device downstream and type1 lock not set then 
    // bEnforceType0Hdcp1xDS will be set, accordingly LS copy of streamIdType 
    // will be updated.
    //
    if (bEnforceType0Hdcp1xDS)
    {
        LwU32 i;

        for (i = 0; i < pSession->sesVariablesRptr.numStreams; i++)
        {
            pSession->sesVariablesRptr.streamIdType[i].streamType = 0;
        }

        if (pSession->bIsAux)
        {
            for (i = 0; i < HDCP22_NUM_DP_TYPE_MASK; i++)
            {
                pSession->sesVariablesRptr.dpTypeMask[i] = 0x0;
            }
        }
    }

label_return:
    return status;
};

/*

 * @brief: This function verifies M with M-Prime.
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22HandleVerifyM
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS     status = FLCN_OK;
    HDCP22_STREAM   streamIdType[HDCP22_NUM_STREAMS_MAX];
    LwU8            rptrMprime[HDCP22_SIZE_RPT_M];

    hdcpmemset(streamIdType, 0, sizeof(streamIdType));
    hdcpmemset(rptrMprime, 0, HDCP22_SIZE_RPT_M);

    if (pSession->bIsAux)
    {
#ifdef HDCP22_DP_ERRATA_V3
        FLCN_TIMESTAMP startTime;

        hdcp22wiredMeasureLwrrentTime(&startTime);
#endif

        // Read  M prime
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_M_PRIME,
            HDCP22_SIZE_RPT_M,
            rptrMprime,
            LW_TRUE));

#ifdef HDCP22_DP_ERRATA_V3
        if (!hdcp22wiredCheckElapsedTimeWithMaxLimit(&startTime, HDCP22_TIMEOUT_MPRIME_READ_DP))
        {
            return RM_FLCN_HDCP22_STATUS_TIMEOUT_MPRIME;
        }
#endif
    }
    else
    {
        LwU8 msgBuffer[HDCP22_SIZE_MSG_RPT_STR_READY];

        hdcpmemset(msgBuffer, 0, HDCP22_SIZE_MSG_RPT_STR_READY);

        CHECK_STATUS(hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
            pSession->priDDCPort,  // HDMI always on primary port.
            LW_HDMI_SCDC_READ_OFFSET,
            HDCP22_SIZE_MSG_RPT_STR_READY,
            msgBuffer,
            LW_TRUE));

        if (msgBuffer[0] != HDCP22_MSG_ID_RPT_STREAM_READY)
        {
            return FLCN_ERR_HDCP22_M_PRIME;
        }

        hdcpmemcpy(&rptrMprime[0], &msgBuffer[HDCP22_SIZE_MSG_ID], HDCP22_SIZE_RPT_M);
    }

    hdcpmemcpy(streamIdType, (LwU8*)pSession->sesVariablesRptr.streamIdType, 
               sizeof(streamIdType));
    status = hdcp22wiredMprimeValidation(pSession->sesVariablesRptr.numStreams,
                                         pSession->sesVariablesRptr.seqNumM,
                                         rptrMprime);

label_return:

    return status;
}

/*
 * @brief: This function validates sends acknowledgement for receiver id list
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22WriteReceiverIdAck
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pSession->bIsAux)
    {
        // Writes V_LSB
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_V_LSB,
            HDCP22_SIZE_V_PRIME,
            pSession->sesVariablesRptr.Vlsb,
            LW_FALSE));
    }
    else
    {
        LwU8 msgBuffer[HDCP22_SIZE_MSG_RPT_RX_ID_ACK];

        hdcpmemset(msgBuffer, 0, HDCP22_SIZE_MSG_RPT_RX_ID_ACK);

        msgBuffer[0] = HDCP22_MSG_ID_RPT_RXID_ACK;

        hdcpmemcpy(&msgBuffer[HDCP22_SIZE_MSG_ID], &pSession->sesVariablesRptr.Vlsb, HDCP22_SIZE_V_PRIME);

        CHECK_STATUS(hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
            pSession->priDDCPort,  // HDMI always on primary port.
            LW_HDMI_SCDC_WRITE_OFFSET,
            HDCP22_SIZE_MSG_RPT_RX_ID_ACK,
            msgBuffer,
            LW_FALSE));
    }

label_return:
    return status;

}

/*
 * @brief: This function sends the 'Stream Manage Message' to downlink repeater
 *         Hard coding StreamID = 0x0, StreamType = 0x0, numStreams = 0x1
 *         Not considering MST case for now.
 *         Stream_Manage_Message = SeqNumM || NumOfStreams || StreamIDType
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22WriteStreamManageMsg
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8       *pStreamIdType;
    LwU8        rptK[HDCP22_SIZE_RPT_K];
    LwU8        seqNumM[HDCP22_SIZE_SEQ_NUM_M];

    hdcpmemset(rptK, 0, HDCP22_SIZE_RPT_K);
    hdcpmemset(seqNumM, 0, HDCP22_SIZE_SEQ_NUM_M);

    // Restart the Authentication if M value Rolls Over
    if(pSession->sesVariablesRptr.seqNumM == HDCP22_SEQ_NUM_M_MAX)
    {
        // Verify if the Seq Number is Rolled Over
        return FLCN_ERR_HDCP22_SEQ_ROLLOVER;
    }

    // Seq_Num_M to write in big endian
    swapEndianness(seqNumM, (LwU8*)&pSession->sesVariablesRptr.seqNumM,
        HDCP22_SIZE_SEQ_NUM_M);

    // Number of streams K to write in big endian
    swapEndianness(rptK, (LwU8*)&pSession->sesVariablesRptr.numStreams,
        HDCP22_SIZE_RPT_K);

    // Type1Locked is already checked and processed at LS and HS.
    pStreamIdType = (LwU8*)pSession->sesVariablesRptr.streamIdType;

    if(pSession->bIsAux)
    {
        // Write the Sequence Number of M
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_SEQ_NUM_M,
            HDCP22_SIZE_SEQ_NUM_M,
            seqNumM,
            LW_FALSE));

        // Write the Number of streams
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_K,
            HDCP22_SIZE_RPT_K,
            rptK,
            LW_FALSE));

        // Write StreamID Type
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_STR_ID_TYPE,
            2 * pSession->sesVariablesRptr.numStreams,
            pStreamIdType,
            LW_FALSE));
    }
    else
    {
        LwU8 msgBuffer[HDCP22_SIZE_MSG_RPT_STR_MANAGE];

        hdcpmemset(msgBuffer, 0, HDCP22_SIZE_MSG_RPT_STR_MANAGE);

        msgBuffer[0] = HDCP22_MSG_ID_RPT_STREAM_MANAGE;

        //
        //  {msg-Id, SeqM, k, StremIdType}
        //  {0x10, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0}
        //
        hdcpmemcpy(&msgBuffer[HDCP22_SIZE_MSG_ID], seqNumM,
            HDCP22_SIZE_SEQ_NUM_M);

        hdcpmemcpy(&msgBuffer[HDCP22_SIZE_MSG_ID + HDCP22_SIZE_SEQ_NUM_M],
            rptK, HDCP22_SIZE_RPT_K);

        hdcpmemcpy(&msgBuffer[HDCP22_SIZE_MSG_ID + HDCP22_SIZE_SEQ_NUM_M + HDCP22_SIZE_RPT_K],
            pStreamIdType,
            2 * pSession->sesVariablesRptr.numStreams);

        CHECK_STATUS(hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
            pSession->priDDCPort,  // HDMI always on primary port.
            LW_HDMI_SCDC_WRITE_OFFSET,
            HDCP22_SIZE_MSG_RPT_STR_MANAGE,
            msgBuffer,
            LW_FALSE));
    }

    // Increment the Seq_Num_M every time the Stream Manage is sent
    pSession->sesVariablesRptr.seqNumM++;

label_return:
    return status;
}

