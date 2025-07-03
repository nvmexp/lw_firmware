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
# Defines all known DPU IMEM overlays.
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
#         (e.g. 256-byte alignment).
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
    LIB_SCP_CRYPT_HS =>
    {
        DESCRIPTION    => 'SCP encryption library',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libScpCryptHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_libScpCryptHs._start'],
        ],
        ENABLED_ON     => [ dVOLTA, ],
    },

    LIB_MEM_HS =>
    {
        DESCRIPTION    => 'HS version of memory-related functions',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB',
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libMemHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_libMemHs._start'],
        ],
        ENABLED_ON     => [ dVOLTA_and_later, ],
    },

    LIB_SEC_BUS_HS =>
    {
        NAME           => 'libSecBusHs',
        DESCRIPTION    => 'Secure engine library functions',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB',
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libSEHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_libSEHs._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    LIB_SHA_HS =>
    {
        DESCRIPTION    => 'SHA HS code.',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            # We are adding both SW and HW SHA code to same overlay and enable only one with compile time flags across different chips
            ['*.o', '.imem_libShaHs.*'],
            ['*.o', '.imem_libShahwHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_libShaHs._start'],
            ['.imem_libShahwHs._start'],
        ],
        ENABLED_ON     => [ dVOLTA_and_later, ],
    },

    LIB_BIG_INT_HS =>
    {
        DESCRIPTION    => 'BigInt HS code.',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libBigIntHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_libBigIntHs._start'],
        ],
        ENABLED_ON     => [ dVOLTA_and_later, ],
    },

    LIB_COMMON_HS =>
    {
        NAME           => 'libCommonHs',
        DESCRIPTION    => 'Common HS code for all tasks',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libCommonHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_libCommonHs._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    # This overlay is a dummy and it's for enabling HS support only.
    TEST_HS =>
    {
        NAME           => 'testHs',
        DESCRIPTION    => 'One-time HS initialization code',
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_testHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_testHs._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    LIB_CCC_HS =>
    {
        NAME           => 'libCccHs',
        DESCRIPTION    => 'LibCCC HS code',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libCccHs.*'],
            ['*CCC.a:*', '.text.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_libCccHs._start'],
        ],
        ENABLED_ON     => [ AD10X_and_later, ],
    },

    SELWRE_ACTION_HS =>
    {
        NAME           => 'selwreActionHs',
        DESCRIPTION    => 'Secure action library functions',
        FLAGS          => ':HS_OVERLAY',
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_selwreActionHs.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_selwreActionHs._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    HDCP22_START_SESSION_HS =>
    {
        NAME           => 'libHdcp22StartSessionHs',
        DESCRIPTION    => 'Hdcp22 StartSession secure action',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22StartSession.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcp22StartSession._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    HDCP22_VALIDATE_CERTRX_SRM_HS =>
    {
        NAME           => 'libHdcp22ValidateCertrxSrmHs',
        DESCRIPTION    => 'Hdcp22 Verify Certificate secure action',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22ValidateCertrxSrm.*'],
            ['*.o',   '.imem_hdcp22SrmRevocation.*'],
            ['*.o',   '.imem_hdcp22RsaSignature.*'],
            ['*.o',   '.imem_hdcp22DsaSignature.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcp22ValidateCertrxSrm._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    HDCP22_GENERATE_KMKD_HS =>
    {
        NAME           => 'libHdcp22GenerateKmkdHs',
        DESCRIPTION    => 'Hdcp22 Generate Km Kd secure action',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22GenerateKmkd.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcp22GenerateKmkd._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    HDCP22_HPRIME_VALIDATION_HS =>
    {
        NAME           => 'libHdcp22HprimeValidationHs',
        DESCRIPTION    => 'Hdcp22 Hprime validation secure action',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22HprimeValidation.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcp22HprimeValidation._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    HDCP22_LPRIME_VALIDATION_HS =>
    {
        NAME           => 'libHdcp22LprimeValidationHs',
        DESCRIPTION    => 'Hdcp22 Lprime validation secure action',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22LprimeValidation.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcp22LprimeValidation._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    HDCP22_GENERATE_SESSIONKEY_HS =>
    {
        NAME           => 'libHdcp22GenerateSessionkeyHs',
        DESCRIPTION    => 'Hdcp22 Generate sessionkey secure action',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22GenerateSessionkey.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcp22GenerateSessionkey._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    HDCP22_CONTROL_ENCRYPTION_HS =>
    {
        NAME           => 'libHdcp22ControlEncryptionHs',
        DESCRIPTION    => 'Hdcp22 HW control encryption secure action',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22ControlEncryption.*'],
            ['*.o',   '.imem_hdcp22WriteDpEcf.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcp22ControlEncryption._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    HDCP22_VPRIME_VALIDATION_HS =>
    {
        NAME           => 'libHdcp22VprimeValidationHs',
        DESCRIPTION    => 'Hdcp22 Vprime validation secure action',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22VprimeValidation.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcp22VprimeValidation._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    HDCP22_MPRIME_VALIDATION_HS =>
    {
        NAME           => 'libHdcp22MprimeValidationHs',
        DESCRIPTION    => 'Hdcp22 Mprime validation secure action',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22MprimeValidation.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcp22MprimeValidation._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    HDCP22_END_SESSION_HS =>
    {
        NAME           => 'libHdcp22EndSessionHs',
        DESCRIPTION    => 'Hdcp22 EndSession secure action',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB' ,
        ALIGNMENT      => 256,
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22EndSession.*'],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_hdcp22EndSession._start'],
        ],
        ENABLED_ON     => [ GV10X_and_later, ],
    },

    # End of prod-signed HS overlays

    #
    # By convention, the section .init regroups all initialization functions
    # that will be exelwted once at boot up. We create a special overlay for
    # that in order to save a significant amount of code space.
    #
    # As the first overlay defined, this overlay must start on 256-byte aligned
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
            ['*.o', '.imem_osTmrCallbackInit.*'],
        ],
        ENABLED_ON     => [ ALL, ],
    },

    MGMT =>
    {
       INPUT_SECTIONS =>
       [
           ['*/task1_*.o', '.text.*'],
       ],
       ENABLED_ON     => [ ALL, ],
    },

    WKRTHD =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_wkrthd*.o', '.text.*'],
        ],
        ENABLED_ON     => [ dVOLTA_and_later, ],
    },

    TESTHSSWITCH =>
    {
        INPUT_SECTIONS =>
        [
            ['*/test_hs_switch*.o', '.text.*'],
        ],
        ENABLED_ON     => [ dVOLTA_and_later, ],
    },

    TESTRTTIMER =>
    {
        INPUT_SECTIONS =>
        [
            ['*/test_rttimer_test*.o', '.text.*'],
        ],
        ENABLED_ON     => [ dVOLTA_and_later, ],
    },

    RTTIMER =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_rttimer.*'],
        ],
        ENABLED_ON     => [ dVOLTA_and_later, ],
    },

    LIB_HDCP =>
    {
       INPUT_SECTIONS =>
       [
           ['*.o', 'imem_libHdcp.*'],
       ],
       ENABLED_ON     => [ GM20X_and_later, ],
    },

    HDCP_CMN =>
    {
       ENABLED_ON     => [ ALL, ],
    },

    HDCP =>
    {
       INPUT_SECTIONS =>
       [
           ['*/task5_*.o', '.text.*'],
           ['*/hdcp/dpu_libintfchdcp*.o', '.text.*'],
       ],
       ENABLED_ON     => [ GM20X_and_later, ],
    },

    AUTH =>
    {
       INPUT_SECTIONS =>
       [
           ['*.o', '.imem_libHdcpauth.*'],
       ],
       ENABLED_ON     => [ GM20X_and_later, ],
    },

    LIB_LW64 => {
        DESCRIPTION    => "Unsigned 64-bit math. Most common operations (add/sub/cmp) are kept resident.",
        INPUT_SECTIONS =>
        [
            ['*_udivdi3.*', '.text'],
            ['*_lshrdi3.*', '.text'],
            ['*_ashldi3.*', '.text'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    LIB_LW64_UMUL => {
        DESCRIPTION    => "Unsigned 64-bit multiplication used by both .libLw64 and .libLwF32",
        INPUT_SECTIONS =>
        [
            ['*_muldi3.*' , '.text'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    LIB_BIG_INT =>
    {
        # TODO: Revisit to see if still need LS version for Volta+ after all HS secure actions implemented.
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libBigInt.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    PVTBUS =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libHdcpbus.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    LIB_SHA1 =>
    {
        # TODO: Revisit to see if still need LS version for Volta+ after all HS secure actions implemented.
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libSha1.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    LIB_I2C =>
    {
        INPUT_SECTIONS =>
        [
            ['*/i2c/*.o', '.text.*'],
            ['*.o', '.imem_libI2c.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    LIB_I2C_HW =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libI2cHw.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    LIB_I2C_SW =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libI2cSw.*'],
        ],
        ENABLED_ON     => [ dPASCAL_and_later, ],
    },

    LIB_DPAUX =>
    {
        DESCRIPTION    => "DP AUX channel access",
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libDpaux.*', ],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    LIB_HDCP22WIRED =>
    {
        DESCRIPTION    => "hdcp22 wired library code",
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libHdcp22wired.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    HDCP22WIRED =>
    {
        DESCRIPTION    => "hdcp22 wired code",
        INPUT_SECTIONS =>
        [
            ['*/task6_*.o', '.text.*'],
            ['*/hdcp22/dpu_libintfchdcp22wired.o', '.text.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    HDCP22_AES =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libHdcp22wiredAes.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, ],
    },

    HDCP22_AKE =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_libHdcp22wiredAke.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    HDCP22_AKEH =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_libHdcp22wiredAkeh.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    HDCP22_LC =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_libHdcp22wiredLc.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    HDCP22_SKE =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_libHdcp22wiredSke.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    HDCP22_REPEATER =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_libHdcp22wiredRepeater.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    HDCP22_CERTRX_SRM =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_libHdcp22wiredCertrxSrm.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    HDCP22_SELWRE_ACTION =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22SelwreAction.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, -DISPLAYLESS, ],
    },

    HDCP22_START_SESSION =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22StartSession.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, -DISPLAYLESS, ],
    },

    HDCP22_VALIDATE_CERTRX_SRM =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22ValidateCertrxSrm.*'],
            ['*.o',   '.imem_hdcp22SrmRevocation.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, -DISPLAYLESS, ],
    },

    HDCP22_RSA_SIGNATURE =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22RsaSignature.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, -DISPLAYLESS, ],
    },

    HDCP22_DSA_SIGNATURE =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22DsaSignature.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, -DISPLAYLESS, ],
    },

    HDCP22_GENERATE_KMKD =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22GenerateKmkd.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, -DISPLAYLESS, ],
    },

    HDCP22_HPRIME_VALIDATION =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22HprimeValidation.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, -DISPLAYLESS, ],
    },

    HDCP22_LPRIME_VALIDATION =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22LprimeValidation.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, -DISPLAYLESS, ],
    },

    HDCP22_GENERATE_SESSIONKEY =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22GenerateSessionkey.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, -DISPLAYLESS, ],
    },

    HDCP22_CONTROL_ENCRYPTION =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22ControlEncryption.*'],
            ['*.o',   '.imem_hdcp22WriteDpEcf.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, -DISPLAYLESS, ],
    },

    HDCP22_VPRIME_VALIDATION =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22VprimeValidation.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, -DISPLAYLESS, ],
    },

    HDCP22_MPRIME_VALIDATION =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22MprimeValidation.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, -DISPLAYLESS, ],
    },

    HDCP22_END_SESSION =>
    {
        INPUT_SECTIONS =>
        [
            ['*.o',   '.imem_hdcp22EndSession.*'],
        ],
        ENABLED_ON     => [ GM20x_thru_dPASCAL, -DISPLAYLESS, ],
    },

    LIB_SE =>
    {
        NAME => 'libSE',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libSE'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
    },

    LIB_SHA =>
    {
        # TODO: Revisit to see if still need LS version for Volta+ after all HS secure actions implemented.
        NAME => 'libSha',
        INPUT_SECTIONS =>
        [
            ['*.o', '.imem_libSha.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    DUMMYTASK =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task7_*.o', '.text.*'],
        ],
        ENABLED_ON     => [ ],
    },

    SCANOUT_LOGGING =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task8_*.o', '.text.*'],
        ],
        ENABLED_ON     => [ GK10X_thru_GP10X, ],
    },

    MSCGWITHFRL =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task9_*.o', '.text.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    ISOHUB =>
    {
        INPUT_SECTIONS =>
        [
            ['*/isohub/*/*.o', '.text.*'],
        ],
        ENABLED_ON     => [ GM20X_and_later, ],
    },

    VRR =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task4_*.o', '.text.*'],
            ['*/vrr/lw/dpu_objvrr.o', '.text.*'],
            ['*/vrr/v02/*.o', '.text.*'],
        ],
        ENABLED_ON     => [ GP10X_and_later, ],
    },
];

# return the raw data of Overlays definition
return $imemOverlaysRaw;

