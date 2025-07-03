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
 * @file    hdcp22wired_akeh.c
 * @brief   Library to implement AKE_H functions for HDCP 2.2.
 *          Generates the value of H.
 *          Reads value of HPrime and Verifies if with H value.
 */

// TODO: Move this code to ake.c as it is part of ake and remove this file completely.

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_aes.h"
#include "hdcp22wired_akeh.h"
#include "sha256.h"
#ifdef GSPLITE_RTOS
#include "sha256_hs.h"
#endif

/* ------------------------ External definations----------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
static FLCN_STATUS _hdcp22ReadAkeSendHPrime(HDCP22_SESSION *pSession, LwU8* pHPrime)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAkeh", "_hdcp22ReadAkeSendHPrime");

FLCN_STATUS  hdcp22HandleVerifyH(HDCP22_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAkeh", "hdcp22HandleVerifyH");

/* ------------------------ Private Functions ------------------------------- */

/*
 * @brief: This function reads H Prime from receiver
 *
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22ReadAkeSendHPrime
(
    HDCP22_SESSION *pSession,
    LwU8           *pHPrime
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
            HDCP22_DPCD_OFFSET_RX_HPRIME,
            HDCP22_SIZE_H,
            pHPrime,
            LW_TRUE));

#ifdef HDCP22_DP_ERRATA_V3
        if (!hdcp22wiredCheckElapsedTimeWithMaxLimit(&startTime, HDCP22_TIMEOUT_HPRIME_READ_DP))
        {
            return RM_FLCN_HDCP22_STATUS_TIMEOUT_H_PRIME;
        }
#endif
    }
    else
    {
        LwU8 msgBuffer[HDCP22_SIZE_MSG_SEND_HPRIME];

        hdcpmemset(msgBuffer, 0, HDCP22_SIZE_MSG_SEND_HPRIME);

        CHECK_STATUS(hdcp22wiredI2cPerformTransaction_HAL(&Hdcp22wired,
            pSession->priDDCPort,  // HDMI always on primary port.
            LW_HDMI_SCDC_READ_OFFSET,
            HDCP22_SIZE_MSG_SEND_HPRIME,
            msgBuffer,
            LW_TRUE));

        if (msgBuffer[0] != HDCP22_MSG_ID_AKE_SEND_HPRIME)
        {
            return FLCN_ERR_HDCP22_H_PRIME;
        }

        hdcpmemcpy(pHPrime, &msgBuffer[HDCP22_SIZE_MSG_ID], HDCP22_SIZE_H);
    }

label_return :
    return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*
 * @brief: This function reads H value from receiver and
 *         compares with computed H.
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22HandleVerifyH
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        hPrime[HDCP22_SIZE_H];

    hdcpmemset(hPrime, 0, HDCP22_SIZE_H);

    CHECK_STATUS(_hdcp22ReadAkeSendHPrime(pSession, hPrime));

    // TODO: bRepeater flag should be saved at certificateValidation action. Move that when it's implemented.
    CHECK_STATUS(hdcp22wiredHprimeValidation(pSession->rxCaps, hPrime, pSession->sesVariablesIr.bRepeater));

label_return:
    return status;
};

