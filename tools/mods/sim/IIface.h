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

#ifndef _IIFACE_H_
#define _IIFACE_H_

#include "IIdList.h"

class IIfaceObject {
public:
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;

protected:
    IIfaceObject() {
    }

public:
    virtual ~IIfaceObject() {
    }

};

typedef void* (*QueryIfaceFn)(IID_TYPE id);
#define QUERY_PROC_NAME			"QueryIface"
#define QUERY_PROC_NAME_DEF		void* QueryIface(IID_TYPE id)

typedef const char* (*ChipLibVerFn)();
#define CHIP_LIB_VERSION        "ChipLibVersion"

#endif // _IIFACE_H_
