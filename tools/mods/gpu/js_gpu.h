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

#ifndef INCLUDED_JS_GPU_H
#define INCLUDED_JS_GPU_H

extern SObject Gpu_Object;
extern SProperty Gpu_SkipConfigSim;
extern SProperty Gpu_StrapOpenGL;
extern SProperty Gpu_Boot0Strap;
extern SProperty Gpu_Boot3Strap;
extern SProperty Gpu_StrapRamcfg;
extern SProperty Gpu_StrapGc6;
extern SProperty Gpu_FbRandomBankSwizzle;
extern SProperty Gpu_FbBankMapPattern;
extern SProperty Gpu_FbCmdMapping;
extern SProperty Gpu_FbBankSize;
extern SProperty Gpu_FbBanks;
extern SProperty Gpu_FbBurstLength;
extern SProperty Gpu_FbColumnBits;
extern SProperty Gpu_FbExtBank;
extern SProperty Gpu_FbRowBits;
extern SProperty Gpu_PerFbpRowBits;
extern SProperty Gpu_FbEnableChannel32;
extern SProperty Gpu_RopEnableMask;
extern SProperty Gpu_VpeEnableMask;
extern SProperty Gpu_ShdEnableMask;
extern SProperty Gpu_SmEnableMaskTpc0;
extern SProperty Gpu_SmEnableMaskTpc1;
extern SProperty Gpu_SmEnableMaskTpc2;
extern SProperty Gpu_SmEnableMaskTpc3;
extern SProperty Gpu_ZlwllEnableMask;
extern SProperty Gpu_SkipDevInit;
extern SProperty Gpu_SkipRmStateInit;
extern SProperty Gpu_StopInitAfterHulk;
extern SProperty Gpu_DramclkDevInitWARkHz;
extern SProperty Gpu_DisableEcc;
extern SProperty Gpu_EnableEcc;
extern SProperty Gpu_DisableDbi;
extern SProperty Gpu_EnableDbi;
extern SProperty Gpu_SkipInfoRomRowRemapping;
extern SProperty Gpu_DisableRowRemapper;
extern SProperty Gpu_Reset;
extern SProperty Gpu_IsFusing;
extern SProperty Gpu_ForceNoDevices;
extern SProperty Gpu_ReverseLVDSTMDS;
extern SProperty Gpu_PreInitCallback;
extern SProperty Gpu_PostInitCallback;
extern SProperty Gpu_IgnoreExtPower;
extern SProperty Gpu_PollInterrupts;
extern SProperty Gpu_AutoInterrupts;
extern SProperty Gpu_IntaInterrupts;
extern SProperty Gpu_MsiInterrupts;
extern SProperty Gpu_MsixInterrupts;
extern SProperty Gpu_SkipIntaIntrCheck;
extern SProperty Gpu_SkipMsiIntrCheck;
extern SProperty Gpu_SkipMsixIntrCheck;
extern SProperty Gpu_MonitorIntervalMs;
extern SProperty Gpu_MonitorFilter;
extern SProperty Gpu_ThreadIntervalMs;
extern SProperty Gpu_EndOfSimAssert;
extern SProperty Gpu_FenceSyncBeforeEos;
extern SProperty Gpu_DisableFenceSyncBeforeEos;
extern SProperty Gpu_ForceRepost;
extern SProperty Gpu_SkipVbiosPost;
extern SProperty Gpu_FilterBug599380Exceptions;
extern SProperty Gpu_UseDisplayID;
extern SProperty Gpu_ResetExceptions;
extern SProperty Gpu_CheckEdcErrors;
extern SProperty Gpu_SkipRestoreDisplay;
extern SProperty Gpu_CheckXenCompatibility;
extern SProperty Gpu_FbHbmDualRankEnabled;
extern SProperty Gpu_FbHbmNumSids;
extern SProperty Gpu_FbRowAMaxCount;
extern SProperty Gpu_FbRowBMaxCount;
extern SProperty Gpu_FbHbmBanksPerRank;
extern SProperty Gpu_FbRefsbDualRequest;
extern SProperty Gpu_FbClamshell;
extern SProperty Gpu_FbBanksPerRowBg;
extern SProperty Gpu_HbmDeviceInitMethod;
extern SProperty Gpu_VprCya0;
extern SProperty Gpu_VprCya1;
extern SProperty Gpu_Acr1Size;
extern SProperty Gpu_Acr1ReadMask;
extern SProperty Gpu_Acr1WriteMask;
extern SProperty Gpu_Acr2Size;
extern SProperty Gpu_Acr2ReadMask;
extern SProperty Gpu_Acr2WriteMask;
extern SProperty Gpu_VprStartAddrMb;
extern SProperty Gpu_Acr1StartAddr;
extern SProperty Gpu_Acr2StartAddr;
extern SProperty Gpu_UprStartAddrList;
extern SProperty Gpu_UprSizeList;
extern SProperty Gpu_HulkDisableFeatures;
extern SProperty Gpu_HulkIgnoreErrors;
extern SProperty Gpu_LoadHulkViaMods;
extern SProperty Gpu_SkipHulkIfRevoked;
extern SProperty Gpu_HbmCfgCheckIgnore;
extern SProperty Gpu_DisableFspSupport;

#endif // INCLUDED_JS_GPU_H
