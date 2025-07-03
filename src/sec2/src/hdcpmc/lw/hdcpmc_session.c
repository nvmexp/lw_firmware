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
 * @file    hdcpmc_session.c
 * @brief   Provides operations for Miracast HDCP sessions.
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "lwosselwreovly.h"

/* ------------------------- Application Includes --------------------------- */
#include "scp_crypt.h"
#include "sec2_hs.h"
#include "sha256.h"
#include "hdcpmc/hdcpmc_data.h"
#include "hdcpmc/hdcpmc_mem.h"
#include "hdcpmc/hdcpmc_scp.h"
#include "hdcpmc/hdcpmc_session.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
#if (HDCP_SESSION_FEATURE_ENCRYPT == HDCP_SESSION_FEATURE_ON)
static FLCN_STATUS _hdcpSessionDoCrypt(LwU32 dmemOffset, LwU32 size, LwU32 dstOffset, LwBool bIsEncrypt)
    GCC_ATTRIB_SECTION("imem_hdcpSessionHs", "_start");
#endif
static FLCN_STATUS _hdcpSessionDecrypt(HDCP_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "_hdcpSessionDecrypt");
static FLCN_STATUS _hdcpSessiolwerifySignature(HDCP_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "_hdcpSessiolwerifySignature");
static FLCN_STATUS _hdcpSessionRead(HDCP_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "_hdcpSessionRead");
static FLCN_STATUS _hdcpSessionWriteEncrypted(HDCP_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "_hdcpSessionWriteEncrypted");
static FLCN_STATUS _hdcpRecordDeactivate(LwU32 recIdx)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "_hdcpRecordDeactivate");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
#if (HDCP_SESSION_FEATURE_ENCRYPT == HDCP_SESSION_FEATURE_ON)
static LwU32 _hdcpSessionIv[4]
    GCC_ATTRIB_ALIGN(16)
    GCC_ATTRIB_SECTION("dmem_hdcpmc", "_hdcpSessionIv") =
{
    0xDEADBEEF, 0xBEEFDEAD, 0xABCD1234, 0x5678DCBA
};
#endif // (HDCP_SESSION_FEATURE_ENCRYPT == HDCP_SESSION_FEATURE_ON)

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief HS entry function to encrypt/decrypt session data.
 *
 * @param[in]   dmemOffset  Offset in DMEM of the data block to be encrypted/
 *                          decrypted.
 * @param[in]   size        Size of the data block to ecrypted/decrypted.
 * @param[in]   dstOffset   Destination offset to write the resulting data.
 *                          This offset can be either in DMEM or FB depending
 *                          on the value of bWriteToFb.
 * @param[in]   bIsEncrypt  Specifies whether the function is to encrypt
 *                          (LW_TRUE) or decrypt (LW_FALSE) the data block.
 * @param[in]   bWriteToFb  Specifies dstOffset is an offset withing DMEM
 *                          (LW_FALSE) or the FB (LW_TRUE).
 *
 * @return  FLCN_OK if the data is properly encrypted/decrypted; otherwise,
 *          a detailed error code is returned.
 */
#if (HDCP_SESSION_FEATURE_ENCRYPT == HDCP_SESSION_FEATURE_ON)
static FLCN_STATUS
_hdcpSessionDoCrypt
(
    LwU32   dmemOffset,
    LwU32   size,
    LwU32   dstOffset,
    LwBool  bIsEncrypt
)
{
    FLCN_STATUS status;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs);
    if ((status = sec2HsPreCheckCommon_HAL(&Sec2, LW_FALSE)) != FLCN_OK)
    {
        goto _hdcpSessionDoCrypt_end;
    }

    //OS_SEC_HS_LIB_VALIDATE(libHdcpCryptHs);
    OS_SEC_HS_LIB_VALIDATE(libScpCryptHs);

    //hdcpSetSessionEncryptKey(bIsEncrypt);
    //status = hdcpEcbCrypt(dmemOffset, size, dstOffset, bIsEncrypt, bWriteToFb,
    //                      HDCP_CBC_CRYPT_MODE_CRYPT);
    status = scpCbcCrypt((LwU8*)(hdcpGlobalData.randNum), bIsEncrypt, LW_FALSE,
                         LW_SCP_SECRET_IDX_7, (LwU8*)dmemOffset, size,
                         (LwU8*)dstOffset, (LwU8*)_hdcpSessionIv);

_hdcpSessionDoCrypt_end:
    //
    // Enable the SCP RNG, and disable big hammer lockdown before returning
    // to light secure mode
    //
    EXIT_HS();
    return status;
}
#endif // (HDCP_SESSION_FEATURE_ENCRYPT == HDCP_SESSION_FEATURE_ON)

/*!
 * Decrypts the session in-place.
 *
 * @param [in/out]  pSession  Pointer to the session to decrypt. The
 *                            session will be decrypted in place.
 *
 * @return FLCN_OK if the session was decrypted successfully; othwerise, the
 *         error code value returned by called functions.
 */
static FLCN_STATUS
_hdcpSessionDecrypt
(
    HDCP_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;

#if (HDCP_SESSION_FEATURE_ENCRYPT == HDCP_SESSION_FEATURE_ON)
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpSessionHs));

    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(hdcpSessionHs), NULL, 0);
    status = _hdcpSessionDoCrypt((LwU32)pSession, HDCP_SB_SESSION_SIZE,
                                 (LwU32)pSession, LW_FALSE);
    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpSessionHs));
#endif

    return status;
}

/*!
 * @brief Verifies the signature of the session is valid.
 *
 * @param [in]  pSession    Pointer to the session to verify.
 *
 * @return FLCN_OK if the session's signature is valid; FLCN_ERROR if the
 *         session's signature is invalid; FLCN_ERR_ILWALID_ARGUMENT if the
 *         pointer to the session is NULL.
 */
static FLCN_STATUS
_hdcpSessiolwerifySignature
(
    HDCP_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;

#if (HDCP_SESSION_FEATURE_SIGNATURE == HDCP_SESSION_FEATURE_ON)
    LwU8 hash[HDCP_SIZE_SESSION_SIGNATURE];

    if (NULL == pSession)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    memset(hash, 0, HDCP_SIZE_SESSION_SIGNATURE);

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    sha256(hash, (LwU8*)((LwU8*)pSession + HDCP_SIZE_SESSION_SIGNATURE),
           sizeof(HDCP_SESSION) - HDCP_SIZE_SESSION_SIGNATURE);
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));

    if (0 != memcmp(hash, pSession->signature, HDCP_SIZE_SESSION_SIGNATURE))
    {
        status = FLCN_ERROR;
    }
#endif

    return status;
}

/*!
 * @brief Reads the session from the FB, decrypts it, and verifies it.
 *
 * @param [in/out]  pSession    Pointer to the session. The specific session
 *                              ID is filled out by the caller.
 */
static FLCN_STATUS
_hdcpSessionRead
(
    HDCP_SESSION *pSession
)
{
    FLCN_STATUS status   = FLCN_OK;
    LwU32       fbOffset = (HDCP_SB_SESSION_OFFSET +
                           (HDCP_GET_SESSION_INDEX(pSession->id) *
                                    HDCP_SB_SESSION_SIZE)) >> 8;

    status = hdcpDmaRead(pSession, hdcpGlobalData.sbOffset + fbOffset,
                         HDCP_SB_SESSION_SIZE);
    if (FLCN_OK != status)
    {
        goto _hdcpSessionRead_end;
    }

    status = _hdcpSessionDecrypt(pSession);
    if (FLCN_OK != status)
    {
        goto _hdcpSessionRead_end;
    }

    status = _hdcpSessiolwerifySignature(pSession);

_hdcpSessionRead_end:
    return status;
}

/*!
 * @brief Encrypts the session in-place and writes it to the frame buffer.
 *
 * @param [in/out]  pSession  Pointer to the session to encrypt and write to
 *                            the FB.
 *
 * @return FLCN_OK if the session was encrypted and written to the FB;
 *         otherwise, the error code value returned by called functions.
 */
static FLCN_STATUS
_hdcpSessionWriteEncrypted
(
    HDCP_SESSION *pSession
)
{
    FLCN_STATUS status   = FLCN_OK;
    LwU32       fbOffset = (HDCP_SB_SESSION_OFFSET +
                           (HDCP_GET_SESSION_INDEX(pSession->id) *
                                    HDCP_SB_SESSION_SIZE)) >> 8;

#if (HDCP_SESSION_FEATURE_ENCRYPT == HDCP_SESSION_FEATURE_ON)
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpSessionHs));

    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(hdcpSessionHs), NULL, 0);
    status = _hdcpSessionDoCrypt((LwU32)pSession, HDCP_SB_SESSION_SIZE,
                                 (LwU32)pSession, LW_TRUE);
    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpSessionHs));
#endif

    status = hdcpDmaWrite(pSession, hdcpGlobalData.sbOffset + fbOffset,
                          HDCP_SB_SESSION_SIZE);

    return status;
}

/*!
 * @brief Deactivates an active recode.
 *
 * @return FLCN_OK if the record was marked as free; FLCN_ERROR if the provided
 *         record index is not in the active record range.
 */
static FLCN_STATUS
_hdcpRecordDeactivate
(
    LwU32 recIdx
)
{
    if (recIdx < HDCP_POR_NUM_RECEIVERS)
    {
        hdcpGlobalData.activeRecs[recIdx].status    = HDCP_SESSION_FREE;
        hdcpGlobalData.activeRecs[recIdx].sessionId = 0;
        return FLCN_OK;
    }

    return FLCN_ERROR;
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Finds a free session index.
 *
 * Loops through the session status mask found in the global data. If a bit
 * set to 0 if found, that indicates that session index is free.
 *
 * @param [out] pSessionIdx     Pointer to store free index. Value is undefined
 *                              when no free session is found (return value is
 *                              LW_FALSE).
 *
 * @return Boolean value indicating if a free session was found (LW_TRUE) or
 *         not (LW_FALSE).
 */
LwBool
hdcpSessionFindFree
(
    LwU32 *pSessionIdx
)
{
    LwU32 i;
    LwU32 j;

    for (i = 0; i < HDCP_SIZE_SESSION_STATUS_MASK; i++)
    {
        if (0xFF == hdcpGlobalData.header.sessionStatusMask[i])
        {
            //
            // All indices in this portion of the mask are taken.
            // Move on to the next byte.
            //
            continue;
        }

        for (j = 0; j < 8; j++)
        {
            if (0 == (BIT(j) & hdcpGlobalData.header.sessionStatusMask[i]))
            {
                // Found a session not in use. Return the index.
                *pSessionIdx = (i * 8) + j;
                return LW_TRUE;
            }
        }
    }

    return LW_FALSE;
}

/*!
 * @brief Sets/clears the bit associated with the session ID in the global
 *        status mask.
 *
 * @param [in]  sessionId   The session ID whose bit to set/clear.
 * @param [in]  bSet        Specifies whether to set (LW_TRUE) or clear
 *                          (LW_FALSE) the bit in the global status mask.
 *
 * @return FLCN_OK if the global status bit was set or cleared. FLCN_ERROR if
 *         the session ID provided was not a valid session ID.
 */
FLCN_STATUS
hdcpSessionSetStatus
(
    LwU32   sessionId,
    LwBool  bSet
)
{
    if (sessionId > HDCP_MAX_SESSIONS)
    {
        return FLCN_ERROR;
    }

    if (LW_TRUE == bSet)
    {
        hdcpGlobalData.header.sessionStatusMask[sessionId / 8] |= BIT(sessionId % 8);
    }
    else
    {
        hdcpGlobalData.header.sessionStatusMask[sessionId / 8] &= ~BIT(sessionId % 8);
    }

    return FLCN_OK;
}

/*!
 * @brief Determines if the given session ID is active or not.
 *
 * @param [out]  pActRecInd
 *      Index in the activeRecs to the session that matches the supplied
 *      session ID. Value is undefined if the session ID is not active
 *      (return value is LW_FALSE).
 *
 * @return LW_TRUE if the supplied session ID is active; LW_FALSE if not active.
 */
LwBool
hdcpSessionIsActive
(
    LwU32   sessionId,
    LwU32  *pActRecInd
)
{
    LwU32 i;

    for (i = 0; i < HDCP_POR_NUM_RECEIVERS; i++)
    {
        if ((hdcpGlobalData.activeRecs[i].sessionId == sessionId) &&
            (hdcpGlobalData.activeRecs[i].status == HDCP_SESSION_ACTIVE))
        {
            *pActRecInd = i;
            return LW_TRUE;
        }
    }

    return LW_FALSE;
}

/*!
 * @brief Deactivates a session.
 *
 * @param[in]  pSession  Session to deactivate.
 */
FLCN_STATUS
hdcpSessionDeactivate
(
    HDCP_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;
    HDCP_SESSION_ACTIVE_ENCRYPTION *pEncState;
    LwU32       actRecIdx;
    LwU32       i;

    if (!hdcpSessionIsActive(pSession->id, &actRecIdx))
    {
        return FLCN_ERROR;
    }

    status = hdcpSessionCryptActive(actRecIdx, LW_FALSE);
    if (FLCN_OK != status)
    {
        goto hdcpSessionDeactivate_end;
    }

    pEncState = &(hdcpGlobalData.activeSessions[actRecIdx]);
    if (HDCP_SESSION_ACTIVE != pEncState->rec.status)
    {
        return FLCN_ERROR;
    }

    status = _hdcpRecordDeactivate(actRecIdx);
    if (FLCN_OK != status)
    {
        goto hdcpSessionDeactivate_end;
    }

    // Update the session with counter values
    for (i = 0; i < pEncState->encryptionState.numStreams; i++)
    {
        pSession->encryptionState.inputCtr[i] =
            pEncState->encryptionState.inputCtr[i];
        pSession->encryptionState.streamCtr[i] =
            pEncState->encryptionState.streamCtr[i];
        pEncState->encryptionState.inputCtr[i] = 0;
        pEncState->encryptionState.streamCtr[i] = 0;
    }

    memset(pEncState->encryptionState.ks, 0, HDCP_SIZE_KS);
    memset(&(pEncState->encryptionState.riv), 0, HDCP_SIZE_RIV);

    pEncState->rec.status    = HDCP_SESSION_FREE;
    pEncState->rec.sessionId = 0;

    status = hdcpSessionCryptActive(actRecIdx, LW_TRUE);
    if (FLCN_OK != status)
    {
        goto hdcpSessionDeactivate_end;
    }

    // Change the session status
    pSession->status = HDCP_SESSION_IN_USE;

hdcpSessionDeactivate_end:
    return status;
}

/*!
 * @brief Resets the session's state.
 *
 * @param[in]  pSession  Session to reset.
 */
void
hdcpSessionReset
(
    HDCP_SESSION *pSession
)
{
    pSession->stage = HDCP_AUTH_STAGE_NONE;
    hdcpSessionDeactivate(pSession);
}

/*!
 * @brief Read a session provided the session ID.
 *
 * @param [out] pSession   Pointer to the buffer that will hold the session data.
 * @param [in]  sessionId  ID of the session which needs to be read.
 *
 * @return SEC_OK if the session was read; FLCN_ERR_ILWALID_ARGUMENT if the
 *         pointer to the buffer is NULL; other errors based on return values
 *         of called functions.
 */
FLCN_STATUS
hdcpSessionReadById
(
    HDCP_SESSION **pSession,
    LwU32          sessionId
)
{
    if (NULL == pSession)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *pSession       = (HDCP_SESSION*)hdcpSessionBuffer;
    (*pSession)->id = sessionId;
    return _hdcpSessionRead(*pSession);
}

/*!
 * @brief Writes the session to the FB in encrypted form.
 *
 * @param [in]  pSession    Pointer to the session to be written.
 *
 * @return FLCN_OK if the session was written to the FB;
 *         FLCN_ERR_ILWALID_ARGUMENT if the pointer to the session was NULL;
 *         other error codes returned from called functions.
 */
FLCN_STATUS
hdcpSessionWrite
(
    HDCP_SESSION *pSession
)
{
    if (NULL == pSession)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

#if (HDCP_SESSION_FEATURE_SIGNATURE == HDCP_SESSION_FEATURE_ON)
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    sha256((LwU8*)&pSession->signature,
           ((LwU8*)(pSession) + HDCP_SIZE_SESSION_SIGNATURE),
           sizeof(HDCP_SESSION) - HDCP_SIZE_SESSION_SIGNATURE);
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
#endif

    return _hdcpSessionWriteEncrypted(pSession);
}

/*!
 * @brief Encrypts/decrypts the active session record in DMEM. Uses the same
 *        key used for session encryption.
 *
 * @param [in]  actResIdx   Active session record index.
 * @param [in]  bIsEncrypt  Specifies whether to encrypt (LW_TRUE) or decrypt
 *                          (LW_FALSE).
 *
 * @return FLCN_OK if the session record was encrypted/decrypted; otherwise,
 *         an error code returned from called functions.
 */
FLCN_STATUS
hdcpSessionCryptActive
(
    LwU32 actResIdx,
    LwBool bIsEncrypt
)
{
    FLCN_STATUS status = FLCN_OK;

#if (HDCP_SESSION_FEATURE_ENCRYPT == HDCP_SESSION_FEATURE_ON)
    LwU32 dmemOffset = (LwU32)&(hdcpGlobalData.activeSessions[actResIdx]);

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpSessionHs));

    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(hdcpSessionHs), NULL, 0);
    status = _hdcpSessionDoCrypt(dmemOffset,
                                 sizeof(HDCP_SESSION_ACTIVE_ENCRYPTION),
                                 dmemOffset, bIsEncrypt);
    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(hdcpSessionHs));
#endif // (HDCP_SESSION_FEATURE_ENCRYPT == HDCP_SESSION_FEATURE_ON)

    return status;
}
#endif
