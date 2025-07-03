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
# Defines all known SOE builds.  Each Build has a mapped profile defined in soe-config.cfg
#
#   A Build contains the following information:
#
#   GP10X =>
#   {
#       ###
#       ### Information used to lwstomize the linker script (g_sections.ld)
#       ###
#       IMEM            => 32,       # (kB) physical IMEM
#       DMEM            => 16,       # (kB) physical DMEM
#       EMEM            => 8,        # (kB) physical EMEM
#           [optional] EMEM (External memory) is enabled when this field is specified.
#           In HS (heavy secure) mode, falcon is locked down.  The communication
#           between RM and Flcn is through EMEM.  It is mapped at the top of DMEM VA
#           space (i.e. from DMEM_VA_MAX to DMEM_VA_MAX + EMEM_SIZE). It doesn't support
#           page ins/page outs.
#       DESC            => 1,        # (kB) < TODO : add description >
#
#       IMEM_VA         => 64,       # (kB) Size of IMEM va space.
#       DMEM_VA         => 16384,    # (kB) Size of DMEM va space
#           [optional] DMEM virtual addressing is enabled when this field is specified
#       DMEM_VA_BOUND   => 192,      # (blocks) 48kB, start of virtually addressable DMEM
#                                    # or equivalently, end of physically addressable DMEM
#           [ONLY_IF DMEM_VA is used]
#
#       RM_SHARE        => 0x2000,   # (bytes)
#           The space is shared between RM and Flcn for communication.  It is resident
#           in physically addressable DMEM, or in EMEM for falcons that can switch to
#           HS mode on the fly.
#
#       FREEABLE_HEAP    => 4,       # (blocks) size of freeable heap in physically addressable DMEM space
#
#       ISR_STACK       => 256,      # (bytes) size of the ISR stack allocated
#       ESR_STACK       => 200,      # (bytes) size of the ESR stack allocated
#
#       ###
#       ### The length of each command/message queue
#       ###
#       CMD_QUEUE_LENGTH => 0x80,           # (bytes)
#       MSG_QUEUE_LENGTH => 0x80,           # (bytes)
#           # Generates following defines in 'g_profile.h' :
#           #   #define PROFILE_CMD_QUEUE_LENGTH (0x80ul)
#           #   #define PROFILE_MSG_QUEUE_LENGTH (0x80ul)
#
#       ###
#       ### The method array sizes and extents for channel interface
#       ###
#       APP_METHOD_ARRAY_SIZE => 0x10,
#       APP_METHOD_MIN        => 0x100,
#       APP_METHOD_MAX        => 0x1CF,
#           # Generates following defines:
#           #   #define APP_METHOD_ARRAY_SIZE (0x10ul)
#           #   #define APP_METHOD_MIN (0x100ul)
#           #   #define APP_METHOD_MIN (0x1CFul)
#   },
#

my $profilesRaw =
[
    LR10 =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)
        # TODO_Apoorvg: COnfirm DMEM_VA and update this configuration if required for SOE
        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 255,      # (blocks) start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        SAFERTOS_IMEM_RESERVED    => 500,         # (bytes) Reserve IMEM for SafeRTOS for Turing
        SAFERTOS_DMEM_RESERVED    => 0,           # (bytes) Reserve DMEM for SafeRTOS for Turing

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 0,  # generate the linker script symbols for HS ucode encryption

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

    },

    LS10 =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)
        # TODO_Apoorvg: COnfirm DMEM_VA and update this configuration if required for SOE
        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 255,      # (blocks) start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        SAFERTOS_IMEM_RESERVED    => 500,         # (bytes) Reserve IMEM for SafeRTOS for Turing
        SAFERTOS_DMEM_RESERVED    => 0,           # (bytes) Reserve DMEM for SafeRTOS for Turing

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 0,  # generate the linker script symbols for HS ucode encryption

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

    },
];

# return the raw data of Profiles definition
return $profilesRaw;
