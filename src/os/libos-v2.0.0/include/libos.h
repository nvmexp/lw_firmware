/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBOS_H_
#define LIBOS_H_

#ifndef LIBOS_KERNEL
#    include "sdk/lwpu/inc/lwmisc.h"
#    include "sdk/lwpu/inc/lwtypes.h"
#endif

#include "libos_ilwerted_pagetable.h"
#include "libos_status.h"
#include "libos_syscalls.h"
#include "libos_task_state.h"
#include "libos-config.h"

#define LIBOS_TIMEOUT_INFINITE 0xFFFFFFFFFFFFFFFFULL
#define LIBOS_SHUTTLE_NONE     0
#define LIBOS_SHUTTLE_ANY      0xFFFFUL

#define LIBOS_RESOURCE_NONE                0
#define LIBOS_SHUTTLE_DEFAULT              1
#define LIBOS_SHUTTLE_DEFAULT_ALT          2
#define LIBOS_SHUTTLE_INTERNAL_TRAP        3
#define LIBOS_SHUTTLE_INTERNAL_REPLAY      4
#define LIBOS_SHUTTLE_INTERNAL_REPLAY_PORT 5

/**     31            16 15              0
 *      ----------------------------------
 *      |   task-id     |  resource-id   |
 *      ----------------------------------
 *
 *      @note Remote handles are only used for port replies
 *            and will only resolve successfully if the
 *            grant-once field has been set the accessing task
 *
 *            In all other cases tasks can only resolve handles w
 *            with a 0 at the top
 */
typedef LwU16 LibosLocalPortId;
typedef LwU16 LibosLocalShuttleId;
typedef LwU8 LibosTaskId;
#define LIBOS_TASK_SELF 0xFF

typedef LwU8 LibosStatus;

/**
 *  @brief Returns the current time in nanoseconds
 *
 *  @note Time is monotonic across call tolibosSuspendProcessor
 */
static inline LwU64 libosGetTimeNs(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TIMER_GET_NS;
    register LwU64 a0 __asm__("a0");

    __asm__ __volatile__("ecall" : "=r"(a0) : "r"(t0) : "memory");

    return a0;
}

/**
 * @brief Flushes GSP's DCACHE by VA
 *
 * @note This call has no effect on PAGED_TCM mappings.
 *
 * @param base
 *     Start of memory region to ilwalidate
 * @param size
 *     Total length of the region to ilwalidate in bytes
 */
static inline void libosTaskIlwalidateData(void *base, LwU64 size)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_DCACHE_ILWALIDATE;
    register void *a0 __asm__("a0") = base;
    register LwU64 a1 __asm__("a1") = size;
    __asm__ __volatile__("ecall" : "+r"(a0) : "r"(t0), "r"(a1) : "memory");
}

/**
 * @brief Flushes all of GSP's DCACHE
 */
static inline void libosTaskIlwalidateDataAll(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_DCACHE_ILWALIDATE_ALL;
    __asm__ __volatile__("ecall" : : "r"(t0) : "memory");
}

/**
 *  @brief Queries memory attributes associated with the given task pointer
 *
 *  @param[in] pointer
 *      Pointer for which memory attributes are to be queried.
 *  @param[out] accessPerms
 *      Access permissions the calling task possesses for the memory region
 *      associated with the input pointer argument.
 *      Legal possible values include:
 *          LIBOS_MEMORY_ACCESS_MASK_READ
 *              The task has read access to the associated memory region.
 *          LIBOS_MEMORY_ACCESS_MASK_WRITE
 *              The task has write access to the associated memory region.
 *          LIBOS_MEMORY_ACCESS_MASK_EXELWTE
 *              The task may execute code within this region.
 *          LIBOS_MEMORY_ACCESS_MASK_CACHED
 *              This region is accessed by the task via a cached mapping.
 *          LIBOS_MEMORY_ACCESS_MASK_PAGED_TCM
 *              This region is paged into TCM upon access.
 *  @param[out] @optional allocationBase
 *      Start of the valid memory region to which the current task has
 *      irrevocable access (type of access indicated by accessPerms).
 *  @param[out] @optional allocationSize
 *      Extent of the valid memory region in bytes.
 *  @param[out] @optional aperture
 *      Indicates aperture of the associated underlying memory region.
 *      Legal possible values include:
 *          LIBOS_MEMORY_APERTURE_FB
 *              The associated memory region resides in FB memory.
 *          LIBOS_MEMORY_APERTURE_SYSCOH
 *              The associated memory region resides in coherent system memory.
 *          LIBOS_MEMORY_APERTURE_MMIO
 *              Mapping of register region (for GSP this corresponds to PRI
 *              ring).
 *  @param[out] @optional physicalOffset
 *      Indicates offset into aperture of associated underlying memory region.
 *
 *  @returns
 *      LIBOS_ERROR_ILWALID_ARGUMENT
 *          Indicates that the input pointer argument does not lie within a
 *          valid memory region.
 *      LIBOS_ERROR_ILWALID_ACCESS
 *          Indicates that the task does not have read access to associated
 *          memory region.
 *      LIBOS_OK
 *          Indicates that there is a valid memory region associated with the
 *          input pointer argument.
 *
 *      @note These properties are ilwariant at runtime.
 */

#define LIBOS_MEMORY_ACCESS_MASK_READ    U(1)
#define LIBOS_MEMORY_ACCESS_MASK_WRITE   U(2)
#define LIBOS_MEMORY_ACCESS_MASK_EXELWTE U(4)
#define LIBOS_MEMORY_ACCESS_MASK_CACHED  U(8)
#define LIBOS_MEMORY_ACCESS_MASK_PAGED   U(16)

#define LIBOS_MEMORY_APERTURE_FB     U(0)
#define LIBOS_MEMORY_APERTURE_SYSCOH U(1)
#define LIBOS_MEMORY_APERTURE_MMIO   U(2)

static inline LibosStatus libosMemoryQuery(
    LibosTaskId task, void *pointer, LwU32 *accessPerms, void **allocationBase, LwU64 *allocationSize,
    LwU32 *aperture, LwU64 *physicalOffset)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_MEMORY_QUERY;
    register LwU64 a0 __asm__("a0") = task;
    register LwU64 a1 __asm__("a1") = (LwU64)pointer;
    register LwU64 a2 __asm__("a2");
    register LwU64 a3 __asm__("a3");
    register LwU64 a4 __asm__("a4");
    register LwU64 a5 __asm__("a5");

    __asm__ __volatile__("ecall"
                         : "+r"(a0), "+r"(a1), "=r"(a2), "=r"(a3), "=r"(a4), "=r"(a5)
                         : "r"(t0)
                         : "memory");

    if (accessPerms)
        *accessPerms = a1;
    if (allocationBase)
        *allocationBase = (void *)a2;
    if (allocationSize)
        *allocationSize = a3;
    if (aperture)
        *aperture = a4;
    if (physicalOffset)
        *physicalOffset = a5;

    return a0;
}

/**
 *  @brief Returns the shuttle to initial state
 *      @note Cancels any outstanding send operation
 *      @note Cancels any outstanding wait operation
 *  @returns
 *      LIBOS_ERROR_ILWALID_ACCESS
 *          1. The current isolation pasid is not granted debug privileges to the target isolation
 * pasid.
 *          2. The current isolation pasid doesn't have write permissions to the provided buffer
 *      LIBOS_OK
 *          Successful reset.
 */
static inline LibosStatus libosShuttleReset(LibosLocalShuttleId shuttleIndex)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_SHUTTLE_RESET;
    register LwU64 a0 __asm__("a0") = shuttleIndex;

    __asm__ __volatile__("ecall" : "+r"(a0) : "r"(t0) : "memory");

    return a0;
}

/**
 *  @brief Sends a message to a port and then wait for a reply on a different port
 *      - Passes the replyPort to the libosPortWait on the other end
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
 *      @params[in] completedRemoteTaskId OPTIONAL
 *
 *          Returns the id of the sender (or receiver) of this message.
 *          Useful when a single port is accessible to many clients.
 *
 *          @note If ports are shared, users are required to match the task-id returned by send
 *                with the task-id returned with reply.
 *
 *      @params[in] timeoutNs OPTIONAL
 *
 *          Timeout in nanoseconds from current time before wait elapses.
 *
 *          @note Outstanding shuttles are not reset.  This may be used to implement user mode
 *                timers.
 *
 *  @returns
 *      LIBOS_ERROR_INCOMPLETE
 *          This oclwrs when the outgoing message is larger than the receiver's buffer.
 *          Both sending and receiving shuttles will return this.
 *
 *          @note This marks the completion of the shuttle.
 *      LIBOS_ERROR_ILWALID_ACCESS
 *          This oclwrs when either
 *              1. Caller does not own sendShuttle or recvShuttle
 *              2. Caller does not have SEND permissions on sendPort
 *              3. Caller does not have OWNER permissions on recvPort
 *      LIBOS_ERROR_ILWALID_ARGUMENT
 *          This oclwrs when one of the two ports is not in reset.
 *
 *      LIBOS_ERROR_TIMEOUT
 *          Timeout elapsed befor shuttles completed.
 *
 *      LIBOS_OK
 *          Successful completion of shuttle.
 */
static inline LibosStatus libosPortSendRecvAndWait(
    LibosLocalShuttleId sendShuttle,     // A0
    LibosLocalPortId    sendPort,        // A1
    void *              sendPayload,     // A2
    LwU64               sendPayloadSize, // A3

    LibosLocalShuttleId recvShuttle,     // A4
    LibosLocalPortId    recvPort,        // A5
    void *              recvPayload,     // A6
    LwU64               recvPayloadSize, // A7

    LwU64                waitId,                // T1
    LibosLocalShuttleId *completedShuttle,      // A6 [out]
    LwU64 *              completedSize,         // A4 [out]
    LibosTaskId *        completedRemoteTaskId, // A5 [out]
    LwU64                timeoutNs              // T2
)
{
    // IOCTL marker
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_PORT_SEND_RECV_WAIT;

    // Send
    register LwU64 a0 __asm__("a0") = sendShuttle;
    register LwU64 a1 __asm__("a1") = sendPort;
    register LwU64 a2 __asm__("a2") = (LwU64)sendPayload;
    register LwU64 a3 __asm__("a3") = sendPayloadSize;

    // Recv
    register LwU64 a4 __asm__("a4")   = recvShuttle;
    register LwU64 a5 __asm__("a5")   = recvPort;
    register LwU64 a6 __asm__("a6")   = (LwU64)recvPayload;
    register LwU64 a7 __asm__("a7")   = recvPayloadSize;

    // Wait
    register LwU64 t1 __asm__("t1") = waitId;
    register LwU64 t2 __asm__("t2") = timeoutNs;

    __asm__ __volatile__("ecall"
                         : "+r"(a0), "+r"(a4), "+r"(a5), "+r"(a6)
                         : "r"(t0), "r"(t1), "r"(t2), "r"(a1), "r"(a2), "r"(a3), "r"(a7)
                         : "memory");

    if (completedShuttle)
        *completedShuttle = a6;
    if (completedRemoteTaskId)
        *completedRemoteTaskId = a5;
    if (completedSize)
        *completedSize = a4;
    return a0;
}

static inline LibosStatus libosPortAsyncSend(
    LibosLocalShuttleId sendShuttle, LibosLocalPortId sendPort, void *sendPayload, LwU64 sendPayloadSize)
{
    LibosStatus status = libosPortSendRecvAndWait(
        // Send
        sendShuttle, sendPort, sendPayload, sendPayloadSize,

        // Recv
        LIBOS_SHUTTLE_NONE, 0, 0, 0,

        // Wait
        LIBOS_SHUTTLE_NONE, 0, 0, 0, 0);

    return status;
}
static inline LibosStatus libosPortSyncSend(
    LibosLocalShuttleId sendShuttle, LibosLocalPortId sendPort, void *sendPayload, LwU64 sendPayloadSize,
    LwU64 *completedSize, LwU64 timeout)
{
    LibosStatus status = libosPortSendRecvAndWait(
        // Send
        sendShuttle, sendPort, sendPayload, sendPayloadSize,

        // Recv
        LIBOS_SHUTTLE_NONE, 0, 0, 0,

        // Wait
        sendShuttle, 0, completedSize, 0, timeout);
    if (status == LIBOS_ERROR_TIMEOUT)
        libosShuttleReset(LIBOS_SHUTTLE_DEFAULT);
    return status;
}

// @todo Document that reply that only be sent to a combined SendRecv
//       Since the water is guaranteed to already be waiting -- the operation is guaranteed not to
//       wait.
static inline LibosStatus libosPortReply(
    LibosLocalShuttleId sendShuttle, // must be the shuttle we received the message on
    void *sendPayload, LwU64 sendPayloadSize, LwU64 *completedSize)
{
    return libosPortSyncSend(sendShuttle, 0, sendPayload, sendPayloadSize, completedSize, 0);
}

static inline LibosStatus libosPortSyncRecv(
    LibosLocalShuttleId recvShuttle, LibosLocalPortId recvPort, void *recvPayload, LwU64 recvPayloadCapacity,
    LwU64 *completedSize, LwU64 timeout)
{
    LibosStatus status = libosPortSendRecvAndWait(
        LIBOS_SHUTTLE_NONE, 0, 0, 0, recvShuttle, recvPort, recvPayload, recvPayloadCapacity, recvShuttle, 0,
        completedSize, 0, timeout);
    if (status == LIBOS_ERROR_TIMEOUT)
        libosShuttleReset(LIBOS_SHUTTLE_DEFAULT);
    return status;
}

static inline LibosStatus libosPortAsyncRecv(
    LibosLocalShuttleId recvShuttle, LibosLocalPortId recvPort, void *recvPayload, LwU64 recvPayloadCapacity)
{
    LibosStatus status = libosPortSendRecvAndWait(
        LIBOS_SHUTTLE_NONE, 0, 0, 0,
        recvShuttle, recvPort, recvPayload, recvPayloadCapacity,
        LIBOS_SHUTTLE_NONE, 0, 0, 0, 0);
    return status;
}

static inline LibosStatus libosPortSyncSendRecv(
    LibosLocalShuttleId sendShuttle, LibosLocalPortId sendPort, void *sendPayload, LwU64 sendPayloadSize,
    LibosLocalShuttleId recvShuttle, LibosLocalPortId recvPort, void *recvPayload, LwU64 recvPayloadCapacity,
    LwU64 *completedSize, LwU64 timeout)
{
    LibosStatus status = libosPortSendRecvAndWait(
        // Send
        sendShuttle, sendPort, sendPayload, sendPayloadSize,

        // Recv
        recvShuttle, recvPort, recvPayload, recvPayloadCapacity,

        // Wait
        recvShuttle, 0, completedSize, 0, timeout);

    if (status == LIBOS_OK || status == LIBOS_ERROR_TIMEOUT)
        libosShuttleReset(sendShuttle);

    if (status == LIBOS_ERROR_TIMEOUT)
        libosShuttleReset(recvShuttle);

    return status;
}

static inline LibosStatus libosPortAsyncSendRecv(
    LibosLocalShuttleId sendShuttle, LibosLocalPortId sendPort, void *sendPayload, LwU64 sendPayloadSize,
    LibosLocalShuttleId recvShuttle, LibosLocalPortId recvPort, void *recvPayload, LwU64 recvPayloadCapacity)
{
    LibosStatus status = libosPortSendRecvAndWait(
        // Send
        sendShuttle, sendPort, sendPayload, sendPayloadSize,

        // Recv
        recvShuttle, recvPort, recvPayload, recvPayloadCapacity,

        // Wait
        LIBOS_SHUTTLE_NONE, 0, 0, 0, 0);
    return status;
}

static inline LibosStatus libosPortWait(
    LibosLocalShuttleId waitShuttle,
    LibosLocalShuttleId *completedShuttle, // A5
    LwU64 *completedSize,                  // T2
    LibosTaskId *completedRemoteTaskId,    // T1
    LwU64 timeoutNs                        // A6
)
{
    LibosStatus status = libosPortSendRecvAndWait(
        // Send
        LIBOS_SHUTTLE_NONE, 0, 0, 0,

        // Recv
        LIBOS_SHUTTLE_NONE, 0, 0, 0,

        // Wait
        waitShuttle, completedShuttle, completedSize, completedRemoteTaskId, timeoutNs);
    return status;
}

/**
 * @brief Yield the GSP to next runnable task
 */
static inline void libosTaskYield(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_YIELD;
    __asm__ __volatile__("ecall" : : "r"(t0) : "memory");
}

/**
 * @brief Put the GSP into the suspended state
 *
 * This call issues a writeback and ilwalidate to the calling task's
 * page entries prior to notifying the host CPU that the GSP is
 * entering the suspended state.
 */
static inline void libosProcessorSuspend(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PROCESSOR_SUSPEND;
    __asm__ __volatile__("ecall" : : "r"(t0) : "memory");
}

/**
 * @brief Switch to another LWRISCV partition.
 * 
 * This call prepares the tasks and kernel for suspension, similar to
 * libosProcessorSuspend(), and issues an ecall to switch to another
 * partition. The arguments to this syscall are passed through directly
 * to the ecall.
 * 
 * When the partition is rebooted, the calling task will be resumed from
 * where it called this syscall.
 */
static inline void libosPartitionSwitch(
    LwU64 arg0,
    LwU64 arg1,
    LwU64 arg2,
    LwU64 arg3,
    LwU64 from_partition,
    LwU64 to_partition
)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PARTITION_SWITCH;
    register LwU64 a0 __asm__("a0") = arg0;
    register LwU64 a1 __asm__("a1") = arg1;
    register LwU64 a2 __asm__("a2") = arg2;
    register LwU64 a3 __asm__("a3") = arg3;
    register LwU64 a4 __asm__("a4") = from_partition;
    register LwU64 a5 __asm__("a5") = to_partition;

    __asm__ __volatile__("ecall"
                         :
                         : "r"(t0), "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5)
                         : "memory");
}

/**
 * @brief Returns the task state for a lwrrently stopped task
 *
 * @note This function has undefined behavior if the task is running or restarted
 *       while the state is being copied.  Registers returned may be a mix of prior and current
 * states.
 *
 * @returns
 *     LIBOS_ERROR_ILWALID_ACCESS
 *         1. The current isolation pasid is not granted debug privileges to the target isolation
 * pasid.
 *         2. The current isolation pasid doesn't have write permissions to the provided buffer
 *     LIBOS_ERROR_ILWALID_ARGUMENT
 *         An invalid task index was provided.
 *     LIBOS_OK
 *         Successful copy of state was made.
 */
static inline LibosStatus libosTaskStateQuery(LwU32 taskIndex, LibosTaskState *state)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_STATE_QUERY;
    register LwU64 a0 __asm__("a0")  = taskIndex;
    register LwU64 a1 __asm__("a1")  = (LwU64)state;

    __asm__ __volatile__("ecall"
                         : "+r"(a0)
                         : "r"(t0), "r"(a1)
                         : "memory");

    return a0;
}

/**
 * @brief Reads the memory of a remote process.
 *
 * @note This function requires debug priveleges for the remote task.  It's intended to be used by
 * the init task to support debugging and call-stack printing.
 *
 * @returns
 *     LIBOS_ERROR_ILWALID_ACCESS
 *         1. The current isolation pasid is not granted debug privileges to the target isolation
 * pasid.
 *         2. The current isolation pasid doesn't have write permissions to the provided buffer
 *     LIBOS_ERROR_ILWALID_ARGUMENT
 *         An invalid task index was provided.
 *     LIBOS_OK
 *         Successful copy of state was made.
 */
static inline LibosStatus
libosTaskMemoryRead(void *buffer, LwU32 remoteTask, LwU64 remoteAddress, LwU64 length)
{
    register LwU64 t0 __asm__("t0")  = LIBOS_SYSCALL_TASK_MEMORY_READ;
    register LwU64 a0 __asm__("a0")  = (LwU64)buffer;
    register LwU64 a1 __asm__("a1")  = remoteTask;
    register LwU64 a2 __asm__("a2")  = remoteAddress;
    register LwU64 a3 __asm__("a3")  = length;

    __asm__ __volatile__("ecall"
                         : "+r"(a0)
                         : "r"(t0), "r"(a1), "r"(a2), "r"(a3)
                         : "memory");

    return a0;
}

/**
 * @brief Register a TCM bounce buffer to be used for DMA when necessary.
 *
 * The TCM buffer should be declared with the PAGED_TCM memory region
 * attribute. This buffer may be used by libos when the task requests
 * DMA through libosDmaCopy().
 *
 * @param[in] va    Virtual address of the TCM bounce buffer
 * @param[in] size  Size in bytes of the TCM bounce buffer
 *
 * @note The bounce buffer should be 4K-aligned and a multiple of 4K.
 *
 * @returns
 *    LIBOS_ERROR_ILWALID_ACCESS
 *      Either va or size is not 4K-aligned
 *      The specified buffer does not reside in a valid PAGED_TCM memory region
 *    LIBOS_OK
 *      The TCM bounce buffer was registered successfully
 */
static inline LibosStatus libosDmaRegisterTcm(LwU64 va, LwU32 size)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_DMA_REGISTER_TCM;
    register LwU64 a0 __asm__("a0") = va;
    register LwU64 a1 __asm__("a1") = size;

    __asm__ __volatile__("ecall" : "+r"(a0) : "r"(t0), "r"(a1) : "memory");
    return a0;
}

/**
 * @brief Use DMA engine to transfer data between buffers.
 *
 * This function will use TCM as a bounce buffer only where necessary (e.g.,
 * where the DMA engine does not support direct copies from source to
 * destination). In those cases, a TCM bounce buffer must have been previously
 * registered by the task via libosDmaRegisterTcm().
 *
 * @param[in] dstVa    Virtual address of destination buffer
 * @param[in] srcVa    Virtual address of source buffer
 * @param[in] size     Number of bytes to transfer
 *
 * @note The virtual addresses and size passed to this function must be 4-byte aligned.
 *
 * @returns
 *    LIBOS_ERROR_ILWALID_ACCESS
 *      One of the input arguments is not 4-byte aligned.
 *      TCM bounce buffer required for DMA but not registered by task.
 *      srcVa/dstVa argument does not represent a valid memory region.
 *    LIBOS_ERROR_OPERATION_FAILED
 *      The DMA engine encountered a failure while queuing the transfer.
 *    LIBOS_OK
 *      The DMA transfer was queued successfully.
 */
static inline LibosStatus libosDmaCopy(LwU64 dstVa, LwU64 srcVa, LwU32 size)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_DMA_COPY;
    register LwU64 a0 __asm__("a0") = dstVa;
    register LwU64 a1 __asm__("a1") = srcVa;
    register LwU32 a2 __asm__("a2") = size;

    __asm__ __volatile__("ecall" : "+r"(a0) : "r"(t0), "r"(a1), "r"(a2) : "memory");
    return a0;
}

/**
 * @brief Issue a DMA flush operation.
 *
 * This call waits for all pending DMA operations to complete before returning.
 */
static inline void libosDmaFlush(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_DMA_FLUSH;
    __asm__ __volatile__("ecall" : : "r"(t0) : "memory");
}

/**
 * @brief Register ilwerted page-table (init task only)
 */
static inline void libosInitRegisterPagetables(libos_pagetable_entry_t *hash, LwU64 lengthPow2, LwU64 probeCount)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_INIT_REGISTER_PAGETABLES;
    register LwU64 a0 __asm__("a0") = (LwU64)hash;
    register LwU64 a1 __asm__("a1") = lengthPow2;
    register LwU64 a2 __asm__("a2") = probeCount;
    __asm__ __volatile__("ecall" : : "r"(t0), "r"(a0), "r"(a1), "r"(a2) : "memory");
}

/**
 * @brief Enable GSP profiler and set up the hardware profiling counters.
 */
static inline void libosProfilerEnable(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PROFILER_ENABLE;
    __asm__ __volatile__("ecall" : : "r"(t0) : "memory");
}

/**
 * @brief Get mask of enabled profiling counters.
 *
 * @returns
 *     The value of scounteren, which is a mask of counters that are enabled and
 *     accessible.  Reading counters not in this mask will generate illegal
 *     instruction faults.
 */
static inline LwU64 libosProfilerGetCounterMask(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PROFILER_GET_COUNTER_MASK;
    register LwU64 a0 __asm__("a0");
    __asm__ __volatile__("ecall" : "=r"(a0) : "r"(t0) : "memory");
    return a0;
}

/**
 * @brief Exit the current task such that it will no longer run.
 *
 * The exit code will be available to TASK_INIT.
 * This syscall does not return.
 *
 * Note: this syscall was made with voluntary task termination in mind
 * although it lwrrently has the same effect as libosTaskPanic (and is
 * handled by the trap handling in TASK_INIT).
 *
 * @param[in] exitCode   Exit code
 */

__attribute__((noreturn))
static inline void libosTaskExit(LwU64 exitCode)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_EXIT;
    register LwU64 a0 __asm__("a0") = exitCode;
    __asm__ __volatile__("ecall" : : "r"(t0), "r"(a0) : "memory");
    __builtin_unreachable();
}

/**
 * @brief Panic the current task such that it will no longer run.
 *
 * The panic reason will be available to TASK_INIT's task trap handling.
 * This syscall does not return.
 *
 * Note: this syscall is also available via a function-like macro wrapper
 * (that only uses basic asm) to allow for its use in attribute((naked))
 * functions.
 *
 * @param[in] reason   Reason for panic
 */

// used by assertion/validation macros on assertion failures
#define LIBOS_TASK_PANIC_REASON_ASSERT_FAILED                         U(1)
// similar to assert, but for conditions checked manually
#define LIBOS_TASK_PANIC_REASON_CONDITION_FAILED                      U(2)
// used to signify that a particular location should be unreachable
#define LIBOS_TASK_PANIC_REASON_UNREACHABLE                           U(3)
// used by stack smashing protection (SSP) on canary check failures
#define LIBOS_TASK_PANIC_REASON_SSP_STACK_CHECK_FAILED                U(4)
// used by address sanitization (ASAN) on memory errors
//   args: addr, size, write, pc, badAddr
#define LIBOS_TASK_PANIC_REASON_ASAN_MEMORY_ERROR                     U(5)

// function-like macro wrapper for libosTaskPanic
#define LIBOS_TASK_PANIC_NAKED(reason)                                \
    __asm__ __volatile__(                                             \
        "li t0, %[IMM_SYSCALL_TASK_PANIC];"                           \
        "li a0, %[imm_reason];"                                       \
        "ecall;"                                                      \
        :: [IMM_SYSCALL_TASK_PANIC] "i"(LIBOS_SYSCALL_TASK_PANIC),    \
           [imm_reason]             "i"((reason))                     \
    )

__attribute__((noreturn))
static inline void libosTaskPanic(LwU64 reason)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_PANIC;
    register LwU64 a0 __asm__("a0") = reason;
    __asm__ __volatile__("ecall" : : "r"(t0), "r"(a0) : "memory");
    __builtin_unreachable();
}

__attribute__((noreturn))
static inline void libosTaskPanicWithArgs(
    LwU64 reason, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4, LwU64 arg5)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_PANIC;
    register LwU64 a0 __asm__("a0") = reason;
    register LwU64 a1 __asm__("a1") = arg1;
    register LwU64 a2 __asm__("a2") = arg2;
    register LwU64 a3 __asm__("a3") = arg3;
    register LwU64 a4 __asm__("a4") = arg4;
    register LwU64 a5 __asm__("a5") = arg5;
    __asm__ __volatile__(
        "ecall" : : "r"(t0), "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5) : "memory"
    );
    __builtin_unreachable();
}

#ifdef PORT_EXTINTR
static inline LibosStatus libosInterruptAsyncRecv(LibosLocalShuttleId interruptShuttle, LwU32 interruptWaitingMask)
{
    // Queue a receive on the interrupt port
    LibosStatus status = libosPortAsyncRecv(interruptShuttle, PORT_EXTINTR, 0, 0);
    if (status != LIBOS_OK)
        return status;

    // Reset and clear any prior interrupts to avoid race where we miss the interrupt
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_INTERRUPT_ARM_EXTERNAL;
    register LwU64 a0 __asm__("a0") = interruptWaitingMask;
    __asm__ __volatile__("ecall" : : "r"(t0), "r"(a0) : "memory");

    return LIBOS_OK;
}
#endif


#endif
