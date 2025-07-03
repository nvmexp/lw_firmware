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
#     Each task that runs in the PMU's RTOS is defined in this file.
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
#             ENTRY_FN             => "taskEntryFunc"                ,
#             PRIORITY             => 3                              ,
#             STACK_SIZE           => 384                            ,
#             WATCHDOG_TIMEOUT_US  => 0                              ,
#             WATCHDOG_TRACKING_ENABLED => 0,                        ,
#             TCB                  => "OsTaskCmdMgmt"                ,
#             MAX_OVERLAYS_IMEM_LS => 0                              ,
#             OVERLAY_IMEM         => "OVL_INDEX_IMEM(cmdmgmt)"      ,
#             MAX_OVERLAYS_DMEM    => 0                              ,
#             OVERLAY_STACK        => "OVL_INDEX_DMEM(stackCmdmgmt)" ,
#             OVERLAY_DMEM         => "OVL_INDEX_DMEM(cmdmgmt)"      ,
#             PRIV_LEVEL_EXT       => 1                              ,
#             PRIV_LEVEL_CSB       => 1                              ,
#             USING_FPU            => "LW_FALSE"                     ,
#         ]
#         OVERLAY_COMBINATION => [ ],
#     },
#     </code>
#
# Task Attributes:
#     Unless otherwise specified, all attributes that follow are required:
#
#     TASK_NAME
#         The name of the task as known to the RTOS.  This is an ascii string
#         that is physically stored in the task's TCB.  It is limited to eight
#         characters including the NULL character. If not defined, the name of
#         the task is assumed
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
#     WATCHDOG_TIMEOUT_US
#         The amount of time that this task can spend completing an item before
#         the watchdog task stops petting the timer and raises an error.
#
#     WATCHDOG_TRACKING_ENABLED
#         Boolean indicating whether or not this task should be tracked by the
#         Watchdog Task.
#
#     TCB
#         The name of the symbol that holds the task's TCB pointer after the
#         task is created.
#
#     MAX_OVERLAYS_IMEM_LS
#         Defines the number of LS IMEM overlays a task can have attached to it at
#         any moment of time. Defaults to 1 when unspecified.
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
#     USING_FPU
#         (optional) If the task uses floating point operations.
#
# OVERLAY_COMBINATION:
#    Defines the list of Overlays required by Falcon tasks for exelwtion
#    For each task, all possible combination of overlays required are to be listed.
#    If any overlay combination is unique to a profile, the profile pattern that
#    the profile matches should be listed.

#
# PMU TASK PRIORITIES:
#
# @section _task_priorities PMU Task Priority Considerations
#
#  - The deepidle task priority can normally be low.  However, it must be
#    raised to just below the pcm task when it is polling the exit timer.
#    Otherwise, the deepidle task may be delayed for an indeterminate time
#    while another task runs, missing the exit timeout.  If that happens,
#    deepidle would exit after the end of the vblank and potentially hang
#    the system.
#
#  - The sequencer task must be the highest priority non-resident task aside
#    from the raised deepidle priority.  This is required since it has the
#    potential to stop the frame-buffer.  Stopping the frame-buffer will
#    prevent the PMU from context-switching if the RTOS must load the code for
#    the task being switched to.  Other tasks may run at a higher-priority,
#    but such tasks must be resident in IMEM at all times.
#
#  - The event-based PCM task's priority should be high enough that, while
#    running WAR routines, the active PCM task does not get interrupted.
#    However, it is unlikely that the sequencer task or deepidle task are
#    running at the same time as the event-based PCM task is doing its WAR
#    work as it should be called very early on, we bump down the its priority
#    just below that of the resident PCM task, the sequencer task, and the
#    deepidle high pri task.
#
#  - The disp task contain smart dimmer feature which should be entirely
#    exelwted within VBLANK in order to avoid visible corruptions.  The initial
#    priority of this task is set just below sequencer so once ISR queues
#    VBLANK event for SD it will not get delayed by other low priority tasks.
#    Once disp task receives VBLANK event it's priority is additionally
#    increased so it will not get preempted and will fit into available VBLANK
#    time.
#
# @endsection
#

my $tasksRaw = [

    #
    # Legacy IDLE task entry for Falcon.
    # On Falcon, IDLE task is created by the RTOS, with its parameters fixed by
    # the design. Since it uses way less stack than allocated 256 bytes support
    # for per-build stack size was added. Desired stack size is exposed to RTOS
    # through @ref vApplicationIdleTaskStackSize().
    #
    PMUTASK_IDLE =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN    =>  "vIdleTask",
            STACK_SIZE  =>  [
                                256 => DEFAULT,
                                264 => [ TU102_and_later, ],
                            ],
        ],
    },

    #
    # New _IDLE task entry for RISCV.
    # On RISCV-enabled chips we want to be able to create IDLE task ourselves,
    # using our own entry-point that runs in user-mode.
    #
    PMUTASK__IDLE =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_idle"        ,
            PRIORITY             => 0                  ,
            STACK_SIZE           => 256                ,
            OVERLAY_IMEM         => "OVL_INDEX_ILWALID",
            OVERLAY_DMEM         => "OVL_INDEX_OS_HEAP",
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackIdle)",
            TCB                  => "OsTaskIdle"       ,
        ],
    },

    PMUTASK_CMDMGMT =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_cmdmgmt"       ,
            PRIORITY             => 3                    ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => [
                                       512 => DEFAULT,
                                       768 => [ GP102_and_later, ],
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 1,
            MAX_OVERLAYS_IMEM_LS => 4                    ,
            MAX_OVERLAYS_DMEM    => 2                    ,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(cmdmgmt)" ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackCmdmgmt)" ,
            TCB                  => "OsTaskCmdMgmt"      ,
            PRIV_LEVEL_CSB       => [
                                      2 => DEFAULT,
                                      0 => [ PASCAL_and_later, -GP100, ],
                                    ],
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ 'pmu-*', ],
                                    ],
        ],

        OVERLAY_COMBINATION => [
            [ 'pmu-*', ] => [
                [ '.imem_cmdmgmt', ],
                [ '.imem_cmdmgmt', '.imem_cmdmgmtMisc', ],
                [ '.imem_cmdmgmt', '.imem_cmdmgmtRpc?', ],
            ],
            [ 'pmu-*', '-pmu-gv11b' ] => [
                [ '.imem_cmdmgmt', '.imem_cmdmgmtMisc', '.imem_gid', '.imem_sha1', ],
            ],
            [ 'pmu-gp10x...', '-pmu-gv11b', ] => [
                [ '.imem_cmdmgmt', '.imem_cmdmgmtRpc?', '.imem_libPrivSec?', ],
            ],
            [ 'pmu-gp100', 'pmu-gv10x', ] => [
                [ '.imem_cmdmgmt', '.imem_cmdmgmtRpc?', '.imem_libLwlink?', ],
            ],
            [ 'pmu-ga10x_riscv', ] => [
                # Watchdog
                [ '.imem_cmdmgmt', '.imem_libVgpu?', ],
            ],
            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-gp100...', ] => [
            #     [ '.dmem_stackCmdmgmt', '.dmem_vbios', ],
            # ],
        ],
    },

    PMUTASK_GCX =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_gcx"       ,
            PRIORITY             => 1                ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => [
                                    512 => DEFAULT,
                                    576 => [ MAXWELL, ],
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(gcx)" ,
            TCB                  => "OsTaskGCX"      ,
            MAX_OVERLAYS_IMEM_LS => 4                ,
        ],

        OVERLAY_COMBINATION => [
            [ 'pmu-*', '-pmu-gp100', '-pmu-gv10*', ] => [
                [ '.imem_gcx?', '.imem_clkLibCntr?', '.imem_clkLibCntrSwSync?', ],
            ],
            [ 'pmu-gm*', ] => [
                [ '.imem_gcx', '.imem_libDi?', '.imem_libSyncGpio?', '.imem_libAp?', ],
            ],

            #
            # Added to bypass errors in automated rule checker based on the static code analysis. Fix ASAP !!!
            #
            [ 'pmu-gm*', ] => [
                [ '.imem_gcx', '.imem_libAp?', '.imem_libDi', '.imem_lpwr'],
            ],
            [ 'pmu-gm*', ] => [
                [ '.imem_gcx', '.imem_libDi', '.imem_libPsi'],
                [ '.imem_gcx', '.imem_libDi', '.imem_libMs'],
            ],
        ],
    },

    PMUTASK_LPWR =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_lpwr"       ,
            PRIORITY             => 2               ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE   => [
                               558 => DEFAULT,
                               416 => [ GP100 ],
                               768 => [ GP102_thru_VOLTA, ],
                               1024 => [ TURING_and_later, ], # TODO: move this to GA10X_and_later
            ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(lpwr)" ,
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(lpwr)" ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackLpwr)" ,
            TCB                  => "OsTaskLpwr"    ,
            MAX_OVERLAYS_IMEM_LS => 13              ,
            MAX_OVERLAYS_DMEM    => 6               ,
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ 'pmu-*', ],
                                    ],
        ],

        OVERLAY_COMBINATION => [
            [ 'pmu-*', ] => [
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_lpwrLoadin?', ],
            ],

            [ 'pmu-gv11b', ] => [
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_lpwrLoadin', ],
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_libGr', '.imem_libFifo?', ],
            ],

            [ 'pmu-*', ] => [
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_lpwrLoadin', '.imem_libGC6?', '.imem_libSyncGpio?', '.imem_rtd3Entry?', ],
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_libMs?', '.imem_libPsi?', '.imem_clkLibCntrSwSync?', '.imem_gcx?', '.imem_libDi?', '.imem_libSyncGpio?',],
            ],
            [ 'pmu-gm20x...', ] => [
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_lpwrLoadin', '.imem_libGC6?', '.imem_libAcr?', ],
            ],

            # lpwr, libLpwr, libAp, libRmOneshot overlays are permanently attached to LPWR task
            # Thus all combinations will have these four overlays
            [ 'pmu-gp100...', '-pmu-gv11b' ] => [
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_libRmOneshot?', '.imem_libGr?', '.imem_libFifo?', '.imem_libPsi?', ],
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_libRmOneshot?', '.imem_libMs?', '.imem_libPsi?', '.imem_clkLibCntrSwSync?', '.imem_libClkVolt', ],
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_libRmOneshot?', '.imem_lpwrLoadin', '.imem_libGC6?', '.imem_libAcr?', '.imem_libGpio', '.imem_libPrivSec?', ],
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_libRmOneshot?', '.imem_lpwrLoadin', 'imem_libSyncGpio?', '.imem_libGC6?', '.imem_libPrivSec?', '.imem_rtd3Entry?', ],

                #PATH: task_lpwr lpwrEventHandler pmuRpcProcessUnitLpwr_IMPL dispRpcLpwrOsmStateChange dispRmOneshotStateUpdate_GM10X dispProgramDwcfIntr_GM10X
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_libRmOneshot?', '.imem_disp?', ],
            ],
            [ 'pmu-tu10x', ] => [
                # PATH: task_lpwr lpwrEventHandler isohubProgramMscgWatermarks_TU10X
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_dispImp', '.imem_perfVf', ],

                #PATH: task_lpwr lpwrEventHandler msMclkAction clkReadDomain_AnyTask osSemaphoreRWTakeRD
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_libClkReadAnyTask', '.imem_libSemaphoreRW', ],

                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_lpwrLoadin', 'imem_libSyncGpio?', '.imem_libGC6', '.imem_libPrivSec', '.imem_thermLibFanCommon', '.imem_libFxpBasic', '.imem_libPwm', '.imem_rtd3Entry?', ],
            ],

            # Debug path for PMU HALT
            [ 'pmu-tu10x', ] => [
                [ '.imem_lpwr', '.imem_libLpwr?', '.imem_libAp?', '.imem_libRmOneshot?', '.imem_libGr?', '.imem_libFifo?', '.imem_libPsi?', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libLw64', '.imem_libLw64Umul', ],
            ],

            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-gp100...', ] => [
            #     [ '.dmem_stackLpwr', '.dmem_lpwr', '.dmem_perf?', ], # pgPsiSleepLwrrentCallwlate : VOLT_RAIL BOARDOBJGRP
                                                                       # pgIslandSleepVoltageUpdate_GP10X : VOLT_DEVICE BOARDOBJGRP
            # [ 'pmu-tu10x', 'pmu-ga100', ] => [
            #     [ '.dmem_stackLpwr', '.dmem_lpwr', '.dmem_perf?', '.dmem_clk3x?', ], # lpwrPerfChangePost path
        ],
    },

    PMUTASK_LPWR_LP =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_lpwr_lp"  ,
            PRIORITY             => 1               ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE   => [
                               768 => DEFAULT,
            ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(lpwrLp)"      ,
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(lpwrLp)"      ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackLpwrLp)" ,
            TCB                  => "OsTaskLpwrLp"  ,
            MAX_OVERLAYS_IMEM_LS => 4               ,
            MAX_OVERLAYS_DMEM    => 4               ,
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ 'pmu-*', ],
                                    ],
        ],

        OVERLAY_COMBINATION => [ ],
    },

    PMUTASK_I2C =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN     => "task_i2c"  ,
            PRIORITY     => 1                     ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE   => [
                               464 => DEFAULT,
                               576 => [ dMAXWELL_thru_GP100, ],
                               768 => [ GP102_and_later, ],
                            ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM => "OVL_INDEX_IMEM(i2c)"      ,
            OVERLAY_STACK => "OVL_INDEX_DMEM(stackI2c)" ,
            TCB          => "OsTaskI2c"           ,
            PRIV_LEVEL_CSB       => [
                                      2 => DEFAULT,
                                      0 => [ PASCAL_and_later, -GP100, ],
                                    ],
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ '...pmu-ga100', ],
                                        "2048U" => [ 'pmu-ga10x_riscv...', ],
                                    ],
        ],

        OVERLAY_COMBINATION => [
            [ 'pmu-*', ] => [
                [ '.imem_i2c', ],
            ],
            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-gp100...', ] => [
            #     [ '.dmem_stackI2c', ],
            # ],
        ],
    },

    PMUTASK_SPI =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_spi"                 ,
            PRIORITY             => 1                          ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => [
                                    480 => DEFAULT,
                                    768 => [ GP102_and_later, ],
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(spi)"      ,
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(spi)"      ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackSpi)" ,
            TCB                  => "OsTaskSpi"                ,
            MAX_OVERLAYS_IMEM_LS => 3                          ,
            MAX_OVERLAYS_DMEM    => 2                          ,
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ '...pmu-ga100', ],
                                        "1024U" => [ 'pmu-ga10x_riscv...', ],
                                    ],
        ],

        OVERLAY_COMBINATION => [
            [ 'pmu-gm20x...', ] => [
                [ '.imem_spi', '.imem_spiLibConstruct', '.imem_libBoardObj', ], # RM_PMU_BOARDOBJ_CMD_GRP SET
                [ '.imem_spi', '.imem_spiLibRomInit', ],
            ],
            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-gp100...', ] => [
            #     [ '.dmem_stackSpi', '.dmem_spi', ],
            # ],
        ],
    },

    PMUTASK_SEQ =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN            => "task_sequencer"  ,
            PRIORITY            => 5                 ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE     => [
                                 464 => DEFAULT,
                                 768 => [ GP102_and_later, ],
                              ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM   => "OVL_INDEX_IMEM(seq)"  ,
            OVERLAY_STACK  => "OVL_INDEX_DMEM(stackSeq)" ,
            TCB            => "OsTaskSeq"       ,
            MAX_OVERLAYS_DMEM => 2              ,
            PRIV_LEVEL_EXT => 0,
            PRIV_LEVEL_CSB => 0,
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ 'pmu-*', ],
                                    ],
        ],

        OVERLAY_COMBINATION => [
            [ 'pmu-*', ] => [
                [ '.imem_seq', ],
            ],
            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-gp100...', ] => [
            #     [ '.dmem_stackSeq', ],
            # ],
        ],
    },

    PMUTASK_PCMEVT =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_pcmEvent"     ,
            PRIORITY             => 4                   ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => [
                                       384 => DEFAULT,
                                       512 => [ GP102_and_later, ],
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(pcmevt)" ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackPcmevt)" ,
            TCB                  => "OsTaskPcmEvent"    ,
            MAX_OVERLAYS_IMEM_LS => 3                   ,
        ],

        OVERLAY_COMBINATION => [
            [ 'pmu-gm20x', ] => [
                [ '.imem_pcmevt', ],
                [ '.imem_pcmevt', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', ],
            ],
        ],
    },

    PMUTASK_PMGR =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_pmgr"       ,
            PRIORITY             => 1                 ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => [
                                        780 => DEFAULT,
                                       1060 => [ GM10X, ],
                                       1512 => [ GM20X, ],
                                       1700 => [ GP100, ],
                                       1792 => [ PASCAL, -GP100, ],
                                       2560 => [ VOLTA, TURING_and_later, ],
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(pmgr)" ,
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(pmgr)" ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackPmgr)" ,
            TCB                  => "OsTaskPmgr"      ,
            MAX_OVERLAYS_IMEM_LS => 19                ,
            MAX_OVERLAYS_DMEM    => [
                                        7 => DEFAULT,
                                       10 => [ TURING_and_later, ],
                                    ],
            USING_FPU            => [
                                        "LW_FALSE" => DEFAULT ,
                                        "LW_TRUE"  => [ AMPERE_and_later, -GA100, ] ,
                                    ],
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ '...pmu-ga100', ],
                                        "2048U" => [ 'pmu-ga10x_riscv...', ],
                                    ],

        ],

        # IMEM saving ideas:
        #     - Many of the pwrPolicyDomGrp* functions are only needed for pwrPolicy evaluation, they do not need to be in imem_pmgr
        OVERLAY_COMBINATION => [
            [ 'pmu-gm10x', ] => [
                # See feature PMU_PMGR_PWR_POLICY_3X_ONLY (i.e. ilwerse of PMU_PMGR_PWR_MONITOR_1X and PMU_PMGR_PWR_POLICY_2X)
                # task_pmgr -> pwrMonitor -> s_pwrMonitor
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_libi2c', '.imem_pmgrLibPwrMonitor', ],

                # task_pmgr -> pwrPoliciesEvaluate_2X
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', ],

                # See feature PMU_PMGR_PWR_MONITOR_1X
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrPwrChannelsQueryLegacy -> pwrMonitorChannelsQuery
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibQuery', ],
            ],
            [ 'pmu-gm*', ] => [
                # See feature PMU_PERF_ELW_PSTATES_2X
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrPerfTablesInfoSet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', ],

                # See feature PMU_PERF_ELW_PSTATES_2X
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrPerfStatusUpdate
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrPerfStatusUpdate -> perfProcessPerfStatus -> pmgrProcessPerfStatus -> pmgrProcessPerfChange -> pwrDevVoltageSetChanged
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', ],
            ],
            [ 'pmu-*', ] => [
                # task_pmgr
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', ],

                # task_pmgr -> pmgrPostInit
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_libPmgrInit', ],

                # See feature PMU_PMGR_BOARDOBJGRP_SUPPORT
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd (CMD_SET base)
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibConstruct', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd (CMD_GET_STATUS base)
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibQuery', ],

                # See feature PMU_PMGR_PWR_POLICY
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set (base)
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibConstruct', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set (construct path)
                [ '.imem_pmgr',                                              '.imem_perfVf',                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyConstruct', '.imem_pmgrLibPwrPolicyClient', '.imem_libPwmConstruct', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_libi2c', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrPoliciesLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_libi2c', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrPoliciesLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrUnloadHelper
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10GetStatus
                [ '.imem_pmgr',                                              '.imem_perfVf',                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibQuery', '.imem_pmgrLibPwrPolicyClient', ],

                # See feature PMU_PMGR_PWR_DEVICE
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrDeviceBoardObjGrpIfaceModel10GetStatus
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibQuery', '.imem_libi2c', '.imem_pmgrLibPwrMonitor', ],


                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrUnload -> s_pmgrUnloadHelper
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibLoad', ],

                # See feature PMU_PMGR_PWR_DEVICE
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrPwrDevVoltageChange -> pwrDevVoltageSetChanged
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', ],
            ],
            [ 'pmu-gm20x...', ] => [
                # See feature (PMU_PMGR_PWR_POLICY_RELATIONSHIP && PMU_PMGR_PWR_POLICY_RELATIONSHIP_BALANCE)
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyRelationshipLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_libPwm', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyRelationshipLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_libPwm', ],

            ],
            [ 'pmu-gm10x', 'pmu-gm20x', ] => [
                # See feature PMU_PERF_ELW_PSTATES_2X & _BA12 PWR_DEV_BA_STATE_SYNC therm hal
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrPerfStatusUpdate -> perfProcessPerfStatus -> pmgrProcessPerfStatus -> pmgrProcessPerfChange -> pwrDevVoltageSetChanged -> pwrDevVoltageSetChanged_BA1XHW -> thermPwrDevBaStateSync_BA12 -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> psiSleepLwrrentCalcMonoRail -> pgCtrlWakeExt
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libLpwr', ],

                # See feature PMU_PMGR_PWR_DEVICE & _BA12 PWR_DEV_BA_STATE_SYNC therm hal
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrPwrDevVoltageChange -> pwrDevVoltageSetChanged -> pwrDevVoltageSetChanged_BA1XHW -> thermPwrDevBaStateSync_BA12 -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> psiSleepLwrrentCalcMonoRail -> pgCtrlWakeExt
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libLpwr', ],

                # task_pmgr -> {callback path} -> s_pwrDevOsCallback -> pwrDevStateSync -> pwrDevStateSync_BA1XHW -> thermPwrDevBaStateSync_BA12 -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> psiSleepLwrrentCalcMonoRail -> pgCtrlWakeExt
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libLpwr', ],

                # task_pmgr -> pwrDevStateSync -> pwrDevStateSync_BA1XHW -> thermPwrDevBaStateSync_BA12 -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> psiSleepLwrrentCalcMonoRail -> pgCtrlWakeExt
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libLpwr', ],

                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD -> pwrPolicyFilter_WORKLOAD -> s_pwrPolicyWorkloadLeakageCompute -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> psiSleepLwrrentCalcMonoRail -> pgCtrlWakeExt
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_libLpwr', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> pwrPolicyDomGrpEvaluate -> pwrPolicyDomGrpEvaluate_WORKLOAD -> s_pwrPolicyWorkloadLeakageCompute -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> psiSleepLwrrentCalcMonoRail -> pgCtrlWakeExt
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_libLpwr', ],
            ],
            [ '...pmu-gv10x', ] => [
                # See feature PMU_PMGR_PWR_POLICY_30
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> (perfReadSemaphoreTake | perfReadSemaphoreGive)
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_libSemaphoreRW?', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter -> pwrMonitorChannelsTupleQuery -> s_pwrMonitorChannelsTupleQuery
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibQuery', '.imem_libBoardObj', '.imem_libi2c', '.imem_pmgrLibPwrMonitor', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', ],
            ],
            [ 'pmu-*', ] => [
                # See feature PMU_PMGR_PWR_MONITOR_2X
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrChannelBoardObjGrpIfaceModel10GetStatus
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibQuery', '.imem_libi2c', '.imem_pmgrLibPwrMonitor', ],
            ],
            [ 'pmu-gm20x...pmu-gv10x', ] => [
                # See feature PMU_PMGR_PWR_POLICY_30 + PMU_PMGR_PWR_POLICY_BALANCE
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> pwrPolicyBalanceEvaluate
                [ '.imem_pmgr', '.imem_libFxpBasic',                         '.imem_perfVf',                     '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_libPwm', ],

                # See feature PMU_PMGR_PWR_POLICY_30 + PMU_PMGR_GPUMON_PWR_3X
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> s_pwrPoliciesGpumonPwrSampleLog ->loggerWritePadded
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_loggerWrite', ],
            ],
            [ 'pmu-gm20x...', ] => [
                # task_pmgr -> pmgrPwrMonitorChannelQuery -> pwrMonitorChannelTupleQuery -> pwrMonitorChannelTupleStatusGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibQuery', '.imem_libi2c', '.imem_pmgrLibPwrMonitor', ],
            ],
            [ 'pmu-gp100', 'pmu-gp10x', ] => [
                # See feature PMU_PMGR_PWR_DEVICE_VOLT_STATE_SYNC & _BA13 PWR_DEV_BA_STATE_SYNC therm hal
                # task_pmgr -> pwrDevVoltStateSync -> s_pwrDevOsCallback -> pwrDevStateSync -> pwrDevStateSync_BA1XHW -> thermPwrDevBaStateSync_BA13 -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libVoltApi', '.imem_pmgrPwrDeviceStateSync', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> {callback path} -> s_pwrDevOsCallback -> pwrDevStateSync -> pwrDevStateSync_BA1XHW -> thermPwrDevBaStateSync_BA13 -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libVoltApi', '.imem_pmgrPwrDeviceStateSync', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],

                # See feature PMU_PMGR_PWR_DEVICE & _BA13 PWR_DEV_BA_STATE_SYNC therm hal
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrPwrDevVoltageChange -> pwrDevVoltageSetChanged -> pwrDevVoltageSetChanged_BA1XHW -> thermPwrDevBaStateSync_BA13 -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libVoltApi', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
            ],
            [ 'pmu-gp100', 'pmu-gp10x', 'pmu-gv10x', ] => [
                # See feature PMU_PMGR_PWR_POLICY_30 + PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter -> pwrPolicy3XFilter
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_MULTIRAIL -> pwrPolicyFilter_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailInputStateGetCommonParams -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_MULTIRAIL -> pwrPolicyFilter_WORKLOAD_MULTIRAIL -> pmgrFreqMHzGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync',],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> pwrViolationsEvaluate
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_libLw64', '.imem_libLw64Umul', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate-> pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
            ],
            [ 'pmu-gp100...', '-pmu-gv11b' ] => [
                # See feature PMU_LIB_GPIO
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrGpioInitCfg
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_libGpio', ],

                # See feature PMU_PMGR_PWR_VIOLATION
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrViolationsLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrViolationsLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', ],

                # See feature (PMU_PMGR_PWR_EQUATION && PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS12)
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrEquationBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> pmgrPwrEquationIfaceModel10SetEntry -> pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS12 -> s_pwrEquationLeakageDtcs12GetIddq
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibConstruct', '.imem_perfVfe', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfVfeEquMonitor', '.imem_libSemaphoreRW', ],

                # See feature (PMU_PMGR_PWR_POLICY && PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD)
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> pmgrPwrPolicyIfaceModel10SetEntry -> pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_MULTIRAIL -> pmgrFreqMHzGet
                [ '.imem_pmgr',                                              '.imem_perfVf',                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                           '.imem_pmgrLibPwrPolicyConstruct', '.imem_pmgrLibPwrPolicyClient', '.imem_libPwmConstruct', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', ],
            ],
            [ 'pmu-gp10x', ] => [
                # See feature PMU_PSI_PMGR_SLEEP_CALLBACK
                # task_pmgr -> psiSleepLwrrentCalcMultiRail -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_libPsiCallback', '.imem_libVoltApi', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> psiSleepLwrrentCalcMultiRail -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_libPsiCallback', '.imem_libVoltApi', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> psiSleepLwrrentCalcMultiRail -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                 '.imem_libLeakage', '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libPsiCallback', '.imem_libVoltApi', '.imem_thermLibSensor2X', '.imem_libi2c', ],

                # See feature PMU_PSI_PMGR_SLEEP_CALLBACK
                # task_pmgr -> {callback path} -> s_psiHandleOsCallbackMultiRail -> psiSleepLwrrentCalcMultiRail -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_libPsiCallback', '.imem_libVoltApi', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> {callback path} -> s_psiHandleOsCallbackMultiRail -> pgCtrlWakeExt
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_libPsiCallback', '.imem_libLpwr', ],
            ],
            [ 'pmu-gp10x...pmu-gv10x', ] => [
                # See feature PMU_PMGR_PWR_POLICY_30 + PMU_PMGR_PWR_POLICY_INPUT_FILTERING + PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD + PMU_PMGR_PWR_VIOLATION
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> (perfReadSemaphoreTake | perfReadSemaphoreGive)
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libSemaphoreRW', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter -> pwrMonitorChannelsTupleQuery -> s_pwrMonitorChannelsTupleQuery
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibQuery','.imem_libBoardObj','.imem_libi2c','.imem_libLw64','.imem_libLw64Umul','.imem_pmgrLibPwrMonitor', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter -> pwrPolicy3XFilter
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_MULTIRAIL -> pwrPolicyFilter_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailInputStateGetCommonParams -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_MULTIRAIL -> pwrPolicyFilter_WORKLOAD_MULTIRAIL -> pmgrFreqMHzGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync',],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> pwrViolationsEvaluate
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_libLw64', '.imem_libLw64Umul', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> pwrPolicyBalanceEvaluate
                [ '.imem_pmgr', '.imem_libFxpBasic',                         '.imem_perfVf',                     '.imem_pmgrPwrPolicyCallback', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_libPwm', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate-> pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> s_pwrPoliciesGpumonPwrSampleLog ->loggerWritePadded
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_loggerWrite', ],
            ],
            [ 'pmu-gp10x...', ] => [
                # See feature (PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER && PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT)
                # task_pmgr -> pmgrPwrMonitorChannelQuery -> pwrMonitorChannelTupleQuery -> pwrMonitorChannelTupleStatusGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibQuery', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', '.imem_perfVf', ],

                # See feature (PMU_PMGR_PWR_DEVICE && PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER && PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT)
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrDeviceBoardObjGrpIfaceModel10GetStatus
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibQuery', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', '.imem_perfVf', ],

                # See feature (PMU_PMGR_PWR_MONITOR_2X && PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER && PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT)
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrChannelBoardObjGrpIfaceModel10GetStatus
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibQuery', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', '.imem_perfVf', ],
            ],
            [ 'pmu-gv10x', ] => [
                # See feature PMU_PMGR_PWR_LWRRENT_COLWERSION
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyLoad -> pwrPolicyLoad_HW_THRESHOLD -> pwrMonitorChannelTupleQuery
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_pmgrLibQuery', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', '.imem_perfVf', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyLoad -> pwrPolicyLoad_HW_THRESHOLD -> pwrMonitorChannelTupleQuery
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_pmgrLibQuery', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', '.imem_perfVf', ],

                # See feature PMU_PMGR_PWR_POLICY_30 + PMU_PMGR_PWR_POLICY_INPUT_FILTERING + PMU_PMGR_PWR_EQUATION_DYNAMIC + PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD + PMU_PMGR_PWR_VIOLATION
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> (perfReadSemaphoreTake | perfReadSemaphoreGive)
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libSemaphoreRW', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter -> pwrMonitorChannelsTupleQuery -> s_pwrMonitorChannelsTupleQuery
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibQuery','.imem_libBoardObj','.imem_libi2c','.imem_libLw64','.imem_libLw64Umul','.imem_pmgrLibPwrMonitor', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter -> pwrPolicy3XFilter
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_MULTIRAIL -> pwrPolicyFilter_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailInputStateGetCommonParams -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> s_pwrPolicies30Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_MULTIRAIL -> pwrPolicyFilter_WORKLOAD_MULTIRAIL -> pmgrFreqMHzGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync',],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> pwrViolationsEvaluate
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_libLw64', '.imem_libLw64Umul', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> pwrPolicyBalanceEvaluate
                [ '.imem_pmgr', '.imem_libFxpBasic',                         '.imem_perfVf',                     '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_libPwm', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate-> pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_30 -> pwrPoliciesEvaluate_30 -> pwrPoliciesEvaluate_3X -> s_pwrPoliciesGpumonPwrSampleLog ->loggerWritePadded
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_loggerWrite', ],
            ],
            [ 'pmu-gv10x', 'pmu-tu10x', 'pmu-ga100', ] => [
                # See feature PMU_PMGR_PWR_DEVICE_VOLT_STATE_SYNC & _BA15 PWR_DEV_BA_STATE_SYNC therm hal
                # task_pmgr -> pwrDevVoltStateSync -> s_pwrDevOsCallback -> pwrDevStateSync -> pwrDevStateSync_BA1XHW -> thermPwrDevBaStateSync_BA15 -> pmgrComputeFactorA -> pwrEquationBA15ScaleComputeScaleA -> pwrEquationDynamicScaleVoltageLwrrent
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libVoltApi', '.imem_pmgrLibFactorA', '.imem_libPwrEquation', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_libBoardObj', ],
                # task_pmgr -> pwrDevVoltStateSync -> s_pwrDevOsCallback -> pwrDevStateSync -> pwrDevStateSync_BA1XHW -> thermPwrDevBaStateSync_BA15 -> voltRailGetVoltage_IMPL
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libVoltApi', '.imem_pmgrLibFactorA', '.imem_libSemaphoreRW', ],
                # task_pmgr -> pwrDevVoltStateSync -> s_pwrDevOsCallback -> pwrDevStateSync -> pwrDevStateSync_BA1XHW -> thermPwrDevBaStateSync_BA15 -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libVoltApi', '.imem_libLeakage',  '.imem_pmgrLibFactorA', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> {callback path} -> s_pwrDevOsCallback -> pwrDevStateSync -> pwrDevStateSync_BA1XHW -> thermPwrDevBaStateSync_BA15 -> pmgrComputeFactorA -> pwrEquationBA15ScaleComputeScaleA -> pwrEquationDynamicScaleVoltageLwrrent
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libVoltApi', '.imem_pmgrLibFactorA', '.imem_libPwrEquation', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_libBoardObj', ],
                # task_pmgr -> {callback path} -> s_pwrDevOsCallback -> pwrDevStateSync -> pwrDevStateSync_BA1XHW -> thermPwrDevBaStateSync_BA15 -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libVoltApi', '.imem_libLeakage',  '.imem_pmgrLibFactorA', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> {callback path} -> s_pwrDevOsCallback -> pwrDevStateSync -> pwrDevStateSync_BA1XHW -> thermPwrDevBaStateSync_BA15 -> voltRailGetVoltage_IMPL
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libVoltApi', '.imem_libLeakage',  '.imem_pmgrLibFactorA', '.imem_libSemaphoreRW', ],

                # See feature PMU_PMGR_PWR_DEVICE & _BA15 PWR_DEV_BA_STATE_SYNC therm hal
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrPwrDevVoltageChange -> pwrDevVoltageSetChanged -> pwrDevVoltageSetChanged_BA1XHW -> thermPwrDevBaStateSync_BA15 -> pmgrComputeFactorA -> pwrEquationBA15ScaleComputeScaleA -> pwrEquationDynamicScaleVoltageLwrrent
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libVoltApi', '.imem_pmgrLibFactorA', '.imem_libPwrEquation', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_libBoardObj', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL -> pmuRpcPmgrPwrDevVoltageChange -> pwrDevVoltageSetChanged -> pwrDevVoltageSetChanged_BA1XHW -> thermPwrDevBaStateSync_BA15 -> voltRailGetVoltage_IMPL
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libVoltApi', '.imem_pmgrLibFactorA', '.imem_libSemaphoreRW', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrPwrDevVoltageChange -> pwrDevVoltageSetChanged -> pwrDevVoltageSetChanged_BA1XHW -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_libVoltApi', '.imem_libLeakage', '.imem_pmgrLibFactorA', '.imem_thermLibSensor2X', '.imem_libi2c',  ],

                # See feature PMU_PMGR_PWR_DEVICE_BA15
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad -> pwrDevLoad -> pwrDevLoad_BA15HW -> pwrEquationBA15ScaleComputeShiftA -> pwrEquationBA15ScaleComputeScaleA
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_libPwrEquation', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_libBoardObj', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad -> pwrDevLoad -> pwrDevLoad_BA15HW -> pwrEquationBA15ScaleComputeShiftA -> pwrEquationBA15ScaleComputeScaleA
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_libPwrEquation', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_libBoardObj', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad -> pwrDevLoad -> pwrDevLoad_BA15HW -> thermPwrDevBaLoad_BA15
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_pmgrLibFactorA', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad -> pwrDevLoad -> pwrDevLoad_BA15HW -> thermPwrDevBaLoad_BA15
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_pmgrLibFactorA', ],
            ],
            [ 'pmu-gv10x...', ] => [
                # See feature (PMU_PMGR_PWR_EQUATION && PMU_PMGR_PWR_EQUATION_BA15_SCALE)
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrEquationBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> pmgrPwrEquationIfaceModel10SetEntry -> pwrEquationGrpIfaceModel10ObjSetImpl_BA15_SCALE -> s_pwrEquationBA15ScaleComputeConstF
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibConstruct', '.imem_libPwrEquation', ],
            ],
            [ 'pmu-tu10x', 'pmu-ga100', ] => [
                # See feature PMU_PMGR_PWR_DEVICE_VOLT_STATE_SYNC & feature PMU_PMGR_PWR_DEVICE_GPUADC10
                # task_pmgr -> pwrDevVoltStateSync -> s_pwrDevOsCallback -> pwrDevStateSync -> pwrDevStateSync_GPUADC10 -> pwrChannelsStatusGet -> pwrChannelIfaceModel10GetStatus
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_pmgrLibQuery', '.imem_libBoardObj', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', ],
                # task_pmgr -> {callback path} -> s_pwrDevOsCallback -> pwrDevStateSync -> pwrDevStateSync_GPUADC10 -> pwrChannelsStatusGet -> pwrChannelIfaceModel10GetStatus
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrDeviceStateSync', '.imem_pmgrLibQuery', '.imem_libBoardObj', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', ],

                # See _TU10X INIT_FACTOR_A_LUT therm hal
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad -> pwrDevLoad -> pwrDevLoad_BA15HW -> thermPwrDevBaLoad_BA15 -> thermInitFactorALut_TU10X -> pmgrComputeFactorA -> pwrEquationBA15ScaleComputeScaleA
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_pmgrLibFactorA', '.imem_libPwrEquation', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_libBoardObj', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad -> pwrDevLoad -> pwrDevLoad_BA15HW -> thermPwrDevBaLoad_BA15 -> thermInitFactorALut_TU10X -> pmgrComputeFactorA -> pwrEquationBA15ScaleComputeScaleA
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_pmgrLibFactorA', '.imem_libPwrEquation', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_libBoardObj', ],
            ],
            [ 'pmu-tu10x', ] => [
                # See feature PMU_PMGR_PWR_MONITOR_SEMAPHORE
                # task_pmgr -> pmgrPostInit -> pwrMonitorConstruct -> lwrtosSemaphoreCreateBinaryGiven
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_libPmgrInit', '.imem_libInit', ],
            ],
            [ 'pmu-tu10x', 'pmu-ga100', ] => [
                # See feature PMU_PMGR_PWR_DEVICE_GPUADC1X
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad -> pwrDevLoad -> pwrDevLoad_GPUADC1X -> pwrDevGpuAdc1xSwResetTrigger
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_pmgrLibPwrMonitor', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad -> pwrDevLoad -> pwrDevLoad_GPUADC1X -> pwrDevGpuAdc1xSwResetTrigger
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_pmgrLibPwrMonitor', ],

                # See feature PMU_PMGR_PWR_DEVICE_GPUADC10
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad -> pwrDevLoad -> pwrDevLoad_GPUADC10 -> s_pwrDevGpuAdc10UpdateFuseParams
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVfe', '.imem_libBoardObj', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfVfeEquMonitor', '.imem_libSemaphoreRW', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad -> pwrDevLoad -> pwrDevLoad_GPUADC10 -> s_pwrDevGpuAdc10UpdateFuseParams
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVfe', '.imem_libBoardObj', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfVfeEquMonitor', '.imem_libSemaphoreRW', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad -> pwrDevLoad -> pwrDevLoad_GPUADC10 -> pwrChannelsStatusGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_pmgrLibQuery', '.imem_libFxpBasic', '.imem_perfVf', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> s_pmgrPwrDevicesLoad -> pwrDevLoad -> pwrDevLoad_GPUADC10 -> pwrChannelsStatusGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_pmgrLibQuery', '.imem_libFxpBasic', '.imem_perfVf', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', ],

                # See feature PMU_PMGR_PWR_POLICY_DOMGRP_PMU_PERF_LIMITS
                # task_pmgr -> pmgrPostInit -> pwrPoliciesConstruct -> pwrPoliciesPostInit_DOMGRP -> perfLimitsClientInit
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_libPmgrInit', '.imem_libInit', '.imem_libPerfLimitClient', ],

                # See feature PMU_PMGR_PERF_VF_ILWALIDATION_NOTIFY
                # task_pmgr -> pwrPoliciesVfIlwalidate
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPff', '.imem_libBoardObj', '.imem_perfVf', '.imem_libSemaphoreRW', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrPoliciesVfIlwalidate
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPff', '.imem_libBoardObj', '.imem_perfVf', '.imem_libSemaphoreRW', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrPoliciesVfIlwalidate
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_pmgrLibPwrPolicy', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPff', '.imem_libBoardObj', '.imem_perfVf', '.imem_libSemaphoreRW', ],

                # See feature (PMU_PMGR_PWR_POLICY && PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD && PMU_PMGR_PWR_POLICY_35)
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> pmgrPwrPolicyIfaceModel10SetEntry -> pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_MULTIRAIL -> pmgrFreqMHzGet
                [ '.imem_pmgr',                                              '.imem_perfVf',                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                           '.imem_pmgrLibPwrPolicyConstruct', '.imem_pmgrLibPwrPolicyClient', '.imem_libPwmConstruct', '.imem_pmgrLibPwrPolicy', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', ],

                # See feature PMU_PMGR_ILLUM_ZONE
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumZoneBoardObjGrpIfaceModel10Set
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                          '.imem_pmgrLibIllumConstruct', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumZoneBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumZoneIfaceModel10SetEntry -> illumZoneGrpIfaceModel10ObjSetImpl_RGB
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                          '.imem_pmgrLibIllumConstruct', '.imem_pmgrLibIllum', '.imem_libi2c', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumZoneBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumZoneIfaceModel10SetEntry -> illumZoneGrpIfaceModel10ObjSetImpl_RGB -> illumDeviceDataSet -> illumDeviceDataSet_GPIO_PWM_SINGLE_COLOR_V10
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                          '.imem_pmgrLibIllumConstruct', '.imem_pmgrLibIllum', '.imem_libi2c', '.imem_libPwm', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumZoneBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumZoneIfaceModel10SetEntry -> illumZoneGrpIfaceModel10ObjSetImpl_RGB -> illumDeviceDataSet -> illumDeviceDataSet_GPIO_PWM_RGBW_V10
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                          '.imem_pmgrLibIllumConstruct', '.imem_pmgrLibIllum', '.imem_libi2c', '.imem_libPwm', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumZoneBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumZoneIfaceModel10SetEntry -> illumZoneGrpIfaceModel10ObjSetImpl_RGBW
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                          '.imem_pmgrLibIllumConstruct', '.imem_pmgrLibIllum', '.imem_libi2c', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumZoneBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumZoneIfaceModel10SetEntry -> illumZoneGrpIfaceModel10ObjSetImpl_RGBW -> illumDeviceDataSet -> illumDeviceDataSet_GPIO_PWM_SINGLE_COLOR_V10
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                          '.imem_pmgrLibIllumConstruct', '.imem_pmgrLibIllum', '.imem_libi2c', '.imem_libPwm', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumZoneBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumZoneIfaceModel10SetEntry -> illumZoneGrpIfaceModel10ObjSetImpl_RGBW -> illumDeviceDataSet -> illumDeviceDataSet_GPIO_PWM_RGBW_V10
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                          '.imem_pmgrLibIllumConstruct', '.imem_pmgrLibIllum', '.imem_libi2c', '.imem_libPwm', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumZoneBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumZoneIfaceModel10SetEntry -> illumZoneGrpIfaceModel10ObjSetImpl_SINGLE_COLOR
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                          '.imem_pmgrLibIllumConstruct', '.imem_pmgrLibIllum', '.imem_libi2c', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumZoneBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumZoneIfaceModel10SetEntry -> illumZoneGrpIfaceModel10ObjSetImpl_SINGLE_COLOR -> illumDeviceDataSet -> illumDeviceDataSet_GPIO_PWM_SINGLE_COLOR_V10
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                          '.imem_pmgrLibIllumConstruct', '.imem_pmgrLibIllum', '.imem_libi2c', '.imem_libPwm', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumZoneBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumZoneIfaceModel10SetEntry -> illumZoneGrpIfaceModel10ObjSetImpl_SINGLE_COLOR -> illumDeviceDataSet -> illumDeviceDataSet_GPIO_PWM_RGBW_V10
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                          '.imem_pmgrLibIllumConstruct', '.imem_pmgrLibIllum', '.imem_libi2c', '.imem_libPwm', ],

                # See feature PMU_PMGR_ILLUM_DEVICE
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumDeviceBoardObjGrpIfaceModel10Set
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                           '.imem_pmgrLibIllumConstruct', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumDeviceBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumDeviceIfaceModel10SetEntry -> illumDeviceGrpIfaceModel10ObjSetImpl_MLWV10 -> illumDeviceGrpIfaceModel10ObjSetImpl_SUPER
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                           '.imem_pmgrLibIllumConstruct', '.imem_pmgrLibIllum', '.imem_libi2c', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumDeviceBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumDeviceIfaceModel10SetEntry -> illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_SINGLE_COLOR_V10
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                           '.imem_pmgrLibIllumConstruct', '.imem_libPwmConstruct', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumDeviceBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumDeviceIfaceModel10SetEntry -> illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_SINGLE_COLOR_V10 -> illumDeviceGrpIfaceModel10ObjSetImpl_SUPER
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                           '.imem_pmgrLibIllumConstruct', '.imem_pmgrLibIllum', '.imem_libi2c', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumDeviceBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumDeviceIfaceModel10SetEntry -> illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_RGBW_V10
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                           '.imem_pmgrLibIllumConstruct', '.imem_libPwmConstruct', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> illumDeviceBoardObjGrpIfaceModel10Set -> boardObjGrpIfaceModel10Set_E32 -> boardObjGrpIfaceModel10Set -> s_illumDeviceIfaceModel10SetEntry -> illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_RGBW_V10 -> illumDeviceGrpIfaceModel10ObjSetImpl_SUPER
                [ '.imem_pmgr',                                                                                  '.imem_pmgrLibBoardObj', '.imem_libBoardObj',                           '.imem_pmgrLibIllumConstruct', '.imem_pmgrLibIllum', '.imem_libi2c', ],

                # See feature PMU_PMGR_PWR_CHANNEL_35
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrChannelsLoad -> pwrChannelsLoad_35
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                                               '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', '.imem_pmgrLibQuery', '.imem_libFxpBasic', '.imem_perfVf', '.imem_libBoardObj', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrChannelsLoad -> pwrChannelsLoad_35
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', '.imem_pmgrLibQuery', '.imem_libFxpBasic', '.imem_perfVf', '.imem_libBoardObj', ],

                # See feature PMU_PMGR_PWR_CHANNEL_35
                # task_pmgr -> {callback path} -> s_pwrChannels35Callback -> pwrChannelsStatusGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrChannelsCallback', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', '.imem_pmgr', '.imem_pmgrLibQuery', '.imem_libFxpBasic', '.imem_perfVf', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibQuery', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', '.imem_perfVf', ],

                # See feature PMU_PMGR_PWR_POLICY_35 + PMU_PMGR_PWR_POLICY_INPUT_FILTERING + PMU_PMGR_PWR_EQUATION_DYNAMIC + PMU_PMGR_PWR_VIOLATION
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> (perfReadSemaphoreTake | perfReadSemaphoreGive)
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_libSemaphoreRW', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrChannelsStatusGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_libi2c', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrMonitor', '.imem_pmgrLibQuery', '.imem_libFxpBasic', '.imem_perfVf', '.imem_libBoardObj', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_MULTIRAIL -> pwrPolicyFilter_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailInputStateGetCommonParams -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_MULTIRAIL -> pwrPolicyFilter_WORKLOAD_MULTIRAIL -> pmgrFreqMHzGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync',],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> pwrViolationsEvaluate
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_libLw64', '.imem_libLw64Umul', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> pwrPolicyBalanceEvaluate
                [ '.imem_pmgr', '.imem_libFxpBasic',                         '.imem_perfVf',                     '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_libPwm', ],
                # See feature PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> pwrPolicyLimitEvaluate
                [ '.imem_pmgr', '.imem_libFxpBasic',                         '.imem_perfVf',                     '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPff', '.imem_libBoardObj', '.imem_perfVf', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate -> pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailGetVoltageFloor -> voltRailGetVoltageMin
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libSemaphoreRW', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate -> pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq -> voltPolicyOffsetGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libSemaphoreRW', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate -> pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq -> voltRailGetLeakage   voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],

                # See feature PMU_PMGR_PWR_POLICY_35 + PMU_PMGR_GPUMON_PWR_3X
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> s_pwrPoliciesGpumonPwrSampleLog ->loggerWritePadded
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_loggerWrite', ],

                # See feature PMU_PMGR_PWR_POLICY_DOMGRP_PMU_PERF_LIMITS
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesArbitrateAndApply -> pwrPolicyDomGrpLimitsSetAll -> s_pwrPolicyDomGrpLimitsSetAll_PMU
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_libPerfLimitClient', ],

                # See feature PMU_PMGR_PWR_CHANNEL_35
                # task_pmgr -> pmgrPwrMonitorChannelQuery -> pwrMonitorPolledTupleGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrLibPwrMonitor', '.imem_libLw64', '.imem_libLw64Umul', ],
            ],
            [ 'pmu-ga100', ] => [
                # See feature PMU_PMGR_RPC_TEST_EXELWTE
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrTestExelwte -> pmgrTestAdcCheck_GA100
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_libPmgrTest', '.imem_libLw64', '.imem_libLw64Umul', ],

                # See feature (PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_SENSED_VOLTAGE && PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD && PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X)
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrPoliciesLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                          '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrPoliciesLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', ],

                # See feature PMU_PMGR_PWR_VIOLATION
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrViolationsLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                          '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libLw64', '.imem_libLw64Umul', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrViolationsLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libLw64', '.imem_libLw64Umul', ],

                # See feature PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_SENSED_VOLTAGE
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyLoad -> pwrPolicyLoad_WORKLOAD_MULTIRAIL -> pwrPolicyLoad_WORKLOAD_MULTIRAIL_INTERFACE -> voltRailGetVoltageSensed
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                          '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libBoardObj', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyLoad -> pwrPolicyLoad_WORKLOAD_MULTIRAIL -> pwrPolicyLoad_WORKLOAD_MULTIRAIL_INTERFACE -> voltRailGetVoltageSensed
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libBoardObj', ],

                # See feature (PMU_PMGR_PWR_POLICY_RELATIONSHIP && PMU_PMGR_PWR_POLICY_RELATIONSHIP_BALANCE)
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyRelationshipLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                          '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libPwm', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyRelationshipLoad
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libPwm', ],

                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libVoltApi', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_MULTIRAIL -> pwrPolicyFilter_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailInputStateGetCommonParams -> voltRailGetVoltage_IMPL
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient',                                      '.imem_libVoltApi', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libVoltApi', '.imem_libSemaphoreRW', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_MULTIRAIL -> pwrPolicyFilter_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailInputStateGetCommonParams -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient',                                      '.imem_libVoltApi', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libVoltApi', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_MULTIRAIL -> pwrPolicyFilter_WORKLOAD_MULTIRAIL -> pmgrFreqMHzGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient',                                      '.imem_libVoltApi', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libVoltApi', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync',],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate (non PWR_MODEL policies)
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libClkVolt', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate -> pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailGetVoltageFloor -> voltRailGetVoltageMin
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libSemaphoreRW', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate -> pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq -> voltPolicyOffsetGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libSemaphoreRW', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate -> pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL -> pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq -> voltRailGetLeakage   voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj', '.imem_pmgrLibPwrPolicyClient', '.imem_pmgrLibPwrPolicyWorkloadMultirail', '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],

                # See feature PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyLoad -> pwrPolicyLoad_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X -> pmgrFreqMHzGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_clkLibCntrSwSync', '.imem_clkLibCntr', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyLoad -> pwrPolicyLoad_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X -> pmgrFreqMHzGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                          '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_clkLibCntrSwSync', '.imem_clkLibCntr', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyLoad -> pwrPolicyLoad_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyLoad -> pwrPolicyLoad_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                          '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyLoad -> pwrPolicyLoad_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X -> voltRailGetVoltageSensed
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libBoardObj', '.imem_libClkVolt', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyLoad -> pwrPolicyLoad_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X -> voltRailGetVoltageSensed
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                          '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libBoardObj', '.imem_libClkVolt', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrBoardObjGrpCmd -> boardObjGrpIfaceModel10CmdHandlerDispatch -> pwrPolicyBoardObjGrpIfaceModel10Set -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyLoad -> pwrPolicyLoad_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X -> voltRailGetVoltageMin
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended',                                     '.imem_pmgrLibBoardObj', '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libSemaphoreRW', ],
                # task_pmgr -> pmuRpcProcessUnitPmgr_IMPL-> pmuRpcPmgrLoad -> s_pmgrLoadHelper -> pwrPoliciesLoad -> pwrPolicyLoad -> pwrPolicyLoad_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X -> voltRailGetVoltageMin
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage',                          '.imem_libBoardObj', '.imem_pmgrLibLoad', '.imem_libi2c', '.imem_perfVf', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfLimitVf', '.imem_libLpwr', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libSemaphoreRW', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_COMBINED_1X
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj',                                                                            '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X -> pmgrFreqMHzGet
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj',                                                                            '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_clkLibCntrSwSync', '.imem_clkLibCntr', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj',                                                                            '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyInputStateGet_WORKLOAD_SINGLE_1X -> voltRailGetVoltageSensed
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj',                                                                            '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libBoardObj', '.imem_libClkVolt', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X -> perfLimitsFreqkHzToVoltageuV
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj',                                                                            '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_perfLimitVf', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X -> perfLimitsVoltageuVToFreqkHz
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj',                                                                            '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_perfLimitVf', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> s_pwrPolicies35Filter -> pwrPolicy3XFilter -> pwrPolicy3XFilter_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyGetVoltageFloor_WORKLOAD_SINGLE_1X -> voltRailGetVoltageMin
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj',                                                                            '.imem_libVoltApi', '.imem_libClkVolt', '.imem_libLpwr', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_pmgrLibPwrPolicyWorkloadShared', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_libVoltApi', '.imem_libSemaphoreRW', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate (PWR_MODEL policies)
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj',                                                                      '.imem_libVoltApi', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libVoltApi', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_pmgrPwrPolicyPerfCfPwrModelScale', '.imem_perfLimitVf', ],
                # task_pmgr -> {callback path} -> pwrPolicies3xCallbackBody_35 -> pwrPoliciesEvaluate_3X -> pwrPoliciesEvaluate_SUPER -> s_pwrPoliciesDomGrpEvaluate -> pwrPolicyDomGrpEvaluate -> pwrPolicyDomGrpEvaluate_WORKLOAD_COMBINED_1X -> s_pwrPolicyWorkloadCombined1xComputeClkMHz -> (pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_COMBINED_1X | s_pwrPolicyWorkloadCombined1xFreqFloorStateGet) -> pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_pmgr', '.imem_libFxpBasic', '.imem_libFxpExtended', '.imem_perfVf', '.imem_libLeakage', '.imem_pmgrPwrPolicyCallback', '.imem_libPwrEquation',                                       '.imem_pmgrLibPwrPolicy', '.imem_libBoardObj',                                                                      '.imem_libVoltApi',                                       '.imem_libVoltApi', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_pmgrPwrPolicyPerfCfPwrModelScale', '.imem_perfLimitVf', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
            ],

            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-gp100...', ] => [
            #     [ '.dmem_stackPmgr', '.dmem_pmgr', '.dmem_i2cDevice', '.dmem_perf', '.dmem_perfDaemon', ],
                                                                      # pwrPolicyWorkloadMultiRailInputStateGet : VOLT_RAIL BOARDOBJGRP
                                                                      # _pwrPoliciesArbitrateAndApply : PCLK_DOMAN
                                                                      # _pwrPolicyWorkloadMultiRailScaleClocks : PCLK_DOMAIN
                                                                      # _pwrEquationLeakageDtcs12GetIddq : PVFE_VAR
                                                                      # pwrPolicyGetDomGrpLimitsByVpstateIdx : PVPSTATE
            #     [ '.dmem_stackPmgr', '.dmem_pmgr', '.dmem_i2cDevice', '.dmem_therm', ],
                                                                       # pwrEquationLeakageEvaluatemA_DTCS12 : THERM_CHANNEL BOARDOBJGRP
                                                                       # pwrViolationsLoad : THRM_MON BOARDOBJGRP
                                                                       # pwrPoliciesEvaluate : THRM_MON BOARDOBJGRP
            #     [ '.dmem_stackPmgr', '.dmem_pmgr', '.dmem_lpwr', '.dmem_perf', ], # pgPsiSleepLwrrentCallwlate : VOLT_RAIL BOARDOBJGRP
                                                                                    # psiISleepUpdate : PSI_CTRL
                                                                                    # _psiHandleOsCallback : PSI_CTRL
            # ],
        ],
    },

    PMUTASK_PERFMON =>
    {
        OSTASK_DEFINE => {
            ENTRY_FN             => "task_perfmon"       ,
            PRIORITY             => 1                    ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => [
                                       516 => DEFAULT,
                                       436 => [ GP100, ],
                                       768 => [ GP102_and_later, ],
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(perfmon)" ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackPerfmon)" ,
            TCB                  => "OsTaskPerfMon"      ,
            MAX_OVERLAYS_IMEM_LS => 5                    ,
            MAX_OVERLAYS_DMEM    => 2                    ,
            PRIV_LEVEL_EXT       => [
                                      2 => DEFAULT,
                                      0 => [ PASCAL_and_later, -GP100, ],
                                    ],
        },

        OVERLAY_COMBINATION => [
            [ '...pmu-gv10x', 'pmu-gv11b', ] => [
                [ '.imem_perfmon', '.imem_libEngUtil', '.imem_libGpumon?', '.imem_libFxpBasic?', '.imem_loggerWrite?', ],
            ],
            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-gp10x', 'pmu-gv10x', 'pmu-gv11b', ] => [
            #     [ '.dmem_stackPerfmon', '.dmem_smbpbi', ],
            # ],
        ],
    },

    PMUTASK_DISP =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_disp"       ,
            PRIORITY             => 4                 ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => [
                                        512 => DEFAULT,
                                       1280 => [ GP102_and_later, ],
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(disp)" ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackDisp)" ,
            TCB                  => "OsTaskDisp"      ,
            MAX_OVERLAYS_IMEM_LS => 6                 ,
            MAX_OVERLAYS_DMEM    => 3                 ,
            #PRIV_LEVEL_EXT       => [
            #                          2 => DEFAULT,
            #                          0 => [ PASCAL_and_later, -GP100, ],
            #                        ],
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ 'pmu-*', ],
                                    ],
        ],

        OVERLAY_COMBINATION => [
            [ 'pmu-*', ] => [
                [ '.imem_disp', '.imem_libi2c', ],
                [ '.imem_disp', '.imem_libi2c', '.imem_libRmOneshot?', ],
            ],

            [ 'pmu-gp10x...', ] => [
                [ '.imem_disp', '.imem_libi2c', ],
                [ '.imem_disp', '.imem_libi2c', '.imem_libRmOneshot?', '.imem_vbios?', '.imem_libVbios?', '.imem_dispPms?'],
            ],

            [ 'pmu-tu10x', ] => [
                [ '.imem_disp', '.imem_dispImp', '.imem_libi2c', '.imem_perfVf', ],
            ],

            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-gp100...', ] => [
            #     [ '.dmem_stackDisp', ],
            # ],
            # [ 'pmu-tu10x', ] => [
            #     [ '.dmem_stackDisp', '.dmem_lpwr', ],
            #     [ '.dmem_stackDisp', '.dmem_lpwr', '.dmem_perf', ],
            # ],
        ],
    },

    PMUTASK_THERM =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_therm"       ,
            PRIORITY             => 1                  ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => [
                                       640  => DEFAULT,
                                       768  => [ GP102_thru_VOLTA, ],
                                       1024 => [ TURING_and_later, ],
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 1,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(therm)" ,
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(therm)",
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackTherm)" ,
            TCB                  => "OsTaskTherm"      ,
            MAX_OVERLAYS_IMEM_LS => 11                 ,
            MAX_OVERLAYS_DMEM    => 6                  ,
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ '...pmu-ga100', ],
                                        "1024U" => [ 'pmu-ga10x_riscv...', ],
                                    ],
        ],

        OVERLAY_COMBINATION => [
            [ 'pmu-*', ] => [
                [ '.imem_therm', '.imem_thermLibConstruct?', '.imem_libBoardObj', ], # _thermProcessRmCmd -> boardObjGrpCmd
                [ '.imem_therm', '.imem_libBoardObj', '.imem_thermLibSensor2X?', ],
                [ '.imem_therm', '.imem_libBoardObj', '.imem_thermLibMonitor?', ],
            ],
            [ 'pmu-*', ] => [
                [ '.imem_therm', '.imem_smbpbi', '.imem_libi2c', ],
                [ '.imem_therm', '.imem_smbpbi', '.imem_libi2c', '.imem_thermLibRPC', ],
                [ '.imem_therm', '.imem_smbpbi', '.imem_libi2c', '.imem_libPwm', ],
                [ '.imem_therm', '.imem_smbpbi', '.imem_libi2c', '.imem_libPwm', '.imem_thermLibFan?', '.imem_libPwmConstruct', ],
                [ '.imem_therm', '.imem_smbpbi', '.imem_libi2c', '.imem_thermLibPolicy?', '.imem_libBoardObj', ], # thermProcessThermPolicyLoad() -> thermPoliciesUnload()
                [ '.imem_therm', '.imem_thermLibFanCommonConstruct?', '.imem_libPwmConstruct', '.imem_libBoardObj', ],
                [ '.imem_therm', '.imem_thermLibFanCommon?', '.imem_libFxpBasic?', '.imem_libPwm?', '.imem_libBoardObj', ],
                [ '.imem_therm', '.imem_libi2c', '.imem_thermLibPolicy?', '.imem_pmgrLibPwrPolicyClient?', '.imem_thermLibSensor2X?', '.imem_libBoardObj', ], # thermProcessThermPolicyLoad() -> thermPoliciesLoad()
                [ '.imem_therm', '.imem_smbpbi', '.imem_libi2c', '.imem_pmgrLibPwrPolicyClient?', ], # smbpbiDispatch() -> _smbpbiPwrPolicyIdxQuery()
            ],
            [ 'pmu-gp100...', ] => [
                [ '.imem_therm', '.imem_smbpbi', '.imem_libi2c', '.imem_thermLibTest', ],
                [ '.imem_therm', '.imem_libBoardObj', '.imem_thermLibConstruct', '.imem_pmgrLibPwrPolicyClient', ], # _thermProcessObjectSet() -> RM_PMU_THERM_OBJECT_THERM_POLICY
                [ '.imem_therm', '.imem_libi2c', '.imem_libBoardObj', '.imem_thermLibMonitor', '.imem_thermLibSensor2X', ], # _thermProcessObjectLoad -> thermMonitorsLoad || _thermProcessObjectUnload -> thermMonitorsUnload || thrmMonitorBoardObjGrpIfaceModel10GetStatus -> thrmQueryThrmMonEntry || thermChannelBoardObjGrpIfaceModel10GetStatus -> thermChannelTempValueGet
                [ '.imem_therm', '.imem_libi2c', '.imem_libBoardObj', '.imem_thermLibMonitor', '.imem_libLw64', '.imem_libLw64Umul', '.imem_thermLibSensor2X', ], # _thermProcessObjectLoad -> thermMonitorsLoad || _thermProcessObjectUnload -> thermMonitorsUnload || thrmMonitorBoardObjGrpIfaceModel10GetStatus -> thrmQueryThrmMonEntry || thermChannelBoardObjGrpIfaceModel10GetStatus -> _thermMonCountUpdate
                [ '.imem_therm', '.imem_libLw64', '.imem_libLw64Umul', '.imem_thermLibMonitor', ], # _thermMonitorTimerOsCallbackExelwte -> _thermMonCountUpdate
            ],
            [ 'pmu-*', ] => [
                [ '.imem_therm', '.imem_smbpbi', '.imem_libi2c', '.imem_thermLibSensor2X', ], # smbpbiDispatch -> thermChannelTempValueGet -> i2cDevReadReg8
                [ '.imem_therm', '.imem_libi2c', '.imem_libBoardObj', '.imem_thermLibSensor2X', ],  # thermChannelBoardObjGrpIfaceModel10GetStatus -> thermChannelTempValueGet
            ],
            [ 'pmu-gm20x...', ] => [
                [ '.imem_therm', '.imem_libi2c', '.imem_libBoardObj', '.imem_thermLibSensor2X', '.imem_thermLibPolicy', ],  # thermChannelBoardObjGrpIfaceModel10GetStatus -> thermChannelTempValueGet || thermPolicyBoardObjGrpIfaceModel10GetStatus
                [ '.imem_therm', '.imem_smbpbi', '.imem_libi2c', '.imem_libLw64?', '.imem_libLw64Umul?', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', ], # RM_PMU_THERM_CMD_ID_CLK_CNTR_SAMPLE
            ],
            [ 'pmu-gv10x...', ] => [
                [ '.imem_therm', '.imem_thermLibFanCommon', '.imem_libPwm', '.imem_libFxpBasic', '.imem_thermLibSensor2X', '.imem_libi2c', 'imem_libBoardObj', ], #fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM || fanPolicyCallback_IIR_TJ_FIXED_DUAL_SLOPE_PWM
                [ '.imem_therm', '.imem_smbpbi', '.imem_libEngUtil', ],
                [ '.imem_therm', '.imem_libi2c', '.imem_smbpbi', '.imem_thermLibSensor2X', '.imem_thermLibTest', ],
                [ '.imem_therm', '.imem_smbpbi', '.imem_smbpbiLwlink?', '.imem_libLw64?', '.imem_libBif?', ],
            ],
            [ 'pmu-tu10x', 'pmu-ga100', ] => [
                [ '.imem_therm',                 '.imem_thermLibPolicy',                                                              '.imem_libBoardObj',               '.imem_perfVf', '.imem_pmgrLibPff', '.imem_libSemaphoreRW', ], # task_therm -> thermPoliciesVfIlwalidate()
                [ '.imem_therm', '.imem_libi2c', '.imem_thermLibPolicy?', '.imem_pmgrLibPwrPolicyClient?', '.imem_thermLibSensor2X?', '.imem_libBoardObj',               '.imem_perfVf', '.imem_pmgrLibPff', '.imem_libSemaphoreRW', ], # thermProcessThermPolicyLoad() -> thermPoliciesVfIlwalidate()
                [ '.imem_therm', '.imem_pmgrLibPwrPolicyClient?', '.imem_libi2c', '.imem_thermLibPolicy?', '.imem_thermLibSensor2X?', '.imem_libBoardObj',               '.imem_perfVf', '.imem_pmgrLibPff', ], # thermPolicyCallbackExelwte with pffEvaluate
                # task_therm -> smbpbiDispatch -> smbpbiBundleLaunch? -> smbpbiHandleMsgboxRequest -> s_smbpbiGetEnergyCounterHandler -> pwrMonitorPolledTupleGet
                [ '.imem_therm', '.imem_smbpbi', '.imem_pmgrLibPwrMonitor', '.imem_libLw64', '.imem_libLw64Umul', ],
            ],
            [ 'pmu-ga100', ] => [
                [ '.imem_therm', '.imem_smbpbi', '.imem_libBoardObj', '.imem_perfVf', '.imem_libSemaphoreRW', ],
                [ '.imem_therm', '.imem_smbpbi', '.imem_libClkReadAnyTask', '.imem_libSemaphoreRW', ],
                [ '.imem_therm', '.imem_smbpbi', '.imem_thermLibSensor2X', '.imem_libFxpBasic?', ],
                [ '.imem_therm', '.imem_libBoardObj', '.imem_thermLibPolicy', '.imem_thermLibSensor2X', '.imem_libFxpBasic?', ],
                [ '.imem_therm', '.imem_thermLibFanCommon', '.imem_libPwm', '.imem_libFxpBasic', '.imem_thermLibSensor2X', '.imem_libi2c', 'imem_libBoardObj', ], #fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM || fanPolicyCallback_IIR_TJ_FIXED_DUAL_SLOPE_PWM
                [ '.imem_therm', '.imem_pmgrLibPwrPolicyClient?', '.imem_libi2c', '.imem_thermLibPolicy?', '.imem_thermLibSensor2X?', '.imem_libBoardObj',               '.imem_perfVf', '.imem_pmgrLibPff', '.imem_libFxpBasic?', ], # thermPolicyCallbackExelwte with pffEvaluate
            ],
            [ 'pmu-tu10x', 'pmu-ga100', ] => [
                [ '.imem_therm', '.imem_smbpbi', '.imem_libBif', ], # smbpbiGetPcieSpeedWidth -> bifBar0ConfigSpaceRead
            ],
            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-gp100...', ] => [
            #     [ '.dmem_stackTherm', '.dmem_therm', '.dmem_i2cDevice', '.dmem_perf', ], # thermPwrDevBaStateSync_BA13 : VOLT_RAIL BOARDOBJGRP
            #     [ '.dmem_stackTherm', '.dmem_therm', '.dmem_i2cDevice', '.dmem_pmgr', '.dmem_perf' ], # _smbpbiPwrPolicyIdxQuery : PPWR_POLICY
                                                                                                        # thermPolicyGrpIfaceModel10ObjSetImpl_DTC_PWR : PPWR_POLICY
                                                                                                        # _thermPolicyTimerOsCallbackExelwte -> pffEvaluate : PPSTATE
            #     [ '.dmem_stackTherm', '.dmem_therm', '.dmem_i2cDevice', '.dmem_smbpbi', '.dmem_pmgr', ],
            #     [ '.dmem_stackTherm', '.dmem_therm', '.dmem_i2cDevice', '.dmem_smbpbiLwlink'],
            #     [ '.dmem_stackTherm', '.dmem_therm', '.dmem_i2cDevice', '.dmem_smbpbi', '.dmem_pmgr', '.dmem_pmgrPwrChannelsStatus', ], # s_smbpbiGetEnergyCounterHandler
            # ],
        ],
    },

    PMUTASK_HDCP =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_hdcp"       ,
            PRIORITY             => 1                 ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => 1592              ,
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            MAX_OVERLAYS_IMEM_LS => 4                 ,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(hdcp)" ,
            TCB                  => "OsTaskHdcp"      ,
        ],

        OVERLAY_COMBINATION => [
            [ 'pmu-gm10x', ] => [
                [ '.imem_hdcp', '.imem_libLw64', '.imem_libLw64Umul', '.imem_sha1', ],
            ],
        ],
    },

    PMUTASK_ACR =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_acr"       ,
            PRIORITY             => 1                ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => [
                                       704 => DEFAULT,
                                       768 => [ GP102_and_later, ],
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(acr)" ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackAcr)" ,
            TCB                  => "OsTaskAcr"      ,
            MAX_OVERLAYS_IMEM_LS => 4                ,
        ],

        OVERLAY_COMBINATION      => [
            [ 'pmu-gm20*', 'pmu-gp100', 'pmu-gv11b', ] => [
                [ '.imem_acr', '.imem_libAcr', ],
            ],
            [ 'pmu-gp10x...', '-pmu-gv11b', ] => [
                [ '.imem_acr', '.imem_libAcr', ],
                [ '.imem_acr', '.imem_libAcr', '.imem_libLw64', '.imem_libLw64Umul', ],
            ],

            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-gp100...', ] => [
            #     [ '.dmem_stackAcr', ],
            # ],
        ],
    },

    PMUTASK_PERF =>
    {
        OSTASK_DEFINE => [
            ENTRY_FN             => "task_perf"            ,
            PRIORITY             => 1                      ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => [
                                       1280 => DEFAULT,
                                       1292 => [ GP100, ],
                                       2816 => [ GP102_thru_GV10X, ],
                                       2048 => [ TU10X_and_later, ],
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 1,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(perf)" ,
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(perf)" ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackPerf)" ,
            TCB                  => "OsTaskPerf"           ,
            MAX_OVERLAYS_IMEM_LS => 17                     ,
            MAX_OVERLAYS_DMEM    => 7                      ,
            USING_FPU            => [
                                        "LW_FALSE" => DEFAULT ,
                                        "LW_TRUE"  => [ AMPERE_and_later, -GA100, ] ,
                                    ],
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ '...pmu-ga100', ],
                                        "2048U" => [ 'pmu-ga10x_riscv...', ],
                                    ],
        ],

        OVERLAY_COMBINATION => [
            # Pstates 3.0-specific overlay combinations
            [ 'pmu-gp100...pmu-gv10x', ] => [
                # CLK PATHS - BEGIN
                # task_perf pmuRpcProcessUnitClk_IMPL pmuRpcClkVfChangeInject clkVfChangeInject
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_perfClkAvfs', '.imem_libVoltApi', '.imem_perfVfe', '.imem_perfVfeEquMonitor', 'imem_libLwF32', '.imem_libClkVolt', 'imem_libClkFreqController', 'imem_libClkFreqCountedAvg', 'imem_libLw64', 'imem_libLw64Umul', 'imem_clkLibCntr', 'imem_clkLibCntrSwSync', ],
                # CLK PATHS - END
            ],

            # Pstates 3.X overlay combinations
            [ 'pmu-gp100...', ] => [
                # VF LOAD - task_perf pmuRpcClkLoad clkDomainsLoad clkDomainLoad clkDomainLoad_35_Primary clkProg3PrimaryLoad clkVfPointLoad clkVfPointLoad_35_VOLT voltRailRoundVoltage voltDeviceRoundVoltage __Mod
                # VF LOAD - task_perf pmuRpcClkLoad clkDomainsLoad clkDomainLoad clkDomainLoad_35_Primay clkProg35PrimaryLoad clkVfPointLoad clkVfPointLoad_35_FREQ boardObjInterfaceGetBoardObjFromInterface
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_vfLoad', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libPerfInit?', '.imem_libInit', ],  # Overlay combination for perf post initialization.
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libVoltConstruct', '.imem_libPwmConstruct', '.imem_libVolt'], # RM_PMU_VOLT_CMD_ID_BOARDOBJ_GRP_SET
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libVolt', ], # RM_PMU_RPC_ID_VOLT_LOAD || RM_PMU_VOLT_RPC_ID_VOLT_POLICY_SANITY_CHECK
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libVolt', '.imem_libPwm', ],  # RM_PMU_VOLT_RPC_ID_VOLT_POLICY_SET_VOLTAGE
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libVolt', '.imem_libVoltApi', '.imem_libSemaphoreRW?', ], #RM_PMU_RPC_ID_VOLT_VOLT_GET_VOLTAGE
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libVolt', '.imem_libPwm', ],  # RM_PMU_VOLT_CMD_ID_BOARDOBJ_GRP_GET_STATUS

                # VFE paths - BEGIN
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfIlwalidation', '.imem_libVolt',  ],       # _vfeTimerCallback() -> voltProcessDynamilwpdate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfIlwalidation', '.imem_perfVfIlwalidation', ], # _vfeTimerCallback() -> preClkIlwalidate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfIlwalidation', '.imem_perfClkAvfs', '.imem_libLpwr?', ],    # _vfeTimerCallback() -> postClkIlwalidate()
                # VFE paths - END

                # PSTATE paths - BEGIN
                # pstateBoardObjGrpIfaceModel10Set()/pstateBoardObjGrpIfaceModel10GetStatus()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj', '.imem_libPstateBoardObj', ],
                # PSTATE paths - END

                # CLK paths - BEGIN
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_perfClkAvfsInit', '.imem_perfClkAvfs', '.imem_libClkConstruct', ], # RM_PMU_BOARDOBJ_CMD_GRP SET
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_perfClkAvfs', '.imem_libVoltApi', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_perfClkAvfs', '.imem_libClkStatus', ], # RM_PMU_BOARDOBJ_CMD_GRP GET_STATUS
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj', '.imem_perfVfeBoardObj', '.imem_libLwF32', '.imem_libLw64Umul', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj', '.imem_perfVfeBoardObj', ], # RM_PMU_BOARDOBJ_CMD_GRP_GET_STATUS
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj', '.imem_libVpstateConstruct?', '.imem_perfVfeBoardObj', '.imem_libLwF32', '.imem_libLw64Umul', ], # RM_PMU_BOARDOBJ_CMD_GRP SET
                # CLK paths - END
            ],

            # Pstates 3.X Non-Auto overlay combinations
            [ 'pmu-gp100...', ] => [
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libClkEffAvg', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libLw64', '.imem_libLw64Umul', ], # _clkFreqEffAvgCallback
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libClkFreqController', '.imem_libClkFreqCountedAvg?', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', ], # LW_RM_PMU_CLK_LOAD_FEATURE_FREQ_CONTROLLER
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libClkFreqController', '.imem_libFxpBasic', '.imem_libClkFreqCountedAvg?', '.imem_perfClkAvfs?', '.imem_libVoltApi', '.imem_libVolt', '.imem_libPwm', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', ], # _clkFreqControllerCallback
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libLwF32', '.imem_libLw64Umul', '.imem_thermLibSensor2X', 'imem_libi2c', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libVolt', '.imem_libGpio', ], # RM_PMU_VOLT_RPC_ID_VOLT_POLICY_SET_VOLTAGE
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libVolt', '.imem_libGpio', ], # RM_PMU_VOLT_CMD_ID_BOARDOBJ_GRP_GET_STATUS
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libClkVolt', '.imem_perfClkAvfsInit', '.imem_perfClkAvfs', '.imem_libClkConstruct', ], # RM_PMU_BOARDOBJ_CMD_GRP SET
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libClkVolt', '.imem_perfClkAvfs', '.imem_libVoltApi', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libClkVolt', ], # _clkAdcVoltageSampleCallback
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_perfClkAvfs', '.imem_libClkStatus', '.imem_libClkVolt', '.imem_libLpwr', ], # RM_PMU_BOARDOBJ_CMD_GRP GET_STATUS
            ],

            # Pstates 3.5 overlay combinations
            [ 'pmu-tu10x...pmu-ga100', ] => [
                # CHANGE_SEQ paths - BEGIN
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libChangeSeq', ],  # Change sequencer requires BoardObj libs, exclusion/inclusion mask interop require it
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW',                       '.imem_changeSeqLoad', '.imem_libVoltApi', '.imem_perfClkAvfs', ], # Change sequence LOAD
                # task_perf perfChangeSeqScriptCompletion _perfChangeSeqProcessPendingChange clkFreqControllersLoad_WRAPPER
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libChangeSeq', '.imem_clkLibCntr?', '.imem_clkLibCntrSwSync?', '.imem_libClkFreqController?', '.imem_libClkFreqCountedAvg?', ],
                # CHANGE_SEQ paths - END

                # VFE paths - BEGIN
                # # vfe<XYZ>BoardObjGrpSet()? -> _vfeTimerCallback() -> vfeVarSampleAll()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj?', '.imem_perfVfe',  '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfIlwalidation', '.imem_thermLibSensor2X', '.imem_libi2c?', '.imem_libFxpBasic?', ],
                # vfe<XYZ>BoardObjGrpSet()? -> _vfeTimerCallback() -> preClkIlwalidate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj?', '.imem_perfVfe',  '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfIlwalidation', '.imem_perfVfIlwalidation', ],
                # vfe<XYZ>BoardObjGrpSet()? -> _vfeTimerCallback() -> preClkIlwalidate() -> clkAdcsIlwalidate() -> VFE
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj?', '.imem_perfVfe',  '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfIlwalidation', '.imem_perfClkAvfs', ],
                # vfe<XYZ>BoardObjGrpSet()? -> _vfeTimerCallback() -> postClkIlwalidate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj?', '.imem_perfVfe',  '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfIlwalidation', '.imem_perfClkAvfs', '.imem_libLpwr?', '.imem_clkLibClk3', ],
                # vfe<XYZ>BoardObjGrpSet()? -> _vfeTimerCallback() -> voltProcessDynamilwpdate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj?', '.imem_perfVfe',  '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfIlwalidation', '.imem_libVolt', '.imem_libPwm?', '.imem_libGpio?', ],
                # VFE paths - END

                # CLK paths - BEGIN
                # clk<XYZ>BoardObjGrpSet() -> clkDomainsLoad()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libClkConstruct', '.imem_vfLoad', ],
                # clk<XYZ>BoardObjGrpSet() -> clkIlwalidate() -> preClkIlwalidate -> clkDomainsCache()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libClkConstruct', '.imem_perfIlwalidation', '.imem_perfVfIlwalidation', ],
                # This path won't actually evaluate VFE code, but must add dummy rule here to pacify Analyze.pm:
                # clk<XYZ>BoardObjGrpSet() -> clkIlwalidate() -> preClkIlwalidate -> clkDomainsCache() -> VFE
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libClkConstruct', '.imem_perfIlwalidation', '.imem_perfVfIlwalidation', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', ],
                # clk<XYZ>BoardObjGrpSet() -> clkIlwalidate() -> preClkIlwalidate -> clkAdcsIlwalidate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libClkConstruct', '.imem_perfIlwalidation', '.imem_perfClkAvfs', ],
                # This path won't actually evaluate VFE code, but must add dummy rule here to pacify Analyze.pm:
                # clk<XYZ>BoardObjGrpSet() -> clkIlwalidate() -> preClkIlwalidate -> clkAdcsIlwalidate() -> VFE
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libClkConstruct', '.imem_perfIlwalidation', '.imem_perfClkAvfs', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', ],
                # clk<XYZ>BoardObjGrpSet() -> clkIlwalidate() -> postClkIlwalidate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libInit', '.imem_libClkConstruct', '.imem_perfIlwalidation', '.imem_perfClkAvfs', '.imem_libLpwr?', '.imem_clkLibClk3', ],
                # CLK paths - END

                # VOLT paths - BEGIN
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libVoltConstruct', '.imem_libPwmConstruct', '.imem_libVolt', '.imem_libSemaphoreRW?', ], # RM_PMU_VOLT_CMD_ID_BOARDOBJ_GRP_SET
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libVolt', '.imem_libPwm', 'imem_libSemaphoreRW?', ],  # RM_PMU_VOLT_RPC_ID_VOLT_POLICY_SET_VOLTAGE || RM_PMU_RPC_ID_VOLT_VOLT_GET_VOLTAGE
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libVolt', '.imem_libPwm', '.imem_libSemaphoreRW?', ],  # RM_PMU_VOLT_CMD_ID_BOARDOBJ_GRP_GET_STATUS
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libVolt', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libSemaphoreRW?', ],
                # VOLT paths - END
            ],

            # Pstates 3.5 Non-Auto Overlay Combinations
            [ 'pmu-tu10x', 'pmu-ga100', ] => [
                # PERF_LIMIT paths  - BEGIN
                # perfPostInit() -> perfLimitsConstruct()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libPerfInit',                                                                                     '.imem_libPerfLimit', '.imem_perfVfeEquMonitor', ],
                # perfLimitBoardObjGrpIfaceModel10Set() -> perfLimitsArbitrateAndApply() -> perfLimitsArbitrate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj', '.imem_libPerfLimitBoardobj',                           '.imem_libPerfLimit', '.imem_perfVfeEquMonitor', '.imem_perfVfe', '.imem_libLw64Umul', '.imem_libLwF32', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj', '.imem_libPerfLimitBoardobj',                           '.imem_libPerfLimit', '.imem_perfVfeEquMonitor', '.imem_perfLimitArbitrate', '.imem_perfLimitVf', '.imem_libPerfPolicy', ],
                # perfLimitBoardObjGrpIfaceModel10Set() -> perfLimitsArbitrateAndApply() -> perfChangeSeqQueueChange()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj', '.imem_libPerfLimitBoardobj',                           '.imem_libPerfLimit',                            '.imem_libChangeSeq',  '.imem_libClkFreqController?', '.imem_clkLibCntr?', '.imem_libClkFreqCountedAvg?', '.imem_libLw64?', '.imem_libLw64Umul', '.imem_clkLibCntrSwSync?', ],
                # perfLimitBoardObjGrpIfaceModel10GetStatus()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj', '.imem_libPerfLimitBoardobj',                           '.imem_libPerfLimit', '.imem_perfVfeEquMonitor', ],
                # (perfLimitsLoad() | PERF_EVENT_ID_PERF_LIMITS_CLIENT) -> perfLimitsArbitrateAndApply() -> perfLimitsArbitrate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW',                                                                                  '.imem_libPerfLimit', '.imem_perfVfeEquMonitor',  '.imem_perfVfe', '.imem_libLw64Umul', '.imem_libLwF32', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW',                                                                                  '.imem_libPerfLimit', '.imem_perfVfeEquMonitor',  '.imem_perfLimitArbitrate', '.imem_perfLimitVf',, '.imem_libPerfPolicy', ],
                # (perfLimitsLoad() | PERF_EVENT_ID_PERF_LIMITS_CLIENT) -> perfLimitsArbitrateAndApply() -> perfChangeSeqQueueChange()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW',                                                                                  '.imem_libPerfLimit',                             '.imem_libChangeSeq',  '.imem_libClkFreqController?', '.imem_clkLibCntr?', '.imem_libClkFreqCountedAvg?', '.imem_libLw64?', '.imem_libLw64Umul', '.imem_clkLibCntrSwSync?', ],
                # pmuRpcPerfPerfLimitsIlwalidate() -> perfLimitsIlwalidate() -> perfLimitsArbitrateAndApply() -> vfeEquMonitorUpdate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW',                                                        '.imem_perfIlwalidation', '.imem_libPerfLimit', '.imem_perfVfeEquMonitor', '.imem_perfVfe', '.imem_libLw64Umul', '.imem_libLwF32', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW',                                                        '.imem_perfIlwalidation', '.imem_libPerfLimit', '.imem_perfVfeEquMonitor', '.imem_perfLimitArbitrate', '.imem_perfLimitVf',, '.imem_libPerfPolicy', ],
                # pmuRpcPerfPerfLimitsIlwalidate() -> perfLimitsIlwalidate() -> perfLimitsArbitrateAndApply() -> perfChangeSeqQueueChange()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW',                                                        '.imem_perfIlwalidation', '.imem_libPerfLimit',                             '.imem_libChangeSeq',  '.imem_libClkFreqController?', '.imem_clkLibCntr?', '.imem_libClkFreqCountedAvg?', '.imem_libLw64?', '.imem_libLw64Umul', '.imem_clkLibCntrSwSync?', ],
                # PERF_LIMIT paths  - END

                # PERF_POLICY paths - BEGIN
                # perfPostInit() -> perfPoliciesConstruct()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libPerfInit', ],
                # (perfPolicyBoardObjGrpIfaceModel10Set() | perfPolicyBoardObjGrpIfaceModel10GetStatus())
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libPerfBoardObj', '.imem_libPerfPolicyBoardObj', ],
                # PERF_POLICY paths - END

                # VFE paths - BEGIN
                # vfe<XYZ>BoardObjGrpSet()? -> _vfeTimerCallback() -> postClkIlwalidate() -> pstatesIlwalidate() -> perfLimitsIlwalidate() -> vfeEquMonitorUpdate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj?', '.imem_perfVfe',  '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfIlwalidation', '.imem_libPerfLimit', ],
                # vfe<XYZ>BoardObjGrpSet()? -> _vfeTimerCallback() -> postClkIlwalidate() -> pstatesIlwalidate() (Detaches VFE overlays) -> perfLimitsIlwalidate() -> perfLimitsArbitrateAndApply() -> perfLimitsArbitrate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj?',                                                                                     '.imem_perfIlwalidation', '.imem_libPerfLimit', '.imem_perfVfeEquMonitor', '.imem_perfVfe', '.imem_libLw64Umul', '.imem_libLwF32', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj?',                                                                                     '.imem_perfIlwalidation', '.imem_libPerfLimit', '.imem_perfVfeEquMonitor', '.imem_perfLimitArbitrate', '.imem_perfLimitVf',, '.imem_libPerfPolicy', ],
                # vfe<XYZ>BoardObjGrpSet()? -> _vfeTimerCallback() -> postClkIlwalidate() -> pstatesIlwalidate() (Detaches VFE overlays) -> perfLimitsIlwalidate() -> perfLimitsArbitrateAndApply() -> perfChangeSeqQueueChange()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj?',                                                                                     '.imem_perfIlwalidation', '.imem_libPerfLimit', '.imem_libChangeSeq', '.imem_libClkFreqController?', '.imem_clkLibCntr?', '.imem_libClkFreqCountedAvg?', '.imem_libLw64?', '.imem_libLw64Umul', '.imem_clkLibCntrSwSync?', ],
                # VFE paths - END

                # PSTATE paths - BEGIN
                # pstateBoardObjGrpIfaceModel10Set() -> perfPstatesIlwalidate() - > perfLimitsIlwalidate -> perfLimitsArbitrateAndApply() -> perfLimitsArbitrate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj', '.imem_libPstateBoardObj', '.imem_perfIlwalidation', '.imem_libPerfLimit', '.imem_perfVfeEquMonitor', '.imem_perfVfe', '.imem_libLw64Umul', '.imem_libLwF32', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj', '.imem_libPstateBoardObj', '.imem_perfIlwalidation', '.imem_libPerfLimit', '.imem_perfVfeEquMonitor', '.imem_perfLimitArbitrate', '.imem_perfLimitVf',, '.imem_libPerfPolicy', ],
                # pstateBoardObjGrpIfaceModel10Set() -> perfPstatesIlwalidate() - > perfLimitsIlwalidate -> perfLimitsArbitrateAndApply() -> perfChangeSeqQueueChange()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj', '.imem_libPstateBoardObj', '.imem_perfIlwalidation', '.imem_libPerfLimit',                             '.imem_libChangeSeq',  '.imem_libClkFreqController?', '.imem_clkLibCntr?', '.imem_libClkFreqCountedAvg?', '.imem_libLw64?', '.imem_libLw64Umul', '.imem_clkLibCntrSwSync?', ],
                # PSTATE paths - END

                # CLK paths - BEGIN
                # clk<XYZ>BoardObjGrpSet() -> clkIlwalidate() -> postClkIlwalidate() -> perfPstatesIlwalidate() -> perfLimitsArbitrateAndApply() -> perfLimitsArbitrate()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libClkConstruct', '.imem_perfIlwalidation', '.imem_libPerfLimit', '.imem_perfVfeEquMonitor', '.imem_perfVfe', '.imem_libLw64Umul', '.imem_libLwF32', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libClkConstruct', '.imem_perfIlwalidation', '.imem_libPerfLimit', '.imem_perfVfeEquMonitor', '.imem_perfLimitArbitrate', '.imem_perfLimitVf',, '.imem_libPerfPolicy', ],
                # clk<XYZ>BoardObjGrpSet() -> clkIlwalidate() -> postClkIlwalidate() -> perfPstatesIlwalidate() -> perfLimitsArbitrateAndApply() -> perfChangeSeqQueueChange()()
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libClkConstruct', '.imem_perfIlwalidation', '.imem_libPerfLimit',                            '.imem_libChangeSeq',  '.imem_libClkFreqController?', '.imem_clkLibCntr?', '.imem_libClkFreqCountedAvg?', '.imem_libLw64?', '.imem_libLw64Umul', '.imem_clkLibCntrSwSync?', ],
                # task_perf clkPostInit clkNafllsCacheGpcsNafllInitSettings_GA10X
                [ '.imem_perf', '.imem_libPerfInit', '.imem_libInit', '.imem_clkLpwr?', ],
                # CLK paths - END

                # CLK_CONTROLLER - BEGIN
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libClkFreqController', '.imem_libFxpBasic', '.imem_libClkFreqCountedAvg?', '.imem_perfClkAvfs?', '.imem_libVolt', '.imem_libPwm', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libSemaphoreRW?', ], # _clkFreqControllerCallback
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libClkFreqController', '.imem_libFxpBasic', '.imem_libClkFreqCountedAvg?', '.imem_perfClkAvfs?', '.imem_libVoltApi', '.imem_libVolt', '.imem_libPwm', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_clkLibClk3?', '.imem_libSemaphoreRW?', ], # _clkFreqControllerCallback
                # CLK_CONTROLLER - END

                # CLK 3.X - BEGIN
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_clkLibCntr', '.imem_libSemaphoreRW', '.imem_libClkConstruct', '.imem_libClkReadPerfDaemon', '.imem_clkLibCntrSwSync', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_clkLibCntr', '.imem_libSemaphoreRW', '.imem_libClkConstruct', '.imem_libClkReadPerfDaemon', '.imem_libLw64', '.imem_libLw64Umul', ],
                [ '.imem_perfClkAvfs', '.imem_clkLibCntr', '.imem_libLw64Umul', '.imem_libClkReadPerfDaemon', '.imem_clkLibClk3', '.imem_libSemaphoreRW', ], # pmuRpcClkFreqDomainRead
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libClkVolt', '.imem_perfClkAvfs', '.imem_libClkConstruct', '.imem_libClkReadPerfDaemon', '.imem_clkLibClk3', '.imem_libSemaphoreRW', ],
                # CLK 3.X - END

                # VOLT paths - BEGIN
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libVolt', '.imem_libGpio', 'imem_libSemaphoreRW?', ], # RM_PMU_VOLT_RPC_ID_VOLT_POLICY_SET_VOLTAGE || RM_PMU_RPC_ID_VOLT_VOLT_GET_VOLTAGE
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libVolt', '.imem_libGpio', '.imem_libSemaphoreRW?', ], # RM_PMU_VOLT_CMD_ID_BOARDOBJ_GRP_GET_STATUS
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libVolt', '.imem_libVoltApi', '.imem_libClkVolt', ], # RM_PMU_VOLT_CMD_ID_BOARDOBJ_GRP_GET_STATUS
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libVolt', '.imem_libVoltTest', ], # RM_PMU_RPC_ID_VOLT_TEST_EXELWTE
                # VOLT paths - END
            ],

            # GV10X changes to support sensed voltage?
            [ 'pmu-gv10x...', ] => [
                # CLK_CONTROLLER - BEGIN
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libClkFreqController', '.imem_libFxpBasic', '.imem_libClkFreqCountedAvg?', '.imem_perfClkAvfs?', '.imem_libVolt', '.imem_libPwm', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_clkLibClk3?', ], # _clkFreqControllerCallback
                # CLK_CONTROLLER - END
            ],

            # Pstates 3.7 - Mostly CLVC
            [ 'pmu-ga100', ] => [
                # CLK_CONTROLLER - BEGIN
                # clkFreqControllerOsCallback_20
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libClkFreqController', '.imem_libFxpBasic', '.imem_libClkFreqCountedAvg?', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libChangeSeq', ],

                # clk<XYZ>BoardObjGrpSet() -> clkIlwalidate() -> preClkIlwalidate -> clkAdcsComputeCodeOffset -> clkAdcComputeCodeOffset_V30 -> voltRailGetVoltage
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libClkConstruct', '.imem_perfIlwalidation', '.imem_perfClkAvfs', '.imem_libVoltApi', ],

                # vfe<XYZ>BoardObjGrpSet()? -> _vfeTimerCallback() -> preClkIlwalidate() -> clkAdcsComputeCodeOffset -> clkAdcComputeCodeOffset_V30 -> voltRailGetVoltage
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libSemaphoreRW', '.imem_libPerfBoardObj?', '.imem_perfVfe',  '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfIlwalidation', '.imem_perfClkAvfs', '.imem_libVoltApi', ],

                # PP-TODO Following rules needs to be updated. The code path is detaching the overlays of parent interfaces to fit the new required overlays for child interfaces but the script is not able to understand such hand tuning.
                [ '.imem_libBoardObj', '.imem_libChangeSeq', '.imem_libPerfBoardObj', '.imem_libVolt', '.imem_libVoltApi', '.imem_perf', '.imem_perfIlwalidation', '.imem_perfVf', ],
                [ '.imem_libBoardObj', '.imem_libChangeSeq', '.imem_libSemaphoreRW', '.imem_libVolt', '.imem_libVoltApi', '.imem_libVoltConstruct', '.imem_perf', ],
                [ '.imem_libBoardObj', '.imem_libChangeSeq', '.imem_libVolt', '.imem_libVoltApi', '.imem_libVoltConstruct', '.imem_perf', '.imem_perfVf', ],
                [ '.imem_libBoardObj', '.imem_libChangeSeq', '.imem_libPerfBoardObj', '.imem_libSemaphoreRW', '.imem_libVolt', '.imem_libVoltApi', '.imem_perf', '.imem_perfIlwalidation', ],
                [ '.imem_libBoardObj', '.imem_libChangeSeq',                         '.imem_libPerfBoardObj', '.imem_libPerfLimit', '.imem_libVolt',        '.imem_libVoltApi', '.imem_perf', '.imem_perfIlwalidation', ],
                [ '.imem_libBoardObj', '.imem_libChangeSeq', '.imem_libClkConstruct',                         '.imem_libPerfLimit', '.imem_libVolt',        '.imem_libVoltApi', '.imem_perf', '.imem_perfIlwalidation', ],
                [ '.imem_libBoardObj', '.imem_libChangeSeq', '.imem_libClkConstruct',                         '.imem_libPerfLimit', '.imem_libSemaphoreRW', '.imem_libVoltApi', '.imem_perf', '.imem_perfIlwalidation', ],
                [ '.imem_libBoardObj', '.imem_libChangeSeq',                         '.imem_libPerfBoardObj', '.imem_libPerfLimit', '.imem_libSemaphoreRW', '.imem_libVoltApi', '.imem_perf', '.imem_perfIlwalidation', ],
                [ '.imem_libBoardObj', '.imem_libChangeSeq',                         '.imem_libPerfBoardObj', '.imem_libPerfLimit', '.imem_perfVf',         '.imem_libVoltApi', '.imem_perf', '.imem_perfIlwalidation', ],
                [ '.imem_libBoardObj', '.imem_libChangeSeq', '.imem_libClkConstruct',                         '.imem_libPerfLimit', '.imem_perfVf',         '.imem_libVoltApi', '.imem_perf', '.imem_perfIlwalidation', ],
                # task_perf osTmrCallbackExelwte s_clkControllerOsCallback perfChangeSeqQueueVoltOffset (Detach imem_libClkController) perfChangeSeqProcessPendingChange_IMPL perfChangeSeqAdjustVoltOffsetValues voltVoltOffsetRangeGet voltRailOffsetRangeGet voltRailRoundVoltage_IMPL boardObjGrpObjGet
                [ '.imem_libChangeSeq', '.imem_libClkController', '.imem_libVoltApi', '.imem_perf', '.imem_perfVf', ],
                # task_perf osTmrCallbackExelwte s_clkControllerOsCallback perfChangeSeqQueueVoltOffset (Detach imem_libClkController) perfChangeSeqProcessPendingChange_IMPL perfChangeSeqAdjustVoltOffsetValues voltVoltOffsetRangeGet voltVoltSanityCheck_IMPL voltRailSanityCheck boardObjGrpObjGet
                [ '.imem_libChangeSeq', '.imem_libClkController', '.imem_libVolt', '.imem_libVoltApi', '.imem_perf', ],
                # task_perf osTmrCallbackExelwte s_clkControllerOsCallback perfChangeSeqQueueVoltOffset (Detach imem_libClkController) perfChangeSeqProcessPendingChange_IMPL perfChangeSeqAdjustVoltOffsetValues voltVoltOffsetRangeGet voltRailOffsetRangeGet voltRailGetVoltageMin osSemaphoreRWTakeRD lwrtosSemaphoreTakeWaitForever lwrtosSemaphoreTake lwrtosQueueReceive
                [ '.imem_libChangeSeq', '.imem_libClkController', '.imem_libSemaphoreRW', '.imem_libVoltApi', '.imem_perf', ],
                [ '.imem_perf', '.imem_libBoardObj', '.imem_libChangeSeq', '.imem_libClkConstruct', '.imem_libClkController', '.imem_libClkVoltController', '.imem_libVoltApi', '.imem_libVolt', '.imem_perfVf', '.imem_libSemaphoreRW', ],

                # PP-TODO Following rules needs to be updated after review with @Sindhus on design/implementation of clock controllers.
                # Clock controllers Init
                [ '.imem_perf', '.imem_libPerfInit', '.imem_libAvg', ],
                # Clock controllers changeseq update
                [ '.imem_perf', '.imem_libChangeSeq', '.imem_libClkController', '.imem_libClkFreqController', '.imem_libBoardObj', ],
                # Clock frequency controllers reset
                ['.imem_perf', '.imem_libChangeSeq', '.imem_libClkController', '.imem_libClkFreqController', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libClkFreqCountedAvg', '.imem_libLw64', '.imem_libLw64Umul', ],
                # Clock voltage controllers callback
                [ '.imem_perf', '.imem_libBoardObj', '.imem_libClkController', '.imem_libClkVoltController', '.imem_libAvg', '.imem_libFxpBasic', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libClkVolt', '.imem_libVoltApi', '.imem_perfVf', ],
                # CLK_CONTROLLER - END
            ],


            #
            # ADD NO MANUAL/ACTUAL RULES BELOW THIS POINT!!!
            #
            # These are garbage rules which were added to unblock when
            # Analyze.pm was deployed.  They have not been manually
            # verified to match the overlay attachments in our code,
            # and could likely be hiding failures.  They need to be
            # fixed, but at the very least they should be segregated
            # from the correct/actual rules above.
            #
            # Added to bypass errors in automated rule checker based
            # on the static code analysis. Fix ASAP !!!
            #
            [ 'pmu-gp100...', ] => [
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libPwm', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libVolt', '.imem_perfIlwalidation', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libVolt',, '.imem_perfClkAvfs'],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libVolt', '.imem_libVoltConstruct',],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfVfe', '.imem_thermLibSensor2X'],
            ],
            [ 'pmu-gp100...', ] => [
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libi2c', '.imem_thermLibSensor2X', '.imem_perfIlwalidation', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfClkAvfs', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libLpwr?', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libClkFreqController', '.imem_libLpwr?', ],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libClkFreqController', '.imem_libGpio', '.imem_libVolt',],
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libGpio', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libVolt', '.imem_perfIlwalidation', ],
            ],
            [ 'pmu-gp10x', 'pmu-tu10x', ] => [
                [ '.imem_perf', '.imem_perfVf', '.imem_libBoardObj', '.imem_libClkConstruct', '.imem_libLpwr', '.imem_perfClkAvfs',],
            ],

            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-gp100...', ] => [
            #     [ '.dmem_stackPerf', '.dmem_perf', '.dmem_pmgr', ], # voltRailGetLeakage : PPWR_EQUATION
            #     [ '.dmem_stackPerf', '.dmem_perf', '.dmem_perfVfe', ], # vfeEquBoardObjGrpIfaceModel10Set vfeVarBoardObjGrpIfaceModel10Set vfeVarBoardObjGrpIfaceModel10GetStatus
            #     [ '.dmem_stackPerf', '.dmem_perf', '.dmem_perfVfe', '.dmem_therm', ], # vfeVarCache_SINGLE_SENSED_TEMP : THERM_CHANNEL BOARDOBJGRP
            #     [ '.dmem_stackPerf', '.dmem_perf', '.dmem_perfDaemon', ], # PERF CHANGE SEQUENCE ALL Interfaces.
            #     [ '.dmem_stackPerf', '.dmem_perf', '.dmem_perfDaemon', '.dmem_perfLimit', '.dmem_perfLimitChangeSeqChange', ], # PERF ARBITER queuing change to PERF CHANGE SEQ. TODO : Use scratch overlay to remove dependency.
            # ],
        ],
    },

    PMUTASK_PERF_DAEMON =>
    {
        OSTASK_DEFINE => {
            ENTRY_FN         => "task_perf_daemon"       ,
            PRIORITY         => 1                       ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
                STACK_SIZE          => [
                                            2304 => DEFAULT,
                                       ],
            WATCHDOG_TIMEOUT_US     => [
                                           0 => DEFAULT, # System-defined Timeout
                                       ],
            WATCHDOG_TRACKING_ENABLED => 1,
            OVERLAY_IMEM            => "OVL_INDEX_IMEM(perfDaemon)"  ,
            OVERLAY_DMEM            => "OVL_INDEX_DMEM(perfDaemon)"  ,
            OVERLAY_STACK           => "OVL_INDEX_DMEM(stackPerfDaemon)" ,
            TCB                     => "OsTaskPerfDaemon"            ,
            MAX_OVERLAYS_IMEM_LS    => 15                            ,
            MAX_OVERLAYS_DMEM       => 7                             ,
            USING_FPU            => [
                                        "LW_FALSE" => DEFAULT ,
                                        "LW_TRUE"  => [ AMPERE_and_later, -GA100, ] ,
                                    ],
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ 'pmu-*', ],
                                    ],
        },

        OVERLAY_COMBINATION => [
            [ 'pmu-tu10x...pmu-ga100', ] => [
                # Permanently attached '.imem_perfDaemon',  '.imem_perf', '.imem_libBoardObj', '.imem_perfVf',

                # _perfDaemonChangeSeqBuildActiveClkDomainsMask
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfDaemonChangeSeq', '.imem_libVolt', '.imem_perfClkAvfs', '.imem_libSemaphoreRW', ],

                # _perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS -> clkNafllGrpDvcoMinCompute
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfDaemonChangeSeq', '.imem_libVolt', '.imem_perfClkAvfs', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_libSemaphoreRW', ],

                # _perfDaemonChangeSeqScriptExelwteStep_VOLT -> voltVoltSetVoltage
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfDaemonChangeSeq', '.imem_libVolt', '.imem_perfClkAvfs', '.imem_clkLibClk3?', '.imem_libPwm',  '.imem_libSemaphoreRW', ],

                # At the beginning of Perf change in function 'perfDaemonChangeSeqScriptExelwte', imem overlays for perf, libVolt and perfClkAvfs are also attached
                # _perfDaemonChangeSeqScriptExelwteStep_BIF -> bifPerfChangeStep
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfDaemonChangeSeq', '.imem_libVolt', '.imem_perfClkAvfs',  '.imem_clkLibClk3?', '.imem_libBif?', ],

                # _perfDaemonChangeSeq35ScriptExelwteStep_CLKS -> clkAdcsProgram
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfDaemonChangeSeq', '.imem_libVolt', '.imem_perfClkAvfs',  '.imem_clkLibClk3?', ],
            ],

            [ 'pmu-gv10x...', ] => [
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfDaemonChangeSeq?', ],
            ],

            [ 'pmu-tu10x', 'pmu-ga100', ] => [
                # _perfDaemonChangeSeq35ScriptExelwteStep_CLKS -> Config and Program overlay combinations
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_perfDaemonChangeSeq', '.imem_perfClkAvfs', '.imem_libClkReadPerfDaemon', '.imem_clkLibClk3', '.imem_libSemaphoreRW', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libClkFreqController', '.imem_libClkFreqCountedAvg', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libClkConfig', ],
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_perfDaemonChangeSeq', '.imem_perfClkAvfs', '.imem_libClkReadPerfDaemon', '.imem_clkLibClk3', '.imem_libSemaphoreRW', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libClkFreqController', '.imem_libClkFreqCountedAvg', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libClkProgram', ],

                # task_perf_daemon pmuRpcProcessUnitClkFreq_IMPL pmuRpcClkFreqDomainProgram clkProgramDomainList_PerfDaemon clkProgram_MclkFreqDomain clkTriggerMClkSwitchOnFbflcn dispProgramPreMClkSwitchImpSettings_TU10X
                # dispImp is optional for displayless chips - GA100
                [ '.imem_perfDaemon', '.imem_perfDaemonChangeSeq', '.imem_clkLibClk3', '.imem_dispImp?', '.imem_libClkProgram', ],

                # _perfDaemonChangeSeqScriptExelwteStep_VOLT -> voltVoltSetVoltage
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_perfDaemonChangeSeq', '.imem_libBoardObj', '.imem_libVolt', '.imem_perfClkAvfs', '.imem_clkLibClk3?', '.imem_libGpio', '.imem_libSemaphoreRW', ],

                # s_perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKS
                # s_perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKS
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_perfDaemonChangeSeq', '.imem_libClkReadPerfDaemon', '.imem_clkLibClk3', '.imem_libBif',  '.imem_libClkConfig', ],
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_perfDaemonChangeSeq', '.imem_libClkReadPerfDaemon', '.imem_perfClkAvfs', '.imem_clkLibClk3', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libClkFreqController', '.imem_libClkFreqCountedAvg', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libClkConfig', ],
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_perfDaemonChangeSeq', '.imem_libClkReadPerfDaemon', '.imem_clkLibClk3', '.imem_libBif', '.imem_libClkProgram', ],
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_perfDaemonChangeSeq', '.imem_libClkReadPerfDaemon', '.imem_perfClkAvfs', '.imem_clkLibClk3', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libClkFreqController', '.imem_libClkFreqCountedAvg', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libClkProgram', ],
            ],

            [ 'pmu-ga100', ] => [
                # Watchdog
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_libBoardObj', '.imem_watchdogClient?', ],
                # task_perf_daemon -> perfDaemonChangeSeqScriptExelwte -> perfDaemonChangeSeqScriptInit_35 -> _perfDaemonChangeSeq35ScriptBuildAndInsert -> clkClockMonsThresholdEvaluate -> vfeEquEvaluate
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_perfDaemonChangeSeq', '.imem_perfClkAvfs', '.imem_libVolt', '.imem_libClockMon', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libBoardObj', '.imem_libLw64Umul', '.imem_libLwF32', ],
                # task_perf_daemon -> perfDaemonChangeSeqScriptExelwte -> perfDaemonChangeSeqScriptExelwteStep_35 -> _perfDaemonChangeSeq35ScriptExelwteStep_CLK_MON -> clkClockMonProgram
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_perfDaemonChangeSeq', '.imem_perfClkAvfs', '.imem_libVolt', '.imem_libClockMon', '.imem_libLw64Umul', '.imem_libLw64?', ],
            ],

            [ 'pmu-tu10x', ] => [
                # task_perf_daemon pgCtrlDisallowExt
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfDaemonChangeSeqLpwr?', '.imem_libLpwr', ],
                # task_perf_daemon perfDaemonChangeSeqLpwrPostScriptExelwte perfDaemonChangeSeqLpwrScriptExelwte
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfDaemonChangeSeqLpwr?', ],
                # task_perf_daemon perfDaemonChangeSeqLpwrPostScriptExelwte perfDaemonChangeSeqLpwrScriptExelwte grGpcRgEntryPreVoltGateLpwr_TU10X grRgGpcResetAssert_TUXXX
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfDaemonChangeSeqLpwr?', '.imem_libGr?', ],
                # task_perf_daemon perfDaemonChangeSeqLpwrPostScriptExelwte perfDaemonChangeSeqLpwrScriptExelwte perfDaemonChangeSeqScriptExelwteStep_CLK_DEINIT clkGpcRgDeInit clkFreqDomainsResetDoubleBuffer clkCntrAccessSync s_clkCntrReadNonSync clkCntrRead_GP100
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfDaemonChangeSeqLpwr?', '.imem_clkLpwr?', '.imem_clkLibClk3?', '.imem_clkLibCntrSwSync?', '.imem_clkLibCntr?', '.imem_perfClkAvfs?', ],
            ],

            [ 'pmu-ga100', ] => [
                # Permanently attached '.imem_perfDaemon',  '.imem_perf', '.imem_libBoardObj', '.imem_perfVf',

                # _perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS -> clkAdcsComputeCodeOffset -> clkAdcComputeCodeOffset_V30 -> voltRailGetVoltage
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfDaemonChangeSeq', '.imem_libVolt', '.imem_perfClkAvfs', '.imem_perfVfe', '.imem_perfVfeEquMonitor', '.imem_libLw64Umul', '.imem_libLwF32', '.imem_libSemaphoreRW', '.imem_libVoltApi' ],

                # task_perf_daemon perfDaemonChangeSeqScriptExelwte perfDaemonChangeSeqScriptExelwteAllSteps perfDaemonChangeSeqScriptExelwteStep_35_IMPL perfDaemonChangeSeqScriptExelwteStep_SUPER perfDaemonChangeSeqScriptExelwteStep_VOLT_IMPL swCounterAvgUpdate _counterGet
                [ '.imem_perfDaemon', '.imem_perfVf', '.imem_libBoardObj', '.imem_perfDaemonChangeSeq', '.imem_libAvg?', '.imem_libLw64?', '.imem_libLw64Umul', ],
            ],

            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-gv10x...', ] => [
            #     [ '.dmem_stackPerfDaemon',  '.dmem_perfDaemon', ],
            # ],
            # [ 'pmu-tu10x', 'pmu-ga100', ] => [
            #   [ '.dmem_stackPerfDaemon', '.dmem_perf', '.dmem_perfdaemon', '.dmem_clk3x', '.dmem_perfVfe', ],
            #   [ '.dmem_stackPerfDaemon', '.dmem_perf', '.dmem_perfdaemon', '.dmem_clk3x', '.dmem bif', ],
            # ],
        ],
    },

    PMUTASK_BIF =>
    {
        OSTASK_DEFINE => {
            ENTRY_FN             => "task_bif"              ,
            PRIORITY             => 1                       ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => [
                                       768 => DEFAULT,
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 1,
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(bif)"  ,
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(bif)"  ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackBif)" ,
            TCB                  => "OsTaskBif"            ,
            MAX_OVERLAYS_IMEM_LS => 4                      ,
            MAX_OVERLAYS_DMEM    => 3                      ,
            PRIV_LEVEL_EXT       => 2                      ,
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ 'pmu-*', ],
                                    ],
        },

        OVERLAY_COMBINATION => [
            [ 'pmu-tu10x...pmu-ga100', ] => [
                # Post-init
                [ '.imem_bif', '.imem_libBifInit', '.imem_libInit', '.imem_libPerfLimitClient?', ],
                # bifSetPmuPerfLimits
                [ '.imem_bif', '.imem_libBif?', '.imem_perfVf', ],
                [ '.imem_bif', '.imem_libBif?', '.imem_libPerfLimitClient?', ],
                #_bifIsPcieClkDomainTypeFixed clkDomainIsFixed
                [ '.imem_bif', '.imem_perfVf', ],
                # bifDoLaneMargining
                [ '.imem_bif', '.imem_libBifMargining?', ],
            ],
            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-tu10x', 'pmu-ga100', ] => [
            #     [ '.dmem_bif', '.dmem_stackBif', '.dmem_bifPcieLimitClient', '.dmem_perf', ],
            #     [ '.dmem_bif', '.dmem_stackBif', '.dmem_bifLaneMargining', ],
            # ],
        ],
    },

    PMUTASK_WATCHDOG =>
    {
        OSTASK_DEFINE => {
            ENTRY_FN             => "task_watchdog"                ,
            PRIORITY             => "OSTASK_PRIORITY_MAX_POSSIBLE" ,
            STACK_SIZE           => [
                                        512 => DEFAULT
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(watchdog)"     ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackWatchdog)",
            TCB                  => "OsTaskWatchdog"               ,
            MAX_OVERLAYS_IMEM_LS => 1                              ,
            MAX_OVERLAYS_DMEM    => 1                              ,
            PRIV_LEVEL_EXT       => 2                              ,
        },

        OVERLAY_COMBINATION => [ ],
    },

    PMUTASK_PERF_CF =>
    {
        OSTASK_DEFINE => {
            ENTRY_FN             => "task_perf_cf",
            PRIORITY             => 1,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => [
                                       2048 => DEFAULT,
                                       4352 => [ 'pmu-gh100...', ],
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(perfCf)",
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(perfCf)",
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackPerfCf)" ,
            TCB                  => "OsTaskPerfCf",
            MAX_OVERLAYS_IMEM_LS => 23,
            MAX_OVERLAYS_DMEM    => 12,
            PRIV_LEVEL_EXT       => 2,
            USING_FPU            => [
                                        "LW_FALSE" => DEFAULT ,
                                        "LW_TRUE"  => [ 'pmu-ga10x_riscv...', ] ,
                                    ],
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ '...pmu-ga100', ],
                                        "33792" => [ 'pmu-ga10x_riscv...', ],
                                    ],
        },

        OVERLAY_COMBINATION => [
            [ 'pmu-tu10x', 'pmu-ga100', ] => [
                # Post-init
                [ '.imem_perfCf', '.imem_perfCfInit', '.imem_libPerfLimitClient?', '.imem_libInit', ],
                # pmuRpcProcessUnitPerfCf_IMPL ->  pmuRpcPerfCfBoardObjGrpCmd
                [ '.imem_perfCf', '.imem_perfCfBoardObj', '.imem_libBoardObj', '.imem_libLw64','.imem_libLw64Umul', '.imem_perfCfTopology', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libLpwr', ],
                # Load(): SENSORS
                [ '.imem_perfCf', '.imem_perfCfLoad', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libLpwr', '.imem_perfCfTopology', '.imem_libEngUtil', ],
                # Load(): TOPOLOGYS
                [ '.imem_perfCf', '.imem_perfCfLoad', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libLpwr', '.imem_perfCfTopology', ],
                # Load(): POLICYS
                [ '.imem_perfCf', '.imem_perfCfLoad', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul', ],
                # Load(): CONTROLLERS limits init
                [ '.imem_perfCf', '.imem_perfCfLoad', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul',                                                                                        '.imem_perfVf', ],
                # Load(): CONTROLLERS-FILTER
                [ '.imem_perfCf', '.imem_perfCfLoad', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libLpwr', '.imem_perfCfTopology', '.imem_perfVf',                                                                                                                                                                                                                                                       '.imem_libFxpBasic',                      '.imem_libSemaphoreRW', ],
                # Timer callbacks: CONTROLLERS-EVALUATE
                [ '.imem_perfCf', '.imem_libBoardObj',                         '.imem_libLw64', '.imem_libLw64Umul',                                                                                        '.imem_perfVf', '.imem_libLwF32', '.imem_libClkVolt?', '.imem_libSemaphoreRW', '.imem_libPerfLimitClient?', ],
                # Timer callbacks: TOPOLOGYS
                [ '.imem_perfCf', '.imem_perfCfTopology', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libLpwr', '.imem_loggerWrite', ],
                # Logger init()
                [ '.imem_perfCf', '.imem_libGpumon', ],
            ],

            [ 'pmu-tu10x', 'pmu-ga100', ] => [
                # Load(): CONTROLLERS-EVALUATE
                [ '.imem_perfCf', '.imem_perfCfLoad', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul',                                                                                        '.imem_perfVf', '.imem_libLwF32',  '.imem_libClkVolt?', '.imem_libSemaphoreRW', '.imem_libPerfLimitClient?', ],
                # Timer callbacks: CONTROLLERS-FILTER
                [ '.imem_perfCf', '.imem_libBoardObj',                     '.imem_libLw64', '.imem_libLw64Umul', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libLpwr', '.imem_perfCfTopology', '.imem_perfVf',                                                                                                                                                                                                                                                       '.imem_libFxpBasic',                       '.imem_libSemaphoreRW', ],
            ],

            [ 'pmu-ga100', ] => [
                # task_perf_cf -> pmuRpcProcessUnitPerfCf_IMPL -> pmuRpcPerfCfBoardObjGrpCmd
                [ '.imem_perfCf', '.imem_perfCfBoardObj', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul', '.imem_perfCfTopology', '.imem_clkLibCntr', '.imem_clkLibCntrSwSync', '.imem_libLpwr', '.imem_perfCfModel', ],
                # task_perf_cf -> pmuRpcProcessUnitPerfCf_IMPL -> pmuRpcPerfCfLoad -> perfCfPwrModelsLoad
                [ '.imem_perfCf', '.imem_perfCfLoad', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul', '.imem_perfCfModel', '.imem_perfVf', ],
                # task_perf_cf -> pmuRpcProcessUnitPerfCf_IMPL -> pmuRpcPerfCfPwrModelScale -> s_pmuRpcPerfCfPwrModelScale_NON_ODP -> osSemaphoreRWTakeRD
                [ '.imem_perfCf', '.imem_perfCfModel', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul', '.imem_perfVf', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_pmgrPwrPolicyPerfCfPwrModelScale', '.imem_perfLimitVf', '.imem_libVoltApi', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_libFxpExtended', '.imem_libFxpBasic', '.imem_libSemaphoreRW', ],
                # task_perf_cf -> pmuRpcProcessUnitPerfCf_IMPL -> pmuRpcPerfCfPwrModelScale -> s_pmuRpcPerfCfPwrModelScale_NON_ODP ->           perfCfPwrModelScale -> perfCfPwrModelScale_TGP_1X -> pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X -> pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_perfCf', '.imem_perfCfModel', '.imem_libBoardObj',                                       '.imem_perfVf', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_pmgrPwrPolicyPerfCfPwrModelScale', '.imem_perfLimitVf', '.imem_libVoltApi',                                       '.imem_libPwrEquation', '.imem_libFxpExtended', '.imem_libFxpBasic', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
                # task_perf_cf -> pmuRpcProcessUnitPerfCf_IMPL -> pmuRpcPerfCfClientPwrModelProfileScale -> osSemaphoreRWTakeRD
                [ '.imem_perfCf', '.imem_perfCfModel', '.imem_libBoardObj', '.imem_libLw64', '.imem_libLw64Umul', '.imem_perfVf', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_pmgrPwrPolicyPerfCfPwrModelScale', '.imem_perfLimitVf', '.imem_libVoltApi', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libPwrEquation', '.imem_libFxpExtended', '.imem_libFxpBasic', '.imem_libSemaphoreRW', ],
                # task_perf_cf -> pmuRpcProcessUnitPerfCf_IMPL -> pmuRpcPerfCfClientPwrModelProfileScale -> clientPerfCfPwrModelProfileScale -> perfCfPwrModelScale -> perfCfPwrModelScale_TGP_1X -> pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X -> pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X -> pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X -> s_pwrPolicyGetEstMetricsAtFreq_WORKLOAD_SINGLE_1X -> voltRailGetLeakage -> pwrEquationGetLeakage -> pwrEquationLeakageEvaluatemW? -> pwrEquationLeakageEvaluatemA -> pwrEquationLeakageEvaluatemA_CORE -> pwrEquationLeakageEvaluatemA_DTCS12
                [ '.imem_perfCf', '.imem_perfCfModel', '.imem_libBoardObj',                                       '.imem_perfVf', '.imem_pmgrPwrPolicyPerfCfPwrModel', '.imem_pmgrPwrPolicyPerfCfPwrModelScale', '.imem_perfLimitVf', '.imem_libVoltApi',                                       '.imem_libPwrEquation', '.imem_libFxpExtended', '.imem_libFxpBasic', '.imem_libLeakage', '.imem_thermLibSensor2X', '.imem_libi2c', ],
            ],

            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-tu10x', 'pmu-ga100', ] => [
            #     [ '.dmem_perfCf', '.dmem_perfCfTopology', '.dmem_perfCfModel', '.dmem_perfCfController', '.dmem_stackPerfCf', '.dmem_perfCfLimitClient', '.dmem_perf', '.dmem_clk3x', '.dmem_pmgr', '.dmem_perfVfe', 'dmem_perfChangeSeqClient', ],
            # [ 'pmu-ga100', ] => [
            #     # pmuRpcPerfCfPwrModelScale()
            #     [ '.dmem_perfCf', '.dmem_perf', '.dmem_perfCfModel', '.dmem_perfCfPwrModelScaleRpcBufNonOdp', ],
            # ],
        ],
    },

    PMUTASK_LOWLATENCY =>
    {
        OSTASK_DEFINE => {
            ENTRY_FN             => "task_lowlatency"             ,
            PRIORITY             => 4                             ,
            # STACK_SIZE used on GP102+ only when OVERLAY_STACK is not defined.
            # Otherwise stack size is controlled within OverlaysDmem.pm file.
            STACK_SIZE           => [
                                       512 => DEFAULT,
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(lowlatency)"  ,
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(lowlatency)"  ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackLowlatency)" ,
            TCB                  => "OsTaskLowlatency"            ,
            MAX_OVERLAYS_IMEM_LS => 2                             ,
            MAX_OVERLAYS_DMEM    => 3                             ,
            PRIV_LEVEL_EXT       => 2                             ,
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ 'pmu-*', ],
                                    ],
        },

        OVERLAY_COMBINATION => [
            [ 'pmu-tu10x', 'pmu-ga100', ] => [
                # lwrtosTASK_FUNCTION -> _lowlatencyEventHandle -> bifServiceMarginingIntr_TU10X
                [ '.imem_lowlatency', '.imem_libBifMargining?', ],
            ],
            # DMEM OVERLAY_COMBINATION(s) - Uncomment when build scripts can parse this
            # [ 'pmu-tu10x', 'pmu-ga100', ] => [
            #     [ '.dmem_stackLowlatency', '.dmem_lowlatency', '.dmem_bifLaneMargining', ],
            # ],
        ],
    },

    PMUTASK_NNE =>
    {
        OSTASK_DEFINE => {
            ENTRY_FN             => "task_nne"                    ,
            PRIORITY             => 1                             ,
            STACK_SIZE           => [
                                        768 => DEFAULT,
                                       1024 => [ ],
                                    ],
            WATCHDOG_TIMEOUT_US  => [
                                       0 => DEFAULT, # System-defined Timeout
                                    ],
            WATCHDOG_TRACKING_ENABLED => 0, # Disabled
            OVERLAY_IMEM         => "OVL_INDEX_IMEM(nne)"         ,
            OVERLAY_DMEM         => "OVL_INDEX_DMEM(nne)"         ,
            OVERLAY_STACK        => "OVL_INDEX_DMEM(stackNne)"    ,
            TCB                  => "OsTaskNne"                   ,
            MAX_OVERLAYS_IMEM_LS => 3                             ,
            MAX_OVERLAYS_DMEM    => 2                             ,
            PRIV_LEVEL_EXT       => 2                             ,
            USING_FPU            => [
                                        "LW_FALSE" => DEFAULT ,
                                        "LW_TRUE"  => [ AMPERE_and_later, -GA100, ] ,
                                    ],
            CMD_SCRATCH_SIZE     => [
                                        "0U" => [ '...pmu-ga100', ],
                                        "2048U" => [ 'pmu-ga10x_riscv...', ],
                                    ],
        },

        OVERLAY_COMBINATION => [ ],
    },
];

# return the raw data of Tasks definition
return $tasksRaw;
