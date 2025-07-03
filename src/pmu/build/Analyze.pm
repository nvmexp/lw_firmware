#
# Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#


#
# rtosParams - OS parameters, mostly stack-related
#
# These will likely be the same for all three devices (PMU, SEC2, & DPU), but
# setting them here avoids hard-coded constants hidden in the code and makes
# them easier to change in the future.
#
my $rtosParams = {
    FALCON => {
        # stack space needed to save context when an interrupt oclwrs
        INTERRUPT_OVERHEAD => 76,

        # these functions have the interrupt overhead subtracted instead of added to
        # their stack usage, since context is saved on the interrupted task's stack
        INTERRUPT_HANDLERS => [
            "IV0_routine",
            "IV1_routine",
            "EV_routine",
            "EV_preinit_routine"
        ],

        # this function is critical (no interrupt overhead) and doesn't have the
        # 4-byte return-address overhead that regular functions do
        START => "start",
    },

    RISCV => {
        # Stack space needed to save context when an interrupt oclwrs.
        # Zero on RISCV since we don't save context on the task stack.
        INTERRUPT_OVERHEAD => 0,

        INTERRUPT_HANDLERS => [
            "vPortTrapHandler"
        ],

        START => "_start",
    },
};


#
# cycleLimits - upper limits on relwrsive functions
#
# These values define the maximum number of times that each cycle in the call
# graph can repeat itself. Without it, the script wouldn't be able to aclwrately
# callwlate the stack usage of any task containing a cycle. (A cycle is any
# sequence of functions where the last calls the first.)
#
# If a function is the root of multiple cycles, use the greatest of its cycles'
# limits as the limit here. And if one cycle contains or is the parent of
# another cycle, list the inner cycle first.
#
# If the limit is not determined yet set it to 'undef', which will be treated as
# 1 for stack usage callwlations (WRONG but some value must be provided in order
# to compile ucode).
#
# Syntax:
#   <FUNCTION NAME> => <CYCLE NUM>,
#       or
#   <FUNCTION NAME> => [
#       value1 => [ CHIP_LIST, ],
#       value2 => DEFAULT,
#       ...
#   ],
#
my $cycleLimits = [
#
# RISCV lwrtos critical section error handling. All cycles are 0 b.c. the error only starts in user-mode,
# but the "relwrsive" call to lwrtosTaskCritical(Enter|Exit) only happens in an if-kernel code path.
#
    'lwrtosTaskCriticalEnter'      => 0,
    'lwrtosTaskCriticalExit'       => 0,
    'lwrtosLwrrentTaskPrioritySet' => 0,
    'printf'                       => 0,
    'puts'                         => 0,
    'sysTaskExit'                  => 0,
    'dbgPutsKernel'                => 0,
    'dbgPrintfKernel'              => 0,

    'pwrChannelContains' => [
        2 => DEFAULT,
    ],
    'pwrMonitorChannelTupleStatusGet' => [
        undef => DEFAULT,
        2     => [ GM20X, ],                          # Worst case stack usage is computed from the GM204 LWDQRO M4000 Power Topology and Relationship setting.
        3     => [ GV10X_and_later, ],                # Worst case stack usage is computed from the GV100 PG500 Power Topology and Relationship setting.
    ],
    'clkDomain3XProgFreqQuantize'               => 1,
    'clkDomainProgFreqQuantize_3X'              => 1,
    'clkDomain3XProgFreqAdjustDeltaMHz'         => 1,
    'clkDomain3XProgFreqAdjustQuantizeMHz'      => 1,
    'clkDomain3XPrimaryFreqAdjustDeltaMHz'      => 1,
    'clkDomain3XSecondaryFreqAdjustDeltaMHz'    => 1,
    'clkProg3XFreqMaxAdjAndQuantize'            => 1,
    'clkProg3XPrimaryFreqTranslatePrimaryToSecondary' => 1,
    'vfeVarEvalByIdx'                           => 4,
    'vfeEquEvalList_IMPL'                       => [
        8   => DEFAULT,                               # Keep in sync with RM_PMU_VFE_EQU_RELWRSION_DEPTH_MAX_DEFAULT
        12   => [ GA102_and_later, ],                 # Keep in sync with RM_PMU_VFE_EQU_RELWRSION_DEPTH_MAX_EXTENDED
    ],
    'vbiosIedExelwteScriptSub'                  => 3,
    'perfCfTopologyUpdateDependencyMask'        => 5, # Max number of topologys is 32 = 2^5
    'perfCfTopologyUpdateTmpReading'            => 5, # Max number of topologys is 32 = 2^5
    's_perfCfControllerDlppc1xIsPowerLessThan'  => 1,
    'perfChangeSeqScriptCompletion'             => 3, # At max two perf change (forced, next) with old vf lwrve + completion on current change.
    'perfChangeSeqProcessPendingChange_IMPL'    => 2, # At max two perf change (forced, next) with old vf lwrve.

# Clocks 3.x: Maximum depth is the longest branch in the schematic dag.
    'clkRead_ADynRamp'                          => 0,
    'clkRead_HPll'                              => 0,
    'clkRead_LdivUnit'                          => 0,
    'clkRead_LdivV2'                            => 0,
    'clkRead_Multiplier'                        => 0,
    'clkRead_Divider'                           => 0,
    'clkRead_Mux'                               => 3,   # See pmu_sw/prod_app/clk/clk3/clksdcheck.c
    'clkRead_PDiv'                              => 0,
    'clkRead_APll'                              => 0,
    'clkRead_ReadOnly'                          => 0,
    'clkRead_RoPDiv'                            => 0,
    'clkRead_RoSdm'                             => 0,
    'clkRead_RoVco'                             => 0,
    'clkRead_ASdm'                              => 0,
    'clkRead_Wire'                              => 0,
    'clkRead_Xtal'                              => 0,
    'clkConfig_ADynRamp'                        => 0,
    'clkConfig_HPll'                            => 0,
    'clkConfig_LdivUnit'                        => 0,
    'clkConfig_LdivV2'                          => 0,
    'clkConfig_Multiplier'                      => 0,
    'clkConfig_Divider'                         => 0,
    'clkConfig_Mux'                             => 3,   # See pmu_sw/prod_app/clk/clk3/clksdcheck.c
    'clkConfig_APll'                            => 0,
    'clkConfig_ReadOnly'                        => 0,
    'clkConfig_Wire'                            => 0,
    'clkProgram_ADynRamp'                       => 0,
    'clkProgram_FreqSrc'                        => 0,
    'clkProgram_HPll'                           => 0,
    'clkProgram_LdivUnit'                       => 0,
    'clkProgram_Mux'                            => 3,   # See pmu_sw/prod_app/clk/clk3/clksdcheck.c
    'clkProgram_APll'                           => 0,
    'clkProgram_ReadOnly'                       => 0,
    'clkProgram_Wire'                           => 0,
    'clkCleanup_ADynRamp'                       => 0,
    'clkCleanup_FreqSrc'                        => 0,
    'clkCleanup_HPll'                           => 0,
    'clkCleanup_LdivUnit'                       => 0,
    'clkCleanup_Mux'                            => 3,   # See pmu_sw/prod_app/clk/clk3/clksdcheck.c
    'clkCleanup_APll'                           => 0,
    'clkCleanup_ReadOnly'                       => 0,
    'clkCleanup_ASdm'                           => 0,
    'clkCleanup_Wire'                           => 0,
    'clkPrint_Mux'                              => 3,   # See pmu_sw/prod_app/clk/clk3/clksdcheck.c
    'clkPrint_Wire'                             => 0,
];


#
# compact - functions to compactify in the calltrees
#
# Functions listed here will be "compacted" by replacing them with -COMPACTED
# versions representing the entire calltree rooted at each one. The compacted
# replacement will have stack usage equal to the original calltree's, but won't
# call any other functions, making the output shorter and easier to use. The
# full calltrees for compacted functions are put in calltree_compacted.txt.
#
# This is intended for OS routines and the like, and won't work if the
# function's calltree contains more than one non-resident overlay.
#
my $compact = [
    'lwosMalloc_IMPL',
    'lwosCalloc_IMPL',

    'lwrtosTaskCreate',
    'lwrtosLwrrentTaskPrioritySet',
    'lwrtosLwrrentTaskPriorityGet',
    'lwrtosQueueCreate',
    'lwrtosQueueReceive',
    'lwrtosQueueSend',
    'lwrtosQueueSendFromISRWithStatus',
    'lwrtosSemaphoreCreateBinaryTaken',
    'lwrtosSemaphoreCreateBinaryGiven',
    'lwosTaskSetupCanary',
    'lwosTaskOverlayAttach',
    'lwosTaskOverlayDetach',
    'lwrtosTaskCriticalEnter',
    'lwrtosTaskCriticalExit',
    'lwosDmaLockAcquire',
    'lwosDmaLockRelease',
    'lwosDmaSuspend',
    'lwosDmaResume',
    'pmuRmRpcExelwte',
    'osTmrCallbackCreate_IMPL',
    'osTmrCallbackSchedule_IMPL',
    'lwrtosLwrrentTaskDelayTicks',
    'appTaskCriticalEnter_FUNC',
    'appTaskCriticalExit_FUNC',
    'lwosMallocAligned_IMPL',

    'kernelVerboseCrash',
    'sysTaskExit',
    'printf',
    'puts',

    '__PortRestoreContextAndEret',

    'sysDmaTransfer',
    '_dmaTransfer',
    'queueFbPtcbRpcRespond',
    'queueFbHeapRpcRespond',
    'cmdmgmtRmCmdRespond',
    'osTaskOverlayDescListExec',
    'mutexAcquire',
    'mutexRelease',

    'pmuPollReg32Ns',

    '__addsf3',
    '__gtsf2',
    '__ltsf2',
];


#
# unreached - functions which shouldn't be reached
#
# Normally ucode shouldn't contain any unreached (dead) code; however,  there
# are some exceptions where we expect functions to be unreached. UcodeAnalyze
# will ignore if any function listed here is unreached, allowing build to pass.
#
# Before adding a function to this list, make sure it really is an exception.
# The script often lists function as unreached due to incomplete FP mappings.
#
my $unreached = [

# ilwoked with "jmp" and not "call"
    '_restore_ctx_full',                # ilwoked with "jmp" and not "call"
    '_restore_ctx_merged',              # ilwoked with "jmp" and not "call"
    '_ctx_save_local',                  # ilwoked with "jmp" and not "call"
    '_ctx_saved',                       # ilwoked with "jmp" and not "call"
    '_ctx_restore_local',               # ilwoked with "jmp" and not "call"
    '_ctx_restored',                    # ilwoked with "jmp" and not "call"
    'perfPstateGetLevelByIndex',        # PP-TODO : This code is in common file but not called on PS 3.0
    's_pwrPolicyGetInterfaceFromBoardObj_WORKLOAD_COMBINED_1X', # This code is not called yet
    's_pwrPolicyGetInterfaceFromBoardObj_WORKLOAD_SINGLE_1X',   # This code is not called yet
    'perfCfControllerMemTuneMonitorSanityCheck_GA10X',   # This code is feature protected and only enabled on GA10x selectively.
    's_perfCfControllerMemTuneMonitorCallback',   # This code is feature protected and only enabled on GA10x selectively.
    's_perfCfControllerMemTuneMonitorMclkFreqkHzGet', # This code is feature protected and only enabled on GA10x selectively.
    'partitionSwitchReturn', # This is return point from SK
    'perfDaemonChangeSeqFlush_STEP_IMPL', # GCC compiler optimizations
];


#
# fpMappings - function pointer mappings
#
# FP mappings define the targets of all function pointers in the code. This is
# necessary because most pointer values can't be resolved statically. Each entry
# has the following format:
#
#    'FP_FUNC' => [
#        'PARENT_FUNC' => [
#            'TARGET',
#            ...
#        ],
#        ...
#    ]
#
# FP_FUNC is the name of a function using at least one function pointer, and
# TARGETs are all the possible values for that pointer when FP_FUNC is a
# descendent of PARENT_FUNC in the call tree. This structure allows a function
# pointer's targets to depend on the context it's called from, which is useful
# for functions which are given other functions as arguments (e.g., boardobj
# functions like boardObjGrpIfaceModel10Set).
#
# Set PARENT_FUNC to 'ALL' if the targets of a pointer are always the same,
# regardless of context. ALL should be the only parent func if it's present.
#
# Add a target of 'NULL' if the function pointer is sometimes not called at all.
# This is useful when the pointer can be null (like for osPTimerCondSpinWaitNs),
# or when none of the target functions are present on some build.
#
my $fpMappings = {
    'partitionSwitch' => {
        'ALL' => [
            'partitionSwitchKernel',
        ],
    },

    'xPortStartScheduler' => {
        'ALL' => [
            'prvTaskStart',
            'vTaskExit',
        ],
    },

    'vprintfmt' => {
        'dbgPrintfKernel' => [
            'debugPutchar',
        ],
        'dbgPrintfUser' => [
            'putchar_buffered',
        ],
    },

    'osTmrCallbackExelwte_IMPL' => {
        'task_lpwr' => [
            's_lpwrHandleOsCallback',
            'NULL',
        ],
        'task_lpwr_lp' => [
            's_eiHandleOsCallback',
            's_lpwrTestCallback',
            'NULL',
        ],
        'task_perf' => [
            's_clkControllerOsCallback',
            'clkFreqControllerOsCallback',
            's_clkFreqEffAvgOsCallback',
            's_clkAdcVoltageSampleOsCallback',
            's_clkCntrOsCallback',
            's_vfeTimerOsCallback',
            's_clkDomainsPmumonCallback',
            's_voltRailsPmumonCallback',
            's_perfPolicyPmumonCallback',
        ],
        'task_perf_cf' => [
            's_perfCfTopologysCallback',
            's_perfCfControllersCallback',
            's_perfCfTopologiesPmumonCallback',
            's_perfCfPmSensorsPmumonCallback',
        ],
        'task_pmgr' => [
            's_psiHandleOsCallbackMultiRail',
            's_pwrPoliciesEvaluateOsCallback_3X',
            's_pwrDevOsCallback',
            's_pwrChannels35Callback',
            's_pwrChannelsPmumonCallback',
        ],
        'task_perfmon' => [
            'perfmonOsDoSample',
        ],
        'task_therm' => [
            's_fanPolicyOsCallbackGeneric',
            's_fanCoolerPmumonCallback',
            's_thermMonitorTimerOsCallbackExelwte',
            's_thermPolicyTimerOsCallbackExelwte',
            's_thermChannelPmumonCallback',
            's_thermCallbackPmumon',
            'NULL',
        ],
        'task_pcmEvent' => [
            's_clkCntrOsCallback',
        ],
        'task_disp' => [
            's_disp1HzOsCallback',
        ],
        'task_spi' => [
            'NULL',
        ],
        'task_bif' => [
            's_bifXusbIsochTimeoutCallbackExelwte',
            's_bifEngageAspmL1Aggressively',
            'NULL',
        ],
        'task_nne' => [
            'NULL',
        ],
    },

    'osTimerCallExpiredCallbacks' => {
        'task_lpwr' => [
            's_lpwrHandleCallback',
            'NULL'
        ],
        'task_perf' => [
            's_clkControllerCallback',
            'clkFreqControllerCallback',
            's_clkFreqEffAvgCallback',
            's_clkAdcVoltageSampleCallback',
            's_clkCntrCallback',
            's_vfeTimerCallback',
        ],
        'task_pmgr' => [
            's_pwrPoliciesEvaluateCallback_3X',
            's_pwrDevCallback',
            'NULL'
        ],
        'task_perfmon' => [
            'perfmonDoSample',
        ],
        'task_therm' => [
            's_fanPolicyCallbackGeneric',
            's_therm1HzCallbacks',
            's_thermMonitorTimerCallbackExelwte',
            's_thermPolicyTimerCallbackExelwte',
            's_thermCallbackPmumon',
        ],
        'task_pcmEvent' => [
            's_clkCntrCallback',
        ],
        'task_disp' => [
            's_disp1HzCallback',
        ],
        'task_spi' => [
            'NULL',
        ],
    },

    'osPTimerCondSpinWaitNs_IMPL' => {
        'osPTimerSpinWaitNs' => [
            'NULL',
        ],
        's_i2cWaitSclReleasedTimedOut' => [
            's_i2cIsLineHigh',
        ],
        'dispEnqMethodForChannel_GMXXX' => [
            's_dispCheckForMethodCompletion_GMXXX',
            'NULL',
        ],
        'dispEnqMethodForChannel_GV10X' => [
            's_dispCheckForMethodCompletion_GV10X',
            'NULL',
        ],
        'pmuPollReg32Ns_IMPL' => [
            'pmuPollReg32Func',
        ],
        'seqWaitSignal_GM10X' => [
            'seqCheckVblank_GMXXX',
        ],
        'seqWaitSignal_GV10X' => [
            's_seqCheckVblank_GV10X',
        ],
        'seqWaitForOk2Switch_GM10X' => [
            'seqCheckSignal',
        ],
        'task_hdcp' => [
            's_hdcpCheckBusReady',      # twice
        ],
        's_clkWaitForGpcpllLock_GMXXX' => [
            's_clkIsGpcpllLocked_GMXXX',
        ],
        'bifChangeBusSpeed_TU10X' => [
            's_bifLtssmIsIdle_TU10X',
            's_bifLtssmIsIdleAndLinkReady_TU10X',
        ],
        'bifPollEomDoneStatus_GA10X' => [
            's_bifEomDoneStatus_GA10X',
        ],
        'bifPollEomDoneStatus_GH100' => [
            's_bifEomDoneStatus_GH100',
        ],
        'bifChangeBusSpeed_GH100' => [
            's_bifLtssmIsIdle_GH100',
            's_bifLtssmIsIdleAndLinkReady_GH100',
        ],
        's_bifLinkSpeedChangeAttempt_TU10X' => [
            's_bifLtssmIsIdleAndLinkReady_TU10X',
        ],
        'clkProgram_HPll' => [
            'clkProgram_IsLocked_HPll',
        ],
        'clkProgram_Mux' => [
            'clkProgram_IsSynced_Mux',
        ],
        'clkProgram_APll' => [
            'clkProgram_IsLocked_APll',
        ],
        'perfCfPmSensorSnap_V10' => [
            's_perfCfPmSensorSwTriggerSnapCleared_V10',
        ],
        'perfCfPmSensorClwV10WaitSnapComplete' => [
            's_perfCfPmSensorClwV10SwTriggerSnapCleared',
        ],
        'perfCfPmSensorsClwV10OobrcRead' => [
            's_perfCfPmSensorsClwV10OobrcDataReady',
        ],
        'dmaWaitForIdle' => [
            'csbPollFunc',
        ],
        'dmaMemTransfer' => [
            'csbPollFunc',
        ],
        'debugPutchar' => [
            'debugBufferIsNotFull',
        ],
        'debugPutByteKernel' => [
            'debugBufferIsNotFull',
        ],
        'dbgDispatchRawDataKernel' => [
            'debugBufferIsNotFull',
        ],
        'debugBufferWaitForEmpty' => [
            'debugBufferIsEmpty',
        ],
        'riscv_scpWait' => [
            'scpDmaPollIsIdle',
        ],
        'osCmdmgmtQueueWaitForEmpty' => [
            's_osCmdmgmtQueueIsEmpty',
        ],
        'irqWaitForSwGenClear' => [
            'irqIsSwGenClear',
        ],
    },

    'boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL' => {
        'pmuRpcFanBoardObjGrpCmd' => [
            'fanCoolersBoardObjGrpIfaceModel10Set',
            'fanCoolersBoardObjGrpIfaceModel10GetStatus',
            'fanPoliciesBoardObjGrpIfaceModel10Set',
            'fanPoliciesBoardObjGrpIfaceModel10GetStatus',
            'fanArbitersBoardObjGrpIfaceModel10Set',
            'fanArbitersBoardObjGrpIfaceModel10GetStatus',
        ],
        'pmuRpcPmgrBoardObjGrpCmd' => [
            'i2cDeviceBoardObjGrpIfaceModel10Set',
            'pwrEquationBoardObjGrpIfaceModel10Set',
            'pwrDeviceBoardObjGrpIfaceModel10Set',
            'pwrDeviceBoardObjGrpIfaceModel10GetStatus',
            'pwrChannelBoardObjGrpIfaceModel10Set',
            'pwrChannelBoardObjGrpIfaceModel10GetStatus',
            'pwrPolicyBoardObjGrpIfaceModel10Set',
            'pwrPolicyBoardObjGrpIfaceModel10GetStatus',
            'illumDeviceBoardObjGrpIfaceModel10Set',
            'illumZoneBoardObjGrpIfaceModel10Set',
        ],
        '_pmgrProcessSuperSurface' => [
            'i2cDeviceBoardObjGrpIfaceModel10Set',
            'pwrEquationBoardObjGrpIfaceModel10Set',
            'pwrDeviceBoardObjGrpIfaceModel10Set',
            'pwrChannelBoardObjGrpIfaceModel10Set',
            'pwrPolicyBoardObjGrpIfaceModel10Set',
        ],
        'pmuRpcThermBoardObjGrpCmd' => [
            'thermDeviceBoardObjGrpIfaceModel10Set',
            'thermChannelBoardObjGrpIfaceModel10Set',
            'thermChannelBoardObjGrpIfaceModel10GetStatus',
            'thermPolicyBoardObjGrpIfaceModel10Set',
            'thrmMonitorBoardObjGrpIfaceModel10Set',
            'thrmMonitorBoardObjGrpIfaceModel10GetStatus',
            'thermPolicyBoardObjGrpIfaceModel10GetStatus',
        ],
        '_thermProcessSuperSurface' => [
            'thermDeviceBoardObjGrpIfaceModel10Set',
            'thermChannelBoardObjGrpIfaceModel10Set',
            'thermPolicyBoardObjGrpIfaceModel10Set',
        ],
        'task_spi' => [
            'spiDeviceBoardObjGrpIfaceModel10Set',
        ],
        '_spiProcessSuperSurface' => [
            'spiDeviceBoardObjGrpIfaceModel10Set',
        ],

        # === task_perf ===
        'pmuRpcPerfBoardObjGrpCmd' => [
            'perfLimitBoardObjGrpIfaceModel10Set',
            'perfLimitBoardObjGrpIfaceModel10GetStatus',
            'perfPolicyBoardObjGrpIfaceModel10Set',
            'perfPolicyBoardObjGrpIfaceModel10GetStatus',
            'vfeVarBoardObjGrpIfaceModel10Set',
            'vfeVarBoardObjGrpIfaceModel10GetStatus',
            'vfeEquBoardObjGrpIfaceModel10Set',
            'vpstateBoardObjGrpIfaceModel10Set',
            'pstateBoardObjGrpIfaceModel10Set',
            'pstateBoardObjGrpIfaceModel10GetStatus',
        ],
        'perfProcessSuperSurface' => [
            'vfeVarBoardObjGrpIfaceModel10Set',
            'vfeEquBoardObjGrpIfaceModel10Set',
        ],
        'pmuRpcPerfCfBoardObjGrpCmd' => [
            'perfCfSensorBoardObjGrpIfaceModel10Set',
            'perfCfSensorBoardObjGrpIfaceModel10GetStatus',
            'perfCfPmSensorBoardObjGrpIfaceModel10Set',
            'perfCfPmSensorBoardObjGrpIfaceModel10GetStatus',
            'perfCfTopologyBoardObjGrpIfaceModel10Set',
            'perfCfTopologyBoardObjGrpIfaceModel10GetStatus',
            'perfCfControllerBoardObjGrpIfaceModel10Set_1X',
            'perfCfControllerBoardObjGrpIfaceModel10GetStatus_1X',
            'perfCfControllerBoardObjGrpIfaceModel10Set_2X',
            'perfCfControllerBoardObjGrpIfaceModel10GetStatus_2X',
            'perfCfClientPerfCfControllerBoardObjGrpIfaceModel10Set',
            'perfCfClientPerfCfControllerBoardObjGrpIfaceModel10GetStatus',
            'perfCfPolicyBoardObjGrpIfaceModel10Set',
            'perfCfPolicyBoardObjGrpIfaceModel10GetStatus',
            'perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10Set',
            'perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10GetStatus',
            'perfCfPwrModelBoardObjGrpIfaceModel10Set',
            'perfCfPwrModelBoardObjGrpIfaceModel10GetStatus',
            'clientPerfCfPwrModelProfileBoardObjGrpIfaceModel10Set',
        ],
        'perfCfProcessSuperSurface' => [
            'perfCfSensorBoardObjGrpIfaceModel10Set',
            'perfCfPmSensorBoardObjGrpIfaceModel10Set',
            'perfCfTopologyBoardObjGrpIfaceModel10Set',
            'perfCfControllerBoardObjGrpSet',
            'perfCfClientControllerBoardObjGrpSet',
            'perfCfPolicyBoardObjGrpIfaceModel10Set',
            'perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10Set',
            'perfCfPwrModelBoardObjGrpIfaceModel10Set',
            'clientPerfCfPwrModelProfileBoardObjGrpIfaceModel10Set',
        ],
        'pmuRpcClkBoardObjGrpCmd' => [
            'clkDomainBoardObjGrpIfaceModel10Set',
            'clkProgBoardObjGrpIfaceModel10Set',
            'clkAdcDevBoardObjGrpIfaceModel10Set',
            'clkAdcDevicesBoardObjGrpIfaceModel10GetStatus',
            'clkNafllDevBoardObjGrpIfaceModel10Set',
            'clkNafllDevicesBoardObjGrpIfaceModel10GetStatus',
            'clkVfPointBoardObjGrpIfaceModel10Set',
            'clkVfPointBoardObjGrpIfaceModel10GetStatus',
            'clientClkVfPointBoardObjGrpIfaceModel10Set',
            'clientClkVfPointBoardObjGrpIfaceModel10GetStatus',
            'clkFreqControllerBoardObjGrpIfaceModel10Set',
            'clkVoltControllerBoardObjGrpIfaceModel10Set',
            'clkFreqControllerBoardObjGrpIfaceModel10GetStatus',
            'clkVoltControllerBoardObjGrpIfaceModel10GetStatus',
            'clkPllDevBoardObjGrpIfaceModel10Set',
            'clkFreqDomainBoardObjGrpIfaceModel10Set',
            'clkFreqDomainBoardObjGrpIfaceModel10GetStatus',
            'clkProgBoardObjGrpIfaceModel10GetStatus',
            'clkEnumBoardObjGrpIfaceModel10Set',
            'clkVfRelBoardObjGrpIfaceModel10Set',
            'clkVfRelBoardObjGrpIfaceModel10GetStatus',
            'clkPropRegimeBoardObjGrpIfaceModel10Set',
            'clkPropTopBoardObjGrpIfaceModel10Set',
            'clkPropTopBoardObjGrpIfaceModel10GetStatus',
            'clkPropTopRelBoardObjGrpIfaceModel10Set',
            'clkClientClkPropTopPolBoardObjGrpIfaceModel10Set',
        ],
        'clkProcessSuperSurface' => [
            'clkDomainBoardObjGrpIfaceModel10Set',
            'clkProgBoardObjGrpIfaceModel10Set',
            'clkPllDevBoardObjGrpIfaceModel10Set',
            'clkAdcDevBoardObjGrpIfaceModel10Set',
            'clkNafllDevBoardObjGrpIfaceModel10Set',
            'clkVfPointBoardObjGrpIfaceModel10Set',
            'clientClkVfPointBoardObjGrpIfaceModel10Set',
            'clkFreqControllerBoardObjGrpIfaceModel10Set',
            'clkVoltControllerBoardObjGrpIfaceModel10Set',
            'clkFreqDomainBoardObjGrpIfaceModel10Set',
            'clkEnumBoardObjGrpIfaceModel10Set',
            'clkVfRelBoardObjGrpIfaceModel10Set',
            'clkPropRegimeBoardObjGrpIfaceModel10Set',
            'clkPropTopBoardObjGrpIfaceModel10Set',
            'clkPropTopRelBoardObjGrpIfaceModel10Set',
            'clkClientClkPropTopPolBoardObjGrpIfaceModel10Set',
        ],
        'pmuRpcVoltBoardObjGrpCmd' => [
            'voltDeviceBoardObjGrpIfaceModel10Set',
            'voltRailBoardObjGrpIfaceModel10Set',
            'voltRailBoardObjGrpIfaceModel10GetStatus',
            'voltPolicyBoardObjGrpIfaceModel10Set',
            'voltPolicyBoardObjGrpIfaceModel10GetStatus',
        ],
        'voltProcessSuperSurface' => [
            'voltDeviceBoardObjGrpIfaceModel10Set',
            'voltRailBoardObjGrpIfaceModel10Set',
            'voltPolicyBoardObjGrpIfaceModel10Set',
        ],

        # === task_nne ===
        'pmuRpcNneBoardObjGrpCmd' => [
            'nneVarBoardObjGrpIfaceModel10Set',
            'nneLayerBoardObjGrpIfaceModel10Set',
            'nneDescBoardObjGrpIfaceModel10Set',
        ],
    },

    'boardObjGrpIfaceModel10Set' => {
        # === task_perf ===
        'perfConstructPstates' => [
            'perfPstateIfaceModel10SetHeader',
            'perfPstateIfaceModel10SetEntry',
        ],
        'perfLimitBoardObjGrpIfaceModel10Set' => [
            's_perfLimitIfaceModel10SetHeader',
            's_perfLimitIfaceModel10SetEntry',
        ],
        'perfPolicyBoardObjGrpIfaceModel10Set' => [
            's_perfPolicyIfaceModel10SetHeader',
            's_perfPolicyIfaceModel10SetEntry',
        ],
        'pstateBoardObjGrpIfaceModel10Set' => [
            'perfPstateIfaceModel10SetHeader',
            'perfPstateIfaceModel10SetEntry',
        ],
        'vpstateBoardObjGrpIfaceModel10Set' => [
            's_vpstateIfaceModel10SetHeader',
            's_vpstateIfaceModel10SetEntry',
        ],
        'vfeEquBoardObjGrpIfaceModel10Set' => [
            's_vfeEquIfaceModel10SetHeader',
            's_vfeEquIfaceModel10SetEntry',
        ],
        'vfeVarBoardObjGrpIfaceModel10Set' => [
            's_vfeVarIfaceModel10SetHeader',
            's_vfeVarIfaceModel10SetEntry',
        ],
        'perfCfSensorBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            's_perfCfSensorIfaceModel10SetEntry',
        ],
        'perfCfPmSensorBoardObjGrpIfaceModel10Set' => [
            's_perfCfPmSensorIfaceModel10SetHeader',
            's_perfCfPmSensorIfaceModel10SetEntry',
        ],
        'perfCfTopologyBoardObjGrpIfaceModel10Set' => [
            's_perfCfTopologyIfaceModel10SetHeader',
            's_perfCfTopologyIfaceModel10SetEntry',
        ],
        'perfCfControllerBoardObjGrpIfaceModel10Set_1X' => [
            'perfCfControllerIfaceModel10SetHeader_SUPER',
            's_perfCfControllerIfaceModel10SetEntry_1X',
        ],
        'perfCfControllerBoardObjGrpIfaceModel10Set_2X' => [
            'perfCfControllerIfaceModel10SetHeader_SUPER',
            's_perfCfControllerIfaceModel10SetEntry_2X',
        ],
        'perfCfClientPerfCfControllerBoardObjGrpIfaceModel10Set' => [
            's_perfCfClientPerfCfControllerConstructHeader',
            's_perfCfClientPerfCfControllerIfaceModel10SetEntry',
        ],
        'perfCfPolicyBoardObjGrpIfaceModel10Set' => [
            's_perfCfPolicyIfaceModel10SetHeader',
            's_perfCfPolicyIfaceModel10SetEntry',
        ],
        'perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10Set' => [
            's_perfCfClientPerfCfPolicyIfaceModel10SetHeader',
            's_perfCfClientPerfCfPolicyIfaceModel10SetEntry',
        ],
        'perfCfPwrModelBoardObjGrpIfaceModel10Set' => [
            's_perfCfPwrModelIfaceModel10SetHeader',
            's_perfCfPwrModelIfaceModel10SetEntry',
        ],
        'clientPerfCfPwrModelProfileBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            's_clientPerfCfPwrModelProfileIfaceModel10SetEntry',
        ],
        'voltPolicyBoardObjGrpIfaceModel10Set' => [
            'voltIfaceModel10SetVoltPolicyHdr',
            'voltVoltPolicyIfaceModel10SetEntry',
        ],
        'voltDeviceBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            'voltVoltDeviceIfaceModel10SetEntry',
        ],
        'voltRailBoardObjGrpIfaceModel10Set' => [
            'voltVoltRailIfaceModel10SetHeader',
            'voltVoltRailIfaceModel10SetEntry',
        ],
        'clkPllDevBoardObjGrpIfaceModel10Set' => [
            's_clkPllDevIfaceModel10SetHeader',
            's_clkPllDevIfaceModel10SetEntry',
        ],
        'clkFreqControllerBoardObjGrpIfaceModel10Set' => [
             's_clkFreqControllerIfaceModel10SetHeader',
             's_clkFreqControllerIfaceModel10SetEntry',
        ],
        'clkVoltControllerBoardObjGrpIfaceModel10Set' => [
             's_clkVoltControllerIfaceModel10SetHeader',
             's_clkVoltControllerIfaceModel10SetEntry',
        ],
        'clkFreqDomainBoardObjGrpIfaceModel10Set' => [
            's_clkFreqDomainGrpIfaceModel10SetHeader',
            's_clkFreqDomainGrpIfaceModel10SetEntry',
        ],
        'clkVfPointBoardObjGrpIfaceModel10Set_30' => [
            'clkVfPointIfaceModel10SetHeader',
            'clkVfPointIfaceModel10SetEntry',
        ],
        'clkVfPointBoardObjGrpIfaceModel10Set_35' => [
            'clkVfPointIfaceModel10SetHeader',
            's_clkVfPointSecIfaceModel10SetHeader',
            'clkVfPointIfaceModel10SetEntry',
        ],
        'clkVfPointBoardObjGrpIfaceModel10Set_40' => [
            'clkVfPointIfaceModel10SetHeader',
            's_clkVfPointSecIfaceModel10SetHeader',
            'clkVfPointIfaceModel10SetEntry',
        ],
        'clientClkVfPointBoardObjGrpIfaceModel10Set' => [
            's_clientClkVfPointIfaceModel10SetHeader',
            's_clientClkVfPointIfaceModel10SetEntry',
        ],
        'clkNafllDevBoardObjGrpIfaceModel10Set' => [
            's_clkNafllIfaceModel10SetHeader',
            's_clkNafllIfaceModel10SetEntry',
        ],
        'clkAdcDevBoardObjGrpIfaceModel10Set' => [
            's_clkAdcDevIfaceModel10SetHeader',
            's_clkAdcDevIfaceModel10SetEntry',
        ],
        'clkProgBoardObjGrpIfaceModel10Set' => [
            's_clkProgIfaceModel10SetHeader',
            's_clkProgIfaceModel10SetEntry',
        ],
        'clkEnumBoardObjGrpIfaceModel10Set' => [
            's_clkEnumConstructHeader',
            's_clkEnumIfaceModel10SetEntry',
        ],
        'clkVfRelBoardObjGrpIfaceModel10Set' => [
            's_clkVfRelIfaceModel10SetHeader',
            's_clkVfRelIfaceModel10SetEntry',
        ],
        'clkPropRegimeBoardObjGrpIfaceModel10Set' => [
            's_clkPropRegimeIfaceModel10SetHeader',
            's_clkPropRegimeIfaceModel10SetEntry',
        ],
        'clkPropTopBoardObjGrpIfaceModel10Set' => [
            's_clkPropTopIfaceModel10SetHeader',
            's_clkPropTopIfaceModel10SetEntry',
        ],
        'clkPropTopRelBoardObjGrpIfaceModel10Set' => [
            's_clkPropTopRelIfaceModel10SetHeader',
            's_clkPropTopRelIfaceModel10SetEntry',
        ],
        'clkClientClkPropTopPolBoardObjGrpIfaceModel10Set' => [
            's_clkClientClkPropTopPolIfaceModel10SetHeader',
            's_clkClientClkPropTopPolIfaceModel10SetEntry',
        ],
        'clkDomainBoardObjGrpIfaceModel10Set' => [
            's_clkDomainIfaceModel10SetHeader',
            's_clkDomainIfaceModel10SetEntry',
        ],

        # === task_pmgr ===
        'pmgrConstructPwrPolicies' => [
            'boardObjGrpIfaceModel10SetHeader',
            'pmgrPwrPolicyRelIfaceModel10SetEntry',
            'pmgrPwrViolationIfaceModel10SetEntry',
        ],
        'pwrDeviceBoardObjGrpIfaceModel10Set' => [
            'pmgrPwrDevIfaceModel10SetHeader',
            'pmgrPwrDevIfaceModel10SetEntry',
        ],
        'pwrEquationBoardObjGrpIfaceModel10Set' => [
            'pmgrPwrEquationIfaceModel10SetHeader',
            'pmgrPwrEquationIfaceModel10SetEntry',
        ],
        'pwrChannelBoardObjGrpIfaceModel10Set' => [
            'pmgrPwrMonitorIfaceModel10SetHeader_SUPER',
            'pmgrPwrChannelIfaceModel10SetEntry',
            'boardObjGrpIfaceModel10SetHeader',
            'pmgrPwrChRelIfaceModel10SetEntry',
        ],
        'pwrPolicyBoardObjGrpIfaceModel10Set' => [
            'pmgrPwrPolicyIfaceModel10SetHeader',
            'pmgrPwrPolicyIfaceModel10SetEntry',
            'boardObjGrpIfaceModel10SetHeader',
            'pmgrPwrPolicyRelIfaceModel10SetEntry',
            'pmgrPwrViolationIfaceModel10SetEntry',
        ],
        'i2cDeviceBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            'i2cDeviceIfaceModel10SetEntry',
        ],
        'illumDeviceBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            's_illumDeviceIfaceModel10SetEntry',
        ],
        'illumZoneBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            's_illumZoneIfaceModel10SetEntry',
        ],

        # === task_therm ===
        'thrmMonitorBoardObjGrpIfaceModel10Set' => [
            'thrmThrmMonIfaceModel10SetHeader',
            'thrmThrmMonIfaceModel10SetEntry',
        ],
        'thermChannelBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            'thermThermChannelIfaceModel10SetEntry',
        ],
        'thermDeviceBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            'thermThermDeviceIfaceModel10SetEntry',
        ],
        'thermPolicyBoardObjGrpIfaceModel10Set' => [
            'thermThermPolicyIfaceModel10SetHeader',
            'thermThermPolicyIfaceModel10SetEntry',
        ],

        'fanCoolersBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            'fanFanCoolerIfaceModel10SetEntry',
        ],
        'fanPoliciesBoardObjGrpIfaceModel10Set' => [
            'fanFanPolicyIfaceModel10SetHeader',
            'fanFanPolicyIfaceModel10SetEntry',
        ],
        'fanArbitersBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            'fanFanArbiterIfaceModel10SetEntry',
        ],

        # === task_nne ===
        'nneVarBoardObjGrpIfaceModel10Set' => [
            's_nneVarIfaceModel10SetHeader',
            's_nneVarIfaceModel10SetEntry',
        ],
        'nneLayerBoardObjGrpIfaceModel10Set' => [
            's_nneLayerIfaceModel10SetHeader',
            's_nneLayerIfaceModel10SetEntry',
        ],
        'nneDescBoardObjGrpIfaceModel10Set' => [
            's_nneDescIfaceModel10SetHeader',
            's_nneDescIfaceModel10SetEntry',
        ],

        # === task_spi ===
        'spiDeviceBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            'spiDeviceIfaceModel10SetEntry',
        ],
    },

    'boardObjGrpIfaceModel10SetAutoDma' => {
        # === task_perf ===
        'vfeEquBoardObjGrpIfaceModel10Set' => [
            's_vfeEquIfaceModel10SetHeader',
            's_vfeEquIfaceModel10SetEntry',
        ],
        'voltPolicyBoardObjGrpIfaceModel10Set' => [
            'voltIfaceModel10SetVoltPolicyHdr',
            'voltVoltPolicyIfaceModel10SetEntry',
        ],
        'voltDeviceBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            'voltVoltDeviceIfaceModel10SetEntry',
        ],
        'voltRailBoardObjGrpIfaceModel10Set' => [
            'voltVoltRailIfaceModel10SetHeader',
            'voltVoltRailIfaceModel10SetEntry',
        ],
        'clkPllDevBoardObjGrpIfaceModel10Set' => [
            's_clkPllDevIfaceModel10SetHeader',
            's_clkPllDevIfaceModel10SetEntry',
        ],
        'clkFreqControllerBoardObjGrpIfaceModel10Set' => [
            's_clkFreqControllerIfaceModel10SetHeader',
            's_clkFreqControllerIfaceModel10SetEntry',
        ],
        'clkVoltControllerBoardObjGrpIfaceModel10Set' => [
            's_clkVoltControllerIfaceModel10SetHeader',
            's_clkVoltControllerIfaceModel10SetEntry',
        ],
        'clkFreqDomainBoardObjGrpIfaceModel10Set' => [
            's_clkFreqDomainGrpIfaceModel10SetHeader',
            's_clkFreqDomainGrpIfaceModel10SetEntry',
        ],
        'clientClkVfPointBoardObjGrpIfaceModel10Set' => [
            's_clientClkVfPointIfaceModel10SetHeader',
            's_clientClkVfPointIfaceModel10SetEntry',
        ],
        'clkNafllDevBoardObjGrpIfaceModel10Set' => [
            's_clkNafllIfaceModel10SetHeader',
            's_clkNafllIfaceModel10SetEntry',
        ],
        'clkAdcDevBoardObjGrpIfaceModel10Set' => [
            's_clkAdcDevIfaceModel10SetHeader',
            's_clkAdcDevIfaceModel10SetEntry',
        ],
        'clkProgBoardObjGrpIfaceModel10Set' => [
            's_clkProgIfaceModel10SetHeader',
            's_clkProgIfaceModel10SetEntry',
        ],
        'clkEnumBoardObjGrpIfaceModel10Set' => [
            's_clkEnumConstructHeader',
            's_clkEnumIfaceModel10SetEntry',
        ],
        'clkVfRelBoardObjGrpIfaceModel10Set' => [
            's_clkVfRelIfaceModel10SetHeader',
            's_clkVfRelIfaceModel10SetEntry',
        ],
        'clkPropRegimeBoardObjGrpIfaceModel10Set' => [
            's_clkPropRegimeIfaceModel10SetHeader',
            's_clkPropRegimeIfaceModel10SetEntry',
        ],
        'clkPropTopBoardObjGrpIfaceModel10Set' => [
            's_clkPropTopIfaceModel10SetHeader',
            's_clkPropTopIfaceModel10SetEntry',
        ],
        'clkPropTopRelBoardObjGrpIfaceModel10Set' => [
            's_clkPropTopRelIfaceModel10SetHeader',
            's_clkPropTopRelIfaceModel10SetEntry',
        ],
        'clkClientClkPropTopPolBoardObjGrpIfaceModel10Set' => [
            's_clkClientClkPropTopPolIfaceModel10SetHeader',
            's_clkClientClkPropTopPolIfaceModel10SetEntry',
        ],
        'clkDomainBoardObjGrpIfaceModel10Set' => [
            's_clkDomainIfaceModel10SetHeader',
            's_clkDomainIfaceModel10SetEntry',
        ],

        # === task_pmgr ===
        'pwrDeviceBoardObjGrpIfaceModel10Set' => [
            'pmgrPwrDevIfaceModel10SetHeader',
            'pmgrPwrDevIfaceModel10SetEntry',
        ],
        'pwrEquationBoardObjGrpIfaceModel10Set' => [
            'pmgrPwrEquationIfaceModel10SetHeader',
            'pmgrPwrEquationIfaceModel10SetEntry',
        ],
        'i2cDeviceBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            'i2cDeviceIfaceModel10SetEntry',
        ],
        'illumDeviceBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            's_illumDeviceIfaceModel10SetEntry',
        ],
        'illumZoneBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            's_illumZoneIfaceModel10SetEntry',
        ],

        # === task_therm ===
        'thrmMonitorBoardObjGrpIfaceModel10Set' => [
            'thrmThrmMonIfaceModel10SetHeader',
            'thrmThrmMonIfaceModel10SetEntry',
        ],
        'thermChannelBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            'thermThermChannelIfaceModel10SetEntry',
        ],
        'thermDeviceBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            'thermThermDeviceIfaceModel10SetEntry',
        ],
        'thermPolicyBoardObjGrpIfaceModel10Set' => [
            'thermThermPolicyIfaceModel10SetHeader',
            'thermThermPolicyIfaceModel10SetEntry',
        ],
        'fanCoolersBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            'fanFanCoolerIfaceModel10SetEntry',
        ],
        'fanPoliciesBoardObjGrpIfaceModel10Set' => [
            'fanFanPolicyIfaceModel10SetHeader',
            'fanFanPolicyIfaceModel10SetEntry',
        ],
        'fanArbitersBoardObjGrpIfaceModel10Set' => [
            'boardObjGrpIfaceModel10SetHeader',
            'fanFanArbiterIfaceModel10SetEntry',
        ],
        # === task_nne ===
        'nneVarBoardObjGrpIfaceModel10Set' => [
            's_nneVarIfaceModel10SetHeader',
            's_nneVarIfaceModel10SetEntry',
        ],
        'nneLayerBoardObjGrpIfaceModel10Set' => [
            's_nneLayerIfaceModel10SetHeader',
            's_nneLayerIfaceModel10SetEntry',
        ],
        'nneDescBoardObjGrpIfaceModel10Set' => [
            's_nneDescIfaceModel10SetHeader',
            's_nneDescIfaceModel10SetEntry',
        ],
    },

    'boardObjGrpIfaceModel10GetStatus_SUPER' => {
        # === task_perf ===
        'perfLimitBoardObjGrpIfaceModel10GetStatus' => [
            's_perfLimitIfaceModel10GetStatusHeader',
            'perfLimitIfaceModel10GetStatus',
        ],
        'perfPolicyBoardObjGrpIfaceModel10GetStatus' => [
            's_perfPolicyIfaceModel10GetStatusHeader',
            's_perfPolicyIfaceModel10GetStatus',
        ],
        'vfeVarBoardObjGrpIfaceModel10GetStatus' => [
            's_vfeVarIfaceModel10GetStatus',
        ],
        'perfCfSensorBoardObjGrpIfaceModel10GetStatus' => [
            's_perfCfSensorIfaceModel10GetStatusHeader',
            's_perfCfSensorIfaceModel10GetStatus',
        ],
        'perfCfPmSensorBoardObjGrpIfaceModel10GetStatus' => [
            's_perfCfPmSensorIfaceModel10GetStatusHeader',
            's_perfCfPmSensorIfaceModel10GetStatus',
            's_perfCfPmSensorsClwOobrcDataReady',
        ],
        'perfCfTopologyBoardObjGrpIfaceModel10GetStatus' => [
            's_perfCfTopologyIfaceModel10GetStatusHeaderSense',
            's_perfCfTopologyIfaceModel10GetStatusHeaderRef',
            's_perfCfTopologyIfaceModel10GetStatus',
        ],
        'perfCfControllerBoardObjGrpIfaceModel10GetStatus_1X' => [
            'perfCfControllerIfaceModel10GetStatusHeader_SUPER',
            'perfCfControllerIfaceModel10GetStatus_1X',
        ],
        'perfCfControllerBoardObjGrpIfaceModel10GetStatus_2X' => [
            'perfCfControllerIfaceModel10GetStatusHeader_SUPER',
            'perfCfControllerIfaceModel10GetStatus_2X',
        ],
        'perfCfClientPerfCfControllerBoardObjGrpIfaceModel10GetStatus' => [
            's_perfCfClientPerfCfControllerIfaceModel10GetStatusHeader',
            's_perfCfClientPerfCfControllerIfaceModel10GetStatus',
        ],
        'perfCfPolicyBoardObjGrpIfaceModel10GetStatus' => [
            's_perfCfPolicyIfaceModel10GetStatusHeader',
            's_perfCfPolicyIfaceModel10GetStatus',
        ],
        'perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10GetStatus' => [
            's_perfCfClientPerfCfPolicyIfaceModel10GetStatusHeader',
            's_perfCfClientPerfCfPolicyIfaceModel10GetStatus',
        ],
        'perfCfPwrModelBoardObjGrpIfaceModel10GetStatus' => [
            's_perfCfPwrModelIfaceModel10GetStatus',
        ],
        'pstateBoardObjGrpIfaceModel10GetStatus' => [
            'pstateIfaceModel10GetStatus',
        ],
        'voltPolicyBoardObjGrpIfaceModel10GetStatus' => [
            'voltVoltPolicyIfaceModel10GetStatus',
        ],
        'voltRailBoardObjGrpIfaceModel10GetStatus' => [
            's_voltRailIfaceModel10GetStatus',
        ],
        'clkFreqControllerBoardObjGrpIfaceModel10GetStatus' => [
            's_clkFreqControllerIfaceModel10GetStatusHeader',
            's_clkFreqControllerIfaceModel10GetStatus',
        ],
        'clkVoltControllerBoardObjGrpIfaceModel10GetStatus' => [
            's_clkVoltControllerIfaceModel10GetStatusHeader',
            's_clkVoltControllerIfaceModel10GetStatus',
        ],
        'clkFreqDomainBoardObjGrpIfaceModel10GetStatus' => [
            's_clkFreqDomainGrpIfaceModel10GetStatusHeader',
            's_clkFreqDomainIfaceModel10GetStatus',
        ],
        'clkVfPointBoardObjGrpIfaceModel10GetStatus_30' => [
            'clkVfPointIfaceModel10GetStatusHeader',
            'clkVfPointIfaceModel10GetStatus',
        ],
        'clkVfPointBoardObjGrpIfaceModel10GetStatus_35' => [
            'clkVfPointIfaceModel10GetStatusHeader',
            'clkVfPointIfaceModel10GetStatus',
        ],
        'clkVfPointBoardObjGrpIfaceModel10GetStatus_40' => [
            'clkVfPointIfaceModel10GetStatusHeader',
            'clkVfPointIfaceModel10GetStatus',
        ],
        'clientClkVfPointBoardObjGrpIfaceModel10GetStatus' => [
            's_clientClkVfPointIfaceModel10GetStatusHeader',
            's_clientClkVfPointIfaceModel10GetStatus',
        ],
        'clkNafllDevicesBoardObjGrpIfaceModel10GetStatus' => [
            'clkNafllDeviceIfaceModel10GetStatus',
        ],
        'clkAdcDevicesBoardObjGrpIfaceModel10GetStatus' => [
            's_clkAdcDeviceIfaceModel10GetStatus',
        ],
        'clkProgBoardObjGrpIfaceModel10GetStatus' => [
            's_clkProgIfaceModel10GetStatus',
        ],
        'clkVfRelBoardObjGrpIfaceModel10GetStatus' => [
            's_clkVfRelIfaceModel10GetStatus',
        ],
        'clkPropTopBoardObjGrpIfaceModel10GetStatus' => [
            's_clkPropTopIfaceModel10GetStatusHeader',
        ],

        # === task_pmgr ===
        'pwrPolicyBoardObjGrpIfaceModel10GetStatus' => [
            'pmgrPwrPolicyIfaceModel10GetStatusHeader',
            'pwrPolicyIfaceModel10GetStatus',
            'pwrPolicyRelationshipIfaceModel10GetStatus',
            'pwrViolationIfaceModel10GetStatus',
        ],
        'pwrDeviceBoardObjGrpIfaceModel10GetStatus' => [
            'pwrDevIfaceModel10GetStatus',
        ],
        'pwrChannelBoardObjGrpIfaceModel10GetStatus' => [
            'pwrChannelIfaceModel10GetStatusHeaderPhysical',
            'pwrChannelIfaceModel10GetStatusHeaderLogical',
            'pwrChannelIfaceModel10GetStatus',
            'pwrChRelationshipIfaceModel10GetStatus',
            'pwrChannelGetStatusHeaderRels',
        ],

        # === task_therm ===
        'thermChannelBoardObjGrpIfaceModel10GetStatus' => [
            'thermThermChannelIfaceModel10GetStatus',
        ],
        'thrmMonitorBoardObjGrpIfaceModel10GetStatus' => [
            'thrmThrmMonIfaceModel10GetStatusHeader',
            'thrmThrmMonIfaceModel10GetStatus',
        ],
        'thermPolicyBoardObjGrpIfaceModel10GetStatus' => [
            'thermThermPolicyIfaceModel10GetStatusHeader',
            'thermThermPolicyIfaceModel10GetStatus',
        ],
        'fanCoolersBoardObjGrpIfaceModel10GetStatus' => [
            'fanFanCoolerIfaceModel10GetStatus',
        ],
        'fanPoliciesBoardObjGrpIfaceModel10GetStatus' => [
             'fanFanPolicyIfaceModel10GetStatus',
        ],
        'fanArbitersBoardObjGrpIfaceModel10GetStatus' => [
             'fanFanArbiterIfaceModel10GetStatus',
        ],
    },

    'boardObjGrpIfaceModel10GetStatusAutoDma_SUPER' => {
        # === task_perf ===
        'voltPolicyBoardObjGrpIfaceModel10GetStatus' => [
            'voltVoltPolicyIfaceModel10GetStatus',
        ],
        'voltRailBoardObjGrpIfaceModel10GetStatus' => [
            's_voltRailIfaceModel10GetStatus',
        ],
        'clkFreqControllerBoardObjGrpIfaceModel10GetStatus' => [
            's_clkFreqControllerIfaceModel10GetStatusHeader',
            's_clkFreqControllerIfaceModel10GetStatus',
        ],
        'clkVoltControllerBoardObjGrpIfaceModel10GetStatus' => [
            's_clkVoltControllerIfaceModel10GetStatusHeader',
            's_clkVoltControllerIfaceModel10GetStatus',
        ],
        'clkFreqDomainBoardObjGrpIfaceModel10GetStatus' => [
            's_clkFreqDomainGrpIfaceModel10GetStatusHeader',
            's_clkFreqDomainIfaceModel10GetStatus',
        ],
        'clientClkVfPointBoardObjGrpIfaceModel10GetStatus' => [
            's_clientClkVfPointIfaceModel10GetStatusHeader',
            's_clientClkVfPointIfaceModel10GetStatus',
        ],
        'clkNafllDevicesBoardObjGrpIfaceModel10GetStatus' => [
            'clkNafllDeviceIfaceModel10GetStatus',
        ],
        'clkAdcDevicesBoardObjGrpIfaceModel10GetStatus' => [
            's_clkAdcDeviceIfaceModel10GetStatus',
        ],
        # === task_pmgr ===
        'pwrDeviceBoardObjGrpIfaceModel10GetStatus' => [
            'pwrDevIfaceModel10GetStatus',
        ],
        'clkProgBoardObjGrpIfaceModel10GetStatus' => [
            's_clkProgIfaceModel10GetStatus',
        ],
        'clkVfRelBoardObjGrpIfaceModel10GetStatus' => [
            's_clkVfRelIfaceModel10GetStatus',
        ],
        'clkPropTopBoardObjGrpIfaceModel10GetStatus' => [
            's_clkPropTopIfaceModel10GetStatusHeader',
        ],

        # === task_therm ===
        'thermChannelBoardObjGrpIfaceModel10GetStatus' => [
            'thermThermChannelIfaceModel10GetStatus',
        ],
        'thrmMonitorBoardObjGrpIfaceModel10GetStatus' => [
            'thrmThrmMonIfaceModel10GetStatusHeader',
            'thrmThrmMonIfaceModel10GetStatus',
        ],
        'thermPolicyBoardObjGrpIfaceModel10GetStatus' => [
            'thermThermPolicyIfaceModel10GetStatusHeader',
            'thermThermPolicyIfaceModel10GetStatus',
        ],
        'fanCoolersBoardObjGrpIfaceModel10GetStatus' => [
            'fanFanCoolerIfaceModel10GetStatus',
        ],
        'fanPoliciesBoardObjGrpIfaceModel10GetStatus' => [
            'fanFanPolicyIfaceModel10GetStatus',
        ],
        'fanArbitersBoardObjGrpIfaceModel10GetStatus' => [
            'fanFanArbiterIfaceModel10GetStatus',
        ],
    },

    'boardObjDynamicCast_IMPL' => {
        'clkVfPointGrpIfaceModel10ObjSet_30_FREQ' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPoint30Cache_FREQ' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointGrpIfaceModel10ObjSet_30_VOLT' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPoint30Cache_VOLT' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointVoltageuVGet_30_VOLT' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointIfaceModel10GetStatus_30' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointAccessor_30' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointVoltageuVGet_30' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointGrpIfaceModel10ObjSet_35_FREQ' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointIfaceModel10GetStatus_35_FREQ' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointLoad_35_FREQ' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPoint35Cache_FREQ' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointIfaceModel10GetStatus_35_VOLT_PRI' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPoint35Cache_VOLT_PRI' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointGrpIfaceModel10ObjSet_35_VOLT_SEC' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointIfaceModel10GetStatus_35_VOLT_SEC' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPoint35Cache_VOLT_SEC' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPoint35FreqTupleAccessor_VOLT_SEC' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointGrpIfaceModel10ObjSet_35_VOLT' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointLoad_35_VOLT_IMPL' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointVoltageuVGet_35_VOLT_IMPL' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointAccessor_35' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointVoltageuVGet_35_IMPL' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointGrpIfaceModel10ObjSet_40_FREQ' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointIfaceModel10GetStatus_40_FREQ' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointIfaceModel10GetStatus_40_VOLT_PRI' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointGrpIfaceModel10ObjSet_40_VOLT_SEC' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointIfaceModel10GetStatus_40_VOLT_SEC' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointGrpIfaceModel10ObjSet_40_VOLT' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPoint40Load_VOLT' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointVoltageuVGet_40_VOLT' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointVoltageuVGet_40' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'clkVfPointGrpIfaceModel10ObjSet_SUPER' => [
            's_vfeVarDynamicCast_30_FREQ',
            's_vfeVarDynamicCast_30_VOLT',
            's_vfeVarDynamicCast_35_FREQ',
            's_vfeVarDynamicCast_35_VOLT_PRI',
            's_vfeVarDynamicCast_35_VOLT_SEC',
            's_vfeVarDynamicCast_40_FREQ',
            's_vfeVarDynamicCast_40_VOLT_PRI',
            's_vfeVarDynamicCast_40_VOLT_SEC',
        ],
        'perfPstateGrpIfaceModel10ObjSet_SUPER' => [
            's_perfPstateDynamicCast_35',
        ],
        'perfPstateGrpIfaceModel10ObjSet_35' => [
            's_perfPstateDynamicCast_35',
        ],
        'perfPstateGetLevelByIndex' => [
            's_perfPstateDynamicCast_35',
        ],
        's_perfCfControllersCallback' => [
            's_perfPstateDynamicCast_35',
        ],
        'perfPstateClkFreqGet_35_IMPL' => [
            's_perfPstateDynamicCast_35',
        ],
        'perfPstateGetByLevel' => [
            's_perfPstateDynamicCast_35',
        ],
        'perfPstatesCache' => [
            's_perfPstateDynamicCast_35',
        ],
        'perfLimitsFreqkHzQuantize' => [
            's_perfPstateDynamicCast_35',
        ],
        'perfLimitsFreqkHzToPstateIdxRange' => [
            's_perfPstateDynamicCast_35',
        ],
        'pstateIfaceModel10GetStatus_35' => [
            's_perfPstateDynamicCast_35',
        ],
        'vfeEquGrpIfaceModel10ObjSetImpl_COMPARE' => [
            's_vfeEquDynamicCast_COMPARE',
            's_vfeEquDynamicCast_MINMAX',
            's_vfeEquDynamicCast_QUADRATIC',
            's_vfeEquDynamicCast_EQUATION_SCALAR',
        ],
        'vfeEquEvalNode_COMPARE_IMPL' => [
            's_vfeEquDynamicCast_COMPARE',
            's_vfeEquDynamicCast_MINMAX',
            's_vfeEquDynamicCast_QUADRATIC',
            's_vfeEquDynamicCast_EQUATION_SCALAR',
        ],
        'vfeEquGrpIfaceModel10ObjSetImpl_MINMAX' => [
            's_vfeEquDynamicCast_COMPARE',
            's_vfeEquDynamicCast_MINMAX',
            's_vfeEquDynamicCast_QUADRATIC',
            's_vfeEquDynamicCast_EQUATION_SCALAR',
        ],
        'vfeEquEvalNode_MINMAX' => [
            's_vfeEquDynamicCast_COMPARE',
            's_vfeEquDynamicCast_MINMAX',
            's_vfeEquDynamicCast_QUADRATIC',
            's_vfeEquDynamicCast_EQUATION_SCALAR',
        ],
        'vfeEquGrpIfaceModel10ObjSetImpl_QUADRATIC' => [
            's_vfeEquDynamicCast_COMPARE',
            's_vfeEquDynamicCast_MINMAX',
            's_vfeEquDynamicCast_QUADRATIC',
            's_vfeEquDynamicCast_EQUATION_SCALAR',
        ],
        'vfeEquEvalNode_QUADRATIC' => [
            's_vfeEquDynamicCast_COMPARE',
            's_vfeEquDynamicCast_MINMAX',
            's_vfeEquDynamicCast_QUADRATIC',
            's_vfeEquDynamicCast_EQUATION_SCALAR',
        ],
        'vfeEquGrpIfaceModel10ObjSetImpl_EQUATION_SCALAR' => [
            's_vfeEquDynamicCast_COMPARE',
            's_vfeEquDynamicCast_MINMAX',
            's_vfeEquDynamicCast_QUADRATIC',
            's_vfeEquDynamicCast_EQUATION_SCALAR',
        ],
        'vfeEquEvalNode_EQUATION_SCALAR' => [
            's_vfeEquDynamicCast_COMPARE',
            's_vfeEquDynamicCast_MINMAX',
            's_vfeEquDynamicCast_QUADRATIC',
            's_vfeEquDynamicCast_EQUATION_SCALAR',
        ],
        'vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_PRODUCT' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarEval_DERIVED_PRODUCT' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_SUM' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarEval_DERIVED_SUM' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_CALLER_SPECIFIED' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarSingleClientInputMatch_SINGLE_CALLER_SPECIFIED' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_FREQUENCY' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarSingleClientInputMatch_SINGLE_FREQUENCY' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarEval_SINGLE_SENSED_FUSE' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_TEMP' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarEval_SINGLE_SENSED_TEMP' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarSample_SINGLE_SENSED_TEMP' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarGrpIfaceModel10ObjSetImpl_SINGLE' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarEval_SINGLE' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarClientValuesLink' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarEval_SINGLE_CALLER' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'vfeVarGrpIfaceModel10ObjSetImpl_SUPER' => [
            's_vfeVarDynamicCast_DERIVED_PRODUCT',
            's_vfeVarDynamicCast_DERIVED_SUM',
            's_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED',
            's_vfeVarDynamicCast_SINGLE_FREQUENCY',
            's_vfeVarDynamicCast_SINGLE_VOLTAGE',
            's_vfeVarDynamicCast_SINGLE_SENSED_FUSE',
            's_vfeVarDynamicCast_SINGLE_SENSED_TEMP',
        ],
        'voltDeviceGrpIfaceModel10ObjSetImpl_SUPER' => [
            's_voltDeviceDynamicCast_PWM',
        ],
        'voltDeviceGrpIfaceModel10ObjSetImpl_PWM' => [
            's_voltDeviceDynamicCast_PWM',
        ],
        'voltDeviceGetVoltage_PWM' => [
            's_voltDeviceDynamicCast_PWM',
        ],
        'voltDeviceConfigure_PWM' => [
            's_voltDeviceDynamicCast_PWM',
        ],
        'voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL' => [
            's_voltPolicyDynamicCast_SINGLE_RAIL',
        ],
        'voltPolicyIfaceModel10GetStatus_SINGLE_RAIL' => [
            's_voltPolicyDynamicCast_SINGLE_RAIL',
        ],
        'voltPolicyLoad_SINGLE_RAIL_IMPL' => [
            's_voltPolicyDynamicCast_SINGLE_RAIL',
        ],
        'voltPolicySanityCheck_SINGLE_RAIL' => [
            's_voltPolicyDynamicCast_SINGLE_RAIL',
        ],
        'voltPolicySetVoltage_SINGLE_RAIL_IMPL' => [
            's_voltPolicyDynamicCast_SINGLE_RAIL',
        ],
        'voltPolicyOffsetVoltage_SINGLE_RAIL' => [
            's_voltPolicyDynamicCast_SINGLE_RAIL',
        ],
        'clkAdcDevGrpIfaceModel10ObjSetImpl_V20' => [
            's_clkAdcDeviceDynamicCast_V20',
        ],
        'clkAdcDeviceIfaceModel10GetStatusEntry_V20' => [
            's_clkAdcDeviceDynamicCast_V20',
        ],
        'clkNafllGrpIfaceModel10ObjSet_SUPER' => [
            's_clkNafllDeviceDynamicCast_V20',
        ],
        'clkNafllGrpIfaceModel10ObjSet_V20' => [
            's_clkNafllDeviceDynamicCast_V20',
        ],
        'clkNafllDeviceIfaceModel10GetStatus' => [
            's_clkNafllDeviceDynamicCast_V20',
        ],
    },

    # start PMGR/PWR_POLICY -------------------------------------------------------
    # virtual pointers.
    'pwrPolicy3XChannelMaskGet' => {
        'ALL' => [
            's_pwrPolicyGetInterfaceFromBoardObj_WorkloadMultiRail',
            's_pwrPolicyGetInterfaceFromBoardObj_TGP',
        ],
    },

    'pwrPolicy3XFilter' => {
        'ALL' => [
            's_pwrPolicyGetInterfaceFromBoardObj_TGP',
        ],
    },

    'pwrPolicyLimitEvaluate_TOTAL_GPU' => {
        'ALL' => [
            's_pwrPolicyGetInterfaceFromBoardObj_TGP',
        ],
    },

    'pwrPolicyFilter_TOTAL_GPU' => {
        'ALL' => [
            's_pwrPolicyGetInterfaceFromBoardObj_TGP',
        ],
    },

    'pwrPolicy3XFilter_TOTAL_GPU' => {
        'ALL' => [
            's_pwrPolicyGetInterfaceFromBoardObj_TGP',
        ],
    },

    # end PWR_POLICY ---------------------------------------------------------

    # === FAN_COOLER, FAN_POLICY and FAN_ARBITER virtual pointers ===

    'fanPolicyPwrChannelQueryResponse_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE' => {
        'ALL' => [
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V20',
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V30',
        ],
    },

    'fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE' => {
        'ALL' => [
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V20',
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V30',
        ],
    },

    'fanPolicyCallback_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE' => {
        'ALL' => [
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V20',
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V30',
        ],
    },

    'fanPolicyFanPwmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE' => {
        'ALL' => [
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V20',
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V30',
        ],
    },

    'fanPolicyFanRpmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE' => {
        'ALL' => [
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V20',
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V30',
        ],
    },

    'fanPolicyFanPwmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE' => {
        'ALL' => [
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V20',
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V30',
        ],
    },

    'fanPolicyFanRpmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE' => {
        'ALL' => [
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V20',
            's_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V30',
        ],
    },

    # === End of FAN_COOLER, FAN_POLICY and FAN_ARBITER virtual pointers ===

    # Clocks 3.x ---------------------------------------------------------
    # Generic client interfaces.

    # clkRead_FreqDomain_VIP

    'clkReadDomainList_PerfDaemon' => {
        'ALL' => [
            'clkRead_NolwolatileFreqDomain',
            'clkRead_VolatileFreqDomain',
            'clkRead_StubFreqDomain',
        ],
    },

    'clkFreqDomainBoardObjGrpIfaceModel10Set' => {
        'ALL' => [
            'clkRead_NolwolatileFreqDomain',
            'clkRead_VolatileFreqDomain',
        ],
    },

    # clkConfig_FreqDomain_VIP and clkProgram_FreqDomain_VIP

    'clkProgramDomainList_PerfDaemon' => {
        'ALL' => [
            'clkConfig_FixedFreqDomain',
            'clkConfig_StubFreqDomain',         # Temporary
            'clkConfig_DispFreqDomain',         # Turing only
            'clkConfig_OsmFreqDomain',          # Turing only
            'clkConfig_SwDivFreqDomain',        # Ampere and after
            'clkConfig_SwDivHPllFreqDomain',    # Ampere and after (except Ada)
            'clkConfig_SwDivAPllFreqDomain',    # Ada
            'clkConfig_BifFreqDomain',
            'clkConfig_SingleNafllFreqDomain',
            'clkConfig_MultiNafllFreqDomain',
            'clkConfig_MclkFreqDomain',
            'clkProgram_FixedFreqDomain',
            'clkProgram_ProgrammableFreqDomain',
            'clkProgram_StubFreqDomain',
            'clkProgram_SingleNafllFreqDomain',
            'clkProgram_MultiNafllFreqDomain',
            'clkProgram_MclkFreqDomain',
        ],
    },

    # Frequency Domain Virtual Interface Points

    # clkRead_Freqsrc_VIP

    'clkRead_VolatileFreqDomain' => {
        'ALL' => [
            'clkRead_Bif',
            'clkRead_ADynRamp',
            'clkRead_HPll',
            'clkRead_LdivUnit',
            'clkRead_LdivV2',
            'clkRead_Multiplier',
            'clkRead_Divider',
            'clkRead_Mux',
            'clkRead_Nafll',
            'clkRead_APll',
            'clkRead_ReadOnly',
            'clkRead_ASdm',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    # clkConfig_Freqsrc_VIP

    'clkConfig_FixedFreqDomain' => {
        'ALL' => [
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_DispFreqDomain' => {         # Turing only
        'ALL' => [
            'clkConfig_ADynRamp',
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_OsmFreqDomain' => {          # Turing only
        'ALL' => [
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_SwDivFreqDomain' => {        # Ampere and after
        'ALL' => [
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_SwDivHPllFreqDomain' => {    # Ampere and after (except Ada)
        'ALL' => [
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_SwDivAPllFreqDomain' => {    # Ada
        'ALL' => [
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_BifFreqDomain' => {
        'ALL' => [
            'clkConfig_Bif',
        ],
    },

    'clkConfig_SingleNafllFreqDomain' => {
        'ALL' => [
            'clkConfig_Nafll',
        ],
    },

    'clkConfig_MultiNafllFreqDomain' => {
        'ALL' => [
            'clkConfig_Nafll',
        ],
    },

    # clkProgram_Freqsrc_VIP

    'clkProgram_ProgrammableFreqDomain' => {
        'ALL' => [
            'clkProgram_Bif',
            'clkProgram_ADynRamp',
            'clkProgram_FreqSrc',
            'clkProgram_LdivUnit',
            'clkProgram_Mux',
            'clkProgram_Nafll',
            'clkProgram_APll',
            'clkProgram_ReadOnly',
            'clkProgram_Wire',
            'clkProgram_Xtal',
            'clkCleanup_Bif',
            'clkCleanup_ADynRamp',
            'clkCleanup_FreqSrc',
            'clkCleanup_LdivUnit',
            'clkCleanup_Mux',
            'clkCleanup_Nafll',
            'clkCleanup_APll',
            'clkCleanup_ReadOnly',
            'clkCleanup_ASdm',
            'clkCleanup_Wire',
            'clkCleanup_Xtal',
        ],
    },

    # clkPrint_Freqsrc_VIP

    'clkPrintAll_FreqDomain' => {
        'ALL' => [
            'clkPrint_Wire',
        ],
    },


    # Clocks 3.x ---------------------------------------------------------
    # Frequency Source Virtual Interface Points

    # clkRead_FreqSrc_VIP

    'clkConstruct_SchematicDag' => {
        'ALL' => [
            'clkRead_ReadOnly',
        ],
    },

    'clkRead_APll' => {
        'ALL' => [
            'clkRead_LdivUnit',
            'clkRead_LdivV2',
            'clkRead_Multiplier',
            'clkRead_Divider',
            'clkRead_Mux',
            'clkRead_APll',
            'clkRead_ReadOnly',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    'clkRead_LdivUnit' => {
        'ALL' => [
            'clkRead_LdivUnit',
            'clkRead_Multiplier',
            'clkRead_Divider',
            'clkRead_Mux',
            'clkRead_APll',
            'clkRead_ReadOnly',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    'clkRead_LdivV2' => {
        'ALL' => [
            'clkRead_LdivV2',
            'clkRead_Multiplier',
            'clkRead_Divider',
            'clkRead_Mux',
            'clkRead_APll',
            'clkRead_ReadOnly',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    'clkRead_Multiplier' => {
        'ALL' => [
            'clkRead_LdivUnit',
            'clkRead_LdivV2',
            'clkRead_Multiplier',
            'clkRead_Mux',
            'clkRead_APll',
            'clkRead_ReadOnly',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    'clkRead_Divider' => {
        'ALL' => [
            'clkRead_LdivUnit',
            'clkRead_LdivV2',
            'clkRead_Divider',
            'clkRead_Mux',
            'clkRead_APll',
            'clkRead_ReadOnly',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    'clkRead_Mux' => {
        'ALL' => [
            'clkRead_LdivUnit',
            'clkRead_LdivV2',
            'clkRead_Multiplier',
            'clkRead_Divider',
            'clkRead_Mux',
            'clkRead_PDiv',
            'clkRead_APll',
            'clkRead_ReadOnly',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    'clkRead_APll' => {
        'ALL' => [
            'clkRead_LdivUnit',
            'clkRead_LdivV2',
            'clkRead_Multiplier',
            'clkRead_Divider',
            'clkRead_Mux',
            'clkRead_ReadOnly',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    'clkRead_ReadOnly' => {
        'ALL' => [
            'clkRead_LdivUnit',
            'clkRead_LdivV2',
            'clkRead_Multiplier',
            'clkRead_Divider',
            'clkRead_Mux',
            'clkRead_APll',
            'clkRead_RoPDiv',
            'clkRead_RoVco',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    'clkRead_Wire' => {
        'ALL' => [
            'clkRead_LdivUnit',
            'clkRead_LdivV2',
            'clkRead_Multiplier',
            'clkRead_Divider',
            'clkRead_Mux',
            'clkRead_APll',
            'clkRead_ReadOnly',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    'clkRead_Xtal' => {
        'ALL' => [
            'clkRead_LdivUnit',
            'clkRead_LdivV2',
            'clkRead_Multiplier',
            'clkRead_Divider',
            'clkRead_Mux',
            'clkRead_APll',
            'clkRead_ReadOnly',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    'clkRead_Bif' => {
        'ALL' => [
            'clkRead_FreqSrc',
            'clkRead_Bif',
        ],
    },

    'clkRead_RoPDiv' => {
        'ALL' => [
            'clkRead_APll',
            'clkRead_RoVco',
            'clkRead_RoSdm',
        ],
    },

    'clkRead_PDiv' => {
        'ALL' => [
            'clkRead_RoVco',
            'clkRead_RoSdm',
        ],
    },

    'clkRead_ADynRamp' => {
        'ALL' => [
            'clkRead_ADynRamp',
            'clkRead_LdivUnit',
            'clkRead_LdivV2',
            'clkRead_Multiplier',
            'clkRead_Divider',
            'clkRead_Mux',
            'clkRead_APll',
            'clkRead_ReadOnly',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    'clkRead_HPll' => {
        'ALL' => [
            'clkRead_Mux',
            'clkRead_APll',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    'clkRead_ASdm' => {
        'ALL' => [
            'clkRead_ASdm',
            'clkRead_LdivUnit',
            'clkRead_LdivV2',
            'clkRead_Multiplier',
            'clkRead_Divider',
            'clkRead_Mux',
            'clkRead_APll',
            'clkRead_ReadOnly',
            'clkRead_Wire',
            'clkRead_Xtal',
        ],
    },

    # clkConfig_FreqSrc_VIP

    'clkConfig_LdivUnit' => {
        'ALL' => [
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_LdivV2' => {
        'ALL' => [
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_Multiplier' => {
        'ALL' => [
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_Divider' => {
        'ALL' => [
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_Mux' => {
        'ALL' => [
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_PDiv',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_APll' => {
        'ALL' => [
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_ReadOnly' => {
        'ALL' => [
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_Wire' => {
        'ALL' => [
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_Xtal' => {
        'ALL' => [
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    'clkConfig_Bif' => {
        'ALL' => [
            'clkConfig_FreqSrc',
            'clkConfig_Bif',
        ],
    },

    'clkConfig_Nafll' => {
        'ALL' => [
            'clkConfig_FreqSrc',
            'clkConfig_Nafll',
        ],
    },

    'clkConfig_ADynRamp' => {
        'ALL' => [
            'clkConfig_ADynRamp',
            'clkConfig_LdivUnit',
            'clkConfig_LdivV2',
            'clkConfig_Multiplier',
            'clkConfig_Divider',
            'clkConfig_Mux',
            'clkConfig_APll',
            'clkConfig_ReadOnly',
            'clkConfig_Wire',
            'clkConfig_Xtal',
        ],
    },

    # clkProgram_FreqSrc_VIP

    'clkProgram_FreqSrc' => {
        'ALL' => [
            'clkProgram_FreqSrc',
            'clkProgram_LdivUnit',
            'clkProgram_Mux',
            'clkProgram_APll',
            'clkProgram_ReadOnly',
            'clkProgram_Wire',
        ],
    },

    'clkProgram_LdivUnit' => {
        'ALL' => [
            'clkProgram_FreqSrc',
            'clkProgram_LdivUnit',
            'clkProgram_Mux',
            'clkProgram_APll',
            'clkProgram_ReadOnly',
            'clkProgram_Wire',
        ],
    },

    'clkProgram_Mux' => {
        'ALL' => [
            'clkProgram_FreqSrc',
            'clkProgram_LdivUnit',
            'clkProgram_Mux',
            'clkProgram_APll',
            'clkProgram_ReadOnly',
            'clkProgram_Wire',
        ],
    },

    'clkProgram_APll' => {
        'ALL' => [
            'clkProgram_FreqSrc',
            'clkProgram_LdivUnit',
            'clkProgram_Mux',
            'clkProgram_APll',
            'clkProgram_ReadOnly',
            'clkProgram_Wire',
        ],
    },

    'clkProgram_ReadOnly' => {
        'ALL' => [
            'clkProgram_FreqSrc',
            'clkProgram_LdivUnit',
            'clkProgram_Mux',
            'clkProgram_APll',
            'clkProgram_ReadOnly',
            'clkProgram_Wire',
        ],
    },

    'clkProgram_Wire' => {
        'ALL' => [
            'clkProgram_FreqSrc',
            'clkProgram_LdivUnit',
            'clkProgram_Mux',
            'clkProgram_PDiv',
            'clkProgram_APll',
            'clkProgram_ReadOnly',
            'clkProgram_Wire',
        ],
    },

    'clkProgram_Bif' => {
        'ALL' => [
            'clkProgram_FreqSrc',
            'clkProgram_Bif',
        ],
    },

    'clkProgram_Nafll' => {
        'ALL' => [
            'clkProgram_FreqSrc',
            'clkProgram_Nafll',
        ],
    },

    'clkProgram_ADynRamp' => {
        'ALL' => [
            'clkProgram_FreqSrc',
            'clkProgram_ADynRamp',
            'clkProgram_LdivUnit',
            'clkProgram_Mux',
            'clkProgram_APll',
            'clkProgram_ReadOnly',
            'clkProgram_Wire',
        ],
    },

    # clkCleanup_FreqSrc_VIP

   'clkCleanup_FreqSrc' => {
        'ALL' => [
            'clkCleanup_FreqSrc',
            'clkCleanup_LdivUnit',
            'clkCleanup_Mux',
            'clkCleanup_APll',
            'clkCleanup_ReadOnly',
            'clkCleanup_Wire',
        ],
    },

    'clkCleanup_LdivUnit' => {
        'ALL' => [
            'clkCleanup_FreqSrc',
            'clkCleanup_LdivUnit',
            'clkCleanup_Mux',
            'clkCleanup_APll',
            'clkCleanup_ReadOnly',
            'clkCleanup_Wire',
        ],
    },

    'clkCleanup_Mux' => {
        'ALL' => [
            'clkCleanup_FreqSrc',
            'clkCleanup_LdivUnit',
            'clkCleanup_Mux',
            'clkCleanup_PDiv',
            'clkCleanup_APll',
            'clkCleanup_ReadOnly',
            'clkCleanup_Wire',
        ],
    },

    'clkCleanup_APll' => {
        'ALL' => [
            'clkCleanup_FreqSrc',
            'clkCleanup_LdivUnit',
            'clkCleanup_Mux',
            'clkCleanup_APll',
            'clkCleanup_ReadOnly',
            'clkCleanup_Wire',
        ],
    },

    'clkCleanup_ReadOnly' => {
        'ALL' => [
            'clkCleanup_FreqSrc',
            'clkCleanup_LdivUnit',
            'clkCleanup_Mux',
            'clkCleanup_APll',
            'clkCleanup_Wire',
        ],
    },

    'clkCleanup_Wire' => {
        'ALL' => [
            'clkCleanup_FreqSrc',
            'clkCleanup_LdivUnit',
            'clkCleanup_Mux',
            'clkCleanup_APll',
            'clkCleanup_ReadOnly',
            'clkCleanup_Wire',
        ],
    },

   'clkCleanup_Bif' => {
        'ALL' => [
            'clkCleanup_FreqSrc',
            'clkCleanup_Bif',
        ],
    },

   'clkCleanup_Nafll' => {
        'ALL' => [
            'clkCleanup_FreqSrc',
            'clkCleanup_Nafll',
        ],
    },

    'clkCleanup_ADynRamp' => {
        'ALL' => [
            'clkCleanup_FreqSrc',
            'clkCleanup_ADynRamp',
            'clkCleanup_LdivUnit',
            'clkCleanup_Mux',
            'clkCleanup_APll',
            'clkCleanup_ReadOnly',
            'clkCleanup_Wire',
        ],
    },

    'clkCleanup_ASdm' => {
        'ALL' => [
            'clkCleanup_FreqSrc',
            'clkCleanup_ASdm',
            'clkCleanup_LdivUnit',
            'clkCleanup_Mux',
            'clkCleanup_APll',
            'clkCleanup_ReadOnly',
            'clkCleanup_Wire',
        ],
    },

    # clkPrint_FreqSrc_VIP

    'clkPrint_Mux' => {
        'ALL' => [
            'clkPrint_Bif',
            'clkPrint_ADynRamp',
            'clkPrint_FreqSrc',
            'clkPrint_HPll',
            'clkPrint_LdivV2',
            'clkPrint_LdivUnit',
            'clkPrint_Mux',
            'clkPrint_Nafll',
            'clkPrint_PDiv',
            'clkPrint_APll',
            'clkPrint_ReadOnly',
            'clkPrint_RoPDiv',
            'clkPrint_RoSdm',
            'clkPrint_RoVco',
            'clkPrint_Wire',
            'clkPrint_Xtal',
        ],
    },

    'clkPrint_Wire' => {
        'ALL' => [
            'clkPrint_Mux',
        ],
    },


    'clkDomainProgMaxFreqMHzGet_3X_PRIMARY' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
        ],
    },

    'clkDomainProgFreqToVolt_3X_PRIMARY' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
        ],
    },

    'clkDomainProgVoltToFreq_3X_PRIMARY' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
        ],
    },

    'clkDomain3XProgFreqAdjustDeltaMHz_PRIMARY' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
        ],
    },

    'clkDomain3XSecondaryFreqAdjustDeltaMHz' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
        ],
    },

    'clkDomainProgMaxFreqMHzGet_3X_SECONDARY' => {
                'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
        ],
    },

    'clkDomainProgVoltToFreq_3X_SECONDARY' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
        ],
    },

    'clkDomainProgFreqAdjustDeltaMHz_3X_SECONDARY' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
        ],
    },

    'clkDomainProgFreqToVolt_3X_SECONDARY' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
        ],
    },

    'clkProg3XPrimaryFreqTranslatePrimaryToSecondary_RATIO' => {
                'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
        ],
    },

    'clkProg3XPrimaryFreqTranslatePrimaryToSecondary_TABLE' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
        ],
    },

    'clkDomain3XProgVoltAdjustDeltauV_SECONDARY' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
        ],
    },

    'clkDomain3XProgIsOVOCEnabled_3X_SECONDARY' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
        ],
    },

    'clkDomainProgMaxFreqMHzGet_3X_SECONDARY' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
        ],
    },

    'clkDomainProgVoltToFreq_3X_SECONDARY' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
        ],
    },

    'clkDomain3XProgFreqToVolt_SECONDARY' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
        ],
    },

    'clkDomain3XProgFreqAdjustDeltaMHz_SECONDARY' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
        ],
    },

    'clkDomain3XPrimaryVoltToFreqLinearSearch' => {
        'ALL' => [
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_RATIO',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_RATIO',
        ],
    },

    'clkDomain3XPrimaryFreqToVoltLinearSearch' => {
        'ALL' => [
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_RATIO',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_RATIO',
        ],
    },

    'clkDomain3XPrimaryVoltToFreqBinarySearch' => {
        'ALL' => [
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_RATIO',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_RATIO',
        ],
    },

    'clkDomain3XPrimaryFreqToVoltBinarySearch' => {
        'ALL' => [
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_RATIO',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_RATIO',
        ],
    },

    'clkDomain3XPrimaryMaxFreqMHzGet' => {
        'ALL' => [
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_RATIO',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_RATIO',
        ],
    },

    'clkDomain3XPrimaryGetPrimaryProgIdxFromSecondaryFreqMHz' => {
        'ALL' => [
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_RATIO',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_RATIO',
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_TABLE',
        ],
    },

    'clkDomainProgFactoryDeltaAdj_3X' => {
        'ALL' => [
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_RATIO',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_RATIO',
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_TABLE',
        ],
    },

    'clkDomain35PrimaryVoltToFreqTuple' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
        ],
    },

    'clkProg3XPrimaryFreqTranslateSecondaryToPrimary_RATIO' => {
        'ALL' => [
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_RATIO',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_RATIO',
        ],
    },

    'clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz_RATIO' => {
        'ALL' => [
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_RATIO',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_RATIO',
        ],
    },

    'clkProg3XPrimaryFreqTranslateSecondaryToPrimary_TABLE' => {
        'ALL' => [
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_TABLE',
        ],
    },

    'clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz_TABLE' => {
        'ALL' => [
            's_clkProgGetInterfaceFromBoardObj_30_PRIMARY_TABLE',
            's_clkProgGetInterfaceFromBoardObj_35_PRIMARY_TABLE',
        ],
    },

    'clkDomainsClkListSanityCheck' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'clkDomainsProgFreqPropagate_35' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'pmuRpcClkClkDomainProgClientFreqDeltaAdj' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'pmuRpcClkClkDomainProgFreqQuantize' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'pmuRpcClkClkDomainProgFreqToVolt' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'pmuRpcClkClkDomainProgVoltToFreq' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'pmuRpcClkClkDomainProgFreqsEnum' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'clkNafllLutEntryCallwlate_20' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'clkNafllLutOffsetTupleFromFreqCompute' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'clkNafllLutEntryCallwlate_30' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'clkNafllLutFreqMHzGet' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    's_perfCfControllersEvaluate' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    's_perfCfControllersCallback' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    's_pwrPolicyWorkloadMultiRailComputeClkMHz' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'pwrPolicyDomGrpEvaluate_WORKLOAD_COMBINED_1X' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    's_pwrPolicyWorkloadCombined1xComputeClkMHz' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    's_pwrPolicyWorkloadCombined1xGetIndexByFreqMHz' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    's_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    's_pwrPolicyWorkloadCombined1xScaleClocks' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'pwrPolicyWorkloadMultiRailGetVoltageFloor' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'pwrDevTupleGet_BA16HW' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'pffIlwalidate' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    's_perfPstateIsFreqQuantized' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfPstateClkFreqGet_30' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
        ],
    },

    'perfLimitsFreqkHzIsNoiseUnaware' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfLimitsFreqkHzToVoltageuV' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfLimitsVoltageuVToFreqkHz' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfLimitsFreqkHzQuantize' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfPstateCache_35' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfPstateIsFreqQuantized' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfPstate35AdjFreqTupleByFactoryOCOffset' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    's_perfDaemonChangeSeq35ScriptBuildAndInsert' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_IMPL' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS_IMPL' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'clkDomainsProgFreqPropagate_40' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'clkPropTopRelFreqPropagate_1X_VOLT' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfCfPwrModelScalePrimaryClkRange_DLPPM_1X' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfCfPwrModelScale_DLPPM_1X' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfCfPwrModelLoad_DLPPM_1X' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    's_perfCfPwrModelFbVoltageLookup_DLPPM_1X' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_30_Primary',
            's_clkDomainGetInterfaceFromBoardObj_30_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfPolicies35UpdateViolationTime' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_35_Primary',
            's_clkDomainGetInterfaceFromBoardObj_35_Secondary',
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    'perfDaemonChangeSeqScriptInit_35' => {
        'ALL' => [
            's_clkDomainGetInterfaceFromBoardObj_40_PROG',
        ],
    },

    # End Clocks 3.x -----------------------------------------------------------


    'pmuSha1' => {
        'task_cmdmgmt' => [
            's_GenerateGidSha1CopyFunc',
        ],
        'task_hdcp' => [
            's_hdcpLCallback',
            's_hdcpVCallback',
            's_hdcpSrmCopyCallback',
        ],
    },

    # === task_lpwr ===
    'pgLogicStateMachine' => {
        'ALL' => [
            's_grPgEntry',
            's_grRgEntry',
            's_grPassiveEntry',
            's_msMscgEntry',
            's_msLtcEntry',
            's_msPassiveEntry',
            's_eiEntry',
            's_eiPassiveEntry',
            's_msDifrSwAsrEntry',
            's_msDifrCgEntry',
            's_dfprEntry',

            's_grPgReset',       # twice
            's_msMscgReset',     # twice

            's_grPgExit',
            's_grRgExit',
            's_grPassiveExit',
            's_msMscgExit',
            's_msLtcExit',
            's_msPassiveExit',
            's_eiExit',
            's_eiPassiveExit',
            's_msDifrSwAsrExit',
            's_msDifrCgExit',
            's_dfprExit',
        ],
    },
    'pmuRpcProcessUnitLpwrLoadin' => {
        'ALL' => [
            'grPgBufferLoad_GMXXX',
            'grPgBufferUnload_GMXXX',
        ],
    },

    # === task_disp ===
    'vbiosIedExelwteScriptSub' => {
        'ALL' => [
            'vbiosOpcode_***',
        ],
    },

    # === resident/OS ===

    'osCmdmgmtQueuePost_IMPL' => {
        'osCmdmgmtRmQueuePostBlocking_IMPL' => [
            'pmuQueuePmuToRmHeadGet_GMXXX',
            'pmuQueuePmuToRmHeadSet_GMXXX',
            'pmuQueuePmuToRmTailGet_GMXXX',
            'osCmdmgmtQueueWrite_DMEM',
            'queueFbHeapQueueDataPost',
            'queueFbPtcbQueueDataPost',
        ],
        'osCmdmgmtRmQueuePostNonBlocking_IMPL' => [
            'pmuQueuePmuToRmHeadGet_GMXXX',
            'pmuQueuePmuToRmHeadSet_GMXXX',
            'pmuQueuePmuToRmTailGet_GMXXX',
            'osCmdmgmtQueueWrite_DMEM',
            'queueFbHeapQueueDataPost',
            'queueFbPtcbQueueDataPost',
        ],
    },
    'icServiceIntr1MainIrq_GMXXX' => {
        'ALL' => [
            's_icServiceIntr1Unhandled_GMXXX',
            's_icServiceIntr1Cmd_GMXXX',
            's_icServiceIntr1Elpg0_GMXXX',
            's_icServiceIntr1Elpg1_GMXXX',
            's_icServiceIntr1Elpg2_GMXXX',
            's_icServiceIntr1Holdoff_GMXXX',
            's_icServiceIntr1Elpg3_GMXXX',
            's_icServiceIntr1Elpg4_GMXXX',
            's_icServiceIntr1Elpg5_GMXXX',
            's_icServiceIntr1Elpg6_GMXXX',
            's_icServiceIntr1Elpg7_GMXXX',
            's_icServiceIntr1Elpg8_GMXXX',
            's_icServiceIntr1Elpg9_GMXXX',
        ],
    },
    'icServiceIntr1MainIrq_TUXXX' => {
        'ALL' => [
            's_icServiceIntr1XusbToPmuReq_TUXXX',
        ],
    },

    's_osCmdmgmtQueueIsEmpty' => {
        'osPTimerCondSpinWaitNs_IMPL' => [
            'pmuQueuePmuToRmHeadGet_GMXXX',
            'pmuQueuePmuToRmHeadSet_GMXXX',
        ],
    },
};


return {
    RTOS_PARAMS     => $rtosParams,
    CYCLE_LIMITS    => $cycleLimits,
    COMPACT         => $compact,
    UNREACHED       => $unreached,
    FP_MAPPINGS     => $fpMappings,
};
