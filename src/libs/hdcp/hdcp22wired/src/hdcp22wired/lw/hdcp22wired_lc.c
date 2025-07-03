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
 * @file    hdcp22wired_lc.c
 * @brief   Library to implement LC stage of HDCP 2.2.
 *          Sends the LC int Message.
 *          Callwlates and verifies the value of L
 *
 */

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_lc.h"
#include "hdcp22wired_aes.h"
#include "sha256.h"

/* ------------------------ External Definations ---------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
static FLCN_STATUS _hdcp22ReadAkeSendPairingInfo(HDCP22_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredLc", "_hdcp22ReadAkeSendPairingInfo");

/* ------------------------ Private Functions ------------------------------- */
/*
 * @brief: This function reads pairing info
 *         Storing  of pairing info is avoided for security considerations, As per
 *         SW IAS we read pairing info and ignore the buffer which got received as
 *         part of send pairing info message, This buffer will get used incase of
 *         Stored Km.
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22ReadAkeSendPairingInfo
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        eKhKm[HDCP22_SIZE_EKH_KM];

    hdcpmemset(eKhKm, 0, HDCP22_SIZE_EKH_KM);

    if(pSession->bIsAux)
    {
#ifdef HDCP22_DP_ERRATA_V3
        FLCN_TIMESTAMP startTime;

        hdcp22wiredMeasureLwrrentTime(&startTime);
#endif

        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_EHKM_RD,
            HDCP22_SIZE_EKH_KM,
            eKhKm,
            LW_TRUE));

#ifdef HDCP22_DP_ERRATA_V3
        if (!hdcp22wiredCheckElapsedTimeWithMaxLimit(&startTime, HDCP22_TIMEOUT_PAIRING_READ_DP))
        {
            return RM_FLCN_HDCP22_STATUS_TIMEOUT_PAIRING;
        }
#endif
    }
    else
    {
        LwU8 msgBuffer[HDCP22_SIZE_MSG_PAIRING_INFO];

        hdcpmemset(msgBuffer, 0, HDCP22_SIZE_MSG_PAIRING_INFO);

        CHECK_STATUS(hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
            pSession->priDDCPort,  // HDMI always on primary port.
            LW_HDMI_SCDC_READ_OFFSET,
            HDCP22_SIZE_MSG_PAIRING_INFO,
            msgBuffer,
            LW_TRUE));

        if (msgBuffer[0] != HDCP22_MSG_ID_AKE_SEND_PAIRING)
        {
            return FLCN_ERR_HDCP22_PAIRING;
        }

    }
label_return:
    return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*
 * @brief: This function reads pairing info, Handles pairing process
 *         if stored Km code needs to store  eKm, m, receiver ID.
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22HandlePairing
(
    HDCP22_SESSION *pSession
)
{
    return _hdcp22ReadAkeSendPairingInfo(pSession);
};

/*
 * @brief: This function sends LC init message to HDCP2.2 receiver
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22HandleLcInit
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS     status = FLCN_OK;

    if (pSession->bIsAux)
    {
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_RN,
            HDCP22_SIZE_R_N,
            (LwU8 *)pSession->sesVariablesSke.Rn,
            LW_FALSE));

    }
    else
    {
        LwU8 msgBuffer[HDCP22_SIZE_MSG_LC_INIT];

        hdcpmemset(msgBuffer, 0, HDCP22_SIZE_MSG_LC_INIT);
        msgBuffer[0] = HDCP22_MSG_ID_LC_INIT;

        // Copy Rn
        hdcpmemcpy(&msgBuffer[HDCP22_SIZE_MSG_ID], &pSession->sesVariablesSke.Rn[0], HDCP22_SIZE_R_N);

        CHECK_STATUS(hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
            pSession->priDDCPort,  // HDMI always on primary port.
            LW_HDMI_SCDC_WRITE_OFFSET,
            HDCP22_SIZE_MSG_LC_INIT,
            msgBuffer,
            LW_FALSE));
   }
label_return:
    return status;
}

/*
 * @brief: This function reads L value from receiver and compares with transmitter L value.
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22HandleVerifyL
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        lPrime[HDCP22_SIZE_L];

    hdcpmemset(lPrime, 0, HDCP22_SIZE_L);

    if (pSession->bIsAux)
    {
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_LPRIME,
            HDCP22_SIZE_L,
            lPrime,
            LW_TRUE));
    }
    else
    {
        LwU8 msgBuffer[HDCP22_SIZE_MSG_SEND_LPRIME];

        hdcpmemset(msgBuffer, 0, HDCP22_SIZE_MSG_SEND_LPRIME);

        CHECK_STATUS(hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
            pSession->priDDCPort,  // HDMI always on primary port.
            LW_HDMI_SCDC_READ_OFFSET,
            HDCP22_SIZE_MSG_SEND_LPRIME,
            msgBuffer,
            LW_TRUE));

        if (msgBuffer[0] != HDCP22_MSG_ID_LC_SEND_LPRIME)
        {
            status = FLCN_ERR_HDCP22_L_PRIME;
            goto label_return;
        }

        hdcpmemcpy(lPrime, &msgBuffer[HDCP22_SIZE_MSG_ID], HDCP22_SIZE_L);
    }

    status = hdcp22wiredLprimeValidation(&pSession->sesVariablesAke.rRx[0],
                                         lPrime);

label_return :
    return status;
}
