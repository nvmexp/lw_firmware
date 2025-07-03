/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FSP_RPC_H
#define FSP_RPC_H

#include "lwstatus.h"
#include "lwtypes.h"
#include "lwuproc.h"

/*
 * @file    fsp_rpc.h
 * @copydoc fsp_rpc.c
 */

#define FSP_RPC_BYTES_PER_DWORD       sizeof(LwU32)

/*
 * Contains client-specific properties and functions.
 */
typedef struct {
    /*
     * @brief EMEM channel Id corresponding to the channel
     */
    LwU32 channelId;

    /*
     * @brief offset (in number of DWORDS) withing EMEM where channel starts
     */
    LwU32 channelOffsetDwords;

    /*
     * @brief size (in DWORDS) of the EMEM channel in FSP allocated to the client.
     */
    LwU32 channelSizeDwords;

    /*
     * @brief specifies whether both client and FSP can initiate communications
     */
    LwBool bDuplexChannel;
} FSP_RPC_CLIENT_INIT_DATA;

/*
 * Initialize FSP Library
 *
 * @param initData Data used for initialization
 *
 * @return LW_OK, or LW_ERR_NO_MEMORY
 */
LW_STATUS fspRpcInit(FSP_RPC_CLIENT_INIT_DATA *initData)
    GCC_ATTRIB_SECTION("imem_libFspRpcInit", "fspRpcInit");

/*
 * @brief Send a MCTP message to FSP via EMEM. This message may be broken down
 * into multiple pieces if it is too large.
 *
 * NOTE 1: Lwrrently on the PMU, there is a potential for deadlock if
 * simultaneous communication between PMU and FSP is desired. This can occur when
 * FSP send a request to a task while the task is in the process of sending a
 * request. In order to solve this, a central task needs to be introduced which
 * will send messages to FSP on behalf of all tasks.
 *
 * NOTE 2: Lwrrently this design assumes (and supports) only one task per client
 * sending messages to FSP. If there is a client where multiple tasks will be
 * sending messages, we need to add a lock here to ensure any message spanning
 * multiple packets is fully sent to FSP before a message from another task can
 * be sent.
 *
 * @param[in] pPayload    pointer to Payload
 * @param[in] sizeDwords  size of the payload in DWORD elements
 * @param[in] lwdmType    LWDM message type of the message to send.
 * @param[in] timeoutUs   Maximum time to wait for each packet within the message to send.
 * @param[in] syncRequest Determines whether any response is required for the request.
 *
 * @return LW_OK                   Message successfully sent to FSP
 *         LW_ERR_NOT_READY        FSP RPC library has not been initialized
 *         LW_ERR_ILWALID_ARGUMENT Invalid argument passed
 *         LW_ERR_TIMEOUT          Timeout oclwred waiting for message to be read
 */
LW_STATUS fspRpcMessageSend(LwU32 *pPayload, LwU32 sizeDwords, LwU32 lwdmType, LwU32 timeoutUs, LwBool bSyncRequest)
    GCC_ATTRIB_SECTION("imem_libFspRpc", "fspRpcMessageSend");

/*
 * @brief Receive MCTP message from FSP via EMEM.
 *
 * @param[in, out] pPayloadBuffer     Pointer to buffer where data will be copied
 * @param[in]      bufferSizeDwords   Size of the buffer in DWORD elements.
 * @param[in, out] pPayloadSizeDwords Number of DWORDS copied to the buffer.
 * @param[in]      expectedLwdmType   Expected LWDM Message Type for received message
 * @param[in]      timeoutUs          Time to wait before bailing out.
 *
 * @return LW_OK                   Message was received and processed.
 *         LW_ERR_NOT_READY        No data available in the EMEM channel
 *         LW_ERR_ILWALID_ARGUMENT One of the arguments was invalid
 *         LW_ERR_ILWALID_DATA     Invalid data found in packet header
 *         LW_ERR_BUFFER_TOO_SMALL The buffer provided to hold the data wasnt large enough
 *         LW_ERR_TIMEOUT          Timeout oclwred.
 */
LW_STATUS fspRpcMessageReceive(LwU32 *pBuffer, LwU32 bufferSizeDwords, LwU32 *pPayloadSizeDwords, LwU32 expectedLwdmType, LwU32 timeoutUs)
    GCC_ATTRIB_SECTION("imem_libFspRpc", "fspRpcMessageReceive");

#endif // FSP_RPC_H

