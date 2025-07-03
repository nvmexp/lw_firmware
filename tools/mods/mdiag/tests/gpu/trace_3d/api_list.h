/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef USE_API_LIST
    #error api_list.h should be included only by selected files.
#endif
#undef USE_API_LIST

API_ENTRY(Host_getEventManager)
API_ENTRY(Host_getGpuManager)
API_ENTRY(Host_getBufferManager)
API_ENTRY(Host_getMem)
API_ENTRY(Host_getTraceManager)
API_ENTRY(Host_getUtilityManager)
API_ENTRY(Host_sendMessage)
API_ENTRY(EventManager_getEvent)
API_ENTRY(EventManager_deleteEvent)
API_ENTRY(Event_setTemplateUint32Arg)
API_ENTRY(Event_setTemplateStringArg)
API_ENTRY(Event_getGeneratedUint32Arg)
API_ENTRY(Event_getGeneratedStringArg)
API_ENTRY(Event__register)
API_ENTRY(Event_unregister)
API_ENTRY(GpuManager_getNumGpus)
API_ENTRY(GpuManager_getGpu)
API_ENTRY(GpuManager_deleteGpu)
API_ENTRY(Gpu_RegRd08)
API_ENTRY(Gpu_RegRd16)
API_ENTRY(Gpu_RegRd32)
API_ENTRY(Gpu_RegWr08)
API_ENTRY(Gpu_RegWr16)
API_ENTRY(Gpu_RegWr32)
API_ENTRY(Gpu_FbRd08)
API_ENTRY(Gpu_FbRd16)
API_ENTRY(Gpu_FbRd32)
API_ENTRY(Gpu_FbRd64)
API_ENTRY(Gpu_FbWr08)
API_ENTRY(Gpu_FbWr16)
API_ENTRY(Gpu_FbWr32)
API_ENTRY(Gpu_FbWr64)
API_ENTRY(BufferManager_createTraceBuffer)
API_ENTRY(BufferManager_deleteBuffer)
API_ENTRY(BufferManager_refreshBufferList)
API_ENTRY(BufferManager_begin)
API_ENTRY(BufferManager_end)
API_ENTRY(BufferManager_GetNumSurfaces)
API_ENTRY(BufferManager_GetSurfaceProperties)
API_ENTRY(Buffer_getGpuVirtAddr)
API_ENTRY(Buffer_getOffsetWithinRegion)
API_ENTRY(Buffer_mapMemory)
API_ENTRY(Buffer_unmapMemory)
API_ENTRY(Buffer_getName)
API_ENTRY(Buffer_getType)
API_ENTRY(Buffer_getSize)
API_ENTRY(Mem_VirtRd08)
API_ENTRY(Mem_VirtRd16)
API_ENTRY(Mem_VirtRd32)
API_ENTRY(Mem_VirtRd64)
API_ENTRY(Mem_VirtWr08)
API_ENTRY(Mem_VirtWr16)
API_ENTRY(Mem_VirtWr32)
API_ENTRY(Mem_VirtWr64)
API_ENTRY(TraceManager_FlushMethodsAllChannels)
API_ENTRY(TraceManager_WaitForDMApushAllChannels)
API_ENTRY(TraceManager_getTraceVersion)
API_ENTRY(TraceManager_WaitForIdleAllChannels)
API_ENTRY(TraceManager_GetNumTraceOps)
API_ENTRY(TraceManager_GetTraceOpProperties)
API_ENTRY(UtilityManager_GetTimeNS)
API_ENTRY(UtilityManager__Yield)
API_ENTRY(UtilityManager_getNumErrLoggerStrings)
API_ENTRY(UtilityManager_getErrorLoggerString)
API_ENTRY(BufferIterator_Advance)
API_ENTRY(BufferIterator_Dereference)
API_ENTRY(BufferIterator_Equal)
API_ENTRY(Surface_WriteTga)
API_ENTRY(Surface_WritePng)
API_ENTRY(Surface_CreatePitchImage)
API_ENTRY(Surface_Fill)
API_ENTRY(Surface_FillRect)
API_ENTRY(Surface_FillRect64)
API_ENTRY(Surface_ReadPixel)
API_ENTRY(Surface_WritePixel)
API_ENTRY(BufferManager_GetNumBuffers)
API_ENTRY(BufferManager_GetBufferProperties)
API_ENTRY(TraceManager_GetNumChannels)
API_ENTRY(TraceManager_GetChannelProperties)
API_ENTRY(Buffer_Get032Addr)
API_ENTRY(UtilityManager_EscapeWrite)
API_ENTRY(UtilityManager_EscapeRead)
API_ENTRY(Gpu_GetRefmanRegOffset)
API_ENTRY(Surface_MapMemory)
API_ENTRY(Surface_UnmapMemory)
API_ENTRY(Surface_GetSizeInBytes)
API_ENTRY(Surface_ReadSurface)
API_ENTRY(Surface_WriteSurface)
API_ENTRY(TraceManager_GetTraceChannelPtr)
API_ENTRY(TraceManager_MethodWrite)
API_ENTRY(TraceManager_SetObject)
API_ENTRY(TraceManager_CreateObject)
API_ENTRY(TraceManager_DestroyObject)
API_ENTRY(TraceManager_GetChannelRefCount)
API_ENTRY(Mem_LwRmMapMemory)
API_ENTRY(Mem_LwRmUnmapMemory)
API_ENTRY(Mem_LwRmMapMemoryDma)
API_ENTRY(Mem_LwRmUnmapMemoryDma)
API_ENTRY(Mem_LwRmAllocOnDevice)
API_ENTRY(Mem_LwRmFree)

// major version 1, minor version 2 adds:
API_ENTRY(TraceManager_GetSubchannelProperties)
API_ENTRY(UtilityManager_GetSimulationMode)

// major version 1, minor version 3 adds:
API_ENTRY(Surface_GetPixelOffset)

// major version 1, minor version 4 adds:
API_ENTRY(Buffer_SetProperty)

// major version 1, minor version 6 adds:
API_ENTRY(Mem_FlushCpuWriteCombineBuffer)

// major version 1, minor version 7 adds:
API_ENTRY(Surface_ReadSurfaceUsingDma)
API_ENTRY(Surface_CreatePitchImageUsingDma)
API_ENTRY(Buffer_ReadUsingDma)

// major version 1, minor version 8 adds:
API_ENTRY(Buffer_getPteKind)

// major version 1, minor version 9 adds:
API_ENTRY(UtilityManager_AllocEvent)
API_ENTRY(UtilityManager_FreeEvent)
API_ENTRY(UtilityManager_ResetEvent)
API_ENTRY(UtilityManager_SetEvent)
API_ENTRY(UtilityManager_IsEventSet)
API_ENTRY(UtilityManager_HookPmuEvent)
API_ENTRY(UtilityManager_UnhookPmuEvent)
API_ENTRY(UtilityManager_FailTest)

// major version 1, minor version 10 adds:
API_ENTRY(UtilityManager_PciRead32)
API_ENTRY(UtilityManager_PciWrite32)

// major version 1, minor version 11 adds:
API_ENTRY(TraceManager_WaitForIdleOneChannel)
API_ENTRY(TraceManager_GetMethodCountOneChannel)

// major version 1, minor version 12 adds:
API_ENTRY(TraceManager_getTraceHeaderFile)
API_ENTRY(Buffer_SendGpEntry)

// major version 1, minor version 13 adds:
API_ENTRY(Buffer_getGpuPhysAddr)

// major version 1, minor version 14 adds:
API_ENTRY(Mem_MapDeviceMemory)
API_ENTRY(Mem_UnMapDeviceMemory)

// major version 1, minor version 15 adds:
API_ENTRY(Buffer_WriteBuffer)

// major version 1, minor version 16 adds:
API_ENTRY(UtilityManager_GetSimulatorTime)

// major version 1, minor version 17 adds:
API_ENTRY(TraceManager_GetMethodWriteCountOneChannel)

// major version 1, minor version 18 adds:
API_ENTRY(UtilityManager_GetVirtualMemoryUsed)
API_ENTRY(UtilityManager_Printf)

// major version 1, minor version 19 adds:
API_ENTRY(GpuManager_GetDeviceIdString)

// major version 1, minor version 20 adds:
API_ENTRY(UtilityManager_GetOSString)

// major version 1, minor version 21:
API_ENTRY(Buffer_getGpuPartitionIndex)
API_ENTRY(Buffer_getGpuL2SliceIndex)
API_ENTRY(Buffer_getGpuXbarRawAddr)

// major version 1, minor version 22:
API_ENTRY(Gpu_GetSMVersion)

// major version 1, minor version 23:
API_ENTRY(Buffer_getGpuSubpartition)
API_ENTRY(Buffer_getGpuRBCAddr)
API_ENTRY(Buffer_getGpuRBCAddrExt)

// major version 1, minor version 24:
API_ENTRY(Surface_getGpuPhysAddr)
API_ENTRY(Surface_getGpuPartitionIndex)
API_ENTRY(Surface_getGpuL2SliceIndex)
API_ENTRY(Surface_getGpuXbarRawAddr)

// major version 1, minor version 25:
API_ENTRY(BufferManager_CreateSurfaceView)
API_ENTRY(BufferManager_GetSurfaceByName)
API_ENTRY(Surface_IsSurfaceView)

// major version 1, minor version 26:
//
API_ENTRY(TraceManager_getTraceOpLineNumber)

// major version 1, minor version 27:
//
API_ENTRY(Host_getJavaScriptManager)
API_ENTRY(Host_getDisplayManager)
API_ENTRY(JavaScriptManager_LoadJavaScript)
API_ENTRY(JavaScriptManager_ParseArgs)
API_ENTRY(JavaScriptManager_CallVoidMethod)
API_ENTRY(JavaScriptManager_CallINT32Method)
API_ENTRY(JavaScriptManager_CallUINT32Method)
API_ENTRY(DisplayManager_GetChannelByHandle)
API_ENTRY(DisplayManager_MethodWrite)
API_ENTRY(DisplayManager_FlushChannel)
API_ENTRY(DisplayManager_CreateContextDma)
API_ENTRY(DisplayManager_GetDmaCtxAddr)
API_ENTRY(DisplayManager_GetDmaCtxOffset)
API_ENTRY(TraceManager_RunTraceOps)

// major version 1, minor version 28:
//
API_ENTRY(TraceManager_StartPerfMon)
API_ENTRY(TraceManager_StopPerfMon)

// major version 1, minor version 29 [RTAPI]
//
API_ENTRY(Host_getSOC)
API_ENTRY(BufferManager_CreateDynamicSurface)
API_ENTRY(Surface_Alloc)
API_ENTRY(Surface_Free)
API_ENTRY(Surface_SetPitch)
API_ENTRY(SOC_RegRd32)
API_ENTRY(SOC_RegWr32)
API_ENTRY(SOC_AllocSyncPoint)
API_ENTRY(Mem_PhysRd08)
API_ENTRY(Mem_PhysRd16)
API_ENTRY(Mem_PhysRd32)
API_ENTRY(Mem_PhysRd64)
API_ENTRY(Mem_PhysWr08)
API_ENTRY(Mem_PhysWr16)
API_ENTRY(Mem_PhysWr32)
API_ENTRY(Mem_PhysWr64)
API_ENTRY(SOC_SyncPoint_Wait)
API_ENTRY(SOC_SyncPoint_HostIncrement)
API_ENTRY(SOC_SyncPoint_GraphicsIncrement)
API_ENTRY(SOC_SyncPoint_GetIndex)

// major version 1, minor version 30:
//
API_ENTRY(UtilityManager_ModsAssertFail)
API_ENTRY(Gpu_GetRegNum)
API_ENTRY(Gpu_SetRegFldDef)
API_ENTRY(Gpu_SetRegFldNum)
API_ENTRY(Gpu_GetRegFldDef)
API_ENTRY(Gpu_GetRegFldNum)
API_ENTRY(Gpu_PciRd08)
API_ENTRY(Gpu_PciRd16)
API_ENTRY(Gpu_PciRd32)
API_ENTRY(Gpu_PciWr08)
API_ENTRY(Gpu_PciWr16)
API_ENTRY(Gpu_PciWr32)

// major version 1, minor version 31:
//
API_ENTRY(Gpu_GetDeviceId)
API_ENTRY(Gpu_GetDeviceIdString)

// major version 1, minor version 32:
//
API_ENTRY(UtilityManager_GetTimeoutMS)
API_ENTRY(UtilityManager_Poll)

// major version 1, minor version 33:
//
API_ENTRY(UtilityManager_DelayNS)

// major version 1, minor version 34:
//
API_ENTRY(Surface_IsAllocated)
API_ENTRY(Buffer_IsAllocated)
API_ENTRY(Buffer_GetTagCount)
API_ENTRY(Buffer_GetTag)
API_ENTRY(Buffer_GetTagList)

// major version 1, minor version 35:
//
API_ENTRY(Gpu_SetPcieLinkSpeed)

// major version 1, minor version 36:
//
API_ENTRY(BufferManager_SetSkipBufferDownloads)

// major version 1, minor version 37:
//
API_ENTRY(Surface_CallwlateCrc)

// major version 1, minor version 38:
//
API_ENTRY(Gpu_LoadPMUFirmware)

// major version 1, minor version 39:
//
API_ENTRY(BufferManager_RedoRelocations)

// major version 1, minor version 40:
//
API_ENTRY(Buffer_IsGpuMapped)
API_ENTRY(Surface_IsGpuMapped)

// major version 1, minor version 41:
//
API_ENTRY(Surface_SetVASpace)
API_ENTRY(Surface_GetTegraVirtAddr)
API_ENTRY(SOC_IsSmmuEnabled)

// major version 1, minor version 42:
//
API_ENTRY(BufferManager_DeclareTegraSharedSurface)

// major version 1, minor version 43:
// Note: these calls were originally added in 1.29.
API_ENTRY(UtilityManager_HookIRQ)
API_ENTRY(UtilityManager_UnhookIRQ)

// major version 1, minor version 44:
//
API_ENTRY(SOC_AllocSyncPointBase)
API_ENTRY(SOC_SyncPointBase_GetIndex)
API_ENTRY(SOC_SyncPointBase_GetValue)
API_ENTRY(SOC_SyncPointBase_SetValue)
API_ENTRY(SOC_SyncPointBase_AddValue)
API_ENTRY(SOC_SyncPointBase_CpuSetValue)
API_ENTRY(SOC_SyncPointBase_CpuAddValue)

// major version 1, minor version 45:
//
API_ENTRY(Gpu_GetPowerFeaturesSupported)
API_ENTRY(Gpu_SetGpuPowerOnOff)

// major version 1, minor version 46:
//
API_ENTRY(Gpu_GobWidth)
API_ENTRY(Gpu_GobHeight)
API_ENTRY(Gpu_GobDepth)

// major version 1, minor version 47:
//
API_ENTRY(Gpu_EnablePrivProtectedRegisters)
API_ENTRY(Gpu_EnablePowerGatingTestDuringRegAccess)

// major version 1, minor version 48:
// modifies UtilityManager::FailTest()

// major version 1, minor version 49:
//
API_ENTRY(BufferManager_DeclareGlobalSharedSurface)

// major version 1, minor version 50:
// modify the name of enum in BufferManager::MemTarget

// major version 1, minor version 51:
//
//API_ENTRY(Gpu_RmControl5070)

// minor version 52:
// moves RmControl5070 from Gpu to DisplayManager
API_ENTRY(DisplayManager_RmControl5070)
API_ENTRY(DisplayManager_GetActiveDisplayIDs)

// minor version 53:
API_ENTRY(DisplayManager_GetMasterSubdevice)

// minor version 54:
API_ENTRY(DisplayManager_GetConfigParams)

// minor version 55:
API_ENTRY(Surface_getPhysAddress)
API_ENTRY(Surface_getBAR1PhysAddress)
API_ENTRY(Surface_GetMemoryMappingMode)
API_ENTRY(Surface_SetMemoryMappingMode)
API_ENTRY(Surface_GetLocation)
API_ENTRY(Surface_SetLocation)
API_ENTRY(Surface_GetGpuCacheMode)
API_ENTRY(Surface_SetGpuCacheMode)

// minor version 56:
API_ENTRY(Mem_AllocIRam)
API_ENTRY(Mem_FreeIRam)
API_ENTRY(Surface_SetVideoProtected)

// minor version 57:
API_ENTRY(UtilityManager_VAPrintf)

// minor version 58:
API_ENTRY(Mem_VirtRd)
API_ENTRY(Mem_VirtWr)

// minor version 59:
API_ENTRY(UtilityManager_GetThreadId)

// minor version 60:
API_ENTRY(FileProxy_Fopen)
API_ENTRY(FileProxy_Fclose)
API_ENTRY(FileProxy_Feof)
API_ENTRY(FileProxy_Ferror)
API_ENTRY(FileProxy_Fwrite)
API_ENTRY(FileProxy_Fread)
API_ENTRY(FileProxy_Fgetc)
API_ENTRY(FileProxy_Fputc)
API_ENTRY(FileProxy_Fgets)
API_ENTRY(FileProxy_Fputs)
API_ENTRY(FileProxy_Ftell)
API_ENTRY(FileProxy_Fseek)

// minor version 61:
API_ENTRY(UtilityManager_IsEmulation)

//minor version 62:
API_ENTRY(UtilityManager_DisableIRQ)
API_ENTRY(UtilityManager_EnableIRQ)
API_ENTRY(UtilityManager_IsIRQHooked)

//minor version 64:
API_ENTRY(Gpu_GetGCxWakeupReason)

//minor version 65:
API_ENTRY(TraceManager_getTestName)

//minor version 66:
API_ENTRY(Gpu_GetGC6PlusIsIslandLoaded)

//minor version 67:
API_ENTRY(SOC_RegRd08)
API_ENTRY(SOC_RegWr08)
API_ENTRY(SOC_RegRd16)
API_ENTRY(SOC_RegWr16)

//minor version 68:
API_ENTRY(UtilityManager_DisableInterrupts)
API_ENTRY(UtilityManager_EnableInterrupts)

//minor version 69:
API_ENTRY(Surface_SwizzleCompressionTags)
API_ENTRY(BufferManager_GetTraversalDirection)

//minor version 70:
API_ENTRY(BufferManager_GetActualPhysAddr)
API_ENTRY(UtilityManager_RegisterUnmapEvent)
API_ENTRY(UtilityManager_RegisterMapEvent)

//minor version 71:
API_ENTRY(Surface_getGpuLtcIndex)
API_ENTRY(Buffer_getGpuLtcIndex)

//minor version 72:
//deprecate Surface_CreatePitchImage and Surface_CreatePitchImageUsingDma
API_ENTRY(Surface_CreateSizedPitchImage)
API_ENTRY(Surface_CreateSizedPitchImageUsingDma)

//minor version 73:
API_ENTRY(Gpu_SetDiGpuReady)

// minor version 74:
//
API_ENTRY(Gpu_GetIrq)

// minor version 75:
//
API_ENTRY(Gpu_UnlockHost2Jtag)
API_ENTRY(Gpu_ReadHost2Jtag)
API_ENTRY(Gpu_WriteHost2Jtag)

// minor version 76:
API_ENTRY(BufferManager_AllocSurface)

// minor version 77:
// modify Gpu::ReadHost2Jtag() and Gpu::WriteHost2Jtag()

// minor version 78:
API_ENTRY(UtilityManager_FindPciDevice)
API_ENTRY(UtilityManager_FindPciClassCode)

// minor version 79:
API_ENTRY(Gpu_GetGpcCount)
API_ENTRY(Gpu_GetTpcCountOnGpc)

// minor version 80:
API_ENTRY(Buffer_getEccInjectRegVal)

// minor version 81:

// minor version 82:
API_ENTRY(UtilityManager_AllocSemaphore)
API_ENTRY(UtilityManager_FreeSemaphore)
API_ENTRY(Semaphore_Acquire)
API_ENTRY(Semaphore_Release)
API_ENTRY(Semaphore_GetPhysAddress)

// minor version 83:
API_ENTRY(Semaphore_AcquireGE)

// minor version 84:
API_ENTRY(Mem_PhysRd)
API_ENTRY(Mem_PhysWr)

// minor version 85:
API_ENTRY(UtilityManager_AllocRawSysMem)

// minor version 86:
API_ENTRY(Gpu_GetLwlinkPowerState)
API_ENTRY(Gpu_SetLwlinkPowerState)

// minor version 87:
// extend Gpu::SetGpuPowerOnOff() interface with RTD3 flag

// minor version 88:
//
API_ENTRY(Gpu_GetNumIrqs)
API_ENTRY(Gpu_GetHookedIntrType)

// minor version 89:
API_ENTRY(Gpu_SetPrePostPowerOnOff)

// minor version 90:
//       modifies Surface::Alloc()
//       Mem::LwRmMapMemory() Mem::LwRmUnmapMemory()
//       Mem::LwRmMapMemoryDma() Mem::LwRmUnmapMemoryDma()
//       Mem::LwRmAllocOnDevice() Mem::LwRmFree()

// minor version 91:
API_ENTRY(Semaphore_SetReleaseFlags)

// minor version 92:
API_ENTRY(TraceManager_GetSyspipe)

// minor version 93:
//      modifies Gpu::regName2Offset
//      Gpu::GetRegNum
//      Gpu::SetRegFldDef
//      Gpu::SetRegFldNum
//      Gpu::GetRegFldNum

// minor version 94:
API_ENTRY(Gpu_GetLwlinkConnectedMask)

// minor version 95:
API_ENTRY(Buffer_mapEccAddrToPhysAddr)

// minor version 96:
API_ENTRY(Gpu_GetLwlinkConnected)
API_ENTRY(Gpu_GetLwlinkLoopProp)
// API_ENTRY(Gpu_GetLwlinkClock)
// API_ENTRY(Gpu_GetLwlinkCommonClock)
API_ENTRY(Gpu_GetLwlinkRemoteLinkNum)
API_ENTRY(Gpu_GetLwlinkLocalLinkNum)
API_ENTRY(Gpu_GetLwlinkDiscoveredMask)
API_ENTRY(Gpu_GetLwlinkEnabledMask)

// minor version 97:
API_ENTRY(Buffer_CreateSegmentList)
API_ENTRY(Surface_CreateSegmentList)
API_ENTRY(SegmentList_GetLength)
API_ENTRY(SegmentList_GetLocation)
API_ENTRY(SegmentList_GetSize)
API_ENTRY(SegmentList_GetPhysAddress)
API_ENTRY(SegmentList_Map)
API_ENTRY(SegmentList_IsMapped)
API_ENTRY(SegmentList_GetAddress)
API_ENTRY(SegmentList_Unmap)

// minor version 98:
API_ENTRY(Gpu_GetLwlinkFomVals)
API_ENTRY(Gpu_GetLwlinkLocalSidVals)
API_ENTRY(Gpu_GetLwlinkRemoteSidVals)
API_ENTRY(Gpu_GetLwlinkLineRateMbps)
API_ENTRY(Gpu_GetLwlinkLinkClockMHz)
API_ENTRY(Gpu_GetLwlinkLinkClockType)
API_ENTRY(Gpu_GetLwlinkDataRateKiBps)

// minor version 99:
//      modifies EventManager::getEvent

// minor version 100:
API_ENTRY(Surface_SetPhysContig)

// minor version 101:
API_ENTRY(Gpu_GetEngineResetId)

// minor version 102:
API_ENTRY(Gpu_GetLwlinkErrInfo)

// minor version 103:
API_ENTRY(UtilityManager_GetChipArgs)

// minor version 104
API_ENTRY(UtilityManager_GpuEscapeWriteBuffer)
API_ENTRY(UtilityManager_GpuEscapeReadBuffer)

// minor version 105:
API_ENTRY(Gpu_EnterSlm)
API_ENTRY(Gpu_ExitSlm)

// minor version 106:
API_ENTRY(Gpu_TestRegFldDef)
API_ENTRY(Gpu_TestRegFldNum)

// minor version 107:
API_ENTRY(UtilityManager_ExitMods)

// minor version 108:
API_ENTRY(Surface_GetGpuVirtAddr)

// minor version 109:
API_ENTRY(UtilityManager_TriggerUtlUserEvent)

// minor version 110:
API_ENTRY(TraceManager_StopVpmCapture)

// minor version 111:
API_ENTRY(Buffer_getGpuRBCAddrExt2)
API_ENTRY(Buffer_getGpuPCIndex)
API_ENTRY(Buffer_getEccInjectExtRegVal)

// minor version 112:
API_ENTRY(Gpu_SwReset)

// minor version 113:
API_ENTRY(TraceManager_AbortTest)

// minor version 114:
API_ENTRY(UtilityManager_TrainIntraNodeConnsParallel)

// minor version 117:
API_ENTRY(Gpu_SetEnableCEPrefetch)

// minor version 118:
API_ENTRY(Surface_WriteSurfaceUsingDma)
