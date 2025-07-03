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
# Defines all known SEC2 DMEM overlays.
#
# Overlays:
#     An "overlay" is any generic piece of data that can be paged in and out of
#     DMEM at runtime. To gcc, this is simply a data section. That is, the
#     overlays used here are not considered by gcc as true overlays as each
#     section has its own unique data address-space.
#
#     NOTE: The ordering of the overlay definitions in this module is
#           significant. Overlays will be placed in the final image in the
#           order that they are defined in this file.
#
# Overlay Attributes:
#
#     DESCRIPTION
#         Optional description of the DMEM overlay.
#
#     NAME
#         Defines the name of the output-section that is created for the
#         overlay. By default, the overlay name is comes from the hash key used
#         below when defining the overlay. The key name is colwerted to
#         camel-case with the leading character in lower-case. This attribute
#         may be used as an override if the resulting name is undesirable.
#
#     SECTION_DESCRIPTION
#         This attribute may be used to specify the exact linker-script text
#         used to define the overlay. It overrides all other attributes (such
#         as the INPUT_SECTIONS) attribute. It should only be used under
#         special cirlwmstances where special syntax is required that is not
#         supported by the linker-script generation tool (ldgen).
#
#     MAX_SIZE_BLOCKS
#         The maximum size of this DMEM overlay in blocks (256 byte units)
#
#     FLAGS
#         Specifies extra attributes using flags, as below:
#         :RESIDENT
#             Specifies that the overlay is a resident overlay (so will be
#             loaded into resident area). It is loaded by the bootloader when
#             booting RTOS, and is never a candidate to be kicked out of
#             physical memory. So we don't need to explicitly load/unload it
#             from the app level.
#
#     ENABLED_ON
#         Defines the list of chips on which the overlay is enabled. If an
#         overlay is not used on a particular chip, that chip must be excluded
#         from this list. We generate symbols in DMEM for every overlay that is
#         enabled, and that oclwpies (limited) DMEM resources. Hence, it is
#         important to make sure that we only enable the overlay only on chips
#         where they are used. Every overlay must specify this attribute.
#
#

my $dmemOverlaysRaw =
[
    GFE =>
    {
        DESCRIPTION     => 'GFE global data',
        MAX_SIZE_BLOCKS => 4,
        ENABLED_ON      => [ GP10X_and_later, -GA100, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    HDCPMC =>
    {
        DESCRIPTION     => 'Miracast HDCP global data',
        MAX_SIZE_BLOCKS => 24,
        ENABLED_ON      => [ ],
    },

    PR =>
    {
        DESCRIPTION     => 'Playready data',
        MAX_SIZE_BLOCKS => 76,
        ENABLED_ON      => [ GP10X_and_later, -GA100, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    PR_SHARED =>
    {
        DESCRIPTION     => 'Playready shared data',
        MAX_SIZE_BLOCKS => 3,
        FLAGS           => ':RESIDENT' ,
        ENABLED_ON      => [ GP10X_and_later, -GA100],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    STACK_CMDMGMT =>
    {
        DESCRIPTION     => 'Stack for the CMDMGMT task.',
        MAX_SIZE_BLOCKS => 2,
        ENABLED_ON      => [ GP10X_and_later, ],
    },

    STACK_CHNMGMT =>
    {
        DESCRIPTION     => 'Stack for the CHNMGMT task.',
        MAX_SIZE_BLOCKS => 2,
        ENABLED_ON      => [ GP10X_and_later, ],
    },

    STACK_HDCPMC =>
    {
        DESCRIPTION     => 'Stack for the Miracast HDCP task.',
        MAX_SIZE_BLOCKS => 16,
        ENABLED_ON      => [ ],
    },

    STACK_GFE =>
    {
        DESCRIPTION     => 'Stack for the GFE task.',
        MAX_SIZE_BLOCKS => 16,
        ENABLED_ON      => [ GP10X_and_later, -GA100, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    STACK_LWSR =>
    {
        DESCRIPTION     => 'Stack for the LWSR task.',
        MAX_SIZE_BLOCKS => 4,
        ENABLED_ON      => [ ],
    },

    STACK_HWV =>
    {
        DESCRIPTION     => 'Stack for the HWV task.',
        MAX_SIZE_BLOCKS => 4,
        ENABLED_ON      => [ TU10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    STACK_PR =>
    {
        DESCRIPTION     => 'Stack for the PR task.',
        MAX_SIZE_BLOCKS => 16,
        ENABLED_ON      => [ GP10X_and_later, -GA100, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    STACK_VPR =>
    {
        DESCRIPTION     => 'Stack for the VPR task.',
        MAX_SIZE_BLOCKS => 4,
        ENABLED_ON      => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-ga100', '-sec2-*_nouveau*', ],
    },

    VPR =>
    {
        DESCRIPTION     => 'VPR global data',
        MAX_SIZE_BLOCKS => 1,
        ENABLED_ON      => [ GP10X_and_later, ],
        PROFILES        => [ 'sec2-*', '-sec2-tu10a', '-sec2-ga100', '-sec2-*_nouveau*', ],
    },

    STACK_ACR =>
    {
        DESCRIPTION     => 'Stack for the ACR task.',
        MAX_SIZE_BLOCKS => 4,
        ENABLED_ON      => [ TU10X_and_later, ],
    },

    ACR =>
    {
        DESCRIPTION     => 'ACR global data',
        MAX_SIZE_BLOCKS => 21,
        ENABLED_ON      => [ TU10X_and_later, ]
    },

    STACK_APM =>
    {
        DESCRIPTION     => 'Stack for the APM task.',
        MAX_SIZE_BLOCKS => 4,
        ENABLED_ON      => [ GA100, GA10X, ],

        # Task under development, lwrrently only support for APM profiles
        PROFILES       => [ 'sec2-*_apm*', ],
    },

    APM =>
    {
        DESCRIPTION     => 'Global data for APM task.',
        MAX_SIZE_BLOCKS => 19,
        ENABLED_ON      => [ GA100, GA10X, ],

        # Task under development, lwrrently only support for APM profiles
        PROFILES       => [ 'sec2-*_apm*', ],
    },

    STACK_SPDM =>
    {
        #
        # NOTE: Be careful when enabling overlay, as includes open-source
        # libspdm code. Releases must meet SWIPAT requirements when included.
        # See LwBug 3314381 for more info.
        #
        DESCRIPTION     => 'Stack for the SPDM task.',
        MAX_SIZE_BLOCKS => 33,

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
        DESCRIPTION     => 'Global data for SPDM task.',
        MAX_SIZE_BLOCKS => 47,

        ENABLED_ON     => [ GA100, GA10X, ],
        PROFILES       => [ 'sec2-*_apm*', ],
    },

    APM_SPDM_SHARED =>
    {
        DESCRIPTION     => 'Shared global data between SPDM and APM tasks.',
        MAX_SIZE_BLOCKS => 27,

        ENABLED_ON     => [ GA100, GA10X, ],
        PROFILES       => [ 'sec2-*_apm*', ],
    },

];

# return the raw data of Overlays definition
return $dmemOverlaysRaw;

