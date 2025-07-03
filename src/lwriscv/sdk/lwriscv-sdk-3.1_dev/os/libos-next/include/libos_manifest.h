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
#include "libos-config.h"

/**
 * @brief Name declarations for ports, shuttles, and tasks.
 *
 * Names are used to refer to kernel objects from user-space.
 * A given name may correspond to different physical objects
 * in different tasks.
 *
 * In order to declare a name (or handle) simply include a
 * name declaration:
 *
 * LibosPortNameDecl(portName);
 * LibosTaskNameDecl(taskNameArray[4]);
 * 
 * @note: Names use no memory in the compiled program.
 */
typedef struct __attribute__((__packed__, aligned(1)))  { LwU8 dummy; }  LibosObjectNameT,    * LibosPortName;
typedef struct  __attribute__((__packed__, aligned(1))) { LwU8 dummy; }  LibosShuttleNameT, * LibosShuttleName;
typedef struct  __attribute__((__packed__, aligned(1))) { LwU8 dummy; }  LibosTaskNameT,    * LibosTaskName;

#define LibosObjectNameDecl(n)    __attribute__(( aligned(1),section(".ObjectNames." #n)))    LibosObjectNameT     n;
#define LibosShuttleNameDecl(n) __attribute__(( aligned(1),section(".ShuttleNames." #n))) LibosShuttleNameT  n;
#define LibosTaskNameDecl(n)    __attribute__(( aligned(1),section(".TaskNames." #n)))    LibosTaskNameT     n;

#define LibosPortNameExtern(n)    extern    LibosObjectNameT     n;
#define LibosShuttleNameExtern(n) extern    LibosShuttleNameT  n;
#define LibosTaskNameExtern(n)    extern    LibosTaskNameT     n;

#define LibosPortNameDecl(n)   LibosObjectNameDecl(n)
#define LibosTimerNameDecl(n)  LibosObjectNameDecl(n)

/**
 * @brief Colwert name to integer ID
 *
 * @note: Unless you're storing names passed between tasks
 *        you may skip this section
 *
 * If you need to store a compact repreentation of a name
 * you may use the colwersion functions below to extract
 * an integer id.
 *
 * LibosPortHandle myPort = ID(portInit);
 */

typedef LwU64 LibosPriority;
#define LIBOS_PRIORITY_NORMAL 0x80000000

#define ID(x)  _Generic((x),                        \
  LibosObjectNameT: ((LibosPortHandle)(LwU64)&(x)),       \
  LibosShuttleNameT: ((LibosShuttleHandle)(LwU64)&(x)))

/**
 * @brief Manifest
 *
 * Each LIBOS ELF contains a mainfest of the ports, tasks, and shuttles
 * required before exelwtion may start.
 *
 * For small firmwares the offline manifest compiler will process this
 * manifest and pre-initialize the scheduler tables.  In this scenario,
 * there exists no code at runtime that may create or destroy ports,
 * shuttles, and tasks.
 *
 * For large firmwares, multiple ELFs are stitched together by the loader.
 * The scheduler tables are then initialized at runtime from the manifests.
 * One ELF may grant access to a port declared in another ELF to it's own
 * tasks.  The manifest is simple C structures, so this stitching oclwrs
 * as a natural biproduct of loading the ELF.
 *
 * Think of manifest entries as an offline request to create a resource.
 *
 */

/**
 * @brief Declare a task in the manifest
 *
 * Use this to create a task
 *
 *    LibosManifestTaskInstance _taskWorker = {
 *     .name = &taskWorker,
 *     .pc = &worker,
 *     .sp = &worker_stack[sizeof(worker_stack)/sizeof(*worker_stack)],
 *     .mpuMappings = { },
 *     .waitingPort    = &taskWorkerPort,
 *     .waitingShuttle = &shuttleSyncRecv
 *    };
 *
 * @note: The kernel will run the task named 'taskInit' on startup.
 *
 * @params[in] name
 *    The kernel API will refer to this task with using this name.
 *
 * @params[in] pc
 *    Entry point.
 *
 * @params[in] sp
 *    Initial stack pointer
 *
 * @params[in] tp
 *    The thread pointer for this task.  The register should not be
 *    changed at runtime as the scheduler will not save tp.
 *
 * @params[in] gp
 *    The global pointer for this task.  The register should not be
 *    changed at runtime as the scheduler will not save gp.
 *
 * @params[in] mpuMappings
 *    Provides a list of MPU mapping indices available to this task.
 *    @see LibosBootstrapMmap
 *
 * @params[i] waitingPort
 *    The task will be begin exelwting once a message is received on
 *    this port.
 *
 *    @note: This value must be 0 for the taskInit, and non-zero for all
 *           other values
 *
 * @params[i] waitingShuttle
 *    This should be set to shuttleSyncRecv if waitingPort is specified
 *
 */
struct LibosManifestTaskInstance {
    LibosTaskName        name;
    LibosPriority        priority;
    void               * sp;
    void               * pc;
    void               * tp;
    void               * gp;
    LwU8                 mpuMappings[LIBOS_CONFIG_MPU_INDEX_COUNT];
    LibosShuttleName     waitingShuttle;
    LibosPortName        waitingPort;
};

#define LibosManifestTask(taskName, taskPriority, taskEntry, taskStack, ...)                                   \
  LibosTaskNameDecl(taskName);                                                                          \
   __attribute__(( used, section(".ManifestTasks"))) struct LibosManifestTaskInstance mf_##taskName = { \
    .priority = taskPriority,                                                                           \
    .name = &taskName,                                                                                  \
    .pc = &taskEntry,                                                                                   \
    .sp = &taskStack[sizeof(taskStack)/sizeof(*taskStack)],                                             \
    .mpuMappings = {__VA_ARGS__},                                                                      \
    .waitingShuttle = 0,                                                                               \
    .waitingPort = 0                                                                                   \
  };

#define LibosManifestTaskWaiting(taskName, taskPriority, waitingPort_, taskEntry, taskStack, ...)              \
  LibosTaskNameDecl(taskName);                                                                          \
  __attribute__(( used, section(".ManifestTasks"))) struct LibosManifestTaskInstance mf_##taskName = {  \
    .name = &taskName,                                                                                  \
    .priority = taskPriority,                                                                           \
    .pc = &taskEntry,                                                                                   \
    .sp = &taskStack[sizeof(taskStack)/sizeof(*taskStack)],                                             \
    .mpuMappings = {__VA_ARGS__},                                                                      \
    .waitingShuttle = &shuttleSyncRecv,                                                                \
    .waitingPort = &waitingPort_                                                                        \
  };  


/**
 * @brief Declare a shuttle in the manifest
 *
 * Shuttles are responsible for tracking individual message sends
 * or receives.  They can be used for waiting for a previously
 * submitted transaction to complete, or to cancel an in flight
 * transaction.
 *
 * @note: If you're only using synchronous message sends, you
 *        should use the per-task shuttleSyncSend and shuttleSyncRecv.
 *
 * libosManifestShuttleInstance _taskWorkerSyncSend = { &taskWorker, &shuttleSyncSend };
 *
 * @params[in] name
 *    The shuttle will be bound to this name for a given task.
 *
 * @params[in] task
 *    The task owning this shuttle.  No other task may refer to the shuttle.
 *
 */
struct LibosManifestShuttleInstance {
    LibosTaskName     task;
    LibosShuttleName  name;
};

#define LibosManifestShuttle(taskName, shuttleName)                                             \
    __attribute__(( used, section(".ManifestShuttles")))                                        \
        struct LibosManifestShuttleInstance mf_##taskName##__##shuttleName = {                  \
        .task = &taskName,                                                                      \
        .name = &shuttleName,                                                                   \
    };                                                                                            \

/**
 * @brief Declare a port instance in the manifest
 *
 * Messages are directed at named ports.  The kernel looks up this name
 * in the given task to find the desired port instance.
 *
 * @note: It is convention to name the port grant _[port owner][port name].
 *        The owner is always the task expected to wait on the port.
 *
 * LibosManifestObjectInstance   _taskWorkerPort;
 *
 */
struct LibosManifestObjectInstance {
    LwU8 reserved;
};

#define LibosManifestPort(portName)                                                                             \
    __attribute__(( used, section(".ManifestPorts"))) struct LibosManifestObjectInstance mf_##portName = {};    \
    LibosPortNameDecl(portName); 

#define LibosManifestTimer(timerName)                                                                             \
    __attribute__(( used, section(".ManifestTimers"))) struct LibosManifestObjectInstance mf_##timerName = {};    \
    LibosPortNameDecl(timerName); 

#define LibosManifestLock(lockName)                                                                               \
    __attribute__(( used, section(".ManifestLocks"))) struct LibosManifestObjectInstance mf_##lockName = {};     \
    LibosPortNameDecl(lockName);     

/**
 * @brief Binds a port to a task
 *
 * A given port may be bound to one or more tasks.
 *
 * @note: It is convention to name the port grant _[port owner][port name][target task]
 *
 *   LibosManifestObjectGrant _taskWorkerPortInitGrant = {
 *      .task    = &taskInit,
 *      .access  = grantSend,
 *      .name    = &taskWorkerPort,
 *      .instance= &_taskWorkerPort
 *   };
 *
 * @params[in] task
 *    The name of the task the port will be bound to
 *
 * @params[in] access = grantSend | grantWait | grantAll
 *    Permissions assigned ot the task.
 *
 * @params[in] name
 *    The port will be bound to this name within the specified task
 *
 * @params[in] instance
 *    The instance of the port that we're binding (@see LibosManifestObjectInstance)
 */
struct LibosManifestObjectGrant {
    LibosTaskName                           task;
    LibosPortName                           name;
    struct LibosManifestObjectInstance  *     instance;
    enum { 
        // Ports
        portGrantAll, portGrantWait, portGrantSend,

        // Timers
        timerGrantAll, timerGrantWait, timerGrantSet,

        // Locks
        lockGrantAll
    } access;
};

#define LibosManifestGrantWait(portName, taskName)                \
    __attribute__(( used, section(".ManifestObjectGrants"))) struct LibosManifestObjectGrant mf_##portName##_##taskName = {   \
    .task     = &taskName,                                  \
    .access   = portGrantWait,                                  \
    .name     = &portName,                                  \
    .instance = &mf_##portName                              \
    };

#define LibosManifestGrantSend(portName, taskName)                \
    __attribute__(( used, section(".ManifestObjectGrants"))) struct LibosManifestObjectGrant mf_##portName##_##taskName = {   \
    .task     = &taskName,                                  \
    .access   = portGrantSend,                                  \
    .name     = &portName,                                  \
    .instance = &mf_##portName                              \
    };    

#define LibosManifestGrantAll(portName, taskName)                \
    __attribute__(( used, section(".ManifestObjectGrants"))) struct LibosManifestObjectGrant mf_##portName##_##taskName = {   \
    .task     = &taskName,                                  \
    .access   = portGrantAll,                                  \
    .name     = &portName,                                  \
    .instance = &mf_##portName                              \
    };        

#define LibosManifestGrantTimerWait(timerName, taskName)                \
    __attribute__(( used, section(".ManifestObjectGrants"))) struct LibosManifestObjectGrant mf_##timerName##_##taskName = {   \
    .task     = &taskName,                                  \
    .access   = timerGrantWait,                                  \
    .name     = &timerName,                                  \
    .instance = &mf_##timerName                              \
    };    

#define LibosManifestGrantTimerSet(timerName, taskName)                \
    __attribute__(( used, section(".ManifestObjectGrants"))) struct LibosManifestObjectGrant mf_##timerName##_##taskName = {   \
    .task     = &taskName,                                  \
    .access   = timerGrantSet,                                  \
    .name     = &timerName,                                  \
    .instance = &mf_##timerName                              \
    };    

#define LibosManifestGrantTimerAll(timerName, taskName)                \
    __attribute__(( used, section(".ManifestObjectGrants"))) struct LibosManifestObjectGrant mf_##timerName##_##taskName = {   \
    .task     = &taskName,                                  \
    .access   = timerGrantAll,                                  \
    .name     = &timerName,                                  \
    .instance = &mf_##timerName                              \
    };    

#define LibosManifestGrantLockAll(lockName, taskName)                \
    __attribute__(( used, section(".ManifestObjectGrants"))) struct LibosManifestObjectGrant mf_##lockName##_##taskName = {   \
    .task     = &taskName,                                  \
    .access   = lockGrantAll,                                  \
    .name     = &lockName,                                  \
    .instance = &mf_##lockName                              \
    };    


/**
 * @brief Kernel reports faulting tasks to this port
 *
 * @todo: Document flow.
 */

// The user must define the init task
LibosTaskNameExtern(taskInit);

/**
 * @todo: Document
 */
LibosShuttleNameExtern(shuttleSyncSend)
LibosShuttleNameExtern(shuttleSyncRecv)

/**
 * @todo: Document pseudo shuttle
 */
LibosShuttleNameExtern(shuttleAny);
