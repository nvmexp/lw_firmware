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

#ifndef _IGPUESCAPE_H_
#define _IGPUESCAPE_H_

#include "ITypes.h"
#include "IIface.h"

class IGpuEscape : public IIfaceObject {
public:
    virtual int EscapeWrite(LwU032 GpuId, const char* path, LwU032 index, LwU032 size, LwU032 value) { return -1; }
    virtual int EscapeRead(LwU032 GpuId, const char* path, LwU032 index, LwU032 size, LwU032* value) { return -1; }

    virtual int GetGpuId(LwU032 bus, LwU032 dev, LwU032 fnc) { return 0; }

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif
