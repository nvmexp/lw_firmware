#
# Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# rtosParams - OS parameters, mostly stack-related
#
# These will likely be the same for all three devices (PMU, SOE, & DPU), but
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
# NOTE: these functions are all in task_pr, which has its own stack-checking
# mechanism
my $cycleLimits = [
    '_DRM_XB_Parse_Object'              => undef,
    '_DRM_XB_Parse_UnknownContainer'    => undef,
    '_DRM_XB_Parse_Container'           => undef,
    '_DRM_XB_AddUnknownContainer'       => undef,
    'DRM_XB_AddEntry'                   => undef,
    '_DRM_XB_GetElementLength'          => undef,
    '_DRM_XB_Serialize_ElementList'     => undef,
    '_DRM_XB_Serialize_Container'       => undef,
    '_ValidateExtensibleRestrictionContainers' => undef,
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
    'lwrtosQueueSend',
    'lwrtosQueueReceive',
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
            'soeQueueRmHeadGet_LR10',
            'soeQueueRmHeadSet_LR10',
            'soeQueueRmTailGet_LR10',
            'osCmdmgmtQueueWrite_DMEM',
        ],
    },
    'osPTimerCondSpinWaitNs_IMPL' => {
        'soePollReg32Ns' => [
            'soePollReg32Func'
        ],
        'bifSetPcieLinkSpeed_LR10' => [
            '_bifLtssmIsIdle_LR10'
        ],
        '_bifWriteNewPcieLinkSpeed_LR10' => [
            '_bifLtssmIsIdleAndLinkReady_LR10'
        ],
        'bifPollEomDoneStatus_LR10' => [
            '_bifEomDoneStatus_LR10',
        ],
        'bifSetPcieLinkSpeed_LS10' => [
            '_bifLtssmIsIdle_LS10'
        ],
        '_bifWriteNewPcieLinkSpeed_LS10' => [
            '_bifLtssmIsIdleAndLinkReady_LS10'
        ],
        'bifPollEomDoneStatus_LS10' => [
            '_bifEomDoneStatus_LS10',
        ],
    },
    'osTimerCallExpiredCallbacks' => {
        'task_smbpbi' => [
            '_smbpbiMonitorDriverState',
        ],
        'task_core' => [
            '_coreSoeResetPollingCallback_LR10',
        ],
    },
    '_discoverEngineAddress' => {
        'getEngineBaseAddress' => [
            '_soePtopParseEntryLr10',
            '_soePtopParseEnumLr10',
            '_soePtopHandleData1Lr10',
            '_soePtopHandleData2Lr10',
            '_soeLwlwParseEntryLr10',
            '_soeLwlwParseEnumLr10',
            '_soeLwlwHandleData1Lr10',
            '_soeLwlwHandleData2Lr10',
            '_soeNpgParseEntryLr10',
            '_soeNpgParseEnumLr10',
            '_soeNpgHandleData1Lr10',
            '_soeNpgHandleData2Lr10',
            '_soeNxbarParseEntryLr10',
            '_soeNxbarParseEnumLr10',
            '_soeNxbarHandleData1Lr10',
            '_soeNxbarHandleData2Lr10',
        ],
    },
    'constructSoe' => {
        'ALL' => [
            'soeHalIfacesSetup_TU102',
            'soeHalIfacesSetup_GH100',
        ],
    },
    'constructLsf' => {
        'ALL' => [
            'lsfHalIfacesSetup_TU102',
            'lsfHalIfacesSetup_GH100',
        ],
    },
    'constructIc' => {
        'ALL' => [
            'icHalIfacesSetup_TU102',
            'icHalIfacesSetup_GH100',
        ],
    },
    'constructSaw' => {
        'ALL' => [
            'sawHalIfacesSetup_TU102',
            'sawHalIfacesSetup_GH100',
        ],
    },
    'constructGin' => {
        'ALL' => [
            'ginHalIfacesSetup_GH100',
        ],
    },
    'constructIntr' => {
        'ALL' => [
            'intrHalIfacesSetup_TU102',
            'intrHalIfacesSetup_GH100',
        ],
    },
    'constructLwlink' => {
        'ALL' => [
            'lwlinkHalIfacesSetup_TU102',
            'lwlinkHalIfacesSetup_GH100',
        ],
    },
    'constructPmgr' => {
        'ALL' => [
            'pmgrHalIfacesSetup_TU102',
            'pmgrHalIfacesSetup_GH100',
        ],
    },
    'constructI2c' => {
        'ALL' => [
            'i2cHalIfacesSetup_TU102',
            'i2cHalIfacesSetup_GH100',
        ],
    },
    'constructGpio' => {
        'ALL' => [
            'gpioHalIfacesSetup_TU102',
            'gpioHalIfacesSetup_GH100',
        ],
    },
    'constructSpi' => {
        'ALL' => [
            'spiHalIfacesSetup_TU102',
            'spiHalIfacesSetup_GH100',
        ],
    },
    'constructIfr' => {
        'ALL' => [
            'ifrHalIfacesSetup_TU102',
            'ifrHalIfacesSetup_GH100',
        ],
    },
    'constructBios' => {
        'ALL' => [
            'biosHalIfacesSetup_TU102',
        ],
    },
    'constructSmbpbi' => {
        'ALL' => [
            'smbpbiHalIfacesSetup_TU102',
            'smbpbiHalIfacesSetup_GH100',
        ],
    },
    'constructTherm' => {
        'ALL' => [
            'thermHalIfacesSetup_TU102',
            'thermHalIfacesSetup_GH100',
        ],
    },
    'constructBif' => {
        'ALL' => [
            'bifHalIfacesSetup_TU102',
            'bifHalIfacesSetup_GH100',
        ],
    },
    'constructCore' => {
        'ALL' => [
            'coreHalIfacesSetup_TU102',
            'coreHalIfacesSetup_GH100',
        ],
    },
    'constructTimer' => {
        'ALL' => [
            'timerHalIfacesSetup_TU102',
            'timerHalIfacesSetup_GH100',
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
