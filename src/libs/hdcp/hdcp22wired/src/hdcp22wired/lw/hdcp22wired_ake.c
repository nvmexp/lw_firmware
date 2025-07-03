/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    hdcp22wired_ake.c
 * @brief   Library to implement AKE functions for HDCP22.
 *          Handles Master Key Exchange.
 *          Generates, encrypts and sends KM to RX
 *
 */

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_ake.h"
#include "hdcp22wired_srm.h"
#include "lib_hdcpauth.h"

/* ------------------------ External Definations ---------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
static FLCN_STATUS  _hdcp22SendAkeNoStoredKm(HDCP22_SESSION *pSession, LwU8 *pEKm)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAke", "_hdcp22SendAkeNoStoredKm");

/* ------------------------ Private Functions ------------------------------- */
/*
 * @brief: This function sends no stored km message
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @param[in]  pEKm             Pointer to encrypted km.
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22SendAkeNoStoredKm
(
    HDCP22_SESSION *pSession,
    LwU8           *pEKm
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pSession->bIsAux)
    {
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_EKM,
            HDCP22_SIZE_EKM,
            (LwU8*)pEKm,
            LW_FALSE));
    }
    else
    {
        LwU8 msgBuffer[HDCP22_SIZE_MSG_NO_STORED_KM];

        hdcpmemset(msgBuffer, 0, HDCP22_SIZE_MSG_NO_STORED_KM);

        msgBuffer[0] = HDCP22_MSG_ID_AKE_NO_STORED_KM;
        hdcpmemcpy(&msgBuffer[HDCP22_SIZE_MSG_ID], pEKm, HDCP22_SIZE_EKM);

        CHECK_STATUS(hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
            pSession->priDDCPort,  // HDMI always on primary port.
            LW_HDMI_SCDC_WRITE_OFFSET,
            HDCP22_SIZE_MSG_NO_STORED_KM,
            msgBuffer,
            LW_FALSE));
    }

label_return:

   return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*
 * @brief: This function reads RX CERT, RX CAPS and R RX from RX
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @param[out] pCertRx          Pointer to HDCP22_CERTIFICATE
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22RecvAkeSendCert
(
    HDCP22_SESSION      *pSession,
    HDCP22_CERTIFICATE  *pCertRx
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pSession->bIsAux)
    {
#ifdef HDCP22_DP_ERRATA_V3
        FLCN_TIMESTAMP startTime;

        hdcp22wiredMeasureLwrrentTime(&startTime);
#endif

        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_CERT_RX,
            HDCP22_SIZE_CERT_RX,
            (LwU8*)pCertRx,
            LW_TRUE));

        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RRX,
            HDCP22_SIZE_R_RX,
            (LwU8*)pSession->sesVariablesAke.rRx,
            LW_TRUE));

        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_CAPS,
            HDCP22_SIZE_RX_CAPS,
            (LwU8*)(pSession->rxCaps),
            LW_TRUE));

#ifdef HDCP22_DP_ERRATA_V3
        if (!hdcp22wiredCheckElapsedTimeWithMaxLimit(&startTime, HDCP22_TIMEOUT_RXCERT_READ_DP))
        {
            return RM_FLCN_HDCP22_STATUS_TIMEOUT_CERT_RX;
        }
#endif
    }
    else
    {
        LwU8 msgBuffer[HDCP22_SIZE_MSG_SEND_CERT];

        hdcpmemset(msgBuffer, 0, HDCP22_SIZE_MSG_SEND_CERT);

        CHECK_STATUS(hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
            pSession->priDDCPort,  // HDMI always on primary port.
            LW_HDMI_SCDC_READ_OFFSET,
            HDCP22_SIZE_MSG_SEND_CERT,
            msgBuffer,
            LW_TRUE));

        if (msgBuffer[0] != HDCP22_MSG_ID_AKE_SEND_CERT)
        {
            return FLCN_ERROR;
        }

        // Received certificate copy the contents
        hdcpmemcpy(pCertRx, &msgBuffer[HDCP22_SIZE_MSG_ID], HDCP22_SIZE_CERT_RX);

        hdcpmemcpy(&pSession->sesVariablesAke.rRx[0],
            &msgBuffer[HDCP22_SIZE_MSG_ID + HDCP22_SIZE_CERT_RX],
            HDCP22_SIZE_R_RX);

        hdcpmemcpy(&pSession->rxCaps[0],
            &msgBuffer[HDCP22_SIZE_MSG_ID + HDCP22_SIZE_CERT_RX + HDCP22_SIZE_R_RX],
            HDCP22_SIZE_RX_CAPS);
    }

    // Retrieve if repeater and save with hash sanity info.
    pSession->sesVariablesIr.bRepeater = pSession->rxCaps[2] & 0x1;
    status = hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                        (LwU32*)&pSession->sesVariablesIr,
                                                        pSession->integrityHashVal,
                                                        sizeof(HDCP22_SESSION_VARS_IR),
                                                        LW_TRUE);

label_return:
    return status;
}

/*
 * @brief: This function exchanges Master Key (eKm)
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @param[in]  pKpubRx          Pointer to receiver public key.
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22HandleMasterKeyExchange
(
    HDCP22_SESSION *pSession,
    LwU8           *pKpubRx
)
{
    FLCN_STATUS status     = FLCN_OK;
    LwU32       eKm[HDCP22_SIZE_EKM/LW_SIZEOF32(LwU32)];

    hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_FALSE);
    // Generate km(kd, dkey2) inside, and output eKm.
    status = hdcp22wiredKmKdGen(eKm, pSession->sesVariablesAke.rRx, pKpubRx);
    // Attach back protocol overlay and let timerEventHandler proceed error handling in case error.
    hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_TRUE);
    CHECK_STATUS(status);

    CHECK_STATUS(_hdcp22SendAkeNoStoredKm(pSession, (LwU8 *)eKm));

label_return:

    return status;
}

