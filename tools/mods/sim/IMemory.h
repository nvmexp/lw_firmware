/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008,2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _IMEMORY_H_
#define _IMEMORY_H_

#include "ITypes.h"
#include "IIface.h"

class IMemory : public IIfaceObject {
public:
    virtual LwU008 MemRd08(LwU032 address) = 0;
    virtual LwU016 MemRd16(LwU032 address) = 0;
    virtual LwU032 MemRd32(LwU032 address) = 0;
    virtual LwU064 MemRd64(LwU032 address) = 0;
    virtual void MemWr08(LwU032 address, LwU008 data) = 0;
    virtual void MemWr16(LwU032 address, LwU016 data) = 0;
    virtual void MemWr32(LwU032 address, LwU032 data) = 0;
    virtual void MemWr64(LwU032 address, LwU064 data) = 0;

    virtual void MemSet08(LwU032 address, LwU032 size, LwU008 data) = 0;
    virtual void MemSet16(LwU032 address, LwU032 size, LwU016 data) = 0;
    virtual void MemSet32(LwU032 address, LwU032 size, LwU032 data) = 0;
    virtual void MemSet64(LwU032 address, LwU032 size, LwU064 data) = 0;
    virtual void MemSetBlk(LwU032 address, LwU032 size, void* data, LwU032 data_size)
    {
        while ( size > 0 ) {
            MemWrBlk(address, data, data_size);
            address += data_size;
            size -= data_size;
        }
    }

    virtual void MemWrBlk(LwU032 address, const void *appdata, LwU032 count) = 0;
    virtual void MemWrBlk32(LwU032 address, const void *appdata, LwU032 count) = 0;
    virtual void MemRdBlk(LwU032 address, void *appdata, LwU032 count) = 0;
    virtual void MemRdBlk32(LwU032 address, void *appdata, LwU032 count) = 0;
    virtual void MemCpBlk(LwU032 address, LwU032 appdata, LwU032 count) = 0;
    virtual void MemCpBlk32(LwU032 address, LwU032 appdata, LwU032 count) = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif
