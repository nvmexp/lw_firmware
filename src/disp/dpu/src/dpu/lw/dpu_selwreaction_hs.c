/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_selwreaction_hs.c
 * @brief  HS routines for secure actions for all tasks
 */

/* ------------------------- System Includes -------------------------------- */
#include "gsp_hs.h"
/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "dpu_objdpu.h"
#include "secbus_hs.h"
#include "hdcp22wired_selwreaction.h"
/* ------------------------- External Defines ----------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS dpuSelwreActionHsEntry(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "start")
    GCC_ATTRIB_USED();
static FLCN_STATUS _dpuSelwreActionHsLibValidation(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "_dpuSelwreActionHsLibValidation");
// Entry function for new LS/HS secure actions.
static FLCN_STATUS _dpuNewSelwreActionHandler(SELWRE_ACTION_ARG  *pArg)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "_dpuNewSelwreActionHandler");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief help function to validate HS libs according to action type.
 *
 * @param[in]  pArg     Pointer to secure action argument.
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
static FLCN_STATUS
_dpuSelwreActionHsLibValidation
(
    SELWRE_ACTION_ARG  *pArg
)
{
    FLCN_STATUS status = FLCN_OK;

    // Common HS libs validation.
    OS_SEC_HS_LIB_VALIDATE(libSecBusHs, HASH_SAVING_DISABLE);
#ifndef HDCP22_KMEM_ENABLED
    OS_SEC_HS_LIB_VALIDATE(libScpCryptHs, HASH_SAVING_DISABLE);
#endif
    OS_SEC_HS_LIB_VALIDATE(libMemHs, HASH_SAVING_DISABLE);
    OS_SEC_HS_LIB_VALIDATE(libCommonHs, HASH_SAVING_DISABLE);
#ifdef LIB_CCC_PRESENT
     OS_SEC_HS_LIB_VALIDATE(libCccHs, HASH_SAVING_DISABLE);
#endif

    switch (pArg->actionType)
    {
        case SELWRE_ACTION_REG_ACCESS:
        case SELWRE_ACTION_SCP_CALLWLATE_HASH:
        case SELWRE_ACTION_SCP_GEN_DKEY:
            // No extra HS libs need to validate.
            break;

        case SELWRE_ACTION_START_SESSION:
            OS_SEC_HS_LIB_VALIDATE(libHdcp22StartSessionHs, HASH_SAVING_DISABLE);
            break;
        case SELWRE_ACTION_VERIFY_CERTIFICATE:
        case SELWRE_ACTION_SRM_REVOCATION:
            OS_SEC_HS_LIB_VALIDATE(libShaHs, HASH_SAVING_DISABLE);
            // SRM1X DSA signature needs libBigIntHs even with HW RSA for SRM2X.
            OS_SEC_HS_LIB_VALIDATE(libBigIntHs, HASH_SAVING_DISABLE);
            OS_SEC_HS_LIB_VALIDATE(libHdcp22ValidateCertrxSrmHs, HASH_SAVING_DISABLE);
            break;
        case SELWRE_ACTION_KMKD_GEN:
            OS_SEC_HS_LIB_VALIDATE(libShaHs, HASH_SAVING_DISABLE);
#ifndef HDCP22_USE_HW_RSA
            OS_SEC_HS_LIB_VALIDATE(libBigIntHs, HASH_SAVING_DISABLE);
#endif
            OS_SEC_HS_LIB_VALIDATE(libHdcp22GenerateKmkdHs, HASH_SAVING_DISABLE);
            break;
        case SELWRE_ACTION_HPRIME_VALIDATION:
            OS_SEC_HS_LIB_VALIDATE(libShaHs, HASH_SAVING_DISABLE);
            OS_SEC_HS_LIB_VALIDATE(libHdcp22HprimeValidationHs, HASH_SAVING_DISABLE);
            break;
        case SELWRE_ACTION_LPRIME_VALIDATION:
            OS_SEC_HS_LIB_VALIDATE(libShaHs, HASH_SAVING_DISABLE);
            OS_SEC_HS_LIB_VALIDATE(libHdcp22LprimeValidationHs, HASH_SAVING_DISABLE);
            break;
        case SELWRE_ACTION_EKS_GEN:
            OS_SEC_HS_LIB_VALIDATE(libHdcp22GenerateSessionkeyHs, HASH_SAVING_DISABLE);
            break;
        case SELWRE_ACTION_CONTROL_ENCRYPTION:
        case SELWRE_ACTION_WRITE_DP_ECF:
            OS_SEC_HS_LIB_VALIDATE(libHdcp22ControlEncryptionHs, HASH_SAVING_DISABLE);
            break;
        case SELWRE_ACTION_VPRIME_VALIDATION:
            OS_SEC_HS_LIB_VALIDATE(libShaHs, HASH_SAVING_DISABLE);
            OS_SEC_HS_LIB_VALIDATE(libHdcp22VprimeValidationHs, HASH_SAVING_DISABLE);
            break;
        case SELWRE_ACTION_MPRIME_VALIDATION:
            OS_SEC_HS_LIB_VALIDATE(libShaHs, HASH_SAVING_DISABLE);
            OS_SEC_HS_LIB_VALIDATE(libHdcp22MprimeValidationHs, HASH_SAVING_DISABLE);
            break;
        case SELWRE_ACTION_END_SESSION:
            OS_SEC_HS_LIB_VALIDATE(libHdcp22EndSessionHs, HASH_SAVING_DISABLE);
            break;
        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }

    return status;
}

/*!
 * @brief Entry function of new LS/HS SelwreActions.
 *
 * @param[in]  pArg     Pointer to secure action argument.
 *
 *  This function provides new secure action handler according to action type:
 *  - SELWRE_ACTION_START_SESSION: start hdcp22 authentication session and initialize internal secrets.
 *  - SELWRE_ACTION_KMKD_GEN: hdcp22 authentication AKE stage to generate km, dkey.
 *  - SELWRE_ACTION_HPRIME_VALIDATION: hdcp22 authentication AKE stage to validate sink replied hprime.
 *  - SELWRE_ACTION_LPRIME_VALIDATION: hdcp22 authentication LC stage to validate sink replied lprime.
 *  - SELWRE_ACTION_EKS_GEN: hdcp22 authentication SKE stage to generate encrypted ks.
 *  - SELWRE_ACTION_CONTROL_ENCRYPTION: hdcp22 HW control to enable/disable encryption.
 *  - SELWRE_ACTION_VPRIME_VALIDATION: hdcp22 authentication REPEATER stage to validate sink replied vprime.
 *  - SELWRE_ACTION_MPRIME_VALIDATION: hdcp22 authentication REPEATER stage to validate sink replied mprime.
 *  - SELWRE_ACTION_WRITE_DP_ECF: hdcp22 write DP ECF.
 *  - SELWRE_ACTION_END_SESSION: end hdcp22 authentication session and reset internal secrets.
 */
static FLCN_STATUS
_dpuNewSelwreActionHandler
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
 *  - SELWRE_ACTION_KMKD_GEN: hdcp22 authentication AKE stage to generate km, dkey.
 *  - SELWRE_ACTION_HPRIME_VALIDATION: hdcp22 authentication AKE stage to validate sink replied hprime.
 *  - SELWRE_ACTION_LPRIME_VALIDATION: hdcp22 authentication LC stage to validate sink replied lprime.
 *  - SELWRE_ACTION_EKS_GEN: hdcp22 authentication SKE stage to generate encrypted ks.
 *  - SELWRE_ACTION_CONTROL_ENCRYPTION: hdcp22 HW control to enable/disable encryption.
 *  - SELWRE_ACTION_VPRIME_VALIDATION: hdcp22 authentication REPEATER stage to validate sink replied vprime.
 *  - SELWRE_ACTION_MPRIME_VALIDATION: hdcp22 authentication REPEATER stage to validate sink replied mprime.
 *  - SELWRE_ACTION_WRITE_DP_ECF: hdcp22 write DP ECF.
 *  - SELWRE_ACTION_END_SESSION: end hdcp22 authentication session and reset internal secrets.
 *
 * @param[in]  pArg                     pointer to secure action argument.
 */
FLCN_STATUS
dpuSelwreActionHsEntry
(
    SELWRE_ACTION_ARG  *pArg
)
{
    FLCN_STATUS status = FLCN_ERROR;

    // Validate HS libs.
    status = _dpuSelwreActionHsLibValidation(pArg);

    if (status != FLCN_OK)
    {
        goto dpuSelwreActionHsEntry_exit;
    }

    // Make sure all HS Common Pre Checks are passing
    status = _enterHS();
    if (status != FLCN_OK)
    {
        goto dpuSelwreActionHsEntry_exit;
    }

    // Make sure all HDCP specific HS Pre Checks are passing
    status = dpuHsHdcpPreCheck_HAL(&Dpu.hal, pArg);
    if (status != FLCN_OK)
    {
        goto dpuSelwreActionHsEntry_exit;
    }

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

                if((pScpCallwlateHashArg->inputSizeinBytes % SCP_BUF_ALIGNMENT) ||
                   (pScpCallwlateHashArg->inputSizeinBytes > MAX_HDCP22_INTEGRITY_CHECK_SIZE))
                {
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                    goto dpuSelwreActionHsEntry_exit;
                }

                status = hdcp22CallwateHashHs(pScpCallwlateHashArg->outHash,
                                              pScpCallwlateHashArg->inputData,
                                              pScpCallwlateHashArg->inputSizeinBytes);
#else
                status = FLCN_ERR_NOT_SUPPORTED;
#endif
            }
            break;

        case SELWRE_ACTION_SCP_GEN_DKEY:
            {
#if defined(HDCP22_USE_SCP_GEN_DKEY)
                PSELWRE_ACTION_SCP_GEN_DKEY_ARG  pScpGenDkeyArg = &pArg->action.scpGenDkeyArg;

                status = hdcp22GenerateDkeyScpHs(pScpGenDkeyArg->message,
                                                 pScpGenDkeyArg->key,
                                                 pScpGenDkeyArg->encryptMessage);
#else
                status = FLCN_ERR_NOT_SUPPORTED;
#endif
            }
            break;

        default:
            // Continue to check new LS/HS secure actions.
            status = _dpuNewSelwreActionHandler(pArg);
            break;
    }

dpuSelwreActionHsEntry_exit:

    // Make sure to complete HS Exit Sequence before returning to LS
    _exitHS();

    return status;
}

