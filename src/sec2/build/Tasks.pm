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
my $tasksRaw = [

    #
    # The IDLE task is created by the RTOS and all it's parameters were fixed by
    # the design.  Since it uses way less stack than allocated 256 bytes support
    # for per-build stack size was added.  Desired stack size is exposed to RTOS
    # through @ref vApplicationIdleTaskStackSize().
    #
    PMUTASK_IDLE =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN   => "vIdleTask",
            STACK_SIZE => 256,
        ],
    },

    SEC2TASK_CMDMGMT =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_cmdmgmt",
            PRIORITY             => 2,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => 512,
            MAX_OVERLAYS_IMEM_LS => 1,
            MAX_OVERLAYS_IMEM_HS => 1,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(cmdmgmt)",
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackCmdmgmt)",
            TCB                  => "OsTaskCmdMgmt",
            PRIV_LEVEL_EXT       => 0,
        ],

        OVERLAY_COMBINATION => [
            [ ALL, ]     => [
                [ '.imem_cleanupHs', '.imem_cmdmgmt', '.imem_libCommonHs', ],
            ],
        ],
    },

    SEC2TASK_CHNMGMT =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_chnmgmt",
            PRIORITY             => 3,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => 512,
            MAX_OVERLAYS_IMEM_LS => 1,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(chnmgmt)",
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackChnmgmt)",
            TCB                  => "OsTaskChnMgmt",
            PRIV_LEVEL_EXT       => 0,
            PRIV_LEVEL_CSB       => 0,
        ],

        OVERLAY_COMBINATION => [
            [ ALL, ]     => [
                [ '.imem_chnmgmt', ],
            ],
        ],
    },

    # Worker Thread
    SEC2TASK_WKRTHD =>
    {
        OSWKRTHD_DEFINE => [
             COUNT          => 1,
             ENTRY_FN       => "task_wkrthd",
             PRIORITY       => 1,
             OVERLAY_IMEM   => "OVL_INDEX_IMEM(wkrthd)",
             TCB_ARRAY      => pOsTaskWkrThd,
             PRIV_LEVEL_EXT => 0,
             PRIV_LEVEL_CSB => 0,
        ],

        JOBS => [
            OSJOB_DEFINE => [
                ENTRY_FN             => "hs_switch_entry",
                STACK_SIZE           => 512,
                MAX_OVERLAYS_IMEM_LS => 2,
                MAX_OVERLAYS_IMEM_HS => 2,
                OVERLAY              => "OVL_INDEX_IMEM(testhsswitch)",
            ],

            OSJOB_DEFINE => [
                ENTRY_FN             => "fakeidle_test_entry",
                STACK_SIZE           => 512,
                MAX_OVERLAYS_IMEM_LS => 3,
                OVERLAY              => "OVL_INDEX_IMEM(testfakeidle)",
            ],

            OSJOB_DEFINE => [
                ENTRY_FN             => "rttimer_test_entry",
                STACK_SIZE           => 512,
                MAX_OVERLAYS_IMEM_LS => 3,
                OVERLAY              => "OVL_INDEX_IMEM(testrttimer)",
            ],
        ],

        OVERLAY_COMBINATION => [
            [ GP10X_thru_AMPERE]     => [
                [ '.imem_wkrthd', '.imem_testhsswitch', '.imem_testHs', '.imem_libCommonHs', ],
                [ '.imem_wkrthd', '.imem_testrttimer?', '.imem_rttimer?', ],
                [ '.imem_wkrthd', '.imem_testfakeidle?', ],
                [ '.imem_wkrthd', '.imem_testfakeidle', '.imem_chnmgmt', ],
            ],
            #TODO - Remove WKRTHD and FAKEIDLE overlay before QS, Bug 200779459 
            [ AD10X]     => [
                [ '.imem_wkrthd', '.imem_testfakeidle', '.imem_chnmgmt', ],
            ],
        ],
    },

    SEC2TASK_HDCPMC =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_hdcpmc",
            PRIORITY             => 1,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => 4096,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(hdcpmc)",
            MAX_OVERLAYS_IMEM_LS => 0, # Set this value appropriately while enabling this task
            MAX_OVERLAYS_IMEM_HS => 0, # Set this value appropriately while enabling this task
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackHdcpmc)",
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(hdcpmc)",
            MAX_OVERLAYS_DMEM    => 0, # Set this value appropriately while enabling this task
            TCB                  => "OsTaskHdcpmc",
        ],

        # Set the overlay combination appropriately while enabling this task. Earlier combination was deleted in r384 file verion #9
        OVERLAY_COMBINATION => [
        ],
    },

    SEC2TASK_GFE =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_gfe",
            PRIORITY             => 1,
            PRIV_LEVEL_EXT       => 0,
            PRIV_LEVEL_CSB       => 0,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => 4096,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(gfe)",
            MAX_OVERLAYS_IMEM_LS => 3,
            MAX_OVERLAYS_IMEM_HS => 2,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackGfe)",
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(gfe)",
            MAX_OVERLAYS_DMEM    => 2,
            TCB                  => "OsTaskGfe",
        ],

        OVERLAY_COMBINATION => [
            [ GP10X_and_later, ] => [
                [ '.imem_gfe?', '.imem_libSha?' , '.imem_gfeCryptHS?', '.imem_libCommonHs?', '.imem_libScpCryptHs?' ],
                [ '.imem_gfe?', '.imem_libSha?' , '.imem_libBigInt?', ],
                # Uncomment this line when build scripts can parse it
                # [GP10X_and_later, ] => [ '.dmem_gfe', ],
            ],
        ],
    },

    SEC2TASK_LWSR =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_lwsr",
            PRIORITY             => 1,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => 1024,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(lwsr)",
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackLwsr)",
            MAX_OVERLAYS_IMEM_LS => 2,
            MAX_OVERLAYS_IMEM_HS => 1,
            TCB                  => "OsTaskLwsr",
        ],

        OVERLAY_COMBINATION => [
            [ GP10X_and_later, ] => [
                [ '.imem_lwsr?', '.imem_libSha?', '.imem_lwsrcrypt?', '.imem_libCommonHs?', '.imem_libScpCryptHs?' ],
            ],
        ],
    },

    SEC2TASK_HWV =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_hwv",
            PRIORITY             => 1,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => 1024,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(hwv)",
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackHwv)",
            MAX_OVERLAYS_IMEM_LS => 2,
            TASK_IMEM_PAGING     => LW_TRUE,
            TCB                  => "OsTaskHwv",
        ],

        OVERLAY_COMBINATION => [
            [ TU10X_and_later, ] => [
                [ '.imem_hwv?', '.imem_libGptmr' ],
            ],
        ],
    },

    SEC2TASK_PR =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN                => "task_pr",
            PRIORITY                => 1,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE              => 4096,
            OVERLAY_IMEM            => "OVL_INDEX_IMEM(pr)",
            MAX_OVERLAYS_IMEM_LS    => 8,
            MAX_OVERLAYS_IMEM_HS    => 5,
            OVERLAY_STACK           => "OVL_INDEX_DMEM(stackPr)",
            OVERLAY_DMEM            => "OVL_INDEX_DMEM(pr)",
            MAX_OVERLAYS_DMEM       => 2,
            TASK_IMEM_PAGING        => LW_TRUE,
            TCB                     => "OsTaskPr",
            PRIV_LEVEL_EXT          => 3,
            PRIV_LEVEL_CSB          => 3,
        ],

        OVERLAY_COMBINATION => [
            [ GP10X_and_later, ] => [
            ],
        ],
    },

    SEC2TASK_VPR =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_vpr",
            PRIORITY             => 1,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => 1024,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(vpr)",
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackVpr)",
            MAX_OVERLAYS_IMEM_LS => 1,
            MAX_OVERLAYS_IMEM_HS => 2,
            TASK_IMEM_PAGING     => LW_TRUE,
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(vpr)",
            MAX_OVERLAYS_DMEM    => 2,
            TCB                  => "OsTaskVpr",
            PRIV_LEVEL_EXT       => 3,
            PRIV_LEVEL_CSB       => 3,
        ],

        OVERLAY_COMBINATION => [
            [ GP10X_and_later, ] => [
                # GV100 TODO (nkuo) - need to remove the ? after libVprPolicyHs after it is compiled
                [ '.imem_vpr?', '.imem_vprHs?' , '.imem_libCommonHs?', '.imem_libVprPolicyHs?', ],
            ],
        ],
    },

    SEC2TASK_ACR =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_acr",
            PRIORITY             => 1,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => 1024,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(acrLs)",
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackAcr)",
            TCB                  => "OsTaskAcr",
            MAX_OVERLAYS_IMEM_LS => 1,
            MAX_OVERLAYS_IMEM_HS => 3,
            TASK_IMEM_PAGING     => LW_TRUE,
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(acr)",
            MAX_OVERLAYS_DMEM    => 2,
            PRIV_LEVEL_EXT       => 3,
            PRIV_LEVEL_CSB       => 3,
        ],

        OVERLAY_COMBINATION     => [
            [ GP10X_and_later, ]  => [
                [ '.imem_acrLs',],
            ],

            [ GA100_and_later, ]  => [
                ['.imem_acr', '.imem_libCommonHs', '.imem_libDmaHs', ],
            ],
       ],
    },

    SEC2TASK_APM =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_apm",
            PRIORITY             => 1,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => 1024,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(apm)",
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackApm)",
            TASK_IMEM_PAGING     => LW_TRUE,
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(apm)",
            MAX_OVERLAYS_DMEM    => 3,
            TCB                  => "OsTaskApm",
        ],

        OVERLAY_COMBINATION     => [
            [ GA100, GA10X, ]  => [
                ['.imem_apm', 'imem_apmSpdmShared?', '.imem_libShahw?'],
            ],
       ],
    },

    SEC2TASK_SPDM =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_spdm",
            PRIORITY             => 1,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => 8448,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(spdm)",
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackSpdm)",
            TASK_IMEM_PAGING     => LW_TRUE,
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(spdm)",
            MAX_OVERLAYS_DMEM    => 3,
            TCB                  => "OsTaskSpdm",
        ],

        OVERLAY_COMBINATION     => [
            [ GA100, GA10X, ]  => [
                ['.imem_spdm', 'imem_apmSpdmShared?', '.imem_libShahw?', '.imem_apm?', ],
            ],
       ],
    },

];

# return the raw data of Tasks definition
return $tasksRaw;
