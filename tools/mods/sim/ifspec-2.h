/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef IFSPEC_2_H
#define IFSPEC_2_H

#include "IChip.h"

// forward declarations
class IInterrupt2;
class IInterrupt2a;
class IInterrupt3;
class IInterrupt4;

extern "C" {
IIfaceObject* call_client_QueryIface(IIfaceObject* system, IID_TYPE id);
void      call_client_Release(IIfaceObject* system);
BusMemRet call_client_BusMemWrBlk(IBusMem*, LwU064 address, const void *appdata, LwU032 count);
BusMemRet call_client_BusMemRdBlk(IBusMem*, LwU064 address, void *appdata, LwU032 count);
BusMemRet call_client_BusMemCpBlk(IBusMem*, LwU064 dest, LwU064 source, LwU032 count);
BusMemRet call_client_BusMemSetBlk(IBusMem*, LwU064 address, LwU032 size, void* data, LwU032 data_size);
void      call_client_HandleInterrupt(IInterrupt* system);
void      call_client_HandleInterrupt2(IInterrupt2* system);
void      call_client_DeassertInterrupt2(IInterrupt2* system);
void      call_client_HandleSpecificInterrupt2a(IInterrupt2a* system, LwU032 whichInterrupt);
void      call_client_DeassertSpecificInterrupt2a(IInterrupt2a* system, LwU032 whichInterrupt);
void      call_client_HandleSpecificInterrupt3(IInterrupt3* system, LwU032 irqNumber);
void      call_client_HandleInterruptVectorChange(IInterrupt4* system, const LwU032 *v, LwU032 n);
}

typedef void(*void_call_void)(IIfaceObject*);
typedef IIfaceObject*(*call_IIfaceObject)(IIfaceObject*, IID_TYPE);
typedef int(*call_Startup)(IIfaceObject*, IIfaceObject* system, char** argv, int argc);
typedef int (*call_AllocSysMem)(IIfaceObject*, int numBytes, LwU032* physAddr);
typedef void (*call_FreeSysMem)(IIfaceObject*, LwU032 physAddr);
typedef void (*call_ClockSimulator)(IIfaceObject*, LwS032 numClocks);
typedef void (*call_Delay)(IIfaceObject*, LwU032 numMicroSeconds);
typedef int (*call_EscapeWrite)(IIfaceObject*, char* path, LwU032 index, LwU032 size, LwU032 value);
typedef int (*call_EscapeRead)(IIfaceObject*, char* path, LwU032 index, LwU032 size, LwU032* value);
typedef int (*call_FindPCIDevice)(IIfaceObject*, LwU016 vendorId, LwU016 deviceId, int index, LwU032* address);
typedef int (*call_FindPCIClassCode)(IIfaceObject*, LwU032 classCode, int index, LwU032* address);
typedef int (*call_GetSimulatorTime)(IIfaceObject*, LwU064* simTime);
typedef double (*call_GetSimulatorTimeUnitsNS)(IIfaceObject*);
typedef IChip::ELEVEL (*call_GetChipLevel)(IIfaceObject*);

typedef BusMemRet (*call_BusMemWrBlk)(IBusMem*, LwU064, const void *, LwU032);
typedef BusMemRet (*call_BusMemRdBlk)(IBusMem*, LwU064, void *, LwU032);
typedef BusMemRet (*call_BusMemCpBlk)(IBusMem*, LwU064, LwU064, LwU032);
typedef BusMemRet (*call_BusMemSetBlk)(IBusMem*, LwU064, LwU032, void*, LwU032);

typedef void (*call_AllocContextDma)(IIfaceObject*, LwU032, LwU032, LwU032, LwU032, LwU032, LwU032,
                                                    LwU032, LwU032 *);
typedef void (*call_AllocChannelDma)(IIfaceObject*, LwU032, LwU032, LwU032, LwU032);
typedef void (*call_FreeChannel)(IIfaceObject*, LwU032);
typedef void (*call_AllocObject)(IIfaceObject*, LwU032, LwU032, LwU032);
typedef void (*call_FreeObject)(IIfaceObject*, LwU032, LwU032);
typedef void (*call_ProcessMethod)(IIfaceObject*, LwU032, LwU032, LwU032, LwU032);
typedef void (*call_AllocChannelGpFifo)(IIfaceObject*, LwU032, LwU032, LwU032, LwU064, LwU032, LwU032);
typedef bool (*call_PassAdditionalVerification)(IIfaceObject*, const char *traceFileName);
typedef const char* (*call_GetModelIdentifierString)(IIfaceObject*);

typedef LwU008 (*call_IoRd08)(IIfaceObject*,  LwU016 address);
typedef LwU016 (*call_IoRd16)(IIfaceObject*,  LwU016 address);
typedef LwU032 (*call_IoRd32)(IIfaceObject*,  LwU016 address);
typedef void (*call_IoWr08)(IIfaceObject*,  LwU016 address, LwU008 data);
typedef void (*call_IoWr16)(IIfaceObject*,  LwU016 address, LwU016 data);
typedef void (*call_IoWr32)(IIfaceObject*,  LwU016 address, LwU032 data);
typedef LwU008 (*call_CfgRd08)(IIfaceObject*,  LwU032 address);
typedef LwU016 (*call_CfgRd16)(IIfaceObject*,  LwU032 address);
typedef LwU032 (*call_CfgRd32)(IIfaceObject*,  LwU032 address);
typedef void (*call_CfgWr08)(IIfaceObject*,  LwU032 address, LwU008 data);
typedef void (*call_CfgWr16)(IIfaceObject*,  LwU032 address, LwU016 data);
typedef void (*call_CfgWr32)(IIfaceObject*,  LwU032 address, LwU032 data);

typedef void (*call_HandleInterrupt)(IInterrupt*);
typedef void (*call_HandleInterrupt2)(IInterrupt2*);
typedef void (*call_DeassertInterrupt2)(IInterrupt2*);
typedef void (*call_HandleSpecificInterrupt2a)(IInterrupt2a*, LwU032);
typedef void (*call_DeassertSpecificInterrupt2a)(IInterrupt2a*, LwU032);
typedef void (*call_HandleSpecificInterrupt3)(IInterrupt3*, LwU032);
typedef void (*call_HandleInterruptVectorChange)(IInterrupt4*, const LwU032*, LwU032);

typedef int  (* call_AllocateIRQs )(IInterruptMgr3*, LwPciDev pciDev,
                                    LwU032 lwecs, LwU032 * irqs, LwU032 flags);
typedef int  (* call_HookInterrupt3)(IInterruptMgr3*, IrqInfo2 irqInfo);
typedef void (* call_FreeIRQs )(IInterruptMgr3*, LwPciDev pciDev);

struct BusMemVTable
{
    void_call_void Release;

    call_BusMemWrBlk BusMemWrBlk;
    call_BusMemRdBlk BusMemRdBlk;
    call_BusMemCpBlk BusMemCpBlk;
    call_BusMemSetBlk BusMemSetBlk;

    BusMemVTable() : Release(0), BusMemWrBlk(0), BusMemRdBlk(0), BusMemCpBlk(0), BusMemSetBlk(0) {}

    bool SanityCheck()
    {
        return Release != 0 && BusMemWrBlk != 0 && BusMemRdBlk != 0 && BusMemCpBlk != 0 && BusMemSetBlk != 0;
    }
};

struct InterruptVTable
{
    void_call_void Release;
    call_HandleInterrupt HandleInterrupt;

    bool SanityCheck()
    {
        return Release != 0 && HandleInterrupt != 0;
    }
};

struct Interrupt2VTable
{
    void_call_void Release;
    call_HandleInterrupt2 HandleInterrupt;
    call_DeassertInterrupt2 DeassertInterrupt;

    bool SanityCheck()
    {
        return Release != 0 && HandleInterrupt != 0 && DeassertInterrupt != 0;
    }
};

struct Interrupt2aVTable
{
    void_call_void Release;
    call_HandleSpecificInterrupt2a HandleSpecificInterrupt2a;
    call_DeassertSpecificInterrupt2a DeassertSpecificInterrupt2a;

    bool SanityCheck()
    {
        return Release != 0 && HandleSpecificInterrupt2a != 0 && DeassertSpecificInterrupt2a != 0;
    }
};

struct Interrupt3VTable
{
    void_call_void Release;
    call_HandleSpecificInterrupt3 HandleSpecificInterrupt;

    bool SanityCheck()
    {
        return Release != 0 && HandleSpecificInterrupt != 0;
    }
};

struct Interrupt4VTable
{
    void_call_void Release;
    call_HandleInterruptVectorChange HandleInterruptVectorChange;

    bool SanityCheck()
    {
        return Release != 0 && HandleInterruptVectorChange != 0;
    }
};

struct InterruptMgr3VTable
{
    void_call_void Release;
    call_AllocateIRQs AllocateIRQs;
    call_HookInterrupt3 HookInterrupt3;
    call_FreeIRQs FreeIRQs;

    bool SanityCheck()
    {
        return Release != 0 && AllocateIRQs != 0 && HookInterrupt3 != 0 && FreeIRQs != 0;
    }
};

#endif /* IFSPEC_2_H */

