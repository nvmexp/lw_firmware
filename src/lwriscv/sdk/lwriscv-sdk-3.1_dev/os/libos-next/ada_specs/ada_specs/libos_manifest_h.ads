pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;
with lwtypes_h;
with System;

package libos_manifest_h is

   --  arg-macro: procedure LibosObjectNameDecl (n)
   --    __attribute__(( aligned(1),section(".ObjectNames"))) LibosObjectNameT n;
   --  arg-macro: procedure LibosShuttleNameDecl (n)
   --    __attribute__(( aligned(1),section(".ShuttleNames"))) LibosShuttleNameT n;
   --  arg-macro: procedure LibosTaskNameDecl (n)
   --    __attribute__(( aligned(1),section(".TaskNames"))) LibosTaskNameT n;
   --  arg-macro: procedure LibosPortNameExtern (n)
   --    extern LibosObjectNameT n;
   --  arg-macro: procedure LibosShuttleNameExtern (n)
   --    extern LibosShuttleNameT n;
   --  arg-macro: procedure LibosTaskNameExtern (n)
   --    extern LibosTaskNameT n;
   --  arg-macro: procedure LibosPortNameDecl (n)
   --    LibosObjectNameDecl(n)
   --  arg-macro: procedure LibosTimerNameDecl (n)
   --    LibosObjectNameDecl(n)
   --  arg-macro: procedure ID (x)
   --    _Generic((x), LibosObjectNameT: ((LibosPortId)(LwU64)and(x)), LibosShuttleNameT: ((LibosShuttleId)(LwU64)and(x)), LibosTaskNameT: ((LibosTaskId)(LwU64)and(x)))
   --  unsupported macro: LibosManifestTask(taskName,taskPriority,taskEntry,taskStack,...) LibosTaskNameDecl(taskName); __attribute__(( used, section(".ManifestTasks"))) struct LibosManifestTaskInstance mf_ ##taskName = { .priority = taskPriority, .name = &taskName, .pc = &taskEntry, .sp = &taskStack[sizeof(taskStack)/sizeof(*taskStack)], .mpuMappings = {__VA_ARGS__}, .waitingShuttle = 0, .waitingPort = 0 };
   --  unsupported macro: LibosManifestTaskWaiting(taskName,taskPriority,waitingPort_,taskEntry,taskStack,...) LibosTaskNameDecl(taskName); __attribute__(( used, section(".ManifestTasks"))) struct LibosManifestTaskInstance mf_ ##taskName = { .name = &taskName, .priority = taskPriority, .pc = &taskEntry, .sp = &taskStack[sizeof(taskStack)/sizeof(*taskStack)], .mpuMappings = {__VA_ARGS__}, .waitingShuttle = &shuttleSyncRecv, .waitingPort = &waitingPort_ };
   --  unsupported macro: LibosManifestShuttle(taskName,shuttleName) __attribute__(( used, section(".ManifestShuttles"))) struct LibosManifestShuttleInstance mf_ ##taskName ##_ ##_shuttleName = { .task = &taskName, .name = &shuttleName, };
   --  unsupported macro: LibosManifestPort(portName) __attribute__(( used, section(".ManifestPorts"))) struct LibosManifestObjectInstance mf_ ##portName = {}; LibosPortNameDecl(portName);
   --  unsupported macro: LibosManifestTimer(timerName) __attribute__(( used, section(".ManifestTimers"))) struct LibosManifestObjectInstance mf_ ##timerName = {}; LibosPortNameDecl(timerName);
   --  unsupported macro: LibosManifestGrantWait(portName,taskName) __attribute__(( used, section(".ManifestObjectGrants"))) struct LibosManifestObjectGrant mf_ ##portName ##_ ##taskName = { .task = &taskName, .access = portGrantWait, .name = &portName, .instance = &mf_ ##portName };
   --  unsupported macro: LibosManifestGrantSend(portName,taskName) __attribute__(( used, section(".ManifestObjectGrants"))) struct LibosManifestObjectGrant mf_ ##portName ##_ ##taskName = { .task = &taskName, .access = portGrantSend, .name = &portName, .instance = &mf_ ##portName };
   --  unsupported macro: LibosManifestGrantAll(portName,taskName) __attribute__(( used, section(".ManifestObjectGrants"))) struct LibosManifestObjectGrant mf_ ##portName ##_ ##taskName = { .task = &taskName, .access = portGrantAll, .name = &portName, .instance = &mf_ ##portName };
   --  unsupported macro: LibosManifestGrantTimerWait(timerName,taskName) __attribute__(( used, section(".ManifestObjectGrants"))) struct LibosManifestObjectGrant mf_ ##timerName ##_ ##taskName = { .task = &taskName, .access = timerGrantWait, .name = &timerName, .instance = &mf_ ##timerName };
   --  unsupported macro: LibosManifestGrantTimerSet(timerName,taskName) __attribute__(( used, section(".ManifestObjectGrants"))) struct LibosManifestObjectGrant mf_ ##timerName ##_ ##taskName = { .task = &taskName, .access = timerGrantSet, .name = &timerName, .instance = &mf_ ##timerName };
   --  unsupported macro: LibosManifestGrantTimerAll(timerName,taskName) __attribute__(( used, section(".ManifestObjectGrants"))) struct LibosManifestObjectGrant mf_ ##timerName ##_ ##taskName = { .task = &taskName, .access = timerGrantAll, .name = &timerName, .instance = &mf_ ##timerName };
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
  -- * @brief Name declarations for ports, shuttles, and tasks.
  -- *
  -- * Names are used to refer to kernel objects from user-space.
  -- * A given name may correspond to different physical objects
  -- * in different tasks.
  -- *
  -- * In order to declare a name (or handle) simply include a
  -- * name declaration:
  -- *
  -- * LibosPortNameDecl(portName);
  -- * LibosTaskNameDecl(taskNameArray[4]);
  -- * 
  -- * @note: Names use no memory in the compiled program.
  --  

   pragma Compile_Time_Warning (True, "probably incorrect record layout");
   type LibosObjectNameT is record
      dummy : aliased lwtypes_h.LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:26
   end record
   with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:26

   -- type LibosPortName is access all ;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:26

   pragma Compile_Time_Warning (True, "probably incorrect record layout");
   type LibosShuttleNameT is record
      dummy : aliased lwtypes_h.LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:27
   end record
   with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:27

   -- type LibosShuttleName is access all ;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:27

   pragma Compile_Time_Warning (True, "probably incorrect record layout");
   type LibosTaskNameT is record
      dummy : aliased lwtypes_h.LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:28
   end record
   with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:28

   -- type LibosTaskName is access all ;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:28

  --*
  -- * @brief Colwert name to integer ID
  -- *
  -- * @note: Unless you're storing names passed between tasks
  -- *        you may skip this section
  -- *
  -- * If you need to store a compact repreentation of a name
  -- * you may use the colwersion functions below to extract
  -- * an integer id.
  -- *
  -- * LibosTaskId myTask = ID(taskInit);
  --  

   subtype LibosTaskId is lwtypes_h.LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:55

   subtype LibosPortId is lwtypes_h.LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:56

   subtype LibosPortInternalIndex is lwtypes_h.LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:57

   subtype LibosShuttleId is lwtypes_h.LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:58

   subtype LibosPriority is lwtypes_h.LwU64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:68

  --*
  -- * @brief Manifest
  -- *
  -- * Each LIBOS ELF contains a mainfest of the ports, tasks, and shuttles
  -- * required before exelwtion may start.
  -- *
  -- * For small firmwares the offline manifest compiler will process this
  -- * manifest and pre-initialize the scheduler tables.  In this scenario,
  -- * there exists no code at runtime that may create or destroy ports,
  -- * shuttles, and tasks.
  -- *
  -- * For large firmwares, multiple ELFs are stitched together by the loader.
  -- * The scheduler tables are then initialized at runtime from the manifests.
  -- * One ELF may grant access to a port declared in another ELF to it's own
  -- * tasks.  The manifest is simple C structures, so this stitching oclwrs
  -- * as a natural biproduct of loading the ELF.
  -- *
  -- * Think of manifest entries as an offline request to create a resource.
  -- *
  --  

  --*
  -- * @brief Declare a task in the manifest
  -- *
  -- * Use this to create a task
  -- *
  -- *    LibosManifestTaskInstance _taskWorker = {
  -- *     .name = &taskWorker,
  -- *     .pc = &worker,
  -- *     .sp = &worker_stack[sizeof(worker_stack)/sizeof(*worker_stack)],
  -- *     .mpuMappings = { },
  -- *     .waitingPort    = &taskWorkerPort,
  -- *     .waitingShuttle = &shuttleSyncRecv
  -- *    };
  -- *
  -- * @note: The kernel will run the task named 'taskInit' on startup.
  -- *
  -- * @params[in] name
  -- *    The kernel API will refer to this task with using this name.
  -- *
  -- * @params[in] pc
  -- *    Entry point.
  -- *
  -- * @params[in] sp
  -- *    Initial stack pointer
  -- *
  -- * @params[in] tp
  -- *    The thread pointer for this task.  The register should not be
  -- *    changed at runtime as the scheduler will not save tp.
  -- *
  -- * @params[in] gp
  -- *    The global pointer for this task.  The register should not be
  -- *    changed at runtime as the scheduler will not save gp.
  -- *
  -- * @params[in] mpuMappings
  -- *    Provides a list of MPU mapping indices available to this task.
  -- *    @see LibosBootstrapMmap
  -- *
  -- * @params[i] waitingPort
  -- *    The task will be begin exelwting once a message is received on
  -- *    this port.
  -- *
  -- *    @note: This value must be 0 for the taskInit, and non-zero for all
  -- *           other values
  -- *
  -- * @params[i] waitingShuttle
  -- *    This should be set to shuttleSyncRecv if waitingPort is specified
  -- *
  --  

   type anon773_array775 is array (0 .. 127) of aliased lwtypes_h.LwU8;
   -- type LibosManifestTaskInstance is record
   --    name : LibosTaskName;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:145
   --    priority : aliased LibosPriority;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:146
   --    sp : System.Address;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:147
   --    pc : System.Address;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:148
   --    tp : System.Address;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:149
   --    gp : System.Address;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:150
   --    mpuMappings : aliased anon773_array775;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:151
   --    waitingShuttle : LibosShuttleName;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:152
   --    waitingPort : LibosPortName;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:153
   -- end record
   -- with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:144

  --*
  -- * @brief Declare a shuttle in the manifest
  -- *
  -- * Shuttles are responsible for tracking individual message sends
  -- * or receives.  They can be used for waiting for a previously
  -- * submitted transaction to complete, or to cancel an in flight
  -- * transaction.
  -- *
  -- * @note: If you're only using synchronous message sends, you
  -- *        should use the per-task shuttleSyncSend and shuttleSyncRecv.
  -- *
  -- * libosManifestShuttleInstance _taskWorkerSyncSend = { &taskWorker, &shuttleSyncSend };
  -- *
  -- * @params[in] name
  -- *    The shuttle will be bound to this name for a given task.
  -- *
  -- * @params[in] task
  -- *    The task owning this shuttle.  No other task may refer to the shuttle.
  -- *
  --  

   -- type LibosManifestShuttleInstance is record
   --    c_task : LibosTaskName;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:202
   --    name : LibosShuttleName;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:203
   -- end record
   -- with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:201

  --*
  -- * @brief Declare a port instance in the manifest
  -- *
  -- * Messages are directed at named ports.  The kernel looks up this name
  -- * in the given task to find the desired port instance.
  -- *
  -- * @note: It is convention to name the port grant _[port owner][port name].
  -- *        The owner is always the task expected to wait on the port.
  -- *
  -- * LibosManifestObjectInstance   _taskWorkerPort;
  -- *
  --  

   type LibosManifestObjectInstance is record
      reserved : aliased lwtypes_h.LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:226
   end record
   with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:225

  --*
  -- * @brief Binds a port to a task
  -- *
  -- * A given port may be bound to one or more tasks.
  -- *
  -- * @note: It is convention to name the port grant _[port owner][port name][target task]
  -- *
  -- *   LibosManifestObjectGrant _taskWorkerPortInitGrant = {
  -- *      .task    = &taskInit,
  -- *      .access  = grantSend,
  -- *      .name    = &taskWorkerPort,
  -- *      .instance= &_taskWorkerPort
  -- *   };
  -- *
  -- * @params[in] task
  -- *    The name of the task the port will be bound to
  -- *
  -- * @params[in] access = grantSend | grantWait | grantAll
  -- *    Permissions assigned ot the task.
  -- *
  -- * @params[in] name
  -- *    The port will be bound to this name within the specified task
  -- *
  -- * @params[in] instance
  -- *    The instance of the port that we're binding (@see LibosManifestObjectInstance)
  --  

  -- Ports
  -- Timers
   type anon779_enum781 is 
     (portGrantAll,
      portGrantWait,
      portGrantSend,
      timerGrantAll,
      timerGrantWait,
      timerGrantSet)
   with Convention => C;
   -- type LibosManifestObjectGrant is record
   --    c_task : LibosTaskName;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:264
   --    name : LibosPortName;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:265
   --    instance : access LibosManifestObjectInstance;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:266
   --    c_access : anon779_enum781;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:273
   -- end record
   -- with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:263

  --*
  -- * @brief Kernel reports faulting tasks to this port
  -- *
  -- * @todo: Document flow.
  --  

  -- The user must define the init task
   taskInit : aliased LibosTaskNameT  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:337
   with Import => True, 
        Convention => C, 
        External_Name => "taskInit";

  --*
  -- * @todo: Document
  --  

   shuttleSyncSend : aliased LibosShuttleNameT  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:342
   with Import => True, 
        Convention => C, 
        External_Name => "shuttleSyncSend";

   shuttleSyncRecv : aliased LibosShuttleNameT  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:343
   with Import => True, 
        Convention => C, 
        External_Name => "shuttleSyncRecv";

  --*
  -- * @todo: Document pseudo shuttle
  --  

   shuttleAny : aliased LibosShuttleNameT  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_manifest.h:348
   with Import => True, 
        Convention => C, 
        External_Name => "shuttleAny";

end libos_manifest_h;
