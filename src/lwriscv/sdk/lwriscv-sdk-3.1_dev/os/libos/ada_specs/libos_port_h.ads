pragma Ada_2012;
pragma Style_Checks (Off);

with libos_manifest_h;
with libos_status_h;
with System;
with lwtypes_h;

package libos_port_h is

   LibosTimeoutInfinite : constant := 16#FFFFFFFFFFFFFFFF#;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_port.h:11

   -- _LWRM_COPYRIGHT_BEGIN_
   -- *
   -- * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
   -- * information contained herein is proprietary and confidential to LWPU
   -- * Corporation.  Any use, reproduction, or disclosure without the written
   -- * permission of LWPU Corporation is prohibited.
   -- *
   -- * _LWRM_COPYRIGHT_END_
   --

   --*
   -- *  @brief Returns the shuttle to initial state
   -- *      @note Cancels any outstanding send operation
   -- *      @note Cancels any outstanding wait operation
   -- *  @returns
   -- *      LibosErrorAccess
   -- *          1. The current isolation pasid is not granted debug privileges to the target isolation
   -- * pasid.
   -- *          2. The current isolation pasid doesn't have write permissions to the provided buffer
   -- *      LibosOk
   -- *          Successful reset.
   --

   function LibosShuttleReset (shuttleIndex : libos_manifest_h.LibosShuttleId) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_port.h:25
     with Import => True,
     Convention => C,
     External_Name => "LibosShuttleReset";

   --*
   -- *  @brief Document synchronous port API
   -- *  @returns
   -- *      LibosErrorAccess
   -- *          1. The current isolation pasid is not granted debug privileges to the target isolation
   -- * pasid.
   -- *          2. The current isolation pasid doesn't have write permissions to the provided buffer
   -- *          3. We're in a scheduler critical section
   -- *      LibosOk
   -- *          Successful reset.
   --

   function LibosPortSyncSend
     (sendPort : libos_manifest_h.LibosPortId;
      sendPayload : System.Address;
      sendPayloadSize : lwtypes_h.LwU64;
      completedSize : access lwtypes_h.LwU64;
      timeout : lwtypes_h.LwU64) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_port.h:40
     with Import => True,
     Convention => C,
     External_Name => "LibosPortSyncSend",
     Global => null;

   function LibosPortSyncRecv
     (recvPort : libos_manifest_h.LibosPortId;
      recvPayload : System.Address;
      recvPayloadCapacity : lwtypes_h.LwU64;
      completedSize : access lwtypes_h.LwU64;
      timeout : lwtypes_h.LwU64) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_port.h:44
     with Import => True,
     Convention => C,
     External_Name => "LibosPortSyncRecv";

   function LibosPortSyncSendRecv
     (sendPort : libos_manifest_h.LibosPortId;
      sendPayload : System.Address;
      sendPayloadSize : lwtypes_h.LwU64;
      recvPort : libos_manifest_h.LibosPortId;
      recvPayload : System.Address;
      recvPayloadCapacity : lwtypes_h.LwU64;
      completedSize : access lwtypes_h.LwU64;
      timeout : lwtypes_h.LwU64) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_port.h:48
     with Import => True,
     Convention => C,
     External_Name => "LibosPortSyncSendRecv";

   function LibosPortSyncReply
     (sendPayload : System.Address;
      sendPayloadSize : lwtypes_h.LwU64;
      completedSize : access lwtypes_h.LwU64) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_port.h:53
     with Import => True,
     Convention => C,
     External_Name => "LibosPortSyncReply";

   --*
   -- *  @brief Sends a message to a port and then wait for a reply on a different port
   -- *      - Passes the replyPort to the LibosPortWait on the other end
   -- *      - Grants privileges to the receiver task for the replyPort
   -- *        Privileges automatically revoke after a single send.
   -- *      - In the event of a timeout, both the send and recv shuttle
   -- *        will remain pending.  This allows the implementation of
   -- *        user based timer support (better locality for paging).
   -- *
   -- *  @params
   -- *      @param[in] sendShuttle  OPTIONAL
   -- *          The shuttle that will track the send portion of the request.
   -- *          If no send is required, this field may be 0.
   -- *
   -- *          @note if both sendShuttle and recvShuttle are non-null
   -- *                the receiving task will be granted a one-time reply
   -- *                permission to the recvPort.  This may be revoked
   -- *                by resetting the recvShuttle on this end.
   -- *
   -- *      @params[in] sendPort    OPTIONAL
   -- *          The port to direct the outgoing message to.  The sendShuttle
   -- *          will track the progress of the operation.
   -- *
   -- *          In the event of a reply, this will be 0 and sendShuttle
   -- *          will be the shuttle previously used for the receieve
   -- *          operation.  The previous receive is good for one reply.
   -- *
   -- *      @params[in] sendPayload/sendPayloadSize  OPTIONAL
   -- *
   -- *          Outgoing message buffer. Memory may not be used until
   -- *          a wait on the sendShuttle returns either error or success.
   -- *
   -- *      @params[in] recvShuttle OPTIONAL
   -- *          The shuttle that will track the receive portion of the request.
   -- *          If no receive is required, this field may be 0.
   -- *
   -- *      @params[in] recvPort OPTIONAL
   -- *          The port to issue a receive operation.  The recvShuttle will
   -- *          track the progress of this operation.
   -- *
   -- *      @params[in] recvPayload/recvPayloadSize  OPTIONAL
   -- *
   -- *          Incoming message buffer. Memory may not be used until
   -- *          a wait on the sendShuttle returns either error or success.
   -- *
   -- *          @note This should be a separate buffer from sendPayload for security
   -- *                reasons.  It's important to note the recv might complete before
   -- *                the send in the case of a malicious client or side band message.
   -- *
   -- *       @params[in] waitId
   -- *
   -- *          Selects which shuttle to block until completion of.  If 0 is specified
   -- *          the call will return on any shuttle completion belonging to this task.
   -- *
   -- *          Wait terminates when
   -- *              1. Shuttle completes
   -- *              2. Timeout elapses
   -- *
   -- *      @params[in] completedShuttle OPTIONAL
   -- *
   -- *          Receives the id of the completed shuttle
   -- *
   -- *      @params[in] completedSize OPTIONAL
   -- *
   -- *          Receives the amount of data transmitted or received.
   -- *
   -- *      @params[in] completedRemoteTaskId OPTIONAL
   -- *
   -- *          Returns the id of the sender (or receiver) of this message.
   -- *          Useful when a single port is accessible to many clients.
   -- *
   -- *          @note If ports are shared, users are required to match the task-id returned by send
   -- *                with the task-id returned with reply.
   -- *
   -- *      @params[in] timeoutNs OPTIONAL
   -- *
   -- *          Timeout in nanoseconds from current time before wait elapses.
   -- *
   -- *          @note Outstanding shuttles are not reset.  This may be used to implement user mode
   -- *                timers.
   -- *
   -- *  @returns
   -- *      LibosErrorIncomplete
   -- *          This oclwrs when the outgoing message is larger than the receiver's buffer.
   -- *          Both sending and receiving shuttles will return this.
   -- *
   -- *          @note This marks the completion of the shuttle.
   -- *      LibosErrorAccess
   -- *          This oclwrs when either
   -- *              1. Caller does not own sendShuttle or recvShuttle
   -- *              2. Caller does not have SEND permissions on sendPort
   -- *              3. Caller does not have OWNER permissions on recvPort
   -- *              4. Attempted wait on a shuttle while in scheduler critical section
   -- *      LibosErrorArgument
   -- *          This oclwrs when one of the two ports is not in reset.
   -- *
   -- *      LibosErrorTimeout
   -- *          Timeout elapsed befor shuttles completed.
   -- *
   -- *      LibosOk
   -- *          Successful completion of shuttle.
   --

   function LibosPortSendRecvAndWait
     (sendShuttle : libos_manifest_h.LibosShuttleId;
      sendPort : libos_manifest_h.LibosPortId;
      sendPayload : System.Address;
      sendPayloadSize : lwtypes_h.LwU64;
      recvShuttle : libos_manifest_h.LibosShuttleId;
      recvPort : libos_manifest_h.LibosPortId;
      recvPayload : System.Address;
      recvPayloadSize : lwtypes_h.LwU64;
      waitId : libos_manifest_h.LibosShuttleId;
      completedShuttle : access libos_manifest_h.LibosShuttleId;
      completedSize : access lwtypes_h.LwU64;
      completedRemoteTaskId : access libos_manifest_h.LibosTaskId;
      timeoutNs : lwtypes_h.LwU64) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_port.h:158
     with Import => True,
     Convention => C,
     External_Name => "LibosPortSendRecvAndWait";

   -- A0
   -- A1
   -- A2
   -- A3
   -- A4
   -- A5
   -- A6
   -- A7
   -- T1
   -- A6 [out]
   -- A4 [out]
   -- A5 [out]
   -- T2
   function LibosPortAsyncReply
     (sendShuttle : libos_manifest_h.LibosShuttleId;
      sendPayload : System.Address;
      sendPayloadSize : lwtypes_h.LwU64;
      completedSize : access lwtypes_h.LwU64) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_port.h:176
     with Import => True,
     Convention => C,
     External_Name => "LibosPortAsyncReply";

   -- must be the shuttle we received the message on
   function LibosPortAsyncSend
     (sendShuttle : libos_manifest_h.LibosShuttleId;
      sendPort : libos_manifest_h.LibosPortId;
      sendPayload : System.Address;
      sendPayloadSize : lwtypes_h.LwU64) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_port.h:180
     with Import => True,
     Convention => C,
     External_Name => "LibosPortAsyncSend";

   function LibosPortAsyncRecv
     (recvShuttle : libos_manifest_h.LibosShuttleId;
      recvPort : libos_manifest_h.LibosPortId;
      recvPayload : System.Address;
      recvPayloadCapacity : lwtypes_h.LwU64) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_port.h:183
     with
       Import => True,
       Convention => C,
       External_Name => "LibosPortAsyncRecv",
       Global => null;

   function LibosPortAsyncSendRecv
     (sendShuttle : libos_manifest_h.LibosShuttleId;
      sendPort : libos_manifest_h.LibosPortId;
      sendPayload : System.Address;
      sendPayloadSize : lwtypes_h.LwU64;
      recvShuttle : libos_manifest_h.LibosShuttleId;
      recvPort : libos_manifest_h.LibosPortId;
      recvPayload : System.Address;
      recvPayloadCapacity : lwtypes_h.LwU64) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_port.h:186
     with Import => True,
     Convention => C,
     External_Name => "LibosPortAsyncSendRecv";

   function LibosPortWait
     (waitShuttle : libos_manifest_h.LibosShuttleId;
      completedShuttle : access libos_manifest_h.LibosShuttleId;
      completedSize : access lwtypes_h.LwU64;
      completedRemoteTaskId : access libos_manifest_h.LibosTaskId;
      timeoutNs : lwtypes_h.LwU64) return libos_status_h.LibosStatus  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_port.h:190
     with
       Import => True,
       Convention => C,
       External_Name => "LibosPortWait",
       Global => null;

   -- A5
   -- T2
   -- T1
   -- A6
end libos_port_h;
