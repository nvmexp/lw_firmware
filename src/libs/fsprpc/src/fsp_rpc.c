/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file  fsp_rpc.c
 * @brief A common library which implements common libraries between FSP and
 *        other clients like PMU, SOE, GSP and CPU.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwtypes.h"
#if PMU_RTOS
    #include "pmu_bar0.h"
#else
    #error !!! Client not supported !!!
#endif
#include "regmacros.h"
#include "lwstatus.h"
#include "lwmisc.h"
#include "dbgprintf.h"
#include "lwoslayer.h"
#include "lwosreg.h"

/* ------------------------- Application Includes --------------------------- */
#include "dev_fsp_addendum.h"
#include "dev_fsp_pri.h"
#include "fsp_rpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define FSP_RPC_DWORDS_PER_EMEM_BLOCK 64
#define FSP_RPC_EMEM_OFFSET_MAX       8192

/* ------------------------ Static variables -------------------------------- */
static FSP_RPC_CLIENT_INIT_DATA fspRpcClientConfig = {
    .channelId = LW_U32_MAX,
    .channelOffsetDwords = LW_U32_MAX,
    .channelSizeDwords = LW_U32_MAX
};

/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static LW_STATUS s_fspRpcConfigEmemc(LwU32 channelId, LwU32 offset);
static void s_fspRpcWriteToEmem(LwU32 channelId, LwU32 *pBuffer, LwU32 size);
static void s_fspRpcReadFromEmem(LwU32 channelId, LwU32 *pBuffer, LwU32 sizeDwords);
static void s_fspRpcQueueHeadTailRequestSet(LwU32 channelId, LwU32 queueHead, LwU32 queueTail);
static void s_fspRpcMsgQueueHeadTailGet(LwU32 channelId, LwU32 *pMsgQueueHead, LwU32 *pMsgQueueTail);
static void s_fspRpcMsgQueueHeadTailSet(LwU32 channelId, LwU32 msgQueueHead, LwU32 msgQueueTail);
static LwU32 s_fspRpcCreateMctpHeader(LwU8 som, LwU8 eom, LwU8 tag, LwU8 seq);
static LwU32 s_fspRpcCreateLwdmHeader(LwU32 lwdmType);
static LW_STATUS s_fspRpcSendPacket(LwU32 *pPacketHeader, LwU32 headerSize, LwU32 *pPacketPayload, LwU32 payloadSize);
static LwBool s_fspRpcIsSinglePacket(LwU32 mctpHeader);
static LwBool s_fspRpcValidateMctpPayloadHeader(LwU32 mctpPayloadHeader, LwU32 expectedLwdmType);

/*
 * @copydoc fspRpcInit
 */
LW_STATUS
fspRpcInit
(
    FSP_RPC_CLIENT_INIT_DATA *pInitData
)
{
    const LwU32 ememOffsetMaxDwords = FSP_RPC_EMEM_OFFSET_MAX / FSP_RPC_BYTES_PER_DWORD;

    if ((pInitData->channelId > FSP_EMEM_CHANNEL_MAX) ||
        ((pInitData->channelOffsetDwords + pInitData->channelSizeDwords) >
         ememOffsetMaxDwords))
    {
        return LW_ERR_ILWALID_ARGUMENT;
    }

    fspRpcClientConfig.channelId     = pInitData->channelId;
    fspRpcClientConfig.channelOffsetDwords = pInitData->channelOffsetDwords;

    if (pInitData->bDuplexChannel)
    {
        fspRpcClientConfig.channelSizeDwords = pInitData->channelSizeDwords / 2;
    }
    else
    {
        fspRpcClientConfig.channelSizeDwords = pInitData->channelSizeDwords;
    }

    return LW_OK;
}

/*
 * @copydoc fspRpcMessageSend
 */
LW_STATUS
fspRpcMessageSend
(
    LwU32                  *pPayload,
    LwU32                  sizeDwords,
    LwU32                  lwdmType,
    LwU32                  timeoutUs,
    LwBool                 bSyncRequest
)
{
    LwU32 packetPayloadCapacity;
    LwU32 headerSizeDwords;
    LwU32 firstPacketHeader[2];
    LwU8  seq = 0;

    if ((fspRpcClientConfig.channelSizeDwords == LW_U32_MAX) ||
        (fspRpcClientConfig.channelId == LW_U32_MAX))
    {
        dbgPrintf(LEVEL_ERROR, "FSP RPC Library not initialized!!\n");
        OS_BREAKPOINT();

        return LW_ERR_NOT_READY;
    }

    if (bSyncRequest)
    {
        dbgPrintf(LEVEL_ERROR, "Synchronous request is lwrrently not supported\n");
        OS_BREAKPOINT();

        return LW_ERR_ILWALID_ARGUMENT;
    }

    if ((pPayload == NULL) || (sizeDwords == 0))
    {
        dbgPrintf(LEVEL_ERROR, "Invalid arguments\n");
        OS_BREAKPOINT();

        return LW_ERR_ILWALID_ARGUMENT;
    }

    //
    // Check if message will fit in single packet
    // We lose 2 DWORDS to MCTP and LWDM headers
    //
    headerSizeDwords = sizeof(firstPacketHeader) / FSP_RPC_BYTES_PER_DWORD;
    packetPayloadCapacity = fspRpcClientConfig.channelSizeDwords - headerSizeDwords;

    if (sizeDwords > packetPayloadCapacity)
    {
        dbgPrintf(LEVEL_ERROR, "Payload size greater than %d not supported\n",
                  packetPayloadCapacity);
        OS_BREAKPOINT();

        return LW_ERR_ILWALID_ARGUMENT;
    }

    //
    // NOTE: Lwrrently this design assumes (and supports) only one task per
    // client sending messages to FSP. If there is a client where multiple tasks
    // will be sending messages, we need to add a lock here to ensure any
    // message spanning multiple packets is fully sent to FSP before a message
    // from another task can be sent.
    //

    // First packet: SOM=1,EOM=?,TAG=0,SEQ=0
    firstPacketHeader[0] = s_fspRpcCreateMctpHeader(1, 1, 0, seq);
    firstPacketHeader[1] = s_fspRpcCreateLwdmHeader(lwdmType);

    return s_fspRpcSendPacket(firstPacketHeader, headerSizeDwords, pPayload, sizeDwords);
}

/*
 * @copydoc fspRpcMessageReceive
 */
LW_STATUS
fspRpcMessageReceive
(
    LwU32 *pBuffer,
    LwU32  bufferSizeDwords,
    LwU32 *pPayloadSizeDwords,
    LwU32  expectedLwdmType,
    LwU32  timeoutUs
)
{
    LW_STATUS status = LW_OK;
    // this only applies to first packet or a single-packet message.
    LwU32 packetHeaderSize = 2 * FSP_RPC_BYTES_PER_DWORD;
    LwU32 packetPayloadSize;
    LwU32 packetPayloadSizeDwords;
    LwU32 msgQueueHead = 0;
    LwU32 msgQueueTail;
    LwU32 mctpHeader;
    LwU32 mctpPayloadHeader;

    if ((pBuffer == NULL) || (bufferSizeDwords == 0) || (pPayloadSizeDwords == NULL))
    {
        dbgPrintf(LEVEL_ERROR, "Invalid arguments\n");
        status = LW_ERR_ILWALID_ARGUMENT;
        OS_BREAKPOINT();

        goto fspRpcMessageReceive_exit;
    }

    s_fspRpcMsgQueueHeadTailGet(fspRpcClientConfig.channelId,
                                &msgQueueHead, &msgQueueTail);
    if (msgQueueHead == msgQueueTail)
    {
        dbgPrintf(LEVEL_DEBUG, "No data available head: %x tail: %x\n", msgQueueHead, msgQueueTail);
        status = LW_ERR_NOT_READY;
        goto fspRpcMessageReceive_exit;
    }

    if ((msgQueueTail - msgQueueHead) < packetHeaderSize)
    {
        dbgPrintf(LEVEL_DEBUG, "Unexpected packet size.\n");
        status = LW_ERR_ILWALID_DATA;
        goto fspRpcMessageReceive_exit;
    }

    packetPayloadSize = (msgQueueTail - msgQueueHead) - packetHeaderSize
                        + FSP_RPC_BYTES_PER_DWORD;
    packetPayloadSizeDwords = packetPayloadSize / FSP_RPC_BYTES_PER_DWORD;
    if (bufferSizeDwords < packetPayloadSizeDwords)
    {
        dbgPrintf(LEVEL_DEBUG, "Provided buffer is too small\n");
        status = LW_ERR_BUFFER_TOO_SMALL;
        goto fspRpcMessageReceive_exit;
    }

    s_fspRpcConfigEmemc(fspRpcClientConfig.channelId,
                        msgQueueHead / FSP_RPC_BYTES_PER_DWORD);

    // First 4 bytes correspond to the MCTP Header
    s_fspRpcReadFromEmem(fspRpcClientConfig.channelId,
                         &mctpHeader,
                         sizeof(mctpHeader) / FSP_RPC_BYTES_PER_DWORD);

    if (!s_fspRpcIsSinglePacket(mctpHeader))
    {
        dbgPrintf(LEVEL_DEBUG, "Multi-packet messages are not supported yet.\n");
        status = LW_ERR_NOT_SUPPORTED;
        goto fspRpcMessageReceive_exit;
    }

    //
    // Next 4 bytes corresponds to MCTP Payload header. This is only present in
    // packets which have som set (i.e. First packet in a multi-packet message
    // or single packet message.)
    //
    s_fspRpcReadFromEmem(fspRpcClientConfig.channelId,
                         &mctpPayloadHeader,
                         sizeof(mctpPayloadHeader) / FSP_RPC_BYTES_PER_DWORD);

    if (!s_fspRpcValidateMctpPayloadHeader(mctpPayloadHeader, expectedLwdmType))
    {
        dbgPrintf(LEVEL_DEBUG, "Packet data is not valid.\n");
        status = LW_ERR_ILWALID_DATA;
        goto fspRpcMessageReceive_exit;
    }

    s_fspRpcReadFromEmem(fspRpcClientConfig.channelId, pBuffer, packetPayloadSizeDwords);
    *pPayloadSizeDwords = packetPayloadSizeDwords;

fspRpcMessageReceive_exit:
    s_fspRpcMsgQueueHeadTailSet(fspRpcClientConfig.channelId, msgQueueHead, msgQueueHead);

    return status;
}

/*!
 * @brief Configure EMEMC for client's queue in FSP EMEM
 *
 * @param[in] offset    Offset to write to EMEMC in DWORDS
 *
 * @return LW_OK or LW_ERR_ILWALID_ARGUMENT
 */
static LW_STATUS
s_fspRpcConfigEmemc
(
    LwU32  channelId,
    LwU32  offset
)
{
    LwU32 offsetBlks;
    LwU32 offsetDwords;
    LwU32 reg32 = 0;

    if (offset < fspRpcClientConfig.channelOffsetDwords ||
        offset > (fspRpcClientConfig.channelOffsetDwords +
                  fspRpcClientConfig.channelSizeDwords))
    {
        return LW_ERR_ILWALID_ARGUMENT;
    }

    //
    // EMEMC offset is encoded in terms of blocks (64 DWORDS) and DWORD offset
    // within a block, so callwlate each.
    //
    offsetBlks = offset / FSP_RPC_DWORDS_PER_EMEM_BLOCK;
    offsetDwords = offset % FSP_RPC_DWORDS_PER_EMEM_BLOCK;

    reg32 = FLD_SET_DRF_NUM(_PFSP, _EMEMC, _OFFS, offsetDwords, reg32);
    reg32 = FLD_SET_DRF_NUM(_PFSP, _EMEMC, _BLK, offsetBlks, reg32);

    reg32 = FLD_SET_DRF(_PFSP, _EMEMC, _AINCW, _TRUE, reg32);
    reg32 = FLD_SET_DRF(_PFSP, _EMEMC, _AINCR, _TRUE, reg32);

    REG_WR32(BAR0, LW_PFSP_EMEMC(channelId), reg32);

    return LW_OK;
}

/*!
 * @brief Write data in buffer to client channel in FSP's EMEM
 *
 * @param[in] channelId EMEM channel ID corresponding to the client
 * @param[in] pBuffer   (DWORD aligned) Buffer with data to write to EMEM
 * @param[in] size      Size of buffer in DWORDs
 */
static void
s_fspRpcWriteToEmem
(
    LwU32    channelId,
    LwU32   *pBuffer,
    LwU32    sizeDwords
)
{
    LwU32 i;

    // Now write to EMEMD
    for (i = 0; i < sizeDwords; i++)
    {
        REG_WR32(BAR0, LW_PFSP_EMEMD(channelId), pBuffer[i]);
    }
}

/*!
 * @brief Read data from client channel in FSP's EMEM to buffer
 *
 * @param[in] channelId EMEM channel ID corresponding to the client
 * @param[in] pBuffer   (DWORD aligned) Buffer to copy data from EMEM
 * @param[in] size      Size of buffer in bytes
 */
static void
s_fspRpcReadFromEmem
(
    LwU32 channelId,
    LwU32 *pBuffer,
    LwU32 sizeDwords
)
{
    LwU32 i;
    LwU32 reg32;
    LwU32 ememOffsetEnd;

    // Now write to EMEMD
    for (i = 0; i < sizeDwords; i++)
    {
        pBuffer[i] = REG_RD32(BAR0, LW_PFSP_EMEMD(channelId));
    }

    // WARNING: Debug code remove before submitting.
    reg32 = REG_RD32(BAR0, LW_PFSP_EMEMC(channelId));
    ememOffsetEnd = DRF_VAL(_PFSP, _EMEMC, _OFFS, reg32);
    ememOffsetEnd += DRF_VAL(_PFSP, _EMEMC, _BLK, reg32) * 64;
    dbgPrintf(LEVEL_DEBUG, "After reading data, ememcOff = 0x%x\n", ememOffsetEnd);
}

/*!
 * @brief Update command queue head and tail pointers
 *
 * @param[in] channelId EMEM channel ID corresponding to the client
 * @param[in] queueHead Offset to write to command queue head
 * @param[in] queueTail Offset to write to command queue tail
 */
static void
s_fspRpcQueueHeadTailRequestSet
(
    LwU32   channelId,
    LwU32   queueHead,
    LwU32   queueTail
)
{

    // The write to HEAD needs to happen after TAIL because it will interrupt FSP
    REG_WR32(BAR0, LW_PFSP_QUEUE_TAIL(channelId), queueTail);
    REG_WR32(BAR0, LW_PFSP_QUEUE_HEAD(channelId), queueHead);
}

/*!
 * @brief Retrieve queue head and tail pointers
 *
 * @param[in] channelId     EMEM channel ID corresponding to the client
 * @param[in] pMsgQueueHead Offset to write to command queue head
 * @param[in] pMsgQueueTail Offset to write to command queue tail
 */
static void
s_fspRpcMsgQueueHeadTailGet
(
    LwU32 channelId,
    LwU32 *pMsgQueueHead,
    LwU32 *pMsgQueueTail
)
{
    *pMsgQueueHead = REG_RD32(BAR0, LW_PFSP_MSGQ_HEAD(channelId));
    *pMsgQueueTail = REG_RD32(BAR0, LW_PFSP_MSGQ_TAIL(channelId));
}

/*!
 * @brief Update message queue head and tail pointers
 *
 * @param[in] channelId    EMEM channel ID corresponding to the client
 * @param[in] msgQueueHead Offset to write to command queue head
 * @param[in] msgQueueTail Offset to write to command queue tail
 */
static void
s_fspRpcMsgQueueHeadTailSet
(
    LwU32 channelId,
    LwU32 msgQueueHead,
    LwU32 msgQueueTail
)
{
    REG_WR32(BAR0, LW_PFSP_MSGQ_HEAD(channelId), msgQueueHead);
    REG_WR32(BAR0, LW_PFSP_MSGQ_TAIL(channelId), msgQueueTail);
}

/*!
 * @brief Create MCTP header
 *
 * @param[in] som Start of Message flag
 * @param[in] eom End of Message flag
 * @param[in] tag Message tag
 * @param[in] seq Packet sequence number
 *
 * @return Constructed MCTP header
 */
static LwU32
s_fspRpcCreateMctpHeader
(
    LwU8    som,
    LwU8    eom,
    LwU8    tag,
    LwU8    seq
)
{
    return REF_NUM(MCTP_HEADER_SOM, som) |
           REF_NUM(MCTP_HEADER_EOM, eom) |
           REF_NUM(MCTP_HEADER_TAG, tag) |
           REF_NUM(MCTP_HEADER_SEQ, seq);
}

/*!
 * @brief Create LWDM payload header
 *
 * @param[in] lwdmType LWDM type to include in header
 *
 * @return Constructed LWDM payload header
 */
static LwU32
s_fspRpcCreateLwdmHeader
(
    LwU32   lwdmType
)
{
    return REF_DEF(MCTP_MSG_HEADER_TYPE, _VENDOR_PCI) |
           REF_DEF(MCTP_MSG_HEADER_VENDOR_ID, _LW)    |
           REF_NUM(MCTP_MSG_HEADER_LWDM_TYPE, lwdmType);
}

/*!
 * @brief Send one MCTP packet to FSP via EMEM
 *
 * @param[in] pPacketHeader  pointer to MCTP packet header
 * @param[in] headerSize     size of packet Header in DWORDs
 * @param[in] pPacketPayload payload size in bytes
 * @param[in] payloadSize    size of packet payload in DWORDs
 *
 * @return LW_OK or LW_ERR_ILWALID_ARGUMENT
 */
static LW_STATUS
s_fspRpcSendPacket
(
    LwU32 *pPacketHeader,
    LwU32  headerSizeDwords,
    LwU32 *pPacketPayload,
    LwU32  payloadSizeDwords
)
{
    LW_STATUS status;
    LwU32 headerSizeBytes;
    LwU32 payloadSizeBytes;
    LwU32 totalSize = headerSizeDwords + payloadSizeDwords;

    if (totalSize > fspRpcClientConfig.channelSizeDwords)
    {
        return LW_ERR_ILWALID_ARGUMENT;
    }

    status = s_fspRpcConfigEmemc(fspRpcClientConfig.channelId,
                                 fspRpcClientConfig.channelOffsetDwords);
    if (status != LW_OK)
    {
        return status;
    }

    s_fspRpcWriteToEmem(fspRpcClientConfig.channelId, pPacketHeader, headerSizeDwords);
    s_fspRpcWriteToEmem(fspRpcClientConfig.channelId, pPacketPayload, payloadSizeDwords);

    //
    // Update HEAD and TAIL with new EMEM offset; Unlike other places, the
    // offset is specified in bytes here.
    //
    headerSizeBytes = headerSizeDwords * FSP_RPC_BYTES_PER_DWORD;
    payloadSizeBytes = payloadSizeDwords * FSP_RPC_BYTES_PER_DWORD;
    s_fspRpcQueueHeadTailRequestSet(fspRpcClientConfig.channelId,
                                    FSP_EMEM_CHANNEL_PMU_SOE_OFFSET,
                                    FSP_EMEM_CHANNEL_PMU_SOE_OFFSET +
                                    headerSizeBytes + payloadSizeBytes - 1 /* DWORD */);

    return LW_OK;
}

/*!
 * @brief determine whether message received fits in a single packet.
 *
 * @param[in] mctpHeader MCTP Header retrieved from the packet
 *
 * @return LW_TRUE  if message can fit in a single packet
 *         LW_FALSE otherwise
 */
static LwBool
s_fspRpcIsSinglePacket
(
    LwU32 mctpHeader
)
{
    return REF_VAL(MCTP_HEADER_SOM, mctpHeader) &&
           REF_VAL(MCTP_HEADER_EOM, mctpHeader);
}

/*!
 * @brief Validate all the expected fields in the payload header.
 *
 * @param[in] mctpPayloadHeader Payload header from the message.
 * @param[in] expectedLwdmType  LWDM type expected in the message
 *
 * @return LW_TRUE  if all expected fields are present in the header.
 *         LW_FALSE otherwise
 */
static LwBool
s_fspRpcValidateMctpPayloadHeader
(
    LwU32 mctpPayloadHeader,
    LwU32 expectedLwdmType
)
{
    LwU16 mctpVendorId;
    LwU8  mctpMessageType;
    LwU32 lwdmType;

    mctpMessageType = REF_VAL(MCTP_MSG_HEADER_TYPE, mctpPayloadHeader);
    if (mctpMessageType != MCTP_MSG_HEADER_TYPE_VENDOR_PCI)
    {
        dbgPrintf(LEVEL_DEBUG, "Invalid MCTP Message type 0x%0x, expecting 0x7e (Vendor Defined PCI)\n",
                  mctpMessageType);

        return LW_FALSE;
    }

    mctpVendorId = REF_VAL(MCTP_MSG_HEADER_VENDOR_ID, mctpPayloadHeader);
    if (mctpVendorId != MCTP_MSG_HEADER_VENDOR_ID_LW)
    {
        dbgPrintf(LEVEL_DEBUG, "Invalid PCI Vendor Id 0x%0x, expecting 0x10de (Lwpu)\n",
                  mctpVendorId);

        return LW_FALSE;
    }

    lwdmType = REF_VAL(MCTP_MSG_HEADER_LWDM_TYPE, mctpPayloadHeader);
    if (lwdmType != expectedLwdmType)
    {
        dbgPrintf(LEVEL_DEBUG, "Unexpected LWDM Type %d, expecting %d\n",
                  lwdmType, expectedLwdmType);

        return LW_FALSE;
    }

    return LW_TRUE;
}

