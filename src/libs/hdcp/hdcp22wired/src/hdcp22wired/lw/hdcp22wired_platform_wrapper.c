/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    hdcp22wired_platform_wrapper.c
 * @brief   Collect platform related wrapper functions.
 *
 */

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "lib_hdcp22wired.h"
#include "hdcp22wired_aes.h"
#include "hdcp22wired_srm.h"
#ifdef UPROC_RISCV
#include "partswitch.h"
#else
#include "lwoscmdmgmt.h"
#endif // UPROC_RISCV
#ifdef GSPLITE_RTOS
#include "secbus_hs.h"
#include "seapi.h"
#endif // GSPLITE_RTOS
#ifdef LIB_CCC_PRESENT
#include "crypto_system_config.h"
#include "tegra_se.h" // This is generic header across CHEETAH and DGPU.
#include "tegra_lwpka_rsa.h" //Todo: Remove cheetah from naming here.
#include "tegra_lwpka_gen.h"
#include "tegra_lwrng_init.h"
#include "tegra_lwrng.h"
#endif

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
extern LwBool   gBType1LockState;
extern LwBool   gBBackGrndReAuthIsInProgress;
extern LwU8     gBgLwrrentReAuthSessionIndex;
extern LwBool   gBInitError;

extern HDCP22_SESSION           lwrrentSession;
extern HDCP22_ACTIVE_SESSION    activeSessions[HDCP22_ACTIVE_SESSIONS_MAX];

/* ------------------------- Static Variables ------------------------------- */
#ifdef GSPLITE_RTOS
/*!
 * These variables are used to store CmdQueueId and SeqNumId of flush type
 * command to send the response back after processing the flush type.
 */
static LwU32 gFlushTypeCmdQueueId;
static LwU32 gFlushTypeSeqNumId;
#endif // GSPLITE_RTOS

#ifndef UPROC_RISCV
static LwU8 HdcpDmaReadBuffer[HDCP22_DMA_BUFFER_SIZE]
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT)
    GCC_ATTRIB_SECTION("dmem_hdcp22wired", "HdcpDmaReadBuffer");
#endif

#ifdef HDCP22_DEBUG_MODE
#define LW_HDCP22_TEST_RCVRID   0

static const LwU8 g_testRtx[LW_HDCP22_TEST_RCVR_COUNT][HDCP22_SIZE_R_TX]
    GCC_ATTRIB_SECTION("dmem_libHdcp22wired", "g_testRtx") =
{
    {0x18, 0xfa, 0xe4, 0x20, 0x6a, 0xfb, 0x51, 0x49},
    {0xf9, 0xf1, 0x30, 0xa8, 0x2d, 0x5b, 0xe5, 0xc3},
};

static const LwU8 g_testRn[LW_HDCP22_TEST_RCVR_COUNT][HDCP22_SIZE_R_N]
    GCC_ATTRIB_SECTION("dmem_libHdcp22wired", "g_testRn") =
{
    {0x32, 0x75, 0x3e, 0xa8, 0x78, 0xa6, 0x38, 0x1c},
    {0xa0, 0xfe ,0x9b, 0xb8, 0x20, 0x60, 0x58, 0xca},
};

static const LwU8 g_testKm[LW_HDCP22_TEST_RCVR_COUNT][HDCP22_SIZE_KM]
    GCC_ATTRIB_SECTION("dmem_libHdcp22wired", "g_testKm") =
{
    {0x68, 0xbc, 0xc5, 0x1b, 0xa9, 0xdb, 0x1b, 0xd0, 0xfa, 0xf1, 0x5e, 0x9a, 0xd8, 0xa5, 0xaf, 0xb9},
    {0xca, 0x9f, 0x83, 0x95, 0x70, 0xd0, 0xd0, 0xf9, 0xcf, 0xe4, 0xeb, 0x54, 0x7e, 0x09, 0xfa, 0x3b},
};

static const LwU8 g_testKs[LW_HDCP22_TEST_RCVR_COUNT][HDCP22_SIZE_KS]
    GCC_ATTRIB_SECTION("dmem_libHdcp22wired", "g_testKs") =
{
    {0xf3, 0xdf, 0x1d, 0xd9, 0x57, 0x96, 0x12, 0x3f, 0x98, 0x97, 0x89, 0xb4, 0x21, 0xe1, 0x2d, 0xe1},
    {0xf3, 0xdf, 0x1d, 0xd9, 0x57, 0x96, 0x12, 0x3f, 0x98, 0x97, 0x89, 0xb4, 0x21, 0xe1, 0x2d, 0xe1},
};

static const LwU8 g_testRiv[LW_HDCP22_TEST_RCVR_COUNT][HDCP22_SIZE_R_IV]
    GCC_ATTRIB_SECTION("dmem_libHdcp22wired", "g_testRiv") =
{
    {0x40, 0x2b, 0x6b, 0x43, 0xc5, 0xe8, 0x86, 0xd8},
    {0x9a, 0x6d, 0x11, 0x00, 0xa9, 0xb7, 0x6f, 0x64},
};

static const LwU8 g_testRsession[HDCP22_SIZE_SESSION_CRYPT]
    GCC_ATTRIB_SECTION("dmem_libHdcp22wired", "g_testRsession") =
    {0xf3, 0xdf, 0x1d, 0xd9, 0x57, 0x96, 0x12, 0x3f, 0x98, 0x97, 0x89, 0xb4, 0x21, 0xe1, 0x2d, 0xe1};
#endif // HDCP22_DEBUG_MODE

/* ------------------------- Function Prototypes ---------------------------- */
#ifdef __COVERITY__
extern void SanitizeGlobal(RM_FLCN_HDCP22_CMD *phdcp22wired);
#endif
#ifdef UPROC_RISCV
extern  LwU64 partitionSwitch(LwU64 partition, LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4);
#endif // UPROC_RISCV

/* ------------------------- Static Functions ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
#ifdef DPU_RTOS
/*
 * This function gets called from isr to process SEC2's request and return
 * status to SEC2 in earliest timing. If some time consuming operations are
 * required to process the request, then isr will need to queue event to HDCP22
 * task to continue the remaining time consuming operations. The reason to
 * directly return status to SEC2 in isr instead of in HDCP22 task is because
 * HDCP22 task can sometimes be busy on handling some other ongoing operations
 * thus the SEC2 request will be deferred a lot and unexpectedly causing SEC2
 * timeout'd waiting for DPU's response.
 *
 * (Since this function is called from isr, please don't do any time consuming
 * operation in this function)
 *
 * @returns  FLCN_OK if the early process is done successfully.
 *           FLCN_ERR_MORE_PROCESSING_REQUIRED to ask isr to queue event to
 *           HDCP22 task if we are not able to process all the required
 *           operations at this function.
 *           Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22EarlyProcessSec2ReqInIsr(void)
{
    LwBool                 bSec2ReqType = LW_FALSE;
    HDCP22_ACTIVE_SESSION  *pActiveSession;
    LwU8                   lwrrentIndex = 0;
    LwBool                 bEncActive = LW_FALSE;
    FLCN_STATUS            status = FLCN_OK;

    CHECK_STATUS(hdcp22wiredIsType1LockActive_HAL(&Hdcp22wired, &bSec2ReqType));
    gBType1LockState = bSec2ReqType;

    if (bSec2ReqType)
    {
        while (lwrrentIndex < HDCP22_ACTIVE_SESSIONS_MAX)
        {
            pActiveSession = &activeSessions[lwrrentIndex];

            CHECK_STATUS(hdcp22wiredEncryptionActive_HAL(&Hdcp22wired, pActiveSession->sorNum, &bEncActive));
            //
            // We expect OPM always enables type1 lock prior SEC2's request comes.
            // So it's a failure condition if there is still any active HDCP2.2
            // session still in type0.
            //
            if ((pActiveSession->bActive) &&
                (bEncActive) &&
                (pActiveSession->bIsAux || pActiveSession->bIsRepeater)
                 && (hdcp22IsConnectionTypeZero(pActiveSession->streamIdType, pActiveSession->numStreams))
                )
            {
                hdcp22wiredSendType1LockRspToSec2_HAL(&Hdcp22wired, HDCP22_SEC2LOCK_FAILURE);
                return FLCN_OK;
            }

            lwrrentIndex++;
        }

        //
        // OPM already enabled type1 lock so only need to update the cached SEC2
        // status then respond SEC2 SUCCESS status
        //
        CHECK_STATUS(hdcp22wiredSendType1LockRspToSec2_HAL(&Hdcp22wired, HDCP22_SEC2LOCK_SUCCESS));
        return FLCN_OK;
    }
    else
    {
        //
        // For unlock request, SEC2 doesn't care if it succeeds or not, thus DPU
        // returns SUCCESS_TYPE1_DISABLED directly, but will still queue event
        // to HDCP22 task to process remaining operations.
        //
        CHECK_STATUS(hdcp22wiredSendType1LockRspToSec2_HAL(&Hdcp22wired, HDCP22_SEC2LOCK_SUCCESS_TYPE1_DISABLED));
    }

    return FLCN_ERR_MORE_PROCESSING_REQUIRED;

label_return:
    hdcp22wiredSendType1LockRspToSec2_HAL(&Hdcp22wired, HDCP22_SEC2LOCK_FAILURE);
    return status;
}

/*!
 * @brief This function handles remaining operations of SEC2's type1 request which was
 * not handled in isr (hdcp22EarlyProcessSec2ReqInIsr). The lock request should
 * already be handled in isr so won't reach this function. For the unlock
 * request, the cached status will be updated then background reauthentication
 * will be kicked off to reconfigure the stream type.
 * @returns    FLCN_STATUS     FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredProcessSec2Req(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwBool bSec2ReqType = LW_FALSE;

    CHECK_STATUS(hdcp22wiredIsType1LockActive_HAL(&Hdcp22wired, &bSec2ReqType));

    if (bSec2ReqType)
    {
        // Lock request should already be handled in isr, so this should not happen
        return FLCN_OK;
    }

    gBgLwrrentReAuthSessionIndex = 0;
    gBBackGrndReAuthIsInProgress = LW_TRUE;
    CHECK_STATUS(hdcp22ProcessBackgroundReAuth());

label_return:
    return status;
}
#endif

#ifdef GSPLITE_RTOS

#ifndef UPROC_RISCV
/*!
 * @brief Read an register using the SelwreBus
 *
 * @param[in]  addr  Address
 * @param[out] pData Data out for read request
 *
 * @return FLCN_OK if request is successful
 */
FLCN_STATUS
hdcp22wiredSelwreBusReadHs
(
    LwU32   addr,
    LwU32  *pData
)
{
    SE_STATUS seStatus = SE_OK;

    seStatus = SE_SECBUS_REG_RD32_HS_ERRCHK(addr, pData);
    if (seStatus != SE_OK)
    {
        return FLCN_ERR_SELWREBUS_REGISTER_READ_ERROR;
    }

    return FLCN_OK;
}

/*!
 * @brief Write an register using the SelwreBus
 *
 * @param[in] addr  Address
 * @param[in] val   If its a write request, the value is written
 *
 * @return FLCN_OK if request is successful
 */
FLCN_STATUS
hdcp22wiredSelwreBusWriteHs
(
    LwU32   addr,
    LwU32   val
)
{
    SE_STATUS seStatus = SE_OK;

    seStatus = SE_SECBUS_REG_WR32_HS_ERRCHK(addr, val);
    if (seStatus != SE_OK)
    {
        return FLCN_ERR_SELWREBUS_REGISTER_WRITE_ERROR;
    }

    return FLCN_OK;
}
#endif // !UPROC_RISCV

/*!
 * This function handles flush type request,the cached status will be updated
 * then background reauthentication will be kicked off to reconfigure the stream
 * type.
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @param[in]  seqNumId         seqNum Id, needed to send the response
 * @param[in]  cmdQueueId       Command Queue identifier
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredProcessFlushType
(
    HDCP22_SESSION     *pSession,
    LwU8                seqNumId,
    LwU8                cmdQueueId
)
{
    FLCN_STATUS status = FLCN_OK;

    CHECK_STATUS(hdcp22wiredIsType1LockActive(&gBType1LockState));
    if (gBType1LockState)
    {
        // If Type1 Lock is active, return failure
        status = FLCN_ERR_HDCP22_FLUSH_TYPE_LOCK_ACTIVE;
        goto label_return;
    }
    else if (gBBackGrndReAuthIsInProgress == LW_TRUE)
    {
        status = FLCN_ERR_HDCP22_FLUSH_TYPE_IN_PROGRESS;
        goto label_return;
    }

    gBgLwrrentReAuthSessionIndex = 0;
    gBBackGrndReAuthIsInProgress = LW_TRUE;
    CHECK_STATUS(hdcp22ProcessBackgroundReAuth());

    if (gBBackGrndReAuthIsInProgress == LW_FALSE)
    {
        status = FLCN_OK;
    }
    else
    {
        status = FLCN_ERR_MORE_PROCESSING_REQUIRED;
    }

label_return:
    if (status != FLCN_ERR_MORE_PROCESSING_REQUIRED)
    {
        if (status == FLCN_OK)
        {
            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_FLUSH_TYPE_SUCCESS;
        }
        else if (status == FLCN_ERR_HDCP22_FLUSH_TYPE_LOCK_ACTIVE)
        {
            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_FLUSH_TYPE_LOCK_ACTIVE;
        }
        else if (status == FLCN_ERR_HDCP22_FLUSH_TYPE_IN_PROGRESS)
        {
            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_FLUSH_TYPE_IN_PROGRESS;
        }
        else
        {
            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_FLUSH_TYPE_FAILURE;
        }

        hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, FLUSH_TYPE),
                                cmdQueueId, seqNumId);
    }
    else
    {
        gFlushTypeCmdQueueId = cmdQueueId;
        gFlushTypeSeqNumId = seqNumId;
    }

    return status;
}

#else
/*!
 * This function handles TEST_SE request.
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @param[in]  seqNumId         seqNum Id, needed to send the response
 * @param[in]  cmdQueueId       Command Queue identifier
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredProcessTestSE
(
    HDCP22_SESSION     *pSession,
    LwU8                seqNumId,
    LwU8                cmdQueueId
)
{
    #if SELWRITY_ENGINE_ENABLED
    FLCN_STATUS           status    = FLCN_OK;
    RM_FLCN_HDCP22_STATUS msgStatus = RM_FLCN_HDCP22_STATUS_TEST_SE_SUCCESS;

    hdcp22AttachDetachOverlay(HDCP22WIRED_LIB_SE, LW_TRUE);
    status = hdcp22wiredSelwrityEngineRsaVerif_HAL(&Hdcp22wired);

    hdcp22AttachDetachOverlay(HDCP22WIRED_LIB_SE, LW_FALSE);
    if (status != FLCN_OK)
    {
        msgStatus = RM_FLCN_HDCP22_STATUS_TEST_SE_FAILURE;
    }

    pSession->msgStatus = msgStatus;
    hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, TEST_SE),
                             cmdQueueId, seqNumId);
    return status;
    #else
    return FLCN_ERR_NOT_SUPPORTED;
    #endif // SELWRITY_ENGINE_ENABLED
}
#endif

/*!
 *  @brief Sends HDCP2.2 message to RM.
 *  @param[in]  cmdQueueId       Command Queue identifier
 *  @param[in]  seqNumId         Sequence number identifier
 *  @param[in]  msgType          Type of message
 *  @param[in]  flcnhdcp22Status Flcn hdcp2.2 return status.
 */
void
hdcp22SendResponse
(
    LwU8                    cmdQueueId,
    LwU8                    seqNumId,
    LwU8                    msgType,
    RM_FLCN_HDCP22_STATUS   flcnhdcp22Status
)
{
    RM_FLCN_QUEUE_HDR  hdr;
    RM_FLCN_HDCP22_MSG hdcp22msg;

    hdr.size                        = RM_FLCN_MSG_SIZE(HDCP22, GENERIC);
    hdcp22msg.msgGeneric.streamType = lwrrentSession.sesVariablesRptr.streamIdType[0].streamType;
    hdcp22msg.msgType               = msgType;
    hdcp22msg.msgGeneric.flcnStatus = flcnhdcp22Status;

    hdr.unitId                = RM_FLCN_UNIT_HDCP22WIRED;
    hdr.ctrlFlags             = 0;
    hdr.seqNumId              = seqNumId;

    // TODO: add error handling that try again if in case it is unable to queue
#if UPROC_RISCV
    if (libIntfcHdcp22SendResponse(hdr, hdcp22msg) != FLCN_OK)
    {
        lwuc_halt();
    }
#else
    if (!osCmdmgmtRmQueuePostBlocking(&hdr, &hdcp22msg))
    {
        lwuc_halt();
    }
#endif // UPROC_RISCV
}

#ifdef HDCP22_SUPPORT_MST
/*!
 * This function handles WRITE_DP_ECF request.
 * @param[in]  pSession         Pointer to HDCP22 active session.
 * @param[in]  pHdcp22wired     Pointer to hdcp22wired command.
 * @param[in]  seqNumId         seqNum Id, needed to send the response.
 * @param[in]  cmdQueueId       Command Queue identifier.
 *
 * @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredProcessWriteDpECF
(
    HDCP22_SESSION     *pSession,
    RM_FLCN_HDCP22_CMD *pHdcp22wired,
    LwU8                seqNumId,
    LwU8                cmdQueueId
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pHdcp22wired->cmdWriteDpEcf.sorNum >= pSession->maxNoOfSors)
    {
        pSession->msgStatus = RM_FLCN_HDCP22_STATUS_WRITE_DP_ECF_FAILURE;
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto label_return;
    }

#ifdef __COVERITY__
    SanitizeGlobal (pHdcp22wired);
#endif

    status = hdcp22wiredWriteDpEcf(pHdcp22wired->cmdWriteDpEcf.sorNum,
                                   pHdcp22wired->cmdWriteDpEcf.ecfTimeslot,
                                   pHdcp22wired->cmdWriteDpEcf.bForceClearEcf,
                                   pHdcp22wired->cmdWriteDpEcf.bAddStreamBack);

    pSession->msgStatus = (status == FLCN_OK) ?
                            RM_FLCN_HDCP22_STATUS_WRITE_DP_ECF_SUCCESS :
                            RM_FLCN_HDCP22_STATUS_WRITE_DP_ECF_FAILURE;

label_return:

    hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, WRITE_DP_ECF),
                            cmdQueueId, seqNumId);
    return status;
}
#endif

#ifdef HDCP22_FORCE_TYPE1_ONLY
/*!
 *  @brief This function force to support type1 only.
 *
 *  @param[in]  pEnableCmd      Pointer to hdcp enable cmd.
 *
 *  @returns    LwBool          returns LW_TRUE if typezero is enabled
 */
void
hdcp22wiredForceTypeOne
(
    RM_FLCN_HDCP22_CMD_ENABLE_HDCP22 *pEnableCmd
)
{
    LwU16  index;

    for (index = 0; index < HDCP22_NUM_STREAMS_MAX; index++)
    {
        pEnableCmd->streamIdType[index].streamType = 1;
    }

    for (index=0; index<HDCP22_NUM_DP_TYPE_MASK; index++)
    {
        pEnableCmd->dpTypeMask[index] = 0xFFFFFFFF;
    }
}
#endif // HDCP22_FORCE_TYPE1_ONLY

/*!
 *  @brief This function checks whether HDCP Type zero is enabled
 *
 *  @param[in]  pStreamIdType   Pointer to streamIdType array
 *  @param[in]  numStreams      Number of streams
 *
 *  @returns    LwBool             returns LW_TRUE if typezero is enabled
 */
LwBool
hdcp22IsConnectionTypeZero
(
    HDCP22_STREAM *pStreamIdType,
    LwU8           numStreams
)
{
    LwU16  index;
    LwBool bIsStreamTypeZero = LW_FALSE;

    for (index = 0; index < numStreams; index++)
    {
        //
        // If original type is zero for any of the stream in the sor, then for both
        // lock and unlock requests we need to change the type
        //
        if (pStreamIdType[index].streamType == 0)
        {
            bIsStreamTypeZero = LW_TRUE;
            break;
        }
    }

    return bIsStreamTypeZero;
}

/*!
 * This function handles response flush type status for background
 * RaAuthentication.
 *
 * @param[in]  pSession                     Pointer to HDCP22 active session.
 * @param[in]  bBackgroundReAuthInProgress  Flag to tell if background in
 *                                          progress.
 * @param[in]  bRegisterError               Flag to tell if reAuth hit error.
 */
void
hdcp22wiredBackgroundReAuthResponse
(
    HDCP22_SESSION *pSession,
    LwBool          bBackgroundReAuthInProgress,
    LwBool          bRegisterError
)
{
#ifdef GSPLITE_RTOS
    //
    // Background authentication is used for changing the type value ehrn tyep1
    // lock is removed.
    //
    if (!bBackgroundReAuthInProgress)
    {
        if (bRegisterError)
        {
            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
        }
        else
        {
            pSession->msgStatus = RM_FLCN_HDCP22_STATUS_FLUSH_TYPE_SUCCESS;
        }
        hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, FLUSH_TYPE),
                                gFlushTypeCmdQueueId, gFlushTypeSeqNumId);
    }
#endif
}

/*!
 * This function handles to update bTypeChangeRequired and corresponding
 * processing.
 *
 * @param[in]  pHdcp22Cmd     HDCP2.2 command
 * @param[in]  pSession       Pointer to HDCP22 active session
 * @param[in]  pActiveSession Pointer to HDCP22 saved session
 * @param[in]  pCmd           Pointer to HDCP22 saved session
 * @param[in]  cmdQueueId     cmdQueue id
 * @param[in]  seqNumId       seqNum id
 * @param[in]  pHashValue     Pointer to session hash value for integrity check
 * @return     FLCN_STATUS    returns FLCN_OK if request is completely handled
 *                            returns FLCN_ERR_MORE_PROCESSING_REQUIRED if request needs to
 *                                    be completed further
 *                            returns ERROR in case of any other error
 */
FLCN_STATUS
hdcp22wiredUpdateTypeChangeRequired
(
    RM_FLCN_HDCP22_CMD     *pHdcp22Cmd,
    HDCP22_SESSION         *pSession,
    HDCP22_ACTIVE_SESSION  *pActiveSession,
    LwU8                    cmdQueueId,
    LwU8                    seqNumId,
    LwU32                  *pHashValue
)
{
    FLCN_STATUS status = FLCN_OK;

    //
    // If type1 lock is enforced, then just save the values and return
    // modified Type1 value will be applied once type1 lock enforcement is disabled.
    //
    if (gBType1LockState)
    {
        status = FLCN_OK;

        // If current type is pure type1 and if existing connection is also type1, then no need to flush it later
        if (!(hdcp22IsConnectionTypeZero(pActiveSession->streamIdType, pActiveSession->numStreams)) &&
            (pActiveSession->bTypeChangeRequired))
        {
            pActiveSession->bTypeChangeRequired = LW_FALSE;
        }
        else
        {
            pActiveSession->bTypeChangeRequired = LW_TRUE;
        }

        // Update actionSessions storage's integrity hash for later check.
        CHECK_STATUS(hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                                (LwU32*)pActiveSession,
                                                                pHashValue,
                                                                sizeof(HDCP22_ACTIVE_SESSION),
                                                                LW_TRUE));
    }
    else
    {
        // If we enable HDCP22 with saved type, clearing type change required flag
        pActiveSession->bTypeChangeRequired = LW_FALSE;

        // Update actionSessions storage's integrity hash for later check.
        CHECK_STATUS(hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                                (LwU32*)pActiveSession,
                                                                pHashValue,
                                                                sizeof(HDCP22_ACTIVE_SESSION),
                                                                LW_TRUE));

        //
        // If type1 lock is not enforced then reauthenticate HDCP 2.2 with
        // modified type1 values. Reauthentication not needed for HDMI HDCP2.2
        // non-repeater case with type change. As for encryption on HDMI non-repeater
        // type does not matter.
        //
        if(!pSession->bIsAux && !(pSession->sesVariablesIr.bRepeater))
        {
            pSession->sesVariablesRptr.streamIdType[0].streamType = pHdcp22Cmd->cmdHdcp22Enable.streamIdType[0].streamType;

            // This call only sets Stream type in Display HW register.
            status = hdcp22wiredControlEncryption(HDMI_NONREPEATER_TYPECHANGE,
                                                  pSession->sorNum,
                                                  pSession->linkIndex,
                                                  pSession->bApplyHwDrmType1LockedWar,
                                                  LW_FALSE,
                                                  pSession->sesVariablesRptr.streamIdType[0].streamType,
                                                  pSession->bIsAux);
        }
        else
        {
            status = hdcp22ProcessReAuth(pSession, pActiveSession, LW_FALSE, cmdQueueId, seqNumId);
            if (status == FLCN_OK)
            {
                status = FLCN_ERR_MORE_PROCESSING_REQUIRED;
            }
        }
    }

label_return:
    return status;
}

/*!
 * This function handles to reply enable cmd response when encryption enabled
 * case.
 *
 * @param[in]  pHdcp22Cmd     HDCP2.2 command
 * @param[in]  pSession       Pointer to HDCP22 active session
 * @param[in]  pActiveSession Pointer to HDCP22 saved session
 * @param[in]  pCmd           Pointer to HDCP22 saved session
 * @param[in]  cmdQueueId     cmdQueue id
 * @param[in]  seqNumId       seqNum id
 * @param[in]  pHashValue     Pointer to session hash value for integrity check
 * @return     FLCN_STATUS    returns FLCN_OK if request is completely handled
 *                            returns ERROR in case of any other error
 */
FLCN_STATUS
hdcp22wiredSendEncryptionEnabledResponse
(
    RM_FLCN_HDCP22_CMD     *pHdcp22wired,
    HDCP22_SESSION         *pSession,
    LwU8                    cmdQueueId,
    LwU8                    seqNumId,
    LwBool                  bRxRestartRequest
)
{
    // Default needs to continue processing.
    FLCN_STATUS status = FLCN_ERR_MORE_PROCESSING_REQUIRED;

    pSession->bApplyHwDrmType1LockedWar = LW_FALSE;

    //
    // If needs reauthentication, disable encryption or possible fail at authentication
    // but encryption keeps on.
    //
    if (bRxRestartRequest ||
        ((pHdcp22wired->cmdHdcp22Enable.sorProtocol != RM_FLCN_HDCP22_SOR_HDMI) &&
        (pHdcp22wired->cmdHdcp22Enable.numStreams != pSession->sesVariablesRptr.numStreams)))
    {
        if (gBType1LockState)
        {
            if (bRxRestartRequest)
            {
                //
                // Continue reAuthentication with WAR sequence.
                //
                // TODO: Another potential fix is wait vblank edge then disable
                // encryption before reAuthentication. Need evaluate security
                // risk and see if works also.
                //
                pSession->bApplyHwDrmType1LockedWar = LW_TRUE;
                status = FLCN_ERR_MORE_PROCESSING_REQUIRED;
            }
            else
            {
                // Override return status that make sure not continue to proceed.
                status = FLCN_OK;

                hdcp22AttachDetachOverlay(HDCP22WIRED_TIMER, LW_FALSE);
                hdcp22ConfigAuxI2cOverlays((RM_FLCN_HDCP22_SOR_PROTOCOL)pHdcp22wired->cmdHdcp22Enable.sorProtocol,
                                           LW_FALSE);
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_TYPE1_LOCK_ACTIVE;
                hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, ENABLE_HDCP22),
                                        cmdQueueId, seqNumId);
                goto label_return;
            }
        }
        else
        {
            // Disable encryption.
            status = hdcp22wiredControlEncryption(ENC_DISABLE,
                                                  pSession->sorNum,
                                                  pSession->linkIndex,
                                                  LW_FALSE,
                                                  LW_TRUE,
                                                  0,
                                                  pSession->bIsAux);
            if (status != FLCN_OK)
            {
                // Override return status that make sure not continue to proceed.
                status = FLCN_OK;
                hdcp22AttachDetachOverlay(HDCP22WIRED_TIMER, LW_FALSE);
                hdcp22ConfigAuxI2cOverlays((RM_FLCN_HDCP22_SOR_PROTOCOL)pHdcp22wired->cmdHdcp22Enable.sorProtocol,
                                           LW_FALSE);
                pSession->msgStatus = RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW;
                hdcp22wiredSendResponse(RM_FLCN_MSG_TYPE(HDCP22, ENABLE_HDCP22),
                                        cmdQueueId, seqNumId);
                goto label_return;
            }
            else
            {
                // EndSession to reset internal state and ignore return code not override original status.
                (void)hdcp22wiredEndSession();

                // After disable encryption, restart from AKE_INIT.
                status = FLCN_ERR_MORE_PROCESSING_REQUIRED;
            }
        }
    }

label_return:
    return status;
}

/*!
 * This function handles to get Type1 lock state.
 *
 * @param[in]  pSession       Pointer to HDCP22 active session
 * @return     FLCN_STATUS    returns FLCN_OK if request is completely handled
 *                            returns ERROR in case of any other error
 */
FLCN_STATUS
hdcp22wiredUpdateType1LockState
(
    HDCP22_SESSION *pSession
)
{
    FLCN_STATUS status = FLCN_OK;

    // Get Type1 lock state from HW reg directly in case transition change.
    CHECK_STATUS(hdcp22wiredIsType1LockActive(&gBType1LockState));
    pSession->sesVariablesIr.bIsStreamTypeLocked = gBType1LockState;

#ifndef HDCP22_FORCE_TYPE1_ONLY
    if (pSession->sesVariablesIr.bIsStreamTypeLocked)
#endif // !HDCP22_FORCE_TYPE1_ONLY
    {
        LwU8 index;

        if (hdcp22IsConnectionTypeZero(pSession->sesVariablesRptr.streamIdType,
                                       pSession->sesVariablesRptr.numStreams))
        {
            pSession->sesVariablesIr.bTypeChangeRequired = LW_TRUE;
        }

        //
        // If type1Locked or force type1, HS override streamType at StartSession and here LS do the same thing
        // to keep consistent type sent to sink.
        //
        for (index=0; index<HDCP22_NUM_STREAMS_MAX; index++)
        {
            pSession->sesVariablesRptr.streamIdType[index].streamType = 1;
        }

        for (index=0; index<HDCP22_NUM_DP_TYPE_MASK; index++)
        {
            pSession->sesVariablesRptr.dpTypeMask[index] = 0xFFFFFFFF;
        }
    }

    CHECK_STATUS(hdcp22wiredCheckOrUpdateStateIntegrity_HAL(&Hdcp22wired,
                                                            (LwU32*)&pSession->sesVariablesIr,
                                                            pSession->integrityHashVal,
                                                            sizeof(HDCP22_SESSION_VARS_IR),
                                                            LW_TRUE));

label_return:

    return status;
}

/*!
 * @brief      This function generates random number of given type Hs version.
 * @param[in]  pRn              Pointer to random number.
 * @param[in]  type             Random number type.
 * @param[in]  inputSize        Random number buffer size.
 *
 * TODO: move the func to common or appropriate file.
 */
FLCN_STATUS
hdcp22wiredChooseRandNum
(
    LwU32              *pRn,
    HDCP22_RAND_TYPE    type,
    LwU32               inputSize
)
{
    FLCN_STATUS status = FLCN_OK;

    if (!pRn)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

#ifdef HDCP22_DEBUG_MODE
    switch (type)
    {
        case HDCP22_RAND_TYPE_RTX:
            {
                if (inputSize != HDCP22_SIZE_R_TX)
                {
                    return FLCN_ERR_ILWALID_ARGUMENT;
                }
                HDCP22WIRED_SEC_ACTION_MEMCPY(pRn, &g_testRtx[LW_HDCP22_TEST_RCVRID][0], HDCP22_SIZE_R_TX);
                break;
            }
        case HDCP22_RAND_TYPE_KM:
            {
                if (inputSize != HDCP22_SIZE_KM)
                {
                    return FLCN_ERR_ILWALID_ARGUMENT;
                }
                HDCP22WIRED_SEC_ACTION_MEMCPY((LwU8 *)pRn, &g_testKm[LW_HDCP22_TEST_RCVRID][0], HDCP22_SIZE_KM);
                break;
            }
        case HDCP22_RAND_TYPE_RN:
            {
                if (inputSize != HDCP22_SIZE_R_N)
                {
                    return FLCN_ERR_ILWALID_ARGUMENT;
                }
                HDCP22WIRED_SEC_ACTION_MEMCPY(pRn, &g_testRn[LW_HDCP22_TEST_RCVRID][0], HDCP22_SIZE_R_N);
                break;
            }
        case HDCP22_RAND_TYPE_KS:
            {
                if (inputSize != HDCP22_SIZE_KS)
                {
                    return FLCN_ERR_ILWALID_ARGUMENT;
                }
                HDCP22WIRED_SEC_ACTION_MEMCPY((LwU8 *)pRn, &g_testKs[LW_HDCP22_TEST_RCVRID][0], HDCP22_SIZE_KS);
                break;
            }
        case HDCP22_RAND_TYPE_RIV:
            {
                if (inputSize != HDCP22_SIZE_R_IV)
                {
                    return FLCN_ERR_ILWALID_ARGUMENT;
                }
                HDCP22WIRED_SEC_ACTION_MEMCPY((LwU8*)pRn, &g_testRiv[LW_HDCP22_TEST_RCVRID][0], HDCP22_SIZE_R_IV);
                break;
            }
        case HDCP22_RAND_TYPE_SESSION:
            {
                if (inputSize != HDCP22_SIZE_SESSION_CRYPT)
                {
                    return FLCN_ERR_ILWALID_ARGUMENT;
                }
                HDCP22WIRED_SEC_ACTION_MEMCPY((LwU8*)pRn, &g_testRsession[0], HDCP22_SIZE_SESSION_CRYPT);
                break;
            }
        default:
            return FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }
#else
    LwU32    size = 0;

    switch (type)
    {
        case HDCP22_RAND_TYPE_RTX:
            size  = HDCP22_SIZE_R_TX/sizeof(LwU32);
            break;
        case HDCP22_RAND_TYPE_KM:
            size = HDCP22_SIZE_KM/sizeof(LwU32);
            break;
        case HDCP22_RAND_TYPE_RN:
            size = HDCP22_SIZE_R_N/sizeof(LwU32);
            break;
        case HDCP22_RAND_TYPE_KS:
            size = HDCP22_SIZE_KS/sizeof(LwU32);
            break;
        case HDCP22_RAND_TYPE_RIV:
            size = HDCP22_SIZE_R_IV/sizeof(LwU32);
            break;
        case HDCP22_RAND_TYPE_SESSION:
            size = HDCP22_SIZE_SESSION_CRYPT_U32;
            break;
        default:
            return FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }

    if (size > inputSize)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

#if defined(GSPLITE_RTOS) && !defined(UPROC_RISCV)
#ifdef LIB_CCC_PRESENT
    status = (hdcp22LibCccGetRandomNumberHs(pRn, size) == FLCN_OK) ?
                FLCN_OK : FLCN_ERR_HS_GEN_RANDOM;
#else
    status = (seTrueRandomGetNumberHs(pRn, size) == SE_OK) ?
                FLCN_OK : FLCN_ERR_HS_GEN_RANDOM;
#endif
#else
    status = hdcp22wiredGetRandNumber_HAL(&Hdcp22wired, pRn, size);
#endif // GSPLITE_RTOS
#endif // HDCP22_DEBUG_MODE

    return status;
}

/*!
 * @brief      This function override dma idx with RM.
 * @param[in]  pHdcp22Cmd   Pointer to hdcp22 Cmd.
 *
 * @return FLCN_OK if request is successful
 */
void
hdcp22wiredSetDmaIndex
(
    RM_FLCN_HDCP22_CMD *pHdcp22Cmd
)
{
    LwU32 *pParams = NULL;

    if (pHdcp22Cmd->cmdType == RM_FLCN_HDCP22_CMD_ID_VALIDATE_SRM2)
    {
        pParams = &(pHdcp22Cmd->cmdValidateSrm2.srm.params);
        *pParams = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                   RM_DPU_DMAIDX_PHYS_SYS_COH, *pParams);

    }
    else if (pHdcp22Cmd->cmdType == RM_FLCN_HDCP22_MSG_ID_ENABLE_HDCP22)
    {
        pParams = &(pHdcp22Cmd->cmdHdcp22Enable.srm.params);

#ifndef UPROC_RISCV
        //
        // RISCV only supports FB DMA transfer now and cannot override to use
        // SYSMEM DMA transfer.
        // TODO (Bug 200417270) - Revisit later to see if still need to use
        // SYSMEM for SRM data.
        //
        *pParams = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                   RM_DPU_DMAIDX_PHYS_SYS_COH, *pParams);
#endif
    }
}

/*!
 * hdcp22wiredGenerateDkeySw
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
hdcp22wiredGenerateDkeySw
(
    LwU8 *pMessage,
    LwU8 *pKey,
    LwU8 *pEncryptMessage
)
{
    FLCN_STATUS status = FLCN_OK;

#if !defined(HDCP22_USE_SCP_GEN_DKEY)
#if !defined(GSPLITE_RTOS) || defined(UPROC_RISCV)
    // preVolta or RISC-V use SW to generate derived key.
    status = hdcp22AesGenerateDkey128(pMessage, pKey, pEncryptMessage, HDCP22_SIZE_DKEY);
#else
    #error "dkey generation with HW SCP must be supported for Volta+."
    status = FLCN_ERR_NOT_SUPPORTED;
#endif
#endif

    return status;
}

/*
 * @brief Handles the Data transfer between FB and DMEM.
 * @param[in]     pDst       Destination in DMEM.
 * @param[in]     pMemDesc   Pointer to transfer memory descriptor
 * @param[in/out] srcOffset  Current fb offset to be used for accessing data in
 *                           FB
 * @param[in]     size       Number of required LwU8s.
 */
FLCN_STATUS
hdcp22wiredFbDmaRead
(
    LwU8               *pDst,
    PRM_FLCN_MEM_DESC   pMemDesc,
    LwU32               srcOffset,
    LwU32               size
)
{
#ifdef UPROC_RISCV
    //
    // GSP all global datas located at FB, and no need to use DMA transfer.
    // TODO (Bug 200417270)
    // - syslib needs to provide API that can get VA of data at FB.
    // - Revisit that put all hdcp22 global datas on DMEM instead FB considering security.
    //
    LwU64 srcVAddr = engineFBGPA_FULL_VA_BASE +
        (((LwU64)pMemDesc->address.hi << 32) | pMemDesc->address.lo) +
        srcOffset;

    hdcpmemcpy((void *)pDst, (void *)srcVAddr, size);

    return FLCN_OK;
#else
    LwU32 offset;
    LwU32 bytesRead;
    LwU32 copySize;
    LwU32 dmaSize;
    LwU16 dmaOffset;
    LwU8  unalignedBytes;

    offset = 0;
    bytesRead = 0;
    unalignedBytes = srcOffset & (DMA_MIN_READ_ALIGNMENT - 1);

    while (size > 0)
    {
        dmaOffset = ALIGN_DOWN(srcOffset + offset, DMA_MIN_READ_ALIGNMENT);
        dmaSize   = LW_MIN(ALIGN_UP(size + unalignedBytes,
                                    DMA_MIN_READ_ALIGNMENT),
                           HDCP22_DMA_BUFFER_SIZE);
        copySize  = LW_MIN(size, HDCP22_DMA_BUFFER_SIZE - unalignedBytes);

        if (FLCN_OK != hdcpDmaRead(HdcpDmaReadBuffer, pMemDesc, dmaOffset, dmaSize))
        {
            break;
        }
        hdcpmemcpy((void *) (pDst + offset),
                   (void *) (HdcpDmaReadBuffer + unalignedBytes), copySize);

        size -= copySize;
        offset += copySize;
        bytesRead += copySize;
        unalignedBytes = 0;
    }

    return (size != 0) ? FLCN_ERROR : FLCN_OK;
#endif
}

#ifdef UPROC_RISCV
/*
 * hdcp22wiredMpSwitchSelwreAction
 *
 * @brief RISC-V PartitionSwitch for selwreAction.
 *
 * @return FLCN_OK if request is successful
 */
FLCN_STATUS
hdcp22wiredMpSwitchSelwreAction(void)
{
#if LWRISCV_PARTITION_SWITCH
    LwU64                   hdcpArgOffset;
    SELWRE_ACTION_STATUS   *pStatus = (SELWRE_ACTION_STATUS *)libIntfcHdcp22wiredGetSelwreActionArgument();

    if(pStatus == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Initialize as processing before switching to HS partition.
    pStatus->state.actionState = SELWRE_ACTION_STATE_PROCESSING;

    hdcpArgOffset = partitionSwitch(PARTITION_HDCP_SELWREACTION_ID,
                                    PARTSWITCH_FUNC_HDCP_SELWREACTION,
                                    0,
                                    0,
                                    0,
                                    0);
    if ((hdcpArgOffset != 0) ||
        (pStatus->state.actionState != SELWRE_ACTION_STATE_COMPLETED))
    {
        //
        // Error due to argument is not in the beginning of shared space or
        // not expecting completed state.
        //
        return FLCN_ERR_HS_SELWRE_ACTION_ARG_CHECK_FAILED;
    }
    return pStatus->state.actionResult;
#else
    return FLCN_ERR_NOT_SUPPORTED;
#endif // LWRISCV_PARTITION_SWITCH
}
#endif

/*
 * hdcp22wiredInitialize
 *
 * @brief Hdcp2X initialization handler.
 *
 */
void
hdcp22wiredInitialize(void)
{
    FLCN_STATUS status = FLCN_OK;

    //
    // gBInitError variable is used to catch register error
    // at this task initialization
    // when a command is received from RM in hdcp22wiredProcessCmd
    // then this variable will be checked if any error oclwred.
    // TODO: Try for more times before failing.
    //
    gBInitError = LW_TRUE;
    gBBackGrndReAuthIsInProgress = LW_FALSE;
    gBgLwrrentReAuthSessionIndex = 0;

    CHECK_STATUS(hdcp22wiredInitSelwreMemoryStorage());

    // Reset hdcp22 internal secure memory state.
    CHECK_STATUS(hdcp22wiredEndSession());

    CHECK_STATUS(hdcp22wiredInitSorHdcp22CtrlReg());

    //
    // Get Type1 Lock State from disp scratch register and initialize
    // global Lock State
    //
    CHECK_STATUS(hdcp22wiredIsType1LockActive(&gBType1LockState));

    // Update succeed to initialize.
    gBInitError = LW_FALSE;

label_return:
    return;
}

#ifdef LIB_CCC_PRESENT
/*!
 * @brief This is a combined init of LWPKA and LWRNG.
 *        This should be done one time during the entire operation
 *
 * @return FLCN_OK if successful. Error code if unsuccessful.
 */
FLCN_STATUS
hdcp22LibCccCryptoDeviceInitHs(void)
{
    static LwBool gLibCccInit = LW_FALSE;
    FLCN_STATUS flcnStatus = FLCN_OK;
    
    if(gLibCccInit == LW_FALSE)
    {
       status_t statusCcc   = ERR_GENERIC;
       CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(init_crypto_devices(), FLCN_ERR_INIT_CRYPTO_DEVICE_FAILED);
       gLibCccInit = LW_TRUE;
    }
    return flcnStatus;
}

/*!
 * @brief Wrapper to implement "Modular Exponentiation" operation for RSA decrypt or encrypt request in HS Mode.
 *
 * Todo: This wrapper has to be moved to LibCCC and once implemented in LibCCC. remove this wrapper. 
 * Tracking Bug:200753570
 *
 * @param[in]   keySize                    Key Size for RSA operation
 * @param[in]   modulus                    The prime modulus for modular exponentiation
 * @param[in]   exponent                   The exponent for modular exponentiation
 * @param[in]   base                       The base for modular exponentiation
 * @param[out]  output                     The ouput buffer to save c
 *
 * @return FLCN_OK if successful. Error code if unsuccessful.
 */
FLCN_STATUS
hdcp22LibCccHwRsaModularExpHs
(
    LwU32 keySize,    
    LwU32 *modulus_arr,
    LwU32 *exponent_arr,
    LwU32 *signature_arr,
    LwU32 *output_arr    
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    status_t statusCcc = ERR_GENERIC;
    engine_t *pEngine = NULL;
    struct se_data_params input;
    struct se_engine_rsa_context rsa_econtext;
    unsigned char result[keySize];
    LwU8  modulus_temp_arr[keySize]   GCC_ATTRIB_ALIGN(0x4);
    LwU8  signature_temp_arr[keySize]   GCC_ATTRIB_ALIGN(0x4);
    LwU8  exponent_temp_arr[sizeof(LwU32)]   GCC_ATTRIB_ALIGN(0x4);

    // Step1: Init Crypto Device
    flcnStatus = hdcp22LibCccCryptoDeviceInitHs();
    if(flcnStatus != FLCN_OK)
        return flcnStatus;

    // Step2: Select the engine you want to use
    CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(ccc_select_engine((const engine_t **) &pEngine, ENGINE_CLASS_RSA, ENGINE_PKA), FLCN_ERR_LWPKA_SELECT_ENGINE_FAILED);

    // Step3: Acquire the mutex operation.
    CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(lwpka_acquire_mutex(pEngine), FLCN_ERR_LWPKA_ACQUIRE_MUTEX_FAILED);    

    // Step4: Initializing the input buffers with zero and set the input parameters
    // Fill in se_engine_rsa_context required by CCC 
    memset_hs(&rsa_econtext, 0, sizeof(rsa_econtext));
    memset_hs(&result, 0, sizeof(result));
    memset_hs(&input, 0, sizeof(input));  
    memset_hs(modulus_temp_arr, 0, keySize);
    memset_hs(signature_temp_arr, 0, keySize);
    memset_hs(exponent_temp_arr, 0, sizeof(LwU32));

    memcpy_hs(modulus_temp_arr, modulus_arr, keySize);
    swapEndiannessHs(modulus_temp_arr, modulus_temp_arr, keySize);

    memcpy_hs(exponent_temp_arr, exponent_arr, sizeof(LwU32));

    memcpy_hs(signature_temp_arr, signature_arr, keySize);
    swapEndiannessHs(signature_temp_arr, signature_temp_arr, keySize);

    uint8_t  *signature   = (uint8_t*)signature_temp_arr;
    uint8_t  *modulus     = (uint8_t*)modulus_temp_arr;
    uint8_t  *pub_exp     = (uint8_t*)exponent_temp_arr;
    uint8_t  pub_exp_size = sizeof(exponent_temp_arr);

    input.input_size  = keySize;
    input.output_size = keySize;
    input.src = signature;
    input.dst = (uint8_t *)result;

    rsa_econtext.engine = pEngine;
    rsa_econtext.rsa_size = keySize;
    rsa_econtext.rsa_flags = RSA_FLAGS_DYNAMIC_KEYSLOT;
    rsa_econtext.rsa_keytype = KEY_TYPE_RSA_PUBLIC;
    rsa_econtext.rsa_pub_exponent = pub_exp;
    rsa_econtext.rsa_pub_expsize = pub_exp_size;
    rsa_econtext.rsa_modulus = modulus;

    // Step5: LWPKA operation.
    CHECK_STATUS_CCC_OK_OR_GOTO_CLEANUP(engine_lwpka_rsa_modular_exp_locked(&input, &rsa_econtext), FLCN_ERR_LWPKA_MODULAR_EXP_LOCK_FAILED);

    memcpy_hs(output_arr, result, keySize);

ErrorExit: 
    // Step6: Mutex Release
    lwpka_release_mutex(pEngine);

    return flcnStatus;
}

/*!
 * @brief Wrapper to get true random number in HS via LWRNG implemented in LibCCC.
 *
 * Todo: This wrapper has to be moved to LibCCC and once implemented in LibCCC. remove this wrapper. 
 * Tracking Bug:200762634
 *
 * @param[out] pRandNum      the random number generated is returned in this
 * @param[in]  sizeInDWords  the size of the random number requested in dwords
 *
 * @return FLCN_OK if successful. Error code if unsuccessful.
 */
FLCN_STATUS
hdcp22LibCccGetRandomNumberHs
(
   LwU32 *pRandNum, 
   LwU32 sizeInDWords
)
{
    FLCN_STATUS flcnStatus       = FLCN_OK;
    status_t statusCcc           = ERR_GENERIC;
    engine_t *engine             = NULL; 
    struct se_engine_rng_context lwrng_econtext;
    struct se_data_params input_params;
    LwU8 dst[sizeInDWords*sizeof(LwU32)];

    // Step1: Init Crypto Device
    CHECK_FLCN_STATUS(hdcp22LibCccCryptoDeviceInitHs());    

    // Step2: Init LWRNG Engine
    CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(lwrng_init((uint8_t*)LW_ADDRESS_MAP_SE_LWRNG_BASE), FLCN_ERR_LWRNG_INIT_CRYPTO_DEVICE_FAILED);

    // Step3: Select LWRNG Engine
    CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_DRNG, ENGINE_RNG1), FLCN_ERR_LWRNG_SELECT_ENGINE_FAILED);
   
    // Step4: Initializing the input buffers with zero and set the input parameters
    memset_hs(&lwrng_econtext, 0, sizeof(lwrng_econtext));
    memset_hs(&input_params, 0, sizeof(input_params));
    memset_hs(&dst, 0, sizeof(dst));
    lwrng_econtext.engine       = engine;    
    input_params.dst            = (uint8_t *)dst;
    input_params.output_size    = sizeInDWords*8;

    // Step5: Generate Random Number
    CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(engine_lwrng_drng_generate_locked(&input_params, &lwrng_econtext), FLCN_ERR_LWRNG_GENERATE_FAILED);

    if (FLCN_OK == flcnStatus)
    {
        LwU32 i; 
        for(i=0; i < sizeInDWords; i++)
        {
            LwU32 rand = ((LwU32)(dst[(i*4)+0]) | (LwU32)((dst[(i*4)+1]) << 8) | (LwU32)((dst[(i*4)+2]) << 16) | (LwU32)((dst[(i*4)+3]) << 24));
            *pRandNum = rand;
            pRandNum++;
        }
    }

ErrorExit:
    return flcnStatus;
}
#endif

