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
        INTERRUPT_OVERHEAD => 76,
        INTERRUPT_HANDLERS => [
            "IV0_routine",
            "IV1_routine",
            "EV_routine",
            "EV_preinit_routine"
        ],
        START => "start",
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
# If the limit is unknown set it to 'undef', which will be treated as 1 for
# stack usage callwlations.
#
my $cycleLimits = [];


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
    'lwrtosQueueSend',
    'lwrtosQueueReceive',
    'xTaskResumeAll',
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
    '_restore_ctx_full',                # ilwoked with "jmp" and not "call"
    '_restore_ctx_merged',              # ilwoked with "jmp" and not "call"
    'testHsEntry',
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
# functions like boardObjGrpConstruct).
#
# Set PARENT_FUNC to 'ALL' if the targets of a pointer are always the same,
# regardless of context. ALL should be the only parent func if it's present.
#
# Add a target of 'NULL' if the function pointer is sometimes not called at all.
# This is useful when the pointer can be null (like for osPTimerCondSpinWaitNs),
# or when none of the target functions are present on some build.
#
my $fpMappings = {
    'osCmdmgmtQueuePost_IMPL' => {
        'ALL' => [
            'dpuQueueRmHeadGet_v02_00',
            'dpuQueueRmHeadSet_v02_00',
            'dpuQueueRmTailGet_v02_00',
            'osCmdmgmtQueueWrite_DMEM',
        ],
    },
    'osPTimerCondSpinWaitNs_IMPL' => {
        'osPTimerSpinWaitNs' => [
            'NULL',
        ],
        '_i2cWaitSclReleasedTimedOut' => [
            '_i2cIsLineHigh',
        ],
        'hdcpWriteStatus_v02_05' => [
            '_hdcpCheckBusReady_v02_05',
        ],
        '_hdcpReadReg_v02_05' => [
            '_hdcpCheckBusReady_v02_05',
        ],
    },
    'osPTimerCondSpinWaitNs_hs' => {
        'hdcp22wiredHandleEnableEnc_v03_00' => [
            'NULL',
        ],
        'hdcp22wiredHandleDisableEnc_v03_00' => [
            'NULL',
        ],
        '_hdcp22WiredLockBeforeProcessingEcf' => [
            'NULL',
        ],
        '_hdcp22WiredTriggerACT' => [
            'NULL',
        ],
    },
    'osTimerCallExpiredCallbacks' => {
        'task9_mscgWithFrl' => [
            '_restoreCwmCallback',
        ],
        'task6_hdcp22' => [
             '_hdcp22wiredTimerCallback',
        ],
        'task4_vrr' => [
            '_vrrForceFrameReleaseCallback',
        ],
    },
    'osTmrCallbackExelwte_IMPL' => {
        'task6_hdcp22' => [
             '_hdcp22wiredTimerCallback',
        ],
    },
    'dpuSelwreActionHsEntry' => {
        'ALL' => [
             '_seSelwreBusHsEntry',
             '_dpuLibCommonHsEntry',
             '_libMemHsEntry',
             '_scpCryptEntry',
             '_libShaHsEntry',
             '_shahwEntry',
             '_libBigIntHsEntry',
             '_libCccHsEntry',
             '_hdcp22wiredStartSessionEntry',
             '_hdcp22wiredValidateCertrxSrmEntry',
             '_hdcp22wiredGenerateKmkdEntry',
             '_hdcp22wiredHprimeValidationEntry',
             '_hdcp22wiredLprimeValidationEntry',
             '_hdcp22wiredGenerateSessionkeyEntry',
             '_hdcp22wiredControlEncryptionEntry',
             '_hdcp22wiredVprimeValidationEntry',
             '_hdcp22wiredMprimeValidationEntry',
             '_hdcp22wiredWriteDpEcfEntry',
             '_hdcp22wiredEndSessionEntry',
        ],
    },
    'testHsEntry' => {
        'ALL' => [
             '_dpuLibCommonHsEntry',
        ],
    },
    'libSha1' => {
        '_hdcpCallwlateV' => [
            '_hdcpVCallback',
        ],
        'hdcpValidateSrm' => [
            '_hdcpSrmCopyCallback',
        ],
        # Compiler optimize that hdcpProcessCmd to call libSha1 directly and need the mapping.
        'hdcpProcessCmd' => [
            '_hdcpVCallback',
        ],
    },
    'constructIsohub' => {
        'ALL' => [
            'isohubHalIfacesSetup_GM200',
            'isohubHalIfacesSetup_GP100',
        ],
    },
    'constructHdcp' => {
        'ALL' => [
            'hdcpHalIfacesSetup_GM200',
            'hdcpHalIfacesSetup_GP100',
        ],
    },
    'constructLsf' => {
        'ALL' => [
            'lsfHalIfacesSetup_GM200',
            'lsfHalIfacesSetup_GP100',
            'lsfHalIfacesSetup_GV100',
            'lsfHalIfacesSetup_TU102',
            'lsfHalIfacesSetup_TU116',
            'lsfHalIfacesSetup_GA102',
            'lsfHalIfacesSetup_AD102',
        ],
    },
    'constructDpu' => {
        'ALL' => [
            'dpuHalIfacesSetup_GF119',
            'dpuHalIfacesSetup_GM107',
            'dpuHalIfacesSetup_GM200',
            'dpuHalIfacesSetup_GP100',
            'dpuHalIfacesSetup_GV100',
            'dpuHalIfacesSetup_TU102',
            'dpuHalIfacesSetup_TU116',
            'dpuHalIfacesSetup_GA102',
            'lsfHalIfacesSetup_AD102',
        ],
    },
    'constructIc' => {
        'ALL' => [
            'icHalIfacesSetup_GF119',
            'icHalIfacesSetup_GM107',
            'icHalIfacesSetup_GM200',
            'icHalIfacesSetup_GP100',
            'icHalIfacesSetup_GV100',
            'icHalIfacesSetup_TU102',
            'icHalIfacesSetup_TU116',
            'icHalIfacesSetup_GA102',
            'icHalIfacesSetup_AD102',
        ],
    },
    'constructHdcp22' => {
        'ALL' => [
            'lsfHalIfacesSetup_GM200',
            'lsfHalIfacesSetup_GP100',
        ],
    },
    'constructVrr' => {
        'ALL' => [
            'vrrHalIfacesSetup_GP100',
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
