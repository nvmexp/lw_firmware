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
#     Each task that runs in the DPU's RTOS is defined in this file.
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
#             ENTRY_FN             => "taskEntryFunc"           ,
#             PRIORITY             => 3                         ,
#             STACK_SIZE           => 384                       ,
#             TCB                  => "OsTaskCmdMgmt"           ,
#             MAX_OVERLAYS_IMEM_LS => 0                         ,
#             OVERLAY_IMEM         => "OVL_INDEX_IMEM(cmdmgmt)" ,
#             MAX_OVERLAYS_DMEM    => 0                         ,
#             OVERLAY_DMEM         => "OVL_INDEX_DMEM(cmdmgmt)" ,
#             PRIV_LEVEL_EXT       => 1                         ,
#             PRIV_LEVEL_CSB       => 1                         ,
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

    DPUTASK_DISPATCH =>
    {
        OSTASK_DEFINE => [
                ENTRY_FN             => "task1_mgmt"           ,
                PRIORITY             => 3                      ,
                STACK_SIZE           => 412                    ,
                MAX_OVERLAYS_IMEM_LS => 1                      ,
                OVERLAY_IMEM         => "OVL_INDEX_IMEM(mgmt)" ,
                TCB                  => "OsTaskCmdMgmt"        ,
        ],

        OVERLAY_COMBINATION => [
            [ ALL, ] => [
                ['.imem_mgmt', ],
                ['.imem_mgmt', '.imem_init', ],
            ],
        ],
    },

    # Worker Thread
    DPUTASK_WKRTHD =>
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
                MAX_OVERLAYS_IMEM_HS => 4,
                OVERLAY              => "OVL_INDEX_IMEM(testhsswitch)",
            ],

            OSJOB_DEFINE => [
                ENTRY_FN             => "rttimer_test_entry",
                STACK_SIZE           => 512,
                MAX_OVERLAYS_IMEM_LS => 3,
                OVERLAY              => "OVL_INDEX_IMEM(testrttimer)",
            ],
        ],

        OVERLAY_COMBINATION => [
            [ dVOLTA_and_later, ]     => [
                [ '.imem_wkrthd', '.imem_testhsswitch', '.imem_testHs', '.imem_libCommonHs', '.imem_libSecBusHs', '.imem_selwreActionHs', ],
            ],

            [ dTURING_and_later, ]    => [
                [ '.imem_wkrthd', '.imem_testrttimer?', '.imem_rttimer?', ],
            ],

            [ dADA_and_later, ]     => [
                [ '.imem_wkrthd', '.imem_testhsswitch', '.imem_testHs', '.imem_libCommonHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_libMemHs', '.imem_selwreActionHs', ],
            ],
        ],
    },

    DPUTASK_HDCP =>
    {
        OSTASK_DEFINE => [
                ENTRY_FN             => "task5_hdcp"        ,
                PRIORITY             => 1                   ,
                # The stack value is according to analysis script result, and increased a lot ( from 3072bytes )
                # after adding HS Certification/SRM handling because sharing same SelwreActionHsEntry with HDCP22.
                # However, HDCP1.X won't call the handler and should never get there with such big stack usage.
                # TODO: reduce to optimized stack usage once separating HDCP1.X, HDCP22 HS entry.
                STACK_SIZE           => 3720                ,
                OVERLAY_IMEM         => "OVL_INDEX_IMEM(hdcp)"   ,
                MAX_OVERLAYS_IMEM_LS => 9                   ,
                MAX_OVERLAYS_IMEM_HS => 9                   ,
                TCB                  => "OsTaskHdcp"        ,
                PRIV_LEVEL_EXT       => 3                   ,
                PRIV_LEVEL_CSB       => 3                   ,
        ],

        OVERLAY_COMBINATION => [
            [ GM20X_thru_dPASCAL, ] => [
                [ '.imem_hdcp', '.imem_hdcpCmn', '.imem_libHdcp', '.imem_auth', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pvtbus', '.imem_libSha1', '.imem_libBigInt', ],
            ],
            [ dVOLTA_and_later ] => [
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_auth', '.imem_libLw64', '.imem_libLw64Umul', '.imem_pvtbus', 'imem_libSE', '.imem_libBigInt', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_auth', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libBigInt', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_auth', '.imem_libLw64', '.imem_libLw64Umul', '.imem_libSha1'],
            ],
            [ dVOLTA ] => [
                # Single SelwreActionHsEntry supports libHdcp22wired actiontypes, therefore, need add overlay to combination even not called by hdcp task.
                # TODO: exclude hdcp22 selwreAction overlays.
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_pvtbus', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libHdcp22EndSessionHs', '.imem_pvtbus', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libHdcp22wired', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libBigIntHs', '.imem_libHdcp22wired', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libHdcp22wired', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libHdcp22HprimeValidationHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libHdcp22LprimeValidationHs', '.imem_libShaHs', '.imem_pvtbus', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libShaHs', '.imem_pvtbus', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libHdcp22MprimeValidationHs', '.imem_libShaHs', '.imem_pvtbus', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs', '.imem_pvtbus', ],

                # To enable SW RSA for any chip need to add enable this combination for that particular chip
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', '.imem_libLw64Umul', '.imem_libHdcp22wired', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', '.imem_libBigIntHs', '.imem_libHdcp22wired', ],
            ],
            [ dTURING ] => [
                # To enable SW RSA for any chip need to add enable this combination for that particular chip
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', '.imem_libLw64Umul', '.imem_libHdcp22wired', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', '.imem_libBigIntHs', '.imem_libHdcp22wired', ],
            ],
            [ dTURING_and_later ] => [
                # Single SelwreActionHsEntry supports libHdcp22wired actiontypes, therefore, need add overlay to combination even not called by hdcp task.
                # TODO: exclude hdcp22 selwreAction overlays.
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_pvtbus', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22EndSessionHs', '.imem_pvtbus', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libHdcp22wired', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libBigIntHs', '.imem_libHdcp22wired', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libHdcp22wired', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22HprimeValidationHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22LprimeValidationHs', '.imem_libShaHs', '.imem_pvtbus', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libShaHs', '.imem_pvtbus', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22MprimeValidationHs', '.imem_libShaHs', '.imem_pvtbus', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs', '.imem_pvtbus', ],
            ],
            [dADA_and_later] => [
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', '.imem_libBigIntHs', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_libShaHs', '.imem_pvtbus', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', ],
                [ '.imem_hdcp', '.imem_libHdcp', '.imem_libCommonHs', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', ],
            ],
        ],
    },

    DPUTASK_HDCP22WIRED =>
    {
        OSTASK_DEFINE => [
                ENTRY_FN             => "task6_hdcp22"      ,
                PRIORITY             => 1                   ,
                STACK_SIZE           => 4864                ,
                OVERLAY_IMEM         => "OVL_INDEX_IMEM(hdcp22wired)" ,
                MAX_OVERLAYS_IMEM_LS => 20                  ,
                MAX_OVERLAYS_IMEM_HS => 9                   ,
                TCB                  => "OsTaskHdcp22wired" ,
                PRIV_LEVEL_EXT       => 3                   ,
                PRIV_LEVEL_CSB       => 3                   ,
        ],

        OVERLAY_COMBINATION => [
            [ GM20X_thru_dPASCAL, ] => [
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22SelwreAction', '.imem_hdcp22StartSession', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Ake', '.imem_libBigInt', '.imem_hdcp22SelwreAction', '.imem_hdcp22GenerateKmkd', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Ake', '.imem_libSha', '.imem_hdcp22SelwreAction', '.imem_hdcp22GenerateKmkd', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Ake', '.imem_hdcp22Aes', '.imem_hdcp22SelwreAction', '.imem_hdcp22GenerateKmkd', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Akeh', '.imem_libSha', '.imem_hdcp22SelwreAction', '.imem_hdcp22HprimeValidation', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Ske', '.imem_hdcp22SelwreAction', '.imem_hdcp22GenerateSessionkey', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Akeh', '.imem_libSha', '.imem_hdcp22SelwreAction', '.imem_hdcp22HprimeValidation', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22SelwreAction', '.imem_hdcp22EndSession', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22SelwreAction', '.imem_hdcp22ControlEncryption', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Lc', '.imem_hdcp22SelwreAction', '.imem_hdcp22LprimeValidation', '.imem_libSha', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Repeater', '.imem_hdcp22SelwreAction', '.imem_hdcp22MprimeValidation', '.imem_libSha', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Repeater', '.imem_hdcp22SelwreAction', '.imem_hdcp22VprimeValidation', '.imem_libSha', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libSE?', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22CertrxSrm', '.imem_libDpaux', '.imem_hdcp22Ake', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_hdcp22Akeh', '.imem_hdcp22Aes', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_hdcp22Akeh', '.imem_libSha', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_hdcp22Lc', '.imem_libSha', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_hdcp22Ske', '.imem_hdcp22Aes', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_hdcp22Repeater', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Repeater', '.imem_libSha', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Repeater', '.imem_libBigInt', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Repeater', '.imem_libSha1', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22CertrxSrm', '.imem_hdcp22SelwreAction', '.imem_hdcp22ValidateCertrxSrm', '.imem_hdcp22RsaSignature', '.imem_libSha', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22CertrxSrm', '.imem_hdcp22SelwreAction', '.imem_hdcp22ValidateCertrxSrm', '.imem_hdcp22RsaSignature', '.imem_libBigInt', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22CertrxSrm', '.imem_hdcp22SelwreAction', '.imem_hdcp22ValidateCertrxSrm', '.imem_hdcp22DsaSignature', '.imem_libSha1', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22CertrxSrm', '.imem_hdcp22SelwreAction', '.imem_hdcp22ValidateCertrxSrm', '.imem_hdcp22DsaSignature', '.imem_libBigInt', ],
                [ '.imem_hdcp22wired', '.imem_hdcpCmn', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_hdcp22ControlEncryption',  ]
            ],
            [ GM20X, ] => [
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Ake', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Akeh', '.imem_hdcp22Aes', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Akeh', '.imem_libSha', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Lc', '.imem_libSha', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Ske', '.imem_hdcp22Aes', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Repeater', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Repeater', '.imem_libSha', ],
            ],
            [ dPASCAL, ] => [
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cSw', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Ake', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cSw', '.imem_hdcp22Akeh', '.imem_hdcp22Aes', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cSw', '.imem_hdcp22Akeh', '.imem_libSha', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cSw', '.imem_hdcp22Lc', '.imem_libSha', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cSw', '.imem_hdcp22Ske', '.imem_hdcp22Aes', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cSw', '.imem_hdcp22Repeater', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Ake', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Akeh', '.imem_hdcp22Aes', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Akeh', '.imem_libSha', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Lc', '.imem_libSha', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Ske', '.imem_hdcp22Aes', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Repeater', ],
                [ '.imem_hdcp22wired', '.imem_libLw64Umul', '.imem_libHdcp22wired', '.imem_hdcp22Repeater', '.imem_libSha', ],
            ],

            [ dVOLTA_and_later, ] => [
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_hdcp22Akeh', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_hdcp22Lc', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_hdcp22Ske', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_libSha1', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_hdcp22Repeater', '.imem_hdcp22CertrxSrm', '.imem_libBigInt', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_hdcp22Repeater', '.imem_hdcp22CertrxSrm', '.imem_libSha1', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Akeh', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Lc', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Ske', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_libSha1',],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Repeater', '.imem_hdcp22CertrxSrm', '.imem_libBigInt', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libI2c', '.imem_libI2cHw', '.imem_hdcp22Repeater', '.imem_hdcp22CertrxSrm', '.imem_libSha1',],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_auth', '.imem_hdcp22CertrxSrm', '.imem_libSha1', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_auth', '.imem_hdcp22CertrxSrm', '.imem_libBigInt', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libLw64Umul', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm',  '.imem_libBigInt', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libLw64', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libSE', '.imem_hdcp22Ake', '.imem_hdcp22Akeh', '.imem_hdcp22CertrxSrm', ],
            ],
            [ dVOLTA, ] => [
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_pvtbus', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_pvtbus', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_pvtbus', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_pvtbus', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22EndSessionHs', '.imem_libScpCryptHs', '.imem_pvtbus',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22EndSessionHs', '.imem_libScpCryptHs', '.imem_pvtbus',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22EndSessionHs', '.imem_libScpCryptHs', '.imem_pvtbus',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22EndSessionHs', '.imem_libScpCryptHs', '.imem_pvtbus',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22HprimeValidationHs', '.imem_pvtbus', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22HprimeValidationHs', '.imem_pvtbus', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22HprimeValidationHs', '.imem_pvtbus', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22HprimeValidationHs', '.imem_pvtbus', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22LprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22LprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22LprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22LprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22MprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22MprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22MprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22MprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', ],

                # To enable SW RSA for any chip need to add enable this combination for that particular chip
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', '.imem_libBigIntHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', '.imem_libBigIntHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', '.imem_libBigIntHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libScpCryptHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', '.imem_libBigIntHs', ],
            ],
            [ dTURING_and_later, ] => [
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_pvtbus', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_pvtbus', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_pvtbus', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_pvtbus', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22EndSessionHs', '.imem_pvtbus',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22EndSessionHs', '.imem_pvtbus',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22EndSessionHs', '.imem_pvtbus',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22EndSessionHs', '.imem_pvtbus',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22HprimeValidationHs', '.imem_pvtbus', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22HprimeValidationHs', '.imem_pvtbus', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22HprimeValidationHs', '.imem_pvtbus', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22HprimeValidationHs', '.imem_pvtbus', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22LprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22LprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22LprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22LprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22MprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22MprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22MprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22MprimeValidationHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', ],
            ],
            [ dTURING, ] => [
                # To enable SW RSA for any chip need to add enable this combination for that particular chip
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', '.imem_libBigIntHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', '.imem_libBigIntHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', '.imem_libBigIntHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', '.imem_libBigIntHs', ],
            ],
            [ dADA_and_later, ] => [
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22StartSessionHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22GenerateKmkdHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libLw64Umul', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22GenerateSessionkeyHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_pvtbus', '.imem_libShaHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22VprimeValidationHs', '.imem_libSecBusHs', '.imem_libCccHs', '.imem_libShaHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs', '.imem_libCccHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs', '.imem_libCccHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs', '.imem_libCccHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22ControlEncryptionHs', '.imem_libSecBusHs', '.imem_libCccHs',  ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ake', '.imem_hdcp22CertrxSrm', '.imem_hdcp22Akeh', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', '.imem_libCccHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Lc', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', '.imem_libCccHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Ske', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', '.imem_libCccHs', ],
                [ '.imem_hdcp22wired', '.imem_libHdcp22wired', '.imem_libDpaux', '.imem_selwreActionHs', '.imem_libMemHs', '.imem_libCommonHs', '.imem_hdcp22Repeater', '.imem_libHdcp22ValidateCertrxSrmHs', '.imem_libSecBusHs', '.imem_pvtbus', '.imem_libShaHs', '.imem_libBigIntHs', '.imem_libCccHs', ],
            ],
        ],
    },

    DPUTASK_SCANOUTLOGGING =>
    {
        OSTASK_DEFINE => [
                ENTRY_FN             => "task8_scanoutLogging"      ,
                PRIORITY             => 1                           ,
                STACK_SIZE           => 384                         ,
                OVERLAY_IMEM         => "OVL_INDEX_IMEM(scanoutLogging)" ,
                MAX_OVERLAYS_IMEM_LS => 1                           ,
                TCB                  => "OsTaskScanoutLoggging"     ,
                PRIV_LEVEL_EXT       => 0                           ,
        ],

        OVERLAY_COMBINATION => [
            [ GK10X_thru_GP10X, ] => [
                [ '.imem_scanoutLogging', ],
            ],
        ],
    },

    DPUTASK_MSCGWITHFRL =>
    {
        OSTASK_DEFINE => [
                ENTRY_FN             => "task9_mscgWithFrl"          ,
                PRIORITY             => 1                            ,
                STACK_SIZE           => 384                          ,
                OVERLAY_IMEM         => "OVL_INDEX_IMEM(mscgwithfrl)"     ,
                MAX_OVERLAYS_IMEM_LS => 2                            ,
                TCB                  => "OsTaskMscgWithFrl"          ,
                PRIV_LEVEL_EXT       => 0                            ,
        ],

        OVERLAY_COMBINATION => [
            [ GM20X_thru_PASCAL, ] => [
                [ '.imem_mscgwithfrl', '.imem_isohub', ],
            ],

            #
            # Added to bypass errors in automated rule checker based on the static code analysis. Fix ASAP !!!
            #
            [ GM20X_thru_PASCAL, ] => [
                [ '.imem_mscgwithfrl', '.imem_libLw64', ],
            ],
        ],
    },

    DPUTASK_VRR =>
    {
        OSTASK_DEFINE => [
                ENTRY_FN             => "task4_vrr"      ,
                PRIORITY             => 1                           ,
                STACK_SIZE           => 384                         ,
                OVERLAY_IMEM         => "OVL_INDEX_IMEM(vrr)" ,
                MAX_OVERLAYS_IMEM_LS => 1                           ,
                TCB                  => "OsTaskVrr"     ,
                PRIV_LEVEL_EXT       => 0,
        ],

        OVERLAY_COMBINATION => [
            [ GP10X, ] => [
                [ '.imem_vrr', ],
            ],
        ],
    },
];

# return the raw data of Tasks definition
return $tasksRaw;
