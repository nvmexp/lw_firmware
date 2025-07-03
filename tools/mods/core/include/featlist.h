/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2014,2016-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#if !defined(DEFINE_GPUSUB_FEATURE) || \
    !defined(DEFINE_GPUDEV_FEATURE) || \
    !defined(DEFINE_MCP_FEATURE) || \
    !defined(DEFINE_SOC_FEATURE)
#error featlist.h should only be #included in js_gpusb.cpp, featstr.cpp, device.cpp, device.h and utlGpu.cpp
#endif

DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_C24_SCANOUT,                    2,
                      "C24 Scanout" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_BLOCK_LINEAR,                   3,
                      "Block Linear" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_ISO_DISPLAY,                    9,
                      "ISO display" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_COMPRESSION_TAGS,              10,
                      "Compression tags" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_COMPRESSION,                   11,
                      "Compression" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_LW2080_CTRL_I2C,               13,
                      "LW2080_CTRL_I2C" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_GPFIFO_CHANNEL,                14,
                      "GpFifo channels" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_PDISP_RG_RASTER_EXTEND,        15,
                      "PDISP_RG_RASTER_EXTEND" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_QUICK_SUSPEND_RESUME,          16,
                      "Quick suspend/resume" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_I2C,                           20,
                      "I2C" )
DEFINE_GPUSUB_FEATURE(GPUSUB_ALLOW_UNDETECTED_DISPLAYS,         22,
                      "Allows undetected displays" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_PAGING,                   28,
                      "Paging" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_TILING,                   30,
                      "Tiling" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_CLAMP_GT_32BPT_TEXTURES,  31,
                      "Clamping greater than 32bpt textures" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_UNIFIED_GART_FLAGS,            81,
                      "Unified GART flags" )
DEFINE_GPUSUB_FEATURE(GPUSUB_STRAPS_SUB_VENDOR,                 85,
                      "sub vendor strap" )
DEFINE_GPUSUB_FEATURE(GPUSUB_STRAPS_SLOT_CLK_CFG,               86,
                      "PCIE refclk of platform" )
DEFINE_GPUSUB_FEATURE(GPUSUB_STRAPS_PEX_PLL_EN_TERM100,         87,
                      "Int refclk's 100 ohms termination" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_16BPP_Z,                       89,
                      "3D engine supports 16-bpp depth buffers" )
DEFINE_GPUSUB_FEATURE(GPUSUB_CAN_CHECK_FB_CALIBRATION,          90,
                      "ability to check for FB calibration errors" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_GPU_SEGMENTATION,         91,
                      "Segmented Memory Allocation on the GPU" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_RM_HYBRID_P2P,            93,
                      "Supports RM Hybrid P2P mappings" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_FRAMEBUFFER_IN_SYSMEM,         96,
                      "Framebuffer is in system memory" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_TEXTURE_COALESCER,             97,
                      "Has a texture coalescer" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_ELPG,                          98,
                      "Has engine level power gating" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_MODS_CONSOLE,             102,
                      "Supports a mods console" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_AZALIA_CONTROLLER,             103,
                      "has a azalia controller" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_KFUSES,                        104,
                      "has KFUSES" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_CLOCK_THROTTLE,                105,
                      "has clock pwm/pulsing hw" )
DEFINE_GPUSUB_FEATURE(GPUSUB_CHIP_INTEGRATED,                   106,
                      "GPU is integrated into chipset" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_MME,                           107,
                      "has method macro expander" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_BAR1_REMAPPER,                 108,
                      "has BAR1 Remapper" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_ASPM_OPTION_IN_PEXTESTS,  109,
                      "GPU supports ASPM option in PEX tests" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_TESSELLATION,             111,
                      "GPU supports tessellation shaders" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_ECC,                      112,
                      "GPU supports ECC" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_PLL_QUERIES,              113,
                      "GPU supports locking PLL queries" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_BOARDS_JS_ENTRIES,             114,
                      "GPU has boards.js entries" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_EDC,                      115,
                      "GPU supports EDC" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_HYBRID_PADS,              116,
                      "GPU supports Hybrid Pads" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_ONBOARD_PWR_SENSORS,           120,
                      "GPU support onboard power sensors like INA29, ChiL, Volterra" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_DUAL_HEAD_MIDFRAME_SWITCH,  121,
                      "GPU support dual head mid frame glitchless mclk switch" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_COHERENT_SYSMEM,               122,
                      "GPU supports coherent system memory" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_SYSMEM,                        123,
                      "GPU supports system memory" )
DEFINE_GPUSUB_FEATURE(GPUSUB_CAN_HAVE_PSTATE20,                 124,
                      "GPU has PState 2.0 support from RM" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_GEN3_PCIE,                125,
                      "GPU supports Gen3 PCIE" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_PRIV_SELWRITY ,                126,
                      "GPU has Priv Security support from RM" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_FB_HEAP,                       127,
                      "GPU has framebuffer memory heap, either native or in system memory carveout " )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_VIDEO_PROTECTED_REGION,        128,
                      "GPU has a video protected memory region" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_32BPP_Z,                       129,
                      "3D engine supports 32-bpp depth buffers" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_SEC_DEBUG,                     130,
                      "SEC engine has a debug mode" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_24BPP_Z,                       131,
                      "3D engine supports 24-bpp depth buffers" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_SPLIT_SOR,                132,
                      "Display hardware supports split SOR feature" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_FSAA,                     133,
                      "GL supports FSAA on this gpu" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_FSAA8X,                   134,
                      "GL supports 8X FSAA on this gpu" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_UNCOMPRESSED_RAWIMAGE,    135,
                      "Mods supports uncompressed raw image on this gpu" )
DEFINE_GPUSUB_FEATURE(GPUSUB_STRAPS_FAN_DEBUG_PWM_66,           136,
                      "Fan Debug PWM 66" )
DEFINE_GPUSUB_FEATURE(GPUSUB_STRAPS_FAN_DEBUG_PWM_33,           137,
                      "Fan Debug PWM 33" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_SOR_XBAR,                 138,
                      "Display hardware supports SOR XBAR feature" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_RMCTRL_I2C_DEVINFO,            139,
                      "GPU supports LW402C_CTRL_CMD_I2C_TABLE_GET_DEV_INFO" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_ACR_REGION,               140,
                      "GPU supports ACR region" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_LWPR,                     141,
                      "GL supports Path Rendering on this gpu" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_MULTIPLE_LAYERS_AND_VPORT,142,
                      "GL supports rendering to multiple layers and viewports" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_HDMI2,                    143,
                      "Display hardware supports HDMI2.0 and above" )
//2d rendering features supported on gpus GM20x and later
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_S8_FORMAT,                144,
                      "GL supports Stencil compression and S8 format" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_16xMsAA,                  145,
                      "GL supports 16x raster samples" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_TIR,                      146,
                      "GL supports Target Independent Rasterization" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_FILLRECT,                 147,
                      "GL supports fill rectangle extension" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_PER_VASPACE_BIG_PAGE,     148,
                      "GPU supports per vaspace big page size" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_SINGLE_HEAD_DUAL_SST,     149,
                      "Display hardware supports single head dual SST" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_SINGLE_HEAD_DUAL_MST,     150,
                      "Display hardware supports single head dual MST" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_PEX_ASPM_L1SS,                 151,
                      "PEX interface has ASPM L1 substates" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_SELWRE_FB_SCRUBBER,            152,
                      "GPU has secure HW scrubber for FB" )
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_NONCOHERENT_SYSMEM,            153,
                      "CPU supports noncoherent system memory" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_BOARDS_DB_V2,             155,
                      "GPU has boards.db version 2 entries" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_POR_BIN_CHECK,            156,
                      "GPU supports checking POR Bin values")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_MIO_LOOPBACK,             157,
                      "GPU supports MIO Loopback")
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_SUBCONTEXTS,                   158,
                      "GPU has subcontexts")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_GC5,                      159,
                      "GPU supports GC5 feature")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_FS_CHECK,                 160,
                      "GPU supports checking floor-swept values")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_YUV_CROPPING,             161,
                      "Display hardware supports YUV cropping")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_VBIOS_HASH,               162,
                      "RM supports retrieving VBIOS hash")
DEFINE_GPUSUB_FEATURE(GPUSUB_REQUIRES_FB_USERD,                 163,
                      "GPU requires USERD be in FB Memory")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_POST_L2_COMPRESSION,      164,
                      "GPU supports post L2 compression")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_ECC_PRIV_INJECTION,       165,
                      "GPU supports ECC error priv-injection")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_PWR_REG_CHECK,            166,
                      "GPU supports checking the power regulators")
DEFINE_GPUSUB_FEATURE(GPUSUB_IS_STANDALONE_TEGRA_GPU,           167,
                      "GPU is a test CheetAh iGPU which looks like dGPU")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_SYSMEM_REFLECTED_MAPPING,        168,
                      "GPU supports sysmem reflected mapping")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_VBIOS_GUID,               169,
                      "RM supports retrieving VBIOS GUID")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_CPU_FB_MAPPINGS_THROUGH_LWLINK, 170,
                      "GPU supports FB mappings on the CPU through LWLINK")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_GENERIC_MEMORY,           171,
                      "GPU supports generic memory ptekind")
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_MME64,                         172,
                      "GPU has Method Macro Expander 64")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_GSYNC_R3,                 173,
                      "GPU supports GSYNC R3 connection")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_RM_GENERIC_ECC,           174,
                      "GPU supports generic RM ECC interfaces")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_HECC_FUSING,              175,
                      "GPU supports Hamming ECC fusing")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_USB,                      176,
                      "GPU supports a USB host controller")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_RTD3,                     177,
                      "GPU supports RTD3 power management")
DEFINE_GPUSUB_FEATURE(GPUSUB_HOST_REQUIRES_ENGINE_ID,           179,
                      "GPU host allocations (channel or TSG) requires engine ID during allocation ")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_HDR_DISPLAY,              180,
                      "GPU supports Display with HDR")
DEFINE_GPUSUB_FEATURE(GPUSUB_LWLINK_LP_SLM_MODE_POR,            181,
                      "Whether lwlink low power single lane mode is POR for a GPU")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_RUNTIME_L2_MODE_CHANGE,   182,
                      "Whether L2 bypass is enabled with a regkey")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_LCE_PCE_ONE_TO_ONE,       183,
                      "Whether the GPU can support a one to one LCE to PCE mapping")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_FPF_WITH_FUB,             184,
                      "GPU supports field programmable fusing through MODS using FUB")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_LEGACY_GC6,               185,
                      "GPU supports legacy GC6 feature")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_TPC_PG,                   187,
                      "GPU supports TPC power gating")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_FUSING_WO_DAUGHTER_CARD,  188,
                      "GPU doesn't use daughter card for secure HW fusing")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_IFF_SW_FUSING,            189,
                      "GPU supports IFF SW fusing")
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_MERGED_FUSE_MACROS,            190,
                      "GPU supports merging of fuse and fpf blocks")
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_IST,                           191,
                      "GPU has In-Silicon-Test capabilities")
DEFINE_GPUSUB_FEATURE(GPUSUB_GPC_REENUMERATION,                 192,
                      "GPU logical GPCs are ordered from least to greatest number of TPCs")
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_GPU_RESET,                     193,
                      "GPU has a functional LW_XVE_RESET_CTRL_GPU_RESET")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_PAGE_BLACKLISTING,        194,
                      "GPU supports page blacklisting")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_ROW_REMAPPING,            195,
                      "GPU supports row remapping")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_LCE_PCE_REMAPPING,        196,
                      "Whether the GPU can support changing the LCE to PCE mapping at runtime")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_PSTATES_IN_PMU,           197,
                      "GPU supports running pstates through the PMU SW stack")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_LWLINK_OFF_TRANSITION,    198,
                      "GPU supports transitioning to LwLink OFF without driver reload")
DEFINE_GPUSUB_FEATURE(GPUSUB_TSEC_SUPPORTS_GFE_APPLICATION,     199,
                      "GPU TSEC supports the GFE application")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_LWLINK_HS_SAFE_CYCLING,   200,
                      "GPU supports transitioning to LwLink from HS to SAFE to HS")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_CRC_FUSE_CHECK,           201,
                      "GPU supports CRC status check")
DEFINE_GPUSUB_FEATURE(GPUSUB_ALLOWS_POISON_RIR,                 202,
                      "GPU allows poison disable fusing")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_VULKAN,                   203,
                      "GPU supports running tests that use the Vulkan API")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_HDMI21_FRL,               204,
                      "Display hardware supports HDMI 2.1 FRL")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_IOBIST,                   205,
                      "Use IOBIST to program SORs")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_GFW_HOLDOFF,              206,
                      "GPU supports GFW initialization holdoff sequence")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_SCPM_AND_POD,             207,
                      "GPU supports SCPM and POD")
DEFINE_GPUSUB_FEATURE(GPUSUB_DEPRECATED_RAW_SWIZZLE,            208,
                      "GPU FBHUB deprecates raw to gob within pitch swizzle")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_COMPUTE_ONLY_GPC,         209,
                      "GPU supports Compute-only GPC[s]")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_LWDAVIDEOFORMAT_444,      210,
                      "GPU supports Lwca video format yuv 4:4:4")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_LWDAVIDEOFORMAT_AV1,      211,
                      "GPU supports Lwca video format Av1")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_DRAM_TEMP,                212,
                      "GPU supports per-chip DRAM temperature")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_AZALIA_LOOPBACK,          213,
                      "Azalia loopback is supported" )
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_KAPPA_BINNING,            214,
                      "GPU supports SLT kappa binning (aka SLT Virtual Binning, SVB)")
DEFINE_GPUSUB_FEATURE(GPUSUB_HAS_SMC_STATUS_BIT,                215,
                      "GPU supports SMC and indicates SMC mode on register")
DEFINE_GPUSUB_FEATURE(GPUSUB_LWLINK_L1_MODE_POR,                216,
                      "Whether lwlink L1 mode is POR for a GPU")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_BAR1_P2P,                 217,
                      "Whether P2P over BAR1 is supported")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_CE_PREFETCH,              218,
                      "Whether surfaces can be prefetched to cache")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_FSP,                      219,
                      "GPU supports FSP")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_GEN5_PCIE,                220,
                      "GPU supports Gen5 PCIE")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_ASPM_L1P,                 221,
                      "GPU supports ASPM L1P")
DEFINE_GPUSUB_FEATURE(GPUSUB_SUPPORTS_256B_PCIE_PACKETS,        222,
                      "GPU supports 256B PCIE packets")

// SOC features
DEFINE_SOC_FEATURE(SOC_SUPPORTS_PCIE,            0,
                   "PCIE supported on the SOC device" )
DEFINE_SOC_FEATURE(SOC_SUPPORTS_KFUSE_ACCESS,    1,
                   "KFUSE access supported on the SOC device" )
DEFINE_SOC_FEATURE(SOC_SUPPORTS_DISPLAY_WITH_FE, 2,
                   "SOC supports Display with FE (channel/method based HW interface)")
DEFINE_SOC_FEATURE(SOC_SUPPORTS_AV1_DECODE,      3,
                   "SOC supports AV1 decoding using Lwdec")
