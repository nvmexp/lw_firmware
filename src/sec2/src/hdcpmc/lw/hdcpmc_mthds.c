/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    hdcpmc_mthds.c
 * @brief   Processes individual methods received from the channel.
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "lwosselwreovly.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_chnmgmt.h"
#include "sec2_objsec2.h"
#include "sha256.h"
#include "hdcpmc/hdcpmc_constants.h"
#include "hdcpmc/hdcpmc_crypt.h"
#include "hdcpmc/hdcpmc_data.h"
#include "hdcpmc/hdcpmc_mem.h"
#include "hdcpmc/hdcpmc_mthds.h"
#include "hdcpmc/hdcpmc_pvtbus.h"
#include "hdcpmc/hdcpmc_rsa.h"
#include "hdcpmc/hdcpmc_scp.h"
#include "hdcpmc/hdcpmc_session.h"
#include "hdcpmc/hdcpmc_srm.h"
#include "hdcpmc/hdcpmc_types.h"

#include "dev_sec_csb.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define HDCP_GENERAL_ERROR(code)                       LW95A1_HDCP_ERROR_##code
#define HDCP_METHOD_ERROR(mthd, code)         LW95A1_HDCP_##mthd##_ERROR_##code

/*!
 * @brief Random number type.
 * Enumeration to specify a random number type. Each of the entries below have
 * its own random number expectations.
 */
typedef enum
{
    HDCP_RANDOM_TYPE_RN,
    HDCP_RANDOM_TYPE_KM,
    HDCP_RANDOM_TYPE_RIV,
    HDCP_RANDOM_TYPE_KS
} HDCP_RANDOM_TYPE;

// PES private header format
#define LW_HDCP_PES_PRIVATE_HDR_0                                           0x0
#define LW_HDCP_PES_PRIVATE_HDR_0_RSVD                                    31:19
#define LW_HDCP_PES_PRIVATE_HDR_0_STR_0                                   18:17
#define LW_HDCP_PES_PRIVATE_HDR_0_MARK_0                                  16:16
#define LW_HDCP_PES_PRIVATE_HDR_0_STR_1                                    15:1
#define LW_HDCP_PES_PRIVATE_HDR_0_MARK_1                                    0:0

#define LW_HDCP_PES_PRIVATE_HDR_1                                           0x1
#define LW_HDCP_PES_PRIVATE_HDR_1_STR_0                                   31:17
#define LW_HDCP_PES_PRIVATE_HDR_1_MARK_0                                  16:16
#define LW_HDCP_PES_PRIVATE_HDR_1_RSVD                                     15:5
#define LW_HDCP_PES_PRIVATE_HDR_1_INP_0                                     4:1
#define LW_HDCP_PES_PRIVATE_HDR_1_MARK_1                                    0:0

#define LW_HDCP_PES_PRIVATE_HDR_2                                           0x3
#define LW_HDCP_PES_PRIVATE_HDR_2_INP_0                                   31:17
#define LW_HDCP_PES_PRIVATE_HDR_2_MARK_0                                  16:16
#define LW_HDCP_PES_PRIVATE_HDR_2_INP_1                                    15:3
#define LW_HDCP_PES_PRIVATE_HDR_2_INP_2                                     2:1
#define LW_HDCP_PES_PRIVATE_HDR_2_MARK_1                                    0:0

#define LW_HDCP_PES_PRIVATE_HDR_3                                           0x4
#define LW_HDCP_PES_PRIVATE_HDR_3_INP_0                                   31:17
#define LW_HDCP_PES_PRIVATE_HDR_3_MARK_0                                  16:16
#define LW_HDCP_PES_PRIVATE_HDR_3_INP_1                                    15:1
#define LW_HDCP_PES_PRIVATE_HDR_3_MARK_1                                    0:0

//
// Stream and Input Ctr mapping to PES private header
// _X_Y X->Index of 32bitDW in the above header format
// Y->Which instance within the DWORD
//
#define LW_HDCP_PES_MAP_STR_0_0                                           31:30
#define LW_HDCP_PES_MAP_STR_0_1                                           29:15
#define LW_HDCP_PES_MAP_STR_1_0                                           14:0

#define LW_HDCP_PES_MAP_INP_1_0                                 (63-32):(60-32)
#define LW_HDCP_PES_MAP_INP_2_0                                 (59-32):(45-32)
#define LW_HDCP_PES_MAP_INP_2_1                                       (44-32):0
#define LW_HDCP_PES_MAP_INP_2_2                                           31:30

#define LW_HDCP_PES_MAP_INP_3_0                                           29:15
#define LW_HDCP_PES_MAP_INP_3_1                                            14:0

#define TSEC_SRM_REVOCATION_CHECK                                             1

/* ------------------------- Prototypes ------------------------------------- */
static LwBool _hdcpMethodIsInitialized(LwU32 *retval, LwU32 sessionId)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "_hdcpMethodIsInitialized");
#if SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF)
static LwBool _hdcpIsTestReceiver(LwU8 *pRcvrId, LwU32 *pIdx)
    GCC_ATTRIB_SECTION("imem_hdcpMthdVerifyCertRx", "_hdcpIsTestReceiver");
#endif // SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF)
static FLCN_STATUS _hdcpChooseRandomNumber(HDCP_RANDOM_TYPE type, HDCP_SESSION *pSession, LwU32 *pRetval, LwU8 *pTmpBuf)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "_hdcpChooseRandomNum");
static FLCN_STATUS _hdcpCallwlateL(HDCP_SESSION *pSession, LwU8 *pLprime, LwU8 *pTmpBuf)
    GCC_ATTRIB_SECTION("imem_hdcpMthdGenerateLcInit", "_hdcpCallwlateL");
static FLCN_STATUS _hdcpFindFreeActiveRecord(LwU32 *pActRecIdx)
    GCC_ATTRIB_SECTION("imem_hdcpMthdSessionCtrl", "_hdcpFindFreeActiveRecord");
static FLCN_STATUS _hdcpActivateRecord(LwU32 sessionId, LwU32 actRecIdx)
    GCC_ATTRIB_SECTION("imem_hdcpMthdSessionCtrl", "_hdcpActivateRecord");
static FLCN_STATUS _hdcpGeneratePesHeader(LwU64 *pPesHdr, LwU32 sctr, LwU64 ictr)
    GCC_ATTRIB_SECTION("imem_hdcpMthdEncrypt", "_hdcpGeneratePesHeader");
static FLCN_STATUS _hdcpComputeV_2_0(HDCP_SESSION *pSession, hdcp_verify_vprime_param *pParam, LwU8 *pRcvrIdList, LwU32 rcvrIdListSize)
    GCC_ATTRIB_SECTION("imem_hdcpMthdVerifyVprime", "_hdcpComputV_2_0");
static FLCN_STATUS _hdcpComputeV_2_1(HDCP_SESSION *pSession, hdcp_verify_vprime_param *pParam, LwU8 *pRcvrIdList, LwU32 rcvrIdListSize)
    GCC_ATTRIB_SECTION("imem_hdcpMthdVerifyVprime", "_hdcpComputeV_2_1");
static FLCN_STATUS _hdcpStreamManage(hdcp_stream_manage_param *pParam, HDCP_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_hdcpMthdStreamManage", "_hdcpStreamManage");
static FLCN_STATUS _hdcpStreamReady(hdcp_stream_manage_param *pParam, HDCP_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_hdcpMthdStreamManage", "_hdcpStreamReady");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * Buffer used by the VERIFY_CERT_RX method to store the receiver's certificate
 * during validation.
 *
 * This buffer is stored in the DMEM overlay HDCP_MTHD_VERIFY_CERT_RX and needs
 * to be explicitly loaded before use.
 */
LwU8 hdcpCertificateRx[LW_ALIGN_UP(HDCP_SIZE_BKSV_LIST, 16)]
    GCC_ATTRIB_ALIGN(256)
    GCC_ATTRIB_SECTION("dmem_hdcpmc", "hdcpCertificateRx");

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Specifies whether the HDCP task has been fully initialized or not.
 *
 * Checks to if the following conditions have been met:
 * 1) Scratch buffer has been allocated and initialized.
 * 2) The session ID is valid.
 *
 * @param[out]  retval     Value to return to the client should one of the
 *                         conditions not be met.
 * @param[in]   sessionId  HDCP session ID.
 *
 * @return LW_TRUE if all the initialization conditions have been met; LW_FALSE
 *         if not.
 */
static LwBool
_hdcpMethodIsInitialized(LwU32 *retval, LwU32 sessionId)
{
    LwBool bPreCondMet = LW_FALSE;

    if (0 == hdcpGlobalData.sbOffset)
    {
        *retval = HDCP_GENERAL_ERROR(SB_NOT_SET);
    }
    else if (FLD_TEST_DRF(_HDCP, _SCRATCH_BUFFER_HEADER_FLAG, _INIT, _NOT_DONE,
                          hdcpGlobalData.header.flags))
    {
        *retval = HDCP_GENERAL_ERROR(NOT_INIT);
    }
    else if (HDCP_GET_SESSION_INDEX(sessionId) > HDCP_MAX_SESSIONS)
    {
        *retval = HDCP_GENERAL_ERROR(ILWALID_SESSION);
    }
    else
    {
        *retval = HDCP_GENERAL_ERROR(NONE);
        bPreCondMet = LW_TRUE;
    }

    return bPreCondMet;
}

#if SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF)
/*!
 * @brief Determines if the given receiver ID known and only for testing
 *        purposes.
 *
 * @return LW_TRUE if test ID; LW_FALSE otherwise.
 */
static LwBool
_hdcpIsTestReceiver
(
    LwU8  *pRcvrId,
    LwU32 *pIdx
)
{
    LwU32 i;

    if ((NULL == pRcvrId) || (NULL == pIdx))
    {
        return LW_FALSE;
    }

    for (i = 0; i < HDCP_TEST_RECEIVER_COUNT; i++)
    {
        if (0 == memcmp(pRcvrId, hdcpTestRcvrIds[i], HDCP_SIZE_RCVR_ID))
        {
            *pIdx = i;
            return LW_TRUE;
        }
    }
    return LW_FALSE;
}
#endif // SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF)

/*!
 * @brief Chooses the random number.
 *
 * @param[in]      type      Type specifier as to type of random number.
 * @param[in]      pSession  Session to generate the random number for.
 * @param[out]     pRetval   Buffer to store the randomly generated number.
 * @param[in/out]  pTmpBuf   A 256-byte aligned buffer used for data transfers.
 *
 * @return FLCN_OK if a valid type was provided; FLCN_ERR_ILWALID_ARGUMENT if
 *         an invalid type was provided; FLCN_ERROR if attempting to use test
 *         keys while not in debug mode.
 */
static FLCN_STATUS
_hdcpChooseRandomNumber
(
    HDCP_RANDOM_TYPE  type,
    HDCP_SESSION     *pSession,
    LwU32            *pRetval,
    LwU8             *pTmpBuf
)
{
    FLCN_STATUS  status = FLCN_OK;
    LwU32        size = 0;
    LwU8        *pDst;
#if SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF)
    LwU8        *pSrc;
#endif

    switch (type)
    {
        case HDCP_RANDOM_TYPE_RN:
        {
            pDst = (LwU8*)(&pSession->sessionState.rn);
            size = HDCP_SIZE_RN;
#if (SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF))
            pSrc = hdcpTestRn[pSession->testKeysIndex];
#endif
            break;
        }
        case HDCP_RANDOM_TYPE_KM:
        {
            pDst = (LwU8*)(&pSession->sessionState.km);
            size = HDCP_SIZE_KM;
#if (SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF))
            pSrc = hdcpTestKm[pSession->testKeysIndex];
#endif
            break;
        }
        case HDCP_RANDOM_TYPE_KS:
        {
            pDst = (LwU8*)(&pSession->encryptionState.ks);
            size = HDCP_SIZE_KS;
#if (SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF))
            pSrc = hdcpTestKs[pSession->testKeysIndex];
#endif
            break;
        }
        case HDCP_RANDOM_TYPE_RIV:
        {
            pDst = (LwU8*)(&pSession->encryptionState.riv);
            size = HDCP_SIZE_RIV;
#if (SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF))
            pSrc = hdcpTestRiv[pSession->testKeysIndex];
#endif
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto _hdcpChooseRandomNum_end;
            break;
        }
    }

    if (pSession->bIsUseTestKeys)
    {
#if (SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF))
        memcpy(pDst, pSrc, size);
#else
        *pRetval = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        status   = FLCN_ERROR;
#endif
    }
    else
    {
        memset(hdcpTempBuffer, 0, HDCP_SIZE_TEMP_BUFFER);

        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpRand));
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpRandHs));

        OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(hdcpRand), NULL, 0);
        hdcpRandomNumber(pTmpBuf, size);
        OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpRandHs));
        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpRand));

        memcpy(pDst, pTmpBuf, size);
    }

_hdcpChooseRandomNum_end:
    return status;
}

/*!
* @brief Callwlates the value of L.
*
* @param[in]   pSession  Pointer to the current session->
* @param[out]  pLprime   Pointer to buffer to store the value of L.
* @param[in]   pTmpBuf   Pointer to a 16-byte aligned buffer.
*
* @return FLCN_OK.
*/
static FLCN_STATUS
_hdcpCallwlateL
(
    HDCP_SESSION *pSession,
    LwU8         *pLprime,
    LwU8         *pTmpBuf
)
{
    LwU8  *pBuf;
    LwU64  hmacOperand = 0;

    pBuf = pTmpBuf;
    memcpy(pTmpBuf, pSession->sessionState.kd, HDCP_SIZE_KD);

    // rrx XORed with the least 64 bits (8 bytes) of Kd
    memcpy(&hmacOperand, &(pSession->sessionState.rrx), HDCP_SIZE_RRX);
    hmacOperand = hmacOperand ^
        (*(LwU64*)(pTmpBuf + HDCP_SIZE_KD - HDCP_SIZE_RRX));

    memcpy((pTmpBuf + HDCP_SIZE_KD - HDCP_SIZE_RRX), &hmacOperand,
           HDCP_SIZE_RRX);

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    hmacSha256(pLprime, (LwU8*)&pSession->sessionState.rn, HDCP_SIZE_RN,
               pTmpBuf, HDCP_SIZE_KD);
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));

    return FLCN_OK;
}

/*!
* @brief Finds a free active record.
*
* @param[out]  pActRecIdx  Returns the index into the active record table to
*                          be used.
*
* @return FLCN_OK if a free record is found; FLCN_ERROR otherwise.
*/
static FLCN_STATUS
_hdcpFindFreeActiveRecord
(
    LwU32 *pActRecIdx
)
{
    LwU32 i;

    for (i = 0; i < HDCP_POR_NUM_RECEIVERS; i++)
    {
        if (HDCP_SESSION_FREE == hdcpGlobalData.activeRecs[i].status)
        {
            *pActRecIdx = i;
            return FLCN_OK;
        }
    }

    return FLCN_ERROR;
}

/*!
* @brief Marks the session record as active.
*
* @param[in]  sessionId  Session ID that using the active record.
* @param[in]  actRecIdx  Index of the active record to modify.
*
* @return FLCN_OK if the session record is marked active; FLCN_ERROR if the
*         record is already active.
*/
static FLCN_STATUS
_hdcpActivateRecord
(
    LwU32 sessionId,
    LwU32 actRecIdx
)
{
    if ((HDCP_POR_NUM_RECEIVERS > actRecIdx) &&
        (HDCP_SESSION_FREE == hdcpGlobalData.activeRecs[actRecIdx].status))
    {
        hdcpGlobalData.activeRecs[actRecIdx].status = HDCP_SESSION_ACTIVE;
        hdcpGlobalData.activeRecs[actRecIdx].sessionId = sessionId;
        return FLCN_OK;
    }

    return FLCN_ERROR;
}

/*!
 * @brief Generate PES header according to HDCP standard.
 *
 * @param[out]  pPesHdr  Resulting PES header
 * @param[in]   sctr     Stream counter
 * @param[in]   ictr     Input counter
 *
 * @return SEC_OK.
 */
static FLCN_STATUS
_hdcpGeneratePesHeader
(
    LwU64 *pPesHdr,
    LwU32  sctr,
    LwU64  ictr
)
{
    LwU32 tmp = 0;
    LwU32 ctmp = 0;
    LwU32 ictr1 = LwU64_LO32(ictr);
    LwU32 ictr2 = LwU64_HI32(ictr);
    LwU32 *pPes = (LwU32*)pPesHdr;

    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_0, _RSVD, 0, tmp);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_0, _MARK_0, 1, tmp);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_0, _MARK_1, 1, tmp);
    ctmp = DRF_VAL(_HDCP, _PES_MAP, _STR_0_0, sctr);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_0, _STR_0, ctmp, tmp);
    ctmp = DRF_VAL(_HDCP, _PES_MAP, _STR_0_1, sctr);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_0, _STR_1, ctmp, tmp);
    pPes[0] = tmp;

    tmp = 0;
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_1, _RSVD, 0, tmp);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_1, _MARK_0, 1, tmp);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_1, _MARK_1, 1, tmp);
    ctmp = DRF_VAL(_HDCP, _PES_MAP, _STR_1_0, sctr);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_1, _STR_0, ctmp, tmp);
    ctmp = DRF_VAL(_HDCP, _PES_MAP, _INP_1_0, ictr2);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_1, _INP_0, ctmp, tmp);
    pPes[1] = tmp;

    tmp = 0;
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_2, _MARK_0, 1, tmp);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_2, _MARK_1, 1, tmp);
    ctmp = DRF_VAL(_HDCP, _PES_MAP, _INP_2_0, ictr2);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_2, _INP_0, ctmp, tmp);
    ctmp = DRF_VAL(_HDCP, _PES_MAP, _INP_2_1, ictr2);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_2, _INP_1, ctmp, tmp);
    ctmp = DRF_VAL(_HDCP, _PES_MAP, _INP_2_2, ictr1);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_2, _INP_2, ctmp, tmp);
    pPes[2] = tmp;

    tmp = 0;
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_3, _MARK_0, 1, tmp);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_3, _MARK_1, 1, tmp);
    ctmp = DRF_VAL(_HDCP, _PES_MAP, _INP_3_0, ictr1);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_3, _INP_0, ctmp, tmp);
    ctmp = DRF_VAL(_HDCP, _PES_MAP, _INP_3_1, ictr1);
    tmp = FLD_SET_DRF_NUM(_HDCP, _PES_PRIVATE_HDR_3, _INP_1, ctmp, tmp);
    pPes[3] = tmp;

    hdcpSwapEndianness((LwU8*)&(pPes[0]), (LwU8*)&(pPes[0]), 4);
    hdcpSwapEndianness((LwU8*)&(pPes[1]), (LwU8*)&(pPes[1]), 4);
    hdcpSwapEndianness((LwU8*)&(pPes[2]), (LwU8*)&(pPes[2]), 4);
    hdcpSwapEndianness((LwU8*)&(pPes[3]), (LwU8*)&(pPes[3]), 4);

    return FLCN_OK;
}

/*!
 * @brief Computes the value of V based on the HDCP 2.0 specification.
 *
 * @param[in]  pSession        Pointer to the HDCP session.
 * @param[in]  pParam          Pointer to the parameter, which contains V'.
 * @param[in]  pRcvrIdList     Pointer to the receiver ID list.
 * @param[in]  rcvrIdListSize  Size of the receiver ID list.
 *
 * @return FLCN_OK if V matches V'; FLCN_ERROR if not.
 */
static FLCN_STATUS
_hdcpComputeV_2_0
(
    HDCP_SESSION             *pSession,
    hdcp_verify_vprime_param *pParam,
    LwU8                     *pRcvrIdList,
    LwU32                     rcvrIdListSize
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        hashV[HDCP_SIZE_VPRIME_2X];

    // Build up rcvrIdList (hdcpBksvList) before applying SHA256
    memcpy(pRcvrIdList + rcvrIdListSize, &pParam->depth, sizeof(LwU8));
    memcpy(pRcvrIdList + rcvrIdListSize + sizeof(LwU8),
           &pParam->deviceCount, sizeof(LwU8));

    // Get hash
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    hmacSha256(hashV, pRcvrIdList, rcvrIdListSize + sizeof(LwU8) * 4,
        (LwU8*)pSession->sessionState.kd, HDCP_SIZE_KD);
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));

    if (0 != memcmp(hashV, pParam->Vprime, HDCP_SIZE_VPRIME_2X))
    {
        status = FLCN_ERROR;
        pParam->retCode = HDCP_METHOD_ERROR(VERIFY_VPRIME, VPRIME_VALD_FAILED);
    }

    return status;
}

/*!
 * @brief Computes the value of V based on the HDCP 2.0 specification.
 *
 * @param[in]  pSession        Pointer to the HDCP session.
 * @param[in]  pParam          Pointer to the parameter, which contains V'.
 * @param[in]  pRcvrIdList     Pointer to the receiver ID list.
 * @param[in]  rcvrIdListSize  Size of the receiver ID list.
 *
 * @return FLCN_OK if V matches V'; FLCN_ERROR if not.
 */
static FLCN_STATUS
_hdcpComputeV_2_1
(
    HDCP_SESSION             *pSession,
    hdcp_verify_vprime_param *pParam,
    LwU8                     *pRcvrIdList,
    LwU32                     rcvrIdListSize
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        hashV[HDCP_SIZE_VPRIME_2X];
    LwU32       seqNumV;

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    if (0 != pSession->displayType)
    {
        //
        // Build up pRcvrIdList (hdcpBksvList) before applying SHA256.
        // v' = pRcvrIdList || RxInfo || seq_num_v
        //
        memcpy(pRcvrIdList + rcvrIdListSize, &pParam->RxInfo,
               HDCP_SIZE_HDMI_22_RXINFO);
        memcpy(pRcvrIdList + rcvrIdListSize + HDCP_SIZE_HDMI_22_RXINFO,
               pParam->seqNumV, HDCP_SIZE_SEQ_NUM_V);

        // Get hash
        hmacSha256(hashV, pRcvrIdList,
                   rcvrIdListSize + HDCP_SIZE_HDMI_22_RXINFO +
                   HDCP_SIZE_SEQ_NUM_V,
                   (LwU8*)&pSession->sessionState.kd, HDCP_SIZE_KD);
    }
    else
    {
        //
        // Build up pRcvrIdList (hdcpBksvList) before applying SHA256.
        // v' = pRcvrIdList || depth || device_count || MAX_DEVS_EXCEEDED ||
        //      MAX_CASCADES_EXCEEDED || HDCP2_0_REPEATER_DOWNSTREAM ||
        //      HDCP1_DEVICE_DOWNSTREAM || seq_num_v
        //
        memcpy(pRcvrIdList + rcvrIdListSize, &pParam->depth,
               sizeof(LwU8));
        memcpy(pRcvrIdList + rcvrIdListSize + sizeof(LwU8),
               &pParam->deviceCount, sizeof(LwU8));

        // Add in space for MAX_DEVS_EXCEEDED and MAX_CASCADES_EXCEEDED.
        memset(pRcvrIdList + rcvrIdListSize + (sizeof(LwU8) * 2),
               0, sizeof(LwU8));
        memset(pRcvrIdList + rcvrIdListSize + (sizeof(LwU8) * 3),
               0, sizeof(LwU8));
        memcpy(pRcvrIdList + rcvrIdListSize + (sizeof(LwU8) * 4),
               &pParam->bHasHdcp20Repeater, sizeof(LwU8));
        memcpy(pRcvrIdList + rcvrIdListSize + (sizeof(LwU8) * 5),
               &pParam->bHasHdcp1xDevice, sizeof(LwU8));
        memcpy(pRcvrIdList + rcvrIdListSize + (sizeof(LwU8) * 6),
               &pParam->seqNumV, HDCP_SIZE_SEQ_NUM_V);

        // Get hash
        hmacSha256(hashV, pRcvrIdList,
                   rcvrIdListSize + (sizeof(LwU8) * 6) + HDCP_SIZE_SEQ_NUM_V,
                   (LwU8*)&pSession->sessionState.kd, HDCP_SIZE_KD);
    }
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));

    if (0 != memcmp(hashV, pParam->Vprime, HDCP_SIZE_VPRIME_2X / 2))
    {
        status = FLCN_ERROR;
        pParam->retCode = HDCP_METHOD_ERROR(VERIFY_VPRIME, VPRIME_VALD_FAILED);
    }
    else
    {
        // If V' passes, copy the 128 LSB to return.
        memcpy(pParam->V128l, hashV + (HDCP_SIZE_VPRIME_2X / 2),
               HDCP_SIZE_VPRIME_2X / 2);
    }

    // Check seq_num_v
    memcpy(&seqNumV, pParam->seqNumV, HDCP_SIZE_SEQ_NUM_V);
    if (FLCN_OK == status)
    {
        if (0 != pSession->seqNumRollover)
        {
            pParam->retCode = HDCP_METHOD_ERROR(VERIFY_VPRIME, SEQ_NUM_V_ROLLOVER);
        }
        else if (HDCP_SEQ_NUM_V_MAX == seqNumV)
        {
            pSession->seqNumRollover = 1;
        }
    }

    return status;
}

/*!
 * @brief Handles stream management in the STREAM_MANAGE method.
 *
 * @param[out]  pParam    Pointer to the client structure.
 * @param[in]   pSession  Pointer to the current session->
 *
 * @return FLCN_OK if no rollover oclwrred of the sequence number. FLCN_ERROR
 *         should a rollover happen. Additionally, pParam->retCode will be set
 *         to LW95A1_HDCP_STREAM_MANAGE_ERROR_SEQ_NUM_M_ROLLOVER.
 */
static FLCN_STATUS
_hdcpStreamManage
(
    hdcp_stream_manage_param *pParam,
    HDCP_SESSION             *pSession
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       seqM = 0;
    LwU32       i;

    if (LW_U32_MAX == pSession->seqNumM)
    {
        pParam->retCode = HDCP_METHOD_ERROR(STREAM_MANAGE, SEQ_NUM_M_ROLLOVER);
        status = FLCN_ERROR;
        goto _hdcpStreamManage_end;
    }

    seqM = pSession->seqNumM;
    hdcpSwapEndianness(&seqM, &seqM, HDCP_SIZE_SEQ_NUM_M);
    memcpy(pParam->seqNumM, &seqM, HDCP_SIZE_SEQ_NUM_M);

    for (i = 0; i < pSession->encryptionState.numStreams; i++)
    {
        pParam->streamCtr[i] = pSession->encryptionState.streamCtr[i];
    }

    pSession->seqNumM++;

_hdcpStreamManage_end:
    return status;
}

/*!
 * @brief Handles stream ready in STREAM_MANAGE method.
 *
 * @param[in]  pParam    Method parameters.
 * @param[in]  pSession  Session managing the stream.
 */
static FLCN_STATUS
_hdcpStreamReady
(
    hdcp_stream_manage_param *pParam,
    HDCP_SESSION             *pSession
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       temp = 0;
    LwU32       i = 0;
    LwU32       m = 0;
    LwU8        kdHash[SHA256_HASH_BLOCK_SIZE];
    LwU8        mMessage[HDCP_SIZE_MPRIME_DATA];
    LwU8        M[HDCP_SIZE_MPRIME];

    for (i = 0; i < pSession->encryptionState.numStreams; i++)
    {
        if (pSession->displayType != 0)
        {
            // HDMI or DP HDCP 2.2
            memcpy(&(mMessage[m]), &pParam->streamIdType,
                   HDCP_SIZE_STREAM_ID_TYPE);
            m += HDCP_SIZE_STREAM_ID_TYPE;
        }
        else
        {
            temp = pSession->encryptionState.streamCtr[i];
            hdcpSwapEndianness((LwU8*)&temp, (LwU8*)&temp, HDCP_SIZE_STREAM_CTR);
            memcpy(&(mMessage[m]), (LwU8*)&temp, HDCP_SIZE_STREAM_CTR);
            m += HDCP_SIZE_STREAM_CTR;
            memcpy(&(mMessage[m]), pParam->contentID[i],
                   HDCP_SIZE_CONTENT_ID);
            m += HDCP_SIZE_CONTENT_ID;
            memcpy(&(mMessage[m]), pParam->strType[i],
                   HDCP_SIZE_CONTENT_TYPE);
            m += HDCP_SIZE_CONTENT_TYPE;
        }
    }

    //
    // If seqNumM == 0, it means that _hdcpStreamManage() has not bee called.
    // There is no harm in generating an invalid M.
    //
    temp = pSession->seqNumM - 1;
    hdcpSwapEndianness(&temp, &temp, HDCP_SIZE_SEQ_NUM_M);
    memcpy(&(mMessage[m]), &temp, HDCP_SIZE_SEQ_NUM_M);

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    sha256(kdHash, (LwU8*)&pSession->sessionState.kd, HDCP_SIZE_KD);
    if (pSession->displayType != 0)
    {
        hmacSha256(M, mMessage, HDCP_SIZE_STREAM_ID_TYPE *
                   pSession->encryptionState.numStreams +
                   HDCP_SIZE_SEQ_NUM_M, kdHash, SHA256_HASH_BLOCK_SIZE);
    }
    else
    {
        hmacSha256(M, mMessage, HDCP_SIZE_MPRIME_DATA, kdHash,
                   SHA256_HASH_BLOCK_SIZE);
    }
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));

    if (0 != memcmp(M, pParam->mprime, HDCP_SIZE_MPRIME))
    {
        status = FLCN_ERROR;
    }

    return status;
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Handler for LW95A1_HDCP_READ_CAPS method.
 *
 * @param [out] pParam
 *      Pointer to the method's parameters. This structure contains SEC2's HDCP
 *      capabilities back to the client.
 */
void
hdcpMethodHandleReadCaps
(
    hdcp_read_caps_param *pParam
)
{
    pParam->lwrrentBuildMode = HDCP_READ_CAPS_LWRRENT_BUILD_MODE;

    pParam->bIsDebugChip = Sec2.bDebugMode;
    pParam->falconIPVer  = falc_ldx_i((LwU32*)LW_CSEC_VERSION, 0);

    pParam->supportedVersionsMask      = BIT(LW95A1_HDCP_VERSION_20) |
                                         BIT(LW95A1_HDCP_VERSION_21) |
                                         BIT(LW95A1_HDCP_VERSION_22) ;
    pParam->maxSessions                 = HDCP_MAX_SESSIONS;
    pParam->maxActiveSessions           = HDCP_POR_NUM_RECEIVERS;
    pParam->maxStreamsPerReceiver       = HDCP_MAX_STREAMS_PER_RECEIVER;
    pParam->scratchBufferSize           = HDCP_SB_HEADER_SIZE + HDCP_SB_KEYS_BLOCK_SIZE
                                         + (HDCP_MAX_SESSIONS * HDCP_SB_SESSION_SIZE);
    pParam->bIsExclusiveDmemAvailable   = LW95A1_HDCP_READ_CAPS_EXCL_DMEM_UNAVAILABLE;
    pParam->bIsStatusSigningSupported   = LW95A1_HDCP_READ_CAPS_STATUS_SIGNING_UNSUPPORTED;
    pParam->bIsStreamValSupported       = LW95A1_HDCP_READ_CAPS_STREAM_VAL_SUPPORTED;
    pParam->bIsKsvListValSupported      = LW95A1_HDCP_READ_CAPS_KSVLIST_VAL_SUPPORTED;
    pParam->bIsPreComputeSupported      = LW95A1_HDCP_READ_CAPS_PRE_COMPUTE_SUPPORTED;
    pParam->bIsRcvSupported             = LW95A1_HDCP_READ_CAPS_RCV_SUPPORTED;
    pParam->retCode                     = LW95A1_HDCP_READ_CAPS_ERROR_NONE;
}

/*!
 * @brief Handler for LW95A1_HDCP_INIT method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleInit
(
    hdcp_init_param *pParam
)
{
    HDCP_SESSION *pSession = (HDCP_SESSION*)hdcpSessionBuffer;
    FLCN_STATUS   status   = FLCN_OK;
    LwU32         i;

    if (0 != hdcpGlobalData.sbOffset)
    {
        // Initialize chip ID
        hdcpGlobalData.chipId = pParam->chipId;

        // Initialize header
        hdcpGlobalData.header.version          = 0x1;
        hdcpGlobalData.header.numSessionsInUse = 0x0;
        hdcpGlobalData.header.flags            = 0x0;
        hdcpGlobalData.header.sessionIdSeqCnt  = 0x0;
        hdcpGlobalData.header.streamCtr        = 0x0;
        memset(hdcpGlobalData.header.sessionStatusMask, 0,
               HDCP_SIZE_SESSION_STATUS_MASK);

        // Generate a random number for session IDs
        memset(hdcpTempBuffer, 0, HDCP_SIZE_TEMP_BUFFER);
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpRand));
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpRandHs));

        OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(hdcpRand), NULL, 0);
        hdcpRandomNumber(hdcpTempBuffer, 16);
        OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpRandHs));
        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpRand));

        memcpy((LwU8*)&hdcpGlobalData.header.sessionIdSeqCnt, hdcpTempBuffer,
               sizeof(LwU32));
        memcpy(hdcpGlobalData.randNum, hdcpTempBuffer,
               sizeof(hdcpGlobalData.randNum));

        // Supported versions
        hdcpGlobalData.supportedHdcpVersionsMask = 0;
        hdcpGlobalData.supportedHdcpVersionsMask |= BIT(LW95A1_HDCP_VERSION_20);
        hdcpGlobalData.supportedHdcpVersionsMask |= BIT(LW95A1_HDCP_VERSION_21);
        hdcpGlobalData.supportedHdcpVersionsMask |= BIT(LW95A1_HDCP_VERSION_22);

        // Setup active sessions
        memset((LwU8*)hdcpGlobalData.activeRecs, 0,
               HDCP_POR_NUM_RECEIVERS * sizeof(HDCP_SESSION_ACTIVE_REC));
        memset((LwU8*)hdcpGlobalData.activeSessions, 0,
               HDCP_POR_NUM_RECEIVERS * sizeof(HDCP_SESSION_ACTIVE_ENCRYPTION));

        for (i = 0; i < HDCP_POR_NUM_RECEIVERS; i++)
        {
            status = hdcpSessionCryptActive(i, LW_TRUE);
            if (FLCN_OK != status)
            {
                pParam->retCode = HDCP_METHOD_ERROR(INIT, ILWALID_KEYS);
                return;
            }
        }

        // Initialize session data
        memset(pSession, 0, sizeof(HDCP_SESSION));
        for (i = 0; i < HDCP_MAX_SESSIONS; i++)
        {
            pSession->id = i;
            pSession->status = HDCP_SESSION_FREE;
            hdcpSessionWrite(pSession);
        }

        // Set init flag in global data
        hdcpGlobalData.header.flags =
            FLD_SET_DRF(_HDCP, _SCRATCH_BUFFER_HEADER_FLAG, _INIT, _DONE,
                        hdcpGlobalData.header.flags);
        pParam->retCode = HDCP_METHOD_ERROR(INIT, NONE);
    }
    else
    {
        hdcpGlobalData.header.flags =
            FLD_SET_DRF(_HDCP, _SCRATCH_BUFFER_HEADER_FLAG, _INIT, _NOT_DONE,
                        hdcpGlobalData.header.flags);
        pParam->retCode = HDCP_METHOD_ERROR(INIT, SB_NOT_SET);
    }
}

/*!
 * @brief Handler for LW95A1_HDCP_CREATE_SESSION method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleCreateSession
(
    hdcp_create_session_param *pParam
)
{
    FLCN_STATUS    status    = FLCN_OK;
    LwU32          sessionId = 0;
    HDCP_SESSION  *pSession  = NULL;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), 0))
    {
        return;
    }

    if (0 == pParam->noOfStreams)
    {
        pParam->retCode = HDCP_METHOD_ERROR(CREATE_SESSION, ILWALID_STREAMS);
        return;
    }

    // Check for max streams
    if (pParam->noOfStreams > HDCP_MAX_STREAMS_PER_RECEIVER)
    {
        pParam->retCode = HDCP_METHOD_ERROR(CREATE_SESSION, ILWALID_STREAMS);
        return;
    }

    // Check for number of sessions supported
    if (hdcpGlobalData.header.numSessionsInUse >= HDCP_MAX_SESSIONS)
    {
        pParam->retCode = HDCP_METHOD_ERROR(CREATE_SESSION, MAX);
        return;
    }

    if (hdcpSessionFindFree(&sessionId))
    {
        status = hdcpSessionReadById(&pSession, sessionId);
        if (FLCN_OK != status)
        {
            pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
            return;
        }

        if ((pSession->status == HDCP_SESSION_FREE) &&
            ((pSession->id & 0x0000FFFF) == sessionId))
        {
            memset(pSession, 0, sizeof(HDCP_SESSION));

            hdcpSessionSetStatus(sessionId, LW_TRUE);
            hdcpGlobalData.header.numSessionsInUse++;
            hdcpGlobalData.header.sessionIdSeqCnt++;

            pSession->id          = (hdcpGlobalData.header.sessionIdSeqCnt << 16) |
                                    sessionId;
            pSession->status      = HDCP_SESSION_IN_USE;
            pSession->sessionType = pParam->sessionType;
            pSession->displayType = pParam->displayType;

            if (LW95A1_HDCP_CREATE_SESSION_TYPE_TMTR == pParam->sessionType)
            {
                //
                // Generate rtx. In case of test receiver, verify cert will
                // take care of updating rtx in the session details.
                //
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpRand));
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpRandHs));

                OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(hdcpRand), NULL, 0);
                hdcpRandomNumber(hdcpTempBuffer, ALIGN_UP(HDCP_SIZE_RTX, 16));
                OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpRandHs));
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpRand));

                memcpy((LwU8*)&(pSession->sessionState.rtx), hdcpTempBuffer,
                       HDCP_SIZE_RTX);

                pSession->transmitterInfo.version = HDCP_TRANSMITTER_VERSION;
                if (0 != pSession->displayType)
                {
                    pSession->transmitterInfo.capabilitiesMask =
                        HDCP_TRANSMITTER_CAPABILITIES_PRECOMPUTE_SUPPORT_NO;
                }
                else
                {
                    pSession->transmitterInfo.capabilitiesMask =
                        HDCP_TRANSMITTER_CAPABILITIES_PRECOMPUTE_SUPPORT_YES;
                }
                memcpy(&(pParam->rtx), &(pSession->sessionState.rtx),
                       HDCP_SIZE_RTX);
            }

            pSession->stage = HDCP_AUTH_STAGE_AKE_INIT;
            pSession->encryptionState.numStreams = pParam->noOfStreams;

            // Response.
            pParam->sessionID = pSession->id;

            status = hdcpSessionWrite(pSession);
            if (FLCN_OK != status)
            {
                return;
            }
        }
        else
        {
            pParam->retCode = HDCP_METHOD_ERROR(CREATE_SESSION, MAX);
        }
    }
    else
    {
        pParam->retCode = HDCP_METHOD_ERROR(CREATE_SESSION, MAX);
    }
}

/*!
 * @brief Handler for LW95A1_HDCP_VERIFY_CERT_RX method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleVerifyCertRx
(
    hdcp_verify_cert_rx_param *pParam
)
{
    FLCN_STATUS   status            = FLCN_OK;
    HDCP_SESSION *pSession          = NULL;
    HDCP_CERTIFICATE *certificate   = (HDCP_CERTIFICATE*)hdcpCertificateRx;
    LwU32         certificateOffset = 0;
    LwU32         dcpExponent       = 0;
    LwBool        bValidCert        = LW_FALSE;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (HDCP_AUTH_STAGE_AKE_INIT != pSession->stage)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_STAGE);
        return;
    }

    // Verify certificate offset and read certificate.
    certificateOffset = HDCP_GET_METHOD_PARAM(HDCP_METHOD_ID(SET_CERT_RX));
    if (0 == certificateOffset)
    {
        pParam->retCode = HDCP_METHOD_ERROR(VERIFY_CERT_RX, CERT_NOT_SET);
        return;
    }

    status = hdcpDmaRead(certificate, certificateOffset,
                         LW_ALIGN_UP(LW95A1_HDCP_SIZE_CERT_RX_8, 16));
    if (FLCN_OK != status)
    {
        return;
    }

    // Get KpubDcp
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpDcpKey));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));

    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(hdcpDcpKey), NULL, 0);
    status = hdcpGetDcpKey();
    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpDcpKey));

    memcpy(&dcpExponent, hdcpKpubDcp + HDCP_SIZE_DCP_KPUB, sizeof(LwU32));
    if (0 == dcpExponent)
    {
        pParam->retCode = HDCP_METHOD_ERROR(VERIFY_CERT_RX, DCP_KPUB_ILWALID);
        return;
    }

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpRsa));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libBigInt));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));

    bValidCert = hdcpVerifyCertificate(hdcpKpubDcp, dcpExponent,
                                       certificate, NULL);

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libBigInt));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpRsa));

    if (!bValidCert)
    {
        pParam->retCode = HDCP_METHOD_ERROR(VERIFY_CERT_RX, ILWALID_CERT);
        return;
    }
    else
    {
        memcpy(&pSession->modulus, &(certificate->modulus), HDCP_SIZE_RX_MODULUS);
        memcpy(&pSession->exponent, &(certificate->exponent), HDCP_SIZE_RX_EXPONENT);
        memcpy(&pSession->receiverId, &(certificate->id), HDCP_SIZE_RCVR_ID);
        pSession->rfu1 = certificate->rfu;

#if (SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF))
        {
            LwU32 testRcvrIdx = 1;

            if (_hdcpIsTestReceiver(pSession->receiverId, &testRcvrIdx))
            {
                pSession->bIsUseTestKeys = LW_TRUE;
                pSession->testKeysIndex  = testRcvrIdx;

                // Replace rtx with test rtx
                memcpy(&pSession->sessionState.rtx, hdcpTestRtx[testRcvrIdx],
                       HDCP_SIZE_RTX);
            }
            else
            {
                pParam->retCode = HDCP_METHOD_ERROR(VERIFY_CERT_RX, ILWALID_CERT);
                return;
            }
        }
#endif // SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF)

        pSession->stage     = HDCP_AUTH_STAGE_VERIFY_CERT;
        pSession->bRepeater = pParam->repeater;
        status = hdcpSessionWrite(pSession);
    }
}

/*!
 * @brief Handler for LW95A1_HDCP_GENERATE_EKM method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleGenerateEkm
(
    hdcp_generate_ekm_param *pParam
)
{
    FLCN_STATUS   status     = FLCN_OK;
    HDCP_SESSION *pSession   = NULL;
    LwBool        bEncrypted = LW_FALSE;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (HDCP_AUTH_STAGE_VERIFY_CERT != pSession->stage)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_STAGE);
        return;
    }

    // Choose random number
    status = _hdcpChooseRandomNumber(HDCP_RANDOM_TYPE_KM, pSession,
                                     &(pParam->retCode), hdcpTempBuffer);
    if (FLCN_OK != status)
    {
        return;
    }

    // Encrypt using session's mod/exponent
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpRsa));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libBigInt));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));

    bEncrypted = hdcpRsaOaep((LwU8*)pParam->eKm, (LwU8*)pSession->sessionState.km,
                             pSession->modulus, pSession->exponent);

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libBigInt));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpRsa));

    if (!bEncrypted)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(UNKNOWN);
        return;
    }

    pSession->stage = HDCP_AUTH_STAGE_GENERATE_EKM;
    status          = hdcpSessionWrite(pSession);
}

/*!
 * @brief Handler for LW95A1_HDCP_VERIFY_HPRIME method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleVerifyHprime
(
    hdcp_verify_hprime_param *pParam
)
{
    FLCN_STATUS   status   = FLCN_OK;
    HDCP_SESSION *pSession = NULL;
    LwU8         *pKd      = NULL;
    LwU8         *pTmpBuf  = NULL;
    LwU8          rtxLsb;
    LwU8          msg[HDCP_SIZE_RTX + 6];

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (HDCP_AUTH_STAGE_GENERATE_EKM != pSession->stage)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_STAGE);
        return;
    }

    pKd     = hdcpTempBuffer;
    pTmpBuf = (LwU8*)(((LwU32)hdcpTempBuffer) + HDCP_SIZE_KD);

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpDkey));
    status = hdcpGetDkey(pSession, pKd, pTmpBuf);
    if (FLCN_OK != status)
    {
        return;
    }

    status = hdcpGetDkey(pSession, pKd + HDCP_SIZE_DKEY, pTmpBuf);
    if (FLCN_OK != status)
    {
        return;
    }
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpDkey));

    memcpy(pSession->sessionState.kd, pKd, HDCP_SIZE_KD);

    memset(msg, 0, sizeof(msg));
    memcpy(msg, &(pSession->sessionState.rtx), HDCP_SIZE_RTX);

    rtxLsb = msg[HDCP_SIZE_RTX - 1];
    if (0 == pSession->displayType)
    {
        msg[HDCP_SIZE_RTX - 1] = rtxLsb ^ pSession->bRepeater;
    }

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    if ((FLD_TEST_DRF(_HDCP, _SESSION, _RFU_PD, _VAL_1, pSession->rfu1)) ||
        (0 != pSession->displayType))
    {
        // If protocol descriptor is 1...
        msg[HDCP_SIZE_RTX] = pSession->receiverInfo.version;
        memcpy(&msg[HDCP_SIZE_RTX + 1],
               &(pSession->receiverInfo.capabilitiesMask),
               sizeof(pSession->receiverInfo.capabilitiesMask));
        msg[HDCP_SIZE_RTX + 3] = pSession->transmitterInfo.version;
        memcpy(&msg[HDCP_SIZE_RTX + 4],
               &(pSession->transmitterInfo.capabilitiesMask),
               sizeof(pSession->transmitterInfo.capabilitiesMask));
        hmacSha256(pTmpBuf, msg, sizeof(msg), pKd, HDCP_SIZE_KD);
    }
    else
    {
        hmacSha256(pTmpBuf, msg, HDCP_SIZE_RTX, pKd, HDCP_SIZE_KD);
    }
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));

    if (0 != memcmp(pTmpBuf, pParam->hprime, HDCP_SIZE_HPRIME))
    {
        pParam->retCode = HDCP_METHOD_ERROR(VERIFY_HPRIME, HPRIME_VALD_FAILED);
        return;
    }

    pSession->stage = HDCP_AUTH_STAGE_VERIFY_HPRIME;
    status = hdcpSessionWrite(pSession);
}

/*!
 * @brief Handler for LW95A1_HDCP_GENERATE_LC_INIT method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleGenerateLcInit
(
    hdcp_generate_lc_init_param *pParam
)
{
    FLCN_STATUS   status = FLCN_OK;
    HDCP_SESSION *pSession = NULL;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (LW95A1_HDCP_CREATE_SESSION_TYPE_TMTR == pSession->sessionType)
    {
        status = _hdcpChooseRandomNumber(HDCP_RANDOM_TYPE_RN, pSession,
                                         &(pParam->retCode), hdcpTempBuffer);
        if (FLCN_OK != status)
        {
            return;
        }
        memcpy(&(pParam->rn), &(pSession->sessionState.rn), HDCP_SIZE_RN);
    }
    else
    {
        memcpy(&(pSession->sessionState.rn), &(pParam->rn), HDCP_SIZE_RN);
        pSession->bIsRevocationCheckDone = LW_TRUE;
    }

    _hdcpCallwlateL(pSession, (LwU8*)pSession->L, hdcpTempBuffer);

    pSession->stage = HDCP_AUTH_STAGE_GENERATE_LC_INIT;
    status = hdcpSessionWrite(pSession);
}

/*!
 * @brief Handler for LW95A1_HDCP_UPDATE_SESSION method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleUpdateSession
(
    hdcp_update_session_param *pParam
)
{
    FLCN_STATUS   status = FLCN_OK;
    HDCP_SESSION *pSession = NULL;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if ((BIT(LW95A1_HDCP_UPDATE_SESSION_MASK_HEAD_PRESENT) & pParam->updmask) != 0)
    {
        // TODO: Update head (from original code)
    }

    if ((BIT(LW95A1_HDCP_UPDATE_SESSION_MASK_ORINDEX_PRESENT) & pParam->updmask) != 0)
    {
        // TODO: Update orIndex (from original code)
    }

    if ((BIT(LW95A1_HDCP_UPDATE_SESSION_MASK_RRX_PRESENT) & pParam->updmask) != 0)
    {
        memcpy(&pSession->sessionState.rrx, &pParam->rrx, HDCP_SIZE_RRX);
        pParam->updmask &= ~BIT(LW95A1_HDCP_UPDATE_SESSION_MASK_RRX_PRESENT);
    }

    if ((BIT(LW95A1_HDCP_UPDATE_SESSION_MASK_RTX_PRESENT) & pParam->updmask) != 0)
    {
        memcpy(&pSession->sessionState.rtx, &pParam->rtx, HDCP_SIZE_RTX);
        pParam->updmask &= ~BIT(LW95A1_HDCP_UPDATE_SESSION_MASK_RTX_PRESENT);
    }

    if ((BIT(LW95A1_HDCP_UPDATE_SESSION_MASK_VERSION_PRESENT) & pParam->updmask) != 0)
    {
        if ((BIT(pParam->hdcpVer) & hdcpGlobalData.supportedHdcpVersionsMask) == 0)
        {
            pParam->retCode =
                HDCP_METHOD_ERROR(UPDATE_SESSION, HDCP_VER_UNSUPPORTED);
            return;
        }

        pSession->version = pParam->hdcpVer;

        //
        // Update the receiver's version. The value we get from the client is:
        //   HDCP 2.0 = LW95A1_HDCP_VERSION_20 (2)
        //   HDCP 2.1 = LW95A1_HDCP_VERSION_21 (3)
        //   HDCP 2.2 = LW95A1_HDCP_VERSION_22 (4)
        //
        // They are stored in the receiver as the following values:
        //   HDCP 2.0 = 0, HDCP 2.1 = 1, HDCP 2.2 = 2
        //
        pSession->receiverInfo.version = pParam->hdcpVer - 2;
        pParam->updmask &= ~BIT(LW95A1_HDCP_UPDATE_SESSION_MASK_VERSION_PRESENT);
    }

    if ((BIT(LW95A1_HDCP_UPDATE_SESSION_MASK_PRE_COMPUTE_PRESENT) & pParam->updmask) != 0)
    {
        pSession->bIsPreComputeSupported = pParam->bRecvPreComputeSupport;

        //
        // Need to update the receiver's capability mask or change
        // hdcpHandleMethodGetRttChallenge to (pSession->bIsPreComputeSupported).
        // Only HDCP 2.1 specification support pre-compute.
        //
        pSession->receiverInfo.capabilitiesMask |=
            HDCP_RECEIVER_CAPABILITIES_PRECOMPUTE_SUPPORT_YES;
        pParam->updmask &=
            ~BIT(LW95A1_HDCP_UPDATE_SESSION_MASK_PRE_COMPUTE_PRESENT);
    }

    if (0 != pParam->updmask)
    {
        pParam->retCode = HDCP_METHOD_ERROR(UPDATE_SESSION, ILWALID_UPD_MASK);
    }

    hdcpSessionWrite(pSession);
}

/*!
 * @brief Handler for LW95A1_HDCP_VERIFY_LPRIME method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleVerifyLprime
(
    hdcp_verify_lprime_param *pParam
)
{
    FLCN_STATUS   status = FLCN_OK;
    HDCP_SESSION *pSession = NULL;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (HDCP_AUTH_STAGE_GENERATE_LC_INIT == pSession->stage)
    {
        if ((pSession->receiverInfo.capabilitiesMask & 0x1) !=
            HDCP_RECEIVER_CAPABILITIES_PRECOMPUTE_SUPPORT_YES)
        {
            pParam->retCode = HDCP_METHOD_ERROR(VERIFY_LPRIME, ILWALID_STAGE);
            return;
        }

        if (0 != memcmp(pSession->L, pParam->lprime, HDCP_SIZE_LPRIME))
        {
            pParam->retCode = HDCP_METHOD_ERROR(VERIFY_LPRIME, LPRIME_VALD_FAILED);
            return;
        }
    }
    else if (HDCP_AUTH_STAGE_GET_RTT_CHALLENGE == pSession->stage)
    {
        if (0 != memcmp(&pSession->L[HDCP_SIZE_LwU64(HDCP_SIZE_LPRIME) / 2],
                        pParam->lprime, HDCP_SIZE_LPRIME / 2))
        {
            pParam->retCode = HDCP_METHOD_ERROR(VERIFY_LPRIME, LPRIME_VALD_FAILED);
            return;
        }
    }
    else
    {
        pParam->retCode = HDCP_METHOD_ERROR(VERIFY_LPRIME, ILWALID_STAGE);
        return;
    }

    pSession->stage = HDCP_AUTH_STAGE_VERIFY_LPRIME;
    hdcpSessionWrite(pSession);
}

/*!
 * @brief Handler for LW95A1_HDCP_GENERATE_SKE_INIT method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleGenerateSkeInit
(
    hdcp_generate_ske_init_param *pParam
)
{
    FLCN_STATUS   status = FLCN_OK;
    HDCP_SESSION *pSession = NULL;
    LwU32         ind = 0;
    LwU8         *pdKey2 = NULL;
    LwU8         *pTmpBuf = NULL;
    LwU8         *pKs = NULL;
    LwU8         *pEks = NULL;
    LwU64        dKey2LSB;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (HDCP_AUTH_STAGE_VERIFY_LPRIME == pSession->stage)
    {
        // Generate Riv
        status = _hdcpChooseRandomNumber(HDCP_RANDOM_TYPE_RIV, pSession,
                                         &(pParam->retCode), hdcpTempBuffer);
        if (FLCN_OK != status)
        {
            return;
        }

        // Generate Ks
        status = _hdcpChooseRandomNumber(HDCP_RANDOM_TYPE_KS, pSession,
                                         &(pParam->retCode), hdcpTempBuffer);
        if (FLCN_OK != status)
        {
            return;
        }

        // Generate dkey2
        pdKey2  = hdcpTempBuffer;
        pTmpBuf = (LwU8*)(((LwU32)hdcpTempBuffer) + LW95A1_HDCP_SIZE_DKEY_8);

        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpDkey));
        status  = hdcpGetDkey(pSession, pdKey2, pTmpBuf);
        OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpDkey));

        if (FLCN_OK != status)
        {
            return;
        }

        //
        // Generate eKs
        // eKs = ks  XOR (dkey2 XOR Rrx) where Rrx is XORed with the 64 LSB of dkey2
        //
        // dkey2 XOR rrx
        memcpy(&dKey2LSB, &(pSession->sessionState.rrx), LW95A1_HDCP_SIZE_RRX_8);
        dKey2LSB = dKey2LSB ^ (*(LwU64*)(pdKey2 + LW95A1_HDCP_SIZE_DKEY_8 - LW95A1_HDCP_SIZE_RRX_8));
        memcpy((void*)(pdKey2 + LW95A1_HDCP_SIZE_DKEY_8 - LW95A1_HDCP_SIZE_RRX_8),
            (void *)&dKey2LSB, LW95A1_HDCP_SIZE_RRX_8);

        // Use tmpBuff to hold necessary keys in 16 byte aligned address
        pKs = pTmpBuf;
        pEks = pKs + LW95A1_HDCP_SIZE_KS_8;
        memcpy(pKs, pSession->encryptionState.ks, LW95A1_HDCP_SIZE_KS_8);

        // Enter HS mode and process the method
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpXor));
        OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(hdcpXor), NULL, 0);

        // XOR pdKey2 result with KS  (128 bits)
        hdcpXor128Bits(pdKey2, pKs, pEks);

        OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);
        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpXor));

        // Copy the result back to the param
        memcpy(pParam->eKs, pEks, LW95A1_HDCP_SIZE_KS_8);
        memcpy((LwU8*)&pParam->riv, (LwU8*)&pSession->encryptionState.riv, LW95A1_HDCP_SIZE_RIV_8);

        // Reset the counters according to spec
        for (ind = 0; ind < pSession->encryptionState.numStreams; ind++)
        {
            (pSession->encryptionState.streamCtr)[ind] = hdcpGlobalData.header.streamCtr;
            (pSession->encryptionState.inputCtr)[ind] = 0;
            hdcpGlobalData.header.streamCtr += 1;
        }
    }
    else
    {
        pParam->retCode = HDCP_METHOD_ERROR(GENERATE_SKE_INIT, ILWALID_STAGE);
        return;
    }

    pSession->stage = HDCP_AUTH_STAGE_GENERATE_SKE_INIT;
    hdcpSessionWrite(pSession);
}

/*!
 * @brief Handler for LW95A1_HDCP_ENCRYPT_PAIRING_INFO method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleEncryptPairingInfo
(
    hdcp_encrypt_pairing_info_param *pParam
)
{
    FLCN_STATUS   status     = FLCN_OK;
    HDCP_SESSION *pSession   = NULL;
    LwU8          m[HDCP_SIZE_M];
    LwU8          epair[HDCP_SIZE_EPAIR];
    LwU8          epairHash[SHA256_HASH_BLOCK_SIZE];
    LwU8         *pTempBufIn  = NULL;
    LwU8         *pEncryptKey = NULL;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (HDCP_AUTH_STAGE_VERIFY_HPRIME != pSession->stage)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_STAGE);
        return;
    }

    // Make sure Km is initialized in the session.
    if ((0 == pSession->sessionState.km[0]) &&
        (0 == pSession->sessionState.km[1]))
    {
        pParam->retCode = HDCP_METHOD_ERROR(ENCRYPT_PAIRING_INFO, ILWALID_KM);
        return;
    }

    //
    // Make sure rtx is initialized in the session. Without rtx, we cannot
    // create m.
    //
    if (0 == pSession->sessionState.rtx)
    {
        pParam->retCode = HDCP_METHOD_ERROR(ENCRYPT_PAIRING_INFO, ILWALID_M);
        return;
    }

    if ((0 == pSession->transmitterInfo.version) ||
        (1 == pSession->transmitterInfo.version))
    {
        // Get M (64 zeroes appended to rtx and all are in big endian format.
        memset(m, 0, HDCP_SIZE_M);
        memcpy(m, &(pSession->sessionState.rtx), HDCP_SIZE_RTX);
    }
    else
    {
        memcpy(m, &(pSession->sessionState.rtx), HDCP_SIZE_RTX);
        memcpy(m + HDCP_SIZE_RTX, &(pSession->sessionState.rrx),
               HDCP_SIZE_RRX);
    }

    // Build up pairing before encryption.
    memset(epair, 0, HDCP_SIZE_EPAIR);
    memcpy(epair, &pSession->receiverId, HDCP_SIZE_RCVR_ID);
    memcpy(epair + HDCP_SIZE_RCVR_ID, m, HDCP_SIZE_M);
    memcpy(epair + HDCP_SIZE_RCVR_ID + HDCP_SIZE_M,
           &pSession->sessionState.km, HDCP_SIZE_KM);
    memcpy(epair + HDCP_SIZE_RCVR_ID + HDCP_SIZE_M + HDCP_SIZE_KM,
           &pParam->eKhKm, HDCP_SIZE_EKH_KM);

    // Get the SHA-256 hash
    memset(epairHash, 0, SHA256_HASH_BLOCK_SIZE);
    sha256(epairHash, epair, (HDCP_SIZE_RCVR_ID + HDCP_SIZE_M + HDCP_SIZE_KM +
                              HDCP_SIZE_EKH_KM) / 8);
    memcpy(epair + HDCP_SIZE_RCVR_ID + HDCP_SIZE_M + HDCP_SIZE_KM +
           HDCP_SIZE_EKH_KM, epairHash, SHA256_HASH_BLOCK_SIZE);

    // Encrypt using the temp. buffer for encrypting the epair
    pTempBufIn  = hdcpTempBuffer;
    pEncryptKey = (LwU8*)(((LwU32)hdcpTempBuffer) + HDCP_SIZE_EPAIR);
    memcpy(pTempBufIn, epair, HDCP_SIZE_EPAIR);

    // Enter HS mode and process the method
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpCryptEpair));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libMemHs));

    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(hdcpCryptEpair), NULL, 0);
    hdcpEncryptEpair(pTempBufIn, pEncryptKey);
    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libMemHs));
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpCryptEpair));

    // Send back to the client.
    memcpy(&pParam->ePair, pTempBufIn, HDCP_SIZE_EPAIR);
    hdcpSessionWrite(pSession);
}

/*!
 * @brief Handler for LW95A1_HDCP_DECRYPT_PAIRING_INFO method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleDecryptPairingInfo
(
    hdcp_decrypt_pairing_info_param *pParam
)
{
    FLCN_STATUS  status      = FLCN_OK;
    HDCP_SESSION *pSession    = NULL;
    LwU8          epairSig[SHA256_HASH_BLOCK_SIZE];
    LwU8          epairHash[SHA256_HASH_BLOCK_SIZE];
    LwU8          km[HDCP_SIZE_KM];
    LwU8         *pTempBufIn  = NULL;
    LwU8         *pEncryptKey = NULL;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (HDCP_AUTH_STAGE_VERIFY_CERT != pSession->stage)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_STAGE);
        return;
    }

    // Decrypt epair using the temp buffer.
    pTempBufIn = hdcpTempBuffer;
    pEncryptKey = (LwU8*)(((LwU32)hdcpTempBuffer) + HDCP_SIZE_EPAIR);
    memcpy(pTempBufIn, pParam->ePair, HDCP_SIZE_EPAIR);

    // Enter HS mode and process the method
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpCryptEpair));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libMemHs));

    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(hdcpCryptEpair), NULL, 0);
    hdcpDecryptEpair(pTempBufIn, pEncryptKey);
    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libMemHs));
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpCryptEpair));

    // Pull out the signature
    memcpy(epairSig, pTempBufIn + HDCP_SIZE_RCVR_ID + HDCP_SIZE_M +
           HDCP_SIZE_KM + HDCP_SIZE_EKH_KM, SHA256_HASH_BLOCK_SIZE);

    //
    // Compute the SHA-256 hash of the epair data and compare it with the
    // signature to verify the integrity of the epair data.
    //
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    sha256(epairHash, pTempBufIn, (HDCP_SIZE_RCVR_ID + HDCP_SIZE_M +
                                   HDCP_SIZE_KM + HDCP_SIZE_EKH_KM) / 8);
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));

    // Check that the signature matches
    if (0 != memcmp(epairSig, epairHash, SHA256_HASH_BLOCK_SIZE))
    {
        pParam->retCode = HDCP_METHOD_ERROR(DECRYPT_PAIRING_INFO, ILWALID_EPAIR);
        return;
    }

    //
    // Pull out M, eKhKm, and Km. Save Km in the session details. M and eKhKm
    // are sent to the client.
    //
    memcpy(&pParam->m, pTempBufIn + HDCP_SIZE_RCVR_ID, HDCP_SIZE_M);
    memcpy(km, pTempBufIn + HDCP_SIZE_RCVR_ID + HDCP_SIZE_M, HDCP_SIZE_KM);
    memcpy(&pParam->eKhKm, pTempBufIn + HDCP_SIZE_RCVR_ID + HDCP_SIZE_M +
           HDCP_SIZE_KM, HDCP_SIZE_EKH_KM);

    // Copy Km to session details
    memcpy(&pSession->sessionState.km, km, HDCP_SIZE_KM);

    pSession->stage = HDCP_AUTH_STAGE_GENERATE_EKM;
    hdcpSessionWrite(pSession);
}

/*!
 * @brief Handler for LW95A1_HDCP_ENCRYPT method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleEncrypt
(
    hdcp_encrypt_param *pParam
)
{
    FLCN_STATUS   status = FLCN_OK;
    HDCP_SESSION *pSession = NULL;
    LwU32         inputBuf;
    LwU32         outputBuf;
    LwU32         actRecIdx;
    LwU32         ptMagic;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    ptMagic = pParam->streamCtr;

    // Get input buffer for encryption
    inputBuf = HDCP_GET_METHOD_PARAM(HDCP_METHOD_ID(SET_ENC_INPUT_BUFFER));
    if (0 == inputBuf)
    {
        pParam->retCode = HDCP_METHOD_ERROR(ENCRYPT, INPUT_BUFFER_NOT_SET);
        return;
    }

    // Get output buffer to store result
    outputBuf = HDCP_GET_METHOD_PARAM(HDCP_METHOD_ID(SET_ENC_OUTPUT_BUFFER));
    if (0 == outputBuf)
    {
        pParam->retCode = HDCP_METHOD_ERROR(ENCRYPT, OUTPUT_BUFFER_NOT_SET);
        return;
    }

    // Check alignment
    if (0 != (pParam->encOffset % 4))
    {
        pParam->retCode = HDCP_METHOD_ERROR(ENCRYPT, ILWALID_ALIGN);
        return;
    }

#if (SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF))
    if (0xABCDDEAD == ptMagic)
    {
        ptMagic = 0;
        actRecIdx = pParam->noOfInputBlocks; // Reusing actRecIdx

        //
        // Pass through for testing purposes. Copy 256 bytes at a time. Don't
        // worry about perf, since it's just for testing purposes.
        //
        while (actRecIdx > (HDCP_SIZE_MAX_DMA / 16))
        {
            actRecIdx -= (HDCP_SIZE_MAX_DMA / 16);
            ptMagic   += HDCP_SIZE_MAX_DMA;
        }

        return;
    }
#endif

    if (!hdcpSessionIsActive(pSession->id, &actRecIdx))
    {
#ifdef HDCP_DEBUG_DEMO_SESSION
        //
        // No active session is found with the given session ID. May be client
        // is trying to run encrypt without bother about authentication part.
        // Create a demo active session and let the client carry on with his
        // testing.
        //
        hdcpCreateDemoActiveSession(pParam, &actRecIdx);
        if (HDCP_METHOD_ERROR(ENCRYPT, NONE) != pParam->retCode)
        {
            return;
        }
#else
        pParam->retCode = HDCP_METHOD_ERROR(ENCRYPT, SESSION_NOT_ACTIVE);
        return;
#endif // HDCP_DEBUG_DEMO_SESSION
    }

    // Decrypt the valid active session->
    status = hdcpSessionCryptActive(actRecIdx, LW_FALSE);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(UNKNOWN);
        return;
    }

    // See if the streamID is valid
    if (pParam->streamID >=
        hdcpGlobalData.activeSessions[actRecIdx].encryptionState.numStreams)
    {
        pParam->retCode = HDCP_METHOD_ERROR(ENCRYPT, ILWALID_STREAM);
        return;
    }

    pParam->inputCtr =
        (hdcpGlobalData.activeSessions[actRecIdx].encryptionState.inputCtr)[pParam->streamID];
    pParam->streamCtr =
        (hdcpGlobalData.activeSessions[actRecIdx].encryptionState.streamCtr)[pParam->streamID];

    memset(pParam->pesPriv, 0, HDCP_SIZE_PES_HDR);
    _hdcpGeneratePesHeader(pParam->pesPriv, pParam->streamCtr,
        pParam->inputCtr);
    hdcpSwapEndianness(&(pParam->inputCtr), &(pParam->inputCtr),
                       HDCP_SIZE_INPUT_CTR);
    hdcpSwapEndianness(&(pParam->streamCtr), &(pParam->streamCtr),
                       HDCP_SIZE_STREAM_CTR);

    // Enter HS mode and process the method
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpEncrypt));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libMemHs));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libDmaHs));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));

    //
    // TODO (from original code): Check the error caused when using temp
    // variable to pass inputCtr to hdcpEncrypt. It initializes to a random
    // number causing an encryption failure.
    //
    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(hdcpEncrypt), NULL, 0);
    status = hdcpEncrypt(&(hdcpGlobalData.activeSessions[actRecIdx].encryptionState),
                         inputBuf, pParam->noOfInputBlocks, pParam->streamID,
                         outputBuf, hdcpTempBuffer,
                         &((hdcpGlobalData.activeSessions[actRecIdx].encryptionState.inputCtr)[pParam->streamID]),
                         pParam->encOffset);
    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libDmaHs));
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libMemHs));
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpEncrypt));

    if (FLCN_OK != status)
    {
        return;
    }

    status = hdcpSessionCryptActive(actRecIdx, LW_TRUE);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(UNKNOWN);
    }
}

/*!
 * @brief Handler for LW95A1_HDCP_VALIDATE_SRM method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleValidateSrm
(
    hdcp_validate_srm_param *pParam
)
{
    FLCN_STATUS status       = FLCN_OK;
    LwU32       srmOffset    = 0;
    LwBool      bUseTestKeys = LW_FALSE;

    if (SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF))
    {
        bUseTestKeys = LW_TRUE;
    }

    srmOffset = HDCP_GET_METHOD_PARAM(HDCP_METHOD_ID(SET_SRM));
    if (0 == srmOffset)
    {
        pParam->retCode = HDCP_METHOD_ERROR(VALIDATE_SRM, SRM_NOT_SET);
        return;
    }

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpSrm));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpRsa));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libBigInt));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpDcpKey));

    status = hdcpRevocationCheck(NULL, 0, srmOffset, hdcpTempBuffer,
                                 bUseTestKeys, NULL);

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpDcpKey));
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libBigInt));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpRsa));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpSrm));

    if ((FLCN_ERR_ILWALID_ARGUMENT == status) ||
        (FLCN_ERROR == status))
    {
        status = FLCN_OK;
        pParam->retCode = HDCP_METHOD_ERROR(VALIDATE_SRM, SRM_VALD_FAILED);
    }
}

/*!
 * @brief Handler for LW95A1_HDCP_REVOCATION_CHECK method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleRevocationCheck
(
    hdcp_revocation_check_param *pParam
)
{
    FLCN_STATUS   status       = FLCN_OK;
    HDCP_SESSION *pSession     = NULL;
    LwU32         srmOffset    = HDCP_GET_METHOD_PARAM(HDCP_METHOD_ID(SET_SRM));
    LwBool        bUseTestKeys = LW_FALSE;
    LwU32         sessionId    = 0;
    LwU8          receiverId[HDCP_SIZE_RCVR_ID];

    if (SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF))
    {
        bUseTestKeys = LW_TRUE;
    }

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->transID.sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->transID.sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->transID.sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (0 == srmOffset)
    {
        pParam->retCode = HDCP_METHOD_ERROR(REVOCATION_CHECK, SRM_NOT_SET);
        return;
    }

    if (pParam->isVerHdcp2x)
    {
        sessionId = pParam->transID.sessionID;
        if (HDCP_AUTH_STAGE_NONE == pSession->stage)
        {
            pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_STAGE);
            return;
        }

        memcpy(receiverId, pSession->receiverId, HDCP_SIZE_RCVR_ID);
        pSession->bIsRevocationCheckDone = LW_FALSE;
    }
    else
    {
        // TODO: 1.x device. Read receiver ID from display HW
    }

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpSrm));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpRsa));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libBigInt));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpDcpKey));

    status = hdcpRevocationCheck(receiverId, 1, srmOffset, hdcpTempBuffer,
                                 bUseTestKeys, NULL);

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpDcpKey));
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libBigInt));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpRsa));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpSrm));

    if (FLCN_ERR_HDCP_ILWALID_SRM == status)
    {
        status = FLCN_OK;
        pParam->retCode = HDCP_METHOD_ERROR(REVOCATION_CHECK, SRM_VALD_FAILED);
    }
    else if (FLCN_ERR_HDCP_RECV_REVOKED == status)
    {
        // Receiver is revoked. Reset the session
        status = FLCN_OK;
        pParam->retCode = HDCP_METHOD_ERROR(REVOCATION_CHECK, RCV_ID_REVOKED);
        if (pParam->isVerHdcp2x)
        {
            hdcpSessionReset(pSession);
        }
    }
    else if (FLCN_OK == status)
    {
        if (pParam->isVerHdcp2x)
        {
            pSession->bIsRevocationCheckDone = LW_TRUE;
            hdcpSessionWrite(pSession);
        }
    }
}

/*!
 * @brief Handler for LW95A1_HDCP_SESSION_CTRL method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleSessionCtrl
(
    hdcp_session_ctrl_param *pParam
)
{
    FLCN_STATUS                     status = FLCN_OK;
    HDCP_SESSION                   *pSession = NULL;
    HDCP_SESSION_ACTIVE_ENCRYPTION *pEncState;
    LwU32                           actRecIdx;
    LwU32                           i;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (LW95A1_HDCP_SESSION_CTRL_FLAG_DELETE == pParam->ctrlFlag)
    {
        if (hdcpSessionIsActive(pSession->id, &actRecIdx))
        {
            pParam->retCode = HDCP_METHOD_ERROR(SESSION_CTRL, SESSION_ACTIVE);
            return;
        }

        // Reset the states in the session details.
        memset(pSession, 0, sizeof(HDCP_SESSION));
        pSession->id = HDCP_GET_SESSION_INDEX(pParam->sessionID);
        pSession->status = HDCP_SESSION_FREE;
    }
    else if (LW95A1_HDCP_SESSION_CTRL_FLAG_ACTIVATE == pParam->ctrlFlag)
    {
        // Cannot activate already active session
        if (hdcpSessionIsActive(pSession->id, &actRecIdx))
        {
            pParam->retCode = HDCP_METHOD_ERROR(SESSION_CTRL, SESSION_ACTIVE);
            return;
        }

        //
        // Validate if the session can be activated. Session needs to be a the
        // stage GENERATE_SKE_INIT or VERIFY_VPRIME, and the revocation check
        // needs to have been done.
        //
        if ((HDCP_AUTH_STAGE_GENERATE_SKE_INIT != pSession->stage) &&
            (HDCP_AUTH_STAGE_VERIFY_VPRIME != pSession->stage))
        {
            pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_STAGE);
            return;
        }

        //
        // Check if V' has been revoked.
        //
        if (HDCP_VPRIME_STATUS_REVOKED == pSession->vprimeStatus)
        {
            pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_STAGE);
            return;
        }

        // Find a free active record.
        actRecIdx = 0;
        if (FLCN_OK != _hdcpFindFreeActiveRecord(&actRecIdx))
        {
            pParam->retCode = HDCP_METHOD_ERROR(SESSION_CTRL, SESSION_ACTIVE_MAX);
            return;
        }

        // Check if the found record does not have any active sessions.
        status = hdcpSessionCryptActive(actRecIdx, LW_FALSE);
        if (FLCN_OK != status)
        {
            return;
        }

        pEncState = &(hdcpGlobalData.activeSessions[actRecIdx]);
        if (HDCP_SESSION_FREE != pEncState->rec.status)
        {
            pParam->retCode = HDCP_METHOD_ERROR(SESSION_CTRL, SESSION_ACTIVE_MAX);
            return;
        }

        status = _hdcpActivateRecord(pParam->sessionID, actRecIdx);
        if (FLCN_OK != status)
        {
            return;
        }

        // Time to update the active session and encrypt again.
        for (i = 0; i < pSession->encryptionState.numStreams; i++)
        {
            pEncState->encryptionState.inputCtr[i] =
                pSession->encryptionState.inputCtr[i];
            pEncState->encryptionState.streamCtr[i] =
                pSession->encryptionState.streamCtr[i];
            pEncState->encryptionState.inputCtrSnap[i] =
                pSession->encryptionState.inputCtrSnap[i];
        }
        pEncState->encryptionState.numStreams =
            pSession->encryptionState.numStreams;

        memcpy(pEncState->encryptionState.ks,
               &(pSession->encryptionState.ks), HDCP_SIZE_KS);
        memcpy(pEncState->encryptionState.iv,
               &(pSession->encryptionState.iv), HDCP_SIZE_IV);
        memcpy(&(pEncState->encryptionState.riv),
               &(pSession->encryptionState.riv), HDCP_SIZE_RIV);

        pEncState->rec.status = HDCP_SESSION_ACTIVE;
        pEncState->rec.sessionId = pSession->id;
        pEncState->rec.bIsDemoSession = LW_FALSE;

        status = hdcpSessionCryptActive(actRecIdx, LW_TRUE);
        if (FLCN_OK != status)
        {
            return;
        }

        if (0 != pSession->displayType)
        {
            OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpPvtBus));
            hdcpWriteLc128();
            hdcpWriteKs((LwU8*)(pSession->encryptionState.ks));
            hdcpWriteRiv((LwU8*)(&pSession->encryptionState.riv));
            hdcpEnableEncryption();
            OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpPvtBus));
        }

        // Change the session status
        pSession->status = HDCP_SESSION_ACTIVE;
    }
    else if (LW95A1_HDCP_SESSION_CTRL_FLAG_DEACTIVATE == pParam->ctrlFlag)
    {
        status = hdcpSessionDeactivate(pSession);
        if (FLCN_ERROR == status)
        {
            pParam->retCode = HDCP_METHOD_ERROR(SESSION_CTRL, SESSION_NOT_ACTIVE);
            status = FLCN_OK;
            return;
        }
        else if (FLCN_OK != status)
        {
            return;
        }
    }

    hdcpSessionWrite(pSession);
}

/*!
 * @brief Handler for LW95A1_HDCP_VERIFY_VPRIME method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleVerifyVprime
(
    hdcp_verify_vprime_param *pParam
)
{
    FLCN_STATUS       status    = FLCN_OK;
    HDCP_SESSION     *pSession  = NULL;
    LwU32             srmOffset;
    LwU32             rcvrIdListOffset;
    LwU32             rcvrIdListSize;
    LwU8              revokedRcvr[HDCP_SIZE_RCVR_ID];
    LwBool            bUseTestKeys = LW_FALSE;

    if (SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF))
    {
        bUseTestKeys = LW_TRUE;
    }

    memset(revokedRcvr, 0, sizeof(revokedRcvr));

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->transID.sessionID))
    {
        return;
    }

    srmOffset = HDCP_GET_METHOD_PARAM(HDCP_METHOD_ID(SET_SRM));
    if (0 == srmOffset)
    {
        pParam->retCode = HDCP_METHOD_ERROR(VERIFY_VPRIME, SRM_NOT_SET);
        return;
    }

    rcvrIdListOffset = HDCP_GET_METHOD_PARAM(HDCP_METHOD_ID(SET_RECEIVER_ID_LIST));
    if (0 == rcvrIdListOffset)
    {
        pParam->retCode = HDCP_METHOD_ERROR(VERIFY_VPRIME, RCVD_ID_LIST_NOT_SET);
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->transID.sessionID);
    if (pSession->id != pParam->transID.sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    // Don't validate V' beyond MAX
    if (pSession->vprimeAttemptCount >= LW95A1_HDCP_VERIFY_VPRIME_MAX_ATTEMPTS)
    {
        pParam->retCode = HDCP_METHOD_ERROR(VERIFY_VPRIME, ATTEMPT_MAX);
        return;
    }

    // Reset V' status
    pSession->vprimeStatus = HDCP_VPRIME_STATUS_NOT_STARTED;

    //
    // Read the receiver ID list (use certificate buffer to hold it to cut down
    // on stack size).
    //
    memset(hdcpCertificateRx, 0, LW_ALIGN_UP(HDCP_SIZE_BKSV_LIST, 16));
    rcvrIdListSize = pParam->deviceCount * HDCP_SIZE_RCVR_ID;

    status = hdcpDmaRead(hdcpCertificateRx, rcvrIdListOffset, LW_ALIGN_UP(HDCP_SIZE_BKSV_LIST, 16));
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_METHOD_ERROR(VERIFY_VPRIME, RCVD_ID_LIST_NOT_SET);
        return;
    }

    // Check if it is a receiver; if so, directly proceed to SRM check
    if (TSEC_SRM_REVOCATION_CHECK != pParam->reserved1[0])
    {
        // Copy to local receiver list
        if (pParam->isVerHdcp2x)
        {
            if (0 == pSession->receiverInfo.version)
            {
                status = _hdcpComputeV_2_0(pSession, pParam, hdcpCertificateRx,
                                           rcvrIdListSize);
            }
            else
            {
                status = _hdcpComputeV_2_1(pSession, pParam, hdcpCertificateRx,
                                           rcvrIdListSize);
            }
        }
        else
        {
#ifdef HDCP_TARGET_TSEC
                status = hdcpComputerV_1_X(pSession, pParam, hdcpCertificateRx,
                                           rcvrIdListSize);
#else
                pParam->retCode = HDCP_METHOD_ERROR(VERIFY_VPRIME, VPRIME_VALD_FAILED);
#endif
        }

        if (FLCN_OK == status)
        {
            pSession->stage = HDCP_AUTH_STAGE_VERIFY_VPRIME;
        }
        else if (HDCP_METHOD_ERROR(VERIFY_VPRIME, VPRIME_VALD_FAILED) ==
                 pParam->retCode)
        {
            status = FLCN_OK;
            pParam->retCode = HDCP_METHOD_ERROR(VERIFY_VPRIME, VPRIME_VALD_FAILED);
            pSession->vprimeStatus = HDCP_VPRIME_STATUS_FAILURE;
            pSession->vprimeAttemptCount++;
            hdcpSessionWrite(pSession);
            return;
        }
        else if (HDCP_METHOD_ERROR(VERIFY_VPRIME, SEQ_NUM_V_ROLLOVER) ==
                 pParam->retCode)
        {
            status = FLCN_OK;
            pParam->retCode = HDCP_METHOD_ERROR(VERIFY_VPRIME, SEQ_NUM_V_ROLLOVER);
            pSession->vprimeStatus = HDCP_VPRIME_STATUS_FAILURE;
            pSession->vprimeAttemptCount++;
            hdcpSessionWrite(pSession);
            return;
        }
        else
        {
            // Unknown error
            return;
        }
    }

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpSrm));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpRsa));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libBigInt));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpDcpKey));

    // V' matches; do a revocation check on the receiver list.
    status = hdcpRevocationCheck(hdcpCertificateRx, pParam->deviceCount, srmOffset,
                                 hdcpTempBuffer, bUseTestKeys, revokedRcvr);

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpDcpKey));
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcpCryptHs));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libBigInt));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpRsa));
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(hdcpSrm));

    if (FLCN_ERR_HDCP_ILWALID_SRM == status)
     {
         pParam->retCode = HDCP_METHOD_ERROR(VERIFY_VPRIME, SRM_VALD_FAILED);
         pSession->vprimeStatus = HDCP_VPRIME_STATUS_SRM_ILWALID;
         pSession->vprimeAttemptCount++;
     }
     else if (FLCN_ERR_HDCP_RECV_REVOKED == status)
     {
         status = FLCN_OK;
         memcpy(pParam->revoID, revokedRcvr, HDCP_SIZE_RCVR_ID);
         pParam->retCode = HDCP_METHOD_ERROR(VERIFY_VPRIME, RCVD_ID_REVOKED);
         pSession->vprimeStatus = HDCP_VPRIME_STATUS_REVOKED;

         // Since revoked, we will not perform any more V' checks
         pSession->vprimeAttemptCount = LW95A1_HDCP_VERIFY_VPRIME_MAX_ATTEMPTS;

         // Deactivate the session to prevent encryption
         hdcpSessionReset(pSession);
     }
     else if (FLCN_OK == status)
     {
         pSession->vprimeStatus = HDCP_VPRIME_STATUS_SUCCESS;
         pSession->vprimeAttemptCount++;
     }
     else
     {
         // Unknown error
         return;
     }

     status = hdcpSessionWrite(pSession);
}

/*!
 * @brief Handler for LW95A1_HDCP_GET_RTT_CHALLENGE method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleGetRttChallenge
(
    hdcp_get_rtt_challenge_param *pParam
)
{
    FLCN_STATUS   status = FLCN_OK;
    HDCP_SESSION *pSession = NULL;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (HDCP_AUTH_STAGE_GENERATE_LC_INIT != pSession->stage)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_STAGE);
        return;
    }

    // Only 2.1 supports precomputation and GetRttChallenge.
    if (1 != pSession->receiverInfo.version)
    {
        pParam->retCode = HDCP_METHOD_ERROR(GET_RTT_CHALLENGE, MSG_UNSUPPORTED);
        return;
    }

    if (HDCP_RECEIVER_CAPABILITIES_PRECOMPUTE_SUPPORT_NO ==
        (pSession->receiverInfo.capabilitiesMask & 0x1))
    {
        pParam->retCode = HDCP_METHOD_ERROR(GET_RTT_CHALLENGE, MSG_UNSUPPORTED);
        return;
    }

    memcpy(pParam->L128l, pSession->L, HDCP_SIZE_LPRIME / 2);
    pSession->stage = HDCP_AUTH_STAGE_GET_RTT_CHALLENGE;
    hdcpSessionWrite(pSession);
}

/*!
 * @brief Handler for LW95A1_HDCP_STREAM_MANAGE method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleStreamManage
(
    hdcp_stream_manage_param *pParam
)
{
    FLCN_STATUS   status = FLCN_OK;
    HDCP_SESSION *pSession = NULL;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (LW95A1_HDCP_STREAM_MANAGE_FLAG_MANAGE == pParam->manageFlag)
    {
        status = _hdcpStreamManage(pParam, pSession);
        if (FLCN_OK == status)
        {
            status = hdcpSessionWrite(pSession);
        }
    }
    else if (LW95A1_HDCP_STREAM_MANAGE_FLAG_READY == pParam->manageFlag)
    {
        status = _hdcpStreamReady(pParam, pSession);
    }
    else
    {
        pParam->retCode = HDCP_METHOD_ERROR(STREAM_MANAGE, ILWALID_FLAG);
    }
}

/*!
 * @brief Handler for LW95A1_HDCP_COMPUTE_SPRIME method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleComputeSprime
(
    hdcp_compute_sprime_param *pParam
)
{
    FLCN_STATUS   status = FLCN_OK;
    HDCP_SESSION *pSession = NULL;
    LwU32         statusMask = 0;
    LwU32         actRecIdx = 0;
    LwU32         i;

    HDCP_SESSION_ACTIVE_ENCRYPTION *pEncState = NULL;

    // Only HDCP 2.0 status reporting is supported
    if (LW95A1_HDCP_COMPUTE_SPRIME_VERSION_HDCP2X_FALSE == pParam->isVerHdcp2x)
    {
        status = FLCN_OK;
        pParam->retCode = HDCP_METHOD_ERROR(COMPUTE_SPRIME,
                                            VERSION_UNSUPPORTED);
        return;
    }

    if (!_hdcpMethodIsInitialized(&(pParam->retCode),
                                  pParam->transID.sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->transID.sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->transID.sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    // Check if first phase authentication completed
    if ((pSession->stage >= HDCP_AUTH_STAGE_GENERATE_SKE_INIT) &&
        (pSession->bIsRevocationCheckDone))
    {
        statusMask = FLD_SET_DRF(95A1_HDCP, _COMPUTE_SPRIME_STATUS,
                                 _AUTHENTICATED, _TRUE, statusMask);
    }

    // Check encrypting state
    if (hdcpSessionIsActive(pSession->id, &actRecIdx))
    {
        status = hdcpSessionCryptActive(actRecIdx, LW_FALSE);
        if (FLCN_OK != status)
        {
            return;
        }
        pEncState = &(hdcpGlobalData.activeSessions[actRecIdx]);

        statusMask = FLD_SET_DRF(95A1_HDCP, _COMPUTE_SPRIME_STATUS,
                                 _ENCRYPTING, _TRUE, statusMask);
        for (i = 0; i < pEncState->encryptionState.numStreams; i++)
        {
            //
            // If inputCtr never changed from last-time we read status, say not
            // being encrypted. Encryption will always be zero after first
            // encryption because if inputCtr == inputCtrSnap
            // TODO: Instead of just comparing, we can also check increase in
            // inputCtr (from original code)
            //
            if (pEncState->encryptionState.inputCtr[i] <=
                pEncState->encryptionState.inputCtrSnap[i])
            {
                statusMask = FLD_SET_DRF(95A1_HDCP, _COMPUTE_SPRIME_STATUS,
                                         _ENCRYPTING, _FALSE, statusMask);
            }
            pEncState->encryptionState.inputCtrSnap[i] =
                pEncState->encryptionState.inputCtr[i];
        }

        status = hdcpSessionCryptActive(actRecIdx, LW_TRUE);
        if (FLCN_OK != status)
        {
            return;
        }
    }

    // External panel - setting it to LW_TRUE by default
    statusMask = FLD_SET_DRF(95A1_HDCP, _COMPUTE_SPRIME_STATUS,
                             _EXTERNAL_PANEL, _TRUE, statusMask);

    // Repeater
    if (pSession->bRepeater)
    {
        statusMask = FLD_SET_DRF(95A1_HDCP, _COMPUTE_SPRIME_STATUS,
                                 _REPEATER, _TRUE, statusMask);
    }

    // V' validation
    if (HDCP_VPRIME_STATUS_NOT_STARTED == pSession->vprimeStatus)
    {
        statusMask = FLD_SET_DRF(95A1_HDCP, _COMPUTE_SPRIME_STATUS,
                                 _REPEATER_VALD, _PENDING, statusMask);
    }
    else if ((HDCP_VPRIME_STATUS_FAILURE == pSession->vprimeStatus) ||
        (HDCP_VPRIME_STATUS_REVOKED == pSession->vprimeStatus))
    {
        statusMask = FLD_SET_DRF(95A1_HDCP, _COMPUTE_SPRIME_STATUS,
                                 _REPEATER_VALD, _FAIL, statusMask);
    }
    else if (HDCP_VPRIME_STATUS_SUCCESS == pSession->vprimeStatus)
    {
        statusMask = FLD_SET_DRF(95A1_HDCP, _COMPUTE_SPRIME_STATUS,
                                 _REPEATER_VALD, _SUCCESS, statusMask);
    }

    // Production keys
    if (!pSession->bIsUseTestKeys)
    {
        statusMask = FLD_SET_DRF(95A1_HDCP, _COMPUTE_SPRIME_STATUS,
                                 _PROD_KEYS, _TRUE, statusMask);
    }

    pParam->status = statusMask;
}

/*!
 * @brief Handler for LW95A1_HDCP_EXCHANGE_INFO method.
 *
 * @param [in/out]  pParam
 *      Pointer to the method's parameters.Additionally, return values
 *      (if applicable) and error codes are sent back to the client via this
 *      structure.
 */
void
hdcpMethodHandleExchangeInfo
(
    hdcp_exchange_info_param *pParam
)
{
    FLCN_STATUS   status   = FLCN_OK;
    HDCP_SESSION *pSession = NULL;

    if (!_hdcpMethodIsInitialized(&(pParam->retCode), pParam->sessionID))
    {
        return;
    }

    status = hdcpSessionReadById(&pSession, pParam->sessionID);
    if (FLCN_OK != status)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if (pSession->id != pParam->sessionID)
    {
        pParam->retCode = HDCP_GENERAL_ERROR(ILWALID_SESSION);
        return;
    }

    if ((LW95A1_HDCP_VERSION_20 == pSession->version) ||
        (LW95A1_HDCP_VERSION_21 == pSession->version))
    {
        pParam->retCode = HDCP_GENERAL_ERROR(MSG_UNSUPPORTED);
        return;
    }

    switch (pParam->methodFlag)
    {
        case LW95A1_HDCP_EXCHANGE_INFO_SET_RCVR_INFO:
        {
            pSession->receiverInfo.version = pParam->info.setRxInfo.version;
            pSession->receiverInfo.capabilitiesMask =
                pParam->info.setRxInfo.rcvrCapsMask;
            pParam->retCode = HDCP_METHOD_ERROR(EXCHANGE_INFO, NONE);
            break;
        }
        case LW95A1_HDCP_EXCHANGE_INFO_GET_RCVR_INFO:
        {
            pParam->info.getRxInfo.version = pSession->receiverInfo.version;
            pParam->info.getRxInfo.rcvrCapsMask =
                pSession->receiverInfo.capabilitiesMask;
            pParam->retCode = HDCP_METHOD_ERROR(EXCHANGE_INFO, NONE);
            break;
        }
        case LW95A1_HDCP_EXCHANGE_INFO_GET_TMTR_INFO:
        {
            pParam->info.getTxInfo.version = pSession->transmitterInfo.version;
            pParam->info.getTxInfo.tmtrCapsMask =
                pSession->transmitterInfo.capabilitiesMask;
            pParam->retCode = HDCP_METHOD_ERROR(EXCHANGE_INFO, NONE);
            break;
        }
        case LW95A1_HDCP_EXCHANGE_INFO_SET_TMTR_INFO:
        {
            pSession->transmitterInfo.version = pParam->info.setTxInfo.version;
            pSession->transmitterInfo.capabilitiesMask =
                pParam->info.setTxInfo.tmtrCapsMask;
            pParam->retCode = HDCP_METHOD_ERROR(EXCHANGE_INFO, NONE);
            break;
        }
        default:
        {
            // Why the inconsistent return codes?
            pParam->retCode = LW95A1_HDCP_EXCHANGE_INFO_ILWALID_METHOD_FLAG;
            break;
        }
    }
}
#endif
