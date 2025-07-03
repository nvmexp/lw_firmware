/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lib_hdcp22wired.h
 */

#ifndef LIB_HDCP22WIRED_H
#define LIB_HDCP22WIRED_H

/* ------------------------ System includes --------------------------------- */
#include "flcntypes.h"

/* ------------------------ Application includes ---------------------------- */
#ifdef UPROC_RISCV
#include <lwriscv/print.h>
#endif

#ifndef DEBUG_CPU_HDCP22
#include "flcnifhdcp22wired.h"
#include "lib_i2c.h"
#include "lib_dpaux.h"
#include "lwos_dma.h"
#else
#include "lwrm.h"
#endif
#include "lib_intfchdcp_cmn.h"
#include "bigint.h"
#include "osptimer.h"
#ifdef GSPLITE_RTOS
#include "mem_hs.h"
#include "bigint_hs.h"
#endif // GSPLITE_RTOS

// TODO: HDCP22_STREAM needs to be duplicated/moved to library header file instead flcnifhdcp22wired.h.
#include "hdcp22wired_protocol.h"

/* ------------------------- External Definitions --------------------------- */
void hdcp22GetScratchBuf(LwU8 **ptmpbuf)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22GetScratchBuf");
void swapEndianness(void* pOutData, const void* pInData, const LwU32 size)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "swapEndianness");
#if defined(GSPLITE_RTOS)
void swapEndiannessHs(void* pOutData, const void* pInData, const LwU32 size)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "swapEndiannessHs");
#endif
FLCN_STATUS PollOnRxStatusMsgSize(HDCP22_SESSION *pSession, LwU16 *pMsgSize, LwU16 *pRxStatus)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "PollOnRxStatusMsgSize");
void hdcp22ConfigAuxI2cOverlays(RM_FLCN_HDCP22_SOR_PROTOCOL sorProtocol, LwBool bAttach)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22ConfigAuxI2cOverlays");
FLCN_STATUS hdcp22wiredStartTimer(LwU32 intervalUs)
    GCC_ATTRIB_SECTION("imem_hdcp22wired", "hdcp22wiredStartTimer");
FLCN_STATUS hdcp22wiredStopTimer(void)
    GCC_ATTRIB_SECTION("imem_hdcp22wired", "hdcp22wiredStopTimer");
void hdcp22wiredMeasureLwrrentTime(FLCN_TIMESTAMP *time)
    GCC_ATTRIB_SECTION("imem_hdcp22wired", "hdcp22wiredMeasureLwrrentTime");
LwBool hdcp22wiredCheckElapsedTimeWithMaxLimit(FLCN_TIMESTAMP *startTime, LwU32 maxTimeLimit)
    GCC_ATTRIB_SECTION("imem_hdcp22wired", "hdcp22wiredCheckElapsedTimeWithMaxLimit");
FLCN_STATUS hdcp22EarlyProcessSec2ReqInIsr(void)
    GCC_ATTRIB_SECTION("imem_resident", "hdcp22EarlyProcessSec2ReqInIsr");
LwBool hdcp22wiredIsType1LockReqFromSec2Active(void)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredIsType1LockReqFromSec2Active");
FLCN_STATUS hdcp22wiredSelwreRegAccessFromLs(LwBool bIsReadReq, LwU32 addr, LwU32 dataIn, LwU32 *pDataOut)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredSelwreRegAccessFromLs");
FLCN_STATUS hdcp22wiredDoCallwlateHash(LwU32 *pHash, LwU32 *pData, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredDoCallwlateHash");
FLCN_STATUS hdcp22wiredGenerateDkey(LwU8 *pMessage, LwU8 *pSkey, LwU8 *pEncryptMessage)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredGenerateDkey");
void  hdcp22AttachDetachOverlay(HDCP_OVERLAY_ID ovlIdx, LwBool bAttach)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22AttachDetachOverlay");
#ifdef GSPLITE_RTOS
FLCN_STATUS hdcp22wiredProcessFlushType(HDCP22_SESSION *pSession, LwU8 seqNumId, LwU8 cmdQueueId)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredProcessFlushType");
#else
FLCN_STATUS hdcp22wiredProcessTestSE(HDCP22_SESSION *pSession, LwU8 seqNumId, LwU8 cmdQueueId)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredProcessTestSE");
#endif
void hdcp22SendResponse(LwU8 cmdQueueId, LwU8 seqNumId, LwU8 msgType, RM_FLCN_HDCP22_STATUS flcnhdcp22Status)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22SendResponse");
#ifdef HDCP22_SUPPORT_MST
FLCN_STATUS hdcp22wiredProcessWriteDpECF(HDCP22_SESSION *pSession, RM_FLCN_HDCP22_CMD *pHdcp22wired, LwU8 seqNumId, LwU8 cmdQueueId)
    GCC_ATTRIB_SECTION("imem_libDpaux", "hdcp22wiredProcessWriteDpECF");
#endif
#ifdef HDCP22_FORCE_TYPE1_ONLY
void hdcp22wiredForceTypeOne(RM_FLCN_HDCP22_CMD_ENABLE_HDCP22 *pEnableCmd)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredForceTypeOne");
#else
LwBool hdcp22IsConnectionTypeZero(HDCP22_STREAM *pStreamIdType, LwU8 numStreams)
    GCC_ATTRIB_SECTION("imem_resident", "hdcp22IsHdcp22TypeZero");
#endif
FLCN_STATUS  hdcp22ProcessBackgroundReAuth(void)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22ProcessBackgroundReAuth");
void hdcp22wiredBackgroundReAuthResponse(HDCP22_SESSION *pSession, LwBool bBackgroundReAuthInProgress, LwBool bRegisterError)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredBackgroundReAuthResponse");
FLCN_STATUS  hdcp22ProcessReAuth(HDCP22_SESSION *pSession, HDCP22_ACTIVE_SESSION *pActiveSession, LwBool bIsSec2ReAuthTypeEnForceReq, LwU8 cmdQueueId, LwU8 seqNumId)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22ProcessReAuth");
FLCN_STATUS hdcp22wiredUpdateTypeChangeRequired(RM_FLCN_HDCP22_CMD *pHdcp22Cmd, HDCP22_SESSION *pSession, HDCP22_ACTIVE_SESSION *pActiveSession, LwU8 cmdQueueId, LwU8 seqNumId, LwU32 *pHashValue)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredUpdateTypeChangeRequired");
FLCN_STATUS hdcp22wiredSendEncryptionEnabledResponse(RM_FLCN_HDCP22_CMD *pHdcp22wired, HDCP22_SESSION *pSession, LwU8 cmdQueueId, LwU8 seqNumId, LwBool bRxRestartRequest)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredSendEncryptionEnabledResponse");
FLCN_STATUS hdcp22wiredUpdateType1LockState(HDCP22_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredUpdateType1LockState");
FLCN_STATUS hdcp22wiredChooseRandNum(LwU32 *pRn, HDCP22_RAND_TYPE type, LwU32 inputSize)
#ifdef GSPLITE_RTOS
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredChooseRandNum");
#else
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredChooseRandNum");
#endif
void hdcp22wiredSetDmaIndex(RM_FLCN_HDCP22_CMD *pHdcp22Cmd)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22wiredSetDmaIndex");
FLCN_STATUS hdcp22wiredGenerateDkeySw(LwU8 *pMessage, LwU8 *pSkey, LwU8 *pEncMessage)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAes", "hdcp22AesGenerateDkeySw");
FLCN_STATUS hdcp22wiredFbDmaRead(LwU8 *pDst, PRM_FLCN_MEM_DESC pMemDesc, LwU32 srcOffset, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredCertrxSrm", "hdcp22wiredFbDmaRead");
#ifdef LIB_CCC_PRESENT
FLCN_STATUS hdcp22LibCccHwRsaModularExpHs(LwU32 keysize, LwU32 *pModulus, LwU32 *pExponent, LwU32 *pBase, LwU32 *pOutput)
    GCC_ATTRIB_SECTION("imem_libCccHs", "hdcp22LibCccHwRsaModularExpHs");
FLCN_STATUS hdcp22LibCccGetRandomNumberHs(LwU32 *pRandNum, LwU32 sizeInDWords)
    GCC_ATTRIB_SECTION("imem_libCccHs", "hdcp22LibCccGetRandomNumberHs");
FLCN_STATUS hdcp22LibCccCryptoDeviceInitHs(void)
    GCC_ATTRIB_SECTION("imem_libCccHs", "hdcp22LibCccCryptoDeviceInit");

// CCC status checking macros
#define CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(status_ccc, status_gsp) \
    if((statusCcc =(status_ccc)) != NO_ERROR)                         \
    {                                                                 \
        return status_gsp;                                            \
    }

#define CHECK_STATUS_CCC_OK_OR_GOTO_CLEANUP(status_ccc, status_gsp) \
    if((statusCcc =(status_ccc)) != NO_ERROR)                       \
    {                                                               \
        flcnStatus = status_gsp;                                    \
        goto ErrorExit;                                             \
    }
#endif
FLCN_STATUS hdcp22SwRsaModularExp(const LwU32 keySize, const LwU8 *pModulus, const LwU32 *pExponent, LwU32 *pBase, LwU32 *pOutput)
#ifdef GSPLITE_RTOS
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22SwRsaModularExp");
#else
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "hdcp22SwRsaModularExp");
#endif

extern HDCP22_SESSION           lwrrentSession;

#define hdcpmemcpy              memcpy
#define hdcpmemset              memset
#define hdcpmemcmp              memcmp
#define hdcpsha256              sha256
#define hdcpsha256Update        sha256Update
#define hdcphmacSha256          hmacSha256
#define hdcpsha256Final         sha256Final
#define hdcpsha256Transform     sha256Transform
#define hdcpsha256Init          sha256Init
#define hdcpbigIntModulusInit   bigIntModulusInit
#define hdcpbigIntPowerMod      bigIntPowerMod

#ifdef HDCP_TEGRA_BUILD
FLCN_STATUS hdcp22wiredDioSnicReadHs(LwU32 addr, LwU32 *pData)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredDioSnicReadHs");
FLCN_STATUS hdcp22wiredDioSnicWriteHs(LwU32 addr, LwU32 val)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredDioSnicWriteHs");

#define hdcp22wiredReadRegisterHs(addr, pData)          hdcp22wiredDioSnicReadHs(addr, pData)
#define hdcp22wiredWriteRegisterHs(addr, val)           hdcp22wiredDioSnicWriteHs(addr, val)
#define hdcp22wiredReadRegister(addr, pDataOut)         dioReadReg(addr, pDataOut)
#define hdcp22wiredWriteRegister(addr, dataIn)          dioWriteReg(addr, dataIn)
#else
    #ifdef UPROC_RISCV
FLCN_STATUS hdcp22wiredDioSeReadHs(LwU32 addr, LwU32 *pData)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredDioSeReadHs");
FLCN_STATUS hdcp22wiredDioSeWriteHs(LwU32 addr, LwU32 val)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredDioSeWriteHs");

#define hdcp22wiredReadRegisterHs(addr, pData)          hdcp22wiredDioSeReadHs(addr, pData)
#define hdcp22wiredWriteRegisterHs(addr, val)           hdcp22wiredDioSeWriteHs(addr, val)
    // TODO: map to libSE APIs after it supports RISC-V.
#define hdcp22wiredReadRegister(addr, pDataOut)         hdcp22wiredSelwreRegAccessFromLs(LW_TRUE, addr, 0, pDataOut)
#define hdcp22wiredWriteRegister(addr, dataIn)          hdcp22wiredSelwreRegAccessFromLs(LW_FALSE, addr, dataIn, NULL)
    #else
FLCN_STATUS hdcp22wiredSelwreBusWriteHs(LwU32 addr, LwU32 val)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredSelwreBusWriteHs");
FLCN_STATUS hdcp22wiredSelwreBusReadHs(LwU32 addr, LwU32 *pData)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredSelwreBusReadHs");

#define hdcp22wiredReadRegisterHs(addr, pData)          hdcp22wiredSelwreBusReadHs(addr, pData)
#define hdcp22wiredWriteRegisterHs(addr, val)           hdcp22wiredSelwreBusWriteHs(addr, val)
#define hdcp22wiredReadRegister(addr, pDataOut)         seSelwreBusReadRegisterErrChk(addr, pDataOut)
#define hdcp22wiredWriteRegister(addr, dataIn)          seSelwreBusWriteRegisterErrChk(addr, dataIn)
    #endif // UPROC_RISCV
#endif // HDCP_TEGRA_BUILD

#define hdcp22wiredSendResponse(msgType, queueId, seqNumId)     \
        hdcp22SendResponse(queueId,                             \
            seqNumId,                                           \
            (msgType),                                          \
            pSession->msgStatus);

#endif // LIB_HDCP22WIRED_H
