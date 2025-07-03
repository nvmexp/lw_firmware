/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _ICHIP_H_
#define _ICHIP_H_

#include "ITypes.h"
#include "IIface.h"

#define ESCAPE_SUPPORT_PREFIX "supported/"

class IChip : public IIfaceObject {
public:
    enum {
        DevNotFound = 0xffffffff,
    };

    enum ELEVEL
    {
        ELEVEL_UNKNOWN    = 0,
        ELEVEL_HW         = 1,
        ELEVEL_RTL        = 2,
        ELEVEL_CMODEL     = 3,
        ELEVEL_REMOTE_HW  = 4
    };

    enum ECLOCK
    {
        ECLOCK_PCICLK   = 0,
        ECLOCK_LWCLK    = 1,
        ECLOCK_HOSTCLK  = 2
    };

    enum EBACKDOOR
    {
        EBACKDOOR_SYS           = 0,
        EBACKDOOR_FB            = 1,
        EBACKDOOR_ROM           = 2,
        EBACKDOOR_HOSTFB        = 3,
        EBACKDOOR_SYSTILEGTLB        = 4
    };

    enum EESCAPE_FLAGS {
        EESCAPE_READ            = 1,
        EESCAPE_WRITE           = 2
    };

    enum EBURST
    {
        EBURST_BAR0           = 0,
        EBURST_BAR1           = 1,
        EBURST_BAR2           = 2
    };

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;

    // IChip Interface
    virtual int Startup(IIfaceObject* system, char** argv, int argc) { return Init(argv, argc); }
    virtual void Shutdown() = 0;
    virtual int AllocSysMem(int numBytes, LwU032* physAddr) = 0;
    virtual void FreeSysMem(LwU032 physAddr) = 0;
    virtual void ClockSimulator(LwS032 numClocks) = 0;
    virtual void Delay(LwU032 numMicroSeconds) = 0;
    virtual int EscapeWrite(char* path, LwU032 index, LwU032 size, LwU032 value) { return -1; }
    virtual int EscapeRead(char* path, LwU032 index, LwU032 size, LwU032* value) { return -1; };
    virtual int FindPCIDevice(LwU016 vendorId, LwU016 deviceId, int index, LwU032* address) { return -1; };
    virtual int FindPCIClassCode(LwU032 classCode, int index, LwU032* address) { return -1; };
    virtual int GetSimulatorTime(LwU064* simTime) { return -1; }
    virtual double GetSimulatorTimeUnitsNS(void) { return 1.0; };
    virtual int GetPCIBaseAddress(LwU032 cfgAddr, int index, LwU032* pAddress, LwU032* pSize) { return -1; };
    virtual ELEVEL GetChipLevel(void) { return ELEVEL_UNKNOWN; };

protected:
    virtual int Init(char** argv, int argc) = 0;    // depricated.  use Startup method
};

#endif // _ICHIP_H_
