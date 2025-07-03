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
# Defines all known SEC2 IMEM overlays.
#
# Overlays:
#     An "overlay" is any generic piece of code that can be paged in and out of
#     IMEM at runtime. To gcc, this is simply a code section. That is, the
#     overlays used here are not considered by gcc as true overlays as each
#     section has its own unique code address-space.
#
#     NOTE: The ordering of the overlay definitions in this module is
#           significant. Overlays will be placed in the final image in the
#           order that they are defined in this file.
#
# Overlay Attributes:
#
#     DESCRIPTION
#         Optional description of the overlay.
#
#     NAME
#         Defines the name of the output-section that is created for the
#         overlay. By default, the overlay name is comes from the hash key used
#         below when defining the overlay. The key name is colwerted to
#         camel-case with the leading character in lower-case. This attribute
#         may be used as an override if the resulting name is undesirable.
#
#     INPUT_SECTIONS
#         Defines a two-dimensional array where each top-level array element
#         defines a filename,section pair. The filename may refer to a specific
#         object file or a file glob. Combined with the section, the pair
#         represents an input code-section that will be mapped into the overlay
#         (output-section) by the linker.
#
#     KEEP_SECTIONS
#         Defines a one-dimensional array where each array element defines a
#         section name that we want the linker to not use link-time garbage
#         collection. It is useful to mark sections that should not be
#         eliminated. Today, we use these for HS overlays whose entry functions
#         are not explicitly called and are, thus, automatic candidates for
#         garbage collection. However, they are called by creating a function
#         pointer at the top of the overlay block. Hence we need them to be
#         present in the final image and also to be placed at the top of the
#         overlay in order to validate the overlay before jumping in. Hence,
#         all keep sections will be put at the top (lower addresses) of the
#         overlay. However, this is based on our use case today. It isn't
#         essential to do. The input sections will follow these at higher
#         addresses. This attribute may be used even in non-HS overlays, but
#         setting it for HS overlays is enforced.
#
#     SECTION_DESCRIPTION
#         This attribute may be used to specify the exact linker-script text
#         used to define the overlay. It overrides all other attributes (such
#         as the INPUT_SECTIONS) attribute. It should only be used under
#         special cirlwmstances where special syntax is required that is not
#         supported by the linker-script generation tool (ldgen).
#
#     ALIGNMENT
#         Defines the starting alignment of the overlay. By default, overlays
#         are not aligned. Each overlay starts where the previous overlay
#         ended. This attribute may be used if a specific alignment is required
#         (e.g. 256-byte alignment). If an overlay is an HS_OVERLAY, even if
#         this attribute is unspecified or an alignment different from 256-byte
#         alignment is specified, we will enforce 256-byte alignment.
#
#    FLAGS
#         Specifies extra attributes using flags, as below:
#         :HS_OVERLAY
#             This attribute may be used to indicate that the overlay is a HS
#             overlay and needs to be placed at the top (lower addresses) of
#             the virtual address space of the image. When HS_OVERLAY is set,
#             we require that KEEP_SECTIONS is set whereby the entry function
#             of the overlay may not be used as a candidate for link-time
#             garbage collection. We also internally enforce a 256-byte
#             alignment for HS overlays, irrespective of what value is set in
#             the ALIGNMENT attribute, or whether it is set at all. If this
#             attribute is not set, it will be compiled as a non-secure (NS) or
#             a light secure (LS) overlay, depending on the profile.
#
#         :HS_LIB
#             Specifies that the overlay is a HS library overlay, that is,
#             a HS overlay that may be shared between other HS overlays. HS
#             library overlays are placed at the top (lower addresses) of the
#             virtual address space, above other HS overlays. This attribute
#             takes effect only when :HS_OVERLAY is set.
#
#         :RESIDENT
#             Specifies that the overlay is a resident overlay, and is never a
#             candidate to be kicked out of physical memory. It will be loaded
#             by the SEC2 application at the very beginning after being booted.
#             There is really no need for this flag to be used with non-heavy
#             secure code since we might as well place it in the contiguous
#             resident code section. With heavy secure code, overlay placement
#             in virtual address space is important because the signature
#             depends on the start address of the overlay. Hence, not only do
#             we need to place heavy secure code at the lower addresses of the
#             virtual address space to avoid needing to resign, but sometimes,
#             we also need to strategically decide positions among HS overlays
#             such that the ones already signed and least likely to change will
#             be at the lowest addresses, thus allowing start address of code
#             after it to change as little as possible. Hence, we cannot really
#             make heavy secure code contiguous with the resident code in case
#             some of it wants to be resident, and in those cases, we use this
#             flag to indicate which overlays intend to be resident.
#
#     ENABLED_ON
#         Defines the list of chips on which the overlay is enabled. If an
#         overlay is not used on a particular chip, that chip must be excluded
#         from this list. We generate symbols in DMEM for every overlay that is
#         enabled, and that oclwpies (limited) DMEM resources. Hence, it is
#         important to make sure that we only enable the overlay only on chips
#         where they are used. Every overlay must specify this attribute.
#

my $imemOverlaysRaw =
[
    #
    # Add the HS overlays that are already prod-signed
    # Since the ordering of the overlays depends on the
    # order they are defined in this file, this makes sure
    # that they do not move.
    #

    #
    # PR HS overlays are defined in below seperate PM file
    #
    __INCLUDE__ => "OverlaysImemTaskPrHs.pm",

    VPR_HS =>
    {
        NAME           => 'vprHs',
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'HS overlay for vpr app',
        KEEP_SECTIONS  =>
        [
            ['.imem_vprHs._start'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-ga100', '-sec2-*_nouveau*', ],
    },

    LIB_VPR_POLICY_HS =>
    {
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'Common VPR policy code called from PR and VPR tasks',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libVprPolicyHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_libVprPolicyHs._start'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-ga100', '-sec2-*_nouveau*', ], # Enable on all profiles except AUTO profile.
    },

    GFE_CRYPT_HS =>
    {
        NAME           => 'gfeCryptHS',
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'HS code to call into encryption library',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_gfeCryptHS.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_gfeCrypt._start'],
        ],
        ENABLED_ON     => [ GP10X_and_later, -GA100, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    INIT_HS =>
    {
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'One-time HS initialization code',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_initHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_initHs._start'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', ], # Enable on all profiles except AUTO profile.
    },

    CLEANUP_HS =>
    {
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'Cleanup that must be done in HS mode',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_cleanupHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_cleanupHs._start'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', ],
    },

    LIB_SCP_CRYPT_HS =>
    {
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'SCP encryption library',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libScpCryptHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_libScpCryptHs._start'],
        ],
        ENABLED_ON     => [ GP10X_and_later, -GA100, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ], # Enable on all profiles except AUTO profile.
    },

    LIB_COMMON_HS =>
    {
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' .
                          ':RESIDENT',
        LS_SIG_GROUP   => 1,
        ALIGNMENT      => 256,
        DESCRIPTION    => 'Common HS code for all tasks',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libCommonHs.*'],
            ['*.o', '.imem_libSEHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_libCommonHs._start'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', ],
    },

    LIB_MEM_HS =>
    {
        DESCRIPTION    => 'HS version of memory-related functions',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB',
        LS_SIG_GROUP   => 1,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libMemHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_libMemHs._start'],
        ],
        ENABLED_ON     => [ GP10X_and_later, -GA100, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    # End of prod-signed HS overlays

    DMEM_CRYPT_HS =>
    {
        FLAGS          => ':HS_OVERLAY' .
                          ':RESIDENT',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'Routines to encrypt and decrypt the DMEM overlays',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_dmemCryptHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_dmemCryptHs._start'],
        ],
        ENABLED_ON     => [ ALL, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    SELWRE_BUS_ACCESS_HS =>
    {
        NAME           => 'selwreBusAccessHs',
        DESCRIPTION    => 'Secure engine library functions',
        FLAGS          => ':HS_OVERLAY',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_selwreBusAccessHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_selwreBusAccessHs._start'],
        ],
        ENABLED_ON     => [ ],
    },

    LIB_DMA_HS =>
    {
        DESCRIPTION    => 'HS version of DMA-related functions',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB',
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libDmaHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_libDmaHs._start'],
        ],
        ENABLED_ON     => [ TU10X_and_later, ],
    },

    TEST_HS =>
    {
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'Temporary HS code to write priv protected registers',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_testHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_testHs._start'],
        ],

        # WKRTHD tests only enabled and used on GP10X lwrrently.
        ENABLED_ON     => [ GP10X, ],
    },

    HDCP_SESSION_HS =>
    {
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'HS entry point to encrypt/decrypt session data',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpSessionHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcpSessionHs._start'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_RAND =>
    {
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'HS entry point to generate a random number',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpRand.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcpRand._start'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_DCP_KEY =>
    {
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'HS entry point to retrieve KpubDcp',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpDcpKey.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcpDcpKey._start'],
        ],
        ENABLED_ON     => [ ],
    },

    LIB_HDCP_CRYPT_HS =>
    {
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'HS library to access the SCP encryption functions',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libHdcpCryptHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_libHdcpCryptHs._start'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_CRYPT_EPAIR =>
    {
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'HS entry point to encrypt pairing info',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpCryptEpair.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcpCryptEpair._start'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_GENERATE_DKEY =>
    {
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'HS function to generate Dkey',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpGenerateDkey.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcpGenerateDkey._start'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_XOR =>
    {
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'HS function to HW XOR functionality',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpXor.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcpXor._start'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_ENCRYPT =>
    {
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'HS entry point to encrypt HDCP content for wireless transmission',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpEncrypt.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcpEncrypt._start'],
        ],
        ENABLED_ON     => [ ],
    },

    LWSRCRYPT =>
    {
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'HS code to call into encryption library',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_lwsrCrypt.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_lwsrCrypt._start'],
        ],
        ENABLED_ON     => [ ],
    },

    ACR =>
    {
        DESCRIPTION    => 'HS overlay for ACR task',
        NAME           => 'acr',
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_acr.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_acr._start'],
        ],
        ENABLED_ON     => [ TU10X_and_later, ],
    },

    # End of HS overlays

    #
    # PR LS overlays are defined in below PM file
    #
    __INCLUDE__ => "OverlaysImemTaskPrLs.pm",

    #
    # In case where PR task is disabled, this will be first LS overlay
    # As first LS overlay defined, this overlay must start on 256-byte aligned address
    # Since this LS overlay is used in LASSAHS Playready stack, we need to keep it
    # at the top in the LS overlays list after the PR LS overlays
    #
    LIBSE =>
    {
        NAME           => 'libSE',
        LS_SIG_GROUP   => 1,
        ALIGNMENT      => 256,
        DESCRIPTION    => 'Secure engine library functions',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libSE.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', ],
    },

    #
    # By convention, the section .init regroups all initialization functions
    # that will be exelwted once at boot up. We create a special overlay for
    # that in order to save a significant amount of code space.
    #
    # .init and .libInit (below) overlays are loaded at start-up, exelwted and
    # will be overwritten by the other overlays. After this point, .init
    # overlay should not be loaded but any task can load .libInit overlay
    # during RM based initialization.
    #
    # In case if PR and libSE overlays are not used, then INIT will be the first LS overlay
    # As the first LS overlay defined, this overlay must start on 256-byte aligned
    # address.
    #
    INIT =>
    {
        ALIGNMENT      => 256,
        DESCRIPTION    => 'Initialization overlay for one-time code.',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_init'],
            ['*.o', '.imem_libInit'],
            ['*.o', '.imem_libInit.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
    },

    CMDMGMT =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_cmdmgmt*.o', '.text.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
    },

    CHNMGMT =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_chnmgmt*.o', '.text.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
    },

    WKRTHD =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_wkrthd*.o', '.text.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    TESTHSSWITCH =>
    {
        INPUT_SECTIONS =>
        [
            ['*/job_hs_switch*.o', '.text.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    TESTFAKEIDLE =>
    {
        INPUT_SECTIONS =>
        [
            ['*/job_fakeidle_test*.o', '.text.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    TESTRTTIMER =>
    {
        INPUT_SECTIONS =>
        [
            ['*/job_rttimer_test*.o', '.text.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    RTTIMER =>
    {
        INPUT_SECTIONS =>
        [
            ['*/rttimer/pascal/*.o', '.text.*'],
            ['*/rttimer/lw/*.o', '.text.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
    },

    LIBSHA =>
    {
        NAME           => 'libSha',
        DESCRIPTION    => 'SHA-256 library functions',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libSha.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, -GA100, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    LIBSHAHW =>
    {
        NAME           => 'libShahw',
        DESCRIPTION    => 'SHA HW library functions',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libShahw.*'],
        ],
        ENABLED_ON     => [ GA100_and_later ],
        PROFILES       => [ 'sec2-*_apm'],
    },

    LIB_BIG_INT =>
    {
       INPUT_SECTIONS =>
       [
           ['*.o', '.imem_libBigInt.*'],
       ],
       ENABLED_ON     => [ GP10X_and_later, -GA100, ],
       PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    LIB_LW64 =>
    {
       DESCRIPTION    => "Unsigned 64-bit math. Most common operations (add/sub/cmp) are kept resident.",
       INPUT_SECTIONS =>
       [
           ['*_udivdi3.*', '.text'],
           ['*_lshrdi3.*', '.text'],
           ['*_ashldi3.*', '.text'],
       ],
       ENABLED_ON     => [ ],
    },

    LIB_GPTMR =>
    {
        DESCRIPTION    => 'Library for GP timer usage.',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libGptmr.*'],
        ],
        ENABLED_ON     => [ TU10X_and_later ],
        PROFILES       => [ 'sec2-*', '-sec2-tu116', '-sec2-tu10a', '-sec2-*_nouveau*', '-sec2-ga10x_*'],
    },

    HDCPMC =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_hdcpmc.o', '.text.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_READ_CAPS =>
    {
        DESCRIPTION    => 'HDCP method handler for READ_CAPS method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdReadCaps.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_INIT =>
    {
        DESCRIPTION    => 'HDCP method handler for INIT method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdInit.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_CREATE_SESSION =>
    {
        DESCRIPTION    => 'HDCP method handler for CREATE_SESSION method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdCreateSession.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_VERIFY_CERT_RX =>
    {
        DESCRIPTION    => 'HDCP method handler for VERIFY_CERT_RX method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdVerifyCertRx.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_GENERATE_EKM =>
    {
        DESCRIPTION    => 'HDCP method handler for GENERATE_EKM method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdGenerateEkm.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_VERIFY_HPRIME =>
    {
        DESCRIPTION    => 'HDCP method handler for VERIFY_HPRIME method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdVerifyHprime.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_GENERATE_LC_INIT =>
    {
        DESCRIPTION    => 'HDCP method handler for GENERATE_LC_INIT method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdGenerateLcInit.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_UPDATE_SESSION =>
    {
        DESCRIPTION    => 'HDCP method handler for UPDATE_SESSION method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdUpdateSession.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_VERIFY_LPRIME =>
    {
        DESCRIPTION    => 'HDCP method handler for VERIFY_L_PRIME method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdVerifyLprime.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_GENERATE_SKE_INIT =>
    {
        DESCRIPTION    => 'HDCP method handler for GENERATE_SKE_INIT method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdGenerateSkeInit.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_ENCRYPT_PAIRING_INFO =>
    {
        DESCRIPTION    => 'HDCP method handler for ENCRYPT_PAIRING_INFO method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdEncryptPairingInfo.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_DECRYPT_PAIRING_INFO =>
    {
        DESCRIPTION    => 'HDCP method handler for DECRYPT_PAIRING_INFO method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdDecryptPairingInfo.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_ENCRYPT =>
    {
        DESCRIPTION    => 'HDCP method handler for ENCRYPT method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdEncrypt.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_VALIDATE_SRM =>
    {
        DESCRIPTION    => 'HDCP method handler for VALIDATE_SRM method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdValidateSrm.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_REVOCATION_CHECK =>
    {
        DESCRIPTION    => 'HDCP method handler for REVOCATION_CHECK method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdRevocationCheck.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_SESSION_CTRL =>
    {
        DESCRIPTION    => 'HDCP method handler for SESSION_CTRL method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdSessionCtrl.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_VERIFY_VPRIME =>
    {
        DESCRIPTION    => 'HDCP method handler for VERIFY_VPRIME method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdVerifyVprime.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_GET_RTT_CHALLENGE =>
    {
        DESCRIPTION    => 'HDCP method handler for GET_RTT_CHALLENGE method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdGetRttChallenge.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_STREAM_MANAGE =>
    {
        DESCRIPTION    => 'HDCP method handler for STREAM_MANAGE method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdStreamManage.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_COMPUTE_SPRIME =>
    {
        DESCRIPTION    => 'HDCP method handler for COMPUTE_SPRIME method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdComputeSprime.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_MTHD_EXCHANGE_INFO =>
    {
        DESCRIPTION    => 'HDCP method handler for EXCHANGE_INFO method',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpMthdExchangeInfo.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_DKEY =>
    {
        DESCRIPTION    => 'Dkey library',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpDkey.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_RSA =>
    {
        DESCRIPTION    => 'RSA encode/decode library',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpRsa.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HDCP_SRM =>
    {
        DESCRIPTION    => 'SRM validate and revocation check library',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_hdcpSrm.*'],
        ],
        ENABLED_ON     => [ ],
    },

    GFE =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_gfe.o', '.text.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, -GA100, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    LWSR =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_lwsr.o', '.text.*'],
        ],
        ENABLED_ON     => [ ],
    },

    HWV =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_hwv.o', '.text.*'],
        ],
        ENABLED_ON     => [ TU10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    VPR =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_vpr.o', '.text.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-ga100', '-sec2-*_nouveau*', ],
    },

    LIB_SHA1 =>
    {
        INPUT_SECTIONS =>
        [
           ['*.o', '.imem_libSha1.*'],
        ],
        ENABLED_ON     => [ ],
    },

    ACR_LS =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_acr.o', '.text.*'],
            ['*/acr/pacsal/*.o', '.text.*'],
            ['*/acr/turing/*.o', '.text.*'],
            ['*.o', '.imem_acrLs.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
    },

    APM =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_apm.o', '.text.*'],
            ['*/apm/lw/*.o', '.text.*'],
            ['*/apm/ampere/*.o', '.text.*'],
        ],
        ENABLED_ON     => [ GA100, GA10X, ],

        PROFILES       => [ 'sec2-*_apm*', ],
    },

    SPDM =>
    {
        #
        # NOTE: Be careful when enabling overlay, as includes open-source
        # libspdm code. Releases must meet SWIPAT requirements when included.
        # See LwBug 3314381 for more info.
        #
        INPUT_SECTIONS =>
        [
            ['*/task_spdm.o', '.text.*'],
            ['*.o',           '.imem_libspdm.*'],
        ],
        
        ENABLED_ON     => [ GA100, GA10X, ],
        PROFILES       => [ 'sec2-*_apm*', ],
    },

    APM_SPDM_SHARED =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',           '.imem_apmSpdmShared.*'],
        ],
        
        ENABLED_ON     => [ GA100, GA10X, ],
        PROFILES       => [ 'sec2-*_apm*', ],
    },

];

# return the raw data of Overlays definition
return $imemOverlaysRaw;

