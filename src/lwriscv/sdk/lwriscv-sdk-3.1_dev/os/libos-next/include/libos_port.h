/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#pragma once

#define LibosTimeoutInfinite 0xFFFFFFFFFFFFFFFFULL

#if !LIBOS_TINY
#define LibosPortHandleNone 0xFFFFFFFFULL

/**
 * @brief Intent to transfer handles. 
 * 
 * The kernel will map the handles into the receiving tasks
 * handle space.
 */
#define LibosPortFlagTransferHandles    0x1
#define LibosPortFlagTransferObjects    0x2 /* Kernel internal, array contains pointers not handles */

typedef struct {
    union {
        LibosHandle handle;     // LibosPortFlagTransferHandles
        void     *  object;     // LibosPortFlagTransferObjects
        LwU32       globalPort;
    };
    LwU64       grant;
} LibosHandleTransfer;

typedef struct {
    LwU64                   messageId;
    LwU64                   count;
    LibosHandleTransfer     handles[1];
} LibosMessageHeader;

#define LIBOS_MESSAGE_HEADER(name, count, maxCount)          \
    LwU64               messageId;                           \
    LwU64               count;                               \
    LibosHandleTransfer name[maxCount];



#define LibosCommandSchedulerPortCreate           0x20
#define LibosCommandSchedulerTaskCreate           0x21

typedef struct
{   
    // @in:  AddressSpace, MemorySet
    // @out: Task
    LIBOS_MESSAGE_HEADER(handles, handleCount, 2);

    // Inputs
    LibosPriority priority;
    LwU64         entryPoint;

    // Outputs
    LibosStatus   status;
} LibosSchedulerTaskCreate;



typedef struct
{   
    // @out: Port
    LIBOS_MESSAGE_HEADER(handles, handleCount, 1);

    // Outputs
    LibosStatus   status;
} LibosSchedulerPortCreate;


// Simple task creation API
// core idea: should wait on a specific port that was passed to it
// also: we can copy port handles as part of first message
LibosStatus LibosTaskCreate(
    LibosTaskHandle       * handle,
    LibosPriority           priority, 
    LibosAddressSpaceHandle addressSpace, 
    LibosMemorySetHandle    memorySet,
    LwU64 entryPoint
);

LibosStatus LibosPortCreate(
    LibosPortHandle * handle
);

LibosStatus LibosShuttleCreate(
    LibosShuttleHandle * handle
);
#endif // !LIBOS_TINY


/**
 *  @brief Returns the shuttle to initial state
 *      @note Cancels any outstanding send operation
 *      @note Cancels any outstanding wait operation
 *  @returns
 *      LibosErrorAccess
 *          1. The current isolation pasid is not granted debug privileges to the target isolation
 * pasid.
 *          2. The current isolation pasid doesn't have write permissions to the provided buffer
 *      LibosOk
 *          Successful reset.
 */
LibosStatus LibosShuttleReset(LibosShuttleHandle shuttleIndex);


/**
 *  @brief Document synchronous port API
 *  @returns
 *      LibosErrorAccess
 *          1. The current isolation pasid is not granted debug privileges to the target isolation
 * pasid.
 *          2. The current isolation pasid doesn't have write permissions to the provided buffer
 *          3. We're in a scheduler critical section
 *      LibosOk
 *          Successful reset.
 */

LibosStatus LibosPortSyncSend(
    LibosPortHandle sendPort, void *sendPayload, LwU64 sendPayloadSize,
    LwU64 *completedSize, LwU64 timeout, LwU64 flags);

LibosStatus LibosPortSyncRecv(
    LibosPortHandle recvPort, void *recvPayload, LwU64 recvPayloadCapacity,
    LwU64 *completedSize, LwU64 timeout, LwU64 flags);

LibosStatus LibosPortSyncSendRecv(
    LibosPortHandle sendPort, void *sendPayload, LwU64 sendPayloadSize,
    LibosPortHandle recvPort, void *recvPayload, LwU64 recvPayloadCapacity,
    LwU64 *completedSize, LwU64 timeout, LwU64 flags);

LibosStatus LibosPortSyncReply(
    void *sendPayload, LwU64 sendPayloadSize, LwU64 *completedSize, LwU64 flags);

/**
 *  @brief Sends a message to a port and then wait for a reply on a different port
 *      - Passes the replyPort to the LibosPortWait on the other end
 *      - Grants privileges to the receiver task for the replyPort
 *        Privileges automatically revoke after a single send.
 *      - In the event of a timeout, both the send and recv shuttle
 *        will remain pending.  This allows the implementation of
 *        user based timer support (better locality for paging).
 *
 *  @params
 *      @param[in] sendShuttle  OPTIONAL
 *          The shuttle that will track the send portion of the request.
 *          If no send is required, this field may be 0.
 *
 *          @note if both sendShuttle and recvShuttle are non-null
 *                the receiving task will be granted a one-time reply
 *                permission to the recvPort.  This may be revoked
 *                by resetting the recvShuttle on this end.
 *
 *      @params[in] sendPort    OPTIONAL
 *          The port to direct the outgoing message to.  The sendShuttle
 *          will track the progress of the operation.
 *
 *          In the event of a reply, this will be 0 and sendShuttle
 *          will be the shuttle previously used for the receieve
 *          operation.  The previous receive is good for one reply.
 *
 *      @params[in] sendPayload/sendPayloadSize  OPTIONAL
 *
 *          Outgoing message buffer. Memory may not be used until
 *          a wait on the sendShuttle returns either error or success.
 *
 *      @params[in] recvShuttle OPTIONAL
 *          The shuttle that will track the receive portion of the request.
 *          If no receive is required, this field may be 0.
 *
 *      @params[in] recvPort OPTIONAL
 *          The port to issue a receive operation.  The recvShuttle will
 *          track the progress of this operation.
 *
 *      @params[in] recvPayload/recvPayloadSize  OPTIONAL
 *
 *          Incoming message buffer. Memory may not be used until
 *          a wait on the sendShuttle returns either error or success.
 *
 *          @note This should be a separate buffer from sendPayload for security
 *                reasons.  It's important to note the recv might complete before
 *                the send in the case of a malicious client or side band message.
 *
 *       @params[in] waitId
 *
 *          Selects which shuttle to block until completion of.  If 0 is specified
 *          the call will return on any shuttle completion belonging to this task.
 *
 *          Wait terminates when
 *              1. Shuttle completes
 *              2. Timeout elapses
 *
 *      @params[in] completedShuttle OPTIONAL
 *
 *          Receives the id of the completed shuttle
 *
 *      @params[in] completedSize OPTIONAL
 *
 *          Receives the amount of data transmitted or received.
 *
 *      @params[in] timeoutNs OPTIONAL
 *
 *          Timeout in nanoseconds from current time before wait elapses.
 *
 *          @note Outstanding shuttles are not reset.  This may be used to implement user mode
 *                timers.
 *
 *  @returns
 *      LibosErrorIncomplete
 *          This oclwrs when the outgoing message is larger than the receiver's buffer.
 *          Both sending and receiving shuttles will return this.
 *
 *          @note This marks the completion of the shuttle.
 *      LibosErrorAccess
 *          This oclwrs when either
 *              1. Caller does not own sendShuttle or recvShuttle
 *              2. Caller does not have SEND permissions on sendPort
 *              3. Caller does not have OWNER permissions on recvPort
 *              4. Attempted wait on a shuttle while in scheduler critical section
 *      LibosErrorArgument
 *          This oclwrs when one of the two ports is not in reset.
 *
 *      LibosErrorTimeout
 *          Timeout elapsed befor shuttles completed.
 *
 *      LibosOk
 *          Successful completion of shuttle.
 */
LibosStatus LibosPortSendRecvAndWait(
    LibosShuttleHandle sendShuttle,     // A0
    LibosPortHandle    sendPort,        // A1
    void *     sendPayload,     // A2
    LwU64              sendPayloadSize, // A3

    LibosShuttleHandle recvShuttle,     // A4
    LibosPortHandle    recvPort,        // A5
    void *     recvPayload,     // A6
    LwU64              recvPayloadSize, // A7

    LibosShuttleHandle waitId,               // T1
    LibosShuttleHandle *completedShuttle,    // A6 [out]
    LwU64 *            completedSize,        // A4 [out]

    LwU64              timeoutNs,       // T2
    LwU64              flags            // T3
);

LibosStatus LibosPortAsyncReply(
    LibosShuttleHandle sendShuttle, // must be the shuttle we received the message on
    void *sendPayload, LwU64 sendPayloadSize, LwU64 *completedSize, LwU64 flags);

LibosStatus LibosPortAsyncSend(
    LibosShuttleHandle sendShuttle, LibosPortHandle sendPort, void *sendPayload, LwU64 sendPayloadSize, LwU64 flags);

LibosStatus LibosPortAsyncRecv(
    LibosShuttleHandle recvShuttle, LibosPortHandle recvPort, void *recvPayload, LwU64 recvPayloadCapacity, LwU64 flags);

LibosStatus LibosPortAsyncSendRecv(
    LibosShuttleHandle sendShuttle, LibosPortHandle sendPort, void *sendPayload, LwU64 sendPayloadSize,
    LibosShuttleHandle recvShuttle, LibosPortHandle recvPort, void *recvPayload, LwU64 recvPayloadCapacity, LwU64 flags);

LibosStatus LibosPortWait(
    LibosShuttleHandle waitShuttle,
    LibosShuttleHandle *completedShuttle,  // A5
    LwU64 *completedSize,                  // T2
    LwU64 timeoutNs                        // A6
);

