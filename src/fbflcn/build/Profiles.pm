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
# Defines all known fbfalcon builds.  Each Build has a mapped profile defined in ./config/fbfalcon-config.cfg
#

my $profilesRaw =
[
    'v0101' =>
    {
        # link information
        IMEM             => 16,        # (kB) physical IMEM
        DMEM             => 16,        # (kB) physical DMEM
        DESC             => 1,
        IMEM_VA          => 64,       # (kB)
        RM_SHARE         => 0x400,    # (bytes) 1 kB
    },
    #
    # the configs for tu10x family are organized as FBFLCN-CHIP-MEMORY with CHIP-MEMORY setup as configs
    # all memories share the same build profile, so each profile here contains a set of MEMORY configs.
    #
    'tu10x' =>
    {
        # link information
        IMEM             => 64,        # (kB) physical IMEM
        DMEM             => 64,        # (kB) physical DMEM
        DESC             => 1,
        IMEM_VA          => 64,       # (kB)
        RM_SHARE         => 0x400,    # (bytes) 1 kB
    },

    #
    # the configs for ga10x family are organized as FBFLCN-CHIP-MEMORY with CHIP-MEMORY setup as configs
    # all memories share the same build profile, so each profile here contains a set of MEMORY configs.
    #
    'ga100'=>
    {
        # link information
        IMEM             => 64,        # (kB) physical IMEM
        DMEM             => 64,        # (kB) physical DMEM
        DESC             => 1,
        IMEM_VA          => 64,       # (kB)
        RM_SHARE         => 0x400,    # (bytes) 1 kB
    },
    # ga10x does replicate the setup used for tu10x
    #
    'ga10x' =>
    {
        # link information
        IMEM             => 64,        # (kB) physical IMEM
        DMEM             => 64,        # (kB) physical DMEM
        DESC             => 1,
        IMEM_VA          => 256,       # (kB)
        RM_SHARE         => 0x400,     # (bytes) 1 kB
        PAFALCON_INCLUDE => "ga10x_pafalcon",
    },
    'ga10x_pafalcon' =>
    {
        # link information
        IMEM             => 8,        # (kB) physical IMEM
        DMEM             => 8,        # (kB) physical DMEM
        DESC             => 1,
        IMEM_VA          => 9,        # (kB)
        RM_SHARE         => 0x400,    # (bytes) 1 kB
    },
    #
    # ad10x targets are replicated ga10x targets
    #
    'ad10x' =>
    {
        # link information
        IMEM             => 64,       # (kB) physical IMEM
        DMEM             => 64,       # (kB) physical DMEM
        DESC             => 1,
        IMEM_VA          => 256,      # (kB)
        RM_SHARE         => 0x400,    # (bytes) 1 kB
        PAFALCON_INCLUDE => "ad10x_pafalcon",
    },
    'ad10x_pafalcon' =>
    {
        # link information
        IMEM             => 8,        # (kB) physical IMEM
        DMEM             => 8,        # (kB) physical DMEM
        DESC             => 1,
        IMEM_VA          => 9,        # (kB)
        RM_SHARE         => 0x400,    # (bytes) 1 kB
    },
    'gh100' =>
    {
        # link information
        IMEM             => 64,        # (kB) physical IMEM
        DMEM             => 64,        # (kB) physical DMEM
        DESC             => 1,
        IMEM_VA          => 64,       # (kB)
        RM_SHARE         => 0x400,    # (bytes) 1 kB
    },
    
    #
    # hopper gddr definitons gh20x
    #
    'gh20x' =>
    {
        # link information
        IMEM             => 64,        # (kB) physical IMEM
        DMEM             => 64,        # (kB) physical DMEM
        DESC             => 1,
        IMEM_VA          => 256,       # (kB)
        RM_SHARE         => 0x400,    # (bytes) 1 kB
        PAFALCON_INCLUDE => "gh20x_pafalcon",
    },
    'gh20x_pafalcon' =>
    {
        # link information
        IMEM             => 8,        # (kB) physical IMEM
        DMEM             => 8,        # (kB) physical DMEM
        DESC             => 1,
        IMEM_VA          => 9,        # (kB)
        RM_SHARE         => 0x400,    # (bytes) 1 kB
    },
];

# return the raw data of Profiles definition
return $profilesRaw;

