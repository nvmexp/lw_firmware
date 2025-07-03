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
 * @file   gsp_selwreaction_hs.c
 * @brief  HS routines for secure actions for all tasks
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "flcntypes.h"

/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "setypes.h"
#include "secbus_hs.h"
#include "gsphs.h"
#include "hdcp22wired_selwreaction.h"

/* ------------------------- External Defines ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS gspSelwreActionHsEntry(SELWRE_ACTION_ARG *pArg);
extern FLCN_STATUS hdcp22ConfidentialSecretDoCryptHs(LwU32 *pRn, LwU32 *pSecret, LwU32 *pDest, LwU32 size, LwBool bIsEncrypt);
extern FLCN_STATUS hdcp22CallwateHashHs(LwU32 *pHash, LwU32 *pData, LwU32 size);
extern FLCN_STATUS hdcp22GenerateDkeyScpHs(LwU8 *pMessage, LwU8 *pKey, LwU8 *pEncryptMessage);

// Entry function for new LS/HS secure actions.
static FLCN_STATUS _gspNewSelwreActionHandler(SELWRE_ACTION_ARG  *pArg)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "_gspNewSelwreActionHandler");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Entry function of the HS SelwreAction overlay.
 *        It is used to validate the overlay blocks corresponding to the secure
 *  overlay before jumping to arbitrary functions inside it. The linker script
 *  should make sure that it puts this section at the top of the overlay to
 *  ensure that we can jump to the overlay's 0th offset before exelwting code
 *  at arbitrary offsets inside the overlay.
 *
 *  This function provides secure action according to action type:
 *  - SELWRE_ACTION_REG_ACCESS: secure access to the read and write of registers
 *      through secure bus.
 *  - SELWRE_ACTION_SCP_CRYPT_SECRET: secure using scp to encrypt/decrypt
 *      confidential secrets.
 *  - SELWRE_ACTION_SCP_CALLWLATE_HASH: secure using scp to generate hash value
 *      for integrity check.
 *  - SELWRE_ACTION_SCP_GEN_DKEY: secure using scp to generate dkey.
 *  - SELWRE_ACTION_START_SESSION: start hdcp22 authentication session and initialize internal secrets.
 *  - SELWRE_ACTION_END_SESSION: end hdcp22 authentication session and reset internal secrets.
 *
 * @param[in]  pArg                     pointer to secure action argument.
 */
FLCN_STATUS
gspSelwreActionHsEntry
(
    SELWRE_ACTION_ARG  *pArg
)
{
    FLCN_STATUS status              = FLCN_OK;
    LwU8       *pArgMemCheckStart   = NULL;
    LwU8       *pArgMemCheckEnd     = NULL;

    // TODO (Bug 200471270) - need to add the SSP support

    if ((pArg == NULL) ||
        ((pArg->actionType != SELWRE_ACTION_REG_ACCESS) &&
         (pArg->actionType != SELWRE_ACTION_SCP_CALLWLATE_HASH) &&
         (pArg->actionType != SELWRE_ACTION_SCP_GEN_DKEY) &&
         (pArg->actionType != SELWRE_ACTION_START_SESSION) &&
         (pArg->actionType != SELWRE_ACTION_VERIFY_CERTIFICATE) &&
         (pArg->actionType != SELWRE_ACTION_KMKD_GEN) &&
         (pArg->actionType != SELWRE_ACTION_HPRIME_VALIDATION) &&
         (pArg->actionType != SELWRE_ACTION_LPRIME_VALIDATION) &&
         (pArg->actionType != SELWRE_ACTION_EKS_GEN) &&
         (pArg->actionType != SELWRE_ACTION_CONTROL_ENCRYPTION) &&
         (pArg->actionType != SELWRE_ACTION_VPRIME_VALIDATION) &&
         (pArg->actionType != SELWRE_ACTION_MPRIME_VALIDATION) &&
         (pArg->actionType != SELWRE_ACTION_WRITE_DP_ECF) &&
         (pArg->actionType != SELWRE_ACTION_END_SESSION) &&
         (pArg->actionType != SELWRE_ACTION_SRM_REVOCATION)))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto gspSelwreActionHsEntry_exit;
    }

    // Check input argument structure that should be zero besides action struct.
    pArgMemCheckEnd = (LwU8*)&pArg->actionType;
    switch (pArg->actionType)
    {
        case SELWRE_ACTION_REG_ACCESS:
            pArgMemCheckStart = ((LwU8*)&pArg->action.regAccessArg) + sizeof(SELWRE_ACTION_REG_ACCESS_ARG);
            break;
        case SELWRE_ACTION_SCP_CALLWLATE_HASH:
            pArgMemCheckStart = ((LwU8*)&pArg->action.scpCallwlateHashArg) + sizeof(SELWRE_ACTION_SCP_CALLWLATE_HASH_ARG);
            break;
        case SELWRE_ACTION_SCP_GEN_DKEY:
            pArgMemCheckStart = ((LwU8*)&pArg->action.scpGenDkeyArg) + sizeof(SELWRE_ACTION_SCP_GEN_DKEY_ARG);
            break;

        // new actions for LS/HS reorg.
        case SELWRE_ACTION_START_SESSION:
            pArgMemCheckStart = ((LwU8*)&pArg->action.secActionStartSession) + sizeof(SELWRE_ACTION_START_SESSION_ARG);
            break;
        case SELWRE_ACTION_VERIFY_CERTIFICATE:
            pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_VERIFY_CERTIFICATE_ARG);
            break;
        case SELWRE_ACTION_KMKD_GEN:
            pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_KMKD_GEN_ARG);
            break;
        case SELWRE_ACTION_HPRIME_VALIDATION:
            pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_HPRIME_VALIDATION_ARG);
            break;
        case SELWRE_ACTION_LPRIME_VALIDATION:
            pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_LPRIME_VALIDATION_ARG);
            break;
        case SELWRE_ACTION_EKS_GEN:
            pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_EKS_GEN_ARG);
            break;
        case SELWRE_ACTION_CONTROL_ENCRYPTION:
            pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_CONTROL_ENCRYPTION_ARG);
            break;
        case SELWRE_ACTION_VPRIME_VALIDATION:
            pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_VPRIME_VALIDATION_ARG);
            break;
        case SELWRE_ACTION_MPRIME_VALIDATION:
            pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_MPRIME_VALIDATION_ARG);
            break;
        case SELWRE_ACTION_WRITE_DP_ECF:
            pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_WRITE_DP_ECF_ARG);
            break;
        case SELWRE_ACTION_END_SESSION:
            pArgMemCheckStart = (LwU8*)&pArg->action.secActionStartSession;
        case SELWRE_ACTION_SRM_REVOCATION:
            pArgMemCheckStart = ((LwU8*)&((SELWRE_ACTION_ARG *)pArg)->action) + sizeof(SELWRE_ACTION_SRM_REVOCATION_ARG);
            break;
        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto gspSelwreActionHsEntry_exit;
            break;
    }

    for( ;pArgMemCheckStart < pArgMemCheckEnd; pArgMemCheckStart++)
    {
        if (*pArgMemCheckStart != 0x00)
        {
            status = FLCN_ERR_HS_SELWRE_ACTION_ARG_CHECK_FAILED;
            goto gspSelwreActionHsEntry_exit;
        }
    }

    //
    // TODO (Bug 200471270) - need to re-enable the HS-precheck after
    // all the security requirement is met (e.g. ACR LS support)
    //
    //if ((status = dpuHsHdcpPreCheck_HAL(&Dpu.hal, pArg)) != FLCN_OK)
    //{
    //    goto dpuSelwreActionHsEntry_exit;
    //}

    switch (pArg->actionType)
    {
        case SELWRE_ACTION_REG_ACCESS:
            {
                SE_STATUS                     secBusStatus  = SE_OK;
                PSELWRE_ACTION_REG_ACCESS_ARG pRegAccessArg = &pArg->action.regAccessArg;
                LwU32                         addr          = pRegAccessArg->addr;

                // Secure Bus access (read or write)
                if (pRegAccessArg->bIsReadReq)
                {
                    secBusStatus = SE_SECBUS_REG_RD32_HS_ERRCHK(addr, &pRegAccessArg->retVal);
                }
                else
                {
                    secBusStatus = SE_SECBUS_REG_WR32_HS_ERRCHK(addr, pRegAccessArg->val);
                }

                if (secBusStatus != SE_OK)
                {
                    if (secBusStatus == SE_ERR_SECBUS_READ_ERROR)
                    {
                        status = FLCN_ERR_SELWREBUS_REGISTER_READ_ERROR;
                    }
                    else if (secBusStatus == SE_ERR_SECBUS_WRITE_ERROR)
                    {
                        status = FLCN_ERR_SELWREBUS_REGISTER_WRITE_ERROR;
                    }
                    else
                    {
                        status = FLCN_ERROR;
                    }
                }
            }
            break;

        case SELWRE_ACTION_SCP_CALLWLATE_HASH:
            {
#if defined(HDCP22_CHECK_STATE_INTEGRITY)
                PSELWRE_ACTION_SCP_CALLWLATE_HASH_ARG pScpCallwlateHashArg = &pArg->action.scpCallwlateHashArg;

                if ((pScpCallwlateHashArg->inputSizeinBytes % SCP_BUF_ALIGNMENT) ||
                    (pScpCallwlateHashArg->inputSizeinBytes > MAX_HDCP22_INTEGRITY_CHECK_SIZE))
                {
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                    goto gspSelwreActionHsEntry_exit;
                }

                status = hdcp22CallwateHashHs(pScpCallwlateHashArg->outHash,
                                              pScpCallwlateHashArg->inputData,
                                              pScpCallwlateHashArg->inputSizeinBytes);
#else
                status = FLCN_ERR_NOT_SUPPORTED;
#endif
            }
            break;

        default:
            // Continue to check new LS/HS secure actions.
            status = _gspNewSelwreActionHandler(pArg);

            break;
    }

gspSelwreActionHsEntry_exit:

    return status;
}


/*!
 * @brief Entry function of new LS/HS SelwreActions.
 *
 * @param[in]  pArg     Pointer to secure action argument.
 *  This function provides new secure action handler according to action type:
 *  - SELWRE_ACTION_START_SESSION: start hdcp22 authentication session and initialize internal secrets.
 *  - SELWRE_ACTION_END_SESSION: end hdcp22 authentication session and reset internal secrets.
 */
static FLCN_STATUS
_gspNewSelwreActionHandler
(
    SELWRE_ACTION_ARG  *pArg
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (pArg->actionType)
    {
        case SELWRE_ACTION_START_SESSION:
            status = hdcp22wiredStartSessionHandler(pArg);
            break;
        case SELWRE_ACTION_VERIFY_CERTIFICATE:
            status = hdcp22wiredVerifyRxCertificateHandler(pArg);
            break;
        case SELWRE_ACTION_KMKD_GEN:
            status = hdcp22wiredKmKdGenHandler(pArg);
            break;
        case SELWRE_ACTION_HPRIME_VALIDATION:
            status = hdcp22wiredHprimeValidationHandler(pArg);
            break;
        case SELWRE_ACTION_LPRIME_VALIDATION:
            status = hdcp22wiredLprimeValidationHandler(pArg);
            break;
        case SELWRE_ACTION_EKS_GEN:
            status = hdcp22wiredEksGenHandler(pArg);
            break;
        case SELWRE_ACTION_CONTROL_ENCRYPTION:
            status = hdcp22wiredControlEncryptionHandler(pArg);
            break;
        case SELWRE_ACTION_VPRIME_VALIDATION:
            status = hdcp22wiredVprimeValidationHandler(pArg);
            break;
        case SELWRE_ACTION_MPRIME_VALIDATION:
            status = hdcp22wiredMprimeValidationHandler(pArg);
            break;
        case SELWRE_ACTION_WRITE_DP_ECF:
            status = hdcp22wiredWriteDpEcfHandler(pArg);
            break;
        case SELWRE_ACTION_END_SESSION:
            status = hdcp22wiredEndSessionHandler(pArg);
            break;
        case SELWRE_ACTION_SRM_REVOCATION:
            status = hdcp22wiredSrmRevocationHandler(pArg);
            break;
        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }

    return status;
}

