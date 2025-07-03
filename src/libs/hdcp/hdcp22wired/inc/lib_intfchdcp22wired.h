/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lib_intfchdcp22wired.h
 */

#ifndef LIB_INTFCHDCP22WIRED_H
#define LIB_INTFCHDCP22WIRED_H

/* ------------------------ System includes --------------------------------- */
#include "lwuproc.h"
#include "flcntypes.h"
#include <string.h>

/* ------------------------ Application includes ---------------------------- */
#include "rmflcncmdif.h"
#include "lib_hdcp22wired.h"
#include "lib_intfchdcp_cmn.h"
#include "unit_dispatch.h"
#include "hdcp22wired_selwreaction.h"
#include "hdcp22wired_timer.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief Structure used to pass HDCP related command and events
 */
typedef union
{
    LwU8                     eventType;
#if defined(GSPLITE_RTOS)
    DISP2UNIT_HDR   hdr;
#endif
    DISP2UNIT_CMD            command;
    HDCP22_EVT               hdcp22Evt;
} DISPATCH_HDCP22WIRED;

/* ------------------------ Prototypes  ------------------------------------- */
FLCN_STATUS dpauxAccess(DPAUX_CMD *pauxTransactionCmd, LwBool bRead);
FLCN_STATUS i2cPerformTransaction(I2C_TRANSACTION *pi2cTransaction);
LwBool hdcp22WiredIsSec2TypeReqSupported(void);
void* libIntfcHdcp22wiredGetSelwreActionArgument(void)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "libIntfcHdcp22wiredGetSelwreActionArgument");
FLCN_STATUS libIntfcHdcp22wiredSelwreAction(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "libIntfcHdcp22wiredSelwreAction");
void hdcp22wiredInitialize(void)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredInitialize");
FLCN_STATUS hdcp22wiredProcessCmd(HDCP22_SESSION *pSession, RM_FLCN_CMD *pCmd, LwU8 seqNumId, LwU8 cmdQueueId)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredProcessCmd");
FLCN_STATUS hdcp22wiredHandleTimerEvent(HDCP22_SESSION *pSession, HDCP22_TIMER_EVT_TYPE timerEvtType)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredHandleTimerEvent");
FLCN_STATUS hdcp22wiredHandleSorInterrupt(LwU8 sorNumber)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredHandleSorInterrupt");
FLCN_STATUS hdcp22wiredProcessSec2Req (void)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredProcessSec2Req");
FLCN_STATUS hdcp22wiredInitSorHdcp22CtrlReg(void)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredInitSorHdcp22CtrlReg");
FLCN_STATUS hdcp22wiredIsType1LockActive(LwBool *pBType1LockActive)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredIsType1LockActive");
FLCN_STATUS hdcp22CallwateHashHs(LwU32 *pHash, LwU32 *pData, LwU32 size)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22CallwateHashHs");
FLCN_STATUS hdcp22GenerateDkeyScpHs(LwU8 *pMessage, LwU8 *pKey, LwU8 *pEncryptMessage)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22GenerateDkeyScpHs");
FLCN_STATUS hdcp22wiredStartSession(HDCP22_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredStartSession");
FLCN_STATUS hdcp22RsaVerifyDcpSignature(LwU8 *pData, LwU32 dataLength, LwU8 *pSignature)
    GCC_ATTRIB_SECTION("imem_hdcp22RsaSignature", "hdcp22RsaVerifyDcpSignature");
FLCN_STATUS hdcp22wiredVerifyCertificate(HDCP22_CERTIFICATE *pCert)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredCertrxSrm", "hdcp22wiredVerifyCertificate");
FLCN_STATUS hdcp22DsaVerifySignature(LwU8 *pData, LwU32 dataLength, LwU8 *pSignature, LwU32  ctlOptions)
    GCC_ATTRIB_SECTION("imem_hdcp22DsaSignature", "hdcp22DsaVerifySignature");
FLCN_STATUS hdcp22wiredKmKdGen(LwU32 *pEkm, LwU64 *pRrx, LwU8 *pKpubRx)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredKmKdGen");
FLCN_STATUS hdcp22wiredHprimeValidation(LwU8 *pRxCaps, LwU8 *pHprime, LwBool bRepeater)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredHprimeValidation");
FLCN_STATUS hdcp22wiredLprimeValidation(LwU64 *pRrx, LwU8 *pLprime)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredLc", "hdcp22wiredLprimeValidation");
FLCN_STATUS hdcp22wiredEksRivGen(LwU64 *pRrx, LwU8 *pEks, LwU8 *pRiv)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredEksGen");
FLCN_STATUS hdcp22wiredControlEncryption(HDCP22_ENC_CONTROL controlAction, LwU8 sorNum, LwU8 linkIndex, LwBool bApplyHwDrmType1LockedWar, LwBool bCheckCryptStatus, LwU8 hdmiNoRepeaterStreamType, LwBool bIsAux)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredControlEncryption");
FLCN_STATUS hdcp22wiredVprimeValidation(LwU8 numDevices, LwU8 *pVinput, LwU8 *pVprime, LwU8 *pVlsb, LwBool *pbEnforceType0Hdcp1xDS)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredRepeater", "hdcp22wiredVprimeValidation");
FLCN_STATUS hdcp22wiredMprimeValidation(LwU16 numStreams, LwU32 seqNumM, LwU8 *pMprime)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredRepeater", "hdcp22wiredMprimeValidation");
FLCN_STATUS hdcp22wiredWriteDpEcf(LwU8 sorNum, LwU32 *pEcfTimeslot, LwBool bForceClearEcf, LwBool bAddStreamBack)
    GCC_ATTRIB_SECTION("imem_libDpaux", "hdcp22wiredWriteDpEcf");
FLCN_STATUS hdcp22wiredEndSession(void)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredEndSession");
FLCN_STATUS hdcp22wiredHandleSrmRevocate(LwU8 *pKsvList, LwU32 noOfKsvs, PRM_FLCN_MEM_DESC pSrmDmaDesc,LwU32 ctrlOptions)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredCertrxSrm", "hdcp22wiredHandleSrmRevocate");
FLCN_STATUS hdcp22wiredInitSelwreMemoryStorage(void)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredInitSelwreMemoryStorage");

#if UPROC_RISCV
FLCN_STATUS libIntfcHdcp22SendResponse(RM_FLCN_QUEUE_HDR hdr, RM_FLCN_HDCP22_MSG msg)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "libIntfcHdcp22SendResponse");
#endif
#endif // LIB_INTFCHDCP22WIRED_H
