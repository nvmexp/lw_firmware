#************************ BEGIN COPYRIGHT NOTICE ***************************#
#                                                                           #
#          Copyright (c) LWPU Corporation.  All rights reserved.          #
#                                                                           #
# All information contained herein is proprietary and confidential to       #
# LWPU Corporation.  Any use, reproduction, or disclosure without the     #
# written permission of LWPU Corporation is prohibited.                   #
#                                                                           #
#************************** END COPYRIGHT NOTICE ***************************#
#
# Tasks:
#     Each task that runs in the SEC2's RTOS is defined in this file.
#     This has the advantage of providing a central location to define the
#     various attributes of the task (name, stack-size, etc ...).
#     scripts will generate separate defines for each attribute of each
#     task.  These defines can later be used by the application for task
#     creation, priority manipulation, etc. Each task should include the
#     following section:
#
#     <code>
#     TASK_NAME =>
#     {
#         OSTASK_DEFINE => [
#             ENTRY_FN             => "taskEntryFunc"                 ,
#             PRIORITY             => 3                               ,
#             STACK_SIZE           => 384                             ,
#             TCB                  => "OsTaskCmdMgmt"                 ,
#             MAX_OVERLAYS_IMEM_LS => 0                               ,
#             MAX_OVERLAYS_IMEM_HS => 0                               ,
#             OVERLAY_STACK        => "OVL_INDEX_DMEM(stackCmdmgmt)"  ,
#             OVERLAY_IMEM         => "OVL_INDEX_IMEM(cmdmgmt)"       ,
#             MAX_OVERLAYS_DMEM    => 0                               ,
#             OVERLAY_DMEM         => "OVL_INDEX_DMEM(cmdmgmt)"       ,
#             PRIV_LEVEL_EXT       => 0                               ,
#             PRIV_LEVEL_CSB       => 0                               ,
#         ]
#         OVERLAY_COMBINATION => [ ],
#     },
#     </code>
#
# Task Attributes:
#     Unless otherwise specified, all attributes that follow are required:
#
#     ENTRY_FN
#         The entry-point or entry-function of the task. Typically named with
#         the convention taskX_taskname where X is either the overlay number
#         where the task's code lives or 'R' if the task is resident.
#
#     PRIORITY
#         Number priority of the task. Must be in the range of 1 to 7
#         (numerically ascending with priority).
#
#     STACK_SIZE
#         The number of bytes to allocate for the task's stack. Each task has a
#         unique stack.  The size of the stack may vary based on the complexity
#         of the task. Stacks are dynamically allocated by the task creation
#         function (@ref xTaskCreate).  For obvious reasons it is best to avoid
#         over-allocating when chosing the size. A good rule-of-thumb is to
#         allocate based on a high-stack mark of 90% utilization. This leaves a
#         little margin for error and some growing room. This value must always
#         be an exact multipe of four.
#
#     TCB
#         The name of the symbol that holds the task's TCB pointer after the
#         task is created.
#
#     MAX_OVERLAYS_IMEM_LS
#         Defines the number of IMEM overlays a task can have attached to it at
#         any moment of time. Defaults to 1 when unspecified.
#
#     MAX_OVERLAYS_IMEM_HS
#         MAX_OVERLAYS_IMEM_HS defines the max number of HS overlays that
#         a task can have attached to it at any moment of time.
#
#     OVERLAY_IMEM
#         (optional) If the task is an overlay task, this is the name of the
#         overlay that holds the primary task's code (event handling loop).
#
#     MAX_OVERLAYS_DMEM
#         Defines the number of DMEM overlays a task can have attached to it at
#         any moment of time. Defaults to 0 when unspecified.
#
#     OVERLAY_STACK
#         (optional) If the task is an overlay task, this is the name of the
#         overlay that holds the task's stack.
#
#     OVERLAY_DMEM
#         (optional) If the task is an overlay task, this is the name of the
#         overlay that holds the primary task's data.
#
#     PRIV_LEVEL_EXT
#     PRIV_LEVEL_CSB
#         Defines the two orthogonal privilege level the falcon should be in
#         before scheduling this task. It will be part of task's private TCB.
#         PRIV_LEVEL_EXT controls access to BAR0 and PRIV_LEVEL_CSB
#         controls access to CSB. Access to any register in the chip can be
#         restricted based on the "level" of requestor.
#            LEVEL - 0 (CPU)
#            LEVEL - 1 (UNUSED)
#            LEVEL - 2 (LS-Light Secure)
#            LEVEL - 3 (HS-Heavy Secure)
#         Falcon at any assigned level can be downgraded and brought back to assigned
#         level, but can not be upgraded higher than assigned level. For example,
#         valid privilege level of any task in LS falcon is 0-2. Defaults to 2
#         when unspecified.
#
# OVERLAY_COMBINATION:
#    Defines the list of Overlays required by Falcon tasks for exelwtion
#    For each task, all possible combination of overlays required are to be listed.
#    If any overlay combination is unique to a chip, the chip-family that the
#    chip belongs to is to be used.
#    Likewise an overlay list can also be defined for a chip-generation.
#
# Worker Threads:
#     [TODO: ssapre. Insert a wiki link here that describes worker threads]
#     Each worker thread that runs in the SEC2's RTOS is defined in this file.
#     However, instead of defining each worker thread separately, we specify all
#     common attributes together and auto-generating that information to a header.
#     This has the advantage of providing a central location to define the
#     various attributes of the worker threads. Certain attributes of the worker
#     thread (like stack size, max overlays, overlay combination) will come from
#     OSJOB defines. As a simplistic first step, each worker thread will use the
#     maximum of all stack sizes assigned to each job and have a max overlay
#     combination corresponding to the max of all worker threads.
#     flcn-rtos-script.pl will generate defines for each attribute of the worker
#     threads. These defines can later be used by for task creation, priority
#     manipulation, etc. The worker thread section should include the following
#     section:
#
#     <code>
#     WKTHD_NAME_PREFIX =>
#     {
#         OSWKRTHD_DEFINE => [
#             COUNT         => 4                        ,
#             ENTRY_FN      => "wkrthdEntryFunc"        ,
#             PRIORITY      => 1                        ,
#             OVERLAY_IMEM  => "OVL_INDEX_IMEM(wkrthd)" ,
#         ],
#         JOBS => [
#             OSJOB_DEFINE => [
#                ENTRY_FN             => "jobEntryFunc"        ,
#                STACK_SIZE           => 512                   ,
#                MAX_OVERLAYS_IMEM_LS => 1                     ,
#                MAX_OVERLAYS_IMEM_HS => 1                     ,
#                MAX_OVERLAYS_DMEM    => 0                     ,
#                OVERLAY              => "OVL_INDEX_IMEM(dma)" ,
#             ],
#         ],
#     },
#     </code>
#
# Worker Thread Attributes:
#     Unless otherwise specified, all attributes that follow are required:
#
#     COUNT
#         The number of worker threads desired. For now, all worker threads are
#         capable of doing all defined jobs. Nothing fancy.
#
#     ENTRY_FN
#         The common entry function of all worker threads.
#
#     PRIORITY
#         The default priority of all worker threads. Each job can specify the
#         priority at which it wants to run and the worker thread chosen to run
#         this instance of the job will inherit that priority before exelwting
#         it. If some portion of a job can run at a different (elevated or
#         lower) priority, it can change the priority of the worker thread
#         dynamically. However, each worker thread will return to its default
#         priority once it has finished exelwting any job.
#
#     OVERLAY
#         The common overlay of all worker threads. The overlays needed by each
#         job will be specified by the job and the worker thread is responsible
#         for attaching all required overlays before performing the job. It is
#         also responsible for detaching all required overlays after finishing
#         exelwtion of the job. Each job can attach overlays intermediately if
#         it wants to, but is responsible for detaching the overlays before it
#         finishes.
#
#     Job Attributes:
#           ENTRY_FN
#               The entry function of this job.
#
#           PRIORITY
#               The priority the job wants to run at. The worker thread will
#               change its priority to this level before exelwting the job.
#               It will return to its default priority once it finishes
#               exelwting the job.
#
#           STACK_SIZE
#               The stack size required by this job. The worker thread creation
#               will take into account the stack size of all jobs in that pool
#               and use the maximum of all stack sizes.
#
#           MAX_OVERLAYS_IMEM_LS
#               The maximum number of LS IMEM overlays that this job can attach to
#               the worker thread exelwting it. The worker thread creation will
#               take into account the max overlays of all jobs in that pool and
#               use the maximum of all (MAX_OVERLAYS_IMEM_LS + 1), since the worker
#               thread's entry function will be part of the worker thread's
#               default overlay (an additional overlay over and above the job
#               overlays)
#
#           MAX_OVERLAYS_IMEM_HS
#               Defines the maximum number of HS IMEM overlays that this job
#               can attach to the worker thread exelwting it.
#               The worker thread creation will take into account the max overlays of
#               all jobs in that pool and use the maximum of all (MAX_OVERLAYS_IMEM_HS).
#
#           MAX_OVERLAYS_DMEM
#               The maximum number of DMEM overlays that this job can attach to
#               the worker thread exelwting it. The worker thread creation will
#               take into account the max overlays of all jobs in that pool and
#               use the maximum of all (MAX_OVERLAYS_DMEM + 1), since the worker
#               thread's entry function will be part of the worker thread's
#               default overlay (an additional overlay over and above the job
#               overlays)
#
#           OVERLAY
#               The default overlay of the job. The worker thread will attach
#               this overlay before jumping into the entry function of the job.
#               All other overlays required by the job can be dynamically
#               attached and detached by the job. The job is responsible for
#               making sure that all attached overlays are appropriately
#               detached before finishing its current exelwtion.
#
#           TASK_IMEM_PAGING
#               A boolean attribute to indicate whether a given task supports
#               on-demand IMEM paging or not. The default behavior for IMEM_MISS is
#               to HALT; but when this attribute is set, RTOS will try to load the
#               corresponding code into IMEM and restart the exelwtion.
#


# fbfalcon does not use these task, this placeholder is only here for the script
# to generate linker scripts

my $tasksRaw = [
];

# return the raw data of Tasks definition
return $tasksRaw;
