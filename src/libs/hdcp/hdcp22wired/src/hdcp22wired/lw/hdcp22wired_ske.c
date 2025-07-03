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
 * @file    hdcp22wired_ske.c
 * @brief   Library is to implement SKE functions for HDCP 2.2.
 *          In SKE, KS (Session Key) and RIV (initialization vector) are generated.
 *          KS is encrypted and sent to RX.
 *          Riv is also sent to RX in the Send_Eks message.
 */

/* ------------------------- Application includes --------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_aes.h"
#include "hdcp22wired_ske.h"

/* ------------------------ External Definations ---------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
static FLCN_STATUS _hdcpSendSkeSendEks(HDCP22_SESSION *pSession, LwU8* pEks, LwU8* pRiv)
          GCC_ATTRIB_SECTION("imem_libHdcp22wiredSke", "_hdcpSendSkeSendEks");

/* ------------------------ Private Functions ------------------------------- */
/*

 * @brief: This function generates sends encrypted Ks to receiver
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @param[in]  pEks             Pointer to encrypted Ks array.
 * @param[in]  pRiv             Pointer to Riv array.
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcpSendSkeSendEks
(
    HDCP22_SESSION *pSession,
    LwU8           *pEks,
    LwU8           *pRiv
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pSession->bIsAux)
    {
        // Write EKS
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_EKS,
            HDCP22_SIZE_EKS,
            pEks,
            LW_FALSE));

        // Write RIV
        CHECK_STATUS(hdcp22wiredAuxPerformTransaction_HAL(&Hdcp22wired,
            HDCP22_AUX_PORT(pSession),
            HDCP22_DPCD_OFFSET_RX_RIV,
            HDCP22_SIZE_R_IV,
            pRiv,
            LW_FALSE));
    }
    else
    {
        LwU8 msgBuffer[HDCP22_SIZE_MSG_SEND_EKS_RIV];

        hdcpmemset(msgBuffer, 0, HDCP22_SIZE_MSG_SEND_EKS_RIV);

        msgBuffer[0] = HDCP22_MSG_ID_SKE_SEND_EKS_RIV;
        hdcpmemcpy(&msgBuffer[HDCP22_SIZE_MSG_ID], pEks, HDCP22_SIZE_EKS);
        hdcpmemcpy(&msgBuffer[HDCP22_SIZE_MSG_ID + HDCP22_SIZE_EKS], pRiv, HDCP22_SIZE_R_IV);

        CHECK_STATUS(hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
            pSession->priDDCPort,   // HDMI always on primary port.
            LW_HDMI_SCDC_WRITE_OFFSET,
            HDCP22_SIZE_MSG_SEND_EKS_RIV,
            msgBuffer,
            LW_FALSE));
    }

label_return:
    return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*
 * @brief: This function handles SKE INIT stage.
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22HandleGenerateSkeInit
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        eKs[HDCP22_SIZE_EKS];
    LwU8        Riv[HDCP22_SIZE_R_IV];

    CHECK_STATUS(hdcp22wiredEksRivGen(pSession->sesVariablesAke.rRx, eKs, Riv));
    status = _hdcpSendSkeSendEks(pSession, (LwU8*)eKs, Riv);

label_return:
    return status;
};

