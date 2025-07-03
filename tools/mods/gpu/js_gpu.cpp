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

/**
 * @file js_gpu.cpp
 * @brief JavaScript interface for the GPU
 *
 *
 */

//------------------------------------------------------------------------------
// Link Gpu to the JavaScript engine.
//------------------------------------------------------------------------------

#include "core/include/script.h"
#include "core/include/jscript.h"
#include "js_gpu.h"
#include "core/include/gpu.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"
#include "core/include/deprecat.h"
#include "core/include/js_uint64.h"
#include "gpu/perf/perfutil.h"
#include <vector>
#include <stdio.h>

JS_CLASS(Gpu);

//! Gpu_Object
/**
 *
 */
/* JS_DOX */ SObject Gpu_Object
(
    "Gpu",
    GpuClass,
    0,
    0,
    "Graphics Processing Unit."
);

// Make Gpu functions callable from JavaScript:

C_(Gpu_List);
static STest Gpu_List
(
    Gpu_Object,
    "List",
    C_Gpu_List,
    0,
    "List GPU to device mapping."
);

C_(Gpu_Initialize);
static STest Gpu_Initialize
(
    Gpu_Object,
    "Initialize",
    C_Gpu_Initialize,
    0,
    "Find & POST graphics boards, initialize resman."
);

C_(Gpu_ShutDown);
static STest Gpu_ShutDown
(
    Gpu_Object,
    "ShutDown",
    C_Gpu_ShutDown,
    0,
    "Cleanup and clear all Gpu state"
);

C_(Gpu_WriteExclusionRegions);
static STest Gpu_WriteExclusionRegions
(
    Gpu_Object,
    "WriteExclusionRegions",
    C_Gpu_WriteExclusionRegions,
    1,
    "Set register write exclusion regions."
);

C_(Gpu_SetHasBugOverride);
static STest Gpu_SetHasBugOverride
(
    Gpu_Object,
    "SetHasBugOverride",
    C_Gpu_SetHasBugOverride,
    3,       // GpuInst, BugNum, HasBug
    "Force whether there is a bug on a GPU."
);

C_(Gpu_RemapRegister);
static STest Gpu_RemapRegister
(
    Gpu_Object,
    "RemapRegister",
    C_Gpu_RemapRegister,
    2,  //fromAddr, toAddr
    "Specify a register to remap."
);

C_(Gpu_LogAccessRegions);
static STest Gpu_LogAccessRegions
(
    Gpu_Object,
    "LogAccessRegions",
    C_Gpu_LogAccessRegions,
    1,
    "Set register access logging regions."
);

C_(Gpu_ReadCrtc);
static SMethod Gpu_ReadCrtc
(
    Gpu_Object,
    "ReadCrtc",
    C_Gpu_ReadCrtc,
    2,
    "Read a VGA CRTC register."
);

C_(Gpu_WriteCrtc);
static STest Gpu_WriteCrtc
(
    Gpu_Object,
    "WriteCrtc",
    C_Gpu_WriteCrtc,
    3,
    "Write a VGA CRTC register."
);

C_(Gpu_ReadSr);
static SMethod Gpu_ReadSr
(
    Gpu_Object,
    "ReadSr",
    C_Gpu_ReadSr,
    2,
    "Read a VGA Sequencer register."
);

C_(Gpu_WriteSr);
static STest Gpu_WriteSr
(
    Gpu_Object,
    "WriteSr",
    C_Gpu_WriteSr,
    3,
    "Write a VGA Sequencer register."
);

C_(Gpu_ReadAr);
static SMethod Gpu_ReadAr
(
    Gpu_Object,
    "ReadAr",
    C_Gpu_ReadAr,
    2,
    "Read a VGA Attribute register."
);

C_(Gpu_WriteAr);
static STest Gpu_WriteAr
(
    Gpu_Object,
    "WriteAr",
    C_Gpu_WriteAr,
    3,
    "Write a VGA Attribute register."
);

C_(Gpu_ReadGr);
static SMethod Gpu_ReadGr
(
    Gpu_Object,
    "ReadGr",
    C_Gpu_ReadGr,
    2,
    "Read a VGA Graphics Controller register."
);

C_(Gpu_WriteGr);
static STest Gpu_WriteGr
(
    Gpu_Object,
    "WriteGr",
    C_Gpu_WriteGr,
    3,
    "Write a VGA Graphics Controller register."
);

C_(Gpu_Initialize)
{
    RETURN_RC(GpuPtr()->Initialize());
}

C_(Gpu_ShutDown)
{
    RETURN_RC(GpuPtr()->ShutDown(Gpu::ShutDownMode::Normal));
}

C_(Gpu_List)
{
    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportError(pContext, "GpuDevMgr is not initialized.");
        return JS_FALSE;
    }
    (((GpuDevMgr *)DevMgrMgr::d_GraphDevMgr)->ListDevices(Tee::PriAlways));

    return JS_TRUE;
}

C_(Gpu_DetectVbridge);
static STest Gpu_DetectVbridge
(
    Gpu_Object,
    "DetectVbridge",
    C_Gpu_DetectVbridge,
    0,
    "Detect the video bridge (will only work right in windows)."
);

// STest
C_(Gpu_DetectVbridge)
{
    static Deprecation depr("Gpu.DetectVbridge", "3/30/2019",
                            "Use GpuSubdevice.DetectVbridge() instead\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    RC rc;
    JavaScriptPtr pJs;

    // Check the arguments.
    JSObject * pReturlwals = 0;
    if ((NumArguments != 1)
        || (OK != pJs->FromJsval(pArguments[0], &pReturlwals)))
    {
        JS_ReportError(pContext, "Usage: Gpu.DetectVbridge([result])");
        return JS_FALSE;
    }

    bool result;

    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    C_CHECK_RC(pSubdev->DetectVbridge(&result));

    RETURN_RC(pJs->SetElement(pReturlwals, 0, result));
}

C_(Gpu_SetLwrrentDevice);
static STest Gpu_SetLwrrentDevice
(
    Gpu_Object,
    "SetLwrrentDevice",
    C_Gpu_SetLwrrentDevice,
    1,
    "Select which graphics board to talk to"
);
C_(Gpu_SetLwrrentDevice)
{
    static Deprecation depr("Gpu.SetLwrrentDevice", "3/30/2019",
                            "No longer necessary.\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    RETURN_RC(RC::OK);
}

C_(Gpu_SetLwrrentSubdevice);
static STest Gpu_SetLwrrentSubdevice
(
    Gpu_Object,
    "SetLwrrentSubdevice",
    C_Gpu_SetLwrrentSubdevice,
    1,
    "Select which graphics board to talk to"
);
C_(Gpu_SetLwrrentSubdevice)
{
    static Deprecation depr("Gpu.SetLwrrentDevice", "3/30/2019",
                            "No longer necessary.\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    RETURN_RC(RC::OK);
}

P_(Gpu_Get_AssertLwrrent);
P_(Gpu_Set_AssertLwrrent);
P_(Gpu_Get_AssertLwrrent)
{
    static Deprecation depr("Gpu.AssertLwrrent", "3/30/2019",
                            "AssertLwrrent no longer necessary");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    bool result = false;
    JavaScriptPtr pJs;
    RC rc = pJs->ToJsval(result, pValue);
    if (OK != rc)
    {
        pJs->Throw(pContext, rc, Utility::StrPrintf(
                   "Error colwerting result in Gpu.AssertLwrrent"));
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}
P_(Gpu_Set_AssertLwrrent)
{
    static Deprecation depr("Gpu.AssertLwrrent", "3/30/2019",
                            "AssertLwrrent no longer necessary");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    return JS_TRUE;
}
static SProperty Gpu_AssertLwrrent
(
    Gpu_Object,
    "AssertLwrrent",
    0,
    0,
    Gpu_Get_AssertLwrrent,
    Gpu_Set_AssertLwrrent,
    0,
    "Set to true to cause MODS to assert when we try to get the current device."
);

PROP_READWRITE(Gpu, GpuPtr(), OnlySocDevice, bool, "Make MODS see only the SOC device");

static SProperty Gpu_GpuNum
(
    Gpu_Object,
    "GpuNum",
    0,
    0,
    0,
    0,
    0,
    "GPU number (do not change manually--use -gpu_num command line option)"
);
SProperty Gpu_SkipConfigSim
(
    Gpu_Object,
    "SkipConfigSim",
    0,
    false,
    0,
    0,
    0,
    "Skip simulation configuration during initialization"
);
SProperty Gpu_StrapOpenGL
(
    Gpu_Object,
    "StrapOpenGL",
    0,
    -1,
    0,
    0,
    0,
    "Turn on/off the OpenGL strap."
);
SProperty Gpu_Boot0Strap
(
    Gpu_Object,
    "Boot0Strap",
    0,
    0xffffffff,
    0,
    0,
    0,
    "Initialize Boot0Strap register with the given value"
);
SProperty Gpu_Boot3Strap
(
    Gpu_Object,
    "Boot3Strap",
    0,
    0xffffffff,
    0,
    0,
    0,
    "Initialize Boot3Strap register with the given value"
);
SProperty Gpu_StrapRamcfg
(
    Gpu_Object,
    "StrapRamcfg",
    0,
    -1,
    0,
    0,
    0,
    "Select which ramcfg in the VBIOS to use."
);
SProperty Gpu_StrapGc6
(
    Gpu_Object,
    "StrapGc6",
    0,
    -1,
    0,
    0,
    0,
    "Initialize GC6 boot strap register with a given value."
);

SProperty Gpu_FbRandomBankSwizzle
(
    Gpu_Object,
    "FbRandomBankSwizzle",
    0,
    0,
    0,
    0,
    0,
    "Set the randome bank swizzle behaviour.  0=default, 1=enable, 2=disable."
);
SProperty Gpu_FbBankMapPattern
(
    Gpu_Object,
    "FbBankMapPattern",
    0,
    0,
    0,
    0,
    0,
    "Set the bank map pattern of framebuffer banks.  0=default, or set to 1 (rotate 12), 2 (rotate 24)."
);
SProperty Gpu_FbCmdMapping
(
    Gpu_Object,
    "FbCmdMapping",
    0,
    0,
    0,
    0,
    0,
    "Set mapping of the CS/RAS/CAS/WE/ADR to CMD[] bus"
);
SProperty Gpu_FbBankSize
(
    Gpu_Object,
    "FbBankSize",
    0,
    0,
    0,
    0,
    0,
    "Set the bank size of framebuffer banks.  0=default, or set to log2(number of bytes)."
);
SProperty Gpu_FbBanks
(
    Gpu_Object,
    "FbBanks",
    0,
    0,
    0,
    0,
    0,
    "Set the number of framebuffer (internal) banks.  0=default, or set to 4 or 8."
);
SProperty Gpu_FbBurstLength
(
    Gpu_Object,
    "FbBurstLength",
    0,
    0,
    0,
    0,
    0,
    "Set the framebuffer burst length.  0=default, or set to 4 or 8."
);
SProperty Gpu_FbColumnBits
(
    Gpu_Object,
    "FbColumnBits",
    0,
    0,
    0,
    0,
    0,
    "Set the number of framebuffer column address bits. (0=default)"
);
SProperty Gpu_FbExtBank
(
    Gpu_Object,
    "FbExtBank",
    0,
    0,
    0,
    0,
    0,
    "Set the number of external framebuffer banks.  0=default, or set to 1 or 2."
);
SProperty Gpu_FbRowBits
(
    Gpu_Object,
    "FbRowBits",
    0,
    0,
    0,
    0,
    0,
    "Set the number of framebuffer row address bits. (0=default)"
);
SProperty Gpu_PerFbpRowBits
(
    Gpu_Object,
    "PerFbpRowBits",
    0,
    0,
    0,
    0,
    0,
    "Set the number of framebuffer row address bits. ([0]=default)"
);

SProperty Gpu_FbEnableChannel32
(
    Gpu_Object,
    "FbEnableChannel32",
    0,
    false,
    0,
    0,
    0,
    "Set the CHANNEL_ENABLE_32_BIT for LW_PFB_CHANNELCFG. (false=default)"
);
SProperty Gpu_RopEnableMask
(
    Gpu_Object,
    "RopEnableMask",
    0,
    0,
    0,
    0,
    0,
    "Set the mask of enabled rop engines. (0=use default)"
);
SProperty Gpu_VpeEnableMask
(
    Gpu_Object,
    "VpeEnableMask",
    0,
    0,
    0,
    0,
    0,
    "Set the mask of enabled VPEs. (0=use default)"
);
SProperty Gpu_ShdEnableMask
(
    Gpu_Object,
    "ShdEnableMask",
    0,
    0,
    0,
    0,
    0,
    "Set the mask of enabled shader pipes. (0=use default)"
);
SProperty Gpu_TpcEnableMask
(
    Gpu_Object,
    "TpcEnableMask",
    0,
    0,
    0,
    0,
    0,
    "Set the mask of enabled TPCs. (0=use default)"
);
SProperty Gpu_SmEnableMask
(
    Gpu_Object,
    "SmEnableMask",
    0,
    0,
    0,
    0,
    0,
    "Set the mask of enabled SMs. (0=use default)"
);
SProperty Gpu_SmEnableMaskTpc0
(
    Gpu_Object,
    "SmEnableMaskTpc0",
    0,
    0,
    0,
    0,
    0,
    "Set the mask of enabled SMs for TPC0. (0=use default)"
);
SProperty Gpu_SmEnableMaskTpc1
(
    Gpu_Object,
    "SmEnableMaskTpc1",
    0,
    0,
    0,
    0,
    0,
    "Set the mask of enabled SMs for TPC1. (0=use default)"
);
SProperty Gpu_SmEnableMaskTpc2
(
    Gpu_Object,
    "SmEnableMaskTpc2",
    0,
    0,
    0,
    0,
    0,
    "Set the mask of enabled SMs for TPC2. (0=use default)"
);
SProperty Gpu_SmEnableMaskTpc3
(
    Gpu_Object,
    "SmEnableMaskTpc3",
    0,
    0,
    0,
    0,
    0,
    "Set the mask of enabled SMs for TPC3. (0=use default)"
);
SProperty Gpu_FbEnableMask
(
    Gpu_Object,
    "FbEnableMask",
    0,
    0,
    0,
    0,
    0,
    "Set the mask of enabled framebuffer partitions. (0=use default)"
);
SProperty Gpu_MevpEnableMask
(
    Gpu_Object,
    "MevpEnableMask",
    0,
    1,
    0,
    0,
    0,
    "Set the mask of enabled mpeg decoders. (1=use default)"
);
SProperty Gpu_ZlwllEnableMask
(
    Gpu_Object,
    "ZlwllEnableMask",
    0,
    0,
    0,
    0,
    0,
    "Set the mask of enabled portions of the zlwll RAM. (0=use default)"
);
PROP_READWRITE
(
    Gpu,
    GpuPtr(),
    RunlistSize,
    UINT32,
    "Set the default size for buffer-based runlists"
);
SProperty Gpu_SkipDevInit
(
    Gpu_Object,
    "SkipDevInit",
    0,
    false,
    0,
    0,
    0,
    "Skip the devinit (both VBIOS and RM) entirely."
);
SProperty Gpu_SkipRmStateInit
(
    Gpu_Object,
    "SkipRmStateInit",
    0,
    false,
    0,
    0,
    0,
    "Skip the RM state init; do only the VBIOS devinit."
);

SProperty Gpu_StopInitAfterHulk
(
    Gpu_Object,
    "StopInitAfterHulk",
    0,
    false,
    0,
    0,
    0,
    "Stop GPU init after processing HULK."
);

SProperty Gpu_DramclkDevInitWARkHz
(
    Gpu_Object,
    "DramclkDevInitWARkHz",
    0,
    0,
    0,
    0,
    0,
    "Apply dramclk freq override during devinit."
);

SProperty Gpu_DisableEcc
(
    Gpu_Object,
    "DisableEcc",
    0,
    false,
    0,
    0,
    0,
    "Disable ECC"
);

SProperty Gpu_EnableEcc
(
    Gpu_Object,
    "EnableEcc",
    0,
    false,
    0,
    0,
    0,
    "Enable ECC"
);

SProperty Gpu_DisableDbi
(
    Gpu_Object,
    "DisableDbi",
    0,
    false,
    0,
    0,
    0,
    "Disable DBI"
);

SProperty Gpu_EnableDbi
(
    Gpu_Object,
    "EnableDbi",
    0,
    false,
    0,
    0,
    0,
    "Enable DBI"
);

SProperty Gpu_SkipInfoRomRowRemapping
(
    Gpu_Object,
    "SkipInfoRomRowRemapping",
    0,
    false,
    0,
    0,
    0,
    "Skip InfoROM row remapping"
);

SProperty Gpu_DisableRowRemapper
(
    Gpu_Object,
    "DisableRowRemapper",
    0,
    false,
    0,
    0,
    0,
    "Disable the row remapper feature"
);

SProperty Gpu_Reset
(
    Gpu_Object,
    "Reset",
    0,
    static_cast<UINT32>(Gpu::ResetMode::NoReset),
    0,
    0,
    0,
    "Perform a reset before GPU initialization"
);

SProperty Gpu_IsFusing
(
    Gpu_Object,
    "IsFusing",
    0,
    false,
    0,
    0,
    0,
    "Flag this run as a fusing run."
);

SProperty Gpu_ForceNoDevices
(
    Gpu_Object,
    "ForceNoDevices",
    0,
    false,
    0,
    0,
    0,
    "Continue to init even if no devices are found"
);

SProperty Gpu_ReverseLVDSTMDS
(
    Gpu_Object,
    "ReverseLVDSTMDS",
    0,
    false,
    0,
    0,
    0,
    "Reverse LVDS and TMDS entries in VBIOS - useful for SOR loopback testing"
);

SProperty Gpu_PreInitCallback
(
    Gpu_Object,
    "PreInitCallback",
    0,
    "HandleGpuPreInitArgs",
    0,
    0,
    0,
    "Pre GPU initialization JS callback function"
);

SProperty Gpu_PostInitCallback
(
    Gpu_Object,
    "PostInitCallback",
    0,
    "HandleGpuPostInitArgs",
    0,
    0,
    0,
    "Post GPU initialization JS callback function"
);

SProperty Gpu_EndOfSimAssert
(
    Gpu_Object,
    "EndOfSimAssert",
    0,
    true,
    0,
    0,
    0,
    "Turn on/off the EndOfSim signal assert."
);

SProperty Gpu_FenceSyncBeforeEos
(
    Gpu_Object,
    "FenceSyncBeforeEos",
    0,
    false,
    0,
    0,
    0,
    "Sync priv register access through fence register before EOS assert."
);

SProperty Gpu_DisableFenceSyncBeforeEos
(
    Gpu_Object,
    "DisableFenceSyncBeforeEos",
    0,
    false,
    0,
    0,
    0,
    "Disable fence sync priv register function before EOS assert."
);

SProperty Gpu_ForceRepost
(
    Gpu_Object,
    "ForceRepost",
    0,
    false,
    0,
    0,
    0,
    "Force the RM to repost the GPU."
);

SProperty Gpu_HbmCfgCheckIgnore
(
    Gpu_Object,
    "HbmCfgCheckIgnore",
    0,
    false,
    0,
    0,
    0,
    "Flag to ignore unsupported HBM Config"
);

SProperty Gpu_SkipVbiosPost
(
    Gpu_Object,
    "SkipVbiosPost",
    0,
    false,
    0,
    0,
    0,
    "Force the RM to skip posting the GPU."
);

SProperty Gpu_FilterBug599380Exceptions
(
    Gpu_Object,
    "FilterBug599380Exceptions",
    0,
    true,
    0,
    0,
    0,
    "Silently ignore harmless CRD_DEAD_TAGS exceptions."
);

SProperty Gpu_UseDisplayID
(
    Gpu_Object,
    "UseDisplayID",
    0,
    false,
    0,
    0,
    0,
    "Initializes Display for DisplayID based Implementation."
);

SProperty Gpu_ResetExceptions
(
    Gpu_Object,
    "ResetExceptions",
    0,
    false,
    0,
    0,
    0,
    "Reset exceptions when shutting down gpu subdev."
);

SProperty Gpu_CheckEdcErrors
(
    Gpu_Object,
    "CheckEdcErrors",
    0,
    true,
    0,
    0,
    0,
    "Keep counting Edc errors during mods exelwtion."
);

SProperty Gpu_FbHbmDualRankEnabled
(
    Gpu_Object,
    "FbHbmDualRankEnabled",
    0,
    false,
    0,
    0,
    0,
    "Enable dual rank HBM dram. Deprecated: use -fb_hbm_num_sids instead."
);

SProperty Gpu_FbHbmNumSids
(
    Gpu_Object,
    "FbHbmNumSids",
    0,
    0,
    0,
    0,
    0,
    "Set the number of HBM ranks."
);

SProperty Gpu_FbRowAMaxCount
(
    Gpu_Object,
    "FbRowAMaxCount",
    0,
    0,
    0,
    0,
    0,
    "Override maximum number of rows in Rank0 when non-zero."
);

SProperty Gpu_FbRowBMaxCount
(
    Gpu_Object,
    "FbRowBMaxCount",
    0,
    0,
    0,
    0,
    0,
    "Override maximum number of rows in Rank1 when non-zero"
);

SProperty Gpu_FbHbmBanksPerRank
(
    Gpu_Object,
    "FbHbmBanksPerRank",
    0,
    0,
    0,
    0,
    0,
    "Set the number of banks per rank when dual rank HBM drams are enabled. Deprecated: use -fb_hbm_num_sids instead."
);

SProperty Gpu_FbRefsbDualRequest
(
    Gpu_Object,
    "FbRefsbDualRequest",
    0,
    0xffffffff,
    0,
    0,
    0,
    "Enable refsb dual request"
);

SProperty Gpu_FbClamshell
(
    Gpu_Object,
    "FbClamshell",
    0,
    0xffffffff,
    0,
    0,
    0,
    "Enable clamshell"
);

SProperty Gpu_FbBanksPerRowBg
(
    Gpu_Object,
    "FbBanksPerRowBg",
    0,
    0,
    0,
    0,
    0,
    "Set FB Banks Per Row Bg"
);

SProperty Gpu_HbmDeviceInitMethod
(
    Gpu_Object,
    "HbmDeviceInitMethod",
    0,
#ifdef SIM_BUILD
    // On Sim builds, there isn't a real HBM chip so we can't read its fuses
    Gpu::DoNotInitHbm,
#else
    Gpu::InitHbmTempDefault, // HBM device interaction can be done using host to jtag
        // read/writes or via the FB priv registers.
        // FB priv path: Broken on GP100 due to a timing bug.
        // Writing Jtag regs: Causes interrupts while initializing RM.
#endif
    0,
    0,
    0,
    "Set HBM Device initialization method."
);

//-------------------------------------------------------------------------------------------------
// This property is strictly for emulation when there is a need to load an older netlist that does
// not have FSP support. When the user passes -disable_fsp_support the call to 
// SetHasFeature(GPUSUB_SUPPORTS_FSP) will be cleared.
SProperty Gpu_DisableFspSupport
(
    Gpu_Object,
    "DisableFspSupport",
    0,
    false,
    0,
    0,
    0,
    "Disable FSP support and fallback to SUPPORTS_GFW_HOLDOFF"
);


// Make Gpu enum values visible in JavaScript.
PROP_CONST( Gpu, MaxNumDevices, Gpu::MaxNumDevices );
PROP_CONST( Gpu, MaxNumSubdevices, Gpu::MaxNumSubdevices );
PROP_CONST( Gpu, MasterSubdevice, Gpu::MasterSubdevice);

#define DEFINE_NEW_GPU( DevIdStart, DevIdEnd, ChipId, GpuId, ...) \
                        PROP_CONST( Gpu, GpuId, Gpu::GpuId );
#define DEFINE_DIS_GPU( DevIdStart, DevIdEnd, ChipId, GpuId, GpuConstant, ...) \
                        PROP_CONST( Gpu, GpuId, GpuConstant );
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU
#undef DEFINE_DIS_GPU

PROP_CONST( Gpu, Pci,  Gpu::Pci );
PROP_CONST( Gpu, PciExpress, Gpu::PciExpress );
PROP_CONST( Gpu, FPci, Gpu::FPci );
PROP_CONST( Gpu, Axi,  Gpu::Axi );
PROP_CONST( Gpu, CoreV,   Gpu::CoreV );
PROP_CONST( Gpu, VOLTAGE_LOGIC, Gpu::VOLTAGE_LOGIC);
PROP_CONST( Gpu, VOLTAGE_SRAM, Gpu::VOLTAGE_SRAM);
PROP_CONST( Gpu, VOLTAGE_MSVDD, Gpu::VOLTAGE_MSVDD);
PROP_CONST( Gpu, SplitVoltageDomain_NUM, Gpu::SplitVoltageDomain_NUM);
PROP_CONST( Gpu, ClkUnkUnsp,   Gpu::ClkUnkUnsp );
PROP_CONST( Gpu, ClkM,    Gpu::ClkM );
PROP_CONST( Gpu, ClkHost, Gpu::ClkHost );
PROP_CONST( Gpu, ClkDisp, Gpu::ClkDisp );
PROP_CONST( Gpu, ClkPA,   Gpu::ClkPA );
PROP_CONST( Gpu, ClkPB,   Gpu::ClkPB );
PROP_CONST( Gpu, ClkX,    Gpu::ClkX );
PROP_CONST( Gpu, ClkGpc2,   Gpu::ClkGpc2 );
PROP_CONST( Gpu, ClkLtc2,   Gpu::ClkLtc2 );
PROP_CONST( Gpu, ClkXbar2,  Gpu::ClkXbar2 );
PROP_CONST( Gpu, ClkSys2,   Gpu::ClkSys2 );
PROP_CONST( Gpu, ClkHub2,   Gpu::ClkHub2 );
PROP_CONST( Gpu, ClkGpc,    Gpu::ClkGpc );
PROP_CONST( Gpu, ClkLtc,    Gpu::ClkLtc );
PROP_CONST( Gpu, ClkXbar,   Gpu::ClkXbar );
PROP_CONST( Gpu, ClkSys,    Gpu::ClkSys );
PROP_CONST( Gpu, ClkHub,    Gpu::ClkHub );
PROP_CONST( Gpu, ClkLeg,    Gpu::ClkLeg );
PROP_CONST( Gpu, ClkUtilS,  Gpu::ClkUtilS );
PROP_CONST( Gpu, ClkPwr,    Gpu::ClkPwr );
PROP_CONST( Gpu, ClkMSD,    Gpu::ClkMSD );
PROP_CONST( Gpu, ClkLwd,    Gpu::ClkLwd );
PROP_CONST( Gpu, ClkPexGen, Gpu::ClkPexGen );
PROP_CONST( Gpu, ClkDomain_NUM, Gpu::ClkDomain_NUM );
PROP_CONST( Gpu, ClkSrcDefault,             Gpu::ClkSrcDefault );
PROP_CONST( Gpu, ClkSrcXTAL,                Gpu::ClkSrcXTAL );
PROP_CONST( Gpu, ClkSrcEXTREF,              Gpu::ClkSrcEXTREF );
PROP_CONST( Gpu, ClkSrcQUALEXTREF,          Gpu::ClkSrcQUALEXTREF );
PROP_CONST( Gpu, ClkSrcEXTSPREAD,           Gpu::ClkSrcEXTSPREAD );
PROP_CONST( Gpu, ClkSrcSPPLL0,              Gpu::ClkSrcSPPLL0 );
PROP_CONST( Gpu, ClkSrcSPPLL1,              Gpu::ClkSrcSPPLL1 );
PROP_CONST( Gpu, ClkSrcMPLL,                Gpu::ClkSrcMPLL );
PROP_CONST( Gpu, ClkSrcXCLK,                Gpu::ClkSrcXCLK );
PROP_CONST( Gpu, ClkSrcXCLK3XDIV2,          Gpu::ClkSrcXCLK3XDIV2 );
PROP_CONST( Gpu, ClkSrcDISPPLL,             Gpu::ClkSrcDISPPLL );
PROP_CONST( Gpu, ClkSrcHOSTCLK,             Gpu::ClkSrcHOSTCLK );
PROP_CONST( Gpu, ClkSrcPEXREFCLK,           Gpu::ClkSrcPEXREFCLK );
PROP_CONST( Gpu, ClkSrcVPLL0,               Gpu::ClkSrcVPLL0 );
PROP_CONST( Gpu, ClkSrcVPLL1,               Gpu::ClkSrcVPLL1 );
PROP_CONST( Gpu, ClkSrcGPCPLL,              Gpu::ClkSrcGPCPLL );
PROP_CONST( Gpu, ClkSrcLTCPLL,              Gpu::ClkSrcLTCPLL );
PROP_CONST( Gpu, ClkSrcXBARPLL,             Gpu::ClkSrcXBARPLL );
PROP_CONST( Gpu, ClkSrcSYSPLL,              Gpu::ClkSrcSYSPLL );
PROP_CONST( Gpu, ClkSrcXTAL4X,              Gpu::ClkSrcXTAL4X );
PROP_CONST( Gpu, ClkSrcREFMPLL,             Gpu::ClkSrcREFMPLL );
PROP_CONST( Gpu, ClkSrcXCLK500,             Gpu::ClkSrcXCLK500 );
PROP_CONST( Gpu, ClkSrcXCLKGEN3,            Gpu::ClkSrcXCLKGEN3 );
PROP_CONST( Gpu, ClkSrcHBMPLL,              Gpu::ClkSrcHBMPLL );
PROP_CONST( Gpu, ClkSrcNAFLL,               Gpu::ClkSrcNAFLL );
PROP_CONST( Gpu, ClkActual,                 0 );  // index into results from GetClock: this is what RM attempst to program
PROP_CONST( Gpu, ClkTarget,                 1 );  // index into results from GetClock: this is what the user/pstate attempts to set
PROP_CONST( Gpu, ClkEffective,              2 );  // index into results from GetClock: this is what the clock counter reports
PROP_CONST( Gpu, ClkSource,                 3 );  // index into results from GetClock
PROP_CONST( Gpu, DoNotInitHbm,              Gpu::DoNotInitHbm );
PROP_CONST( Gpu, InitHbmFbPrivToIeee,       Gpu::InitHbmFbPrivToIeee );
PROP_CONST( Gpu, InitHbmHostToJtag,         Gpu::InitHbmHostToJtag );
PROP_CONST( Gpu, InitHbmTempDefault,        Gpu::InitHbmTempDefault );
PROP_CONST( Gpu, Celsius,                   GpuDevice::Celsius );
PROP_CONST( Gpu, Rankine,                   GpuDevice::Rankine );
PROP_CONST( Gpu, Lwrie,                     GpuDevice::Lwrie );
PROP_CONST( Gpu, Tesla,                     GpuDevice::Tesla );
PROP_CONST( Gpu, Fermi,                     GpuDevice::Fermi );
PROP_CONST( Gpu, Kepler,                    GpuDevice::Kepler );
PROP_CONST( Gpu, Maxwell,                   GpuDevice::Maxwell );
PROP_CONST( Gpu, Pascal,                    GpuDevice::Pascal );
PROP_CONST( Gpu, Volta,                     GpuDevice::Volta );
PROP_CONST( Gpu, Turing,                    GpuDevice::Turing );
PROP_CONST( Gpu, Ampere,                    GpuDevice::Ampere );
PROP_CONST( Gpu, Ada,                       GpuDevice::Ada );
PROP_CONST( Gpu, Hopper,                    GpuDevice::Hopper );
PROP_CONST( Gpu, Blackwell,                 GpuDevice::Blackwell );
PROP_CONST( Gpu, G00X,                      GpuDevice::G00X );
PROP_CONST( Gpu, ResetDevice,               static_cast<UINT32>(Gpu::ResetMode::ResetDevice) );
PROP_CONST( Gpu, FundamentalReset,          static_cast<UINT32>(Gpu::ResetMode::FundamentalReset) );
PROP_CONST( Gpu, HotReset,                  static_cast<UINT32>(Gpu::ResetMode::HotReset) );
PROP_CONST( Gpu, SwReset,                   static_cast<UINT32>(Gpu::ResetMode::SwReset) );
PROP_CONST( Gpu, NoReset,                   static_cast<UINT32>(Gpu::ResetMode::NoReset) );
PROP_CONST( Gpu, FLReset,                   static_cast<UINT32>(Gpu::ResetMode::FLReset) );
PROP_CONST( Gpu, MonitorAll,                Gpu::MF_ALL );
PROP_CONST( Gpu, MonitorOnlyEngines,        Gpu::MF_ONLY_ENGINES );
PROP_CONST( Gpu, MonitorFilterCount,        Gpu::MF_NUM_FILTERS );

// Make Gpu "get" functions into JavaScript readonly properties of the Gpu object.
//-----------------------------------------------------------------------------
PROP_READONLY( Gpu, GpuPtr(), IsInitialized,        bool,     "true if Gpu.Initialize has been called." );

#define DEPR_PROP_READONLY(funcName, defaultValue, resultType, helpmsg) \
P_(Gpu_Get_##funcName);                                                 \
P_(Gpu_Get_##funcName)                                                  \
{                                                                       \
    static Deprecation depr("Gpu." #funcName, "3/30/2019",              \
                            "Gpu." #funcName " has been deprecated."    \
                            " Use GpuDevice/GpuSubdevice instead.");    \
    if (!depr.IsAllowed(pContext))                                      \
        return JS_FALSE;                                                \
                                                                        \
    resultType result = defaultValue;                                   \
    JavaScriptPtr pJs;                                                  \
    RC rc = pJs->ToJsval(result, pValue);                               \
    if (OK != rc)                                                       \
    {                                                                   \
        pJs->Throw(pContext, rc, Utility::StrPrintf(                    \
                   "Error colwerting result in Gpu.%s",                 \
                   #funcName));                                         \
        *pValue = JSVAL_NULL;                                           \
        return JS_FALSE;                                                \
    }                                                                   \
    return JS_TRUE;                                                     \
}                                                                       \
static SProperty Gpu_ ## funcName                                       \
(                                                                       \
    Gpu_Object,                                                         \
    #funcName,                                                          \
    0,                                                                  \
    0,                                                                  \
    Gpu_Get_ ## funcName,                                               \
    0,                                                                  \
    0,                                                                  \
    helpmsg                                                             \
)

DEPR_PROP_READONLY( LwrrentDevice,              0, UINT32, "Current mods graphics device number." );
DEPR_PROP_READONLY( NumSubdevices,              0, UINT32, "Number of GPUs on a device (1 unless MC or MB)." );
DEPR_PROP_READONLY( LwrrentSubdevice,           0, UINT32, "Lwrrently selected chip of a multichip device." );
DEPR_PROP_READONLY( RmDeviceReference,          0, UINT32, "Resource manager device reference for current board/chip." );
DEPR_PROP_READONLY( HQualType,                  0, UINT32, "Hardware Qualification sample type" );
DEPR_PROP_READONLY( PciDomainNumber,            0, UINT32, "PCI DomainNumber for current graphics device." );
DEPR_PROP_READONLY( PciBusNumber,               0, UINT32, "PCI BusNumber for current graphics device." );
DEPR_PROP_READONLY( PciDeviceNumber,            0, UINT32, "PCI DeviceNumber for current graphics device." );
DEPR_PROP_READONLY( PciFunctionNumber,          0, UINT32, "PCI FunctionNumber for current graphics device." );
DEPR_PROP_READONLY( Foundry,                    0, UINT32, "Foundry number for current graphics device." );
DEPR_PROP_READONLY( PciDeviceId,                0, UINT32, "PCI DeviceId for current graphics device." );
DEPR_PROP_READONLY( DeviceId,                   0, UINT32, "LW DeviceId for current graphics device." );
DEPR_PROP_READONLY( DeviceName,                 0, string, "Marketing name for current graphics device." );
DEPR_PROP_READONLY( Irq,                        0, UINT32, "Irq this device is hooked to." );
DEPR_PROP_READONLY( Revision,                   0, UINT32, "lwpu chip revision" );
DEPR_PROP_READONLY( RevisionString,            "", string, "lwpu chip revision as a string." );
DEPR_PROP_READONLY( PhysLwBase,                 0, PHYSADDR, "register physical base address" );
DEPR_PROP_READONLY( PhysFbBase,                 0, PHYSADDR, "frame buffer physical base address" );
DEPR_PROP_READONLY( PhysInstBase,               0, PHYSADDR, "instance memory physical base address" );
DEPR_PROP_READONLY( LwApertureSize,             0, UINT32, "register aperture size" );
DEPR_PROP_READONLY( FbApertureSize,             0, UINT64, "frame buffer aperture size" );
DEPR_PROP_READONLY( InstApertureSize,           0,  INT64, "instance aperture size" );
DEPR_PROP_READONLY( FbHeapSize,                 0, UINT64, "size of the usable region of the FB" );
DEPR_PROP_READONLY( SubsystemVendorId,          0, UINT32, "SubsystemVendorId" );
DEPR_PROP_READONLY( SubsystemDeviceId,          0, UINT32, "SubsystemDeviceId" );
DEPR_PROP_READONLY( BusType,                    0, UINT32, "is pci express" );
DEPR_PROP_READONLY( CrystalFrequency,           0, UINT32, "Crystal frequency" );
DEPR_PROP_READONLY( BoardId,                    0, UINT32, "Board ID" );
DEPR_PROP_READONLY( FramebufferBarSize,         0, UINT64, "Framebuffer aperture size" );
DEPR_PROP_READONLY( PciAdNormal,                0, UINT32, "PCI bus normal or swizzled" );
DEPR_PROP_READONLY( PciEHostLinkWidth,          0, UINT32, "Host PCI Express link width" );
DEPR_PROP_READONLY( PciEGpuLinkWidth,           0, UINT32, "Gpu PCI Express link width" );
DEPR_PROP_READONLY( PciELinkSpeed,              0, UINT32, "PCI Express link speed (in MB/s)" );
DEPR_PROP_READONLY( SupportsNonCoherent,    false,   bool, "whether Non-Coherent memory can be allocated");
DEPR_PROP_READONLY( SupportsRenderToSysMem, false,   bool, "whether can render to system memory");
DEPR_PROP_READONLY( RopCount,                   0, UINT32, "number of ROPs");
DEPR_PROP_READONLY( ShaderCount,                0, UINT32, "number of shader pipes");
DEPR_PROP_READONLY( VpeCount,                   0, UINT32, "number of VPEs");
DEPR_PROP_READONLY( DeviceCount,                0, UINT32, "number of devices");

DEPR_PROP_READONLY( TpcMask,                    0, UINT32, "TPCs mask");
DEPR_PROP_READONLY( FbMask,                     0, UINT32, "FB Partition mask");
DEPR_PROP_READONLY( SmMask,                     0, UINT32, "SM mask");
DEPR_PROP_READONLY( XpMask,                     0, UINT32, "3gio pads mask");
DEPR_PROP_READONLY( MevpMask,                   0, UINT32, "Mevp units mask");


P_(Gpu_Get_ChipIdCooked);
P_(Gpu_Get_ChipIdCooked)
{
    static Deprecation depr("Gpu.ChipIdCooked", "3/30/2019",
                            "ChipIdCooked has been deprecated.");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    string result = "";

    if (OK != JavaScriptPtr()->ToJsval(result, pValue))
    {
        JS_ReportError(pContext, "Failed to get Gpu.ChipIdCooked" );
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}
static SProperty Gpu_ChipIdCooked
(
    Gpu_Object,
    "ChipIdCooked",
    0,
    0,
    Gpu_Get_ChipIdCooked,
    0,
    0,
    "lwpu chip ID as a human-readable string."
);

//! @todo Doxygenize this

PROP_READWRITE
(
    Gpu,
    GpuPtr(),
    UseVirtualDma,
    bool,
    "is virtual-memory DMA encouraged"
);

CLASS_PROP_READWRITE_LWSTOM_FULL
(
    Gpu,
    DefaultTimeoutMs,
    "Default timeout for operations that aren't test-specific",
    0,
    Tasker::GetDefaultTimeoutMs()
);

P_(Gpu_Get_DefaultTimeoutMs)
{
    FLOAT64 result = Tasker::GetDefaultTimeoutMs();

    if (OK != JavaScriptPtr()->ToJsval(result, pValue))
    {
        JS_ReportError(pContext, "Failed to get Gpu.DefaultTimeoutMs");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(Gpu_Set_DefaultTimeoutMs)
{
    FLOAT64 Value;

    if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))
    {
        JS_ReportError(pContext, "Failed to set Gpu.DefaultTimeoutMs");
        return JS_FALSE;
    }

    if (OK != Tasker::SetDefaultTimeoutMs(Value))
    {
        JS_ReportWarning(pContext, "Error Setting Gpu.DefaultTimeoutMs");
        *pValue = JSVAL_NULL;
        return JS_TRUE;
    }
    return JS_TRUE;
}

SProperty Gpu_IgnoreExtPower
(
    Gpu_Object,
    "IgnoreExtPower",
    0,
    false,
    0,
    0,
    0,
    "Ignore missing external power during Gpu.Initialize()"
);

SProperty Gpu_PollInterrupts
(
    Gpu_Object,
    "PollInterrupts",
    0,
    false,
    0,
    0,
    0,
    "Poll for GPU interrupts."
);

SProperty Gpu_AutoInterrupts
(
    Gpu_Object,
    "AutoInterrupts",
    0,
    false,
    0,
    0,
    0,
    "Use whatever interrupt mechanism is available, without failing."
);

SProperty Gpu_IntaInterrupts
(
    Gpu_Object,
    "IntaInterrupts",
    0,
    false,
    0,
    0,
    0,
    "Use INTA protocol for GPU interrupts."
);

SProperty Gpu_MsiInterrupts
(
    Gpu_Object,
    "MsiInterrupts",
    0,
    false,
    0,
    0,
    0,
    "Use MSI protocol for GPU interrupts."
);

SProperty Gpu_MsixInterrupts
(
    Gpu_Object,
    "MsixInterrupts",
    0,
    false,
    0,
    0,
    0,
    "Use MSI-X protocol for GPU interrupts."
);

SProperty Gpu_SkipIntaIntrCheck
(
    Gpu_Object,
    "SkipIntaIntrCheck",
    0,
    false,
    0,
    0,
    0,
    "Skip validation of INTA interrupts."
);

SProperty Gpu_SkipMsiIntrCheck
(
    Gpu_Object,
    "SkipMsiIntrCheck",
    0,
    false,
    0,
    0,
    0,
    "Skip validation of MSI interrupts."
);

SProperty Gpu_SkipMsixIntrCheck
(
    Gpu_Object,
    "SkipMsixIntrCheck",
    0,
    false,
    0,
    0,
    0,
    "Skip validation of MSI-X interrupts."
);

SProperty Gpu_MonitorIntervalMs
(
    Gpu_Object,
    "MonitorIntervalMs",
    0,
    0,
    0,
    0,
    0,
    "If nonzero, run a background GPU monitor, polling every that many milliseconds."
);

SProperty Gpu_MonitorFilter
(
    Gpu_Object,
    "MonitorFilter",
    0,
    0,
    0,
    0,
    0,
    "Filter for controlling what the background GPU montioring thread actually monitors."
);

SProperty Gpu_ThreadIntervalMs
(
    Gpu_Object,
    "ThreadIntervalMs",
    0,
    0,
    0,
    0,
    0,
    "If nonzero, run a background JS function every that many milliseconds."
);

static SProperty Gpu_MB_Master
(
    Gpu_Object,
    "MB_Master",
    0,
    0,
    0,
    0,
    0,
    "If MultiBoard is enabled, make this board the master (default 0)."
);

SProperty Gpu_SkipRestoreDisplay
(
    Gpu_Object,
    "SkipRestoreDisplay",
    0,
    false,
    0,
    0,
    0,
    "Skip any attempts to restore display via state cache."
);

SProperty Gpu_CheckXenCompatibility
(
    Gpu_Object,
    "CheckXenCompatibility",
    0,
    false,
    0,
    0,
    0,
    "Verify GPU is configured correctly for vGPU and pass through modes."
);

SProperty Gpu_VprCya0
(
    Gpu_Object,
    "VprCya0",
    0,
    0,
    0,
    0,
    0,
    "Override the lower 32b of the VPR CYA value."
);

SProperty Gpu_VprCya1
(
    Gpu_Object,
    "VprCya1",
    0,
    0,
    0,
    0,
    0,
    "Override the upper 32b of the VPR CYA value."
);

SProperty Gpu_Acr1Size
(
    Gpu_Object,
    "Acr1Size",
    0,
    0,
    0,
    0,
    0,
    "Indicates the size of the ACR1 region."
);

SProperty Gpu_Acr1ReadMask
(
    Gpu_Object,
    "Acr1ReadMask",
    0,
    0,
    0,
    0,
    0,
    "Specifies the read mask of ACR1 region."
);

SProperty Gpu_Acr1WriteMask
(
    Gpu_Object,
    "Acr1WriteMask",
    0,
    0,
    0,
    0,
    0,
    "Specifies the write mask of ACR1 region."
);

SProperty Gpu_Acr2Size
(
    Gpu_Object,
    "Acr2Size",
    0,
    0,
    0,
    0,
    0,
    "Indicates the size of the ACR2 region."
);

SProperty Gpu_Acr2ReadMask
(
    Gpu_Object,
    "Acr2ReadMask",
    0,
    0,
    0,
    0,
    0,
    "Specifies the read mask of ACR2 region."
);

SProperty Gpu_Acr2WriteMask
(
    Gpu_Object,
    "Acr2WriteMask",
    0,
    0,
    0,
    0,
    0,
    "Specifies the write mask of ACR2 region."
);

SProperty Gpu_VprStartAddrMb
(
    Gpu_Object,
    "VprStartAddrMb",
    0,
    0xffffffff,
    0,
    0,
    0,
    "Specifies the VPR starting physical address (aligned to MB)."
);

SProperty Gpu_Acr1StartAddr
(
    Gpu_Object,
    "Acr1StartAddr",
    0,
    0,
    0,
    0,
    0,
    "Specifies the ACR1 starting physical address."
);

SProperty Gpu_Acr2StartAddr
(
    Gpu_Object,
    "Acr2StartAddr",
    0,
    0,
    0,
    0,
    0,
    "Specifies the ACR2 starting physical address."
);

SProperty Gpu_UprStartAddrList
(
    Gpu_Object,
    "UprStartAddrList",
    0,
    0,
    0,
    0,
    0,
    "A list of starting physical addresses for regions of confidential computing unprotected memory."
);

SProperty Gpu_UprSizeList
(
    Gpu_Object,
    "UprSizeList",
    0,
    0,
    0,
    0,
    0,
    "A list of sizes for regions of confidential computing unprotected memory."
);

P_( Gpu_Set_OnlyPciDev );
P_( Gpu_Get_OnlyPciDev );
static SProperty OnlyPciDev
(
    Gpu_Object,
    "OnlyPciDev",
    0,
    0,
    Gpu_Get_OnlyPciDev,
    Gpu_Set_OnlyPciDev,
    0,
    "Make RM see only Nth supported GPU found on the PCI bus"
);

P_( Gpu_Set_OnlyPciDev )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsArray array;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &array))
    {
        JS_ReportError(pContext, "Failed to colwert input value");
        return JS_FALSE;
    }

    set<UINT64> devices;
    for (size_t i=0; i < array.size(); i++)
    {
        JSString *devStr;
        string dev;
        if (OK != JavaScriptPtr()->FromJsval(array[i], &devStr))
        {
            JS_ReportError(pContext, "Failed to colwert input value");
            return JS_FALSE;
        }
        dev = DeflateJSString(pContext, devStr);

        vector<string> args;
        if (OK != Utility::Tokenizer(dev, ":.", &args))
        {
            JS_ReportError(pContext, "Failed to tokenize only_pci_dev val: %s", dev.c_str());
            return JS_FALSE;
        }

        unsigned pciDomain = 0;
        unsigned pciBus = 0;
        unsigned pciDev = 0;
        unsigned pciFunc = 0;
        switch (args.size())
        {
            case 1:
                pciBus = Utility::ColwertStrToHex(args[0]);
                break;
            case 3:
                pciBus  = Utility::ColwertStrToHex(args[0]);
                pciDev  = Utility::ColwertStrToHex(args[1]);
                pciFunc = Utility::ColwertStrToHex(args[2]);
                break;
            case 4:
                pciDomain = Utility::ColwertStrToHex(args[0]);
                pciBus    = Utility::ColwertStrToHex(args[1]);
                pciDev    = Utility::ColwertStrToHex(args[2]);
                pciFunc   = Utility::ColwertStrToHex(args[3]);
                break;

            default:
                JS_ReportError(pContext, "Failed to colwert input value");
                return JS_FALSE;
        }
        if ((pciDomain >= 0x10000) || (pciBus >= 0x100) || (pciDev >= 0x20) || (pciFunc >= 8))
        {
            JS_ReportError(pContext, "Failed to colwert input value");
            return JS_FALSE;
        }
        devices.insert(((static_cast<UINT64>(pciDomain & 0xFFFF)) << 48) |
                       ((static_cast<UINT64>(pciBus    & 0xFFFF)) << 32) |
                       ((static_cast<UINT64>(pciDev    & 0xFFFF)) << 16) |
                        (static_cast<UINT64>(pciFunc   & 0xFFFF)));
    }

    GpuPtr()->SetAllowedPciDevices(devices);
    return JS_TRUE;
}

P_( Gpu_Get_OnlyPciDev )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    set<UINT64> devices;
    GpuPtr()->GetAllowedPciDevices(&devices);

    JsArray array;
    set<UINT64>::const_iterator devIt = devices.begin();
    for ( ; devIt != devices.end(); ++devIt)
    {
        const unsigned pciDomain = (*devIt >> 48) & 0xFFFF;
        const unsigned pciBus    = (*devIt >> 32) & 0xFF;
        const unsigned pciDev    = (*devIt >> 16) & 0x1F;
        const unsigned pciFunc   =  *devIt        & 0x7;
        char s[16];
        sprintf(s, "%04x:%x:%02x.%x", pciDomain, pciBus, pciDev, pciFunc);
        jsval val;
        if (JavaScriptPtr()->ToJsval(s, &val) != RC::OK)
        {
            JS_ReportError(pContext, "Failed to colwert output value");
            *pValue = JSVAL_NULL;
            return JS_FALSE;
        }
        array.push_back(val);
    }

    if (OK != JavaScriptPtr()->ToJsval(&array, pValue))
    {
        JS_ReportError(pContext, "Failed to colwert output value");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(Gpu_Set_GpuUnderTest);
P_(Gpu_Get_GpuUnderTest);
static SProperty GpuUnderTest
(
    Gpu_Object,
    "GpuUnderTest",
    0,
    0,
    Gpu_Get_GpuUnderTest,
    Gpu_Set_GpuUnderTest,
    0,
    "List the Gpus to be put under test"
);

P_(Gpu_Set_GpuUnderTest)
{
    MASSERT(pContext && pValue);

    JsArray array;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &array))
    {
        JS_ReportError(pContext, "Failed to colwert the input value");
        return JS_FALSE;
    }

    set<string> gpus;
    for (size_t i=0; i < array.size(); i++)
    {
        string tmp;
        if (OK != JavaScriptPtr()->FromJsval(array[i], &tmp))
        {
            JS_ReportError(pContext, "Failed to colwert the input value");
            return JS_FALSE;
        }
        gpus.insert(tmp);
    }
    GpuPtr()->SetGpusUnderTest(gpus);
    return JS_TRUE;
}

P_(Gpu_Get_GpuUnderTest)
{
    MASSERT(pContext && pValue);

    set<string> gpus;
    GpuPtr()->GetGpusUnderTest(&gpus);

    JsArray array;
    jsval val;

    set<string>::const_iterator gpusIter = gpus.begin();
    while(gpusIter != gpus.end())
    {
        string tmp = *gpusIter;
        if (OK != JavaScriptPtr()->ToJsval(tmp, &val))
        {
            JS_ReportError(pContext, "Failed to colwert output value");
            *pValue = JSVAL_NULL;
            return JS_FALSE;
        }
        array.push_back(val);
        gpusIter++;
    }

    if (OK != JavaScriptPtr()->ToJsval(&array, pValue))
    {
        JS_ReportError(pContext, "Failed to colwert output value");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(Gpu_Set_AllowedGpuDevId);
P_(Gpu_Get_AllowedGpuDevId);
static SProperty GpuDevId
(
    Gpu_Object,
    "AllowedGpuDevId",
    0,
    0,
    Gpu_Get_AllowedGpuDevId,
    Gpu_Set_AllowedGpuDevId,
    0,
    "List the Gpus Device Id to be put under test"
);

P_(Gpu_Set_AllowedGpuDevId)
{
    MASSERT(pContext && pValue);
    JsArray array;

    if (OK != JavaScriptPtr()->FromJsval(*pValue, &array))
    {
        JS_ReportError(pContext, "Failed to colwert the input value");
        return JS_FALSE;
    }

    set<UINT32> gpuDevIds;
    for (size_t i=0; i < array.size(); i++)
    {
        UINT32 tmp;
        if (OK != JavaScriptPtr()->FromJsval(array[i], &tmp))
        {
            JS_ReportError(pContext, "Failed to colwert the input value");
            return JS_FALSE;
        }

        gpuDevIds.insert(tmp);
    }

    GpuPtr()->SetAllowedDevIds(gpuDevIds);
    return JS_TRUE;
}

P_(Gpu_Get_AllowedGpuDevId)
{
    MASSERT(pContext && pValue);
    set<UINT32> gpuDevIds;
    GpuPtr()->GetAllowedDevIds(&gpuDevIds);

    JsArray array;
    jsval val;
    set<UINT32>::const_iterator gpusIter = gpuDevIds.begin();
    while(gpusIter != gpuDevIds.end())
    {
        UINT32 tmp = *gpusIter;
        if (OK != JavaScriptPtr()->ToJsval(tmp, &val))
        {
            JS_ReportError(pContext, "Failed to colwert output value");
            *pValue = JSVAL_NULL;
            return JS_FALSE;
        }

        array.push_back(val);
        gpusIter++;
    }

    if (OK != JavaScriptPtr()->ToJsval(&array, pValue))
    {
        JS_ReportError(pContext, "Failed to colwert output value");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(Gpu_CheckAssertLwrrent);
static STest Gpu_CheckAssertLwrrent
(
    Gpu_Object,
    "CheckAssertLwrrent",
    C_Gpu_CheckAssertLwrrent,
    0,
    "Checks for invalid uses of current device/subdevice."
);

// STest
C_(Gpu_CheckAssertLwrrent)
{
    static Deprecation depr("Gpu.CheckAssertLwrrent", "3/30/2019",
                            "CheckAssertLwrrent is no longer necessary");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    RETURN_RC(RC::OK);
}

//------------------------------------------------------------------------------
static JSBool SortPciAscendingGetter
(
    JSContext * cx,
    JSObject * obj,
    jsval id,
    jsval *vp
)
{
    MASSERT(vp);

    JavaScriptPtr pJs;
    UINT32 tmpUI = Gpu::s_SortPciAscending ? 1 : 0;
    pJs->ToJsval(tmpUI, vp);
    return JS_TRUE;
}
static JSBool SortPciAscendingSetter
(
    JSContext * cx,
    JSObject * obj,
    jsval id,
    jsval *vp
)
{
    MASSERT(cx);
    MASSERT(vp);

    UINT32 tmpUI;
    JavaScriptPtr()->FromJsval(*vp, &tmpUI);
    Gpu::s_SortPciAscending = (tmpUI != 0);
    return JS_TRUE;
}

static SProperty Gpu_SortPciAscending
(
    Gpu_Object,
    "SortPciAscending",
    0,
    Gpu::s_SortPciAscending,
    SortPciAscendingGetter,
    SortPciAscendingSetter,
    0,
    "If true, assign device number from PCI bus,slot low to high, else high to low."
);

// SMethod
C_(Gpu_ReadCrtc)
{
    static Deprecation depr("Gpu.ReadCrtc", "3/30/2019",
                            "Use GpuSubdevice.ReadCrtc instead\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    const char ErrorMessage[] = "Usage: Gpu.ReadCrtc(index, head = 0)";

    // Check the arguments.
    UINT32 Index;
    INT32  Head = 0; // default value
    if (((NumArguments != 1) && (NumArguments != 2))
        || (OK != pJavaScript->FromJsval(pArguments[0], &Index)))
    {
        JS_ReportError(pContext, ErrorMessage);
        return JS_FALSE;
    }

    if (2 == NumArguments)
    {
        if (OK != pJavaScript->FromJsval(pArguments[1], &Head))
        {
            JS_ReportError(pContext, ErrorMessage);
            return JS_FALSE;
        }
    }

    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    if (OK != pJavaScript->ToJsval(pSubdev->ReadCrtc(Index,Head), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Gpu.ReadCrtc()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// STest
C_(Gpu_WriteCrtc)
{
    static Deprecation depr("Gpu.WriteCrtc", "3/30/2019",
                            "Use GpuSubdevice.WriteCrtc instead\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    const char ErrorMessage[] = "Usage: Gpu.WriteCrtc(index, data, head = 0)";

    // Check the arguments.
    UINT32 Index;
    UINT32 Data;
    INT32  Head = 0; // default value
    if (((NumArguments != 2) && (NumArguments != 3))
        || (OK != pJavaScript->FromJsval(pArguments[0], &Index))
        || (OK != pJavaScript->FromJsval(pArguments[1], &Data)))
    {
        JS_ReportError(pContext, ErrorMessage);
        return JS_FALSE;
    }

    if (3 == NumArguments)
    {
        if (OK != pJavaScript->FromJsval(pArguments[2], &Head))
        {
            JS_ReportError(pContext, ErrorMessage);
            return JS_FALSE;
        }
    }

    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    pSubdev->WriteCrtc(Index, Data, Head);

    RETURN_RC(OK);
}

// SMethod
C_(Gpu_ReadSr)
{
    static Deprecation depr("Gpu.ReadSr", "3/30/2019",
                            "Use GpuSubdevice.ReadSr instead\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    const char ErrorMessage[] = "Usage: Gpu.ReadSr(index, head = 0)";

    // Check the arguments.
    UINT32 Index;
    INT32  Head = 0; // default value
    if (((NumArguments != 1) && (NumArguments != 2))
        || (OK != pJavaScript->FromJsval(pArguments[0], &Index)))
    {
        JS_ReportError(pContext, ErrorMessage);
        return JS_FALSE;
    }

    if (2 == NumArguments)
    {
        if (OK != pJavaScript->FromJsval(pArguments[1], &Head))
        {
            JS_ReportError(pContext, ErrorMessage);
            return JS_FALSE;
        }
    }

    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    if (OK != pJavaScript->ToJsval(pSubdev->ReadSr(Index,Head), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Gpu.ReadSr()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// STest
C_(Gpu_WriteSr)
{
    static Deprecation depr("Gpu.WriteSr", "3/30/2019",
                            "Use GpuSubdevice.WriteSr instead\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    const char ErrorMessage[] = "Usage: Gpu.WriteSr(index, data, head = 0)";

    // Check the arguments.
    UINT32 Index;
    UINT32 Data;
    INT32  Head = 0; // default value
    if (((NumArguments != 2) && (NumArguments != 3))
        || (OK != pJavaScript->FromJsval(pArguments[0], &Index))
        || (OK != pJavaScript->FromJsval(pArguments[1], &Data)))
    {
        JS_ReportError(pContext, ErrorMessage);
        return JS_FALSE;
    }

    if (3 == NumArguments)
    {
        if (OK != pJavaScript->FromJsval(pArguments[2], &Head))
        {
            JS_ReportError(pContext, ErrorMessage);
            return JS_FALSE;
        }
    }

    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    pSubdev->WriteSr(Index, Data, Head);

    RETURN_RC(OK);
}

// SMethod
C_(Gpu_ReadAr)
{
    static Deprecation depr("Gpu.ReadAr", "3/30/2019",
                            "Use GpuSubdevice.ReadAr instead\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    const char ErrorMessage[] = "Usage: Gpu.ReadAr(index, head = 0)";

    // Check the arguments.
    UINT32 Index;
    INT32  Head = 0; // default value
    if (((NumArguments != 1) && (NumArguments != 2))
        || (OK != pJavaScript->FromJsval(pArguments[0], &Index)))
    {
        JS_ReportError(pContext, ErrorMessage);
        return JS_FALSE;
    }

    if (2 == NumArguments)
    {
        if (OK != pJavaScript->FromJsval(pArguments[1], &Head))
        {
            JS_ReportError(pContext, ErrorMessage);
            return JS_FALSE;
        }
    }

    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    if (OK != pJavaScript->ToJsval(pSubdev->ReadAr(Index,Head), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Gpu.ReadAr()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// STest
C_(Gpu_WriteAr)
{
    static Deprecation depr("Gpu.WriteAr", "3/30/2019",
                            "Use GpuSubdevice.WriteAr instead\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    const char ErrorMessage[] = "Usage: Gpu.WriteAr(index, data, head = 0)";

    // Check the arguments.
    UINT32 Index;
    UINT32 Data;
    INT32  Head = 0; // default value
    if (((NumArguments != 2) && (NumArguments != 3))
        || (OK != pJavaScript->FromJsval(pArguments[0], &Index))
        || (OK != pJavaScript->FromJsval(pArguments[1], &Data)))
    {
        JS_ReportError(pContext, ErrorMessage);
        return JS_FALSE;
    }

    if (3 == NumArguments)
    {
        if (OK != pJavaScript->FromJsval(pArguments[2], &Head))
        {
            JS_ReportError(pContext, ErrorMessage);
            return JS_FALSE;
        }
    }

    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    pSubdev->WriteAr(Index, Data, Head);

    RETURN_RC(OK);
}

// SMethod
C_(Gpu_ReadGr)
{
    static Deprecation depr("Gpu.ReadGr", "3/30/2019",
                            "Use GpuSubdevice.ReadGr instead\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    const char ErrorMessage[] = "Usage: Gpu.ReadGr(index, head = 0)";

    // Check the arguments.
    UINT32 Index;
    INT32  Head = 0; // default value
    if (((NumArguments != 1) && (NumArguments != 2))
        || (OK != pJavaScript->FromJsval(pArguments[0], &Index)))
    {
        JS_ReportError(pContext, ErrorMessage);
        return JS_FALSE;
    }

    if (2 == NumArguments)
    {
        if (OK != pJavaScript->FromJsval(pArguments[1], &Head))
        {
            JS_ReportError(pContext, ErrorMessage);
            return JS_FALSE;
        }
    }

    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    if (OK != pJavaScript->ToJsval(pSubdev->ReadGr(Index,Head), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Gpu.ReadGr()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// STest
C_(Gpu_WriteGr)
{
    static Deprecation depr("Gpu.WriteGr", "3/30/2019",
                            "Use GpuSubdevice.WriteGr instead\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    const char ErrorMessage[] = "Usage: Gpu.WriteGr(index, data, head = 0)";

    // Check the arguments.
    UINT32 Index;
    UINT32 Data;
    INT32  Head = 0; // default value
    if (((NumArguments != 2) && (NumArguments != 3))
        || (OK != pJavaScript->FromJsval(pArguments[0], &Index))
        || (OK != pJavaScript->FromJsval(pArguments[1], &Data)))
    {
        JS_ReportError(pContext, ErrorMessage);
        return JS_FALSE;
    }

    if (3 == NumArguments)
    {
        if (OK != pJavaScript->FromJsval(pArguments[2], &Head))
        {
            JS_ReportError(pContext, ErrorMessage);
            return JS_FALSE;
        }
    }

    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    pSubdev->WriteGr(Index, Data, Head);

    RETURN_RC(OK);
}

//------------------------------------------------------------------------------
// Code for dealing with register write exclusion regions
//------------------------------------------------------------------------------
vector<REGISTER_RANGE> g_GpuWriteExclusionRegions;
C_(Gpu_WriteExclusionRegions)
{
    RC rc = OK;
    JavaScriptPtr pJs;

    // Check the arguments.
    JsArray OuterArray;
    if ((NumArguments != 1)
        || (OK != pJs->FromJsval(pArguments[0], &OuterArray)))
    {
        Printf(Tee::PriNormal,
               "Usage: Gpu.WriteExclusionRegions([{GpuInst1,start1,end1,msk1],"
               " {GpuInst2,start2,end2, msk2}, ...]);\n\n"
               "MODS includes a function called Gpu.WriteExclusionRegions that allows you\n"
               "to limit the registers that MODS and the resource manager will write.\n"
               "To use this feature, pass an array of structures.  Each three element\n"
               "structure contains the starting and ending address of the ranges"
               " to exclude, the bitmask and the Gpu Instance.\n"
               "   Gpu.WriteExclusionRegions([{GpuInst1,start1,end1,Bitmask}, "
               "{GpuInst2,start2,end2,Bitmask2}, ...});\n"
               "This simple example will keep the LW_PBUS_POWERCTRL_0 register from being\n"
               "written:\n"
               "Here's an example that will keep LW_PBUS_POWERCTRL_0 and all LW_PFB_*\n"
               "registers from being written.\n"
               "Gpu.WriteExclusionRegions({0,0x1580,0x1580,0}, {0,0x100200,0x100FFC,0}]);\n"
               "To disable this feature, just pass in a null array:\n"
               "Gpu.WriteExclusionRegions([]);\n"
               "This function can be called at any time, including before Gpu init.  If\n"
               "you want your exclusion region to be in effect when the Gpu initializes,\n"
               "the best thing to do is just put your call to Gpu.WriteExclusionRegions()\n"
               "at the very top of mods.js or mfgtest.js, right above the file's copyright\n"
               "notice.\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    size_t NumRanges = OuterArray.size();
    UINT32 i;

    // If the user just passed in a null array, turn off all regions.
    if(0 == NumRanges)
    {
        Printf(Tee::PriLow,"Turning off register exclusion regions.\n");
        g_RegWrMask.clear();
        RETURN_RC(OK);
    }

    UINT32 Addr1, Addr2, GpuInst, Bitmask;
    vector<RegMaskRange> NewRegions;
    RegMaskRange NewRange;

    // Parse the two-dimensional array.
    for (i = 0; i < NumRanges; i++)
    {
        JSObject * pRegMaskObj;

        C_CHECK_RC(pJs->FromJsval(OuterArray[i], &pRegMaskObj));
        if ((OK != pJs->GetProperty(pRegMaskObj, "Addr1", &Addr1)
            || (OK != pJs->GetProperty(pRegMaskObj, "Addr2", &Addr2)))
            || (OK != pJs->GetProperty(pRegMaskObj, "Bitmask", &Bitmask))
            || (OK != pJs->GetProperty(pRegMaskObj, "GpuInst", &GpuInst)))
        {
            Printf(Tee::PriNormal, "WriteExclusionRegions(): Couldn't "
                                   "translate element %d into a two-element array\n",i);

            RETURN_RC(RC::BAD_PARAMETER);
        }
        NewRange.SetValues(GpuInst, Addr1, Addr2, Bitmask);
        NewRegions.push_back(NewRange);
    }

    // Now that everything is OK, clear the original contents and copy new ones.
    g_RegWrMask.clear();
    g_RegWrMask = NewRegions;

    RETURN_RC(OK);
}

C_(Gpu_SetHasBugOverride)
{
    JavaScriptPtr pJs;
    UINT32 DevInst;
    UINT32 BugNum;
    bool   HasBug;
    if ((NumArguments != 3) ||
        (OK != pJs->FromJsval(pArguments[0], &DevInst)) ||
        (OK != pJs->FromJsval(pArguments[1], &BugNum)) ||
        (OK != pJs->FromJsval(pArguments[2], &HasBug)))
    {
        Printf(Tee::PriNormal,
               "Usage: Gpu.SetHasBugOverride(DevInst, BugNum, HasBug))\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    GpuPtr()->SetHasBugOverride(DevInst, BugNum, HasBug);
    RETURN_RC(OK);
}

//------------------------------------------------------------------------------
// Code for dealing with remapping register addresses
//------------------------------------------------------------------------------
C_(Gpu_RemapRegister)
{
    RC rc;
    JavaScriptPtr pJs;
    UINT32 fromAddr = 0; // current register address defined in the hwref manuals
    UINT32 toAddr = 0;   // old register address being used by a specific netlist
    if ((NumArguments != 2)
        || RC::OK != pJs->FromJsval(pArguments[0], &fromAddr)
        || RC::OK != pJs->FromJsval(pArguments[1], &toAddr))
    {
        Printf(Tee::PriNormal, "Usage: Gpu.RemapRegs(fromAddr, toAddr).\n");
        return JS_FALSE;
    }
    if (Gpu::GetRemappedReg(toAddr) != toAddr)
    {
        Printf(Tee::PriNormal, 
               "Gpu.RemapRegs: Multiple indirection is not allowed, "
               "toAddr:%08x is already remapped to:%08x.\n",
               toAddr, Gpu::GetRemappedReg(toAddr));
        RETURN_RC(RC::BAD_COMMAND_LINE_ARGUMENT);
    }
    RETURN_RC(Gpu::RemapReg(fromAddr, toAddr));
}

//------------------------------------------------------------------------------
// Code for dealing with logging register accesses
//------------------------------------------------------------------------------
vector<REGISTER_RANGE> g_GpuLogAccessRegions;

C_(Gpu_LogAccessRegions)
{
    RC rc = OK;
    JavaScriptPtr pJs;

    // Check the arguments.
    JsArray OuterArray;
    if ((NumArguments != 1)
        || (OK != pJs->FromJsval(pArguments[0], &OuterArray)))
    {
        Printf(Tee::PriNormal,
               "Usage: Gpu.LogAccessRegions([[start1,end1], [start2,end2], ...]);\n\n"
               "MODS includes a function called Gpu.LogAccessRegions that allows you\n"
               "to log accesses to registers that MODS and the resource manager will read/write.\n"
               "To use this feature, pass an array of two-element arrays.  Each two-element\n"
               "array contains the starting and ending address of the ranges to log.\n"
               "   Gpu.LogAccessRegions([[start1,end1], [start2,end2], ...]);\n"
               "This simple example will log read/write accesses to LW_PBUS_POWERCTRL_0 register:\n"
               "   Gpu.LogAccessRegions([[0x1580,0x1580]]);\n"
               "To disable this feature, just pass in a null array:\n"
               "   Gpu.LogAccessRegions([]);\n"
               "This function can be called at any time, including before Gpu init.  If\n"
               "you want your register access logging to be in effect when the Gpu initializes,\n"
               "the best thing to do is just put your call to Gpu.LogAccessRegions()\n"
               "at the very top of mods.js or mfgtest.js, right above the file's copyright\n"
               "notice.\n");

        return JS_FALSE;
    }

    size_t NumRanges = OuterArray.size();
    UINT32 i;

    // If the user just passed in a null array, turn off all regions.
    if(0 == NumRanges)
    {
        Printf(Tee::PriLow,"Turning off register exclusion regions.\n");
        g_GpuLogAccessRegions.clear();
        RETURN_RC(OK);
    }

    vector<REGISTER_RANGE> NewRegions;
    REGISTER_RANGE NewRange;

    // Parse the two-dimensional array.
    for (i = 0; i < NumRanges; i++)
    {
        JsArray InnerArray;
        if (OK != (rc = pJs->FromJsval(OuterArray[i], &InnerArray)))
        {
            RETURN_RC(rc);
        }

        if ((2 != InnerArray.size())
            ||  (OK != pJs->FromJsval(InnerArray[0], &(NewRange.Start)))
            ||  (OK != pJs->FromJsval(InnerArray[1], &(NewRange.End))))
        {
            Printf(Tee::PriNormal, "WriteExclusionRegions(): Couldn't "
                                   "translate element %d into a two-element array\n",i);

            RETURN_RC(RC::BAD_PARAMETER);
        }

        NewRegions.push_back(NewRange);
    }

    // Now that everything is OK, clear the original contents and copy new ones.
    g_GpuLogAccessRegions.clear();

    g_GpuLogAccessRegions = NewRegions;

    vector<REGISTER_RANGE>::iterator rangeIter = g_GpuLogAccessRegions.begin();

    Printf(Tee::PriLow,"Register access log regions:\n");
    while (rangeIter != g_GpuLogAccessRegions.end())
    {
        Printf(Tee::PriLow,"  %06x to %06x\n", rangeIter->Start, rangeIter->End);
        rangeIter ++;
    }

    RETURN_RC(OK);
}

C_(Gpu_SetVPLLLockDelay);
static SMethod Gpu_SetVPLLLockDelay
(
    Gpu_Object,
    "SetVPLLLockDelay",
    C_Gpu_SetVPLLLockDelay,
    1,
    "Set VPLLLockDelay ."
);

C_(Gpu_GetVPLLLockDelay);
static SMethod Gpu_GetVPLLLockDelay
(
    Gpu_Object,
    "GetVPLLLockDelay",
    C_Gpu_GetVPLLLockDelay,
    0,
    "Return value of VPLLLockDelay."
);

C_(Gpu_SetVPLLLockDelay)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;
    GpuPtr pGpu;
    UINT32 Delay;

    if ((NumArguments != 1)
        || (OK != pJs->FromJsval(pArguments[0], &Delay)))
    {
        JS_ReportError(pContext, "Usage: Gpu.SetVPLLLockDelay(Delay)");
        return JS_FALSE;
    }

    RETURN_RC(pGpu->SetVPLLLockDelay(Delay));
}

C_(Gpu_GetVPLLLockDelay)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;
    GpuPtr pGpu;
    UINT32 Delay;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Gpu.GetVPLLLockDelay()");
        return JS_FALSE;
    }

    pGpu->GetVPLLLockDelay(&Delay);

    if (OK != pJs->ToJsval(Delay, pReturlwalue))
    {
        JS_ReportError(pContext, "Usage: Gpu.GetVPLLLockDelay()");
        return JS_FALSE;
    }

    return JS_TRUE;
}

C_(Gpu_ClkSrcToString);
static SMethod Gpu_ClkSrcToString
(
    Gpu_Object,
    "ClkSrcToString",
    C_Gpu_ClkSrcToString,
    1,
    "Return a string based on a clk source enum value."
);
C_(Gpu_ClkSrcToString)
{
    JavaScriptPtr pJs;
    UINT32 src;

    // Check the arguments.
    if ((NumArguments != 1)
        || (OK != pJs->FromJsval(pArguments[0], &src)))
    {
        JS_ReportError(pContext, "Usage: Gpu.ClkSrcToString(src_enum_val)");
        return JS_FALSE;
    }

    const char * s = Gpu::ClkSrcToString(Gpu::ClkSrc(src));

    if (pJs->ToJsval(s, pReturlwalue) != RC::OK)
    {
        JS_ReportError(pContext, "Error oclwrred in Gpu.ClkSrcToString()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(Gpu_ClkDomainToString);
static SMethod Gpu_ClkDomainToString
(
    Gpu_Object,
    "ClkDomainToString",
    C_Gpu_ClkDomainToString,
    1,
    "Return a string based on a Gpu.ClkXXX enum value."
);
C_(Gpu_ClkDomainToString)
{
    JavaScriptPtr pJs;
    UINT32 dom;

    // Check the arguments.
    if ((NumArguments != 1)
        || (OK != pJs->FromJsval(pArguments[0], &dom)))
    {
        JS_ReportError(pContext, "Usage: Gpu.ClkDomainToString(enum_val)");
        return JS_FALSE;
    }

    const char * s = PerfUtil::ClkDomainToStr(Gpu::ClkDomain(dom));

    if (pJs->ToJsval(s, pReturlwalue) != RC::OK)
    {
        JS_ReportError(pContext, "Error oclwrred in Gpu.ClkDomainToString()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(Gpu, RegimeToString, 1,
                  "Return the corresponding string for a regime")
{
    STEST_HEADER(1, 1, "Usage: var regimeStr = Gpu.RegimeToString(regime)");
    STEST_ARG(0, INT32, regimeId);
    Perf::RegimeSetting regime = static_cast<Perf::RegimeSetting>(regimeId);
    const char* regimeStr = PerfUtil::RegimeSettingToStr(regime);

    if (pJavaScript->ToJsval(regimeStr, pReturlwalue) != RC::OK)
    {
        JS_ReportError(pContext, "Error oclwrred in Gpu.RegimeToString");
        return JS_FALSE;
    }

    return JS_TRUE;
}
C_(Gpu_DeviceIdToString);
static SMethod Gpu_DeviceIdToString
(
    Gpu_Object,
    "DeviceIdToString",
    C_Gpu_DeviceIdToString,
    1,
    "Return a string based on a DeviceId enum value."
);
C_(Gpu_DeviceIdToString)
{
    JavaScriptPtr pJs;
    UINT32 did;

    // Check the arguments.
    if ((NumArguments != 1)
        || (OK != pJs->FromJsval(pArguments[0], &did)))
    {
        JS_ReportError(pContext, "Usage: Gpu.DeviceIdToString(did_enum_val)");
        return JS_FALSE;
    }

    string s = Gpu::DeviceIdToString(Gpu::LwDeviceId(did));

    if (OK != pJs->ToJsval(s, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Gpu.DeviceIdToString()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(Gpu_DeviceIdToInternalString);
static SMethod Gpu_DeviceIdToInternalString
(
    Gpu_Object,
    "DeviceIdToInternalString",
    C_Gpu_DeviceIdToInternalString,
    1,
    "Return an internal device name based on a DeviceId enum value."
);
C_(Gpu_DeviceIdToInternalString)
{
    JavaScriptPtr pJs;
    UINT32 did;

    // Check the arguments.
    if ((NumArguments != 1)
        || (OK != pJs->FromJsval(pArguments[0], &did)))
    {
        JS_ReportError(pContext, "Usage: Gpu.DeviceIdToInternalString(did_enum_val)");
        return JS_FALSE;
    }

    string s = Gpu::DeviceIdToInternalString(Gpu::LwDeviceId(did));

    if (OK != pJs->ToJsval(s, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Gpu.DeviceIdToInternalString()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(Gpu_DeviceIdToExternalString);
static SMethod Gpu_DeviceIdToExternalString
(
    Gpu_Object,
    "DeviceIdToExternalString",
    C_Gpu_DeviceIdToExternalString,
    1,
    "Return an external device name based on a DeviceId enum value."
);
C_(Gpu_DeviceIdToExternalString)
{
    JavaScriptPtr pJs;
    UINT32 did;

    // Check the arguments.
    if ((NumArguments != 1)
        || (OK != pJs->FromJsval(pArguments[0], &did)))
    {
        JS_ReportError(pContext, "Usage: Gpu.DeviceIdToExternalString(did_enum_val)");
        return JS_FALSE;
    }

    string s = Gpu::DeviceIdToExternalString(Gpu::LwDeviceId(did));

    if (OK != pJs->ToJsval(s, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Gpu.DeviceIdToExternalString()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(Gpu_GetPteKindFromName);
static SMethod Gpu_GetPteKindFromName
(
    Gpu_Object,
    "GetPteKindFromName",
    C_Gpu_GetPteKindFromName,
    1,
    "Get the PTE Kind from name"
);

C_(Gpu_GetPteKindFromName)
{
    static Deprecation depr("Gpu.GetPteKindFromName", "3/30/2019",
                            "Use GpuSubdevice.GetPteKindFromName() instead\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    JavaScriptPtr pJavaScript;

    string sName;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &sName)))
    {
        JS_ReportError(pContext, "Usage: Gpu.GetPteKindFromName(PteKindName)\n");
        return JS_FALSE;
    }

    UINT32 PteKind;
    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    bool rv = pSubdev->GetPteKindFromName(sName.c_str(),&PteKind);
    if(!rv)
    {
        JS_ReportError(pContext, "ERROR: GpuPtr()->GetPteKindFromName() cannot"
        " find kind, \"%s\".\n",sName.c_str());
        return JS_FALSE;
    }

    if (OK != pJavaScript->ToJsval(PteKind, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Gpu.ReadCrtc()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

C_(Gpu_SplitVoltDomainToStr);
static SMethod Gpu_SplitVoltDomainToStr
(
    Gpu_Object,
    "SplitVoltDomainToStr",
    C_Gpu_SplitVoltDomainToStr,
    1,
    "Return a string based on a Gpu.VOLTAGE_XXX enum value."
);
C_(Gpu_SplitVoltDomainToStr)
{
    JavaScriptPtr pJs;
    UINT32 dom;

    // Check the arguments.
    if ((NumArguments != 1)
        || (OK != pJs->FromJsval(pArguments[0], &dom)))
    {
        JS_ReportError(pContext, "Usage: Gpu.SplitVoltDomainToStr(enum_val)");
        return JS_FALSE;
    }

    const char * s = PerfUtil::SplitVoltDomainToStr(Gpu::SplitVoltageDomain(dom));

    if (pJs->ToJsval(s, pReturlwalue) != RC::OK)
    {
        JS_ReportError(pContext, "Error oclwrred in Gpu.SplitVoltDomainToStr()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(Gpu_ThreadFile);
static STest Gpu_ThreadFile
(
    Gpu_Object,
    "ThreadFile",
    C_Gpu_ThreadFile,
    0,
    "Set file name with a thread function"
);

C_(Gpu_ThreadFile)
{
    JavaScriptPtr pJavaScript;

    string FileName;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FileName)))
    {
        JS_ReportError(pContext, "Usage: Gpu.Thread(FileName)\n");
        return JS_FALSE;
    }

    RETURN_RC(GpuPtr()->ThreadFile(FileName));
}

C_(Gpu_SetDebugState);
static STest Gpu_SetDebugState
(
    Gpu_Object,
    "SetDebugState",
    C_Gpu_SetDebugState,
    2,
    "Set the specified GPU debug feature to specified value"
);

C_(Gpu_SetDebugState)
{
    static Deprecation depr("Gpu.SetDebugState", "3/30/2019",
                            "Use GpuSubdevice.SetDebugState\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    JavaScriptPtr pJs;
    string name;
    UINT32 val;

    if ((NumArguments != 2) || (OK != pJs->FromJsval(pArguments[0], &name))
        || (OK != pJs->FromJsval(pArguments[1], &val )))
    {
        JS_ReportError(pContext, "Usage: Gpu.SetDebugState(name, value)");
        return JS_FALSE;
    }

    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    RETURN_RC(pSubdev->SetDebugState(name, val));
}

JS_STEST_BIND_ONE_ARG_NAMESPACE(Gpu, IgnoreFamily, string, Family, "Ignore gpus in this family");
JS_STEST_BIND_ONE_ARG_NAMESPACE(Gpu, OnlyFamily,   string, Family, "Ignore gpus not in this family");

PROP_READWRITE(Gpu, GpuPtr(), RequireChannelEngineId, bool,
               "Require channel allocations to specify engine ID");

//------------------------------------------------------------------------------
// HULK related properties and functions

SProperty Gpu_HulkDisableFeatures
(
    Gpu_Object,
    "HulkDisableFeatures",
    0,
    0,
    0,
    0,
    0,
    "Suicide bits to disable features for HULK"
);

SProperty Gpu_HulkIgnoreErrors
(
    Gpu_Object,
    "HulkIgnoreErrors",
    0,
    false,
    0,
    0,
    0,
    "Ignore HULK binary errors"
);

SProperty Gpu_LoadHulkViaMods
(
    Gpu_Object,
    "LoadHulkViaMods",
    0,
    false,
    0,
    0,
    0,
    "Load HULK via MODS instead of RM"
);

SProperty Gpu_SkipHulkIfRevoked
(
    Gpu_Object,
    "SkipHulkIfRevoked",
    0,
    false,
    0,
    0,
    0,
    "Skip loading HULK (ECID / DEVID) if DEVID HULKs are revoked"
);

JS_SMETHOD_LWSTOM(Gpu,
                  SetHulkCert,
                  1,
                  "Set the HULK license binary")
{
    STEST_HEADER(2, 2, "Usage: Gpu.SetHulkCertificate(binary, length)");
    STEST_ARG(0, JsArray, JsBinary);
    STEST_ARG(1, UINT32,  Length);

    vector<UINT08> hulk(Length);
    for (UINT32 i = 0; i < Length; i++)
    {
        UINT08 byte;
        if (pJavaScript->FromJsval(JsBinary[i], &byte) != RC::OK)
        {
            return JS_FALSE;
        }
        hulk[i] = byte;
    }
    GpuPtr()->SetHulkCert(hulk);
    return JS_TRUE;
}


CLASS_PROP_READWRITE_LWSTOM_FULL
(
    Gpu,
    StrapFb,
    "Set the size of the framebuffer BAR in megabytes.",
    0,
    0
);

P_(Gpu_Get_StrapFb)
{
    UINT64 strapFb = GpuPtr()->GetStrapFb();

    JsUINT64* pJsUINT64 = new JsUINT64(strapFb);
    MASSERT(pJsUINT64);
    RC rc;

    JavaScriptPtr pJs;
    if ((rc = pJsUINT64->CreateJSObject(pContext)) != RC::OK ||
        (rc = pJs->ToJsval(pJsUINT64->GetJSObject(), pValue)) != RC::OK)
    {
        pJs->Throw(pContext,
                   rc,
                   "Error oclwrred in StrapFb: Unable to create return value");
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(Gpu_Set_StrapFb)
{
    JavaScriptPtr pJs;
    JSObject * pJsoStrapFb;
    RC rc;
    if (RC::OK != (rc = JavaScriptPtr()->FromJsval(*pValue, &pJsoStrapFb)))
    {
        pJs->Throw(pContext,
                   rc,
                   "Error oclwrred in StrapFb: Unable to parse jsval to set");
        return JS_FALSE;
    }

    JsUINT64* pJsStrapFb = static_cast<JsUINT64*>(GetPrivateAndComplainIfNull(pContext,
                                                                              pJsoStrapFb,
                                                                              "JsUINT64",
                                                                              "UINT64"));
    if (!pJsStrapFb)
    {
        pJs->Throw(pContext,
                   RC::CANNOT_COLWERT_JSVAL_TO_OJBECT,
                   "Error oclwrred in StrapFb: JsUINT64 value not found");
        return JS_FALSE;
    }

    GpuPtr()->SetStrapFb(pJsStrapFb->GetValue());
    return JS_TRUE;
}

PROP_READWRITE(Gpu, GpuPtr(), UsePlatformFLR, bool,
               "Use the platform implementation for FLR rather than MODS");

PROP_READWRITE(Gpu, GpuPtr(), GenerateSweepJsonArg, bool,
    "Generate and dump a sweep.json with the final command line floorsweeping options.");

PROP_READWRITE(Gpu, GpuPtr(), HulkXmlCertArg, string, "Handle HULK license verification.");