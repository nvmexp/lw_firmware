#
# Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
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
    'hdcpDcpCert_imem',                 # certificate stored in IMEM
    'sec2CleanupHs_GM20X',
    '_libMemHsEntry',
    '_sec2LibCommonHsEntry',
    '_prSbHsEntry_Entry',
    '_prSbHsDecryptMPK_Entry',
    '_prSbHsExit_Entry',
    '_sec2LibVprPollicyHsEntry',
    '_seSelwreBusHsEntry',
    'initHsEntry',
    'testHsEntry',
    'sec2Bar0ErrChkRegWr32NonPostedHs_GP10X',
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
            'sec2QueueRmHeadGet_GP10X',
            'sec2QueueRmHeadSet_GP10X',
            'sec2QueueRmTailGet_GP10X',
            'osCmdmgmtQueueWrite_DMEM',
        ],
    },
    'osPTimerCondSpinWaitNs_IMPL' => {
        'sec2PollReg32Ns' => [
            'sec2PollReg32Func'
        ],
    },
    'osPTimerCondSpinWaitNs_hs' => {
        'sec2PollReg32Ns' => [
            'sec2PollReg32Func'
        ],
    },
    'gfeCryptEntry' => {
        'ALL' => [
            '_scpCryptEntry',
            '_sec2LibCommonHsEntry',
        ],
    },
    'hdcpCryptEpair' => {
        'ALL' => [
            '_libHdcpCryptHsEntry',
            '_libMemHsEntry',
            '_sec2LibCommonHsEntry',
        ],
    },
    'hdcpGetDcpKey' => {
        'ALL' => [
            '_libHdcpCryptHsEntry',
            '_sec2LibCommonHsEntry',
        ],
    },
    '_hdcpSessionDoCrypt' => {
        'ALL' => [
            '_scpCryptEntry',
            '_sec2LibCommonHsEntry',
        ],
    },
    'hdcpEncrypt' => {
        'ALL' => [
            '_libMemHsEntry',
            '_libDmaHsEntry',
            '_libHdcpCryptHsEntry',
            '_sec2LibCommonHsEntry',
        ],
    },
    'hdcpRandomNumber' => {
        'ALL' => [
            '_scpRandEntry',
            '_sec2LibCommonHsEntry',
        ],
    },
    'hdcpGenerateDkey' => {
        'ALL' => [
            '_sec2LibCommonHsEntry',
        ],
    },
    'hdcpXor128Bits' => {
        'ALL' => [
            '_sec2LibCommonHsEntry',
        ],
    },
    'lwsrCryptEntry' => {
        'ALL' => [
            '_scpCryptEntry',
            '_sec2LibCommonHsEntry',
        ],
    },
    'osDmemCrypt' => {
        'ALL' => [
            '_scpCryptEntry',
        ],
    },
    'sec2SelwreBusAccessHsEntry' => {
        'ALL' => [
            '_seSelwreBusHsEntry',
        ],
    },
    'ComputeDmhash_HS' => {
        'ALL' => [
            '_libMemHsEntry',
        ],
    },
    '_OEM_TEE_UpdateDispBlankingPolicy_HS' => {
        'ALL' => [
            '_sec2LibCommonHsEntry',
        ],
    },
    'Oem_Aes_EncryptOneBlockWithKeyForKDFMode_LWIDIA_HS' => {
        'ALL' => [
            '_libMemHsEntry',
        ],
    },
    'prSbHsEntry' => {
        'ALL' => [
            '_libMemHsEntry',
        ],
    },
    'prSbHsExit' => {
        'ALL' => [
            '_libMemHsEntry',
        ],
    },
    'OEM_TEE_HsCommonEntry' => {
        'ALL' => [
            '_sec2LibCommonHsEntry',
        ],
    },
    'testHsEntry' => {
        'ALL' => [
            '_sec2LibCommonHsEntry',
        ],
    },
    'initHsEntry' => {
        'ALL' => [
            '_sec2LibCommonHsEntry',
        ],
    },
    'initDevidKeys' => {
        'ALL' => [
            '_scpCryptEntry',
        ],
    },
    '_vprProcessCommandHs' => {
        'ALL' => [
            '_sec2LibCommonHsEntry',
            '_seSelwreBusHsEntry',
        ],
    },
    'vprReleaseHdcpType1Lock_GP10X' => {
        'ALL' => [
            '_sec2LibVprPollicyHsEntry',
        ],
    },
    '_acrGetRegionAndBootFalconWithHaltHs' => {
        'ALL' => [
            '_libDmaHsEntry',
            '_sec2LibCommonHsEntry',
        ],
    },
    'sec2CleanupHs_GM20X' => {
        'ALL' => [
            '_sec2LibCommonHsEntry',
        ],
    },
    'apmInitiateCopy' => {
        'ALL' => [
            'apmDoCryptedDma',
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
