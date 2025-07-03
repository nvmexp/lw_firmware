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

#ifndef _IBUSMEM_H_
#define _IBUSMEM_H_

#include "ITypes.h"
#include "IIface.h"

enum BusMemRet {
    BUSMEM_HANDLED      = 0,
    BUSMEM_NOTHANDLED   = 1,
};

class IBusMem : public IIfaceObject {
public:
    virtual BusMemRet BusMemWrBlk(LwU064 address, const void *appdata, LwU032 count) = 0;
    virtual BusMemRet BusMemRdBlk(LwU064 address, void *appdata, LwU032 count) = 0;
    virtual BusMemRet BusMemCpBlk(LwU064 dest, LwU064 source, LwU032 count) = 0;
    virtual BusMemRet BusMemSetBlk(LwU064 address, LwU032 size, void* data, LwU032 data_size) = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif // _IBUSMEM_H_
