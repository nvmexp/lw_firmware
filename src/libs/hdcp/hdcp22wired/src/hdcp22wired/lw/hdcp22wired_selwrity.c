/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_selwrity.c
 * @brief  Hdcp22 wrapper Functions for security
 */
#ifdef GSPLITE_RTOS
/* ------------------------ System includes --------------------------------- */
/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_selwreaction.h"
#include "scp_internals.h"
#include "scp_crypt.h"

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------- Defines ---------------------------------------- */
#define HDCP22WIRED_CLEAR_SCP_REGISTER_HS(scpReg)          \
    do                                                     \
    {                                                      \
        falc_scp_secret(0, scpReg);                        \
        falc_scp_xor(scpReg, scpReg);                      \
    } while (LW_FALSE)

/* ------------------------ Function Definitions ---------------------------- */
#if defined(HDCP22_CHECK_STATE_INTEGRITY)
#if UPROC_FALCON
/*!
 * @brief This function callwlate hash value with the given buffer.
 * @param[in]  pHash            Pointer to hash value array.
 * @param[out] pData            Pointer to data.
 * @param[in]  size             Size to callwlate
 *
 * @return     FLCN_STATUS      FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22CallwateHashHs
(
    LwU32              *pHash,
    LwU32              *pData,
    LwU32               size
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       doneSize = 0;
    LwU32       keyPa;
    LwU32       hashPa = osDmemAddressVa2PaGet(pHash);

    if ((pHash == NULL) || (pData == NULL ) || (size % SCP_BUF_ALIGNMENT) ||
        (hashPa % SCP_BUF_ALIGNMENT))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    while (doneSize < size)
    {
        keyPa = osDmemAddressVa2PaGet(&(pData[doneSize/sizeof(LwU32)]));
        if (keyPa % SCP_BUF_ALIGNMENT)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto label_return;
        }

        // H_i = H_{i-1} XOR encrypt(key = m_i, pMessage = H_{i-1})
        falc_scp_trap(TC_INFINITY);
        falc_trapped_dmwrite(falc_sethi_i((LwU32)keyPa, SCP_R2));
        falc_dmwait();
        falc_trapped_dmwrite(falc_sethi_i((LwU32)hashPa, SCP_R3));
        falc_dmwait();
        falc_scp_key(SCP_R2);
        falc_scp_encrypt(SCP_R3, SCP_R4);
        falc_scp_xor(SCP_R4, SCP_R3);
        falc_trapped_dmread(falc_sethi_i((LwU32)hashPa, SCP_R3));
        falc_dmwait();
        falc_scp_trap(0);

        doneSize += 16;
    }

label_return:
    // Scrub SCP registers to avoid leaking any secret data
    // We are loading first index zero, to make sure that scrubbing operation do not get dropped.
    HDCP22WIRED_CLEAR_SCP_REGISTER_HS(SCP_R2);
    HDCP22WIRED_CLEAR_SCP_REGISTER_HS(SCP_R3);
    HDCP22WIRED_CLEAR_SCP_REGISTER_HS(SCP_R4);

    return status;
}
#endif
#endif

#if defined(HDCP22_USE_SCP_GEN_DKEY)
#if UPROC_FALCON
/*!
 * hdcp22GenerateDkeyScpHs
 *
 * @brief Generate derived key with scp instruction.
 *
 * @param[in]  pMessage          The message to be encrypted
 * @param[in]  pKey              The Key with which message is to be encrypted
 * @param[out] pEncryptMessage   The encrypted Message
 *
 * @return     FLCN_STATUS       FLCN_OK if succeed else error.
 */
FLCN_STATUS
hdcp22GenerateDkeyScpHs
(
    LwU8 *pMessage,
    LwU8 *pKey,
    LwU8 *pEncryptMessage
)
{
    LwU32       messagePa;
    LwU32       keyPa;
    LwU32       encryptMessagePa;

    // Check for NULL pointers
    if ((NULL == pMessage) ||
        (NULL == pKey) ||
        (NULL == pEncryptMessage))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    messagePa = osDmemAddressVa2PaGet(pMessage);
    keyPa = osDmemAddressVa2PaGet(pKey);
    encryptMessagePa = osDmemAddressVa2PaGet(pEncryptMessage);

    // Check physical memory alignment.
    if (((messagePa % SCP_BUF_ALIGNMENT) != 0) ||
        ((keyPa % SCP_BUF_ALIGNMENT) != 0) ||
        ((encryptMessagePa % SCP_BUF_ALIGNMENT) != 0))
    {
        return FLCN_ERR_ILLEGAL_OPERATION;
    }

    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i(messagePa, SCP_R1));
    falc_trapped_dmwrite(falc_sethi_i(keyPa, SCP_R2));
    falc_dmwait();
    falc_scp_key(SCP_R2);
    falc_scp_encrypt(SCP_R1, SCP_R4);
    falc_trapped_dmread(falc_sethi_i(encryptMessagePa, SCP_R4));
    falc_dmwait();
    falc_scp_trap(0);

    // Scrub SCP registers to avoid leaking any secret data
    // We are loading first index zero, to make sure that scrubbing operation do not get dropped.
    HDCP22WIRED_CLEAR_SCP_REGISTER_HS(SCP_R1);
    HDCP22WIRED_CLEAR_SCP_REGISTER_HS(SCP_R2);
    HDCP22WIRED_CLEAR_SCP_REGISTER_HS(SCP_R4);

    return FLCN_OK;
}
#endif
#endif

/*!
 * @brief This is a function for accessing L3 registers from LS mode by switching to HS. 
 *        TODO: need to remove this LS level API ( add Initialize selwreAction, and revisit
 *              selwreECF hdcp22_ctrl lock write ).
 *
 * @param[in]  bIsReadReq        Is secure bus read request or write request
 * @param[in]  addr              address of register to be read or written
 * @param[in]  dataIn            Value to be written (not applicable for read request)
 * @param[out] pDataOut          Pointer to be filled with read value (not applicable for write request)
 *
 * @return     FLCN_STATUS       FLCN_OK if read or write request is successful
 */
FLCN_STATUS
hdcp22wiredSelwreRegAccessFromLs
(
    LwBool  bIsReadReq,
    LwU32   addr,
    LwU32   dataIn,
    LwU32  *pDataOut
)
{
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();
    FLCN_STATUS         status  = FLCN_OK;

    if((pArg == NULL) || (bIsReadReq && pDataOut == NULL))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType                      = SELWRE_ACTION_REG_ACCESS;
    pArg->action.regAccessArg.bIsReadReq  = bIsReadReq;
    pArg->action.regAccessArg.addr        = addr;
    pArg->action.regAccessArg.val         = dataIn;

    status = HDCP22WIRED_SELWRE_ACTION(REG_ACCESS, pArg);

    if (bIsReadReq)
    {
        // To read request, retVal is filled up in above function.
        if (status == FLCN_OK)
        {
            *pDataOut = pArg->action.regAccessArg.retVal;
        }
        else
        {
            *pDataOut  = 0xbadfffff;
        }
    }

    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));

    return status;
}

/*!
 * @brief This is a function to callwate hash value of data
 * secret in HS mode
 * @param[out] pHash            Pointer to hash buffer.
 * @param[in]  pData            Pointer to input data.
 * @param[in]  size             Size to calucate.
 *
 * @return     FLCN_STATUS      FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredDoCallwlateHash
(
    LwU32              *pHash,
    LwU32              *pData,
    LwU32               size
)
{
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();
    FLCN_STATUS         status  = FLCN_OK;

    if((pArg == NULL) || (pHash == NULL) || (pData == NULL) || 
       (size > MAX_HDCP22_INTEGRITY_CHECK_SIZE))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType                                  = SELWRE_ACTION_SCP_CALLWLATE_HASH;
    pArg->action.scpCallwlateHashArg.inputSizeinBytes = LW_ALIGN_UP(size, SCP_BUF_ALIGNMENT);
    memcpy(pArg->action.scpCallwlateHashArg.inputData, pData, size);

    status = libIntfcHdcp22wiredSelwreAction(pArg);

    memcpy(pHash, pArg->action.scpCallwlateHashArg.outHash,
           HDCP22_SIZE_INTEGRITY_HASH);
    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));

    return status;
}
/*!
 * @brief This is a function for generating dkey in HS mode
 * @param[in]  pMessage         The message to be encrypted
 * @param[in]  pKey             The Key with which message is to be encrypted
 * @param[out] pEncryptMessage  The encrypted Message
 *
 * @return     FLCN_STATUS      FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredGenerateDkey
(
    LwU8 *pMessage,
    LwU8 *pKey,
    LwU8 *pEncryptMessage
)
{
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();
    FLCN_STATUS         status  = FLCN_OK;

    if((pArg == NULL) || (pMessage == NULL) || (pKey == NULL) || (pEncryptMessage == NULL))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType = SELWRE_ACTION_SCP_GEN_DKEY;

    memcpy(pArg->action.scpGenDkeyArg.message, pMessage, HDCP22_SIZE_DKEY);
    memcpy(pArg->action.scpGenDkeyArg.key, pKey, HDCP22_SIZE_DKEY);

    status = libIntfcHdcp22wiredSelwreAction(pArg);

    memcpy(pEncryptMessage, pArg->action.scpGenDkeyArg.encryptMessage,
           HDCP22_SIZE_DKEY);
    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));

    return status;
}
#endif

