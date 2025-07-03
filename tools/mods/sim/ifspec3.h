/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Generated with //sw/mods/chiplib/shim/generate/generate_shim.py#32
//   by lajones on 2020-10-23.

// DO NOT HAND-EDIT.
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#if !defined(INCLUDED_IFSPEC3_H)
#define INCLUDED_IFSPEC3_H

// Ick, we need these to get size_t, used in IMapMemoryExt.h
#include <stddef.h>
using namespace std;

#include "IDeviceDiscovery.h"
#include "IIface.h"
#include "IInterruptMask.h"
#include "IMultiHeap.h"
#include "IIo.h"
#include "IAModelBackdoor.h"
#include "IBusMem.h"
#include "IMapDma.h"
#include "IClockMgr.h"
#include "IInterrupt4.h"
#include "IInterrupt3.h"
#include "IGpuEscape.h"
#include "IMapMemory.h"
#include "IInterrupt.h"
#include "IGpuEscape2.h"
#include "ICPUModel.h"
#include "IPciDev.h"
#include "IInterruptMgr2.h"
#include "IInterruptMgr3.h"
#include "IPpc.h"
#include "ICPUModel.h"
#include "IInterruptMgr.h"
#include "IMemAlloc64.h"
#include "IMultiHeap2.h"
#include "IMemory.h"
#include "IChip.h"
#include "IPciDev2.h"
#include "IMapMemoryExt.h"

// Shared between mods and sim.

namespace IFSPEC3
{

typedef void           (* FN_PTR_IDeviceDiscovery_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IDeviceDiscovery_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IDeviceDiscovery_QueryIface )(void* pvthis, IID_TYPE id);
typedef int            (* FN_PTR_IDeviceDiscovery_HasPciBus )(void* pvthis);
typedef int            (* FN_PTR_IDeviceDiscovery_GetNumDevices )(void* pvthis);
typedef int            (* FN_PTR_IDeviceDiscovery_GetNumBARs )(void* pvthis, int devIdx);
typedef int            (* FN_PTR_IDeviceDiscovery_GetDeviceBAR )(void* pvthis, int devIdx, int barIdx, LwU064* pPhysAddr, LwU064* pSize);

struct IDeviceDiscoveryFuncs
{
    FN_PTR_IDeviceDiscovery_AddRef AddRef;
    FN_PTR_IDeviceDiscovery_Release Release;
    FN_PTR_IDeviceDiscovery_QueryIface QueryIface;
    FN_PTR_IDeviceDiscovery_HasPciBus HasPciBus;
    FN_PTR_IDeviceDiscovery_GetNumDevices GetNumDevices;
    FN_PTR_IDeviceDiscovery_GetNumBARs GetNumBARs;
    FN_PTR_IDeviceDiscovery_GetDeviceBAR GetDeviceBAR;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != HasPciBus)  &&
             (0 != GetNumDevices)  &&
             (0 != GetNumBARs)  &&
             (0 != GetDeviceBAR) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IIfaceObject_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IIfaceObject_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IIfaceObject_QueryIface )(void* pvthis, IID_TYPE id);

struct IIfaceObjectFuncs
{
    FN_PTR_IIfaceObject_AddRef     AddRef;
    FN_PTR_IIfaceObject_Release    Release;
    FN_PTR_IIfaceObject_QueryIface QueryIface;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};
typedef bool (* FN_PTR_PushIIfaceObjectFuncs )(const IIfaceObjectFuncs& vtable);

typedef void           (* FN_PTR_IInterruptMask_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IInterruptMask_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IInterruptMask_QueryIface )(void* pvthis, IID_TYPE id);
typedef int            (* FN_PTR_IInterruptMask_SetInterruptMask )(void* pvthis, LwU032 irqNumber, LwU064 barAddr, LwU032 barSize, LwU032 regOffset, LwU064 andMask, LwU064 orMask, LwU008 irqType, LwU008 maskType, LwU016 domain, LwU016 bus, LwU016 device, LwU016 function);
typedef int            (* FN_PTR_IInterruptMask_SetInterruptMultiMask )(void* pvthis, LwU032 irqNumber, LwU008 irqType, LwU064 barAddr, LwU032 barSize, const PciInfo* pciInfo, LwU032 maskInfoCount, const MaskInfo* pMaskInfoList);

struct IInterruptMaskFuncs
{
    FN_PTR_IInterruptMask_AddRef   AddRef;
    FN_PTR_IInterruptMask_Release  Release;
    FN_PTR_IInterruptMask_QueryIface QueryIface;
    FN_PTR_IInterruptMask_SetInterruptMask SetInterruptMask;
    FN_PTR_IInterruptMask_SetInterruptMultiMask SetInterruptMultiMask;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != SetInterruptMask)  &&
             (0 != SetInterruptMultiMask) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IMultiHeap_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IMultiHeap_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IMultiHeap_QueryIface )(void* pvthis, IID_TYPE id);
typedef int            (* FN_PTR_IMultiHeap_AllocSysMem64 )(void* pvthis, LwU064 sz, LwU032 t, LwU064 align, LwU064* pRtnAddr);
typedef void           (* FN_PTR_IMultiHeap_FreeSysMem64 )(void* pvthis, LwU064 addr);
typedef int            (* FN_PTR_IMultiHeap_AllocSysMem32 )(void* pvthis, LwU032 sz, LwU032 t, LwU032 align, LwU032* pRtnAddr);
typedef void           (* FN_PTR_IMultiHeap_FreeSysMem32 )(void* pvthis, LwU032 addr);
typedef bool           (* FN_PTR_IMultiHeap_Support64 )(void* pvthis);

struct IMultiHeapFuncs
{
    FN_PTR_IMultiHeap_AddRef       AddRef;
    FN_PTR_IMultiHeap_Release      Release;
    FN_PTR_IMultiHeap_QueryIface   QueryIface;
    FN_PTR_IMultiHeap_AllocSysMem64 AllocSysMem64;
    FN_PTR_IMultiHeap_FreeSysMem64 FreeSysMem64;
    FN_PTR_IMultiHeap_AllocSysMem32 AllocSysMem32;
    FN_PTR_IMultiHeap_FreeSysMem32 FreeSysMem32;
    FN_PTR_IMultiHeap_Support64    Support64;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != AllocSysMem64)  &&
             (0 != FreeSysMem64)  &&
             (0 != AllocSysMem32)  &&
             (0 != FreeSysMem32)  &&
             (0 != Support64) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IIo_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IIo_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IIo_QueryIface )(void* pvthis, IID_TYPE id);
typedef LwU008         (* FN_PTR_IIo_IoRd08 )(void* pvthis, LwU016 address);
typedef LwU016         (* FN_PTR_IIo_IoRd16 )(void* pvthis, LwU016 address);
typedef LwU032         (* FN_PTR_IIo_IoRd32 )(void* pvthis, LwU016 address);
typedef void           (* FN_PTR_IIo_IoWr08 )(void* pvthis, LwU016 address, LwU008 data);
typedef void           (* FN_PTR_IIo_IoWr16 )(void* pvthis, LwU016 address, LwU016 data);
typedef void           (* FN_PTR_IIo_IoWr32 )(void* pvthis, LwU016 address, LwU032 data);
typedef LwU008         (* FN_PTR_IIo_CfgRd08 )(void* pvthis, LwU032 address);
typedef LwU016         (* FN_PTR_IIo_CfgRd16 )(void* pvthis, LwU032 address);
typedef LwU032         (* FN_PTR_IIo_CfgRd32 )(void* pvthis, LwU032 address);
typedef void           (* FN_PTR_IIo_CfgWr08 )(void* pvthis, LwU032 address, LwU008 data);
typedef void           (* FN_PTR_IIo_CfgWr16 )(void* pvthis, LwU032 address, LwU016 data);
typedef void           (* FN_PTR_IIo_CfgWr32 )(void* pvthis, LwU032 address, LwU032 data);

struct IIoFuncs
{
    FN_PTR_IIo_AddRef              AddRef;
    FN_PTR_IIo_Release             Release;
    FN_PTR_IIo_QueryIface          QueryIface;
    FN_PTR_IIo_IoRd08              IoRd08;
    FN_PTR_IIo_IoRd16              IoRd16;
    FN_PTR_IIo_IoRd32              IoRd32;
    FN_PTR_IIo_IoWr08              IoWr08;
    FN_PTR_IIo_IoWr16              IoWr16;
    FN_PTR_IIo_IoWr32              IoWr32;
    FN_PTR_IIo_CfgRd08             CfgRd08;
    FN_PTR_IIo_CfgRd16             CfgRd16;
    FN_PTR_IIo_CfgRd32             CfgRd32;
    FN_PTR_IIo_CfgWr08             CfgWr08;
    FN_PTR_IIo_CfgWr16             CfgWr16;
    FN_PTR_IIo_CfgWr32             CfgWr32;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != IoRd08)  &&
             (0 != IoRd16)  &&
             (0 != IoRd32)  &&
             (0 != IoWr08)  &&
             (0 != IoWr16)  &&
             (0 != IoWr32)  &&
             (0 != CfgRd08)  &&
             (0 != CfgRd16)  &&
             (0 != CfgRd32)  &&
             (0 != CfgWr08)  &&
             (0 != CfgWr16)  &&
             (0 != CfgWr32) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IAModelBackdoor_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IAModelBackdoor_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IAModelBackdoor_QueryIface )(void* pvthis, IID_TYPE id);
typedef void           (* FN_PTR_IAModelBackdoor_AllocChannelDma )(void* pvthis, LwU032 ChID, LwU032 Class, LwU032 CtxDma, LwU032 ErrorNotifierCtxDma);
typedef void           (* FN_PTR_IAModelBackdoor_FreeChannel )(void* pvthis, LwU032 ChID);
typedef void           (* FN_PTR_IAModelBackdoor_AllocContextDma )(void* pvthis, LwU032 ChID, LwU032 Handle, LwU032 Class, LwU032 target, LwU032 Limit, LwU032 Base, LwU032 Protect, LwU032* PageTable);
typedef void           (* FN_PTR_IAModelBackdoor_AllocObject )(void* pvthis, LwU032 ChID, LwU032 Handle, LwU032 Class);
typedef void           (* FN_PTR_IAModelBackdoor_FreeObject )(void* pvthis, LwU032 ChID, LwU032 Handle);
typedef void           (* FN_PTR_IAModelBackdoor_AllocChannelGpFifo )(void* pvthis, LwU032 ChID, LwU032 Class, LwU032 CtxDma, LwU064 GpFifoOffset, LwU032 GpFifoEntries, LwU032 ErrorNotifierCtxDma);
typedef bool           (* FN_PTR_IAModelBackdoor_PassAdditionalVerification )(void* pvthis, const char* traceFileName);
typedef const char*    (* FN_PTR_IAModelBackdoor_GetModelIdentifierString )(void* pvthis);

struct IAModelBackdoorFuncs
{
    FN_PTR_IAModelBackdoor_AddRef  AddRef;
    FN_PTR_IAModelBackdoor_Release Release;
    FN_PTR_IAModelBackdoor_QueryIface QueryIface;
    FN_PTR_IAModelBackdoor_AllocChannelDma AllocChannelDma;
    FN_PTR_IAModelBackdoor_FreeChannel FreeChannel;
    FN_PTR_IAModelBackdoor_AllocContextDma AllocContextDma;
    FN_PTR_IAModelBackdoor_AllocObject AllocObject;
    FN_PTR_IAModelBackdoor_FreeObject FreeObject;
    FN_PTR_IAModelBackdoor_AllocChannelGpFifo AllocChannelGpFifo;
    FN_PTR_IAModelBackdoor_PassAdditionalVerification PassAdditionalVerification;
    FN_PTR_IAModelBackdoor_GetModelIdentifierString GetModelIdentifierString;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != AllocChannelDma)  &&
             (0 != FreeChannel)  &&
             (0 != AllocContextDma)  &&
             (0 != AllocObject)  &&
             (0 != FreeObject)  &&
             (0 != AllocChannelGpFifo)  &&
             (0 != PassAdditionalVerification)  &&
             (0 != GetModelIdentifierString) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IBusMem_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IBusMem_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IBusMem_QueryIface )(void* pvthis, IID_TYPE id);
typedef BusMemRet      (* FN_PTR_IBusMem_BusMemWrBlk )(void* pvthis, LwU064 address, const void* appdata, LwU032 count);
typedef BusMemRet      (* FN_PTR_IBusMem_BusMemRdBlk )(void* pvthis, LwU064 address, void* appdata, LwU032 count);
typedef BusMemRet      (* FN_PTR_IBusMem_BusMemCpBlk )(void* pvthis, LwU064 dest, LwU064 source, LwU032 count);
typedef BusMemRet      (* FN_PTR_IBusMem_BusMemSetBlk )(void* pvthis, LwU064 address, LwU032 size, void* data, LwU032 data_size);

struct IBusMemFuncs
{
    FN_PTR_IBusMem_AddRef          AddRef;
    FN_PTR_IBusMem_Release         Release;
    FN_PTR_IBusMem_QueryIface      QueryIface;
    FN_PTR_IBusMem_BusMemWrBlk     BusMemWrBlk;
    FN_PTR_IBusMem_BusMemRdBlk     BusMemRdBlk;
    FN_PTR_IBusMem_BusMemCpBlk     BusMemCpBlk;
    FN_PTR_IBusMem_BusMemSetBlk    BusMemSetBlk;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != BusMemWrBlk)  &&
             (0 != BusMemRdBlk)  &&
             (0 != BusMemCpBlk)  &&
             (0 != BusMemSetBlk) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};
typedef bool (* FN_PTR_PushIBusMemFuncs )(const IBusMemFuncs& vtable);

typedef void           (* FN_PTR_IMapDma_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IMapDma_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IMapDma_QueryIface )(void* pvthis, IID_TYPE id);
typedef int            (* FN_PTR_IMapDma_SetDmaMask )(void* pvthis, LwPciDev pciDev, LwU032 bits);
typedef int            (* FN_PTR_IMapDma_DmaMapMemory )(void* pvthis, LwPciDev pciDev, LwU064 physAddr);
typedef int            (* FN_PTR_IMapDma_DmaUnmapMemory )(void* pvthis, LwPciDev pciDev, LwU064 physAddr);
typedef int            (* FN_PTR_IMapDma_GetDmaMappedAddress )(void* pvthis, LwPciDev pciDev, LwU064 physAddr, LwU064* pIoVirtAddr);

struct IMapDmaFuncs
{
    FN_PTR_IMapDma_AddRef          AddRef;
    FN_PTR_IMapDma_Release         Release;
    FN_PTR_IMapDma_QueryIface      QueryIface;
    FN_PTR_IMapDma_SetDmaMask      SetDmaMask;
    FN_PTR_IMapDma_DmaMapMemory    DmaMapMemory;
    FN_PTR_IMapDma_DmaUnmapMemory  DmaUnmapMemory;
    FN_PTR_IMapDma_GetDmaMappedAddress GetDmaMappedAddress;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != SetDmaMask)  &&
             (0 != DmaMapMemory)  &&
             (0 != DmaUnmapMemory)  &&
             (0 != GetDmaMappedAddress) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IClockMgr_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IClockMgr_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IClockMgr_QueryIface )(void* pvthis, IID_TYPE id);
typedef int            (* FN_PTR_IClockMgr_GetClockHandle )(void* pvthis, const char* clkDevice, const char* clkController, LwU064* pHandle);
typedef int            (* FN_PTR_IClockMgr_GetClockParent )(void* pvthis, LwU064 handle, LwU064* pParentHandle);
typedef int            (* FN_PTR_IClockMgr_SetClockParent )(void* pvthis, LwU064 handle, LwU064 parentHandle);
typedef int            (* FN_PTR_IClockMgr_GetClockEnabled )(void* pvthis, LwU064 handle, LwU032* pEnableCount);
typedef int            (* FN_PTR_IClockMgr_SetClockEnabled )(void* pvthis, LwU064 handle, int enabled);
typedef int            (* FN_PTR_IClockMgr_GetClockRate )(void* pvthis, LwU064 handle, LwU064* pRateHz);
typedef int            (* FN_PTR_IClockMgr_SetClockRate )(void* pvthis, LwU064 handle, LwU064 rateHz);
typedef int            (* FN_PTR_IClockMgr_AssertClockReset )(void* pvthis, LwU064 handle, int assertReset);

struct IClockMgrFuncs
{
    FN_PTR_IClockMgr_AddRef        AddRef;
    FN_PTR_IClockMgr_Release       Release;
    FN_PTR_IClockMgr_QueryIface    QueryIface;
    FN_PTR_IClockMgr_GetClockHandle GetClockHandle;
    FN_PTR_IClockMgr_GetClockParent GetClockParent;
    FN_PTR_IClockMgr_SetClockParent SetClockParent;
    FN_PTR_IClockMgr_GetClockEnabled GetClockEnabled;
    FN_PTR_IClockMgr_SetClockEnabled SetClockEnabled;
    FN_PTR_IClockMgr_GetClockRate  GetClockRate;
    FN_PTR_IClockMgr_SetClockRate  SetClockRate;
    FN_PTR_IClockMgr_AssertClockReset AssertClockReset;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != GetClockHandle)  &&
             (0 != GetClockParent)  &&
             (0 != SetClockParent)  &&
             (0 != GetClockEnabled)  &&
             (0 != SetClockEnabled)  &&
             (0 != GetClockRate)  &&
             (0 != SetClockRate)  &&
             (0 != AssertClockReset) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IInterrupt4_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IInterrupt4_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IInterrupt4_QueryIface )(void* pvthis, IID_TYPE id);
typedef void           (* FN_PTR_IInterrupt4_HandleInterruptVectorChange )(void* pvthis, const LwU032* pVector, LwU032 numWords);

struct IInterrupt4Funcs
{
    FN_PTR_IInterrupt4_AddRef      AddRef;
    FN_PTR_IInterrupt4_Release     Release;
    FN_PTR_IInterrupt4_QueryIface  QueryIface;
    FN_PTR_IInterrupt4_HandleInterruptVectorChange HandleInterruptVectorChange;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != HandleInterruptVectorChange) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};
typedef bool (* FN_PTR_PushIInterrupt4Funcs )(const IInterrupt4Funcs& vtable);

typedef void           (* FN_PTR_IInterrupt3_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IInterrupt3_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IInterrupt3_QueryIface )(void* pvthis, IID_TYPE id);
typedef void           (* FN_PTR_IInterrupt3_HandleSpecificInterrupt )(void* pvthis, LwU032 irqNumber);

struct IInterrupt3Funcs
{
    FN_PTR_IInterrupt3_AddRef      AddRef;
    FN_PTR_IInterrupt3_Release     Release;
    FN_PTR_IInterrupt3_QueryIface  QueryIface;
    FN_PTR_IInterrupt3_HandleSpecificInterrupt HandleSpecificInterrupt;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != HandleSpecificInterrupt) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};
typedef bool (* FN_PTR_PushIInterrupt3Funcs )(const IInterrupt3Funcs& vtable);

typedef void           (* FN_PTR_IGpuEscape_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IGpuEscape_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IGpuEscape_QueryIface )(void* pvthis, IID_TYPE id);
typedef int            (* FN_PTR_IGpuEscape_EscapeWrite )(void* pvthis, LwU032 GpuId, const char* path, LwU032 index, LwU032 size, LwU032 value);
typedef int            (* FN_PTR_IGpuEscape_EscapeRead )(void* pvthis, LwU032 GpuId, const char* path, LwU032 index, LwU032 size, LwU032* value);
typedef int            (* FN_PTR_IGpuEscape_GetGpuId )(void* pvthis, LwU032 bus, LwU032 dev, LwU032 fnc);

struct IGpuEscapeFuncs
{
    FN_PTR_IGpuEscape_AddRef       AddRef;
    FN_PTR_IGpuEscape_Release      Release;
    FN_PTR_IGpuEscape_QueryIface   QueryIface;
    FN_PTR_IGpuEscape_EscapeWrite  EscapeWrite;
    FN_PTR_IGpuEscape_EscapeRead   EscapeRead;
    FN_PTR_IGpuEscape_GetGpuId     GetGpuId;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != EscapeWrite)  &&
             (0 != EscapeRead)  &&
             (0 != GetGpuId) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IMapMemory_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IMapMemory_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IMapMemory_QueryIface )(void* pvthis, IID_TYPE id);
typedef void           (* FN_PTR_IMapMemory_GetPhysicalAddress )(void* pvthis, void* virtual_base, LwU032 virtual_size, LwU032* physical_base_p, LwU032* physical_size_p);
typedef LwU032         (* FN_PTR_IMapMemory_LinearToPhysical )(void* pvthis, void* LinearAddress);
typedef void*          (* FN_PTR_IMapMemory_PhysicalToLinear )(void* pvthis, LwU032 PhysicalAddress);
typedef int            (* FN_PTR_IMapMemory_MapMemoryRegion )(void* pvthis, LwU032 base, LwU032 size, LwU016 type);

struct IMapMemoryFuncs
{
    FN_PTR_IMapMemory_AddRef       AddRef;
    FN_PTR_IMapMemory_Release      Release;
    FN_PTR_IMapMemory_QueryIface   QueryIface;
    FN_PTR_IMapMemory_GetPhysicalAddress GetPhysicalAddress;
    FN_PTR_IMapMemory_LinearToPhysical LinearToPhysical;
    FN_PTR_IMapMemory_PhysicalToLinear PhysicalToLinear;
    FN_PTR_IMapMemory_MapMemoryRegion MapMemoryRegion;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != GetPhysicalAddress)  &&
             (0 != LinearToPhysical)  &&
             (0 != PhysicalToLinear)  &&
             (0 != MapMemoryRegion) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IInterrupt_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IInterrupt_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IInterrupt_QueryIface )(void* pvthis, IID_TYPE id);
typedef void           (* FN_PTR_IInterrupt_HandleInterrupt )(void* pvthis);

struct IInterruptFuncs
{
    FN_PTR_IInterrupt_AddRef       AddRef;
    FN_PTR_IInterrupt_Release      Release;
    FN_PTR_IInterrupt_QueryIface   QueryIface;
    FN_PTR_IInterrupt_HandleInterrupt HandleInterrupt;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != HandleInterrupt) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};
typedef bool (* FN_PTR_PushIInterruptFuncs )(const IInterruptFuncs& vtable);

typedef void           (* FN_PTR_IGpuEscape2_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IGpuEscape2_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IGpuEscape2_QueryIface )(void* pvthis, IID_TYPE id);
typedef LwErr          (* FN_PTR_IGpuEscape2_EscapeWriteBuffer )(void* pvthis, LwU032 gpuId, const char* path, LwU032 index, size_t size, const void* buf);
typedef LwErr          (* FN_PTR_IGpuEscape2_EscapeReadBuffer )(void* pvthis, LwU032 GpuId, const char* path, LwU032 index, size_t size, void* buf);
typedef LwU032         (* FN_PTR_IGpuEscape2_GetGpuId )(void* pvthis, LwU032 bus, LwU032 dev, LwU032 fnc);

struct IGpuEscape2Funcs
{
    FN_PTR_IGpuEscape2_AddRef      AddRef;
    FN_PTR_IGpuEscape2_Release     Release;
    FN_PTR_IGpuEscape2_QueryIface  QueryIface;
    FN_PTR_IGpuEscape2_EscapeWriteBuffer EscapeWriteBuffer;
    FN_PTR_IGpuEscape2_EscapeReadBuffer EscapeReadBuffer;
    FN_PTR_IGpuEscape2_GetGpuId    GetGpuId;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != EscapeWriteBuffer)  &&
             (0 != EscapeReadBuffer)  &&
             (0 != GetGpuId) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_ICPUModel2_AddRef )(void* pvthis);
typedef void           (* FN_PTR_ICPUModel2_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_ICPUModel2_QueryIface )(void* pvthis, IID_TYPE id);
typedef void           (* FN_PTR_ICPUModel2_CPUModelInitialize )(void* pvthis);
typedef void           (* FN_PTR_ICPUModel2_CPUModelEnableResponse )(void* pvthis, bool enable);
typedef void           (* FN_PTR_ICPUModel2_CPUModelEnableResponseSpecific )(void* pvthis, bool enable, CPUModelEvent event);
typedef bool           (* FN_PTR_ICPUModel2_CPUModelHasResponse )(void* pvthis);
typedef void           (* FN_PTR_ICPUModel2_CPUModelGetResponse )(void* pvthis, CPUModelResponse * response);
typedef CPUModelRet    (* FN_PTR_ICPUModel2_CPUModelRead )(void* pvthis, LwU032 uniqueId, LwU064 address, LwU032 sizeBytes, bool isCoherentlyCaching, bool isProbing);
typedef CPUModelRet    (* FN_PTR_ICPUModel2_CPUModelGetCacheData )(void* pvthis, LwU064 gpa, LwU064 * data);
typedef CPUModelRet    (* FN_PTR_ICPUModel2_CPUModelWrite )(void* pvthis, LwU032 uniqueId, LwU064 address, LwU032 offset, LwU064 data, LwU032 sizeBytes, bool isCoherentlyCaching, bool isProbing, bool isPosted);
typedef void           (* FN_PTR_ICPUModel2_CPUModelAtsShootDown )(void* pvthis, LwU032 uniqueId, LwPciDev bdf, LwU032 pasid, LwU064 address, LwU032 atSize, bool isGpa, bool flush, bool isGlobal);

struct ICPUModel2Funcs
{
    FN_PTR_ICPUModel2_AddRef       AddRef;
    FN_PTR_ICPUModel2_Release      Release;
    FN_PTR_ICPUModel2_QueryIface   QueryIface;
    FN_PTR_ICPUModel2_CPUModelInitialize CPUModelInitialize;
    FN_PTR_ICPUModel2_CPUModelEnableResponse CPUModelEnableResponse;
    FN_PTR_ICPUModel2_CPUModelEnableResponseSpecific CPUModelEnableResponseSpecific;
    FN_PTR_ICPUModel2_CPUModelHasResponse CPUModelHasResponse;
    FN_PTR_ICPUModel2_CPUModelGetResponse CPUModelGetResponse;
    FN_PTR_ICPUModel2_CPUModelRead CPUModelRead;
    FN_PTR_ICPUModel2_CPUModelGetCacheData CPUModelGetCacheData;
    FN_PTR_ICPUModel2_CPUModelWrite CPUModelWrite;
    FN_PTR_ICPUModel2_CPUModelAtsShootDown CPUModelAtsShootDown;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != CPUModelInitialize)  &&
             (0 != CPUModelEnableResponse)  &&
             (0 != CPUModelEnableResponseSpecific)  &&
             (0 != CPUModelHasResponse)  &&
             (0 != CPUModelGetResponse)  &&
             (0 != CPUModelRead)  &&
             (0 != CPUModelGetCacheData)  &&
             (0 != CPUModelWrite)  &&
             (0 != CPUModelAtsShootDown) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IPciDev_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IPciDev_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IPciDev_QueryIface )(void* pvthis, IID_TYPE id);
typedef LwErr          (* FN_PTR_IPciDev_GetPciBarInfo )(void* pvthis, LwPciDev pciDevice, int index, LwU064* pAddress, LwU064* pSize);
typedef LwErr          (* FN_PTR_IPciDev_GetPciIrq )(void* pvthis, LwPciDev pciDevice, LwU032* pIrq);
typedef LwErr          (* FN_PTR_IPciDev_GetPciMappedPhysicalAddress )(void* pvthis, LwU064 address, LwU032 offset, LwU064* pMappedAddress);
typedef LwErr          (* FN_PTR_IPciDev_FindPciDevice )(void* pvthis, LwU016 vendorId, LwU016 deviceId, int index, LwPciDev* pciDev);
typedef LwErr          (* FN_PTR_IPciDev_FindPciClassCode )(void* pvthis, LwU032 classCode, int index, LwPciDev* pciDev);
typedef LwErr          (* FN_PTR_IPciDev_PciCfgRd08 )(void* pvthis, LwPciDev pciDev, LwU032 address, LwU008* pData);
typedef LwErr          (* FN_PTR_IPciDev_PciCfgRd16 )(void* pvthis, LwPciDev pciDev, LwU032 address, LwU016* pData);
typedef LwErr          (* FN_PTR_IPciDev_PciCfgRd32 )(void* pvthis, LwPciDev pciDev, LwU032 address, LwU032* pData);
typedef LwErr          (* FN_PTR_IPciDev_PciCfgWr08 )(void* pvthis, LwPciDev pciDev, LwU032 address, LwU008 data);
typedef LwErr          (* FN_PTR_IPciDev_PciCfgWr16 )(void* pvthis, LwPciDev pciDev, LwU032 address, LwU016 data);
typedef LwErr          (* FN_PTR_IPciDev_PciCfgWr32 )(void* pvthis, LwPciDev pciDev, LwU032 address, LwU032 data);

struct IPciDevFuncs
{
    FN_PTR_IPciDev_AddRef          AddRef;
    FN_PTR_IPciDev_Release         Release;
    FN_PTR_IPciDev_QueryIface      QueryIface;
    FN_PTR_IPciDev_GetPciBarInfo   GetPciBarInfo;
    FN_PTR_IPciDev_GetPciIrq       GetPciIrq;
    FN_PTR_IPciDev_GetPciMappedPhysicalAddress GetPciMappedPhysicalAddress;
    FN_PTR_IPciDev_FindPciDevice   FindPciDevice;
    FN_PTR_IPciDev_FindPciClassCode FindPciClassCode;
    FN_PTR_IPciDev_PciCfgRd08      PciCfgRd08;
    FN_PTR_IPciDev_PciCfgRd16      PciCfgRd16;
    FN_PTR_IPciDev_PciCfgRd32      PciCfgRd32;
    FN_PTR_IPciDev_PciCfgWr08      PciCfgWr08;
    FN_PTR_IPciDev_PciCfgWr16      PciCfgWr16;
    FN_PTR_IPciDev_PciCfgWr32      PciCfgWr32;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != GetPciBarInfo)  &&
             (0 != GetPciIrq)  &&
             (0 != GetPciMappedPhysicalAddress)  &&
             (0 != FindPciDevice)  &&
             (0 != FindPciClassCode)  &&
             (0 != PciCfgRd08)  &&
             (0 != PciCfgRd16)  &&
             (0 != PciCfgRd32)  &&
             (0 != PciCfgWr08)  &&
             (0 != PciCfgWr16)  &&
             (0 != PciCfgWr32) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IInterruptMgr2_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IInterruptMgr2_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IInterruptMgr2_QueryIface )(void* pvthis, IID_TYPE id);
typedef int            (* FN_PTR_IInterruptMgr2_HookInterrupt )(void* pvthis, IrqInfo2 irqInfo);

struct IInterruptMgr2Funcs
{
    FN_PTR_IInterruptMgr2_AddRef   AddRef;
    FN_PTR_IInterruptMgr2_Release  Release;
    FN_PTR_IInterruptMgr2_QueryIface QueryIface;
    FN_PTR_IInterruptMgr2_HookInterrupt HookInterrupt;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != HookInterrupt) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IInterruptMgr3_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IInterruptMgr3_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IInterruptMgr3_QueryIface )(void* pvthis, IID_TYPE id);
typedef int            (* FN_PTR_IInterruptMgr3_AllocateIRQs )(void* pvthis, LwPciDev pciDev, LwU032 lwecs, LwU032 * irqs, LwU032 flags);
typedef int            (* FN_PTR_IInterruptMgr3_HookInterrupt3 )(void* pvthis, IrqInfo2 irqInfo);
typedef void           (* FN_PTR_IInterruptMgr3_FreeIRQs )(void* pvthis, LwPciDev pciDev);

struct IInterruptMgr3Funcs
{
    FN_PTR_IInterruptMgr3_AddRef   AddRef;
    FN_PTR_IInterruptMgr3_Release  Release;
    FN_PTR_IInterruptMgr3_QueryIface QueryIface;
    FN_PTR_IInterruptMgr3_AllocateIRQs AllocateIRQs;
    FN_PTR_IInterruptMgr3_HookInterrupt3 HookInterrupt3;
    FN_PTR_IInterruptMgr3_FreeIRQs FreeIRQs;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != AllocateIRQs)  &&
             (0 != HookInterrupt3)  &&
             (0 != FreeIRQs) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IPpc_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IPpc_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IPpc_QueryIface )(void* pvthis, IID_TYPE id);
typedef LwErr          (* FN_PTR_IPpc_SetupDmaBase )(void* pvthis, LwPciDev pciDevice, TCE_BYPASS_MODE mode, LwU064 devDmaMask, LwU064* pDmaBase);

struct IPpcFuncs
{
    FN_PTR_IPpc_AddRef             AddRef;
    FN_PTR_IPpc_Release            Release;
    FN_PTR_IPpc_QueryIface         QueryIface;
    FN_PTR_IPpc_SetupDmaBase       SetupDmaBase;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != SetupDmaBase) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_ICPUModelCallbacks2_AddRef )(void* pvthis);
typedef void           (* FN_PTR_ICPUModelCallbacks2_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_ICPUModelCallbacks2_QueryIface )(void* pvthis, IID_TYPE id);
typedef void           (* FN_PTR_ICPUModelCallbacks2_CPUModelAtsRequest )(void* pvthis, LwPciDev bdf, LwU032 pasid, LwU064 address, bool isGpa, LwU032 numPages, bool numPageAlign, LwU032 * pageSize, CPUModelAtsResult * results);

struct ICPUModelCallbacks2Funcs
{
    FN_PTR_ICPUModelCallbacks2_AddRef AddRef;
    FN_PTR_ICPUModelCallbacks2_Release Release;
    FN_PTR_ICPUModelCallbacks2_QueryIface QueryIface;
    FN_PTR_ICPUModelCallbacks2_CPUModelAtsRequest CPUModelAtsRequest;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != CPUModelAtsRequest) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};
typedef bool (* FN_PTR_PushICPUModelCallbacks2Funcs )(const ICPUModelCallbacks2Funcs& vtable);

typedef void           (* FN_PTR_IInterruptMgr_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IInterruptMgr_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IInterruptMgr_QueryIface )(void* pvthis, IID_TYPE id);
typedef int            (* FN_PTR_IInterruptMgr_HookInterrupt )(void* pvthis, LwU032 irqNumber);
typedef int            (* FN_PTR_IInterruptMgr_UnhookInterrupt )(void* pvthis, LwU032 irqNumber);
typedef void           (* FN_PTR_IInterruptMgr_PollInterrupts )(void* pvthis);

struct IInterruptMgrFuncs
{
    FN_PTR_IInterruptMgr_AddRef    AddRef;
    FN_PTR_IInterruptMgr_Release   Release;
    FN_PTR_IInterruptMgr_QueryIface QueryIface;
    FN_PTR_IInterruptMgr_HookInterrupt HookInterrupt;
    FN_PTR_IInterruptMgr_UnhookInterrupt UnhookInterrupt;
    FN_PTR_IInterruptMgr_PollInterrupts PollInterrupts;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != HookInterrupt)  &&
             (0 != UnhookInterrupt)  &&
             (0 != PollInterrupts) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IMemAlloc64_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IMemAlloc64_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IMemAlloc64_QueryIface )(void* pvthis, IID_TYPE id);
typedef int            (* FN_PTR_IMemAlloc64_AllocSysMem64 )(void* pvthis, LwU064 sz, LwU064* pRtnAddr);
typedef void           (* FN_PTR_IMemAlloc64_FreeSysMem64 )(void* pvthis, LwU064 addr);

struct IMemAlloc64Funcs
{
    FN_PTR_IMemAlloc64_AddRef      AddRef;
    FN_PTR_IMemAlloc64_Release     Release;
    FN_PTR_IMemAlloc64_QueryIface  QueryIface;
    FN_PTR_IMemAlloc64_AllocSysMem64 AllocSysMem64;
    FN_PTR_IMemAlloc64_FreeSysMem64 FreeSysMem64;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != AllocSysMem64)  &&
             (0 != FreeSysMem64) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IMultiHeap2_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IMultiHeap2_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IMultiHeap2_QueryIface )(void* pvthis, IID_TYPE id);
typedef int            (* FN_PTR_IMultiHeap2_DeviceAllocSysMem64 )(void* pvthis, LwPciDev dev, LwU064 sz, LwU032 t, LwU064 align, LwU064* pRtnAddr);
typedef int            (* FN_PTR_IMultiHeap2_AllocSysMem64 )(void* pvthis, LwU064 sz, LwU032 t, LwU064 align, LwU064* pRtnAddr);
typedef void           (* FN_PTR_IMultiHeap2_FreeSysMem64 )(void* pvthis, LwU064 addr);
typedef int            (* FN_PTR_IMultiHeap2_DeviceAllocSysMem32 )(void* pvthis, LwPciDev dev, LwU032 sz, LwU032 t, LwU032 align, LwU032* pRtnAddr);
typedef int            (* FN_PTR_IMultiHeap2_AllocSysMem32 )(void* pvthis, LwU032 sz, LwU032 t, LwU032 align, LwU032* pRtnAddr);
typedef void           (* FN_PTR_IMultiHeap2_FreeSysMem32 )(void* pvthis, LwU032 addr);
typedef bool           (* FN_PTR_IMultiHeap2_Support64 )(void* pvthis);

struct IMultiHeap2Funcs
{
    FN_PTR_IMultiHeap2_AddRef      AddRef;
    FN_PTR_IMultiHeap2_Release     Release;
    FN_PTR_IMultiHeap2_QueryIface  QueryIface;
    FN_PTR_IMultiHeap2_DeviceAllocSysMem64 DeviceAllocSysMem64;
    FN_PTR_IMultiHeap2_AllocSysMem64 AllocSysMem64;
    FN_PTR_IMultiHeap2_FreeSysMem64 FreeSysMem64;
    FN_PTR_IMultiHeap2_DeviceAllocSysMem32 DeviceAllocSysMem32;
    FN_PTR_IMultiHeap2_AllocSysMem32 AllocSysMem32;
    FN_PTR_IMultiHeap2_FreeSysMem32 FreeSysMem32;
    FN_PTR_IMultiHeap2_Support64   Support64;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != DeviceAllocSysMem64)  &&
             (0 != AllocSysMem64)  &&
             (0 != FreeSysMem64)  &&
             (0 != DeviceAllocSysMem32)  &&
             (0 != AllocSysMem32)  &&
             (0 != FreeSysMem32)  &&
             (0 != Support64) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IMemory_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IMemory_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IMemory_QueryIface )(void* pvthis, IID_TYPE id);
typedef LwU008         (* FN_PTR_IMemory_MemRd08 )(void* pvthis, LwU032 address);
typedef LwU016         (* FN_PTR_IMemory_MemRd16 )(void* pvthis, LwU032 address);
typedef LwU032         (* FN_PTR_IMemory_MemRd32 )(void* pvthis, LwU032 address);
typedef LwU064         (* FN_PTR_IMemory_MemRd64 )(void* pvthis, LwU032 address);
typedef void           (* FN_PTR_IMemory_MemWr08 )(void* pvthis, LwU032 address, LwU008 data);
typedef void           (* FN_PTR_IMemory_MemWr16 )(void* pvthis, LwU032 address, LwU016 data);
typedef void           (* FN_PTR_IMemory_MemWr32 )(void* pvthis, LwU032 address, LwU032 data);
typedef void           (* FN_PTR_IMemory_MemWr64 )(void* pvthis, LwU032 address, LwU064 data);
typedef void           (* FN_PTR_IMemory_MemSet08 )(void* pvthis, LwU032 address, LwU032 size, LwU008 data);
typedef void           (* FN_PTR_IMemory_MemSet16 )(void* pvthis, LwU032 address, LwU032 size, LwU016 data);
typedef void           (* FN_PTR_IMemory_MemSet32 )(void* pvthis, LwU032 address, LwU032 size, LwU032 data);
typedef void           (* FN_PTR_IMemory_MemSet64 )(void* pvthis, LwU032 address, LwU032 size, LwU064 data);
typedef void           (* FN_PTR_IMemory_MemSetBlk )(void* pvthis, LwU032 address, LwU032 size, void* data, LwU032 data_size);
typedef void           (* FN_PTR_IMemory_MemWrBlk )(void* pvthis, LwU032 address, const void* appdata, LwU032 count);
typedef void           (* FN_PTR_IMemory_MemWrBlk32 )(void* pvthis, LwU032 address, const void* appdata, LwU032 count);
typedef void           (* FN_PTR_IMemory_MemRdBlk )(void* pvthis, LwU032 address, void* appdata, LwU032 count);
typedef void           (* FN_PTR_IMemory_MemRdBlk32 )(void* pvthis, LwU032 address, void* appdata, LwU032 count);
typedef void           (* FN_PTR_IMemory_MemCpBlk )(void* pvthis, LwU032 address, LwU032 appdata, LwU032 count);
typedef void           (* FN_PTR_IMemory_MemCpBlk32 )(void* pvthis, LwU032 address, LwU032 appdata, LwU032 count);

struct IMemoryFuncs
{
    FN_PTR_IMemory_AddRef          AddRef;
    FN_PTR_IMemory_Release         Release;
    FN_PTR_IMemory_QueryIface      QueryIface;
    FN_PTR_IMemory_MemRd08         MemRd08;
    FN_PTR_IMemory_MemRd16         MemRd16;
    FN_PTR_IMemory_MemRd32         MemRd32;
    FN_PTR_IMemory_MemRd64         MemRd64;
    FN_PTR_IMemory_MemWr08         MemWr08;
    FN_PTR_IMemory_MemWr16         MemWr16;
    FN_PTR_IMemory_MemWr32         MemWr32;
    FN_PTR_IMemory_MemWr64         MemWr64;
    FN_PTR_IMemory_MemSet08        MemSet08;
    FN_PTR_IMemory_MemSet16        MemSet16;
    FN_PTR_IMemory_MemSet32        MemSet32;
    FN_PTR_IMemory_MemSet64        MemSet64;
    FN_PTR_IMemory_MemSetBlk       MemSetBlk;
    FN_PTR_IMemory_MemWrBlk        MemWrBlk;
    FN_PTR_IMemory_MemWrBlk32      MemWrBlk32;
    FN_PTR_IMemory_MemRdBlk        MemRdBlk;
    FN_PTR_IMemory_MemRdBlk32      MemRdBlk32;
    FN_PTR_IMemory_MemCpBlk        MemCpBlk;
    FN_PTR_IMemory_MemCpBlk32      MemCpBlk32;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != MemRd08)  &&
             (0 != MemRd16)  &&
             (0 != MemRd32)  &&
             (0 != MemRd64)  &&
             (0 != MemWr08)  &&
             (0 != MemWr16)  &&
             (0 != MemWr32)  &&
             (0 != MemWr64)  &&
             (0 != MemSet08)  &&
             (0 != MemSet16)  &&
             (0 != MemSet32)  &&
             (0 != MemSet64)  &&
             (0 != MemSetBlk)  &&
             (0 != MemWrBlk)  &&
             (0 != MemWrBlk32)  &&
             (0 != MemRdBlk)  &&
             (0 != MemRdBlk32)  &&
             (0 != MemCpBlk)  &&
             (0 != MemCpBlk32) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IChip_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IChip_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IChip_QueryIface )(void* pvthis, IID_TYPE id);
typedef int            (* FN_PTR_IChip_Startup )(void* pvthis, IIfaceObject* system, char** argv, int argc);
typedef void           (* FN_PTR_IChip_Shutdown )(void* pvthis);
typedef int            (* FN_PTR_IChip_AllocSysMem )(void* pvthis, int numBytes, LwU032* physAddr);
typedef void           (* FN_PTR_IChip_FreeSysMem )(void* pvthis, LwU032 physAddr);
typedef void           (* FN_PTR_IChip_ClockSimulator )(void* pvthis, LwS032 numClocks);
typedef void           (* FN_PTR_IChip_Delay )(void* pvthis, LwU032 numMicroSeconds);
typedef int            (* FN_PTR_IChip_EscapeWrite )(void* pvthis, char* path, LwU032 index, LwU032 size, LwU032 value);
typedef int            (* FN_PTR_IChip_EscapeRead )(void* pvthis, char* path, LwU032 index, LwU032 size, LwU032* value);
typedef int            (* FN_PTR_IChip_FindPCIDevice )(void* pvthis, LwU016 vendorId, LwU016 deviceId, int index, LwU032* address);
typedef int            (* FN_PTR_IChip_FindPCIClassCode )(void* pvthis, LwU032 classCode, int index, LwU032* address);
typedef int            (* FN_PTR_IChip_GetSimulatorTime )(void* pvthis, LwU064* simTime);
typedef double         (* FN_PTR_IChip_GetSimulatorTimeUnitsNS )(void* pvthis);
typedef int            (* FN_PTR_IChip_GetPCIBaseAddress )(void* pvthis, LwU032 cfgAddr, int index, LwU032* pAddress, LwU032* pSize);
typedef IChip::ELEVEL  (* FN_PTR_IChip_GetChipLevel )(void* pvthis);

struct IChipFuncs
{
    FN_PTR_IChip_AddRef            AddRef;
    FN_PTR_IChip_Release           Release;
    FN_PTR_IChip_QueryIface        QueryIface;
    FN_PTR_IChip_Startup           Startup;
    FN_PTR_IChip_Shutdown          Shutdown;
    FN_PTR_IChip_AllocSysMem       AllocSysMem;
    FN_PTR_IChip_FreeSysMem        FreeSysMem;
    FN_PTR_IChip_ClockSimulator    ClockSimulator;
    FN_PTR_IChip_Delay             Delay;
    FN_PTR_IChip_EscapeWrite       EscapeWrite;
    FN_PTR_IChip_EscapeRead        EscapeRead;
    FN_PTR_IChip_FindPCIDevice     FindPCIDevice;
    FN_PTR_IChip_FindPCIClassCode  FindPCIClassCode;
    FN_PTR_IChip_GetSimulatorTime  GetSimulatorTime;
    FN_PTR_IChip_GetSimulatorTimeUnitsNS GetSimulatorTimeUnitsNS;
    FN_PTR_IChip_GetPCIBaseAddress GetPCIBaseAddress;
    FN_PTR_IChip_GetChipLevel      GetChipLevel;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != Startup)  &&
             (0 != Shutdown)  &&
             (0 != AllocSysMem)  &&
             (0 != FreeSysMem)  &&
             (0 != ClockSimulator)  &&
             (0 != Delay)  &&
             (0 != EscapeWrite)  &&
             (0 != EscapeRead)  &&
             (0 != FindPCIDevice)  &&
             (0 != FindPCIClassCode)  &&
             (0 != GetSimulatorTime)  &&
             (0 != GetSimulatorTimeUnitsNS)  &&
             (0 != GetPCIBaseAddress)  &&
             (0 != GetChipLevel) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IPciDev2_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IPciDev2_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IPciDev2_QueryIface )(void* pvthis, IID_TYPE id);
typedef LwErr          (* FN_PTR_IPciDev2_PciResetFunction )(void* pvthis, LwPciDev pciDev);

struct IPciDev2Funcs
{
    FN_PTR_IPciDev2_AddRef         AddRef;
    FN_PTR_IPciDev2_Release        Release;
    FN_PTR_IPciDev2_QueryIface     QueryIface;
    FN_PTR_IPciDev2_PciResetFunction PciResetFunction;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != PciResetFunction) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

typedef void           (* FN_PTR_IMapMemoryExt_AddRef )(void* pvthis);
typedef void           (* FN_PTR_IMapMemoryExt_Release )(void* pvthis);
typedef IIfaceObject*  (* FN_PTR_IMapMemoryExt_QueryIface )(void* pvthis, IID_TYPE id);
typedef int            (* FN_PTR_IMapMemoryExt_MapMemoryRegion )(void* pvthis, void** pReturnedVirtualAddress, LwU064 PhysicalAddress, size_t NumBytes, int Attrib, int Protect);
typedef void           (* FN_PTR_IMapMemoryExt_UnMapMemoryRegion )(void* pvthis, void* VirtualAddress);
typedef int            (* FN_PTR_IMapMemoryExt_RemapPages )(void* pvthis, void* VirtualAddress, LwU064 PhysicalAddress, size_t NumBytes, int Protect);

struct IMapMemoryExtFuncs
{
    FN_PTR_IMapMemoryExt_AddRef    AddRef;
    FN_PTR_IMapMemoryExt_Release   Release;
    FN_PTR_IMapMemoryExt_QueryIface QueryIface;
    FN_PTR_IMapMemoryExt_MapMemoryRegion MapMemoryRegion;
    FN_PTR_IMapMemoryExt_UnMapMemoryRegion UnMapMemoryRegion;
    FN_PTR_IMapMemoryExt_RemapPages RemapPages;

    bool SanityCheck() const
    {
        if (
             (0 != AddRef)  &&
             (0 != Release)  &&
             (0 != QueryIface)  &&
             (0 != MapMemoryRegion)  &&
             (0 != UnMapMemoryRegion)  &&
             (0 != RemapPages) 
           )
        {
            return true; // All ptrs are set.
        }
        else
        {
            return false;
        }
    }
};

}; // namespace IFSPEC3
#endif // !defined(INCLUDED_IFSPEC3_H)
