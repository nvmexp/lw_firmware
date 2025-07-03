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
# Defines all known DPU builds.  Each Build has a mapped profile defined in dpu-config.cfg
#
#   A Build contains the following information:
#
#   v0205 =>
#   {
#       ###
#       ### Information used to lwstomize the linker script (g_sections.ld)
#       ###
#       IMEM            => 24,       # (kB) physical IMEM
#       DMEM            => 24,       # (kB) physical DMEM
#       DESC            => 1,        # (kB) < TODO : add description >
#       IMEM_VA         => 64,       # (kB) Size of IMEM va space.
#       RM_SHARE        => 0x400,    # (bytes)
#           The space is shared between RM and Flcn for communication.  It is resident
#           in physically addressable DMEM.
#       ISR_STACK       => 140,      # (bytes) size of the ISR stack allocated
#       ESR_STACK       => 140,      # (bytes) size of the ESR stack allocated
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
#   },

my $profilesRaw =
[
    v0201 =>
    {
        #  link information
        IMEM             => 16,       # (kB)
        DMEM             => 8,        # (kB)
        DESC             => 1,
        IMEM_VA          => 64,       # (kB)
        RM_SHARE         => 0x400,    # (bytes) 1 kB
        ISR_STACK        => 160,      # (bytes)
        ESR_STACK        => 160,      # (bytes)

        #  queue length information
        CMD_QUEUE_LENGTH => 0x40,
        MSG_QUEUE_LENGTH => 0x80,
    },

    v0205 =>
    {
        #  link information
        IMEM             => 24,       # (kB)
        DMEM             => 24,       # (kB)
        DESC             => 1,
        IMEM_VA          => 64,       # (kB)
        RM_SHARE         => 0x400,    # (bytes) 1 kB
        ISR_STACK        => 160,      # (bytes)
        ESR_STACK        => 160,      # (bytes)

        #  queue length information
        CMD_QUEUE_LENGTH => 0x80,
        MSG_QUEUE_LENGTH => 0x80,
    },

    v0207 =>
    {
        #  link information
        IMEM             => 24,       # (kB)
        DMEM             => 24,       # (kB)
        DESC             => 1,
        IMEM_VA          => 64,       # (kB)
        RM_SHARE         => 0x400,    # (bytes) 1 kB
        ISR_STACK        => 160,      # (bytes)
        ESR_STACK        => 160,      # (bytes)

        #  queue length information
        CMD_QUEUE_LENGTH => 0x80,
        MSG_QUEUE_LENGTH => 0x80,
    },

    gv100 =>
    {
        # link informvation
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 1,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 164,      # (blocks) Enable single block for VA

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM


        ISR_STACK        => 256,      # (bytes)
        ESR_STACK        => 200,      # (bytes)

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)
    },

    tu10x =>
    {
        # link informvation
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 1,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 164,      # (blocks) Enable single block for VA

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        ISR_STACK        => 256,      # (bytes)
        ESR_STACK        => 200,      # (bytes)

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)
    },

    tu116 =>
    {
        # link informvation
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 1,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 164,      # (blocks) Enable single block for VA

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        ISR_STACK        => 256,      # (bytes)
        ESR_STACK        => 200,      # (bytes)

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)
    },

    ga10x =>
    {
        # link informvation
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 1,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 164,      # (blocks) Enable single block for VA

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 1,

        ISR_STACK        => 256,      # (bytes)
        ESR_STACK        => 200,      # (bytes)

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)
    },

    ad10x =>
    {
        # link informvation
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 1,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 164,      # (blocks) Enable single block for VA

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 1,

        ISR_STACK        => 256,      # (bytes)
        ESR_STACK        => 200,      # (bytes)

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)
    },

    ga10x_boot_from_hs =>
    {
        # link informvation
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 1,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 164,      # (blocks) Enable single block for VA

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 3,

        RM_PATCHING_HS_SIGNATURE_ENABLED => 1,  # generate linker script variable for HS Ovl signature array in case runtime patching of sigs is enabled

        ISR_STACK        => 256,      # (bytes)
        ESR_STACK        => 200,      # (bytes)

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        BOOT_FROM_HS_ENABLED  => 1,   # Use secure bootloader
    },

    ad10x_boot_from_hs =>
    {
        # link informvation
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 1,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 164,      # (blocks) Enable single block for VA

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 1,

        RM_PATCHING_HS_SIGNATURE_ENABLED => 1,  # generate linker script variable for HS Ovl signature array in case runtime patching of sigs is enabled

        ISR_STACK        => 256,      # (bytes)
        ESR_STACK        => 200,      # (bytes)

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        BOOT_FROM_HS_ENABLED  => 1,   # Use secure bootloader
    },
];

# return the raw data of Profiles definition
return $profilesRaw;

