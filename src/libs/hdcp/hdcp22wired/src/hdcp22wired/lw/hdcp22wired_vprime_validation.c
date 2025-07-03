/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_vprime_validation.c
 * @brief  Hdcp22 wrapper Functions for security
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "lib_hdcp22wired.h"
#include "csb.h"
#include "common_hslib.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
#ifdef GSPLITE_RTOS
static void _hdcp22wiredVprimeValidationEntry(void)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_hdcp22VprimeValidation", "start");
#endif

static FLCN_STATUS _hdcp22ComputeAndVerifyVPrime(LwU8 numDevices, LwU8 *pVinput, LwU8 *pVprime, LwU8 *pVlsb)
    GCC_ATTRIB_SECTION("imem_hdcp22VprimeValidation", "_hdcp22ComputeAndVerifyVPrime");
FLCN_STATUS hdcp22wiredVprimeValidationHandler(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_hdcp22VprimeValidation", "hdcp22wireVprimeValidationHandler");

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Definitions ---------------------------- */
/* ------------------------ Private Functions ------------------------------- */
#ifdef GSPLITE_RTOS
/*!
 * @brief Entry function of HS lib overlay.
 */
static void
_hdcp22wiredVprimeValidationEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();

    return;
}
#endif

/*
 * @brief: This function computes value of V and compares it with received
 *         vPrime(MSB of v) and Stores LSB of V.
 * @param[in]  numDevices       RxIdList number of devices.
 * @param[in]  pVinput          Pointer to V(V') input data.
 * @param[in]  pVprime          Pointer to Vprime MSB from receiver.
 * @param[out] pVlsb            Pointer to Vprime LSB.
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22ComputeAndVerifyVPrime
(
    LwU8    numDevices,
    LwU8   *pVinput,
    LwU8   *pVprime,
    LwU8   *pVlsb
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        sizeKSVList = 0;
    LwU8        hashV[HDCP22_SIZE_V];
    LwU32       kd[HDCP22_SIZE_KD/sizeof(LwU32)];

    HDCP22WIRED_SEC_ACTION_MEMSET(hashV, 0, sizeof(hashV));

    sizeKSVList = numDevices * HDCP22_SIZE_RECV_ID_8;

    // Retrieve confidential Kd.
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_KD,
                                                     (LwU8 *)kd));

    if (status == FLCN_OK)
    {
        // v' = hmacsha256(ReceiverIdList || RxInfo || SeqV, Kd)
        status = HDCP22WIRED_SEC_ACTION_HMAC_SHA256(hashV,
                                                    pVinput,
                                                    (sizeKSVList + HDCP22_SIZE_SEQ_NUM_V + HDCP22_SIZE_RX_INFO),
                                                    (LwU8 *)kd,
                                                    HDCP22_SIZE_KD);

        if ((status != FLCN_OK) ||
            HDCP22WIRED_SEC_ACTION_MEMCMP(hashV, pVprime, HDCP22_SIZE_V_PRIME))
        {
            status = FLCN_ERR_HDCP22_V_PRIME;
        }
        else
        {
            // Store the value of V LSB
            HDCP22WIRED_SEC_ACTION_MEMCPY(pVlsb, &hashV[HDCP22_SIZE_V_PRIME], HDCP22_SIZE_V_PRIME);
        }
    }

label_return:
    // Scrub secret from stack.
    HDCP22WIRED_SEC_ACTION_MEMSET(hashV, 0, HDCP22_SIZE_V);
    HDCP22WIRED_SEC_ACTION_MEMSET(kd, 0, HDCP22_SIZE_KD);

    return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Function to validate Vprime from sink.
 * @param[in]  pArg         Pointer to secure action argument.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredVprimeValidationHandler
(
    SELWRE_ACTION_ARG *pArg
)
{
    PSELWRE_ACTION_VPRIME_VALIDATION_ARG pActionArg = &pArg->action.secActiolwprimeValidation;
    SELWRE_ACTION_TYPE  prevState = SELWRE_ACTION_ILWALID;
    FLCN_STATUS         status = FLCN_OK;

    // Check previous state.
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_PREV_STATE,
                                                     (LwU8 *)&prevState));

    //
    // READY notification could happen during authentication or after authenticated.
    // To READY notification after authenticated situation, startSession already
    // check SOR encryption state and no need to check if active here.
    //
    if((prevState != SELWRE_ACTION_EKS_GEN) &&
       (prevState != SELWRE_ACTION_REPEATER_START_SESSION))
    {
        status = FLCN_ERR_HS_HDCP22_WRONG_SEQUENCE;
        goto label_return;
    }

    CHECK_STATUS(_hdcp22ComputeAndVerifyVPrime(pActionArg->numDevices,
                                               pActionArg->vInput,
                                               pActionArg->vPrime,
                                               pActionArg->vLsb));

    if (pActionArg->bEnforceType0Hdcp1xDS)
    {
        LwU16   rxInfo;

        // Swap the endiness to correct the Byte order
        HDCP22WIRED_SEC_ACTION_SWAP_ENDIANNESS(&rxInfo,
                                               &pActionArg->vInput[pActionArg->numDevices * HDCP22_SIZE_RECV_ID_8],
                                               HDCP22_SIZE_RX_INFO);

        // If there is HDCP1.X device downstream and Type=1 is being enforced, enable Type0
        if (DRF_VAL(_RCV_HDCP22_RX_INFO, _HDCP1_X_DEVICE, _DOWNSTREAM, rxInfo))
        {
            HDCP22_STREAM   streamIdType[HDCP22_NUM_STREAMS_MAX];
            LwU32           dpTypeMask[HDCP22_NUM_DP_TYPE_MASK];
            LwU32           i;

            // TODO: wait review conclusion for type1Lock case and may refine later.

            CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                             HDCP22_SECRET_STREAM_ID_TYPE,
                                                             (LwU8*)streamIdType));

            CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                             HDCP22_SECRET_DP_TYPE_MASK,
                                                             (LwU8*)dpTypeMask));

            for (i = 0; i < HDCP22_NUM_STREAMS_MAX; i++)
            {
                streamIdType[i].streamType = 0;
            }

            for (i = 0; i < HDCP22_NUM_DP_TYPE_MASK; i++)
            {
                dpTypeMask[i] = 0x0;
            }

            // Save streamIdType, dpTypeMask value and will program to HW at controlEncryption.
            CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                            HDCP22_SECRET_STREAM_ID_TYPE,
                                                            (LwU8 *)streamIdType,
                                                            LW_FALSE));

            CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                            HDCP22_SECRET_DP_TYPE_MASK,
                                                            (LwU8 *)dpTypeMask,
                                                            LW_FALSE));
        }
        else
        {
            // Update output flag to tell LS that no HDCP1X DS situation happening.
            pActionArg->bEnforceType0Hdcp1xDS = LW_FALSE;
        }
    }

    // Update Previous state.
    prevState = SELWRE_ACTION_VPRIME_VALIDATION;
    CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                    HDCP22_SECRET_PREV_STATE,
                                                    (LwU8 *)&prevState,
                                                    LW_FALSE));

label_return:
    return status;
}

