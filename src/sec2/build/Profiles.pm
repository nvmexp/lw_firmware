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
# Defines all known SEC2 builds.  Each Build has a mapped profile defined in sec2-config.cfg
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
    #
    # WARNING : Make sure to update GP10X_PRMODS profile if you are updating GP10X profile
    #
    GP10X =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 256,      # (bytes)
        ESR_STACK        => 200,      # (bytes)

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    GP10X_PRMODS =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    GP10X_NOUVEAU =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    GV10X =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    TU10X =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)


        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    TU10X_DUALCORE   =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)


        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    TU10X_PRMODS =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 164,      # (blocks) 41kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    TU116 =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)


        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    TU116_DUALCORE   =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)


        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    TU116_PRMODS =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 164,      # (blocks) 41kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    TU10A =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 164,      # (blocks) 41kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        # AUTO profile does not have HS overlays for now. Uncomment when HS ovelays are to be added.
        #HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    GA100 =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 2,

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    GA100_NOUVEAU =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 2,

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    GA100_APM =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 2,

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    GA100_RISCV_LS =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used
        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    GA10X =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    GA10X_NEW_WPR_BLOB =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        #
        # TODO: Temporarily disabling freeable heap, as it is failing with NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL > 1
        # We need it for Playready, so should be enabled before that by fixing dependency on NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL
        #
        # FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 2,

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    GA10X_PRMODS =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 164,      # (blocks) 41kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used


        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    AD10X =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 1,
        
        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (biytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    AD10X_BOOT_FROM_HS =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 1,
        
        RM_PATCHING_HS_SIGNATURE_ENABLED => 1,  # generate linker script variable for HS Ovl signature array in case runtime patching of sigs is enabled

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (biytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
	
        BOOT_FROM_HS_ENABLED  => 1,     # Use secure bootloader
    },

    GA10X_BOOT_FROM_HS =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 5,

        RM_PATCHING_HS_SIGNATURE_ENABLED => 1,  # generate linker script variable for HS Ovl signature array in case runtime patching of sigs is enabled

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)

        BOOT_FROM_HS_ENABLED  => 1,     # Use secure bootloader
    },

    GA10X_PR44_ALT_IMG_BOOT_FROM_HS =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 4,

        RM_PATCHING_HS_SIGNATURE_ENABLED => 1,  # generate linker script variable for HS Ovl signature array in case runtime patching of sigs is enabled

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)

        BOOT_FROM_HS_ENABLED  => 1,     # Use secure bootloader
    },

    GA10X_PRMODS_BOOT_FROM_HS =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 164,      # (blocks) 41kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 4,

        RM_PATCHING_HS_SIGNATURE_ENABLED => 1,  # generate linker script variable for HS Ovl signature array in case runtime patching of sigs is enabled

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)

        BOOT_FROM_HS_ENABLED  => 1,     # Use secure bootloader
    },

    GA10X_APM_BOOT_FROM_HS =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 120,      # (blocks) 30kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 3,

        RM_PATCHING_HS_SIGNATURE_ENABLED => 1,  # generate linker script variable for HS Ovl signature array in case runtime patching of sigs is enabled

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)

        BOOT_FROM_HS_ENABLED  => 1,     # Use secure bootloader
    },

    GA10X_NOUVEAU_BOOT_FROM_HS =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used

        NUM_SIG_PER_HS_UCODE_LINKER_SYMBOL => 4,

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)

        BOOT_FROM_HS_ENABLED  => 1,     # Use secure bootloader
    },

    GH100 =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used
        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    GH20X =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },

    G00x =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        EMEM             => 8,        # (kB) physical EMEM
        DESC             => 2,        # (kB)

        IMEM_VA          => 4096,     # (kB)  4MB IMEM va space
        DMEM_VA          => 16384,    # (kB) 16MB DMEM va space
        DMEM_VA_BOUND    => 148,      # (blocks) 37kB, start of virtually addressable DMEM space

        RM_SHARE         => 0x2000,   # (bytes) 8 kB in EMEM

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        HS_UCODE_PKC_ENABLED          => 1,     # generate the linker script symbols and allocate space for when PKC is enabled
        HS_UCODE_PKC_SIG_SIZE_IN_BITS => 3072,  # size of the signature when PKC is used
        HS_UCODE_ENCRYPTION_LINKER_SYMBOLS => 1,  # generate the linker script symbols for HS ucode encryption

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
    },
];

# return the raw data of Profiles definition
return $profilesRaw;
