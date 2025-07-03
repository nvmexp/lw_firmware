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
 * @file   hdcp22wired_mprime_validation.c
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
static void _hdcp22wiredMprimeValidationEntry(void)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_hdcp22MprimeValidation", "start");
#endif

static FLCN_STATUS _hdcp22HandleComputeM(LwU16 numStreams, LwU8 *pStreamIdType, LwU32 seqNumM, LwU8 *pM)
    GCC_ATTRIB_SECTION("imem_hdcp22MprimeValidation", "_hdcp22HandleComputeM");
FLCN_STATUS hdcp22wiredMprimeValidationHandler(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_hdcp22MprimeValidation", "hdcp22wiredMprimeValidationHandler");

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Private Functions ------------------------------- */
#ifdef GSPLITE_RTOS
/*!
 * @brief Entry function of HS lib overlay.
 */
static void
_hdcp22wiredMprimeValidationEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();

    return;
}
#endif

/*
 * @brief: This function computes value of M and  Verifies M.
 * @param[in]  numStreams       MST number of streams.
 * @param[in]  pStreamIdType    Pointer to receiver streamIdType.
 * @param[in]  seqNumM          stream management sequence numM
 * @param[in]  MLprime          Pointer to Mprime array.
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22HandleComputeM
(
    LwU16   numStreams,
    LwU8   *pStreamIdType,
    LwU32   seqNumM,
    LwU8   *pM
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       tmp;
    // We copy streamId structure directly and expect structure is packed with size 2 bytes.
    ct_assert(sizeof(HDCP22_STREAM) == HDCP22_STREAM_IDTYPE_SIZE);
    LwU8        mMessage[sizeof(HDCP22_STREAM) * HDCP22_NUM_STREAMS_MAX + HDCP22_SIZE_SEQ_NUM_M];
    // HWSha needs to be 4bytes-aligned, HWSHDCP22_SIZE_X is 4X and no residues after division.
    LwU32       sha2Kd[HDCP22_SIZE_KD/sizeof(LwU32)];
    LwU32       kd[HDCP22_SIZE_KD/sizeof(LwU32)];

    HDCP22WIRED_SEC_ACTION_MEMSET(mMessage, 0, sizeof(mMessage));
    HDCP22WIRED_SEC_ACTION_MEMSET(sha2Kd, 0, sizeof(sha2Kd));
    HDCP22WIRED_SEC_ACTION_MEMSET(kd, 0, HDCP22_SIZE_KD);

    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_KD,
                                                     (LwU8 *)kd));

    HDCP22WIRED_SEC_ACTION_MEMCPY(mMessage, pStreamIdType,
                                  sizeof(HDCP22_STREAM) * numStreams);

    tmp = seqNumM - 1;

    HDCP22WIRED_SEC_ACTION_SWAP_ENDIANNESS((LwU8*)&tmp, (LwU8*)&tmp, HDCP22_SIZE_SEQ_NUM_M);

    HDCP22WIRED_SEC_ACTION_MEMCPY(&mMessage[sizeof(HDCP22_STREAM) * numStreams],
                                  (LwU8*)&tmp, HDCP22_SIZE_SEQ_NUM_M);

    CHECK_STATUS(HDCP22WIRED_SEC_ACTION_SHA256((LwU8*)sha2Kd, (LwU8 *)kd, HDCP22_SIZE_KD));
    CHECK_STATUS(HDCP22WIRED_SEC_ACTION_HMAC_SHA256(pM, mMessage,
                                                    sizeof(HDCP22_STREAM) * numStreams + HDCP22_SIZE_SEQ_NUM_M,
                                                    (LwU8*)sha2Kd, HDCP22_SIZE_KD));

label_return:
    HDCP22WIRED_SEC_ACTION_MEMSET(kd, 0, HDCP22_SIZE_KD);

    return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Function to validate Lprime from sink.
 * @param[in]  pArg         Pointer to secure action argument.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredMprimeValidationHandler
(
    SELWRE_ACTION_ARG *pArg
)
{
    PSELWRE_ACTION_MPRIME_VALIDATION_ARG pActionArg = &pArg->action.secActionMprimeValidation;
    SELWRE_ACTION_TYPE  prevState = SELWRE_ACTION_ILWALID;
    FLCN_STATUS         status = FLCN_OK;
    // HWSha needs to be 4bytes-aligned, HWSHDCP22_SIZE_X is 4X and no residues after division.
    LwU32               rptrM[HDCP22_SIZE_RPT_M/sizeof(LwU32)];
    LwU8                streamIdType[HDCP22_NUM_STREAMS_MAX * sizeof(HDCP22_STREAM)];

    // Check previous state.
    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_PREV_STATE,
                                                     (LwU8 *)&prevState));
    if(prevState != SELWRE_ACTION_VPRIME_VALIDATION)
    {
        status = FLCN_ERR_HS_HDCP22_WRONG_SEQUENCE;
        goto label_return;
    }

    HDCP22WIRED_SEC_ACTION_MEMSET(rptrM, 0, HDCP22_SIZE_RPT_M);

    CHECK_STATUS(hdcp22wiredReadFromSelwreMemory_HAL(&Hdcp22wired,
                                                     HDCP22_SECRET_STREAM_ID_TYPE,
                                                     (LwU8 *)streamIdType));

    status = _hdcp22HandleComputeM(pActionArg->numStreams,
                                   streamIdType,
                                   pActionArg->seqNumM,
                                   (LwU8*)rptrM);
    if (status == FLCN_OK)
    {
        // Check the value of MPrime against the computed M value
        if(HDCP22WIRED_SEC_ACTION_MEMCMP(pActionArg->mPrime, rptrM, HDCP22_SIZE_RPT_M))
        {
            status = FLCN_ERR_HDCP22_M_PRIME;
        }
        else
        {
            // Update Previous state.
            prevState = SELWRE_ACTION_MPRIME_VALIDATION;
            CHECK_STATUS(hdcp22wiredWriteToSelwreMemory_HAL(&Hdcp22wired,
                                                            HDCP22_SECRET_PREV_STATE,
                                                            (LwU8 *)&prevState,
                                                            LW_FALSE));
        }
    }

label_return:

    // Scrub signatures from stack.
    HDCP22WIRED_SEC_ACTION_MEMSET(rptrM, 0, sizeof(rptrM));

    return status;
}

