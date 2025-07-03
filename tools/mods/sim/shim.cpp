/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012, 2015-2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/xp.h"
#include "core/include/massert.h"
#include "shim.h"
#include "core/include/tee.h"
#include "simcallmutex.h"

ChipProxy::ChipProxy(void *module, IChip* chip)
: m_Module(module), m_Chip(chip), m_RefCount(1)
{
    m_VTable.AddRef = (void_call_void)Xp::GetDLLProc(module, "call_server_AddRef");
    m_VTable.Release = (void_call_void)Xp::GetDLLProc(module, "call_server_Release");
    m_VTable.QueryIface = (call_IIfaceObject)Xp::GetDLLProc(module, "call_server_QueryIface");

    m_VTable.Startup = (call_Startup)Xp::GetDLLProc(module, "call_server_Startup");
    m_VTable.Shutdown = (void_call_void)Xp::GetDLLProc(module, "call_server_Shutdown");
    m_VTable.AllocSysMem = (call_AllocSysMem)Xp::GetDLLProc(module, "call_server_AllocSysMem");
    m_VTable.FreeSysMem = (call_FreeSysMem)Xp::GetDLLProc(module, "call_server_FreeSysMem");
    m_VTable.ClockSimulator = (call_ClockSimulator)Xp::GetDLLProc(module, "call_server_ClockSimulator");
    m_VTable.Delay = (call_Delay)Xp::GetDLLProc(module, "call_server_Delay");
    m_VTable.EscapeWrite = (call_EscapeWrite)Xp::GetDLLProc(module, "call_server_EscapeWrite");
    m_VTable.EscapeRead = (call_EscapeRead)Xp::GetDLLProc(module, "call_server_EscapeRead");
    m_VTable.FindPCIDevice = (call_FindPCIDevice)Xp::GetDLLProc(module, "call_server_FindPCIDevice");
    m_VTable.FindPCIClassCode = (call_FindPCIClassCode)Xp::GetDLLProc(module, "call_server_FindPCIClassCode");
    m_VTable.GetSimulatorTime = (call_GetSimulatorTime)Xp::GetDLLProc(module, "call_server_GetSimulatorTime");
    m_VTable.GetSimulatorTimeUnitsNS = (call_GetSimulatorTimeUnitsNS)Xp::GetDLLProc(module, "call_server_GetSimulatorTimeUnitsNS");
    m_VTable.GetSimulatorTimeUnitsNS = (call_GetSimulatorTimeUnitsNS)Xp::GetDLLProc(module, "call_server_GetSimulatorTimeUnitsNS");
    m_VTable.GetChipLevel = (call_GetChipLevel)Xp::GetDLLProc(module, "call_server_GetChipLevel");

    MASSERT(m_VTable.SanityCheck());
}

void ChipProxy::AddRef()
{
    SimCallMutex m;
    m_VTable.AddRef(m_Chip);
    ++m_RefCount;
}

void ChipProxy::Release()
{
    MASSERT(m_RefCount > 0);
    SimCallMutex m;
    m_VTable.Release(m_Chip);
    if (--m_RefCount == 0)
        delete this;
}

IIfaceObject* ChipProxy::QueryIface(IID_TYPE id)
{
    SimCallMutex m;
    IIfaceObject* interface = m_VTable.QueryIface(m_Chip, id);

    switch (id)
    {
        case IID_BUSMEM_IFACE:
            Printf(Tee::PriDebug, "IFSPEC2: sim supplies IBusMem to mods.\n");
            return new BusMemProxy(m_Module, interface);
        case IID_AMODEL_BACKDOOR_IFACE:
            Printf(Tee::PriDebug, "IFSPEC2: sim supplies IAModelBackDoor to mods.\n");
            return new AModelBackdoorProxy(m_Module, interface);
        case IID_MEMORY_IFACE:
            Printf(Tee::PriDebug, "IFSPEC2: sim supplies IMemory to mods.\n");
            return new MemoryProxy(m_Module, interface);
        case IID_MULTIHEAP_IFACE:
            return 0;
        case IID_MULTIHEAP2_IFACE:
            return 0;
        case IID_MEMALLOC64_IFACE:
            return 0;
        case IID_GPUESCAPE_IFACE:
            return 0;
        case IID_GPUESCAPE2_IFACE:
            return 0;
        case IID_INTERRUPT_MGR_IFACE:
            return 0;
        case IID_INTERRUPT_MGR2_IFACE:
            return 0;
        case IID_INTERRUPT_MASK_IFACE:
            return 0;
        case IID_IO_IFACE:
            Printf(Tee::PriDebug, "IFSPEC2: sim supplies IIo to mods.\n");
            return new IoProxy(m_Module, interface);
        case IID_CLOCK_MGR_IFACE:
            return 0;
        case IID_PCI_DEV_IFACE:
            return 0;
        case IID_PCI_DEV2_IFACE:
            return 0;
        case IID_PPC_IFACE:
            return 0;
        case IID_INTERRUPT_MGR3_IFACE:
            Printf(Tee::PriDebug, "IFSPEC2: sim supplies InterruptMgr3 to mods.\n");
            if (interface)
                return new InterruptMgr3Proxy(m_Module, interface);
            else
                return 0;
        default:
            MASSERT(!"Unknown interface!");
            return 0;
    }
}

int ChipProxy::Startup(IIfaceObject* system, char** argv, int argc)
{
    SimCallMutex m;
    return m_VTable.Startup(m_Chip, system, argv, argc);
}

void ChipProxy::Shutdown()
{
    SimCallMutex m;
    return m_VTable.Shutdown(m_Chip);
}

int ChipProxy::AllocSysMem(int numBytes, LwU032* physAddr)
{
    SimCallMutex m;
    return m_VTable.AllocSysMem(m_Chip, numBytes, physAddr);
}

void ChipProxy::FreeSysMem(LwU032 physAddr)
{
    SimCallMutex m;
    return m_VTable.FreeSysMem(m_Chip, physAddr);
}

void ChipProxy::ClockSimulator(LwS032 numClocks)
{
    SimCallMutex m;
    return m_VTable.ClockSimulator(m_Chip, numClocks);
}

void ChipProxy::Delay(LwU032 numMicroSeconds)
{
    SimCallMutex m;
    return m_VTable.Delay(m_Chip, numMicroSeconds);
}

int ChipProxy::EscapeWrite(char* path, LwU032 index, LwU032 size, LwU032 value)
{
    SimCallMutex m;
    return m_VTable.EscapeWrite(m_Chip, path, index, size, value);
}

int ChipProxy::EscapeRead(char* path, LwU032 index, LwU032 size, LwU032* value)
{
    SimCallMutex m;
    return m_VTable.EscapeRead(m_Chip, path, index, size, value);
}

int ChipProxy::FindPCIDevice(LwU016 vendorId, LwU016 deviceId, int index, LwU032* address)
{
    SimCallMutex m;
    return m_VTable.FindPCIDevice(m_Chip, vendorId, deviceId, index, address);
}

int ChipProxy::FindPCIClassCode(LwU032 classCode, int index, LwU032* address)
{
    SimCallMutex m;
    return m_VTable.FindPCIClassCode(m_Chip, classCode, index, address);
}

int ChipProxy::GetSimulatorTime(LwU064* simTime)
{
    SimCallMutex m;
    return m_VTable.GetSimulatorTime(m_Chip, simTime);
}

double ChipProxy::GetSimulatorTimeUnitsNS()
{
    SimCallMutex m;
    return m_VTable.GetSimulatorTimeUnitsNS(m_Chip);
}

IChip::ELEVEL ChipProxy::GetChipLevel()
{
    SimCallMutex m;
    return m_VTable.GetChipLevel(m_Chip);
}

//----------------------------------------------------------------------
// Proxy for IBusMem interface
//----------------------------------------------------------------------
BusMemProxy::BusMemProxy(void *module, IIfaceObject* chip) : m_Bus((IBusMem*)chip)
{
    m_VTable.Release = (void_call_void)Xp::GetDLLProc(module, "call_server_Release");

    m_VTable.BusMemWrBlk = (call_BusMemWrBlk)Xp::GetDLLProc(module, "call_server_BusMemWrBlk");
    m_VTable.BusMemRdBlk = (call_BusMemRdBlk)Xp::GetDLLProc(module, "call_server_BusMemRdBlk");
    m_VTable.BusMemCpBlk = (call_BusMemCpBlk)Xp::GetDLLProc(module, "call_server_BusMemCpBlk");
    m_VTable.BusMemSetBlk = (call_BusMemSetBlk)Xp::GetDLLProc(module, "call_server_BusMemSetBlk");

    MASSERT(m_VTable.SanityCheck());
}

BusMemRet BusMemProxy::BusMemWrBlk(LwU064 address, const void *appdata, LwU032 count)
{
    SimCallMutex m;
    return m_VTable.BusMemWrBlk(m_Bus, address, appdata, count);
}

BusMemRet BusMemProxy::BusMemRdBlk(LwU064 address, void *appdata, LwU032 count)
{
    SimCallMutex m;
    return m_VTable.BusMemRdBlk(m_Bus, address, appdata, count);
}

BusMemRet BusMemProxy::BusMemCpBlk(LwU064 dest, LwU064 source, LwU032 count)
{
    SimCallMutex m;
    return m_VTable.BusMemCpBlk(m_Bus, dest, source, count);
}

BusMemRet BusMemProxy::BusMemSetBlk(LwU064 address, LwU032 size, void* data, LwU032 data_size)
{
    SimCallMutex m;
    return m_VTable.BusMemSetBlk(m_Bus, address, size, data, data_size);
}

void BusMemProxy::Release()
{
    SimCallMutex m;
    m_VTable.Release(m_Bus);

    // No need to keep ref count since AddRef & QueryIface aren't implemented
    delete this;
}

//----------------------------------------------------------------------
// Proxy for IAModelBackdoor interface
//----------------------------------------------------------------------
AModelBackdoorProxy::AModelBackdoorProxy(void* module, IIfaceObject* backdoor) : m_AModelBackdoor(backdoor)
{
    m_VTable.Release = (void_call_void)Xp::GetDLLProc(module, "call_server_Release");

    m_VTable.AllocContextDma = (call_AllocContextDma)Xp::GetDLLProc(module, "call_server_AllocContextDma");
    m_VTable.AllocChannelDma = (call_AllocChannelDma)Xp::GetDLLProc(module, "call_server_AllocChannelDma");
    m_VTable.FreeChannel = (call_FreeChannel)Xp::GetDLLProc(module, "call_server_FreeChannel");
    m_VTable.AllocObject = (call_AllocObject)Xp::GetDLLProc(module, "call_server_AllocObject");
    m_VTable.FreeObject = (call_FreeObject)Xp::GetDLLProc(module, "call_server_FreeObject");
    m_VTable.AllocChannelGpFifo = (call_AllocChannelGpFifo)Xp::GetDLLProc(module, "call_server_AllocChannelGpFifo");
    m_VTable.PassAdditionalVerification = (call_PassAdditionalVerification)Xp::GetDLLProc(module, "call_server_PassAdditionalVerification");
    m_VTable.GetModelIdentifierString = (call_GetModelIdentifierString)Xp::GetDLLProc(module, "call_server_GetModelIdentifierString");

    MASSERT(m_VTable.SanityCheck());
}

void AModelBackdoorProxy::Release()
{
    SimCallMutex m;
    m_VTable.Release(m_AModelBackdoor);

    // No need to keep ref count since AddRef & QueryIface aren't implemented
    delete this;
}

void AModelBackdoorProxy::AllocContextDma(LwU032 ChID, LwU032 Handle, LwU032 Class, LwU032 target,
        LwU032 Limit, LwU032 Base, LwU032 Protect, LwU032 *PageTable)
{
    SimCallMutex m;
    return m_VTable.AllocContextDma(m_AModelBackdoor, ChID, Handle, Class, target, Limit, Base,
                                                   Protect, PageTable);
}

void AModelBackdoorProxy::AllocChannelDma(LwU032 ChID, LwU032 Class, LwU032 CtxDma, LwU032 ErrorNotifierCtxDma)
{
    SimCallMutex m;
    return m_VTable.AllocChannelDma(m_AModelBackdoor, ChID, Class, CtxDma, ErrorNotifierCtxDma);
}

void AModelBackdoorProxy::FreeChannel(LwU032 ChID)
{
    SimCallMutex m;
    return m_VTable.FreeChannel(m_AModelBackdoor, ChID);
}

void AModelBackdoorProxy::AllocObject(LwU032 ChID, LwU032 Handle, LwU032 Class)
{
    SimCallMutex m;
    return m_VTable.AllocObject(m_AModelBackdoor, ChID, Handle, Class);
}

void AModelBackdoorProxy::FreeObject(LwU032 ChID, LwU032 Handle)
{
    SimCallMutex m;
    return m_VTable.FreeObject(m_AModelBackdoor, ChID, Handle);
}

void AModelBackdoorProxy::AllocChannelGpFifo(LwU032 ChID, LwU032 Class, LwU032 CtxDma,
                 LwU064 GpFifoOffset, LwU032 GpFifoEntries, LwU032 ErrorNotifierCtxDma)
{
    SimCallMutex m;
    return m_VTable.AllocChannelGpFifo(m_AModelBackdoor, ChID, Class, CtxDma, GpFifoOffset,
            GpFifoEntries, ErrorNotifierCtxDma);
}

bool AModelBackdoorProxy::PassAdditionalVerification(const char *traceFileName)
{
    SimCallMutex m;
    return m_VTable.PassAdditionalVerification(m_AModelBackdoor, traceFileName);
}

const char* AModelBackdoorProxy::GetModelIdentifierString()
{
    SimCallMutex m;
    return m_VTable.GetModelIdentifierString ? m_VTable.GetModelIdentifierString(m_AModelBackdoor) : 0;
}

//----------------------------------------------------------------------
// Proxy for IMemory interface
//----------------------------------------------------------------------
MemoryProxy::MemoryProxy(void* module, IIfaceObject* memory) : m_Memory(memory)
{
    m_VTable.Release = (void_call_void)Xp::GetDLLProc(module, "call_server_Release");

    MASSERT(m_VTable.SanityCheck());
}

void MemoryProxy::Release()
{
    SimCallMutex m;
    m_VTable.Release(m_Memory);

    // No need to keep ref count since AddRef & QueryIface aren't implemented
    delete this;
}

//----------------------------------------------------------------------
// Proxy for IIo interface
//----------------------------------------------------------------------
IoProxy::IoProxy(void* module, IIfaceObject* io) : m_Io(io)
{
    m_VTable.IoRd08 = (call_IoRd08)Xp::GetDLLProc(module, "call_server_IoRd08");
    m_VTable.IoRd16 = (call_IoRd16)Xp::GetDLLProc(module, "call_server_IoRd16");
    m_VTable.IoRd32 = (call_IoRd32)Xp::GetDLLProc(module, "call_server_IoRd32");
    m_VTable.IoWr08 = (call_IoWr08)Xp::GetDLLProc(module, "call_server_IoWr08");
    m_VTable.IoWr16 = (call_IoWr16)Xp::GetDLLProc(module, "call_server_IoWr16");
    m_VTable.IoWr32 = (call_IoWr32)Xp::GetDLLProc(module, "call_server_IoWr32");
    m_VTable.CfgRd08 = (call_CfgRd08)Xp::GetDLLProc(module, "call_server_CfgRd08");
    m_VTable.CfgRd16 = (call_CfgRd16)Xp::GetDLLProc(module, "call_server_CfgRd16");
    m_VTable.CfgRd32 = (call_CfgRd32)Xp::GetDLLProc(module, "call_server_CfgRd32");
    m_VTable.CfgWr08 = (call_CfgWr08)Xp::GetDLLProc(module, "call_server_CfgWr08");
    m_VTable.CfgWr16 = (call_CfgWr16)Xp::GetDLLProc(module, "call_server_CfgWr16");
    m_VTable.CfgWr32 = (call_CfgWr32)Xp::GetDLLProc(module, "call_server_CfgWr32");
    m_VTable.Release = (void_call_void)Xp::GetDLLProc(module, "call_server_Release");

    MASSERT(m_VTable.SanityCheck());
}

void IoProxy::Release()
{
    SimCallMutex m;
    m_VTable.Release(m_Io);

    // No need to keep ref count since AddRef & QueryIface aren't implemented
    delete this;
}

void IoProxy::IoWr08(LwU016 address, LwU008 data)
{
    SimCallMutex m;
    return m_VTable.IoWr08(m_Io, address, data);
}

void IoProxy::IoWr16(LwU016 address, LwU016 data)
{
    SimCallMutex m;
    return m_VTable.IoWr16(m_Io, address, data);
}

void IoProxy::IoWr32(LwU016 address, LwU032 data)
{
    SimCallMutex m;
    return m_VTable.IoWr32(m_Io, address, data);
}

LwU008 IoProxy::IoRd08(LwU016 address)
{
    SimCallMutex m;
    return m_VTable.IoRd08(m_Io, address);
}

LwU016 IoProxy::IoRd16(LwU016 address)
{
    SimCallMutex m;
    return m_VTable.IoRd16(m_Io, address);
}

LwU032 IoProxy::IoRd32(LwU016 address)
{
    SimCallMutex m;
    return m_VTable.IoRd32(m_Io, address);
}

void IoProxy::CfgWr08(LwU032 address, LwU008 data)
{
    SimCallMutex m;
    return m_VTable.CfgWr08(m_Io, address, data);
}

void IoProxy::CfgWr16(LwU032 address, LwU016 data)
{
    SimCallMutex m;
    return m_VTable.CfgWr16(m_Io, address, data);
}

void IoProxy::CfgWr32(LwU032 address, LwU032 data)
{
    SimCallMutex m;
    return m_VTable.CfgWr32(m_Io, address, data);
}

LwU008 IoProxy::CfgRd08(LwU032 address)
{
    SimCallMutex m;
    return m_VTable.CfgRd08(m_Io, address);
}

LwU016 IoProxy::CfgRd16(LwU032 address)
{
    SimCallMutex m;
    return m_VTable.CfgRd16(m_Io, address);
}

LwU032 IoProxy::CfgRd32(LwU032 address)
{
    SimCallMutex m;
    return m_VTable.CfgRd32(m_Io, address);
}

// ----------------------------------------------------------------------------

extern "C" {
IIfaceObject* call_mods_ifspec2_QueryIface(IIfaceObject* system, IID_TYPE id)
{
    return system->QueryIface(id);
}

void call_mods_ifspec2_Release(IIfaceObject* system)
{
    system->Release();
}

BusMemRet call_mods_ifspec2_BusMemWrBlk(IBusMem* bus, LwU064 address, const void *appdata, LwU032 count)
{
    return bus->BusMemWrBlk(address, appdata, count);
}

BusMemRet call_mods_ifspec2_BusMemRdBlk(IBusMem* bus, LwU064 address, void *appdata, LwU032 count)
{
    return bus->BusMemRdBlk(address, appdata, count);
}

BusMemRet call_mods_ifspec2_BusMemCpBlk(IBusMem* bus, LwU064 dest, LwU064 source, LwU032 count)
{
    return bus->BusMemCpBlk(dest, source, count);
}

BusMemRet call_mods_ifspec2_BusMemSetBlk(IBusMem* bus, LwU064 address, LwU032 size, void* data, LwU032 data_size)
{
    return bus->BusMemSetBlk(address, size, data, data_size);
}

void call_mods_ifspec2_HandleInterrupt(IInterrupt* system)
{
    system->HandleInterrupt();
}

void call_mods_ifspec2_HandleSpecificInterrupt(IInterrupt3* system, LwU032 irqNumber)
{
    system->HandleSpecificInterrupt(irqNumber);
}

}

//----------------------------------------------------------------------
// Proxy for InterruptMgr3 interface
//----------------------------------------------------------------------
InterruptMgr3Proxy::InterruptMgr3Proxy(void *module, IIfaceObject* chip) :
    m_InterruptMgr3((IInterruptMgr3*)chip)
{
    m_VTable.Release = (void_call_void)Xp::GetDLLProc(module, "call_server_Release");

    m_VTable.AllocateIRQs = (call_AllocateIRQs) Xp::GetDLLProc(module, "call_server_AllocateIRQs");
    m_VTable.HookInterrupt3 = (call_HookInterrupt3) Xp::GetDLLProc(module,
                                 "call_server_HookInterrupt3");
    m_VTable.FreeIRQs = (call_FreeIRQs) Xp::GetDLLProc(module, "call_server_FreeIRQs");
    MASSERT(m_VTable.SanityCheck());
}

int InterruptMgr3Proxy::AllocateIRQs(LwPciDev pciDev, LwU032 lwecs, LwU032 *irqs, LwU032 flags)
{
    SimCallMutex m;
    return m_VTable.AllocateIRQs(m_InterruptMgr3, pciDev, lwecs, irqs, flags);
}

int InterruptMgr3Proxy::HookInterrupt3(IrqInfo2 irqInfo)
{
    SimCallMutex m;
    return m_VTable.HookInterrupt3(m_InterruptMgr3, irqInfo);
}

void InterruptMgr3Proxy::FreeIRQs(LwPciDev pciDev)
{
    SimCallMutex m;
    return m_VTable.FreeIRQs(m_InterruptMgr3, pciDev);
}

void InterruptMgr3Proxy::Release()
{
    SimCallMutex m;
    m_VTable.Release(m_InterruptMgr3);

    // No need to keep ref count since AddRef & QueryIface aren't implemented
    delete this;
}
