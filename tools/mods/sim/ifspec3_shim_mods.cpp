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

#include "ifspec3_shim_mods.h"
#include "assert.h"
#include <list>
#include <map>
#ifndef SOCKCHIP
#include "core/include/tee.h"
#else
#include "tee.h"
#endif
#include "simcallmutex.h"

#define DECLSPEC
namespace IFSPEC3
{

typedef list<IIfaceObject*> ProxyList;
static map<const void*, ProxyList> s_ModsProxies;

void FreeProxies(void* pSimLibModule)
{
    // First, free the mods-side proxies.
    ProxyList::iterator it = s_ModsProxies[pSimLibModule].begin();
    while (it != s_ModsProxies[pSimLibModule].end())
    {
        delete *it;
        ++it;
    }
    s_ModsProxies.erase(pSimLibModule);

    // Now call to the library to free the sim-side proxies.
    void (*pfFreeSimProxies)();
    pfFreeSimProxies = (void (*)()) Xp::GetDLLProc(pSimLibModule, "FreeSimProxies");
    if (0 == pfFreeSimProxies)
        Printf(Tee::PriDebug, "Can not find FreeSimProxies iface!\n");
    else
        (*pfFreeSimProxies)();
}

// Wrap a sim object with a proxy.
IIfaceObject* CreateProxyForSimObject
(
    IID_TYPE   id,
    void*      simObj,
    void*      pSimLibModule
)
{
    if (0 == simObj)
        return 0;

    IIfaceObject * tmp = 0;

    switch(id)
    {
        case IID_DEVICE_DISCOVERY_IFACE:
            tmp = new IDeviceDiscoveryModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IDeviceDiscovery to mods.\n");
            break;
        case IID_QUERY_IFACE:
            tmp = new IIfaceObjectModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IIfaceObject to mods.\n");
            break;
        case IID_INTERRUPT_MASK_IFACE:
            tmp = new IInterruptMaskModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IInterruptMask to mods.\n");
            break;
        case IID_MULTIHEAP_IFACE:
            tmp = new IMultiHeapModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IMultiHeap to mods.\n");
            break;
        case IID_IO_IFACE:
            tmp = new IIoModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IIo to mods.\n");
            break;
        case IID_AMODEL_BACKDOOR_IFACE:
            tmp = new IAModelBackdoorModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IAModelBackdoor to mods.\n");
            break;
        case IID_BUSMEM_IFACE:
            tmp = new IBusMemModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IBusMem to mods.\n");
            break;
        case IID_MAP_DMA_IFACE:
            tmp = new IMapDmaModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IMapDma to mods.\n");
            break;
        case IID_CLOCK_MGR_IFACE:
            tmp = new IClockMgrModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IClockMgr to mods.\n");
            break;
        case IID_GPUESCAPE_IFACE:
            tmp = new IGpuEscapeModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IGpuEscape to mods.\n");
            break;
        case IID_MAP_MEMORY_IFACE:
            tmp = new IMapMemoryModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IMapMemory to mods.\n");
            break;
        case IID_GPUESCAPE2_IFACE:
            tmp = new IGpuEscape2ModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IGpuEscape2 to mods.\n");
            break;
        case IID_CPUMODEL2_IFACE:
            tmp = new ICPUModel2ModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies ICPUModel2 to mods.\n");
            break;
        case IID_PCI_DEV_IFACE:
            tmp = new IPciDevModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IPciDev to mods.\n");
            break;
        case IID_INTERRUPT_MGR2_IFACE:
            tmp = new IInterruptMgr2ModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IInterruptMgr2 to mods.\n");
            break;
        case IID_INTERRUPT_MGR3_IFACE:
            tmp = new IInterruptMgr3ModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IInterruptMgr3 to mods.\n");
            break;
        case IID_PPC_IFACE:
            tmp = new IPpcModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IPpc to mods.\n");
            break;
        case IID_INTERRUPT_MGR_IFACE:
            tmp = new IInterruptMgrModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IInterruptMgr to mods.\n");
            break;
        case IID_MEMALLOC64_IFACE:
            tmp = new IMemAlloc64ModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IMemAlloc64 to mods.\n");
            break;
        case IID_MULTIHEAP2_IFACE:
            tmp = new IMultiHeap2ModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IMultiHeap2 to mods.\n");
            break;
        case IID_MEMORY_IFACE:
            tmp = new IMemoryModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IMemory to mods.\n");
            break;
        case IID_CHIP_IFACE:
            tmp = new IChipModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IChip to mods.\n");
            break;
        case IID_PCI_DEV2_IFACE:
            tmp = new IPciDev2ModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IPciDev2 to mods.\n");
            break;
        case IID_MAP_MEMORYEXT_IFACE:
            tmp = new IMapMemoryExtModsProxy(simObj, pSimLibModule);
            Printf(Tee::PriDebug, "IFSPEC3: sim supplies IMapMemoryExt to mods.\n");
            break;
        default:
             MASSERT(!"Unknown interface");
             return 0;
    }
    s_ModsProxies[pSimLibModule].push_back(tmp);
    return tmp;
}

// === IDeviceDiscoveryModsProxy (mods-side wrapper for sim implementation) ===

IDeviceDiscoveryModsProxy::IDeviceDiscoveryModsProxy(void* pvIDeviceDiscovery, void* pSimLibModule)
  : m_pvthis(pvIDeviceDiscovery),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IDeviceDiscoveryFuncs.AddRef = (FN_PTR_IDeviceDiscovery_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IDeviceDiscovery_AddRef");
    m_IDeviceDiscoveryFuncs.Release = (FN_PTR_IDeviceDiscovery_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IDeviceDiscovery_Release");
    m_IDeviceDiscoveryFuncs.QueryIface = (FN_PTR_IDeviceDiscovery_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IDeviceDiscovery_QueryIface");
    m_IDeviceDiscoveryFuncs.HasPciBus = (FN_PTR_IDeviceDiscovery_HasPciBus) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IDeviceDiscovery_HasPciBus");
    m_IDeviceDiscoveryFuncs.GetNumDevices = (FN_PTR_IDeviceDiscovery_GetNumDevices) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IDeviceDiscovery_GetNumDevices");
    m_IDeviceDiscoveryFuncs.GetNumBARs = (FN_PTR_IDeviceDiscovery_GetNumBARs) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IDeviceDiscovery_GetNumBARs");
    m_IDeviceDiscoveryFuncs.GetDeviceBAR = (FN_PTR_IDeviceDiscovery_GetDeviceBAR) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IDeviceDiscovery_GetDeviceBAR");

    MASSERT(m_IDeviceDiscoveryFuncs.SanityCheck());
}

IDeviceDiscoveryModsProxy::~IDeviceDiscoveryModsProxy ()
{
}

void IDeviceDiscoveryModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IDeviceDiscoveryFuncs.AddRef)(m_pvthis);
}

void IDeviceDiscoveryModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IDeviceDiscoveryFuncs.Release)(m_pvthis);
}

IIfaceObject* IDeviceDiscoveryModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IDeviceDiscoveryFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

int IDeviceDiscoveryModsProxy::HasPciBus()
{
    SimCallMutex m;
    return (*m_IDeviceDiscoveryFuncs.HasPciBus)(m_pvthis);
}

int IDeviceDiscoveryModsProxy::GetNumDevices()
{
    SimCallMutex m;
    return (*m_IDeviceDiscoveryFuncs.GetNumDevices)(m_pvthis);
}

int IDeviceDiscoveryModsProxy::GetNumBARs(int devIdx)
{
    SimCallMutex m;
    return (*m_IDeviceDiscoveryFuncs.GetNumBARs)(m_pvthis, devIdx);
}

int IDeviceDiscoveryModsProxy::GetDeviceBAR(int devIdx, int barIdx, LwU064* pPhysAddr, LwU064* pSize)
{
    SimCallMutex m;
    return (*m_IDeviceDiscoveryFuncs.GetDeviceBAR)(m_pvthis, devIdx, barIdx, pPhysAddr, pSize);
}

// === IIfaceObjectModsProxy (mods-side wrapper for sim implementation) ===

IIfaceObjectModsProxy::IIfaceObjectModsProxy(void* pvIIfaceObject, void* pSimLibModule)
  : m_pvthis(pvIIfaceObject),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IIfaceObjectFuncs.AddRef = (FN_PTR_IIfaceObject_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIfaceObject_AddRef");
    m_IIfaceObjectFuncs.Release = (FN_PTR_IIfaceObject_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIfaceObject_Release");
    m_IIfaceObjectFuncs.QueryIface = (FN_PTR_IIfaceObject_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIfaceObject_QueryIface");

    MASSERT(m_IIfaceObjectFuncs.SanityCheck());
}

IIfaceObjectModsProxy::~IIfaceObjectModsProxy ()
{
}

void IIfaceObjectModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IIfaceObjectFuncs.AddRef)(m_pvthis);
}

void IIfaceObjectModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IIfaceObjectFuncs.Release)(m_pvthis);
}

IIfaceObject* IIfaceObjectModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IIfaceObjectFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

// === IInterruptMaskModsProxy (mods-side wrapper for sim implementation) ===

IInterruptMaskModsProxy::IInterruptMaskModsProxy(void* pvIInterruptMask, void* pSimLibModule)
  : m_pvthis(pvIInterruptMask),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IInterruptMaskFuncs.AddRef = (FN_PTR_IInterruptMask_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMask_AddRef");
    m_IInterruptMaskFuncs.Release = (FN_PTR_IInterruptMask_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMask_Release");
    m_IInterruptMaskFuncs.QueryIface = (FN_PTR_IInterruptMask_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMask_QueryIface");
    m_IInterruptMaskFuncs.SetInterruptMask = (FN_PTR_IInterruptMask_SetInterruptMask) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMask_SetInterruptMask");
    m_IInterruptMaskFuncs.SetInterruptMultiMask = (FN_PTR_IInterruptMask_SetInterruptMultiMask) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMask_SetInterruptMultiMask");

    MASSERT(m_IInterruptMaskFuncs.SanityCheck());
}

IInterruptMaskModsProxy::~IInterruptMaskModsProxy ()
{
}

void IInterruptMaskModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IInterruptMaskFuncs.AddRef)(m_pvthis);
}

void IInterruptMaskModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IInterruptMaskFuncs.Release)(m_pvthis);
}

IIfaceObject* IInterruptMaskModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IInterruptMaskFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

int IInterruptMaskModsProxy::SetInterruptMask(LwU032 irqNumber, LwU064 barAddr, LwU032 barSize, LwU032 regOffset, LwU064 andMask, LwU064 orMask, LwU008 irqType, LwU008 maskType, LwU016 domain, LwU016 bus, LwU016 device, LwU016 function)
{
    SimCallMutex m;
    return (*m_IInterruptMaskFuncs.SetInterruptMask)(m_pvthis, irqNumber, barAddr, barSize, regOffset, andMask, orMask, irqType, maskType, domain, bus, device, function);
}

int IInterruptMaskModsProxy::SetInterruptMultiMask(LwU032 irqNumber, LwU008 irqType, LwU064 barAddr, LwU032 barSize, const PciInfo* pciInfo, LwU032 maskInfoCount, const MaskInfo* pMaskInfoList)
{
    SimCallMutex m;
    return (*m_IInterruptMaskFuncs.SetInterruptMultiMask)(m_pvthis, irqNumber, irqType, barAddr, barSize, pciInfo, maskInfoCount, pMaskInfoList);
}

// === IMultiHeapModsProxy (mods-side wrapper for sim implementation) ===

IMultiHeapModsProxy::IMultiHeapModsProxy(void* pvIMultiHeap, void* pSimLibModule)
  : m_pvthis(pvIMultiHeap),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IMultiHeapFuncs.AddRef = (FN_PTR_IMultiHeap_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap_AddRef");
    m_IMultiHeapFuncs.Release = (FN_PTR_IMultiHeap_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap_Release");
    m_IMultiHeapFuncs.QueryIface = (FN_PTR_IMultiHeap_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap_QueryIface");
    m_IMultiHeapFuncs.AllocSysMem64 = (FN_PTR_IMultiHeap_AllocSysMem64) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap_AllocSysMem64");
    m_IMultiHeapFuncs.FreeSysMem64 = (FN_PTR_IMultiHeap_FreeSysMem64) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap_FreeSysMem64");
    m_IMultiHeapFuncs.AllocSysMem32 = (FN_PTR_IMultiHeap_AllocSysMem32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap_AllocSysMem32");
    m_IMultiHeapFuncs.FreeSysMem32 = (FN_PTR_IMultiHeap_FreeSysMem32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap_FreeSysMem32");
    m_IMultiHeapFuncs.Support64 = (FN_PTR_IMultiHeap_Support64) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap_Support64");

    MASSERT(m_IMultiHeapFuncs.SanityCheck());
}

IMultiHeapModsProxy::~IMultiHeapModsProxy ()
{
}

void IMultiHeapModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IMultiHeapFuncs.AddRef)(m_pvthis);
}

void IMultiHeapModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IMultiHeapFuncs.Release)(m_pvthis);
}

IIfaceObject* IMultiHeapModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IMultiHeapFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

int IMultiHeapModsProxy::AllocSysMem64(LwU064 sz, LwU032 t, LwU064 align, LwU064* pRtnAddr)
{
    SimCallMutex m;
    return (*m_IMultiHeapFuncs.AllocSysMem64)(m_pvthis, sz, t, align, pRtnAddr);
}

void IMultiHeapModsProxy::FreeSysMem64(LwU064 addr)
{
    SimCallMutex m;
    return (*m_IMultiHeapFuncs.FreeSysMem64)(m_pvthis, addr);
}

int IMultiHeapModsProxy::AllocSysMem32(LwU032 sz, LwU032 t, LwU032 align, LwU032* pRtnAddr)
{
    SimCallMutex m;
    return (*m_IMultiHeapFuncs.AllocSysMem32)(m_pvthis, sz, t, align, pRtnAddr);
}

void IMultiHeapModsProxy::FreeSysMem32(LwU032 addr)
{
    SimCallMutex m;
    return (*m_IMultiHeapFuncs.FreeSysMem32)(m_pvthis, addr);
}

bool IMultiHeapModsProxy::Support64()
{
    SimCallMutex m;
    return (*m_IMultiHeapFuncs.Support64)(m_pvthis);
}

// === IIoModsProxy (mods-side wrapper for sim implementation) ===

IIoModsProxy::IIoModsProxy(void* pvIIo, void* pSimLibModule)
  : m_pvthis(pvIIo),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IIoFuncs.AddRef = (FN_PTR_IIo_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_AddRef");
    m_IIoFuncs.Release = (FN_PTR_IIo_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_Release");
    m_IIoFuncs.QueryIface = (FN_PTR_IIo_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_QueryIface");
    m_IIoFuncs.IoRd08 = (FN_PTR_IIo_IoRd08) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_IoRd08");
    m_IIoFuncs.IoRd16 = (FN_PTR_IIo_IoRd16) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_IoRd16");
    m_IIoFuncs.IoRd32 = (FN_PTR_IIo_IoRd32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_IoRd32");
    m_IIoFuncs.IoWr08 = (FN_PTR_IIo_IoWr08) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_IoWr08");
    m_IIoFuncs.IoWr16 = (FN_PTR_IIo_IoWr16) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_IoWr16");
    m_IIoFuncs.IoWr32 = (FN_PTR_IIo_IoWr32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_IoWr32");
    m_IIoFuncs.CfgRd08 = (FN_PTR_IIo_CfgRd08) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_CfgRd08");
    m_IIoFuncs.CfgRd16 = (FN_PTR_IIo_CfgRd16) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_CfgRd16");
    m_IIoFuncs.CfgRd32 = (FN_PTR_IIo_CfgRd32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_CfgRd32");
    m_IIoFuncs.CfgWr08 = (FN_PTR_IIo_CfgWr08) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_CfgWr08");
    m_IIoFuncs.CfgWr16 = (FN_PTR_IIo_CfgWr16) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_CfgWr16");
    m_IIoFuncs.CfgWr32 = (FN_PTR_IIo_CfgWr32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IIo_CfgWr32");

    MASSERT(m_IIoFuncs.SanityCheck());
}

IIoModsProxy::~IIoModsProxy ()
{
}

void IIoModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IIoFuncs.AddRef)(m_pvthis);
}

void IIoModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IIoFuncs.Release)(m_pvthis);
}

IIfaceObject* IIoModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IIoFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

LwU008 IIoModsProxy::IoRd08(LwU016 address)
{
    SimCallMutex m;
    return (*m_IIoFuncs.IoRd08)(m_pvthis, address);
}

LwU016 IIoModsProxy::IoRd16(LwU016 address)
{
    SimCallMutex m;
    return (*m_IIoFuncs.IoRd16)(m_pvthis, address);
}

LwU032 IIoModsProxy::IoRd32(LwU016 address)
{
    SimCallMutex m;
    return (*m_IIoFuncs.IoRd32)(m_pvthis, address);
}

void IIoModsProxy::IoWr08(LwU016 address, LwU008 data)
{
    SimCallMutex m;
    return (*m_IIoFuncs.IoWr08)(m_pvthis, address, data);
}

void IIoModsProxy::IoWr16(LwU016 address, LwU016 data)
{
    SimCallMutex m;
    return (*m_IIoFuncs.IoWr16)(m_pvthis, address, data);
}

void IIoModsProxy::IoWr32(LwU016 address, LwU032 data)
{
    SimCallMutex m;
    return (*m_IIoFuncs.IoWr32)(m_pvthis, address, data);
}

LwU008 IIoModsProxy::CfgRd08(LwU032 address)
{
    SimCallMutex m;
    return (*m_IIoFuncs.CfgRd08)(m_pvthis, address);
}

LwU016 IIoModsProxy::CfgRd16(LwU032 address)
{
    SimCallMutex m;
    return (*m_IIoFuncs.CfgRd16)(m_pvthis, address);
}

LwU032 IIoModsProxy::CfgRd32(LwU032 address)
{
    SimCallMutex m;
    return (*m_IIoFuncs.CfgRd32)(m_pvthis, address);
}

void IIoModsProxy::CfgWr08(LwU032 address, LwU008 data)
{
    SimCallMutex m;
    return (*m_IIoFuncs.CfgWr08)(m_pvthis, address, data);
}

void IIoModsProxy::CfgWr16(LwU032 address, LwU016 data)
{
    SimCallMutex m;
    return (*m_IIoFuncs.CfgWr16)(m_pvthis, address, data);
}

void IIoModsProxy::CfgWr32(LwU032 address, LwU032 data)
{
    SimCallMutex m;
    return (*m_IIoFuncs.CfgWr32)(m_pvthis, address, data);
}

// === IAModelBackdoorModsProxy (mods-side wrapper for sim implementation) ===

IAModelBackdoorModsProxy::IAModelBackdoorModsProxy(void* pvIAModelBackdoor, void* pSimLibModule)
  : m_pvthis(pvIAModelBackdoor),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IAModelBackdoorFuncs.AddRef = (FN_PTR_IAModelBackdoor_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IAModelBackdoor_AddRef");
    m_IAModelBackdoorFuncs.Release = (FN_PTR_IAModelBackdoor_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IAModelBackdoor_Release");
    m_IAModelBackdoorFuncs.QueryIface = (FN_PTR_IAModelBackdoor_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IAModelBackdoor_QueryIface");
    m_IAModelBackdoorFuncs.AllocChannelDma = (FN_PTR_IAModelBackdoor_AllocChannelDma) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IAModelBackdoor_AllocChannelDma");
    m_IAModelBackdoorFuncs.FreeChannel = (FN_PTR_IAModelBackdoor_FreeChannel) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IAModelBackdoor_FreeChannel");
    m_IAModelBackdoorFuncs.AllocContextDma = (FN_PTR_IAModelBackdoor_AllocContextDma) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IAModelBackdoor_AllocContextDma");
    m_IAModelBackdoorFuncs.AllocObject = (FN_PTR_IAModelBackdoor_AllocObject) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IAModelBackdoor_AllocObject");
    m_IAModelBackdoorFuncs.FreeObject = (FN_PTR_IAModelBackdoor_FreeObject) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IAModelBackdoor_FreeObject");
    m_IAModelBackdoorFuncs.AllocChannelGpFifo = (FN_PTR_IAModelBackdoor_AllocChannelGpFifo) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IAModelBackdoor_AllocChannelGpFifo");
    m_IAModelBackdoorFuncs.PassAdditionalVerification = (FN_PTR_IAModelBackdoor_PassAdditionalVerification) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IAModelBackdoor_PassAdditionalVerification");
    m_IAModelBackdoorFuncs.GetModelIdentifierString = (FN_PTR_IAModelBackdoor_GetModelIdentifierString) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IAModelBackdoor_GetModelIdentifierString");

    MASSERT(m_IAModelBackdoorFuncs.SanityCheck());
}

IAModelBackdoorModsProxy::~IAModelBackdoorModsProxy ()
{
}

void IAModelBackdoorModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IAModelBackdoorFuncs.AddRef)(m_pvthis);
}

void IAModelBackdoorModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IAModelBackdoorFuncs.Release)(m_pvthis);
}

IIfaceObject* IAModelBackdoorModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IAModelBackdoorFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

void IAModelBackdoorModsProxy::AllocChannelDma(LwU032 ChID, LwU032 Class, LwU032 CtxDma, LwU032 ErrorNotifierCtxDma)
{
    SimCallMutex m;
    return (*m_IAModelBackdoorFuncs.AllocChannelDma)(m_pvthis, ChID, Class, CtxDma, ErrorNotifierCtxDma);
}

void IAModelBackdoorModsProxy::FreeChannel(LwU032 ChID)
{
    SimCallMutex m;
    return (*m_IAModelBackdoorFuncs.FreeChannel)(m_pvthis, ChID);
}

void IAModelBackdoorModsProxy::AllocContextDma(LwU032 ChID, LwU032 Handle, LwU032 Class, LwU032 target, LwU032 Limit, LwU032 Base, LwU032 Protect, LwU032* PageTable)
{
    SimCallMutex m;
    return (*m_IAModelBackdoorFuncs.AllocContextDma)(m_pvthis, ChID, Handle, Class, target, Limit, Base, Protect, PageTable);
}

void IAModelBackdoorModsProxy::AllocObject(LwU032 ChID, LwU032 Handle, LwU032 Class)
{
    SimCallMutex m;
    return (*m_IAModelBackdoorFuncs.AllocObject)(m_pvthis, ChID, Handle, Class);
}

void IAModelBackdoorModsProxy::FreeObject(LwU032 ChID, LwU032 Handle)
{
    SimCallMutex m;
    return (*m_IAModelBackdoorFuncs.FreeObject)(m_pvthis, ChID, Handle);
}

void IAModelBackdoorModsProxy::AllocChannelGpFifo(LwU032 ChID, LwU032 Class, LwU032 CtxDma, LwU064 GpFifoOffset, LwU032 GpFifoEntries, LwU032 ErrorNotifierCtxDma)
{
    SimCallMutex m;
    return (*m_IAModelBackdoorFuncs.AllocChannelGpFifo)(m_pvthis, ChID, Class, CtxDma, GpFifoOffset, GpFifoEntries, ErrorNotifierCtxDma);
}

bool IAModelBackdoorModsProxy::PassAdditionalVerification(const char* traceFileName)
{
    SimCallMutex m;
    return (*m_IAModelBackdoorFuncs.PassAdditionalVerification)(m_pvthis, traceFileName);
}

const char* IAModelBackdoorModsProxy::GetModelIdentifierString()
{
    SimCallMutex m;
    return (*m_IAModelBackdoorFuncs.GetModelIdentifierString)(m_pvthis);
}

// === IBusMemModsProxy (mods-side wrapper for sim implementation) ===

IBusMemModsProxy::IBusMemModsProxy(void* pvIBusMem, void* pSimLibModule)
  : m_pvthis(pvIBusMem),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IBusMemFuncs.AddRef = (FN_PTR_IBusMem_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IBusMem_AddRef");
    m_IBusMemFuncs.Release = (FN_PTR_IBusMem_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IBusMem_Release");
    m_IBusMemFuncs.QueryIface = (FN_PTR_IBusMem_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IBusMem_QueryIface");
    m_IBusMemFuncs.BusMemWrBlk = (FN_PTR_IBusMem_BusMemWrBlk) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IBusMem_BusMemWrBlk");
    m_IBusMemFuncs.BusMemRdBlk = (FN_PTR_IBusMem_BusMemRdBlk) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IBusMem_BusMemRdBlk");
    m_IBusMemFuncs.BusMemCpBlk = (FN_PTR_IBusMem_BusMemCpBlk) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IBusMem_BusMemCpBlk");
    m_IBusMemFuncs.BusMemSetBlk = (FN_PTR_IBusMem_BusMemSetBlk) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IBusMem_BusMemSetBlk");

    MASSERT(m_IBusMemFuncs.SanityCheck());
}

IBusMemModsProxy::~IBusMemModsProxy ()
{
}

void IBusMemModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IBusMemFuncs.AddRef)(m_pvthis);
}

void IBusMemModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IBusMemFuncs.Release)(m_pvthis);
}

IIfaceObject* IBusMemModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IBusMemFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

BusMemRet IBusMemModsProxy::BusMemWrBlk(LwU064 address, const void* appdata, LwU032 count)
{
    SimCallMutex m;
    return (*m_IBusMemFuncs.BusMemWrBlk)(m_pvthis, address, appdata, count);
}

BusMemRet IBusMemModsProxy::BusMemRdBlk(LwU064 address, void* appdata, LwU032 count)
{
    SimCallMutex m;
    return (*m_IBusMemFuncs.BusMemRdBlk)(m_pvthis, address, appdata, count);
}

BusMemRet IBusMemModsProxy::BusMemCpBlk(LwU064 dest, LwU064 source, LwU032 count)
{
    SimCallMutex m;
    return (*m_IBusMemFuncs.BusMemCpBlk)(m_pvthis, dest, source, count);
}

BusMemRet IBusMemModsProxy::BusMemSetBlk(LwU064 address, LwU032 size, void* data, LwU032 data_size)
{
    SimCallMutex m;
    return (*m_IBusMemFuncs.BusMemSetBlk)(m_pvthis, address, size, data, data_size);
}

// === IMapDmaModsProxy (mods-side wrapper for sim implementation) ===

IMapDmaModsProxy::IMapDmaModsProxy(void* pvIMapDma, void* pSimLibModule)
  : m_pvthis(pvIMapDma),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IMapDmaFuncs.AddRef = (FN_PTR_IMapDma_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapDma_AddRef");
    m_IMapDmaFuncs.Release = (FN_PTR_IMapDma_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapDma_Release");
    m_IMapDmaFuncs.QueryIface = (FN_PTR_IMapDma_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapDma_QueryIface");
    m_IMapDmaFuncs.SetDmaMask = (FN_PTR_IMapDma_SetDmaMask) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapDma_SetDmaMask");
    m_IMapDmaFuncs.DmaMapMemory = (FN_PTR_IMapDma_DmaMapMemory) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapDma_DmaMapMemory");
    m_IMapDmaFuncs.DmaUnmapMemory = (FN_PTR_IMapDma_DmaUnmapMemory) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapDma_DmaUnmapMemory");
    m_IMapDmaFuncs.GetDmaMappedAddress = (FN_PTR_IMapDma_GetDmaMappedAddress) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapDma_GetDmaMappedAddress");

    MASSERT(m_IMapDmaFuncs.SanityCheck());
}

IMapDmaModsProxy::~IMapDmaModsProxy ()
{
}

void IMapDmaModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IMapDmaFuncs.AddRef)(m_pvthis);
}

void IMapDmaModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IMapDmaFuncs.Release)(m_pvthis);
}

IIfaceObject* IMapDmaModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IMapDmaFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

int IMapDmaModsProxy::SetDmaMask(LwPciDev pciDev, LwU032 bits)
{
    SimCallMutex m;
    return (*m_IMapDmaFuncs.SetDmaMask)(m_pvthis, pciDev, bits);
}

int IMapDmaModsProxy::DmaMapMemory(LwPciDev pciDev, LwU064 physAddr)
{
    SimCallMutex m;
    return (*m_IMapDmaFuncs.DmaMapMemory)(m_pvthis, pciDev, physAddr);
}

int IMapDmaModsProxy::DmaUnmapMemory(LwPciDev pciDev, LwU064 physAddr)
{
    SimCallMutex m;
    return (*m_IMapDmaFuncs.DmaUnmapMemory)(m_pvthis, pciDev, physAddr);
}

int IMapDmaModsProxy::GetDmaMappedAddress(LwPciDev pciDev, LwU064 physAddr, LwU064* pIoVirtAddr)
{
    SimCallMutex m;
    return (*m_IMapDmaFuncs.GetDmaMappedAddress)(m_pvthis, pciDev, physAddr, pIoVirtAddr);
}

// === IClockMgrModsProxy (mods-side wrapper for sim implementation) ===

IClockMgrModsProxy::IClockMgrModsProxy(void* pvIClockMgr, void* pSimLibModule)
  : m_pvthis(pvIClockMgr),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IClockMgrFuncs.AddRef = (FN_PTR_IClockMgr_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IClockMgr_AddRef");
    m_IClockMgrFuncs.Release = (FN_PTR_IClockMgr_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IClockMgr_Release");
    m_IClockMgrFuncs.QueryIface = (FN_PTR_IClockMgr_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IClockMgr_QueryIface");
    m_IClockMgrFuncs.GetClockHandle = (FN_PTR_IClockMgr_GetClockHandle) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IClockMgr_GetClockHandle");
    m_IClockMgrFuncs.GetClockParent = (FN_PTR_IClockMgr_GetClockParent) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IClockMgr_GetClockParent");
    m_IClockMgrFuncs.SetClockParent = (FN_PTR_IClockMgr_SetClockParent) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IClockMgr_SetClockParent");
    m_IClockMgrFuncs.GetClockEnabled = (FN_PTR_IClockMgr_GetClockEnabled) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IClockMgr_GetClockEnabled");
    m_IClockMgrFuncs.SetClockEnabled = (FN_PTR_IClockMgr_SetClockEnabled) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IClockMgr_SetClockEnabled");
    m_IClockMgrFuncs.GetClockRate = (FN_PTR_IClockMgr_GetClockRate) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IClockMgr_GetClockRate");
    m_IClockMgrFuncs.SetClockRate = (FN_PTR_IClockMgr_SetClockRate) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IClockMgr_SetClockRate");
    m_IClockMgrFuncs.AssertClockReset = (FN_PTR_IClockMgr_AssertClockReset) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IClockMgr_AssertClockReset");

    MASSERT(m_IClockMgrFuncs.SanityCheck());
}

IClockMgrModsProxy::~IClockMgrModsProxy ()
{
}

void IClockMgrModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IClockMgrFuncs.AddRef)(m_pvthis);
}

void IClockMgrModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IClockMgrFuncs.Release)(m_pvthis);
}

IIfaceObject* IClockMgrModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IClockMgrFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

int IClockMgrModsProxy::GetClockHandle(const char* clkDevice, const char* clkController, LwU064* pHandle)
{
    SimCallMutex m;
    return (*m_IClockMgrFuncs.GetClockHandle)(m_pvthis, clkDevice, clkController, pHandle);
}

int IClockMgrModsProxy::GetClockParent(LwU064 handle, LwU064* pParentHandle)
{
    SimCallMutex m;
    return (*m_IClockMgrFuncs.GetClockParent)(m_pvthis, handle, pParentHandle);
}

int IClockMgrModsProxy::SetClockParent(LwU064 handle, LwU064 parentHandle)
{
    SimCallMutex m;
    return (*m_IClockMgrFuncs.SetClockParent)(m_pvthis, handle, parentHandle);
}

int IClockMgrModsProxy::GetClockEnabled(LwU064 handle, LwU032* pEnableCount)
{
    SimCallMutex m;
    return (*m_IClockMgrFuncs.GetClockEnabled)(m_pvthis, handle, pEnableCount);
}

int IClockMgrModsProxy::SetClockEnabled(LwU064 handle, int enabled)
{
    SimCallMutex m;
    return (*m_IClockMgrFuncs.SetClockEnabled)(m_pvthis, handle, enabled);
}

int IClockMgrModsProxy::GetClockRate(LwU064 handle, LwU064* pRateHz)
{
    SimCallMutex m;
    return (*m_IClockMgrFuncs.GetClockRate)(m_pvthis, handle, pRateHz);
}

int IClockMgrModsProxy::SetClockRate(LwU064 handle, LwU064 rateHz)
{
    SimCallMutex m;
    return (*m_IClockMgrFuncs.SetClockRate)(m_pvthis, handle, rateHz);
}

int IClockMgrModsProxy::AssertClockReset(LwU064 handle, int assertReset)
{
    SimCallMutex m;
    return (*m_IClockMgrFuncs.AssertClockReset)(m_pvthis, handle, assertReset);
}

// === IGpuEscapeModsProxy (mods-side wrapper for sim implementation) ===

IGpuEscapeModsProxy::IGpuEscapeModsProxy(void* pvIGpuEscape, void* pSimLibModule)
  : m_pvthis(pvIGpuEscape),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IGpuEscapeFuncs.AddRef = (FN_PTR_IGpuEscape_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IGpuEscape_AddRef");
    m_IGpuEscapeFuncs.Release = (FN_PTR_IGpuEscape_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IGpuEscape_Release");
    m_IGpuEscapeFuncs.QueryIface = (FN_PTR_IGpuEscape_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IGpuEscape_QueryIface");
    m_IGpuEscapeFuncs.EscapeWrite = (FN_PTR_IGpuEscape_EscapeWrite) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IGpuEscape_EscapeWrite");
    m_IGpuEscapeFuncs.EscapeRead = (FN_PTR_IGpuEscape_EscapeRead) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IGpuEscape_EscapeRead");
    m_IGpuEscapeFuncs.GetGpuId = (FN_PTR_IGpuEscape_GetGpuId) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IGpuEscape_GetGpuId");

    MASSERT(m_IGpuEscapeFuncs.SanityCheck());
}

IGpuEscapeModsProxy::~IGpuEscapeModsProxy ()
{
}

void IGpuEscapeModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IGpuEscapeFuncs.AddRef)(m_pvthis);
}

void IGpuEscapeModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IGpuEscapeFuncs.Release)(m_pvthis);
}

IIfaceObject* IGpuEscapeModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IGpuEscapeFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

int IGpuEscapeModsProxy::EscapeWrite(LwU032 GpuId, const char* path, LwU032 index, LwU032 size, LwU032 value)
{
    SimCallMutex m;
    return (*m_IGpuEscapeFuncs.EscapeWrite)(m_pvthis, GpuId, path, index, size, value);
}

int IGpuEscapeModsProxy::EscapeRead(LwU032 GpuId, const char* path, LwU032 index, LwU032 size, LwU032* value)
{
    SimCallMutex m;
    return (*m_IGpuEscapeFuncs.EscapeRead)(m_pvthis, GpuId, path, index, size, value);
}

int IGpuEscapeModsProxy::GetGpuId(LwU032 bus, LwU032 dev, LwU032 fnc)
{
    SimCallMutex m;
    return (*m_IGpuEscapeFuncs.GetGpuId)(m_pvthis, bus, dev, fnc);
}

// === IMapMemoryModsProxy (mods-side wrapper for sim implementation) ===

IMapMemoryModsProxy::IMapMemoryModsProxy(void* pvIMapMemory, void* pSimLibModule)
  : m_pvthis(pvIMapMemory),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IMapMemoryFuncs.AddRef = (FN_PTR_IMapMemory_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapMemory_AddRef");
    m_IMapMemoryFuncs.Release = (FN_PTR_IMapMemory_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapMemory_Release");
    m_IMapMemoryFuncs.QueryIface = (FN_PTR_IMapMemory_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapMemory_QueryIface");
    m_IMapMemoryFuncs.GetPhysicalAddress = (FN_PTR_IMapMemory_GetPhysicalAddress) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapMemory_GetPhysicalAddress");
    m_IMapMemoryFuncs.LinearToPhysical = (FN_PTR_IMapMemory_LinearToPhysical) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapMemory_LinearToPhysical");
    m_IMapMemoryFuncs.PhysicalToLinear = (FN_PTR_IMapMemory_PhysicalToLinear) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapMemory_PhysicalToLinear");
    m_IMapMemoryFuncs.MapMemoryRegion = (FN_PTR_IMapMemory_MapMemoryRegion) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapMemory_MapMemoryRegion");

    MASSERT(m_IMapMemoryFuncs.SanityCheck());
}

IMapMemoryModsProxy::~IMapMemoryModsProxy ()
{
}

void IMapMemoryModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IMapMemoryFuncs.AddRef)(m_pvthis);
}

void IMapMemoryModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IMapMemoryFuncs.Release)(m_pvthis);
}

IIfaceObject* IMapMemoryModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IMapMemoryFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

void IMapMemoryModsProxy::GetPhysicalAddress(void* virtual_base, LwU032 virtual_size, LwU032* physical_base_p, LwU032* physical_size_p)
{
    SimCallMutex m;
    return (*m_IMapMemoryFuncs.GetPhysicalAddress)(m_pvthis, virtual_base, virtual_size, physical_base_p, physical_size_p);
}

LwU032 IMapMemoryModsProxy::LinearToPhysical(void* LinearAddress)
{
    SimCallMutex m;
    return (*m_IMapMemoryFuncs.LinearToPhysical)(m_pvthis, LinearAddress);
}

void* IMapMemoryModsProxy::PhysicalToLinear(LwU032 PhysicalAddress)
{
    SimCallMutex m;
    return (*m_IMapMemoryFuncs.PhysicalToLinear)(m_pvthis, PhysicalAddress);
}

int IMapMemoryModsProxy::MapMemoryRegion(LwU032 base, LwU032 size, LwU016 type)
{
    SimCallMutex m;
    return (*m_IMapMemoryFuncs.MapMemoryRegion)(m_pvthis, base, size, type);
}

// === IGpuEscape2ModsProxy (mods-side wrapper for sim implementation) ===

IGpuEscape2ModsProxy::IGpuEscape2ModsProxy(void* pvIGpuEscape2, void* pSimLibModule)
  : m_pvthis(pvIGpuEscape2),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IGpuEscape2Funcs.AddRef = (FN_PTR_IGpuEscape2_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IGpuEscape2_AddRef");
    m_IGpuEscape2Funcs.Release = (FN_PTR_IGpuEscape2_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IGpuEscape2_Release");
    m_IGpuEscape2Funcs.QueryIface = (FN_PTR_IGpuEscape2_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IGpuEscape2_QueryIface");
    m_IGpuEscape2Funcs.EscapeWriteBuffer = (FN_PTR_IGpuEscape2_EscapeWriteBuffer) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IGpuEscape2_EscapeWriteBuffer");
    m_IGpuEscape2Funcs.EscapeReadBuffer = (FN_PTR_IGpuEscape2_EscapeReadBuffer) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IGpuEscape2_EscapeReadBuffer");
    m_IGpuEscape2Funcs.GetGpuId = (FN_PTR_IGpuEscape2_GetGpuId) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IGpuEscape2_GetGpuId");

    MASSERT(m_IGpuEscape2Funcs.SanityCheck());
}

IGpuEscape2ModsProxy::~IGpuEscape2ModsProxy ()
{
}

void IGpuEscape2ModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IGpuEscape2Funcs.AddRef)(m_pvthis);
}

void IGpuEscape2ModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IGpuEscape2Funcs.Release)(m_pvthis);
}

IIfaceObject* IGpuEscape2ModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IGpuEscape2Funcs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

LwErr IGpuEscape2ModsProxy::EscapeWriteBuffer(LwU032 gpuId, const char* path, LwU032 index, size_t size, const void* buf)
{
    SimCallMutex m;
    return (*m_IGpuEscape2Funcs.EscapeWriteBuffer)(m_pvthis, gpuId, path, index, size, buf);
}

LwErr IGpuEscape2ModsProxy::EscapeReadBuffer(LwU032 GpuId, const char* path, LwU032 index, size_t size, void* buf)
{
    SimCallMutex m;
    return (*m_IGpuEscape2Funcs.EscapeReadBuffer)(m_pvthis, GpuId, path, index, size, buf);
}

LwU032 IGpuEscape2ModsProxy::GetGpuId(LwU032 bus, LwU032 dev, LwU032 fnc)
{
    SimCallMutex m;
    return (*m_IGpuEscape2Funcs.GetGpuId)(m_pvthis, bus, dev, fnc);
}

// === ICPUModel2ModsProxy (mods-side wrapper for sim implementation) ===

ICPUModel2ModsProxy::ICPUModel2ModsProxy(void* pvICPUModel2, void* pSimLibModule)
  : m_pvthis(pvICPUModel2),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_ICPUModel2Funcs.AddRef = (FN_PTR_ICPUModel2_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_ICPUModel2_AddRef");
    m_ICPUModel2Funcs.Release = (FN_PTR_ICPUModel2_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_ICPUModel2_Release");
    m_ICPUModel2Funcs.QueryIface = (FN_PTR_ICPUModel2_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_ICPUModel2_QueryIface");
    m_ICPUModel2Funcs.CPUModelInitialize = (FN_PTR_ICPUModel2_CPUModelInitialize) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_ICPUModel2_CPUModelInitialize");
    m_ICPUModel2Funcs.CPUModelEnableResponse = (FN_PTR_ICPUModel2_CPUModelEnableResponse) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_ICPUModel2_CPUModelEnableResponse");
    m_ICPUModel2Funcs.CPUModelEnableResponseSpecific = (FN_PTR_ICPUModel2_CPUModelEnableResponseSpecific) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_ICPUModel2_CPUModelEnableResponseSpecific");
    m_ICPUModel2Funcs.CPUModelHasResponse = (FN_PTR_ICPUModel2_CPUModelHasResponse) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_ICPUModel2_CPUModelHasResponse");
    m_ICPUModel2Funcs.CPUModelGetResponse = (FN_PTR_ICPUModel2_CPUModelGetResponse) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_ICPUModel2_CPUModelGetResponse");
    m_ICPUModel2Funcs.CPUModelRead = (FN_PTR_ICPUModel2_CPUModelRead) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_ICPUModel2_CPUModelRead");
    m_ICPUModel2Funcs.CPUModelGetCacheData = (FN_PTR_ICPUModel2_CPUModelGetCacheData) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_ICPUModel2_CPUModelGetCacheData");
    m_ICPUModel2Funcs.CPUModelWrite = (FN_PTR_ICPUModel2_CPUModelWrite) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_ICPUModel2_CPUModelWrite");
    m_ICPUModel2Funcs.CPUModelAtsShootDown = (FN_PTR_ICPUModel2_CPUModelAtsShootDown) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_ICPUModel2_CPUModelAtsShootDown");

    MASSERT(m_ICPUModel2Funcs.SanityCheck());
}

ICPUModel2ModsProxy::~ICPUModel2ModsProxy ()
{
}

void ICPUModel2ModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_ICPUModel2Funcs.AddRef)(m_pvthis);
}

void ICPUModel2ModsProxy::Release()
{
    SimCallMutex m;
    return (*m_ICPUModel2Funcs.Release)(m_pvthis);
}

IIfaceObject* ICPUModel2ModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_ICPUModel2Funcs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

void ICPUModel2ModsProxy::CPUModelInitialize()
{
    SimCallMutex m;
    return (*m_ICPUModel2Funcs.CPUModelInitialize)(m_pvthis);
}

void ICPUModel2ModsProxy::CPUModelEnableResponse(bool enable)
{
    SimCallMutex m;
    return (*m_ICPUModel2Funcs.CPUModelEnableResponse)(m_pvthis, enable);
}

void ICPUModel2ModsProxy::CPUModelEnableResponseSpecific(bool enable, CPUModelEvent event)
{
    SimCallMutex m;
    return (*m_ICPUModel2Funcs.CPUModelEnableResponseSpecific)(m_pvthis, enable, event);
}

bool ICPUModel2ModsProxy::CPUModelHasResponse()
{
    SimCallMutex m;
    return (*m_ICPUModel2Funcs.CPUModelHasResponse)(m_pvthis);
}

void ICPUModel2ModsProxy::CPUModelGetResponse(CPUModelResponse * response)
{
    SimCallMutex m;
    return (*m_ICPUModel2Funcs.CPUModelGetResponse)(m_pvthis, response);
}

CPUModelRet ICPUModel2ModsProxy::CPUModelRead(LwU032 uniqueId, LwU064 address, LwU032 sizeBytes, bool isCoherentlyCaching, bool isProbing)
{
    SimCallMutex m;
    return (*m_ICPUModel2Funcs.CPUModelRead)(m_pvthis, uniqueId, address, sizeBytes, isCoherentlyCaching, isProbing);
}

CPUModelRet ICPUModel2ModsProxy::CPUModelGetCacheData(LwU064 gpa, LwU064 * data)
{
    SimCallMutex m;
    return (*m_ICPUModel2Funcs.CPUModelGetCacheData)(m_pvthis, gpa, data);
}

CPUModelRet ICPUModel2ModsProxy::CPUModelWrite(LwU032 uniqueId, LwU064 address, LwU032 offset, LwU064 data, LwU032 sizeBytes, bool isCoherentlyCaching, bool isProbing, bool isPosted)
{
    SimCallMutex m;
    return (*m_ICPUModel2Funcs.CPUModelWrite)(m_pvthis, uniqueId, address, offset, data, sizeBytes, isCoherentlyCaching, isProbing, isPosted);
}

void ICPUModel2ModsProxy::CPUModelAtsShootDown(LwU032 uniqueId, LwPciDev bdf, LwU032 pasid, LwU064 address, LwU032 atSize, bool isGpa, bool flush, bool isGlobal)
{
    SimCallMutex m;
    return (*m_ICPUModel2Funcs.CPUModelAtsShootDown)(m_pvthis, uniqueId, bdf, pasid, address, atSize, isGpa, flush, isGlobal);
}

// === IPciDevModsProxy (mods-side wrapper for sim implementation) ===

IPciDevModsProxy::IPciDevModsProxy(void* pvIPciDev, void* pSimLibModule)
  : m_pvthis(pvIPciDev),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IPciDevFuncs.AddRef = (FN_PTR_IPciDev_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_AddRef");
    m_IPciDevFuncs.Release = (FN_PTR_IPciDev_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_Release");
    m_IPciDevFuncs.QueryIface = (FN_PTR_IPciDev_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_QueryIface");
    m_IPciDevFuncs.GetPciBarInfo = (FN_PTR_IPciDev_GetPciBarInfo) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_GetPciBarInfo");
    m_IPciDevFuncs.GetPciIrq = (FN_PTR_IPciDev_GetPciIrq) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_GetPciIrq");
    m_IPciDevFuncs.GetPciMappedPhysicalAddress = (FN_PTR_IPciDev_GetPciMappedPhysicalAddress) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_GetPciMappedPhysicalAddress");
    m_IPciDevFuncs.FindPciDevice = (FN_PTR_IPciDev_FindPciDevice) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_FindPciDevice");
    m_IPciDevFuncs.FindPciClassCode = (FN_PTR_IPciDev_FindPciClassCode) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_FindPciClassCode");
    m_IPciDevFuncs.PciCfgRd08 = (FN_PTR_IPciDev_PciCfgRd08) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_PciCfgRd08");
    m_IPciDevFuncs.PciCfgRd16 = (FN_PTR_IPciDev_PciCfgRd16) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_PciCfgRd16");
    m_IPciDevFuncs.PciCfgRd32 = (FN_PTR_IPciDev_PciCfgRd32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_PciCfgRd32");
    m_IPciDevFuncs.PciCfgWr08 = (FN_PTR_IPciDev_PciCfgWr08) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_PciCfgWr08");
    m_IPciDevFuncs.PciCfgWr16 = (FN_PTR_IPciDev_PciCfgWr16) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_PciCfgWr16");
    m_IPciDevFuncs.PciCfgWr32 = (FN_PTR_IPciDev_PciCfgWr32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev_PciCfgWr32");

    MASSERT(m_IPciDevFuncs.SanityCheck());
}

IPciDevModsProxy::~IPciDevModsProxy ()
{
}

void IPciDevModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IPciDevFuncs.AddRef)(m_pvthis);
}

void IPciDevModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IPciDevFuncs.Release)(m_pvthis);
}

IIfaceObject* IPciDevModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IPciDevFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

LwErr IPciDevModsProxy::GetPciBarInfo(LwPciDev pciDevice, int index, LwU064* pAddress, LwU064* pSize)
{
    SimCallMutex m;
    return (*m_IPciDevFuncs.GetPciBarInfo)(m_pvthis, pciDevice, index, pAddress, pSize);
}

LwErr IPciDevModsProxy::GetPciIrq(LwPciDev pciDevice, LwU032* pIrq)
{
    SimCallMutex m;
    return (*m_IPciDevFuncs.GetPciIrq)(m_pvthis, pciDevice, pIrq);
}

LwErr IPciDevModsProxy::GetPciMappedPhysicalAddress(LwU064 address, LwU032 offset, LwU064* pMappedAddress)
{
    SimCallMutex m;
    return (*m_IPciDevFuncs.GetPciMappedPhysicalAddress)(m_pvthis, address, offset, pMappedAddress);
}

LwErr IPciDevModsProxy::FindPciDevice(LwU016 vendorId, LwU016 deviceId, int index, LwPciDev* pciDev)
{
    SimCallMutex m;
    return (*m_IPciDevFuncs.FindPciDevice)(m_pvthis, vendorId, deviceId, index, pciDev);
}

LwErr IPciDevModsProxy::FindPciClassCode(LwU032 classCode, int index, LwPciDev* pciDev)
{
    SimCallMutex m;
    return (*m_IPciDevFuncs.FindPciClassCode)(m_pvthis, classCode, index, pciDev);
}

LwErr IPciDevModsProxy::PciCfgRd08(LwPciDev pciDev, LwU032 address, LwU008* pData)
{
    SimCallMutex m;
    return (*m_IPciDevFuncs.PciCfgRd08)(m_pvthis, pciDev, address, pData);
}

LwErr IPciDevModsProxy::PciCfgRd16(LwPciDev pciDev, LwU032 address, LwU016* pData)
{
    SimCallMutex m;
    return (*m_IPciDevFuncs.PciCfgRd16)(m_pvthis, pciDev, address, pData);
}

LwErr IPciDevModsProxy::PciCfgRd32(LwPciDev pciDev, LwU032 address, LwU032* pData)
{
    SimCallMutex m;
    return (*m_IPciDevFuncs.PciCfgRd32)(m_pvthis, pciDev, address, pData);
}

LwErr IPciDevModsProxy::PciCfgWr08(LwPciDev pciDev, LwU032 address, LwU008 data)
{
    SimCallMutex m;
    return (*m_IPciDevFuncs.PciCfgWr08)(m_pvthis, pciDev, address, data);
}

LwErr IPciDevModsProxy::PciCfgWr16(LwPciDev pciDev, LwU032 address, LwU016 data)
{
    SimCallMutex m;
    return (*m_IPciDevFuncs.PciCfgWr16)(m_pvthis, pciDev, address, data);
}

LwErr IPciDevModsProxy::PciCfgWr32(LwPciDev pciDev, LwU032 address, LwU032 data)
{
    SimCallMutex m;
    return (*m_IPciDevFuncs.PciCfgWr32)(m_pvthis, pciDev, address, data);
}

// === IInterruptMgr2ModsProxy (mods-side wrapper for sim implementation) ===

IInterruptMgr2ModsProxy::IInterruptMgr2ModsProxy(void* pvIInterruptMgr2, void* pSimLibModule)
  : m_pvthis(pvIInterruptMgr2),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IInterruptMgr2Funcs.AddRef = (FN_PTR_IInterruptMgr2_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr2_AddRef");
    m_IInterruptMgr2Funcs.Release = (FN_PTR_IInterruptMgr2_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr2_Release");
    m_IInterruptMgr2Funcs.QueryIface = (FN_PTR_IInterruptMgr2_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr2_QueryIface");
    m_IInterruptMgr2Funcs.HookInterrupt = (FN_PTR_IInterruptMgr2_HookInterrupt) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr2_HookInterrupt");

    MASSERT(m_IInterruptMgr2Funcs.SanityCheck());
}

IInterruptMgr2ModsProxy::~IInterruptMgr2ModsProxy ()
{
}

void IInterruptMgr2ModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IInterruptMgr2Funcs.AddRef)(m_pvthis);
}

void IInterruptMgr2ModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IInterruptMgr2Funcs.Release)(m_pvthis);
}

IIfaceObject* IInterruptMgr2ModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IInterruptMgr2Funcs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

int IInterruptMgr2ModsProxy::HookInterrupt(IrqInfo2 irqInfo)
{
    SimCallMutex m;
    return (*m_IInterruptMgr2Funcs.HookInterrupt)(m_pvthis, irqInfo);
}

// === IInterruptMgr3ModsProxy (mods-side wrapper for sim implementation) ===

IInterruptMgr3ModsProxy::IInterruptMgr3ModsProxy(void* pvIInterruptMgr3, void* pSimLibModule)
  : m_pvthis(pvIInterruptMgr3),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IInterruptMgr3Funcs.AddRef = (FN_PTR_IInterruptMgr3_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr3_AddRef");
    m_IInterruptMgr3Funcs.Release = (FN_PTR_IInterruptMgr3_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr3_Release");
    m_IInterruptMgr3Funcs.QueryIface = (FN_PTR_IInterruptMgr3_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr3_QueryIface");
    m_IInterruptMgr3Funcs.AllocateIRQs = (FN_PTR_IInterruptMgr3_AllocateIRQs) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr3_AllocateIRQs");
    m_IInterruptMgr3Funcs.HookInterrupt3 = (FN_PTR_IInterruptMgr3_HookInterrupt3) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr3_HookInterrupt3");
    m_IInterruptMgr3Funcs.FreeIRQs = (FN_PTR_IInterruptMgr3_FreeIRQs) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr3_FreeIRQs");

    MASSERT(m_IInterruptMgr3Funcs.SanityCheck());
}

IInterruptMgr3ModsProxy::~IInterruptMgr3ModsProxy ()
{
}

void IInterruptMgr3ModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IInterruptMgr3Funcs.AddRef)(m_pvthis);
}

void IInterruptMgr3ModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IInterruptMgr3Funcs.Release)(m_pvthis);
}

IIfaceObject* IInterruptMgr3ModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IInterruptMgr3Funcs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

int IInterruptMgr3ModsProxy::AllocateIRQs(LwPciDev pciDev, LwU032 lwecs, LwU032 * irqs, LwU032 flags)
{
    SimCallMutex m;
    return (*m_IInterruptMgr3Funcs.AllocateIRQs)(m_pvthis, pciDev, lwecs, irqs, flags);
}

int IInterruptMgr3ModsProxy::HookInterrupt3(IrqInfo2 irqInfo)
{
    SimCallMutex m;
    return (*m_IInterruptMgr3Funcs.HookInterrupt3)(m_pvthis, irqInfo);
}

void IInterruptMgr3ModsProxy::FreeIRQs(LwPciDev pciDev)
{
    SimCallMutex m;
    return (*m_IInterruptMgr3Funcs.FreeIRQs)(m_pvthis, pciDev);
}

// === IPpcModsProxy (mods-side wrapper for sim implementation) ===

IPpcModsProxy::IPpcModsProxy(void* pvIPpc, void* pSimLibModule)
  : m_pvthis(pvIPpc),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IPpcFuncs.AddRef = (FN_PTR_IPpc_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPpc_AddRef");
    m_IPpcFuncs.Release = (FN_PTR_IPpc_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPpc_Release");
    m_IPpcFuncs.QueryIface = (FN_PTR_IPpc_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPpc_QueryIface");
    m_IPpcFuncs.SetupDmaBase = (FN_PTR_IPpc_SetupDmaBase) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPpc_SetupDmaBase");

    MASSERT(m_IPpcFuncs.SanityCheck());
}

IPpcModsProxy::~IPpcModsProxy ()
{
}

void IPpcModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IPpcFuncs.AddRef)(m_pvthis);
}

void IPpcModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IPpcFuncs.Release)(m_pvthis);
}

IIfaceObject* IPpcModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IPpcFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

LwErr IPpcModsProxy::SetupDmaBase(LwPciDev pciDevice, TCE_BYPASS_MODE mode, LwU064 devDmaMask, LwU064* pDmaBase)
{
    SimCallMutex m;
    return (*m_IPpcFuncs.SetupDmaBase)(m_pvthis, pciDevice, mode, devDmaMask, pDmaBase);
}

// === IInterruptMgrModsProxy (mods-side wrapper for sim implementation) ===

IInterruptMgrModsProxy::IInterruptMgrModsProxy(void* pvIInterruptMgr, void* pSimLibModule)
  : m_pvthis(pvIInterruptMgr),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IInterruptMgrFuncs.AddRef = (FN_PTR_IInterruptMgr_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr_AddRef");
    m_IInterruptMgrFuncs.Release = (FN_PTR_IInterruptMgr_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr_Release");
    m_IInterruptMgrFuncs.QueryIface = (FN_PTR_IInterruptMgr_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr_QueryIface");
    m_IInterruptMgrFuncs.HookInterrupt = (FN_PTR_IInterruptMgr_HookInterrupt) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr_HookInterrupt");
    m_IInterruptMgrFuncs.UnhookInterrupt = (FN_PTR_IInterruptMgr_UnhookInterrupt) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr_UnhookInterrupt");
    m_IInterruptMgrFuncs.PollInterrupts = (FN_PTR_IInterruptMgr_PollInterrupts) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IInterruptMgr_PollInterrupts");

    MASSERT(m_IInterruptMgrFuncs.SanityCheck());
}

IInterruptMgrModsProxy::~IInterruptMgrModsProxy ()
{
}

void IInterruptMgrModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IInterruptMgrFuncs.AddRef)(m_pvthis);
}

void IInterruptMgrModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IInterruptMgrFuncs.Release)(m_pvthis);
}

IIfaceObject* IInterruptMgrModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IInterruptMgrFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

int IInterruptMgrModsProxy::HookInterrupt(LwU032 irqNumber)
{
    SimCallMutex m;
    return (*m_IInterruptMgrFuncs.HookInterrupt)(m_pvthis, irqNumber);
}

int IInterruptMgrModsProxy::UnhookInterrupt(LwU032 irqNumber)
{
    SimCallMutex m;
    return (*m_IInterruptMgrFuncs.UnhookInterrupt)(m_pvthis, irqNumber);
}

void IInterruptMgrModsProxy::PollInterrupts()
{
    SimCallMutex m;
    return (*m_IInterruptMgrFuncs.PollInterrupts)(m_pvthis);
}

// === IMemAlloc64ModsProxy (mods-side wrapper for sim implementation) ===

IMemAlloc64ModsProxy::IMemAlloc64ModsProxy(void* pvIMemAlloc64, void* pSimLibModule)
  : m_pvthis(pvIMemAlloc64),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IMemAlloc64Funcs.AddRef = (FN_PTR_IMemAlloc64_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemAlloc64_AddRef");
    m_IMemAlloc64Funcs.Release = (FN_PTR_IMemAlloc64_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemAlloc64_Release");
    m_IMemAlloc64Funcs.QueryIface = (FN_PTR_IMemAlloc64_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemAlloc64_QueryIface");
    m_IMemAlloc64Funcs.AllocSysMem64 = (FN_PTR_IMemAlloc64_AllocSysMem64) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemAlloc64_AllocSysMem64");
    m_IMemAlloc64Funcs.FreeSysMem64 = (FN_PTR_IMemAlloc64_FreeSysMem64) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemAlloc64_FreeSysMem64");

    MASSERT(m_IMemAlloc64Funcs.SanityCheck());
}

IMemAlloc64ModsProxy::~IMemAlloc64ModsProxy ()
{
}

void IMemAlloc64ModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IMemAlloc64Funcs.AddRef)(m_pvthis);
}

void IMemAlloc64ModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IMemAlloc64Funcs.Release)(m_pvthis);
}

IIfaceObject* IMemAlloc64ModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IMemAlloc64Funcs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

int IMemAlloc64ModsProxy::AllocSysMem64(LwU064 sz, LwU064* pRtnAddr)
{
    SimCallMutex m;
    return (*m_IMemAlloc64Funcs.AllocSysMem64)(m_pvthis, sz, pRtnAddr);
}

void IMemAlloc64ModsProxy::FreeSysMem64(LwU064 addr)
{
    SimCallMutex m;
    return (*m_IMemAlloc64Funcs.FreeSysMem64)(m_pvthis, addr);
}

// === IMultiHeap2ModsProxy (mods-side wrapper for sim implementation) ===

IMultiHeap2ModsProxy::IMultiHeap2ModsProxy(void* pvIMultiHeap2, void* pSimLibModule)
  : m_pvthis(pvIMultiHeap2),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IMultiHeap2Funcs.AddRef = (FN_PTR_IMultiHeap2_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap2_AddRef");
    m_IMultiHeap2Funcs.Release = (FN_PTR_IMultiHeap2_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap2_Release");
    m_IMultiHeap2Funcs.QueryIface = (FN_PTR_IMultiHeap2_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap2_QueryIface");
    m_IMultiHeap2Funcs.DeviceAllocSysMem64 = (FN_PTR_IMultiHeap2_DeviceAllocSysMem64) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap2_DeviceAllocSysMem64");
    m_IMultiHeap2Funcs.AllocSysMem64 = (FN_PTR_IMultiHeap2_AllocSysMem64) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap2_AllocSysMem64");
    m_IMultiHeap2Funcs.FreeSysMem64 = (FN_PTR_IMultiHeap2_FreeSysMem64) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap2_FreeSysMem64");
    m_IMultiHeap2Funcs.DeviceAllocSysMem32 = (FN_PTR_IMultiHeap2_DeviceAllocSysMem32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap2_DeviceAllocSysMem32");
    m_IMultiHeap2Funcs.AllocSysMem32 = (FN_PTR_IMultiHeap2_AllocSysMem32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap2_AllocSysMem32");
    m_IMultiHeap2Funcs.FreeSysMem32 = (FN_PTR_IMultiHeap2_FreeSysMem32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap2_FreeSysMem32");
    m_IMultiHeap2Funcs.Support64 = (FN_PTR_IMultiHeap2_Support64) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMultiHeap2_Support64");

    MASSERT(m_IMultiHeap2Funcs.SanityCheck());
}

IMultiHeap2ModsProxy::~IMultiHeap2ModsProxy ()
{
}

void IMultiHeap2ModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IMultiHeap2Funcs.AddRef)(m_pvthis);
}

void IMultiHeap2ModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IMultiHeap2Funcs.Release)(m_pvthis);
}

IIfaceObject* IMultiHeap2ModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IMultiHeap2Funcs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

int IMultiHeap2ModsProxy::DeviceAllocSysMem64(LwPciDev dev, LwU064 sz, LwU032 t, LwU064 align, LwU064* pRtnAddr)
{
    SimCallMutex m;
    return (*m_IMultiHeap2Funcs.DeviceAllocSysMem64)(m_pvthis, dev, sz, t, align, pRtnAddr);
}

int IMultiHeap2ModsProxy::AllocSysMem64(LwU064 sz, LwU032 t, LwU064 align, LwU064* pRtnAddr)
{
    SimCallMutex m;
    return (*m_IMultiHeap2Funcs.AllocSysMem64)(m_pvthis, sz, t, align, pRtnAddr);
}

void IMultiHeap2ModsProxy::FreeSysMem64(LwU064 addr)
{
    SimCallMutex m;
    return (*m_IMultiHeap2Funcs.FreeSysMem64)(m_pvthis, addr);
}

int IMultiHeap2ModsProxy::DeviceAllocSysMem32(LwPciDev dev, LwU032 sz, LwU032 t, LwU032 align, LwU032* pRtnAddr)
{
    SimCallMutex m;
    return (*m_IMultiHeap2Funcs.DeviceAllocSysMem32)(m_pvthis, dev, sz, t, align, pRtnAddr);
}

int IMultiHeap2ModsProxy::AllocSysMem32(LwU032 sz, LwU032 t, LwU032 align, LwU032* pRtnAddr)
{
    SimCallMutex m;
    return (*m_IMultiHeap2Funcs.AllocSysMem32)(m_pvthis, sz, t, align, pRtnAddr);
}

void IMultiHeap2ModsProxy::FreeSysMem32(LwU032 addr)
{
    SimCallMutex m;
    return (*m_IMultiHeap2Funcs.FreeSysMem32)(m_pvthis, addr);
}

bool IMultiHeap2ModsProxy::Support64()
{
    SimCallMutex m;
    return (*m_IMultiHeap2Funcs.Support64)(m_pvthis);
}

// === IMemoryModsProxy (mods-side wrapper for sim implementation) ===

IMemoryModsProxy::IMemoryModsProxy(void* pvIMemory, void* pSimLibModule)
  : m_pvthis(pvIMemory),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IMemoryFuncs.AddRef = (FN_PTR_IMemory_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_AddRef");
    m_IMemoryFuncs.Release = (FN_PTR_IMemory_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_Release");
    m_IMemoryFuncs.QueryIface = (FN_PTR_IMemory_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_QueryIface");
    m_IMemoryFuncs.MemRd08 = (FN_PTR_IMemory_MemRd08) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemRd08");
    m_IMemoryFuncs.MemRd16 = (FN_PTR_IMemory_MemRd16) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemRd16");
    m_IMemoryFuncs.MemRd32 = (FN_PTR_IMemory_MemRd32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemRd32");
    m_IMemoryFuncs.MemRd64 = (FN_PTR_IMemory_MemRd64) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemRd64");
    m_IMemoryFuncs.MemWr08 = (FN_PTR_IMemory_MemWr08) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemWr08");
    m_IMemoryFuncs.MemWr16 = (FN_PTR_IMemory_MemWr16) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemWr16");
    m_IMemoryFuncs.MemWr32 = (FN_PTR_IMemory_MemWr32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemWr32");
    m_IMemoryFuncs.MemWr64 = (FN_PTR_IMemory_MemWr64) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemWr64");
    m_IMemoryFuncs.MemSet08 = (FN_PTR_IMemory_MemSet08) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemSet08");
    m_IMemoryFuncs.MemSet16 = (FN_PTR_IMemory_MemSet16) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemSet16");
    m_IMemoryFuncs.MemSet32 = (FN_PTR_IMemory_MemSet32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemSet32");
    m_IMemoryFuncs.MemSet64 = (FN_PTR_IMemory_MemSet64) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemSet64");
    m_IMemoryFuncs.MemSetBlk = (FN_PTR_IMemory_MemSetBlk) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemSetBlk");
    m_IMemoryFuncs.MemWrBlk = (FN_PTR_IMemory_MemWrBlk) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemWrBlk");
    m_IMemoryFuncs.MemWrBlk32 = (FN_PTR_IMemory_MemWrBlk32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemWrBlk32");
    m_IMemoryFuncs.MemRdBlk = (FN_PTR_IMemory_MemRdBlk) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemRdBlk");
    m_IMemoryFuncs.MemRdBlk32 = (FN_PTR_IMemory_MemRdBlk32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemRdBlk32");
    m_IMemoryFuncs.MemCpBlk = (FN_PTR_IMemory_MemCpBlk) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemCpBlk");
    m_IMemoryFuncs.MemCpBlk32 = (FN_PTR_IMemory_MemCpBlk32) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMemory_MemCpBlk32");

    MASSERT(m_IMemoryFuncs.SanityCheck());
}

IMemoryModsProxy::~IMemoryModsProxy ()
{
}

void IMemoryModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.AddRef)(m_pvthis);
}

void IMemoryModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.Release)(m_pvthis);
}

IIfaceObject* IMemoryModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IMemoryFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

LwU008 IMemoryModsProxy::MemRd08(LwU032 address)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemRd08)(m_pvthis, address);
}

LwU016 IMemoryModsProxy::MemRd16(LwU032 address)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemRd16)(m_pvthis, address);
}

LwU032 IMemoryModsProxy::MemRd32(LwU032 address)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemRd32)(m_pvthis, address);
}

LwU064 IMemoryModsProxy::MemRd64(LwU032 address)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemRd64)(m_pvthis, address);
}

void IMemoryModsProxy::MemWr08(LwU032 address, LwU008 data)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemWr08)(m_pvthis, address, data);
}

void IMemoryModsProxy::MemWr16(LwU032 address, LwU016 data)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemWr16)(m_pvthis, address, data);
}

void IMemoryModsProxy::MemWr32(LwU032 address, LwU032 data)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemWr32)(m_pvthis, address, data);
}

void IMemoryModsProxy::MemWr64(LwU032 address, LwU064 data)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemWr64)(m_pvthis, address, data);
}

void IMemoryModsProxy::MemSet08(LwU032 address, LwU032 size, LwU008 data)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemSet08)(m_pvthis, address, size, data);
}

void IMemoryModsProxy::MemSet16(LwU032 address, LwU032 size, LwU016 data)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemSet16)(m_pvthis, address, size, data);
}

void IMemoryModsProxy::MemSet32(LwU032 address, LwU032 size, LwU032 data)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemSet32)(m_pvthis, address, size, data);
}

void IMemoryModsProxy::MemSet64(LwU032 address, LwU032 size, LwU064 data)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemSet64)(m_pvthis, address, size, data);
}

void IMemoryModsProxy::MemSetBlk(LwU032 address, LwU032 size, void* data, LwU032 data_size)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemSetBlk)(m_pvthis, address, size, data, data_size);
}

void IMemoryModsProxy::MemWrBlk(LwU032 address, const void* appdata, LwU032 count)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemWrBlk)(m_pvthis, address, appdata, count);
}

void IMemoryModsProxy::MemWrBlk32(LwU032 address, const void* appdata, LwU032 count)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemWrBlk32)(m_pvthis, address, appdata, count);
}

void IMemoryModsProxy::MemRdBlk(LwU032 address, void* appdata, LwU032 count)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemRdBlk)(m_pvthis, address, appdata, count);
}

void IMemoryModsProxy::MemRdBlk32(LwU032 address, void* appdata, LwU032 count)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemRdBlk32)(m_pvthis, address, appdata, count);
}

void IMemoryModsProxy::MemCpBlk(LwU032 address, LwU032 appdata, LwU032 count)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemCpBlk)(m_pvthis, address, appdata, count);
}

void IMemoryModsProxy::MemCpBlk32(LwU032 address, LwU032 appdata, LwU032 count)
{
    SimCallMutex m;
    return (*m_IMemoryFuncs.MemCpBlk32)(m_pvthis, address, appdata, count);
}

// === IChipModsProxy (mods-side wrapper for sim implementation) ===

IChipModsProxy::IChipModsProxy(void* pvIChip, void* pSimLibModule)
  : m_pvthis(pvIChip),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IChipFuncs.AddRef = (FN_PTR_IChip_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_AddRef");
    m_IChipFuncs.Release = (FN_PTR_IChip_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_Release");
    m_IChipFuncs.QueryIface = (FN_PTR_IChip_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_QueryIface");
    m_IChipFuncs.Startup = (FN_PTR_IChip_Startup) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_Startup");
    m_IChipFuncs.Shutdown = (FN_PTR_IChip_Shutdown) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_Shutdown");
    m_IChipFuncs.AllocSysMem = (FN_PTR_IChip_AllocSysMem) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_AllocSysMem");
    m_IChipFuncs.FreeSysMem = (FN_PTR_IChip_FreeSysMem) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_FreeSysMem");
    m_IChipFuncs.ClockSimulator = (FN_PTR_IChip_ClockSimulator) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_ClockSimulator");
    m_IChipFuncs.Delay = (FN_PTR_IChip_Delay) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_Delay");
    m_IChipFuncs.EscapeWrite = (FN_PTR_IChip_EscapeWrite) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_EscapeWrite");
    m_IChipFuncs.EscapeRead = (FN_PTR_IChip_EscapeRead) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_EscapeRead");
    m_IChipFuncs.FindPCIDevice = (FN_PTR_IChip_FindPCIDevice) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_FindPCIDevice");
    m_IChipFuncs.FindPCIClassCode = (FN_PTR_IChip_FindPCIClassCode) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_FindPCIClassCode");
    m_IChipFuncs.GetSimulatorTime = (FN_PTR_IChip_GetSimulatorTime) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_GetSimulatorTime");
    m_IChipFuncs.GetSimulatorTimeUnitsNS = (FN_PTR_IChip_GetSimulatorTimeUnitsNS) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_GetSimulatorTimeUnitsNS");
    m_IChipFuncs.GetPCIBaseAddress = (FN_PTR_IChip_GetPCIBaseAddress) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_GetPCIBaseAddress");
    m_IChipFuncs.GetChipLevel = (FN_PTR_IChip_GetChipLevel) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IChip_GetChipLevel");

    MASSERT(m_IChipFuncs.SanityCheck());
}

IChipModsProxy::~IChipModsProxy ()
{
}

void IChipModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IChipFuncs.AddRef)(m_pvthis);
}

void IChipModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IChipFuncs.Release)(m_pvthis);
}

IIfaceObject* IChipModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IChipFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

int IChipModsProxy::Startup(IIfaceObject* system, char** argv, int argc)
{
    SimCallMutex m;
    return (*m_IChipFuncs.Startup)(m_pvthis, system, argv, argc);
}

void IChipModsProxy::Shutdown()
{
    SimCallMutex m;
    return (*m_IChipFuncs.Shutdown)(m_pvthis);
}

int IChipModsProxy::AllocSysMem(int numBytes, LwU032* physAddr)
{
    SimCallMutex m;
    return (*m_IChipFuncs.AllocSysMem)(m_pvthis, numBytes, physAddr);
}

void IChipModsProxy::FreeSysMem(LwU032 physAddr)
{
    SimCallMutex m;
    return (*m_IChipFuncs.FreeSysMem)(m_pvthis, physAddr);
}

void IChipModsProxy::ClockSimulator(LwS032 numClocks)
{
    SimCallMutex m;
    return (*m_IChipFuncs.ClockSimulator)(m_pvthis, numClocks);
}

void IChipModsProxy::Delay(LwU032 numMicroSeconds)
{
    SimCallMutex m;
    return (*m_IChipFuncs.Delay)(m_pvthis, numMicroSeconds);
}

int IChipModsProxy::EscapeWrite(char* path, LwU032 index, LwU032 size, LwU032 value)
{
    SimCallMutex m;
    return (*m_IChipFuncs.EscapeWrite)(m_pvthis, path, index, size, value);
}

int IChipModsProxy::EscapeRead(char* path, LwU032 index, LwU032 size, LwU032* value)
{
    SimCallMutex m;
    return (*m_IChipFuncs.EscapeRead)(m_pvthis, path, index, size, value);
}

int IChipModsProxy::FindPCIDevice(LwU016 vendorId, LwU016 deviceId, int index, LwU032* address)
{
    SimCallMutex m;
    return (*m_IChipFuncs.FindPCIDevice)(m_pvthis, vendorId, deviceId, index, address);
}

int IChipModsProxy::FindPCIClassCode(LwU032 classCode, int index, LwU032* address)
{
    SimCallMutex m;
    return (*m_IChipFuncs.FindPCIClassCode)(m_pvthis, classCode, index, address);
}

int IChipModsProxy::GetSimulatorTime(LwU064* simTime)
{
    SimCallMutex m;
    return (*m_IChipFuncs.GetSimulatorTime)(m_pvthis, simTime);
}

double IChipModsProxy::GetSimulatorTimeUnitsNS()
{
    SimCallMutex m;
    return (*m_IChipFuncs.GetSimulatorTimeUnitsNS)(m_pvthis);
}

int IChipModsProxy::GetPCIBaseAddress(LwU032 cfgAddr, int index, LwU032* pAddress, LwU032* pSize)
{
    SimCallMutex m;
    return (*m_IChipFuncs.GetPCIBaseAddress)(m_pvthis, cfgAddr, index, pAddress, pSize);
}

IChip::ELEVEL IChipModsProxy::GetChipLevel()
{
    SimCallMutex m;
    return (*m_IChipFuncs.GetChipLevel)(m_pvthis);
}

// === IPciDev2ModsProxy (mods-side wrapper for sim implementation) ===

IPciDev2ModsProxy::IPciDev2ModsProxy(void* pvIPciDev2, void* pSimLibModule)
  : m_pvthis(pvIPciDev2),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IPciDev2Funcs.AddRef = (FN_PTR_IPciDev2_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev2_AddRef");
    m_IPciDev2Funcs.Release = (FN_PTR_IPciDev2_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev2_Release");
    m_IPciDev2Funcs.QueryIface = (FN_PTR_IPciDev2_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev2_QueryIface");
    m_IPciDev2Funcs.PciResetFunction = (FN_PTR_IPciDev2_PciResetFunction) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IPciDev2_PciResetFunction");

    MASSERT(m_IPciDev2Funcs.SanityCheck());
}

IPciDev2ModsProxy::~IPciDev2ModsProxy ()
{
}

void IPciDev2ModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IPciDev2Funcs.AddRef)(m_pvthis);
}

void IPciDev2ModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IPciDev2Funcs.Release)(m_pvthis);
}

IIfaceObject* IPciDev2ModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IPciDev2Funcs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

LwErr IPciDev2ModsProxy::PciResetFunction(LwPciDev pciDev)
{
    SimCallMutex m;
    return (*m_IPciDev2Funcs.PciResetFunction)(m_pvthis, pciDev);
}

// === IMapMemoryExtModsProxy (mods-side wrapper for sim implementation) ===

IMapMemoryExtModsProxy::IMapMemoryExtModsProxy(void* pvIMapMemoryExt, void* pSimLibModule)
  : m_pvthis(pvIMapMemoryExt),
    m_pSimLibModule(pSimLibModule)
{
    MASSERT(m_pvthis);
    MASSERT(m_pSimLibModule);

    // Fill in our VTable by querying sim library.

    m_IMapMemoryExtFuncs.AddRef = (FN_PTR_IMapMemoryExt_AddRef) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapMemoryExt_AddRef");
    m_IMapMemoryExtFuncs.Release = (FN_PTR_IMapMemoryExt_Release) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapMemoryExt_Release");
    m_IMapMemoryExtFuncs.QueryIface = (FN_PTR_IMapMemoryExt_QueryIface) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapMemoryExt_QueryIface");
    m_IMapMemoryExtFuncs.MapMemoryRegion = (FN_PTR_IMapMemoryExt_MapMemoryRegion) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapMemoryExt_MapMemoryRegion");
    m_IMapMemoryExtFuncs.UnMapMemoryRegion = (FN_PTR_IMapMemoryExt_UnMapMemoryRegion) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapMemoryExt_UnMapMemoryRegion");
    m_IMapMemoryExtFuncs.RemapPages = (FN_PTR_IMapMemoryExt_RemapPages) Xp::GetDLLProc(
            m_pSimLibModule, "call_sim_IMapMemoryExt_RemapPages");

    MASSERT(m_IMapMemoryExtFuncs.SanityCheck());
}

IMapMemoryExtModsProxy::~IMapMemoryExtModsProxy ()
{
}

void IMapMemoryExtModsProxy::AddRef()
{
    SimCallMutex m;
    return (*m_IMapMemoryExtFuncs.AddRef)(m_pvthis);
}

void IMapMemoryExtModsProxy::Release()
{
    SimCallMutex m;
    return (*m_IMapMemoryExtFuncs.Release)(m_pvthis);
}

IIfaceObject* IMapMemoryExtModsProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* tmp = (*m_IMapMemoryExtFuncs.QueryIface)(m_pvthis, id);
    tmp = CreateProxyForSimObject(id, tmp, m_pSimLibModule);
    return tmp;
}

int IMapMemoryExtModsProxy::MapMemoryRegion(void** pReturnedVirtualAddress, LwU064 PhysicalAddress, size_t NumBytes, int Attrib, int Protect)
{
    SimCallMutex m;
    return (*m_IMapMemoryExtFuncs.MapMemoryRegion)(m_pvthis, pReturnedVirtualAddress, PhysicalAddress, NumBytes, Attrib, Protect);
}

void IMapMemoryExtModsProxy::UnMapMemoryRegion(void* VirtualAddress)
{
    SimCallMutex m;
    return (*m_IMapMemoryExtFuncs.UnMapMemoryRegion)(m_pvthis, VirtualAddress);
}

int IMapMemoryExtModsProxy::RemapPages(void* VirtualAddress, LwU064 PhysicalAddress, size_t NumBytes, int Protect)
{
    SimCallMutex m;
    return (*m_IMapMemoryExtFuncs.RemapPages)(m_pvthis, VirtualAddress, PhysicalAddress, NumBytes, Protect);
}

// === IIfaceObject ("C" hooks for sim to call mods-side implementation) ===

extern "C" {
DECLSPEC void call_mods_IIfaceObject_AddRef (void* pvthis)
{
    IIfaceObject * pIIfaceObject = (IIfaceObject *)(pvthis);
    return pIIfaceObject->AddRef();
}
DECLSPEC void call_mods_IIfaceObject_Release (void* pvthis)
{
    IIfaceObject * pIIfaceObject = (IIfaceObject *)(pvthis);
    return pIIfaceObject->Release();
}
DECLSPEC IIfaceObject* call_mods_IIfaceObject_QueryIface (void* pvthis, IID_TYPE id)
{
    IIfaceObject * pIIfaceObject = (IIfaceObject *)(pvthis);
    return pIIfaceObject->QueryIface(id);
}
} // extern "C"

RC PushIIfaceObjectFptrsToSim(void* pSimLibModule)
{
    FN_PTR_PushIIfaceObjectFuncs fpPushVtable = 
            (FN_PTR_PushIIfaceObjectFuncs) Xp::GetDLLProc(pSimLibModule, "PushIIfaceObjectFuncs");
    if (0 == fpPushVtable)
    {
        Printf(Tee::PriHigh, "Can not get PushIIfaceObjectFuncs iface from sim.\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }

    IIfaceObjectFuncs vtable;
    vtable.AddRef = call_mods_IIfaceObject_AddRef;
    vtable.Release = call_mods_IIfaceObject_Release;
    vtable.QueryIface = call_mods_IIfaceObject_QueryIface;

    if (!(*fpPushVtable)(vtable))
    {
        Printf(Tee::PriHigh, "PushIIfaceObjectFuncs failed.\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }
    return OK;
}

// === IBusMem ("C" hooks for sim to call mods-side implementation) ===

extern "C" {
DECLSPEC void call_mods_IBusMem_AddRef (void* pvthis)
{
    IBusMem * pIBusMem = (IBusMem *)(pvthis);
    return pIBusMem->AddRef();
}
DECLSPEC void call_mods_IBusMem_Release (void* pvthis)
{
    IBusMem * pIBusMem = (IBusMem *)(pvthis);
    return pIBusMem->Release();
}
DECLSPEC IIfaceObject* call_mods_IBusMem_QueryIface (void* pvthis, IID_TYPE id)
{
    IBusMem * pIBusMem = (IBusMem *)(pvthis);
    return pIBusMem->QueryIface(id);
}
DECLSPEC BusMemRet call_mods_IBusMem_BusMemWrBlk (void* pvthis, LwU064 address, const void* appdata, LwU032 count)
{
    IBusMem * pIBusMem = (IBusMem *)(pvthis);
    return pIBusMem->BusMemWrBlk(address, appdata, count);
}
DECLSPEC BusMemRet call_mods_IBusMem_BusMemRdBlk (void* pvthis, LwU064 address, void* appdata, LwU032 count)
{
    IBusMem * pIBusMem = (IBusMem *)(pvthis);
    return pIBusMem->BusMemRdBlk(address, appdata, count);
}
DECLSPEC BusMemRet call_mods_IBusMem_BusMemCpBlk (void* pvthis, LwU064 dest, LwU064 source, LwU032 count)
{
    IBusMem * pIBusMem = (IBusMem *)(pvthis);
    return pIBusMem->BusMemCpBlk(dest, source, count);
}
DECLSPEC BusMemRet call_mods_IBusMem_BusMemSetBlk (void* pvthis, LwU064 address, LwU032 size, void* data, LwU032 data_size)
{
    IBusMem * pIBusMem = (IBusMem *)(pvthis);
    return pIBusMem->BusMemSetBlk(address, size, data, data_size);
}
} // extern "C"

RC PushIBusMemFptrsToSim(void* pSimLibModule)
{
    FN_PTR_PushIBusMemFuncs fpPushVtable = 
            (FN_PTR_PushIBusMemFuncs) Xp::GetDLLProc(pSimLibModule, "PushIBusMemFuncs");
    if (0 == fpPushVtable)
    {
        Printf(Tee::PriHigh, "Can not get PushIBusMemFuncs iface from sim.\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }

    IBusMemFuncs vtable;
    vtable.AddRef = call_mods_IBusMem_AddRef;
    vtable.Release = call_mods_IBusMem_Release;
    vtable.QueryIface = call_mods_IBusMem_QueryIface;
    vtable.BusMemWrBlk = call_mods_IBusMem_BusMemWrBlk;
    vtable.BusMemRdBlk = call_mods_IBusMem_BusMemRdBlk;
    vtable.BusMemCpBlk = call_mods_IBusMem_BusMemCpBlk;
    vtable.BusMemSetBlk = call_mods_IBusMem_BusMemSetBlk;

    if (!(*fpPushVtable)(vtable))
    {
        Printf(Tee::PriHigh, "PushIBusMemFuncs failed.\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }
    return OK;
}

// === IInterrupt4 ("C" hooks for sim to call mods-side implementation) ===

extern "C" {
DECLSPEC void call_mods_IInterrupt4_AddRef (void* pvthis)
{
    IInterrupt4 * pIInterrupt4 = (IInterrupt4 *)(pvthis);
    return pIInterrupt4->AddRef();
}
DECLSPEC void call_mods_IInterrupt4_Release (void* pvthis)
{
    IInterrupt4 * pIInterrupt4 = (IInterrupt4 *)(pvthis);
    return pIInterrupt4->Release();
}
DECLSPEC IIfaceObject* call_mods_IInterrupt4_QueryIface (void* pvthis, IID_TYPE id)
{
    IInterrupt4 * pIInterrupt4 = (IInterrupt4 *)(pvthis);
    return pIInterrupt4->QueryIface(id);
}
DECLSPEC void call_mods_IInterrupt4_HandleInterruptVectorChange (void* pvthis, const LwU032* pVector, LwU032 numWords)
{
    IInterrupt4 * pIInterrupt4 = (IInterrupt4 *)(pvthis);
    return pIInterrupt4->HandleInterruptVectorChange(pVector, numWords);
}
} // extern "C"

RC PushIInterrupt4FptrsToSim(void* pSimLibModule)
{
    FN_PTR_PushIInterrupt4Funcs fpPushVtable = 
            (FN_PTR_PushIInterrupt4Funcs) Xp::GetDLLProc(pSimLibModule, "PushIInterrupt4Funcs");
    if (0 == fpPushVtable)
    {
        Printf(Tee::PriHigh, "Can not get PushIInterrupt4Funcs iface from sim.\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }

    IInterrupt4Funcs vtable;
    vtable.AddRef = call_mods_IInterrupt4_AddRef;
    vtable.Release = call_mods_IInterrupt4_Release;
    vtable.QueryIface = call_mods_IInterrupt4_QueryIface;
    vtable.HandleInterruptVectorChange = call_mods_IInterrupt4_HandleInterruptVectorChange;

    if (!(*fpPushVtable)(vtable))
    {
        Printf(Tee::PriHigh, "PushIInterrupt4Funcs failed.\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }
    return OK;
}

// === IInterrupt3 ("C" hooks for sim to call mods-side implementation) ===

extern "C" {
DECLSPEC void call_mods_IInterrupt3_AddRef (void* pvthis)
{
    IInterrupt3 * pIInterrupt3 = (IInterrupt3 *)(pvthis);
    return pIInterrupt3->AddRef();
}
DECLSPEC void call_mods_IInterrupt3_Release (void* pvthis)
{
    IInterrupt3 * pIInterrupt3 = (IInterrupt3 *)(pvthis);
    return pIInterrupt3->Release();
}
DECLSPEC IIfaceObject* call_mods_IInterrupt3_QueryIface (void* pvthis, IID_TYPE id)
{
    IInterrupt3 * pIInterrupt3 = (IInterrupt3 *)(pvthis);
    return pIInterrupt3->QueryIface(id);
}
DECLSPEC void call_mods_IInterrupt3_HandleSpecificInterrupt (void* pvthis, LwU032 irqNumber)
{
    IInterrupt3 * pIInterrupt3 = (IInterrupt3 *)(pvthis);
    return pIInterrupt3->HandleSpecificInterrupt(irqNumber);
}
} // extern "C"

RC PushIInterrupt3FptrsToSim(void* pSimLibModule)
{
    FN_PTR_PushIInterrupt3Funcs fpPushVtable = 
            (FN_PTR_PushIInterrupt3Funcs) Xp::GetDLLProc(pSimLibModule, "PushIInterrupt3Funcs");
    if (0 == fpPushVtable)
    {
        Printf(Tee::PriHigh, "Can not get PushIInterrupt3Funcs iface from sim.\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }

    IInterrupt3Funcs vtable;
    vtable.AddRef = call_mods_IInterrupt3_AddRef;
    vtable.Release = call_mods_IInterrupt3_Release;
    vtable.QueryIface = call_mods_IInterrupt3_QueryIface;
    vtable.HandleSpecificInterrupt = call_mods_IInterrupt3_HandleSpecificInterrupt;

    if (!(*fpPushVtable)(vtable))
    {
        Printf(Tee::PriHigh, "PushIInterrupt3Funcs failed.\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }
    return OK;
}

// === IInterrupt ("C" hooks for sim to call mods-side implementation) ===

extern "C" {
DECLSPEC void call_mods_IInterrupt_AddRef (void* pvthis)
{
    IInterrupt * pIInterrupt = (IInterrupt *)(pvthis);
    return pIInterrupt->AddRef();
}
DECLSPEC void call_mods_IInterrupt_Release (void* pvthis)
{
    IInterrupt * pIInterrupt = (IInterrupt *)(pvthis);
    return pIInterrupt->Release();
}
DECLSPEC IIfaceObject* call_mods_IInterrupt_QueryIface (void* pvthis, IID_TYPE id)
{
    IInterrupt * pIInterrupt = (IInterrupt *)(pvthis);
    return pIInterrupt->QueryIface(id);
}
DECLSPEC void call_mods_IInterrupt_HandleInterrupt (void* pvthis)
{
    IInterrupt * pIInterrupt = (IInterrupt *)(pvthis);
    return pIInterrupt->HandleInterrupt();
}
} // extern "C"

RC PushIInterruptFptrsToSim(void* pSimLibModule)
{
    FN_PTR_PushIInterruptFuncs fpPushVtable = 
            (FN_PTR_PushIInterruptFuncs) Xp::GetDLLProc(pSimLibModule, "PushIInterruptFuncs");
    if (0 == fpPushVtable)
    {
        Printf(Tee::PriHigh, "Can not get PushIInterruptFuncs iface from sim.\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }

    IInterruptFuncs vtable;
    vtable.AddRef = call_mods_IInterrupt_AddRef;
    vtable.Release = call_mods_IInterrupt_Release;
    vtable.QueryIface = call_mods_IInterrupt_QueryIface;
    vtable.HandleInterrupt = call_mods_IInterrupt_HandleInterrupt;

    if (!(*fpPushVtable)(vtable))
    {
        Printf(Tee::PriHigh, "PushIInterruptFuncs failed.\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }
    return OK;
}

// === ICPUModelCallbacks2 ("C" hooks for sim to call mods-side implementation) ===

extern "C" {
DECLSPEC void call_mods_ICPUModelCallbacks2_AddRef (void* pvthis)
{
    ICPUModelCallbacks2 * pICPUModelCallbacks2 = (ICPUModelCallbacks2 *)(pvthis);
    return pICPUModelCallbacks2->AddRef();
}
DECLSPEC void call_mods_ICPUModelCallbacks2_Release (void* pvthis)
{
    ICPUModelCallbacks2 * pICPUModelCallbacks2 = (ICPUModelCallbacks2 *)(pvthis);
    return pICPUModelCallbacks2->Release();
}
DECLSPEC IIfaceObject* call_mods_ICPUModelCallbacks2_QueryIface (void* pvthis, IID_TYPE id)
{
    ICPUModelCallbacks2 * pICPUModelCallbacks2 = (ICPUModelCallbacks2 *)(pvthis);
    return pICPUModelCallbacks2->QueryIface(id);
}
DECLSPEC void call_mods_ICPUModelCallbacks2_CPUModelAtsRequest (void* pvthis, LwPciDev bdf, LwU032 pasid, LwU064 address, bool isGpa, LwU032 numPages, bool numPageAlign, LwU032 * pageSize, CPUModelAtsResult * results)
{
    ICPUModelCallbacks2 * pICPUModelCallbacks2 = (ICPUModelCallbacks2 *)(pvthis);
    return pICPUModelCallbacks2->CPUModelAtsRequest(bdf, pasid, address, isGpa, numPages, numPageAlign, pageSize, results);
}
} // extern "C"

RC PushICPUModelCallbacks2FptrsToSim(void* pSimLibModule)
{
    FN_PTR_PushICPUModelCallbacks2Funcs fpPushVtable = 
            (FN_PTR_PushICPUModelCallbacks2Funcs) Xp::GetDLLProc(pSimLibModule, "PushICPUModelCallbacks2Funcs");
    if (0 == fpPushVtable)
    {
        Printf(Tee::PriHigh, "Can not get PushICPUModelCallbacks2Funcs iface from sim.\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }

    ICPUModelCallbacks2Funcs vtable;
    vtable.AddRef = call_mods_ICPUModelCallbacks2_AddRef;
    vtable.Release = call_mods_ICPUModelCallbacks2_Release;
    vtable.QueryIface = call_mods_ICPUModelCallbacks2_QueryIface;
    vtable.CPUModelAtsRequest = call_mods_ICPUModelCallbacks2_CPUModelAtsRequest;

    if (!(*fpPushVtable)(vtable))
    {
        Printf(Tee::PriHigh, "PushICPUModelCallbacks2Funcs failed.\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }
    return OK;
}
}; // namespace IFSPEC3
