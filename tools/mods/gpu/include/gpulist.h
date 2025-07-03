/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// List of GPUs.
//
// *please* think twice before you #include this file.  Its intent is to
// unify all of the places where a list of possible GPUs are required
// 1.  In gpu.cpp, tceId enum
// 2.  In gpu_stub.h, to set up the LwDeviceId enum for non-GPU builds
// 3.  In js_gpu.cpp, to expose eaco associate each GPU with a device ID and a FrameBuffer
// 4.  In gpu.h, to set up the LwDevih GPU as a Javascript constant
// If you need it in some other place, you're probably doing something wrong.
// Please get buy-in from the Mods-GPU email alias before you proceed.
//
// In some case, hardware has created functionally-equivalent GPUs with
// different device ID ranges.  Examples of this are LW40 and LW48, which are
// the same netlist build at different foundries.  MODS treats these
// identically.  To define a second PCI device ID range for an existing GPU
// use DEFINE_DUP_GPU()
#ifndef DEFINE_NEW_GPU
#error gpulist.h should only be #included in gpu.cpp, gpu.h, gpu_stub.h, and js_gpu.cpp
#endif

// See //hw/doc/engr/Dev_ID/DeviceID_master_list.txt

// Chip Id is
//    Big GPU : (PMC_BOOT_0_ARCHITECTURE << 4) | PMC_BOOT_0_IMPLEMENTATION
//    SOC GPU : 0x0E000000 | (SOC CHIP ID)

// This is required because windows compilers don't always play nicely with __VA_ARGS__
#define EXPAND( x ) x

#ifndef DEFINE_SIM_GPU
    #if defined(SIM_BUILD) || defined(REGHAL_SIM)
        #define DEFINE_SIM_GPU(...) EXPAND(DEFINE_NEW_GPU(__VA_ARGS__))
    #else
        #define DEFINE_SIM_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, Constant, Family, ...) \
                DEFINE_OBS_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, Constant, Family)
    #endif
#endif

// The DEFINE_GPU macro is used to specify GPUs that are enabled or disabled
// by lwconfig.
//
// The first argument is the name of the lwconfig macro (after GLOBAL_)
// that is used to determine whether a particular GPU in the table below is
// enabled or disabled.  For example, if the lwconfig test macro is
// GLOBAL_GPU_IMPL_TU102, then we only put GPU_IMPL_TU102 as the first argument.
//
// The DEFINE_GPU macro works as follows:
// - If the specified lwconfig macro resolves to 1 (enabled), then the DEFINE_GPU
//   macro is replaced with DEFINE_GPU_1, which in turn is replaced with
//   DEFINE_NEW_GPU.
// - If the lwconfig macro resolves to 0 (disabled), then the DEFINE_GPU macro
//   is replaced with DEFINE_GPU_0, which in turn is replaced with DEFINE_DIS_GPU.
//   DEFINE_DIS_GPU marks the GPU as disabled.  In many places code for "obsolete"
//   GPUs is simply not compiled.
// An example of this is Gpu ids: for GPUs enabled by lwconfig, the GPU enum
// identifiers are declared, but for GPUs disabled by lwconfig these enum
// identifiers don't exist, so any code trying to reference them will not compile.
//
#define DEFINE_GPU(CFG, ...) DEFINE_GPU_INTERNAL(LWCFG(GLOBAL_ ## CFG)(__VA_ARGS__))
#define DEFINE_GPU_INTERNAL(X) DEFINE_GPU_INTERNAL_VALUE(X)
#define DEFINE_GPU_INTERNAL_VALUE(X) DEFINE_GPU_ ## X
#define DEFINE_GPU_1 DEFINE_NEW_GPU
#define DEFINE_GPU_0 DEFINE_DIS_GPU

// By default, disabled GPUs behave like obsolete GPUs.  We have the distinction
// between disabled and obsolete GPUs so that we can still export GPU ids to JS
// for GPUs which are disabled via lwconfig, but not obsolete.
#ifndef DEFINE_DIS_GPU
#define DEFINE_DIS_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, Constant, Family, ...) \
        DEFINE_OBS_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, Constant, Family)
#endif

#define NULL_INTERFACE

// SIM builds do not need the XavierTegraLwLink interface and it actually causes problems
// on SIM builds due to its dependency on RMAPI CheetAh header files
#ifdef SIM_BUILD
    #define XavierTegraLwLink NULL_INTERFACE
#endif

// Don't include fuse files in CheetAh builds
#ifdef TEGRA_MODS
    #define KeplerFuse  NullFuse
    #define MaxwellFuse NullFuse
    #define PascalFuse  NullFuse
    #define VoltaFuse   NullFuse
    #define TuringFuse  NullFuse
#endif

// Note that interfaces are actually part of the GPU subdevice implementation so if
// a GPU subdevice points to a different GPU subdevice class that it actaully is
// it will get the interfaces for the GPU it points to rather than the ones defined
// in the DEFINE_GPU call
//
// For example, if GH100 points to GA100GpuSubdevice it will always get the interfaces
// from the DEFINE_GPU call for GA100 rather than the ones for GH100.

//                DIDStart      DIDEnd         ChipId  LwId   Constant Family
// ----------------------------------------------------------------------------
DEFINE_OBS_GPU(     0x0020,     0x0020,        0x04,   LW4,    0x04,   None )
DEFINE_OBS_GPU(     0x0028,     0x002D,        0x05,   LW5,    0x05,   None )
DEFINE_OBS_GPU(     0x00A0,     0x00A0,        0x06,   LWA,    0x0A,   None )
DEFINE_OBS_GPU(     0x0100,     0x0103,        0x10,   LW10,   0x10,   Celsius )
DEFINE_OBS_GPU(     0x0110,     0x0113,        0x11,   LW11,   0x11,   Celsius )
DEFINE_OBS_GPU(     0x01A0,     0x01A0,        0x1A,   LW1A,   0x15,   Celsius )
DEFINE_OBS_GPU(     0x0150,     0x0153,        0x15,   LW15,   0x17,   Celsius )
DEFINE_OBS_GPU(     0x0170,     0x017F,        0x17,   LW17,   0x18,   Celsius )
DEFINE_OBS_GPU(     0x0180,     0x018F,        0x18,   LW18,   0x1A,   Celsius )
DEFINE_OBS_GPU(     0x01F0,     0x01FF,        0x1F,   LW1F,   0x1F,   Celsius )
DEFINE_OBS_GPU(     0x0200,     0x0203,        0x20,   LW20,   0x20,   Celsius )
DEFINE_OBS_GPU(     0x0250,     0x025F,        0x25,   LW25,   0x25,   Celsius )
DEFINE_OBS_GPU(     0x0280,     0x028F,        0x28,   LW28,   0x28,   Celsius )
DEFINE_OBS_GPU(     0x0300,     0x030F,        0x30,   LW30,   0x30,   Rankine )
DEFINE_OBS_GPU(     0x0310,     0x031F,        0x31,   LW31,   0x31,   Rankine )
DEFINE_OBS_GPU(     0x0320,     0x032F,        0x34,   LW34,   0x34,   Rankine )
DEFINE_OBS_GPU(     0x0330,     0x033F,        0x35,   LW35,   0x35,   Rankine )
DEFINE_OBS_GPU(     0x0340,     0x034F,        0x36,   LW36,   0x36,   Rankine )
DEFINE_OBS_GPU(     0x0040,     0x004F,        0x40,   LW40,   0x40,   Lwrie   )
DEFINE_OBS_GPU(     0x0210,     0x021F,        0x48,   LW40,   0x40,   Lwrie   )
DEFINE_OBS_GPU(     0x00C0,     0x00CF,        0x41,   LW41,   0x41,   Lwrie   )
DEFINE_OBS_GPU(     0x0120,     0x012F,        0x42,   LW42,   0x42,   Lwrie   )
DEFINE_OBS_GPU(     0x0140,     0x014F,        0x43,   LW43,   0x43,   Lwrie   )
DEFINE_OBS_GPU(     0x0160,     0x016F,        0x44,   LW44,   0x44,   Lwrie   )
DEFINE_OBS_GPU(     0x0220,     0x022F,        0x44,   LW44,   0x44,   Lwrie   )
DEFINE_OBS_GPU(     0x03D0,     0x03DF,        0x4c,   LW4C,   0x4C,   Lwrie   )
DEFINE_OBS_GPU(     0x0240,     0x024F,        0x4e,   LW4E,   0x4E,   Lwrie   )
DEFINE_OBS_GPU(     0x0530,     0x053F,        0x67,   LW67,   0x48,   Lwrie   )
DEFINE_OBS_GPU(     0x07E0,     0x07EF,        0x63,   LW73,   0x4F,   Lwrie   )
DEFINE_OBS_GPU(     0x0090,     0x009F,        0x47,   G70,    0x47,   Lwrie   )
DEFINE_OBS_GPU(     0x0290,     0x029F,        0x49,   G71,    0x49,   Lwrie   )
DEFINE_OBS_GPU(     0x01D0,     0x01DF,        0x46,   G72,    0x46,   Lwrie   )
DEFINE_OBS_GPU(     0x0390,     0x039F,        0x4B,   G73,    0x4B,   Lwrie   )
DEFINE_OBS_GPU(     0x0190,     0x019F,        0x50,   G80,    0x50,   Tesla   )
DEFINE_OBS_GPU(     0x0400,     0x040B,        0x84,   G84,    0x84,   Tesla   )
DEFINE_OBS_GPU(     0x040D,     0x040F,        0x84,   G84,    0x84,   Tesla   )
DEFINE_OBS_GPU(     0x0420,     0x042F,        0x86,   G86,    0x86,   Tesla   )
DEFINE_OBS_GPU(     0x0600,     0x061F,        0x92,   G92,    0x92,   Tesla   )
DEFINE_OBS_GPU(     0x040C,     0x040C,        0x92,   G92,    0x92,   Tesla   )
DEFINE_OBS_GPU(     0x0410,     0x041F,        0x92,   G92,    0x92,   Tesla   )
DEFINE_OBS_GPU(     0x0620,     0x063F,        0x94,   G94,    0x94,   Tesla   )
DEFINE_OBS_GPU(     0x0640,     0x065F,        0x96,   G96,    0x96,   Tesla   )
DEFINE_OBS_GPU(     0x06E0,     0x06FF,        0x98,   G98,    0x98,   Tesla   )
DEFINE_OBS_GPU(     0x05E0,     0x05FF,        0xA0,   GT200,  0x200,  Tesla   )
DEFINE_OBS_GPU(     0x05C0,     0x05DF,        0xA6,   GT206,  0x206,  Tesla   )
DEFINE_OBS_GPU(     0x0840,     0x085F,        0xAA,   MCP77,  0x207,  Tesla   )
DEFINE_OBS_GPU(     0x0AE0,     0x0AFF,        0xA7,   iGT206, 0x208,  Tesla   )
DEFINE_OBS_GPU(     0x0860,     0x087F,        0xAC,   MCP79,  0x209,  Tesla   )
DEFINE_OBS_GPU(     0x0B00,     0x0B1F,        0xA9,   iGT209, 0x20a,  Tesla   )
DEFINE_OBS_GPU(     0x0A00,     0x0A1F,        0xA2,   GT212,  0x212,  Tesla   )
DEFINE_OBS_GPU(     0x06A0,     0x06BF,        0x00,   GT214,  0x214,  Tesla   )
DEFINE_OBS_GPU(     0x0CA0,     0x0CBF,        0xA3,   GT215,  0x215,  Tesla   )
DEFINE_OBS_GPU(     0x0A20,     0x0A3F,        0xA5,   GT216,  0x216,  Tesla   )
DEFINE_OBS_GPU(     0x0A60,     0x0A7F,        0xA8,   GT218,  0x218,  Tesla   )
DEFINE_OBS_GPU(     0x10C0,     0x10DF,        0xA8,   GT218,  0x218,  Tesla   )
DEFINE_OBS_GPU(     0x0880,     0x089F,        0xAD,   iGT21A, 0x21a,  Tesla   )
DEFINE_OBS_GPU(     0x08A0,     0x08BF,        0xAF,   MCP89,  0x21b,  Tesla   )
DEFINE_OBS_GPU(     0x0E18,     0x0E18,        0x9D,   iGT21D, 0x21d,  Tesla   )
DEFINE_OBS_GPU(     0x0920,     0x093F,        0x9E,   MCP8D,  0x21e,  Tesla   )
DEFINE_OBS_GPU(     0x06C0,     0x06DF,        0xC0,   GF100,  0x400,  Fermi   )
DEFINE_OBS_GPU(     0x1080,     0x109F,        0xC8,   GF100B, 0x40B,  Fermi   )
DEFINE_OBS_GPU(     0x10A0,     0x10BF,        0xC8,   GF100B, 0x40C,  Fermi   )
DEFINE_OBS_GPU(     0x0E20,     0x0E3F,        0xC4,   GF104,  0x404,  Fermi   )
DEFINE_OBS_GPU(     0x0F20,     0x0F3F,        0xC4,   GF104,  0x403,  Fermi   )
DEFINE_OBS_GPU(     0x1200,     0x123F,        0xCE,   GF104B, 0x414,  Fermi   )
DEFINE_OBS_GPU(     0x0DA0,     0x0DBF,        0x00,   GF105,  0x405,  Fermi   )
DEFINE_OBS_GPU(     0x0DC0,     0x0DDF,        0xC3,   GF106,  0x406,  Fermi   )
DEFINE_OBS_GPU(     0x0EE0,     0x0EFF,        0xC3,   GF106,  0x407,  Fermi   )
DEFINE_OBS_GPU(     0x1240,     0x127F,        0xCF,   GF106B, 0x416,  Fermi   )
DEFINE_OBS_GPU(     0x0DE0,     0x0DFF,        0xC1,   GF108,  0x408,  Fermi   )
DEFINE_OBS_GPU(     0x0F00,     0x0F1F,        0xC1,   GF108,  0x409,  Fermi   )
DEFINE_OBS_GPU(     0x1100,     0x113F,        0xD0,   GF110,  0x410,  Fermi   )
DEFINE_OBS_GPU(     0x1140,     0x117F,        0xD7,   GF117,  0x417,  Fermi   )
DEFINE_OBS_GPU(     0x0EA0,     0x0EBF,        0x00,   GF118,  0x418,  Fermi   )
DEFINE_OBS_GPU(     0x1040,     0x107F,        0xD9,   GF119,  0x419,  Fermi   )
DEFINE_OBS_GPU(     0x0CC0,     0x0CDF,        0xC6,   GF110D, 0x50D,  Fermi   )
DEFINE_OBS_GPU(     0x0CE0,     0x0CFF,        0xC7,   GF110F, 0x50F,  Fermi   )
DEFINE_OBS_GPU(     0x0E0D,     0x0E0D,        0xCB,   GF110F2,0x51F,  Fermi   )
DEFINE_OBS_GPU(     0x0FA0,     0x0FA0,        0xCC,   GF110F3,0x52F,  Fermi   )
DEFINE_OBS_GPU(     0x1180,     0x11BF,        0xE4,   GK104,  0x604,  Kepler  )
DEFINE_OBS_GPU(     0x11C0,     0x11FF,        0xE6,   GK106,  0x606,  Kepler  )
DEFINE_OBS_GPU(     0x0FC0,     0x0FFF,        0xE7,   GK107,  0x607,  Kepler  )
DEFINE_OBS_GPU(     0x1000,     0x103F,        0xF0,   GK110,  0x610,  Kepler  )
DEFINE_OBS_GPU(     0x1000,     0x103F,        0xF1,   GK110B, 0x611,  Kepler  )
DEFINE_OBS_GPU(     0x1000,     0x103F,        0xF2,   GK110C, 0x612,  Kepler  )
DEFINE_OBS_GPU(     0x1300,     0x133F,        0xED,   GK207,  0x618,  Kepler  )
DEFINE_OBS_GPU(     0x14C0,     0x14FF,        0xED,   GK207,  0x618,  Kepler  )
DEFINE_OBS_GPU(     0x1280,     0x12BF,       0x108,   GK208,  0x628,  Kepler  )
DEFINE_OBS_GPU(     0x1280,     0x12BF,       0x106,   GK208S, 0x629,  Kepler  )

#if !defined(REGHAL_TEGRA)
//                             DIDStart    DIDEnd       ChipId    LwId   Constant Family   SubdeviceType       FrameBufferType          IsmType      FuseType     FsType    GCxType  HBMType    HwrefDir       DispDir LwLinkType         XusbHostCtrl,   PortPolicyCtrl, LwLinkThroughputCountersType  PcieType     I2cType  GpioType  
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Maxwell GPUs
DEFINE_GPU(ARCH_MAXWELL,       0x1340,     0x137F,       0x118,   GM108,  0x708,  Maxwell, GM108GpuSubdevice,  CreateGM10XFrameBuffer,  GMxxxGpuIsm, MaxwellFuse, GM10xFs,  GCx,     NullHBM,   maxwell/gm108, None,   NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               MaxwellPcie, GpuI2c,   GpuGpio   )  // GM108
DEFINE_GPU(ARCH_MAXWELL,       0x1380,     0x13BF,       0x117,   GM107,  0x707,  Maxwell, GM107GpuSubdevice,  CreateGM10XFrameBuffer,  GMxxxGpuIsm, MaxwellFuse, GM10xFs,  GCx,     NullHBM,   maxwell/gm107, v02_04, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               MaxwellPcie, GpuI2c,   GpuGpio   )  // GM107
DEFINE_GPU(ARCH_MAXWELL,       0x17c0,     0x17FF,       0x120,   GM200,  0x720,  Maxwell, GM200GpuSubdevice,  CreateGM20XFrameBuffer,  GM2xxGpuIsm, MaxwellFuse, GM20xFs,  GCx,     NullHBM,   maxwell/gm200, v02_05, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               MaxwellPcie, GpuI2c,   GpuGpio   )  // GM200
DEFINE_GPU(ARCH_MAXWELL,       0x13c0,     0x13FF,       0x124,   GM204,  0x724,  Maxwell, GM204GpuSubdevice,  CreateGM20XFrameBuffer,  GM2xxGpuIsm, MaxwellFuse, GM20xFs,  GCx,     NullHBM,   maxwell/gm204, v02_05, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               MaxwellPcie, GpuI2c,   GpuGpio   )  // GM204
DEFINE_GPU(ARCH_MAXWELL,       0x1400,     0x143F,       0x126,   GM206,  0x726,  Maxwell, GM206GpuSubdevice,  CreateGM20XFrameBuffer,  GM2xxGpuIsm, MaxwellFuse, GM20xFs,  GCx,     NullHBM,   maxwell/gm206, v02_06, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               MaxwellPcie, GpuI2c,   GpuGpio   )  // GM206

// Pascal GPUs
DEFINE_GPU(ARCH_PASCAL,        0x15C0,     0x15FF,       0x130,   GP100,  0x800,  Pascal,  GP100GpuSubdevice,  CreateGP100FrameBuffer,  GPxxxGpuIsm, PascalFuse,  GP100Fs,  NullGCx, PascalHBM, pascal/gp100,  v02_07, PascalLwLink,      NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV1,   PascalPcie,  GpuI2c,   GpuGpio   ) // GP100
DEFINE_GPU(ARCH_PASCAL,        0x1B00,     0x1B3F,       0x132,   GP102,  0x802,  Pascal,  GP102GpuSubdevice,  CreateGPLIT3FrameBuffer, GPxxxGpuIsm, PascalFuse,  GP104Fs,  GCx,     NullHBM,   pascal/gp102,  v02_08, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               PascalPcie,  GpuI2c,   GpuGpio   ) // GP102
DEFINE_GPU(ARCH_PASCAL,        0x1B80,     0x1BBF,       0x134,   GP104,  0x804,  Pascal,  GP104GpuSubdevice,  CreateGPLIT3FrameBuffer, GPxxxGpuIsm, PascalFuse,  GP104Fs,  GCx,     NullHBM,   pascal/gp104,  v02_08, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               PascalPcie,  GpuI2c,   GpuGpio   ) // GP104
DEFINE_GPU(ARCH_PASCAL,        0x1B80,     0x1BBF,       0x135,   GP105,  0x805,  Pascal,  GP104GpuSubdevice,  CreateGPLIT3FrameBuffer, GPxxxGpuIsm, PascalFuse,  GP104Fs,  GCx,     NullHBM,   pascal/gp104,  v02_08, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               PascalPcie,  GpuI2c,   GpuGpio   ) // GP105
DEFINE_GPU(ARCH_PASCAL,        0x1C00,     0x1C3F,       0x136,   GP106,  0x806,  Pascal,  GP106GpuSubdevice,  CreateGPLIT3FrameBuffer, GPxxxGpuIsm, PascalFuse,  GP107Fs,  GCx,     NullHBM,   pascal/gp106,  v02_08, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               PascalPcie,  GpuI2c,   GpuGpio   ) // GP106
DEFINE_GPU(ARCH_PASCAL,        0x1C80,     0x1CBF,       0x137,   GP107,  0x807,  Pascal,  GP107GpuSubdevice,  CreateGPLIT4FrameBuffer, GPxxxGpuIsm, PascalFuse,  GP107Fs,  GCx,     NullHBM,   pascal/gp107,  v02_09, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               PascalPcie,  GpuI2c,   GpuGpio   ) // GP107
DEFINE_GPU(ARCH_PASCAL,        0x1D00,     0x1D3F,       0x138,   GP108,  0x808,  Pascal,  GP108GpuSubdevice,  CreateGPLIT4FrameBuffer, GPxxxGpuIsm, PascalFuse,  GP107Fs,  GCx,     NullHBM,   pascal/gp108,  v02_09, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               PascalPcie,  GpuI2c,   GpuGpio   ) // GP108

// Volta GPUs
DEFINE_GPU(ARCH_VOLTA,         0x1D80,     0x1DBF,       0x140,   GV100,  0x900,  Volta,   GV100GpuSubdevice,  CreateGVLIT1FrameBuffer, GVxxxGpuIsm, VoltaFuse,   GV10xFs,  NullGCx, VoltaHBM,  volta/gv100,   v03_00, VoltaLwLink,       NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV2,   VoltaPcie,   GpuI2c,   GpuGpio   ) // GV100
DEFINE_GPU(ARCH_VOLTA,         0x1D80,     0x1DBF,       0x145,   GV105,  0x905,  Volta,   GV100GpuSubdevice,  CreateGVLIT1FrameBuffer, GVxxxGpuIsm, GpuFuse,     GV10xFs,  NullGCx, VoltaHBM,  volta/gv100,   v03_00, VoltaLwLink,       NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               VoltaPcie,   GpuI2c,   GpuGpio   ) // GV105

// Turing GPUs
DEFINE_GPU(GPU_IMPL_TU102,     0x1E00,     0x1E3F,       0x162,   TU102,  0xA02,  Turing,  TU102GpuSubdevice,  CreateTULIT2FrameBuffer, TUxxxGpuIsm, TuringFuse,  TU10xFs,  GCx,     NullHBM,   turing/tu102,  v04_00, TuringLwLink,      TuringXusb,     TuringPPC,      LwLinkThroughputCountersV2,   TuringPcie,  GpuI2c,   GpuGpio   ) // TU102
DEFINE_GPU(GPU_IMPL_TU104,     0x1E80,     0x1EBF,       0x164,   TU104,  0xA04,  Turing,  TU104GpuSubdevice,  CreateTULIT2FrameBuffer, TUxxxGpuIsm, TuringFuse,  TU10xFs,  GCx,     NullHBM,   turing/tu104,  v04_00, TuringLwLink,      TuringXusb,     TuringPPC,      LwLinkThroughputCountersV2,   TuringPcie,  GpuI2c,   GpuGpio   ) // TU104
DEFINE_GPU(GPU_IMPL_TU106,     0x1F00,     0x1F3F,       0x166,   TU106,  0xA06,  Turing,  TU106GpuSubdevice,  CreateTULIT2FrameBuffer, TUxxxGpuIsm, TuringFuse,  TU10xFs,  GCx,     NullHBM,   turing/tu106,  v04_00, TuringLwLink,      TuringXusb,     TuringPPC,      LwLinkThroughputCountersV2,   TuringPcie,  GpuI2c,   GpuGpio   ) // TU106
DEFINE_GPU(GPU_IMPL_TU116,     0x2180,     0x21BF,       0x168,   TU116,  0xA08,  Turing,  TU116GpuSubdevice,  CreateTULIT2FrameBuffer, TUxxxGpuIsm, TuringFuse,  TU10xFs,  GCx,     NullHBM,   turing/tu116,  v04_00, TuringLwLink,      TuringXusb,     TuringPPC,      LwLinkThroughputCountersV2,   TuringPcie,  GpuI2c,   GpuGpio   ) // TU116
DEFINE_GPU(GPU_IMPL_TU117,     0x1F80,     0x1FBF,       0x167,   TU117,  0xA07,  Turing,  TU117GpuSubdevice,  CreateTULIT2FrameBuffer, TUxxxGpuIsm, TuringFuse,  TU10xFs,  GCx,     NullHBM,   turing/tu117,  v04_00, TuringLwLink,      NULL_INTERFACE, TuringPPC,      LwLinkThroughputCountersV2,   TuringPcie,  GpuI2c,   GpuGpio   ) // TU117

// Ampere GPUs
DEFINE_GPU(GPU_IMPL_GA100,     0x2080,     0x20BF,       0x170,   GA100,  0xC00,  Ampere,  GA100GpuSubdevice,  CreateGALIT1FrameBuffer, GAxxxGpuIsm, AmpereFuse,  AmpereFs, NullGCx, VoltaHBM,  ampere/ga100,  None,   AmpereLwLink,      NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV3,   AmperePcie,  GpuI2c,   GpuGpio   ) // GA100
DEFINE_GPU(GPU_IMPL_GA102,     0x2200,     0x223F,       0x172,   GA102,  0xC02,  Ampere,  GA102GpuSubdevice,  CreateGALIT2FrameBuffer, GA10xGpuIsm, GA10xFuse,   GA10xFs,  GCx,     NullHBM,   ampere/ga102,  v04_01, GA10xLwLink,       NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV3,   GA10xPcie,   GpuI2c,   GpuGpio   ) // GA102
DEFINE_GPU(GPU_IMPL_GA102F,    0x2294,     0x2294,       0x17F,   GA102F, 0xC0F,  Ampere,  GA102GpuSubdevice,  CreateGALIT2FrameBuffer, GA10xGpuIsm, GA10xFuse,   GA10xFs,  NullGCx, NullHBM,   ampere/ga102,  v04_01, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               GA10xPcie,   GpuI2c,   GpuGpio   ) // GA102F
DEFINE_GPU(GPU_IMPL_GA103,     0x2400,     0x243F,       0x173,   GA103,  0xC03,  Ampere,  GA103GpuSubdevice,  CreateGALIT2FrameBuffer, GA10xGpuIsm, GA10xFuse,   GA10xFs,  GCx,     NullHBM,   ampere/ga103,  v04_01, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               GA10xPcie,   GpuI2c,   GpuGpio   ) // GA103
DEFINE_GPU(GPU_IMPL_GA104,     0x2480,     0x24BF,       0x174,   GA104,  0xC04,  Ampere,  GA104GpuSubdevice,  CreateGALIT2FrameBuffer, GA10xGpuIsm, GA10xFuse,   GA10xFs,  GCx,     NullHBM,   ampere/ga104,  v04_01, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               GA10xPcie,   GpuI2c,   GpuGpio   ) // GA104
DEFINE_GPU(GPU_IMPL_GA106,     0x2500,     0x253F,       0x176,   GA106,  0xC06,  Ampere,  GA106GpuSubdevice,  CreateGALIT2FrameBuffer, GA10xGpuIsm, GA10xFuse,   GA10xFs,  GCx,     NullHBM,   ampere/ga106,  v04_01, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               GA10xPcie,   GpuI2c,   GpuGpio   ) // GA106
DEFINE_GPU(GPU_IMPL_GA107,     0x2580,     0x25BF,       0x177,   GA107,  0xC07,  Ampere,  GA107GpuSubdevice,  CreateGALIT2FrameBuffer, GA10xGpuIsm, GA10xFuse,   GA10xFs,  GCx,     NullHBM,   ampere/ga107,  v04_01, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               GA10xPcie,   GpuI2c,   GpuGpio   ) // GA107

// Ada GPUs
DEFINE_GPU(GPU_IMPL_AD102,     0x2680,     0x26BF,       0x192,   AD102,  0xD02,  Ada,     AD102GpuSubdevice,  CreateGALIT2FrameBuffer, AD10xGpuIsm, GA10xFuse,   AD10xFs,  GCx,     NullHBM,   ada/ad102,     v04_04, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV3,   GA10xPcie,   GpuI2c,   GpuGpio   ) // AD102
DEFINE_GPU(GPU_IMPL_AD103,     0x2700,     0x273F,       0x193,   AD103,  0xD03,  Ada,     AD102GpuSubdevice,  CreateGALIT2FrameBuffer, AD10xGpuIsm, GA10xFuse,   AD10xFs,  GCx,     NullHBM,   ada/ad103,     v04_04, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV3,   GA10xPcie,   GpuI2c,   GpuGpio   ) // AD103
DEFINE_GPU(GPU_IMPL_AD104,     0x2780,     0x27BF,       0x194,   AD104,  0xD04,  Ada,     AD102GpuSubdevice,  CreateGALIT2FrameBuffer, AD10xGpuIsm, GA10xFuse,   AD10xFs,  GCx,     NullHBM,   ada/ad104,     v04_04, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV3,   GA10xPcie,   GpuI2c,   GpuGpio   ) // AD104
DEFINE_GPU(GPU_IMPL_AD106,     0x2800,     0x283F,       0x196,   AD106,  0xD06,  Ada,     AD102GpuSubdevice,  CreateGALIT2FrameBuffer, AD10xGpuIsm, GA10xFuse,   AD10xFs,  GCx,     NullHBM,   ada/ad106,     v04_04, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV3,   GA10xPcie,   GpuI2c,   GpuGpio   ) // AD106
DEFINE_GPU(GPU_IMPL_AD107,     0x2880,     0x28BF,       0x197,   AD107,  0xD07,  Ada,     AD102GpuSubdevice,  CreateGALIT2FrameBuffer, AD10xGpuIsm, GA10xFuse,   AD10xFs,  GCx,     NullHBM,   ada/ad107,     v04_04, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV3,   GA10xPcie,   GpuI2c,   GpuGpio   ) // AD107
DEFINE_GPU(GPU_IMPL_AD102F,    0x2294,     0x2294,       0x19F,   AD102F, 0xD0F,  Ada,     AD102GpuSubdevice,  CreateGALIT2FrameBuffer, AD10xGpuIsm, GA10xFuse,   AD10xFs,  NullGCx, NullHBM,   ada/ad102,     v04_04, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               GA10xPcie,   GpuI2c,   GpuGpio   ) // AD102F

// Hopper GPUs
DEFINE_GPU(GPU_IMPL_GH100,     0x2300,     0x233F,       0x180,   GH100,  0xE00,  Hopper,  GH100GpuSubdevice,  CreateGHLIT1FrameBuffer, GHxxxGpuIsm, HopperFuse,  HopperFs, NullGCx, NullHBM,   hopper/gh100,  v04_04, HopperLwLink,      NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV4,   HopperPcie,  GpuI2c,   GpuGpio,  HopperC2C   ) // GH100
DEFINE_GPU(GPU_IMPL_GH202,     0x2600,     0x263F,       0x182,   GH202,  0xE02,  Hopper,  GH202GpuSubdevice,  CreateGHLIT1FrameBuffer, GHxxxGpuIsm, HopperFuse,  HopperFs, GCx,     NullHBM,   hopper/gh202,  v04_04, HopperLwLink,      NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV4,   HopperPcie,  GpuI2c,   GpuGpio,  HopperC2C   ) // GH202

// Blackwell GPUs
DEFINE_GPU(GPU_IMPL_GB100,     0x2900,     0x293F,       0x1A0,   GB100,  0xF00,  Blackwell,  GB100GpuSubdevice,  CreateGHLIT1FrameBuffer, GBxxxGpuIsm, HopperFuse,  HopperFs, NullGCx, NullHBM,   blackwell/gb100,  v04_04, HopperLwLink,      NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV4,   HopperPcie,  GpuI2c,   GpuGpio,  HopperC2C   ) // GB100
DEFINE_GPU(GPU_IMPL_GB102,     0x2980,     0x29BF,       0x1A2,   GB102,  0xF02,  Blackwell,  GB100GpuSubdevice,  CreateGHLIT1FrameBuffer, GBxxxGpuIsm, HopperFuse,  HopperFs, NullGCx, NullHBM,   blackwell/gb102,  v04_04, HopperLwLink,      NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV4,   HopperPcie,  GpuI2c,   GpuGpio,  HopperC2C   ) // GB102
#endif

// Superchip
#if defined(REGHAL_SIM) || !defined(MATS_STANDALONE)
DEFINE_GPU(GPU_IMPL_G000,      0x0FB0,     0x0FB0,       0x1E0,   G000,   0xFFE0, G00X,    GB100GpuSubdevice,  CreateGHLIT1FrameBuffer, NullIsm,     AmpereFuse,  HopperFs, NullGCx, NullHBM,   g00x/g000,     v03_00, HopperLwLink,      NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV4,   HopperPcie,  GpuI2c,   GpuGpio,  HopperC2C   ) // G000 (prototype)
#endif

// Dummy GPUs
#if defined(REGHAL_SIM) || !defined(MATS_STANDALONE)
DEFINE_SIM_GPU(                0x0FA1,     0x0FA1,        0x00,   SOC,    0xFFF1, None,    DummyGpuSubdevice,  0,                       NullIsm,     NullFuse,    NullFs,   NullGCx, NullHBM,   None,          None,   NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               SocPcie,     GpuI2c,   GpuGpio   ) // SOC
DEFINE_GPU(GPU_IMPL_G000,      0x0000,     0x001F,       0x1F0,   AMODEL, 0xFFF0, None,    AmodGpuSubdevice,   CreateAmodFrameBuffer,   NullIsm,     NullFuse,    NullFs,   NullGCx, NullHBM,   kepler/gk104,  None,   NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               KeplerPcie,  GpuI2c,   GpuGpio   ) // AMODEL, ID constant must be max
#endif

// Simulated standalone CheetAh GPUs
#if defined(REGHAL_SIM) || !defined(MATS_STANDALONE)
DEFINE_OBS_GPU(                0x0000,     0x0000,        0xEA,   GK20A,  0x62A,  Kepler                                                                                                                                                                                                                                                 ) // GK20A (prototype)
DEFINE_OBS_GPU(                0x0000,     0x0000,       0x12B,   GM20B,  0x72B,  Maxwell                                                                                                                                                                                                                                                ) // GM20B (prototype)
DEFINE_OBS_GPU(                0x0000,     0x0000,       0x12E,   GM21B,  0x72E,  Maxwell                                                                                                                                                                                                                                                ) // GM20B (prototype)
DEFINE_OBS_GPU(                0x0000,     0x0000,       0x12D,   GM20D,  0x72D,  Maxwell                                                                                                                                                                                                                                                ) // GM20D (prototype)
DEFINE_OBS_GPU(                0x0000,     0x0000,       0x13B,   GP10B,  0x80B,  Pascal                                                                                                                                                                                                                                                 ) // GP10B (prototype)
DEFINE_GPU(CHIP_T194,          0x0000,     0x0000,       0x15B,   GV11B,  0x91B,  Volta,   GV11BGpuSubdevice,  CreateTegraFrameBuffer,  NullIsm,     NullFuse,    GV11BFs,  NullGCx, NullHBM,   volta/gv11b,   None,   NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               PascalPcie,  GpuI2c,   GpuGpio   ) // GV11B (prototype)
DEFINE_GPU(CHIP_T234,          0x0000,     0x0000,       0x17B,   GA10B,  0xC0B,  Ampere,  GA10BGpuSubdevice,  CreateTegraFrameBuffer,  NullIsm,     NullFuse,    GA10BFs,  NullGCx, NullHBM,   ampere/ga10b,  None,   NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               AmperePcie,  GpuI2c,   GpuGpio   ) // GA10B (prototype)
DEFINE_GPU(CHIP_T234,          0x0000,     0x0000,       0x17E,   GA10F,  0xC0E,  Ampere,  GA10FGpuSubdevice,  CreateTegraFrameBuffer,  NullIsm,     NullFuse,    GA10BFs,  NullGCx, NullHBM,   ampere/ga10f,  None,   NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               AmperePcie,  GpuI2c,   GpuGpio   ) // GA10F (prototype)
DEFINE_GPU(CHIP_T234,          0x0000,     0x0000,       0x17C,   GA11B,  0xC0A,  Ampere,  GA11BGpuSubdevice,  CreateTegraFrameBuffer,  NullIsm,     NullFuse,    GA10BFs,  NullGCx, NullHBM,   ampere/ga11b,  None,   NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               AmperePcie,  GpuI2c,   GpuGpio   ) // GA11B (prototype)
DEFINE_GPU(CHIP_T256,          0x0000,     0x0000,       0x19B,   AD10B,  0xD0B,  Ada,     AD10BGpuSubdevice,  CreateTegraFrameBuffer,  NullIsm,     NullFuse,    GA10BFs,  NullGCx, NullHBM,   ada/ad10b,     None,   NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               AD10BPcie,  GpuI2c,   GpuGpio   ) // AD10B (prototype)
DEFINE_GPU(CHIP_T256,          0x0000,     0x0000,       0x1D0,   IG000,  0xFFF2, IG00X,   AD10BGpuSubdevice,  CreateTegraFrameBuffer,  NullIsm,     NullFuse,    GA10BFs,  NullGCx, NullHBM,   ig00x/ig000,   None,   NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               AD10BPcie,  GpuI2c,   GpuGpio   ) // IG000 (prototype)
#endif

// CheetAh GPUs
#if defined(REGHAL_TEGRA) || defined(REGHAL_SIM) || !defined(MATS_STANDALONE)
DEFINE_GPU(CHIP_T194,      0xE0000000, 0xE0000000,  0xE0000000,   Txxx,   0x1FF,  None,    NoGpuSubdevice,     CreateTegraFrameBuffer,  NullIsm,     NullFuse,    NullFs,   NullGCx, NullHBM,   None,          None,   NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               SocPcie                          ) // CheetAh without GPU
DEFINE_OBS_GPU(            0xE0000030, 0xE0000030,  0xE0000030,   T30,    0x100,  None                                                                                                                                                                                                                                                   ) // T30
DEFINE_OBS_GPU(            0xE0000035, 0xE0000035,  0xE0000035,   T114,   0x114,  None                                                                                                                                                                                                                                                   ) // T114
DEFINE_OBS_GPU(            0xE0000014, 0xE0000014,  0xE0000014,   T148,   0x148,  None                                                                                                                                                                                                                                                   ) // T148
DEFINE_OBS_GPU(            0xE0000040, 0xE0000040,  0xE0000040,   T124,   0x124,  Kepler                                                                                                                                                                                                                                                 ) // T124 (GK20A SOC)
DEFINE_OBS_GPU(            0xE0000013, 0xE0000013,  0xE0000013,   T132,   0x132,  Kepler                                                                                                                                                                                                                                                 ) // T132 (GK20A SOC)
DEFINE_OBS_GPU(            0xE0000021, 0xE0000021,  0xE0000021,   T210,   0x210,  Maxwell                                                                                                                                                                                                                                                ) // T210 (GM20B SOC)
DEFINE_OBS_GPU(            0xE0000018, 0xE0000018,  0xE0000018,   T186,   0x186,  Pascal                                                                                                                                                                                                                                                 ) // T186 (GP10B SOC)
DEFINE_GPU(CHIP_T194,      0xE0000019, 0xE0000019,  0xE0000019,   T194,   0x194,  Volta,   T194GpuSubdevice,   CreateTegraFrameBuffer,  T194Ism,     NullFuse,    GV11BFs,  NullGCx, NullHBM,   t19x/t194,     None,   XavierTegraLwLink, NULL_INTERFACE, NULL_INTERFACE, LwLinkThroughputCountersV2SW, SocPcie,     GpuI2c,    GpuGpio  ) // T194 (GV11B SOC)
DEFINE_GPU(CHIP_T234,      0xE0000023, 0xE0000023,  0xE0000023,   T234,   0x234,  Ampere,  T234GpuSubdevice,   CreateTegraFrameBuffer,  T234Ism,     NullFuse,    GA10BFs,  NullGCx, NullHBM,   t23x/t234,     v04_02, NULL_INTERFACE,    NULL_INTERFACE, NULL_INTERFACE, NULL_INTERFACE,               SocPcie,     GpuI2c,    GpuGpio  ) // T234 (GA10B SOC)
#endif

#undef DEFINE_MODS_GPU
#undef DEFINE_SIM_GPU
#undef DEFINE_DIS_GPU
#undef DEFINE_GPU_0
#undef DEFINE_GPU_1
#undef DEFINE_GPU
#undef DEFINE_GPU_INTERNAL
#undef DEFINE_GPU_INTERNAL_VALUE
#undef NULL_INTERFACE
#undef EXPAND
#undef XavierTegraLwLink
