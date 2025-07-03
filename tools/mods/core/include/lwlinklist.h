/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// List of Non-GPU LwLink devices.
//
// *please* think twice before you #include this file.
// This file is mainly a mirror of gpulist.h and all its rules about
// where to be included also apply here.

#ifndef DEFINE_LWL_DEV
#error lwlinklist.h should only be #included in device.h and reghaltable.cpp
#endif

// This is required because windows compilers don't always play nicely with __VA_ARGS__
#define EXPAND( x ) x

#ifndef DEFINE_ARM_MFG_LWL_DEV
    #if (defined(LWCPU_FAMILY_ARM) && defined(LINUX_MFG)) || defined(REGHAL_T194LWL)
        #define DEFINE_ARM_MFG_LWL_DEV(...) EXPAND(DEFINE_LWL_DEV(__VA_ARGS__))
    #else
        #define DEFINE_ARM_MFG_LWL_DEV(...) EXPAND(DEFINE_OBS_LWL_DEV(__VA_ARGS__))
    #endif
#endif

#ifndef DEFINE_SIM_LWL_DEV
    #if defined(SIM_BUILD) || defined(REGHAL_SIM)
        #define DEFINE_SIM_LWL_DEV(...) EXPAND(DEFINE_LWL_DEV(__VA_ARGS__))
    #else
        #define DEFINE_SIM_LWL_DEV(...) EXPAND(DEFINE_OBS_LWL_DEV(__VA_ARGS__))
    #endif
#endif

// The DEFINE_LWL macro is used to specify GPUs that are enabled or disabled
// by lwconfig.
// For information how this works under the hood, please refer to gpulist.h
#define DEFINE_LWL(CFG, ...) DEFINE_LWL_INTERNAL(LWCFG(GLOBAL_LWSWITCH_IMPL_ ## CFG))(__VA_ARGS__)
#define DEFINE_LWL_INTERNAL(X) DEFINE_LWL_INTERNAL_VALUE(X)
#define DEFINE_LWL_INTERNAL_VALUE(X) DEFINE_LWL_ ## X
#define DEFINE_LWL_1 DEFINE_LWL_DEV
#define DEFINE_LWL_0 DEFINE_OBS_LWL_DEV

// See //hw/doc/engr/Dev_ID/DeviceID_master_list.txt

//                      DIDStart  DIDEnd   ChipId  LwId         Constant  HwrefDir         DispDir PcieType      LwLinkType      LwLinkThroughputCountersType   I2cType     GpioType     
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
DEFINE_LWL_DEV(         0x0000,   0x0000,  0x000,  IBMNPU,      0xF001,   pascal/gp100,    v02_07, IbmNpuPcie,   IbmNpuLwLink                                                              ) // IBMNPU
DEFINE_LWL(SVNP01,      0x10F5,   0x10F5,  0x005,  SVNP01,      0xF002,   lwswitch/svnp01, None,   LwSwitchPcie, LwSwitchLwLink,  LwLinkThroughputCountersV2SW, LwSwitchI2c, LwSwitchGpio  ) // SVNP01 (Willow) (LwSwitch)
DEFINE_LWL(LR10,        0x0000,   0x0000,  0x000,  LR10,        0xF003,   lwswitch/lr10,   None,   LimerockPcie, LimerockLwLink,  LwLinkThroughputCountersV3,   LwSwitchI2c, LwSwitchGpio  ) // LR10 (Limerock) (LwSwitch)
DEFINE_LWL(LR10,        0x0000,   0x0000,  0x000,  IBMP9P,      0xF004,   ampere/ga100,    None,   IbmNpuPcie,   IbmP9PLwLink                                                              ) // IBMP9P
DEFINE_LWL(LS10,        0x0000,   0x0000,  0x000,  LS10,        0xF005,   lwswitch/ls10,   None,   LagunaPcie,   LagunaLwLink,    LwLinkThroughputCountersV4,   LwSwitchI2c, LwSwitchGpio  ) // LS10 (Laguna Seca) (LwSwitch)
DEFINE_LWL(LS10,        0x0000,   0x0000,  0x000,  S000,        0xF006,   lwswitch/s000,   None,   LagunaPcie,   LagunaLwLink,    LwLinkThroughputCountersV4,   LwSwitchI2c, LwSwitchGpio  ) // S000 (Fmodel) (Laguna Seca) (LwSwitch)

DEFINE_ARM_MFG_LWL_DEV( 0x0000,   0x0000,  0x000,  T194MFGLWL,  0xF100,   t19x/t194,       None,   SimPcie,      XavierMfgLwLink, LwLinkThroughputCountersV2SW                             ) // CheetAh (MFG MODS)

// Simulated Devices
DEFINE_SIM_LWL_DEV(     0x0000,   0x0000,  0x000,  SIMEBRIDGE,  0xFF00,   None,            None,   SimPcie,      SimEbridgeLwLink                                                          ) // SIM EBRIDGE
DEFINE_SIM_LWL_DEV(     0x0000,   0x0000,  0x000,  SIMIBMNPU,   0xFF01,   None,            None,   SimPcie,      SimLwLink                                                                 ) // SIM IBMNPU
DEFINE_SIM_LWL_DEV(     0x0000,   0x0000,  0x000,  SIMLWSWITCH, 0xFF02,   None,            None,   SimPcie,      SimLwSwitchLwLink, SimI2c                                                 ) // SIM SVNP01 (Willow) (LwSwitch)
DEFINE_SIM_LWL_DEV(     0x0000,   0x0000,  0x000,  SIMLR10,     0xFF03,   None,            None,   SimPcie,      SimLimerockLwLink, SimI2c                                                 ) // SIM LR10 (Limerock) (LwSwitch)
DEFINE_SIM_LWL_DEV(     0x0000,   0x0000,  0x000,  SIMGPU,      0xFF04,   None,            None,   SimPcie,      SimLwLink,         SimI2c                                                 ) // SIM GPU
DEFINE_LWL(LR10,        0x0000,   0x0000,  0x000,  TREX,        0xFF05,   None,            None,                 TrexLwLink                                                                ) // TREX pseudo-device (LR10)

#undef DEFINE_ARM_MFG_LWL_DEV
#undef DEFINE_SIM_LWL_DEV
#undef EXPAND
