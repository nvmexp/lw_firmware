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
 * @file   hdcp22wired_selwreactions.c
 * @brief  Hdcp22 selwreactions wrapper funcs
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "flcnifcmn.h"
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_srm.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief This is function to init internal session data.
 * @param[in]  pSession         Pointer to active HDCP22 session.
 *
 * @return     FLCN_STATUS      FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredStartSession
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS         status = FLCN_OK;
    SELWRE_ACTION_ARG  *pArg   = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();

    CHECK_STATUS((pArg ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT));
    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType = SELWRE_ACTION_START_SESSION;

    pArg->action.secActionStartSession.sorNum = pSession->sorNum;
    pArg->action.secActionStartSession.linkIndex = pSession->linkIndex;
    pArg->action.secActionStartSession.numStreams = pSession->sesVariablesRptr.numStreams;
    pArg->action.secActionStartSession.bApplyHwDrmType1LockedWar = pSession->bApplyHwDrmType1LockedWar;

    memcpy((LwU8*)pArg->action.secActionStartSession.streamIdType,
           (LwU8*)pSession->sesVariablesRptr.streamIdType,
           sizeof(HDCP22_STREAM) * HDCP22_NUM_STREAMS_MAX);

    memcpy((LwU8*)pArg->action.secActionStartSession.dpTypeMask,
           (LwU8*)pSession->sesVariablesRptr.dpTypeMask,
           sizeof(LwU32) * HDCP22_NUM_DP_TYPE_MASK);

    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_START_SESSION);
    status = HDCP22WIRED_SELWRE_ACTION(START_SESSION, pArg);
    hdcpDetachOverlay(HDCP22WIRED_OVL_START_SESSION);
    hdcpDetachOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);

    if (status == FLCN_OK)
    {
        LwU8 txCaps[HDCP22_SIZE_TX_CAPS] = HDCP22_TX_CAPS_PATTERN_BIG_ENDIAN;

        memcpy(pSession->sesVariablesAke.rTx, pArg->action.secActionStartSession.rTx,
               HDCP22_SIZE_R_TX);
        memcpy(pSession->sesVariablesSke.Rn, pArg->action.secActionStartSession.rRn,
               HDCP22_SIZE_R_N);
        memcpy(pSession->txCaps, txCaps, HDCP22_SIZE_TX_CAPS);
    }

label_return:
    return status;
}

/*!
 * @brief This is function to validate sink certification
 * @param[in]   pCert           Pointer to HDCP22_CERTIFICATE
 *
 * @return      FLCN_STATUS     FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredVerifyCertificate
(
    HDCP22_CERTIFICATE   *pCert
)
{
    FLCN_STATUS         status  = FLCN_OK;
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();

    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType = SELWRE_ACTION_VERIFY_CERTIFICATE;

    memcpy(&pArg->action.secActiolwerifyCertificate.rxCert, pCert,
           sizeof(HDCP22_CERTIFICATE));

    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_VALIDATE_CERTRX_SRM);
    status = HDCP22WIRED_SELWRE_ACTION(VERIFY_CERTIFICATE, pArg);
    hdcpDetachOverlay(HDCP22WIRED_OVL_VALIDATE_CERTRX_SRM);
    hdcpDetachOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);

    return status;
}

/*!
 * @brief This is function to reset internal session data.
 * @param[out]  pEkm            Pointer to eKm to receiver.
 * @param[in]   pRrx            Pointer to rRx from receiver.
 * @param[in]   pKpubRx         Pointer to RSA public key from receiver's certificate.
 *
 * @return      FLCN_STATUS     FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredKmKdGen
(
    LwU32  *pEkm,
    LwU64  *pRrx,
    LwU8   *pKpubRx
)
{
    FLCN_STATUS         status  = FLCN_OK;
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();

    CHECK_STATUS((pArg ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT));
    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType = SELWRE_ACTION_KMKD_GEN;
    memcpy(pArg->action.secActionKmKdGen.rRx, pRrx, HDCP22_SIZE_R_RX);
    memcpy(pArg->action.secActionKmKdGen.kPubRxEkm.kPubRx, pKpubRx,
           (HDCP22_SIZE_RX_MODULUS_8 + HDCP22_SIZE_RX_EXPONENT_8));

    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_GENERATE_KMKD);
    status = HDCP22WIRED_SELWRE_ACTION(KMKD_GEN, pArg);
    hdcpDetachOverlay(HDCP22WIRED_OVL_GENERATE_KMKD);
    hdcpDetachOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);

    if (status == FLCN_OK)
    {
        memcpy(pEkm, pArg->action.secActionKmKdGen.kPubRxEkm.eKm, HDCP22_SIZE_EKM);
    }

label_return:
    return status;
}

/*!
 * @brief This is function to validate hprime at AKE stage.
 * @param[in]   pRxCaps         Pointer to rxCaps from receiver.
 * @param[in]   pHprime         Pointer to H' from receiver.
 * @param[in]   bRepeater       Repeater flag.
 *
 * @return      FLCN_STATUS     FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredHprimeValidation
(
    LwU8   *pRxCaps,
    LwU8   *pHprime,
    LwBool  bRepeater
)
{
    FLCN_STATUS         status  = FLCN_OK;
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();

    CHECK_STATUS((pArg ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT));
    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType = SELWRE_ACTION_HPRIME_VALIDATION;
    memcpy(pArg->action.secActionHprimeValidation.rxCaps, pRxCaps, HDCP22_SIZE_TX_CAPS);
    memcpy(pArg->action.secActionHprimeValidation.hPrime, pHprime, HDCP22_SIZE_H);
    // TODO: bRepeater flag should be saved at certificateValidation action. Move that when it's implemented.
    pArg->action.secActionHprimeValidation.bRepeater = bRepeater;

    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_HPRIME_VALIDATION);
    status = HDCP22WIRED_SELWRE_ACTION(HPRIME_VALIDATION, pArg);
    hdcpDetachOverlay(HDCP22WIRED_OVL_HPRIME_VALIDATION);
    hdcpDetachOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);

label_return:
    return status;
}

/*!
 * @brief Function to validate Lprime from sink.
 * @param[in]  pRrx             Pointer to rRx from receiver.
 * @param[in]  pLprime          Pointer to Lprime array.
 *
 * @return     FLCN_STATUS      FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredLprimeValidation
(
    LwU64  *pRrx,
    LwU8   *pLprime
)
{
    FLCN_STATUS         status  = FLCN_OK;
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();

    CHECK_STATUS((pArg ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT));
    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType = SELWRE_ACTION_LPRIME_VALIDATION;
    memcpy(pArg->action.secActionLprimeValidation.rRx, pRrx, HDCP22_SIZE_R_RX);
    memcpy(pArg->action.secActionLprimeValidation.lPrime, pLprime, HDCP22_SIZE_L);

    hdcp22AttachDetachOverlay(HDCP22WIRED_LIB_SHA, LW_TRUE);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_LPRIME_VALIDATION);
    status = HDCP22WIRED_SELWRE_ACTION(LPRIME_VALIDATION, pArg);
    hdcpDetachOverlay(HDCP22WIRED_OVL_LPRIME_VALIDATION);
    hdcpDetachOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcp22AttachDetachOverlay(HDCP22WIRED_LIB_SHA, LW_FALSE);

label_return:
    return status;
}

/*!
 * @brief This is function to geneate SKE stage eKs.
 * @param[in]   pRrx            Pointer to rRx from receiver.
 * @param[out]  pEks            Pointer to output encrypted Ks to receiver at SKE stage.
 * @param[out]  pRiv            Pointer to output Riv to receiver at SKE stage.
 *
 * @return      FLCN_STATUS     FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredEksRivGen
(
    LwU64  *pRrx,
    LwU8   *pEks,
    LwU8   *pRiv
)
{
    FLCN_STATUS         status  = FLCN_OK;
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();

    CHECK_STATUS((pArg ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT));
    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType = SELWRE_ACTION_EKS_GEN;
    memcpy(pArg->action.secActionEksGen.rRx, pRrx, HDCP22_SIZE_R_RX);

    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_GENERATE_SESSIONKEY);
    status = HDCP22WIRED_SELWRE_ACTION(EKS_GEN, pArg);
    hdcpDetachOverlay(HDCP22WIRED_OVL_GENERATE_SESSIONKEY);
    hdcpDetachOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);

    if (status == FLCN_OK)
    {
        memcpy(pEks, pArg->action.secActionEksGen.eKs, HDCP22_SIZE_EKS);
        memcpy(pRiv, pArg->action.secActionEksGen.rRiv, HDCP22_SIZE_R_IV);
    }

label_return:
    return status;
}

/*!
 * @brief This is function to enable/disable HW encryption.
 * @param[in]  controlAction                Enable/disable HW encryption or update HDMI non-repeater streamType reg.
 * @param[in]  sorNum                       SOR index number.
 * @param[in]  linkIndex                    Linkx index number.
 * @param[in]  bRepeater                    Flag to tell if sink repeater or not.
 * @param[in]  bApplyHwDrmType1LockedWar    Flag to tell if applying HwDRM type1Locked WAR
 * @param[in]  bCheckCryptStatus            Flag to tell if need to wait crypt inactive at disable.
 * @param[in]  hdmiNonRepeaterStreamType    streamType for HDMI non-repeater type change case.
 * @param[in]  bIsAux                       Flag to tell if DP connection.
 *
 * @return     FLCN_STATUS          FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredControlEncryption
(
    HDCP22_ENC_CONTROL  controlAction,
    LwU8                sorNum,
    LwU8                linkIndex,
    LwBool              bApplyHwDrmType1LockedWar,
    LwBool              bCheckCryptStatus,
    LwU8                hdmiNonRepeaterStreamType,
    LwBool              bIsAux
)
{
    FLCN_STATUS         status  = FLCN_OK;
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();

    CHECK_STATUS((pArg ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT));
    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType = SELWRE_ACTION_CONTROL_ENCRYPTION;
    pArg->action.secActionControlEncryption.controlAction = controlAction;
    pArg->action.secActionControlEncryption.sorNum = sorNum;
    pArg->action.secActionControlEncryption.linkIndex = linkIndex;
    pArg->action.secActionControlEncryption.bApplyHwDrmType1LockedWar = bApplyHwDrmType1LockedWar;
    pArg->action.secActionControlEncryption.bCheckCryptStatus = bCheckCryptStatus;
    pArg->action.secActionControlEncryption.hdmiNonRepeaterStreamType = hdmiNonRepeaterStreamType;
    pArg->action.secActionControlEncryption.bIsAux = bIsAux;

    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_CONTROL_ENCRYPTION);
    status = HDCP22WIRED_SELWRE_ACTION(CONTROL_ENCRYPTION, pArg);
    hdcpDetachOverlay(HDCP22WIRED_OVL_CONTROL_ENCRYPTION);
    hdcpDetachOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);

label_return:
    return status;
}

/*!
 * @brief Function to validate Vprime from sink.
 * @param[in]  numDevices               RxIdList number of devices.
 * @param[in]  pVinput                  Pointer to V(V') input data.
 * @param[in]  pVprime                  Pointer to Vprime MSB from receiver.
 * @param[out] pVlsb                    Pointer to Vprime LSB.
 * @param[in]  pbEnforceType0Hdcp1xDS   Pointer to flag that telling if force type0 with HDCP1.X DS.
 *
 * @return     FLCN_STATUS              FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredVprimeValidation
(
    LwU8            numDevices,
    LwU8           *pVinput,
    LwU8           *pVprime,
    LwU8           *pVlsb,
    LwBool         *pbEnforceType0Hdcp1xDS
)
{
    FLCN_STATUS         status  = FLCN_OK;
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();

    CHECK_STATUS((pArg ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT));
    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType = SELWRE_ACTION_VPRIME_VALIDATION;

    pArg->action.secActiolwprimeValidation.numDevices = numDevices;
    memcpy(pArg->action.secActiolwprimeValidation.vInput, pVinput,
           (HDCP22_KSV_LIST_SIZE_MAX + HDCP22_SIZE_RX_INFO + HDCP22_SIZE_SEQ_NUM_V));
    memcpy(pArg->action.secActiolwprimeValidation.vPrime, pVprime, HDCP22_SIZE_V_PRIME);
    pArg->action.secActiolwprimeValidation.bEnforceType0Hdcp1xDS = *pbEnforceType0Hdcp1xDS;

    hdcp22AttachDetachOverlay(HDCP22WIRED_LIB_SHA, LW_TRUE);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_VPRIME_VALIDATION);
    status = HDCP22WIRED_SELWRE_ACTION(VPRIME_VALIDATION, pArg);
    hdcpDetachOverlay(HDCP22WIRED_OVL_VPRIME_VALIDATION);
    hdcpDetachOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcp22AttachDetachOverlay(HDCP22WIRED_LIB_SHA, LW_FALSE);

    if (status == FLCN_OK)
    {
        memcpy(pVlsb, pArg->action.secActiolwprimeValidation.vLsb, HDCP22_SIZE_V_PRIME);
        *pbEnforceType0Hdcp1xDS = pArg->action.secActiolwprimeValidation.bEnforceType0Hdcp1xDS;
    }

label_return:
    return status;
}

/*!
 * @brief Function to validate Lprime from sink.
 * @param[in]  numStreams       MST number of streams.
 * @param[in]  seqNumM          stream management sequence numM
 * @param[in]  MLprime          Pointer to Mprime array.
 *
 * @return     FLCN_STATUS      FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredMprimeValidation
(
    LwU16   numStreams,
    LwU32   seqNumM,
    LwU8   *pMprime
)
{
    FLCN_STATUS         status  = FLCN_OK;
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();

    CHECK_STATUS((pArg ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT));
    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType = SELWRE_ACTION_MPRIME_VALIDATION;

    pArg->action.secActionMprimeValidation.numStreams = numStreams;
    pArg->action.secActionMprimeValidation.seqNumM = seqNumM;
    memcpy(pArg->action.secActionMprimeValidation.mPrime, pMprime, HDCP22_SIZE_RPT_M);

    hdcp22AttachDetachOverlay(HDCP22WIRED_LIB_SHA, LW_TRUE);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_MPRIME_VALIDATION);
    status = HDCP22WIRED_SELWRE_ACTION(MPRIME_VALIDATION, pArg);
    hdcpDetachOverlay(HDCP22WIRED_OVL_MPRIME_VALIDATION);
    hdcpDetachOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcp22AttachDetachOverlay(HDCP22WIRED_LIB_SHA, LW_FALSE);

label_return:
    return status;
}

/*!
 * @brief Function to write DP ECF.
 * @param[in] sorNum            SOR number
 * @param[in] pEcfTimeslot      Mask array of timeSlots requiring encryption
 * @param[in] bForceClearEcf    If set, will delete time slots and then clear ECF
 * @param[in] bAddStreamBack    If set, will add back streams after deletion,
 *                              this is only valid if bForceClearEcf is set
 *
 * @return     FLCN_STATUS      FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredWriteDpEcf
(
    LwU8    sorNum,
    LwU32  *pEcfTimeslot,
    LwBool  bForceClearEcf,
    LwBool  bAddStreamBack
)
{
    FLCN_STATUS         status  = FLCN_OK;
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();

    CHECK_STATUS((pArg ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT));
    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType = SELWRE_ACTION_WRITE_DP_ECF;

    pArg->action.secActionWriteDpEcf.sorNum = sorNum;
    memcpy(pArg->action.secActionWriteDpEcf.ecfTimeslot, pEcfTimeslot,
           sizeof(pArg->action.secActionWriteDpEcf.ecfTimeslot));
    pArg->action.secActionWriteDpEcf.bForceClearEcf = bForceClearEcf;
    pArg->action.secActionWriteDpEcf.bAddStreamBack = bAddStreamBack;

    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_CONTROL_ENCRYPTION);
    status = HDCP22WIRED_SELWRE_ACTION(WRITE_DP_ECF, pArg);
    hdcpDetachOverlay(HDCP22WIRED_OVL_CONTROL_ENCRYPTION);
    hdcpDetachOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);

label_return:
    return status;
}

/*!
 * @brief This is function to reset internal session data.
 *
 * @return     FLCN_STATUS      FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredEndSession(void)
{
    FLCN_STATUS         status  = FLCN_OK;
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();

    CHECK_STATUS((pArg ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT));
    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType = SELWRE_ACTION_END_SESSION;

    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_END_SESSION);
    status = HDCP22WIRED_SELWRE_ACTION(END_SESSION, pArg);
    hdcpDetachOverlay(HDCP22WIRED_OVL_END_SESSION);
    hdcpDetachOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);

label_return:
    return status;
}

/*!
 * @brief This is function to execute hdcp1x hdcp2x SRM revocation.
 * @param[in]  pKsvList         List of KSVs to be verified.
 * @param[in]  noOfKsvs         Number of Ksvs in above list
 * @param[in]  pSrmDmaDesc      Pointer to SRM DMA descriptor
 * @param[in]   ctlOptions      Flagto tell if using debug/prod setting.
 *
 * @return      FLCN_STATUS     FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredHandleSrmRevocate
(
    LwU8               *pKsvList,
    LwU32               noOfKsvs,
    PRM_FLCN_MEM_DESC   pSrmDmaDesc,
    LwU32               ctrlOptions
)
{
    FLCN_STATUS         status  = FLCN_OK;
    SELWRE_ACTION_ARG  *pArg    = (SELWRE_ACTION_ARG *)libIntfcHdcp22wiredGetSelwreActionArgument();

    // If no device or no SRM need to check, return success directly.
    if ((!noOfKsvs) || !DMA_ADDR_IS_ZERO(pSrmDmaDesc))
    {
        return FLCN_OK;
    }

    if ((noOfKsvs > HDCP22_KSV_LIST_NUM_MAX) || (!pKsvList))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto label_return;
    }

    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));
    pArg->actionType = SELWRE_ACTION_SRM_REVOCATION;

    memcpy(pArg->action.secActionSrmRevocation.ksvList, 
           pKsvList, 
           noOfKsvs * HDCP22_SIZE_RECV_ID_8);
    pArg->action.secActionSrmRevocation.noOfKsvs = noOfKsvs;
    CHECK_STATUS(hdcp22wiredSrmRead(pSrmDmaDesc,
                                    (LwU8*)pArg->action.secActionSrmRevocation.srm, 
                                    &pArg->action.secActionSrmRevocation.totalSrmSize));
    pArg->action.secActionSrmRevocation.ctrlOptions = ctrlOptions;

    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);
    hdcpAttachAndLoadOverlay(HDCP22WIRED_OVL_VALIDATE_CERTRX_SRM);
    status = HDCP22WIRED_SELWRE_ACTION(SRM_REVOCATION, pArg);
    hdcpDetachOverlay(HDCP22WIRED_OVL_VALIDATE_CERTRX_SRM);
    hdcpDetachOverlay(HDCP22WIRED_OVL_SELWRE_ACTION);

label_return:
    return status;
}

