/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  drvexport.cpp
 * @brief Dummy function linked by MODS so that the symbols are exported
 *
 * Please add new MODS functions to drvexport if they need to be visible
 */

#include "core/include/modsdrv.h"
#include "core/include/tracy.h"

void CallAllModsDrv(va_list args)
{
    PROFILE_ZONE_BEGIN(GENERIC)
    PROFILE_ZONE_END()
    ModsDrvLoadLibrary(NULL, 0);
    ModsDrvLwGetExportTable(0, 0);
    ModsDrvInterpretModsRC(0);
    ModsDrvBreakPoint(NULL,0);
    ModsDrvBreakPointWithReason(0,NULL,0);
    ModsDrvBreakPointWithErrorInfo(0, NULL, 0, NULL);
    ModsDrvPrintString(0, NULL);
    ModsDrvIsrPrintString(0, NULL);
    ModsDrvFlushLogFile();
    ModsDrvPrintf(0, NULL);
    ModsDrvVPrintf(0, NULL, args);
    ModsDrvSetGpuId(0);
    ModsDrvSetSwitchId(0);
    ModsDrvLogError(NULL);
    ModsDrvLogInfo(NULL);
    ModsDrvFlushLog();
    ModsDrvAlloc(0);
    ModsDrvCalloc(0, 0);
    ModsDrvRealloc(NULL, 0);
    ModsDrvFree(NULL);
    ModsDrvAlignedMalloc(0, 0, 0);
    ModsDrvAlignedFree(NULL);
    ModsDrvExecMalloc(0);
    ModsDrvExecFree(NULL, 0);
    ModsDrvCreateThread(NULL, NULL, 0, NULL);
    ModsDrvJoinThread(0);
    ModsDrvGetLwrrentThreadId();
    ModsDrvExitThread();
    ModsDrvYield();
    ModsDrvTlsAlloc(NULL);
    ModsDrvTlsFree(0);
    ModsDrvTlsGet(0);
    ModsDrvTlsSet(0, 0);
    ModsDrvAllocMutex();
    ModsDrvFreeMutex(NULL);
    ModsDrvAcquireMutex(NULL);
    ModsDrvTryAcquireMutex(NULL);
    ModsDrvReleaseMutex(NULL);
    ModsDrvIsMutexOwner(NULL);
    ModsDrvAllocCondition();
    ModsDrvFreeCondition(NULL);
    ModsDrvWaitCondition(NULL, NULL);
    ModsDrvWaitConditionTimeout(NULL, NULL, 0);
    ModsDrvSignalCondition(NULL);
    ModsDrvBroadcastCondition(NULL);
    ModsDrvAllocEvent(NULL);
    ModsDrvAllocEventAutoReset(NULL);
    ModsDrvAllocSystemEvent(NULL);
    ModsDrvFreeEvent(NULL);
    ModsDrvResetEvent(NULL);
    ModsDrvSetEvent(NULL);
    ModsDrvWaitOnEvent(NULL, 0);
    ModsDrvWaitOnMultipleEvents(NULL, 0, NULL, 0, NULL, 0);
    ModsDrvHandleResmanEvent(0, 0);
    ModsDrvGetOsEvent(NULL, 0, 0);
    ModsDrvCreateSemaphore(0, NULL);
    ModsDrvDestroySemaphore(NULL);
    ModsDrvAcquireSemaphore(NULL, 0);
    ModsDrvReleaseSemaphore(NULL);
    ModsDrvAllocSpinLock();
    ModsDrvDestroySpinLock(NULL);
    ModsDrvAcquireSpinLock(NULL);
    ModsDrvReleaseSpinLock(NULL);
    ModsDrvAtomicRead(NULL);
    ModsDrvAtomicWrite(NULL, 0);
    ModsDrvAtomicXchg32(NULL, 0);
    ModsDrvAtomicCmpXchg32(NULL, 0, 0);
    ModsDrvAtomicCmpXchg64(NULL, 0, 0);
    ModsDrvAtomicAdd(NULL, 0);
    ModsDrvFlushCpuCache();
    ModsDrvFlushCpuCacheRange(NULL, NULL, 0);
    ModsDrvFlushCpuWriteCombineBuffer();
    ModsDrvFindPciDevice(0, 0, 0, NULL, NULL, NULL, NULL);
    ModsDrvFindPciClassCode(0, 0, NULL, NULL, NULL, NULL);
    ModsDrvPciInitHandle(0, 0, 0, 0, NULL, NULL);
    ModsDrvPciGetBarInfo(0, 0, 0, 0, 0, NULL, NULL);
    ModsDrvPciGetIRQ(0, 0, 0, 0, NULL);
    ModsDrvPciRd08(0, 0, 0, 0, 0);
    ModsDrvPciRd16(0, 0, 0, 0, 0);
    ModsDrvPciRd32(0, 0, 0, 0, 0);
    ModsDrvPciWr08(0, 0, 0, 0, 0, 0);
    ModsDrvPciWr16(0, 0, 0, 0, 0, 0);
    ModsDrvPciWr32(0, 0, 0, 0, 0, 0);
    ModsDrvPciEnableAtomics(0, 0, 0, 0, NULL, NULL);
    ModsDrvPciFlr(0, 0, 0, 0);
    ModsDrvIoRd08(0);
    ModsDrvIoRd16(0);
    ModsDrvIoRd32(0);
    ModsDrvIoWr08(0, 0);
    ModsDrvIoWr16(0, 0);
    ModsDrvIoWr32(0, 0);
    ModsDrvGetSOCDeviceAperture(0, 0, 0, 0);
    ModsDrvGetSOCDeviceApertureByName(0, 0, 0);
    ModsDrvGetPageSize();
    ModsDrvAllocPages(0, (MODS_BOOL)0, 0, 0, 0, 0);
    ModsDrvAllocPagesForPciDev(0, (MODS_BOOL)0, 0, 0, 0, 0, 0, 0);
    ModsDrvAllocPagesAligned(0, 0, 0, 0, 0, 0);
    ModsDrvFreePages(NULL);
    ModsDrvMapPages(NULL, 0, 0, 0);
    ModsDrvUnMapPages(NULL);
    ModsDrvGetPhysicalAddress(NULL, 0);
    ModsDrvGetMappedPhysicalAddress(0, 0, 0, 0, NULL, 0);
    ModsDrvSetDmaMask(0, 0, 0, 0, 0);
    ModsDrvDmaMapMemory(0, 0, 0, 0, NULL);
    ModsDrvDmaUnmapMemory(0, 0, 0, 0, NULL);
    ModsDrvReservePages(0, 0);
    ModsDrvMapDeviceMemory(0, 0, 0, 0);
    ModsDrvUnMapDeviceMemory(NULL);
    ModsDrvSetMemRange(0, 0, 0);
    ModsDrvUnSetMemRange(0, 0);
    ModsDrvSyncForDevice(0, 0, 0, 0, NULL, 0);
    ModsDrvSyncForCpu(0, 0, 0, 0, NULL, 0);
    ModsDrvGetCarveout(0, 0);
    ModsDrvGetVPR(0, 0);
    ModsDrvGetGenCarveout(NULL, NULL, ~0U, 0);
    ModsDrvCallACPIGeneric(0, 0, NULL, NULL, NULL);
    ModsDrvLWIFMethod(0, 0, NULL, 0, NULL, NULL, NULL);
    ModsDrvCpuIsCPUIDSupported();
    ModsDrvCpuCPUID(0, 0, NULL, NULL, NULL, NULL);
    ModsDrvReadRegistryDword(NULL, NULL, NULL);
    ModsDrvGpuReadRegistryDword(0, NULL, NULL, NULL);
    ModsDrvReadRegistryString(NULL, NULL, NULL, NULL);
    ModsDrvGpuReadRegistryString(0, NULL, NULL, NULL, NULL);
    ModsDrvReadRegistryBinary(NULL, NULL, NULL, NULL);
    ModsDrvGpuReadRegistryBinary(0, NULL, NULL, NULL, NULL);
    ModsDrvPackRegistry(NULL, NULL);
    ModsDrvPause();
    ModsDrvDelayUS(0);
    ModsDrvDelayNS(0);
    ModsDrvSleep(0);
    ModsDrvGetTimeNS();
    ModsDrvGetTimeUS();
    ModsDrvGetTimeMS();
    ModsDrvGetNTPTime(NULL, NULL);
    ModsDrvGetLwrrentIrql();
    ModsDrvRaiseIrql(0);
    ModsDrvLowerIrql(0);
    ModsDrvHookIRQ(0, NULL, NULL);
    ModsDrvUnhookIRQ(0, NULL, NULL);
    ModsDrvHookInt(NULL, NULL, NULL);
    ModsDrvUnhookInt(NULL, NULL, NULL);
    ModsDrvMemRd08(NULL);
    ModsDrvMemRd16(NULL);
    ModsDrvMemRd32(NULL);
    ModsDrvMemRd64(NULL);
    ModsDrvMemWr08(NULL, 0);
    ModsDrvMemWr16(NULL, 0);
    ModsDrvMemWr32(NULL, 0);
    ModsDrvMemWr64(NULL, 0);
    ModsDrvMemCopy(NULL, NULL, 0);
    ModsDrvMemSet(NULL, 0, 0);
    ModsDrvSimChiplibLoaded();
    ModsDrvClockSimulator();
    ModsDrvGetSimulatorTime();
    ModsDrvGetSimulationMode();
    ModsDrvExtMemAllocSupported();
    ModsDrvSimEscapeWrite(NULL, 0, 0, 0);
    ModsDrvSimEscapeRead(NULL, 0, 0, NULL);
    ModsDrvSimGpuEscapeWrite(0, NULL, 0, 0, 0);
    ModsDrvSimGpuEscapeRead(0, NULL, 0, 0, NULL);
    ModsDrvSimGpuEscapeWriteBuffer(0, NULL, 0, 0, 0);
    ModsDrvSimGpuEscapeReadBuffer(0, NULL, 0, 0, NULL);
    ModsDrvAmodAllocChannelDma(0, 0,0 ,0);
    ModsDrvAmodAllocChannelGpFifo(0, 0, 0, 0, 0, 0);
    ModsDrvAmodFreeChannel(0);
    ModsDrvAmodAllocContextDma(0, 0, 0, 0, 0, 0, 0);
    ModsDrvAmodAllocObject(0, 0, 0);
    ModsDrvAmodFreeObject(0, 0);
    ModsDrvGpuRegWriteExcluded(0, 0);
    ModsDrvRegRd32(0);
    ModsDrvRegWr32(0, 0);
    ModsDrvLogRegRd(0, 0, 0, 0);
    ModsDrvLogRegWr(0, 0, 0, 0);
    ModsDrvGpuPreVBIOSSetup(0);
    ModsDrvGpuPostVBIOSSetup(0);
    ModsDrvGetVbiosFromLws(0);
    ModsDrvAddGpuSubdevice(0);
    ModsDrvGpuDisplaySelected();
    ModsDrvGpuGrIntrHook(0, 0, 0, 0, 0);
    ModsDrvGpuDispIntrHook(0, 0);
    ModsDrvGpuRcCheckCallback(0);
    ModsDrvGpuRcCallback(0, 0, 0, 0, 0, 0, NULL, NULL);
    ModsDrvGpuStopEngineScheduling(0, 0, MODS_FALSE);
    ModsDrvGpuCheckEngineScheduling(0, 0);
    ModsDrvHandleRmPolicyManagerEvent(0, 0);
    ModsDrvClockGetHandle(MODS_CLKDEV_UNKNOWN, 0, 0);
    ModsDrvClockEnable(0, MODS_FALSE);
    ModsDrvClockSetRate(0, 0);
    ModsDrvClockGetRate(0, 0);
    ModsDrvClockSetParent(0, 0);
    ModsDrvClockGetParent(0, 0);
    ModsDrvClockReset(0, MODS_FALSE);
    ModsDrvGpioEnable(0, MODS_FALSE);
    ModsDrvGpioRead(0, 0);
    ModsDrvGpioWrite(0, 0);
    ModsDrvGpioSetDirection(0, MODS_GPIO_DIR_INPUT);
    ModsDrvI2CRead(0, 0, 0, 0, 0, 0);
    ModsDrvI2CWrite(0, 0, 0, 0, 0, 0);
    ModsDrvTegraSetGpuVolt(0, NULL);
    ModsDrvTegraGetGpuVolt(NULL);
    ModsDrvTegraGetGpuVoltInfo(NULL);
    ModsDrvTegraGetChipInfo(NULL, NULL, NULL, NULL);
    ModsDrvGetFakeProcessID();
    ModsDrvGetFbConsoleInfo(NULL, NULL);
    ModsDrvSetupDmaBase(0, 0, 0, 0, 0, 0, NULL);
    ModsDrvGetGpuDmaBaseAddress(0);
    ModsDrvGetPpcTceBypassMode(0, NULL);
    ModsDrvGetAtsTargetAddressRange(0, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0);
    ModsDrvGetForcedLwLinkConnections(0, 0, NULL);
    ModsDrvGetForcedC2CConnections(0, 0, NULL);
    ModsDrvVirtualAlloc(NULL, NULL, 0, 0, 0);
    ModsDrvVirtualFree(NULL, 0, MODSDRV_VFT_DECOMMIT);
    ModsDrvVirtualProtect(NULL, 0, 0);
    ModsDrvVirtualMadvise(NULL, 0, MODSDRV_MT_NORMAL);
    ModsDrvIsLwSwitchPresent();
    ModsDrvSetLwLinkSysmemTrained(0, 0, 0, 0, MODS_FALSE);
    ModsDrvGetLwLinkLineRate(0, 0, 0, 0, NULL);
    ModsDrvAllocSharedSysmem(NULL, 0, 0, 0, NULL);
    ModsDrvImportSharedSysmem(0, NULL, 0, 0, NULL, NULL);
    ModsDrvFreeSharedSysmem(NULL);
    ModsDrvFindSharedSysmem(NULL, 0, NULL, NULL, NULL);
    ModsDrvAllocPagesIlwPR(0, NULL, NULL, NULL);
    ModsDrvTestApiGetContext();
    ModsDrvReadPFPciConfigIlwF(0, NULL);
    ModsDrvReadTarFile(NULL, NULL, NULL, NULL);
    ModsDrvGetOperatingSystem();
}
