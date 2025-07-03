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

#if !defined(INCLUDED_IFSPEC3_SHIM_MODS_H)
#define INCLUDED_IFSPEC3_SHIM_MODS_H

#include "ifspec3.h"
#ifndef SOCKCHIP
#include "core/include/xp.h"
#include "core/include/massert.h"
#else
#include "xp.h"
#include "massert.h"
#endif
#include "irqinfo.h"

namespace IFSPEC3
{

// Push mods "C" function-pointers to the sim so that it can
// call our IFSPEC-class implementations.
// This must be done before passing any such mods C++ objects
// to the sim, so it can populate its proxy object vtable.
RC PushIIfaceObjectFptrsToSim(void* pSimLibModule);
RC PushIBusMemFptrsToSim(void* pSimLibModule);
RC PushIInterrupt4FptrsToSim(void* pSimLibModule);
RC PushIInterrupt3FptrsToSim(void* pSimLibModule);
RC PushIInterruptFptrsToSim(void* pSimLibModule);
RC PushICPUModelCallbacks2FptrsToSim(void* pSimLibModule);

// Free any xxxProxy objects associated with this library,
// both the mods proxies that wrap sim objects, and the
// sim proxies that wrap mods objects.
void FreeProxies(void* pSimLibModule);

// Wrap a sim object with a proxy.
IIfaceObject* CreateProxyForSimObject
(
    IID_TYPE   id,
    void*      simObj,
    void*      pSimLibModule
);

class IDeviceDiscoveryModsProxy : public IDeviceDiscovery
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IDeviceDiscoveryFuncs  m_IDeviceDiscoveryFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IDeviceDiscoveryModsProxy(void* pvIDeviceDiscovery, void* pSimLibModule);

    virtual ~IDeviceDiscoveryModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual int            HasPciBus();
    virtual int            GetNumDevices();
    virtual int            GetNumBARs(int devIdx);
    virtual int            GetDeviceBAR(int devIdx, int barIdx, LwU064* pPhysAddr, LwU064* pSize);
};

class IIfaceObjectModsProxy : public IIfaceObject
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IIfaceObjectFuncs  m_IIfaceObjectFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IIfaceObjectModsProxy(void* pvIIfaceObject, void* pSimLibModule);

    virtual ~IIfaceObjectModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
};

class IInterruptMaskModsProxy : public IInterruptMask
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IInterruptMaskFuncs  m_IInterruptMaskFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IInterruptMaskModsProxy(void* pvIInterruptMask, void* pSimLibModule);

    virtual ~IInterruptMaskModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual int            SetInterruptMask(LwU032 irqNumber, LwU064 barAddr, LwU032 barSize, LwU032 regOffset, LwU064 andMask, LwU064 orMask, LwU008 irqType, LwU008 maskType, LwU016 domain, LwU016 bus, LwU016 device, LwU016 function);
    virtual int            SetInterruptMultiMask(LwU032 irqNumber, LwU008 irqType, LwU064 barAddr, LwU032 barSize, const PciInfo* pciInfo, LwU032 maskInfoCount, const MaskInfo* pMaskInfoList);
};

class IMultiHeapModsProxy : public IMultiHeap
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IMultiHeapFuncs  m_IMultiHeapFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IMultiHeapModsProxy(void* pvIMultiHeap, void* pSimLibModule);

    virtual ~IMultiHeapModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual int            AllocSysMem64(LwU064 sz, LwU032 t, LwU064 align, LwU064* pRtnAddr);
    virtual void           FreeSysMem64(LwU064 addr);
    virtual int            AllocSysMem32(LwU032 sz, LwU032 t, LwU032 align, LwU032* pRtnAddr);
    virtual void           FreeSysMem32(LwU032 addr);
    virtual bool           Support64();
};

class IIoModsProxy : public IIo
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IIoFuncs  m_IIoFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IIoModsProxy(void* pvIIo, void* pSimLibModule);

    virtual ~IIoModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual LwU008         IoRd08(LwU016 address);
    virtual LwU016         IoRd16(LwU016 address);
    virtual LwU032         IoRd32(LwU016 address);
    virtual void           IoWr08(LwU016 address, LwU008 data);
    virtual void           IoWr16(LwU016 address, LwU016 data);
    virtual void           IoWr32(LwU016 address, LwU032 data);
    virtual LwU008         CfgRd08(LwU032 address);
    virtual LwU016         CfgRd16(LwU032 address);
    virtual LwU032         CfgRd32(LwU032 address);
    virtual void           CfgWr08(LwU032 address, LwU008 data);
    virtual void           CfgWr16(LwU032 address, LwU016 data);
    virtual void           CfgWr32(LwU032 address, LwU032 data);
};

class IAModelBackdoorModsProxy : public IAModelBackdoor
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IAModelBackdoorFuncs  m_IAModelBackdoorFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IAModelBackdoorModsProxy(void* pvIAModelBackdoor, void* pSimLibModule);

    virtual ~IAModelBackdoorModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual void           AllocChannelDma(LwU032 ChID, LwU032 Class, LwU032 CtxDma, LwU032 ErrorNotifierCtxDma);
    virtual void           FreeChannel(LwU032 ChID);
    virtual void           AllocContextDma(LwU032 ChID, LwU032 Handle, LwU032 Class, LwU032 target, LwU032 Limit, LwU032 Base, LwU032 Protect, LwU032* PageTable);
    virtual void           AllocObject(LwU032 ChID, LwU032 Handle, LwU032 Class);
    virtual void           FreeObject(LwU032 ChID, LwU032 Handle);
    virtual void           AllocChannelGpFifo(LwU032 ChID, LwU032 Class, LwU032 CtxDma, LwU064 GpFifoOffset, LwU032 GpFifoEntries, LwU032 ErrorNotifierCtxDma);
    virtual bool           PassAdditionalVerification(const char* traceFileName);
    virtual const char*    GetModelIdentifierString();
};

class IBusMemModsProxy : public IBusMem
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IBusMemFuncs  m_IBusMemFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IBusMemModsProxy(void* pvIBusMem, void* pSimLibModule);

    virtual ~IBusMemModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual BusMemRet      BusMemWrBlk(LwU064 address, const void* appdata, LwU032 count);
    virtual BusMemRet      BusMemRdBlk(LwU064 address, void* appdata, LwU032 count);
    virtual BusMemRet      BusMemCpBlk(LwU064 dest, LwU064 source, LwU032 count);
    virtual BusMemRet      BusMemSetBlk(LwU064 address, LwU032 size, void* data, LwU032 data_size);
};

class IMapDmaModsProxy : public IMapDma
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IMapDmaFuncs  m_IMapDmaFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IMapDmaModsProxy(void* pvIMapDma, void* pSimLibModule);

    virtual ~IMapDmaModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual int            SetDmaMask(LwPciDev pciDev, LwU032 bits);
    virtual int            DmaMapMemory(LwPciDev pciDev, LwU064 physAddr);
    virtual int            DmaUnmapMemory(LwPciDev pciDev, LwU064 physAddr);
    virtual int            GetDmaMappedAddress(LwPciDev pciDev, LwU064 physAddr, LwU064* pIoVirtAddr);
};

class IClockMgrModsProxy : public IClockMgr
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IClockMgrFuncs  m_IClockMgrFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IClockMgrModsProxy(void* pvIClockMgr, void* pSimLibModule);

    virtual ~IClockMgrModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual int            GetClockHandle(const char* clkDevice, const char* clkController, LwU064* pHandle);
    virtual int            GetClockParent(LwU064 handle, LwU064* pParentHandle);
    virtual int            SetClockParent(LwU064 handle, LwU064 parentHandle);
    virtual int            GetClockEnabled(LwU064 handle, LwU032* pEnableCount);
    virtual int            SetClockEnabled(LwU064 handle, int enabled);
    virtual int            GetClockRate(LwU064 handle, LwU064* pRateHz);
    virtual int            SetClockRate(LwU064 handle, LwU064 rateHz);
    virtual int            AssertClockReset(LwU064 handle, int assertReset);
};

class IGpuEscapeModsProxy : public IGpuEscape
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IGpuEscapeFuncs  m_IGpuEscapeFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IGpuEscapeModsProxy(void* pvIGpuEscape, void* pSimLibModule);

    virtual ~IGpuEscapeModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual int            EscapeWrite(LwU032 GpuId, const char* path, LwU032 index, LwU032 size, LwU032 value);
    virtual int            EscapeRead(LwU032 GpuId, const char* path, LwU032 index, LwU032 size, LwU032* value);
    virtual int            GetGpuId(LwU032 bus, LwU032 dev, LwU032 fnc);
};

class IMapMemoryModsProxy : public IMapMemory
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IMapMemoryFuncs  m_IMapMemoryFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IMapMemoryModsProxy(void* pvIMapMemory, void* pSimLibModule);

    virtual ~IMapMemoryModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual void           GetPhysicalAddress(void* virtual_base, LwU032 virtual_size, LwU032* physical_base_p, LwU032* physical_size_p);
    virtual LwU032         LinearToPhysical(void* LinearAddress);
    virtual void*          PhysicalToLinear(LwU032 PhysicalAddress);
    virtual int            MapMemoryRegion(LwU032 base, LwU032 size, LwU016 type);
};

class IGpuEscape2ModsProxy : public IGpuEscape2
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IGpuEscape2Funcs  m_IGpuEscape2Funcs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IGpuEscape2ModsProxy(void* pvIGpuEscape2, void* pSimLibModule);

    virtual ~IGpuEscape2ModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual LwErr          EscapeWriteBuffer(LwU032 gpuId, const char* path, LwU032 index, size_t size, const void* buf);
    virtual LwErr          EscapeReadBuffer(LwU032 GpuId, const char* path, LwU032 index, size_t size, void* buf);
    virtual LwU032         GetGpuId(LwU032 bus, LwU032 dev, LwU032 fnc);
};

class ICPUModel2ModsProxy : public ICPUModel2
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    ICPUModel2Funcs  m_ICPUModel2Funcs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    ICPUModel2ModsProxy(void* pvICPUModel2, void* pSimLibModule);

    virtual ~ICPUModel2ModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual void           CPUModelInitialize();
    virtual void           CPUModelEnableResponse(bool enable);
    virtual void           CPUModelEnableResponseSpecific(bool enable, CPUModelEvent event);
    virtual bool           CPUModelHasResponse();
    virtual void           CPUModelGetResponse(CPUModelResponse * response);
    virtual CPUModelRet    CPUModelRead(LwU032 uniqueId, LwU064 address, LwU032 sizeBytes, bool isCoherentlyCaching, bool isProbing);
    virtual CPUModelRet    CPUModelGetCacheData(LwU064 gpa, LwU064 * data);
    virtual CPUModelRet    CPUModelWrite(LwU032 uniqueId, LwU064 address, LwU032 offset, LwU064 data, LwU032 sizeBytes, bool isCoherentlyCaching, bool isProbing, bool isPosted);
    virtual void           CPUModelAtsShootDown(LwU032 uniqueId, LwPciDev bdf, LwU032 pasid, LwU064 address, LwU032 atSize, bool isGpa, bool flush, bool isGlobal);
};

class IPciDevModsProxy : public IPciDev
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IPciDevFuncs  m_IPciDevFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IPciDevModsProxy(void* pvIPciDev, void* pSimLibModule);

    virtual ~IPciDevModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual LwErr          GetPciBarInfo(LwPciDev pciDevice, int index, LwU064* pAddress, LwU064* pSize);
    virtual LwErr          GetPciIrq(LwPciDev pciDevice, LwU032* pIrq);
    virtual LwErr          GetPciMappedPhysicalAddress(LwU064 address, LwU032 offset, LwU064* pMappedAddress);
    virtual LwErr          FindPciDevice(LwU016 vendorId, LwU016 deviceId, int index, LwPciDev* pciDev);
    virtual LwErr          FindPciClassCode(LwU032 classCode, int index, LwPciDev* pciDev);
    virtual LwErr          PciCfgRd08(LwPciDev pciDev, LwU032 address, LwU008* pData);
    virtual LwErr          PciCfgRd16(LwPciDev pciDev, LwU032 address, LwU016* pData);
    virtual LwErr          PciCfgRd32(LwPciDev pciDev, LwU032 address, LwU032* pData);
    virtual LwErr          PciCfgWr08(LwPciDev pciDev, LwU032 address, LwU008 data);
    virtual LwErr          PciCfgWr16(LwPciDev pciDev, LwU032 address, LwU016 data);
    virtual LwErr          PciCfgWr32(LwPciDev pciDev, LwU032 address, LwU032 data);
};

class IInterruptMgr2ModsProxy : public IInterruptMgr2
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IInterruptMgr2Funcs  m_IInterruptMgr2Funcs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IInterruptMgr2ModsProxy(void* pvIInterruptMgr2, void* pSimLibModule);

    virtual ~IInterruptMgr2ModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual int            HookInterrupt(IrqInfo2 irqInfo);
};

class IInterruptMgr3ModsProxy : public IInterruptMgr3
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IInterruptMgr3Funcs  m_IInterruptMgr3Funcs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IInterruptMgr3ModsProxy(void* pvIInterruptMgr3, void* pSimLibModule);

    virtual ~IInterruptMgr3ModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual int            AllocateIRQs(LwPciDev pciDev, LwU032 lwecs, LwU032 * irqs, LwU032 flags);
    virtual int            HookInterrupt3(IrqInfo2 irqInfo);
    virtual void           FreeIRQs(LwPciDev pciDev);
};

class IPpcModsProxy : public IPpc
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IPpcFuncs  m_IPpcFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IPpcModsProxy(void* pvIPpc, void* pSimLibModule);

    virtual ~IPpcModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual LwErr          SetupDmaBase(LwPciDev pciDevice, TCE_BYPASS_MODE mode, LwU064 devDmaMask, LwU064* pDmaBase);
};

class IInterruptMgrModsProxy : public IInterruptMgr
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IInterruptMgrFuncs  m_IInterruptMgrFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IInterruptMgrModsProxy(void* pvIInterruptMgr, void* pSimLibModule);

    virtual ~IInterruptMgrModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual int            HookInterrupt(LwU032 irqNumber);
    virtual int            UnhookInterrupt(LwU032 irqNumber);
    virtual void           PollInterrupts();
};

class IMemAlloc64ModsProxy : public IMemAlloc64
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IMemAlloc64Funcs  m_IMemAlloc64Funcs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IMemAlloc64ModsProxy(void* pvIMemAlloc64, void* pSimLibModule);

    virtual ~IMemAlloc64ModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual int            AllocSysMem64(LwU064 sz, LwU064* pRtnAddr);
    virtual void           FreeSysMem64(LwU064 addr);
};

class IMultiHeap2ModsProxy : public IMultiHeap2
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IMultiHeap2Funcs  m_IMultiHeap2Funcs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IMultiHeap2ModsProxy(void* pvIMultiHeap2, void* pSimLibModule);

    virtual ~IMultiHeap2ModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual int            DeviceAllocSysMem64(LwPciDev dev, LwU064 sz, LwU032 t, LwU064 align, LwU064* pRtnAddr);
    virtual int            AllocSysMem64(LwU064 sz, LwU032 t, LwU064 align, LwU064* pRtnAddr);
    virtual void           FreeSysMem64(LwU064 addr);
    virtual int            DeviceAllocSysMem32(LwPciDev dev, LwU032 sz, LwU032 t, LwU032 align, LwU032* pRtnAddr);
    virtual int            AllocSysMem32(LwU032 sz, LwU032 t, LwU032 align, LwU032* pRtnAddr);
    virtual void           FreeSysMem32(LwU032 addr);
    virtual bool           Support64();
};

class IMemoryModsProxy : public IMemory
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IMemoryFuncs  m_IMemoryFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IMemoryModsProxy(void* pvIMemory, void* pSimLibModule);

    virtual ~IMemoryModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual LwU008         MemRd08(LwU032 address);
    virtual LwU016         MemRd16(LwU032 address);
    virtual LwU032         MemRd32(LwU032 address);
    virtual LwU064         MemRd64(LwU032 address);
    virtual void           MemWr08(LwU032 address, LwU008 data);
    virtual void           MemWr16(LwU032 address, LwU016 data);
    virtual void           MemWr32(LwU032 address, LwU032 data);
    virtual void           MemWr64(LwU032 address, LwU064 data);
    virtual void           MemSet08(LwU032 address, LwU032 size, LwU008 data);
    virtual void           MemSet16(LwU032 address, LwU032 size, LwU016 data);
    virtual void           MemSet32(LwU032 address, LwU032 size, LwU032 data);
    virtual void           MemSet64(LwU032 address, LwU032 size, LwU064 data);
    virtual void           MemSetBlk(LwU032 address, LwU032 size, void* data, LwU032 data_size);
    virtual void           MemWrBlk(LwU032 address, const void* appdata, LwU032 count);
    virtual void           MemWrBlk32(LwU032 address, const void* appdata, LwU032 count);
    virtual void           MemRdBlk(LwU032 address, void* appdata, LwU032 count);
    virtual void           MemRdBlk32(LwU032 address, void* appdata, LwU032 count);
    virtual void           MemCpBlk(LwU032 address, LwU032 appdata, LwU032 count);
    virtual void           MemCpBlk32(LwU032 address, LwU032 appdata, LwU032 count);
};

class IChipModsProxy : public IChip
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IChipFuncs  m_IChipFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IChipModsProxy(void* pvIChip, void* pSimLibModule);

    virtual ~IChipModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual int            Startup(IIfaceObject* system, char** argv, int argc);
    virtual void           Shutdown();
    virtual int            AllocSysMem(int numBytes, LwU032* physAddr);
    virtual void           FreeSysMem(LwU032 physAddr);
    virtual void           ClockSimulator(LwS032 numClocks);
    virtual void           Delay(LwU032 numMicroSeconds);
    virtual int            EscapeWrite(char* path, LwU032 index, LwU032 size, LwU032 value);
    virtual int            EscapeRead(char* path, LwU032 index, LwU032 size, LwU032* value);
    virtual int            FindPCIDevice(LwU016 vendorId, LwU016 deviceId, int index, LwU032* address);
    virtual int            FindPCIClassCode(LwU032 classCode, int index, LwU032* address);
    virtual int            GetSimulatorTime(LwU064* simTime);
    virtual double         GetSimulatorTimeUnitsNS();
    virtual int            GetPCIBaseAddress(LwU032 cfgAddr, int index, LwU032* pAddress, LwU032* pSize);
    virtual IChip::ELEVEL  GetChipLevel();
protected:
    virtual int Init(char** argv, int argc)
    {
        MASSERT(!"IChip::Init is deprecated");
        return 0;
    }
};

class IPciDev2ModsProxy : public IPciDev2
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IPciDev2Funcs  m_IPciDev2Funcs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IPciDev2ModsProxy(void* pvIPciDev2, void* pSimLibModule);

    virtual ~IPciDev2ModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual LwErr          PciResetFunction(LwPciDev pciDev);
};

class IMapMemoryExtModsProxy : public IMapMemoryExt
{
private:
    // Pointer to wrapped sim object.
    void* m_pvthis;

    IMapMemoryExtFuncs  m_IMapMemoryExtFuncs;

    // The .so library module, for use with Xp::GetDLLProc.
    void* m_pSimLibModule;

public:
    IMapMemoryExtModsProxy(void* pvIMapMemoryExt, void* pSimLibModule);

    virtual ~IMapMemoryExtModsProxy();

    virtual void           AddRef();
    virtual void           Release();
    virtual IIfaceObject*  QueryIface(IID_TYPE id);
    virtual int            MapMemoryRegion(void** pReturnedVirtualAddress, LwU064 PhysicalAddress, size_t NumBytes, int Attrib, int Protect);
    virtual void           UnMapMemoryRegion(void* VirtualAddress);
    virtual int            RemapPages(void* VirtualAddress, LwU064 PhysicalAddress, size_t NumBytes, int Protect);
};
}; // namespace IFSPEC3
#endif // !defined(INCLUDED_IFSPEC3_SHIM_MODS_H)
